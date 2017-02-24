/* Copyright (C) 2001-2012 Artifex Software, Inc.
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

/* PDF 1.4 blending functions */

#include "memory_.h"
#include "gx.h"
#include "gp.h"
#include "gstparam.h"
#include "gxblend.h"
#include "gxcolor2.h"
#include "gsicc_cache.h"
#include "gsicc_manage.h"
#include "gdevp14.h"

typedef int art_s32;

#if RAW_DUMP
extern unsigned int global_index;
extern unsigned int clist_band_count;
#endif

/* For spot colors, blend modes must be white preserving and separable.  The
 * order of the blend modes should be reordered so this is a single compare */
bool
blend_valid_for_spot(gs_blend_mode_t blend_mode)
{
    if (blend_mode == BLEND_MODE_Difference ||
        blend_mode == BLEND_MODE_Exclusion ||
        blend_mode == BLEND_MODE_Hue ||
        blend_mode == BLEND_MODE_Saturation ||
        blend_mode == BLEND_MODE_Color ||
        blend_mode == BLEND_MODE_Luminosity)
        return false;
    else
        return true;
}

/* This function is used for mapping the SMask source to a
   monochrome luminosity value which basically is the alpha value
   Note, that separation colors are not allowed here.  Everything
   must be in CMYK, RGB or monochrome.  */

/* Note, data is planar */

void
smask_luminosity_mapping(int num_rows, int num_cols, int n_chan, int row_stride,
                         int plane_stride, byte *src, const byte *dst, bool isadditive,
                         gs_transparency_mask_subtype_t SMask_SubType)
{
    int x,y;
    int mask_alpha_offset,mask_C_offset,mask_M_offset,mask_Y_offset,mask_K_offset;
    int mask_R_offset,mask_G_offset,mask_B_offset;
    byte *dstptr;

#if RAW_DUMP
    dump_raw_buffer(num_rows, row_stride, n_chan,
                plane_stride, row_stride,
                "Raw_Mask", src);

    global_index++;
#endif
    dstptr = (byte *)dst;
    /* If subtype is Luminosity then we should just grab the Y channel */
    if ( SMask_SubType == TRANSPARENCY_MASK_Luminosity ){
        memcpy(dstptr, &(src[plane_stride]), plane_stride);
        return;
    }
    /* If we are alpha type, then just grab that */
    /* We need to optimize this so that we are only drawing alpha in the rect fills */
    if ( SMask_SubType == TRANSPARENCY_MASK_Alpha ){
        mask_alpha_offset = (n_chan - 1) * plane_stride;
        memcpy(dstptr, &(src[mask_alpha_offset]), plane_stride);
        return;
    }
    /* To avoid the if statement inside this loop,
    decide on additive or subractive now */
    if (isadditive || n_chan == 2) {
        /* Now we need to split Gray from RGB */
        if( n_chan == 2 ) {
            /* Gray Scale case */
           mask_alpha_offset = (n_chan - 1) * plane_stride;
           mask_R_offset = 0;
            for ( y = 0; y < num_rows; y++ ) {
                for ( x = 0; x < num_cols; x++ ){
                    /* With the current design this will indicate if
                    we ever did a fill at this pixel. if not then move on.
                    This could have some serious optimization */
                    if (src[x + mask_alpha_offset] != 0x00) {
                        dstptr[x] = src[x + mask_R_offset];
                    }
                }
               dstptr += row_stride;
               mask_alpha_offset += row_stride;
               mask_R_offset += row_stride;
            }
        } else {
            /* RGB case */
           mask_R_offset = 0;
           mask_G_offset = plane_stride;
           mask_B_offset = 2 * plane_stride;
           mask_alpha_offset = (n_chan - 1) * plane_stride;
            for ( y = 0; y < num_rows; y++ ) {
               for ( x = 0; x < num_cols; x++ ){
                    /* With the current design this will indicate if
                    we ever did a fill at this pixel. if not then move on */
                    if (src[x + mask_alpha_offset] != 0x00) {
                        /* Get luminosity of Device RGB value */
                        float temp;
                        temp = ( 0.30 * src[x + mask_R_offset] +
                            0.59 * src[x + mask_G_offset] +
                            0.11 * src[x + mask_B_offset] );
                        temp = temp * (1.0 / 255.0 );  /* May need to be optimized */
                        dstptr[x] = float_color_to_byte_color(temp);
                    }
                }
               dstptr += row_stride;
               mask_alpha_offset += row_stride;
               mask_R_offset += row_stride;
               mask_G_offset += row_stride;
               mask_B_offset += row_stride;
            }
        }
    } else {
       /* CMYK case */
       mask_alpha_offset = (n_chan - 1) * plane_stride;
       mask_C_offset = 0;
       mask_M_offset = plane_stride;
       mask_Y_offset = 2 * plane_stride;
       mask_K_offset = 3 * plane_stride;
       for ( y = 0; y < num_rows; y++ ){
            for ( x = 0; x < num_cols; x++ ){
                /* With the current design this will indicate if
                we ever did a fill at this pixel. if not then move on */
                if (src[x + mask_alpha_offset] != 0x00){
                  /* PDF spec says to use Y = 0.30 (1 - C)(1 - K) +
                  0.59 (1 - M)(1 - K) + 0.11 (1 - Y)(1 - K) */
                    /* For device CMYK */
                    float temp;
                    temp = ( 0.30 * ( 0xff - src[x + mask_C_offset]) +
                        0.59 * ( 0xff - src[x + mask_M_offset]) +
                        0.11 * ( 0xff - src[x + mask_Y_offset]) ) *
                        ( 0xff - src[x + mask_K_offset]);
                    temp = temp * (1.0 / 65025.0 );  /* May need to be optimized */
                    dstptr[x] = float_color_to_byte_color(temp);
                }
            }
           dstptr += row_stride;
           mask_alpha_offset += row_stride;
           mask_C_offset += row_stride;
           mask_M_offset += row_stride;
           mask_Y_offset += row_stride;
           mask_K_offset += row_stride;
        }
    }
}

/* soft mask gray buffer should be blended with its transparency planar data
   during the pop for a luminosity case if we have a soft mask within a soft
   mask.  This situation is detected in the code so that we only do this
   blending in those rare situations */
void
smask_blend(byte *src, int width, int height, int rowstride,
                      int planestride)
{
    int x, y;
    int position;
    byte comp, a;
    int tmp;
    byte bg = 0;

    for (y = 0; y < height; y++) {
        position = y * rowstride;
        for (x = 0; x < width; x++) {
            a = src[position + planestride];
            if ((a + 1) & 0xfe) {
                a ^= 0xff;
                comp  = src[position];
                tmp = ((bg - comp) * a) + 0x80;
                comp += (tmp + (tmp >> 8)) >> 8;
                src[position] = comp;
            } else if (a == 0) {
                src[position] = 0;
            }
            position+=1;
        }
    }
}

void smask_copy(int num_rows, int num_cols, int row_stride,
                        byte *src, const byte *dst)
{
    int y;
    byte *dstptr,*srcptr;

    dstptr = (byte *)dst;
    srcptr = src;
    for ( y = 0; y < num_rows; y++ ) {
        memcpy(dstptr,srcptr,num_cols);
        dstptr += row_stride;
        srcptr += row_stride;
    }
}

void smask_icc(gx_device *dev, int num_rows, int num_cols, int n_chan,
               int row_stride, int plane_stride, byte *src, const byte *dst,
               gsicc_link_t *icclink)
{
    gsicc_bufferdesc_t input_buff_desc;
    gsicc_bufferdesc_t output_buff_desc;

#if RAW_DUMP
    dump_raw_buffer(num_rows, row_stride, n_chan,
                plane_stride, row_stride,
                "Raw_Mask_ICC", src);
    global_index++;
#endif
/* Set up the buffer descriptors. Note that pdf14 always has
   the alpha channels at the back end (last planes).
   We will just handle that here and let the CMM know
   nothing about it */

    gsicc_init_buffer(&input_buff_desc, n_chan-1, 1,
                  false, false, true, plane_stride, row_stride,
                  num_rows, num_cols);
    gsicc_init_buffer(&output_buff_desc, 1, 1,
                  false, false, true, plane_stride,
                  row_stride, num_rows, num_cols);
    /* Transform the data */
    (icclink->procs.map_buffer)(dev, icclink, &input_buff_desc, &output_buff_desc,
                                (void*) src, (void*) dst);
}

void
art_blend_luminosity_rgb_8(int n_chan, byte *dst, const byte *backdrop,
                           const byte *src)
{
    int rb = backdrop[0], gb = backdrop[1], bb = backdrop[2];
    int rs = src[0], gs = src[1], bs = src[2];
    int delta_y;
    int r, g, b;

    /*
     * From section 7.4 of the PDF 1.5 specification, for RGB, the luminosity
     * is:  Y = 0.30 R + 0.59 G + 0.11 B)
     */
    delta_y = ((rs - rb) * 77 + (gs - gb) * 151 + (bs - bb) * 28 + 0x80) >> 8;
    r = rb + delta_y;
    g = gb + delta_y;
    b = bb + delta_y;
    if ((r | g | b) & 0x100) {
        int y;
        int scale;

        y = (rs * 77 + gs * 151 + bs * 28 + 0x80) >> 8;
        if (delta_y > 0) {
            int max;

            max = r > g ? r : g;
            max = b > max ? b : max;
            scale = ((255 - y) << 16) / (max - y);
        } else {
            int min;

            min = r < g ? r : g;
            min = b < min ? b : min;
            scale = (y << 16) / (y - min);
        }
        r = y + (((r - y) * scale + 0x8000) >> 16);
        g = y + (((g - y) * scale + 0x8000) >> 16);
        b = y + (((b - y) * scale + 0x8000) >> 16);
    }
    dst[0] = r;
    dst[1] = g;
    dst[2] = b;
}

void
art_blend_luminosity_custom_8(int n_chan, byte *dst, const byte *backdrop,
                                const byte *src)
{
    int delta_y = 0, test = 0;
    int r[ART_MAX_CHAN];
    int i;

    /*
     * Since we do not know the details of the blending color space, we are
     * simply using the average as the luminosity.  First we need the
     * delta luminosity values.
     */
    for (i = 0; i < n_chan; i++)
        delta_y += src[i] - backdrop[i];
    delta_y = (delta_y + n_chan / 2) / n_chan;
    for (i = 0; i < n_chan; i++) {
        r[i] = backdrop[i] + delta_y;
        test |= r[i];
    }

    if (test & 0x100) {
        int y;
        int scale;

        /* Assume that the luminosity is simply the average of the backdrop. */
        y = src[0];
        for (i = 1; i < n_chan; i++)
            y += src[i];
        y = (y + n_chan / 2) / n_chan;

        if (delta_y > 0) {
            int max;

            max = r[0];
            for (i = 1; i < n_chan; i++)
                max = max(max, r[i]);
            scale = ((255 - y) << 16) / (max - y);
        } else {
            int min;

            min = r[0];
            for (i = 1; i < n_chan; i++)
                min = min(min, r[i]);
            scale = (y << 16) / (y - min);
        }
        for (i = 0; i < n_chan; i++)
            r[i] = y + (((r[i] - y) * scale + 0x8000) >> 16);
    }
    for (i = 0; i < n_chan; i++)
        dst[i] = r[i];
}

/*
 * The PDF 1.4 spec. does not give the details of the math involved in the
 * luminosity blending.  All we are given is:
 *   "Creates a color with the luminance of the source color and the hue
 *    and saturation of the backdrop color. This produces an inverse
 *    effect to that of the Color mode."
 * From section 7.4 of the PDF 1.5 specification, which is duscussing soft
 * masks, we are given that, for CMYK, the luminosity is:
 *    Y = 0.30 (1 - C)(1 - K) + 0.59 (1 - M)(1 - K) + 0.11 (1 - Y)(1 - K)
 * However the results of this equation do not match the results seen from
 * Illustrator CS.  Very different results are obtained if process gray
 * (.5, .5, .5, 0) is blended over pure cyan, versus gray (0, 0, 0, .5) over
 * the same pure cyan.  The first gives a medium cyan while the later gives a
 * medium gray.  This routine seems to match Illustrator's actions.  C, M and Y
 * are treated similar to RGB in the previous routine and black is treated
 * separately.
 *
 * Our component values have already been complemented, i.e. (1 - X).
 */
void
art_blend_luminosity_cmyk_8(int n_chan, byte *dst, const byte *backdrop,
                           const byte *src)
{
    int i;

    /* Treat CMY the same as RGB. */
    art_blend_luminosity_rgb_8(3, dst, backdrop, src);
    for (i = 3; i < n_chan; i++)
        dst[i] = src[i];
}

void
art_blend_saturation_rgb_8(int n_chan, byte *dst, const byte *backdrop,
                           const byte *src)
{
    int rb = backdrop[0], gb = backdrop[1], bb = backdrop[2];
    int rs = src[0], gs = src[1], bs = src[2];
    int minb, maxb;
    int mins, maxs;
    int y;
    int scale;
    int r, g, b;

    minb = rb < gb ? rb : gb;
    minb = minb < bb ? minb : bb;
    maxb = rb > gb ? rb : gb;
    maxb = maxb > bb ? maxb : bb;
    if (minb == maxb) {
        /* backdrop has zero saturation, avoid divide by 0 */
        dst[0] = gb;
        dst[1] = gb;
        dst[2] = gb;
        return;
    }

    mins = rs < gs ? rs : gs;
    mins = mins < bs ? mins : bs;
    maxs = rs > gs ? rs : gs;
    maxs = maxs > bs ? maxs : bs;

    scale = ((maxs - mins) << 16) / (maxb - minb);
    y = (rb * 77 + gb * 151 + bb * 28 + 0x80) >> 8;
    r = y + ((((rb - y) * scale) + 0x8000) >> 16);
    g = y + ((((gb - y) * scale) + 0x8000) >> 16);
    b = y + ((((bb - y) * scale) + 0x8000) >> 16);

    if ((r | g | b) & 0x100) {
        int scalemin, scalemax;
        int min, max;

        min = r < g ? r : g;
        min = min < b ? min : b;
        max = r > g ? r : g;
        max = max > b ? max : b;

        if (min < 0)
            scalemin = (y << 16) / (y - min);
        else
            scalemin = 0x10000;

        if (max > 255)
            scalemax = ((255 - y) << 16) / (max - y);
        else
            scalemax = 0x10000;

        scale = scalemin < scalemax ? scalemin : scalemax;
        r = y + (((r - y) * scale + 0x8000) >> 16);
        g = y + (((g - y) * scale + 0x8000) >> 16);
        b = y + (((b - y) * scale + 0x8000) >> 16);
    }

    dst[0] = r;
    dst[1] = g;
    dst[2] = b;
}

void
art_blend_saturation_custom_8(int n_chan, byte *dst, const byte *backdrop,
                           const byte *src)
{
    int minb, maxb;
    int mins, maxs;
    int y;
    int scale;
    int r[ART_MAX_CHAN];
    int test = 0;
    int temp, i;

    /* Determine min and max of the backdrop */
    minb = maxb = temp = backdrop[0];
    for (i = 1; i < n_chan; i++) {
        temp = backdrop[i];
        minb = min(minb, temp);
        maxb = max(maxb, temp);
    }

    if (minb == maxb) {
        /* backdrop has zero saturation, avoid divide by 0 */
        for (i = 0; i < n_chan; i++)
            dst[i] = temp;
        return;
    }

    /* Determine min and max of the source */
    mins = maxs = src[0];
    for (i = 1; i < n_chan; i++) {
        temp = src[i];
        mins = min(minb, temp);
        maxs = max(minb, temp);
    }

    scale = ((maxs - mins) << 16) / (maxb - minb);

    /* Assume that the saturation is simply the average of the backdrop. */
    y = backdrop[0];
    for (i = 1; i < n_chan; i++)
        y += backdrop[i];
    y = (y + n_chan / 2) / n_chan;

    /* Calculate the saturated values */
    for (i = 0; i < n_chan; i++) {
        r[i] = y + ((((backdrop[i] - y) * scale) + 0x8000) >> 16);
        test |= r[i];
    }

    if (test & 0x100) {
        int scalemin, scalemax;
        int min, max;

        /* Determine min and max of our blended values */
        min = max = temp = r[0];
        for (i = 1; i < n_chan; i++) {
            temp = src[i];
            min = min(min, temp);
            max = max(max, temp);
        }

        if (min < 0)
            scalemin = (y << 16) / (y - min);
        else
            scalemin = 0x10000;

        if (max > 255)
            scalemax = ((255 - y) << 16) / (max - y);
        else
            scalemax = 0x10000;

        scale = scalemin < scalemax ? scalemin : scalemax;
        for (i = 0; i < n_chan; i++)
            r[i] = y + (((r[i] - y) * scale + 0x8000) >> 16);
    }

    for (i = 0; i < n_chan; i++)
        dst[i] = r[i];
}

/* Our component values have already been complemented, i.e. (1 - X). */
void
art_blend_saturation_cmyk_8(int n_chan, byte *dst, const byte *backdrop,
                           const byte *src)
{
    int i;

    /* Treat CMY the same as RGB */
    art_blend_saturation_rgb_8(3, dst, backdrop, src);
    for (i = 3; i < n_chan; i++)
        dst[i] = backdrop[i];
}

/* This array consists of floor ((x - x * x / 255.0) * 65536 / 255 +
   0.5) for x in [0..255]. */
const unsigned int art_blend_sq_diff_8[256] = {
    0, 256, 510, 762, 1012, 1260, 1506, 1750, 1992, 2231, 2469, 2705,
    2939, 3171, 3401, 3628, 3854, 4078, 4300, 4519, 4737, 4953, 5166,
    5378, 5588, 5795, 6001, 6204, 6406, 6606, 6803, 6999, 7192, 7384,
    7573, 7761, 7946, 8129, 8311, 8490, 8668, 8843, 9016, 9188, 9357,
    9524, 9690, 9853, 10014, 10173, 10331, 10486, 10639, 10790, 10939,
    11086, 11232, 11375, 11516, 11655, 11792, 11927, 12060, 12191, 12320,
    12447, 12572, 12695, 12816, 12935, 13052, 13167, 13280, 13390, 13499,
    13606, 13711, 13814, 13914, 14013, 14110, 14205, 14297, 14388, 14477,
    14564, 14648, 14731, 14811, 14890, 14967, 15041, 15114, 15184, 15253,
    15319, 15384, 15446, 15507, 15565, 15622, 15676, 15729, 15779, 15827,
    15874, 15918, 15960, 16001, 16039, 16075, 16110, 16142, 16172, 16200,
    16227, 16251, 16273, 16293, 16311, 16327, 16341, 16354, 16364, 16372,
    16378, 16382, 16384, 16384, 16382, 16378, 16372, 16364, 16354, 16341,
    16327, 16311, 16293, 16273, 16251, 16227, 16200, 16172, 16142, 16110,
    16075, 16039, 16001, 15960, 15918, 15874, 15827, 15779, 15729, 15676,
    15622, 15565, 15507, 15446, 15384, 15319, 15253, 15184, 15114, 15041,
    14967, 14890, 14811, 14731, 14648, 14564, 14477, 14388, 14297, 14205,
    14110, 14013, 13914, 13814, 13711, 13606, 13499, 13390, 13280, 13167,
    13052, 12935, 12816, 12695, 12572, 12447, 12320, 12191, 12060, 11927,
    11792, 11655, 11516, 11375, 11232, 11086, 10939, 10790, 10639, 10486,
    10331, 10173, 10014, 9853, 9690, 9524, 9357, 9188, 9016, 8843, 8668,
    8490, 8311, 8129, 7946, 7761, 7573, 7384, 7192, 6999, 6803, 6606,
    6406, 6204, 6001, 5795, 5588, 5378, 5166, 4953, 4737, 4519, 4300,
    4078, 3854, 3628, 3401, 3171, 2939, 2705, 2469, 2231, 1992, 1750,
    1506, 1260, 1012, 762, 510, 256, 0
};

/* This array consists of SoftLight (x, 255) - x, for values of x in
   the range [0..255] (normalized to [0..255 range). The original
   values were directly sampled from Adobe Illustrator 9. I've fit a
   quadratic spline to the SoftLight (x, 1) function as follows
   (normalized to [0..1] range):

   Anchor point (0, 0)
   Control point (0.0755, 0.302)
   Anchor point (0.18, 0.4245)
   Control point (0.4263, 0.7131)
   Anchor point (1, 1)

   I don't believe this is _exactly_ the function that Adobe uses,
   but it really should be close enough for all practical purposes.  */
const byte art_blend_soft_light_8[256] = {
    0, 3, 6, 9, 11, 14, 16, 19, 21, 23, 26, 28, 30, 32, 33, 35, 37, 39,
    40, 42, 43, 45, 46, 47, 48, 49, 51, 52, 53, 53, 54, 55, 56, 57, 57,
    58, 58, 59, 60, 60, 60, 61, 61, 62, 62, 62, 62, 63, 63, 63, 63, 63,
    63, 63, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64,
    64, 64, 64, 63, 63, 63, 63, 63, 63, 63, 63, 63, 63, 63, 62, 62, 62,
    62, 62, 62, 62, 61, 61, 61, 61, 61, 61, 60, 60, 60, 60, 60, 59, 59,
    59, 59, 59, 58, 58, 58, 58, 57, 57, 57, 57, 56, 56, 56, 56, 55, 55,
    55, 55, 54, 54, 54, 54, 53, 53, 53, 52, 52, 52, 51, 51, 51, 51, 50,
    50, 50, 49, 49, 49, 48, 48, 48, 47, 47, 47, 46, 46, 46, 45, 45, 45,
    44, 44, 43, 43, 43, 42, 42, 42, 41, 41, 40, 40, 40, 39, 39, 39, 38,
    38, 37, 37, 37, 36, 36, 35, 35, 35, 34, 34, 33, 33, 33, 32, 32, 31,
    31, 31, 30, 30, 29, 29, 28, 28, 28, 27, 27, 26, 26, 25, 25, 25, 24,
    24, 23, 23, 22, 22, 21, 21, 21, 20, 20, 19, 19, 18, 18, 17, 17, 16,
    16, 15, 15, 15, 14, 14, 13, 13, 12, 12, 11, 11, 10, 10, 9, 9, 8, 8, 7,
    7, 6, 6, 5, 5, 4, 4, 3, 3, 2, 2, 1, 1, 0, 0
};

void
art_blend_pixel_8(byte *dst, const byte *backdrop,
                  const byte *src, int n_chan, gs_blend_mode_t blend_mode,
                  const pdf14_nonseparable_blending_procs_t * pblend_procs,
                  pdf14_device *p14dev)
{
    int i;
    byte b, s;
    bits32 t;

    switch (blend_mode) {
        case BLEND_MODE_Normal:
        case BLEND_MODE_Compatible:	/* todo */
            memcpy(dst, src, n_chan);
            break;
        case BLEND_MODE_Multiply:
            for (i = 0; i < n_chan; i++) {
                t = ((bits32) backdrop[i]) * ((bits32) src[i]);
                t += 0x80;
                t += (t >> 8);
                dst[i] = t >> 8;
            }
            break;
        case BLEND_MODE_Screen:
            for (i = 0; i < n_chan; i++) {
                t =
                    ((bits32) (0xff - backdrop[i])) *
                    ((bits32) (0xff - src[i]));
                t += 0x80;
                t += (t >> 8);
                dst[i] = 0xff - (t >> 8);
            }
            break;
        case BLEND_MODE_Overlay:
            for (i = 0; i < n_chan; i++) {
                b = backdrop[i];
                s = src[i];
                if (b < 0x80)
                    t = 2 * ((bits32) b) * ((bits32) s);
                else
                    t = 0xfe01 -
                        2 * ((bits32) (0xff - b)) * ((bits32) (0xff - s));
                t += 0x80;
                t += (t >> 8);
                dst[i] = t >> 8;
            }
            break;
        case BLEND_MODE_SoftLight:
            for (i = 0; i < n_chan; i++) {
                b = backdrop[i];
                s = src[i];
                if (s < 0x80) {
                    t = (0xff - (s << 1)) * art_blend_sq_diff_8[b];
                    t += 0x8000;
                    dst[i] = b - (t >> 16);
                } else {
                    t =
                        ((s << 1) -
                         0xff) * ((bits32) (art_blend_soft_light_8[b]));
                    t += 0x80;
                    t += (t >> 8);
                    dst[i] = b + (t >> 8);
                }
            }
            break;
        case BLEND_MODE_HardLight:
            for (i = 0; i < n_chan; i++) {
                b = backdrop[i];
                s = src[i];
                if (s < 0x80)
                    t = 2 * ((bits32) b) * ((bits32) s);
                else
                    t = 0xfe01 -
                        2 * ((bits32) (0xff - b)) * ((bits32) (0xff - s));
                t += 0x80;
                t += (t >> 8);
                dst[i] = t >> 8;
            }
            break;
        case BLEND_MODE_ColorDodge:
            for (i = 0; i < n_chan; i++) {
                b = backdrop[i];
                s = 0xff - src[i];
                if (b == 0)
                    dst[i] = 0;
                else if (b >= s)
                    dst[i] = 0xff;
                else
                    dst[i] = (0x1fe * b + s) / (s << 1);
            }
            break;
        case BLEND_MODE_ColorBurn:
            for (i = 0; i < n_chan; i++) {
                b = 0xff - backdrop[i];
                s = src[i];
                if (b == 0)
                    dst[i] = 0xff;
                else if (b >= s)
                    dst[i] = 0;
                else
                    dst[i] = 0xff - (0x1fe * b + s) / (s << 1);
            }
            break;
        case BLEND_MODE_Darken:
            for (i = 0; i < n_chan; i++) {
                b = backdrop[i];
                s = src[i];
                dst[i] = b < s ? b : s;
            }
            break;
        case BLEND_MODE_Lighten:
            for (i = 0; i < n_chan; i++) {
                b = backdrop[i];
                s = src[i];
                dst[i] = b > s ? b : s;
            }
            break;
        case BLEND_MODE_Difference:
            for (i = 0; i < n_chan; i++) {
                art_s32 tmp;

                tmp = ((art_s32) backdrop[i]) - ((art_s32) src[i]);
                dst[i] = tmp < 0 ? -tmp : tmp;
            }
            break;
        case BLEND_MODE_Exclusion:
            for (i = 0; i < n_chan; i++) {
                b = backdrop[i];
                s = src[i];
                t = ((bits32) (0xff - b)) * ((bits32) s) +
                    ((bits32) b) * ((bits32) (0xff - s));
                t += 0x80;
                t += (t >> 8);
                dst[i] = t >> 8;
            }
            break;
        case BLEND_MODE_Luminosity:
            pblend_procs->blend_luminosity(n_chan, dst, backdrop, src);
            break;
        case BLEND_MODE_Color:
            pblend_procs->blend_luminosity(n_chan, dst, src, backdrop);
            break;
        case BLEND_MODE_Saturation:
            pblend_procs->blend_saturation(n_chan, dst, backdrop, src);
            break;
        case BLEND_MODE_Hue:
            {
                byte tmp[4];

                pblend_procs->blend_luminosity(n_chan, tmp, src, backdrop);
                pblend_procs->blend_saturation(n_chan, dst, tmp, backdrop);
            }
            break;
            /* This mode requires information about the color space as
             * well as the overprint mode.  See Section 7.6.3 of
             * PDF specification */
        case BLEND_MODE_CompatibleOverprint:
            {
                gx_color_index drawn_comps = p14dev->drawn_comps;
                gx_color_index comps;
                /* If overprint mode is true and the current color space and
                 * the group color space are CMYK (or CMYK and spots), then
                 * B(cb, cs) = cs if cs is nonzero otherwise it is cb for CMYK.
                 * Spot colors are always set to cb.  The nice thing about the PDF14
                 * compositor is that it always has CMYK + spots with spots after
                 * the CMYK colorants (see gx_put_blended_image_cmykspot).
                 * that way we don't have to worry about where the process colors
                 * are. */
                if (p14dev->overprint_mode && p14dev->color_info.num_components > 3
                    && !(p14dev->ctx->additive)) {
                    for (i = 0; i < 4; i++) {
                        b = backdrop[i];
                        s = src[i];
                        dst[i] = s < 0xff ? s : b; /* Subtractive zero */
                    }
                    for (i = 4; i < n_chan; i++) {
                        dst[i] = backdrop[i];
                    }
                } else {
                    /* Otherwise we have B(cb, cs)= cs if cs is specified in
                     * the current color space all other color should get cb.
                     * Essentially the standard overprint case. */
                    for (i = 0, comps = drawn_comps; comps != 0; ++i, comps >>= 1) {
                        if ((comps & 0x1) != 0) {
                            dst[i] = src[i];
                        } else {
                            dst[i] = backdrop[i];
                        }
                    }
                }
                break;
            }
        default:
#ifndef GS_THREADSAFE
            dlprintf1("art_blend_pixel_8: blend mode %d not implemented\n",
                      blend_mode);
#endif
            memcpy(dst, src, n_chan);
            break;
    }
}

byte
art_pdf_union_8(byte alpha1, byte alpha2)
{
    int tmp;

    tmp = (0xff - alpha1) * (0xff - alpha2) + 0x80;
    return 0xff - ((tmp + (tmp >> 8)) >> 8);
}

byte
art_pdf_union_mul_8(byte alpha1, byte alpha2, byte alpha_mask)
{
    int tmp;

    if (alpha_mask == 0xff) {
        tmp = (0xff - alpha1) * (0xff - alpha2) + 0x80;
        return 0xff - ((tmp + (tmp >> 8)) >> 8);
    } else {
        tmp = alpha2 * alpha_mask + 0x80;
        tmp = (tmp + (tmp >> 8)) >> 8;
        tmp = (0xff - alpha1) * (0xff - tmp) + 0x80;
        return 0xff - ((tmp + (tmp >> 8)) >> 8);
    }
}

static void
art_pdf_knockout_composite_pixel_alpha_8(byte *backdrop, byte tos_shape, byte *dst,
                        const byte *src, int n_chan, gs_blend_mode_t blend_mode,
                        const pdf14_nonseparable_blending_procs_t * pblend_procs,
                        pdf14_device *p14dev)
{
    byte a_b, a_s;
    unsigned int a_r;
    int tmp;
    int src_scale;
    int c_b, c_s;
    int i;

    a_s = src[n_chan];
    a_b = backdrop[n_chan];
    if (a_s == 0) {
        /* source alpha is zero, if we have a src shape value there then copy
           the backdrop, else leave it alone */
        if (tos_shape)
           memcpy(dst, backdrop, n_chan + 1);
        return;
    }

    /* In this case a_s is not zero */
    if (a_b == 0) {
        /* backdrop alpha is zero but not source alpha, just copy source pixels and
           avoid computation. */
        memcpy(dst, src, n_chan + 1);
        return;
    }

    /* Result alpha is Union of backdrop and source alpha */
    tmp = (0xff - a_b) * (0xff - a_s) + 0x80;
    a_r = 0xff - (((tmp >> 8) + tmp) >> 8);
    /* todo: verify that a_r is nonzero in all cases */

    /* Compute a_s / a_r in 16.16 format */
    src_scale = ((a_s << 16) + (a_r >> 1)) / a_r;

    if (blend_mode == BLEND_MODE_Normal) {
        /* Do simple compositing of source over backdrop */
        for (i = 0; i < n_chan; i++) {
            c_s = src[i];
            c_b = backdrop[i];
            tmp = (c_b << 16) + src_scale * (c_s - c_b) + 0x8000;
            dst[i] = tmp >> 16;
        }
    } else {
        /* Do compositing with blending */
        byte blend[ART_MAX_CHAN];

        art_blend_pixel_8(blend, backdrop, src, n_chan, blend_mode, pblend_procs,
                        p14dev);
        for (i = 0; i < n_chan; i++) {
            int c_bl;		/* Result of blend function */
            int c_mix;		/* Blend result mixed with source color */

            c_s = src[i];
            c_b = backdrop[i];
            c_bl = blend[i];
            tmp = a_b * (c_bl - ((int)c_s)) + 0x80;
            c_mix = c_s + (((tmp >> 8) + tmp) >> 8);
            tmp = (c_b << 16) + src_scale * (c_mix - c_b) + 0x8000;
            dst[i] = tmp >> 16;
        }
    }
    dst[n_chan] = a_r;
}

void
art_pdf_composite_pixel_alpha_8(byte *dst, const byte *src, int n_chan,
        gs_blend_mode_t blend_mode,
        const pdf14_nonseparable_blending_procs_t * pblend_procs, pdf14_device *p14dev)
{
    byte a_b, a_s;
    unsigned int a_r;
    int tmp;
    int src_scale;
    int c_b, c_s;
    int i;

    a_s = src[n_chan];
    if (a_s == 0) {
        /* source alpha is zero, avoid all computations and possible
           divide by zero errors. */
        return;
    }

    a_b = dst[n_chan];
    if (a_b == 0) {
        /* backdrop alpha is zero, just copy source pixels and avoid
           computation. */

        memcpy (dst, src, n_chan + 1);

        return;
    }

    /* Result alpha is Union of backdrop and source alpha */
    tmp = (0xff - a_b) * (0xff - a_s) + 0x80;
    a_r = 0xff - (((tmp >> 8) + tmp) >> 8);
    /* todo: verify that a_r is nonzero in all cases */

    /* Compute a_s / a_r in 16.16 format */
    src_scale = ((a_s << 16) + (a_r >> 1)) / a_r;

    if (blend_mode == BLEND_MODE_Normal) {
        /* Do simple compositing of source over backdrop */
        for (i = 0; i < n_chan; i++) {
            c_s = src[i];
            c_b = dst[i];
            tmp = (c_b << 16) + src_scale * (c_s - c_b) + 0x8000;
            dst[i] = tmp >> 16;
        }
    } else {
        /* Do compositing with blending */
        byte blend[ART_MAX_CHAN];

        art_blend_pixel_8(blend, dst, src, n_chan, blend_mode, pblend_procs, p14dev);
        for (i = 0; i < n_chan; i++) {
            int c_bl;		/* Result of blend function */
            int c_mix;		/* Blend result mixed with source color */

            c_s = src[i];
            c_b = dst[i];
            c_bl = blend[i];
            tmp = a_b * (c_bl - ((int)c_s)) + 0x80;
            c_mix = c_s + (((tmp >> 8) + tmp) >> 8);
            tmp = (c_b << 16) + src_scale * (c_mix - c_b) + 0x8000;
            dst[i] = tmp >> 16;
        }
    }
    dst[n_chan] = a_r;
}

void
art_pdf_composite_pixel_alpha_8_fast(byte *dst, const byte *src, int n_chan,
                                     gs_blend_mode_t blend_mode,
                                     const pdf14_nonseparable_blending_procs_t * pblend_procs,
                                     int stride, pdf14_device *p14dev)
{
    byte a_b, a_s;
    unsigned int a_r;
    int tmp;
    int src_scale;
    int c_b, c_s;
    int i;

    a_s = src[n_chan];

    a_b = dst[n_chan * stride];

    /* Result alpha is Union of backdrop and source alpha */
    tmp = (0xff - a_b) * (0xff - a_s) + 0x80;
    a_r = 0xff - (((tmp >> 8) + tmp) >> 8);
    /* todo: verify that a_r is nonzero in all cases */

    /* Compute a_s / a_r in 16.16 format */
    src_scale = ((a_s << 16) + (a_r >> 1)) / a_r;

    if (blend_mode == BLEND_MODE_Normal) {
        /* Do simple compositing of source over backdrop */
        for (i = 0; i < n_chan; i++) {
            c_s = src[i];
            c_b = dst[i * stride];
            tmp = (c_b << 16) + src_scale * (c_s - c_b) + 0x8000;
            dst[i * stride] = tmp >> 16;
       }
    } else {
        /* Do compositing with blending */
        byte blend[ART_MAX_CHAN];
        byte dst_tmp[ART_MAX_CHAN];

        for (i = 0; i < n_chan; i++) {
            dst_tmp[i] = dst[i * stride];
        }
        art_blend_pixel_8(blend, dst_tmp, src, n_chan, blend_mode, pblend_procs, p14dev);
        for (i = 0; i < n_chan; i++) {
            int c_bl;		/* Result of blend function */
            int c_mix;		/* Blend result mixed with source color */

            c_s = src[i];
            c_b = dst_tmp[i];
            c_bl = blend[i];
            tmp = a_b * (c_bl - ((int)c_s)) + 0x80;
            c_mix = c_s + (((tmp >> 8) + tmp) >> 8);
            tmp = (c_b << 16) + src_scale * (c_mix - c_b) + 0x8000;
            dst[i * stride] = tmp >> 16;
        }
    }
    dst[n_chan * stride] = a_r;
}

void
art_pdf_composite_pixel_alpha_8_fast_mono(byte *dst, const byte *src,
                                          gs_blend_mode_t blend_mode,
                                          const pdf14_nonseparable_blending_procs_t * pblend_procs,
                                          int stride, pdf14_device *p14dev)
{
    byte a_b, a_s;
    unsigned int a_r;
    int tmp;
    int src_scale;
    int c_b, c_s;

    a_s = src[1];

    a_b = dst[stride];

    /* Result alpha is Union of backdrop and source alpha */
    tmp = (0xff - a_b) * (0xff - a_s) + 0x80;
    a_r = 0xff - (((tmp >> 8) + tmp) >> 8);
    /* todo: verify that a_r is nonzero in all cases */

    /* Compute a_s / a_r in 16.16 format */
    src_scale = ((a_s << 16) + (a_r >> 1)) / a_r;

    if (blend_mode == BLEND_MODE_Normal) {
        /* Do simple compositing of source over backdrop */
        c_s = src[0];
        c_b = dst[0];
        tmp = (c_b << 16) + src_scale * (c_s - c_b) + 0x8000;
        dst[0] = tmp >> 16;
    } else {
        /* Do compositing with blending */
        byte blend[ART_MAX_CHAN];

        art_blend_pixel_8(blend, dst, src, 1, blend_mode, pblend_procs, p14dev);
        {
            int c_bl;		/* Result of blend function */
            int c_mix;		/* Blend result mixed with source color */

            c_s = src[0];
            c_b = dst[0];
            c_bl = blend[0];
            tmp = a_b * (c_bl - ((int)c_s)) + 0x80;
            c_mix = c_s + (((tmp >> 8) + tmp) >> 8);
            tmp = (c_b << 16) + src_scale * (c_mix - c_b) + 0x8000;
            dst[0] = tmp >> 16;
        }
    }
    dst[stride] = a_r;
}

void
art_pdf_recomposite_group_8(byte *dst, byte *dst_alpha_g,
        const byte *src, byte src_alpha_g, int n_chan,
        byte alpha, gs_blend_mode_t blend_mode,
        const pdf14_nonseparable_blending_procs_t * pblend_procs,
        pdf14_device *p14dev, int num_spots)
{
    byte dst_alpha;
    int i;
    int tmp;
    int scale;

    if (src_alpha_g == 0)
        return;

    if (blend_mode == BLEND_MODE_Normal && alpha == 255) {
        /* In this case, uncompositing and recompositing cancel each
           other out. Note: if the reason that alpha == 255 is that
           there is no constant mask and no soft mask, then this
           operation should be optimized away at a higher level. */

        memcpy(dst, src, n_chan + 1);
        if (dst_alpha_g != NULL) {
            tmp = (255 - *dst_alpha_g) * (255 - src_alpha_g) + 0x80;
            *dst_alpha_g = 255 - ((tmp + (tmp >> 8)) >> 8);
        }
        return;
    } else {
        /* "interesting" blend mode */
        byte ca[ART_MAX_CHAN + 1];	/* $C, \alpha$ */

        dst_alpha = dst[n_chan];
        if (src_alpha_g == 255 || dst_alpha == 0) {
            memcpy(ca, src, n_chan + 3);
        } else {
            /* Uncomposite the color. In other words, solve
               "src = (ca, src_alpha_g) over dst" for ca */
            scale = (dst_alpha * 255 * 2 + src_alpha_g) / (src_alpha_g << 1) -
                dst_alpha;
            for (i = 0; i < n_chan; i++) {
                int si, di;

                si = src[i];
                di = dst[i];
                tmp = (si - di) * scale + 0x80;
                tmp = si + ((tmp + (tmp >> 8)) >> 8);

                /* todo: it should be possible to optimize these cond branches */
                if (tmp < 0)
                    tmp = 0;
                if (tmp > 255)
                    tmp = 255;
                ca[i] = tmp;
            }
        }

        tmp = src_alpha_g * alpha + 0x80;
        tmp = (tmp + (tmp >> 8)) >> 8;
        ca[n_chan] = tmp;
        if (dst_alpha_g != NULL) {
            tmp = (255 - *dst_alpha_g) * (255 - tmp) + 0x80;
            *dst_alpha_g = 255 - ((tmp + (tmp >> 8)) >> 8);
        }
        if (num_spots > 0 && !blend_valid_for_spot(blend_mode)) {
            /* Split and do the spots with normal blend mode.
               Blending functions assume alpha is last component so do some
               movements here */
            byte temp_spot_src = ca[n_chan - num_spots];
            byte temp_spot_dst = dst[n_chan - num_spots];
            ca[n_chan - num_spots] = ca[n_chan];
            dst[n_chan - num_spots] = dst[n_chan];

            /* Blend process */
            art_pdf_composite_pixel_alpha_8(dst, ca, n_chan - num_spots,
                blend_mode, pblend_procs, p14dev);

            /* Restore colorants that were blown away by alpha and blend spots */
            dst[n_chan - num_spots] = temp_spot_dst;
            ca[n_chan - num_spots] = temp_spot_src;
            art_pdf_composite_pixel_alpha_8(&(dst[n_chan - num_spots]),
                &(ca[n_chan - num_spots]), num_spots,
                BLEND_MODE_Normal, pblend_procs, p14dev);
        } else {
            art_pdf_composite_pixel_alpha_8(dst, ca, n_chan, blend_mode,
                pblend_procs, p14dev);
        }
    }
    /* todo: optimize BLEND_MODE_Normal buf alpha != 255 case */
}

void
art_pdf_composite_knockout_group_8(byte *backdrop, byte tos_shape, byte *dst,
        byte *dst_alpha_g, const byte *src, int n_chan, byte alpha,
        gs_blend_mode_t blend_mode,
        const pdf14_nonseparable_blending_procs_t * pblend_procs,
        pdf14_device *p14dev, bool has_mask)
{
    byte src_alpha;		/* $\alpha g_n$ */
    byte src_tmp[ART_MAX_CHAN + 1];
    int tmp;

    if (tos_shape == 0) {
        /* If a softmask was present pass it along Bug 693548 */
        if (has_mask)
            dst[n_chan] = alpha;
        return;
    }

    if (alpha == 255) {
        art_pdf_knockout_composite_pixel_alpha_8(backdrop, tos_shape, dst, src,
                                                 n_chan, blend_mode, pblend_procs,
                                                 p14dev);
        if (dst_alpha_g != NULL) {
            tmp = (255 - *dst_alpha_g) * (255 - src[n_chan]) + 0x80;
            *dst_alpha_g = 255 - ((tmp + (tmp >> 8)) >> 8);
        }
    } else {
        if (tos_shape != 255) return;
        src_alpha = src[n_chan];
        if (src_alpha == 0)
            return;
        memcpy(src_tmp, src, n_chan + 3);
        tmp = src_alpha * alpha + 0x80;
        src_tmp[n_chan] = (tmp + (tmp >> 8)) >> 8;
        art_pdf_knockout_composite_pixel_alpha_8(backdrop, tos_shape, dst,
                                                 src_tmp, n_chan,
                                                 blend_mode, pblend_procs,
                                                 p14dev);
        if (dst_alpha_g != NULL) {
            tmp = (255 - *dst_alpha_g) * (255 - src_tmp[n_chan]) + 0x80;
            *dst_alpha_g = 255 - ((tmp + (tmp >> 8)) >> 8);
        }
    }
}

void
art_pdf_composite_group_8(byte *dst, byte *dst_alpha_g,
        const byte *src, int n_chan, byte alpha, gs_blend_mode_t blend_mode,
        const pdf14_nonseparable_blending_procs_t * pblend_procs,
        pdf14_device *p14dev)
{
    byte src_alpha;		/* $\alpha g_n$ */
    byte src_tmp[ART_MAX_CHAN + 1];
    int tmp;

    if (alpha == 255) {
        art_pdf_composite_pixel_alpha_8(dst, src, n_chan,
            blend_mode, pblend_procs, p14dev);
        if (dst_alpha_g != NULL) {
            tmp = (255 - *dst_alpha_g) * (255 - src[n_chan]) + 0x80;
            *dst_alpha_g = 255 - ((tmp + (tmp >> 8)) >> 8);
        }
    } else {
        src_alpha = src[n_chan];
        if (src_alpha == 0)
            return;
        memcpy(src_tmp, src, n_chan + 3);
        tmp = src_alpha * alpha + 0x80;
        src_tmp[n_chan] = (tmp + (tmp >> 8)) >> 8;
        art_pdf_composite_pixel_alpha_8(dst, src_tmp, n_chan,
                                        blend_mode, pblend_procs,
                                        p14dev);
        if (dst_alpha_g != NULL) {
            tmp = (255 - *dst_alpha_g) * (255 - src_tmp[n_chan]) + 0x80;
            *dst_alpha_g = 255 - ((tmp + (tmp >> 8)) >> 8);
        }
    }
}

/* A very simple case.  Knockout isolated group going to a parent that is not
   a knockout.  Simply copy over everwhere where we have a non-zero alpha value */
void
art_pdf_knockoutisolated_group_8(byte *dst, const byte *src, int n_chan)
{
    byte src_alpha;

    src_alpha = src[n_chan];
    if (src_alpha == 0)
        return;

    memcpy (dst, src, n_chan + 1);
}

/* An odd case where we have an alpha from the AA device and we have a current
   source alpha.  This is done only in the case where we are doing AA and a
   stroke fill at the same time.
   We have to first do a blend with the AA alpha if there
   is something to blend with and then set the alpha to the source alpha.
   In such a case an isolated knockout group was created and in the
   copy_alpha code we end up here to handle the alpha from the AA code
   differently from the other alpha.  This ensures that the stroke and fill
   end up blended with each other on the inside of the stroke path
   but that the alpha from the AA does not end up getting blended with the
   backdrop (unless the source alpha is not opaque) while the outside of the
   stroke path ends up with the alpha for both the AA effect and source alpha */
void
art_pdf_knockoutisolated_group_aa_8(byte *dst, const byte *src, byte src_alpha,
                        byte aa_alpha, int n_chan, pdf14_device *p14dev)
{
    int dst_alpha = dst[n_chan];
    byte temp_src[ART_MAX_CHAN + 1];
    int i;

    /* Note: src[n_chan] is a blend of the aa_alpha and src_alpha */
    if (src[n_chan] == 0)
        return;

    /* Check what is at the destination.  If nothing there then just copy */
    if (dst_alpha == 0) {
        memcpy(dst, src, n_chan + 1);
        return;
    }

    /* Now the more complex case as something is there. First blend with the AA
       alpha and then set our alpha to src_alpha so it will end up blending properly
       with the backdrop if we had an global alpha for this fill/stroke */
    for (i = 0; i < n_chan; i++)
        temp_src[i] = src[i];
    temp_src[n_chan] = aa_alpha;
    art_pdf_composite_pixel_alpha_8(dst, temp_src, n_chan, BLEND_MODE_Normal,
        NULL, p14dev);
    dst[n_chan] = src_alpha;
}

void
art_pdf_composite_knockout_8(byte *dst,
                                    const byte *src,
                                    int n_chan,
                                    gs_blend_mode_t blend_mode,
                                    const pdf14_nonseparable_blending_procs_t * pblend_procs,
                                    pdf14_device *p14dev)
{
    byte src_shape = src[n_chan];
    int i, tmp;

    if (blend_mode == BLEND_MODE_Normal) {
        /* Do simple compositing of source over backdrop */
        if (src_shape == 0)
            return;
        else if (src_shape == 255) {
            memcpy (dst, src, n_chan + 3);
            return;
        } else {
            /* Use src_shape to interpolate (in premultiplied alpha space)
               between dst and (src, opacity). */
            int dst_alpha = dst[n_chan];
            byte result_alpha;

            tmp = (255 - dst_alpha) * src_shape + 0x80;
            result_alpha = dst_alpha + ((tmp + (tmp >> 8)) >> 8);

            if (result_alpha != 0)
                for (i = 0; i < n_chan; i++) {
                    /* todo: optimize this - can strength-reduce so that
                       inner loop is a single interpolation */
                    tmp = dst[i] * dst_alpha * (255 - src_shape) +
                        ((int)src[i]) * 255 * src_shape + (result_alpha << 7);
                    dst[i] = tmp / (result_alpha * 255);
                }
            dst[n_chan] = result_alpha;
        }
    } else {
        /* Do compositing with blending */
        byte blend[ART_MAX_CHAN];
        byte a_b, a_s;
        unsigned int a_r;
        int src_scale;
        int c_b, c_s;

        a_s = src[n_chan];
        a_b = dst[n_chan];

        /* Result alpha is Union of backdrop and source alpha */
        tmp = (0xff - a_b) * (0xff - a_s) + 0x80;
        a_r = 0xff - (((tmp >> 8) + tmp) >> 8);
        /* todo: verify that a_r is nonzero in all cases */

        /* Compute a_s / a_r in 16.16 format */
        src_scale = ((a_s << 16) + (a_r >> 1)) / a_r;

        art_blend_pixel_8(blend, dst, src, n_chan, blend_mode, pblend_procs, p14dev);
        for (i = 0; i < n_chan; i++) {
            int c_bl;		/* Result of blend function */
            int c_mix;		/* Blend result mixed with source color */

            c_s = src[i];
            c_b = dst[i];
            c_bl = blend[i];
            tmp = a_b * (c_bl - ((int)c_s)) + 0x80;
            c_mix = c_s + (((tmp >> 8) + tmp) >> 8);
            tmp = (c_b << 16) + src_scale * (c_mix - c_b) + 0x8000;
            dst[i] = tmp >> 16;
        }
        dst[n_chan] = a_r;
    }
}

#if RAW_DUMP
/* Debug dump of buffer data from pdf14 device.  Saved in
   planar form with global indexing and tag information in
   file name */
void
dump_raw_buffer(int num_rows, int width, int n_chan,
                int plane_stride, int rowstride,
                char filename[],byte *Buffer)
{
    char full_file_name[50];
    FILE *fid;
    int z,y;
    byte *buff_ptr;
    int max_bands;

   /* clist_band_count is incremented at every pdf14putimage */
   /* Useful for catching this thing and only dumping */
   /* during a particular band if we have a large file */
   /* if (clist_band_count != 65) return; */
    buff_ptr = Buffer;
#if RAW_DUMP_AS_PAM
    /* FIXME: GRAY + ALPHA + SHAPE + TAGS will be interpreted as RGB + ALPHA */
    if ((n_chan == 2) || (n_chan == 3)) {
        int x;
        gs_sprintf(full_file_name,"%02d)%s.pam",global_index,filename);
        fid = gp_fopen(full_file_name,"wb");
        fprintf(fid, "P7\nWIDTH %d\nHEIGHT %d\nDEPTH 4\nMAXVAL 255\nTUPLTYPE GRAYSCALE_ALPHA\nENDHDR\n",
                width, num_rows);
        for(y=0; y<num_rows; y++)
            for(x=0; x<width; x++)
                for(z=0; z<2; z++)
                    fputc(Buffer[z*plane_stride + y*rowstride + x], fid);
        fclose(fid);
        if (n_chan == 3) {
            gs_sprintf(full_file_name,"%02d)%s_shape.pam",global_index,filename);
            fid = gp_fopen(full_file_name,"wb");
            fprintf(fid, "P7\nWIDTH %d\nHEIGHT %d\nDEPTH 1\nMAXVAL 255\nTUPLTYPE GRAYSCALE\nENDHDR\n",
                    width, num_rows);
            for(y=0; y<num_rows; y++)
                for(x=0; x<width; x++)
                    fputc(Buffer[2*plane_stride + y*rowstride + x], fid);
            fclose(fid);
        }
    }
    if ((n_chan == 4) || (n_chan == 5) || (n_chan == 6)) {
        int x;
        gs_sprintf(full_file_name,"%02d)%s.pam",global_index,filename);
        fid = gp_fopen(full_file_name,"wb");
        fprintf(fid, "P7\nWIDTH %d\nHEIGHT %d\nDEPTH 4\nMAXVAL 255\nTUPLTYPE RGB_ALPHA\nENDHDR\n",
                width, num_rows);
        for(y=0; y<num_rows; y++)
            for(x=0; x<width; x++)
                for(z=0; z<4; z++)
                    fputc(Buffer[z*plane_stride + y*rowstride + x], fid);
        fclose(fid);
        if (n_chan > 4) {
            gs_sprintf(full_file_name,"%02d)%s_shape.pam",global_index,filename);
            fid = gp_fopen(full_file_name,"wb");
            fprintf(fid, "P7\nWIDTH %d\nHEIGHT %d\nDEPTH 1\nMAXVAL 255\nTUPLTYPE GRAYSCALE\nENDHDR\n",
                    width, num_rows);
            for(y=0; y<num_rows; y++)
                for(x=0; x<width; x++)
                    fputc(Buffer[4*plane_stride + y*rowstride + x], fid);
            fclose(fid);
        }
        if (n_chan == 6) {
            gs_sprintf(full_file_name,"%02d)%s_tags.pam",global_index,filename);
            fid = gp_fopen(full_file_name,"wb");
            fprintf(fid, "P7\nWIDTH %d\nHEIGHT %d\nDEPTH 1\nMAXVAL 255\nTUPLTYPE GRAYSCALE\nENDHDR\n",
                    width, num_rows);
            for(y=0; y<num_rows; y++)
                for(x=0; x<width; x++)
                    fputc(Buffer[5*plane_stride + y*rowstride + x], fid);
            fclose(fid);
        }
        return;
    }
#endif
    max_bands = ( n_chan < 57 ? n_chan : 56);   /* Photoshop handles at most 56 bands */
    gs_sprintf(full_file_name,"%02d)%s_%dx%dx%d.raw",global_index,filename,width,num_rows,max_bands);
    fid = gp_fopen(full_file_name,"wb");

    for (z = 0; z < max_bands; ++z) {
        /* grab pointer to the next plane */
        buff_ptr = &(Buffer[z*plane_stride]);
        for ( y = 0; y < num_rows; y++ ) {
            /* write out each row */
            fwrite(buff_ptr,sizeof(unsigned char),width,fid);
            buff_ptr += rowstride;
        }
    }
    fclose(fid);
}
#endif
