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


#include "gxdownscale.h"
#include "gserrors.h"
#include "string_.h"
#include "gdevprn.h"
#include "assert_.h"

#ifdef WITH_CAL
#include "cal_ets.h"
#else
#include "ets.h"
#endif

/* Nasty inline declaration, as gxht_thresh.h requires penum */
void gx_ht_threshold_row_bit_sub(byte *contone,  byte *threshold_strip,
                             int contone_stride, byte *halftone,
                             int dithered_stride, int width, int num_rows,
                             int offset_bits);

enum
{
    MAX_ETS_PLANES = 8
};

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

/* Subsidiary function to pack the data from 8 bits to 1 */
static void pack_8to1(byte *outp, byte *inp, int w)
{
    int mask  = 128;
    int value = 0;
    for (; w > 0; w--)
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

static void down_core(gx_downscaler_t *ds,
                      byte            *out_buffer,
                      byte            *in_buffer,
                      int              row,
                      int              plane,
                      int              span)
{
    int        x, xx, y, value;
    int        e_downleft, e_down, e_forward = 0;
    int        pad_white;
    byte      *inp, *outp;
    int        width     = ds->width;
    int        awidth    = ds->awidth;
    int        factor    = ds->factor;
    int       *errors    = ds->errors + (awidth+3)*plane;
    const int  threshold = factor*factor*128;
    const int  max_value = factor*factor*255;

    pad_white = (awidth - width) * factor;
    if (pad_white < 0)
        pad_white = 0;

    if (pad_white)
    {
        inp = in_buffer + width*factor;
        for (y = factor; y > 0; y--)
        {
            memset(inp, 0xFF, pad_white);
            inp += span;
        }
    }

    inp = in_buffer;
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
    pack_8to1(out_buffer, outp, awidth);
}

static void down_core_ets_1(gx_downscaler_t *ds,
                            byte            *out_buffer,
                            byte            *in_buffer,
                            int              row,
                            int              plane,
                            int              span)
{
    unsigned char *dest[MAX_ETS_PLANES];
    ETS_SrcPixel *src[MAX_ETS_PLANES];
    int pad_white, y;
    int factor = ds->factor;

    pad_white = (ds->awidth - ds->width) * factor * 4;
    if (pad_white < 0)
        pad_white = 0;

    if (pad_white)
    {
        unsigned char *inp = in_buffer + ds->width * factor * 4;
        for (y = factor; y > 0; y--)
        {
            memset(inp, 0xFF, pad_white);
            inp += span;
        }
    }

    if (ds->ets_downscale)
        ds->ets_downscale(ds, in_buffer, in_buffer, row, plane, span);

    src[0] = in_buffer;
    dest[0] = in_buffer;
    ets_line((ETS_Ctx *)ds->ets_config, dest, (const ETS_SrcPixel * const *)src);

    pack_8to1(out_buffer, in_buffer, ds->awidth);
}

static void down_core_1(gx_downscaler_t *ds,
                        byte            *out_buffer,
                        byte            *in_buffer,
                        int              row,
                        int              plane,
                        int              span)
{
    int        x, value;
    int        e_downleft, e_down, e_forward = 0;
    int        pad_white;
    byte      *inp, *outp;
    int        width     = ds->width;
    int        awidth    = ds->awidth;
    int       *errors    = ds->errors + (awidth+3)*plane;
    const int  threshold = 128;
    const int  max_value = 255;

    pad_white = (awidth - width);
    if (pad_white < 0)
        pad_white = 0;

    if (pad_white)
    {
        memset(in_buffer + width, 0xFF, pad_white);
    }

    inp = in_buffer;
    if ((row & 1) == 0)
    {
        /* Left to Right pass (no min feature size) */
        errors += 2;
        outp = inp;
        for (x = awidth; x > 0; x--)
        {
            value = e_forward + *errors + *inp++;
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
        errors += awidth;
        inp += awidth-1;
        outp = inp;
        for (x = awidth; x > 0; x--)
        {
            value = e_forward + *errors + *inp--;
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
    pack_8to1(out_buffer, outp, awidth);
}

static void down_core_2(gx_downscaler_t *ds,
                        byte            *out_buffer,
                        byte            *in_buffer,
                        int              row,
                        int              plane,
                        int              span)
{
    int        x, value;
    int        e_downleft, e_down, e_forward = 0;
    int        pad_white;
    byte      *inp, *outp;
    int        width     = ds->width;
    int        awidth    = ds->awidth;
    int       *errors    = ds->errors + (awidth+3)*plane;
    const int  threshold = 2*2*128;
    const int  max_value = 2*2*255;

    pad_white = (awidth - width) * 2;
    if (pad_white < 0)
        pad_white = 0;

    if (pad_white)
    {
        inp = in_buffer + width*2;
        for (x = 2; x > 0; x--)
        {
            memset(inp, 0xFF, pad_white);
            inp += span;
        }
    }

    inp = in_buffer;
    if ((row & 1) == 0)
    {
        /* Left to Right pass (no min feature size) */
        errors += 2;
        outp = inp;
        for (x = awidth; x > 0; x--)
        {
            value = e_forward + *errors + inp[0] + inp[1] + inp[span] + inp[span+1];
            inp += 2;
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
        errors += awidth;
        inp += (awidth-1)*2;
        outp = inp;
        for (x = awidth; x > 0; x--)
        {
            value = e_forward + *errors + inp[0] + inp[1] + inp[span] + inp[span+1];
            inp -= 2;
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
    pack_8to1(out_buffer, outp, awidth);
}

static void down_core_3(gx_downscaler_t *ds,
                        byte            *out_buffer,
                        byte            *in_buffer,
                        int              row,
                        int              plane,
                        int              span)
{
    int        x, value;
    int        e_downleft, e_down, e_forward = 0;
    int        pad_white;
    byte      *inp, *outp;
    int        width     = ds->width;
    int        awidth    = ds->awidth;
    int       *errors    = ds->errors + (awidth+3)*plane;
    const int  threshold = 3*3*128;
    const int  max_value = 3*3*255;

    pad_white = (awidth - width) * 3;
    if (pad_white < 0)
        pad_white = 0;

    if (pad_white)
    {
        inp = in_buffer + width*3;
        for (x = 3; x > 0; x--)
        {
            memset(inp, 0xFF, pad_white);
            inp += span;
        }
    }

    inp = in_buffer;
    if ((row & 1) == 0)
    {
        /* Left to Right pass (no min feature size) */
        errors += 2;
        outp = inp;
        for (x = awidth; x > 0; x--)
        {
            value = e_forward + *errors +
                    inp[     0] + inp[       1] + inp[       2] +
                    inp[span  ] + inp[span  +1] + inp[span  +2] +
                    inp[span*2] + inp[span*2+1] + inp[span*2+2];
            inp += 3;
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
        errors += awidth;
        inp += (awidth-1)*3;
        outp = inp;
        for (x = awidth; x > 0; x--)
        {
            value = e_forward + *errors +
                    inp[     0] + inp[       1] + inp[       2] +
                    inp[span  ] + inp[span  +1] + inp[span  +2] +
                    inp[span*2] + inp[span*2+1] + inp[span*2+2];
            inp -= 3;
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
    pack_8to1(out_buffer, outp, awidth);
}

static void down_core_4(gx_downscaler_t *ds,
                        byte            *out_buffer,
                        byte            *in_buffer,
                        int              row,
                        int              plane,
                        int              span)
{
    int        x, value;
    int        e_downleft, e_down, e_forward = 0;
    int        pad_white;
    byte      *inp, *outp;
    int        width     = ds->width;
    int        awidth    = ds->awidth;
    int       *errors    = ds->errors + (awidth+3)*plane;
    const int  threshold = 4*4*128;
    const int  max_value = 4*4*255;

    pad_white = (awidth - width) * 4;
    if (pad_white < 0)
        pad_white = 0;

    if (pad_white)
    {
        inp = in_buffer + width*4;
        for (x = 4; x > 0; x--)
        {
            memset(inp, 0xFF, pad_white);
            inp += span;
        }
    }

    inp = in_buffer;
    if ((row & 1) == 0)
    {
        /* Left to Right pass (no min feature size) */
        errors += 2;
        outp = inp;
        for (x = awidth; x > 0; x--)
        {
            value = e_forward + *errors +
                    inp[     0] + inp[       1] + inp[       2] + inp[3       ] +
                    inp[span  ] + inp[span  +1] + inp[span  +2] + inp[span  +3] +
                    inp[span*2] + inp[span*2+1] + inp[span*2+2] + inp[span*2+3] +
                    inp[span*3] + inp[span*3+1] + inp[span*3+2] + inp[span*3+3];
            inp += 4;
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
        errors += awidth;
        inp += (awidth-1)*4;
        outp = inp;
        for (x = awidth; x > 0; x--)
        {
            value = e_forward + *errors +
                    inp[     0] + inp[       1] + inp[       2] + inp[3       ] +
                    inp[span  ] + inp[span  +1] + inp[span  +2] + inp[span  +3] +
                    inp[span*2] + inp[span*2+1] + inp[span*2+2] + inp[span*2+3] +
                    inp[span*3] + inp[span*3+1] + inp[span*3+2] + inp[span*3+3];
            inp -= 4;
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
    pack_8to1(out_buffer, outp, awidth);
}

static void down_core_mfs(gx_downscaler_t *ds,
                          byte            *out_buffer,
                          byte            *in_buffer,
                          int              row,
                          int              plane,
                          int              span)
{
    int        x, xx, y, value;
    int        e_downleft, e_down, e_forward = 0;
    int        pad_white;
    byte      *inp, *outp;
    int        width     = ds->width;
    int        awidth    = ds->awidth;
    int        factor    = ds->factor;
    int       *errors    = ds->errors + (awidth+3)*plane;
    byte      *mfs_data  = ds->mfs_data + (awidth+1)*plane;
    const int  threshold = factor*factor*128;
    const int  max_value = factor*factor*255;

    pad_white = (awidth - width) * factor;
    if (pad_white < 0)
        pad_white = 0;

    if (pad_white)
    {
        inp = in_buffer + width*factor;
        for (y = factor; y > 0; y--)
        {
            memset(inp, 0xFF, pad_white);
            inp += span;
        }
    }

    inp = in_buffer;
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
        *mfs_data-- = mfs_clear;
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
    pack_8to1(out_buffer, outp, awidth);
}

/* CMYK 32 -> 4bit core */
static void down_core4(gx_downscaler_t *ds,
                       byte            *out_buffer,
                       byte            *in_buffer,
                       int              row,
                       int              plane /* unused */,
                       int              span)
{
    int        x, xx, y, value, comp;
    int        e_downleft, e_down, e_forward = 0;
    int        pad_white;
    byte      *inp, *outp;
    int        width     = ds->width;
    int        awidth    = ds->awidth;
    int        factor    = ds->factor;
    int       *errors    = ds->errors;
    const int  threshold = factor*factor*128;
    const int  max_value = factor*factor*255;

    pad_white = (awidth - width) * factor * 4;
    if (pad_white < 0)
        pad_white = 0;

    if (pad_white)
    {
        inp = in_buffer + width*factor * 4;
        for (y = factor; y > 0; y--)
        {
            memset(inp, 0xFF, pad_white);
            inp += span;
        }
    }

    if ((row & 1) == 0)
    {
        /* Left to Right pass (no min feature size) */
        const int back = span * factor - 4;
        for (comp = 0; comp < 4; comp++)
        {
            errors = ds->errors + (awidth+3)*comp + 2;
            inp = in_buffer + comp;
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
                    *outp = 1; outp += 4;
                    value -= max_value;
                }
                else
                {
                    *outp = 0; outp += 4;
                }
                e_forward  = value * 7/16;
                e_downleft = value * 3/16;
                e_down     = value * 5/16;
                value     -= e_forward + e_downleft + e_down;
                errors[-2] += e_downleft;
                errors[-1] += e_down;
                *errors++   = value;
            }
        }
        outp = in_buffer;
    }
    else
    {
        /* Right to Left pass (no min feature size) */
        const int back = span * factor + 4;
        for (comp = 0; comp < 4; comp++)
        {
            errors = ds->errors + (awidth+3)*comp + awidth;
            inp = in_buffer + awidth*factor*4 - 4 + comp;
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
                    *outp = 1; outp -= 4;
                    value -= max_value;
                }
                else
                {
                    *outp = 0; outp -= 4;
                }
                e_forward  = value * 7/16;
                e_downleft = value * 3/16;
                e_down     = value * 5/16;
                value     -= e_forward + e_downleft + e_down;
                errors[2] += e_downleft;
                errors[1] += e_down;
                *errors--   = value;
            }
        }
        outp = in_buffer + awidth*factor*4 - (awidth*4);
    }
    pack_8to1(out_buffer, outp, awidth*4);
}

static void down_core4_ht(gx_downscaler_t *ds,
                          byte            *out_buffer, /* Guaranteed aligned */
                          byte            *in_buffer,  /* Not guaranteed aligned */
                          int              row,
                          int              plane /* unused */,
                          int              span)
{
    int pad_white, y;
    int factor = ds->factor;
    int i;
    int nc = ds->early_cm ? ds->post_cm_num_comps : ds->num_comps;
    byte *downscaled_data = ds->inbuf;

    pad_white = (ds->awidth - ds->width) * factor * 4;
    if (pad_white < 0)
        pad_white = 0;

    if (pad_white)
    {
        unsigned char *inp = in_buffer + ds->width * factor * 4;
        for (y = factor; y > 0; y--)
        {
            memset(inp, 0xFF, pad_white);
            inp += span;
        }
    }

    /* Color conversion has already happened. Do any downscale required. */
    if (ds->ets_downscale)
        ds->ets_downscale(ds, downscaled_data, in_buffer, row, plane, span);
    else if ((31 & (intptr_t)in_buffer) == 0)
        downscaled_data = in_buffer; /* Already aligned! Yay! */
    else
        memcpy(downscaled_data, in_buffer, nc*ds->width); /* Copy to align */

    /* Do the halftone */
    for (i = 0; i < nc; i++)
    {
        /* Make the expanded threshold row */
        byte *d = ds->htrow + i;
        int len = ds->width;
        const byte *srow = ds->ht[i].data + ds->ht[i].stride * ((row + ds->ht[i].y_phase) % ds->ht[i].h);
        {
            int o = ds->ht[i].x_phase;
            int run = ds->ht[i].w - o;
            const byte *s = &srow[o];
            if (run > len)
                run = len;
            len -= run;
            do {
                *d = *s++;
                d += nc;
            } while (--run);
        }
        while (len)
        {
            const byte *s = srow;
            int run = ds->ht[i].w;
            if (run > len)
                run = len;
            len -= run;
            do {
                *d = *s++;
                d += nc;
            }
            while (--run);
        }
    }

    /* Do the halftone */
    gx_ht_threshold_row_bit_sub(downscaled_data, ds->htrow, 0,
                                out_buffer, 0,
                                ds->width * nc, 1, 0);
}

static void down_core4_ets(gx_downscaler_t *ds,
                           byte            *out_buffer,
                           byte            *in_buffer,
                           int              row,
                           int              plane /* unused */,
                           int              span)
{
    unsigned char *dest[MAX_ETS_PLANES];
    ETS_SrcPixel *src[MAX_ETS_PLANES];
    int pad_white, y;
    int factor = ds->factor;

    pad_white = (ds->awidth - ds->width) * factor * 4;
    if (pad_white < 0)
        pad_white = 0;

    if (pad_white)
    {
        unsigned char *inp = in_buffer + ds->width * factor * 4;
        for (y = factor; y > 0; y--)
        {
            memset(inp, 0xFF, pad_white);
            inp += span;
        }
    }

    if (ds->ets_downscale)
        ds->ets_downscale(ds, in_buffer, in_buffer, row, plane, span);

    src[0] = in_buffer+3;
    dest[0] = in_buffer+3;
    src[1] = in_buffer+1;
    dest[1] = in_buffer+1;
    src[2] = in_buffer+0;
    dest[2] = in_buffer+0;
    src[3] = in_buffer+2;
    dest[3] = in_buffer+2;
    ets_line((ETS_Ctx *)ds->ets_config, dest, (const ETS_SrcPixel * const *)src);

    pack_8to1(out_buffer, in_buffer, ds->awidth * 4);
}

static void down_core4_mfs(gx_downscaler_t *ds,
                           byte            *out_buffer,
                           byte            *in_buffer,
                           int              row,
                           int              plane /* unused */,
                           int              span)
{
    int        x, xx, y, value, comp;
    int        e_downleft, e_down, e_forward = 0;
    int        pad_white;
    byte      *inp, *outp;
    int        width     = ds->width;
    int        awidth    = ds->awidth;
    int        factor    = ds->factor;
    int       *errors;
    const int  threshold = factor*factor*128;
    const int  max_value = factor*factor*255;
    byte      *mfs_data;

    pad_white = (awidth - width) * factor * 4;
    if (pad_white < 0)
        pad_white = 0;

    if (pad_white)
    {
        inp = in_buffer + width*factor*4;
        for (y = factor*4; y > 0; y--)
        {
            memset(inp, 0xFF, pad_white);
            inp += span;
        }
    }

    if ((row & 1) == 0)
    {
        /* Left to Right pass (with min feature size = 2) */
        const int back = span * factor - 4;
        for (comp = 0; comp < 4; comp++)
        {
            byte mfs, force_forward = 0;
            errors = ds->errors + (awidth+3)*comp + 2;
            inp = in_buffer + comp;
            outp = inp;
            mfs_data = ds->mfs_data + (awidth+1)*comp;
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
                    *outp = 1; outp += 4;
                    value -= max_value;
                    force_forward = 0;
                }
                else if (value >= threshold)
                {
                    /* We want to be 1 anyway */
                    *outp = 1; outp += 4;
                    value -= max_value;
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
                         * we were 1. */
                        mfs_data[-2] |= mfs_above_is_0;
                        mfs_data[-1] |= mfs_above_left_is_0;
                    }
                }
                else
                {
                    *outp = 0; outp += 4;
                }
                e_forward  = value * 7/16;
                e_downleft = value * 3/16;
                e_down     = value * 5/16;
                value     -= e_forward + e_downleft + e_down;
                errors[-2] += e_downleft;
                errors[-1] += e_down;
                *errors++   = value;
            }
        }
        outp = in_buffer;
    }
    else
    {
        /* Right to Left pass (with min feature size = 2) */
        const int back = span * factor + 4;
        for (comp = 0; comp < 4; comp++)
        {
            byte mfs, force_forward = 0;
            errors = ds->errors + (awidth+3)*comp + awidth;
            inp = in_buffer + awidth*factor*4 - 4 + comp;
            outp = inp;
            mfs_data = ds->mfs_data + (awidth+1)*comp + awidth;
            *mfs_data-- = mfs_clear;
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
                    *outp = 1; outp -= 4;
                    value -= max_value;
                    force_forward = 0;
                }
                else if (value >= threshold)
                {
                    *outp = 1; outp -= 4;
                    value -= max_value;
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
                         * we were 1. */
                        mfs_data[1] |= mfs_above_is_0;
                        mfs_data[2] |= mfs_above_left_is_0;
                    }
                }
                else
                {
                    *outp = 0; outp -= 4;
                }
                e_forward  = value * 7/16;
                e_downleft = value * 3/16;
                e_down     = value * 5/16;
                value     -= e_forward + e_downleft + e_down;
                errors[2] += e_downleft;
                errors[1] += e_down;
                *errors--   = value;
            }
        }
        outp = in_buffer + awidth*factor*4 - (awidth*4);
    }
    pack_8to1(out_buffer, outp, awidth*4);
}

/* Grey (or planar) downscale code */
static void down_core16(gx_downscaler_t *ds,
                        byte            *outp,
                        byte            *in_buffer,
                        int              row,
                        int              plane,
                        int              span)
{
    int   x, xx, y, value;
    int   pad_white;
    byte *inp;
    int   width  = ds->width;
    int   awidth = ds->awidth;
    int   factor = ds->factor;
    int   div    = factor*factor;

    pad_white = (awidth - width) * factor;
    if (pad_white < 0)
        pad_white = 0;

    if (pad_white)
    {
        inp = in_buffer + width*2*factor;
        for (y = factor; y > 0; y--)
        {
            memset(inp, 0xFF, pad_white*2);
            inp += span;
        }
    }

    inp = in_buffer;
    {
        /* Left to Right pass (no min feature size) */
        const int back = span * factor -2;
        for (x = awidth; x > 0; x--)
        {
            value = 0;
            for (xx = factor; xx > 0; xx--)
            {
                for (y = factor; y > 0; y--)
                {
                    value += inp[0]<<8;
                    value += inp[1];
                    inp += span;
                }
                inp -= back;
            }
            value = (value + (div>>1))/div;
            outp[0] = value>>8;
            outp[1] = value;
            outp += 2;
        }
    }
}

static void down_core8(gx_downscaler_t *ds,
                       byte            *outp,
                       byte            *in_buffer,
                       int              row,
                       int              plane,
                       int              span)
{
    int   x, xx, y, value;
    int   pad_white;
    byte *inp;
    int   width  = ds->width;
    int   awidth = ds->awidth;
    int   factor = ds->factor;
    int   div    = factor*factor;

    pad_white = (awidth - width) * factor;
    if (pad_white < 0)
        pad_white = 0;

    if (pad_white)
    {
        inp = in_buffer + width*factor;
        for (y = factor; y > 0; y--)
        {
            memset(inp, 0xFF, pad_white);
            inp += span;
        }
    }

    inp = in_buffer;
    {
        /* Left to Right pass (no min feature size) */
        const int back = span * factor -1;
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

static void down_core8_2(gx_downscaler_t *ds,
                         byte            *outp,
                         byte            *in_buffer,
                         int              row,
                         int              plane,
                         int              span)
{
    int   x;
    int   pad_white;
    byte *inp;
    int   width  = ds->width;
    int   awidth = ds->awidth;

    pad_white = (awidth - width) * 2;
    if (pad_white < 0)
        pad_white = 0;

    if (pad_white)
    {
        inp = in_buffer + width*2;
        for (x = 2; x > 0; x--)
        {
            memset(inp, 0xFF, pad_white);
            inp += span;
        }
    }

    inp = in_buffer;

    /* Left to Right pass (no min feature size) */
    for (x = awidth; x > 0; x--)
    {
        *outp++ = (inp[0] + inp[1] + inp[span] + inp[span+1] + 2)>>2;
        inp += 2;
    }
}

static void down_core8_3(gx_downscaler_t *ds,
                         byte            *outp,
                         byte            *in_buffer,
                         int              row,
                         int              plane,
                         int              span)
{
    int   x;
    int   pad_white;
    byte *inp;
    int   width  = ds->width;
    int   awidth = ds->awidth;

    pad_white = (awidth - width) * 3;
    if (pad_white < 0)
        pad_white = 0;

    if (pad_white)
    {
        inp = in_buffer + width*3;
        for (x = 3; x > 0; x--)
        {
            memset(inp, 0xFF, pad_white);
            inp += span;
        }
    }

    inp = in_buffer;

    /* Left to Right pass (no min feature size) */
    for (x = awidth; x > 0; x--)
    {
        *outp++ = (inp[0     ] + inp[       1] + inp[       2] +
                   inp[span  ] + inp[span  +1] + inp[span  +2] +
                   inp[span*2] + inp[span*2+1] + inp[span*2+2] + 4)/9;
        inp += 3;
    }
}

static void down_core8_4(gx_downscaler_t *ds,
                         byte            *outp,
                         byte            *in_buffer,
                         int              row,
                         int              plane,
                         int              span)
{
    int   x;
    int   pad_white;
    byte *inp;
    int   width  = ds->width;
    int   awidth = ds->awidth;

    pad_white = (awidth - width) * 4;
    if (pad_white < 0)
        pad_white = 0;

    if (pad_white)
    {
        inp = in_buffer + width*4;
        for (x = 4; x > 0; x--)
        {
            memset(inp, 0xFF, pad_white);
            inp += span;
        }
    }

    inp = in_buffer;

    /* Left to Right pass (no min feature size) */
    for (x = awidth; x > 0; x--)
    {
        *outp++ = (inp[0     ] + inp[       1] + inp[       2] + inp[       3] +
                   inp[span  ] + inp[span  +1] + inp[span  +2] + inp[span  +3] +
                   inp[span*2] + inp[span*2+1] + inp[span*2+2] + inp[span*2+3] +
                   inp[span*3] + inp[span*3+1] + inp[span*3+2] + inp[span*3+3] +
                   8)>>4;
        inp += 4;
    }
}

static void down_core8_3_2(gx_downscaler_t *ds,
                           byte            *outp,
                           byte            *in_buffer,
                           int              row,
                           int              plane,
                           int              span)
{
    int   x;
    int   pad_white;
    byte *inp;
    int   width  = ds->width;
    int   awidth = ds->awidth;
    int   dspan  = ds->scaled_span;

    pad_white = (awidth - width) * 3 / 2;
    if (pad_white < 0)
        pad_white = 0;

    if (pad_white)
    {
        inp = in_buffer + width*3/2;
        for (x = 2; x > 0; x--)
        {
            memset(inp, 0xFF, pad_white);
            inp += span;
        }
    }

    inp = in_buffer;

    /* Left to Right pass (no min feature size) */
    for (x = awidth/2; x > 0; x--)
    {
        int a = inp[       0];
        int b = inp[       1];
        int c = inp[       2];
        int d = inp[  span+0];
        int e = inp[  span+1];
        int f = inp[  span+2];
        int g = inp[2*span+0];
        int h = inp[2*span+1];
        int i = inp[2*span+2];
        outp[0      ] = (4*a+2*b+2*d+e+4)/9;
        outp[1      ] = (4*c+2*b+2*f+e+4)/9;
        outp[dspan+0] = (4*g+2*h+2*d+e+4)/9;
        outp[dspan+1] = (4*i+2*h+2*f+e+4)/9;
        outp += 2;
        inp += 3;
    }
}

static void down_core8_3_4(gx_downscaler_t *ds,
                           byte            *outp,
                           byte            *in_buffer,
                           int              row,
                           int              plane,
                           int              span)
{
    int   x;
    int   pad_white;
    byte *inp;
    int   width  = ds->width;
    int   awidth = ds->awidth;
    int   dspan  = ds->scaled_span;

    pad_white = (awidth - width) * 3 / 4;
    if (pad_white < 0)
        pad_white = 0;

    if (pad_white)
    {
        inp = in_buffer + width*3/4;
        for (x = 4; x > 0; x--)
        {
            memset(inp, 0xFF, pad_white);
            inp += span;
        }
    }

    inp = in_buffer;

    /* Left to Right pass (no min feature size) */
    for (x = awidth/4; x > 0; x--)
    {
        int a = inp[       0];
        int b = inp[       1];
        int c = inp[       2];
        int d = inp[  span+0];
        int e = inp[  span+1];
        int f = inp[  span+2];
        int g = inp[2*span+0];
        int h = inp[2*span+1];
        int i = inp[2*span+2];
        outp[        0] = a;
        outp[        1] = (a+2*b+1)/3;
        outp[        2] = (c+2*b+1)/3;
        outp[        3] = c;
        outp[  dspan+0] = (a+2*d+1)/3;
        outp[  dspan+1] = (a+2*b+2*d+4*e+3)/9;
        outp[  dspan+2] = (c+2*b+2*f+4*e+3)/9;
        outp[  dspan+3] = (c+2*f+1)/3;
        outp[2*dspan+0] = (g+2*d+1)/3;
        outp[2*dspan+1] = (g+2*h+2*d+4*e+3)/9;
        outp[2*dspan+2] = (i+2*h+2*f+4*e+3)/9;
        outp[2*dspan+3] = (i+2*f+1)/3;
        outp[3*dspan+0] = g;
        outp[3*dspan+1] = (g+2*h+1)/3;
        outp[3*dspan+2] = (i+2*h+1)/3;
        outp[3*dspan+3] = i;
        outp += 4;
        inp += 3;
    }
}

/* RGB downscale (no error diffusion) code */

static void down_core24(gx_downscaler_t *ds,
                        byte            *outp,
                        byte            *in_buffer,
                        int              row,
                        int              plane,
                        int              span)
{
    int   x, xx, y, value;
    int   pad_white;
    byte *inp;
    int   width  = ds->width;
    int   awidth = ds->awidth;
    int   factor = ds->factor;
    int   div    = factor*factor;

    pad_white = (awidth - width) * factor * 3;
    if (pad_white < 0)
        pad_white = 0;

    if (pad_white)
    {
        inp = in_buffer + width*factor*3;
        for (y = factor; y > 0; y--)
        {
            memset(inp, 0xFF, pad_white);
            inp += span;
        }
    }

    inp = in_buffer;
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

/* CMYK downscale (no error diffusion) code */

static void down_core32(gx_downscaler_t *ds,
                        byte            *outp,
                        byte            *in_buffer,
                        int              row,
                        int              plane,
                        int              span)
{
    int   x, xx, y, value;
    int   pad_white;
    byte *inp;
    int   width  = ds->width;
    int   awidth = ds->awidth;
    int   factor = ds->factor;
    int   div    = factor*factor;

    pad_white = (awidth - width) * factor * 4;
    if (pad_white < 0)
        pad_white = 0;

    if (pad_white)
    {
        inp = in_buffer + width*factor*4;
        for (y = factor; y > 0; y--)
        {
            memset(inp, 0xFF, pad_white);
            inp += span;
        }
    }

    inp = in_buffer;
    {
        /* Left to Right pass (no min feature size) */
        const int back  = span * factor - 4;
        const int back2 = factor * 4 - 1;
        for (x = awidth; x > 0; x--)
        {
            /* C */
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
            /* M */
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
            /* Y */
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
            /* K */
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
            inp -= 3;
            *outp++ = (value+(div>>1))/div;
        }
    }
}

static void decode_factor(int factor, int *up, int *down)
{
    if (factor == 32)
        *down = 3, *up = 2;
    else if (factor == 34)
        *down = 3, *up = 4;
    else
        *down = factor, *up = 1;
}

int
gx_downscaler_scale(int width, int factor)
{
    int up, down;

    decode_factor(factor, &up, &down);
    return (width*up)/down;
}

int gx_downscaler_adjust_bandheight(int factor, int band_height)
{
    int up, down;

    decode_factor(factor, &up, &down);
    return (band_height/down)*down;
}

int
gx_downscaler_scale_rounded(int width, int factor)
{
    int up, down;

    decode_factor(factor, &up, &down);
    return (width*up + down-1)/down;
}

static int get_planar_line_for_trap(void *arg, unsigned char *buf)
{
    gx_downscaler_t *ds = (gx_downscaler_t *)arg;
    gs_int_rect rect;
    gs_get_bits_params_t params; /* params (if trapping) */
    int nc = ds->num_planes;
    int i, code;
    unsigned char *buf2;

    rect.p.x = 0;
    rect.p.y = ds->claptrap_y++;
    rect.q.x = ds->dev->width;
    rect.q.y = rect.p.y+1;
    /* Allow for devices (like psdcmyk) that make several passes through
     * the image. */
    if (ds->claptrap_y == ds->dev->height)
        ds->claptrap_y = 0;

    params = *ds->claptrap_params;
    buf2 = buf;
    for (i = 0; i < nc; i++)
    {
        params.data[i] = buf2;
        buf2 += ds->width;
    }

    code = (*dev_proc(ds->dev, get_bits_rectangle))(ds->dev, &rect, &params, NULL);
    if (code < 0)
        return code;

    /* Now cope with the fact we might have been returned pointers */
    for (i = 0; i < nc; i++)
    {
        if (params.data[i] != buf)
            memcpy(buf, params.data[i], ds->width);
        buf += ds->width;
    }

    return code;
}

static int check_trapping(gs_memory_t *memory, int trap_w, int trap_h,
                          int num_comps, const int *comp_order)
{
    if (trap_w < 0 || trap_h < 0)
    {
        dmprintf(memory, "Trapping range must be >= 0");
        return_error(gs_error_rangecheck);
    }

    if (trap_w > 0 || trap_h > 0)
    {
        if (comp_order == NULL)
        {
            emprintf(memory, "Trapping cannot be used without comp_order being defined");
            return_error(gs_error_rangecheck);
        }

        /* Check that the comp_order we have been passed is sane */
        {
            char comps[GS_CLIENT_COLOR_MAX_COMPONENTS] = { 0 };
            int i;

            for (i = 0; i < num_comps; i++)
            {
                int n = comp_order[i];
                if (n < 0 || n >= num_comps || comps[n] != 0)
                    break;
                comps[n] = 1;
            }
            if (i != num_comps)
            {
                emprintf(memory, "Illegal component order passed to trapping");
                return_error(gs_error_rangecheck);
            }
        }
    }
    return 0;
}

static void
find_aspect_ratio(float *res, int *a, int *b)
{
    float xres = res[0];
    float yres = res[1];
    float f;

    if (xres == yres) {
        *a = *b = 1;
        return;
    }
    else if (xres > yres)
    {
        xres /= yres;
        f = xres - (int)xres;
        if (f >= 0.2 && f < 0.3)
            xres *= 4, yres = 4;
        else if (f >= 0.3 && f < 0.4)
            xres *= 3, yres = 3;
        else if (f >= 0.4 && f < 0.6)
            xres *= 2, yres = 2;
        else if (f >= 0.6 && f < 0.7)
            xres *= 3, yres = 3;
        else if (f >= 0.7 && f < 0.8)
            xres *= 4, yres = 4;
        else
            yres = 1;
        *a = (int)(xres + 0.5);
        *b = (int)yres;
    }
    else
    {
        yres /= xres;
        f = yres - (int)yres;
        if (f >= 0.2 && f < 0.3)
            yres *= 4, xres = 4;
        else if (f >= 0.3 && f < 0.4)
            yres *= 3, xres = 3;
        else if (f >= 0.4 && f < 0.6)
            yres *= 2, xres = 2;
        else if (f >= 0.6 && f < 0.7)
            yres *= 3, xres = 3;
        else if (f >= 0.7 && f < 0.8)
            yres *= 4, xres = 4;
        else
            xres = 1;
        *a = (int)xres;
        *b = (int)(yres + 0.5);
    }
}

static int init_ets(gx_downscaler_t *ds, int num_planes, gx_downscale_core *downscale_core)
{
    ETS_Params params = { 0 };
    int strengths[MAX_ETS_PLANES] = { 128, 51, 51, 13, 13, 13, 13, 13 };
    int lut[ETS_SRC_MAX+1];
    int *luts[MAX_ETS_PLANES];
    int rs_lut[ETS_SRC_MAX+1];
    int *rs_luts[MAX_ETS_PLANES];
    int i;
    int c1_scale[MAX_ETS_PLANES] = { 1, 1, 1, 1, 1, 1, 1, 1 };
    ETS_POLARITY polarity = ETS_BLACK_IS_ONE;

    polarity = ETS_BLACK_IS_ONE;

    if (num_planes > MAX_ETS_PLANES)
        return gs_error_rangecheck;

    ds->ets_downscale = downscale_core;

    /* Setup a simple gamma scale */
    {
        double scale = ETS_SRC_MAX;
        for (i = 0; i < (ETS_SRC_MAX+1); i++)
            lut[i] = (int)((1 << 24) * (pow (i / scale, 1.0)));
    }
    for (i = 0; i < (ETS_SRC_MAX+1); i++)
        rs_lut[i] = 2 << 16;
    for (i = 0; i < num_planes; i++)
        luts[i] = lut;
    for (i = 0; i < num_planes; i++)
        rs_luts[i] = rs_lut;

#ifdef WITH_CAL
    params.context = ds->dev->memory->gs_lib_ctx->core->cal_ctx;
#endif
    params.width = ds->width;
    params.n_planes = num_planes;
    params.levels = 2;
    params.luts = luts;
    params.distscale = 0;
    find_aspect_ratio(ds->dev->HWResolution, &params.aspect_x, &params.aspect_y);
    params.strengths = strengths;
    params.rand_scale = 0;
    params.c1_scale = c1_scale;
    params.ets_bias = ETS_BIAS_REDUCE_POSITIVE;
    params.r_style = ETS_RSTYLE_THRESHOLD;
    params.dump_file = NULL;
    params.dump_level = 0;
    params.rand_scale_luts = rs_luts;
    params.polarity = polarity;

    ds->ets_config = ets_create(ds->dev->memory, &params);
    if (ds->ets_config == NULL)
        return gs_note_error(gs_error_VMerror);

    return 0;
}

static int init_ht(gx_downscaler_t *ds, int num_planes, gx_downscale_core *downscale_core)
{
    int nc = ds->early_cm ? ds->post_cm_num_comps : ds->num_comps;

    ds->ets_downscale = downscale_core;

    /* Allocate us a row (with padding for alignment) so we can hold the
     * expanded threshold array. */
    ds->htrow_alloc = gs_alloc_bytes(ds->dev->memory, ds->width * nc + 64,
                                     "gx_downscaler(htrow)");
    if (ds->htrow_alloc == NULL)
        return gs_error_VMerror;
    /* Make an aligned version */
    ds->htrow = ds->htrow_alloc + ((32-(intptr_t)ds->htrow_alloc) & 31);

    /* Allocate us a row (with padding for alignment) for the downscaled data. */
    ds->inbuf_alloc = gs_alloc_bytes(ds->dev->memory, ds->width * nc + 64,
                                     "gx_downscaler(inbuf)");
    if (ds->inbuf_alloc == NULL)
    {
        gs_free_object(ds->dev->memory, ds->htrow_alloc, "gx_downscaler(htrow)");
        ds->htrow_alloc = ds->htrow = NULL;
        return gs_error_VMerror;
    }
    /* Make an aligned version */
    ds->inbuf = ds->inbuf_alloc + ((32-(intptr_t)ds->inbuf_alloc) & 31);

    return 0;
}

int gx_downscaler_init_planar(gx_downscaler_t      *ds,
                              gx_device            *dev,
                              gs_get_bits_params_t *params,
                              int                   num_comps,
                              int                   factor,
                              int                   mfs,
                              int                   src_bpc,
                              int                   dst_bpc)
{
    return gx_downscaler_init_planar_trapped_cm(ds, dev, params, num_comps,
        factor, mfs, src_bpc, dst_bpc, 0, 0, NULL, NULL, NULL, num_comps);
}

int gx_downscaler_init_planar_trapped(gx_downscaler_t      *ds,
                                      gx_device            *dev,
                                      gs_get_bits_params_t *params,
                                      int                   num_comps,
                                      int                   factor,
                                      int                   mfs,
                                      int                   src_bpc,
                                      int                   dst_bpc,
                                      int                   trap_w,
                                      int                   trap_h,
                                      const int            *comp_order)
{
    return gx_downscaler_init_planar_trapped_cm(ds, dev, params, num_comps,
                                                factor, mfs, src_bpc, dst_bpc,
                                                trap_w, trap_h, comp_order,
                                                NULL, NULL, num_comps);
}

int gx_downscaler_init_planar_trapped_cm(gx_downscaler_t      *ds,
                                         gx_device            *dev,
                                         gs_get_bits_params_t *params,
                                         int                   num_comps,
                                         int                   factor,
                                         int                   mfs,
                                         int                   src_bpc,
                                         int                   dst_bpc,
                                         int                   trap_w,
                                         int                   trap_h,
                                         const int            *comp_order,
                                         gx_downscale_cm_fn   *apply_cm,
                                         void                 *apply_cm_arg,
                                         int                   post_cm_num_comps)
{
    int                span = bitmap_raster(dev->width * src_bpc);
    int                post_span = bitmap_raster(dev->width * src_bpc);
    int                width;
    int                code;
    gx_downscale_core *core;
    int                i;
    int                upfactor, downfactor;

    decode_factor(factor, &upfactor, &downfactor);

    /* width = scaled width */
    width = (dev->width*upfactor)/downfactor;
    memset(ds, 0, sizeof(*ds));
    ds->dev               = dev;
    ds->width             = width;
    ds->awidth            = width;
    ds->span              = span;
    ds->factor            = factor;
    ds->num_planes        = num_comps;
    ds->src_bpc           = src_bpc;
    ds->scaled_data       = NULL;
    ds->scaled_span       = bitmap_raster((dst_bpc*dev->width*upfactor + downfactor-1)/downfactor);
    ds->apply_cm          = apply_cm;
    ds->apply_cm_arg      = apply_cm_arg;
    ds->early_cm          = dst_bpc < src_bpc;
    ds->post_cm_num_comps = post_cm_num_comps;

    if (apply_cm) {
        for (i = 0; i < post_cm_num_comps; i++) {
            ds->post_cm[i] = gs_alloc_bytes(dev->memory, post_span * downfactor,
                                                "gx_downscaler(planar_data)");
            if (ds->post_cm[i] == NULL) {
                code = gs_note_error(gs_error_VMerror);
                goto cleanup;
            }
        }
    }

    code = check_trapping(dev->memory, trap_w, trap_h, num_comps, comp_order);
    if (code < 0)
        return code;

    if (trap_w > 0 || trap_h > 0) {
        ds->claptrap = ClapTrap_Init(dev->memory, width, dev->height, num_comps, comp_order, trap_w, trap_h, get_planar_line_for_trap, ds);
        if (ds->claptrap == NULL) {
            emprintf(dev->memory, "Trapping initialisation failed");
            code = gs_note_error(gs_error_VMerror);
            goto cleanup;
        }
    }
    else
        ds->claptrap = NULL;

    memcpy(&ds->params, params, sizeof(*params));
    ds->params.raster = span;
    for (i = 0; i < num_comps; i++) {
        ds->pre_cm[i] = gs_alloc_bytes(dev->memory, span * downfactor,
                                            "gx_downscaler(planar_data)");
        if (ds->pre_cm[i] == NULL) {
            code = gs_note_error(gs_error_VMerror);
            goto cleanup;
        }
    }
    if (upfactor > 1) {
        ds->scaled_data = gs_alloc_bytes(dev->memory, ds->scaled_span * upfactor * num_comps,
                                         "gx_downscaler(scaled_data)");
        if (ds->scaled_data == NULL) {
            code = gs_note_error(gs_error_VMerror);
            goto cleanup;
        }
    }

    if ((src_bpc == 8) && (dst_bpc == 8) && (factor == 32)) {
        core = &down_core8_3_2;
    } else if ((src_bpc == 8) && (dst_bpc == 8) && (factor == 34)) {
        core = &down_core8_3_4;
    } else if (factor > 8) {
        code = gs_note_error(gs_error_rangecheck);
        goto cleanup;
    } else if (dst_bpc == 1) {
        if (mfs > 1)
            core = &down_core_mfs;
        else if (factor == 4)
            core = &down_core_4;
        else if (factor == 3)
            core = &down_core_3;
        else if (factor == 2)
            core = &down_core_2;
        else if (factor == 1)
            core = &down_core_1;
        else
            core = &down_core;
    } else if (factor == 1)
        core = NULL;
    else if (src_bpc == 16)
        core = &down_core16;
    else if (factor == 4)
        core = &down_core8_4;
    else if (factor == 3)
        core = &down_core8_3;
    else if (factor == 2)
        core = &down_core8_2;
    else
        core = &down_core8;
    ds->down_core = core;

    if (mfs > 1) {
        ds->mfs_data = (byte *)gs_alloc_bytes(dev->memory,
                                              (width+1) * num_comps,
                                              "gx_downscaler(mfs)");
        if (ds->mfs_data == NULL) {
            code = gs_note_error(gs_error_VMerror);
            goto cleanup;
        }
        memset(ds->mfs_data, 0, (width+1) * num_comps);
    }
    if (dst_bpc == 1) {
        ds->errors = (int *)gs_alloc_bytes(dev->memory,
                                           num_comps*(width+3)*sizeof(int),
                                           "gx_downscaler(errors)");
        if (ds->errors == NULL) {
            code = gs_note_error(gs_error_VMerror);
            goto cleanup;
        }
        memset(ds->errors, 0, num_comps * (width+3) * sizeof(int));
    }

    return 0;

  cleanup:
    gx_downscaler_fin(ds);
    return code;
}

static int get_line_for_trap(void *arg, unsigned char *buf)
{
    gx_downscaler_t *ds = (gx_downscaler_t *)arg;

    /* Allow for devices (like psdcmyk) that make several passes through
     * the image. */
    if (ds->claptrap_y == ds->dev->height)
        ds->claptrap_y = 0;

    return (*dev_proc(ds->dev, get_bits))(ds->dev, ds->claptrap_y++, buf, NULL);
}

int gx_downscaler_init(gx_downscaler_t   *ds,
                       gx_device         *dev,
                       int                src_bpc,
                       int                dst_bpc,
                       int                num_comps,
                       int                factor,
                       int                mfs,
                       int              (*adjust_width_proc)(int, int),
                       int                adjust_width)
{
    return gx_downscaler_init_trapped_cm_ets(ds, dev, src_bpc, dst_bpc, num_comps,
        factor, mfs, adjust_width_proc, adjust_width, 0, 0, NULL, NULL, NULL, 0, 0);
}


int gx_downscaler_init_ets(gx_downscaler_t   *ds,
                           gx_device         *dev,
                           int                src_bpc,
                           int                dst_bpc,
                           int                num_comps,
                           int                factor,
                           int                mfs,
                           int              (*adjust_width_proc)(int, int),
                           int                adjust_width,
                           int                ets)
{
    return gx_downscaler_init_trapped_cm_ets(ds, dev, src_bpc, dst_bpc, num_comps,
        factor, mfs, adjust_width_proc, adjust_width, 0, 0, NULL, NULL, NULL, 0, ets);
}

int gx_downscaler_init_trapped(gx_downscaler_t   *ds,
                               gx_device         *dev,
                               int                src_bpc,
                               int                dst_bpc,
                               int                num_comps,
                               int                factor,
                               int                mfs,
                               int              (*adjust_width_proc)(int, int),
                               int                adjust_width,
                               int                trap_w,
                               int                trap_h,
                               const int         *comp_order)
{
    return gx_downscaler_init_trapped_cm_ets(ds, dev, src_bpc, dst_bpc,
                                             num_comps, factor, mfs,
                                             adjust_width_proc, adjust_width,
                                             trap_w, trap_h, comp_order,
                                             NULL, NULL, 0, 0);
}

int gx_downscaler_init_trapped_ets(gx_downscaler_t   *ds,
                                   gx_device         *dev,
                                   int                src_bpc,
                                   int                dst_bpc,
                                   int                num_comps,
                                   int                factor,
                                   int                mfs,
                                   int              (*adjust_width_proc)(int, int),
                                   int                adjust_width,
                                   int                trap_w,
                                   int                trap_h,
                                   const int         *comp_order,
                                   int                ets)
{
    return gx_downscaler_init_trapped_cm_ets(ds, dev, src_bpc, dst_bpc,
                                             num_comps, factor, mfs,
                                             adjust_width_proc, adjust_width,
                                             trap_w, trap_h, comp_order,
                                             NULL, NULL, 0, ets);
}
int gx_downscaler_init_cm(gx_downscaler_t    *ds,
                           gx_device          *dev,
                           int                 src_bpc,
                           int                 dst_bpc,
                           int                 num_comps,
                           int                 factor,
                           int                 mfs,
                           int               (*adjust_width_proc)(int, int),
                           int                 adjust_width,
                           gx_downscale_cm_fn *apply_cm,
                           void               *apply_cm_arg,
                           int                 post_cm_num_comps)
{
    return gx_downscaler_init_trapped_cm_ets(ds, dev, src_bpc, dst_bpc,
                                             num_comps, factor, mfs,
                                             adjust_width_proc, adjust_width,
                                             0, 0, NULL,
                                             apply_cm, apply_cm_arg, post_cm_num_comps, 0);
}

int gx_downscaler_init_cm_ets(gx_downscaler_t    *ds,
                              gx_device          *dev,
                              int                 src_bpc,
                              int                 dst_bpc,
                              int                 num_comps,
                              int                 factor,
                              int                 mfs,
                              int               (*adjust_width_proc)(int, int),
                              int                 adjust_width,
                              gx_downscale_cm_fn *apply_cm,
                              void               *apply_cm_arg,
                              int                 post_cm_num_comps,
                              int                 ets)
{
    return gx_downscaler_init_trapped_cm_ets(ds, dev, src_bpc, dst_bpc,
                                             num_comps, factor, mfs,
                                             adjust_width_proc, adjust_width,
                                             0, 0, NULL,
                                             apply_cm, apply_cm_arg, post_cm_num_comps, ets);
}

int gx_downscaler_init_trapped_cm(gx_downscaler_t    *ds,
                                  gx_device          *dev,
                                  int                 src_bpc,
                                  int                 dst_bpc,
                                  int                 num_comps,
                                  int                 factor,
                                  int                 mfs,
                                  int               (*adjust_width_proc)(int, int),
                                  int                 adjust_width,
                                  int                 trap_w,
                                  int                 trap_h,
                                  const int          *comp_order,
                                  gx_downscale_cm_fn *apply_cm,
                                  void               *apply_cm_arg,
                                  int                 post_cm_num_comps)
{
    return gx_downscaler_init_trapped_cm_ets(ds, dev, src_bpc, dst_bpc,
                                             num_comps, factor, mfs,
                                             adjust_width_proc, adjust_width,
                                             trap_w, trap_h, comp_order,
                                             apply_cm, apply_cm_arg, post_cm_num_comps,
                                             0);
}

static gx_downscaler_ht_t bogus_ets_halftone;

int gx_downscaler_init_trapped_cm_ets(gx_downscaler_t    *ds,
                                  gx_device          *dev,
                                  int                 src_bpc,
                                  int                 dst_bpc,
                                  int                 num_comps,
                                  int                 factor,
                                  int                 mfs,
                                  int               (*adjust_width_proc)(int, int),
                                  int                 adjust_width,
                                  int                 trap_w,
                                  int                 trap_h,
                                  const int          *comp_order,
                                  gx_downscale_cm_fn *apply_cm,
                                  void               *apply_cm_arg,
                                  int                 post_cm_num_comps,
                                  int                 ets)
{
    return gx_downscaler_init_trapped_cm_halftone(ds,
                                                  dev,
                                                  src_bpc,
                                                  dst_bpc,
                                                  num_comps,
                                                  factor,
                                                  mfs,
                                                  adjust_width_proc,
                                                  adjust_width,
                                                  trap_w,
                                                  trap_h,
                                                  comp_order,
                                                  apply_cm,
                                                  apply_cm_arg,
                                                  post_cm_num_comps,
                                                  ets ? &bogus_ets_halftone : NULL);
}


static gx_downscale_core *
select_8_to_8_core(int nc, int factor)
{
    if (factor == 1)
        return NULL; /* No sense doing anything */
    if (nc == 1)
    {
        if (factor == 4)
            return &down_core8_4;
        else if (factor == 3)
            return &down_core8_3;
        else if (factor == 2)
            return &down_core8_2;
        else
            return &down_core8;
    }
    else if (nc == 3)
        return &down_core24;
    else if (nc == 4)
        return &down_core32;

    return NULL;
}

int
gx_downscaler_init_trapped_cm_halftone(gx_downscaler_t    *ds,
                                  gx_device          *dev,
                                  int                 src_bpc,
                                  int                 dst_bpc,
                                  int                 num_comps,
                                  int                 factor,
                                  int                 mfs,
                                  int               (*adjust_width_proc)(int, int),
                                  int                 adjust_width,
                                  int                 trap_w,
                                  int                 trap_h,
                                  const int          *comp_order,
                                  gx_downscale_cm_fn *apply_cm,
                                  void               *apply_cm_arg,
                                  int                 post_cm_num_comps,
                                  gx_downscaler_ht_t *ht)
{
    int                size;
    int                post_size;
    int                span;
    int                width;
    int                awidth;
    int                pad_white;
    int                code;
    gx_downscale_core *core;
    int                upfactor;
    int                downfactor;
    int                nc;

    size = gdev_mem_bytes_per_scan_line((gx_device *)dev);
    post_size = bitmap_raster(dev->width * src_bpc * post_cm_num_comps);

    decode_factor(factor, &upfactor, &downfactor);

    /* width = scaled width */
    width = (dev->width * upfactor)/downfactor;
    awidth = width;
    if (adjust_width_proc != NULL)
        awidth = (*adjust_width_proc)(width, adjust_width);
    pad_white = awidth - width;
    if (pad_white < 0)
        pad_white = 0;

    /* size = unscaled size. span = unscaled size + padding */
    span = size + pad_white*downfactor*num_comps/upfactor + downfactor-1;
    memset(ds, 0, sizeof(*ds));
    ds->dev               = dev;
    ds->width             = width;
    ds->awidth            = awidth;
    ds->span              = span;
    ds->factor            = factor;
    ds->num_planes        = 0;
    ds->src_bpc           = src_bpc;
    ds->apply_cm          = apply_cm;
    ds->apply_cm_arg      = apply_cm_arg;
    ds->early_cm          = dst_bpc < src_bpc;
    ds->post_cm_num_comps = post_cm_num_comps;
    ds->ht                = ht;
    ds->dst_bpc           = dst_bpc;
    ds->num_comps         = num_comps;

    code = check_trapping(dev->memory, trap_w, trap_h, num_comps, comp_order);
    if (code < 0)
        return code;

    if (trap_w > 0 || trap_h > 0) {
        ds->claptrap = ClapTrap_Init(dev->memory, width, dev->height, num_comps, comp_order, trap_w, trap_h, get_line_for_trap, ds);
        if (ds->claptrap == NULL) {
            emprintf(dev->memory, "Trapping initialisation failed");
            code = gs_note_error(gs_error_VMerror);
            goto cleanup;
        }
    } else
        ds->claptrap = NULL;

    /* Choose an appropriate core. Try to honour our early_cm
     * choice, and fallback to late cm if we can't. */
    core = NULL;
    while (1)
    {
        nc = ds->early_cm ? post_cm_num_comps : num_comps;

        if (factor > 8) {
            code = gs_note_error(gs_error_rangecheck);
            goto cleanup;
        }
        else if ((src_bpc == 16) && (dst_bpc == 16) && (nc == 1))
        {
            core = &down_core16;
        }
        else if ((src_bpc == 8) && (dst_bpc == 1) && (nc == 4))
        {
            if (mfs > 1)
                core = &down_core4_mfs;
            else if (ht == &bogus_ets_halftone)
            {
                code = init_ets(ds, 4, select_8_to_8_core(nc, factor));
                if (code)
                    goto cleanup;
                core = &down_core4_ets;
            }
            else if (ht != NULL)
            {
                code = init_ht(ds, 4, select_8_to_8_core(nc, factor));
                if (code)
                    goto cleanup;
                core = &down_core4_ht;
            }
            else
                core = &down_core4;
        }
        else if ((src_bpc == 8) && (dst_bpc == 1) && (nc == 1))
        {
            if (mfs > 1)
                core = &down_core_mfs;
            else if (ht == &bogus_ets_halftone)
            {
                code = init_ets(ds, 1, select_8_to_8_core(nc, factor));
                if (code)
                    goto cleanup;
                core = &down_core_ets_1;
            }
            else if (factor == 4)
                core = &down_core_4;
            else if (factor == 3)
                core = &down_core_3;
            else if (factor == 2)
                core = &down_core_2;
            else if (factor == 1)
                core = &down_core_1;
            else
                core = &down_core;
        }
        else if ((factor == 1) && (src_bpc == dst_bpc))
            break;
        else if (src_bpc == 8 && dst_bpc == 8)
            core = select_8_to_8_core(nc, factor);

        /* If we found one, or we have nothing to fallback to, exit */
        if (core || !ds->early_cm)
            break;

        /* Fallback */
        ds->early_cm = false;
    }
    if (factor == 1 && src_bpc == dst_bpc) {
        /* core can permissibly be NULL */
    } else if (core == NULL) {
        code = gs_note_error(gs_error_rangecheck);
        goto cleanup;
    }
    ds->down_core = core;

    if (apply_cm) {
        ds->post_cm[0] = gs_alloc_bytes(dev->memory,
                                        post_size * downfactor,
                                        "gx_downscaler(data)");
        if (ds->post_cm[0] == NULL) {
            code = gs_note_error(gs_error_VMerror);
            goto cleanup;
        }
    }

    if (core != NULL || apply_cm) {
        ds->pre_cm[0] = gs_alloc_bytes(dev->memory,
            span * downfactor,
            "gx_downscaler(data)");
        if (ds->pre_cm[0] == NULL) {
            code = gs_note_error(gs_error_VMerror);
            goto cleanup;
        }
    }
    if (core != NULL) {
        if (mfs > 1) {
            ds->mfs_data = (byte *)gs_alloc_bytes(dev->memory,
                                                  (awidth+1)*nc,
                                                  "gx_downscaler(mfs)");
            if (ds->mfs_data == NULL) {
                code = gs_note_error(gs_error_VMerror);
                goto cleanup;
            }
            memset(ds->mfs_data, 0, (awidth+1)*nc);
        }
        if (dst_bpc == 1) {
            ds->errors = (int *)gs_alloc_bytes(dev->memory,
                                               nc*(awidth+3)*sizeof(int),
                                               "gx_downscaler(errors)");
            if (ds->errors == NULL) {
                code = gs_note_error(gs_error_VMerror);
                goto cleanup;
            }
            memset(ds->errors, 0, nc * (awidth+3) * sizeof(int));
        }
    }

    return 0;

  cleanup:
    gx_downscaler_fin(ds);
    return code;
}

void gx_downscaler_fin(gx_downscaler_t *ds)
{
    int plane;
    for (plane=0; plane < GS_CLIENT_COLOR_MAX_COMPONENTS; plane++) {
        gs_free_object(ds->dev->memory, ds->pre_cm[plane],
                       "gx_downscaler(planar_data)");
        gs_free_object(ds->dev->memory, ds->post_cm[plane],
                       "gx_downscaler(planar_data)");
    }
    ds->num_planes = 0;

    gs_free_object(ds->dev->memory, ds->mfs_data, "gx_downscaler(mfs)");
    ds->mfs_data = NULL;
    gs_free_object(ds->dev->memory, ds->errors, "gx_downscaler(errors)");
    ds->errors = NULL;
    gs_free_object(ds->dev->memory, ds->scaled_data, "gx_downscaler(scaled_data)");
    ds->scaled_data = NULL;
    gs_free_object(ds->dev->memory, ds->htrow_alloc, "gx_downscaler(htrow)");
    ds->htrow = NULL;
    ds->htrow_alloc = NULL;

    if (ds->claptrap)
        ClapTrap_Fin(ds->dev->memory, ds->claptrap);

    if (ds->ets_config)
        ets_destroy(ds->dev->memory, ds->ets_config);
}

/* Chunky case */
int gx_downscaler_getbits(gx_downscaler_t *ds,
                          byte            *out_data,
                          int              row)
{
    int   code = 0;
    int   y, y_end;
    byte *data_ptr;
    int   upfactor, downfactor;

    decode_factor(ds->factor, &upfactor, &downfactor);

    /* Check for the simple case */
    if (ds->down_core == NULL) {
        if (ds->claptrap)
            code = ClapTrap_GetLine(ds->claptrap, ds->apply_cm ? ds->pre_cm[0] : out_data);
        else
            code = (*dev_proc(ds->dev, get_bits))(ds->dev, row, ds->apply_cm ? ds->pre_cm[0] : out_data, NULL);
        if (code < 0)
            return code;
        if (ds->apply_cm) {
            data_ptr = out_data;
            return ds->apply_cm(ds->apply_cm_arg, &data_ptr, ds->pre_cm, ds->width, 1, 0);
        }
        return 0;
    }

    /* Get factor rows worth of data */
    y        = row * downfactor;
    y_end    = y + downfactor;
    data_ptr = ds->pre_cm[0];
    if (ds->claptrap) {
        do {
            code = ClapTrap_GetLine(ds->claptrap, data_ptr);
            if (code < 0)
                return code;
            data_ptr += ds->span;
            y++;
        } while (y < y_end);
    } else {
        do {
            code = (*dev_proc(ds->dev, get_bits))(ds->dev, y, data_ptr, NULL);
            if (code < 0)
                return code;
            data_ptr += ds->span;
            y++;
        } while (y < y_end);
    }

    if (ds->apply_cm) {
        if (ds->early_cm) {
            code = ds->apply_cm(ds->apply_cm_arg, ds->post_cm, ds->pre_cm, ds->dev->width, 1, 0);
            if (code < 0)
                return code;
            (ds->down_core)(ds, out_data, ds->post_cm[0], row, 0, ds->span);
        } else {
            data_ptr = out_data;
            (ds->down_core)(ds, ds->post_cm[0], ds->pre_cm[0], row, 0, ds->span);
            code = ds->apply_cm(ds->apply_cm_arg, &out_data, ds->post_cm, ds->width, 1, 0);
            if (code < 0)
                return code;
        }
    } else
        (ds->down_core)(ds, out_data, ds->pre_cm[0], row, 0, ds->span);

    return code;
}

/* Planar case */
int gx_downscaler_get_bits_rectangle(gx_downscaler_t      *ds,
                                     gs_get_bits_params_t *params,
                                     int                   row)
{
    int                   code;
    gs_int_rect           rect;
    int                   plane;
    int                   factor = ds->factor;
    gs_get_bits_params_t  params2;
    int                   upfactor, downfactor;
    int                   subrow;
    int                   copy = (ds->dev->width * ds->src_bpc + 7)>>3;
    int                   i, j;

    decode_factor(factor, &upfactor, &downfactor);

    subrow = row % upfactor;
    if (subrow) {
        /* Just copy a previous row from our stored buffer */
        for (plane=0; plane < ds->num_planes; plane++)
            params->data[plane] = ds->scaled_data + (upfactor * plane + subrow) * ds->scaled_span;
        return 0;
    }

    rect.p.x = 0;
    rect.p.y = (row/upfactor) * downfactor;
    rect.q.x = ds->dev->width;
    rect.q.y = ((row/upfactor) + 1) * downfactor;

    /* Check for the simple case */
    if (ds->down_core == NULL && ds->claptrap == NULL) {
        gs_get_bits_params_t saved;
        if (ds->apply_cm) {
            /* Always do the request giving our own workspace,
             * and be prepared to accept a pointer */
            saved = *params;
            for (i = 0; i < ds->num_planes; i++)
                params->data[i] = ds->pre_cm[i];
            params->options |= GB_RETURN_POINTER;
        }
        code = (*dev_proc(ds->dev, get_bits_rectangle))(ds->dev, &rect, params, NULL);
        if (code < 0)
            return code;
        if (ds->apply_cm) {
            byte **buffer;
            if (saved.options & GB_RETURN_COPY) {
                /* They will accept a copy. Let's use the buffer they supplied */
                params->options &= ~GB_RETURN_POINTER;
                buffer = saved.data;
            } else
                buffer = ds->pre_cm;
            code = ds->apply_cm(ds->apply_cm_arg, params->data, buffer, ds->dev->width, rect.q.y - rect.p.y, params->raster);
            if ((saved.options & GB_RETURN_COPY) == 0)
                for (i = 0; i < ds->num_planes; i++)
                    params->data[i] = buffer[i];
        }
        return code;
    }

    /* Copy the params, because get_bits_rectangle can helpfully overwrite
     * them. */
    memcpy(&params2, &ds->params, sizeof(params2));
    for (i = 0; i < ds->num_planes; i++)
         params2.data[i] = ds->pre_cm[i];

    /* Get downfactor rows worth of data */
    if (ds->claptrap)
        code = gs_error_rangecheck; /* Always work a line at a time with claptrap */
    else
        code = (*dev_proc(ds->dev, get_bits_rectangle))(ds->dev, &rect, &params2, NULL);
    if (code == gs_error_rangecheck) {
        /* At the bottom of a band, the get_bits_rectangle call can fail to be
         * able to return us enough lines of data at the same time. We therefore
         * drop back to reading them one at a time, and copying them into our
         * own buffer. */
        for (i = 0; i < downfactor; i++) {
            rect.q.y = rect.p.y+1;
            if (rect.q.y > ds->dev->height)
                break;
            memcpy(&params2, &ds->params, sizeof(params2));
            for (j = 0; j < ds->num_planes; j++)
                params2.data[j] = ds->pre_cm[j] + i * ds->span;
            if (ds->claptrap) {
                ds->claptrap_params = &params2;
                code = ClapTrap_GetLinePlanar(ds->claptrap, &params2.data[0]);
            } else {
                /* We always want a copy */
                params2.options &= ~GB_RETURN_POINTER;
                params2.options |=  GB_RETURN_COPY;
                code = (*dev_proc(ds->dev, get_bits_rectangle))(ds->dev, &rect, &params2, NULL);
            }
            if (code < 0)
                break;
            rect.p.y++;
        }
        if (i == 0)
            return code;
        /* If we still haven't got enough, we've hit the end of the page; just
         * duplicate the last line we did get. */
        for (;i < downfactor; i++)
            for (j = 0; j < ds->num_planes; j++)
                memcpy(ds->pre_cm[j] + i*ds->span, ds->pre_cm[j] + (i-1)*ds->span, copy);
    }
    if (code < 0)
        return code;

    if (ds->early_cm && ds->apply_cm) {
        code = ds->apply_cm(ds->apply_cm_arg, ds->params.data, ds->post_cm, ds->dev->width, downfactor, params->raster);
        if (code < 0)
            return code;
        for (j = 0; j < ds->num_planes; j++)
            params2.data[j] = ds->post_cm[j];
    }

    if (upfactor > 1) {
        /* Downscale the block of lines into our output buffer */
        for (plane=0; plane < ds->num_planes; plane++) {
            byte *scaled = ds->scaled_data + upfactor * plane * ds->scaled_span;
            (ds->down_core)(ds, scaled, params2.data[plane], row, plane, params2.raster);
            params->data[plane] = scaled;
        }
    } else if (ds->down_core != NULL) {
        /* Downscale direct into output buffer */
        for (plane=0; plane < ds->num_planes; plane++)
            (ds->down_core)(ds, params->data[plane], params2.data[plane], row, plane, params2.raster);
    } else {
        /* Copy into output buffer */
        /* No color management can be required here */
        assert(!ds->early_cm || ds->apply_cm == NULL);
        for (plane=0; plane < ds->num_planes; plane++)
            memcpy(params->data[plane], params2.data[plane], params2.raster);
    }

    if (!ds->early_cm && ds->apply_cm) {
        code = ds->apply_cm(ds->apply_cm_arg, ds->params.data, params2.data, ds->width, 1, params->raster);
        if (code < 0)
            return code;
    }

    return code;
}

typedef struct downscaler_process_page_arg_s
{
    gx_process_page_options_t *orig_options;
    int upfactor;
    int downfactor;
    gx_downscaler_t ds;
}
downscaler_process_page_arg_t;

typedef struct downscaler_process_page_buffer_s
{
    gx_device *bdev;
    void *orig_buffer;
}
downscaler_process_page_buffer_t;

static int downscaler_init_fn(void *arg_, gx_device *dev, gs_memory_t *memory, int w, int h, void **pbuffer)
{
    downscaler_process_page_arg_t *arg = (downscaler_process_page_arg_t *)arg_;
    downscaler_process_page_buffer_t *buffer;
    int code = 0;

    buffer = (downscaler_process_page_buffer_t *)gs_alloc_bytes(memory, sizeof(*buffer), "downscaler process_page buffer");
    if (buffer == NULL)
        return_error(gs_error_VMerror);
    memset(buffer, 0, sizeof(*buffer));

    if (arg->upfactor > arg->downfactor) {
        code = gx_default_create_buf_device(&buffer->bdev, dev,
                          (h*arg->upfactor + arg->downfactor-1)/arg->downfactor,
                          NULL, memory, NULL);
        if (code < 0) {
            gs_free_object(memory, buffer, "downscaler process_page buffer");
            return code;
        }
    }

    if (arg->orig_options && arg->orig_options->init_buffer_fn) {
        code = arg->orig_options->init_buffer_fn(arg->orig_options->arg, dev, memory,
                                                 (w * arg->upfactor + arg->downfactor-1)/arg->downfactor,
                                                 (h * arg->upfactor + arg->downfactor-1)/arg->downfactor,
                                                 &buffer->orig_buffer);
        if (code < 0) {
            if (buffer->bdev)
                dev_proc(dev, close_device)(dev);
            gs_free_object(memory, buffer, "downscaler process_page buffer");
            return code;
        }
    }

    *pbuffer = (void *)buffer;
    return code;
}

static int downscaler_process_fn(void *arg_, gx_device *dev, gx_device *bdev, const gs_int_rect *rect, void *buffer_)
{
    downscaler_process_page_arg_t *arg = (downscaler_process_page_arg_t *)arg_;
    downscaler_process_page_buffer_t *buffer = (downscaler_process_page_buffer_t *)buffer_;
    int code, raster_in, raster_out;
    gs_get_bits_params_t params;
    gs_int_rect in_rect, out_rect;
    byte *in_ptr, *out_ptr;

    in_rect.p.x = 0;
    in_rect.p.y = 0;
    in_rect.q.x = rect->q.x - rect->p.x;
    in_rect.q.y = rect->q.y - rect->p.y;
    out_rect.p.x = 0;
    out_rect.p.y = 0;
    out_rect.q.x = (in_rect.q.x * arg->upfactor + arg->downfactor-1) / arg->downfactor;
    out_rect.q.y = (in_rect.q.y * arg->upfactor + arg->downfactor-1) / arg->downfactor;

    /* Where do we get the data from? */
    params.options = GB_COLORS_NATIVE | GB_ALPHA_NONE | GB_PACKING_CHUNKY | GB_RETURN_POINTER | GB_ALIGN_ANY | GB_OFFSET_0 | GB_RASTER_ANY;
    code = dev_proc(bdev, get_bits_rectangle)(bdev, &in_rect, &params, NULL);
    if (code < 0)
        return code;
    raster_in = params.raster;
    in_ptr = params.data[0];

    /* Where do we write it to? */
    if (buffer->bdev) {
        code = dev_proc(bdev, get_bits_rectangle)(buffer->bdev, &out_rect, &params, NULL);
        if (code < 0)
            return code;
        raster_out = params.raster;
        out_ptr = params.data[0];
    } else {
        raster_out = raster_in;
        out_ptr = params.data[0];
    }

    /* Do the downscale */
    if (arg->ds.down_core) {
        int y;
        for (y = rect->p.y; y < rect->q.y; y += arg->downfactor)
        {
            arg->ds.down_core(&arg->ds, out_ptr, in_ptr, y, 0, arg->ds.span);
            in_ptr += arg->ds.span * arg->downfactor;
            out_ptr += raster_out * arg->upfactor;
        }
    }

    /* Pass on to further processing */
    if (code >= 0 && arg->orig_options && arg->orig_options->process_fn) {
        out_rect.p.y = rect->p.y*arg->upfactor/arg->downfactor;
        out_rect.q.y += out_rect.p.y;
        code = arg->orig_options->process_fn(arg->orig_options->arg, dev,
                                             (buffer->bdev ? buffer->bdev : bdev),
                                             &out_rect, buffer->orig_buffer);
    }
    return code;
}

static void
downscaler_free_fn(void *arg_, gx_device *dev, gs_memory_t *memory, void *buffer_)
{
    downscaler_process_page_arg_t *arg = (downscaler_process_page_arg_t *)arg_;
    downscaler_process_page_buffer_t *buffer = (downscaler_process_page_buffer_t *)buffer_;

    arg->orig_options->free_buffer_fn(arg->orig_options->arg, dev, memory,
                                      buffer->orig_buffer);
    if (buffer->bdev)
        dev_proc(dev, close_device)(dev);
    gs_free_object(memory, buffer, "downscaler process_page buffer");
}

static int
downscaler_output_fn(void *arg_, gx_device *dev, void *buffer_)
{
    downscaler_process_page_arg_t *arg = (downscaler_process_page_arg_t *)arg_;
    downscaler_process_page_buffer_t *buffer = (downscaler_process_page_buffer_t *)buffer_;

    return arg->orig_options->output_fn(arg->orig_options->arg, dev,
                                        buffer->orig_buffer);
}

/* No error diffusion with process_page as bands need to be handled
 * separately. */
int gx_downscaler_process_page(gx_device                 *dev,
                               gx_process_page_options_t *options,
                               int                        factor)
{
    downscaler_process_page_arg_t arg = { 0 };
    gx_process_page_options_t my_options = { 0 };
    int num_comps = dev->color_info.num_components;
    int src_bpc = dev->color_info.comp_bits[0];
    int scaled_w;
    gx_downscale_core *core;

    arg.orig_options = options;
    decode_factor(factor, &arg.upfactor, &arg.downfactor);
    arg.ds.dev = dev;
    arg.ds.width = (dev->width * arg.upfactor + arg.downfactor-1)/arg.downfactor;
    arg.ds.awidth = arg.ds.width;
    arg.ds.span = bitmap_raster(dev->width * num_comps * src_bpc);
    scaled_w = (dev->width * arg.upfactor + arg.downfactor-1)/arg.downfactor;
    arg.ds.factor = factor;
    arg.ds.src_bpc = src_bpc;
    arg.ds.scaled_span = bitmap_raster(scaled_w * num_comps * src_bpc);
    arg.ds.num_planes = 0;

    /* Choose an appropriate core */
    if (factor > 8)
    {
        return gs_note_error(gs_error_rangecheck);
    }
    else if ((src_bpc == 16) && (num_comps == 1))
    {
        core = &down_core16;
    }
    else if (factor == 1)
        core = NULL;
    else if ((src_bpc == 8) && (num_comps == 1))
    {
        if (factor == 4)
            core = &down_core8_4;
        else if (factor == 3)
            core = &down_core8_3;
        else if (factor == 2)
            core = &down_core8_2;
        else
            core = &down_core8;
    }
    else if ((src_bpc == 8) && (num_comps == 3))
        core = &down_core24;
    else if ((src_bpc == 8) && (num_comps == 4))
         core = &down_core32;
    else {
        return gs_note_error(gs_error_rangecheck);
    }
    arg.ds.down_core = core;

    my_options.init_buffer_fn = downscaler_init_fn;
    my_options.process_fn = downscaler_process_fn;
    my_options.output_fn = downscaler_output_fn;
    my_options.free_buffer_fn = downscaler_free_fn;
    my_options.arg = &arg;

    return dev_proc(dev, process_page)(dev, &my_options);
}

int gx_downscaler_read_params(gs_param_list        *plist,
                              gx_downscaler_params *params,
                              int                   features)
{
    int code;
    int downscale, mfs, ets;
    int trap_w, trap_h;
    const char *param_name;
    gs_param_int_array trap_order;

    trap_order.data = NULL;

    switch (code = param_read_int(plist,
                                   (param_name = "DownScaleFactor"),
                                   &downscale)) {
        case 1:
            break;
        case 0:
            if (downscale >= 1) {
                params->downscale_factor = downscale;
                break;
            }
            code = gs_error_rangecheck;
        default:
            param_signal_error(plist, param_name, code);
            return code;
    }

    if (features & GX_DOWNSCALER_PARAMS_MFS)
    {
        switch (code = param_read_int(plist, (param_name = "MinFeatureSize"), &mfs)) {
            case 1:
                break;
            case 0:
                if ((mfs >= 0) && (mfs <= 4)) {
                    params->min_feature_size = mfs;
                    break;
                }
                code = gs_error_rangecheck;
            default:
                param_signal_error(plist, param_name, code);
                return code;
        }
    }

    if (features & GX_DOWNSCALER_PARAMS_TRAP)
    {
        switch (code = param_read_int(plist,
                                      (param_name = "TrapX"),
                                      &trap_w)) {
            case 1:
                break;
            case 0:
                if (trap_w >= 0)
                {
                    params->trap_w = trap_w;
                    break;
                }
                code = gs_error_rangecheck;
            default:
                param_signal_error(plist, param_name, code);
                return code;
        }
        switch (code = param_read_int(plist,
                                      (param_name = "TrapY"),
                                      &trap_h)) {
            case 1:
                break;
            case 0:
                if (trap_h >= 0)
                {
                    params->trap_h = trap_h;
                    break;
                }
                code = gs_error_rangecheck;
            default:
                param_signal_error(plist, param_name, code);
                return code;
        }
        switch (code = param_read_int_array(plist, (param_name = "TrapOrder"), &trap_order)) {
            case 0:
                break;
            case 1:
                trap_order.data = 0;          /* mark as not filled */
                break;
            default:
                param_signal_error(plist, param_name, code);
                return code;
        }

        if (trap_order.data != NULL)
        {
            int i;
            int n = trap_order.size;

            if (n > GS_CLIENT_COLOR_MAX_COMPONENTS)
                n = GS_CLIENT_COLOR_MAX_COMPONENTS;

            for (i = 0; i < n; i++)
            {
                params->trap_order[i] = trap_order.data[i];
            }
            for (; i < GS_CLIENT_COLOR_MAX_COMPONENTS; i++)
            {
                params->trap_order[i] = i;
            }
        }
        else
        {
            /* Set some sane defaults */
            int i;

            params->trap_order[0] = 3; /* K */
            params->trap_order[1] = 1; /* M */
            params->trap_order[2] = 0; /* C */
            params->trap_order[3] = 2; /* Y */

            for (i = 4; i < GS_CLIENT_COLOR_MAX_COMPONENTS; i++)
            {
                params->trap_order[i] = i;
            }
        }
    }
    if (features & GX_DOWNSCALER_PARAMS_ETS)
    {
        switch (code = param_read_int(plist,
                                      (param_name = "DownScaleETS"),
                                      &ets)) {
            case 1:
                break;
            case 0:
                if (ets >= 0)
                {
                    params->ets = ets;
                    break;
                }
                code = gs_error_rangecheck;
            default:
                param_signal_error(plist, param_name, code);
                return code;
        }
    }

    return 0;
}

int gx_downscaler_write_params(gs_param_list        *plist,
                               gx_downscaler_params *params,
                               int                   features)
{
    int code;
    int ecode = 0;
    gs_param_int_array trap_order;

    trap_order.data = params->trap_order;
    trap_order.size = GS_CLIENT_COLOR_MAX_COMPONENTS;
    trap_order.persistent = false;

    if ((code = param_write_int(plist, "DownScaleFactor", &params->downscale_factor)) < 0)
        ecode = code;
    if (features & GX_DOWNSCALER_PARAMS_MFS)
    {
        if ((code = param_write_int(plist, "MinFeatureSize", &params->min_feature_size)) < 0)
            ecode = code;
    }
    if (features & GX_DOWNSCALER_PARAMS_TRAP)
    {
        if ((code = param_write_int(plist, "TrapX", &params->trap_w)) < 0)
            ecode = code;
        if ((code = param_write_int(plist, "TrapY", &params->trap_h)) < 0)
            ecode = code;
        if ((code = param_write_int_array(plist, "TrapOrder", &trap_order)) < 0)
            ecode = code;
    }
    if (features & GX_DOWNSCALER_PARAMS_ETS)
    {
        if ((code = param_write_int(plist, "DownScaleETS", &params->ets)) < 0)
            ecode = code;
    }

    return ecode;
}

/* ETS relies on some malloc wrappers */
void *ets_malloc(void *malloc_arg, int size)
{
    return gs_alloc_bytes((gs_memory_t *)malloc_arg, size, "ets_malloc");
}

void *ets_calloc(void *malloc_arg, int count, int size)
{
    void *p = ets_malloc(malloc_arg, count * size);
    if (p)
        memset(p, 0, count * size);
    return p;
}

void ets_free(void *malloc_arg, void *p)
{
    if (!p)
        return;

    gs_free_object((gs_memory_t *)malloc_arg, p, "ets_malloc");
}
