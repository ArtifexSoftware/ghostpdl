/* Copyright (C) 2011 Artifex Software, Inc.
   All Rights Reserved.

   This software is provided AS-IS with no warranty, either express or
   implied.

   This software is distributed under license and may not be copied, modified
   or distributed except as expressly authorized under the terms of that
   license.  Refer to licensing information at http://www.artifex.com/
   or contact Artifex Software, Inc.,  7 Mt. Lassen Drive - Suite A-134,
   San Rafael, CA  94903, U.S.A., +1(415)492-9861, for further information.
*/

#include "gxdownscale.h"
#include "gserrors.h"
#include "gserror.h"
#include "gdevprn.h"

/* Error diffusion data is stored in errors block.
 * We have 1 empty entry at each end to avoid overflow. When
 * moving left to right we read from entries 2->width+1 (inclusive), and
 * write to 1->width. When moving right to left we read from width->1 and
 * write to width+1->2.
 *
 * Minimum feature size data is stored in the mfs_data block.
 * We have 1 extra entry at the end to avoid overflow. When moving left to
 * right we read from entries 1->width (inclusive), and write to 0->width-1.
 * When moving right to left we read from width-1->0 and write to width->1.
 */

enum {
    mfs_clear           = 0,
    mfs_force_off       = 1,
    mfs_above_is_0      = 2,
    mfs_above_left_is_0 = 4,
};

/* Mono downscale/error diffusion/min feature size code */
static void down_core(gx_downscaler_t *ds,
                      byte            *out_buffer,
                      int              row)
{
    int        x, xx, y, value;
    int        mask;
    int        e_downleft, e_down, e_forward = 0;
    int        pad_white;
    byte      *inp, *outp;
    int        width     = ds->width;
    int        awidth    = ds->awidth;
    int        factor    = ds->factor;
    int        span      = ds->span;
    int       *errors    = ds->errors;
    byte      *mfs_data  = ds->mfs_data;
    const int  threshold = factor*factor*128;
    const int  max_value = factor*factor*255;

    pad_white = (awidth - width) * factor;
    if (pad_white < 0)
        pad_white = 0;

    if (pad_white)
    {
        inp = ds->data + width*factor;
        for (y = factor; y > 0; y--)
        {
            memset(inp, 0xFF, pad_white);
            inp += span;
        }
    }

    inp = ds->data;
    if (mfs_data == NULL)
    {
        if ((row & 1) == 0)
        {
            /* Left to Right pass (no min feature size) */
            const int back = span * factor - 1;
            errors += 2;
            outp = inp;
            for (x = awidth; x > 0; x--)
            {
                value = e_forward + *errors;
                for (xx = factor; xx > 0; xx--)
                {
                    for (y = factor; y > 0; y--)
                    {
                        value += *inp;
                        inp += span;
                    }
                    inp -= back;
                }
                if (value >= threshold)
                {
                    *outp++ = 1;
                    value -= max_value;
                }
                else
                {
                    *outp++ = 0;
                }
                e_forward  = value * 7/16;
                e_downleft = value * 3/16;
                e_down     = value * 5/16;
                value     -= e_forward + e_downleft + e_down;
                errors[-2] += e_downleft;
                errors[-1] += e_down;
                *errors++   = value;
            }
            outp -= awidth;
        }
        else
        {
            /* Right to Left pass (no min feature size) */
            const int back = span * factor + 1;
            errors += awidth;
            inp += awidth*factor-1;
            outp = inp;
            for (x = awidth; x > 0; x--)
            {
                value = e_forward + *errors;
                for (xx = factor; xx > 0; xx--)
                {
                    for (y = factor; y > 0; y--)
                    {
                        value += *inp;
                        inp += span;
                    }
                    inp -= back;
                }
                if (value >= threshold)
                {
                    *outp-- = 1;
                    value -= max_value;
                }
                else
                {
                    *outp-- = 0;
                }
                e_forward  = value * 7/16;
                e_downleft = value * 3/16;
                e_down     = value * 5/16;
                value     -= e_forward + e_downleft + e_down;
                errors[2] += e_downleft;
                errors[1] += e_down;
                *errors--   = value;
            }
            outp++;
        }
    }
    else
    {
        if ((row & 1) == 0)
        {
            /* Left to Right pass (with min feature size = 2) */
            const int back = span * factor -1;
            byte mfs, force_forward = 0;
            errors += 2;
            outp = inp;
            *mfs_data++ = mfs_clear;
            for (x = awidth; x > 0; x--)
            {
                value = e_forward + *errors;
                for (xx = factor; xx > 0; xx--)
                {
                    for (y = factor; y > 0; y--)
                    {
                        value += *inp;
                        inp += span;
                    }
                    inp -= back;
                }
                mfs = *mfs_data;
                *mfs_data++ = mfs_clear;
                if ((mfs & mfs_force_off) || force_forward)
                {
                    /* We are being forced to be 0 */
                    *outp++ = 0;
                    force_forward = 0;
                }
                else if (value < threshold)
                {
                    /* We want to be 0 anyway */
                    *outp++ = 0;
                    if ((mfs & (mfs_above_is_0 | mfs_above_left_is_0))
                            != (mfs_above_is_0 | mfs_above_left_is_0))
                    {
                        /* We aren't in a group anyway, so must force other
                         * pixels. */
                        mfs_data[-2] |= mfs_force_off;
                        mfs_data[-1] |= mfs_force_off;
                        force_forward = 1;
                    }
                    else
                    {
                        /* No forcing, but we need to tell other pixels that
                         * we were 0. */
                        mfs_data[-2] |= mfs_above_is_0;
                        mfs_data[-1] |= mfs_above_left_is_0;
                    }
                }
                else
                {
                    *outp++ = 1;
                    value -= max_value;
                }
                e_forward  = value * 7/16;
                e_downleft = value * 3/16;
                e_down     = value * 5/16;
                value     -= e_forward + e_downleft + e_down;
                errors[-2] += e_downleft;
                errors[-1] += e_down;
                *errors++   = value;
            }
            outp -= awidth;
        }
        else
        {
            /* Right to Left pass (with min feature size = 2) */
            const int back = span * factor + 1;
            byte mfs, force_forward = 0;
            errors += awidth;
            mfs_data += awidth;
            inp += awidth*factor-1;
            outp = inp;
            *mfs_data-- = 0;
            for (x = awidth; x > 0; x--)
            {
                value = e_forward + *errors;
                for (xx = factor; xx > 0; xx--)
                {
                    for (y = factor; y > 0; y--)
                    {
                        value += *inp;
                        inp += span;
                    }
                    inp -= back;
                }
                mfs = *mfs_data;
                *mfs_data-- = mfs_clear;
                if ((mfs & mfs_force_off) || force_forward)
                {
                    /* We are being forced to be 0 */
                    *outp-- = 0;
                    force_forward = 0;
                }
                else if (value < threshold)
                {
                    *outp-- = 0;
                    if ((mfs & (mfs_above_is_0 | mfs_above_left_is_0))
                            != (mfs_above_is_0 | mfs_above_left_is_0))
                    {
                        /* We aren't in a group anyway, so must force other
                         * pixels. */
                        mfs_data[1] |= mfs_force_off;
                        mfs_data[2] |= mfs_force_off;
                        force_forward = 1;
                    }
                    else
                    {
                        /* No forcing, but we need to tell other pixels that
                         * we were 0. */
                        mfs_data[1] |= mfs_above_is_0;
                        mfs_data[2] |= mfs_above_left_is_0;
                    }
                }
                else
                {
                    *outp-- = 1;
                    value -= max_value;
                }
                e_forward  = value * 7/16;
                e_downleft = value * 3/16;
                e_down     = value * 5/16;
                value     -= e_forward + e_downleft + e_down;
                errors[2] += e_downleft;
                errors[1] += e_down;
                *errors--   = value;
            }
            outp++;
        }
    }
    /* Now pack the data pointed to by outp into byte form */
    inp   = outp;
    outp  = out_buffer;
    mask  = 128;
    value = 0;
    for (x = awidth; x > 0; x--)
    {
        if (*inp++)
            value |= mask;
        mask >>= 1;
        if (mask == 0) {
            mask = 128;
            *outp++= value;
            value = 0;
        }
    }
    if (mask != 128) {
        *outp++ = value;
    }
}

/* Grey downscale code */
#ifdef DO_STUPID_ERROR_DIFFUSION
static void down_core8(gx_downscaler_t *ds,
                       byte            *outp,
                       int              row)
{
    int        x, xx, y, value;
    int        mask;
    int        e_downleft, e_down, e_forward = 0;
    int        pad_white;
    byte      *inp;
    int        width     = ds->width;
    int        awidth    = ds->awidth;
    int        factor    = ds->factor;
    int        span      = ds->span;
    int       *errors    = ds->errors;
    byte      *mfs_data  = ds->mfs_data;
    int        div       = factor*factor;

    pad_white = (awidth - width) * factor;
    if (pad_white < 0)
        pad_white = 0;

    if (pad_white)
    {
        inp = ds->data + width*factor;
        for (y = factor; y > 0; y--)
        {
            memset(inp, 0xFF, pad_white);
            inp += span;
        }
    }

    inp = ds->data;
    if ((row & 1) == 0)
    {
        /* Left to Right pass (no min feature size) */
        const int back = span * factor -1;
        errors += 2;
        for (x = awidth; x > 0; x--)
        {
            int res;
            value = e_forward + *errors;
            for (xx = factor; xx > 0; xx--)
            {
                for (y = factor; y > 0; y--)
                {
                    value += *inp;
                    inp += span;
                }
                inp -= back;
            }
            res = (value+(div>>1))/div;
            *outp++ = res;
            value -= res*div;
            e_forward  = value * 7/16;
            e_downleft = value * 3/16;
            e_down     = value * 5/16;
            value     -= e_forward + e_downleft + e_down;
            errors[-2] += e_downleft;
            errors[-1] += e_down;
            *errors++   = value;
        }
    }
    else
    {
        /* Right to Left pass (no min feature size) */
        const int back = span * factor + 1;
        errors += awidth;
        inp += awidth*factor-1;
        outp += awidth-1;
        for (x = awidth; x > 0; x--)
        {
            int res;
            value = e_forward + *errors;
            for (xx = factor; xx > 0; xx--)
            {
                for (y = factor; y > 0; y--)
                {
                    value += *inp;
                    inp += span;
                }
                inp -= back;
            }
            res = (value + (div>>1))/div;
            *outp-- = res;
            value -= res*div;
            e_forward  = value * 7/16;
            e_downleft = value * 3/16;
            e_down     = value * 5/16;
            value     -= e_forward + e_downleft + e_down;
            errors[2] += e_downleft;
            errors[1] += e_down;
            *errors--   = value;
        }
    }
}
#else
static void down_core8(gx_downscaler_t *ds,
                       byte            *outp,
                       int              row)
{
    int        x, xx, y, value;
    int        pad_white;
    byte      *inp;
    int        width     = ds->width;
    int        awidth    = ds->awidth;
    int        factor    = ds->factor;
    int        span      = ds->span;
    int       *errors    = ds->errors;
    int        div       = factor*factor;

    pad_white = (awidth - width) * factor;
    if (pad_white < 0)
        pad_white = 0;

    if (pad_white)
    {
        inp = ds->data + width*factor;
        for (y = factor; y > 0; y--)
        {
            memset(inp, 0xFF, pad_white);
            inp += span;
        }
    }

    inp = ds->data;
    {
        /* Left to Right pass (no min feature size) */
        const int back = span * factor -1;
        errors += 2;
        for (x = awidth; x > 0; x--)
        {
            value = 0;
            for (xx = factor; xx > 0; xx--)
            {
                for (y = factor; y > 0; y--)
                {
                    value += *inp;
                    inp += span;
                }
                inp -= back;
            }
            *outp++ = (value+(div>>1))/div;
        }
    }
}
#endif

/* RGB downscale/error diffusion code */
#ifdef DO_STUPID_ERROR_DIFFUSION
static void down_core24(gx_downscaler_t *ds,
                        byte            *outp,
                        int              row)
{
    int        x, xx, y, value;
    int        mask;
    int        e_downleft, e_down;
    int        e_forward_r = 0;
    int        e_forward_g = 0;
    int        e_forward_b = 0;
    int        pad_white;
    byte      *inp;
    int        width     = ds->width;
    int        awidth    = ds->awidth;
    int        factor    = ds->factor;
    int        span      = ds->span;
    int       *errors    = ds->errors;
    byte      *mfs_data  = ds->mfs_data;
    int        div       = factor*factor;

    pad_white = (awidth - width) * factor * 3;
    if (pad_white < 0)
        pad_white = 0;

    if (pad_white)
    {
        inp = ds->data + width*factor*3;
        for (y = factor; y > 0; y--)
        {
            memset(inp, 0xFF, pad_white);
            inp += span;
        }
    }

    inp = ds->data;
    if ((row & 1) == 0)
    {
        /* Left to Right pass (no min feature size) */
        const int back  = span * factor - 3;
        const int back2 = factor * 3 - 1;
        errors += 6;
        for (x = awidth; x > 0; x--)
        {
            int res;
            /* R */
            value = e_forward_r + *errors;
            for (xx = factor; xx > 0; xx--)
            {
                for (y = factor; y > 0; y--)
                {
                    value += *inp;
                    inp += span;
                }
                inp -= back;
            }
            inp -= back2;
            res = (value+(div>>1))/div;
            *outp++ = res;
            value -= res*div;
            e_forward_r = value * 7/16;
            e_downleft  = value * 3/16;
            e_down      = value * 5/16;
            value      -= e_forward_r + e_downleft + e_down;
            errors[-2] += e_downleft;
            errors[-1] += e_down;
            *errors++   = value;
            /* G */
            value = e_forward_g + *errors;
            for (xx = factor; xx > 0; xx--)
            {
                for (y = factor; y > 0; y--)
                {
                    value += *inp;
                    inp += span;
                }
                inp -= back;
            }
            inp -= back2;
            res = (value+(div>>1))/div;
            *outp++ = res;
            value -= res*div;
            e_forward_g = value * 7/16;
            e_downleft  = value * 3/16;
            e_down      = value * 5/16;
            value      -= e_forward_g + e_downleft + e_down;
            errors[-2] += e_downleft;
            errors[-1] += e_down;
            *errors++   = value;
            /* B */
            value = e_forward_b + *errors;
            for (xx = factor; xx > 0; xx--)
            {
                for (y = factor; y > 0; y--)
                {
                    value += *inp;
                    inp += span;
                }
                inp -= back;
            }
            inp -= 2;
            res = (value+(div>>1))/div;
            *outp++ = res;
            value -= res*div;
            e_forward_b = value * 7/16;
            e_downleft  = value * 3/16;
            e_down      = value * 5/16;
            value      -= e_forward_b + e_downleft + e_down;
            errors[-2] += e_downleft;
            errors[-1] += e_down;
            *errors++   = value;
        }
    }
    else
    {
        /* Right to Left pass (no min feature size) */
        const int back  = span * factor + 3;
        const int back2 = factor * 3 - 1;
        errors += 3*awidth;
        inp += 3*awidth*factor-1;
        outp += 3*awidth - 1;
        for (x = awidth; x > 0; x--)
        {
            int res;
            /* B */
            value = e_forward_b + *errors;
            for (xx = factor; xx > 0; xx--)
            {
                for (y = factor; y > 0; y--)
                {
                    value += *inp;
                    inp += span;
                }
                inp -= back;
            }
            inp += back2;
            res = (value + (div>>1))/div;
            *outp-- = res;
            value -= res*div;
            e_forward_b = value * 7/16;
            e_downleft  = value * 3/16;
            e_down      = value * 5/16;
            value      -= e_forward_b + e_downleft + e_down;
            errors[2]  += e_downleft;
            errors[1]  += e_down;
            *errors--   = value;
            /* G */
            value = e_forward_g + *errors;
            for (xx = factor; xx > 0; xx--)
            {
                for (y = factor; y > 0; y--)
                {
                    value += *inp;
                    inp += span;
                }
                inp -= back;
            }
            inp += back2;
            res = (value + (div>>1))/div;
            *outp-- = res;
            value -= res*div;
            e_forward_g = value * 7/16;
            e_downleft  = value * 3/16;
            e_down      = value * 5/16;
            value      -= e_forward_g + e_downleft + e_down;
            errors[2]  += e_downleft;
            errors[1]  += e_down;
            *errors--   = value;
            /* R */
            value = e_forward_r + *errors;
            for (xx = factor; xx > 0; xx--)
            {
                for (y = factor; y > 0; y--)
                {
                    value += *inp;
                    inp += span;
                }
                inp -= back;
            }
            inp += 2;
            res = (value + (div>>1))/div;
            *outp-- = res;
            value -= res*div;
            e_forward_r = value * 7/16;
            e_downleft  = value * 3/16;
            e_down      = value * 5/16;
            value      -= e_forward_r + e_downleft + e_down;
            errors[2]  += e_downleft;
            errors[1]  += e_down;
            *errors--   = value;
        }
    }
}
#else
static void down_core24(gx_downscaler_t *ds,
                        byte            *outp,
                        int              row)
{
    int        x, xx, y, value;
    int        pad_white;
    byte      *inp;
    int        width     = ds->width;
    int        awidth    = ds->awidth;
    int        factor    = ds->factor;
    int        span      = ds->span;
    int        div       = factor*factor;

    pad_white = (awidth - width) * factor * 3;
    if (pad_white < 0)
        pad_white = 0;

    if (pad_white)
    {
        inp = ds->data + width*factor*3;
        for (y = factor; y > 0; y--)
        {
            memset(inp, 0xFF, pad_white);
            inp += span;
        }
    }

    inp = ds->data;
    {
        /* Left to Right pass (no min feature size) */
        const int back  = span * factor - 3;
        const int back2 = factor * 3 - 1;
        for (x = awidth; x > 0; x--)
        {
            /* R */
            value = 0;
            for (xx = factor; xx > 0; xx--)
            {
                for (y = factor; y > 0; y--)
                {
                    value += *inp;
                    inp += span;
                }
                inp -= back;
            }
            inp -= back2;
            *outp++ = (value+(div>>1))/div;
            /* G */
            value = 0;
            for (xx = factor; xx > 0; xx--)
            {
                for (y = factor; y > 0; y--)
                {
                    value += *inp;
                    inp += span;
                }
                inp -= back;
            }
            inp -= back2;
            *outp++ = (value+(div>>1))/div;
            /* B */
            value = 0;
            for (xx = factor; xx > 0; xx--)
            {
                for (y = factor; y > 0; y--)
                {
                    value += *inp;
                    inp += span;
                }
                inp -= back;
            }
            inp -= 2;
            *outp++ = (value+(div>>1))/div;
        }
    }
}
#endif

int gx_downscaler_init(gx_downscaler_t   *ds,
                       gx_device         *dev,
                       int                src_bpc,
                       int                dst_bpc,
                       int                num_comps,
                       int                factor,
                       int                mfs,
                       int              (*adjust_width_proc)(int, int),
                       int				  adjust_width)
{
    int                size = gdev_mem_bytes_per_scan_line((gx_device *)dev);
    int                span;
    int                width = dev->width/factor;
    int                awidth = width;
    int                pad_white;
    int                code;
    gx_downscale_core *core;
    
    if (adjust_width != NULL)
        awidth = (*adjust_width_proc)(width, adjust_width);
    pad_white = awidth - width;
    if (pad_white < 0)
        pad_white = 0;

    span = size + pad_white*factor*num_comps + factor-1;
    memset(ds, 0, sizeof(*ds));
    ds->dev    = dev;
    ds->width  = width;
    ds->awidth = awidth;
    ds->span   = span;
    ds->factor = factor;
    
    /* Choose an appropriate core */
    if ((src_bpc == 8) && (dst_bpc == 1) && (num_comps == 1))
        core = &down_core;
    else if (factor == 1)
        core = NULL;
    else if ((src_bpc == 8) && (dst_bpc == 8) && (num_comps == 1))
        core = &down_core8;
    else if ((src_bpc == 8) && (dst_bpc == 8) && (num_comps == 3))
        core = &down_core24;
    else {
        code = gs_note_error(gs_error_rangecheck);
        goto cleanup;
    }
    ds->down_core = core;
    
    if (core != NULL) {
        ds->data = gs_alloc_bytes(dev->memory,
                                  span * factor,
                                  "gx_downscaler(data)");
        if (ds->data == NULL)
            return_error(gs_error_VMerror);
    
        if (mfs > 1) {
            ds->mfs_data = (byte *)gs_alloc_bytes(dev->memory,
                                                  awidth+1,
                                                  "gx_downscaler(mfs)");
            if (ds->mfs_data == NULL) {
                code = gs_note_error(gs_error_VMerror);
                goto cleanup;
            }
            memset(ds->mfs_data, 0, awidth+1);
        }
#ifndef DO_STUPID_ERROR_DIFFUSION
        if (dst_bpc == 1)
#endif
        {
            ds->errors = (int *)gs_alloc_bytes(dev->memory,
                                               num_comps*(awidth+3)*sizeof(int),
                                               "gx_downscaler(errors)");
            if (ds->errors == NULL)
            {
                code = gs_note_error(gs_error_VMerror);
                goto cleanup;
            }
            memset(ds->errors, 0, num_comps * (awidth+3) * sizeof(int));
        }
    }
    
    return 0;

  cleanup:
    gx_downscaler_fin(ds);
    return code;
}

void gx_downscaler_fin(gx_downscaler_t *ds)
{
    gs_free_object(ds->dev->memory, ds->mfs_data, "gx_downscaler(mfs)");
    ds->mfs_data = NULL;
    gs_free_object(ds->dev->memory, ds->errors, "gx_downscaler(errors)");
    ds->errors = NULL;
    gs_free_object(ds->dev->memory, ds->data, "gx_downscaler(data)");
    ds->data = NULL;
}

int gx_downscaler_getbits(gx_downscaler_t *ds,
                          byte            *out_data,
                          int              row)
{
    int   code = 0;
    int   y, y_end;
    byte *data_ptr;

    /* Check for the simple case */
    if (ds->down_core == NULL) {
        return (*dev_proc(ds->dev, get_bits))(ds->dev, row, out_data, NULL);
    }

    /* Get factor rows worth of data */
    y        = row * ds->factor;
    y_end    = y + ds->factor;
    data_ptr = ds->data;
    do {
        code = (*dev_proc(ds->dev, get_bits))(ds->dev, y, data_ptr, NULL);
        if (code < 0)
            return code;
        data_ptr += ds->span;
        y++;
    } while (y < y_end);
    
    (ds->down_core)(ds, out_data, row);

    return code;
}

int
gx_downscaler_copy_scan_lines(gx_downscaler_t *ds, int y,
                              byte * str, uint size)
{
    uint line_size = gx_device_raster(ds->dev, 0);
    int count = size / line_size;
    int i;
    int height = ds->dev->height/ds->factor;
    byte *dest = str;

    count = min(count, height - y);
    for (i = 0; i < count; i++, dest += line_size) {
        int code = gx_downscaler_getbits(ds, dest, y+i);
        if (code < 0)
            return code;
    }
    return count;
}
