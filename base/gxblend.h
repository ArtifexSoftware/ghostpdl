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

#ifndef gxblend_INCLUDED
#  define gxblend_INCLUDED

#include "gxcindex.h"
#include "gxcvalue.h"
#include "gxfrac.h"
#include "gxdevcli.h"

#define RAW_DUMP 0
/* We now dump as PAMs, not raws. This is because it's much easier
 * (for me at least) to view them using:
 *   http://ghostscript.com/~robin/pamview.html
 * than anything else. */

/* #define DUMP_TO_PNG */

#define	PDF14_MAX_PLANES GX_DEVICE_COLOR_MAX_COMPONENTS+3  /* Needed for alpha channel, shape, group alpha */

typedef bits16 ArtPixMaxDepth;

#define ART_MAX_CHAN GX_DEVICE_COLOR_MAX_COMPONENTS

typedef struct pdf14_device_s pdf14_device;

typedef struct pdf14_buf_s pdf14_buf;

typedef struct gs_separations_s gs_separations;

/*
 * This structure contains procedures for processing which differ
 * between the different blending color spaces.
 *
 * The Luminosity, Color, Saturation, and Hue blend modes depend
 * upon the blending color space.  Currently the blending color space
 * matches the process color model of the compositing device.  We need
 * two routines to implement the four 'non separable' blend modes.
 */
typedef struct {
    /*
     * Perform luminosity and color blending.  (Also used for hue blending.)
     */
    void (* blend_luminosity)(int n_chan, byte *gs_restrict dst,
                    const byte *gs_restrict backdrop, const byte *gs_restrict src);
    /*
     * Perform saturation blending.  (Also used for hue blending.)
     */
    void (* blend_saturation)(int n_chan, byte *gs_restrict dst,
                    const byte *gs_restrict backdrop, const byte *gs_restrict src);
    /* And 16 bit variants */
    void (* blend_luminosity16)(int n_chan, uint16_t *gs_restrict dst,
                    const uint16_t *gs_restrict backdrop, const uint16_t *gs_restrict src);
    void (* blend_saturation16)(int n_chan, uint16_t *gs_restrict dst,
                    const uint16_t *gs_restrict backdrop, const uint16_t *gs_restrict src);
} pdf14_nonseparable_blending_procs_s;

typedef pdf14_nonseparable_blending_procs_s
                pdf14_nonseparable_blending_procs_t;

/* This is used to so that we can change procedures based
 * upon the Smask color space. previously we always
 *  went to the device space */

typedef struct {

    pdf14_nonseparable_blending_procs_t device_procs;
    gx_device_procs color_mapping_procs;

} pdf14_parent_cs_params_s;

typedef pdf14_parent_cs_params_s pdf14_parent_cs_params_t;

/* This function is used for mapping Smask CMYK or RGB data to a monochrome alpha buffer */
void smask_luminosity_mapping(int num_rows, int num_cols, int n_chan, int row_stride,
                              int plane_stride, const byte *gs_restrict src, byte *gs_restrict des, bool isadditive,
                              gs_transparency_mask_subtype_t SMask_SubType, bool deep
#if RAW_DUMP
                              , const gs_memory_t *mem
#endif
                              );
void smask_blend(byte *gs_restrict src, int width, int height, int rowstride,
                 int planestride, bool deep);

void smask_copy(int num_rows, int num_cols, int row_stride,
                         byte *gs_restrict src, const byte *gs_restrict des);
int smask_icc(gx_device *dev, int num_rows, int num_cols, int n_chan,
               int row_stride, int plane_stride, byte *gs_restrict src, const byte *gs_restrict des,
               gsicc_link_t *icclink, bool deep);
/* For spot colors, blend modes must be white preserving and separable */
bool blend_valid_for_spot(gs_blend_mode_t blend_mode);

/**
 * art_blend_pixel_8: Compute PDF 1.4 blending function on 8-bit pixels.
 * @dst: Where to store resulting pixel.
 * @backdrop: Backdrop pixel color.
 * @src: Source pixel color.
 * @n_chan: Number of channels.
 * @blend_mode: Blend mode.
 * @pblend_procs: Procs for handling non separable blending modes.
 * @p14dev: pdf14 device.  Needed for handling CompatibleOverprint mode
 *
 * Computes the blend of two pixels according the PDF 1.4 transparency
 * spec (section 3.2, Blend Mode). A few things to keep in mind about
 * this implementation:
 *
 * 1. This is a reference implementation, not a high-performance one.
 * Blending using this function will incur a function call and switch
 * statement per pixel, and will also incur the extra cost of 16 bit
 * math.
 *
 * 2. Zero is black, one is white. In a subtractive color space such
 * as CMYK, all pixels should be represented as "complemented", as
 * described in section 3.1 (Blending Color Space) of the PDF 1.4
 * transparency spec.
 *
 * 3. I haven't really figured out how to handle the Compatible blend
 * mode. I wouldn't be surprised if it required an API change.
 **/
void
art_blend_pixel_8(byte *gs_restrict dst, const byte *gs_restrict backdrop,
                const byte *gs_restrict src, int n_chan, gs_blend_mode_t blend_mode,
                const pdf14_nonseparable_blending_procs_t * pblend_procs,
                pdf14_device *p14dev);

void
art_blend_pixel_16(uint16_t *gs_restrict dst, const uint16_t *gs_restrict backdrop,
                   const uint16_t *gs_restrict src, int n_chan, gs_blend_mode_t blend_mode,
                   const pdf14_nonseparable_blending_procs_t * pblend_procs,
                   pdf14_device *p14dev);

#ifdef UNUSED
/**
 * art_pdf_union_8: Union together two alpha values.
 * @alpha1: One alpha value.
 * @alpha2: Another alpha value.
 *
 * Return value: Union (@alpha1, @alpha2).
 **/
byte art_pdf_union_8(byte alpha1, byte alpha2);
#endif

/**
 * art_pdf_union_mul_8: Union together two alpha values, with mask.
 * @alpha1: One alpha value.
 * @alpha2: Another alpha value.
 * @alpha_mask: A mask alpha value;
 *
 * Return value: Union (@alpha1, @alpha2 * @alpha_mask).
 **/
/* byte art_pdf_union_mul_8(byte alpha1, byte alpha2, byte alpha_mask); */

static inline byte
art_pdf_union_mul_8(byte alpha1, byte alpha2, byte alpha_mask)
{
    int tmp;
    if (alpha_mask != 0xff)
    {
        tmp = alpha2 * alpha_mask + 0x80;
        alpha2 = (tmp + (tmp >> 8))>>8;
    }
    tmp = (0xff - alpha1) * (0xff - alpha2) + 0x80;
    return 0xff - ((tmp + (tmp >> 8)) >> 8);
}

static inline uint16_t
art_pdf_union_mul_16(uint16_t alpha1, uint16_t alpha2_, uint16_t alpha_mask)
{
    int alpha2 = alpha2_;
    if (alpha_mask != 0xffff)
    {
        int am = alpha_mask + (alpha_mask>>15);
        alpha2 = (alpha2 * am + 0x8000)>>16;
    }
    alpha2 += alpha2>>15;
    alpha2 = ((0xffff - alpha1) * (0x10000 - alpha2) + 0x8000)>>16;
    return 0xffff - (alpha2 >> 16);
}

/**
 * art_pdf_composite_pixel_alpha_8: Composite two alpha pixels.
 * @dst: Where to store resulting pixel, also initially backdrop color.
 * @src: Source pixel color.
 * @n_chan: Number of channels.
 * @blend_mode: Blend mode.
 * @pblend_procs: Procs for handling non separable blending modes.
 * @p14dev: pdf14 device.
 *
 * Composites two pixels using the basic compositing operation. A few
 * things to keep in mind:
 *
 * 1. This is a reference implementation, not a high-performance one.
 *
 * 2. All pixels are assumed to have a single alpha channel.
 *
 * 3. Zero is black, one is white.
 *
 * The first "first_spot" channels are blended with blend_mode. The
 * remaining channels are blended with BLEND_MODE_Normal.
 *
 * Also note that src and dst are expected to be allocated aligned to
 * 32 bit boundaries, ie bytes from [0] to [(n_chan + 3) & -4] may
 * be accessed.
 **/
void
art_pdf_composite_pixel_alpha_8(byte *gs_restrict dst, const byte *gs_restrict src, int n_chan,
        gs_blend_mode_t blend_mode, int first_spot,
        const pdf14_nonseparable_blending_procs_t * pblend_procs,
        pdf14_device *p14dev);

void
art_pdf_composite_pixel_alpha_16(uint16_t *gs_restrict dst, const uint16_t *gs_restrict src, int n_chan,
        gs_blend_mode_t blend_mode, int first_spot,
        const pdf14_nonseparable_blending_procs_t * pblend_procs,
        pdf14_device *p14dev);

/**
 * art_pdf_composite_knockout_8: knockout compositing.
 * @dst: Destination pixel array -- has been initialized with background
 * @src: Source pixel.
 * n_chan: Number of channels.
 * p14dev: pdf14 device
 *
 * This function handles the knockout case: an isolated knockout group,
 * and an elementary shape. The alpha channel of @src is interpreted as shape.
 **/
void
art_pdf_composite_knockout_8(byte *gs_restrict dst,
                                    const byte *gs_restrict src,
                                    int n_chan,
                                    gs_blend_mode_t blend_mode,
                                    const pdf14_nonseparable_blending_procs_t * pblend_procs,
                                    pdf14_device *p14dev);

void
art_pdf_composite_knockout_16(uint16_t *gs_restrict dst,
                                    const uint16_t *gs_restrict src,
                                    int n_chan,
                                    gs_blend_mode_t blend_mode,
                                    const pdf14_nonseparable_blending_procs_t * pblend_procs,
                                    pdf14_device *p14dev);

/**
 * art_pdf_knockoutisolated_group_8: Knockout for isolated group.
 * @dst: Destination pixel.
 * @src: Source pixel.
 * @n_chan: Number of channels.
 *
 * This function handles the simple case with an isolated knockout group.
 **/
void
art_pdf_knockoutisolated_group_8(byte *gs_restrict dst, const byte *gs_restrict src, int n_chan);

void
art_pdf_knockoutisolated_group_16(uint16_t *gs_restrict dst, const uint16_t *gs_restrict src, int n_chan);

/**
* art_pdf_knockoutisolated_group_8: Knockout for isolated group.
* @dst: Destination pixel.
* @src: Source pixel.
* @src_alpha: current alpha from the graphic state
* @aa_alpha:  alpha coming from the anti-aliasing buffer
* @n_chan: Number of channels.
* @p14dev: pdf14 device
*
* This function handles the simple case with an isolated knockout group but where
* we have an alpha from AA and from the current graphic state.
**/
void
art_pdf_knockoutisolated_group_aa_8(byte *gs_restrict dst, const byte *gs_restrict src, byte src_alpha,
    byte aa_alpha, int n_chan, pdf14_device *p14dev);

/*
 * Routines for handling the non separable blending modes.
 */
/* RGB blending color space */
void art_blend_luminosity_rgb_8(int n_chan, byte *gs_restrict dst, const byte *gs_restrict backdrop,
                           const byte *gs_restrict src);
void art_blend_saturation_rgb_8(int n_chan, byte *gs_restrict dst, const byte *gs_restrict backdrop,
                           const byte *gs_restrict src);
void art_blend_luminosity_rgb_16(int n_chan, uint16_t *gs_restrict dst, const uint16_t *gs_restrict backdrop,
                           const uint16_t *gs_restrict src);
void art_blend_saturation_rgb_16(int n_chan, uint16_t *gs_restrict dst, const uint16_t *gs_restrict backdrop,
                           const uint16_t *gs_restrict src);
/* CMYK and CMYK + spot blending color space */
void art_blend_saturation_cmyk_8(int n_chan, byte *gs_restrict dst, const byte *gs_restrict backdrop,
                           const byte *gs_restrict src);
void art_blend_luminosity_cmyk_8(int n_chan, byte *gs_restrict dst, const byte *gs_restrict backdrop,
                           const byte *gs_restrict src);
void art_blend_saturation_cmyk_16(int n_chan, uint16_t *gs_restrict dst, const uint16_t *gs_restrict backdrop,
                           const uint16_t *gs_restrict src);
void art_blend_luminosity_cmyk_16(int n_chan, uint16_t *gs_restrict dst, const uint16_t *gs_restrict backdrop,
                           const uint16_t *gs_restrict src);
/* 'Custom' i.e. unknown blending color space. */
void art_blend_luminosity_custom_8(int n_chan, byte *gs_restrict dst, const byte *gs_restrict backdrop,
                           const byte *gs_restrict src);
void art_blend_saturation_custom_8(int n_chan, byte *gs_restrict dst, const byte *gs_restrict backdrop,
                           const byte *gs_restrict src);
void art_blend_luminosity_custom_16(int n_chan, uint16_t *gs_restrict dst, const uint16_t *gs_restrict backdrop,
                           const uint16_t *gs_restrict src);
void art_blend_saturation_custom_16(int n_chan, uint16_t *gs_restrict dst, const uint16_t *gs_restrict backdrop,
                           const uint16_t *gs_restrict src);

void pdf14_unpack_rgb_mix(int num_comp, gx_color_index color,
                          pdf14_device * p14dev, byte * out);
void pdf14_unpack_gray_mix(int num_comp, gx_color_index color,
                           pdf14_device * p14dev, byte * out);
void pdf14_unpack_additive(int num_comp, gx_color_index color,
                           pdf14_device * p14dev, byte * out);
void pdf14_unpack_subtractive(int num_comp, gx_color_index color,
                              pdf14_device * p14dev, byte * out);
void pdf14_unpack_custom(int num_comp, gx_color_index color,
                         pdf14_device * p14dev, byte * out);

void pdf14_unpack16_rgb_mix(int num_comp, gx_color_index color,
                            pdf14_device * p14dev, uint16_t * out);
void pdf14_unpack16_gray_mix(int num_comp, gx_color_index color,
                             pdf14_device * p14dev, uint16_t * out);
void pdf14_unpack16_additive(int num_comp, gx_color_index color,
                             pdf14_device * p14dev, uint16_t * out);
void pdf14_unpack16_subtractive(int num_comp, gx_color_index color,
                                pdf14_device * p14dev, uint16_t * out);
void pdf14_unpack16_custom(int num_comp, gx_color_index color,
                           pdf14_device * p14dev, uint16_t * out);

void pdf14_preserve_backdrop(pdf14_buf *buf, pdf14_buf *tos, bool knockout_buff
#if RAW_DUMP
                             , gs_memory_t *mem
#endif
                             );

int pdf14_preserve_backdrop_cm(pdf14_buf *buf, cmm_profile_t *group_profile,
                               pdf14_buf *tos, cmm_profile_t *tos_profile,
                               gs_memory_t *memory, gs_gstate *pgs,
                               gx_device *dev, bool knockout_buff);

void pdf14_compose_group(pdf14_buf *tos, pdf14_buf *nos, pdf14_buf *maskbuf,
              int x0, int x1, int y0, int y1, int n_chan, bool additive,
              const pdf14_nonseparable_blending_procs_t * pblend_procs,
              bool has_matte, bool overprint, gx_color_index drawn_comps,
              gs_memory_t *memory, gx_device *dev);

void pdf14_compose_alphaless_group(pdf14_buf *tos, pdf14_buf *nos,
                                   int x0, int x1, int y0, int y1,
                                   gs_memory_t *memory, gx_device *dev);

gx_color_index pdf14_encode_color(gx_device *dev, const gx_color_value colors[]);
gx_color_index pdf14_encode_color_tag(gx_device *dev, const gx_color_value colors[]);
gx_color_index pdf14_encode_color16(gx_device *dev, const gx_color_value colors[]);
gx_color_index pdf14_encode_color16_tag(gx_device *dev, const gx_color_value colors[]);

int pdf14_decode_color(gx_device * dev, gx_color_index color, gx_color_value * out);
int pdf14_decode_color16(gx_device * dev, gx_color_index color, gx_color_value * out);
void pdf14_gray_cs_to_cmyk_cm(const gx_device * dev, frac gray, frac out[]);
void pdf14_rgb_cs_to_cmyk_cm(const gx_device * dev, const gs_gstate *pgs,
                           frac r, frac g, frac b, frac out[]);
void pdf14_cmyk_cs_to_cmyk_cm(const gx_device * dev, frac c, frac m, frac y, frac k, frac out[]);

void pdf14_gray_cs_to_rgbspot_cm(const gx_device * dev, frac gray, frac out[]);
void pdf14_rgb_cs_to_rgbspot_cm(const gx_device * dev, const gs_gstate *pgs,
    frac r, frac g, frac b, frac out[]);
void pdf14_cmyk_cs_to_rgbspot_cm(const gx_device * dev, frac c, frac m, frac y, frac k, frac out[]);

void pdf14_gray_cs_to_grayspot_cm(const gx_device * dev, frac gray, frac out[]);
void pdf14_rgb_cs_to_grayspot_cm(const gx_device * dev, const gs_gstate *pgs,
    frac r, frac g, frac b, frac out[]);
void pdf14_cmyk_cs_to_grayspot_cm(const gx_device * dev, frac c, frac m, frac y, frac k, frac out[]);

void gx_build_blended_image_row(const byte *gs_restrict buf_ptr, int planestride,
                                int width, int num_comp, uint16_t bg, byte *gs_restrict linebuf);
void gx_build_blended_image_row16(const byte *gs_restrict buf_ptr, int planestride,
                                  int width, int num_comp, uint16_t bg, byte *gs_restrict linebuf);
void gx_blend_image_buffer(byte *buf_ptr, int width, int height,
                      int rowstride, int planestride, int num_comp, byte bg);
void gx_blend_image_buffer16(byte *buf_ptr, int width, int height,
    int rowstride, int planestride, int num_comp, uint16_t bg, bool keep_native);
void gx_blend_image_buffer8to16(const byte *buf_ptr, unsigned short *buf_ptr_out,
    int width, int height, int rowstride, int planestride, int num_comp, byte bg);
int gx_put_blended_image_custom(gx_device *target, byte *buf_ptr,
                      int planestride, int rowstride,
                      int x0, int y0, int width, int height, int num_comp, uint16_t bg, bool deep);

/* Function moved between compilation units to allow inlining. */
int pdf14_mark_fill_rectangle(gx_device * dev, int x, int y, int w, int h,
                                     gx_color_index color, const gx_device_color *pdc,
                                     bool devn);



#if RAW_DUMP

void dump_raw_buffer(const gs_memory_t *mem,
                     int num_rows, int width, int n_chan,
                     int plane_stride, int rowstride,
                     char filename[],const byte *Buffer, bool deep);
void dump_raw_buffer_be(const gs_memory_t *mem,
                        int num_rows, int width, int n_chan,
                        int plane_stride, int rowstride,
                        char filename[],const byte *Buffer, bool deep);
#endif

#endif /* gxblend_INCLUDED */
