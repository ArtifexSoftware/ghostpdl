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
        inp += awidth*2-1;
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
        inp += awidth*3-1;
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
        inp += awidth*4-1;
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
    pack_8to1(out_buffer, outp, awidth);
}

/* Grey downscale code */
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

int gx_downscaler_init_planar(gx_downscaler_t      *ds,
                              gx_device            *dev,
                              gs_get_bits_params_t *params,
                              int                   num_comps,
                              int                   factor,
                              int                   mfs,
                              int                   dst_bpc)
{
    int                span = bitmap_raster(dev->width * 8);
    int                width = dev->width/factor;
    int                code;
    gx_downscale_core *core;
    int                i;

    memset(ds, 0, sizeof(*ds));
    ds->dev        = dev;
    ds->width      = width;
    ds->awidth     = width;
    ds->span       = span;
    ds->factor     = factor;
    ds->num_planes = num_comps;

    memcpy(&ds->params, params, sizeof(*params));
    ds->params.raster = span;
    for (i = 0; i < num_comps; i++)
    {
        ds->params.data[i] = gs_alloc_bytes(dev->memory, span * factor,
                                            "gx_downscaler(planar_data)");
        if (ds->params.data[i] == NULL)
            goto cleanup;
    }

    if (dst_bpc == 1)
    {
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
    }
    else if (factor == 1)
        core = NULL;
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
    if (dst_bpc == 1)
    {
        ds->errors = (int *)gs_alloc_bytes(dev->memory,
                                           num_comps*(width+3)*sizeof(int),
                                           "gx_downscaler(errors)");
        if (ds->errors == NULL)
        {
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
    int                size = gdev_mem_bytes_per_scan_line((gx_device *)dev);
    int                span;
    int                width = dev->width/factor;
    int                awidth = width;
    int                pad_white;
    int                code;
    gx_downscale_core *core;
    
    if (adjust_width_proc != NULL)
        awidth = (*adjust_width_proc)(width, adjust_width);
    pad_white = awidth - width;
    if (pad_white < 0)
        pad_white = 0;

    span = size + pad_white*factor*num_comps + factor-1;
    memset(ds, 0, sizeof(*ds));
    ds->dev        = dev;
    ds->width      = width;
    ds->awidth     = awidth;
    ds->span       = span;
    ds->factor     = factor;
    ds->num_planes = 0;
    
    /* Choose an appropriate core */
    if ((src_bpc == 8) && (dst_bpc == 1) && (num_comps == 1))
    {
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
    }
    else if (factor == 1)
        core = NULL;
    else if ((src_bpc == 8) && (dst_bpc == 8) && (num_comps == 1))
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
        if (dst_bpc == 1)
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
    int plane;
    for (plane=0; plane < ds->num_planes; plane++)
    {
        gs_free_object(ds->dev->memory, ds->params.data[plane],
                       "gx_downscaler(planar_data)");
    }
    ds->num_planes = 0;

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
    
    (ds->down_core)(ds, out_data, ds->data, row, 0, ds->span);

    return code;
}

int gx_downscaler_get_bits_rectangle(gx_downscaler_t      *ds,
                                     gs_get_bits_params_t *params,
                                     int                   row)
{
    int                   code;
    gs_int_rect           rect;
    int                   plane;
    int                   factor = ds->factor;
    gs_get_bits_params_t  params2;


    rect.p.x = 0;
    rect.p.y = row * factor;
    rect.q.x = ds->dev->width;
    rect.q.y = (row + 1) * factor;

    /* Check for the simple case */
    if (ds->down_core == NULL) {
        return (*dev_proc(ds->dev, get_bits_rectangle))(ds->dev, &rect, params, NULL);
    }

    /* Copy the params, because get_bits_rectangle can helpfully overwrite
     * them. */
    memcpy(&params2, &ds->params, sizeof(params2));
    /* Get factor rows worth of data */
    code = (*dev_proc(ds->dev, get_bits_rectangle))(ds->dev, &rect, &params2, NULL);
    if (code == gs_error_rangecheck)
    {
        int i, j;
        int copy = ds->dev->width; /* Assumes 8bpp */
        /* At the bottom of a band, the get_bits_rectangle call can fail to be
         * able to return us enough lines of data at the same time. We therefore
         * drop back to reading them one at a time, and copying them into our
         * own buffer. */
        for (i = 0; i < factor; i++) {
            rect.q.y = rect.p.y+1;
            if (rect.q.y > ds->dev->height)
                break;
            memcpy(&params2, &ds->params, sizeof(params2));
            code = (*dev_proc(ds->dev, get_bits_rectangle))(ds->dev, &rect, &params2, NULL);
            if (code < 0)
                break;
            for (j = 0; j < ds->num_planes; j++) {
                memcpy(ds->params.data[j] + i*ds->span, params2.data[j], copy);
            }
            rect.p.y++;
        }
        if (i == 0)
            return code;
        /* If we still haven't got enough, we've hit the end of the page; just
         * duplicate the last line we did get. */
        for (;i < factor; i++) {
            for (j = 0; j < ds->num_planes; j++) {
                memcpy(ds->params.data[j] + i*ds->span, ds->params.data[j] + (i-1)*ds->span, copy);
            }
        }
        for (j = 0; j < ds->num_planes; j++) {
            params2.data[j] = ds->params.data[j];
        }
    }
    if (code < 0)
        return code;

    for (plane=0; plane < ds->num_planes; plane++)
    {
        (ds->down_core)(ds, params->data[plane], params2.data[plane], row, plane, params2.raster);
    }

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
