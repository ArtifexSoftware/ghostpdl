/* Copyright (C) 2001-2024 Artifex Software, Inc.
   All Rights Reserved.

   This software is provided AS-IS with no warranty, either express or
   implied.

   This software is distributed under license and may not be copied,
   modified or distributed except as expressly authorized under the terms
   of the license contained in the file LICENSE in this distribution.

   Refer to licensing information at http://www.artifex.com or contact
   Artifex Software, Inc.,  39 Mesa Street, Suite 108A, San Francisco,
   CA 94129, USA, for further information.
*/


/* Command list reading for Ghostscript. */
#include "memory_.h"
#include "gx.h"
#include "gp.h"			/* for gp_fmode_rb */
#include "gpcheck.h"
#include "gserrors.h"
#include "gxdevice.h"
#include "gscoord.h"		/* requires gsmatrix.h */
#include "gsdevice.h"		/* for gs_deviceinitialmatrix */
#include "gxdevmem.h"		/* must precede gxcldev.h */
#include "gxcldev.h"
#include "gxgetbit.h"
#include "gxhttile.h"
#include "gdevplnx.h"
#include "gdevp14.h"
#include "gsmemory.h"
#include "gsicc_cache.h"
/*
 * We really don't like the fact that gdevprn.h is included here, since
 * command lists are supposed to be usable for purposes other than printer
 * devices; but gdev_prn_color_usage and gdev_create_buf_device are
 * currently only applicable to printer devices.
 */
#include "gdevprn.h"
#include "stream.h"
#include "strimpl.h"

/* #define EXTRA_OFFSET_MAP_DEBUGGING */

/* forward decl */
private_st_clist_icctable_entry();
private_st_clist_icctable();

/* ------ Band file reading stream ------ */

#ifdef DEBUG
/* An auxiliary table for mapping clist buffer offsets to cfile offsets. */
typedef struct {
    uint buffered;
    int64_t file_offset;
} cbuf_offset_map_elem;
#endif

/*
 * To separate banding per se from command list interpretation,
 * we make the command list interpreter simply read from a stream.
 * When we are actually doing banding, the stream filters the band file
 * and only passes through the commands for the current bands (or band
 * ranges that include a current band).
 */
typedef struct stream_band_read_state_s {
    stream_state_common;
    gx_band_page_info_t page_info;
    int band_first, band_last;
    uint left;			/* amount of data left in this run */
    cmd_block b_this;
    gs_memory_t *local_memory;
#ifdef DEBUG
    bool skip_first;
    cbuf_offset_map_elem *offset_map;
    int bytes_skipped;
    int offset_map_length;
    int offset_map_max_length;
    int skip_next;
#endif
} stream_band_read_state;

static int
s_band_read_init(stream_state * st)
{
    stream_band_read_state *const ss = (stream_band_read_state *) st;
    const clist_io_procs_t *io_procs = ss->page_info.io_procs;

    ss->left = 0;
    ss->b_this.band_min = 0;
    ss->b_this.band_max = 0;
    ss->b_this.pos = 0;
    return io_procs->rewind(ss->page_info.bfile, false, ss->page_info.bfname);
}

#ifdef DEBUG
static int
s_band_read_init_offset_map(gx_device_clist_reader *crdev, stream_state * st)
{
    stream_band_read_state *const ss = (stream_band_read_state *) st;

    if (gs_debug_c('L')) {
        ss->offset_map_length = 0;
        ss->offset_map_max_length = cbuf_size + 1; /* fixme: Wanted a more accurate implementation. */
        ss->offset_map = (cbuf_offset_map_elem *)gs_alloc_byte_array(crdev->memory,
                    ss->offset_map_max_length, sizeof(*ss->offset_map), "s_band_read_init_offset_map");
        if (ss->offset_map == NULL)
            return_error(gs_error_VMerror);
        ss->offset_map[0].buffered = 0;
        ss->bytes_skipped = 0;
        crdev->offset_map = ss->offset_map; /* Prevent collecting it as garbage.
                                            Debugged with ppmraw -r300 014-09.ps . */
    } else {
        ss->offset_map_length = 0;
        ss->offset_map_max_length = 0;
        ss->offset_map = NULL;
        ss->bytes_skipped = 0;
        crdev->offset_map = NULL;
    }
    ss->skip_first = true;
    ss->skip_next = 0;
    return 0;
}

static void
s_band_read_dnit_offset_map(gx_device_clist_reader *crdev, stream_state * st)
{
    if (gs_debug_c('L')) {
        stream_band_read_state *const ss = (stream_band_read_state *) st;

        gs_free_object(crdev->memory, ss->offset_map, "s_band_read_dnit_offset_map");
        crdev->offset_map = 0;
    }
}
#endif

static int
s_band_read_process(stream_state * st, stream_cursor_read * ignore_pr,
                    stream_cursor_write * pw, bool last)
{
    stream_band_read_state *const ss = (stream_band_read_state *) st;
    register byte *q = pw->ptr;
    byte *wlimit = pw->limit;
    clist_file_ptr cfile = ss->page_info.cfile;
    clist_file_ptr bfile = ss->page_info.bfile;
    uint left = ss->left;
    int status = 1;
    uint count;
    const clist_io_procs_t *io_procs = ss->page_info.io_procs;
    int64_t pos;

    /* left = number of bytes unread in the current command. */
    /* count = number of bytes we have room in our buffer for. */
    while ((count = wlimit - q) != 0) {
        int bmin, bmax;
        /* If there is more data to be read in the current command, then pull that in. */
        if (left) {
            if (count > left)
                count = left;
#ifdef DEBUG
            if (gs_debug_c('L')) {
                if (ss->skip_next) {
                    /* This buffer fill is NOT going into the normal buffer. */
                    ss->skip_next = 0;
                    ss->bytes_skipped += count;
#ifdef EXTRA_OFFSET_MAP_DEBUGGING
                    if (ss->offset_map_length != 1) {
                        dmlprintf(ss->local_memory, "offset_map: confused!\n");
                        exit(1);
                    }
#endif
                } else {
#ifdef EXTRA_OFFSET_MAP_DEBUGGING
                    if (ss->offset_map[ss->offset_map_length - 1].buffered + count > cbuf_size*2) {
                        dmlprintf2(ss->local_memory, "Invalid update to buffered. %d %d\n",
                                   ss->offset_map[ss->offset_map_length - 1].buffered, count);
                        exit(1);
                    }
#endif
                    ss->offset_map[ss->offset_map_length - 1].buffered += count;
                }
            }
#endif
            io_procs->fread_chars(q + 1, count, cfile);
            if (io_procs->ferror_code(cfile) < 0) {
                status = ERRC;
                break;
            }
            q += count;
            left -= count;
            process_interrupts(ss->local_memory);
            continue;
        }
        /* The current command is over. So find the next command in the bfile
         * that applies to the current band(s) and read that in. */
        do {
            int nread;
            /* If we hit eof, end! */
            /* Could this test be moved into the nread < sizeof() test below? */
            if (ss->b_this.band_min == cmd_band_end &&
                io_procs->ftell(bfile) == ss->page_info.bfile_end_pos) {
                pw->ptr = q;
                ss->left = left;
                return EOFC;
            }

            /* Read the next cmd_block from the bfile. Each cmd_block contains
             * the bands to use, and the file position of the END of the data.
             * We therefore want to read the data from the file position given
             * in the PREVIOUS record onwards, and compare to the band min/max
             * given there too. */
            bmin = ss->b_this.band_min;
            bmax = ss->b_this.band_max;
            pos = ss->b_this.pos; /* Record where our data starts! */
            nread = io_procs->fread_chars(&ss->b_this, sizeof(ss->b_this), bfile);
            if (nread < sizeof(ss->b_this)) {
                DISCARD(gs_note_error(gs_error_unregistered)); /* Must not happen. */
                return ERRC;
            }
        } while (ss->band_last < bmin || ss->band_first > bmax);
        /* So let's set up to read the actual command data from cfile. Seek... */
        io_procs->fseek(cfile, pos, SEEK_SET, ss->page_info.cfname);
        left = (uint) (ss->b_this.pos - pos);
#ifdef DEBUG
        if (left > 0  && gs_debug_c('L')) {
            if (ss->offset_map_length >= ss->offset_map_max_length) {
                DISCARD(gs_note_error(gs_error_unregistered)); /* Must not happen. */
                return ERRC;
            }
            ss->offset_map[ss->offset_map_length].file_offset = pos;
            ss->offset_map[ss->offset_map_length].buffered = 0;
            ss->offset_map_length++;
        }
#endif
        if_debug5m('l', ss->local_memory,
                   "[l]reading for bands (%d,%d) at bfile %"PRId64", cfile %"PRId64", length %u\n",
                   bmin, bmax,
                   (io_procs->ftell(bfile) - sizeof(ss->b_this)), (int64_t)pos, left);
    }
    pw->ptr = q;
    ss->left = left;
    return status;
}

/* Stream template */
static const stream_template s_band_read_template = {
    &st_stream_state, s_band_read_init, s_band_read_process, 1, cbuf_size
};

#ifdef DEBUG
/* In DEBUG builds, we maintain an "offset_map" within stream_band_read_state,
 * that allows us to relate offsets within the buffer, to offsets within the
 * cfile.
 *
 * At any given point, for stream_band_read_state *ss:
 *    There are n = ss->offset_map_length records in the table.
 *    offset = 0;
 *    for (i = 0; i < n; i++)
 *       // Offset 'offset' in the buffer corresponds to ss->offset_map[i].file_offset in the file.
 *       offset += ss->offset_map[i].buffered
 *
 * As we pull data from the stream, we keep file_offset and buffered up to date. Note that
 * there are 2 cbuf_size sized buffers in play here. The cmd_buffer has one cbuf_size sized
 * buffer in it. Data is pulled into that from the stream, which has another cbuf_sized
 * buffer into it. Accordingly, 'buffered' should never be > 2*cbuf_size = 8192.
 *
 * Sometimes we will pull data out of the stream, bypassing the cmd_buffer's buffer. In this
 * case, we 'skip' data, and record the number of bytes skipped in ss->bytes_skipped. This
 * should only ever happen when we have already advanced as much as possible (i.e. when the
 * current offset is in the first record).
 */

/* Given buffer_offset (an offset within the buffer), return the number of the offset_map
 * record that contains it. Also fill poffset0 in with the offset of the start of that
 * record within the buffer. (NOTE, depending on how much of the record has already been
 * read, some bytes may already have been lost). */
static int
buffer_segment_index(const stream_band_read_state *ss, uint buffer_offset, uint *poffset0)
{
    uint i, offset0, offset = 0;

#ifdef EXTRA_OFFSET_MAP_DEBUGGING
    dmlprintf1(ss->local_memory, "buffer_segment_index: buffer_offset=%d\n", buffer_offset);
    for (i = 0; i < ss->offset_map_length; i++) {
        dmlprintf3(ss->local_memory, " offset_map[%d].file_offset=%"PRId64" buffered=%d\n", i, ss->offset_map[i].file_offset, ss->offset_map[i].buffered);
    }
#endif
    for (i = 0; i < ss->offset_map_length; i++) {
        offset0 = offset;
        offset += ss->offset_map[i].buffered;
        if (buffer_offset < offset) {
            *poffset0 = offset0;
            return i;
        }
    }
    /* Now cope with the case where we've read exactly to the end of the buffer.
    * There might be more data still to come. */
    if (buffer_offset == offset) {
      *poffset0 = offset0;
      return i-1;
    }
#ifdef EXTRA_OFFSET_MAP_DEBUGGING
    dmlprintf1(ss->local_memory, "buffer_segment_index fail: buffer_offset=%d not found\n", buffer_offset);
    exit(1);
#else
    (void)gs_note_error(gs_error_unregistered); /* Must not happen. */
#endif
    return -1;
}

/* Map from a buffer offset, to the offset of the corresponding byte in the
 * cfile. */
int64_t
clist_file_offset(const stream_state * st, uint buffer_offset)
{
    const stream_band_read_state *ss = (const stream_band_read_state *) st;
    uint offset0;
    int i = buffer_segment_index(ss, buffer_offset, &offset0);

    return ss->offset_map[i].file_offset + (uint)(buffer_offset - offset0);
}

void
top_up_offset_map(stream_state * st, const byte *buf, const byte *ptr, const byte *end)
{
    /* NOTE: The clist data are buffered in the clist reader buffer and in the
       internal buffer of the clist stream. Since the 1st buffer is not accessible
       from s_band_read_process, offset_map corresponds the union of the 2 buffers.
     */
    stream_band_read_state *const ss = (stream_band_read_state *) st;
    uint buffer_offset, offset0, consumed;
    int i;

#ifdef EXTRA_OFFSET_MAP_DEBUGGING
    if (ptr < buf || end < ptr || end < buf || end > buf + cbuf_size)
    {
        dmlprintf3(ss->local_memory, "Invalid pointers for top_up_offset_map: buf=%p ptr=%p end=%p\n", buf, ptr, end);
    }
#endif

    if (!gs_debug_c('L'))
        return;
    if (ss->skip_first) {
        /* Work around the trick with initializing the buffer pointer with the buffer end. */
        ss->skip_first = false;
        return;
    }
    if (ptr == buf)
        return;

    /* We know that buf <= ptr <= end <= buf+4096, so uint is quite enough! */
    buffer_offset = ptr - buf;
    i = buffer_segment_index(ss, buffer_offset, &offset0);

    consumed = buffer_offset - offset0;
#ifdef EXTRA_OFFSET_MAP_DEBUGGING
    dmlprintf3(ss->local_memory, "offset_map: dump %d entries + %d bytes + %d skipped bytes\n", i, consumed, ss->bytes_skipped);
    if (ss->offset_map[i].buffered < consumed) {
        dmlprintf2(ss->local_memory, "Invalid update to buffered. B %d %d\n", ss->offset_map[i].buffered, consumed);
        exit(1);
    }
#endif
    ss->offset_map[i].buffered -= consumed;
    ss->offset_map[i].file_offset += consumed;
    ss->bytes_skipped = 0;
    if (i) {
        memmove(ss->offset_map, ss->offset_map + i,
                (ss->offset_map_length - i) * sizeof(*ss->offset_map));
        ss->offset_map_length -= i;
    }
}

/* This function is called when data is copied from the stream out into a separate
 * buffer without going through the usual clist buffers. Essentially data for the
 * id we are reading at buffer_offset within the buffer is skipped. */
void adjust_offset_map_for_skipped_data(stream_state *st, uint buffer_offset, uint skipped)
{
    uint offset0;
    stream_band_read_state *const ss = (stream_band_read_state *) st;
    int i;

    if (!gs_debug_c('L'))
        return;

    i = buffer_segment_index(ss, buffer_offset, &offset0);

    ss->offset_map[i].buffered -= skipped;
    ss->offset_map[i].file_offset += skipped;
}

void
offset_map_next_data_out_of_band(stream_state *st)
{
    stream_band_read_state *const ss = (stream_band_read_state *) st;

    if (!gs_debug_c('L'))
        return;

    ss->skip_next = 1;
}
#endif /* DEBUG */

/* ------ Reading/rendering ------ */

/* Calculate the raster for a chunky or planar device. */
static int
clist_plane_raster(const gx_device *dev, const gx_render_plane_t *render_plane)
{
    return gx_device_raster_plane(dev, render_plane);
}

/* Select full-pixel rendering if required for RasterOp. */
void
clist_select_render_plane(gx_device *dev, int y, int height,
                          gx_render_plane_t *render_plane, int index)
{
    if (index >= 0) {
        gx_color_usage_t color_usage;
        int ignore_start;

        gdev_prn_color_usage(dev, y, height, &color_usage,  &ignore_start);
        if (color_usage.slow_rop)
            index = -1;
    }
    if (index < 0)
        render_plane->index = index;
    else
        gx_render_plane_init(render_plane, dev, index);
}

/*
 * Do device setup from params stored in command list. This is only for
 * async rendering & assumes that the first command in every command list
 * is a put_params command which sets all space-related parameters to the
 * value they will have for the duration of that command list.
 */
int
clist_setup_params(gx_device *dev)
{
    gx_device_clist *cldev = (gx_device_clist *)dev;
    gx_device_clist_reader * const crdev = &cldev->reader;
    int code = clist_render_init(cldev);

    if (code < 0)
        return code;

    code = clist_playback_file_bands(playback_action_setup,
                                     crdev, &crdev->page_info, 0, 0, 0, 0, 0);

    /* put_params may have reinitialized device into a writer */
    clist_render_init(cldev);

    return code;
}

int
clist_close_writer_and_init_reader(gx_device_clist *cldev)
{
    gx_device_clist_reader * const crdev = &cldev->reader;
    gs_memory_t *base_mem = crdev->memory->thread_safe_memory;
    gs_memory_status_t mem_status;
    int code = 0;

    /* Initialize for rendering if we haven't done so yet. */
    if (crdev->ymin < 0) {
        code = clist_end_page(&cldev->writer);
        if (code < 0)
            return code;
        code = clist_render_init(cldev);
        if (code < 0)
            return code;
        /* allocate and load the color_usage_array */
        code = clist_read_color_usage_array(crdev);
        if (code < 0)
            return code;
        /* Check for and get ICC profile table */
        code = clist_read_icctable(crdev);
        if (code < 0)
            return code;
        /* Allocate the icc cache for the clist reader */
        /* Since we may be rendering in multiple threads, make sure the memory */
        /* is thread safe by using a known thread_safe memory allocator */
        gs_memory_status(base_mem, &mem_status);
        if (mem_status.is_thread_safe == false) {
            return_error(gs_error_VMerror);
        }

        if (crdev->icc_cache_cl == NULL) {
            code = (crdev->icc_cache_cl = gsicc_cache_new(base_mem)) == NULL ? gs_error_VMerror : code;
        }
    }

    check_device_compatible_encoding((gx_device *)cldev);

    return code;
}

/* Used to find the command block information in the bfile
   that is related to extra information stored in a psuedo band.
   Currently application of this is storage of the ICC profile
   table, the per-band color_usage array, and the spot equivalent
   colors when doing overprint simulation.  We may eventually
   use this for storing other information like compressed images.   */

static int
clist_find_pseudoband(gx_device_clist_reader *crdev, int band, cmd_block *cb)
{

    gx_band_page_info_t *page_info = &(crdev->page_info);
    clist_file_ptr bfile = page_info->bfile;
    int64_t save_pos = page_info->bfile_end_pos;
    int64_t start_pos;
    int code;

    if (bfile == NULL) {
        /* files haven't been opened yet. Do it now */
        char fmode[4];

        strcpy(fmode, "r");
        strncat(fmode, gp_fmode_binary_suffix, 1);
        if ((code=page_info->io_procs->fopen(page_info->cfname, fmode,
                      &page_info->cfile,
                      crdev->memory, crdev->memory, true)) < 0 ||
                      (code=page_info->io_procs->fopen(page_info->bfname, fmode,
                      &page_info->bfile,
                      crdev->memory, crdev->memory, false)) < 0) {
            return code;
        }
        bfile = page_info->bfile;
    }
    /* Go to the start of the last command block */
    start_pos = page_info->bfile_end_pos - sizeof(cmd_block);
    page_info->io_procs->fseek(bfile, start_pos, SEEK_SET, page_info->bfname);
    while( 1 ) {
        int read = page_info->io_procs->fread_chars(cb, sizeof(cmd_block), bfile);

        if (read < sizeof(cmd_block))
	    return -1;
        if (cb->band_max == band && cb->band_min == band) {
            page_info->io_procs->fseek(bfile, save_pos, SEEK_SET, page_info->bfname);
            return(0);  /* Found it */
        }
        start_pos -= sizeof(cmd_block);
        if (start_pos < 0) {
           page_info->io_procs->fseek(bfile, save_pos, SEEK_SET, page_info->bfname);
           return(-1);  /* Did not find it before getting into other stuff in normal bands */
        }
        page_info->io_procs->fseek(bfile, start_pos, SEEK_SET, page_info->bfname);
    }
}

/* A procedure to read a chunk of data from the cfile at a particular location into buff */
int
clist_read_chunk(gx_device_clist_reader *crdev, int64_t position, int size, unsigned char *buf)
{
    clist_file_ptr cfile = crdev->page_info.cfile;
    int64_t save_pos;

    /* Save our current location */
    save_pos = crdev->page_info.io_procs->ftell(cfile);
    /* Go to our new position */
    crdev->page_info.io_procs->fseek(cfile, position, SEEK_SET, crdev->page_info.cfname);
    /* Get the data */
    crdev->page_info.io_procs->fread_chars(buf, size, cfile);
    /* Restore our position */
    crdev->page_info.io_procs->fseek(cfile, save_pos, SEEK_SET, crdev->page_info.cfname);
    return 0;
}

/* read the color_usage_array back from the pseudo band */
int
clist_read_color_usage_array(gx_device_clist_reader *crdev)
{
    int code, size_data = crdev->nbands * sizeof(gx_color_usage_t );
    cmd_block cb;

    if (crdev->color_usage_array != NULL)
        gs_free_object(crdev->memory, crdev->color_usage_array,
                       "clist reader color_usage_array");
    crdev->color_usage_array = (gx_color_usage_t *)gs_alloc_bytes(crdev->memory, size_data,
                       "clist reader color_usage_array");
    if (crdev->color_usage_array == NULL)
        return_error(gs_error_VMerror);

    code = clist_find_pseudoband(crdev, crdev->nbands + COLOR_USAGE_OFFSET - 1, &cb);
    if (code < 0)
        return code;

    code = clist_read_chunk(crdev, cb.pos, size_data, (unsigned char *)crdev->color_usage_array);
    return code;
}

/* read the cmyk equivalent spot colors */
int
clist_read_op_equiv_cmyk_colors(gx_device_clist_reader *crdev,
    equivalent_cmyk_color_params *op_equiv_cmyk_colors)
{
    int code;
    cmd_block cb;

    code = clist_find_pseudoband(crdev, crdev->nbands + SPOT_EQUIV_COLORS - 1, &cb);
    if (code < 0)
        return code;

    code = clist_read_chunk(crdev, cb.pos, sizeof(equivalent_cmyk_color_params),
        (unsigned char *)op_equiv_cmyk_colors);
    return code;
}

/* Unserialize the icc table information stored in the cfile and
   place it in the reader device */
static int
clist_unserialize_icctable(gx_device_clist_reader *crdev, cmd_block *cb)
{
    clist_file_ptr cfile = crdev->page_info.cfile;
    clist_icctable_t *icc_table = crdev->icc_table;
    int64_t save_pos;
    int number_entries, size_data;
    unsigned char *buf, *buf_start;
    clist_icctable_entry_t *curr_entry;
    int k;
    gs_memory_t *stable_mem = crdev->memory->stable_memory;

    if ( icc_table != NULL )
        return(0);
    save_pos = crdev->page_info.io_procs->ftell(cfile);
    crdev->page_info.io_procs->fseek(cfile, cb->pos, SEEK_SET, crdev->page_info.cfname);
    /* First four bytes tell us the number of entries. */
    crdev->page_info.io_procs->fread_chars(&number_entries, sizeof(number_entries), cfile);
    /* Allocate the space */
    size_data = number_entries*sizeof(clist_icc_serial_entry_t);
    buf = gs_alloc_bytes(crdev->memory, size_data, "clist_read_icctable");
    buf_start = buf;
    if (buf == NULL)
        return gs_rethrow(-1, "insufficient memory for icc table buffer reader");
    /* Get the data */
    clist_read_chunk(crdev, cb->pos + 4, size_data, buf);
    icc_table = gs_alloc_struct(stable_mem, clist_icctable_t,
                                &st_clist_icctable, "clist_read_icctable");
    if (icc_table == NULL) {
        gs_free_object(stable_mem, buf_start, "clist_read_icctable");
        return gs_rethrow(-1, "insufficient memory for icc table buffer reader");
    }
    icc_table->memory = stable_mem;
    icc_table->head = NULL;
    icc_table->final = NULL;
   /* Allocate and fill each entry */
    icc_table->tablesize = number_entries;
    crdev->icc_table = icc_table;
    for (k = 0; k < number_entries; k++) {
        curr_entry = gs_alloc_struct(stable_mem, clist_icctable_entry_t,
                &st_clist_icctable_entry, "clist_read_icctable");
        if (curr_entry == NULL) {
            gs_free_object(stable_mem, buf_start, "clist_read_icctable");
            return gs_rethrow(-1, "insufficient memory for icc table entry");
        }
        memcpy(&(curr_entry->serial_data), buf, sizeof(clist_icc_serial_entry_t));
        buf += sizeof(clist_icc_serial_entry_t);
        curr_entry->icc_profile = NULL;
        if ( icc_table->head == NULL ) {
            icc_table->head = curr_entry;
            icc_table->final = curr_entry;
        } else {
            icc_table->final->next = curr_entry;
            icc_table->final = curr_entry;
        }
        curr_entry->next = NULL;
    }
    gs_free_object(crdev->memory, buf_start, "clist_read_icctable");
    crdev->page_info.io_procs->fseek(cfile, save_pos, SEEK_SET, crdev->page_info.cfname);
    return 0;
}

/* Get the ICC profile table information from the clist */
int
clist_read_icctable(gx_device_clist_reader *crdev)
{
    /* Look for the command block of the ICC Profile. */
    cmd_block cb;
    int code;

    /* First get the command block which will tell us where the
       information is stored in the cfile */
    code = clist_find_pseudoband(crdev, crdev->nbands + ICC_TABLE_OFFSET - 1, &cb);
    if (code < 0)
        return(0);   /* No ICC information */
    /* Unserialize the icc_table from the cfile */
    code = clist_unserialize_icctable(crdev, &cb);
    return(code);
}

/* Initialize for reading. */
int
clist_render_init(gx_device_clist *dev)
{
    gx_device_clist_reader * const crdev = &dev->reader;

    crdev->ymin = crdev->ymax = 0;
    crdev->yplane.index = -1;
    /* For normal rasterizing, pages and num_pages is 1. */
    crdev->pages = 0;
    crdev->num_pages = 1;		/* always at least one page */
    crdev->offset_map = NULL;
    crdev->icc_table = NULL;
    crdev->color_usage_array = NULL;
    crdev->render_threads = NULL;

    return 0;
}

/* Copy a rasterized rectangle to the client, rasterizing if needed. */
int
clist_get_bits_rectangle(gx_device *dev, const gs_int_rect * prect,
                         gs_get_bits_params_t *params)
{
    gx_device_clist *cldev = (gx_device_clist *)dev;
    gx_device_clist_reader *crdev = &cldev->reader;
    gx_device_clist_common *cdev = (gx_device_clist_common *)dev;
    gs_get_bits_options_t options = params->options;
    int y = prect->p.y;
    int end_y = prect->q.y;
    int line_count = end_y - y;
    gs_int_rect band_rect;
    int lines_rasterized;
    gx_device *bdev;
    uint num_planes =
        (options & GB_PACKING_CHUNKY ? 1 :
         options & GB_PACKING_PLANAR ? dev->color_info.num_components :
         options & GB_PACKING_BIT_PLANAR ? dev->color_info.depth :
         0 /****** NOT POSSIBLE ******/);
    gx_render_plane_t render_plane;
    int plane_index;
    int my;
    int code;

    if (prect->p.x < 0 || prect->q.x > dev->width ||
        y < 0 || end_y > dev->height
        )
        return_error(gs_error_rangecheck);
    if (line_count <= 0 || prect->p.x >= prect->q.x)
        return 0;

    /*
     * Calculate the render_plane from the params.  There are two cases:
     * full pixels, or a single plane.
     */
    plane_index = -1;
    if (options & GB_SELECT_PLANES) {
        /* Look for the one selected plane. */
        int i;

        for (i = 0; i < num_planes; ++i)
            if (params->data[i]) {
                if (plane_index >= 0)  /* >1 plane requested */
                    return gx_default_get_bits_rectangle(dev, prect, params);
                plane_index = i;
            }
    }

    if (0 > (code = clist_close_writer_and_init_reader(cldev)))
        return code;

    clist_select_render_plane(dev, y, line_count, &render_plane, plane_index);
    code = gdev_create_buf_device(cdev->buf_procs.create_buf_device,
                                  &bdev, cdev->target, y, &render_plane,
                                  dev->memory,
                                  &(crdev->color_usage_array[y/crdev->page_info.band_params.BandHeight]));
    if (code < 0)
        return code;
    code = clist_rasterize_lines(dev, y, line_count, bdev, &render_plane, &my);
    if (code >= 0) {
        lines_rasterized = min(code, line_count);
        /* Return as much of the rectangle as falls within the rasterized lines. */
        band_rect = *prect;
        band_rect.p.y = my;
        band_rect.q.y = my + lines_rasterized;
        code = dev_proc(bdev, get_bits_rectangle)
            (bdev, &band_rect, params);
    }
    cdev->buf_procs.destroy_buf_device(bdev);
    if (code < 0 || lines_rasterized == line_count)
        return code;
    /*
     * We'll have to return the rectangle in pieces.  Force GB_RETURN_COPY
     * rather than GB_RETURN_POINTER, and require all subsequent pieces to
     * use the same values as the first piece for all of the other format
     * options.  If copying isn't allowed, or if there are any unread
     * rectangles, punt.
     */
    if (!(options & GB_RETURN_COPY) || code > 0)
        return gx_default_get_bits_rectangle(dev, prect, params);
    options = params->options;
    if (!(options & GB_RETURN_COPY)) {
        /* Redo the first piece with copying. */
        params->options = options =
            (params->options & ~GB_RETURN_ALL) | GB_RETURN_COPY;
        lines_rasterized = 0;
    }
    {
        gs_get_bits_params_t band_params;
        uint raster = gx_device_raster(bdev, true);

        code = gdev_create_buf_device(cdev->buf_procs.create_buf_device,
                                      &bdev, cdev->target, y, &render_plane,
                                      dev->memory,
                                      &(crdev->color_usage_array[y/crdev->page_info.band_params.BandHeight]));
        if (code < 0)
            return code;
        band_params = *params;
        while ((y += lines_rasterized) < end_y) {
            int i;

            /* Increment data pointers by lines_rasterized. */
            for (i = 0; i < num_planes; ++i)
                if (band_params.data[i])
                    band_params.data[i] += raster * lines_rasterized;
            line_count = end_y - y;
            code = clist_rasterize_lines(dev, y, line_count, bdev,
                                         &render_plane, &my);
            if (code < 0)
                break;
            lines_rasterized = min(code, line_count);
            band_rect.p.y = my;
            band_rect.q.y = my + lines_rasterized;
            code = dev_proc(bdev, get_bits_rectangle)
                (bdev, &band_rect, &band_params);
            if (code < 0)
                break;
            params->options = options = band_params.options;
            if (lines_rasterized == line_count)
                break;
        }
        cdev->buf_procs.destroy_buf_device(bdev);
    }
    return code;
}

/* Copy scan lines to the client.  This is where rendering gets done. */
/* Processes min(requested # lines, # lines available thru end of band) */
int	/* returns -ve error code, or # scan lines copied */
clist_rasterize_lines(gx_device *dev, int y, int line_count,
                      gx_device *bdev, const gx_render_plane_t *render_plane,
                      int *pmy)
{
    gx_device_clist * const cldev = (gx_device_clist *)dev;
    gx_device_clist_reader * const crdev = &cldev->reader;
    gx_device *target = crdev->target;
    uint raster = clist_plane_raster(target, render_plane);
    byte *mdata = crdev->data + crdev->page_info.tile_cache_size;
    byte *mlines = (crdev->page_info.line_ptrs_offset == 0 ? NULL : mdata + crdev->page_info.line_ptrs_offset);
    int plane_index = (render_plane ? render_plane->index : -1);
    int code;

    /* Render a band if necessary, and copy it incrementally. */
    if (crdev->ymin < 0 || crdev->yplane.index != plane_index ||
        !(y >= crdev->ymin && y < crdev->ymax)
        ) {
        int band_height = crdev->page_info.band_params.BandHeight;
        int band = y / band_height;
        int band_begin_line = band * band_height;
        int band_end_line = band_begin_line + band_height;
        int band_num_lines;
        gs_int_rect band_rect;

        if (band_end_line > dev->height)
            band_end_line = dev->height;
        /* Clip line_count to current band */
        if (line_count > band_end_line - y)
            line_count = band_end_line - y;
        band_num_lines = band_end_line - band_begin_line;

        if (y < 0 || y > dev->height)
            return_error(gs_error_rangecheck);
        code = crdev->buf_procs.setup_buf_device
            (bdev, mdata, raster, (byte **)mlines, 0, band_num_lines, band_num_lines);
        band_rect.p.x = 0;
        band_rect.p.y = band_begin_line;
        band_rect.q.x = dev->width;
        band_rect.q.y = band_end_line;
        if (code >= 0)
            code = clist_render_rectangle(cldev, &band_rect, bdev, render_plane,
                                          true);
        /* Reset the band boundaries now, so that we don't get */
        /* an infinite loop. */
        crdev->ymin = band_begin_line;
        crdev->ymax = band_end_line;
        crdev->offset_map = NULL;
        if (code < 0)
            return code;
    }

    if (line_count > crdev->ymax - y)
        line_count = crdev->ymax - y;
    code = crdev->buf_procs.setup_buf_device
        (bdev, mdata, raster, (byte **)mlines, y - crdev->ymin, line_count,
         crdev->ymax - crdev->ymin);
    if (code < 0)
        return code;

    *pmy = 0;
    return line_count;
}

/*
 * Render a rectangle to a client-supplied device.  There is no necessary
 * relationship between band boundaries and the region being rendered.
 */
int
clist_render_rectangle(gx_device_clist *cldev, const gs_int_rect *prect,
                       gx_device *bdev,
                       const gx_render_plane_t *render_plane, bool clear)
{
    gx_device_clist_reader * const crdev = &cldev->reader;
    const gx_placed_page *ppages;
    int num_pages = crdev->num_pages;
    int band_height = crdev->page_info.band_params.BandHeight;
    int band_first = prect->p.y / band_height;
    int band_last = (prect->q.y - 1) / band_height;
    gx_band_page_info_t *pinfo;
    gx_band_page_info_t page_info;
    int code = 0;
    int i;
    bool save_pageneutralcolor;

    if (render_plane)
        crdev->yplane = *render_plane;
    else
        crdev->yplane.index = -1;
    if_debug2m('l', bdev->memory, "[l]rendering bands (%d,%d)\n", band_first, band_last);

    ppages = crdev->pages;

    /* Before playing back the clist, make sure that the gray detection is disabled */
    /* so we don't slow down the rendering (primarily high level images).           */
    save_pageneutralcolor = crdev->icc_struct->pageneutralcolor;
    crdev->icc_struct->pageneutralcolor = false;

    for (i = 0; i < num_pages && code >= 0; ++i) {
        bool pdf14_needed = false;
        int band;

        if (ppages == NULL) {
                /*
                 * If we aren't rendering saved pages, do the current one.
                 * Note that this is the only case in which we may encounter
                 * a gx_saved_page with non-zero cfile or bfile.
                 */
                bdev->band_offset_x = 0;
                bdev->band_offset_y = band_first * (long)band_height;
                pinfo = &(crdev->page_info);
        } else {
            const gx_placed_page *ppage = &ppages[i];

            /* Store the page information. */
            page_info.cfile = page_info.bfile = NULL;
            memcpy(page_info.cfname, ppage->page->cfname, sizeof(page_info.cfname));
            memcpy(page_info.bfname, ppage->page->bfname, sizeof(page_info.bfname));
            page_info.io_procs = ppage->page->io_procs;
            page_info.tile_cache_size = ppage->page->tile_cache_size;
            page_info.line_ptrs_offset = ppage->page->line_ptrs_offset;
            page_info.bfile_end_pos = ppage->page->bfile_end_pos;
            page_info.band_params = ppage->page->band_params;
            pinfo = &page_info;

            /*
             * Set the band_offset_? values in case the buffer device
             * needs this. Example, a device may need to adjust the
             * phase of the dithering based on the page position, NOT
             * the position within the band buffer to avoid band stitch
             * lines in the dither pattern. The old wtsimdi device did this
             *
             * The band_offset_x is not important for placed pages that
             * are nested on a 'master' page (imposition) since each
             * page expects to be dithered independently, but setting
             * this allows pages to be contiguous without a dithering
             * shift.
             *
             * The following sets the band_offset_? relative to the
             * master page.
             */
            bdev->band_offset_x = ppage->offset.x;
            bdev->band_offset_y = ppage->offset.y + (band_first * band_height);
        }
        /* if any of the requested bands need transparency, use it for all of them   */
        /* The pdf14_ok_to_optimize checks if the target device (bdev) is compatible */
        /* with the pdf14 compositor info that was written to the clist: colorspace, */
        /* colorspace, etc.                                                          */
        pdf14_needed = !pdf14_ok_to_optimize(bdev);
        for (band=band_first; !pdf14_needed && band <= band_last; band++)
            pdf14_needed |= (crdev->color_usage_array[band].trans_bbox.p.y <=
            crdev->color_usage_array[band].trans_bbox.q.y) ? true : false;

        code = clist_playback_file_bands(pdf14_needed ?
                                         playback_action_render : playback_action_render_no_pdf14,
                                         crdev, pinfo,
                                         bdev, band_first, band_last,
                                         prect->p.x - bdev->band_offset_x,
                                         prect->p.y);
    }
    crdev->icc_struct->pageneutralcolor = save_pageneutralcolor;	/* restore it */
    return code;
}

/* Playback the band file, taking the indicated action w/ its contents. */
int
clist_playback_file_bands(clist_playback_action action,
                          gx_device_clist_reader *crdev,
                          gx_band_page_info_t *page_info, gx_device *target,
                          int band_first, int band_last, int x0, int y0)
{
    int code = 0;
    bool opened_bfile = false;
    bool opened_cfile = false;

    /* We have to pick some allocator for rendering.... */
    gs_memory_t *mem =crdev->memory;

    stream_band_read_state rs;

    /* setup stream */
    s_init_state((stream_state *)&rs, &s_band_read_template,
                 (gs_memory_t *)0); /* Not mem, as we don't want to free rs */
    rs.band_first = band_first;
    rs.band_last = band_last;
    rs.page_info = *page_info;
    rs.local_memory = mem;

    /* If this is a saved page, open the files. */
    if (rs.page_info.cfile == 0) {
        code = crdev->page_info.io_procs->fopen(rs.page_info.cfname,
                           gp_fmode_rb, &rs.page_info.cfile, crdev->bandlist_memory,
                           crdev->bandlist_memory, true);
        opened_cfile = (code >= 0);
    }
    if (rs.page_info.bfile == 0 && code >= 0) {
        code = crdev->page_info.io_procs->fopen(rs.page_info.bfname,
                           gp_fmode_rb, &rs.page_info.bfile, crdev->bandlist_memory,
                           crdev->bandlist_memory, false);
        opened_bfile = (code >= 0);
    }
    if (rs.page_info.cfile != 0 && rs.page_info.bfile != 0) {
        stream s;
        byte sbuf[cbuf_size];
        static const stream_procs no_procs = {
            s_std_noavailable, s_std_noseek, s_std_read_reset,
            s_std_read_flush, s_std_close, s_band_read_process
        };

        s_band_read_init((stream_state *)&rs);
#	ifdef DEBUG
        s_band_read_init_offset_map(crdev, (stream_state *)&rs);
#	endif
          /* The stream doesn't need a memory, but we'll need to access s.memory->gs_lib_ctx. */
        s_init(&s, mem);
        s_std_init(&s, sbuf, cbuf_size, &no_procs, s_mode_read);
        s.foreign = 1;
        s.state = (stream_state *)&rs;

        code = clist_playback_band(action, crdev, &s, target, x0, y0, mem);
#	ifdef DEBUG
        s_band_read_dnit_offset_map(crdev, (stream_state *)&rs);
#	endif
    }

    /* Close the files if we just opened them. */
    if (opened_bfile && rs.page_info.bfile != 0)
        crdev->page_info.io_procs->fclose(rs.page_info.bfile, rs.page_info.bfname, false);
    if (opened_cfile && rs.page_info.cfile != 0)
        crdev->page_info.io_procs->fclose(rs.page_info.cfile, rs.page_info.cfname, false);

    return code;
}
