/* Copyright (C) 2001-2017 Artifex Software, Inc.
   All Rights Reserved.

   This software is provided AS-IS with no warranty, either express or
   implied.

   This software is distributed under license and may not be copied,
   modified or distributed except as expressly authorized under the terms
   of the license contained in the file LICENSE in this distribution.

   Refer to licensing information at http://www.artifex.com or contact
   Artifex Software, Inc.,  7 Mt. Lassen Drive - Suite A-134, San Rafael,
   CA  94903, U.S.A., +1(415)492-9861, for further information.
*/

/* Image scaling filters */
#include "math_.h"
#include "memory_.h"
#include "stdio_.h"
#include "stdint_.h"
#include "gdebug.h"
#include "strimpl.h"
#include "siscale.h"
#include "gxfrac.h"
#include "cal.h"
#include "assert_.h"

/* ImageScaleEncode / ImageScaleDecode */
typedef struct stream_IScale_cal_state_s {
    /* The client sets the params values before initialization. */
    stream_image_scale_state_common;  /* = state_common + params */
    /* The init procedure sets the following. */
    cal_rescaler *rescaler;
    uint8_t *src;
    uint8_t *dst;
    byte *tmp;
    int pre_scan_bytes;
    int post_scan_bytes;
    /* The following are updated dynamically. */
    int src_y;
    uint src_offset, src_size;
    int dst_y;
    uint dst_offset, dst_size;
} stream_IScale_cal_state;

/* FIXME: */
gs_private_st_ptrs2(st_IScale_cal_state, stream_IScale_cal_state,
    "ImageScaleEncode/Decode state",
    iscale_state_enum_ptrs, iscale_state_reloc_ptrs,
    dst, src);

/* ------ Stream implementation ------ */

/* Forward references */
static void s_IScale_cal_release(stream_state * st);

/* Set default parameter values (actually, just clear pointers) */
static void
s_IScale_cal_set_defaults(stream_state * st)
{
    stream_IScale_cal_state *const ss = (stream_IScale_cal_state *) st;

    ss->rescaler = NULL;
}

/*

    Some notes:

    (ss->params.XXXX is shown as XXXX in the following for sanity)

    Conceptually we are scaling a bitmap that was EntireWidthIn x EntireHeightIn
    in size, to be EntireWidthOut x EntireHeightOut in size.

    But, we only actually care about a sub rectangle of this in the destination,
    given by (LeftMarginOut, TopMarginOut) + (PatchWidthOut, PatchHeightOut).
    Anything else is clipped away. There are times when this sub rectangle can
    be very "sub" indeed, so the ability to avoid rescaling all the data we
    don't care about is a vital one.

    To confuse this further, we don't get fed scanlines of EntireWidthIn pixels,
    instead we get WidthIn pixels. Similarly we don't feed scanlines out of
    EntireWidthOut pixels, but rather of WidthOut pixels. Width{In,Out} are not
    (always) the same as PatchWidth{In,Out} either.

    Accordingly there may be padding before and after the active region. We make
    the effort to ensure these bytes are set to zero.
*/


static int
s_IScale_cal_init(stream_state * st)
{
    stream_IScale_cal_state *const ss = (stream_IScale_cal_state *) st;
    gs_memory_t *mem = ss->memory;
    int abs_interp_limit = ss->params.abs_interp_limit;
    int limited_WidthOut = (ss->params.WidthOut + abs_interp_limit - 1) / abs_interp_limit;
    int limited_PatchWidthOut = (ss->params.PatchWidthOut + abs_interp_limit - 1) / abs_interp_limit;
    int limited_PatchHeightOut = (ss->params.PatchHeightOut2 + abs_interp_limit - 1) / abs_interp_limit;
    int limited_EntireWidthOut = (ss->params.EntireWidthOut + abs_interp_limit - 1) / abs_interp_limit;
    int limited_EntireHeightOut = (ss->params.EntireHeightOut + abs_interp_limit - 1) / abs_interp_limit;
    int limited_LeftMarginOut = (ss->params.LeftMarginOut) / abs_interp_limit;
    int limited_TopMarginOut = (ss->params.TopMarginOut2) / abs_interp_limit;
    int limited_PadY = (ss->params.pad_y + abs_interp_limit/2 ) / abs_interp_limit;
    int dst_bytes_per_pixel = ss->params.BitsPerComponentOut / 8;
    int src_bytes_per_pixel = ss->params.BitsPerComponentIn / 8;

    ss->src_offset = 0;
    ss->src_size = ss->params.WidthIn * ss->params.spp_interp * src_bytes_per_pixel;
    ss->dst_offset = 0;
    ss->dst_size = limited_WidthOut * ss->params.spp_interp * dst_bytes_per_pixel;
    ss->dst_y = -limited_PadY;
    ss->pre_scan_bytes = limited_LeftMarginOut * ss->params.spp_interp * dst_bytes_per_pixel;
    ss->post_scan_bytes = (limited_WidthOut - limited_PatchWidthOut - limited_LeftMarginOut) * ss->params.spp_interp * dst_bytes_per_pixel;

    ss->dst = gs_alloc_byte_array(mem, ss->dst_size, 1, "image_scale dst");
    if (ss->dst == NULL)
        goto fail;

    ss->src = gs_alloc_byte_array(mem, ss->params.EntireWidthIn * ss->params.spp_interp,
                                  src_bytes_per_pixel, "image_scale dst");
    if (ss->src == NULL)
        goto fail;

    ss->rescaler = cal_rescaler_init(mem->gs_lib_ctx->core->cal_ctx,
                                     mem->non_gc_memory,
                                     ss->params.EntireWidthIn,
                                     ss->params.EntireHeightIn,
                                     0,
                                     ss->params.src_y_offset,
                                     ss->params.WidthIn,
                                     ss->params.HeightIn,
                                     limited_EntireWidthOut,
                                     limited_EntireHeightOut,
                                     limited_LeftMarginOut,
                                     limited_TopMarginOut,
                                     limited_PatchWidthOut,
                                     limited_PatchHeightOut,
                                     CAL_MITCHELL,
                                     src_bytes_per_pixel,
                                     dst_bytes_per_pixel,
                                     ss->params.spp_interp,
                                     ss->params.MaxValueIn,
                                     ss->params.MaxValueOut);
    if (ss->rescaler == NULL)
        goto fail;

    if (ss->pre_scan_bytes)
        memset(ss->dst, 0, ss->pre_scan_bytes);
    if (ss->post_scan_bytes)
        memset(ss->dst + ss->dst_size - ss->post_scan_bytes, 0, ss->post_scan_bytes);

    return 0;

fail:
    if (ss->rescaler)
        cal_rescaler_fin(ss->rescaler, mem->non_gc_memory);
    gs_free_object(mem, ss->src, "image_scale src");
    gs_free_object(mem, ss->dst, "image_scale dst");
    return ERRC;
}

/* Process a buffer.  Note that this handles Encode and Decode identically. */
static int
s_IScale_cal_process(stream_state * st, stream_cursor_read * pr,
                     stream_cursor_write * pw, bool last)
{
    stream_IScale_cal_state *const ss = (stream_IScale_cal_state *) st;
    gs_memory_t *mem = ss->memory;
    int abs_interp_limit = ss->params.abs_interp_limit;
    int limited_HeightOut = (ss->params.HeightOut + abs_interp_limit - 1) / abs_interp_limit;
    uint wleft;
    uint rleft;
    int any_output = 0;
    const byte *input = NULL;

    /* If we have no more data to pull out, we're done. */
    if (ss->dst_y == limited_HeightOut)
        return EOFC;

    /* How much room do we have left in the output buffer? */
    wleft = pw->limit - pw->ptr;

    /* If no room left, exit */
    if (wleft == 0)
        return 1;

    /* If we need to send some padding at the top, do so */
    if (ss->dst_y < 0)
    {
        uint wcount = ss->dst_size - ss->dst_offset;
        uint ncopy = wcount;

        if (ncopy > wleft)
            ncopy = wleft;
        memset(pw->ptr + 1, 0, ncopy);
        pw->ptr += ncopy;
        wcount -= ncopy;
        if (wcount == 0)
        {
            ss->dst_offset = 0;
            ss->dst_y++;
        }
        else
            ss->dst_offset += ncopy;
        wleft -= ncopy;
        /* Unless we can get a whole new line out, pass out what we have */
        if (wleft < ss->dst_size)
            return 1;
        any_output = 1;
    }

    /* Pass out any buffered data we have */
    if (ss->dst_offset != 0)
    {
        uint wcount = ss->dst_size - ss->dst_offset;
        uint ncopy = wcount;

        if (ncopy > wleft)
            ncopy = wleft;
        memcpy(pw->ptr + 1, (byte *) ss->dst + ss->dst_offset, ncopy);
        pw->ptr += ncopy;
        wcount -= ncopy;
        if (wcount == 0)
        {
            ss->dst_offset = 0;
            ss->dst_y++;
        }
        else
            ss->dst_offset += ncopy;
        wleft -= ncopy;
        /* Unless we can get a whole new line out, pass out what we have */
        if (wleft < ss->dst_size)
            return 1;
        any_output = 1;
    }

    /* How much data do we have in the incoming buffer? */
    rleft = pr->limit - pr->ptr;
    if (rleft > 0 && ss->src_offset > 0) {
        /* We have part of a line buffered. Let's fill that out. */
        uint ncopy = ss->src_size - ss->src_offset;
        if (ncopy > rleft)
            ncopy = rleft;
        memcpy(ss->src + ss->src_offset, pr->ptr + 1, ncopy);
        pr->ptr += ncopy;
        rleft -= ncopy;
        ss->src_offset += ncopy;
        if (ss->src_offset == ss->src_size)
        {
            ss->src_offset = 0;
            input = ss->src;
        }
    }
    else if (rleft >= ss->src_size)
        input = pr->ptr+1;

    /* If we can extract a whole extra output line, then try for that.
     * Try anyway if we haven't managed to output anything. */
    while (wleft >= ss->dst_size || any_output == 0)
    {
        uint8_t *row;
        int ret;

        if (wleft >= ss->dst_size) {
            /* We can scale the row directly into the output. */
            row = pw->ptr + 1;
        } else {
            /* We'll have to buffer the row. */
            row = ss->dst;
        }
        ret = cal_rescaler_process(ss->rescaler, mem, input, row + ss->pre_scan_bytes);
        if (ret & 1)
        {
            /* Input consumed */
            if (input != ss->src)
            {
                pr->ptr += ss->src_size;
                rleft -= ss->src_size;
            }
            input = (rleft >= ss->src_size) ? pr->ptr+1 : NULL;
            ss->src_y++;
        }
        if (ret & 2)
        {
            /* Output given */
            if (row == ss->dst)
            {
                /* Copy as much of the the buffered data out as
                 * possible - we can't manage a whole line. */
                memcpy(pw->ptr+1, ss->dst, wleft);
                pw->ptr += wleft;
                ss->dst_offset = wleft;
                return 1;
            }
            else
            {
                /* We are scaling direct into the output. Clear any pre and
                 * post sections for neatness. */
                if (ss->pre_scan_bytes != 0)
                    memset(row, 0, ss->pre_scan_bytes);
                if (ss->post_scan_bytes != 0)
                    memset(row + ss->dst_size - ss->post_scan_bytes, 0, ss->post_scan_bytes);
            }
            pw->ptr += ss->dst_size;
            ss->dst_y++;
            wleft -= ss->dst_size;
            any_output = 1;
        }
        /* If nothing happened, nothing to be gained from calling again */
        if (ret == 0)
            break;
    }

    if (any_output == 0 && rleft > 0)
    {
        /* We've not managed to output anything, so the rescaler
         * must be waiting for input data. If we had a whole line of
         * data, we'd have tried to pass it in above. So we must
         * have only the start of a line. */
        assert(rleft < ss->src_size && ss->dst_offset == 0);
        memcpy(ss->src, pr->ptr + 1, rleft);
        ss->src_offset += rleft;
        pr->ptr += rleft;
        return 1;
    }

    return any_output;
}

/* Release the filter's storage. */
static void
s_IScale_cal_release(stream_state * st)
{
    stream_IScale_cal_state *const ss = (stream_IScale_cal_state *) st;
    gs_memory_t *mem = ss->memory;

    if (ss->rescaler)
    {
        cal_rescaler_fin(ss->rescaler, mem->non_gc_memory);
        ss->rescaler = NULL;
    }
    gs_free_object(mem, ss->src, "image_scale src");
    ss->src = NULL;
    gs_free_object(mem, ss->dst, "image_scale dst");
    ss->dst = NULL;
}

/* Stream template */
const stream_template s_IScale_template = {
    &st_IScale_cal_state, s_IScale_cal_init, s_IScale_cal_process, 1, 1,
    s_IScale_cal_release, s_IScale_cal_set_defaults
};
