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


/* Command list writing utilities. */

#include "memory_.h"
#include "string_.h"
#include "gx.h"
#include "gp.h"
#include "gpcheck.h"
#include "gserrors.h"
#include "gxdevice.h"
#include "gxdevmem.h"		/* must precede gxcldev.h */
#include "gxcldev.h"
#include "gxclpath.h"
#include "gsparams.h"

#include "valgrind.h"
#include <limits.h>

/* ---------------- Statistics ---------------- */

#ifdef DEBUG
const char *const cmd_op_names[16] =
{cmd_op_name_strings};
static const char *const cmd_misc_op_names[16] =
{cmd_misc_op_name_strings};
static const char *const cmd_misc2_op_names[16] =
{cmd_misc2_op_name_strings};
static const char *const cmd_segment_op_names[16] =
{cmd_segment_op_name_strings};
static const char *const cmd_path_op_names[16] =
{cmd_path_op_name_strings};
const char *const *const cmd_sub_op_names[16] =
{cmd_misc_op_names, 0, 0, 0, 0, 0, 0, 0,
 0, 0, 0, 0,
 0, cmd_misc2_op_names, cmd_segment_op_names, cmd_path_op_names
};
const char *cmd_extend_op_names[256] =
{cmd_extend_op_name_strings};

#ifdef COLLECT_STATS_CLIST
struct stats_cmd_s {
    ulong op_counts[512];
    ulong op_sizes[512];
    ulong tile_reset, tile_found, tile_added;
    ulong same_band, other_band;
} stats_cmd;
extern ulong stats_cmd_diffs[5];	/* in gxclpath.c */
int
cmd_count_op(int op, uint size,const gs_memory_t *mem)
{
    stats_cmd.op_counts[op]++;
    stats_cmd.op_sizes[op] += size;
    if (gs_debug_c('L')) {
        const char *const *sub = cmd_sub_op_names[op >> 4];

        if (sub)
            dmlprintf2(mem, ", %s(%u)\n", sub[op & 0xf], size);
        else
            dmlprintf3(mem, ", %s %d(%u)\n", cmd_op_names[op >> 4], op & 0xf,
                      size);
        dmflush(mem);
    }
    return op;
}
int
cmd_count_extended_op(int op, uint size,const gs_memory_t *mem)
{
    stats_cmd.op_counts[cmd_opv_extend]++;
    stats_cmd.op_sizes[cmd_opv_extend] += size;
    stats_cmd.op_counts[256+op]++;
    stats_cmd.op_sizes[256+op] += size;
    if (gs_debug_c('L')) {
        const char *ext = cmd_extend_op_names[op];

        if (ext)
            dmlprintf2(mem, ", %s(%u)\n", ext, size);
        else
            dmlprintf2(mem, ", ?0x%02x?(%u)\n", op,
                      size);
        dmflush(mem);
    }
    return op;
}
void
cmd_uncount_op(int op, uint size)
{
    stats_cmd.op_counts[op]--;
    stats_cmd.op_sizes[op] -= size;
}
#endif
#endif

/* Print statistics. */
#ifdef COLLECT_STATS_CLIST
void
cmd_print_stats(const gs_memory_t *mem)
{
    int ci, cj;

    dmlprintf3(mem, "[l]counts: reset = %lu, found = %lu, added = %lu\n",
              stats_cmd.tile_reset, stats_cmd.tile_found,
              stats_cmd.tile_added);
    dmlprintf5(mem, "     diff 2.5 = %lu, 3 = %lu, 4 = %lu, 2 = %lu, >4 = %lu\n",
              stats_cmd_diffs[0], stats_cmd_diffs[1], stats_cmd_diffs[2],
              stats_cmd_diffs[3], stats_cmd_diffs[4]);
    dmlprintf2(mem, "     same_band = %lu, other_band = %lu\n",
              stats_cmd.same_band, stats_cmd.other_band);
    for (ci = 0; ci < 0x100; ci += 0x10) {
        const char *const *sub = cmd_sub_op_names[ci >> 4];

        if (sub != 0) {
            dmlprintf1(mem, "[l]  %s =", cmd_op_names[ci >> 4]);
            for (cj = ci; cj < ci + 0x10; cj += 2)
                dmprintf6(mem, "\n\t%s = %lu(%lu), %s = %lu(%lu)",
                         sub[cj - ci],
                         stats_cmd.op_counts[cj], stats_cmd.op_sizes[cj],
                         sub[cj - ci + 1],
                   stats_cmd.op_counts[cj + 1], stats_cmd.op_sizes[cj + 1]);
        } else {
            ulong tcounts = 0, tsizes = 0;

            for (cj = ci; cj < ci + 0x10; cj++)
                tcounts += stats_cmd.op_counts[cj],
                    tsizes += stats_cmd.op_sizes[cj];
            dmlprintf3(mem, "[l]  %s (%lu,%lu) =\n\t",
                      cmd_op_names[ci >> 4], tcounts, tsizes);
            for (cj = ci; cj < ci + 0x10; cj++)
                if (stats_cmd.op_counts[cj] == 0)
                    dmputs(mem, " -");
                else
                    dmprintf2(mem, " %lu(%lu)", stats_cmd.op_counts[cj],
                             stats_cmd.op_sizes[cj]);
        }
        dmputs(mem, "\n");
    }
    for (ci = 0x100; ci < 0x200; ci ++) {
        const char *ext = cmd_extend_op_names[ci-0x100];

        if (ext != NULL) {
            dmprintf3(mem, "[l] %s (%lu,%lu)\n",
                         ext,
                         stats_cmd.op_counts[ci], stats_cmd.op_sizes[ci]);
        } else if (stats_cmd.op_counts[ci] || stats_cmd.op_sizes[ci]) {
            dmprintf3(mem, "[l] ?0x%02x? (%lu,%lu)\n",
                      ci-0x100,
                      stats_cmd.op_counts[ci], stats_cmd.op_sizes[ci]);
        }
        dmputs(mem, "\n");
    }
}
#endif /* DEBUG */

/* ---------------- Writing utilities ---------------- */

/* Update the 'trans_bbox' in the states for bands affected by the given rectangle */
/* The caller has determined the the PDF 1.4 transparency will actuall be needed   */
/* for the given rectangle (conservatively). This will allow some bands that only  */
/* paint to the page level with full opacity to skip the pdf14 compositor during   */
/* rendering/reading and thus run faster and with less memory for those bands.     */
void
clist_update_trans_bbox(gx_device_clist_writer *cldev, gs_int_rect *bbox)
{
    int p_y, q_y;
    int band, first_band, last_band;

    first_band = max(0, bbox->p.y / cldev->page_info.band_params.BandHeight);
    p_y = bbox->p.y - (first_band * cldev->page_info.band_params.BandHeight);
    last_band = min((cldev->nbands - 1), bbox->q.y / cldev->page_info.band_params.BandHeight);

    for (band=first_band; band <= last_band; band++) {
        if (cldev->states[band].color_usage.trans_bbox.p.y > p_y)
            cldev->states[band].color_usage.trans_bbox.p.y = p_y;
        if (cldev->states[band].color_usage.trans_bbox.p.x > bbox->p.x)
            cldev->states[band].color_usage.trans_bbox.p.x = bbox->p.x;
        p_y = 0;	/* will be top of next band */
        q_y = (band == last_band) ? bbox->q.y - (last_band * cldev->page_info.band_params.BandHeight) :
                                      cldev->page_info.band_params.BandHeight - 1;
        if (cldev->states[band].color_usage.trans_bbox.q.y < q_y)
            cldev->states[band].color_usage.trans_bbox.q.y = q_y;
        if (cldev->states[band].color_usage.trans_bbox.q.x < bbox->q.x)
            cldev->states[band].color_usage.trans_bbox.q.x = bbox->q.x;
    }
}

/* Write the commands for one band or band range. */
static int	/* ret 0 all ok, -ve error code, or +1 ok w/low-mem warning */
cmd_write_band(gx_device_clist_writer * cldev, int band_min, int band_max,
               cmd_list * pcl, byte cmd_end)
{
    const cmd_prefix *cp = pcl->head;
    int code_b = 0;
    int code_c = 0;

    if (cp != 0 || cmd_end != cmd_opv_end_run) {
        clist_file_ptr cfile = cldev->page_info.cfile;
        clist_file_ptr bfile = cldev->page_info.bfile;
        cmd_block cb;
        byte end;

        if (cfile == 0 || bfile == 0)
            return_error(gs_error_ioerror);
        cb.band_min = band_min;
        cb.band_max = band_max;
        cb.pos = cldev->page_info.io_procs->ftell(cfile);
        if_debug3m('l', cldev->memory, "[l]writing for bands (%d,%d) at %"PRId64"\n",
                  band_min, band_max, cb.pos);
        cldev->page_info.io_procs->fwrite_chars(&cb, sizeof(cb), bfile);
        if (cp != 0) {
            pcl->tail->next = 0;	/* terminate the list */
            for (; cp != 0; cp = cp->next) {
#ifdef DEBUG
                if ((const byte *)cp < cldev->cbuf ||
                    (const byte *)cp >= cldev->cend ||
                    cp->size > cldev->cend - (const byte *)cp
                    ) {
                    mlprintf1(cldev->memory, "cmd_write_band error at "PRI_INTPTR"\n", (intptr_t) cp);
                    return_error(gs_error_Fatal);
                }
#endif
                if_debug2m('L', cldev->memory, "[L] cmd id=%ld at %"PRId64"\n",
                           cp->id, cldev->page_info.io_procs->ftell(cfile));
                cldev->page_info.io_procs->fwrite_chars(cp + 1, cp->size, cfile);
            }
            pcl->head = pcl->tail = 0;
        }
        if_debug0m('L', cldev->memory, "[L] adding terminator\n");
        end  = cmd_count_op(cmd_end, 1, cldev->memory);
        cldev->page_info.io_procs->fwrite_chars(&end, 1, cfile);
        process_interrupts(cldev->memory);
        code_b = cldev->page_info.io_procs->ferror_code(bfile);
        code_c = cldev->page_info.io_procs->ferror_code(cfile);
        if (code_b < 0)
            return_error(code_b);
        if (code_c < 0)
            return_error(code_c);
    }
    return code_b | code_c;
}

/* Write out a pseudo-band block of data, using the specific pseudo_band_offset */
int
cmd_write_pseudo_band(gx_device_clist_writer * cldev, unsigned char *pbuf, int data_size, int pseudo_band_offset)
{

    /* Data is written out maxband + pseudo_band_offset */

    int band = cldev->band_range_max + pseudo_band_offset;
    clist_file_ptr cfile = cldev->page_info.cfile;
    clist_file_ptr bfile = cldev->page_info.bfile;
    cmd_block cb;
    int code_b, code_c;

    if (cfile == 0 || bfile == 0)
        return_error(gs_error_ioerror);

    /* Set up the command block information that
       is stored in the bfile. */

    cb.band_min = band;
    cb.band_max = band;
    cb.pos = cldev->page_info.io_procs->ftell(cfile);

    if_debug2m('l', cldev->memory, "[l]writing pseudo band %d cb pos %"PRId64"\n",
                  band, cb.pos);

    cldev->page_info.io_procs->fwrite_chars(&cb, sizeof(cb), bfile);

    /* Now store the information in the cfile */
    if_debug2m('l', cldev->memory, "[l]writing %d bytes into cfile at %"PRId64"\n",
            data_size, cldev->page_info.io_procs->ftell(cfile));

    cldev->page_info.io_procs->fwrite_chars(pbuf, data_size, cfile);

    process_interrupts(cldev->memory);
    code_b = cldev->page_info.io_procs->ferror_code(bfile);
    code_c = cldev->page_info.io_procs->ferror_code(cfile);

    if (code_b < 0)
        return_error(code_b);
    if (code_c < 0)
        return_error(code_c);

    return code_b | code_c;
}

/* Write out the buffered commands, and reset the buffer. */
int	/* ret 0 all-ok, -ve error code, or +1 ok w/low-mem warning */
cmd_write_buffer(gx_device_clist_writer * cldev, byte cmd_end)
{
    int nbands = cldev->nbands;
    gx_clist_state *pcls;
    int band;
    int code = cmd_write_band(cldev, cldev->band_range_min,
                              cldev->band_range_max,
                              cldev->band_range_list,
                              cmd_opv_end_run);

    int warning = code;

    for (band = 0, pcls = cldev->states;
         code >= 0 && band < nbands; band++, pcls++
         ) {
        code = cmd_write_band(cldev, band, band, &pcls->list, cmd_end);
        warning |= code;
    }
    /* If an error occurred, finish cleaning up the pointers. */
    for (; band < nbands; band++, pcls++)
        pcls->list.head = pcls->list.tail = 0;
    cldev->cnext = cldev->cbuf;
#ifdef HAVE_VALGRIND
    VALGRIND_MAKE_MEM_UNDEFINED(cldev->cbuf, cldev->cend - cldev->cbuf);
#endif
    cldev->ccl = 0;
#ifdef COLLECT_STATS_CLIST
    if (gs_debug_c('l'))
        cmd_print_stats(cldev->memory);
#endif
    return_check_interrupt(cldev->memory, code != 0 ? code : warning);
}

/*
 * Add a command to the appropriate band list, and allocate space for its
 * data.  Return the pointer to the data area.  If an error or (low-memory
 * warning) occurs, set cldev->error_code and return 0.
 */
#define cmd_headroom (sizeof(cmd_prefix) + ARCH_ALIGN_PTR_MOD)
byte *
cmd_put_list_op(gx_device_clist_writer * cldev, cmd_list * pcl, uint size)
{
    byte *dp = cldev->cnext;

    CMD_CHECK_LAST_OP_BLOCK_DEFINED(cldev);

    if (size + cmd_headroom > cldev->cend - dp) {
        cldev->error_code = cmd_write_buffer(cldev, cmd_opv_end_run);
        /* error_code can come back as +ve as a warning that memory
         * is getting tight. Don't fail on that. */
        if (cldev->error_code < 0 ||
            (size + cmd_headroom > cldev->cend - cldev->cnext)) {
            if (cldev->error_code == 0)
                cldev->error_code = gs_error_VMerror;
            return 0;
        }
        else
            return cmd_put_list_op(cldev, pcl, size);
    }
    if (cldev->ccl == pcl) {	/* We're adding another command for the same band. */
        /* Tack it onto the end of the previous one. */
        cmd_count_add1(stats_cmd.same_band);
#ifdef DEBUG
        if (pcl->tail->size > dp - (byte *) (pcl->tail + 1)) {
            lprintf1("cmd_put_list_op error at "PRI_INTPTR"\n", (intptr_t)pcl->tail);
        }
#endif
        if_debug2m('L', cldev->memory, "[L] id:%ld+%ld",
                   pcl->tail->id, (long)pcl->tail->size);
        pcl->tail->size += size;
    } else {
        /* Skip to an appropriate alignment boundary. */
        /* (We assume the command buffer itself is aligned.) */
        cmd_prefix *cp = (cmd_prefix *)
            (dp + ((cldev->cbuf - dp) & (ARCH_ALIGN_PTR_MOD - 1)));

        cp->id = cldev->ins_count++;
#ifdef DEBUG
        if (gs_debug_c('L'))
        {
            if (pcl == cldev->band_range_list)
                dmlprintf2(cldev->memory, "[L]Change to bands=(%d->%d)", cldev->band_range_min, cldev->band_range_max);
            else
                dmlprintf1(cldev->memory, "[L]Change to band=%d",
                           (int)(((intptr_t)pcl-(intptr_t)&cldev->states->list)/sizeof(*cldev->states)));

            dmlprintf2(cldev->memory, ", align=%d\n[L] id:%ld+0",
                       (int)((char *)cp-(char *)dp), cp->id);
        }
#endif

        cmd_count_add1(stats_cmd.other_band);
        dp = (byte *) (cp + 1);
        if (pcl->tail != 0) {
#ifdef DEBUG
            if (pcl->tail < pcl->head ||
                pcl->tail->size > dp - (byte *) (pcl->tail + 1)
                ) {
                lprintf1("cmd_put_list_op error at "PRI_INTPTR"\n",
                         (intptr_t)pcl->tail);
            }
#endif
            pcl->tail->next = cp;
        } else
            pcl->head = cp;
        pcl->tail = cp;
        cldev->ccl = pcl;
        cp->size = size;
    }
    cldev->cnext = dp + size;
    return dp;
}

byte *
cmd_put_list_extended_op(gx_device_clist_writer *cldev, cmd_list *pcl, int op, uint size)
{
    byte *dp = cmd_put_list_op(cldev, pcl, size);

    if (dp) {
        dp[1] = op;

        if (gs_debug_c('L')) {
            clist_debug_op(cldev->memory, dp);
            dmlprintf1(cldev->memory, "[%u]\n", size);
        }
    }

    return dp;
}

/* Request a space in the buffer.
   Writes out the buffer if necessary.
   Returns the size of available space. */
int
cmd_get_buffer_space(gx_device_clist_writer * cldev, gx_clist_state * pcls, uint size)
{
    size_t z;
    CMD_CHECK_LAST_OP_BLOCK_DEFINED(cldev);

    if (size + cmd_headroom > cldev->cend - cldev->cnext) {
        /* error_code can come back as +ve as a warning that memory
         * is getting tight. Don't fail on that. */
        cldev->error_code = cmd_write_buffer(cldev, cmd_opv_end_run);
        if (cldev->error_code < 0) {
            return cldev->error_code;
        }
    }
    /* Calculate the available size as a size_t. If this won't fit in
     * an int, clip the value. This is a bit crap, but it should be
     * safe at least until we can change the clist to use size_t's
     * where appropriate. */
    z = cldev->cend - cldev->cnext - cmd_headroom;
    if (z > INT_MAX)
        z = INT_MAX;
    return z;
}

#ifdef DEBUG
byte *
cmd_put_op(gx_device_clist_writer * cldev, gx_clist_state * pcls, uint size)
{
    return cmd_put_list_op(cldev, &pcls->list, size);
}
#endif

/* Add a command for a range of bands. */
byte *
cmd_put_range_op(gx_device_clist_writer * cldev, int band_min, int band_max,
                 uint size)
{
    CMD_CHECK_LAST_OP_BLOCK_DEFINED(cldev);

    if (cldev->ccl != 0 &&
        (cldev->ccl != cldev->band_range_list ||
         band_min != cldev->band_range_min ||
         band_max != cldev->band_range_max)
        ) {
        cldev->error_code = cmd_write_buffer(cldev, cmd_opv_end_run);
        /* error_code can come back as +ve as a warning that memory
         * is getting tight. Don't fail on that. */
        if (cldev->error_code < 0) {
            return NULL;
        }
        cldev->band_range_min = band_min;
        cldev->band_range_max = band_max;
        if_debug2m('L', cldev->memory, "[L]Band range(%d,%d)\n",
                   band_min, band_max);
    }
    return cmd_put_list_op(cldev, cldev->band_range_list, size);
}

/* Write a variable-size positive integer. */
int
cmd_size_w(register uint w)
{
    register int size = 1;

    while (w > 0x7f)
        w >>= 7, size++;
    return size;
}
byte *
cmd_put_w(register uint w, register byte * dp)
{
    while (w > 0x7f)
        *dp++ = w | 0x80, w >>= 7;
    *dp = w;
    return dp + 1;
}
/* Write a variable-size positive fractional. */
int
cmd_size_frac31(register frac31 w)
{
    register int size = 1;
    register uint32_t v = w;

    while (v & 0x01FFFFFF)
        v <<= 7, size++;
    return size;
}
byte *
cmd_put_frac31(register frac31 w, register byte * dp)
{
    register uint32_t v = w;

    while (v & 0x01FFFFFF)
        *dp++ = (v >> 24) | 1, v <<= 7;
    *dp = (v >> 24);
    return dp + 1;
}

/*
 * This next two arrays are used for the 'delta' mode of placing a color
 * in the clist.  These arrays are indexed by the number of bytes in the
 * color value (the depth).
 *
 * Delta values are calculated by subtracting the old value for the color
 * from the desired new value.  Then each byte of the differenece is
 * examined.  For most bytes, if the difference fits into 4 bits (signed)
 * then those bits are packed into the clist along with an opcode.  If
 * the size of the color (the depth) is an odd number of bytes then instead
 * of four bits per byte, extra bits are used for the upper three bytes
 * of the color.  In this case, five bits are used for the first byte,
 * six bits for the second byte, and five bits for third byte.  This
 * maximizes the chance that the 'delta' mode can be used for placing
 * colors in the clist.
 */
/*
 * Depending upon the compiler and user choices, the size of a gx_color_index
 * may be 4 to 8 bytes.  We will define table entries for up to 8 bytes.
 * This macro is being used to prevent compiler warnings if gx_color_index is
 * only 4 bytes.
 */
#define tab_entry(x) ((x) & (~((gx_color_index) 0)))

const gx_color_index cmd_delta_offsets[] = {
        tab_entry(0),
        tab_entry(0),
        tab_entry(0x0808),
        tab_entry(0x102010),
        tab_entry(0x08080808)
#if ARCH_SIZEOF_GX_COLOR_INDEX > 4
        ,
        tab_entry(0x1020100808),
        tab_entry(0x080808080808),
        tab_entry(0x10201008080808),
        tab_entry(0x0808080808080808)
#endif
        };

static const gx_color_index cmd_delta_masks[] = {
        tab_entry(0),
        tab_entry(0),
        tab_entry(0x0f0f),
        tab_entry(0x1f3f1f),
        tab_entry(0x0f0f0f0f)
#if ARCH_SIZEOF_GX_COLOR_INDEX > 4
        ,
        tab_entry(0x1f3f1f0f0f),
        tab_entry(0x0f0f0f0f0f0f),
        tab_entry(0x1f3f1f0f0f0f0f),
        tab_entry(0x0f0f0f0f0f0f0f0f)
#endif
        };

#undef tab_entry

/*
 * There are currently only four different color "types" that can be placed
 * into the clist.  These are called "color0", "color1", and "tile_color0",
 * and "tile_color1".  There are separate command codes for color0 versus
 * color1, both for the full value and delta commands - see cmd_put_color.
 * Tile colors are preceded by a cmd_opv_set_tile_color command.
 */
const clist_select_color_t
    clist_select_color0 = {cmd_op_set_color0, cmd_opv_delta_color0, 0},
    clist_select_color1 = {cmd_op_set_color1, cmd_opv_delta_color1, 0},
    clist_select_tile_color0 = {cmd_op_set_color0, cmd_opv_delta_color0, 1},
    clist_select_tile_color1 = {cmd_op_set_color1, cmd_opv_delta_color1, 1};

/*
 * This routine is used to place a color into the clist.  Colors, in the
 * clist, can be specified either as by a full value or by a "delta" value.
 *
 * See the comments before cmd_delta_offsets[] for a description of the
 * 'delta' mode.  The delta mode may allow for a smaller command in the clist.
 *
 * For the full value mode, values are sent as a cmd code plus n bytes of
 * data.  To minimize the number of bytes, a count is made of any low order
 * bytes which are zero.  This count is packed into the low order 4 bits
 * of the cmd code.  The data for these bytes are not sent.
 *
 * The gx_no_color_index value is treated as a special case.  This is done
 * because it is both a commonly sent value and because it may require
 * more bytes then the other color values.
 *
 * Parameters:
 *   cldev - Pointer to clist device
 *   pcls - Pointer to clist state
 *   select - Descriptor record for type of color being sent.  See comments
 *       by clist_select_color_t.
 *   color - The new color value.
 *   pcolor - Pointer to previous color value.  (If the color value is the
 *       same as the previous value then nothing is placed into the clist.)
 *
 * Returns:
 *   Error code
 *   clist and pcls and cldev may be updated.
 */
int
cmd_put_color(gx_device_clist_writer * cldev, gx_clist_state * pcls,
              const clist_select_color_t * select,
              gx_color_index color, gx_color_index * pcolor)
{
    byte * dp;		/* This is manipulated by the set_cmd_put_op macro */
    gx_color_index diff = color - *pcolor;
    byte op, op_delta;
    int code;

    if (diff == 0)
        return 0;

    /* If this is a tile color then send tile color type */
    if (select->tile_color) {
        code = set_cmd_put_op(&dp, cldev, pcls, cmd_opv_set_tile_color, 1);
        if (code < 0)
            return code;
    }
    op = select->set_op;
    op_delta = select->delta_op;
    if (color == gx_no_color_index) {
        /*
         * We must handle this specially, because it may take more
         * bytes than the color depth.
         */
        code = set_cmd_put_op(&dp, cldev, pcls, op + cmd_no_color_index, 1);
        if (code < 0)
            return code;
    } else {
        /* Check if the "delta" mode command can be used. */
        /* clist_color_info may be different than target device due to
         * transparency group during clist writing phase */
        int depth = (cldev->clist_color_info.depth <= sizeof(gx_color_index)*8 ?
                     cldev->clist_color_info.depth : sizeof(gx_color_index)*8);
        int num_bytes = (depth + 7) >> 3;
        int delta_bytes = (num_bytes + 1) / 2;
        gx_color_index delta_offset = cmd_delta_offsets[num_bytes];
        gx_color_index delta_mask = cmd_delta_masks[num_bytes];
        gx_color_index delta = (diff + delta_offset) & delta_mask;
        bool use_delta = (color == (*pcolor + delta - delta_offset));
        int bytes_dropped = 0;
        gx_color_index data = color;

        /*
         * If we use the full value mode, we do not send low order bytes
         * which are zero. Determine how many low order bytes are zero.
         */
        if (color == 0) {
            bytes_dropped = num_bytes;
        }
        else  {
            while ((data & 0xff) == 0) {
                bytes_dropped++;
                data >>= 8;
            }
        }
        /* Now send one of the two command forms */
        if (use_delta && delta_bytes < (num_bytes - bytes_dropped)) {
            code = set_cmd_put_op(&dp, cldev, pcls,
                                        op_delta, delta_bytes + 1);
            if (code < 0)
                return code;
            /*
             * If we have an odd number of bytes then use extra bits for
             * the high order three bytes of the color.
             */
            if ((num_bytes >= 3) && (num_bytes & 1)) {
                data = delta >> ((num_bytes - 3) * 8);
                dp[delta_bytes--] = (byte)(((data >> 13) & 0xf8) + ((data >> 11) & 0x07));
                dp[delta_bytes--] = (byte)(((data >> 3) & 0xe0) + (data & 0x1f));
            }
            for(; delta_bytes>0; delta_bytes--) {
                dp[delta_bytes] = (byte)((delta >> 4) + delta);
                delta >>= 16;
            }
        }
        else {
            num_bytes -= bytes_dropped;
            code = set_cmd_put_op(&dp, cldev, pcls,
                                (byte)(op + bytes_dropped), num_bytes + 1);
            if (code < 0)
                return code;
            for(; num_bytes>0; num_bytes--) {
                dp[num_bytes] = (byte)data;
                data >>= 8;
            }
        }
    }
    *pcolor = color;
    return 0;
}

/* Put out a command to set the tile colors. */
int
cmd_set_tile_colors(gx_device_clist_writer * cldev, gx_clist_state * pcls,
                    gx_color_index color0, gx_color_index color1)
{
    int code = 0;

    if (color0 != pcls->tile_colors[0]) {
        code = cmd_put_color(cldev, pcls,
                             &clist_select_tile_color0,
                             color0, &pcls->tile_colors[0]);
        if (code != 0)
            return code;
    }
    if (color1 != pcls->tile_colors[1])
        code = cmd_put_color(cldev, pcls,
                             &clist_select_tile_color1,
                             color1, &pcls->tile_colors[1]);
    return code;
}

/* Put out a command to set the tile phase. */
int
cmd_set_tile_phase_generic(gx_device_clist_writer * cldev, gx_clist_state * pcls,
                   int px, int py, bool all_bands)
{
    int pcsize;
    byte *dp;
    int code;

    pcsize = 1 + cmd_size2w(px, py);
    if (all_bands)
        code = set_cmd_put_all_op(&dp, cldev, (byte)cmd_opv_set_tile_phase, pcsize);
    else
        code = set_cmd_put_op(&dp, cldev, pcls, (byte)cmd_opv_set_tile_phase, pcsize);
    if (code < 0)
        return code;
    ++dp;
    pcls->tile_phase.x = px;
    pcls->tile_phase.y = py;
    cmd_putxy(pcls->tile_phase, &dp);
    return 0;
}

int
cmd_set_tile_phase(gx_device_clist_writer * cldev, gx_clist_state * pcls,
                   int px, int py)
{
    return cmd_set_tile_phase_generic(cldev, pcls, px, py, false);
}

int
cmd_set_screen_phase_generic(gx_device_clist_writer * cldev, gx_clist_state * pcls,
                             int px, int py, gs_color_select_t color_select,
                             bool all_bands)
{
    int pcsize;
    byte *dp;
    int code;

    pcsize = 1 + cmd_size2w(px, py);
    if (all_bands)
        code = set_cmd_put_all_op(&dp, cldev, (byte)cmd_opv_set_screen_phaseT + color_select, pcsize);
    else
        code = set_cmd_put_op(&dp, cldev, pcls, (byte)cmd_opv_set_screen_phaseT + color_select, pcsize);
    if (code < 0)
        return code;
    ++dp;
    pcls->screen_phase[color_select].x = px;
    pcls->screen_phase[color_select].y = py;
    cmd_putxy(pcls->screen_phase[color_select], &dp);
    return 0;
}

int
cmd_set_screen_phase(gx_device_clist_writer * cldev, gx_clist_state * pcls,
                     int px, int py, gs_color_select_t color_select)
{
    return cmd_set_screen_phase_generic(cldev, pcls, px, py, color_select, false);
}

/* Write a command to enable or disable the logical operation. */
int
cmd_put_enable_lop(gx_device_clist_writer * cldev, gx_clist_state * pcls,
                   int enable)
{
    byte *dp;
    int code = set_cmd_put_op(&dp, cldev, pcls,
                              (byte)(enable ? cmd_opv_enable_lop :
                                     cmd_opv_disable_lop),
                              1);

    if (code < 0)
        return code;
    pcls->lop_enabled = enable;
    return 0;
}

/* Write a command to enable or disable clipping. */
/* This routine is only called if the path extensions are included. */
int
cmd_put_enable_clip(gx_device_clist_writer * cldev, gx_clist_state * pcls,
                    int enable)
{
    byte *dp;
    int code = set_cmd_put_op(&dp, cldev, pcls,
                              (byte)(enable ? cmd_opv_enable_clip :
                                     cmd_opv_disable_clip),
                              1);

    if (code < 0)
        return code;
    pcls->clip_enabled = enable;
    return 0;
}

/* Write a command to set the logical operation. */
int
cmd_set_lop(gx_device_clist_writer * cldev, gx_clist_state * pcls,
            gs_logical_operation_t lop)
{
    byte *dp;
    uint lop_msb = lop >> 6;
    int code = set_cmd_put_op(&dp, cldev, pcls,
                              cmd_opv_set_misc, 2 + cmd_size_w(lop_msb));

    if (code < 0)
        return code;
    dp[1] = cmd_set_misc_lop + (lop & 0x3f);
    cmd_put_w(lop_msb, dp + 2);
    pcls->lop = lop;
    return 0;
}

/* Disable (if default) or enable the logical operation, setting it if */
/* needed. */
int
cmd_update_lop(gx_device_clist_writer *cldev, gx_clist_state *pcls,
               gs_logical_operation_t lop)
{
    int code;

    if (lop == lop_default)
        return cmd_disable_lop(cldev, pcls);
    code = cmd_set_lop(cldev, pcls, lop);
    if (code < 0)
        return code;
    return cmd_enable_lop(cldev, pcls);
}

/* Write a parameter list */
int	/* ret 0 all ok, -ve error */
cmd_put_params(gx_device_clist_writer *cldev,
               gs_param_list *param_list) /* NB open for READ */
{
    byte *dp;
    int code;
    byte local_buf[512];	/* arbitrary */
    int param_length;

    /* Get serialized list's length + try to get it into local var if it fits. */
    param_length = code =
        gs_param_list_serialize(param_list, local_buf, sizeof(local_buf));
    if (param_length > 0) {
        /* Get cmd buffer space for serialized */
        code = set_cmd_put_all_extended_op(&dp, cldev, cmd_opv_ext_put_params,
                                  2 + sizeof(unsigned) + param_length);
        if (code < 0)
            return code;

        /* write param list to cmd list: needs to all fit in cmd buffer */
        if_debug1m('l', cldev->memory, "[l]put_params, length=%d\n", param_length);
        dp += 2;
        memcpy(dp, &param_length, sizeof(unsigned));
        dp += sizeof(unsigned);
        if (param_length > sizeof(local_buf)) {
            int old_param_length = param_length;

            param_length = code =
                gs_param_list_serialize(param_list, dp, old_param_length);
            if (param_length >= 0)
                code = (old_param_length != param_length ?
                        gs_note_error(gs_error_unknownerror) : 0);
            if (code < 0) {
                /* error serializing: back out by writing a 0-length parm list */
                memset(dp - sizeof(unsigned), 0, sizeof(unsigned));
                cmd_shorten_list_op(cldev, cldev->band_range_list,
                                    old_param_length);
            }
        } else
            memcpy(dp, local_buf, param_length);	    /* did this when computing length */
    }
    return code;
}

/* Initialize CCITTFax filters. */
static void
clist_cf_init(stream_CF_state *ss, int width)
{
    ss->K = -1;
    ss->Columns = width;
#if 0 /* Disabled due to a crash with ppmraw -r216 c327.bin :
         the decoding filter overruns in 1 byte.
       */
    ss->EndOfBlock = false;
#else
    ss->EndOfBlock = true;
#endif
    ss->BlackIs1 = true;
    ss->DecodedByteAlign = align_bitmap_mod;
}
void
clist_cfe_init(stream_CFE_state *ss, int width, gs_memory_t *mem)
{
    s_init_state((stream_state *)ss, &s_CFE_template, mem);
    s_CFE_set_defaults_inline(ss);
    clist_cf_init((stream_CF_state *)ss, width);
    s_CFE_template.init((stream_state *)(ss));
}
void
clist_cfd_init(stream_CFD_state *ss, int width, int height, gs_memory_t *mem)
{
    s_init_state((stream_state *)ss, &s_CFD_template, mem);
    s_CFD_template.set_defaults((stream_state *)ss);
    clist_cf_init((stream_CF_state *)ss, width);
    ss->Rows = height;
    s_CFD_template.init((stream_state *)(ss));
}

/* Initialize RunLength filters. */
void
clist_rle_init(stream_RLE_state *ss)
{
    s_init_state((stream_state *)ss, &s_RLE_template, (gs_memory_t *)0);
    ss->templat->set_defaults((stream_state *)ss);
    ss->templat->init((stream_state *)ss);
}
void
clist_rld_init(stream_RLD_state *ss)
{
    s_init_state((stream_state *)ss, &s_RLD_template, (gs_memory_t *)0);
    ss->templat->set_defaults((stream_state *)ss);
    ss->templat->init((stream_state *)ss);
}

/* Read a transformation matrix. */
const byte *
cmd_read_matrix(gs_matrix * pmat, const byte * cbp)
{
    stream s;

    s_init(&s, NULL);
    sread_string(&s, cbp, 1 + sizeof(*pmat));
    sget_matrix(&s, pmat);
    return cbp + stell(&s);
}

/*
Some notes on understanding the output of -ZL.

The examples here are given from:
  gs -o out.png -r96 -sDEVICE=png16m -dBandHeight=20 -dMaxBitmap=1000 -ZL examples/tiger.eps

Not every line in that output is explained here!

When writing a command list, we gather up a list of 'commands' into the
clist (cfile). We then have a series of indexes that says which of these
commands is needed for each band (bfile).

So, while writing, we can be writing for 1 band, or for a range of bands
at any given time. Commands that follow one another for the same band
(or range of bands) will be crammed together into a single command block.
These command blocks are each given an id for debugging purposes. When
the set of bands for which we are writing changes, the id changes.

Somewhere towards the top of the output (i.e. within a
hundred lines or so) you should see:

  [L]Resetting: Band range(0,56)

So, we are writing some commands that will apply to bands 0 to 56.

  [L] id:0+0, put_fill_dcolor(13)
  [L] id:0+13, fill_rect 0(5)

So, for id 0, at 0 bytes offset, we first have a put_fill_dcolor command
that takes 13 bytes. Then, still in id 0, at 13 bytes offset, we have
a fill_rect that takes 5 bytes.

Then we change the band:

  [L]Change to band=0, align=6

When we change the band, we change to a new command block, and the id
changes - so you'll see the subsequent entries listed with id 1.
Subsequent command blocks are aligned, so you'll see some alignment
(padding) bytes used - here 6 bytes.

  [L] id:1+0, set_misc2(6)
  [L] id:1+6, begin_clip(1)
  [L] id:1+7, fill_rect 0(7)

Here we see various commands, each for id 1, at the expected offsets
given their respective sizes. Then we get some debugging from elsewhere
in the clist system.

[L]  r6:0,793,0,1123

This indicates details about the fill_rect (in particular the way
the fill_rect is encoded, and the parameters it uses). Such lines can
be differentiated fairly easily from the command block writing code
as they do not start with 'id:'.

We continue with more commands:

  [L] id:1+14, end_clip(1)
  [L] id:1+15, put_fill_dcolor(13)
  [L]  rmoveto:0: 0 0
  [L] id:1+28, rmoveto(5)
  [L]  rlineto:0: 0 1123
  [L] id:1+33, vlineto(4)
  [L]  rlineto:0: 793 0
  [L] id:1+37, hlineto(3)
  [L]  rlineto:0: 0 -1123
  [L] id:1+40, vlineto(4)
  [L]  closepath:0:
  [L] id:1+44, closepath(1)
  [L] id:1+45, fill(1)

Here we note a couple of things. The clist command encoding system
works by first reserving the required number of bytes for a command,
then filling in those bytes. Because lots of parameters vary in length
according to their particular value, we often have to do a lot of the
encoding work twice; once to count how many bytes we need to reserve
and then once to fill in the block.

The command buffer debug lines (i.e. the ones starting 'id:') are output
at the point the buffer is reserved. Other debug lines for the same
command can happen either before or after these lines. So the 'r6' line
happened after the command reservation that it corresponded to, whereas
the 'rmoveto' (and others) above happen before the command reservation.
This can be confusing.

Another confusing thing is that the commands can appear to change. The
non-command block debug above mentions 4 rlineto's, but these all
appear in the command list as vlineto or hlineto. This is because
the command block queueing attempts to be smart and to simplify the
sequence of commands. This can mean pulling a command into a previous
one, or (as in this case) realising that a simpler encoding can be
used.

And we continue...

  [L]Change to band=1, align=2
  [L] id:2+0, set_misc2(6)
  [L] id:2+6, begin_clip(1)

After a while, we move to an output phase where things are actually
written to the file. These come in groups like:

  [l]writing for bands (0,56) at 0
  [L] cmd id=0 at 0
  [L] adding terminator, end_run(1)

So this is writing out a note that bands 0 to 56 should execute the following
id's. We then write out the id's in question (id 0, goes into cfile at offset 0).
This is then terminated by a single byte 'end_run' marker.

This repeats, with the file offsets increasing as we go. Some cases have more
than one id, for instance:

  [l]writing for bands (7,7) at 640
  [L] cmd id=8 at 640
  [L] cmd id=194 at 685
  [L] cmd id=215 at 785
  [L] cmd id=712 at 928
  [L] cmd id=720 at 969
  [L] cmd id=726 at 986
  [L] cmd id=732 at 1016
  [L] cmd id=809 at 1046
  [L] cmd id=817 at 1185
  [L] cmd id=822 at 1258
  [L] adding terminator, end_page(1)

So, by matching up the id's in this section, together with their offsets,
we can find out what command was written there.

For instance, suppose we hit a problem when reading the cfile at offset 1029.
We can look to see that this is id=732 + 13 bytes. We can look back in the
output to where id:732 was being output, and we see:

  [L] id:732+13, rmoveto(5)

Most clist bugs tend to involve the reader and writer disagreeing on how
many bytes a given command should be and getting out of step. By looking at
where the writer puts stuff, and the reader is trying to read stuff, we can
hopefully spot this.

The writing phase ends with:

  [l]writing pseudo band 57 cb pos 92521
  [l]writing 1824 bytes into cfile at 92521
  [l]writing end for bands (-1,-1) at 94345

FIXME: Explain the pseudo band.

The next section of the logging shows the reader reading. For each band
in turn, we'll see a section where we announce what band we are
rendering:

  [l]rendering bands (0,0)

Then we will read through the different band records that were output
above.

  [l]reading for bands (0,0) at bfile 0, cfile 0, length 0
  [l]reading for bands (0,56) at bfile 16, cfile 0, length 19
  [l]reading for bands (0,0) at bfile 32, cfile 19, length 47

If we look back, we can see that the first of these corresponded to an
empty record. The second of these corresponded to the write of
"cmd id=0 at 0", and the third corresponds to the write of
"cmd id=1 at 19".

When these records have been read in, we actually execute the data. Each
line gives the offset from which the command was read (which allows us
to track it back to what it *should* be in the case of a mismatch),
and is followed by the command name, and a selection of its parameters:

  [L] 0: put_fill_dcolor cmd_opv_ext_put_drawing_color
  [L] 13: fill_rect 0 x=0 y=0 w=0 h=0
  [L] 18: end_run
  [L] 19: set_misc2
  [L]      CJ=-1 AC=1 SA=1
  [L]      BM=0 TK=1 OPM=0 OP=0 op=0 RI=1
  [L] 25: begin_clip
  [L] 26: fill_rect 0 x=0 y=0 w=793 h=1123
  [L] 33: end_clip
  [L] 34: put_fill_dcolor cmd_opv_ext_put_drawing_color
  [L] 47: rmoveto (0,0) 0 0
  [L] 52: vlineto 1123
  [L] 56: hlineto 793
  [L] 59: vlineto -1123
  [L] 63: closepath
  [L] 64: fill
  [L] 65: end_page

Then we repeat gathering the data for the next band:

  [l]rendering bands (1,1)
  [l]reading for bands (0,56) at bfile 16, cfile 0, length 19
  [l]reading for bands (1,1) at bfile 48, cfile 66, length 46

and so on.

*/
