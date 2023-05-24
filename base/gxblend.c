/* Copyright (C) 2001-2023 Artifex Software, Inc.
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
#include "gsrect.h"		/* for rect_merge */
#include "math_.h"		/* for ceil, floor */
#ifdef WITH_CAL
#include "cal.h"
#endif

typedef int art_s32;

#if RAW_DUMP
extern unsigned int global_index;
extern unsigned int clist_band_count;
#endif

#undef TRACK_COMPOSE_GROUPS
#ifdef TRACK_COMPOSE_GROUPS
int compose_groups[1<<17];

static int track_compose_groups = 0;

static void dump_track_compose_groups(void);
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
static void
do_smask_luminosity_mapping(int num_rows, int num_cols, int n_chan, int row_stride,
                            int plane_stride, const byte *gs_restrict src,
                            byte *gs_restrict dst, bool isadditive,
                            gs_transparency_mask_subtype_t SMask_SubType
#if RAW_DUMP
                            , const gs_memory_t *mem
#endif
                            )
{
    int x,y;
    int mask_alpha_offset,mask_C_offset,mask_M_offset,mask_Y_offset,mask_K_offset;
    int mask_R_offset,mask_G_offset,mask_B_offset;
    byte *dstptr;

#if RAW_DUMP
    dump_raw_buffer(mem, num_rows, row_stride, n_chan,
                    plane_stride, row_stride,
                   "Raw_Mask", src, 0);

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

static void
do_smask_luminosity_mapping_16(int num_rows, int num_cols, int n_chan, int row_stride,
                               int plane_stride, const uint16_t *gs_restrict src,
                               uint16_t *gs_restrict dst, bool isadditive,
                               gs_transparency_mask_subtype_t SMask_SubType
#if RAW_DUMP
                               , const gs_memory_t *mem
#endif
                               )
{
    int x,y;
    int mask_alpha_offset,mask_C_offset,mask_M_offset,mask_Y_offset,mask_K_offset;
    int mask_R_offset,mask_G_offset,mask_B_offset;
    uint16_t *dstptr;

#if RAW_DUMP
    dump_raw_buffer_be(mem, num_rows, row_stride, n_chan,
                       plane_stride, row_stride,
                       "Raw_Mask", (const byte *)src, 0);

    global_index++;
#endif
    dstptr = dst;
    /* If subtype is Luminosity then we should just grab the Y channel */
    if ( SMask_SubType == TRANSPARENCY_MASK_Luminosity ){
        memcpy(dstptr, &(src[plane_stride]), plane_stride*2);
        return;
    }
    /* If we are alpha type, then just grab that */
    /* We need to optimize this so that we are only drawing alpha in the rect fills */
    if ( SMask_SubType == TRANSPARENCY_MASK_Alpha ){
        mask_alpha_offset = (n_chan - 1) * plane_stride;
        memcpy(dstptr, &(src[mask_alpha_offset]), plane_stride*2);
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
                        temp = temp * (1.0 / 65535.0 );  /* May need to be optimized */
                        dstptr[x] = float_color_to_color16(temp);
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
                    temp = ( 0.30 * ( 0xffff - src[x + mask_C_offset]) +
                        0.59 * ( 0xffff - src[x + mask_M_offset]) +
                        0.11 * ( 0xffff - src[x + mask_Y_offset]) ) *
                        ( 0xffff - src[x + mask_K_offset]);
                    temp = temp * (1.0 / (65535.0*65535.0) );  /* May need to be optimized */
                    dstptr[x] = float_color_to_color16(temp);
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

void
smask_luminosity_mapping(int num_rows, int num_cols, int n_chan, int row_stride,
                         int plane_stride, const byte *gs_restrict src,
                         byte *gs_restrict dst, bool isadditive,
                         gs_transparency_mask_subtype_t SMask_SubType, bool deep
#if RAW_DUMP
                         , const gs_memory_t *mem
#endif
                         )
{
    if (deep)
        do_smask_luminosity_mapping_16(num_rows, num_cols, n_chan, row_stride>>1,
                                       plane_stride>>1, (const uint16_t *)(const void *)src,
                                       (uint16_t *)(void *)dst, isadditive, SMask_SubType
#if RAW_DUMP
                                       , mem
#endif
                                       );
    else
        do_smask_luminosity_mapping(num_rows, num_cols, n_chan, row_stride,
                                    plane_stride, src, dst, isadditive, SMask_SubType
#if RAW_DUMP
                                    , mem
#endif
                                    );
}

/* soft mask gray buffer should be blended with its transparency planar data
   during the pop for a luminosity case if we have a soft mask within a soft
   mask.  This situation is detected in the code so that we only do this
   blending in those rare situations */
void
smask_blend(byte *gs_restrict src, int width, int height, int rowstride,
            int planestride, bool deep)
{
    int x, y;
    int position;

    if (deep) {
        uint16_t comp, a;
        const uint16_t bg = 0;
        uint16_t *src16 = (uint16_t *)(void *)src;
        rowstride >>= 1;
        planestride >>= 1;
        for (y = 0; y < height; y++) {
            position = y * rowstride;
            for (x = 0; x < width; x++) {
                a = src16[position + planestride];
                if (a == 0) {
                    src16[position] = 0;
                } else if (a != 0xffff) {
                    a ^= 0xffff;
                    a += a>>15;
                    comp  = src16[position];
                    comp += (((bg - comp) * a) + 0x8000)>>16;
                    /* Errors in bit 16 and above are ignored */
                    src16[position] = comp;
                }
                position+=1;
            }
        }
    } else {
        byte comp, a;
        int tmp;
        const byte bg = 0;
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
}

void smask_copy(int num_rows, int num_cols, int row_stride,
                byte *gs_restrict src, const byte *gs_restrict dst)
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

int
smask_icc(gx_device *dev, int num_rows, int num_cols, int n_chan,
               int row_stride, int plane_stride, byte *gs_restrict src, const byte *gs_restrict dst,
               gsicc_link_t *icclink, bool deep)
{
    gsicc_bufferdesc_t input_buff_desc;
    gsicc_bufferdesc_t output_buff_desc;

#if RAW_DUMP
    dump_raw_buffer(dev->memory, num_rows, row_stride>>deep, n_chan,
                    plane_stride, row_stride,
                    "Raw_Mask_ICC", src, deep);
    global_index++;
#endif
/* Set up the buffer descriptors. Note that pdf14 always has
   the alpha channels at the back end (last planes).
   We will just handle that here and let the CMM know
   nothing about it */

    gsicc_init_buffer(&input_buff_desc, n_chan-1, 1<<deep,
                  false, false, true, plane_stride, row_stride,
                  num_rows, num_cols);
    gsicc_init_buffer(&output_buff_desc, 1, 1<<deep,
                  false, false, true, plane_stride,
                  row_stride, num_rows, num_cols);
    /* Transform the data */
    return (icclink->procs.map_buffer)(dev, icclink, &input_buff_desc, &output_buff_desc,
                                (void*) src, (void*) dst);
}

void
art_blend_luminosity_rgb_8(int n_chan, byte *gs_restrict dst, const byte *gs_restrict backdrop,
                           const byte *gs_restrict src)
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
art_blend_luminosity_rgb_16(int n_chan, uint16_t *gs_restrict dst, const uint16_t *gs_restrict backdrop,
                            const uint16_t *gs_restrict src)
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
    if ((r | g | b) & 0x10000) {
        int y;
        int64_t scale;

        /* Resort to 64 bit to avoid calculations with scale overflowing */
        y = (rs * 77 + gs * 151 + bs * 28 + 0x80) >> 8;
        if (delta_y > 0) {
            int max;

            max = r > g ? r : g;
            max = b > max ? b : max;
            scale = ((65535 - (int64_t)y) << 16) / (max - y);
        } else {
            int min;

            min = r < g ? r : g;
            min = b < min ? b : min;
            scale = (((int64_t)y) << 16) / (y - min);
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
art_blend_luminosity_custom_8(int n_chan, byte *gs_restrict dst, const byte *gs_restrict backdrop,
                              const byte *gs_restrict src)
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

void
art_blend_luminosity_custom_16(int n_chan, uint16_t *gs_restrict dst, const uint16_t *gs_restrict backdrop,
                               const uint16_t *gs_restrict src)
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

    if (test & 0x10000) {
        int y;
        int64_t scale;

        /* Resort to 64bit to avoid calculations with scale overflowing */
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
            scale = ((65535 - (int64_t)y) << 16) / (max - y);
        } else {
            int min;

            min = r[0];
            for (i = 1; i < n_chan; i++)
                min = min(min, r[i]);
            scale = (((int64_t)y) << 16) / (y - min);
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
art_blend_luminosity_cmyk_8(int n_chan, byte *gs_restrict dst, const byte *gs_restrict backdrop,
                           const byte *gs_restrict src)
{
    int i;

    /* Treat CMY the same as RGB. */
    art_blend_luminosity_rgb_8(3, dst, backdrop, src);
    for (i = 3; i < n_chan; i++)
        dst[i] = src[i];
}

void
art_blend_luminosity_cmyk_16(int n_chan, uint16_t *gs_restrict dst, const uint16_t *gs_restrict backdrop,
                             const uint16_t *gs_restrict src)
{
    int i;

    /* Treat CMY the same as RGB. */
    art_blend_luminosity_rgb_16(3, dst, backdrop, src);
    for (i = 3; i < n_chan; i++)
        dst[i] = src[i];
}


/*

Some notes on saturation blendmode:

To test the results of deep color rendering, we ran a psdcmyk vs
psdcmyk16 comparison. This showed differences on page 17 of the
Altona_technical_v20_x4.pdf file in one patch. Simplifying the
file shows that the saturation blend mode is showing significant
differences between 8 and 16 bit rendering.

Saturation blend mode is defined to not make any changes if we
are writing over a pure grey color (as there is no 'hue' for
it to saturate). You'd expect that the blending function would be
continuous (i.e. that a small peturbation of the background color
should only produce a small peturbation in the output), but this
is NOT the case around pure greys.

The example in the tested file, shows that psdcmyk is called with
7a, 7a, 7a, which therefore leaves the background unchanged. For
psdcmyk16, it's called with 7a01 7a03 7a01, which therefore does
NOT leave the background unchanged. Testing by changing the 8 bit
inputs to 7b 7a 7b (a small peturbation), gives output of 99 64 99
(a large change).

So, actually, the results given seem reasonable in that case.

As a further indication that saturation blend mode results are
'unstable' for 'near greys', the same patch in acrobat renders
slightly blue, where the 16bit rendering in gs renders slightly
pink. This can be explained by a small peturbation in the input
color, which itself can be explained by small differences in the
color profiles used.

*/

void
art_blend_saturation_rgb_8(int n_chan, byte *gs_restrict dst, const byte *gs_restrict backdrop,
                           const byte *gs_restrict src)
{
    int32_t rb = backdrop[0], gb = backdrop[1], bb = backdrop[2];
    int rs = src[0], gs = src[1], bs = src[2];
    int mins, maxs, minb, maxb;
    int satCs, lumCb, lumC, d;
    int scale;

    if (rb == gb && gb == bb) {
        /* backdrop has zero saturation, no change. */
        dst[0] = gb;
        dst[1] = gb;
        dst[2] = gb;
        return;
    }

    /* Lum(Cb) */
    lumCb = (rb * 77 + gb * 151 + bb * 28 + 0x80) >> 8;

    mins = rs < gs ? rs : gs;
    maxs = rs < gs ? gs : rs;
    mins = mins < bs ? mins : bs;
    maxs = maxs < bs ? bs : maxs;

    /* Sat(Cs) = maxs - mins */
    satCs = maxs - mins;

    /* C = {rb, bb, gb} = SetSat(Cb, Sat(Cs)) */
    minb = rb < gb ? rb : gb;
    maxb = rb < gb ? gb : rb;
    minb = minb < bb ? minb : bb;
    maxb = maxb < bb ? bb : maxb;
    scale = (satCs<<8) / (maxb - minb);
    rb = ((rb - minb) * scale + 0x80)>>8;
    gb = ((gb - minb) * scale + 0x80)>>8;
    bb = ((bb - minb) * scale + 0x80)>>8;
    /* Leaves us with Cmin = 0, Cmax = s, and Cmid all as per the spec. */

    /* SetLum(SetSat(Cb, Sat(Cs)), Lum(Cb)) */
    /* lumC = Lum(C) */
    lumC = (rb * 77 + gb * 151 + bb * 28 + 0x80) >> 8;
    d = lumCb - lumC;
    /* ClipColor(C) */
    /* We know that Cmin = 0, Cmax = satCs. Therefore, given we are about
     * to add 'd' back on to reset the luminance, we'll have overflow
     * problems if d < 0 or d+satCs > 255. We further know that as
     * 0 <= satCs <= 255, so only one of those can be true a time. */
    if (d < 0) {
        scale = (lumCb<<8) / lumC;
        goto correct_overflow;
    } else if (d + satCs > 255) {
        scale = ((255 - lumCb)<<8) / (satCs - lumC);
correct_overflow:
        rb = lumCb + (((rb - lumC) * scale + 0x80)>>8);
        gb = lumCb + (((gb - lumC) * scale + 0x80)>>8);
        bb = lumCb + (((bb - lumC) * scale + 0x80)>>8);
    } else {
        /* C += d */
        rb += d;
        gb += d;
        bb += d;
    }

    dst[0] = rb;
    dst[1] = gb;
    dst[2] = bb;
}

void
art_blend_saturation_rgb_16(int n_chan, uint16_t *gs_restrict dst, const uint16_t *gs_restrict backdrop,
                            const uint16_t *gs_restrict src)
{
    int rb = backdrop[0], gb = backdrop[1], bb = backdrop[2];
    int rs = src[0], gs = src[1], bs = src[2];
    int mins, maxs, minb, maxb;
    int satCs, lumCb, lumC, d;
    uint64_t scale;

    if (rb == gb && gb == bb) {
        /* backdrop has zero saturation, no change. */
        dst[0] = gb;
        dst[1] = gb;
        dst[2] = gb;
        return;
    }

    /* Lum(Cb) */
    lumCb = (rb * 77 + gb * 151 + bb * 28 + 0x80) >> 8;

    mins = rs < gs ? rs : gs;
    maxs = rs < gs ? gs : rs;
    mins = mins < bs ? mins : bs;
    maxs = maxs < bs ? bs : maxs;

    /* Sat(Cs) = maxs - mins */
    satCs = maxs - mins;

    /* SetSat(Cb, Sat(Cs)) */
    minb = rb < gb ? rb : gb;
    maxb = rb < gb ? gb : rb;
    minb = minb < bb ? minb : bb;
    maxb = maxb < bb ? bb : maxb;
    /* 0 <= maxb - minb <= 65535 */
    /* 0 <= satCs <= 65535 */
    scale = ((unsigned int)(satCs<<16)) / (maxb - minb);
    rb = ((rb - minb) * scale + 0x8000)>>16;
    gb = ((gb - minb) * scale + 0x8000)>>16;
    bb = ((bb - minb) * scale + 0x8000)>>16;
    /* Leaves us with Cmin = 0, Cmax = s, and Cmid all as per the spec. */

    /* SetLum(SetSat(Cb, Sat(Cs)), Lum(Cb)) */
    /* lumC = Lum(C) */
    lumC = (rb * 77 + gb * 151 + bb * 28 + 0x80) >> 8;
    d = lumCb - lumC;
    /* ClipColor(C) */
    /* We know that Cmin = 0, Cmax = satCs. Therefore, given we are about
     * to add 'd' back on to reset the luminance, we'll have overflow
     * problems if d < 0 or d+satCs > 65535. We further know that as
     * 0 <= satCs <= 65535, so only one of those can be true a time. */
    if (d < 0) {
        scale = ((unsigned int)(lumCb<<16)) / (unsigned int)lumC;
        goto correct_overflow;
    } else if (d + satCs > 65535) {
        scale = ((unsigned int)((65535 - lumCb)<<16)) / (unsigned int)(satCs - lumC);
correct_overflow:
        rb = lumCb + (((rb - lumC) * scale + 0x8000)>>16);
        gb = lumCb + (((gb - lumC) * scale + 0x8000)>>16);
        bb = lumCb + (((bb - lumC) * scale + 0x8000)>>16);
    } else {
        /* C += d */
        rb += d;
        gb += d;
        bb += d;
    }

    dst[0] = rb;
    dst[1] = gb;
    dst[2] = bb;
}

void
art_blend_saturation_custom_8(int n_chan, byte *gs_restrict dst, const byte *gs_restrict backdrop,
                              const byte *gs_restrict src)
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

void
art_blend_saturation_custom_16(int n_chan, uint16_t *gs_restrict dst, const uint16_t *gs_restrict backdrop,
                               const uint16_t *gs_restrict src)
{
    int minb, maxb;
    int mins, maxs;
    int y;
    int scale;
    int r[ART_MAX_CHAN];
    int test = 0;
    int temp, i;

    /* FIXME: Test this */

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

    if (test & 0x10000) {
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

        if (max > 65535)
            scalemax = ((65535 - y) << 16) / (max - y);
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
art_blend_saturation_cmyk_8(int n_chan, byte *gs_restrict dst, const byte *gs_restrict backdrop,
                           const byte *gs_restrict src)
{
    int i;

    /* Treat CMY the same as RGB */
    art_blend_saturation_rgb_8(3, dst, backdrop, src);
    for (i = 3; i < n_chan; i++)
        dst[i] = backdrop[i];
}

void
art_blend_saturation_cmyk_16(int n_chan, uint16_t *gs_restrict dst, const uint16_t *gs_restrict backdrop,
                             const uint16_t *gs_restrict src)
{
    int i;

    /* Treat CMY the same as RGB */
    art_blend_saturation_rgb_16(3, dst, backdrop, src);
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

static forceinline void
art_blend_pixel_8_inline(byte *gs_restrict dst, const byte *gs_restrict backdrop,
                  const byte *gs_restrict src, int n_chan, gs_blend_mode_t blend_mode,
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
                byte tmp[ART_MAX_CHAN];

                pblend_procs->blend_luminosity(n_chan, tmp, src, backdrop);
                pblend_procs->blend_saturation(n_chan, dst, tmp, backdrop);
            }
            break;
            /* This mode requires information about the color space as
             * well as the overprint mode.  See Section 7.6.3 of
             * PDF specification */
        case BLEND_MODE_CompatibleOverprint:
            {
                gx_color_index drawn_comps = p14dev->op_state == PDF14_OP_STATE_FILL ?
                                             p14dev->drawn_comps_fill : p14dev->drawn_comps_stroke;
                bool opm = p14dev->op_state == PDF14_OP_STATE_FILL ?
                    p14dev->effective_overprint_mode : p14dev->stroke_effective_op_mode;
                gx_color_index comps;
                /* If overprint mode is true and the current color space and
                 * the group color space are CMYK (or CMYK and spots), then
                 * B(cb, cs) = cs if cs is nonzero otherwise it is cb for CMYK.
                 * Spot colors are always set to cb.  The nice thing about the PDF14
                 * compositor is that it always has CMYK + spots with spots after
                 * the CMYK colorants (see gx_put_blended_image_cmykspot).
                 * that way we don't have to worry about where the process colors
                 * are.

                 * Note:  The spec claims the following:

                 If the overprint mode is 1 (nonzero overprint mode) and the
                 current color space and group color space are both DeviceCMYK,
                 then only process color components with nonzero values replace
                 the corresponding component values of the backdrop. All other
                 component values leave the existing backdrop value unchanged.
                 That is, the value of the blend function B(Cb,Cs) is the source
                 component cs for any process (DeviceCMYK) color component whose
                 (subtractive) color value is nonzero; otherwise it is the
                 backdrop component cb. For spot color components, the value is
                 always cb.

                 The equation for compositing is

                    ar*Cr = (1-as)*Cb + as*[(1-ab)*Cs+ab*B(Cb,Cs)]

                 Now if I simply set B(cb,cs) to cb for the case when the
                 DevieCMYK value (with opm true) is zero I get

                 ar*Cr = (1-as)*Cb + as*[(1-ab)*Cs+ab*Cb]

                 But what I am seeing with AR is
                    ar*Cr = (1-as)*Cb + as*[(1-ab)*Cb+ab*Cb] = (1-as)*Cb + as*Cb = Cb
                 which is what I think we want.

                 The description in the spec is confusing as it says
                "then only process color components with nonzero values replace
                 the corresponding component values of the backdrop. All other
                 component values leave the existing backdrop value unchanged"

                 which makes sense for overprinting,

                 vs.

                 "That is, the value of the blend function B(Cb,Cs) is the source
                 component cs for any process (DeviceCMYK) color component whose
                 (subtractive) color value is nonzero; otherwise it is the
                 backdrop component cb."

                 Which is NOT the same thing as leaving the backdrop unchanged
                 with the compositing equation
                 ar*Cr = (1-as)*Cb + as*[(1-ab)*Cs+ab*B(Cb,Cs)]

                 For this to work, we need to carry out the operation during
                 the mixing of the source with the blend result.  Essentially
                 replacing that mixing with the color we have here.
                 */
                if (opm && p14dev->color_info.num_components > 3
                    && !(p14dev->ctx->additive)) {
                    for (i = 0, comps = drawn_comps; i < 4; i++, comps >>= 1) {
                        if ((comps & 0x1) != 0) {
                            dst[i] = src[i];
                        } else {
                            dst[i] = backdrop[i];
                        }
                    }
                    for (i = 4; i < n_chan; i++) {
                        dst[i] = backdrop[i];
                    }
                } else {
                    /* Otherwise we have B(cb, cs)= cs if cs is specified in
                     * the current color space all other color should get cb.
                     * Essentially the standard overprint case. */
                    for (i = 0, comps = drawn_comps; i < n_chan; ++i, comps >>= 1) {
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
            dlprintf1("art_blend_pixel_8: blend mode %d not implemented\n",
                      blend_mode);
            memcpy(dst, src, n_chan);
            break;
    }
}

void
art_blend_pixel_8(byte *gs_restrict dst, const byte *gs_restrict backdrop,
                  const byte *gs_restrict src, int n_chan, gs_blend_mode_t blend_mode,
                  const pdf14_nonseparable_blending_procs_t * pblend_procs,
                  pdf14_device *p14dev)
{
    art_blend_pixel_8_inline(dst, backdrop, src, n_chan, blend_mode,
                             pblend_procs, p14dev);
}

static forceinline void
art_blend_pixel_16_inline(uint16_t *gs_restrict dst, const uint16_t *gs_restrict backdrop,
                  const uint16_t *gs_restrict src, int n_chan, gs_blend_mode_t blend_mode,
                  const pdf14_nonseparable_blending_procs_t * pblend_procs,
                  pdf14_device *p14dev)
{
    int i;
    int b, s;
    bits32 t;

    switch (blend_mode) {
        case BLEND_MODE_Normal:
        case BLEND_MODE_Compatible:	/* todo */
            memcpy(dst, src, n_chan*2);
            break;
        case BLEND_MODE_Multiply:
            for (i = 0; i < n_chan; i++) {
                t = backdrop[i];
                t += t >> 15;
                t = t * src[i] + 0x8000;
                dst[i] = t >> 16;
            }
            break;
        case BLEND_MODE_Screen:
            for (i = 0; i < n_chan; i++) {
                t = backdrop[i];
                t += t >> 15;
                t = (0x10000-t) * (0xffff - src[i]) + 0x8000;
                dst[i] = 0xffff - (t >> 16);
            }
            break;
        case BLEND_MODE_Overlay:
            for (i = 0; i < n_chan; i++) {
                b = backdrop[i];
                b += b >> 15;
                s = src[i];
                if (b < 0x8000)
                    t = (2 * b * s);
                else
                    t = 0xffff0000 -
                        2 * (0x10000 - b) * (0xffff - s);
                t = (t+0x8000)>>16;
                dst[i] = t;
            }
            break;
        case BLEND_MODE_SoftLight:
            for (i = 0; i < n_chan; i++) {
                b = backdrop[i];
                s = src[i];
                if (s < 0x8000) {
                    unsigned int b2 = ((unsigned int)(b * (b + (b>>15))))>>16;
                    b2 = b - b2;
                    b2 += b2>>15;
                    t = ((0xffff - (s << 1)) * b2) + 0x8000;
                    dst[i] = b - (t >> 16);
                } else {
#define art_blend_soft_light_16(B) (art_blend_soft_light_8[(B)>>8]*0x101)
                    t = ((s << 1) - 0xffff) * art_blend_soft_light_16(b) + 0x8000;
                    dst[i] = b + (t >> 16);
                }
            }
            break;
        case BLEND_MODE_HardLight:
            for (i = 0; i < n_chan; i++) {
                b = backdrop[i];
                b += b>>15;
                s = src[i];
                if (s < 0x8000)
                    t = 2 * b * s;
                else
                    t = 0xffff0000 - 2 * (0x10000 - b) * (0xffff - s);
                t += 0x8000;
                dst[i] = t >> 16;
            }
            break;
        case BLEND_MODE_ColorDodge:
            for (i = 0; i < n_chan; i++) {
                b = backdrop[i];
                s = 0xffff - src[i];
                if (b == 0)
                    dst[i] = 0;
                else if (b >= s)
                    dst[i] = 0xffff;
                else
                    dst[i] = ((unsigned int)(0xffff * b + (s>>1))) / s;
            }
            break;
        case BLEND_MODE_ColorBurn:
            for (i = 0; i < n_chan; i++) {
                b = 0xffff - backdrop[i];
                s = src[i];
                if (b == 0)
                    dst[i] = 0xffff;
                else if (b >= s)
                    dst[i] = 0;
                else
                    dst[i] = 0xffff - ((unsigned int)(0xffff * b + (s>>1))) / s;
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
                b += b>>15;
                s = src[i];
                t = (0x10000 - b) * s + b * (0xffff - s) + 0x8000;
                dst[i] = t >> 16;
            }
            break;
        case BLEND_MODE_Luminosity:
            pblend_procs->blend_luminosity16(n_chan, dst, backdrop, src);
            break;
        case BLEND_MODE_Color:
            pblend_procs->blend_luminosity16(n_chan, dst, src, backdrop);
            break;
        case BLEND_MODE_Saturation:
            pblend_procs->blend_saturation16(n_chan, dst, backdrop, src);
            break;
        case BLEND_MODE_Hue:
            {
                uint16_t tmp[ART_MAX_CHAN];

                pblend_procs->blend_luminosity16(n_chan, tmp, src, backdrop);
                pblend_procs->blend_saturation16(n_chan, dst, tmp, backdrop);
            }
            break;
            /* This mode requires information about the color space as
             * well as the overprint mode.  See Section 7.6.3 of
             * PDF specification */
        case BLEND_MODE_CompatibleOverprint:
            {
                gx_color_index drawn_comps = p14dev->op_state == PDF14_OP_STATE_FILL ?
                                             p14dev->drawn_comps_fill : p14dev->drawn_comps_stroke;
                bool opm = p14dev->op_state == PDF14_OP_STATE_FILL ?
                    p14dev->effective_overprint_mode : p14dev->stroke_effective_op_mode;
                gx_color_index comps;
                /* If overprint mode is true and the current color space and
                 * the group color space are CMYK (or CMYK and spots), then
                 * B(cb, cs) = cs if cs is nonzero otherwise it is cb for CMYK.
                 * Spot colors are always set to cb.  The nice thing about the PDF14
                 * compositor is that it always has CMYK + spots with spots after
                 * the CMYK colorants (see gx_put_blended_image_cmykspot).
                 * that way we don't have to worry about where the process colors
                 * are. */
                if (opm && p14dev->color_info.num_components > 3
                    && !(p14dev->ctx->additive)) {
                    for (i = 0, comps = drawn_comps; i < 4; i++, comps >>= 1) {
                        if ((comps & 0x1) != 0) {
                            dst[i] = src[i];
                        } else {
                            dst[i] = backdrop[i];
                        }
                    }
                    for (i = 4; i < n_chan; i++) {
                        dst[i] = backdrop[i];
                    }
                } else {
                    /* Otherwise we have B(cb, cs)= cs if cs is specified in
                     * the current color space all other color should get cb.
                     * Essentially the standard overprint case. */
                    for (i = 0, comps = drawn_comps; i < n_chan; ++i, comps >>= 1) {
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
            dlprintf1("art_blend_pixel_16: blend mode %d not implemented\n",
                      blend_mode);
            memcpy(dst, src, n_chan*2);
            break;
    }
}

void
art_blend_pixel_16(uint16_t *gs_restrict dst, const uint16_t *gs_restrict backdrop,
                   const uint16_t *gs_restrict src, int n_chan, gs_blend_mode_t blend_mode,
                   const pdf14_nonseparable_blending_procs_t * pblend_procs,
                   pdf14_device *p14dev)
{
    art_blend_pixel_16_inline(dst, backdrop, src, n_chan, blend_mode,
                              pblend_procs, p14dev);
}

#ifdef UNUSED
byte
art_pdf_union_8(byte alpha1, byte alpha2)
{
    int tmp;

    tmp = (0xff - alpha1) * (0xff - alpha2) + 0x80;
    return 0xff - ((tmp + (tmp >> 8)) >> 8);
}
#endif

static byte*
art_pdf_knockout_composite_pixel_alpha_8(byte *gs_restrict backdrop, byte tos_shape,
                        byte *gs_restrict dst, byte *gs_restrict src, int n_chan,
                        gs_blend_mode_t blend_mode,
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
           return backdrop;
        return NULL;
    }

    /* In this case a_s is not zero */
    if (a_b == 0) {
        /* backdrop alpha is zero but not source alpha, just copy source pixels and
           avoid computation. */
        return src;
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
    return dst;
}

static forceinline uint16_t*
art_pdf_knockout_composite_pixel_alpha_16(uint16_t *gs_restrict backdrop, uint16_t tos_shape, uint16_t *gs_restrict dst,
                        uint16_t *gs_restrict src, int n_chan, gs_blend_mode_t blend_mode,
                        const pdf14_nonseparable_blending_procs_t * pblend_procs,
                        pdf14_device *p14dev)
{
    int a_b, a_s;
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
            return backdrop;
        return NULL;
    }

    /* In this case a_s is not zero */
    if (a_b == 0) {
        /* backdrop alpha is zero but not source alpha, just copy source pixels and
           avoid computation. */
        return src;
    }

    /* Result alpha is Union of backdrop and source alpha */
    a_b += a_b>>15;
    tmp = (0x10000 - a_b) * (0xffff - a_s) + 0x8000;
    a_r = 0xffff - (tmp >> 16);
    /* todo: verify that a_r is nonzero in all cases */

    /* Compute a_s / a_r in 16.16 format */
    src_scale = ((a_s << 16) + (a_r >> 1)) / a_r;

    src_scale >>= 1; /* Lose a bit to avoid overflow */
    if (blend_mode == BLEND_MODE_Normal) {
        /* Do simple compositing of source over backdrop */
        for (i = 0; i < n_chan; i++) {
            c_s = src[i];
            c_b = backdrop[i];
            tmp = src_scale * (c_s - c_b) + 0x4000;
            dst[i] = c_b + (tmp >> 15);
        }
    } else {
        /* Do compositing with blending */
        uint16_t blend[ART_MAX_CHAN];

        art_blend_pixel_16(blend, backdrop, src, n_chan, blend_mode, pblend_procs,
                           p14dev);
        a_b >>= 1; /* Lose a bit to avoid overflow */
        for (i = 0; i < n_chan; i++) {
            int c_bl;		/* Result of blend function */
            int c_mix;		/* Blend result mixed with source color */

            c_s = src[i];
            c_b = backdrop[i];
            c_bl = blend[i];
            tmp = a_b * (c_bl - c_s) + 0x4000;
            c_mix = c_s + (tmp >> 15);
            tmp = src_scale * (c_mix - c_b) + 0x4000;
            dst[i] = c_b + (tmp >> 15);
        }
    }
    dst[n_chan] = a_r;
    return dst;
}

void
art_pdf_composite_pixel_alpha_8(byte *gs_restrict dst, const byte *gs_restrict src, int n_chan,
        gs_blend_mode_t blend_mode, int first_spot,
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

    if (first_spot != 0) {
        /* Do compositing with blending */
        byte blend[ART_MAX_CHAN];

        art_blend_pixel_8(blend, dst, src, first_spot, blend_mode, pblend_procs, p14dev);
        for (i = 0; i < first_spot; i++) {
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

    dst += first_spot;
    src += first_spot;
    n_chan -= first_spot;
    if (n_chan == 0)
        return;

    /* Do simple compositing of source over backdrop */
    for (i = 0; i < n_chan; i++) {
        c_s = src[i];
        c_b = dst[i];
        tmp = (c_b << 16) + src_scale * (c_s - c_b) + 0x8000;
        dst[i] = tmp >> 16;
    }
}

void
art_pdf_composite_pixel_alpha_16(uint16_t *gs_restrict dst, const uint16_t *gs_restrict src, int n_chan,
        gs_blend_mode_t blend_mode, int first_spot,
        const pdf14_nonseparable_blending_procs_t * pblend_procs, pdf14_device *p14dev)
{
    int a_b, a_s;
    unsigned int a_r;
    unsigned int tmp;
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

        memcpy (dst, src, (n_chan + 1)*2);

        return;
    }

    /* Result alpha is Union of backdrop and source alpha */
    tmp = (0xffff - a_b) * (0xffff - a_s) + 0x8000;
    a_r = 0xffff - (((tmp >> 16) + tmp) >> 16);
    /* todo: verify that a_r is nonzero in all cases */

    /* Compute a_s / a_r in 16.16 format */
    src_scale = ((unsigned int)((a_s << 16) + (a_r >> 1))) / a_r;

    src_scale >>= 1; /* Lose a bit to avoid overflow */
    if (first_spot != 0) {
        /* Do compositing with blending */
        uint16_t blend[ART_MAX_CHAN];

        a_b >>= 1; /* Lose a bit to avoid overflow */
        art_blend_pixel_16(blend, dst, src, first_spot, blend_mode, pblend_procs, p14dev);
        for (i = 0; i < first_spot; i++) {
            int c_bl;		/* Result of blend function */
            int c_mix;		/* Blend result mixed with source color */

            c_s = src[i];
            c_b = dst[i];
            c_bl = blend[i];
            tmp = a_b * (c_bl - ((int)c_s)) + 0x4000;
            c_mix = c_s + (((tmp >> 16) + tmp) >> 15);
            tmp = src_scale * (c_mix - c_b) + 0x4000;
            dst[i] = c_b + (tmp >> 15);
        }
    }
    dst[n_chan] = a_r;

    dst += first_spot;
    src += first_spot;
    n_chan -= first_spot;
    if (n_chan == 0)
        return;

    /* Do simple compositing of source over backdrop */
    for (i = 0; i < n_chan; i++) {
        c_s = src[i];
        c_b = dst[i];
        tmp = src_scale * (c_s - c_b) + 0x4000;
        dst[i] = c_b + (tmp >> 15);
    }
}

static forceinline byte *
art_pdf_composite_pixel_alpha_8_inline(byte *gs_restrict dst, byte *gs_restrict src, int n_chan,
        gs_blend_mode_t blend_mode, int first_spot,
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
        return NULL; /* No change to destination at all! */
    }

    a_b = dst[n_chan];
    if (a_b == 0) {
        /* backdrop alpha is zero, just copy source pixels and avoid
           computation. */
        return src;
    }

    /* Result alpha is Union of backdrop and source alpha */
    tmp = (0xff - a_b) * (0xff - a_s) + 0x80;
    a_r = 0xff - (((tmp >> 8) + tmp) >> 8);
    /* todo: verify that a_r is nonzero in all cases */

    /* Compute a_s / a_r in 16.16 format */
    src_scale = ((a_s << 16) + (a_r >> 1)) / a_r;

    if (first_spot != 0) {
        /* Do compositing with blending */
        byte blend[ART_MAX_CHAN];

        art_blend_pixel_8_inline(blend, dst, src, first_spot, blend_mode, pblend_procs, p14dev);

        if (blend_mode == BLEND_MODE_CompatibleOverprint) {
            for (i = 0; i < first_spot; i++) {
                /* No mixing.  Blend[i] is backdrop or src */
                tmp = (dst[i] << 16) + src_scale * (blend[i] - dst[i]) + 0x8000;
                dst[i] = tmp >> 16;
            }
        } else {
            for (i = 0; i < first_spot; i++) {
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
    }
    dst[n_chan] = a_r;

    n_chan -= first_spot;
    if (n_chan == 0)
        return dst;
    dst += first_spot;
    src += first_spot;

    /* Do simple compositing of source over backdrop */
    for (i = 0; i < n_chan; i++) {
        c_s = src[i];
        c_b = dst[i];
        tmp = (c_b << 16) + src_scale * (c_s - c_b) + 0x8000;
        dst[i] = tmp >> 16;
    }
    return dst - first_spot;
}

static forceinline uint16_t *
art_pdf_composite_pixel_alpha_16_inline(uint16_t *gs_restrict dst, uint16_t *gs_restrict src, int n_chan,
        gs_blend_mode_t blend_mode, int first_spot,
        const pdf14_nonseparable_blending_procs_t * pblend_procs, pdf14_device *p14dev)
{
    int a_b, a_s;
    unsigned int a_r;
    int tmp;
    int src_scale;
    int c_b, c_s;
    int i;

    a_s = src[n_chan];
    if (a_s == 0) {
        /* source alpha is zero, avoid all computations and possible
           divide by zero errors. */
        return NULL; /* No change to destination at all! */
    }

    a_b = dst[n_chan];
    if (a_b == 0) {
        /* backdrop alpha is zero, just copy source pixels and avoid
           computation. */
        return src;
    }

    /* Result alpha is Union of backdrop and source alpha */
    a_b += a_b>>15; /* a_b in 0...0x10000 range */
    tmp = (0x10000 - a_b) * (0xffff - a_s) + 0x8000;
    a_r = 0xffff - (((unsigned int)tmp) >> 16); /* a_r in 0...0xffff range */
    /* todo: verify that a_r is nonzero in all cases */

    /* Compute a_s / a_r in 16.16 format */
    src_scale = ((unsigned int)((a_s << 16) + (a_r >> 1))) / a_r;

    src_scale >>= 1; /* Lose a bit to avoid overflow */
    if (first_spot != 0) {
        /* Do compositing with blending */
        uint16_t blend[ART_MAX_CHAN];

        art_blend_pixel_16_inline(blend, dst, src, first_spot, blend_mode, pblend_procs, p14dev);

        if (blend_mode == BLEND_MODE_CompatibleOverprint) {
            for (i = 0; i < first_spot; i++) {
                /* No mixing.  Blend[i] is backdrop or src */
                dst[i] += (src_scale * (blend[i] - dst[i]) + 0x4000) >> 15;
            }
        } else {
            a_b >>= 1; /* Lose a bit to avoid overflow */
            for (i = 0; i < first_spot; i++) {
                int c_bl;		/* Result of blend function */

                c_s = src[i];
                c_b = dst[i];
                c_bl = blend[i];

                c_s += (a_b * (c_bl - c_s) + 0x4000) >> 15;
                c_b += (src_scale * (c_s - c_b) + 0x4000) >> 15;
                dst[i] = c_b;
            }
        }
    }
    dst[n_chan] = a_r;

    n_chan -= first_spot;
    if (n_chan == 0)
        return dst;
    dst += first_spot;
    src += first_spot;

    /* Do simple compositing of source over backdrop */
    for (i = 0; i < n_chan; i++) {
        c_s = src[i];
        c_b = dst[i];
        c_b += (src_scale * (c_s - c_b) + 0x4000)>>15;
        dst[i] = c_b;
    }
    return dst - first_spot;
}

/**
 * art_pdf_composite_pixel_alpha_8_fast_mono: Tweaked version of art_pdf_composite_pixel_alpha_8_fast.
 * Same args, except n_chan, which is assumed to be 1:
 * @stride: stride between dst pixel values.
 * @p14dev: pdf14 device
 * Dst data is therefore in dst[i * stride] for 0 <= i <= 1.
 * Called with the guarantee that dst[stride] != 0, src[1] != 0, and that blend_mode != Normal
 */
static inline void
art_pdf_composite_pixel_alpha_8_fast_mono(byte *gs_restrict dst, const byte *gs_restrict src,
                                          gs_blend_mode_t blend_mode,
                                          const pdf14_nonseparable_blending_procs_t * pblend_procs,
                                          int stride, pdf14_device *p14dev)
{
    byte a_b, a_s;
    unsigned int a_r;
    int tmp;
    int src_scale;
    int c_b, c_s;
    byte blend[ART_MAX_CHAN];

    a_s = src[1];

    a_b = dst[stride];

    /* Result alpha is Union of backdrop and source alpha */
    tmp = (0xff - a_b) * (0xff - a_s) + 0x80;
    a_r = 0xff - (((tmp >> 8) + tmp) >> 8);
    /* todo: verify that a_r is nonzero in all cases */

    /* Compute a_s / a_r in 16.16 format */
    src_scale = ((a_s << 16) + (a_r >> 1)) / a_r;

    /* Do compositing with blending */
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
    dst[stride] = a_r;
}

static inline void
art_pdf_composite_pixel_alpha_16_fast_mono(uint16_t *gs_restrict dst, const uint16_t *gs_restrict src,
                                           gs_blend_mode_t blend_mode,
                                           const pdf14_nonseparable_blending_procs_t * pblend_procs,
                                           int stride, pdf14_device *p14dev)
{
    uint16_t a_b, a_s;
    unsigned int a_r;
    int tmp;
    int src_scale;
    int c_b, c_s;
    uint16_t blend[ART_MAX_CHAN];

    a_s = src[1];

    a_b = dst[stride];
    a_b += a_b>>15;

    /* Result alpha is Union of backdrop and source alpha */
    tmp = (0x10000 - a_b) * (0xffff - a_s) + 0x8000;
    a_r = 0xffff - (tmp >> 16);
    /* todo: verify that a_r is nonzero in all cases */

    /* Compute a_s / a_r in 16.16 format */
    src_scale = ((a_s << 16) + (a_r >> 1)) / a_r;

    src_scale >>= 1; /* Lose a bit to avoid overflow */
    a_b >>= 1; /* Lose a bit to avoid overflow */
    /* Do compositing with blending */
    art_blend_pixel_16(blend, dst, src, 1, blend_mode, pblend_procs, p14dev);
    {
        int c_bl;		/* Result of blend function */

        c_s = src[0];
        c_b = dst[0];
        c_bl = blend[0];
        tmp = a_b * (c_bl - c_s) + 0x4000;
        c_s += (tmp>>15);
        dst[0] = c_b + ((src_scale * (c_s - c_b) + 0x4000)>>15);
    }
    dst[stride] = a_r;
}

/**
 * art_pdf_recomposite_group_8: Recomposite group pixel.
 * @dst: Where to store pixel, also initial backdrop of group.
 * @dst_alpha_g: Optional pointer to alpha g value associated with @dst.
 * @alpha: Alpha mask value.
 * @src_alpha_g: alpha_g value associated with @src.
 * @blend_mode: Blend mode for compositing.
 *
 * Note: this is only for non-isolated groups. This covers only the
 * single-alpha case. A separate function is needed for dual-alpha,
 * and that probably needs to treat knockout separately.
 * Also note the need to know if the spot colorants should be blended
 * normal.  This occurs when we have spot colorants and the blending is set
 * for non-separable or non-white preserving blend modes
 * @src_alpha_g corresponds to $\alpha g_n$ in the Adobe notation.
 *
 * @alpha corresponds to $fk_i \cdot fm_i \cdot qk_i \cdot qm_i$.
 *
 * @NOTE: This function may corrupt src.
 *
 * Returns 1 if we need to call art_pdf_composite_pixel_alpha_8.
 **/
static forceinline int
art_pdf_recomposite_group_8(byte *gs_restrict *dstp, byte *gs_restrict dst_alpha_g,
        byte *gs_restrict src, byte src_alpha_g, int n_chan,
        byte alpha, gs_blend_mode_t blend_mode)
{
    byte dst_alpha;
    int i;
    int tmp;
    int scale;
    byte *gs_restrict dst = *dstp;

    if (src_alpha_g == 0)
        return 0;

    if (blend_mode == BLEND_MODE_Normal && alpha == 255) {
        /* In this case, uncompositing and recompositing cancel each
           other out. Note: if the reason that alpha == 255 is that
           there is no constant mask and no soft mask, then this
           operation should be optimized away at a higher level. */

        if (dst_alpha_g != NULL) {
            tmp = (255 - *dst_alpha_g) * (255 - src_alpha_g) + 0x80;
            *dst_alpha_g = 255 - ((tmp + (tmp >> 8)) >> 8);
        }
        *dstp = src;
        return 0;
    } else {
        /* "interesting" blend mode */
        dst_alpha = dst[n_chan];
        if (src_alpha_g != 255 && dst_alpha != 0) {
            /* Uncomposite the color. In other words, solve
               "src = (src, src_alpha_g) over dst" for src */
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
                src[i] = tmp;
            }
        }

        tmp = src_alpha_g * alpha + 0x80;
        tmp = (tmp + (tmp >> 8)) >> 8;
        src[n_chan] = tmp;
        if (dst_alpha_g != NULL) {
            tmp = (255 - *dst_alpha_g) * (255 - tmp) + 0x80;
            *dst_alpha_g = 255 - ((tmp + (tmp >> 8)) >> 8);
        }
    }
    return 1;
    /* todo: optimize BLEND_MODE_Normal buf alpha != 255 case */
}

static forceinline int
art_pdf_ko_recomposite_group_8(byte tos_shape,
    byte src_alpha_g, byte* gs_restrict* dstp,
    byte* gs_restrict dst_alpha_g, byte* gs_restrict src,
    int n_chan, byte alpha, gs_blend_mode_t blend_mode, bool has_mask)
{
    byte* gs_restrict dst = *dstp;

    if (tos_shape == 0 || src_alpha_g == 0) {
        /* If a softmask was present pass it along Bug 693548 */
        if (has_mask)
            dst[n_chan] = alpha;
        return 0;
    }

    return art_pdf_recomposite_group_8(dstp, dst_alpha_g, src, src_alpha_g,
                                       n_chan, alpha, blend_mode);
}

static forceinline int
art_pdf_recomposite_group_16(uint16_t *gs_restrict *dstp, uint16_t *gs_restrict dst_alpha_g,
        uint16_t *gs_restrict src, uint16_t src_alpha_g, int n_chan,
        uint16_t alpha, gs_blend_mode_t blend_mode)
{
    uint16_t dst_alpha;
    int i;
    uint32_t tmp;
    uint16_t *gs_restrict dst = *dstp;

    if (src_alpha_g == 0)
        return 0;

    if (blend_mode == BLEND_MODE_Normal && alpha == 65535) {
        /* In this case, uncompositing and recompositing cancel each
           other out. Note: if the reason that alpha == 65535 is that
           there is no constant mask and no soft mask, then this
           operation should be optimized away at a higher level. */

        if (dst_alpha_g != NULL) {
            int d = *dst_alpha_g;
            d += d>>15;
            tmp = (0x10000 - d) * (0xffff - src_alpha_g) + 0x8000;
            *dst_alpha_g = 0xffff - (tmp>>16);
        }
        *dstp = src;
        return 0;
    } else {
        /* "interesting" blend mode */
        dst_alpha = dst[n_chan];
        if (src_alpha_g != 65535 && dst_alpha != 0) {
            /* Uncomposite the color. In other words, solve
               "src = (src, src_alpha_g) over dst" for src */
            uint32_t scale = ((unsigned int)(dst_alpha * 65535 + (src_alpha_g>>1))) / src_alpha_g -
                dst_alpha;
            /* scale is NOT in 16.16 form here. I've seen values of 0xfefe01, for example. */
            for (i = 0; i < n_chan; i++) {
                int si, di;
                int64_t tmp64;
                int t;

                si = src[i];
                di = dst[i];
                /* RJW: Nasty that we have to resort to 64bit here, but we'll live with it. */
                tmp64 = (si - di) * (uint64_t)scale + 0x8000;
                t = si + (tmp64 >> 16);
                if (t < 0)
                    t = 0;
                else if (t > 65535)
                    t = 65535;
                src[i] = t;
            }
        }

        tmp = alpha + (alpha>>15);
        tmp = (src_alpha_g * tmp + 0x8000)>>16;
        src[n_chan] = tmp;
        if (dst_alpha_g != NULL) {
            uint32_t d = *dst_alpha_g;
            d += d>>15;
            tmp = (0x10000 - d) * (0xffff - tmp) + 0x8000;
            *dst_alpha_g = 0xffff - (tmp >> 16);
        }
    }
    return 1;
    /* todo: optimize BLEND_MODE_Normal buf alpha != 255 case */
}

static forceinline int
art_pdf_ko_recomposite_group_16(uint16_t tos_shape,
    uint16_t src_alpha_g, uint16_t* gs_restrict* dstp,
    uint16_t* gs_restrict dst_alpha_g, uint16_t* gs_restrict src,
    int n_chan, uint16_t alpha, gs_blend_mode_t blend_mode,
    bool has_mask)
{
    uint16_t* gs_restrict dst = *dstp;

    if (tos_shape == 0 || src_alpha_g == 0) {
        /* If a softmask was present pass it along Bug 693548 */
        if (has_mask)
            dst[n_chan] = alpha;
        return 0;
    }

    return art_pdf_recomposite_group_16(dstp, dst_alpha_g, src,
                                        src_alpha_g, n_chan, alpha,
                                        blend_mode);
}

/**
 * art_pdf_composite_group_8: Composite group pixel.
 * @dst: Where to store pixel, also initial backdrop of group.
 * @dst_alpha_g: Optional pointer to alpha g value.
 * @alpha: Alpha mask value.
 * @blend_mode: Blend mode for compositing.
 * @pblend_procs: Procs for handling non separable blending modes.
 *
 * Note: this is only for isolated groups. This covers only the
 * single-alpha case. A separate function is needed for dual-alpha,
 * and that probably needs to treat knockout separately.
 *
 * Components 0 to first_spot are blended with blend_mode.
 * Components first_spot to n_chan are blended with BLEND_MODE_Normal.
 *
 * @alpha corresponds to $fk_i \cdot fm_i \cdot qk_i \cdot qm_i$.
 *
 * @NOTE: This function may corrupt src.
 *
 * Returns 1 if we need to call art_pdf_composite_pixel_alpha_8.
 **/
static forceinline int
art_pdf_composite_group_8(byte *gs_restrict dst, byte *gs_restrict dst_alpha_g,
        byte *gs_restrict src, int n_chan, byte alpha)
{
    byte src_alpha = src[n_chan];		/* $\alpha g_n$ */

    if (src_alpha == 0)
        return 0;

    if (alpha != 255) {
        int tmp = src_alpha * alpha + 0x80;
        src[n_chan] = (tmp + (tmp >> 8)) >> 8;
    }

    if (dst_alpha_g != NULL) {
        int tmp = (255 - *dst_alpha_g) * (255 - src[n_chan]) + 0x80;
        *dst_alpha_g = 255 - ((tmp + (tmp >> 8)) >> 8);
    }

    return 1;
}

static forceinline int
art_pdf_ko_composite_group_8(byte tos_shape,
    byte* gs_restrict src_alpha_g, byte* gs_restrict dst,
    byte* gs_restrict dst_alpha_g, byte* gs_restrict src,
    int n_chan, byte alpha, bool has_mask)
{
    byte src_alpha;		/* $\alpha g_n$ */
    int tmp;

    if (tos_shape == 0 || (src_alpha_g != NULL && *src_alpha_g == 0)) {
        /* If a softmask was present pass it along Bug 693548 */
        if (has_mask)
            dst[n_chan] = alpha;
        return 0;
    }

    if (alpha != 255) {
        if (tos_shape != 255)
            return 0;
        src_alpha = src[n_chan];
        if (src_alpha == 0)
            return 0;
        tmp = src_alpha * alpha + 0x80;
        src[n_chan] = (tmp + (tmp >> 8)) >> 8;
    }

    if (dst_alpha_g != NULL) {
        tmp = (255 - *dst_alpha_g) * (255 - src[n_chan]) + 0x80;
        *dst_alpha_g = 255 - ((tmp + (tmp >> 8)) >> 8);
    }
    return 1;
}

static forceinline int
art_pdf_composite_group_16(uint16_t *gs_restrict dst, uint16_t *gs_restrict dst_alpha_g,
        uint16_t *gs_restrict src, int n_chan, uint16_t alpha)
{
    uint16_t src_alpha = src[n_chan];		/* $\alpha g_n$ */

    if (src_alpha == 0)
        return 0;

    if (alpha != 65535) {
        int tmp = alpha + (alpha>>15);
        src[n_chan] = (src_alpha * tmp + 0x8000)>>16;
    }

    if (dst_alpha_g != NULL) {
        int tmp = *dst_alpha_g;
        tmp += tmp>>15;
        tmp = (0x10000 - tmp) * (0xffff - src[n_chan]) + 0x8000;
        *dst_alpha_g = 0xffff - (tmp >> 16);
    }

    return 1;
}

static forceinline int
art_pdf_ko_composite_group_16(uint16_t tos_shape,
    uint16_t* gs_restrict src_alpha_g, uint16_t* gs_restrict dst,
    uint16_t* gs_restrict dst_alpha_g, uint16_t* gs_restrict src,
    int n_chan, uint16_t alpha, bool has_mask)
{
    uint16_t src_alpha;
    int tmp;

    if (tos_shape == 0 || (src_alpha_g != NULL && *src_alpha_g == 0)) {
        /* If a softmask was present pass it along Bug 693548 */
        if (has_mask)
            dst[n_chan] = alpha;
        return 0;
    }

    if (alpha != 65535) {
        if (tos_shape != 65535)
            return 0;
        src_alpha = src[n_chan];
        if (src_alpha == 0)
            return 0;
        tmp = alpha + (alpha >> 15);
        src[n_chan] = (src_alpha * tmp + 0x8000) >> 16;
    }

    if (dst_alpha_g != NULL) {
        tmp = *dst_alpha_g;
        tmp += tmp >> 15;
        tmp = (0x10000 - tmp) * (0xffff - src[n_chan]) + 0x8000;
        *dst_alpha_g = 0xffff - (tmp >> 16);
    }
    return 1;
}

/* A very simple case.  Knockout isolated group going to a parent that is not
   a knockout.  Simply copy over everwhere where we have a non-zero alpha value */
void
art_pdf_knockoutisolated_group_8(byte *gs_restrict dst, const byte *gs_restrict src, int n_chan)
{
    byte src_alpha;

    src_alpha = src[n_chan];
    if (src_alpha == 0)
        return;

    memcpy (dst, src, n_chan + 1);
}

void
art_pdf_knockoutisolated_group_16(uint16_t *gs_restrict dst, const uint16_t *gs_restrict src, int n_chan)
{
    uint16_t src_alpha;

    src_alpha = src[n_chan];
    if (src_alpha == 0)
        return;

    memcpy (dst, src, 2*(n_chan + 1));
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
art_pdf_knockoutisolated_group_aa_8(byte *gs_restrict dst, const byte *gs_restrict src, byte src_alpha,
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
        n_chan, NULL, p14dev);
    dst[n_chan] = src_alpha;
}

void
art_pdf_composite_knockout_8(byte *gs_restrict dst,
                             const byte *gs_restrict src,
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
            memcpy (dst, src, n_chan + 1);
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
                    tmp = tmp / (result_alpha * 255);
                    if (tmp > 255) tmp = 255;
                    dst[i] = tmp;
                }
            dst[n_chan] = result_alpha;
        }
    } else {
        /* Do compositing with blending */
        byte blend[ART_MAX_CHAN];
        byte a_b, a_s;
        unsigned int a_r;
        /* Using a volatile variable set to 0 here works around
           a gcc (10.3.0) compiler optimiser bug that appears to
           drop the "if (a_r != 0)" test below, meaning we can end
           up dividing by a_r when it is zero.
         */
        volatile const unsigned int vzero = 0;
        int src_scale;
        int c_b, c_s;

        a_s = src[n_chan];
        a_b = dst[n_chan];

        /* Result alpha is Union of backdrop and source alpha */
        tmp = (0xff - a_b) * (0xff - a_s) + 0x80;
        a_r = 0xff - (((tmp >> 8) + tmp) >> 8);

        if (a_r != vzero) {
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
        }
        dst[n_chan] = a_r;
    }
}

void
art_pdf_composite_knockout_16(uint16_t *gs_restrict dst,
                              const uint16_t *gs_restrict src,
                              int n_chan,
                              gs_blend_mode_t blend_mode,
                              const pdf14_nonseparable_blending_procs_t * pblend_procs,
                              pdf14_device *p14dev)
{
    uint16_t src_shape = src[n_chan];
    int i;
    unsigned int tmp;

    if (blend_mode == BLEND_MODE_Normal) {
        /* Do simple compositing of source over backdrop */
        if (src_shape == 0)
            return;
        else if (src_shape == 65535) {
            memcpy (dst, src, (n_chan + 1)*2);
            return;
        } else {
            /* Use src_shape to interpolate (in premultiplied alpha space)
               between dst and (src, opacity). */
            int dst_alpha = dst[n_chan];
            uint16_t result_alpha;

            tmp = (65535 - dst_alpha) * src_shape + 0x8000;
            result_alpha = dst_alpha + ((tmp + (tmp >> 16)) >> 16);

            if (result_alpha != 0) {
                dst_alpha += dst_alpha>>15;
                for (i = 0; i < n_chan; i++) {
                    /* todo: optimize this - can strength-reduce so that
                       inner loop is a single interpolation */
                    tmp = dst[i] * dst_alpha;
                    tmp = (tmp>>16) * (65535 - src_shape) +
                           src[i] * src_shape + (result_alpha>>1);
                    tmp = tmp / result_alpha;
                    if (tmp > 65535) tmp = 65535;
                    dst[i] = tmp;

                }
            }
            dst[n_chan] = result_alpha;
        }
    } else {
        /* Do compositing with blending */
        uint16_t blend[ART_MAX_CHAN];
        uint16_t a_b, a_s;
        unsigned int a_r;
        /* Using a volatile variable set to 0 here works around
           a gcc (10.3.0) compiler optimiser bug that appears to
           drop the "if (a_r != 0)" test below, meaning we can end
           up dividing by a_r when it is zero.
         */
        volatile const unsigned int vzero = 0;
        int src_scale;
        int c_b, c_s;

        a_s = src[n_chan];
        a_b = dst[n_chan];

        /* Result alpha is Union of backdrop and source alpha */
        tmp = (0xffff - a_b) * (0xffff - a_s) + 0x8000;
        a_r = 0xffff - (((tmp >> 16) + tmp) >> 16);

        if (a_r != vzero) {
            /* Compute a_s / a_r in 16.16 format */
            src_scale = ((a_s << 16) + (a_r >> 1)) / a_r;

            src_scale >>= 1; /* Lose a bit to avoid overflow */
            a_b >>= 1; /* Lose a bit to avoid overflow */
            art_blend_pixel_16(blend, dst, src, n_chan, blend_mode, pblend_procs, p14dev);
            for (i = 0; i < n_chan; i++) {
                int c_bl;		/* Result of blend function */
                int c_mix;		/* Blend result mixed with source color */
                int stmp;

                c_s = src[i];
                c_b = dst[i];
                c_bl = blend[i];
                stmp = a_b * (c_bl - ((int)c_s)) + 0x4000;
                c_mix = c_s + (((stmp >> 16) + stmp) >> 15);
                tmp = src_scale * (c_mix - c_b) + 0x4000;
                dst[i] = c_b + (tmp >> 15);
            }
        }
        dst[n_chan] = a_r;
    }
}

#if RAW_DUMP
/* Debug dump of buffer data from pdf14 device.  Saved in
   planar form with global indexing and tag information in
   file name */
static void
do_dump_raw_buffer(const gs_memory_t *mem, int num_rows, int width, int n_chan,
                   int plane_stride, int rowstride,
                   char filename[], const byte *Buffer, bool deep, bool be)
{
    char full_file_name[50];
    gp_file *fid;
    int x, y, z;
    const byte *buff_ptr;

   /* clist_band_count is incremented at every pdf14putimage */
   /* Useful for catching this thing and only dumping */
   /* during a particular band if we have a large file */
   /* if (clist_band_count != 65) return; */
    buff_ptr = Buffer;
    dlprintf2("%02d)%s.pam\n",global_index,filename);dflush();
    gs_snprintf(full_file_name,sizeof(full_file_name),"%02d)%s.pam",global_index,filename);
    fid = gp_fopen(mem,full_file_name,"wb");
    gp_fprintf(fid, "P7\nWIDTH %d\nHEIGHT %d\nDEPTH %d\nMAXVAL %d\nTUPLTYPE %s\nENDHDR\n",
               width, num_rows, n_chan, deep ? 65535 : 255,
               n_chan == 1 ? "GRAYSCALE" :
               n_chan == 2 ? "GRAYSCALE_ALPHA" :
               n_chan == 3 ? "RGB" :
               n_chan == 4 ? "CMYK" :
               n_chan == 5 ? "CMYK_ALPHA" :
                             "CMYK_SPOTS"
    );
    if (deep) {
        for(y=0; y<num_rows; y++)
            for(x=0; x<width; x++)
                for(z=0; z<n_chan; z++) {
                    gp_fputc(Buffer[z*plane_stride + y*rowstride + x*2 + be^1], fid);
                    gp_fputc(Buffer[z*plane_stride + y*rowstride + x*2 + be  ], fid);
                }
    } else {
        for(y=0; y<num_rows; y++)
            for(x=0; x<width; x++)
                for(z=0; z<n_chan; z++)
                    gp_fputc(Buffer[z*plane_stride + y*rowstride + x], fid);
    }
    gp_fclose(fid);
}

void
dump_raw_buffer(const gs_memory_t *mem, int num_rows, int width, int n_chan,
                int plane_stride, int rowstride,
                char filename[],const byte *Buffer, bool deep)
{
    do_dump_raw_buffer(mem, num_rows, width, n_chan, plane_stride,
                       rowstride, filename, Buffer, deep, 0);
}

void
dump_raw_buffer_be(const gs_memory_t *mem, int num_rows, int width, int n_chan,
                   int plane_stride, int rowstride,
                   char filename[],const byte *Buffer, bool deep)
{
    do_dump_raw_buffer(mem, num_rows, width, n_chan, plane_stride,
                       rowstride, filename, Buffer, deep, 1);
}
#endif

typedef void (*art_pdf_compose_group_fn)(byte *tos_ptr, bool tos_isolated, int tos_planestride, int tos_rowstride,
                                         byte alpha, byte shape, gs_blend_mode_t blend_mode, bool tos_has_shape,
                                         int tos_shape_offset, int tos_alpha_g_offset, int tos_tag_offset, bool tos_has_tag,
                                         byte *tos_alpha_g_ptr, byte *nos_ptr, bool nos_isolated, int nos_planestride, int nos_rowstride,
                                         byte *nos_alpha_g_ptr, bool nos_knockout, int nos_shape_offset, int nos_tag_offset,
                                         byte *mask_row_ptr, int has_mask, pdf14_buf *maskbuf, byte mask_bg_alpha, const byte *mask_tr_fn,
                                         byte *backdrop_ptr, bool has_matte, int n_chan, bool additive, int num_spots, bool overprint,
                                         gx_color_index drawn_comps, int x0, int y0, int x1, int y1,
                                         const pdf14_nonseparable_blending_procs_t *pblend_procs, pdf14_device *pdev);

static forceinline void
template_compose_group(byte *gs_restrict tos_ptr, bool tos_isolated,
                       int tos_planestride, int tos_rowstride,
                       byte alpha, byte shape, gs_blend_mode_t blend_mode,
                       bool tos_has_shape, int tos_shape_offset,
                       int tos_alpha_g_offset, int tos_tag_offset,
                       bool tos_has_tag, byte *gs_restrict tos_alpha_g_ptr,
                       byte *gs_restrict nos_ptr,
                       bool nos_isolated, int nos_planestride,
                       int nos_rowstride, byte *gs_restrict nos_alpha_g_ptr,
                       bool nos_knockout, int nos_shape_offset,
                       int nos_tag_offset, byte *gs_restrict mask_row_ptr,
                       int has_mask, pdf14_buf *gs_restrict maskbuf,
                       byte mask_bg_alpha, const byte *gs_restrict mask_tr_fn,
                       byte *gs_restrict backdrop_ptr, bool has_matte,
                       int n_chan, bool additive, int num_spots,
                       bool overprint, gx_color_index drawn_comps,
                       int x0, int y0, int x1, int y1,
                       const pdf14_nonseparable_blending_procs_t *pblend_procs,
                       pdf14_device *pdev, int has_alpha)
{
    byte *gs_restrict mask_curr_ptr = NULL;
    int width = x1 - x0;
    int x, y;
    int i;
    byte tos_pixel[PDF14_MAX_PLANES];
    byte nos_pixel[PDF14_MAX_PLANES];
    byte back_drop[PDF14_MAX_PLANES];
    bool in_mask_rect_y;
    bool in_mask_rect;
    byte pix_alpha;
    byte matte_alpha = 0xff;
    int first_spot = n_chan - num_spots;
    int first_blend_spot = n_chan;
    bool has_mask2 = has_mask;
    byte *gs_restrict dst;
    byte group_shape = (byte)(255 * pdev->shape + 0.5);

    if (!nos_knockout && num_spots > 0 && !blend_valid_for_spot(blend_mode)) {
        first_blend_spot = first_spot;
    }
    if (blend_mode == BLEND_MODE_Normal)
        first_blend_spot = 0;
    if (!nos_isolated && backdrop_ptr != NULL)
        has_mask2 = false;

    for (y = y1 - y0; y > 0; --y) {
        mask_curr_ptr = mask_row_ptr;
        in_mask_rect_y = (has_mask && y1 - y >= maskbuf->rect.p.y && y1 - y < maskbuf->rect.q.y);
        for (x = 0; x < width; x++) {
            in_mask_rect = (in_mask_rect_y && x0 + x >= maskbuf->rect.p.x && x0 + x < maskbuf->rect.q.x);
            pix_alpha = alpha;
            /* If we have a soft mask, then we have some special handling of the
               group alpha value */
            if (maskbuf != NULL) {
                if (!in_mask_rect) {
                    /* Special case where we have a soft mask but are outside
                       the range of the soft mask and must use the background
                       alpha value */
                    pix_alpha = mask_bg_alpha;
                    matte_alpha = 0xff;
                } else {
                    if (has_matte)
                        matte_alpha = mask_tr_fn[*mask_curr_ptr];
                }
            }

            /* Matte present, need to undo premultiplied alpha prior to blend */
            if (has_matte && matte_alpha != 0 && matte_alpha < 0xff) {
                for (i = 0; i < n_chan; i++) {
                    /* undo */
                    byte matte = maskbuf->matte[i]>>8;
                    int val = tos_ptr[i * tos_planestride] - matte;
                    int temp = ((((val * 0xff) << 8) / matte_alpha) >> 8) + matte;

                    /* clip */
                    if (temp > 0xff)
                        tos_pixel[i] = 0xff;
                    else if (temp < 0)
                        tos_pixel[i] = 0;
                    else
                        tos_pixel[i] = temp;

                    if (!additive) {
                        /* Pure subtractive */
                        tos_pixel[i] = 255 - tos_pixel[i];
                        nos_pixel[i] = 255 - nos_ptr[i * nos_planestride];
                    } else {
                        /* additive or hybrid */
                        if (i >= first_spot)
                            nos_pixel[i] = 255 - nos_ptr[i * nos_planestride];
                        else
                            nos_pixel[i] = nos_ptr[i * nos_planestride];
                    }
                }
            } else {
                /* No matte present */
                if (!additive) {
                    /* Pure subtractive */
                    for (i = 0; i < n_chan; ++i) {
                        tos_pixel[i] = 255 - tos_ptr[i * tos_planestride];
                        nos_pixel[i] = 255 - nos_ptr[i * nos_planestride];
                    }
                } else {
                    /* Additive or hybrid */
                    for (i = 0; i < first_spot; ++i) {
                        tos_pixel[i] = tos_ptr[i * tos_planestride];
                        nos_pixel[i] = nos_ptr[i * nos_planestride];
                    }
                    for (; i < n_chan; i++) {
                        tos_pixel[i] = 255 - tos_ptr[i * tos_planestride];
                        nos_pixel[i] = 255 - nos_ptr[i * nos_planestride];
                    }
                }
            }
            /* alpha */
            tos_pixel[n_chan] = has_alpha ? tos_ptr[n_chan * tos_planestride] : 255;
            nos_pixel[n_chan] = has_alpha ? nos_ptr[n_chan * nos_planestride] : 255;

            if (mask_curr_ptr != NULL) {
                if (in_mask_rect) {
                    byte mask = mask_tr_fn[*mask_curr_ptr++];
                    int tmp = pix_alpha * mask + 0x80;
                    pix_alpha = (tmp + (tmp >> 8)) >> 8;
                } else {
                    mask_curr_ptr++;
                }
            }

            dst = nos_pixel;
            if (nos_knockout) {
                /* We need to be knocking out what ever is on the nos, but may
                   need to combine with it's backdrop */
                byte tos_shape = 255;

                if (tos_has_shape)
                    tos_shape = tos_ptr[tos_shape_offset];

                if (nos_isolated || backdrop_ptr == NULL) {
                    /* We do not need to compose with the backdrop */
                    back_drop[n_chan] = 0;
                    /* FIXME: The blend here can be simplified */
                } else {
                    /* Per the PDF spec, since the tos is not isolated and we are
                       going onto a knock out group, we do the composition with
                       the nos initial backdrop. */
                    if (additive) {
                        /* additive or hybrid */
                        for (i = 0; i < first_spot; ++i) {
                            back_drop[i] = backdrop_ptr[i * nos_planestride];
                        }
                        for (; i < n_chan; i++) {
                            back_drop[i] = 255 - backdrop_ptr[i * nos_planestride];
                        }
                    } else {
                        /* pure subtractive */
                        for (i = 0; i < n_chan; ++i) {
                            back_drop[i] = 255 - backdrop_ptr[i * nos_planestride];
                        }
                    }
                    /* alpha */
                    back_drop[n_chan] = backdrop_ptr[n_chan * nos_planestride];
                }
                if (tos_isolated ?
                    art_pdf_ko_composite_group_8(tos_shape, tos_alpha_g_ptr,
                                                 nos_pixel, nos_alpha_g_ptr,
                                                 tos_pixel, n_chan, pix_alpha,
                                                 has_mask2) :
                    art_pdf_ko_recomposite_group_8(tos_shape, has_alpha ? tos_ptr[tos_alpha_g_offset] : 255,
                                                   &dst, nos_alpha_g_ptr, tos_pixel, n_chan, pix_alpha,
                                                   blend_mode, has_mask2))
                    dst = art_pdf_knockout_composite_pixel_alpha_8(back_drop, tos_shape,
                                                                   nos_pixel, tos_pixel,
                                                                   n_chan, blend_mode,
                                                                   pblend_procs, pdev);
            } else if (tos_isolated ?
                       art_pdf_composite_group_8(nos_pixel, nos_alpha_g_ptr,
                                                 tos_pixel, n_chan, pix_alpha) :
                       art_pdf_recomposite_group_8(&dst, nos_alpha_g_ptr,
                           tos_pixel, has_alpha ? tos_ptr[tos_alpha_g_offset] : 255, n_chan,
                                                   pix_alpha, blend_mode)) {
                dst = art_pdf_composite_pixel_alpha_8_inline(nos_pixel, tos_pixel, n_chan,
                                                blend_mode, first_blend_spot,
                                                pblend_procs, pdev);
            }
            if (nos_shape_offset && pix_alpha != 0) {
                nos_ptr[nos_shape_offset] =
                    art_pdf_union_mul_8(nos_ptr[nos_shape_offset],
                                        has_alpha ? tos_ptr[tos_shape_offset] : group_shape,
                                        shape);
            }
            if (dst)
            {
                /* Complement the results for subtractive color spaces.  Again,
                 * if we are in an additive blending color space, we are not
                 * going to be fooling with overprint of spot colors */
                if (additive) {
                    /* additive or hybrid */
                    for (i = 0; i < first_spot; ++i) {
                        nos_ptr[i * nos_planestride] = dst[i];
                    }
                    for (; i < n_chan; i++) {
                        nos_ptr[i * nos_planestride] = 255 - dst[i];
                    }
                } else {
                    /* Pure subtractive */
                    for (i = 0; i < n_chan; ++i)
                        nos_ptr[i * nos_planestride] = 255 - dst[i];
                }
                /* alpha */
                nos_ptr[n_chan * nos_planestride] = dst[n_chan];
            }
            /* tags */
            if (nos_tag_offset && tos_has_tag) {
                nos_ptr[nos_tag_offset] |= tos_ptr[tos_tag_offset];
             }

            if (nos_alpha_g_ptr != NULL)
                ++nos_alpha_g_ptr;
            if (tos_alpha_g_ptr != NULL)
                ++tos_alpha_g_ptr;
            if (backdrop_ptr != NULL)
                ++backdrop_ptr;
            ++tos_ptr;
            ++nos_ptr;
        }
        tos_ptr += tos_rowstride - width;
        nos_ptr += nos_rowstride - width;
        if (tos_alpha_g_ptr != NULL)
            tos_alpha_g_ptr += tos_rowstride - width;
        if (nos_alpha_g_ptr != NULL)
            nos_alpha_g_ptr += nos_rowstride - width;
        if (mask_row_ptr != NULL)
            mask_row_ptr += maskbuf->rowstride;
        if (backdrop_ptr != NULL)
            backdrop_ptr += nos_rowstride - width;
    }
}

static void
compose_group_knockout(byte *tos_ptr, bool tos_isolated, int tos_planestride, int tos_rowstride, byte alpha, byte shape, gs_blend_mode_t blend_mode, bool tos_has_shape,
              int tos_shape_offset, int tos_alpha_g_offset, int tos_tag_offset, bool tos_has_tag, byte *tos_alpha_g_ptr,
              byte *nos_ptr, bool nos_isolated, int nos_planestride, int nos_rowstride, byte *nos_alpha_g_ptr, bool nos_knockout,
              int nos_shape_offset, int nos_tag_offset,
              byte *mask_row_ptr, int has_mask, pdf14_buf *maskbuf, byte mask_bg_alpha, const byte *mask_tr_fn,
              byte *backdrop_ptr,
              bool has_matte, int n_chan, bool additive, int num_spots, bool overprint, gx_color_index drawn_comps, int x0, int y0, int x1, int y1,
              const pdf14_nonseparable_blending_procs_t *pblend_procs, pdf14_device *pdev)
{
    template_compose_group(tos_ptr, tos_isolated, tos_planestride, tos_rowstride, alpha, shape, blend_mode, tos_has_shape,
        tos_shape_offset, tos_alpha_g_offset, tos_tag_offset, tos_has_tag, tos_alpha_g_ptr,
        nos_ptr, nos_isolated, nos_planestride, nos_rowstride, nos_alpha_g_ptr, /* nos_knockout = */1,
        nos_shape_offset, nos_tag_offset, mask_row_ptr, has_mask, maskbuf, mask_bg_alpha, mask_tr_fn,
        backdrop_ptr, has_matte, n_chan, additive, num_spots, overprint, drawn_comps, x0, y0, x1, y1, pblend_procs, pdev, 1);
}

static void
compose_group_nonknockout_blend(byte *tos_ptr, bool tos_isolated, int tos_planestride, int tos_rowstride, byte alpha, byte shape, gs_blend_mode_t blend_mode, bool tos_has_shape,
              int tos_shape_offset, int tos_alpha_g_offset, int tos_tag_offset, bool tos_has_tag, byte *tos_alpha_g_ptr,
              byte *nos_ptr, bool nos_isolated, int nos_planestride, int nos_rowstride, byte *nos_alpha_g_ptr, bool nos_knockout,
              int nos_shape_offset, int nos_tag_offset,
              byte *mask_row_ptr, int has_mask, pdf14_buf *maskbuf, byte mask_bg_alpha, const byte *mask_tr_fn,
              byte *backdrop_ptr,
              bool has_matte, int n_chan, bool additive, int num_spots, bool overprint, gx_color_index drawn_comps, int x0, int y0, int x1, int y1,
              const pdf14_nonseparable_blending_procs_t *pblend_procs, pdf14_device *pdev)
{
    template_compose_group(tos_ptr, tos_isolated, tos_planestride, tos_rowstride, alpha, shape, blend_mode, tos_has_shape,
        tos_shape_offset, tos_alpha_g_offset, tos_tag_offset, tos_has_tag, tos_alpha_g_ptr,
        nos_ptr, nos_isolated, nos_planestride, nos_rowstride, nos_alpha_g_ptr, /* nos_knockout = */0,
        nos_shape_offset, nos_tag_offset, mask_row_ptr, has_mask, maskbuf, mask_bg_alpha, mask_tr_fn,
        backdrop_ptr, has_matte, n_chan, additive, num_spots, overprint, drawn_comps, x0, y0, x1, y1, pblend_procs, pdev, 1);
}

static void
compose_group_nonknockout_nonblend_isolated_allmask_common(byte *tos_ptr, bool tos_isolated, int tos_planestride, int tos_rowstride, byte alpha, byte shape, gs_blend_mode_t blend_mode, bool tos_has_shape,
              int tos_shape_offset, int tos_alpha_g_offset, int tos_tag_offset, bool tos_has_tag, byte *tos_alpha_g_ptr,
              byte *nos_ptr, bool nos_isolated, int nos_planestride, int nos_rowstride, byte *nos_alpha_g_ptr, bool nos_knockout,
              int nos_shape_offset, int nos_tag_offset,
              byte *mask_row_ptr, int has_mask, pdf14_buf *maskbuf, byte mask_bg_alpha, const byte *mask_tr_fn,
              byte *backdrop_ptr,
              bool has_matte, int n_chan, bool additive, int num_spots, bool overprint, gx_color_index drawn_comps, int x0, int y0, int x1, int y1,
              const pdf14_nonseparable_blending_procs_t *pblend_procs, pdf14_device *pdev)
{
    int width = x1 - x0;
    int x, y;
    int i;

    for (y = y1 - y0; y > 0; --y) {
        byte *gs_restrict mask_curr_ptr = mask_row_ptr;
        for (x = 0; x < width; x++) {
            byte mask = mask_tr_fn[*mask_curr_ptr++];
            byte src_alpha = tos_ptr[n_chan * tos_planestride];
            if (src_alpha != 0) {
                byte a_b;

                int tmp = alpha * mask + 0x80;
                byte pix_alpha = (tmp + (tmp >> 8)) >> 8;

                if (pix_alpha != 255) {
                    int tmp = src_alpha * pix_alpha + 0x80;
                    src_alpha = (tmp + (tmp >> 8)) >> 8;
                }

                a_b = nos_ptr[n_chan * nos_planestride];
                if (a_b == 0) {
                    /* Simple copy of colors plus alpha. */
                    for (i = 0; i < n_chan; i++) {
                        nos_ptr[i * nos_planestride] = tos_ptr[i * tos_planestride];
                    }
                    nos_ptr[i * nos_planestride] = src_alpha;
                } else {
                    /* Result alpha is Union of backdrop and source alpha */
                    int tmp = (0xff - a_b) * (0xff - src_alpha) + 0x80;
                    unsigned int a_r = 0xff - (((tmp >> 8) + tmp) >> 8);

                    /* Compute src_alpha / a_r in 16.16 format */
                    int src_scale = ((src_alpha << 16) + (a_r >> 1)) / a_r;

                    nos_ptr[n_chan * nos_planestride] = a_r;

                    /* Do simple compositing of source over backdrop */
                    for (i = 0; i < n_chan; i++) {
                        int c_s = tos_ptr[i * tos_planestride];
                        int c_b = nos_ptr[i * nos_planestride];
                        tmp = src_scale * (c_s - c_b) + 0x8000;
                        nos_ptr[i * nos_planestride] = c_b + (tmp >> 16);
                    }
                }
            }
            ++tos_ptr;
            ++nos_ptr;
        }
        tos_ptr += tos_rowstride - width;
        nos_ptr += nos_rowstride - width;
        mask_row_ptr += maskbuf->rowstride;
    }
}

static void
compose_group_nonknockout_nonblend_isolated_mask_common(byte *tos_ptr, bool tos_isolated, int tos_planestride, int tos_rowstride, byte alpha, byte shape, gs_blend_mode_t blend_mode, bool tos_has_shape,
              int tos_shape_offset, int tos_alpha_g_offset, int tos_tag_offset, bool tos_has_tag, byte *tos_alpha_g_ptr,
              byte *nos_ptr, bool nos_isolated, int nos_planestride, int nos_rowstride, byte *nos_alpha_g_ptr, bool nos_knockout,
              int nos_shape_offset, int nos_tag_offset,
              byte *mask_row_ptr, int has_mask, pdf14_buf *maskbuf, byte mask_bg_alpha, const byte *mask_tr_fn,
              byte *backdrop_ptr,
              bool has_matte, int n_chan, bool additive, int num_spots, bool overprint, gx_color_index drawn_comps, int x0, int y0, int x1, int y1,
              const pdf14_nonseparable_blending_procs_t *pblend_procs, pdf14_device *pdev)
{
    byte *gs_restrict mask_curr_ptr = NULL;
    int width = x1 - x0;
    int x, y;
    int i;
    bool in_mask_rect_y;
    bool in_mask_rect;
    byte pix_alpha, src_alpha;

    for (y = y1 - y0; y > 0; --y) {
        mask_curr_ptr = mask_row_ptr;
        in_mask_rect_y = (has_mask && y1 - y >= maskbuf->rect.p.y && y1 - y < maskbuf->rect.q.y);
        for (x = 0; x < width; x++) {
            in_mask_rect = (in_mask_rect_y && has_mask && x0 + x >= maskbuf->rect.p.x && x0 + x < maskbuf->rect.q.x);
            pix_alpha = alpha;
            /* If we have a soft mask, then we have some special handling of the
               group alpha value */
            if (maskbuf != NULL) {
                if (!in_mask_rect) {
                    /* Special case where we have a soft mask but are outside
                       the range of the soft mask and must use the background
                       alpha value */
                    pix_alpha = mask_bg_alpha;
                }
            }

            if (mask_curr_ptr != NULL) {
                if (in_mask_rect) {
                    byte mask = mask_tr_fn[*mask_curr_ptr++];
                    int tmp = pix_alpha * mask + 0x80;
                    pix_alpha = (tmp + (tmp >> 8)) >> 8;
                } else {
                    mask_curr_ptr++;
                }
            }

            src_alpha = tos_ptr[n_chan * tos_planestride];
            if (src_alpha != 0) {
                byte a_b;

                if (pix_alpha != 255) {
                    int tmp = src_alpha * pix_alpha + 0x80;
                    src_alpha = (tmp + (tmp >> 8)) >> 8;
                }

                a_b = nos_ptr[n_chan * nos_planestride];
                if (a_b == 0) {
                    /* Simple copy of colors plus alpha. */
                    for (i = 0; i < n_chan; i++) {
                        nos_ptr[i * nos_planestride] = tos_ptr[i * tos_planestride];
                    }
                    nos_ptr[i * nos_planestride] = src_alpha;
                } else {
                    /* Result alpha is Union of backdrop and source alpha */
                    int tmp = (0xff - a_b) * (0xff - src_alpha) + 0x80;
                    unsigned int a_r = 0xff - (((tmp >> 8) + tmp) >> 8);

                    /* Compute src_alpha / a_r in 16.16 format */
                    int src_scale = ((src_alpha << 16) + (a_r >> 1)) / a_r;

                    nos_ptr[n_chan * nos_planestride] = a_r;

                    /* Do simple compositing of source over backdrop */
                    for (i = 0; i < n_chan; i++) {
                        int c_s = tos_ptr[i * tos_planestride];
                        int c_b = nos_ptr[i * nos_planestride];
                        tmp = (c_b << 16) + src_scale * (c_s - c_b) + 0x8000;
                        nos_ptr[i * nos_planestride] = tmp >> 16;
                    }
                }
            }
            ++tos_ptr;
            ++nos_ptr;
        }
        tos_ptr += tos_rowstride - width;
        nos_ptr += nos_rowstride - width;
        if (mask_row_ptr != NULL)
            mask_row_ptr += maskbuf->rowstride;
    }
}

static void
compose_group_nonknockout_nonblend_isolated_nomask_common(byte *tos_ptr, bool tos_isolated, int tos_planestride, int tos_rowstride, byte alpha, byte shape, gs_blend_mode_t blend_mode, bool tos_has_shape,
              int tos_shape_offset, int tos_alpha_g_offset, int tos_tag_offset, bool tos_has_tag, byte *tos_alpha_g_ptr,
              byte *nos_ptr, bool nos_isolated, int nos_planestride, int nos_rowstride, byte *nos_alpha_g_ptr, bool nos_knockout,
              int nos_shape_offset, int nos_tag_offset,
              byte *mask_row_ptr, int has_mask, pdf14_buf *maskbuf, byte mask_bg_alpha, const byte *mask_tr_fn,
              byte *backdrop_ptr,
              bool has_matte, int n_chan, bool additive, int num_spots, bool overprint, gx_color_index drawn_comps, int x0, int y0, int x1, int y1,
              const pdf14_nonseparable_blending_procs_t *pblend_procs, pdf14_device *pdev)
{
    template_compose_group(tos_ptr, /*tos_isolated*/1, tos_planestride, tos_rowstride, alpha, shape, BLEND_MODE_Normal, /*tos_has_shape*/0,
        tos_shape_offset, tos_alpha_g_offset, tos_tag_offset, /*tos_has_tag*/0, /*tos_alpha_g_ptr*/0,
        nos_ptr, /*nos_isolated*/0, nos_planestride, nos_rowstride, /*nos_alpha_g_ptr*/0, /* nos_knockout = */0,
        /*nos_shape_offset*/0, /*nos_tag_offset*/0, mask_row_ptr, /*has_mask*/0, /*maskbuf*/NULL, mask_bg_alpha, mask_tr_fn,
        backdrop_ptr, /*has_matte*/0, n_chan, /*additive*/1, /*num_spots*/0, /*overprint*/0, /*drawn_comps*/0, x0, y0, x1, y1, pblend_procs, pdev, 1);
}

static void
compose_group_nonknockout_nonblend_nonisolated_mask_common(byte *tos_ptr, bool tos_isolated, int tos_planestride, int tos_rowstride, byte alpha, byte shape, gs_blend_mode_t blend_mode, bool tos_has_shape,
              int tos_shape_offset, int tos_alpha_g_offset, int tos_tag_offset, bool tos_has_tag, byte *tos_alpha_g_ptr,
              byte *nos_ptr, bool nos_isolated, int nos_planestride, int nos_rowstride, byte *nos_alpha_g_ptr, bool nos_knockout,
              int nos_shape_offset, int nos_tag_offset,
              byte *mask_row_ptr, int has_mask, pdf14_buf *maskbuf, byte mask_bg_alpha, const byte *mask_tr_fn,
              byte *backdrop_ptr,
              bool has_matte, int n_chan, bool additive, int num_spots, bool overprint, gx_color_index drawn_comps, int x0, int y0, int x1, int y1,
              const pdf14_nonseparable_blending_procs_t *pblend_procs, pdf14_device *pdev)
{
    template_compose_group(tos_ptr, /*tos_isolated*/0, tos_planestride, tos_rowstride, alpha, shape, BLEND_MODE_Normal, /*tos_has_shape*/0,
        tos_shape_offset, tos_alpha_g_offset, tos_tag_offset, /*tos_has_tag*/0, /*tos_alpha_g_ptr*/0,
        nos_ptr, /*nos_isolated*/0, nos_planestride, nos_rowstride, /*nos_alpha_g_ptr*/0, /* nos_knockout = */0,
        /*nos_shape_offset*/0, /*nos_tag_offset*/0, mask_row_ptr, has_mask, maskbuf, mask_bg_alpha, mask_tr_fn,
        backdrop_ptr, /*has_matte*/0, n_chan, /*additive*/1, /*num_spots*/0, /*overprint*/0, /*drawn_comps*/0, x0, y0, x1, y1, pblend_procs, pdev, 1);
}

static void
compose_group_nonknockout_nonblend_nonisolated_nomask_common(byte *tos_ptr, bool tos_isolated, int tos_planestride, int tos_rowstride, byte alpha, byte shape, gs_blend_mode_t blend_mode, bool tos_has_shape,
              int tos_shape_offset, int tos_alpha_g_offset, int tos_tag_offset, bool tos_has_tag, byte *tos_alpha_g_ptr,
              byte *nos_ptr, bool nos_isolated, int nos_planestride, int nos_rowstride, byte *nos_alpha_g_ptr, bool nos_knockout,
              int nos_shape_offset, int nos_tag_offset,
              byte *mask_row_ptr, int has_mask, pdf14_buf *maskbuf, byte mask_bg_alpha, const byte *mask_tr_fn,
              byte *backdrop_ptr,
              bool has_matte, int n_chan, bool additive, int num_spots, bool overprint, gx_color_index drawn_comps, int x0, int y0, int x1, int y1,
              const pdf14_nonseparable_blending_procs_t *pblend_procs, pdf14_device *pdev)
{
    template_compose_group(tos_ptr, /*tos_isolated*/0, tos_planestride, tos_rowstride, alpha, shape, BLEND_MODE_Normal, /*tos_has_shape*/0,
        tos_shape_offset, tos_alpha_g_offset, tos_tag_offset, /*tos_has_tag*/0, /*tos_alpha_g_ptr*/0,
        nos_ptr, /*nos_isolated*/0, nos_planestride, nos_rowstride, /*nos_alpha_g_ptr*/0, /* nos_knockout = */0,
        /*nos_shape_offset*/0, /*nos_tag_offset*/0, mask_row_ptr, /*has_mask*/0, /*maskbuf*/NULL, mask_bg_alpha, mask_tr_fn,
        backdrop_ptr, /*has_matte*/0, n_chan, /*additive*/1, /*num_spots*/0, /*overprint*/0, /*drawn_comps*/0, x0, y0, x1, y1, pblend_procs, pdev, 1);
}

static void
compose_group_nonknockout_noblend_general(byte *tos_ptr, bool tos_isolated, int tos_planestride, int tos_rowstride, byte alpha, byte shape, gs_blend_mode_t blend_mode, bool tos_has_shape,
              int tos_shape_offset, int tos_alpha_g_offset, int tos_tag_offset, bool tos_has_tag, byte *tos_alpha_g_ptr,
              byte *nos_ptr, bool nos_isolated, int nos_planestride, int nos_rowstride, byte *nos_alpha_g_ptr, bool nos_knockout,
              int nos_shape_offset, int nos_tag_offset,
              byte *mask_row_ptr, int has_mask, pdf14_buf *maskbuf, byte mask_bg_alpha, const byte *mask_tr_fn,
              byte *backdrop_ptr,
              bool has_matte, int n_chan, bool additive, int num_spots, bool overprint, gx_color_index drawn_comps, int x0, int y0, int x1, int y1,
              const pdf14_nonseparable_blending_procs_t *pblend_procs, pdf14_device *pdev)
{
    template_compose_group(tos_ptr, tos_isolated, tos_planestride, tos_rowstride, alpha, shape, BLEND_MODE_Normal, tos_has_shape,
        tos_shape_offset, tos_alpha_g_offset, tos_tag_offset, tos_has_tag, tos_alpha_g_ptr,
        nos_ptr, nos_isolated, nos_planestride, nos_rowstride, nos_alpha_g_ptr, /* nos_knockout = */0,
        nos_shape_offset, nos_tag_offset, mask_row_ptr, has_mask, maskbuf, mask_bg_alpha, mask_tr_fn,
        backdrop_ptr, has_matte, n_chan, additive, num_spots, overprint, drawn_comps, x0, y0, x1, y1, pblend_procs, pdev, 1);
}

static void
compose_group_alphaless_knockout(byte *tos_ptr, bool tos_isolated, int tos_planestride, int tos_rowstride, byte alpha, byte shape, gs_blend_mode_t blend_mode, bool tos_has_shape,
              int tos_shape_offset, int tos_alpha_g_offset, int tos_tag_offset, bool tos_has_tag, byte *tos_alpha_g_ptr,
              byte *nos_ptr, bool nos_isolated, int nos_planestride, int nos_rowstride, byte *nos_alpha_g_ptr, bool nos_knockout,
              int nos_shape_offset, int nos_tag_offset,
              byte *mask_row_ptr, int has_mask, pdf14_buf *maskbuf, byte mask_bg_alpha, const byte *mask_tr_fn,
              byte *backdrop_ptr,
              bool has_matte, int n_chan, bool additive, int num_spots, bool overprint, gx_color_index drawn_comps, int x0, int y0, int x1, int y1,
              const pdf14_nonseparable_blending_procs_t *pblend_procs, pdf14_device *pdev)
{
    template_compose_group(tos_ptr, tos_isolated, tos_planestride, tos_rowstride, alpha, shape, blend_mode, tos_has_shape,
        tos_shape_offset, tos_alpha_g_offset, tos_tag_offset, tos_has_tag, tos_alpha_g_ptr,
        nos_ptr, nos_isolated, nos_planestride, nos_rowstride, nos_alpha_g_ptr, /* nos_knockout = */1,
        nos_shape_offset, nos_tag_offset, /* mask_row_ptr */ NULL, /* has_mask */ 0, /* maskbuf */ NULL, mask_bg_alpha, /* mask_tr_fn */ NULL,
        backdrop_ptr, /* has_matte */ false , n_chan, additive, num_spots, overprint, drawn_comps, x0, y0, x1, y1, pblend_procs, pdev, 0);
}

static void
compose_group_alphaless_nonknockout(byte *tos_ptr, bool tos_isolated, int tos_planestride, int tos_rowstride, byte alpha, byte shape, gs_blend_mode_t blend_mode, bool tos_has_shape,
              int tos_shape_offset, int tos_alpha_g_offset, int tos_tag_offset, bool tos_has_tag, byte *tos_alpha_g_ptr,
              byte *nos_ptr, bool nos_isolated, int nos_planestride, int nos_rowstride, byte *nos_alpha_g_ptr, bool nos_knockout,
              int nos_shape_offset, int nos_tag_offset,
              byte *mask_row_ptr, int has_mask, pdf14_buf *maskbuf, byte mask_bg_alpha, const byte *mask_tr_fn,
              byte *backdrop_ptr,
              bool has_matte, int n_chan, bool additive, int num_spots, bool overprint, gx_color_index drawn_comps, int x0, int y0, int x1, int y1,
              const pdf14_nonseparable_blending_procs_t *pblend_procs, pdf14_device *pdev)
{
    template_compose_group(tos_ptr, tos_isolated, tos_planestride, tos_rowstride, alpha, shape, blend_mode, tos_has_shape,
        tos_shape_offset, tos_alpha_g_offset, tos_tag_offset, tos_has_tag, tos_alpha_g_ptr,
        nos_ptr, nos_isolated, nos_planestride, nos_rowstride, nos_alpha_g_ptr, /* nos_knockout = */0,
        nos_shape_offset, nos_tag_offset, /* mask_row_ptr */ NULL, /* has_mask */ 0, /* maskbuf */ NULL, mask_bg_alpha, /* mask_tr_fn */ NULL,
        backdrop_ptr, /* has_matte */ false , n_chan, additive, num_spots, overprint, drawn_comps, x0, y0, x1, y1, pblend_procs, pdev, 0);
}

static void
do_compose_group(pdf14_buf *tos, pdf14_buf *nos, pdf14_buf *maskbuf,
              int x0, int x1, int y0, int y1, int n_chan, bool additive,
              const pdf14_nonseparable_blending_procs_t * pblend_procs,
              bool has_matte, bool overprint, gx_color_index drawn_comps,
              gs_memory_t *memory, gx_device *dev)
{
    int num_spots = tos->num_spots;
    byte alpha = tos->alpha>>8;
    byte shape = tos->shape>>8;
    gs_blend_mode_t blend_mode = tos->blend_mode;
    byte *tos_ptr = tos->data + x0 - tos->rect.p.x +
        (y0 - tos->rect.p.y) * tos->rowstride;
    byte *nos_ptr = nos->data + x0 - nos->rect.p.x +
        (y0 - nos->rect.p.y) * nos->rowstride;
    byte *mask_row_ptr = NULL;
    int tos_planestride = tos->planestride;
    int nos_planestride = nos->planestride;
    byte mask_bg_alpha = 0; /* Quiet compiler. */
    bool tos_isolated = tos->isolated;
    bool nos_isolated = nos->isolated;
    bool nos_knockout = nos->knockout;
    byte *nos_alpha_g_ptr;
    byte *tos_alpha_g_ptr;
    int tos_shape_offset = n_chan * tos_planestride;
    int tos_alpha_g_offset = tos_shape_offset + (tos->has_shape ? tos_planestride : 0);
    bool tos_has_tag = tos->has_tags;
    int tos_tag_offset = tos_planestride * (tos->n_planes - 1);
    int nos_shape_offset = n_chan * nos_planestride;
    int nos_alpha_g_offset = nos_shape_offset + (nos->has_shape ? nos_planestride : 0);
    int nos_tag_offset = nos_planestride * (nos->n_planes - 1);
    byte *mask_tr_fn = NULL; /* Quiet compiler. */
    bool is_ident = true;
    bool has_mask = false;
    byte *backdrop_ptr = NULL;
    pdf14_device *pdev = (pdf14_device *)dev;


#if RAW_DUMP
    byte *composed_ptr = NULL;
    int width = x1 - x0;
#endif
    art_pdf_compose_group_fn fn;

    if ((tos->n_chan == 0) || (nos->n_chan == 0))
        return;
    rect_merge(nos->dirty, tos->dirty);
    if (nos->has_tags)
        if_debug7m('v', memory,
                   "pdf14_pop_transparency_group y0 = %d, y1 = %d, w = %d, alpha = %d, shape = %d, tag = %d, bm = %d\n",
                   y0, y1, x1 - x0, alpha, shape, dev->graphics_type_tag & ~GS_DEVICE_ENCODES_TAGS, blend_mode);
    else
        if_debug6m('v', memory,
                   "pdf14_pop_transparency_group y0 = %d, y1 = %d, w = %d, alpha = %d, shape = %d, bm = %d\n",
                   y0, y1, x1 - x0, alpha, shape, blend_mode);
    if (!nos->has_shape)
        nos_shape_offset = 0;
    if (!nos->has_tags)
        nos_tag_offset = 0;
    if (nos->has_alpha_g) {
        nos_alpha_g_ptr = nos_ptr + nos_alpha_g_offset;
    } else
        nos_alpha_g_ptr = NULL;
    if (tos->has_alpha_g) {
        tos_alpha_g_ptr = tos_ptr + tos_alpha_g_offset;
    } else
        tos_alpha_g_ptr = NULL;
    if (nos->backdrop != NULL) {
        backdrop_ptr = nos->backdrop + x0 - nos->rect.p.x +
                       (y0 - nos->rect.p.y) * nos->rowstride;
    }
    if (blend_mode != BLEND_MODE_Compatible && blend_mode != BLEND_MODE_Normal)
        overprint = false;

    if (maskbuf != NULL) {
        int tmp;

        mask_tr_fn = maskbuf->transfer_fn;

        is_ident = maskbuf->is_ident;
        /* Make sure we are in the mask buffer */
        if (maskbuf->data != NULL) {
            mask_row_ptr = maskbuf->data + x0 - maskbuf->rect.p.x +
                    (y0 - maskbuf->rect.p.y) * maskbuf->rowstride;
            has_mask = true;
        }
        /* We may have a case, where we are outside the maskbuf rect. */
        /* We would have avoided creating the maskbuf->data */
        /* In that case, we should use the background alpha value */
        /* See discussion on the BC entry in the PDF spec.   */
        mask_bg_alpha = maskbuf->alpha>>8;
        /* Adjust alpha by the mask background alpha.   This is only used
           if we are outside the soft mask rect during the filling operation */
        mask_bg_alpha = mask_tr_fn[mask_bg_alpha];
        tmp = alpha * mask_bg_alpha + 0x80;
        mask_bg_alpha = (tmp + (tmp >> 8)) >> 8;
    }
    n_chan--; /* Now the true number of colorants (i.e. not including alpha)*/
#if RAW_DUMP
    composed_ptr = nos_ptr;
    dump_raw_buffer(memory, y1-y0, width, tos->n_planes, tos_planestride, tos->rowstride,
                    "bImageTOS", tos_ptr, tos->deep);
    dump_raw_buffer(memory, y1-y0, width, nos->n_planes, nos_planestride, nos->rowstride,
                    "cImageNOS", nos_ptr, tos->deep);
    if (maskbuf !=NULL && maskbuf->data != NULL) {
        dump_raw_buffer(memory, maskbuf->rect.q.y - maskbuf->rect.p.y,
                        maskbuf->rect.q.x - maskbuf->rect.p.x, maskbuf->n_planes,
                        maskbuf->planestride, maskbuf->rowstride, "dMask",
                        maskbuf->data, maskbuf->deep);
    }
#endif

    /* You might hope that has_mask iff maskbuf != NULL, but this is
     * not the case. Certainly we can see cases where maskbuf != NULL
     * and has_mask = 0. What's more, treating such cases as being
     * has_mask = 0 causes diffs. */
#ifdef TRACK_COMPOSE_GROUPS
    {
        int code = 0;

        code += !!nos_knockout;
        code += (!!nos_isolated)<<1;
        code += (!!tos_isolated)<<2;
        code += (!!tos->has_shape)<<3;
        code += (!!tos_has_tag)<<4;
        code += (!!additive)<<5;
        code += (!!overprint)<<6;
        code += (!!has_mask || maskbuf != NULL)<<7;
        code += (!!has_matte)<<8;
        code += (backdrop_ptr != NULL)<<9;
        code += (num_spots != 0)<<10;
        code += blend_mode<<11;

        if (track_compose_groups == 0)
        {
            atexit(dump_track_compose_groups);
            track_compose_groups = 1;
        }
        compose_groups[code]++;
    }
#endif

    /* We have tested the files on the cluster to see what percentage of
     * files/devices hit the different options. */
    if (nos_knockout)
        fn = &compose_group_knockout; /* Small %ages, nothing more than 1.1% */
    else if (blend_mode != 0)
        fn = &compose_group_nonknockout_blend; /* Small %ages, nothing more than 2% */
    else if (tos->has_shape == 0 && tos_has_tag == 0 && nos_isolated == 0 && nos_alpha_g_ptr == NULL &&
             nos_shape_offset == 0 && nos_tag_offset == 0 && backdrop_ptr == NULL && has_matte == 0 && num_spots == 0 &&
             overprint == 0 && tos_alpha_g_ptr == NULL) {
             /* Additive vs Subtractive makes no difference in normal blend mode with no spots */
        if (tos_isolated) {
            if (has_mask && maskbuf) {/* 7% */
                /* AirPrint test case hits this */
                if (maskbuf && maskbuf->rect.p.x <= x0 && maskbuf->rect.p.y <= y0 &&
                    maskbuf->rect.q.x >= x1 && maskbuf->rect.q.y >= y1) {
                    /* AVX and SSE accelerations only valid if maskbuf transfer
                       function is identity and we have no matte color replacement */
                    if (is_ident && !has_matte) {
                        fn = compose_group_nonknockout_nonblend_isolated_allmask_common;
#ifdef WITH_CAL
			fn = (art_pdf_compose_group_fn)cal_get_compose_group(
					 memory->gs_lib_ctx->core->cal_ctx,
					 (cal_composer_proc_t *)fn,
					 tos->n_chan-1);
#endif
                    } else {
                        fn = compose_group_nonknockout_nonblend_isolated_allmask_common;
                    }
                } else
                    fn = &compose_group_nonknockout_nonblend_isolated_mask_common;
            } else
                if (maskbuf) {
                    /* Outside mask */
                    fn = &compose_group_nonknockout_nonblend_isolated_mask_common;
                } else
                    fn = &compose_group_nonknockout_nonblend_isolated_nomask_common;
        } else {
            if (has_mask || maskbuf) /* 4% */
                fn = &compose_group_nonknockout_nonblend_nonisolated_mask_common;
            else /* 15% */
                fn = &compose_group_nonknockout_nonblend_nonisolated_nomask_common;
        }
    } else
        fn = compose_group_nonknockout_noblend_general;

    fn(tos_ptr, tos_isolated, tos_planestride, tos->rowstride, alpha, shape,
        blend_mode, tos->has_shape, tos_shape_offset, tos_alpha_g_offset,
        tos_tag_offset, tos_has_tag, tos_alpha_g_ptr, nos_ptr, nos_isolated, nos_planestride,
        nos->rowstride, nos_alpha_g_ptr, nos_knockout, nos_shape_offset,
        nos_tag_offset, mask_row_ptr, has_mask, maskbuf, mask_bg_alpha,
        mask_tr_fn, backdrop_ptr, has_matte, n_chan, additive, num_spots,
        overprint, drawn_comps, x0, y0, x1, y1, pblend_procs, pdev);

#if RAW_DUMP
    dump_raw_buffer(memory, y1-y0, width, nos->n_planes, nos_planestride, nos->rowstride,
                    "eComposed", composed_ptr, nos->deep);
    global_index++;
#endif
}

static inline uint16_t
interp16(const uint16_t *table, uint16_t idx)
{
    byte     top = idx>>8;
    uint16_t a   = table[top];
    int      b   = table[top+1]-a;

    return a + ((0x80 + b*(idx & 0xff))>>8);
}

typedef void (*art_pdf_compose_group16_fn)(uint16_t *tos_ptr, bool tos_isolated, int tos_planestride, int tos_rowstride,
                                         uint16_t alpha, uint16_t shape, gs_blend_mode_t blend_mode, bool tos_has_shape,
                                         int tos_shape_offset, int tos_alpha_g_offset, int tos_tag_offset, bool tos_has_tag,
                                         uint16_t *tos_alpha_g_ptr,
                                         uint16_t *nos_ptr, bool nos_isolated, int nos_planestride, int nos_rowstride,
                                         uint16_t *nos_alpha_g_ptr, bool nos_knockout, int nos_shape_offset, int nos_tag_offset,
                                         uint16_t *mask_row_ptr, int has_mask, pdf14_buf *maskbuf, uint16_t mask_bg_alpha, const uint16_t *mask_tr_fn,
                                         uint16_t *backdrop_ptr, bool has_matte, int n_chan, bool additive, int num_spots, bool overprint,
                                         gx_color_index drawn_comps, int x0, int y0, int x1, int y1,
                                         const pdf14_nonseparable_blending_procs_t *pblend_procs, pdf14_device *pdev);

static forceinline void
template_compose_group16(uint16_t *gs_restrict tos_ptr, bool tos_isolated,
                         int tos_planestride, int tos_rowstride,
                         uint16_t alpha, uint16_t shape, gs_blend_mode_t blend_mode,
                         bool tos_has_shape, int tos_shape_offset,
                         int tos_alpha_g_offset, int tos_tag_offset,
                         bool tos_has_tag,  uint16_t *gs_restrict tos_alpha_g_ptr,
                         uint16_t *gs_restrict nos_ptr,
                         bool nos_isolated, int nos_planestride,
                         int nos_rowstride, uint16_t *gs_restrict nos_alpha_g_ptr,
                         bool nos_knockout, int nos_shape_offset,
                         int nos_tag_offset, uint16_t *gs_restrict mask_row_ptr,
                         int has_mask, pdf14_buf *gs_restrict maskbuf,
                         uint16_t mask_bg_alpha, const uint16_t *gs_restrict mask_tr_fn,
                         uint16_t *gs_restrict backdrop_ptr, bool has_matte,
                         int n_chan, bool additive, int num_spots,
                         bool overprint, gx_color_index drawn_comps,
                         int x0, int y0, int x1, int y1,
                         const pdf14_nonseparable_blending_procs_t *pblend_procs,
                         pdf14_device *pdev, int has_alpha, bool tos_is_be)
{
    uint16_t *gs_restrict mask_curr_ptr = NULL;
    int width = x1 - x0;
    int x, y;
    int i;
    uint16_t tos_pixel[PDF14_MAX_PLANES];
    uint16_t nos_pixel[PDF14_MAX_PLANES];
    uint16_t back_drop[PDF14_MAX_PLANES];
    bool in_mask_rect_y;
    bool in_mask_rect;
    uint16_t pix_alpha;
    uint16_t matte_alpha = 0xffff;
    int first_spot = n_chan - num_spots;
    int first_blend_spot = n_chan;
    bool has_mask2 = has_mask;
    uint16_t *gs_restrict dst;
    uint16_t group_shape = (uint16_t)(65535 * pdev->shape + 0.5);

    if (!nos_knockout && num_spots > 0 && !blend_valid_for_spot(blend_mode)) {
        first_blend_spot = first_spot;
    }
    if (blend_mode == BLEND_MODE_Normal)
        first_blend_spot = 0;
    if (!nos_isolated && backdrop_ptr != NULL)
        has_mask2 = false;

/* TOS data being passed to this routine is usually in native
 * endian format (i.e. if it's from another pdf14 buffer). Occasionally,
 * if it's being passed in from pdf_compose_alphaless_group16, it can be
 * from memory produced by another memory device (such as a pattern
 * cache device). That data is in big endian form. So we have a crufty
 * macro to get 16 bits of data from either native or bigendian into
 * a native value. This should resolve nicely at compile time. */
#define GET16_2NATIVE(be, v) \
    ((be) ? ((((byte *)&v)[0]<<8) | (((byte *)&v)[1])) : v)

    for (y = y1 - y0; y > 0; --y) {
        mask_curr_ptr = mask_row_ptr;
        in_mask_rect_y = (has_mask && y1 - y >= maskbuf->rect.p.y && y1 - y < maskbuf->rect.q.y);
        for (x = 0; x < width; x++) {
            in_mask_rect = (in_mask_rect_y && x0 + x >= maskbuf->rect.p.x && x0 + x < maskbuf->rect.q.x);
            pix_alpha = alpha;
            /* If we have a soft mask, then we have some special handling of the
               group alpha value */
            if (maskbuf != NULL) {
                if (!in_mask_rect) {
                    /* Special case where we have a soft mask but are outside
                       the range of the soft mask and must use the background
                       alpha value */
                    pix_alpha = mask_bg_alpha;
                    matte_alpha = 0xffff;
                } else {
                    if (has_matte)
                        matte_alpha = interp16(mask_tr_fn, *mask_curr_ptr);
                }
            }

            /* Matte present, need to undo premultiplied alpha prior to blend */
            if (has_matte && matte_alpha != 0 && matte_alpha != 0xffff) {
                for (i = 0; i < n_chan; i++) {
                    /* undo */
                    int val = GET16_2NATIVE(tos_is_be, tos_ptr[i * tos_planestride]) - maskbuf->matte[i];
                    int temp = (((unsigned int)(val * 0xffff)) / matte_alpha) + maskbuf->matte[i];

                    /* clip */
                    if (temp > 0xffff)
                        tos_pixel[i] = 0xffff;
                    else if (temp < 0)
                        tos_pixel[i] = 0;
                    else
                        tos_pixel[i] = temp;

                    if (!additive) {
                        /* Pure subtractive */
                        tos_pixel[i] = 65535 - tos_pixel[i];
                        nos_pixel[i] = 65535 - nos_ptr[i * nos_planestride];
                    } else {
                        /* additive or hybrid */
                        if (i >= first_spot)
                            nos_pixel[i] = 65535 - nos_ptr[i * nos_planestride];
                        else
                            nos_pixel[i] = nos_ptr[i * nos_planestride];
                    }
                }
            } else {
                /* No matte present */
                if (!additive) {
                    /* Pure subtractive */
                    for (i = 0; i < n_chan; ++i) {
                        tos_pixel[i] = 65535 - GET16_2NATIVE(tos_is_be, tos_ptr[i * tos_planestride]);
                        nos_pixel[i] = 65535 - nos_ptr[i * nos_planestride];
                    }
                } else {
                    /* Additive or hybrid */
                    for (i = 0; i < first_spot; ++i) {
                        tos_pixel[i] = GET16_2NATIVE(tos_is_be, tos_ptr[i * tos_planestride]);
                        nos_pixel[i] = nos_ptr[i * nos_planestride];
                    }
                    for (; i < n_chan; i++) {
                        tos_pixel[i] = 65535 - GET16_2NATIVE(tos_is_be, tos_ptr[i * tos_planestride]);
                        nos_pixel[i] = 65535 - nos_ptr[i * nos_planestride];
                    }
                }
            }
            /* alpha */
            tos_pixel[n_chan] = has_alpha ? GET16_2NATIVE(tos_is_be, tos_ptr[n_chan * tos_planestride]) : 65535;
            nos_pixel[n_chan] = has_alpha ? nos_ptr[n_chan * nos_planestride] : 65535;

            if (mask_curr_ptr != NULL) {
                if (in_mask_rect) {
                    uint16_t mask = interp16(mask_tr_fn, *mask_curr_ptr++);
                    int tmp = pix_alpha * (mask+(mask>>15)) + 0x8000;
                    pix_alpha = (tmp >> 16);
                } else {
                    mask_curr_ptr++;
                }
            }

            dst = nos_pixel;
            if (nos_knockout) {
                /* We need to be knocking out what ever is on the nos, but may
                   need to combine with it's backdrop */
                uint16_t tos_shape = 65535;

                if (tos_has_shape)
                    tos_shape = GET16_2NATIVE(tos_is_be, tos_ptr[tos_shape_offset]);

                if (nos_isolated || backdrop_ptr == NULL) {
                    /* We do not need to compose with the backdrop */
                    back_drop[n_chan] = 0;
                    /* FIXME: The blend here can be simplified */
                } else {
                    /* Per the PDF spec, since the tos is not isolated and we are
                       going onto a knock out group, we do the composition with
                       the nos initial backdrop. */
                    if (additive) {
                        /* additive or hybrid */
                        for (i = 0; i < first_spot; ++i) {
                            back_drop[i] = backdrop_ptr[i * nos_planestride];
                        }
                        for (; i < n_chan; i++) {
                            back_drop[i] = 65535 - backdrop_ptr[i * nos_planestride];
                        }
                    } else {
                        /* pure subtractive */
                        for (i = 0; i < n_chan; ++i) {
                            back_drop[i] = 65535 - backdrop_ptr[i * nos_planestride];
                        }
                    }
                    /* alpha */
                    back_drop[n_chan] = backdrop_ptr[n_chan * nos_planestride];
                }

                if (tos_isolated ?
                    art_pdf_ko_composite_group_16(tos_shape, tos_alpha_g_ptr,
                        nos_pixel, nos_alpha_g_ptr,
                        tos_pixel, n_chan, pix_alpha,
                        has_mask2) :
                    art_pdf_ko_recomposite_group_16(tos_shape, has_alpha ? tos_ptr[tos_alpha_g_offset] : 65535,
                        &dst, nos_alpha_g_ptr, tos_pixel, n_chan, pix_alpha,
                        blend_mode, has_mask2)) {
                    dst = art_pdf_knockout_composite_pixel_alpha_16(back_drop, tos_shape, nos_pixel, tos_pixel,
                        n_chan, blend_mode, pblend_procs, pdev);
                }
            }
            else if (tos_isolated ?
                       art_pdf_composite_group_16(nos_pixel, nos_alpha_g_ptr,
                                                  tos_pixel, n_chan, pix_alpha) :
                       art_pdf_recomposite_group_16(&dst, nos_alpha_g_ptr,
                                                    tos_pixel,
                                                    has_alpha ? GET16_2NATIVE(tos_is_be, tos_ptr[tos_alpha_g_offset]) : 65535,
                                                    n_chan,
                                                    pix_alpha, blend_mode)) {
                dst = art_pdf_composite_pixel_alpha_16_inline(nos_pixel, tos_pixel, n_chan,
                                                blend_mode, first_blend_spot,
                                                pblend_procs, pdev);
            }
            if (nos_shape_offset && pix_alpha != 0) {
                nos_ptr[nos_shape_offset] =
                    art_pdf_union_mul_16(nos_ptr[nos_shape_offset],
                                         has_alpha ? GET16_2NATIVE(tos_is_be, tos_ptr[tos_shape_offset]) : group_shape,
                                         shape);
            }
            if (dst)
            {
                /* Complement the results for subtractive color spaces.  Again,
                 * if we are in an additive blending color space, we are not
                 * going to be fooling with overprint of spot colors */
                if (additive) {
                    /* additive or hybrid */
                    for (i = 0; i < first_spot; ++i) {
                        nos_ptr[i * nos_planestride] = dst[i];
                    }
                    for (; i < n_chan; i++) {
                        nos_ptr[i * nos_planestride] = 65535 - dst[i];
                    }
                } else {
                    /* Pure subtractive */
                    for (i = 0; i < n_chan; ++i)
                        nos_ptr[i * nos_planestride] = 65535 - dst[i];
                }
                /* alpha */
                nos_ptr[n_chan * nos_planestride] = dst[n_chan];
            }
            /* tags */
            if (nos_tag_offset && tos_has_tag) {
                nos_ptr[nos_tag_offset] |= tos_ptr[tos_tag_offset];
             }

            if (nos_alpha_g_ptr != NULL)
                ++nos_alpha_g_ptr;
            if (tos_alpha_g_ptr != NULL)
                ++tos_alpha_g_ptr;
            if (backdrop_ptr != NULL)
                ++backdrop_ptr;
            ++tos_ptr;
            ++nos_ptr;
        }
        tos_ptr += tos_rowstride - width;
        nos_ptr += nos_rowstride - width;
        if (tos_alpha_g_ptr != NULL)
            tos_alpha_g_ptr += tos_rowstride - width;
        if (nos_alpha_g_ptr != NULL)
            nos_alpha_g_ptr += nos_rowstride - width;
        if (mask_row_ptr != NULL)
            mask_row_ptr += maskbuf->rowstride>>1;
        if (backdrop_ptr != NULL)
            backdrop_ptr += nos_rowstride - width;
    }
}

static void
compose_group16_knockout(uint16_t *tos_ptr, bool tos_isolated, int tos_planestride, int tos_rowstride,
              uint16_t alpha, uint16_t shape, gs_blend_mode_t blend_mode, bool tos_has_shape, int tos_shape_offset,
              int tos_alpha_g_offset, int tos_tag_offset, bool tos_has_tag, uint16_t *tos_alpha_g_ptr,
              uint16_t *nos_ptr, bool nos_isolated, int nos_planestride, int nos_rowstride, uint16_t *nos_alpha_g_ptr, bool nos_knockout,
              int nos_shape_offset, int nos_tag_offset,
              uint16_t *mask_row_ptr, int has_mask, pdf14_buf *maskbuf, uint16_t mask_bg_alpha, const uint16_t *mask_tr_fn,
              uint16_t *backdrop_ptr,
              bool has_matte, int n_chan, bool additive, int num_spots, bool overprint, gx_color_index drawn_comps, int x0, int y0, int x1, int y1,
              const pdf14_nonseparable_blending_procs_t *pblend_procs, pdf14_device *pdev)
{
    template_compose_group16(tos_ptr, tos_isolated, tos_planestride, tos_rowstride, alpha, shape, blend_mode, tos_has_shape,
        tos_shape_offset, tos_alpha_g_offset, tos_tag_offset, tos_has_tag, tos_alpha_g_ptr,
        nos_ptr, nos_isolated, nos_planestride, nos_rowstride, nos_alpha_g_ptr, /* nos_knockout = */1,
        nos_shape_offset, nos_tag_offset, mask_row_ptr, has_mask, maskbuf, mask_bg_alpha, mask_tr_fn,
        backdrop_ptr, has_matte, n_chan, additive, num_spots, overprint, drawn_comps, x0, y0, x1, y1, pblend_procs, pdev, 1, 0);
}

static void
compose_group16_nonknockout_blend(uint16_t *tos_ptr, bool tos_isolated, int tos_planestride, int tos_rowstride,
              uint16_t alpha, uint16_t shape, gs_blend_mode_t blend_mode, bool tos_has_shape, int tos_shape_offset, int tos_alpha_g_offset,
              int tos_tag_offset, bool tos_has_tag, uint16_t *tos_alpha_g_ptr, uint16_t *nos_ptr, bool nos_isolated, int nos_planestride,
              int nos_rowstride, uint16_t *nos_alpha_g_ptr, bool nos_knockout, int nos_shape_offset, int nos_tag_offset,
              uint16_t *mask_row_ptr, int has_mask, pdf14_buf *maskbuf, uint16_t mask_bg_alpha, const uint16_t *mask_tr_fn, uint16_t *backdrop_ptr,
              bool has_matte, int n_chan, bool additive, int num_spots, bool overprint, gx_color_index drawn_comps, int x0, int y0, int x1, int y1,
              const pdf14_nonseparable_blending_procs_t *pblend_procs, pdf14_device *pdev)
{
    template_compose_group16(tos_ptr, tos_isolated, tos_planestride, tos_rowstride,
        alpha, shape, blend_mode, tos_has_shape,
        tos_shape_offset, tos_alpha_g_offset, tos_tag_offset, tos_has_tag,
        tos_alpha_g_ptr, nos_ptr, nos_isolated, nos_planestride, nos_rowstride, nos_alpha_g_ptr, /* nos_knockout = */0,
        nos_shape_offset, nos_tag_offset, mask_row_ptr, has_mask, maskbuf, mask_bg_alpha, mask_tr_fn,
        backdrop_ptr, has_matte, n_chan, additive, num_spots, overprint, drawn_comps, x0, y0, x1, y1, pblend_procs, pdev, 1, 0);
}

static void
compose_group16_nonknockout_nonblend_isolated_allmask_common(uint16_t *tos_ptr, bool tos_isolated, int tos_planestride, int tos_rowstride,
              uint16_t alpha, uint16_t shape, gs_blend_mode_t blend_mode, bool tos_has_shape, int tos_shape_offset, int tos_alpha_g_offset,
              int tos_tag_offset, bool tos_has_tag, uint16_t *tos_alpha_g_ptr, uint16_t *nos_ptr, bool nos_isolated, int nos_planestride,
              int nos_rowstride, uint16_t *nos_alpha_g_ptr, bool nos_knockout, int nos_shape_offset, int nos_tag_offset,
              uint16_t *mask_row_ptr, int has_mask, pdf14_buf *maskbuf, uint16_t mask_bg_alpha, const uint16_t *mask_tr_fn, uint16_t *backdrop_ptr,
              bool has_matte, int n_chan, bool additive, int num_spots, bool overprint, gx_color_index drawn_comps, int x0, int y0, int x1, int y1,
              const pdf14_nonseparable_blending_procs_t *pblend_procs, pdf14_device *pdev)
{
    int width = x1 - x0;
    int x, y;
    int i;

    for (y = y1 - y0; y > 0; --y) {
        uint16_t *gs_restrict mask_curr_ptr = mask_row_ptr;
        for (x = 0; x < width; x++) {
            unsigned int mask = interp16(mask_tr_fn, *mask_curr_ptr++);
            uint16_t src_alpha = tos_ptr[n_chan * tos_planestride];
            if (src_alpha != 0) {
                uint16_t a_b;
                unsigned int pix_alpha;

                mask += mask>>15;
                pix_alpha = (alpha * mask + 0x8000)>>16;

                if (pix_alpha != 0xffff) {
                    pix_alpha += pix_alpha>>15;
                    src_alpha = (src_alpha * pix_alpha + 0x8000)>>16;
                }

                a_b = nos_ptr[n_chan * nos_planestride];
                if (a_b == 0) {
                    /* Simple copy of colors plus alpha. */
                    for (i = 0; i < n_chan; i++) {
                        nos_ptr[i * nos_planestride] = tos_ptr[i * tos_planestride];
                    }
                    nos_ptr[i * nos_planestride] = src_alpha;
                } else {
                    unsigned int a_r;
                    int src_scale;
                    unsigned int tmp;

                    /* Result alpha is Union of backdrop and source alpha */
                    tmp = (0xffff - a_b) * (0xffff - src_alpha) + 0x8000;
                    tmp += tmp>>16;
                    a_r = 0xffff - (tmp >> 16);

                    /* Compute src_alpha / a_r in 16.16 format */
                    src_scale = ((src_alpha << 16) + (a_r >> 1)) / a_r;

                    nos_ptr[n_chan * nos_planestride] = a_r;

                    src_scale >>= 1; /* Will overflow unless we lose a bit */
                    /* Do simple compositing of source over backdrop */
                    for (i = 0; i < n_chan; i++) {
                        int c_s = tos_ptr[i * tos_planestride];
                        int c_b = nos_ptr[i * nos_planestride];
                        nos_ptr[i * nos_planestride] = c_b + ((src_scale * (c_s - c_b) + 0x4000) >> 15);
                    }
                }
            }
            ++tos_ptr;
            ++nos_ptr;
        }
        tos_ptr += tos_rowstride - width;
        nos_ptr += nos_rowstride - width;
        mask_row_ptr += maskbuf->rowstride>>1;
    }
}

static void
compose_group16_nonknockout_nonblend_isolated_mask_common(uint16_t *tos_ptr, bool tos_isolated, int tos_planestride, int tos_rowstride,
              uint16_t alpha, uint16_t shape, gs_blend_mode_t blend_mode, bool tos_has_shape, int tos_shape_offset, int tos_alpha_g_offset,
              int tos_tag_offset, bool tos_has_tag, uint16_t *tos_alpha_g_ptr, uint16_t *nos_ptr, bool nos_isolated, int nos_planestride,
              int nos_rowstride, uint16_t *nos_alpha_g_ptr, bool nos_knockout, int nos_shape_offset, int nos_tag_offset,
              uint16_t *mask_row_ptr, int has_mask, pdf14_buf *maskbuf, uint16_t mask_bg_alpha, const uint16_t *mask_tr_fn, uint16_t *backdrop_ptr,
              bool has_matte, int n_chan, bool additive, int num_spots, bool overprint, gx_color_index drawn_comps, int x0, int y0, int x1, int y1,
              const pdf14_nonseparable_blending_procs_t *pblend_procs, pdf14_device *pdev)
{
    uint16_t *gs_restrict mask_curr_ptr = NULL;
    int width = x1 - x0;
    int x, y;
    int i;
    bool in_mask_rect_y;
    bool in_mask_rect;
    uint16_t pix_alpha, src_alpha;

    for (y = y1 - y0; y > 0; --y) {
        mask_curr_ptr = mask_row_ptr;
        in_mask_rect_y = (has_mask && y1 - y >= maskbuf->rect.p.y && y1 - y < maskbuf->rect.q.y);
        for (x = 0; x < width; x++) {
            in_mask_rect = (in_mask_rect_y && has_mask && x0 + x >= maskbuf->rect.p.x && x0 + x < maskbuf->rect.q.x);
            pix_alpha = alpha;
            /* If we have a soft mask, then we have some special handling of the
               group alpha value */
            if (maskbuf != NULL) {
                if (!in_mask_rect) {
                    /* Special case where we have a soft mask but are outside
                       the range of the soft mask and must use the background
                       alpha value */
                    pix_alpha = mask_bg_alpha;
                }
            }

            if (mask_curr_ptr != NULL) {
                if (in_mask_rect) {
                    unsigned int mask = interp16(mask_tr_fn, *mask_curr_ptr++);
                    mask += mask>>15;
                    pix_alpha = (pix_alpha * mask + 0x8000)>>16;
                } else {
                    mask_curr_ptr++;
                }
            }

            src_alpha = tos_ptr[n_chan * tos_planestride];
            if (src_alpha != 0) {
                uint16_t a_b;

                if (pix_alpha != 65535) {
                    pix_alpha += pix_alpha>>15;
                    src_alpha = (src_alpha * pix_alpha + 0x8000)>>16;
                }

                a_b = nos_ptr[n_chan * nos_planestride];
                if (a_b == 0) {
                    /* Simple copy of colors plus alpha. */
                    for (i = 0; i < n_chan; i++) {
                        nos_ptr[i * nos_planestride] = tos_ptr[i * tos_planestride];
                    }
                    nos_ptr[i * nos_planestride] = src_alpha;
                } else {
                    unsigned int a_r;
                    int src_scale;
                    unsigned int tmp;

                    /* Result alpha is Union of backdrop and source alpha */
                    tmp = (0xffff - a_b) * (0xffff - src_alpha) + 0x8000;
                    tmp += tmp>>16;
                    a_r = 0xffff - (tmp >> 16);

                    /* Compute src_alpha / a_r in 16.16 format */
                    src_scale = ((src_alpha << 16) + (a_r >> 1)) / a_r;

                    nos_ptr[n_chan * nos_planestride] = a_r;

                    src_scale >>= 1; /* Need to lose a bit to avoid overflow */
                    /* Do simple compositing of source over backdrop */
                    for (i = 0; i < n_chan; i++) {
                        int c_s = tos_ptr[i * tos_planestride];
                        int c_b = nos_ptr[i * nos_planestride];
                        nos_ptr[i * nos_planestride] = c_b + ((src_scale * (c_s - c_b) + 0x4000) >> 15);
                    }
                }
            }
            ++tos_ptr;
            ++nos_ptr;
        }
        tos_ptr += tos_rowstride - width;
        nos_ptr += nos_rowstride - width;
        if (mask_row_ptr != NULL)
            mask_row_ptr += maskbuf->rowstride>>1;
    }
}

static void
compose_group16_nonknockout_nonblend_isolated_nomask_common(uint16_t *tos_ptr, bool tos_isolated, int tos_planestride, int tos_rowstride,
              uint16_t alpha, uint16_t shape, gs_blend_mode_t blend_mode, bool tos_has_shape, int tos_shape_offset, int tos_alpha_g_offset,
              int tos_tag_offset, bool tos_has_tag, uint16_t *tos_alpha_g_ptr, uint16_t *nos_ptr, bool nos_isolated, int nos_planestride,
              int nos_rowstride, uint16_t *nos_alpha_g_ptr, bool nos_knockout, int nos_shape_offset, int nos_tag_offset,
              uint16_t *mask_row_ptr, int has_mask, pdf14_buf *maskbuf, uint16_t mask_bg_alpha, const uint16_t *mask_tr_fn, uint16_t *backdrop_ptr,
              bool has_matte, int n_chan, bool additive, int num_spots, bool overprint, gx_color_index drawn_comps, int x0, int y0, int x1, int y1,
              const pdf14_nonseparable_blending_procs_t *pblend_procs, pdf14_device *pdev)
{
    template_compose_group16(tos_ptr, /*tos_isolated*/1, tos_planestride, tos_rowstride, alpha, shape, BLEND_MODE_Normal, /*tos_has_shape*/0,
        tos_shape_offset, tos_alpha_g_offset, tos_tag_offset, /*tos_has_tag*/0, /*tos_alpha_g_ptr*/ 0,
        nos_ptr, /*nos_isolated*/0, nos_planestride, nos_rowstride, /*nos_alpha_g_ptr*/0, /* nos_knockout = */0,
        /*nos_shape_offset*/0, /*nos_tag_offset*/0, mask_row_ptr, /*has_mask*/0, /*maskbuf*/NULL, mask_bg_alpha, mask_tr_fn,
        backdrop_ptr, /*has_matte*/0, n_chan, /*additive*/1, /*num_spots*/0, /*overprint*/0, /*drawn_comps*/0, x0, y0, x1, y1, pblend_procs, pdev, 1, 0);
}

static void
compose_group16_nonknockout_nonblend_nonisolated_mask_common(uint16_t *tos_ptr, bool tos_isolated, int tos_planestride, int tos_rowstride,
              uint16_t alpha, uint16_t shape, gs_blend_mode_t blend_mode, bool tos_has_shape, int tos_shape_offset, int tos_alpha_g_offset,
              int tos_tag_offset, bool tos_has_tag, uint16_t *tos_alpha_g_ptr, uint16_t *nos_ptr, bool nos_isolated, int nos_planestride,
              int nos_rowstride, uint16_t *nos_alpha_g_ptr, bool nos_knockout, int nos_shape_offset, int nos_tag_offset,
              uint16_t *mask_row_ptr, int has_mask, pdf14_buf *maskbuf, uint16_t mask_bg_alpha, const uint16_t *mask_tr_fn,
              uint16_t *backdrop_ptr,
              bool has_matte, int n_chan, bool additive, int num_spots, bool overprint, gx_color_index drawn_comps, int x0, int y0, int x1, int y1,
              const pdf14_nonseparable_blending_procs_t *pblend_procs, pdf14_device *pdev)
{
    template_compose_group16(tos_ptr, /*tos_isolated*/0, tos_planestride, tos_rowstride, alpha, shape, BLEND_MODE_Normal, /*tos_has_shape*/0,
        tos_shape_offset, tos_alpha_g_offset, tos_tag_offset, /*tos_has_tag*/0, /*tos_alpha_g_ptr*/0,
        nos_ptr, /*nos_isolated*/0, nos_planestride, nos_rowstride, /*nos_alpha_g_ptr*/0, /* nos_knockout = */0,
        /*nos_shape_offset*/0, /*nos_tag_offset*/0, mask_row_ptr, has_mask, maskbuf, mask_bg_alpha, mask_tr_fn,
        backdrop_ptr, /*has_matte*/0, n_chan, /*additive*/1, /*num_spots*/0, /*overprint*/0, /*drawn_comps*/0, x0, y0, x1, y1, pblend_procs, pdev, 1, 0);
}

static void
compose_group16_nonknockout_nonblend_nonisolated_nomask_common(uint16_t *tos_ptr, bool tos_isolated, int tos_planestride, int tos_rowstride,
              uint16_t alpha, uint16_t shape, gs_blend_mode_t blend_mode, bool tos_has_shape, int tos_shape_offset, int tos_alpha_g_offset,
              int tos_tag_offset, bool tos_has_tag, uint16_t *tos_alpha_g_ptr, uint16_t *nos_ptr, bool nos_isolated, int nos_planestride,
              int nos_rowstride, uint16_t *nos_alpha_g_ptr, bool nos_knockout, int nos_shape_offset, int nos_tag_offset,
              uint16_t *mask_row_ptr, int has_mask, pdf14_buf *maskbuf, uint16_t mask_bg_alpha, const uint16_t *mask_tr_fn, uint16_t *backdrop_ptr,
              bool has_matte, int n_chan, bool additive, int num_spots, bool overprint, gx_color_index drawn_comps, int x0, int y0, int x1, int y1,
              const pdf14_nonseparable_blending_procs_t *pblend_procs, pdf14_device *pdev)
{
    template_compose_group16(tos_ptr, /*tos_isolated*/0, tos_planestride, tos_rowstride, alpha, shape, BLEND_MODE_Normal, /*tos_has_shape*/0,
        tos_shape_offset, tos_alpha_g_offset, tos_tag_offset, /*tos_has_tag*/0, /*tos_alpha_g_ptr*/0,
        nos_ptr, /*nos_isolated*/0, nos_planestride, nos_rowstride, /*nos_alpha_g_ptr*/0, /* nos_knockout = */0,
        /*nos_shape_offset*/0, /*nos_tag_offset*/0, mask_row_ptr, /*has_mask*/0, /*maskbuf*/NULL, mask_bg_alpha, mask_tr_fn,
        backdrop_ptr, /*has_matte*/0, n_chan, /*additive*/1, /*num_spots*/0, /*overprint*/0, /*drawn_comps*/0, x0, y0, x1, y1, pblend_procs, pdev, 1, 0);
}

static void
compose_group16_nonknockout_noblend_general(uint16_t *tos_ptr, bool tos_isolated, int tos_planestride, int tos_rowstride,
              uint16_t alpha, uint16_t shape, gs_blend_mode_t blend_mode, bool tos_has_shape, int tos_shape_offset,
              int tos_alpha_g_offset, int tos_tag_offset, bool tos_has_tag, uint16_t *tos_alpha_g_ptr, uint16_t *nos_ptr,
              bool nos_isolated, int nos_planestride, int nos_rowstride, uint16_t *nos_alpha_g_ptr, bool nos_knockout,
              int nos_shape_offset, int nos_tag_offset, uint16_t *mask_row_ptr, int has_mask, pdf14_buf *maskbuf,
              uint16_t mask_bg_alpha, const uint16_t *mask_tr_fn, uint16_t *backdrop_ptr, bool has_matte, int n_chan,
              bool additive, int num_spots, bool overprint, gx_color_index drawn_comps, int x0, int y0, int x1, int y1,
              const pdf14_nonseparable_blending_procs_t *pblend_procs, pdf14_device *pdev)
{
    template_compose_group16(tos_ptr, tos_isolated, tos_planestride, tos_rowstride, alpha, shape, BLEND_MODE_Normal, tos_has_shape,
        tos_shape_offset, tos_alpha_g_offset, tos_tag_offset, tos_has_tag, tos_alpha_g_ptr,
        nos_ptr, nos_isolated, nos_planestride, nos_rowstride, nos_alpha_g_ptr, /* nos_knockout = */0,
        nos_shape_offset, nos_tag_offset, mask_row_ptr, has_mask, maskbuf, mask_bg_alpha, mask_tr_fn,
        backdrop_ptr, has_matte, n_chan, additive, num_spots, overprint, drawn_comps, x0, y0, x1, y1, pblend_procs, pdev, 1, 0);
}

static void
compose_group16_alphaless_knockout(uint16_t *tos_ptr, bool tos_isolated, int tos_planestride, int tos_rowstride,
              uint16_t alpha, uint16_t shape, gs_blend_mode_t blend_mode, bool tos_has_shape, int tos_shape_offset,
              int tos_alpha_g_offset, int tos_tag_offset, bool tos_has_tag, uint16_t *tos_alpha_g_ptr, uint16_t *nos_ptr,
              bool nos_isolated, int nos_planestride, int nos_rowstride, uint16_t *nos_alpha_g_ptr, bool nos_knockout,
              int nos_shape_offset, int nos_tag_offset,
              uint16_t *mask_row_ptr, int has_mask, pdf14_buf *maskbuf, uint16_t mask_bg_alpha, const uint16_t *mask_tr_fn,
              uint16_t *backdrop_ptr,
              bool has_matte, int n_chan, bool additive, int num_spots, bool overprint, gx_color_index drawn_comps, int x0, int y0, int x1, int y1,
              const pdf14_nonseparable_blending_procs_t *pblend_procs, pdf14_device *pdev)
{
    template_compose_group16(tos_ptr, tos_isolated, tos_planestride, tos_rowstride,
        alpha, shape, blend_mode, tos_has_shape, tos_shape_offset, tos_alpha_g_offset, tos_tag_offset, tos_has_tag, tos_alpha_g_ptr,
        nos_ptr, nos_isolated, nos_planestride, nos_rowstride, nos_alpha_g_ptr, /* nos_knockout = */1,
        nos_shape_offset, nos_tag_offset, /* mask_row_ptr */ NULL, /* has_mask */ 0, /* maskbuf */ NULL, mask_bg_alpha, /* mask_tr_fn */ NULL,
        backdrop_ptr, /* has_matte */ false , n_chan, additive, num_spots, overprint, drawn_comps, x0, y0, x1, y1, pblend_procs, pdev, 0, 1);
}

static void
compose_group16_alphaless_nonknockout(uint16_t *tos_ptr, bool tos_isolated, int tos_planestride, int tos_rowstride,
              uint16_t alpha, uint16_t shape, gs_blend_mode_t blend_mode, bool tos_has_shape, int tos_shape_offset, int tos_alpha_g_offset,
              int tos_tag_offset, bool tos_has_tag, uint16_t *tos_alpha_g_ptr, uint16_t *nos_ptr, bool nos_isolated, int nos_planestride,
              int nos_rowstride, uint16_t *nos_alpha_g_ptr, bool nos_knockout, int nos_shape_offset, int nos_tag_offset,
              uint16_t *mask_row_ptr, int has_mask, pdf14_buf *maskbuf, uint16_t mask_bg_alpha, const uint16_t *mask_tr_fn,
              uint16_t *backdrop_ptr,
              bool has_matte, int n_chan, bool additive, int num_spots, bool overprint, gx_color_index drawn_comps, int x0, int y0, int x1, int y1,
              const pdf14_nonseparable_blending_procs_t *pblend_procs, pdf14_device *pdev)
{
    template_compose_group16(tos_ptr, tos_isolated, tos_planestride, tos_rowstride,
        alpha, shape, blend_mode, tos_has_shape, tos_shape_offset, tos_alpha_g_offset, tos_tag_offset, tos_has_tag,
        tos_alpha_g_ptr, nos_ptr, nos_isolated, nos_planestride, nos_rowstride, nos_alpha_g_ptr, /* nos_knockout = */0,
        nos_shape_offset, nos_tag_offset, /* mask_row_ptr */ NULL, /* has_mask */ 0, /* maskbuf */ NULL, mask_bg_alpha, /* mask_tr_fn */ NULL,
        backdrop_ptr, /* has_matte */ false , n_chan, additive, num_spots, overprint, drawn_comps, x0, y0, x1, y1, pblend_procs, pdev, 0, 1);
}

static void
do_compose_group16(pdf14_buf *tos, pdf14_buf *nos, pdf14_buf *maskbuf,
                   int x0, int x1, int y0, int y1, int n_chan, bool additive,
                   const pdf14_nonseparable_blending_procs_t * pblend_procs,
                   bool has_matte, bool overprint, gx_color_index drawn_comps,
                   gs_memory_t *memory, gx_device *dev)
{
    int num_spots = tos->num_spots;
    uint16_t alpha = tos->alpha;
    uint16_t shape = tos->shape;
    gs_blend_mode_t blend_mode = tos->blend_mode;
    uint16_t *tos_ptr =
        (uint16_t *)(void *)(tos->data + (x0 - tos->rect.p.x)*2 +
                             (y0 - tos->rect.p.y) * tos->rowstride);
    uint16_t *nos_ptr =
        (uint16_t *)(void *)(nos->data + (x0 - nos->rect.p.x)*2 +
                             (y0 - nos->rect.p.y) * nos->rowstride);
    uint16_t *mask_row_ptr = NULL;
    int tos_planestride = tos->planestride;
    int nos_planestride = nos->planestride;
    uint16_t mask_bg_alpha = 0; /* Quiet compiler. */
    bool tos_isolated = tos->isolated;
    bool nos_isolated = nos->isolated;
    bool nos_knockout = nos->knockout;
    uint16_t *nos_alpha_g_ptr;
    uint16_t *tos_alpha_g_ptr;
    int tos_shape_offset = n_chan * tos_planestride;
    int tos_alpha_g_offset = tos_shape_offset + (tos->has_shape ? tos_planestride : 0);
    bool tos_has_tag = tos->has_tags;
    int tos_tag_offset = tos_planestride * (tos->n_planes - 1);
    int nos_shape_offset = n_chan * nos_planestride;
    int nos_alpha_g_offset = nos_shape_offset + (nos->has_shape ? nos_planestride : 0);
    int nos_tag_offset = nos_planestride * (nos->n_planes - 1);
    const uint16_t *mask_tr_fn = NULL; /* Quiet compiler. */
    bool has_mask = false;
    uint16_t *backdrop_ptr = NULL;
    pdf14_device *pdev = (pdf14_device *)dev;
#if RAW_DUMP
    uint16_t *composed_ptr = NULL;
    int width = x1 - x0;
#endif
    art_pdf_compose_group16_fn fn;

    if ((tos->n_chan == 0) || (nos->n_chan == 0))
        return;
    rect_merge(nos->dirty, tos->dirty);
    if (nos->has_tags)
        if_debug7m('v', memory,
                   "pdf14_pop_transparency_group y0 = %d, y1 = %d, w = %d, alpha = %d, shape = %d, tag = %d, bm = %d\n",
                   y0, y1, x1 - x0, alpha, shape, dev->graphics_type_tag & ~GS_DEVICE_ENCODES_TAGS, blend_mode);
    else
        if_debug6m('v', memory,
                   "pdf14_pop_transparency_group y0 = %d, y1 = %d, w = %d, alpha = %d, shape = %d, bm = %d\n",
                   y0, y1, x1 - x0, alpha, shape, blend_mode);
    if (!nos->has_shape)
        nos_shape_offset = 0;
    if (!nos->has_tags)
        nos_tag_offset = 0;
    if (nos->has_alpha_g) {
        nos_alpha_g_ptr = nos_ptr + (nos_alpha_g_offset>>1);
    } else
        nos_alpha_g_ptr = NULL;
    if (tos->has_alpha_g) {
        tos_alpha_g_ptr = tos_ptr + (tos_alpha_g_offset>>1);
    } else
        tos_alpha_g_ptr = NULL;
    if (nos->backdrop != NULL) {
        backdrop_ptr =
            (uint16_t *)(void *)(nos->backdrop + (x0 - nos->rect.p.x)*2 +
                                 (y0 - nos->rect.p.y) * nos->rowstride);
    }
    if (blend_mode != BLEND_MODE_Compatible && blend_mode != BLEND_MODE_Normal)
        overprint = false;

    if (maskbuf != NULL) {
        unsigned int tmp;
        mask_tr_fn = (uint16_t *)maskbuf->transfer_fn;
        /* Make sure we are in the mask buffer */
        if (maskbuf->data != NULL) {
            mask_row_ptr =
                (uint16_t *)(void *)(maskbuf->data + (x0 - maskbuf->rect.p.x)*2 +
                                     (y0 - maskbuf->rect.p.y) * maskbuf->rowstride);
            has_mask = true;
        }
        /* We may have a case, where we are outside the maskbuf rect. */
        /* We would have avoided creating the maskbuf->data */
        /* In that case, we should use the background alpha value */
        /* See discussion on the BC entry in the PDF spec.   */
        mask_bg_alpha = maskbuf->alpha;
        /* Adjust alpha by the mask background alpha.   This is only used
           if we are outside the soft mask rect during the filling operation */
        mask_bg_alpha = interp16(mask_tr_fn, mask_bg_alpha);
        tmp = alpha * mask_bg_alpha + 0x8000;
        mask_bg_alpha = (tmp + (tmp >> 8)) >> 8;
    }
    n_chan--; /* Now the true number of colorants (i.e. not including alpha)*/
#if RAW_DUMP
    composed_ptr = nos_ptr;
    dump_raw_buffer(memory, y1-y0, width, tos->n_planes, tos_planestride, tos->rowstride,
                    "bImageTOS", (byte *)tos_ptr, tos->deep);
    dump_raw_buffer(memory, y1-y0, width, nos->n_planes, nos_planestride, nos->rowstride,
                    "cImageNOS", (byte *)nos_ptr, tos->deep);
    if (maskbuf !=NULL && maskbuf->data != NULL) {
        dump_raw_buffer(memory, maskbuf->rect.q.y - maskbuf->rect.p.y,
                        maskbuf->rect.q.x - maskbuf->rect.p.x, maskbuf->n_planes,
                        maskbuf->planestride, maskbuf->rowstride, "dMask",
                        maskbuf->data, maskbuf->deep);
    }
#endif

    /* You might hope that has_mask iff maskbuf != NULL, but this is
     * not the case. Certainly we can see cases where maskbuf != NULL
     * and has_mask = 0. What's more, treating such cases as being
     * has_mask = 0 causes diffs. */
#ifdef TRACK_COMPOSE_GROUPS
    {
        int code = 0;

        code += !!nos_knockout;
        code += (!!nos_isolated)<<1;
        code += (!!tos_isolated)<<2;
        code += (!!tos->has_shape)<<3;
        code += (!!tos_has_tag)<<4;
        code += (!!additive)<<5;
        code += (!!overprint)<<6;
        code += (!!has_mask || maskbuf != NULL)<<7;
        code += (!!has_matte)<<8;
        code += (backdrop_ptr != NULL)<<9;
        code += (num_spots != 0)<<10;
        code += blend_mode<<11;

        if (track_compose_groups == 0)
        {
            atexit(dump_track_compose_groups);
            track_compose_groups = 1;
        }
        compose_groups[code]++;
    }
#endif

    /* We have tested the files on the cluster to see what percentage of
     * files/devices hit the different options. */
    if (nos_knockout)
        fn = &compose_group16_knockout; /* Small %ages, nothing more than 1.1% */
    else if (blend_mode != 0)
        fn = &compose_group16_nonknockout_blend; /* Small %ages, nothing more than 2% */
    else if (tos->has_shape == 0 && tos_has_tag == 0 && nos_isolated == 0 && nos_alpha_g_ptr == NULL &&
             nos_shape_offset == 0 && nos_tag_offset == 0 && backdrop_ptr == NULL && has_matte == 0 && num_spots == 0 &&
             overprint == 0 && tos_alpha_g_ptr == NULL) {
             /* Additive vs Subtractive makes no difference in normal blend mode with no spots */
        if (tos_isolated) {
            if (has_mask && maskbuf) {/* 7% */
                /* AirPrint test case hits this */
                if (maskbuf && maskbuf->rect.p.x <= x0 && maskbuf->rect.p.y <= y0 &&
                    maskbuf->rect.q.x >= x1 && maskbuf->rect.q.y >= y1)
                    fn = &compose_group16_nonknockout_nonblend_isolated_allmask_common;
                else
                    fn = &compose_group16_nonknockout_nonblend_isolated_mask_common;
            } else
                if (maskbuf) {
                    /* Outside mask data but still has mask */
                    fn = &compose_group16_nonknockout_nonblend_isolated_mask_common;
                } else {
                    fn = &compose_group16_nonknockout_nonblend_isolated_nomask_common;
                }
        } else {
            if (has_mask || maskbuf) /* 4% */
                fn = &compose_group16_nonknockout_nonblend_nonisolated_mask_common;
            else /* 15% */
                fn = &compose_group16_nonknockout_nonblend_nonisolated_nomask_common;
        }
    } else
        fn = compose_group16_nonknockout_noblend_general;

    tos_planestride >>= 1;
    tos_shape_offset >>= 1;
    tos_alpha_g_offset >>= 1;
    tos_tag_offset >>= 1;
    nos_planestride >>= 1;
    nos_shape_offset >>= 1;
    nos_tag_offset >>= 1;
    fn(tos_ptr, tos_isolated, tos_planestride, tos->rowstride>>1, alpha, shape, blend_mode, tos->has_shape,
                  tos_shape_offset, tos_alpha_g_offset, tos_tag_offset, tos_has_tag,
                  tos_alpha_g_ptr, nos_ptr, nos_isolated, nos_planestride, nos->rowstride>>1, nos_alpha_g_ptr, nos_knockout,
                  nos_shape_offset, nos_tag_offset,
                  mask_row_ptr, has_mask, maskbuf, mask_bg_alpha, mask_tr_fn,
                  backdrop_ptr,
                  has_matte, n_chan, additive, num_spots, overprint, drawn_comps, x0, y0, x1, y1,
                  pblend_procs, pdev);

#if RAW_DUMP
    dump_raw_buffer(memory, y1-y0, width, nos->n_planes, nos_planestride<<1, nos->rowstride,
                    "eComposed", (byte *)composed_ptr, nos->deep);
    global_index++;
#endif
}

void
pdf14_compose_group(pdf14_buf *tos, pdf14_buf *nos, pdf14_buf *maskbuf,
              int x0, int x1, int y0, int y1, int n_chan, bool additive,
              const pdf14_nonseparable_blending_procs_t * pblend_procs,
              bool has_matte, bool overprint, gx_color_index drawn_comps,
              gs_memory_t *memory, gx_device *dev)
{
    if (tos->deep)
        do_compose_group16(tos, nos, maskbuf, x0, x1, y0, y1, n_chan,
                           additive, pblend_procs, has_matte, overprint,
                           drawn_comps, memory, dev);
    else
        do_compose_group(tos, nos, maskbuf, x0, x1, y0, y1, n_chan,
                         additive, pblend_procs, has_matte, overprint,
                         drawn_comps, memory, dev);
}

static void
do_compose_alphaless_group(pdf14_buf *tos, pdf14_buf *nos,
                           int x0, int x1, int y0, int y1,
                           gs_memory_t *memory, gx_device *dev)
{
    pdf14_device *pdev = (pdf14_device *)dev;
    bool overprint = pdev->op_state == PDF14_OP_STATE_FILL ? pdev->overprint : pdev->stroke_overprint;
    bool additive = pdev->ctx->additive;
    gx_color_index drawn_comps = pdev->op_state == PDF14_OP_STATE_FILL ?
                                     pdev->drawn_comps_fill : pdev->drawn_comps_stroke;
    int n_chan = nos->n_chan;
    int num_spots = tos->num_spots;
    byte alpha = tos->alpha>>8;
    byte shape = tos->shape>>8;
    gs_blend_mode_t blend_mode = tos->blend_mode;
    byte *tos_ptr = tos->data + x0 - tos->rect.p.x +
        (y0 - tos->rect.p.y) * tos->rowstride;
    byte *nos_ptr = nos->data + x0 - nos->rect.p.x +
        (y0 - nos->rect.p.y) * nos->rowstride;
    byte *mask_row_ptr = NULL;
    int tos_planestride = tos->planestride;
    int nos_planestride = nos->planestride;
    byte mask_bg_alpha = 0; /* Quiet compiler. */
    bool tos_isolated = false;
    bool nos_isolated = nos->isolated;
    bool nos_knockout = nos->knockout;
    byte *nos_alpha_g_ptr, *tos_alpha_g_ptr;
    int tos_shape_offset = n_chan * tos_planestride;
    int tos_alpha_g_offset = tos_shape_offset + (tos->has_shape ? tos_planestride : 0);
    bool tos_has_tag = tos->has_tags;
    int tos_tag_offset = tos_planestride * (tos->n_planes - 1);
    int nos_shape_offset = n_chan * nos_planestride;
    int nos_alpha_g_offset = nos_shape_offset + (nos->has_shape ? nos_planestride : 0);
    int nos_tag_offset = nos_planestride * (nos->n_planes - 1);
    const byte *mask_tr_fn = NULL; /* Quiet compiler. */
    bool has_mask = false;
    byte *backdrop_ptr = NULL;
#if RAW_DUMP
    byte *composed_ptr = NULL;
    int width = x1 - x0;
#endif
    art_pdf_compose_group_fn fn;

    if ((tos->n_chan == 0) || (nos->n_chan == 0))
        return;
    rect_merge(nos->dirty, tos->dirty);
    if (nos->has_tags)
        if_debug7m('v', memory,
                   "pdf14_pop_transparency_group y0 = %d, y1 = %d, w = %d, alpha = %d, shape = %d, tag = %d, bm = %d\n",
                   y0, y1, x1 - x0, alpha, shape, dev->graphics_type_tag & ~GS_DEVICE_ENCODES_TAGS, blend_mode);
    else
        if_debug6m('v', memory,
                   "pdf14_pop_transparency_group y0 = %d, y1 = %d, w = %d, alpha = %d, shape = %d, bm = %d\n",
                   y0, y1, x1 - x0, alpha, shape, blend_mode);
    if (!nos->has_shape)
        nos_shape_offset = 0;
    if (!nos->has_tags)
        nos_tag_offset = 0;
    if (nos->has_alpha_g) {
        nos_alpha_g_ptr = nos_ptr + nos_alpha_g_offset;
    } else
        nos_alpha_g_ptr = NULL;
    if (tos->has_alpha_g) {
        tos_alpha_g_ptr = tos_ptr + tos_alpha_g_offset;
    } else
        tos_alpha_g_ptr = NULL;
    if (nos->backdrop != NULL) {
        backdrop_ptr = nos->backdrop + x0 - nos->rect.p.x +
                       (y0 - nos->rect.p.y) * nos->rowstride;
    }
    if (blend_mode != BLEND_MODE_Compatible && blend_mode != BLEND_MODE_Normal)
        overprint = false;

    n_chan--; /* Now the true number of colorants (i.e. not including alpha)*/
#if RAW_DUMP
    composed_ptr = nos_ptr;
    dump_raw_buffer(memory, y1-y0, width, tos->n_planes, tos_planestride, tos->rowstride,
                    "bImageTOS", tos_ptr, tos->deep);
    dump_raw_buffer(memory, y1-y0, width, nos->n_planes, nos_planestride, nos->rowstride,
                    "cImageNOS", nos_ptr, nos->deep);
    /* maskbuf is NULL in here */
#endif

    /* You might hope that has_mask iff maskbuf != NULL, but this is
     * not the case. Certainly we can see cases where maskbuf != NULL
     * and has_mask = 0. What's more, treating such cases as being
     * has_mask = 0 causes diffs. */
#ifdef TRACK_COMPOSE_GROUPS
    {
        int code = 0;

        code += !!nos_knockout;
        code += (!!nos_isolated)<<1;
        code += (!!tos_isolated)<<2;
        code += (!!tos->has_shape)<<3;
        code += (!!tos_has_tag)<<4;
        code += (!!additive)<<5;
        code += (!!overprint)<<6;
        code += (!!has_mask)<<7;
        code += (backdrop_ptr != NULL)<<9;
        code += (num_spots != 0)<<10;
        code += blend_mode<<11;

        if (track_compose_groups == 0)
        {
            atexit(dump_track_compose_groups);
            track_compose_groups = 1;
        }
        compose_groups[code]++;
    }
#endif

    /* We have tested the files on the cluster to see what percentage of
     * files/devices hit the different options. */
    if (nos_knockout)
        fn = &compose_group_alphaless_knockout;
    else
        fn = &compose_group_alphaless_nonknockout;

    fn(tos_ptr, tos_isolated, tos_planestride, tos->rowstride, alpha, shape, blend_mode, tos->has_shape,
                  tos_shape_offset, tos_alpha_g_offset, tos_tag_offset, tos_has_tag, tos_alpha_g_ptr,
                  nos_ptr, nos_isolated, nos_planestride, nos->rowstride, nos_alpha_g_ptr, nos_knockout,
                  nos_shape_offset, nos_tag_offset,
                  mask_row_ptr, has_mask, /* maskbuf */ NULL, mask_bg_alpha, mask_tr_fn,
                  backdrop_ptr,
                  /* has_matte */ 0, n_chan, additive, num_spots, overprint, drawn_comps, x0, y0, x1, y1,
                  pdev->blend_procs, pdev);

#if RAW_DUMP
    dump_raw_buffer(memory, y1-y0, width, nos->n_planes, nos_planestride, nos->rowstride,
                    "eComposed", composed_ptr, nos->deep);
    global_index++;
#endif
}

static void
do_compose_alphaless_group16(pdf14_buf *tos, pdf14_buf *nos,
                             int x0, int x1, int y0, int y1,
                             gs_memory_t *memory, gx_device *dev)
{
    pdf14_device *pdev = (pdf14_device *)dev;
    bool overprint = pdev->op_state == PDF14_OP_STATE_FILL ? pdev->overprint : pdev->stroke_overprint;
    bool additive = pdev->ctx->additive;
    gx_color_index drawn_comps = pdev->op_state == PDF14_OP_STATE_FILL ?
                                     pdev->drawn_comps_fill : pdev->drawn_comps_stroke;
    int n_chan = nos->n_chan;
    int num_spots = tos->num_spots;
    uint16_t alpha = tos->alpha;
    uint16_t shape = tos->shape;
    gs_blend_mode_t blend_mode = tos->blend_mode;
    uint16_t *tos_ptr =
        (uint16_t *)(void *)(tos->data + (x0 - tos->rect.p.x)*2 +
                             (y0 - tos->rect.p.y) * tos->rowstride);
    uint16_t *nos_ptr =
        (uint16_t *)(void *)(nos->data + (x0 - nos->rect.p.x)*2 +
                             (y0 - nos->rect.p.y) * nos->rowstride);
    uint16_t *mask_row_ptr = NULL;
    int tos_planestride = tos->planestride;
    int nos_planestride = nos->planestride;
    uint16_t mask_bg_alpha = 0; /* Quiet compiler. */
    bool tos_isolated = false;
    bool nos_isolated = nos->isolated;
    bool nos_knockout = nos->knockout;
    uint16_t *nos_alpha_g_ptr;
    uint16_t *tos_alpha_g_ptr;
    int tos_shape_offset = n_chan * tos_planestride;
    int tos_alpha_g_offset = tos_shape_offset + (tos->has_shape ? tos_planestride : 0);
    bool tos_has_tag = tos->has_tags;
    int tos_tag_offset = tos_planestride * (tos->n_planes - 1);
    int nos_shape_offset = n_chan * nos_planestride;
    int nos_alpha_g_offset = nos_shape_offset + (nos->has_shape ? nos_planestride : 0);
    int nos_tag_offset = nos_planestride * (nos->n_planes - 1);
    bool has_mask = false;
    uint16_t *backdrop_ptr = NULL;
#if RAW_DUMP
    uint16_t *composed_ptr = NULL;
    int width = x1 - x0;
#endif
    art_pdf_compose_group16_fn fn;

    if ((tos->n_chan == 0) || (nos->n_chan == 0))
        return;
    rect_merge(nos->dirty, tos->dirty);
    if (nos->has_tags)
        if_debug7m('v', memory,
                   "pdf14_pop_transparency_group y0 = %d, y1 = %d, w = %d, alpha = %d, shape = %d, tag = %d, bm = %d\n",
                   y0, y1, x1 - x0, alpha, shape, dev->graphics_type_tag & ~GS_DEVICE_ENCODES_TAGS, blend_mode);
    else
        if_debug6m('v', memory,
                   "pdf14_pop_transparency_group y0 = %d, y1 = %d, w = %d, alpha = %d, shape = %d, bm = %d\n",
                   y0, y1, x1 - x0, alpha, shape, blend_mode);
    if (!nos->has_shape)
        nos_shape_offset = 0;
    if (!nos->has_tags)
        nos_tag_offset = 0;
    if (nos->has_alpha_g) {
        nos_alpha_g_ptr = nos_ptr + (nos_alpha_g_offset>>1);
    } else
        nos_alpha_g_ptr = NULL;
    if (tos->has_alpha_g) {
        tos_alpha_g_ptr = tos_ptr + (tos_alpha_g_offset>>1);
    } else
        tos_alpha_g_ptr = NULL;

    if (nos->backdrop != NULL) {
        backdrop_ptr =
            (uint16_t *)(void *)(nos->backdrop + (x0 - nos->rect.p.x)*2 +
                                 (y0 - nos->rect.p.y) * nos->rowstride);
    }
    if (blend_mode != BLEND_MODE_Compatible && blend_mode != BLEND_MODE_Normal)
        overprint = false;

    n_chan--; /* Now the true number of colorants (i.e. not including alpha)*/
#if RAW_DUMP
    composed_ptr = nos_ptr;
    dump_raw_buffer_be(memory, y1-y0, width, tos->n_planes, tos_planestride, tos->rowstride,
                       "bImageTOS", (byte *)tos_ptr, tos->deep);
    dump_raw_buffer(memory, y1-y0, width, nos->n_planes, nos_planestride, nos->rowstride,
                    "cImageNOS", (byte *)nos_ptr, nos->deep);
    /* maskbuf is NULL in here */
#endif

    /* You might hope that has_mask iff maskbuf != NULL, but this is
     * not the case. Certainly we can see cases where maskbuf != NULL
     * and has_mask = 0. What's more, treating such cases as being
     * has_mask = 0 causes diffs. */
#ifdef TRACK_COMPOSE_GROUPS
    {
        int code = 0;

        code += !!nos_knockout;
        code += (!!nos_isolated)<<1;
        code += (!!tos_isolated)<<2;
        code += (!!tos->has_shape)<<3;
        code += (!!tos_has_tag)<<4;
        code += (!!additive)<<5;
        code += (!!overprint)<<6;
        code += (!!has_mask)<<7;
        code += (backdrop_ptr != NULL)<<9;
        code += (num_spots != 0)<<10;
        code += blend_mode<<11;

        if (track_compose_groups == 0)
        {
            atexit(dump_track_compose_groups);
            track_compose_groups = 1;
        }
        compose_groups[code]++;
    }
#endif

    /* We have tested the files on the cluster to see what percentage of
     * files/devices hit the different options. */
    if (nos_knockout)
        fn = &compose_group16_alphaless_knockout;
    else
        fn = &compose_group16_alphaless_nonknockout;

    fn(tos_ptr, tos_isolated, tos_planestride>>1, tos->rowstride>>1, alpha, shape, blend_mode, tos->has_shape,
                  tos_shape_offset>>1, tos_alpha_g_offset>>1, tos_tag_offset>>1, tos_has_tag, tos_alpha_g_ptr,
                  nos_ptr, nos_isolated, nos_planestride>>1, nos->rowstride>>1, nos_alpha_g_ptr, nos_knockout,
                  nos_shape_offset>>1, nos_tag_offset>>1,
                  mask_row_ptr, has_mask, /* maskbuf */ NULL, mask_bg_alpha, NULL,
                  backdrop_ptr,
                  /* has_matte */ 0, n_chan, additive, num_spots, overprint, drawn_comps, x0, y0, x1, y1,
                  pdev->blend_procs, pdev);

#if RAW_DUMP
    dump_raw_buffer(memory, y1-y0, width, nos->n_planes, nos_planestride, nos->rowstride,
                    "eComposed", (byte *)composed_ptr, nos->deep);
    global_index++;
#endif
}

void
pdf14_compose_alphaless_group(pdf14_buf *tos, pdf14_buf *nos,
                              int x0, int x1, int y0, int y1,
                              gs_memory_t *memory, gx_device *dev)
{
    if (tos->deep)
        do_compose_alphaless_group16(tos, nos, x0, x1, y0, y1, memory, dev);
    else
        do_compose_alphaless_group(tos, nos, x0, x1, y0, y1, memory, dev);
}

typedef void (*pdf14_mark_fill_rect_fn)(int w, int h, byte *gs_restrict dst_ptr, byte *gs_restrict src, int num_comp, int num_spots, int first_blend_spot,
               byte src_alpha, int rowstride, int planestride, bool additive, pdf14_device *pdev, gs_blend_mode_t blend_mode,
               bool overprint, gx_color_index drawn_comps, int tag_off, gs_graphics_type_tag_t curr_tag,
               int alpha_g_off, int shape_off, byte shape);

static forceinline void
template_mark_fill_rect(int w, int h, byte *gs_restrict dst_ptr, byte *gs_restrict src, int num_comp, int num_spots, int first_blend_spot,
               byte src_alpha, int rowstride, int planestride, bool additive, pdf14_device *pdev, gs_blend_mode_t blend_mode,
               bool overprint, gx_color_index drawn_comps, int tag_off, gs_graphics_type_tag_t curr_tag,
               int alpha_g_off, int shape_off, byte shape)
{
    int i, j, k;
    byte dst[PDF14_MAX_PLANES] = { 0 };
    byte dest_alpha;
    bool tag_blend = blend_mode == BLEND_MODE_Normal ||
        blend_mode == BLEND_MODE_Compatible ||
        blend_mode == BLEND_MODE_CompatibleOverprint;

    for (j = h; j > 0; --j) {
        for (i = w; i > 0; --i) {
            if ((blend_mode == BLEND_MODE_Normal && src[num_comp] == 0xff && !overprint) || dst_ptr[num_comp * planestride] == 0) {
                /* dest alpha is zero (or normal, and solid src) just use source. */
                if (additive) {
                    /* Hybrid case */
                    for (k = 0; k < (num_comp - num_spots); k++) {
                        dst_ptr[k * planestride] = src[k];
                    }
                    for (k = 0; k < num_spots; k++) {
                        dst_ptr[(k + num_comp - num_spots) * planestride] =
                                255 - src[k + num_comp - num_spots];
                    }
                } else {
                    /* Pure subtractive */
                    for (k = 0; k < num_comp; k++) {
                        dst_ptr[k * planestride] = 255 - src[k];
                    }
                }
                /* alpha */
                dst_ptr[num_comp * planestride] = src[num_comp];
            } else if (src[num_comp] != 0) {
                byte *pdst;
                /* Complement subtractive planes */
                if (!additive) {
                    /* Pure subtractive */
                    for (k = 0; k < num_comp; ++k)
                        dst[k] = 255 - dst_ptr[k * planestride];
                } else {
                    /* Hybrid case, additive with subtractive spots */
                    for (k = 0; k < (num_comp - num_spots); k++) {
                        dst[k] = dst_ptr[k * planestride];
                    }
                    for (k = 0; k < num_spots; k++) {
                        dst[k + num_comp - num_spots] =
                            255 - dst_ptr[(k + num_comp - num_spots) * planestride];
                    }
                }
                dst[num_comp] = dst_ptr[num_comp * planestride];
                dest_alpha = dst[num_comp];
                pdst = art_pdf_composite_pixel_alpha_8_inline(dst, src, num_comp, blend_mode, first_blend_spot,
                            pdev->blend_procs, pdev);
                /* Post blend complement for subtractive and handling of drawncomps
                   if overprint.  We will have already done the compatible overprint
                   mode in the above composition */
                if (!additive && !overprint) {
                    /* Pure subtractive */
                    for (k = 0; k < num_comp; ++k)
                        dst_ptr[k * planestride] = 255 - pdst[k];
                } else if (!additive && overprint) {
                    int comps;
                    /* If this is an overprint case, and alpha_r is different
                       than alpha_d then we will need to adjust
                       the colors of the non-drawn components here too */
                    if (dest_alpha != pdst[num_comp] && pdst[num_comp] != 0) {
                        /* dest_alpha > pdst[num_comp], and dst[num_comp] != 0.
                         * Therefore dest_alpha / pdst[num_comp] <= 255 */
                        uint32_t scale = 256 * dest_alpha / pdst[num_comp];
                        for (k = 0, comps = drawn_comps; k < num_comp; ++k, comps >>= 1) {
                            if ((comps & 0x1) != 0) {
                                dst_ptr[k * planestride] = 255 - pdst[k];
                            } else {
                                /* We need val_new = (val_old * old_alpha) / new_alpha */
                                uint32_t val = (scale * (255 - pdst[k]) + 128)>>8;
                                if (val > 255)
                                    val = 255;
                                dst_ptr[k * planestride] = val;
                            }
                        }
                    } else {
                        for (k = 0, comps = drawn_comps; k < num_comp; ++k, comps >>= 1) {
                            if ((comps & 0x1) != 0) {
                                dst_ptr[k * planestride] = 255 - pdst[k];
                            }
                        }
                    }
                } else {
                    /* Hybrid case, additive with subtractive spots */
                    for (k = 0; k < (num_comp - num_spots); k++) {
                        dst_ptr[k * planestride] = pdst[k];
                    }
                    for (k = 0; k < num_spots; k++) {
                        dst_ptr[(k + num_comp - num_spots) * planestride] =
                                255 - pdst[k + num_comp - num_spots];
                    }
                }
                /* The alpha channel */
                dst_ptr[num_comp * planestride] = pdst[num_comp];
            }
            if (tag_off) {
                /* If src alpha is 100% then set to curr_tag, else or */
                /* other than Normal BM, we always OR */
                if (src[num_comp] == 255 && tag_blend) {
                    dst_ptr[tag_off] = curr_tag;
                } else {
                    dst_ptr[tag_off] |= curr_tag;
                }
            }
            if (alpha_g_off) {
                int tmp = (255 - dst_ptr[alpha_g_off]) * src_alpha + 0x80;
                dst_ptr[alpha_g_off] = 255 - ((tmp + (tmp >> 8)) >> 8);
            }
            if (shape_off) {
                int tmp = (255 - dst_ptr[shape_off]) * shape + 0x80;
                dst_ptr[shape_off] = 255 - ((tmp + (tmp >> 8)) >> 8);
            }
            ++dst_ptr;
        }
        dst_ptr += rowstride;
    }
}

static void
mark_fill_rect_alpha0(int w, int h, byte *gs_restrict dst_ptr, byte *gs_restrict src, int num_comp, int num_spots, int first_blend_spot,
               byte src_alpha, int rowstride, int planestride, bool additive, pdf14_device *pdev, gs_blend_mode_t blend_mode,
               bool overprint, gx_color_index drawn_comps, int tag_off, gs_graphics_type_tag_t curr_tag,
               int alpha_g_off, int shape_off, byte shape)
{
    int i, j;

    for (j = h; j > 0; --j) {
        for (i = w; i > 0; --i) {
            if (alpha_g_off) {
                int tmp = (255 - dst_ptr[alpha_g_off]) * src_alpha + 0x80;
                dst_ptr[alpha_g_off] = 255 - ((tmp + (tmp >> 8)) >> 8);
            }
            if (shape_off) {
                int tmp = (255 - dst_ptr[shape_off]) * shape + 0x80;
                dst_ptr[shape_off] = 255 - ((tmp + (tmp >> 8)) >> 8);
            }
            ++dst_ptr;
        }
        dst_ptr += rowstride;
    }
}

static void
mark_fill_rect(int w, int h, byte *gs_restrict dst_ptr, byte *gs_restrict src, int num_comp, int num_spots, int first_blend_spot,
               byte src_alpha, int rowstride, int planestride, bool additive, pdf14_device *pdev, gs_blend_mode_t blend_mode,
               bool overprint, gx_color_index drawn_comps, int tag_off, gs_graphics_type_tag_t curr_tag,
               int alpha_g_off, int shape_off, byte shape)
{
    template_mark_fill_rect(w, h, dst_ptr, src, num_comp, num_spots, first_blend_spot,
               src_alpha, rowstride, planestride, additive, pdev, blend_mode,
               overprint, drawn_comps, tag_off, curr_tag,
               alpha_g_off, shape_off, shape);
}

static void
mark_fill_rect_sub4_fast(int w, int h, byte *gs_restrict dst_ptr, byte *gs_restrict src, int num_comp, int num_spots, int first_blend_spot,
               byte src_alpha, int rowstride, int planestride, bool additive, pdf14_device *pdev, gs_blend_mode_t blend_mode,
               bool overprint, gx_color_index drawn_comps, int tag_off, gs_graphics_type_tag_t curr_tag,
               int alpha_g_off, int shape_off, byte shape)
{
    int i, j, k;

    for (j = h; j > 0; --j) {
        for (i = w; i > 0; --i) {
            byte a_s = src[4];
            byte a_b = dst_ptr[4 * planestride];
            if ((a_s == 0xff) || a_b == 0) {
                /* dest alpha is zero (or normal, and solid src) just use source. */
                dst_ptr[0 * planestride] = 255 - src[0];
                dst_ptr[1 * planestride] = 255 - src[1];
                dst_ptr[2 * planestride] = 255 - src[2];
                dst_ptr[3 * planestride] = 255 - src[3];
                /* alpha */
                dst_ptr[4 * planestride] = a_s;
            } else if (a_s != 0) {
                /* Result alpha is Union of backdrop and source alpha */
                int tmp = (0xff - a_b) * (0xff - a_s) + 0x80;
                unsigned int a_r = 0xff - (((tmp >> 8) + tmp) >> 8);

                /* Compute a_s / a_r in 16.16 format */
                int src_scale = ((a_s << 16) + (a_r >> 1)) / a_r;

                dst_ptr[4 * planestride] = a_r;

                /* Do simple compositing of source over backdrop */
                for (k = 0; k < 4; k++) {
                    int c_s = src[k];
                    int c_b = 255 - dst_ptr[k * planestride];
                    tmp = (c_b << 16) + src_scale * (c_s - c_b) + 0x8000;
                    dst_ptr[k * planestride] = 255 - (tmp >> 16);
                }
            }
            ++dst_ptr;
        }
        dst_ptr += rowstride;
    }
}

static void
mark_fill_rect_add_nospots(int w, int h, byte *gs_restrict dst_ptr, byte *gs_restrict src, int num_comp, int num_spots, int first_blend_spot,
               byte src_alpha, int rowstride, int planestride, bool additive, pdf14_device *pdev, gs_blend_mode_t blend_mode,
               bool overprint, gx_color_index drawn_comps, int tag_off, gs_graphics_type_tag_t curr_tag,
               int alpha_g_off, int shape_off, byte shape)
{
    template_mark_fill_rect(w, h, dst_ptr, src, num_comp, /*num_spots*/0, first_blend_spot,
               src_alpha, rowstride, planestride, /*additive*/1, pdev, blend_mode,
               /*overprint*/0, /*drawn_comps*/0, tag_off, curr_tag,
               alpha_g_off, shape_off, shape);
}

static void
mark_fill_rect_add_nospots_common(int w, int h, byte *gs_restrict dst_ptr, byte *gs_restrict src, int num_comp, int num_spots, int first_blend_spot,
               byte src_alpha, int rowstride, int planestride, bool additive, pdf14_device *pdev, gs_blend_mode_t blend_mode,
               bool overprint, gx_color_index drawn_comps, int tag_off, gs_graphics_type_tag_t curr_tag,
               int alpha_g_off, int shape_off, byte shape)
{
    template_mark_fill_rect(w, h, dst_ptr, src, num_comp, /*num_spots*/0, /*first_blend_spot*/0,
               src_alpha, rowstride, planestride, /*additive*/1, pdev, /*blend_mode*/BLEND_MODE_Normal,
               /*overprint*/0, /*drawn_comps*/0, /*tag_off*/0, curr_tag,
               alpha_g_off, /*shape_off*/0, shape);
}

static void
mark_fill_rect_add_nospots_common_no_alpha_g(int w, int h, byte *gs_restrict dst_ptr, byte *gs_restrict src, int num_comp, int num_spots, int first_blend_spot,
               byte src_alpha, int rowstride, int planestride, bool additive, pdf14_device *pdev, gs_blend_mode_t blend_mode,
               bool overprint, gx_color_index drawn_comps, int tag_off, gs_graphics_type_tag_t curr_tag,
               int alpha_g_off, int shape_off, byte shape)
{
    template_mark_fill_rect(w, h, dst_ptr, src, num_comp, /*num_spots*/0, /*first_blend_spot*/0,
               src_alpha, rowstride, planestride, /*additive*/1, pdev, /*blend_mode*/BLEND_MODE_Normal,
               /*overprint*/0, /*drawn_comps*/0, /*tag_off*/0, curr_tag,
               /*alpha_g_off*/0, /*shape_off*/0, shape);
}

static void
mark_fill_rect_add3_common(int w, int h, byte *gs_restrict dst_ptr, byte *gs_restrict src, int num_comp, int num_spots, int first_blend_spot,
               byte src_alpha, int rowstride, int planestride, bool additive, pdf14_device *pdev, gs_blend_mode_t blend_mode,
               bool overprint, gx_color_index drawn_comps, int tag_off, gs_graphics_type_tag_t curr_tag,
               int alpha_g_off, int shape_off, byte shape)
{
    int i, j, k;

    for (j = h; j > 0; --j) {
        for (i = w; i > 0; --i) {
            byte a_s = src[3];
            byte a_b = dst_ptr[3 * planestride];
            if (a_s == 0xff || a_b == 0) {
                /* dest alpha is zero (or solid source) just use source. */
                dst_ptr[0 * planestride] = src[0];
                dst_ptr[1 * planestride] = src[1];
                dst_ptr[2 * planestride] = src[2];
                /* alpha */
                dst_ptr[3 * planestride] = a_s;
            } else if (a_s != 0) {
                /* Result alpha is Union of backdrop and source alpha */
                int tmp = (0xff - a_b) * (0xff - a_s) + 0x80;
                unsigned int a_r = 0xff - (((tmp >> 8) + tmp) >> 8);
                /* todo: verify that a_r is nonzero in all cases */

                /* Compute a_s / a_r in 16.16 format */
                int src_scale = ((a_s << 16) + (a_r >> 1)) / a_r;

                dst_ptr[3 * planestride] = a_r;

                /* Do simple compositing of source over backdrop */
                for (k = 0; k < 3; k++) {
                    int c_s = src[k];
                    int c_b = dst_ptr[k * planestride];
                    tmp = (c_b << 16) + src_scale * (c_s - c_b) + 0x8000;
                    dst_ptr[k * planestride] = tmp >> 16;
                }
            }
            ++dst_ptr;
        }
        dst_ptr += rowstride;
    }
}

static void
mark_fill_rect_add1_no_spots(int w, int h, byte *gs_restrict dst_ptr, byte *gs_restrict src, int num_comp, int num_spots, int first_blend_spot,
               byte src_alpha, int rowstride, int planestride, bool additive, pdf14_device *pdev, gs_blend_mode_t blend_mode,
               bool overprint, gx_color_index drawn_comps, int tag_off, gs_graphics_type_tag_t curr_tag,
               int alpha_g_off, int shape_off, byte shape)
{
    int i;
    bool tag_blend = blend_mode == BLEND_MODE_Normal ||
        blend_mode == BLEND_MODE_Compatible ||
        blend_mode == BLEND_MODE_CompatibleOverprint;

    for (; h > 0; --h) {
        for (i = w; i > 0; --i) {
            /* background empty, nothing to change, or solid source */
            byte a_s = src[1];
            if ((blend_mode == BLEND_MODE_Normal && a_s == 0xff) || dst_ptr[planestride] == 0) {
                dst_ptr[0] = src[0];
                dst_ptr[planestride] = a_s;
            } else {
                art_pdf_composite_pixel_alpha_8_fast_mono(dst_ptr, src,
                                                blend_mode, pdev->blend_procs,
                                                planestride, pdev);
            }
            if (tag_off) {
                /* If src alpha is 100% then set to curr_tag, else or */
                /* other than Normal BM, we always OR */
                if (tag_blend && a_s == 255) {
                     dst_ptr[tag_off] = curr_tag;
                } else {
                    dst_ptr[tag_off] |= curr_tag;
                }
            }
            if (alpha_g_off) {
                int tmp = (255 - dst_ptr[alpha_g_off]) * src_alpha + 0x80;
                dst_ptr[alpha_g_off] = 255 - ((tmp + (tmp >> 8)) >> 8);
            }
            if (shape_off) {
                int tmp = (255 - dst_ptr[shape_off]) * shape + 0x80;
                dst_ptr[shape_off] = 255 - ((tmp + (tmp >> 8)) >> 8);
            }
            ++dst_ptr;
        }
        dst_ptr += rowstride;
    }
}

static void
mark_fill_rect_add1_no_spots_normal(int w, int h, byte *gs_restrict dst_ptr, byte *gs_restrict src, int num_comp, int num_spots, int first_blend_spot,
               byte src_alpha, int rowstride, int planestride, bool additive, pdf14_device *pdev, gs_blend_mode_t blend_mode,
               bool overprint, gx_color_index drawn_comps, int tag_off, gs_graphics_type_tag_t curr_tag,
               int alpha_g_off, int shape_off, byte shape)
{
    int i;

    for (; h > 0; --h) {
        for (i = w; i > 0; --i) {
            /* background empty, nothing to change, or solid source */
            byte a_s = src[1];
            byte a_b = dst_ptr[planestride];
            if (a_s == 0xff || a_b == 0) {
                dst_ptr[0] = src[0];
                dst_ptr[planestride] = a_s;
            } else {
                /* Result alpha is Union of backdrop and source alpha */
                int tmp = (0xff - a_b) * (0xff - a_s) + 0x80;
                unsigned int a_r = 0xff - (((tmp >> 8) + tmp) >> 8);

                /* Compute a_s / a_r in 16.16 format */
                int src_scale = ((a_s << 16) + (a_r >> 1)) / a_r;

                /* Do simple compositing of source over backdrop */
                int c_s = src[0];
                int c_b = dst_ptr[0];
                tmp = (c_b << 16) + src_scale * (c_s - c_b) + 0x8000;
                dst_ptr[0] = tmp >> 16;
                dst_ptr[planestride] = a_r;
            }
            if (tag_off) {
                /* If src alpha is 100% then set to curr_tag, else or */
                /* other than Normal BM, we always OR */
                if (a_s == 255) {
                     dst_ptr[tag_off] = curr_tag;
                } else {
                    dst_ptr[tag_off] |= curr_tag;
                }
            }
            if (alpha_g_off) {
                int tmp = (255 - dst_ptr[alpha_g_off]) * src_alpha + 0x80;
                dst_ptr[alpha_g_off] = 255 - ((tmp + (tmp >> 8)) >> 8);
            }
            if (shape_off) {
                int tmp = (255 - dst_ptr[shape_off]) * shape + 0x80;
                dst_ptr[shape_off] = 255 - ((tmp + (tmp >> 8)) >> 8);
            }
            ++dst_ptr;
        }
        dst_ptr += rowstride;
    }
}

static void
mark_fill_rect_add1_no_spots_fast(int w, int h, byte *gs_restrict dst_ptr, byte *gs_restrict src, int num_comp, int num_spots, int first_blend_spot,
               byte src_alpha, int rowstride, int planestride, bool additive, pdf14_device *pdev, gs_blend_mode_t blend_mode,
               bool overprint, gx_color_index drawn_comps, int tag_off, gs_graphics_type_tag_t curr_tag,
               int alpha_g_off, int shape_off, byte shape)
{
    int i;

    for (; h > 0; --h) {
        for (i = w; i > 0; --i) {
            /* background empty, nothing to change, or solid source */
            byte a_s = src[1];
            byte a_b = dst_ptr[planestride];
            if (a_s == 0xff || a_b == 0) {
                dst_ptr[0] = src[0];
                dst_ptr[planestride] = a_s;
            } else if (a_s != 0) {
                /* Result alpha is Union of backdrop and source alpha */
                int tmp = (0xff - a_b) * (0xff - a_s) + 0x80;
                unsigned int a_r = 0xff - (((tmp >> 8) + tmp) >> 8);

                /* Compute a_s / a_r in 16.16 format */
                int src_scale = ((a_s << 16) + (a_r >> 1)) / a_r;

                /* Do simple compositing of source over backdrop */
                int c_s = src[0];
                int c_b = dst_ptr[0];
                tmp = (c_b << 16) + src_scale * (c_s - c_b) + 0x8000;
                dst_ptr[0] = tmp >> 16;
                dst_ptr[planestride] = a_r;
            }
            ++dst_ptr;
        }
        dst_ptr += rowstride;
    }
}

static int
do_mark_fill_rectangle(gx_device * dev, int x, int y, int w, int h,
                       gx_color_index color, const gx_device_color *pdc,
                       bool devn)
{
    pdf14_device *pdev = (pdf14_device *)dev;
    pdf14_buf *buf = pdev->ctx->stack;
    int j;
    byte *dst_ptr;
    byte src[PDF14_MAX_PLANES];
    gs_blend_mode_t blend_mode = pdev->blend_mode;
    bool additive = pdev->ctx->additive;
    int rowstride = buf->rowstride;
    int planestride = buf->planestride;
    gs_graphics_type_tag_t curr_tag = GS_UNKNOWN_TAG; /* Quite compiler */
    bool has_alpha_g = buf->has_alpha_g;
    bool has_shape = buf->has_shape;
    bool has_tags = buf->has_tags;
    int num_chan = buf->n_chan;
    int num_comp = num_chan - 1;
    int shape_off = num_chan * planestride;
    int alpha_g_off = shape_off + (has_shape ? planestride : 0);
    int tag_off = alpha_g_off + (has_alpha_g ? planestride : 0);
    bool overprint = pdev->op_state == PDF14_OP_STATE_FILL ? pdev->overprint : pdev->stroke_overprint;
    gx_color_index drawn_comps = pdev->op_state == PDF14_OP_STATE_FILL ?
                                     pdev->drawn_comps_fill : pdev->drawn_comps_stroke;
    byte shape = 0; /* Quiet compiler. */
    byte src_alpha;
    const gx_color_index mask = ((gx_color_index)1 << 8) - 1;
    const int shift = 8;
    int num_spots = buf->num_spots;
    int first_blend_spot = num_comp;
    pdf14_mark_fill_rect_fn fn;

    /* If we are going out to a CMYK or CMYK + spots pdf14 device (i.e.
       subtractive) and we are doing overprint with drawn_comps == 0
       then this is a no-operation */
    if (overprint && drawn_comps == 0 && !buf->group_color_info->isadditive)
        return 0;

    /* This is a fix to handle the odd case where overprint is active
       but drawn comps is zero due to the colorants that are present
       in the sep or devicen color space.  For example, if the color
       fill was cyan in a sep color space but we are drawing in a
       RGB blend space.  In this case the drawn comps is 0 and we should
       not be using compatible overprint mode here. */
    if (drawn_comps == 0 && blend_mode == BLEND_MODE_CompatibleOverprint &&
        buf->group_color_info->isadditive) {
        blend_mode = BLEND_MODE_Normal;
    }

    if (num_spots > 0 && !blend_valid_for_spot(blend_mode))
        first_blend_spot = num_comp - num_spots;
    if (blend_mode == BLEND_MODE_Normal)
        first_blend_spot = 0;

    if (buf->data == NULL)
        return 0;
    /* NB: gx_color_index is 4 or 8 bytes */
#if 0
    if (sizeof(color) <= sizeof(ulong))
        if_debug8m('v', dev->memory,
                   "[v]pdf14_mark_fill_rectangle, (%d, %d), %d x %d color = %lx  bm %d, nc %d, overprint %d\n",
                   x, y, w, h, (ulong)color, blend_mode, num_chan, overprint);
    else
        if_debug9m('v', dev->memory,
                   "[v]pdf14_mark_fill_rectangle, (%d, %d), %d x %d color = %08lx%08lx  bm %d, nc %d, overprint %d\n",
                   x, y, w, h,
                   (ulong)(color >> 8*(sizeof(color) - sizeof(ulong))), (ulong)color,
                   blend_mode, num_chan, overprint);
#endif
    /*
     * Unpack the gx_color_index values.  Complement the components for subtractive
     * color spaces.
     */

    if (devn) {
        if (has_tags) {
            curr_tag = pdc->tag;
        }
        if (additive) {
            for (j = 0; j < (num_comp - num_spots); j++) {
                src[j] = ((pdc->colors.devn.values[j]) >> shift & mask);
            }
            for (j = 0; j < num_spots; j++) {
                src[j + num_comp - num_spots] =
                    255 - ((pdc->colors.devn.values[j + num_comp - num_spots]) >> shift & mask);
            }
        } else {
            for (j = 0; j < num_comp; j++) {
                src[j] = 255 - ((pdc->colors.devn.values[j]) >> shift & mask);
            }
        }
    } else {
        if (has_tags) {
            curr_tag = (color >> (num_comp * 8)) & 0xff;
        }
        pdev->pdf14_procs->unpack_color(num_comp, color, pdev, src);
    }
    src_alpha = src[num_comp] = (byte)floor (255 * pdev->alpha + 0.5);
    if (has_shape)
        shape = (byte)floor (255 * pdev->shape + 0.5);
    /* Fit the mark into the bounds of the buffer */
    if (x < buf->rect.p.x) {
        w += x - buf->rect.p.x;
        x = buf->rect.p.x;
    }
    if (y < buf->rect.p.y) {
      h += y - buf->rect.p.y;
      y = buf->rect.p.y;
    }
    if (x + w > buf->rect.q.x) w = buf->rect.q.x - x;
    if (y + h > buf->rect.q.y) h = buf->rect.q.y - y;
    /* Update the dirty rectangle with the mark */
    if (x < buf->dirty.p.x) buf->dirty.p.x = x;
    if (y < buf->dirty.p.y) buf->dirty.p.y = y;
    if (x + w > buf->dirty.q.x) buf->dirty.q.x = x + w;
    if (y + h > buf->dirty.q.y) buf->dirty.q.y = y + h;
    dst_ptr = buf->data + (x - buf->rect.p.x) + (y - buf->rect.p.y) * rowstride;
    src_alpha = 255-src_alpha;
    shape = 255-shape;
    if (!has_alpha_g)
        alpha_g_off = 0;
    if (!has_shape)
        shape_off = 0;
    if (!has_tags)
        tag_off = 0;
    rowstride -= w;
    /* The num_comp == 1 && additive case is very common (mono output
     * devices no spot support), so we optimise that specifically here. */
    if (src[num_comp] == 0)
        fn = mark_fill_rect_alpha0;
    else if (additive && num_spots == 0) {
        if (num_comp == 1) {
            if (blend_mode == BLEND_MODE_Normal) {
                if (tag_off == 0 && shape_off == 0 &&  alpha_g_off == 0)
                    fn = mark_fill_rect_add1_no_spots_fast;
                else
                    fn = mark_fill_rect_add1_no_spots_normal;
            } else
                fn = mark_fill_rect_add1_no_spots;
        } else if (tag_off == 0 && shape_off == 0 && blend_mode == BLEND_MODE_Normal) {
            if (alpha_g_off == 0) {
                if (num_comp == 3)
                    fn = mark_fill_rect_add3_common;
                else
                    fn = mark_fill_rect_add_nospots_common_no_alpha_g;
            } else
                fn = mark_fill_rect_add_nospots_common;
        } else
            fn = mark_fill_rect_add_nospots;
    } else if (!additive && num_spots == 0 && num_comp == 4 &&
        first_blend_spot == 0 && blend_mode == BLEND_MODE_Normal &&
        !overprint && tag_off == 0 && alpha_g_off == 0 && shape_off == 0)
        fn = mark_fill_rect_sub4_fast;
    else
        fn = mark_fill_rect;

    fn(w, h, dst_ptr, src, num_comp, num_spots, first_blend_spot, src_alpha,
       rowstride, planestride, additive, pdev, blend_mode, overprint,
       drawn_comps, tag_off, curr_tag, alpha_g_off, shape_off, shape);

#if 0
/* #if RAW_DUMP */
    /* Dump the current buffer to see what we have. */

    if(global_index/10.0 == (int) (global_index/10.0) )
        dump_raw_buffer(pdev->ctx->mem,
                        pdev->ctx->stack->rect.q.y-pdev->ctx->stack->rect.p.y,
                        pdev->ctx->stack->rect.q.x-pdev->ctx->stack->rect.p.x,
                        pdev->ctx->stack->n_planes,
                        pdev->ctx->stack->planestride, pdev->ctx->stack->rowstride,
                        "Draw_Rect", pdev->ctx->stack->data, pdev->ctx->stack->deep);

    global_index++;
#endif
    return 0;
}

typedef void (*pdf14_mark_fill_rect16_fn)(int w, int h, uint16_t *gs_restrict dst_ptr, uint16_t *gs_restrict src, int num_comp, int num_spots, int first_blend_spot,
               uint16_t src_alpha, int rowstride, int planestride, bool additive, pdf14_device *pdev, gs_blend_mode_t blend_mode,
               bool overprint, gx_color_index drawn_comps, int tag_off, gs_graphics_type_tag_t curr_tag,
               int alpha_g_off, int shape_off, uint16_t shape);

static forceinline void
template_mark_fill_rect16(int w, int h, uint16_t *gs_restrict dst_ptr, uint16_t *gs_restrict src, int num_comp, int num_spots, int first_blend_spot,
               uint16_t src_alpha_, int rowstride, int planestride, bool additive, pdf14_device *pdev, gs_blend_mode_t blend_mode,
               bool overprint, gx_color_index drawn_comps, int tag_off, gs_graphics_type_tag_t curr_tag,
               int alpha_g_off, int shape_off, uint16_t shape_)
{
    int i, j, k;
    uint16_t dst[PDF14_MAX_PLANES] = { 0 };
    uint16_t dest_alpha;
    /* Expand src_alpha and shape to be 0...0x10000 rather than 0...0xffff */
    int src_alpha = src_alpha_ + (src_alpha_>>15);
    int shape = shape_ + (shape_>>15);
    bool tag_blend = blend_mode == BLEND_MODE_Normal ||
        blend_mode == BLEND_MODE_Compatible ||
        blend_mode == BLEND_MODE_CompatibleOverprint;

    for (j = h; j > 0; --j) {
        for (i = w; i > 0; --i) {
            if ((blend_mode == BLEND_MODE_Normal && src[num_comp] == 0xffff && !overprint) || dst_ptr[num_comp * planestride] == 0) {
                /* dest alpha is zero (or normal, and solid src) just use source. */
                if (additive) {
                    /* Hybrid case */
                    for (k = 0; k < (num_comp - num_spots); k++) {
                        dst_ptr[k * planestride] = src[k];
                    }
                    for (k = 0; k < num_spots; k++) {
                        dst_ptr[(k + num_comp - num_spots) * planestride] =
                                65535 - src[k + num_comp - num_spots];
                    }
                } else {
                    /* Pure subtractive */
                    for (k = 0; k < num_comp; k++) {
                        dst_ptr[k * planestride] = 65535 - src[k];
                    }
                }
                /* alpha */
                dst_ptr[num_comp * planestride] = src[num_comp];
            } else if (src[num_comp] != 0) {
                uint16_t *pdst;
                /* Complement subtractive planes */
                if (!additive) {
                    /* Pure subtractive */
                    for (k = 0; k < num_comp; ++k)
                        dst[k] = 65535 - dst_ptr[k * planestride];
                } else {
                    /* Hybrid case, additive with subtractive spots */
                    for (k = 0; k < (num_comp - num_spots); k++) {
                        dst[k] = dst_ptr[k * planestride];
                    }
                    for (k = 0; k < num_spots; k++) {
                        dst[k + num_comp - num_spots] =
                            65535 - dst_ptr[(k + num_comp - num_spots) * planestride];
                    }
                }
                dst[num_comp] = dst_ptr[num_comp * planestride];
                dest_alpha = dst[num_comp];
                pdst = art_pdf_composite_pixel_alpha_16_inline(dst, src, num_comp, blend_mode, first_blend_spot,
                            pdev->blend_procs, pdev);
                /* Post blend complement for subtractive and handling of drawncomps
                   if overprint.  We will have already done the compatible overprint
                   mode in the above composition */
                if (!additive && !overprint) {
                    /* Pure subtractive */
                    for (k = 0; k < num_comp; ++k)
                        dst_ptr[k * planestride] = 65535 - pdst[k];
                } else if (!additive && overprint) {
                    int comps;
                    /* If this is an overprint case, and alpha_r is different
                       than alpha_d then we will need to adjust
                       the colors of the non-drawn components here too */
                    if (dest_alpha != pdst[num_comp] && pdst[num_comp] != 0) {
                        /* dest_alpha > pdst[num_comp], and dst[num_comp] != 0.
                         * Therefore dest_alpha / pdst[num_comp] <= 65535 */
                        uint64_t scale = (uint64_t)65536 * dest_alpha / pdst[num_comp];
                        for (k = 0, comps = drawn_comps; comps != 0; ++k, comps >>= 1) {
                            if ((comps & 0x1) != 0) {
                                dst_ptr[k * planestride] = 65535 - pdst[k];
                            } else  {
                                /* We need val_new = (val_old * old_alpha) / new_alpha */
                                uint64_t val = (scale * (65535 - pdst[k]) + 32768)>>16;
                                if (val > 65535)
                                    val = 65535;
                                dst_ptr[k * planestride] = val;
                            }
                        }
                    } else {
                        for (k = 0, comps = drawn_comps; comps != 0; ++k, comps >>= 1) {
                            if ((comps & 0x1) != 0) {
                                dst_ptr[k * planestride] = 65535 - pdst[k];
                            }
                        }
                    }
                } else {
                    /* Hybrid case, additive with subtractive spots */
                    for (k = 0; k < (num_comp - num_spots); k++) {
                        dst_ptr[k * planestride] = pdst[k];
                    }
                    for (k = 0; k < num_spots; k++) {
                        dst_ptr[(k + num_comp - num_spots) * planestride] =
                            65535 - pdst[k + num_comp - num_spots];
                    }
                }
                /* The alpha channel */
                dst_ptr[num_comp * planestride] = pdst[num_comp];
            }
            if (tag_off) {
                /* If src alpha is 100% then set to curr_tag, else or */
                /* other than Normal BM, we always OR */
                if (src[num_comp] == 65535 && tag_blend) {
                    dst_ptr[tag_off] = curr_tag;
                } else {
                    dst_ptr[tag_off] |= curr_tag;
                }
            }
            if (alpha_g_off) {
                int tmp = (65535 - dst_ptr[alpha_g_off]) * src_alpha + 0x8000;
                dst_ptr[alpha_g_off] = 65535 - (tmp >> 16);
            }
            if (shape_off) {
                int tmp = (65535 - dst_ptr[shape_off]) * shape + 0x8000;
                dst_ptr[shape_off] = 65535 - (tmp >> 16);
            }
            ++dst_ptr;
        }
        dst_ptr += rowstride;
    }
}

static void
mark_fill_rect16_alpha0(int w, int h, uint16_t *gs_restrict dst_ptr, uint16_t *gs_restrict src, int num_comp, int num_spots, int first_blend_spot,
               uint16_t src_alpha_, int rowstride, int planestride, bool additive, pdf14_device *pdev, gs_blend_mode_t blend_mode,
               bool overprint, gx_color_index drawn_comps, int tag_off, gs_graphics_type_tag_t curr_tag,
               int alpha_g_off, int shape_off, uint16_t shape_)
{
    int i, j;
    int src_alpha = src_alpha_;
    int shape = shape_;

    src_alpha += src_alpha>>15;
    shape += shape>>15;
    for (j = h; j > 0; --j) {
        for (i = w; i > 0; --i) {
            if (alpha_g_off) {
                int tmp = (65535 - dst_ptr[alpha_g_off]) * src_alpha + 0x8000;
                dst_ptr[alpha_g_off] = 65535 - (tmp >> 16);
            }
            if (shape_off) {
                int tmp = (65535 - dst_ptr[shape_off]) * shape + 0x8000;
                dst_ptr[shape_off] = 65535 - (tmp >> 16);
            }
            ++dst_ptr;
        }
        dst_ptr += rowstride;
    }
}

static void
mark_fill_rect16(int w, int h, uint16_t *gs_restrict dst_ptr, uint16_t *gs_restrict src, int num_comp, int num_spots, int first_blend_spot,
               uint16_t src_alpha, int rowstride, int planestride, bool additive, pdf14_device *pdev, gs_blend_mode_t blend_mode,
               bool overprint, gx_color_index drawn_comps, int tag_off, gs_graphics_type_tag_t curr_tag,
               int alpha_g_off, int shape_off, uint16_t shape)
{
    template_mark_fill_rect16(w, h, dst_ptr, src, num_comp, num_spots, first_blend_spot,
               src_alpha, rowstride, planestride, additive, pdev, blend_mode,
               overprint, drawn_comps, tag_off, curr_tag,
               alpha_g_off, shape_off, shape);
}

static void
mark_fill_rect16_sub4_fast(int w, int h, uint16_t *gs_restrict dst_ptr, uint16_t *gs_restrict src, int num_comp, int num_spots, int first_blend_spot,
               uint16_t src_alpha, int rowstride, int planestride, bool additive, pdf14_device *pdev, gs_blend_mode_t blend_mode,
               bool overprint, gx_color_index drawn_comps, int tag_off, gs_graphics_type_tag_t curr_tag,
               int alpha_g_off, int shape_off, uint16_t shape)
{
    int i, j, k;

    for (j = h; j > 0; --j) {
        for (i = w; i > 0; --i) {
            uint16_t a_s = src[4];
            int a_b = dst_ptr[4 * planestride];
            if ((a_s == 0xffff) || a_b == 0) {
                /* dest alpha is zero (or normal, and solid src) just use source. */
                dst_ptr[0 * planestride] = 65535 - src[0];
                dst_ptr[1 * planestride] = 65535 - src[1];
                dst_ptr[2 * planestride] = 65535 - src[2];
                dst_ptr[3 * planestride] = 65535 - src[3];
                /* alpha */
                dst_ptr[4 * planestride] = a_s;
            } else if (a_s != 0) {
                /* Result alpha is Union of backdrop and source alpha */
                unsigned int tmp, src_scale;
                unsigned int a_r;

                a_b += a_b>>15;
                tmp = (0x10000 - a_b) * (0xffff - a_s) + 0x8000;
                a_r = 0xffff - (tmp >> 16);

                /* Compute a_s / a_r in 16.16 format */
                src_scale = ((a_s << 16) + (a_r >> 1)) / a_r;

                dst_ptr[4 * planestride] = a_r;

                src_scale >>= 1; /* Lose a bit to avoid overflow */
                /* Do simple compositing of source over backdrop */
                for (k = 0; k < 4; k++) {
                    int c_s = src[k];
                    int c_b = 65535 - dst_ptr[k * planestride];
                    tmp = src_scale * (c_s - c_b) + 0x4000;
                    dst_ptr[k * planestride] = 0xffff - c_b - (tmp >> 15);
                }
            }
            ++dst_ptr;
        }
        dst_ptr += rowstride;
    }
}

static void
mark_fill_rect16_add_nospots(int w, int h, uint16_t *gs_restrict dst_ptr, uint16_t *gs_restrict src, int num_comp, int num_spots, int first_blend_spot,
               uint16_t src_alpha, int rowstride, int planestride, bool additive, pdf14_device *pdev, gs_blend_mode_t blend_mode,
               bool overprint, gx_color_index drawn_comps, int tag_off, gs_graphics_type_tag_t curr_tag,
               int alpha_g_off, int shape_off, uint16_t shape)
{
    template_mark_fill_rect16(w, h, dst_ptr, src, num_comp, /*num_spots*/0, first_blend_spot,
               src_alpha, rowstride, planestride, /*additive*/1, pdev, blend_mode,
               /*overprint*/0, /*drawn_comps*/0, tag_off, curr_tag,
               alpha_g_off, shape_off, shape);
}

static void
mark_fill_rect16_add_nospots_common(int w, int h, uint16_t *gs_restrict dst_ptr, uint16_t *gs_restrict src, int num_comp, int num_spots, int first_blend_spot,
               uint16_t src_alpha, int rowstride, int planestride, bool additive, pdf14_device *pdev, gs_blend_mode_t blend_mode,
               bool overprint, gx_color_index drawn_comps, int tag_off, gs_graphics_type_tag_t curr_tag,
               int alpha_g_off, int shape_off, uint16_t shape)
{
    template_mark_fill_rect16(w, h, dst_ptr, src, num_comp, /*num_spots*/0, /*first_blend_spot*/0,
               src_alpha, rowstride, planestride, /*additive*/1, pdev, /*blend_mode*/BLEND_MODE_Normal,
               /*overprint*/0, /*drawn_comps*/0, /*tag_off*/0, curr_tag,
               alpha_g_off, /*shape_off*/0, shape);
}

static void
mark_fill_rect16_add_nospots_common_no_alpha_g(int w, int h, uint16_t *gs_restrict dst_ptr, uint16_t *gs_restrict src, int num_comp, int num_spots, int first_blend_spot,
               uint16_t src_alpha, int rowstride, int planestride, bool additive, pdf14_device *pdev, gs_blend_mode_t blend_mode,
               bool overprint, gx_color_index drawn_comps, int tag_off, gs_graphics_type_tag_t curr_tag,
               int alpha_g_off, int shape_off, uint16_t shape)
{
    template_mark_fill_rect16(w, h, dst_ptr, src, num_comp, /*num_spots*/0, /*first_blend_spot*/0,
               src_alpha, rowstride, planestride, /*additive*/1, pdev, /*blend_mode*/BLEND_MODE_Normal,
               /*overprint*/0, /*drawn_comps*/0, /*tag_off*/0, curr_tag,
               /*alpha_g_off*/0, /*shape_off*/0, shape);
}

static void
mark_fill_rect16_add3_common(int w, int h, uint16_t *gs_restrict dst_ptr, uint16_t *gs_restrict src, int num_comp, int num_spots, int first_blend_spot,
               uint16_t src_alpha, int rowstride, int planestride, bool additive, pdf14_device *pdev, gs_blend_mode_t blend_mode,
               bool overprint, gx_color_index drawn_comps, int tag_off, gs_graphics_type_tag_t curr_tag,
               int alpha_g_off, int shape_off, uint16_t shape)
{
    int i, j, k;

    for (j = h; j > 0; --j) {
        for (i = w; i > 0; --i) {
            uint16_t a_s = src[3];
            int a_b = dst_ptr[3 * planestride];
            if (a_s == 0xffff || a_b == 0) {
                /* dest alpha is zero (or solid source) just use source. */
                dst_ptr[0 * planestride] = src[0];
                dst_ptr[1 * planestride] = src[1];
                dst_ptr[2 * planestride] = src[2];
                /* alpha */
                dst_ptr[3 * planestride] = a_s;
            } else if (a_s != 0) {
                unsigned int tmp, src_scale;
                unsigned int a_r;

                a_b += a_b >> 15;
                /* Result alpha is Union of backdrop and source alpha */
                tmp = (0x10000 - a_b) * (0xffff - a_s) + 0x8000;
                a_r = 0xffff - (tmp >> 16);
                /* todo: verify that a_r is nonzero in all cases */

                /* Compute a_s / a_r in 16.16 format */
                src_scale = ((a_s << 16) + (a_r >> 1)) / a_r;

                dst_ptr[3 * planestride] = a_r;

                src_scale >>= 1; /* Lose a bit to avoid overflow */
                /* Do simple compositing of source over backdrop */
                for (k = 0; k < 3; k++) {
                    int c_s = src[k];
                    int c_b = dst_ptr[k * planestride];
                    tmp = src_scale * (c_s - c_b) + 0x4000;
                    dst_ptr[k * planestride] = c_b + (tmp >> 15);
                }
            }
            ++dst_ptr;
        }
        dst_ptr += rowstride;
    }
}

static void
mark_fill_rect16_add1_no_spots(int w, int h, uint16_t *gs_restrict dst_ptr, uint16_t *gs_restrict src, int num_comp, int num_spots, int first_blend_spot,
               uint16_t src_alpha_, int rowstride, int planestride, bool additive, pdf14_device *pdev, gs_blend_mode_t blend_mode,
               bool overprint, gx_color_index drawn_comps, int tag_off, gs_graphics_type_tag_t curr_tag,
               int alpha_g_off, int shape_off, uint16_t shape_)
{
    int i;
    int src_alpha = src_alpha_;
    int shape = shape_;
    bool tag_blend = blend_mode == BLEND_MODE_Normal ||
        blend_mode == BLEND_MODE_Compatible ||
        blend_mode == BLEND_MODE_CompatibleOverprint;

    src_alpha += src_alpha>>15;
    shape += shape>>15;
    for (; h > 0; --h) {
        for (i = w; i > 0; --i) {
            /* background empty, nothing to change, or solid source */
            uint16_t a_s = src[1];
            if ((blend_mode == BLEND_MODE_Normal && a_s == 0xffff) || dst_ptr[planestride] == 0) {
                dst_ptr[0] = src[0];
                dst_ptr[planestride] = a_s;
            } else {
                art_pdf_composite_pixel_alpha_16_fast_mono(dst_ptr, src,
                                                blend_mode, pdev->blend_procs,
                                                planestride, pdev);
            }
            if (tag_off) {
                /* If src alpha is 100% then set to curr_tag, else or */
                /* other than Normal BM, we always OR */
                if (tag_blend && a_s == 65535) {
                     dst_ptr[tag_off] = curr_tag;
                } else {
                    dst_ptr[tag_off] |= curr_tag;
                }
            }
            if (alpha_g_off) {
                int tmp = (65535 - dst_ptr[alpha_g_off]) * src_alpha + 0x8000;
                dst_ptr[alpha_g_off] = 65535 - (tmp >> 16);
            }
            if (shape_off) {
                int tmp = (65535 - dst_ptr[shape_off]) * shape + 0x8000;
                dst_ptr[shape_off] = 65535 - (tmp >> 16);
            }
            ++dst_ptr;
        }
        dst_ptr += rowstride;
    }
}

static void
mark_fill_rect16_add1_no_spots_normal(int w, int h, uint16_t *gs_restrict dst_ptr, uint16_t *gs_restrict src, int num_comp, int num_spots, int first_blend_spot,
               uint16_t src_alpha_, int rowstride, int planestride, bool additive, pdf14_device *pdev, gs_blend_mode_t blend_mode,
               bool overprint, gx_color_index drawn_comps, int tag_off, gs_graphics_type_tag_t curr_tag,
               int alpha_g_off, int shape_off, uint16_t shape_)
{
    int i;
    int src_alpha = src_alpha_;
    int shape = shape_;

    src_alpha += src_alpha>>15;
    shape += shape>>15;

    for (; h > 0; --h) {
        for (i = w; i > 0; --i) {
            /* background empty, nothing to change, or solid source */
            uint16_t a_s = src[1];
            uint16_t a_b = dst_ptr[planestride];
            if (a_s == 0xffff || a_b == 0) {
                dst_ptr[0] = src[0];
                dst_ptr[planestride] = a_s;
            } else {
                /* Result alpha is Union of backdrop and source alpha */
                unsigned int tmp, src_scale;
                unsigned int a_r;
                int c_s, c_b;

                a_b += a_b>>15;
                tmp = (0x10000 - a_b) * (0xffff - a_s) + 0x8000;
                a_r = 0xffff - (tmp >> 16);

                /* Compute a_s / a_r in 16.16 format */
                src_scale = ((a_s << 16) + (a_r >> 1)) / a_r;

                src_scale >>= 1; /* Lose a bit to avoid overflow */
                /* Do simple compositing of source over backdrop */
                c_s = src[0];
                c_b = dst_ptr[0];
                tmp = src_scale * (c_s - c_b) + 0x4000;
                dst_ptr[0] = c_b + (tmp >> 15);
                dst_ptr[planestride] = a_r;
            }
            if (tag_off) {
                /* If src alpha is 100% then set to curr_tag, else or */
                /* other than Normal BM, we always OR */
                if (a_s == 65535) {
                     dst_ptr[tag_off] = curr_tag;
                } else {
                    dst_ptr[tag_off] |= curr_tag;
                }
            }
            if (alpha_g_off) {
                int tmp = (65535 - dst_ptr[alpha_g_off]) * src_alpha + 0x8000;
                dst_ptr[alpha_g_off] = 65535 - (tmp >> 16);
            }
            if (shape_off) {
                int tmp = (65535 - dst_ptr[shape_off]) * shape + 0x8000;
                dst_ptr[shape_off] = 65535 - (tmp >> 16);
            }
            ++dst_ptr;
        }
        dst_ptr += rowstride;
    }
}

static void
mark_fill_rect16_add1_no_spots_fast(int w, int h, uint16_t *gs_restrict dst_ptr, uint16_t *gs_restrict src, int num_comp, int num_spots, int first_blend_spot,
               uint16_t src_alpha, int rowstride, int planestride, bool additive, pdf14_device *pdev, gs_blend_mode_t blend_mode,
               bool overprint, gx_color_index drawn_comps, int tag_off, gs_graphics_type_tag_t curr_tag,
               int alpha_g_off, int shape_off, uint16_t shape)
{
    int i;

    for (; h > 0; --h) {
        for (i = w; i > 0; --i) {
            /* background empty, nothing to change, or solid source */
            uint16_t a_s = src[1];
            int a_b = dst_ptr[planestride];
            if (a_s == 0xffff || a_b == 0) {
                dst_ptr[0] = src[0];
                dst_ptr[planestride] = a_s;
            } else if (a_s != 0) {
                /* Result alpha is Union of backdrop and source alpha */
                unsigned int tmp, src_scale;
                unsigned int a_r;
                int c_s, c_b;

                a_b += a_b>>15;
                tmp = (0x10000 - a_b) * (0xffff - a_s) + 0x8000;
                a_r = 0xffff - (tmp >> 16);

                /* Compute a_s / a_r in 16.16 format */
                src_scale = ((a_s << 16) + (a_r >> 1)) / a_r;

                src_scale >>= 1; /* Lose a bit to avoid overflow */
                /* Do simple compositing of source over backdrop */
                c_s = src[0];
                c_b = dst_ptr[0];
                tmp = src_scale * (c_s - c_b) + 0x4000;
                dst_ptr[0] = c_b + (tmp >> 15);
                dst_ptr[planestride] = a_r;
            }
            ++dst_ptr;
        }
        dst_ptr += rowstride;
    }
}

static int
do_mark_fill_rectangle16(gx_device * dev, int x, int y, int w, int h,
                         gx_color_index color, const gx_device_color *pdc,
                         bool devn)
{
    pdf14_device *pdev = (pdf14_device *)dev;
    pdf14_buf *buf = pdev->ctx->stack;
    int j;
    uint16_t *dst_ptr;
    uint16_t src[PDF14_MAX_PLANES];
    gs_blend_mode_t blend_mode = pdev->blend_mode;
    bool additive = pdev->ctx->additive;
    int rowstride = buf->rowstride;
    int planestride = buf->planestride;
    gs_graphics_type_tag_t curr_tag = GS_UNKNOWN_TAG; /* Quite compiler */
    bool has_alpha_g = buf->has_alpha_g;
    bool has_shape = buf->has_shape;
    bool has_tags = buf->has_tags;
    int num_chan = buf->n_chan;
    int num_comp = num_chan - 1;
    int shape_off = num_chan * planestride;
    int alpha_g_off = shape_off + (has_shape ? planestride : 0);
    int tag_off = alpha_g_off + (has_alpha_g ? planestride : 0);
    bool overprint = pdev->op_state == PDF14_OP_STATE_FILL ? pdev->overprint : pdev->stroke_overprint;
    gx_color_index drawn_comps = pdev->op_state == PDF14_OP_STATE_FILL ?
                                 pdev->drawn_comps_fill : pdev->drawn_comps_stroke;
    uint16_t shape = 0; /* Quiet compiler. */
    uint16_t src_alpha;
    int num_spots = buf->num_spots;
    int first_blend_spot = num_comp;
    pdf14_mark_fill_rect16_fn fn;

   /* If we are going out to a CMYK or CMYK + spots pdf14 device (i.e.
   subtractive) and we are doing overprint with drawn_comps == 0
   then this is a no-operation */
    if (overprint && drawn_comps == 0 && !buf->group_color_info->isadditive)
        return 0;

  /* This is a fix to handle the odd case where overprint is active
   but drawn comps is zero due to the colorants that are present
   in the sep or devicen color space.  For example, if the color
   fill was cyan in a sep color space but we are drawing in a
   RGB blend space.  In this case the drawn comps is 0 and we should
   not be using compatible overprint mode here. */
    if (drawn_comps == 0 && blend_mode == BLEND_MODE_CompatibleOverprint &&
        buf->group_color_info->isadditive) {
        blend_mode = BLEND_MODE_Normal;
    }

    if (num_spots > 0 && !blend_valid_for_spot(blend_mode))
        first_blend_spot = num_comp - num_spots;
    if (blend_mode == BLEND_MODE_Normal)
        first_blend_spot = 0;

    if (buf->data == NULL)
        return 0;
    /* NB: gx_color_index is 4 or 8 bytes */
#if 0
    if (sizeof(color) <= sizeof(ulong))
        if_debug8m('v', dev->memory,
                   "[v]pdf14_mark_fill_rectangle, (%d, %d), %d x %d color = %lx  bm %d, nc %d, overprint %d\n",
                   x, y, w, h, (ulong)color, blend_mode, num_chan, overprint);
    else
        if_debug9m('v', dev->memory,
                   "[v]pdf14_mark_fill_rectangle, (%d, %d), %d x %d color = %08lx%08lx  bm %d, nc %d, overprint %d\n",
                   x, y, w, h,
                   (ulong)(color >> 8*(sizeof(color) - sizeof(ulong))), (ulong)color,
                   blend_mode, num_chan, overprint);
#endif
    /*
     * Unpack the gx_color_index values.  Complement the components for subtractive
     * color spaces.
     */
    if (devn) {
        if (has_tags) {
            curr_tag = pdc->tag;
        }
        if (additive) {
            for (j = 0; j < (num_comp - num_spots); j++) {
                src[j] = pdc->colors.devn.values[j];
            }
            for (j = 0; j < num_spots; j++) {
                src[j + num_comp - num_spots] =
                    65535 - pdc->colors.devn.values[j + num_comp - num_spots];
            }
        } else {
            for (j = 0; j < num_comp; j++) {
                src[j] = 65535 - pdc->colors.devn.values[j];
            }
        }
    } else {
        if (has_tags) {
            curr_tag = (color >> (num_comp * 16)) & 0xff;
        }
        pdev->pdf14_procs->unpack_color16(num_comp, color, pdev, src);
    }
    src_alpha = src[num_comp] = (uint16_t)floor (65535 * pdev->alpha + 0.5);
    if (has_shape)
        shape = (uint16_t)floor (65535 * pdev->shape + 0.5);
    /* Fit the mark into the bounds of the buffer */
    if (x < buf->rect.p.x) {
        w += x - buf->rect.p.x;
        x = buf->rect.p.x;
    }
    if (y < buf->rect.p.y) {
      h += y - buf->rect.p.y;
      y = buf->rect.p.y;
    }
    if (x + w > buf->rect.q.x) w = buf->rect.q.x - x;
    if (y + h > buf->rect.q.y) h = buf->rect.q.y - y;
    /* Update the dirty rectangle with the mark */
    if (x < buf->dirty.p.x) buf->dirty.p.x = x;
    if (y < buf->dirty.p.y) buf->dirty.p.y = y;
    if (x + w > buf->dirty.q.x) buf->dirty.q.x = x + w;
    if (y + h > buf->dirty.q.y) buf->dirty.q.y = y + h;
    dst_ptr = (uint16_t *)(buf->data + (x - buf->rect.p.x) * 2 + (y - buf->rect.p.y) * rowstride);
    src_alpha = 65535-src_alpha;
    shape = 65535-shape;
    if (!has_alpha_g)
        alpha_g_off = 0;
    if (!has_shape)
        shape_off = 0;
    if (!has_tags)
        tag_off = 0;
    rowstride -= w<<1;
    /* The num_comp == 1 && additive case is very common (mono output
     * devices no spot support), so we optimise that specifically here. */
    if (src[num_comp] == 0)
        fn = mark_fill_rect16_alpha0;
    else if (additive && num_spots == 0) {
        if (num_comp == 1) {
            if (blend_mode == BLEND_MODE_Normal) {
                if (tag_off == 0 && shape_off == 0 &&  alpha_g_off == 0)
                    fn = mark_fill_rect16_add1_no_spots_fast;
                else
                    fn = mark_fill_rect16_add1_no_spots_normal;
            } else
                fn = mark_fill_rect16_add1_no_spots;
        } else if (tag_off == 0 && shape_off == 0 && blend_mode == BLEND_MODE_Normal) {
            if (alpha_g_off == 0) {
                if (num_comp == 3)
                    fn = mark_fill_rect16_add3_common;
                else
                    fn = mark_fill_rect16_add_nospots_common_no_alpha_g;
            } else
                fn = mark_fill_rect16_add_nospots_common;
        } else
            fn = mark_fill_rect16_add_nospots;
    } else if (!additive && num_spots == 0 && num_comp == 4 && num_spots == 0 &&
        first_blend_spot == 0 && blend_mode == BLEND_MODE_Normal &&
        !overprint && tag_off == 0 && alpha_g_off == 0 && shape_off == 0)
        fn = mark_fill_rect16_sub4_fast;
    else
        fn = mark_fill_rect16;

    /* Pass values as array offsets, not byte diffs */
    rowstride >>= 1;
    planestride >>= 1;
    tag_off >>= 1;
    alpha_g_off >>= 1;
    shape_off >>= 1;
    fn(w, h, dst_ptr, src, num_comp, num_spots, first_blend_spot, src_alpha,
       rowstride, planestride, additive, pdev, blend_mode, overprint,
       drawn_comps, tag_off, curr_tag, alpha_g_off, shape_off, shape);

#if 0
/* #if RAW_DUMP */
    /* Dump the current buffer to see what we have. */

    if(global_index/10.0 == (int) (global_index/10.0) )
        dump_raw_buffer(pdev->ctx->mem,
                        pdev->ctx->stack->rect.q.y-pdev->ctx->stack->rect.p.y,
                        pdev->ctx->stack->rect.q.x-pdev->ctx->stack->rect.p.x,
                        pdev->ctx->stack->n_planes,
                        pdev->ctx->stack->planestride, pdev->ctx->stack->rowstride,
                        "Draw_Rect", pdev->ctx->stack->data, pdev->ctx->stack->deep);

    global_index++;
#endif
    return 0;
}

int
pdf14_mark_fill_rectangle(gx_device * dev, int x, int y, int w, int h,
                          gx_color_index color, const gx_device_color *pdc,
                          bool devn)
{
    pdf14_device *pdev = (pdf14_device *)dev;
    pdf14_buf *buf = pdev->ctx->stack;

    if (buf->deep)
        return do_mark_fill_rectangle16(dev, x, y, w, h, color, pdc, devn);
    else
        return do_mark_fill_rectangle(dev, x, y, w, h, color, pdc, devn);
}

/* Keep this at the end because of the #undef print */

#ifdef TRACK_COMPOSE_GROUPS
static void
dump_track_compose_groups(void)
{
    int i;

    for (i = 0; i < (1<<17); i++)
    {
        if (compose_groups[i] == 0)
            continue;
#undef printf
        printf("COMPOSE_GROUPS: %04x:%d\n", i, compose_groups[i]);
    }
}
#endif
