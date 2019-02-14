/* Copyright (C) 2001-2019 Artifex Software, Inc.
   All Rights Reserved.

   This software is provided AS-IS with no warranty, either express or
   implied.

   This software is distributed under license and may not be copied,
   modified or distributed except as expressly authorized under the terms
   of the license contained in the file LICENSE in this distribution.

   Refer to licensing information at http://www.artifex.com or contact
   Artifex Software, Inc.,  1305 Grant Avenue - Suite 200, Novato,
   CA 94945, U.S.A., +1(415)492-9861, for further information.
*/


/* pximage.c */
/* PCL XL bitmap painting operators */

#include "std.h"
#include "string_.h"
#include "pxerrors.h"
#include "pxoper.h"
#include "pxstate.h"
#include "gdebug.h"
#include "gsrop.h"
#include "gsrefct.h"
#include "gsstruct.h"
#include "gsstate.h"
#include "gscoord.h"
#include "gsimage.h"
#include "gspaint.h"
#include "gspath.h"
#include "gspath2.h"
#include "gsuid.h"              /* for gxpcolor.h */
#include "gsutil.h"
#include "gxbitmap.h"
#include "gxcspace.h"
#include "gxdevice.h"           /* for gxpcolor.h */
#include "gxcolor2.h"
#include "gxdcolor.h"
#include "gxpcolor.h"
#include "scommon.h"
#include "strimpl.h"
#include "srlx.h"
#include "pldraw.h"
#include "jpeglib_.h"           /* for jpeg filter */
#include "sdct.h"
#include "sjpeg.h"
#include "pxptable.h"

/* Define the "freeing" procedure for patterns in a dictionary. */
void
px_free_pattern(gs_memory_t * mem, void *vptr, client_name_t cname)
{
    px_pattern_t *pattern = vptr;

    rc_decrement(pattern, cname);
}
/* Define the real freeing procedure for patterns. */
static void
rc_free_px_pattern(gs_memory_t * mem, void *vptr, client_name_t cname)
{
    px_pattern_t *pattern = vptr;

    gs_free_string(mem, (void *)pattern->palette.data, pattern->palette.size,
                   cname);
    gs_free_object(mem, pattern->data, cname);
    gs_free_object(mem, pattern, cname);
}
/* Define the purging procedure for the Pattern cache. */
/* The proc_data points to a pxePatternPersistence_t that specifies */
/* the maximum persistence level to purge. */
static bool
px_pattern_purge_proc(gx_color_tile * ctile, void *proc_data)
{
    return ctile->uid.id % pxePatternPersistence_next <=
        *(pxePatternPersistence_t *) proc_data;
}
void
px_purge_pattern_cache(px_state_t * pxs, pxePatternPersistence_t max_persist)
{
    gx_pattern_cache_winnow(gstate_pattern_cache(pxs->pgs),
                            px_pattern_purge_proc, (void *)&max_persist);
}

/* active decompression types */
typedef enum
{
    unset  = 0,
    nocomp = 1,
    rle    = 2,
    jpeg   = 4,
    delta  = 8,
} decomp_init_t;

static inline bool nocomp_inited(uint comp)  {return comp & nocomp;}
static inline bool rle_inited(uint comp)     {return comp & rle;}
static inline bool jpeg_inited(uint comp)    {return comp & jpeg;}
static inline bool delta_inited(uint comp)   {return comp & delta;}

static inline void nocomp_set(uint *comp) {*comp |= nocomp;}
static inline void rle_set(uint *comp)    {*comp |= rle;}
static inline void jpeg_set(uint *comp)   {*comp |= jpeg;}
static inline void delta_set(uint *comp)  {*comp |= delta;}

static inline void comp_unset(uint *comp)  {*comp = unset;}

/* pxl delta row decompression state machine states */
typedef enum
{
    next_is_bytecount,
    partial_bytecount,
    next_is_cmd,
    partial_offset,
    partial_cnt
} deltarow_parse_state_t;

/* pxl delta row decompression state storage */
typedef struct deltarow_state_s
{
    deltarow_parse_state_t state;
    uint row_byte_count;
    uint short_cnt;
    uint short_offset;
    byte *seedrow;
    uint rowwritten;
} deltarow_state_t;

/* Define the structure for enumerating a bitmap being downloaded. */
typedef struct px_bitmap_enum_s
{
    gs_memory_t *mem;           /* used only for the jpeg filter */
    uint data_per_row;          /* ditto minus possible trailing padding */
    uint initialized;
    stream_RLD_state rld_stream_state;  /* decompressor states */
    stream_DCT_state dct_stream_state;
    jpeg_decompress_data jdd;
    deltarow_state_t deltarow_state;

    /* Entries to deal with 'rebuffering' - the idea of making
     * input image data appear a different (but equivalent) shape.
     * Currently just morphing '1xH' bitmaps into 'Hx1' ones. */
    uint rebuffered_data_per_row; /* equal to data_per_row except when flipping */
    bool rebuffered;
    uint rebuffered_width;
    uint rebuffered_height;
    int rebuffered_row_pos;
    bool grayscale;
} px_bitmap_enum_t;

typedef struct px_begin_image_args_s {
    uint width, height;
    int depth;
    pxeColorMapping_t mapping;
    real dest_width, dest_height;
} px_bitmap_args_t;

/* Define our image enumerator. */
#ifndef px_image_enum_DEFINED
#  define px_image_enum_DEFINED
typedef struct px_image_enum_s px_image_enum_t;
#endif
struct px_image_enum_s
{
    gs_image_t image;
    byte *row;                  /* buffer for one row of data */
    void *info;                 /* state structure for driver */
    px_bitmap_enum_t benum;
    px_bitmap_args_t bi_args;
    bool enum_started;
};

gs_private_st_simple(st_px_image_enum, px_image_enum_t, "px_image_enum_t");


static int
stream_error(stream_state * st, const char *str)
{
    return_error(-1);
}

static int
px_jpeg_init(px_bitmap_enum_t * benum, px_bitmap_params_t * params, px_args_t * par)
{
    stream_cursor_read r;
    stream_cursor_write w;
    uint used;
    const byte *data = par->source.data;
    stream_DCT_state *ss = (&benum->dct_stream_state);
    jpeg_decompress_data *jddp = &(benum->jdd);
    uint avail = par->source.available;
    int code = 0;

    if (!jpeg_inited(benum->initialized)) {
        s_init_state((stream_state *)ss, &s_DCTD_template, benum->mem);
        ss->report_error = stream_error;
        s_DCTD_template.set_defaults((stream_state *)ss);

        ss->jpeg_memory = benum->mem;
        ss->data.decompress = jddp;

        jddp->templat = s_DCTD_template;
        jddp->memory = benum->mem;
        jddp->scanline_buffer = NULL;
        jddp->PassThrough = 0;
        jddp->PassThroughfn = 0;
        jddp->device = (void *)0;

        if (gs_jpeg_create_decompress(ss) < 0)
            return_error(errorInsufficientMemory);

        (*s_DCTD_template.init) ((stream_state *)ss);
        jpeg_set(&benum->initialized);
    }

    r.ptr = data - 1;
    r.limit = r.ptr + avail;

    /* this seems to be the way to just get the header info */
    w.ptr = w.limit = 0;
    code =
        (*s_DCTD_template.process) ((stream_state *)ss, &r, &w, false);

    if (code < 0)
        goto error;

    used = r.ptr + 1 - data;
    par->source.data = r.ptr + 1;
    par->source.available = avail - used;

    /* Needs more data to complete reading the header */
    if (ss->phase < 2)
        return pxNeedData;

    params->width = jddp->dinfo.output_width;
    params->height = jddp->dinfo.output_height;
    {
        int comps = jddp->dinfo.output_components;
        if (comps == 1) {
            params->color_space = eGray;
        } else if (comps == 3) {
            params->color_space = eRGB;
        }
        else {
            code = -1;
            goto error;
        }
        params->depth = 8;
        return 0;
    }

error:
    gs_jpeg_destroy(ss);
    return_error(code);
}

/* Extract the parameters for reading a bitmap image or raster pattern. */
static int
begin_bitmap(px_bitmap_params_t * params, px_bitmap_enum_t * benum,
             const px_bitmap_args_t * bar, const px_state_t * pxs, bool is_jpeg, px_args_t * par)
{
    int depth;
    int num_components;
    px_gstate_t *pxgs = pxs->pxgs;

    benum->mem = pxs->memory;

    if (is_jpeg) {
        int code = px_jpeg_init(benum, params, par);
        if (code < 0 || code == pxNeedData)
            return_error(code);
        depth = params->depth;
        num_components = (params->color_space == eGray ? 1 : 3);
        if (num_components == 3 && pxgs->color_space == eGray)
            benum->grayscale = true;
    } else {
        px_gstate_t *pxgs = pxs->pxgs;
        depth = "\001\004\010"[bar->depth];
        num_components = (pxgs->color_space == eGray ? 1 : 3);
        params->width = bar->width;
        params->height = bar->height;
        params->depth = depth;
        params->color_space = pxgs->color_space;
    }

    if (bar->mapping == eIndexedPixel && !is_jpeg) {
        if (pxgs->palette.data == 0)
            return_error(errorMissingPalette);
        if (pxgs->palette.size != (1 << depth) * num_components)
            return_error(errorImagePaletteMismatch);
        params->indexed = true;
        num_components = 1;
    } else
        params->indexed = false;

    params->dest_width = bar->dest_width;
    params->dest_height = bar->dest_height;
    benum->data_per_row =
        ((params->width * params->depth * num_components) + 7) >> 3;
    return 0;
}

/* Extract the parameters for reading a bitmap image or raster pattern. */
/* Attributes: pxaColorMapping, pxaColorDepth, pxaSourceWidth, */
/* pxaSourceHeight, pxaDestinationSize. */
static int
begin_rebuffered_bitmap(px_bitmap_params_t * params, px_bitmap_enum_t * benum,
                        const px_bitmap_args_t * bar, const px_state_t * pxs, bool is_jpeg, px_args_t * par)
{
    int code = begin_bitmap(params, benum, bar, pxs, is_jpeg, par);

    if (code < 0 || code == pxNeedData)
        return_error(code);

    if (params->width == 1 && params->height > 1) {
        benum->rebuffered_data_per_row = benum->data_per_row * params->height;
        benum->rebuffered = 1;
        benum->rebuffered_width = params->height;
        benum->rebuffered_height = params->width;
        benum->rebuffered_row_pos = 0;
    } else {
        benum->rebuffered_data_per_row = benum->data_per_row;
        benum->rebuffered = 0;
        benum->rebuffered_width = params->width;
        benum->rebuffered_height = params->height;
    }

    return code;
}

static int
read_jpeg_bitmap_data(px_bitmap_enum_t * benum, byte ** pdata,
                      px_args_t * par, bool last)
{
    uint data_per_row = benum->data_per_row;    /* jpeg doesn't pad */
    uint avail = par->source.available;
    uint end_pos = data_per_row * par->pv[1]->value.i;
    uint pos_in_row = par->source.position % data_per_row;
    const byte *data = par->source.data;
    stream_DCT_state *ss = (&benum->dct_stream_state);
    stream_cursor_read r;
    stream_cursor_write w;
    uint used;
    int code = -1;

    /* consumed all of the data */
    if ((par->source.position >= end_pos) && (ss->phase != 4) && (par->source.available == 0)) {
        /* shutdown jpeg filter if necessary */
        if (jpeg_inited(benum->initialized))
            gs_jpeg_destroy((&benum->dct_stream_state));
        return 0;
    }

    if (last)
        return_error(errorIllegalDataLength);

    if (!jpeg_inited(benum->initialized)) {
        jpeg_decompress_data *jddp = &(benum->jdd);

        /* we do not allow switching from other compression schemes to jpeg */
        if (benum->initialized != unset)
            return_error(errorIllegalAttributeValue);
        /* use the graphics library support for DCT streams */
        ss->memory = benum->mem;
        ss->templat = &s_DCTD_template;
        s_DCTD_template.set_defaults((stream_state *) ss);
        ss->report_error = stream_error;
        ss->data.decompress = jddp;
        /* set now for allocation */
        jddp->memory = ss->jpeg_memory = benum->mem;
        /* set this early for safe error exit */
        jddp->scanline_buffer = NULL;
        jddp->PassThrough = 0;
        jddp->PassThroughfn = 0;
        jddp->device = (void *)0;
        if (gs_jpeg_create_decompress(ss) < 0)
            return_error(errorInsufficientMemory);
        (*s_DCTD_template.init) ((stream_state *) ss);
        jddp->templat = s_DCTD_template;
        jpeg_set(&benum->initialized);
    }
    r.ptr = data - 1;
    r.limit = r.ptr + avail;
    if (pos_in_row < data_per_row) {
        /* Read more of the current row. */
        byte *data = *pdata;

        w.ptr = data + pos_in_row - 1;
        w.limit = data + data_per_row - 1;
        code =
            (*s_DCTD_template.process) ((stream_state *) ss, &r, &w, false);
        /* code = num scanlines processed (0=need more data, -ve=error) */
        used = w.ptr + 1 - data - pos_in_row;

        /* the filter process returns 1 to indicate more room is
         * needed for the output data, which should not happen unless
         * the parameters are specified incorrectly.
         */

        if (code == 1)
            code = -1;
        if ((code == EOFC) && (used > 0))
            code = 1;
        pos_in_row += used;
        par->source.position += used;
    }
    used = r.ptr + 1 - data;
    par->source.data = r.ptr + 1;
    par->source.available = avail - used;
    /* The spec for this function says: Return 0 if we've processed all the
     * data in the block, 1 if we have a complete scan line, pxNeedData for
     * an incomplete scan line, or <0 for an error condition. The only way
     * we can return 0 currently is if we get an EOF from the underlying
     * decoder. */
    if (code == 0)
        return pxNeedData;
    if (code == EOFC)
        return 0;
    if (code > 0)
        return 1;
    return code;
}

static int
read_uncompressed_bitmap_data(px_bitmap_enum_t * benum, byte ** pdata,
                              px_args_t * par, bool last)
{
    int code;
    uint avail = par->source.available;
    uint data_per_row = benum->data_per_row;
    uint pad = 4;               /* default padding */
    const byte *data = par->source.data;
    uint pos_in_row, data_per_row_padded, used;

    /* overrided default padding */
    if (par->pv[3])
        pad = par->pv[3]->value.i;

    data_per_row_padded = round_up(data_per_row, (int)pad);
    pos_in_row = par->source.position % data_per_row_padded;

    /* consumed all of the data */
    if (par->source.position >= data_per_row_padded * par->pv[1]->value.i)
        return 0;

    if (last)
        return_error(errorIllegalDataLength);

    if (avail >= data_per_row_padded && pos_in_row == 0) {
        /* Use the data directly from the input buffer. */
        *pdata = (byte *) data;
        used = data_per_row_padded;
        code = 1;
    } else {
        used = min(avail, data_per_row_padded - pos_in_row);
        if (pos_in_row < data_per_row)
            memcpy(*pdata + pos_in_row, data,
                   min(used, data_per_row - pos_in_row));
        code = (pos_in_row + used < data_per_row_padded ? pxNeedData : 1);
    }
    par->source.position += used;
    par->source.data = data + used;
    par->source.available = avail - used;
    return code;
}

static int
read_rle_bitmap_data(px_bitmap_enum_t * benum, byte ** pdata, px_args_t * par, bool last)
{
    stream_RLD_state *ss = (&benum->rld_stream_state);
    uint avail = par->source.available;
    const byte *data = par->source.data;
    uint pad = 4;
    stream_cursor_read r;
    stream_cursor_write w;
    uint pos_in_row, data_per_row, data_per_row_padded, used;

    /* overrided default padding */
    if (par->pv[3])
        pad = par->pv[3]->value.i;
    data_per_row = benum->data_per_row;
    data_per_row_padded = round_up(benum->data_per_row, (int)pad);
    pos_in_row = par->source.position % data_per_row_padded;

    /* consumed all of the data */
    if (par->source.position >= data_per_row_padded * par->pv[1]->value.i)
        return 0;

    if (last)
        return_error(errorIllegalDataLength);

    if (!rle_inited(benum->initialized)) {
        ss->EndOfData = false;
        ss->templat = &s_RLD_template;
        s_RLD_init_inline(ss);
        rle_set(&benum->initialized);
    }
    r.ptr = data - 1;
    r.limit = r.ptr + avail;
    if (pos_in_row < data_per_row) {
        /* Read more of the current row. */
        byte *data = *pdata;

        w.ptr = data + pos_in_row - 1;
        w.limit = data + data_per_row - 1;
        (*s_RLD_template.process) ((stream_state *) ss, &r, &w, false);
        used = w.ptr + 1 - data - pos_in_row;
        pos_in_row += used;
        par->source.position += used;
    }
    if (pos_in_row >= data_per_row && pos_in_row < data_per_row_padded) {       /* We've read all the real data; skip the padding. */
        byte pad[3];            /* maximum padding per row */

        w.ptr = pad - 1;
        w.limit = w.ptr + data_per_row_padded - pos_in_row;
        (*s_RLD_template.process) ((stream_state *) ss, &r, &w, false);
        used = w.ptr + 1 - pad;
        pos_in_row += used;
        par->source.position += used;
    }

    used = r.ptr + 1 - data;
    par->source.data = r.ptr + 1;
    par->source.available = avail - used;
    return (pos_in_row < data_per_row_padded ? pxNeedData : 1);
}

/** PCL XL delta row decompression
 *
 * delta row Algorithm:
 *
 * Seed Row is initialized with zeros.
 *
 * lsb,msb row byte count
 * delta row data
 * repeat for each row
 *
 * if row byte count is zero duplicate previous row
 * if row byte count doesn't fill row duplicate remainder and end the row (undocumented)
 *
 * delta row data: command byte, optional extra offset bytes, delta raster snippit
 * command byte 7-5 delta raster length: 4-0 offset
 *
 * offset = bits 4-0;
 * if offset == 31 then do { add next byte } repeat while next byte was 0xFF
 * example offset = 31 + 255 + 255 + 255 + 4
 *
 * delta length = bits 5-7  + 1; range 1 to 8 bytes.
 *
 * output raster is:
 * last position + offset; "copies" old data
 * copy delta length bytes from input to output
 *
 * Internal Algorithm:
 *
 *  No row padding is used.
 *  State is need since available data can be short at any time.
 *  read = *pin++; // out of data? save state, return eNeedData
 *
 * deltarow.state maintains state between requests for more data
 * state             : description
 *                   -> next state
 * ---------------------------------------
 * next_is_bytecount : lsb of row bytecount
 *                   -> partial_bytecount
 * partial_bytecount : msb of row bytecount
 *                   -> next_is_cmd
 * next_is_cmd       : 1 byte cmd contains cnt and partial offset
 *                   -> partial_offset or partial_cnt
 * partial_offset    : accumulates extra offset bytes, moves output by offset
 *                   -> partial_offset or partial_cnt
 * partial_cnt       : copies cnt bytes one at a time from input
 *                   -> partial_cnt or next_is_cmd or (next_bytecount && end_of_row)
 *
 * RETURN values:
 *  0 == end of image   // end of row returns, next call returns end of image.
 *  1 == end of row
 *  eNeedData == on need more input
 */
static int
read_deltarow_bitmap_data(px_bitmap_enum_t * benum, byte ** pdata,
                          px_args_t * par, bool last)
{
    deltarow_state_t *deltarow = &benum->deltarow_state;
    uint avail = par->source.available;
    const byte *pin = par->source.data;
    byte *pout = *pdata + par->source.position % benum->data_per_row;
    const byte *pout_start = pout;
    bool end_of_row = false;

    if (delta_inited(benum->initialized) && deltarow->rowwritten == par->pv[1]->value.i) {
        deltarow->rowwritten = 0;
        return 0;
    }

    if (last)
        return_error(errorIllegalDataLength);

    /* initialize at begin of image */
    if (!delta_inited(benum->initialized)) {
        /* zero seed row */
        deltarow->seedrow =
            gs_alloc_bytes(benum->mem, benum->data_per_row,
                           "read_deltarow_bitmap_data");
        memset(deltarow->seedrow, 0, benum->data_per_row);
        deltarow->row_byte_count = 0;
        deltarow->short_cnt = 0;
        deltarow->short_offset = 0;
        deltarow->state = next_is_bytecount;
        deltarow->rowwritten = 0;
        delta_set(&benum->initialized);
    }

    if (deltarow->row_byte_count == 0) {
        memcpy(*pdata, deltarow->seedrow, benum->data_per_row);
    }

    /* one byte at a time until end of input or end of row */
    while (avail && !end_of_row) {
        switch (deltarow->state) {

            case next_is_bytecount:{
                    deltarow->short_cnt = *pin++;
                    --avail;
                    deltarow->state = partial_bytecount;
                    break;
                }

            case partial_bytecount:{
                    deltarow->row_byte_count =
                        deltarow->short_cnt + ((*pin++) << 8);
                    --avail;

                    if (deltarow->row_byte_count == 0) {
                        /* duplicate the row */
                        deltarow->state = next_is_bytecount;
                        end_of_row = true;
                    } else
                        deltarow->state = next_is_cmd;
                    break;
                }

            case next_is_cmd:{
                    uint val = *pin++;

                    --avail;
                    deltarow->row_byte_count--;
                    deltarow->short_cnt = (val >> 5) + 1;       /* 1 to 8 new bytes to copy */
                    deltarow->short_offset = val & 0x1f;        /* num to retain from last row, skip */
                    if (deltarow->short_offset == 0x1f)
                        deltarow->state = partial_offset;       /* accumulate more offset */
                    else {
                        pout += deltarow->short_offset; /* skip keeps old data in row */
                        deltarow->state = partial_cnt;  /* done with offset do count */
                    }
                    break;
                }

            case partial_offset:{
                    uint offset = *pin++;

                    avail--;
                    deltarow->row_byte_count--;

                    deltarow->short_offset += offset;

                    if (offset == 0xff)
                        deltarow->state = partial_offset;       /* 0x1f + ff ff ff ff ff + 1 */
                    else {
                        pout += deltarow->short_offset; /* skip keeps old data in row */
                        deltarow->state = partial_cnt;  /* done with offset do count */
                    }
                    break;
                }

            case partial_cnt:{
                    /* check for possible row overflow */
                    if (pout >= *pdata + benum->data_per_row)
                        return -1;
                    *pout++ = *pin++;   /* copy new data into row */
                    avail--;
                    deltarow->row_byte_count--;
                    deltarow->short_cnt--;

                    if (deltarow->row_byte_count == 0) {
                        end_of_row = true;
                        deltarow->state = next_is_bytecount;
                    } else if (deltarow->short_cnt == 0)
                        deltarow->state = next_is_cmd;
                    /* else more bytes to copy */
                    break;
                }

        }                       /* end switch */
    }                           /* end of while */

    par->source.available -= pin - par->source.data;    /* subract comressed data used */
    par->source.data = pin;     /* new compressed data position */

    if (end_of_row) {
        /* uncompressed raster position */
        par->source.position =
            (par->source.position / benum->data_per_row +
             1) * benum->data_per_row;
        deltarow->rowwritten++;
        memcpy(deltarow->seedrow, *pdata, benum->data_per_row);
        return 1;
    }
    par->source.position += pout - pout_start;  /* amount of raster written */
    return pxNeedData;          /* not end of row so request more data */
}

/*
 * Read a (possibly partial) row of bitmap data.  This is most of the
 * implementation of ReadImage and ReadRastPattern.  We use source.position
 * to track the uncompressed input byte position within the current block.
 * Return 0 if we've processed all the data in the block, 1 if we have a
 * complete scan line, pxNeedData for an incomplete scan line, or <0 for
 * an error condition.  *pdata must point to a scan line buffer; this
 * routine may reset it to point into the input buffer (if a complete
 * scan line is available).
 * Attributes: pxaStartLine (ignored), pxaBlockHeight, pxaCompressMode.
 */
static int
read_bitmap(px_bitmap_enum_t * benum, byte ** pdata, px_args_t * par, bool last)
{
    switch (par->pv[2]->value.i) {
        case eRLECompression:
            return read_rle_bitmap_data(benum, pdata, par, last);
        case eJPEGCompression:
            return read_jpeg_bitmap_data(benum, pdata, par, last);
        case eDeltaRowCompression:
            return read_deltarow_bitmap_data(benum, pdata, par, last);
        case eNoCompression:
            return read_uncompressed_bitmap_data(benum, pdata, par, last);
        default:
            break;
    }
    return -1;
}

static int read_rebuffered_bitmap(px_bitmap_enum_t * benum, byte ** pdata, px_args_t * par)
{
    int code;

    if (!benum->rebuffered)
        return read_bitmap(benum, pdata, par, /* last */ false);

    {
        int w = benum->rebuffered_width;
        byte *rowptr = *pdata;
        byte *data2 = rowptr + benum->rebuffered_row_pos * benum->data_per_row;
        for (; benum->rebuffered_row_pos < w; benum->rebuffered_row_pos++) {
            byte *data3 = data2;
            code = read_bitmap(benum, &data3, par, /* last */ false);
            if (code == 0) {
                return 0;
            } else if (code == 1) {
                /* got a scanline! (well, 1 pixel really, cos we are flipping) */
            } else {
                if (par->source.available == 0)
                    return pxNeedData;
                return code;
            }
            if (data3 != data2) {
                memcpy(data2, data3, benum->data_per_row);
            }
            data2 += benum->data_per_row;
        }
        benum->rebuffered_row_pos = 0;
    }
    return code;
}


/* ---------------- Image operators ---------------- */

const byte apxBeginImage[] = {
    pxaColorMapping, pxaColorDepth, pxaSourceWidth, pxaSourceHeight,
    pxaDestinationSize, 0, 0
};

int
pxBeginImage(px_args_t * par, px_state_t * pxs)
{
    px_image_enum_t *pxenum;
    px_bitmap_args_t bi_args;

    pxenum =
        gs_alloc_struct(pxs->memory, px_image_enum_t,
                        &st_px_image_enum, "setup_bitmap(pxenum)");

    if (pxenum == 0)
        return_error(errorInsufficientMemory);

    /*
     * store a copy of the args.  For JPEG these parameters may be
     * revised depending on the color parameters in the JPEG data.
     * We defer image set up until we read the image data.
     */

    bi_args.mapping     = par->pv[0]->value.i;
    bi_args.depth       = par->pv[1]->value.i;
    bi_args.width       = par->pv[2]->value.i;
    bi_args.height      = par->pv[3]->value.i;
    bi_args.dest_width  = real_value(par->pv[4], 0);
    bi_args.dest_height = real_value(par->pv[4], 1);

    pxenum->bi_args = bi_args;
    pxenum->enum_started = false;
    pxs->image_enum = pxenum;
    memset(&pxenum->benum, 0, sizeof(pxenum->benum));
    return 0;
}

static int
px_begin_image(px_state_t * pxs, bool is_jpeg, px_args_t * par)
{
    gs_point origin;
    px_bitmap_params_t params;
    gs_gstate *pgs = pxs->pgs;
    px_gstate_t *pxgs = pxs->pxgs;
    px_image_enum_t *pxenum = pxs->image_enum;
    px_bitmap_enum_t *pbenum = &pxenum->benum;
    int code;

    if (gs_currentpoint(pgs, &origin) < 0)
        return_error(errorCurrentCursorUndefined);
    /*
     * If the current logical operation doesn't involve the texture,
     * don't set a null brush, which would cause the image not to
     * appear.
     */
    if (pxs->pxgs->brush.type == pxpNull &&
        !rop3_uses_T(gs_currentrasterop(pgs)))
        code = gs_setgray(pgs, 0.0);
    else
        code = px_set_paint(&pxgs->brush, pxs);

    if (code < 0)
        return code;
    /*
     * Make sure the proper halftone is current.
     */
    code = px_set_halftone(pxs);
    if (code < 0)
        return code;
    code = begin_rebuffered_bitmap(&params, pbenum, &pxenum->bi_args, pxs, is_jpeg, par);
    if (code < 0 || code == pxNeedData)
        return_error(code);

    pxenum->row = gs_alloc_byte_array(pxs->memory, 1, pbenum->rebuffered_data_per_row,
                                      "pxReadImage(row)");
    if (pxenum->row == 0)
        code = gs_note_error(errorInsufficientMemory);
    else
        code =
            px_image_color_space(&pxenum->image, &params,
                                 (const gs_string *)&pxgs->palette, pgs);

    if (code < 0) {
        gs_free_object(pxs->memory, pxenum->row, "pxReadImage(row)");
        gs_free_object(pxs->memory, pxenum, "pxReadImage(pxenum)");
        return code;
    }

    /* Set up the image parameters. */
    pxenum->image.Width = pbenum->rebuffered_width;
    pxenum->image.Height = pbenum->rebuffered_height;
    {
        gs_matrix imat, dmat;

        /* We need the cast because height is unsigned. */
        /* We also need to account for the upside-down H-P */
        /* coordinate system. */
        gs_make_scaling(params.width, params.height, &imat);
        if (pbenum->rebuffered) {
            imat.xy = imat.xx; imat.xx = 0;
            imat.yx = imat.yy; imat.yy = 0;
        }
        gs_make_translation(origin.x, origin.y, &dmat);
        gs_matrix_scale(&dmat, params.dest_width, params.dest_height, &dmat);
        /* The ImageMatrix is dmat' * imat. */
        code = gs_matrix_invert(&dmat, &dmat);
        if (code < 0)
            return code;
        gs_matrix_multiply(&dmat, &imat, &pxenum->image.ImageMatrix);
    }
    pxenum->image.CombineWithColor = true;
    pxenum->image.Interpolate = pxs->interpolate;

    code = pl_begin_image(pgs, &pxenum->image, &pxenum->info);
    if (code < 0) {
        /* This procedure will be re-invoked if we are remapping the
           color so don't free the resources */
        if (code != gs_error_Remap_Color) {
            gs_free_object(pxs->memory, pxenum->row, "pxReadImage(row)");
            gs_free_object(pxs->memory, pxenum, "pxBeginImage(pxenum)");
        }
        return code;
    }
    return 0;

}

const byte apxReadImage[] = {
    pxaStartLine, pxaBlockHeight, pxaCompressMode, 0, pxaPadBytesMultiple,
        pxaBlockByteLength, 0
};

int
pxReadImage(px_args_t * par, px_state_t * pxs)
{
    px_image_enum_t *pxenum = pxs->image_enum;
    int code = 0;

    if (par->pv[1]->value.i == 0)
        return 0;               /* no data */
    /* Make a quick check for the first call, when no data is available. */
    if (par->source.available == 0)
        return pxNeedData;
    if (!pxenum->enum_started) {
        bool is_jpeg = par->pv[2]->value.i == eJPEGCompression;
        code = px_begin_image(pxs, is_jpeg, par);
        if (code < 0 || code == pxNeedData)
            return code;
        pxenum->enum_started = true;
    }
    for (;;) {
        byte *data = pxenum->row;
        int code = read_rebuffered_bitmap(&pxenum->benum, &data, par);
        if (code != 1)
            return code;

        if (pxenum->benum.grayscale) {
            int i;
            for (i = 2; i < pxenum->benum.rebuffered_data_per_row; i+=3) {
                int r = data[i-2];
                int g = data[i-1];
                int b = data[i];
                /* we think this simple conversion is commensurate
                   with typical XL business graphics use cases. */
                int gray = (r + g + b) / 3;
                data[i-2] = data[i-1] = data[i] = gray;
            }
        }

        code = pl_image_data(pxs->pgs, pxenum->info, (const byte **)&data, 0,
                             pxenum->benum.rebuffered_data_per_row, 1);
        if (code < 0)
            return code;

        pxs->have_page = true;
    }
}

const byte apxEndImage[] = { 0, 0 };
int
pxEndImage(px_args_t * par, px_state_t * pxs)
{
    px_image_enum_t *pxenum = pxs->image_enum;
    px_bitmap_enum_t *pbenum = &pxenum->benum;
    int code = pl_end_image(pxs->pgs, pxenum->info, true);

    gs_free_object(pxs->memory, pxenum->row, "pxEndImage(row)");
    gs_free_object(pbenum->mem, pbenum->deltarow_state.seedrow,
                   "pxEndImage(seedrow)");
    if (pxenum->image.ColorSpace)
        rc_decrement(pxenum->image.ColorSpace,
                     "pxEndImage(image.ColorSpace)");
    gs_free_object(pxs->memory, pxenum, "pxEndImage(pxenum)");
    pxs->image_enum = 0;
    return code;
}

/* ---------------- Raster pattern operators ---------------- */

/* Define the enumerator for downloading raster patterns. */
#ifndef px_pattern_enum_DEFINED
#  define px_pattern_enum_DEFINED
typedef struct px_pattern_enum_s px_pattern_enum_t;
#endif
struct px_pattern_enum_s
{
    px_bitmap_enum_t benum;
    int32_t pattern_id;
    pxePatternPersistence_t persistence;
    px_pattern_t *pattern;
    int lines_rendered;
};

gs_private_st_simple(st_px_pattern_enum, px_pattern_enum_t,
                     "px_pattern_enum_t");

const byte apxBeginRastPattern[] = {
    pxaColorMapping, pxaColorDepth, pxaSourceWidth, pxaSourceHeight,
    pxaDestinationSize, pxaPatternDefineID, pxaPatternPersistence, 0, 0
};
int
pxBeginRastPattern(px_args_t * par, px_state_t * pxs)
{
    gs_memory_t *mem = pxs->memory;
    px_bitmap_params_t params;
    px_pattern_t *pattern;
    px_pattern_enum_t *pxenum;
    px_bitmap_enum_t benum;
    px_bitmap_args_t bi_args;
    byte *data;
    uint psize;
    byte *pdata;
    int code;
    static const gs_memory_struct_type_t st_px_pattern =
        { sizeof(px_pattern_t), "", 0, 0, 0, 0, 0 };


    bi_args.mapping     = par->pv[0]->value.i;
    bi_args.depth       = par->pv[1]->value.i;
    bi_args.width       = par->pv[2]->value.i;
    bi_args.height      = par->pv[3]->value.i;
    bi_args.dest_width  = real_value(par->pv[4], 0);
    bi_args.dest_height = real_value(par->pv[4], 1);

    memset(&benum, 0, sizeof(benum));
    code = begin_bitmap(&params, &benum, &bi_args, pxs, false, par);

    if (code < 0)
        return code;
    rc_alloc_struct_1(pattern, px_pattern_t, &st_px_pattern, mem,
                      return_error(errorInsufficientMemory),
                      "raster pattern");
    pattern->rc.free = rc_free_px_pattern;
    data = gs_alloc_byte_array(mem, params.height, benum.data_per_row,
                               "raster pattern data");
    if (params.indexed) {
        psize = pxs->pxgs->palette.size;
        pdata = gs_alloc_string(mem, psize, "raster pattern palette");
        if (pdata != 0)
            memcpy(pdata, pxs->pxgs->palette.data, psize);
    } else {
        psize = 0;
        pdata = 0;
    }
    pxenum = gs_alloc_struct(mem, px_pattern_enum_t, &st_px_pattern_enum,
                             "raster pattern enum");
    if (data == 0 || (params.indexed && pdata == 0) || pxenum == 0) {
        gs_free_object(mem, pxenum, "raster pattern enum");
        gs_free_string(mem, pdata, psize, "raster pattern palette");
        gs_free_object(mem, data, "raster pattern data");
        gs_free_object(mem, pattern, "raster pattern");
        return_error(errorInsufficientMemory);
    }
    pxenum->benum = benum;
    pxenum->pattern_id = par->pv[5]->value.i;
    pxenum->persistence = par->pv[6]->value.i;
    pxenum->lines_rendered = 0;
    pattern->params = params;
    pattern->palette.data = pdata;
    pattern->palette.size = psize;
    pattern->data = data;
    pattern->id = gs_next_ids(mem, 1);
    pxenum->pattern = pattern;
    pxs->pattern_enum = pxenum;
    return 0;
}

const byte apxReadRastPattern[] = {
    pxaStartLine, pxaBlockHeight, pxaCompressMode, 0, pxaPadBytesMultiple,
        pxaBlockByteLength, 0
};

int
pxReadRastPattern(px_args_t * par, px_state_t * pxs)
{
    px_pattern_enum_t *pxenum = pxs->pattern_enum;
    int code;
    byte *plimit = pxenum->pattern->data +
        (pxenum->benum.data_per_row * pxenum->pattern->params.height);

    if (par->pv[1]->value.i == 0)
        return 0;               /* no data */

    /* first call */
    if (par->source.available == 0)
        pxenum->lines_rendered = 0;

    for (;;) {
        byte *data = pxenum->pattern->data +
            (par->pv[0]->value.i + pxenum->lines_rendered) * pxenum->benum.data_per_row;
        byte *rdata = data;

        if (data > plimit)
            return_error(gs_error_rangecheck);

        code = read_bitmap(&pxenum->benum, &rdata, par, /* last */ data == plimit);
        if (code != 1)
            break;

        pxenum->lines_rendered++;

        if (rdata != data)
            memcpy(data, rdata, pxenum->benum.data_per_row);
    }

    return code;
}

const byte apxEndRastPattern[] = { 0, 0 };
int
pxEndRastPattern(px_args_t * par, px_state_t * pxs)
{
    px_pattern_enum_t *pxenum = pxs->pattern_enum;
    /* We extract the key and value from the pattern_enum structure */
    /* and then free the structure, to encourage LIFO allocation. */
    px_pattern_t *pattern = pxenum->pattern;
    int32_t id = pxenum->pattern_id;
    px_value_t key;
    px_dict_t *pdict;

    switch (pxenum->persistence) {
        case eTempPattern:
            pdict = &pxs->pxgs->temp_pattern_dict;
            break;
        case ePagePattern:
            pdict = &pxs->page_pattern_dict;
            break;
        case eSessionPattern:
            pdict = &pxs->session_pattern_dict;
            break;
        default:               /* can't happen */
            return_error(errorIllegalAttributeValue);
    }
    key.type = pxd_array | pxd_ubyte;
    key.value.array.data = (byte *) & id;
    key.value.array.size = sizeof(id);
    gs_free_object(pxs->memory, pxenum, "pxEndRastPattern(pxenum)");
    return px_dict_put(pdict, &key, pattern);
}

/* ---------------- Scan line operators ---------------- */

const byte apxBeginScan[] = { 0, 0 };
int
pxBeginScan(px_args_t * par, px_state_t * pxs)
{
    int code = px_set_paint(&pxs->pxgs->brush, pxs);

    if (code < 0)
        return code;
    /* We may as well reset the path now instead of at the end. */
    return gs_newpath(pxs->pgs);
}

const byte apxEndScan[] = { 0, 0 };
int
pxEndScan(px_args_t * par, px_state_t * pxs)
{
    return 0;
}

const byte apxScanLineRel[] = {
    0, pxaNumberOfScanLines, 0
};
int
pxScanLineRel(px_args_t * par, px_state_t * pxs)
{                               /*
                                 * In order to keep the number of intermediate states down to a
                                 * reasonable number, we require enough data to be present to be
                                 * able to read the control information for each line, or an entire
                                 * x-pair.  Initially, source.position is zero.  As soon as we have
                                 * read the X/YStart type byte, we change it to:
                                 *      (X/YStart type) << 28 + (x-pair type << 24) +
                                 *        (# of full or partial scan lines left to process) + 1
                                 * We use the separate variable source.count to keep track of
                                 * the number of x-pairs left in the scan line.
                                 */
    gs_gstate *pgs = pxs->pgs;
    bool big_endian = pxs->data_source_big_endian;
    const byte *data = par->source.data;
    pxeDataType_t
        xystart_type = (par->source.position >> 28) & 0xf,
        xpair_type = (par->source.position >> 24) & 0xf;
    int code = 0;
    int rcount;
    gs_rect rlist[20];          /* 20 is arbitrary */

    /* High level pattern support kludge. We need to know if we are executing
     * a high level pattern before we go consuming any of the input, because if
     * we are we need to return to the interpreter, set up the high level
     * pattern and then come back here. Obviously if we've consumed the data
     * we can't come back and rerun it. So we do a 'no op' rectfill which will
     * set the color, if it is a high level pattern then we will get an error
     * which we return.
     */
    code = gs_rectfill(pgs, NULL, 0);
    if (code < 0)
        return code;

    /* Check for initial state. */
    if (par->source.position == 0) {    /* Read XStart/YStart data type. */
        if (par->source.available < 1)
            return pxNeedData;
        xystart_type = data[0];
        if (xystart_type != eSInt16)
            return_error(errorIllegalDataValue);
        par->source.position =
            ((ulong) xystart_type << 28) +
            (par->pv[0] ? par->pv[0]->value.i : 1) + 1;
        par->source.data = data += 1;
        par->source.available -= 1;
        par->source.count = 0;
    }
    for (rcount = 0;;) {

        /* Check for start of scan line. */
        if (par->source.count == 0) {
            int ystart;

            if ((par->source.position & 0xffffff) == 1) {
                code = 0;
                break;
            }
            /* Read XStart and YStart values. */
            /* We know that eSInt16 is the only possible data type. */
            if (par->source.available < 7) {
                code = pxNeedData;
                break;
            }
            pxs->scan_point.x = sint16at(data, big_endian);
            ystart = sint16at(data + 2, big_endian);
            pxs->scan_point.y0 = ystart - 0.5;
            pxs->scan_point.y1 = ystart + 0.5;
            par->source.count = uint16at(data + 4, big_endian);
            if (par->source.count == 0) {
                code = gs_note_error(errorIllegalDataValue);
                break;
            }
            xpair_type = data[6];
            par->source.position =
                (par->source.position & 0xf0ffffff) +
                ((ulong) xpair_type << 24);
            par->source.data = data += 7;
            par->source.available -= 7;
        }
        /* Read and process one x-pair. */
        {
            uint x0, x1;
            uint used;

            switch (xpair_type) {
                case eUByte:
                    if (par->source.available < 2) {
                        code = pxNeedData;
                        goto out;       /* 2-level break */
                    }
                    x0 = data[0];
                    x1 = data[1];
                    used = 2;
                    break;
                case eUInt16:
                    if (par->source.available < 4) {
                        code = pxNeedData;
                        goto out;       /* 2-level break */
                    }
                    x0 = uint16at(data, big_endian);
                    x1 = uint16at(data + 2, big_endian);
                    used = 4;
                    break;
                default:
                    code = gs_note_error(errorIllegalDataValue);
                    goto out;   /* 2-level break; */
            }
            if (rcount == countof(rlist)) {
                code = gs_rectfill(pgs, rlist, rcount);
                rcount = 0;
                pxs->have_page = true;
                if (code < 0)
                    break;
            }
            {
                gs_rect *pr = &rlist[rcount++];

                pr->p.x = pxs->scan_point.x += x0;
                pr->p.y = pxs->scan_point.y0;
                pr->q.x = pxs->scan_point.x += x1;
                pr->q.y = pxs->scan_point.y1;
            }
            par->source.data = data += used;
            par->source.available -= used;
        }
        if (!--(par->source.count))
            --(par->source.position);
    }
  out:
    if (rcount > 0 && code >= 0) {
        int rcode = gs_rectfill(pgs, rlist, rcount);

        pxs->have_page = true;
        if (rcode < 0)
            code = rcode;
    }
    return code;
}
