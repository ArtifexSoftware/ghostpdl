/* Copyright (C) 2001-2021 Artifex Software, Inc.
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

/* Compositing devices for implementing	PDF 1.4	imaging	model */

#include "assert_.h"
#include "math_.h"
#include "memory_.h"
#include "gx.h"
#include "gserrors.h"
#include "gscdefs.h"
#include "gxdevice.h"
#include "gsdevice.h"
#include "gsstruct.h"
#include "gxgstate.h"
#include "gxdcolor.h"
#include "gxiparam.h"
#include "gstparam.h"
#include "gxblend.h"
#include "gxtext.h"
#include "gsimage.h"
#include "gsrect.h"
#include "gscoord.h"
#include "gzstate.h"
#include "gdevdevn.h"
#include "gdevmem.h"
#include "gdevp14.h"
#include "gdevprn.h"		/* for prn_device structures */
#include "gdevppla.h"		/* for gdev_prn_open_planar */
#include "gdevdevnprn.h"
#include "gscdevn.h"
#include "gsovrc.h"
#include "gxcmap.h"
#include "gscolor1.h"
#include "gstrans.h"
#include "gsutil.h"
#include "gxcldev.h"
#include "gxclpath.h"
#include "gxdcconv.h"
#include "gsptype2.h"
#include "gxpcolor.h"
#include "gsptype1.h"
#include "gzcpath.h"
#include "gxpaint.h"
#include "gsicc_manage.h"
#include "gsicc_cache.h"
#include "gxclist.h"
#include "gxiclass.h"
#include "gximage.h"
#include "gsmatrix.h"
#include "gxdevsop.h"
#include "gsicc.h"
#ifdef WITH_CAL
#include "cal.h"
#define CAL_SLOP 16
#else
#define CAL_SLOP 0
#endif
#include "assert_.h"
#include "gxgetbit.h"

#if RAW_DUMP
unsigned int global_index = 0;
unsigned int clist_band_count = 0;
#endif

#define DUMP_MASK_STACK 0

/* Static prototypes */
/* Used for filling rects when we are doing a fill with a pattern that
   has transparency */
static int pdf14_tile_pattern_fill(gx_device * pdev, const gs_gstate * pgs,
                                   gx_path * ppath, const gx_fill_params * params,
                                   const gx_device_color * pdevc, const gx_clip_path * pcpath);
static pdf14_mask_t *pdf14_mask_element_new(gs_memory_t *memory);
static void pdf14_free_smask_color(pdf14_device * pdev);
static int compute_group_device_int_rect(pdf14_device *pdev, gs_int_rect *rect,
                                         const gs_rect *pbbox, gs_gstate *pgs);
static int pdf14_clist_update_params(pdf14_clist_device * pdev,
                                     const gs_gstate * pgs,
                                     bool crop_blend_params,
                                     gs_pdf14trans_params_t *group_params);
static int pdf14_mark_fill_rectangle_ko_simple(gx_device *	dev, int x, int y,
                                               int w, int h, gx_color_index color,
                                               const gx_device_color *pdc,
                                               bool devn);
static int pdf14_copy_alpha_color(gx_device * dev, const byte * data, int data_x,
                                  int aa_raster, gx_bitmap_id id, int x, int y, int w, int h,
                                  gx_color_index color, const gx_device_color *pdc,
                                  int depth, bool devn);

/* Functions for dealing with soft mask color */
static int pdf14_decrement_smask_color(gs_gstate * pgs, gx_device * dev);
static int pdf14_increment_smask_color(gs_gstate * pgs, gx_device * dev);

/*
 * We chose the blending color space based upon the process color model of the
 * output device.  For gray, RGB, CMYK, or CMYK+spot devices, the choice is
 * usually simple.  For other devices or if the user is doing custom color
 * processing then the user may want to control this choice.
 */
#define AUTO_USE_CUSTOM_BLENDING 0
#define ALWAYS_USE_CUSTOM_BLENDING 1
#define DO_NOT_USE_CUSTOM_BLENDING 2

#define CUSTOM_BLENDING_MODE AUTO_USE_CUSTOM_BLENDING

# define INCR(v) DO_NOTHING

/* Forward prototypes */
void pdf14_cmyk_cs_to_cmyk_cm(const gx_device *, frac, frac, frac, frac, frac *);
static int gs_pdf14_device_push(gs_memory_t *mem, gs_gstate * pgs,
                                gx_device ** pdev, gx_device * target,
                                const gs_pdf14trans_t * pdf14pct);
static int gs_pdf14_clist_device_push(gs_memory_t * mem, gs_gstate * pgs,
                                      gx_device ** pdev, gx_device * target,
                                      const gs_pdf14trans_t * pdf14pct);
static int pdf14_tile_pattern_fill(gx_device * pdev,
                const gs_gstate * pgs, gx_path * ppath,
                const gx_fill_params * params,
                const gx_device_color * pdevc, const gx_clip_path * pcpath);
static pdf14_mask_t * pdf14_mask_element_new(gs_memory_t * memory);
#ifdef DEBUG
static void pdf14_debug_mask_stack_state(pdf14_ctx *ctx);
#endif

/* Buffer stack	data structure */
gs_private_st_ptrs7(st_pdf14_buf, pdf14_buf, "pdf14_buf",
                    pdf14_buf_enum_ptrs, pdf14_buf_reloc_ptrs,
                    saved, data, backdrop, transfer_fn, mask_stack,
                    matte, group_color_info);

gs_private_st_ptrs3(st_pdf14_ctx, pdf14_ctx, "pdf14_ctx",
                    pdf14_ctx_enum_ptrs, pdf14_ctx_reloc_ptrs,
                    stack, mask_stack, base_color);

gs_private_st_ptrs1(st_pdf14_clr, pdf14_group_color_t, "pdf14_clr",
                    pdf14_clr_enum_ptrs, pdf14_clr_reloc_ptrs, previous);

gs_private_st_ptrs2(st_pdf14_mask, pdf14_mask_t, "pdf_mask",
                    pdf14_mask_enum_ptrs, pdf14_mask_reloc_ptrs,
                    rc_mask, previous);

gs_private_st_ptrs1(st_pdf14_rcmask, pdf14_rcmask_t, "pdf_rcmask",
                    pdf14_rcmask_enum_ptrs, pdf14_rcmask_reloc_ptrs,
                    mask_buf);

gs_private_st_ptrs1(st_pdf14_smaskcolor, pdf14_smaskcolor_t, "pdf14_smaskcolor",
                    pdf14_smaskcolor_enum_ptrs, pdf14_smaskcolor_reloc_ptrs,
                    profiles);

/* ------ The device descriptors ------	*/

/*
 * Default X and Y resolution.
 */
#define	X_DPI 72
#define	Y_DPI 72

static int pdf14_initialize_device(gx_device *dev);

static	int pdf14_open(gx_device * pdev);
static	dev_proc_close_device(pdf14_close);
static	int pdf14_output_page(gx_device	* pdev,	int num_copies,	int flush);
static	dev_proc_put_params(pdf14_put_params);
static	dev_proc_get_color_comp_index(pdf14_cmykspot_get_color_comp_index);
static	dev_proc_get_color_comp_index(pdf14_rgbspot_get_color_comp_index);
static	dev_proc_get_color_comp_index(pdf14_grayspot_get_color_comp_index);
static	dev_proc_get_color_mapping_procs(pdf14_cmykspot_get_color_mapping_procs);
static	dev_proc_get_color_mapping_procs(pdf14_rgbspot_get_color_mapping_procs);
static	dev_proc_get_color_mapping_procs(pdf14_grayspot_get_color_mapping_procs);
dev_proc_encode_color(pdf14_encode_color);
dev_proc_encode_color(pdf14_encode_color_tag);
dev_proc_decode_color(pdf14_decode_color);
dev_proc_encode_color(pdf14_encode_color16);
dev_proc_encode_color(pdf14_encode_color16_tag);
dev_proc_decode_color(pdf14_decode_color16);
static	dev_proc_fill_rectangle(pdf14_fill_rectangle);
static  dev_proc_fill_rectangle_hl_color(pdf14_fill_rectangle_hl_color);
static	dev_proc_fill_path(pdf14_fill_path);
static	dev_proc_fill_stroke_path(pdf14_fill_stroke_path);
static  dev_proc_copy_mono(pdf14_copy_mono);
static	dev_proc_fill_mask(pdf14_fill_mask);
static	dev_proc_stroke_path(pdf14_stroke_path);
static	dev_proc_begin_typed_image(pdf14_begin_typed_image);
static	dev_proc_text_begin(pdf14_text_begin);
static	dev_proc_composite(pdf14_composite);
static	dev_proc_composite(pdf14_forward_composite);
static	dev_proc_begin_transparency_group(pdf14_begin_transparency_group);
static	dev_proc_end_transparency_group(pdf14_end_transparency_group);
static	dev_proc_begin_transparency_mask(pdf14_begin_transparency_mask);
static	dev_proc_end_transparency_mask(pdf14_end_transparency_mask);
static  dev_proc_dev_spec_op(pdf14_dev_spec_op);
static	dev_proc_push_transparency_state(pdf14_push_transparency_state);
static	dev_proc_pop_transparency_state(pdf14_pop_transparency_state);
static  dev_proc_ret_devn_params(pdf14_ret_devn_params);
static  dev_proc_update_spot_equivalent_colors(pdf14_update_spot_equivalent_colors);
static  dev_proc_copy_alpha(pdf14_copy_alpha);
static  dev_proc_copy_planes(pdf14_copy_planes);
static  dev_proc_copy_alpha_hl_color(pdf14_copy_alpha_hl_color);
static  dev_proc_discard_transparency_layer(pdf14_discard_trans_layer);
static  dev_proc_strip_tile_rect_devn(pdf14_strip_tile_rect_devn);
static	const gx_color_map_procs *
    pdf14_get_cmap_procs(const gs_gstate *, const gx_device *);

#define	XSIZE (int)(8.5	* X_DPI)	/* 8.5 x 11 inch page, by default */
#define	YSIZE (int)(11 * Y_DPI)

/* 24-bit color. */

static void
pdf14_procs_initialize(gx_device *dev,
                       dev_proc_get_color_mapping_procs(get_color_mapping_procs),
                       dev_proc_get_color_comp_index(get_color_comp_index),
                       dev_proc_encode_color(encode_color),
                       dev_proc_decode_color(decode_color))
{
    set_dev_proc(dev, initialize_device, pdf14_initialize_device);
    set_dev_proc(dev, open_device, pdf14_open);
    set_dev_proc(dev, output_page, pdf14_output_page);
    set_dev_proc(dev, close_device, pdf14_close);
    set_dev_proc(dev, map_rgb_color, encode_color);
    set_dev_proc(dev, map_color_rgb, decode_color);
    set_dev_proc(dev, fill_rectangle, pdf14_fill_rectangle);
    set_dev_proc(dev, copy_mono, pdf14_copy_mono);
    set_dev_proc(dev, get_params, gx_forward_get_params);
    set_dev_proc(dev, put_params, pdf14_put_params);
    set_dev_proc(dev, copy_alpha, pdf14_copy_alpha);
    set_dev_proc(dev, fill_path, pdf14_fill_path);
    set_dev_proc(dev, stroke_path, pdf14_stroke_path);
    set_dev_proc(dev, fill_mask, pdf14_fill_mask);
    set_dev_proc(dev, begin_typed_image, pdf14_begin_typed_image);
    set_dev_proc(dev, composite, pdf14_composite);
    set_dev_proc(dev, text_begin, pdf14_text_begin);
    set_dev_proc(dev, begin_transparency_group, pdf14_begin_transparency_group);
    set_dev_proc(dev, end_transparency_group, pdf14_end_transparency_group);
    set_dev_proc(dev, begin_transparency_mask, pdf14_begin_transparency_mask);
    set_dev_proc(dev, end_transparency_mask, pdf14_end_transparency_mask);
    set_dev_proc(dev, discard_transparency_layer, pdf14_discard_trans_layer);
    set_dev_proc(dev, get_color_mapping_procs, get_color_mapping_procs);
    set_dev_proc(dev, get_color_comp_index, get_color_comp_index);
    set_dev_proc(dev, encode_color, encode_color);
    set_dev_proc(dev, decode_color, decode_color);
    set_dev_proc(dev, fill_rectangle_hl_color, pdf14_fill_rectangle_hl_color);
    set_dev_proc(dev, update_spot_equivalent_colors, gx_forward_update_spot_equivalent_colors);
    set_dev_proc(dev, ret_devn_params, pdf14_ret_devn_params);
    set_dev_proc(dev, push_transparency_state, pdf14_push_transparency_state);
    set_dev_proc(dev, pop_transparency_state, pdf14_pop_transparency_state);
    set_dev_proc(dev, dev_spec_op, pdf14_dev_spec_op);
    set_dev_proc(dev, copy_planes, pdf14_copy_planes);
    set_dev_proc(dev, set_graphics_type_tag, gx_forward_set_graphics_type_tag);
    set_dev_proc(dev, strip_tile_rect_devn, pdf14_strip_tile_rect_devn);
    set_dev_proc(dev, copy_alpha_hl_color, pdf14_copy_alpha_hl_color);
    set_dev_proc(dev, fill_stroke_path, pdf14_fill_stroke_path);
}

static void
pdf14_Gray_initialize_device_procs(gx_device *dev)
{
    pdf14_procs_initialize(dev,
                           gx_default_DevGray_get_color_mapping_procs,
                           gx_default_DevGray_get_color_comp_index,
                           pdf14_encode_color,
                           pdf14_decode_color);
}

static void
pdf14_RGB_initialize_device_procs(gx_device *dev)
{
    pdf14_procs_initialize(dev,
                           gx_default_DevRGB_get_color_mapping_procs,
                           gx_default_DevRGB_get_color_comp_index,
                           pdf14_encode_color,
                           pdf14_decode_color);
}

static void
pdf14_CMYK_initialize_device_procs(gx_device *dev)
{
    pdf14_procs_initialize(dev,
                           gx_default_DevCMYK_get_color_mapping_procs,
                           gx_default_DevCMYK_get_color_comp_index,
                           pdf14_encode_color,
                           pdf14_decode_color);
}

static void
pdf14_CMYKspot_initialize_device_procs(gx_device *dev)
{
    pdf14_procs_initialize(dev,
                           pdf14_cmykspot_get_color_mapping_procs,
                           pdf14_cmykspot_get_color_comp_index,
                           pdf14_encode_color,
                           pdf14_decode_color);
}

static void
pdf14_RGBspot_initialize_device_procs(gx_device *dev)
{
    pdf14_procs_initialize(dev,
                           pdf14_rgbspot_get_color_mapping_procs,
                           pdf14_rgbspot_get_color_comp_index,
                           pdf14_encode_color,
                           pdf14_decode_color);
}

static void
pdf14_Grayspot_initialize_device_procs(gx_device *dev)
{
    pdf14_procs_initialize(dev,
                           pdf14_grayspot_get_color_mapping_procs,
                           pdf14_grayspot_get_color_comp_index,
                           pdf14_encode_color,
                           pdf14_decode_color);
}

static void
pdf14_custom_initialize_device_procs(gx_device *dev)
{
    pdf14_procs_initialize(dev,
                           gx_forward_get_color_mapping_procs,
                           gx_forward_get_color_comp_index,
                           gx_forward_encode_color,
                           gx_forward_decode_color);
}

static struct_proc_finalize(pdf14_device_finalize);

gs_private_st_composite_use_final(st_pdf14_device, pdf14_device, "pdf14_device",
                                  pdf14_device_enum_ptrs, pdf14_device_reloc_ptrs,
                          pdf14_device_finalize);

static int pdf14_put_image(gx_device * dev, gs_gstate * pgs,
                                                        gx_device * target);
static int pdf14_cmykspot_put_image(gx_device * dev, gs_gstate * pgs,
                                                        gx_device * target);
static int pdf14_custom_put_image(gx_device * dev, gs_gstate * pgs,
                                                        gx_device * target);

/* Alter pdf14 device color model based upon group or softmask. This occurs
   post clist or in immediate rendering case. Data stored with buffer */
static pdf14_group_color_t* pdf14_push_color_model(gx_device *dev,
                              gs_transparency_color_t group_color, int64_t icc_hashcode,
                              cmm_profile_t *iccprofile, bool is_mask);
static void pdf14_pop_color_model(gx_device* dev, pdf14_group_color_t* group_color);

/* Alter clist writer device color model based upon group or softmask. Data
   stored in the device color model stack */
static int pdf14_clist_push_color_model(gx_device* dev, gx_device* cdev, gs_gstate* pgs,
    const gs_pdf14trans_t* pdf14pct, gs_memory_t* mem, bool is_mask);
static int pdf14_clist_pop_color_model(gx_device* dev, gs_gstate* pgs);

/* Used for cleaning up the stack if things go wrong */
static void pdf14_pop_group_color(gx_device *dev, const gs_gstate *pgs);

static const pdf14_procs_t gray_pdf14_procs = {
    pdf14_unpack_additive,
    pdf14_put_image,
    pdf14_unpack16_additive
};

static const pdf14_procs_t rgb_pdf14_procs = {
    pdf14_unpack_additive,
    pdf14_put_image,
    pdf14_unpack16_additive
};

static const pdf14_procs_t cmyk_pdf14_procs = {
    pdf14_unpack_subtractive,
    pdf14_put_image,
    pdf14_unpack16_subtractive
};

static const pdf14_procs_t cmykspot_pdf14_procs = {
    pdf14_unpack_custom,	/* should never be used since we will use devn values */
    pdf14_cmykspot_put_image,
    pdf14_unpack16_custom	/* should never be used since we will use devn values */
};

static const pdf14_procs_t rgbspot_pdf14_procs = {
    pdf14_unpack_rgb_mix,
    pdf14_cmykspot_put_image,
    pdf14_unpack16_rgb_mix
};

static const pdf14_procs_t grayspot_pdf14_procs = {
    pdf14_unpack_gray_mix,
    pdf14_cmykspot_put_image,
    pdf14_unpack16_gray_mix
};

static const pdf14_procs_t custom_pdf14_procs = {
    pdf14_unpack_custom,
    pdf14_custom_put_image,
    pdf14_unpack16_custom
};

static const pdf14_nonseparable_blending_procs_t gray_blending_procs = {
    art_blend_luminosity_custom_8,
    art_blend_saturation_custom_8,
    art_blend_luminosity_custom_16,
    art_blend_saturation_custom_16
};

static const pdf14_nonseparable_blending_procs_t rgb_blending_procs = {
    art_blend_luminosity_rgb_8,
    art_blend_saturation_rgb_8,
    art_blend_luminosity_rgb_16,
    art_blend_saturation_rgb_16
};

static const pdf14_nonseparable_blending_procs_t cmyk_blending_procs = {
    art_blend_luminosity_cmyk_8,
    art_blend_saturation_cmyk_8,
    art_blend_luminosity_cmyk_16,
    art_blend_saturation_cmyk_16
};

static const pdf14_nonseparable_blending_procs_t rgbspot_blending_procs = {
    art_blend_luminosity_rgb_8,
    art_blend_saturation_rgb_8,
    art_blend_luminosity_rgb_16,
    art_blend_saturation_rgb_16
};

static const pdf14_nonseparable_blending_procs_t grayspot_blending_procs = {
    art_blend_luminosity_custom_8,
    art_blend_saturation_custom_8,
    art_blend_luminosity_custom_16,
    art_blend_saturation_custom_16
};

static const pdf14_nonseparable_blending_procs_t custom_blending_procs = {
    art_blend_luminosity_custom_8,
    art_blend_saturation_custom_8,
    art_blend_luminosity_custom_16,
    art_blend_saturation_custom_16
};

const pdf14_device gs_pdf14_Gray_device	= {
    std_device_std_color_full_body_type(pdf14_device,
                                        pdf14_Gray_initialize_device_procs,
                                        "pdf14gray",
                                        &st_pdf14_device,
                                        XSIZE, YSIZE, X_DPI, Y_DPI, 8,
                                        0, 0, 0, 0, 0, 0),
    { 0 },			/* Procs */
    NULL,			/* target */
    { 0 },			/* devn_params - not used */
    &gray_pdf14_procs,
    &gray_blending_procs,
    1
};

const pdf14_device gs_pdf14_RGB_device = {
    std_device_color_stype_body(pdf14_device,
                                pdf14_RGB_initialize_device_procs,
                                "pdf14RGB",
                                &st_pdf14_device,
                                XSIZE, YSIZE, X_DPI, Y_DPI, 24, 255, 256),
    { 0 },			/* Procs */
    NULL,			/* target */
    { 0 },			/* devn_params - not used */
    &rgb_pdf14_procs,
    &rgb_blending_procs,
    3
};

const pdf14_device gs_pdf14_CMYK_device = {
    std_device_std_color_full_body_type(pdf14_device,
                                        pdf14_CMYK_initialize_device_procs,
                                        "pdf14cmyk",
                                        &st_pdf14_device,
                                        XSIZE, YSIZE, X_DPI, Y_DPI, 32,
                                        0, 0, 0, 0, 0, 0),
    { 0 },			/* Procs */
    NULL,			/* target */
    { 0 },			/* devn_params - not used */
    &cmyk_pdf14_procs,
    &cmyk_blending_procs,
    4
};

const pdf14_device gs_pdf14_CMYKspot_device	= {
    std_device_part1_(pdf14_device,
                      pdf14_CMYKspot_initialize_device_procs,
                      "pdf14cmykspot",
                      &st_pdf14_device,
                      open_init_closed),
    dci_values(GX_DEVICE_COLOR_MAX_COMPONENTS,64,255,255,256,256),
    std_device_part2_(XSIZE, YSIZE, X_DPI, Y_DPI),
    offset_margin_values(0, 0, 0, 0, 0, 0),
    std_device_part3_(),
    { 0 },			/* Procs */
    NULL,			/* target */
    /* DeviceN parameters */
    { 8,			/* Not used - Bits per color */
      DeviceCMYKComponents,	/* Names of color model colorants */
      4,			/* Number colorants for CMYK */
      0,			/* MaxSeparations has not been specified */
      -1,			/* PageSpotColors has not been specified */
      {0},			/* SeparationNames */
      0,			/* SeparationOrder names */
      {0, 1, 2, 3, 4, 5, 6, 7 }	/* Initial component SeparationOrder */
    },
    &cmykspot_pdf14_procs,
    &cmyk_blending_procs,
    4
};

const pdf14_device gs_pdf14_RGBspot_device = {
    std_device_part1_(pdf14_device,
                      pdf14_RGBspot_initialize_device_procs,
                      "pdf14rgbspot",
                      &st_pdf14_device,
                      open_init_closed),
    dci_values(GX_DEVICE_COLOR_MAX_COMPONENTS,64,255,255,256,256),
    std_device_part2_(XSIZE, YSIZE, X_DPI, Y_DPI),
    offset_margin_values(0, 0, 0, 0, 0, 0),
    std_device_part3_(),
    { 0 },			/* Procs */
    NULL,			/* target */
                    /* DeviceN parameters */
    { 8,			/* Not used - Bits per color */
    0,	            /* Names of color model colorants */
    3,			    /* Number colorants for RGB */
    0,			    /* MaxSeparations has not been specified */
    -1,			    /* PageSpotColors has not been specified */
    { 0 },			/* SeparationNames */
    0,			    /* SeparationOrder names */
    { 0, 1, 2, 3, 4, 5, 6, 7 }	/* Initial component SeparationOrder */
    },
    &rgbspot_pdf14_procs,
    &rgbspot_blending_procs,
    3
};

const pdf14_device gs_pdf14_Grayspot_device = {
    std_device_part1_(pdf14_device,
                      pdf14_Grayspot_initialize_device_procs,
                      "pdf14grayspot",
                      &st_pdf14_device,
                      open_init_closed),
    dci_values(GX_DEVICE_COLOR_MAX_COMPONENTS,64,255,255,256,256),
    std_device_part2_(XSIZE, YSIZE, X_DPI, Y_DPI),
    offset_margin_values(0, 0, 0, 0, 0, 0),
    std_device_part3_(),
    { 0 },			/* Procs */
    NULL,			/* target */
                    /* DeviceN parameters */
    { 8,			/* Not used - Bits per color */
    0,	            /* Names of color model colorants */
    3,			    /* Number colorants for RGB */
    0,			    /* MaxSeparations has not been specified */
    -1,			    /* PageSpotColors has not been specified */
    { 0 },			/* SeparationNames */
    0,			    /* SeparationOrder names */
    { 0, 1, 2, 3, 4, 5, 6, 7 }	/* Initial component SeparationOrder */
    },
    &grayspot_pdf14_procs,
    &grayspot_blending_procs,
    1
};

/*
 * The 'custom' PDF 1.4 compositor device is for working with those devices
 * which support spot colors but do not have a CMYK process color model.
 *
 * This causes some problems with the Hue, Saturation, Color, and Luminosity
 * blending modes.  These blending modes are 'non separable' and depend upon
 * knowing the details of the blending color space.  However we use the
 * process color model of the output device for our blending color space.
 * With an unknown process color model, we have to fall back to some 'guesses'
 * about how to treat these blending modes.
 */
const pdf14_device gs_pdf14_custom_device = {
    std_device_part1_(pdf14_device,
                      pdf14_custom_initialize_device_procs,
                      "pdf14custom",
                      &st_pdf14_device,
                      open_init_closed),
    dci_values(GX_DEVICE_COLOR_MAX_COMPONENTS,64,255,255,256,256),
    std_device_part2_(XSIZE, YSIZE, X_DPI, Y_DPI),
    offset_margin_values(0, 0, 0, 0, 0, 0),
    std_device_part3_(),
    { 0 },			/* Procs */
    NULL,			/* target */
    /* DeviceN parameters */
    { 8,			/* Not used - Bits per color */
      DeviceCMYKComponents,	/* Names of color model colorants */
      4,			/* Number colorants for CMYK */
      0,			/* MaxSeparations has not been specified */
      -1,			/* PageSpotColors has not been specified */
      {0},			/* SeparationNames */
      0,			/* SeparationOrder names */
      {0, 1, 2, 3, 4, 5, 6, 7 }	/* Initial component SeparationOrder */
    },
    &custom_pdf14_procs,
    &custom_blending_procs,
    4
};

/* Devices used for pdf14-accum-* device, one for  each image colorspace, */
/* Gray, RGB, CMYK, DeviceN. Before calling gdev_prn_open, the following  */
/* are set from the target device: width, height, xdpi, ydpi, MaxBitmap.  */

static dev_proc_print_page(no_print_page);
static  dev_proc_ret_devn_params(pdf14_accum_ret_devn_params);
static  dev_proc_get_color_comp_index(pdf14_accum_get_color_comp_index);
static  dev_proc_get_color_mapping_procs(pdf14_accum_get_color_mapping_procs);
static  dev_proc_update_spot_equivalent_colors(pdf14_accum_update_spot_equivalent_colors);

static int
no_print_page(gx_device_printer *pdev, gp_file *prn_stream)
{
    return_error(gs_error_unknownerror);
}

struct gx_device_pdf14_accum_s {
    gx_devn_prn_device_common;
    gx_device *save_p14dev;		/* the non-clist pdf14 deivce saved for after accum */
};
typedef struct gx_device_pdf14_accum_s gx_device_pdf14_accum;

int
pdf14_accum_dev_spec_op(gx_device *pdev, int dev_spec_op, void *data, int size)
{
    gx_device_pdf14_accum *adev = (gx_device_pdf14_accum *)pdev;

    if (dev_spec_op == gxdso_device_child) {
        gxdso_device_child_request *req = (gxdso_device_child_request *)data;
        if (size < sizeof(*req))
            return gs_error_unknownerror;
        req->target = adev->save_p14dev;
        req->n = 0;
        return 0;
    }

    return gdev_prn_dev_spec_op(pdev, dev_spec_op, data, size);
}

gs_private_st_suffix_add1_final(st_gx_devn_accum_device, gx_device_pdf14_accum,
        "gx_device_pdf14_accum", pdf14_accum_device_enum_ptrs, pdf14_accum_device_reloc_ptrs,
                          gx_devn_prn_device_finalize, st_gx_devn_prn_device, save_p14dev);

static void
pdf14_accum_Gray_initialize_device_procs(gx_device *dev)
{
    gdev_prn_initialize_device_procs_gray8(dev);

    set_dev_proc(dev, encode_color, gx_default_8bit_map_gray_color);
    set_dev_proc(dev, decode_color, gx_default_8bit_map_color_gray);
}

const gx_device_pdf14_accum pdf14_accum_Gray = {
    prn_device_stype_body(gx_device_pdf14_accum,
                          pdf14_accum_Gray_initialize_device_procs,
                          "pdf14-accum-Gray",
                          &st_gx_devn_accum_device,
                          0/*width*/, 0/*height*/, 300/*xdpi*/, 300/*ydpi*/,
                          0/*lm*/, 0/*bm*/, 0/*rm*/, 0/*tm*/,
                          1/*ncomp*/, 8/*depth*/, 255/*max_gray*/, 0/*max_color*/,
                          256/*dither_grays*/, 0/*dither_colors*/,
                          no_print_page),
    { 0 },			/* devn_params - not used */
    { 0 },			/* equivalent_cmyk_color_params - not used */
    0/*save_p14dev*/
};

static void
pdf14_accum_RGB_initialize_device_procs(gx_device *dev)
{
    gdev_prn_initialize_device_procs_rgb(dev);
}

const gx_device_pdf14_accum pdf14_accum_RGB = {
    prn_device_stype_body(gx_device_pdf14_accum,
                          pdf14_accum_RGB_initialize_device_procs,
                          "pdf14-accum-RGB",
                          &st_gx_devn_accum_device,
                          0/*width*/, 0/*height*/, 300/*xdpi*/, 300/*ydpi*/,
                          0/*lm*/, 0/*bm*/, 0/*rm*/, 0/*tm*/,
                          3/*ncomp*/, 24/*depth*/, 0/*max_gray*/, 255/*max_color*/,
                          1/*dither_grays*/, 256/*dither_colors*/,
                          no_print_page),
    { 0 },			/* devn_params - not used */
    { 0 },			/* equivalent_cmyk_color_params - not used */
    0/*save_p14dev*/
};

static void
pdf14_accum_CMYK_initialize_device_procs(gx_device *dev)
{
    gdev_prn_initialize_device_procs_cmyk8(dev);

    set_dev_proc(dev, encode_color, cmyk_8bit_map_cmyk_color);
    set_dev_proc(dev, decode_color, cmyk_8bit_map_color_cmyk);
}

const gx_device_pdf14_accum pdf14_accum_CMYK = {
    prn_device_stype_body(gx_device_pdf14_accum,
                          pdf14_accum_CMYK_initialize_device_procs,
                          "pdf14-accum-CMYK",
                          &st_gx_devn_accum_device,
                          0/*width*/, 0/*height*/, 300/*xdpi*/, 300/*ydpi*/,
                          0/*lm*/, 0/*bm*/, 0/*rm*/, 0/*tm*/,
                          4/*ncomp*/, 32/*depth*/, 255/*max_gray*/, 255/*max_color*/,
                          256/*dither_grays*/, 256/*dither_colors*/,
                          no_print_page),
    { 0 },			/* devn_params - not used */
    { 0 },			/* equivalent_cmyk_color_params - not used */
    0/*save_p14dev*/
};

static void
pdf14_accum_initialize_device_procs_cmykspot(gx_device *dev)
{
    pdf14_accum_CMYK_initialize_device_procs(dev);

    set_dev_proc(dev, get_color_mapping_procs, pdf14_accum_get_color_mapping_procs);
    set_dev_proc(dev, get_color_comp_index, pdf14_accum_get_color_comp_index);
    set_dev_proc(dev, update_spot_equivalent_colors, pdf14_accum_update_spot_equivalent_colors);
    set_dev_proc(dev, ret_devn_params, pdf14_accum_ret_devn_params);
}

const gx_device_pdf14_accum pdf14_accum_CMYKspot = {
    prn_device_stype_body(gx_device_pdf14_accum,
                          pdf14_accum_initialize_device_procs_cmykspot,
                          "pdf14-accum-CMYKspot",
                          &st_gx_devn_accum_device,
                          0/*width*/, 0/*height*/, 300/*xdpi*/, 300/*ydpi*/,
                          0/*lm*/, 0/*bm*/, 0/*rm*/, 0/*tm*/,
                          4/*ncomp*/, 32/*depth*/, 255/*max_gray*/, 255/*max_color*/,
                          256/*dither_grays*/, 256/*dither_colors*/,
                          no_print_page),
    /* DeviceN parameters */
    { 8,			/* Not used - Bits per color */
      DeviceCMYKComponents,	/* Names of color model colorants */
      4,			/* Number colorants for CMYK */
      0,			/* MaxSeparations has not been specified */
      -1,			/* PageSpotColors has not been specified */
      { 0 },			/* SeparationNames */
      0,			/* SeparationOrder names */
      {0, 1, 2, 3, 4, 5, 6, 7 }	/* Initial component SeparationOrder */
    },
    { true },			/* equivalent_cmyk_color_params */
    0/*save_p14dev*/
};

/* GC procedures */
static
ENUM_PTRS_WITH(pdf14_device_enum_ptrs, pdf14_device *pdev)
{
    index -= 5;
    if (index < pdev->devn_params.separations.num_separations)
        ENUM_RETURN(pdev->devn_params.separations.names[index].data);
    index -= pdev->devn_params.separations.num_separations;
    if (index < pdev->devn_params.pdf14_separations.num_separations)
        ENUM_RETURN(pdev->devn_params.pdf14_separations.names[index].data);
    return 0;
}
case 0:	return ENUM_OBJ(pdev->ctx);
case 1: return ENUM_OBJ(pdev->color_model_stack);
case 2: return ENUM_OBJ(pdev->smaskcolor);
case 3:	ENUM_RETURN(gx_device_enum_ptr(pdev->target));
case 4:	ENUM_RETURN(gx_device_enum_ptr(pdev->pclist_device));
ENUM_PTRS_END

static	RELOC_PTRS_WITH(pdf14_device_reloc_ptrs, pdf14_device *pdev)
{
    {
        int i;

        for (i = 0; i < pdev->devn_params.separations.num_separations; ++i) {
            RELOC_PTR(pdf14_device, devn_params.separations.names[i].data);
        }
    }
    RELOC_VAR(pdev->ctx);
    RELOC_VAR(pdev->smaskcolor);
    RELOC_VAR(pdev->color_model_stack);
    pdev->target = gx_device_reloc_ptr(pdev->target, gcst);
    pdev->pclist_device = gx_device_reloc_ptr(pdev->pclist_device, gcst);
}
RELOC_PTRS_END

/* ------ Private definitions ------ */

static void
resolve_matte(pdf14_buf *maskbuf, byte *src_data, int src_planestride, int src_rowstride,
              int width, int height, cmm_profile_t *src_profile, int deep)
{
    if (deep) {
        int x, y, i;
        uint16_t *mask_row_ptr  = (uint16_t *)maskbuf->data;
        uint16_t *src_row_ptr   = (uint16_t *)src_data;
        uint16_t *mask_tr_fn    = (uint16_t *)maskbuf->transfer_fn;

        src_planestride >>= 1;
        src_rowstride >>= 1;

        for (y = 0; y < height; y++) {
            uint16_t *mask_curr_ptr = mask_row_ptr;
            uint16_t *src_curr_ptr = src_row_ptr;
            for (x = 0; x < width; x++) {
                uint16_t idx = *mask_curr_ptr;
                byte     top = idx>>8;
                uint16_t a   = mask_tr_fn[top];
                int      b   = mask_tr_fn[top+1]-a;
                uint16_t matte_alpha = a + ((0x80 + b*(idx & 0xff))>>8);

                /* matte's happen rarely enough that we allow ourselves to
                 * resort to 64bit here. */
                if (matte_alpha != 0 && matte_alpha != 0xffff) {
                    for (i = 0; i < src_profile->num_comps; i++) {
                        int val = src_curr_ptr[i * src_planestride] - maskbuf->matte[i];
                        int temp = (((int64_t)val) * 0xffff / matte_alpha) + maskbuf->matte[i];

                        /* clip */
                        if (temp > 0xffff)
                            src_curr_ptr[i * src_planestride] = 0xffff;
                        else if (temp < 0)
                            src_curr_ptr[i * src_planestride] = 0;
                        else
                            src_curr_ptr[i * src_planestride] = temp;
                    }
                }
                mask_curr_ptr++;
                src_curr_ptr++;
            }
            src_row_ptr += src_rowstride;
            mask_row_ptr += (maskbuf->rowstride>>1);
        }
    } else {
        int x, y, i;
        byte *mask_row_ptr  = maskbuf->data;
        byte *src_row_ptr   = src_data;
        byte *mask_tr_fn    = maskbuf->transfer_fn;

        for (y = 0; y < height; y++) {
            byte *mask_curr_ptr = mask_row_ptr;
            byte *src_curr_ptr = src_row_ptr;
            for (x = 0; x < width; x++) {
                byte matte_alpha = mask_tr_fn[*mask_curr_ptr];
                if (matte_alpha != 0 && matte_alpha != 0xff) {
                    for (i = 0; i < src_profile->num_comps; i++) {
                        byte matte = maskbuf->matte[i]>>8;
                        int val = src_curr_ptr[i * src_planestride] - matte;
                        int temp = ((((val * 0xff) << 8) / matte_alpha) >> 8) + matte;

                        /* clip */
                        if (temp > 0xff)
                            src_curr_ptr[i * src_planestride] = 0xff;
                        else if (temp < 0)
                            src_curr_ptr[i * src_planestride] = 0;
                        else
                            src_curr_ptr[i * src_planestride] = temp;
                    }
                }
                mask_curr_ptr++;
                src_curr_ptr++;
            }
            src_row_ptr += src_rowstride;
            mask_row_ptr += maskbuf->rowstride;
        }
    }
}

/* Transform of color data and copy noncolor data.  Used in
   group pop and during the pdf14 put image calls when the blend color space
   is different than the target device color space.  The function will try do
   in-place conversion if possible.  If not, it will do an allocation.  The
   put_image call needs to know if an allocation was made so that it can adjust
   for the fact that we likely don't have a full page any longer and we don't
   need to do the offset to our data in the buffer. Bug 700686: If we are in
   a softmask that includes a matte entry, then we need to undo the matte
   entry here at this time in the image's native color space not the parent
   color space.   The endian_swap term here is only set to true if the data
   has been baked as BE during the put_image blending operation and we are
   on a LE machine.  */
static forceinline pdf14_buf*
template_transform_color_buffer(gs_gstate *pgs, pdf14_ctx *ctx, gx_device *dev,
    pdf14_buf *src_buf, byte *src_data, cmm_profile_t *src_profile,
    cmm_profile_t *des_profile, int x0, int y0, int width, int height, bool *did_alloc,
    bool has_matte, bool deep, bool endian_swap)
{
    gsicc_rendering_param_t rendering_params;
    gsicc_link_t *icc_link;
    gsicc_bufferdesc_t src_buff_desc;
    gsicc_bufferdesc_t des_buff_desc;
    int src_planestride = src_buf->planestride;
    int src_rowstride = src_buf->rowstride;
    int src_n_planes = src_buf->n_planes;
    int src_n_chan = src_buf->n_chan;
    int des_planestride = src_planestride;
    int des_rowstride = src_rowstride;
    int des_n_planes = src_n_planes;
    int des_n_chan = src_n_chan;
    int diff;
    int k, j;
    byte *des_data = NULL;
    pdf14_buf *output = src_buf;
    pdf14_mask_t *mask_stack;
    pdf14_buf *maskbuf;
    int code;

    *did_alloc = false;

    /* Same profile */
    if (gsicc_get_hash(src_profile) == gsicc_get_hash(des_profile))
        return src_buf;

    /* Define the rendering intent get the link */
    rendering_params.black_point_comp = gsBLACKPTCOMP_ON;
    rendering_params.graphics_type_tag = GS_IMAGE_TAG;
    rendering_params.override_icc = false;
    rendering_params.preserve_black = gsBKPRESNOTSPECIFIED;
    rendering_params.rendering_intent = gsPERCEPTUAL;
    rendering_params.cmm = gsCMM_DEFAULT;
    icc_link = gsicc_get_link_profile(pgs, dev, src_profile, des_profile,
        &rendering_params, pgs->memory, false);
    if (icc_link == NULL)
        return NULL;

    /* If different data sizes, we have to do an allocation */
    diff = des_profile->num_comps - src_profile->num_comps;
    if (diff != 0) {
        byte *src_ptr;
        byte *des_ptr;

        *did_alloc = true;
        des_rowstride = ((width + 3) & -4)<<deep;
        des_planestride = height * des_rowstride;
        des_n_planes = src_n_planes + diff;
        des_n_chan = src_n_chan + diff;
        des_data = gs_alloc_bytes(ctx->memory,
                                  (size_t)des_planestride * des_n_planes + CAL_SLOP,
                                  "pdf14_transform_color_buffer");
        if (des_data == NULL)
            return NULL;

        /* Copy over the noncolor planes. May only be a dirty part, so have
           to copy row by row */
        src_ptr = src_data;
        des_ptr = des_data;
        for (j = 0; j < height; j++) {
            for (k = 0; k < (src_n_planes - src_profile->num_comps); k++) {
                memcpy(des_ptr + des_planestride * (k + des_profile->num_comps),
                       src_ptr + src_planestride * (k + src_profile->num_comps),
                       width<<deep);
            }
            src_ptr += src_rowstride;
            des_ptr += des_rowstride;
        }
    } else
        des_data = src_data;

    /* Set up the buffer descriptors. */
    gsicc_init_buffer(&src_buff_desc, src_profile->num_comps, 1<<deep, false,
                      false, true, src_planestride, src_rowstride, height, width);
    gsicc_init_buffer(&des_buff_desc, des_profile->num_comps, 1<<deep, false,
                      false, true, des_planestride, des_rowstride, height, width);

    src_buff_desc.endian_swap = endian_swap;
    des_buff_desc.endian_swap = endian_swap;

    /* If we have a matte entry, undo the pre-blending now.  Also set pdf14
       context to ensure that this is not done again during the group
       composition */
    if (has_matte &&
        /* Should always happen, but check for safety */
        ((mask_stack = ctx->mask_stack) != NULL) &&
        ((maskbuf = mask_stack->rc_mask->mask_buf) != NULL))
    {
        resolve_matte(maskbuf, src_data, src_planestride, src_rowstride, width, height, src_profile, deep);
    }

    /* Transform the data. Since the pdf14 device should be using RGB, CMYK or
       Gray buffers, this transform does not need to worry about the cmap procs
       of the target device. */
    code = (icc_link->procs.map_buffer)(dev, icc_link, &src_buff_desc, &des_buff_desc,
        src_data, des_data);
    gsicc_release_link(icc_link);
    if (code < 0)
        return NULL;

    output->planestride = des_planestride;
    output->rowstride = des_rowstride;
    output->n_planes = des_n_planes;
    output->n_chan = des_n_chan;
    /* If not in-place conversion, then release. */
    if (des_data != src_data) {
        gs_free_object(ctx->memory, output->data,
            "pdf14_transform_color_buffer");
        output->data = des_data;
        /* Note, this is needed for case where we did a put image, as the
           resulting transformed buffer may not be a full page. */
        output->rect.p.x = x0;
        output->rect.p.y = y0;
        output->rect.q.x = x0 + width;
        output->rect.q.y = y0 + height;
    }
    return output;
}

/* This is a routine to do memset's but with 16 bit values.
 * Note, that we still take bytes, NOT "num values to set".
 * We assume dest is 16 bit aligned. We assume that bytes is
 * a multiple of 2. */
static void gs_memset16(byte *dest_, uint16_t value, int bytes)
{
    uint16_t *dest = (uint16_t *)(void *)dest_;
    uint32_t v;
    if (bytes < 0)
        return;
    if (((intptr_t)dest) & 2) {
        *dest++ = value;
        bytes--;
        if (bytes == 0)
            return;
    }
    v = value | (value<<16);
    bytes -= 2;
    while (bytes > 0) {
        *(uint32_t *)dest = v;
        dest += 2;
        bytes -= 4;
    }
    bytes += 2;
    if (bytes & 2) {
        *dest = value;
    }
}

static pdf14_buf*
pdf14_transform_color_buffer_no_matte(gs_gstate *pgs, pdf14_ctx *ctx, gx_device *dev,
    pdf14_buf *src_buf, byte *src_data, cmm_profile_t *src_profile,
    cmm_profile_t *des_profile, int x0, int y0, int width, int height, bool *did_alloc,
    bool deep, bool endian_swap)
{
    if (deep)
        return template_transform_color_buffer(pgs, ctx, dev, src_buf, src_data, src_profile,
            des_profile, x0, y0, width, height, did_alloc, false, true, endian_swap);
    else
        return template_transform_color_buffer(pgs, ctx, dev, src_buf, src_data, src_profile,
            des_profile, x0, y0, width, height, did_alloc, false, false, endian_swap);
}

static pdf14_buf*
pdf14_transform_color_buffer_with_matte(gs_gstate *pgs, pdf14_ctx *ctx, gx_device *dev,
    pdf14_buf *src_buf, byte *src_data, cmm_profile_t *src_profile,
    cmm_profile_t *des_profile, int x0, int y0, int width, int height, bool *did_alloc,
    bool deep, bool endian_swap)
{
    if (deep)
        return template_transform_color_buffer(pgs, ctx, dev, src_buf, src_data, src_profile,
            des_profile, x0, y0, width, height, did_alloc, true, true, endian_swap);
    else
        return template_transform_color_buffer(pgs, ctx, dev, src_buf, src_data, src_profile,
            des_profile, x0, y0, width, height, did_alloc, true, false, endian_swap);
}

/**
 * pdf14_buf_new: Allocate a new PDF 1.4 buffer.
 * @n_chan: Number of pixel channels including alpha.
 *
 * Return value: Newly allocated buffer, or NULL on failure.
 **/
static	pdf14_buf *
pdf14_buf_new(gs_int_rect *rect, bool has_tags, bool has_alpha_g,
              bool has_shape, bool idle, int n_chan, int num_spots,
              gs_memory_t *memory, bool deep)
{

    /* Note that alpha_g is the alpha for the GROUP */
    /* This is distinct from the alpha that may also exist */
    /* for the objects within the group.  Hence it can introduce */
    /* yet another plane */

    pdf14_buf *result;
    int rowstride = ((rect->q.x - rect->p.x + 3) & -4)<<deep;
    int height = (rect->q.y - rect->p.y);
    int n_planes = n_chan + (has_shape ? 1 : 0) + (has_alpha_g ? 1 : 0) +
                   (has_tags ? 1 : 0);
    int planestride;
    double dsize = (((double) rowstride) * height) * n_planes;

    if (dsize > (double)max_uint)
      return NULL;

    result = gs_alloc_struct(memory, pdf14_buf, &st_pdf14_buf,
                             "pdf14_buf_new");
    if (result == NULL)
        return result;

    result->memory = memory;
    result->backdrop = NULL;
    result->saved = NULL;
    result->isolated = false;
    result->knockout = false;
    result->has_alpha_g = has_alpha_g;
    result->has_shape = has_shape;
    result->has_tags = has_tags;
    result->rect = *rect;
    result->n_chan = n_chan;
    result->n_planes = n_planes;
    result->rowstride = rowstride;
    result->transfer_fn = NULL;
    result->is_ident = true;
    result->matte_num_comps = 0;
    result->matte = NULL;
    result->mask_stack = NULL;
    result->idle = idle;
    result->mask_id = 0;
    result->num_spots = num_spots;
    result->deep = deep;
    result->page_group = false;
    result->group_color_info = NULL;
    result->group_popped = false;

    if (idle || height <= 0) {
        /* Empty clipping - will skip all drawings. */
        result->planestride = 0;
        result->data = 0;
    } else {
        planestride = rowstride * height;
        result->planestride = planestride;
        result->data = gs_alloc_bytes(memory,
                                      (size_t)planestride * n_planes + CAL_SLOP,
                                      "pdf14_buf_new");
        if (result->data == NULL) {
            gs_free_object(memory, result, "pdf14_buf_new");
            return NULL;
        }
        if (has_alpha_g) {
            int alpha_g_plane = n_chan + (has_shape ? 1 : 0);
            /* Memsetting by 0, so this copes with the deep case too */
            memset(result->data + alpha_g_plane * planestride, 0, planestride);
        }
        if (has_tags) {
            int tags_plane = n_chan + (has_shape ? 1 : 0) + (has_alpha_g ? 1 : 0);
            /* Memsetting by 0, so this copes with the deep case too */
            memset (result->data + tags_plane * planestride,
                    GS_UNTOUCHED_TAG, planestride);
        }
    }
    /* Initialize dirty box with an invalid rectangle (the reversed rectangle).
     * Any future drawing will make it valid again, so we won't blend back
     * more than we need. */
    result->dirty.p.x = rect->q.x;
    result->dirty.p.y = rect->q.y;
    result->dirty.q.x = rect->p.x;
    result->dirty.q.y = rect->p.y;
    return result;
}

static	void
pdf14_buf_free(pdf14_buf *buf)
{
    pdf14_group_color_t *group_color_info = buf->group_color_info;
    gs_memory_t *memory = buf->memory;

    if (buf->mask_stack && buf->mask_stack->rc_mask)
        rc_decrement(buf->mask_stack->rc_mask, "pdf14_buf_free");

    gs_free_object(memory, buf->mask_stack, "pdf14_buf_free");
    gs_free_object(memory, buf->transfer_fn, "pdf14_buf_free");
    gs_free_object(memory, buf->matte, "pdf14_buf_free");
    gs_free_object(memory, buf->data, "pdf14_buf_free");

    while (group_color_info) {
       if (group_color_info->icc_profile != NULL) {
           gsicc_adjust_profile_rc(group_color_info->icc_profile, -1, "pdf14_buf_free");
       }
       buf->group_color_info = group_color_info->previous;
       gs_free_object(memory, group_color_info, "pdf14_buf_free");
       group_color_info = buf->group_color_info;
    }

    gs_free_object(memory, buf->backdrop, "pdf14_buf_free");
    gs_free_object(memory, buf, "pdf14_buf_free");
}

static void
rc_pdf14_maskbuf_free(gs_memory_t * mem, void *ptr_in, client_name_t cname)
{
    /* Ending the mask buffer. */
    pdf14_rcmask_t *rcmask = (pdf14_rcmask_t * ) ptr_in;
    /* free the pdf14 buffer. */
    if ( rcmask->mask_buf != NULL ){
        pdf14_buf_free(rcmask->mask_buf);
    }
    gs_free_object(mem, rcmask, "rc_pdf14_maskbuf_free");
}

static	pdf14_rcmask_t *
pdf14_rcmask_new(gs_memory_t *memory)
{
    pdf14_rcmask_t *result;

    result = gs_alloc_struct(memory, pdf14_rcmask_t, &st_pdf14_rcmask,
                             "pdf14_maskbuf_new");
    if (result == NULL)
        return NULL;
    rc_init_free(result, memory, 1, rc_pdf14_maskbuf_free);
    result->mask_buf = NULL;
    result->memory = memory;
    return result;
}

static	pdf14_ctx *
pdf14_ctx_new(gx_device *dev, bool deep)
{
    pdf14_ctx *result;
    gs_memory_t	*memory = dev->memory->stable_memory;

    result = gs_alloc_struct(memory, pdf14_ctx, &st_pdf14_ctx, "pdf14_ctx_new");
    if (result == NULL)
        return result;
    result->stack = NULL;
    result->mask_stack = pdf14_mask_element_new(memory);
    result->mask_stack->rc_mask = pdf14_rcmask_new(memory);
    result->memory = memory;
    result->smask_depth = 0;
    result->smask_blend = false;
    result->deep = deep;
    result->base_color = NULL;
    return result;
}

static	void
pdf14_ctx_free(pdf14_ctx *ctx)
{
    pdf14_buf *buf, *next;

    if (ctx->base_color) {
       gsicc_adjust_profile_rc(ctx->base_color->icc_profile, -1, "pdf14_ctx_free");
        gs_free_object(ctx->memory, ctx->base_color, "pdf14_ctx_free");
    }
    if (ctx->mask_stack) {
        /* A mask was created but was not used in this band. */
        rc_decrement(ctx->mask_stack->rc_mask, "pdf14_ctx_free");
        gs_free_object(ctx->memory, ctx->mask_stack, "pdf14_ctx_free");
    }
    for (buf = ctx->stack; buf != NULL; buf = next) {
        next = buf->saved;
        pdf14_buf_free(buf);
    }
    gs_free_object (ctx->memory, ctx, "pdf14_ctx_free");
}

/**
 * pdf14_find_backdrop_buf: Find backdrop buffer.
 *
 * Return value: Backdrop buffer for current group operation, or NULL
 * if backdrop is fully transparent.
 **/
static	pdf14_buf *
pdf14_find_backdrop_buf(pdf14_ctx *ctx, bool *is_backdrop)
{
    /* Our new buffer is buf */
    pdf14_buf *buf = ctx->stack;

    *is_backdrop = false;

    if (buf != NULL) {
        /* If the new buffer is isolated there is no backdrop */
        if (buf->isolated) return NULL;

        /* If the previous buffer is a knockout group
           then we need to use its backdrop as the backdrop. If
           it was isolated then that back drop was NULL */
        if (buf->saved != NULL && buf->saved->knockout) {
            /* Per the spec, if we have a non-isolated group
               in a knockout group the non-isolated group
               uses the backdrop of its parent group (the knockout group)
               as its own backdrop.  The non-isolated group must
               go through the standard re-composition operation
               to avoid the double application of the backdrop */
            *is_backdrop = true;
            return buf->saved;
        }
        /* This should be the non-isolated case where its parent is
           not a knockout */
        if (buf->saved != NULL) {
            return buf->saved;
        }
    }
    return NULL;
}

static pdf14_group_color_t*
pdf14_make_base_group_color(gx_device* dev)
{
    pdf14_device* pdev = (pdf14_device*)dev;
    pdf14_group_color_t* group_color;
    bool deep = pdev->ctx->deep;

    if_debug0m('v', dev->memory, "[v]pdf14_make_base_group_color\n");

    group_color = gs_alloc_struct(pdev->ctx->memory,
        pdf14_group_color_t, &st_pdf14_clr,
        "pdf14_make_base_group_color");

    if (group_color == NULL)
        return NULL;
    memset(group_color, 0, sizeof(pdf14_group_color_t));

    group_color->blend_procs = pdev->blend_procs;
    group_color->polarity = pdev->color_info.polarity;
    group_color->num_components = pdev->color_info.num_components;
    group_color->isadditive = pdev->ctx->additive;
    group_color->unpack_procs = pdev->pdf14_procs;
    group_color->max_color = pdev->color_info.max_color = deep ? 65535 : 255;
    group_color->max_gray = pdev->color_info.max_gray = deep ? 65535 : 255;
    group_color->depth = pdev->color_info.depth;
    group_color->decode = dev_proc(pdev, decode_color);
    group_color->encode = dev_proc(pdev, encode_color);
    group_color->group_color_mapping_procs = dev_proc(pdev, get_color_mapping_procs);
    group_color->group_color_comp_index = dev_proc(pdev, get_color_comp_index);
    memcpy(&(group_color->comp_bits), &(pdev->color_info.comp_bits),
        GX_DEVICE_COLOR_MAX_COMPONENTS);
    memcpy(&(group_color->comp_shift), &(pdev->color_info.comp_shift),
        GX_DEVICE_COLOR_MAX_COMPONENTS);
    group_color->get_cmap_procs = pdf14_get_cmap_procs;
    group_color->icc_profile =
        pdev->icc_struct->device_profile[GS_DEFAULT_DEVICE_PROFILE];
    gsicc_adjust_profile_rc(group_color->icc_profile, 1, "pdf14_make_base_group_color");

    return group_color;
}

/* This wil create the first buffer when we have
   either the first drawing operation or transparency
   group push.  At that time, the color space in which
   we are going to be doing the alpha blend will be known. */
static int
pdf14_initialize_ctx(gx_device* dev, int n_chan, bool additive, const gs_gstate* pgs)
{
    pdf14_device* pdev = (pdf14_device*)dev;
    bool has_tags = device_encodes_tags(dev);
    int num_spots = pdev->ctx->num_spots;
    pdf14_buf* buf;
    gs_memory_t* memory = dev->memory->stable_memory;

    /* Check for a blank idle group as a base group */
    if (pdev->ctx->stack != NULL && pdev->ctx->stack->group_popped &&
        pdev->ctx->stack->idle) {
        pdf14_buf_free(pdev->ctx->stack);
        pdev->ctx->stack = NULL;
    }

    if (pdev->ctx->stack != NULL)
        return 0;

    if_debug2m('v', dev->memory, "[v]pdf14_initialize_ctx: width = %d, height = %d\n",
        dev->width, dev->height);

    buf = pdf14_buf_new(&(pdev->ctx->rect), has_tags, false, false, false, n_chan + 1,
        num_spots, memory, pdev->ctx->deep);
    if (buf == NULL) {
        return gs_error_VMerror;
    }
    if_debug5m('v', memory,
        "[v]base buf: %d x %d, %d color channels, %d planes, deep=%d\n",
        buf->rect.q.x, buf->rect.q.y, buf->n_chan, buf->n_planes, pdev->ctx->deep);

    /* This check is not really needed */
    if (buf->data != NULL) {
        /* Memsetting by 0, so this copes with the deep case too */
        if (buf->has_tags) {
            memset(buf->data, 0, (size_t)buf->planestride * (buf->n_planes - 1));
        }
        else {
            memset(buf->data, 0, (size_t)buf->planestride * buf->n_planes);
        }
    }
    buf->saved = NULL;
    pdev->ctx->stack = buf;
    pdev->ctx->n_chan = n_chan;
    pdev->ctx->additive = additive;

    /* Every buffer needs group color information including the base
       one that is created for when we have no group */
    buf->group_color_info = gs_alloc_struct(pdev->memory->stable_memory,
            pdf14_group_color_t, &st_pdf14_clr, "pdf14_initialize_ctx");
    if (buf->group_color_info == NULL)
        return gs_error_VMerror;

    if (pgs != NULL)
        buf->group_color_info->get_cmap_procs = pgs->get_cmap_procs;
    else
        buf->group_color_info->get_cmap_procs = pdf14_get_cmap_procs;

    buf->group_color_info->group_color_mapping_procs =
        dev_proc(pdev, get_color_mapping_procs);
    buf->group_color_info->group_color_comp_index =
        dev_proc(pdev, get_color_comp_index);
    buf->group_color_info->blend_procs = pdev->blend_procs;
    buf->group_color_info->polarity = pdev->color_info.polarity;
    buf->group_color_info->num_components = pdev->color_info.num_components;
    buf->group_color_info->isadditive = pdev->ctx->additive;
    buf->group_color_info->unpack_procs = pdev->pdf14_procs;
    buf->group_color_info->depth = pdev->color_info.depth;
    buf->group_color_info->max_color = pdev->color_info.max_color;
    buf->group_color_info->max_gray = pdev->color_info.max_gray;
    buf->group_color_info->encode = dev_proc(pdev, encode_color);
    buf->group_color_info->decode = dev_proc(pdev, decode_color);
    memcpy(&(buf->group_color_info->comp_bits), &(pdev->color_info.comp_bits),
        GX_DEVICE_COLOR_MAX_COMPONENTS);
    memcpy(&(buf->group_color_info->comp_shift), &(pdev->color_info.comp_shift),
        GX_DEVICE_COLOR_MAX_COMPONENTS);
    buf->group_color_info->previous = NULL;  /* used during clist writing */
    buf->group_color_info->icc_profile =
        pdev->icc_struct->device_profile[GS_DEFAULT_DEVICE_PROFILE];
    if (buf->group_color_info->icc_profile != NULL)
        gsicc_adjust_profile_rc(buf->group_color_info->icc_profile, 1, "pdf14_initialize_ctx");

    return 0;
}

static pdf14_group_color_t*
pdf14_clone_group_color_info(gx_device* pdev, pdf14_group_color_t* src)
{
    pdf14_group_color_t* des = gs_alloc_struct(pdev->memory->stable_memory,
        pdf14_group_color_t, &st_pdf14_clr, "pdf14_clone_group_color_info");
    if (des == NULL)
        return NULL;

    memcpy(des, src, sizeof(pdf14_group_color_t));
    if (des->icc_profile != NULL)
        gsicc_adjust_profile_rc(des->icc_profile, 1, "pdf14_clone_group_color_info");
    des->previous = NULL;  /* used during clist writing for state stack */

    return des;
}

static	int
pdf14_push_transparency_group(pdf14_ctx	*ctx, gs_int_rect *rect, bool isolated,
                              bool knockout, uint16_t alpha, uint16_t shape, uint16_t opacity,
                              gs_blend_mode_t blend_mode, bool idle, uint mask_id,
                              int numcomps, bool cm_back_drop, bool shade_group,
                              cmm_profile_t *group_profile, cmm_profile_t *tos_profile,
                              pdf14_group_color_t* group_color, gs_gstate *pgs,
                              gx_device *dev)
{
    pdf14_buf *tos = ctx->stack;
    pdf14_buf *buf, * pdf14_backdrop;
    bool has_shape = false;
    bool is_backdrop;
    int num_spots;

    if_debug1m('v', ctx->memory,
               "[v]pdf14_push_transparency_group, idle = %d\n", idle);

    if (tos != NULL)
        has_shape = tos->has_shape || tos->knockout;

    if (ctx->smask_depth > 0)
        num_spots = 0;
    else
        num_spots = ctx->num_spots;


    buf = pdf14_buf_new(rect, ctx->has_tags, !isolated, has_shape, idle, numcomps + 1,
                        num_spots, ctx->memory, ctx->deep);
    if (buf == NULL)
        return_error(gs_error_VMerror);

    if_debug4m('v', ctx->memory,
        "[v]base buf: %d x %d, %d color channels, %d planes\n",
        buf->rect.q.x, buf->rect.q.y, buf->n_chan, buf->n_planes);
    buf->isolated = isolated;
    buf->knockout = knockout;
    buf->alpha = alpha;
    buf->shape = shape;
    buf->opacity = opacity;
    buf->blend_mode = blend_mode;
    buf->mask_id = mask_id;
    buf->mask_stack = ctx->mask_stack; /* Save because the group rendering may
                                          set up another (nested) mask. */
    ctx->mask_stack = NULL; /* Clean the mask field for rendering this group.
                            See pdf14_pop_transparency_group how to handle it. */
    buf->saved = tos;
    buf->group_color_info = group_color;

    if (tos == NULL)
        buf->page_group = true;

    ctx->stack = buf;
    if (buf->data == NULL)
        return 0;
    if (idle)
        return 0;
    pdf14_backdrop = pdf14_find_backdrop_buf(ctx, &is_backdrop);

    /* Initializes buf->data with the backdrop or as opaque */
    if (pdf14_backdrop == NULL || (is_backdrop && pdf14_backdrop->backdrop == NULL)) {
        /* Note, don't clear out tags set by pdf14_buf_new == GS_UNKNOWN_TAG */
        /* Memsetting by 0, so this copes with the deep case too */
        memset(buf->data, 0, (size_t)buf->planestride *
                                          (buf->n_chan +
                                           (buf->has_shape ? 1 : 0) +
                                           (buf->has_alpha_g ? 1 : 0)));
    } else {
        if (!cm_back_drop) {
            pdf14_preserve_backdrop(buf, pdf14_backdrop, is_backdrop
#if RAW_DUMP
                                    , ctx->memory
#endif
                                    );
        } else {
            /* We must have an non-isolated group with a mismatch in color spaces.
                In this case, we can't just copy the buffer but must CM it */
            pdf14_preserve_backdrop_cm(buf, group_profile, pdf14_backdrop, tos_profile,
                                        ctx->memory, pgs, dev, is_backdrop);
        }
    }

    /* If our new group is a non-isolated knockout group, we have to maintain
       a copy of the backdrop in case we are drawing nonisolated groups on top of the
       knockout group. They have to always blend with the groups backdrop
       not what is currently drawn in the group. Selection of the backdrop
       depends upon the properties of the parent group. For example, if
       the parent itself is a knockout group we actually
       need to blend with its backdrop. This could be NULL if the parent was
       an isolated knockout group. */
    if (buf->knockout && pdf14_backdrop != NULL) {
        buf->backdrop = gs_alloc_bytes(ctx->memory,
                                       (size_t)buf->planestride * buf->n_planes + CAL_SLOP,
                                       "pdf14_push_transparency_group");
        if (buf->backdrop == NULL) {
            return gs_throw(gs_error_VMerror, "Knockout backdrop allocation failed");
        }

        memcpy(buf->backdrop, buf->data,
               (size_t)buf->planestride * buf->n_planes);

#if RAW_DUMP
        /* Dump the current buffer to see what we have. */
        dump_raw_buffer(ctx->memory,
            ctx->stack->rect.q.y - ctx->stack->rect.p.y,
            ctx->stack->rowstride >> buf->deep, buf->n_planes,
            ctx->stack->planestride, ctx->stack->rowstride,
            "KnockoutBackDrop", buf->backdrop, buf->deep);
        global_index++;
#endif
    }
#if RAW_DUMP
    /* Dump the current buffer to see what we have. */
    dump_raw_buffer(ctx->memory,
                    ctx->stack->rect.q.y-ctx->stack->rect.p.y,
                    ctx->stack->rowstride>>buf->deep, ctx->stack->n_planes,
                    ctx->stack->planestride, ctx->stack->rowstride,
                    "TransGroupPush", ctx->stack->data, buf->deep);
    global_index++;
#endif
    return 0;
}

static	int
pdf14_pop_transparency_group(gs_gstate *pgs, pdf14_ctx *ctx,
    const pdf14_nonseparable_blending_procs_t * pblend_procs,
    int tos_num_color_comp, cmm_profile_t *curr_icc_profile, gx_device *dev)
{
    pdf14_buf *tos = ctx->stack;
    pdf14_buf *nos = tos->saved;
    pdf14_mask_t *mask_stack = tos->mask_stack;
    pdf14_buf *maskbuf;
    int x0, x1, y0, y1;
    int nos_num_color_comp;
    bool no_icc_match;
    pdf14_device *pdev = (pdf14_device *)dev;
    bool overprint = pdev->overprint;
    gx_color_index drawn_comps = pdev->drawn_comps_stroke | pdev->drawn_comps_fill;
    bool has_matte = false;
    int code = 0;

#ifdef DEBUG
    pdf14_debug_mask_stack_state(ctx);
#endif
    if (mask_stack == NULL) {
        maskbuf = NULL;
    }
    else {
        maskbuf = mask_stack->rc_mask->mask_buf;
    }

    if (maskbuf != NULL && maskbuf->matte != NULL)
        has_matte = true;

    /* Check if this is our last buffer, if yes, there is nothing to
       compose to.  Keep this buffer until we have the put image.
       If we have another group push, this group must be destroyed.
       This only occurs sometimes when at clist creation time
       push_shfill_group occured and nothing was drawn in this group.
       There is also the complication if we have a softmask.  There
       are two approaches to this problem.  Apply the softmask during
       the put image or handle it now.  I choose the later as the
       put_image code is already way to complicated. */
    if (nos == NULL && maskbuf == NULL) {
        tos->group_popped = true;
        return 0;
    }

    /* Here is the case with the soft mask.  Go ahead and create a new
       target buffer (nos) with the same color information etc, but blank
       and go ahead and do the blend with the softmask so that it gets applied. */
    if (nos == NULL && maskbuf != NULL) {
        nos = pdf14_buf_new(&(tos->rect), ctx->has_tags, !tos->isolated, tos->has_shape,
            tos->idle, tos->n_chan, tos->num_spots, ctx->memory, ctx->deep);
        if (nos == NULL) {
            code = gs_error_VMerror;
            goto exit;
        }

        if_debug4m('v', ctx->memory,
            "[v] special buffer for softmask application: %d x %d, %d color channels, %d planes\n",
            nos->rect.q.x, nos->rect.q.y, nos->n_chan, nos->n_planes);

        nos->dirty = tos->dirty;
        nos->isolated = tos->isolated;
        nos->knockout = tos->knockout;
        nos->alpha = 65535;
        nos->shape = 65535;
        nos->opacity = 65535;
        nos->blend_mode = tos->blend_mode;
        nos->mask_id = tos->mask_id;
        nos->group_color_info = pdf14_clone_group_color_info(dev, tos->group_color_info);

        if (nos->data != NULL)
            memset(nos->data, 0,
                   (size_t)nos->planestride *
                                          (nos->n_chan +
                                           (nos->has_shape ? 1 : 0) +
                                           (nos->has_alpha_g ? 1 : 0)));
    }

    nos_num_color_comp = nos->group_color_info->num_components - tos->num_spots;
    tos_num_color_comp = tos_num_color_comp - tos->num_spots;

    /* Sanitise the dirty rectangles, in case some of the drawing routines
     * have made them overly large. */
    rect_intersect(tos->dirty, tos->rect);
    rect_intersect(nos->dirty, nos->rect);
    /* dirty = the marked bbox. rect = the entire bounds of the buffer. */
    /* Everything marked on tos that fits onto nos needs to be merged down. */
    y0 = max(tos->dirty.p.y, nos->rect.p.y);
    y1 = min(tos->dirty.q.y, nos->rect.q.y);
    x0 = max(tos->dirty.p.x, nos->rect.p.x);
    x1 = min(tos->dirty.q.x, nos->rect.q.x);
    if (ctx->mask_stack) {
        /* This can occur when we have a situation where we are ending out of
           a group that has internal to it a soft mask and another group.
           The soft mask left over from the previous trans group pop is put
           into ctx->masbuf, since it is still active if another trans group
           push occurs to use it.  If one does not occur, but instead we find
           ourselves popping from a parent group, then this softmask is no
           longer needed.  We will rc_decrement and set it to NULL. */
        rc_decrement(ctx->mask_stack->rc_mask, "pdf14_pop_transparency_group");
        if (ctx->mask_stack->rc_mask == NULL ){
            gs_free_object(ctx->memory, ctx->mask_stack, "pdf14_pop_transparency_group");
        }
        ctx->mask_stack = NULL;
    }
    ctx->mask_stack = mask_stack;  /* Restore the mask saved by pdf14_push_transparency_group. */
    tos->mask_stack = NULL;        /* Clean the pointer sinse the mask ownership is now passed to ctx. */
    if (tos->idle)
        goto exit;
    if (maskbuf != NULL && maskbuf->data == NULL && maskbuf->alpha == 255)
        goto exit;

#if RAW_DUMP
    /* Dump the current buffer to see what we have. */
    dump_raw_buffer(ctx->memory,
                    ctx->stack->rect.q.y-ctx->stack->rect.p.y,
                    ctx->stack->rowstride>>ctx->stack->deep, ctx->stack->n_planes,
                    ctx->stack->planestride, ctx->stack->rowstride,
                    "aaTrans_Group_Pop", ctx->stack->data, ctx->stack->deep);
    global_index++;
#endif
/* Note currently if a pattern space has transparency, the ICC profile is not used
   for blending purposes.  Instead we rely upon the gray, rgb, or cmyk parent space.
   This is partially due to the fact that pdf14_pop_transparency_group and
   pdf14_push_transparnecy_group have no real ICC interaction and those are the
   operations called in the tile transparency code.  Instead we may want to
   look at pdf14_begin_transparency_group and pdf14_end_transparency group which
   is where all the ICC information is handled.  We will return to look at that later */
    if (nos->group_color_info->icc_profile != NULL) {
        no_icc_match = !gsicc_profiles_equal(nos->group_color_info->icc_profile, curr_icc_profile);
    } else {
        /* Let the other tests make the decision if we need to transform */
        no_icc_match = false;
    }
    /* If the color spaces are different and we actually did do a swap of
       the procs for color */
    if ((nos->group_color_info->group_color_mapping_procs != NULL &&
        nos_num_color_comp != tos_num_color_comp) || no_icc_match) {
        if (x0 < x1 && y0 < y1) {
            pdf14_buf *result;
            bool did_alloc; /* We don't care here */

            if (has_matte) {
                result = pdf14_transform_color_buffer_with_matte(pgs, ctx, dev,
                    tos, tos->data, curr_icc_profile, nos->group_color_info->icc_profile,
                    tos->rect.p.x, tos->rect.p.y, tos->rect.q.x - tos->rect.p.x,
                    tos->rect.q.y - tos->rect.p.y, &did_alloc, tos->deep, false);
                has_matte = false;
            } else {
                result = pdf14_transform_color_buffer_no_matte(pgs, ctx, dev,
                    tos, tos->data, curr_icc_profile, nos->group_color_info->icc_profile,
                    tos->rect.p.x, tos->rect.p.y, tos->rect.q.x - tos->rect.p.x,
                    tos->rect.q.y - tos->rect.p.y, &did_alloc, tos->deep, false);
            }
            if (result == NULL) {
                /* Clean up and return error code */
                code = gs_error_unknownerror;
                goto exit;
            }

#if RAW_DUMP
            /* Dump the current buffer to see what we have. */
            dump_raw_buffer(ctx->memory,
                            ctx->stack->rect.q.y-ctx->stack->rect.p.y,
                            ctx->stack->rowstride>>ctx->stack->deep, ctx->stack->n_chan,
                            ctx->stack->planestride, ctx->stack->rowstride,
                            "aCMTrans_Group_ColorConv", ctx->stack->data,
                            ctx->stack->deep);
#endif
             /* compose. never do overprint in this case */
            pdf14_compose_group(tos, nos, maskbuf, x0, x1, y0, y1, nos->n_chan,
                 nos->group_color_info->isadditive,
                 nos->group_color_info->blend_procs,
                 has_matte, false, drawn_comps, ctx->memory, dev);
        }
    } else {
        /* Group color spaces are the same.  No color conversions needed */
        if (x0 < x1 && y0 < y1)
            pdf14_compose_group(tos, nos, maskbuf, x0, x1, y0, y1, nos->n_chan,
                                ctx->additive, pblend_procs, has_matte, overprint,
                                drawn_comps, ctx->memory, dev);
    }
exit:
    ctx->stack = nos;
    /* We want to detect the cases where we have luminosity soft masks embedded
       within one another.  The "alpha" channel really needs to be merged into
       the luminosity channel in this case.  This will occur during the mask pop */
    if (ctx->smask_depth > 0 && maskbuf != NULL) {
        /* Set the trigger so that we will blend if not alpha. Since
           we have softmasks embedded in softmasks */
        ctx->smask_blend = true;
    }
    if_debug1m('v', ctx->memory, "[v]pop buf, idle=%d\n", tos->idle);
    pdf14_buf_free(tos);
    if (code < 0)
        return_error(code);
    return 0;
}

/*
 * Create a transparency mask that will be used as the mask for
 * the next transparency group that is created afterwards.
 * The sequence of calls is:
 * push_mask, draw the mask, pop_mask, push_group, draw the group, pop_group
 */
static	int
pdf14_push_transparency_mask(pdf14_ctx *ctx, gs_int_rect *rect,	uint16_t bg_alpha,
                             byte *transfer_fn, bool is_ident, bool idle,
                             bool replacing, uint mask_id,
                             gs_transparency_mask_subtype_t subtype,
                             int numcomps, int Background_components,
                             const float Background[], int Matte_components,
                             const float Matte[], const float GrayBackground,
                             pdf14_group_color_t* group_color)
{
    pdf14_buf *buf;
    int i;

    if_debug2m('v', ctx->memory,
               "[v]pdf14_push_transparency_mask, idle=%d, replacing=%d\n",
               idle, replacing);
    ctx->smask_depth += 1;

    if (ctx->stack == NULL) {
        return_error(gs_error_VMerror);
    }

    /* An optimization to consider is that if the SubType is Alpha
       then we really should only be allocating the alpha band and
       only draw with that channel.  Current architecture makes that
       a bit tricky.  We need to create this based upon the size of
       the color space + an alpha channel. NOT the device size
       or the previous ctx size */
    /* A mask doesn't worry about tags */
    buf = pdf14_buf_new(rect, false, false, false, idle, numcomps + 1, 0,
                        ctx->memory, ctx->deep);
    if (buf == NULL)
        return_error(gs_error_VMerror);
    buf->alpha = bg_alpha;
    buf->is_ident = is_ident;
    /* fill in, but these values aren't really used */
    buf->isolated = true;
    buf->knockout = false;
    buf->shape = 0xffff;
    buf->blend_mode = BLEND_MODE_Normal;
    buf->transfer_fn = transfer_fn;
    buf->matte_num_comps = Matte_components;
    buf->group_color_info = group_color;

    if (Matte_components) {
        buf->matte = (uint16_t *)gs_alloc_bytes(ctx->memory, Matte_components * sizeof(uint16_t) + CAL_SLOP,
                                                "pdf14_push_transparency_mask");
        if (buf->matte == NULL)
            return_error(gs_error_VMerror);
        for (i = 0; i < Matte_components; i++) {
            buf->matte[i] = (uint16_t) floor(Matte[i] * 65535.0 + 0.5);
        }
    }
    buf->mask_id = mask_id;
    /* If replacing=false, we start the mask for an image with SMask.
       In this case the image's SMask temporary replaces the
       mask of the containing group. Save the containing droup's mask
       in buf->mask_stack */
    buf->mask_stack = ctx->mask_stack;
    if (buf->mask_stack){
        rc_increment(buf->mask_stack->rc_mask);
    }
#if RAW_DUMP
    /* Dump the current buffer to see what we have. */
    if (ctx->stack->planestride > 0 ){
        dump_raw_buffer(ctx->memory,
                        ctx->stack->rect.q.y-ctx->stack->rect.p.y,
                        ctx->stack->rowstride>>ctx->stack->deep, ctx->stack->n_planes,
                        ctx->stack->planestride, ctx->stack->rowstride,
                        "Raw_Buf_PreSmask", ctx->stack->data, ctx->stack->deep);
        global_index++;
    }
#endif
    buf->saved = ctx->stack;
    ctx->stack = buf;
    /* Soft Mask related information so we know how to
       compute luminosity when we pop the soft mask */
    buf->SMask_SubType = subtype;
    if (buf->data != NULL) {
        /* We need to initialize it to the BC if it existed */
        /* According to the spec, the CS has to be the same */
        /* If the back ground component is black, then don't bother
           with this.  Since we are forcing the rendering to gray
           earlier now, go ahead and just use the GrayBackGround color
           directly. */
        if ( Background_components && GrayBackground != 0.0 ) {
            if (buf->deep) {
                uint16_t gray = (uint16_t) (65535.0 * GrayBackground);
                gs_memset16(buf->data, gray, buf->planestride);
                /* If we have a background component that was not black, then we
                   need to set the alpha for this mask as if we had drawn in the
                   entire soft mask buffer */
                gs_memset16(buf->data + buf->planestride, 65535,
                            buf->planestride *(buf->n_chan - 1));
            } else {
                unsigned char gray = (unsigned char) (255.0 * GrayBackground);
                memset(buf->data, gray, buf->planestride);
                /* If we have a background component that was not black, then we
                   need to set the alpha for this mask as if we had drawn in the
                   entire soft mask buffer */
                memset(buf->data + buf->planestride, 255,
                       (size_t)buf->planestride * (buf->n_chan - 1));
            }
        } else {
            /* Compose mask with opaque background */
            memset(buf->data, 0, (size_t)buf->planestride * buf->n_chan);
        }
    }
    return 0;
}

static void pdf14_free_mask_stack(pdf14_ctx *ctx, gs_memory_t *memory)
{
    pdf14_mask_t *mask_stack = ctx->mask_stack;

    if (mask_stack->rc_mask != NULL) {
        pdf14_mask_t *curr_mask = mask_stack;
        pdf14_mask_t *old_mask;
        while (curr_mask != NULL) {
            rc_decrement(curr_mask->rc_mask, "pdf14_free_mask_stack");
            old_mask = curr_mask;
            curr_mask = curr_mask->previous;
            gs_free_object(old_mask->memory, old_mask, "pdf14_free_mask_stack");
        }
    } else {
        gs_free_object(memory, mask_stack, "pdf14_free_mask_stack");
    }
    ctx->mask_stack = NULL;
}

static	int
pdf14_pop_transparency_mask(pdf14_ctx *ctx, gs_gstate *pgs, gx_device *dev)
{
    pdf14_buf* tos = ctx->stack;
    pdf14_buf* nos = tos->saved;
    byte *new_data_buf;
    int icc_match;
    cmm_profile_t *des_profile = nos->group_color_info->icc_profile; /* If set, this should be a gray profile */
    cmm_profile_t *src_profile;
    gsicc_rendering_param_t rendering_params;
    gsicc_link_t *icc_link;
    gsicc_rendering_param_t render_cond;
    cmm_dev_profile_t *dev_profile;
    int code = 0;

    dev_proc(dev, get_profile)(dev,  &dev_profile);
    gsicc_extract_profile(GS_UNKNOWN_TAG, dev_profile, &src_profile,
                          &render_cond);
    ctx->smask_depth -= 1;
    /* icc_match == -1 means old non-icc code.
       icc_match == 0 means use icc code
       icc_match == 1 mean no conversion needed */
    if (des_profile != NULL && src_profile != NULL ) {
        icc_match = gsicc_profiles_equal(des_profile, src_profile);
    } else {
        icc_match = -1;
    }
    if_debug1m('v', ctx->memory, "[v]pdf14_pop_transparency_mask, idle=%d\n",
               tos->idle);
    ctx->stack = tos->saved;
    tos->saved = NULL;  /* To avoid issues with GC */
    if (tos->mask_stack) {
        /* During the soft mask push, the mask_stack was copied (not moved) from
           the ctx to the tos mask_stack. We are done with this now so it is safe to
           just set to NULL.  However, before we do that we must perform
           rc decrement to match the increment that occured was made.  Also,
           if this is the last ref count of the rc_mask, we should free the
           buffer now since no other groups need it. */
        rc_decrement(tos->mask_stack->rc_mask,
                     "pdf14_pop_transparency_mask(tos->mask_stack->rc_mask)");
        if (tos->mask_stack->rc_mask) {
            if (tos->mask_stack->rc_mask->rc.ref_count == 1){
                rc_decrement(tos->mask_stack->rc_mask,
                            "pdf14_pop_transparency_mask(tos->mask_stack->rc_mask)");
            }
        }
        tos->mask_stack = NULL;
    }
    if (tos->data == NULL ) {
        /* This can occur in clist rendering if the soft mask does
           not intersect the current band.  It would be nice to
           catch this earlier and just avoid creating the structure
           to begin with.  For now we need to delete the structure
           that was created.  Only delete if the alpha value is 65535 */
        if ((tos->alpha == 65535 && tos->is_ident) ||
            (!tos->is_ident && (tos->transfer_fn[tos->alpha>>8] == 255))) {
            pdf14_buf_free(tos);
            if (ctx->mask_stack != NULL) {
                pdf14_free_mask_stack(ctx, ctx->memory);
            }
        } else {
            /* Assign as mask buffer */
            if (ctx->mask_stack != NULL) {
                pdf14_free_mask_stack(ctx, ctx->memory);
            }
            ctx->mask_stack = pdf14_mask_element_new(ctx->memory);
            ctx->mask_stack->rc_mask = pdf14_rcmask_new(ctx->memory);
            ctx->mask_stack->rc_mask->mask_buf = tos;
        }
        ctx->smask_blend = false;  /* just in case */
    } else {
        /* If we are already in the source space then there is no reason
           to do the transformation */
        /* Lets get this to a monochrome buffer and map it to a luminance only value */
        /* This will reduce our memory.  We won't reuse the existing one, due */
        /* Due to the fact that on certain systems we may have issues recovering */
        /* the data after a resize */
        new_data_buf = gs_alloc_bytes(ctx->memory, tos->planestride + CAL_SLOP,
                                        "pdf14_pop_transparency_mask");
        if (new_data_buf == NULL)
            return_error(gs_error_VMerror);
        /* Initialize with 0.  Need to do this since in Smask_Luminosity_Mapping
           we won't be filling everything during the remap if it had not been
           written into by the PDF14 fill rect */
        memset(new_data_buf, 0, tos->planestride);
        /* If the subtype was alpha, then just grab the alpha channel now
           and we are all done */
        if (tos->SMask_SubType == TRANSPARENCY_MASK_Alpha) {
            ctx->smask_blend = false;  /* not used in this case */
            smask_copy(tos->rect.q.y - tos->rect.p.y,
                       (tos->rect.q.x - tos->rect.p.x)<<tos->deep,
                       tos->rowstride,
                       (tos->data)+tos->planestride, new_data_buf);
#if RAW_DUMP
            /* Dump the current buffer to see what we have. */
            dump_raw_buffer(ctx->memory,
                            tos->rect.q.y-tos->rect.p.y,
                            tos->rowstride>>tos->deep, tos->n_planes,
                            tos->planestride, tos->rowstride,
                            "SMask_Pop_Alpha(Mask_Plane1)",tos->data,
                            tos->deep);
            global_index++;
#endif
        } else {
            if (icc_match == 1 || tos->n_chan == 2) {
#if RAW_DUMP
                /* Dump the current buffer to see what we have. */
                dump_raw_buffer(ctx->memory,
                                tos->rect.q.y-tos->rect.p.y,
                                tos->rowstride>>tos->deep, tos->n_planes,
                                tos->planestride, tos->rowstride,
                                "SMask_Pop_Lum(Mask_Plane0)",tos->data,
                                tos->deep);
                global_index++;
#endif
                /* There is no need to color convert.  Data is already gray scale.
                   We just need to copy the gray plane.  However it is
                   possible that the soft mask could have a soft mask which
                   would end us up with some alpha blending information
                   (Bug691803). In fact, according to the spec, the alpha
                   blending has to occur.  See FTS test fts_26_2601.pdf
                   for an example of this.  Softmask buffer is intialized
                   with BG values.  It would be nice to keep track if buffer
                   ever has a alpha value not 1 so that we could detect and
                   avoid this blend if not needed. */
                smask_blend(tos->data, tos->rect.q.x - tos->rect.p.x,
                            tos->rect.q.y - tos->rect.p.y, tos->rowstride,
                            tos->planestride, tos->deep);
#if RAW_DUMP
                /* Dump the current buffer to see what we have. */
                dump_raw_buffer(ctx->memory,
                                tos->rect.q.y-tos->rect.p.y,
                                tos->rowstride>>tos->deep, tos->n_planes,
                                tos->planestride, tos->rowstride,
                                "SMask_Pop_Lum_Post_Blend",tos->data,
                                tos->deep);
                global_index++;
#endif
                smask_copy(tos->rect.q.y - tos->rect.p.y,
                           (tos->rect.q.x - tos->rect.p.x)<<tos->deep,
                           tos->rowstride, tos->data, new_data_buf);
            } else {
                if ( icc_match == -1 ) {
                    /* The slow old fashioned way */
                    smask_luminosity_mapping(tos->rect.q.y - tos->rect.p.y ,
                        tos->rect.q.x - tos->rect.p.x,tos->n_chan,
                        tos->rowstride, tos->planestride,
                        tos->data,  new_data_buf, ctx->additive, tos->SMask_SubType,
                        tos->deep
#if RAW_DUMP
                        , ctx->memory
#endif
                        );
                } else {
                    /* ICC case where we use the CMM */
                    /* Request the ICC link for the transform that we will need to use */
                    rendering_params.black_point_comp = gsBLACKPTCOMP_OFF;
                    rendering_params.graphics_type_tag = GS_IMAGE_TAG;
                    rendering_params.override_icc = false;
                    rendering_params.preserve_black = gsBKPRESNOTSPECIFIED;
                    rendering_params.rendering_intent = gsPERCEPTUAL;
                    rendering_params.cmm = gsCMM_DEFAULT;
                    icc_link = gsicc_get_link_profile(pgs, dev, des_profile,
                        src_profile, &rendering_params, pgs->memory, false);
                    code = smask_icc(dev, tos->rect.q.y - tos->rect.p.y,
                              tos->rect.q.x - tos->rect.p.x, tos->n_chan,
                              tos->rowstride, tos->planestride,
                              tos->data, new_data_buf, icc_link, tos->deep);
                    /* Release the link */
                    gsicc_release_link(icc_link);
                }
            }
        }
        /* Free the old object, NULL test was above */
        gs_free_object(ctx->memory, tos->data, "pdf14_pop_transparency_mask");
        tos->data = new_data_buf;
        /* Data is single channel now */
        tos->n_chan = 1;
        tos->n_planes = 1;
        /* Assign as reference counted mask buffer */
        if (ctx->mask_stack != NULL) {
            /* In this case, the source file is wacky as it already had a
               softmask and now is getting a replacement. We need to clean
               up the softmask stack before doing this free and creating
               a new stack. Bug 693312 */
            pdf14_free_mask_stack(ctx, ctx->memory);
        }
        ctx->mask_stack = pdf14_mask_element_new(ctx->memory);
        if (ctx->mask_stack == NULL)
            return gs_note_error(gs_error_VMerror);
        ctx->mask_stack->rc_mask = pdf14_rcmask_new(ctx->memory);
        if (ctx->mask_stack->rc_mask == NULL)
            return gs_note_error(gs_error_VMerror);
        ctx->mask_stack->rc_mask->mask_buf = tos;
    }
    return code;
}

static pdf14_mask_t *
pdf14_mask_element_new(gs_memory_t *memory)
{
    pdf14_mask_t *result;

    result = gs_alloc_struct(memory, pdf14_mask_t, &st_pdf14_mask,
                             "pdf14_mask_element_new");
    if (result == NULL)
        return NULL;
    /* Get the reference counted mask */
    result->rc_mask = NULL;
    result->previous = NULL;
    result->memory = memory;
    return result;
}

static int
pdf14_push_transparency_state(gx_device *dev, gs_gstate *pgs)
{
    /* We need to push the current soft mask.  We need to
       be able to recover it if we draw a new one and
       then obtain a Q operation ( a pop ) */

    pdf14_device *pdev = (pdf14_device *)dev;
    pdf14_ctx *ctx = pdev->ctx;
    pdf14_mask_t *new_mask;

    if_debug0m('v', ctx->memory, "pdf14_push_transparency_state\n");
    /* We need to push the current mask buffer   */
    /* Allocate a new element for the stack.
       Don't do anything if there is no mask present.*/
    if (ctx->mask_stack != NULL) {
        new_mask = pdf14_mask_element_new(ctx->memory);
        /* Duplicate and make the link */
        new_mask->rc_mask = ctx->mask_stack->rc_mask;
        rc_increment(new_mask->rc_mask);
        new_mask->previous = ctx->mask_stack;
        ctx->mask_stack = new_mask;
    }
#ifdef DEBUG
    pdf14_debug_mask_stack_state(pdev->ctx);
#endif
    return 0;
}

static int
pdf14_pop_transparency_state(gx_device *dev, gs_gstate *pgs)
{
    /* Pop the soft mask.  It is no longer needed. Likely due to
       a Q that has occurred. */
    pdf14_device *pdev = (pdf14_device *)dev;
    pdf14_ctx *ctx = pdev->ctx;
    pdf14_mask_t *old_mask;

    if_debug0m('v', ctx->memory, "pdf14_pop_transparency_state\n");
    /* rc decrement the current link after we break it from
       the list, then free the stack element.  Don't do
       anything if there is no mask present. */
    if (ctx->mask_stack != NULL) {
        old_mask = ctx->mask_stack;
        ctx->mask_stack = ctx->mask_stack->previous;
        if (old_mask->rc_mask) {
            rc_decrement(old_mask->rc_mask, "pdf14_pop_transparency_state");
        }
        gs_free_object(old_mask->memory, old_mask, "pdf14_pop_transparency_state");
        /* We need to have some special handling here for when we have nested
           soft masks.  There may be a copy in the stack that we may need to
           adjust. */
        if (ctx->smask_depth > 0) {
            if (ctx->stack != NULL && ctx->stack->mask_stack != NULL) {
                ctx->stack->mask_stack = ctx->mask_stack;
            }
        }
    }
#ifdef DEBUG
    pdf14_debug_mask_stack_state(pdev->ctx);
#endif
    return 0;
}

static	int
pdf14_open(gx_device *dev)
{
    pdf14_device *pdev = (pdf14_device *)dev;

    /* If we are reenabling the device dont create a new ctx. Bug 697456 */
    if (pdev->ctx == NULL) {
        bool has_tags = device_encodes_tags(dev);
        int bits_per_comp = ((dev->color_info.depth - has_tags*8) /
                             dev->color_info.num_components);
        pdev->ctx = pdf14_ctx_new(dev, bits_per_comp > 8);
        if (pdev->ctx == NULL)
            return_error(gs_error_VMerror);

        pdev->ctx->rect.p.x = 0;
        pdev->ctx->rect.p.y = 0;
        pdev->ctx->rect.q.x = dev->width;
        pdev->ctx->rect.q.y = dev->height;
        pdev->ctx->has_tags = has_tags;
        pdev->ctx->num_spots = pdev->color_info.num_components - pdev->num_std_colorants;
        pdev->ctx->additive = (pdev->color_info.polarity == GX_CINFO_POLARITY_ADDITIVE);
        pdev->ctx->n_chan = pdev->color_info.num_components;
    }
    pdev->free_devicen = true;
    pdev->text_group = PDF14_TEXTGROUP_NO_BT;
    return 0;
}

static const gx_cm_color_map_procs pdf14_DeviceCMYKspot_procs = {
    pdf14_gray_cs_to_cmyk_cm, pdf14_rgb_cs_to_cmyk_cm, pdf14_cmyk_cs_to_cmyk_cm
};

static const gx_cm_color_map_procs pdf14_DeviceRGBspot_procs = {
    pdf14_gray_cs_to_rgbspot_cm, pdf14_rgb_cs_to_rgbspot_cm, pdf14_cmyk_cs_to_rgbspot_cm
};

static const gx_cm_color_map_procs pdf14_DeviceGrayspot_procs = {
    pdf14_gray_cs_to_grayspot_cm, pdf14_rgb_cs_to_grayspot_cm, pdf14_cmyk_cs_to_grayspot_cm
};

static const gx_cm_color_map_procs *
pdf14_cmykspot_get_color_mapping_procs(const gx_device * dev, const gx_device **tdev)
{
    *tdev = dev;
    return &pdf14_DeviceCMYKspot_procs;
}

static const gx_cm_color_map_procs *
pdf14_rgbspot_get_color_mapping_procs(const gx_device * dev, const gx_device **tdev)
{
    *tdev = dev;
    return &pdf14_DeviceRGBspot_procs;
}

static const gx_cm_color_map_procs *
pdf14_grayspot_get_color_mapping_procs(const gx_device * dev, const gx_device **tdev)
{
    *tdev = dev;
    return &pdf14_DeviceGrayspot_procs;
}

static void
be_rev_cpy(uint16_t *dst,const uint16_t *src,int n)
{
    for (; n != 0; n--) {
        uint16_t in = *src++;
        ((byte *)dst)[0] = in>>8;
        ((byte *)dst)[1] = in;
        dst++;
    }
}

/* Used to pass along information about the buffer created by the
   pdf14 device.  This is used by the pattern accumulator when the
   pattern contains transparency.  Note that if free_device is true then
   we need to go ahead and get the buffer data copied and free up the
   device.  This only occurs at the end of a pattern accumulation operation */
int
pdf14_get_buffer_information(const gx_device * dev,
                             gx_pattern_trans_t *transbuff, gs_memory_t *mem,
                             bool free_device)
{
    const pdf14_device * pdev = (pdf14_device *)dev;
    pdf14_buf *buf;
    gs_int_rect rect;
    int x1,y1,width,height;

    if ( pdev->ctx == NULL){
        return 0;  /* this can occur if the pattern is a clist */
    }
#ifdef DEBUG
    pdf14_debug_mask_stack_state(pdev->ctx);
#endif
    buf = pdev->ctx->stack;
    rect = buf->rect;
    transbuff->buf = (free_device ? NULL : buf);
    x1 = min(pdev->width, rect.q.x);
    y1 = min(pdev->height, rect.q.y);
    width = x1 - rect.p.x;
    height = y1 - rect.p.y;

    transbuff->n_chan    = buf->n_chan;
    transbuff->has_tags  = buf->has_tags;
    transbuff->has_shape = buf->has_shape;
    transbuff->width     = buf->rect.q.x - buf->rect.p.x;
    transbuff->height    = buf->rect.q.y - buf->rect.p.y;
    transbuff->deep      = buf->deep;

    if (width <= 0 || height <= 0 || buf->data == NULL) {
        transbuff->planestride = 0;
        transbuff->rowstride = 0;
        return 0;
    }

    if (free_device) {
        transbuff->pdev14 = NULL;
        transbuff->rect = rect;
        if ((width < transbuff->width) || (height < transbuff->height)) {
            /* If the bbox is smaller than the whole buffer than go ahead and
               create a new one to use.  This can occur if we drew in a smaller
               area than was specified by the transparency group rect. */
            int rowstride = ((width + 3) & -4)<<buf->deep;
            int planestride = rowstride * height;
            int k, j;
            byte *buff_ptr_src, *buff_ptr_des;

            transbuff->planestride = planestride;
            transbuff->rowstride = rowstride;
            transbuff->transbytes =
                         gs_alloc_bytes(mem,
                                        (size_t)planestride *
                                                (buf->n_chan +
                                                 buf->has_tags ? 1 : 0) + CAL_SLOP,
                                        "pdf14_get_buffer_information");
            if (transbuff->transbytes == NULL)
                return gs_error_VMerror;

            transbuff->mem = mem;
            if (transbuff->deep) {
                for (j = 0; j < transbuff->n_chan; j++) {
                    buff_ptr_src = buf->data + j * buf->planestride +
                               buf->rowstride * rect.p.y + (rect.p.x<<buf->deep);
                    buff_ptr_des = transbuff->transbytes + j * planestride;
                    for (k = 0; k < height; k++) {
                        be_rev_cpy((uint16_t *)buff_ptr_des, (const uint16_t *)buff_ptr_src, rowstride>>1);
                        buff_ptr_des += rowstride;
                        buff_ptr_src += buf->rowstride;
                    }
                }
            } else {
                for (j = 0; j < transbuff->n_chan; j++) {
                    buff_ptr_src = buf->data + j * buf->planestride +
                               buf->rowstride * rect.p.y + (rect.p.x<<buf->deep);
                    buff_ptr_des = transbuff->transbytes + j * planestride;
                    for (k = 0; k < height; k++) {
                        memcpy(buff_ptr_des, buff_ptr_src, rowstride);
                        buff_ptr_des += rowstride;
                        buff_ptr_src += buf->rowstride;
                    }
                }
            }

        } else {
            /* The entire buffer is used.  Go ahead and grab the pointer and
               clear the pointer in the pdf14 device data buffer so it is not
               freed when we close the device */
            transbuff->planestride = buf->planestride;
            transbuff->rowstride = buf->rowstride;
            transbuff->transbytes = buf->data;
            transbuff->mem = buf->memory;
            buf->data = NULL;  /* So that the buffer is not freed */
            if (transbuff->deep) {
                /* We have the data in native endian. We need it in big endian. Do an in-place conversion. */
                /* FIXME: This is a nop on big endian machines. Is the compiler smart enough to spot that? */
                uint16_t *buff_ptr;
                int j, k, z;
                int rowstride = transbuff->rowstride>>1;
                int planestride = transbuff->planestride;
                for (j = 0; j < transbuff->n_chan; j++) {
                    buff_ptr = (uint16_t *)(transbuff->transbytes + j * planestride);
                    for (k = 0; k < height; k++) {
                        for (z = 0; z < width; z++) {
                            uint16_t in = buff_ptr[z];
                            ((byte *)(&buff_ptr[z]))[0] = in>>8;
                            ((byte *)(&buff_ptr[z]))[1] = in;
                        }
                        buff_ptr += rowstride;
                    }
                }
            }
        }
#if RAW_DUMP
        /* Dump the buffer that should be going into the pattern */;
        dump_raw_buffer_be(buf->memory,
                           height, width, transbuff->n_chan,
                           transbuff->planestride, transbuff->rowstride,
                           "pdf14_pattern_buff", transbuff->transbytes,
                           transbuff->deep);
        global_index++;
#endif
        /* Go ahead and free up the pdf14 device */
        dev_proc(dev, close_device)((gx_device *)dev);
    } else {
        /* Here we are coming from one of the fill image / pattern / mask
           operations */
        transbuff->pdev14 = dev;
        transbuff->planestride = buf->planestride;
        transbuff->rowstride = buf->rowstride;
        transbuff->transbytes = buf->data;
        transbuff->mem = buf->memory;
        transbuff->rect = rect;
#if RAW_DUMP
    /* Dump the buffer that should be going into the pattern */;
        dump_raw_buffer(buf->memory,
                        height, width, buf->n_chan,
                        pdev->ctx->stack->planestride, pdev->ctx->stack->rowstride,
                        "pdf14_pattern_buff",
                        buf->data,
                        transbuff->deep);
        global_index++;
#endif
    }
    return 0;
}

typedef void(*blend_image_row_proc_t) (const byte *gs_restrict buf_ptr,
    int planestride, int width, int num_comp, uint16_t bg, byte *gs_restrict linebuf);


static int
pdf14_put_image_color_convert(const pdf14_device* dev, gs_gstate* pgs, cmm_profile_t* src_profile,
                        cmm_dev_profile_t* dev_target_profile, pdf14_buf** buf,
                        byte** buf_ptr, bool was_blended, int x, int y, int width, int height)
{
    pdf14_buf* cm_result = NULL;
    cmm_profile_t* des_profile;
    gsicc_rendering_param_t render_cond;
    bool did_alloc;
    bool endian_swap;

    gsicc_extract_profile(GS_UNKNOWN_TAG, dev_target_profile, &des_profile,
        &render_cond);

#if RAW_DUMP
    dump_raw_buffer(dev->ctx->memory,
        height, width, (*buf)->n_planes, (*buf)->planestride,
        (*buf)->rowstride, "pdf14_put_image_color_convert_pre", *buf_ptr, (*buf)->deep);
    global_index++;
#endif

    /* If we are doing a 16 bit buffer it will be big endian if we have already done the
       blend, otherwise it will be native endian. GS expects its 16bit buffers to be BE
       but for sanity pdf14 device maintains 16bit buffers in native format.  The CMM
       will need to know if it is dealing with native or BE data. */
    if (was_blended && (*buf)->deep) {
        /* Data is in BE.  If we are in a LE machine, CMM will need to swap for
           color conversion */
#if ARCH_IS_BIG_ENDIAN
        endian_swap = false;
#else
        endian_swap = true;
#endif
    } else {
        /* Data is in native format. No swap needed for CMM */
        endian_swap = false;
    }

    cm_result = pdf14_transform_color_buffer_no_matte(pgs, dev->ctx, (gx_device*) dev, *buf,
        *buf_ptr, src_profile, des_profile, x, y, width,
        height, &did_alloc, (*buf)->deep, endian_swap);

    if (cm_result == NULL)
        return_error(gs_error_VMerror);

    /* Update */
    *buf = cm_result;

    /* Make sure our buf_ptr is pointing to the proper location */
    if (did_alloc)
        *buf_ptr = cm_result->data;  /* Note the lack of offset */

#if RAW_DUMP
    dump_raw_buffer(dev->ctx->memory,
        height, width, (*buf)->n_planes, (*buf)->planestride,
        (*buf)->rowstride, "pdf14_put_image_color_convert_post", *buf_ptr, (*buf)->deep);
    global_index++;
#endif
    return 0;
}

/**
 * pdf14_put_image: Put rendered image to target device.
 * @pdev: The PDF 1.4 rendering device.
 * @pgs: State for image draw operation.
 * @target: The target device.
 *
 * Puts the rendered image in @pdev's buffer to @target. This is called
 * as part of the sequence of popping the PDF 1.4 device filter.
 *
 * Return code: negative on error.
 **/
static	int
pdf14_put_image(gx_device * dev, gs_gstate * pgs, gx_device * target)
{
    const pdf14_device * pdev = (pdf14_device *)dev;
    int code;
    gs_image1_t image;
    gx_image_enum_common_t *info;
    pdf14_buf *buf = pdev->ctx->stack;
    gs_int_rect rect;
    int y;
    int num_comp;
    byte *linebuf, *linebuf_unaligned;
    gs_color_space *pcs;
    int x1, y1, width, height;
    byte *buf_ptr;
    int num_rows_left;
    cmm_profile_t* src_profile = NULL;
    cmm_profile_t* des_profile = NULL;
    cmm_dev_profile_t *pdf14dev_profile;
    cmm_dev_profile_t *dev_target_profile;
    uint16_t bg;
    bool has_tags = device_encodes_tags(dev);
    bool deep = pdev->ctx->deep;
    int planestride;
    int rowstride;
    blend_image_row_proc_t blend_row;
    bool color_mismatch = false;
    bool supports_alpha = false;
    int i;
    int alpha_offset, tag_offset;
    const byte* buf_ptrs[GS_CLIENT_COLOR_MAX_COMPONENTS];

    /* Nothing was ever drawn. */
    if (buf == NULL)
        return 0;

    bg = buf->group_color_info->isadditive ? 65535 : 0;
    src_profile = buf->group_color_info->icc_profile;

    num_comp = buf->n_chan - 1;
    rect = buf->rect;
    planestride = buf->planestride;
    rowstride = buf->rowstride;

    /* Make sure that this is the only item on the stack. Fuzzing revealed a
       potential problem. Bug 694190 */
    if (buf->saved != NULL) {
        return gs_throw(gs_error_unknownerror, "PDF14 device push/pop out of sync");
    }
    if_debug0m('v', dev->memory, "[v]pdf14_put_image\n");
    rect_intersect(rect, buf->dirty);
    x1 = min(pdev->width, rect.q.x);
    y1 = min(pdev->height, rect.q.y);
    width = x1 - rect.p.x;
    height = y1 - rect.p.y;
#ifdef DUMP_TO_PNG
    dump_planar_rgba(pdev->memory, buf);
#endif
    if (width <= 0 || height <= 0 || buf->data == NULL)
        return 0;
    buf_ptr = buf->data + (rect.p.y - buf->rect.p.y) * buf->rowstride + ((rect.p.x - buf->rect.p.x) << deep);

    /* Check that target is OK.  From fuzzing results the target could have been
       destroyed, for e.g if it were a pattern accumulator that was closed
       prematurely (Bug 694154).  We should always be able to to get an ICC
       profile from the target. */
    code = dev_proc(target, get_profile)(target,  &dev_target_profile);
    if (code < 0)
        return code;
    if (dev_target_profile == NULL)
        return gs_throw_code(gs_error_Fatal);

    if (src_profile == NULL) {
        code = dev_proc(dev, get_profile)(dev, &pdf14dev_profile);
        if (code < 0) {
            return code;
        }
        src_profile = pdf14dev_profile->device_profile[GS_DEFAULT_DEVICE_PROFILE];
    }

    /* Check if we have a color conversion issue */
    des_profile = dev_target_profile->device_profile[GS_DEFAULT_DEVICE_PROFILE];
    if (pdev->using_blend_cs || !gsicc_profiles_equal(des_profile, src_profile))
        color_mismatch = true;

    /* Check if target supports alpha */
    supports_alpha = dev_proc(target, dev_spec_op)(target, gxdso_supports_alpha, NULL, 0);
    code = 0;

#if RAW_DUMP
    dump_raw_buffer(pdev->ctx->memory, height, width, buf->n_planes,
        pdev->ctx->stack->planestride, pdev->ctx->stack->rowstride,
        "pre_final_blend", buf_ptr, deep);
#endif

    /* Note. The logic below will need a little rework if we ever
       have a device that has tags and alpha support */
    if (supports_alpha) {
        if (!color_mismatch) {
            alpha_offset = num_comp;
            tag_offset = buf->has_tags ? buf->n_chan : 0;

            for (i = 0; i < buf->n_planes; i++)
                buf_ptrs[i] = buf_ptr + i * planestride;
            code = dev_proc(target, put_image) (target, target, buf_ptrs, num_comp,
                rect.p.x, rect.p.y, width, height,
                rowstride, alpha_offset,
                tag_offset);
            /* Right now code has number of rows written */
        } else {
            /* In this case, just color convert and maintain alpha.  This is a case
               where we either either blend in the right color space and have no
               alpha for the output device or hand back the wrong color space with
               alpha data.  We choose the later. */
            code = pdf14_put_image_color_convert(pdev, pgs, src_profile,
                dev_target_profile, &buf, &buf_ptr, false, rect.p.x, rect.p.y,
                width, height);
            if (code < 0)
                return code;

            /* reset */
            rowstride = buf->rowstride;
            planestride = buf->planestride;
            num_comp = buf->n_chan - 1;
            alpha_offset = num_comp;
            tag_offset = buf->has_tags ? buf->n_chan : 0;

            /* And then out */
            for (i = 0; i < buf->n_planes; i++)
                buf_ptrs[i] = buf_ptr + i * planestride;
            code = dev_proc(target, put_image) (target, target, buf_ptrs, num_comp,
                rect.p.x, rect.p.y, width, height, rowstride, alpha_offset,
                tag_offset);
            /* Right now code has number of rows written */
        }
    } else if (has_tags) {
        /* We are going out to a device that supports tags */
        if (deep) {
            gx_blend_image_buffer16(buf_ptr, width, height, rowstride,
                buf->planestride, num_comp, bg, false);
        } else {
            gx_blend_image_buffer(buf_ptr, width, height, rowstride,
                buf->planestride, num_comp, bg >> 8);
        }

#if RAW_DUMP
        dump_raw_buffer(pdev->ctx->memory, height, width, buf->n_planes,
            pdev->ctx->stack->planestride, pdev->ctx->stack->rowstride,
            "post_final_blend", buf_ptr, deep);
#endif

        /* Take care of color issues */
        if (color_mismatch) {
            /* In this case, just color convert and maintain alpha.  This is a case
               where we either either blend in the right color space and have no
               alpha for the output device or hand back the wrong color space with
               alpha data.  We choose the later. */
            code = pdf14_put_image_color_convert(pdev, pgs, src_profile, dev_target_profile,
                &buf, &buf_ptr, true, rect.p.x, rect.p.y, width, height);
            if (code < 0)
                return code;

#if RAW_DUMP
            dump_raw_buffer(pdev->ctx->memory, height, width, buf->n_planes,
                pdev->ctx->stack->planestride, pdev->ctx->stack->rowstride,
                "final_color_manage", buf_ptr, deep);
            global_index++;
#endif
        }

        /* reset */
        rowstride = buf->rowstride;
        planestride = buf->planestride;
        num_comp = buf->n_chan - 1;
        alpha_offset = 0;  /* It is there but this indicates we have done the blend */
        tag_offset = buf->has_tags ? buf->n_chan : 0;

        /* And then out */
        for (i = 0; i < buf->n_planes; i++)
            buf_ptrs[i] = buf_ptr + i * planestride;
        code = dev_proc(target, put_image) (target, target, buf_ptrs, num_comp,
            rect.p.x, rect.p.y, width, height, rowstride, alpha_offset,
            tag_offset);
        /* Right now code has number of rows written */

    }

    /* If code > 0 then put image worked.  Let it finish and then exit */
    if (code > 0) {
        /* We processed some or all of the rows.  Continue until we are done */
        num_rows_left = height - code;
        while (num_rows_left > 0) {
            code = dev_proc(target, put_image) (target, target, buf_ptrs, num_comp,
                                                rect.p.x, rect.p.y + code, width,
                                                num_rows_left, rowstride,
                                                alpha_offset, tag_offset);
            num_rows_left = num_rows_left - code;
        }
        return 0;
    }

    /* Target device did not support alpha or tags.
     * Set color space in preparation for sending an image.
     * color conversion will occur after blending with through
     * the begin typed image work flow.
     */

    planestride = buf->planestride;
    rowstride = buf->rowstride;
    code = gs_cspace_build_ICC(&pcs, NULL, pgs->memory);
    if (code < 0)
        return code;
    /* Need to set this to avoid color management during the image color render
       operation.  Exception is for the special case when the destination was
       CIELAB.  Then we need to convert from default RGB to CIELAB in the put
       image operation.  That will happen here as we should have set the profile
       for the pdf14 device to RGB and the target will be CIELAB.  In addition,
       the case when we have a blend color space that is different than the
       target device color space */
    pcs->cmm_icc_profile_data = src_profile;

    /* pcs takes a reference to the profile data it just retrieved. */
    gsicc_adjust_profile_rc(pcs->cmm_icc_profile_data, 1, "pdf14_put_image");
    gsicc_set_icc_range(&(pcs->cmm_icc_profile_data));
    gs_image_t_init_adjust(&image, pcs, false);
    image.ImageMatrix.xx = (float)width;
    image.ImageMatrix.yy = (float)height;
    image.Width = width;
    image.Height = height;
    image.BitsPerComponent = deep ? 16 : 8;
    image.ColorSpace = pcs;
    ctm_only_writable(pgs).xx = (float)width;
    ctm_only_writable(pgs).xy = 0;
    ctm_only_writable(pgs).yx = 0;
    ctm_only_writable(pgs).yy = (float)height;
    ctm_only_writable(pgs).tx = (float)rect.p.x;
    ctm_only_writable(pgs).ty = (float)rect.p.y;
    code = dev_proc(target, begin_typed_image) (target,
                                                pgs, NULL,
                                                (gs_image_common_t *)&image,
                                                NULL, NULL, NULL,
                                                pgs->memory, &info);
    if (code < 0) {
        rc_decrement_only_cs(pcs, "pdf14_put_image");
        return code;
    }
#if RAW_DUMP
    /* Dump the current buffer to see what we have. */
    dump_raw_buffer(pdev->ctx->memory,
                    pdev->ctx->stack->rect.q.y-pdev->ctx->stack->rect.p.y,
                    pdev->ctx->stack->rect.q.x-pdev->ctx->stack->rect.p.x,
                    pdev->ctx->stack->n_planes,
                    pdev->ctx->stack->planestride, pdev->ctx->stack->rowstride,
                    "pdF14_putimage", pdev->ctx->stack->data, deep);
    dump_raw_buffer(pdev->ctx->memory,
                    height, width, buf->n_planes,
                    pdev->ctx->stack->planestride, pdev->ctx->stack->rowstride,
                    "PDF14_PUTIMAGE_SMALL", buf_ptr, deep);
    global_index++;
    clist_band_count++;
#endif
    /* Allocate on 32-byte border for AVX CMYK case. Four byte overflow for RGB case */
    /* 28 byte overflow for AVX CMYK case. */
#define SSE_ALIGN 32
#define SSE_OVERFLOW 28
    linebuf_unaligned = gs_alloc_bytes(pdev->memory, width * (num_comp<<deep) + SSE_ALIGN + SSE_OVERFLOW, "pdf14_put_image");
    if (linebuf_unaligned == NULL)
        return gs_error_VMerror;
    linebuf = linebuf_unaligned + ((-(intptr_t)linebuf_unaligned) & (SSE_ALIGN-1));

    blend_row = deep ? gx_build_blended_image_row16 :
                       gx_build_blended_image_row;
#ifdef WITH_CAL
    blend_row = cal_get_blend_row(pdev->memory->gs_lib_ctx->core->cal_ctx,
                                  blend_row, num_comp, deep);
#endif

    if (!deep)
        bg >>= 8;
    for (y = 0; y < height; y++) {
        gx_image_plane_t planes;
        int rows_used;

        blend_row(buf_ptr, buf->planestride, width, num_comp, bg, linebuf);
        planes.data = linebuf;
        planes.data_x = 0;
        planes.raster = width * num_comp;
        info->procs->plane_data(info, &planes, 1, &rows_used);
        /* todo: check return value */
        buf_ptr += buf->rowstride;
    }
    gs_free_object(pdev->memory, linebuf_unaligned, "pdf14_put_image");
    info->procs->end_image(info, true);
    /* This will also decrement the device profile */
    rc_decrement_only_cs(pcs, "pdf14_put_image");
    return code;
}

/* Overprint simulation with spots.  Collapse to CMYK */
static void
template_spots_to_cmyk(byte *buf_ptr, int width, int height, int rowstride,
    int planestride, int num_comp, int spot_start, int tag_offset,
    cmyk_composite_map *map, bool keep_alpha)
{
    int comp_num;
    uint cyan, magenta, yellow, black;
    cmyk_composite_map *cmyk_map_entry;
    int x, y;
    int position;
    byte comp, a;

    for (y = 0; y < height; y++) {
        position = y * rowstride;
        for (x = 0; x < width; x++) {
            a = buf_ptr[position + planestride * num_comp];
            if (a != 0) {
                cyan = buf_ptr[position] * frac_1;
                magenta = buf_ptr[position + planestride] * frac_1;
                yellow = buf_ptr[position + planestride * 2] * frac_1;
                black = buf_ptr[position + planestride * 3] * frac_1;
                cmyk_map_entry = &(map[4]);
                for (comp_num = spot_start; comp_num < num_comp; comp_num++) {
                    comp = buf_ptr[position + planestride * comp_num];
                    cyan += cmyk_map_entry->c * comp;
                    magenta += cmyk_map_entry->m * comp;
                    yellow += cmyk_map_entry->y * comp;
                    black += cmyk_map_entry->k * comp;
                    cmyk_map_entry++;
                }
                cyan /= frac_1;
                magenta /= frac_1;
                yellow /= frac_1;
                black /= frac_1;

                if (cyan > 255)
                    cyan = 255;
                if (magenta > 255)
                    magenta = 255;
                if (yellow > 255)
                    yellow = 255;
                if (black > 255)
                    black = 255;

                buf_ptr[position] = cyan;
                buf_ptr[position + planestride] = magenta;
                buf_ptr[position + planestride * 2] = yellow;
                buf_ptr[position + planestride * 3] = black;
            }
            if (keep_alpha) {
                /* Move the alpha and tag data */
                buf_ptr[position + planestride * 4] = a;
                if (tag_offset > 0) {
                    buf_ptr[position + planestride * 5] =
                        buf_ptr[position + planestride * tag_offset];
                }
            } else {
                /* Remove alpha but keep tags */
                if (tag_offset > 0) {
                    buf_ptr[position + planestride * 4] =
                        buf_ptr[position + planestride * tag_offset];
                }

            }
            position += 1;
        }
    }
}

static void
template_spots_to_cmyk_16(byte *buf_ptr_, int width, int height, int rowstride,
    int planestride, int num_comp, int spot_start, int tag_offset,
    cmyk_composite_map *map, bool keep_alpha)
{
    int comp_num;
    ulong cyan, magenta, yellow, black;
    cmyk_composite_map *cmyk_map_entry;
    int x, y;
    int position;
    ulong comp, a;
    uint16_t *buf_ptr = (uint16_t *)(void *)buf_ptr_;

    /* planestride and rowstride are in bytes, and we want them in shorts */
    planestride >>= 1;
    rowstride >>= 1;

    for (y = 0; y < height; y++) {
        position = y * rowstride;
        for (x = 0; x < width; x++) {
            a = buf_ptr[position + planestride * num_comp];
            if (a != 0) {
                cyan = (ulong)buf_ptr[position] * frac_1_long;
                magenta = (ulong)buf_ptr[position + planestride] * frac_1_long;
                yellow = (ulong)buf_ptr[position + planestride * 2] * frac_1_long;
                black = (ulong)buf_ptr[position + planestride * 3] * frac_1_long;
                cmyk_map_entry = &(map[4]);
                for (comp_num = spot_start; comp_num < num_comp; comp_num++) {
                    comp = buf_ptr[position + planestride * comp_num];
                    cyan += (ulong)cmyk_map_entry->c * comp;
                    magenta += (ulong)cmyk_map_entry->m * comp;
                    yellow += (ulong)cmyk_map_entry->y * comp;
                    black += (ulong)cmyk_map_entry->k * comp;
                    cmyk_map_entry++;
                }
                cyan /= frac_1_long;
                magenta /= frac_1_long;
                yellow /= frac_1_long;
                black /= frac_1_long;

                if (cyan > 65535)
                    cyan = 65535;
                if (magenta > 65535)
                    magenta = 65535;
                if (yellow > 65535)
                    yellow = 65535;
                if (black > 65535)
                    black = 65535;

#if ARCH_IS_BIG_ENDIAN
                buf_ptr[position] = cyan;
                buf_ptr[position + planestride] = magenta;
                buf_ptr[position + planestride * 2] = yellow;
                buf_ptr[position + planestride * 3] = black;
#else
                ((byte *)&buf_ptr[position])[0] = cyan >> 8;
                ((byte *)&buf_ptr[position])[1] = cyan;
                ((byte *)&buf_ptr[position + planestride])[0] = magenta >> 8;
                ((byte *)&buf_ptr[position + planestride])[1] = magenta;
                ((byte *)&buf_ptr[position + planestride * 2])[0] = yellow >> 8;
                ((byte *)&buf_ptr[position + planestride * 2])[1] = yellow;
                ((byte *)&buf_ptr[position + planestride * 3])[0] = black >> 8;
                ((byte *)&buf_ptr[position + planestride * 3])[1] = black;
#endif
            }
            /* Move the alpha and tag data */
#if ARCH_IS_BIG_ENDIAN
            if (keep_alpha) {
                buf_ptr[position + planestride * 4] = a;
                if (tag_offset > 0) {
                    buf_ptr[position + planestride * 5] =
                        buf_ptr[position + planestride * tag_offset];
                }
            } else {
                if (tag_offset > 0) {
                    buf_ptr[position + planestride * 4] =
                        buf_ptr[position + planestride * tag_offset];
                }
            }
#else
            if (keep_alpha) {
                ((byte *)&buf_ptr[position + planestride * 4])[0] = a >> 8;
                ((byte *)&buf_ptr[position + planestride * 4])[1] = a;
                if (tag_offset > 0) {
                    ((byte *)&buf_ptr[position + planestride * 5])[0] =
                        buf_ptr[position + planestride * tag_offset] >> 8;
                    ((byte *)&buf_ptr[position + planestride * 5])[1] =
                        buf_ptr[position + planestride * tag_offset];
                }
            } else {
                if (tag_offset > 0) {
                    ((byte *)&buf_ptr[position + planestride * 4])[0] =
                        buf_ptr[position + planestride * tag_offset] >> 8;
                    ((byte *)&buf_ptr[position + planestride * 4])[1] =
                        buf_ptr[position + planestride * tag_offset];
                }
            }
#endif
            position += 1;
        }
    }
}

static void
pdf14_spots_to_cmyk(byte *buf_ptr, int width, int height, int rowstride,
    int planestride, int num_comp, int spot_start, int tag_offset,
    cmyk_composite_map *map, bool keep_alpha, bool deep)
{
    if (deep) {
        if (keep_alpha) {
            if (tag_offset > 0) {
                template_spots_to_cmyk_16(buf_ptr, width, height, rowstride,
                    planestride, num_comp, spot_start, tag_offset,
                    map, true);
            } else {
                template_spots_to_cmyk_16(buf_ptr, width, height, rowstride,
                    planestride, num_comp, spot_start, 0,
                    map, true);
            }
        } else {
            if (tag_offset > 0) {
                template_spots_to_cmyk_16(buf_ptr, width, height, rowstride,
                    planestride, num_comp, spot_start, tag_offset,
                    map, false);
            } else {
                template_spots_to_cmyk_16(buf_ptr, width, height, rowstride,
                    planestride, num_comp, spot_start, 0,
                    map, false);
            }
        }
    } else {
        if (keep_alpha) {
            if (tag_offset > 0) {
                template_spots_to_cmyk(buf_ptr, width, height, rowstride,
                    planestride, num_comp, spot_start, tag_offset,
                    map, true);
            } else {
                template_spots_to_cmyk(buf_ptr, width, height, rowstride,
                    planestride, num_comp, spot_start, 0,
                    map, true);
            }
        } else {
            if (tag_offset > 0) {
                template_spots_to_cmyk(buf_ptr, width, height, rowstride,
                    planestride, num_comp, spot_start, tag_offset,
                    map, false);
            } else {
                template_spots_to_cmyk(buf_ptr, width, height, rowstride,
                    planestride, num_comp, spot_start, 0,
                    map, false);
            }
        }
    }
}

/* This is for the case where we have mixture of spots and additive color.
   For example, RGB + spots or Gray + spots */
static void
pdf14_blend_image_mixed_buffer(byte* buf_ptr, int width, int height, int rowstride,
    int planestride, int num_comp, int spot_start)
{
    int x, y;
    int position;
    byte comp, a;
    int tmp, comp_num;

    for (y = 0; y < height; y++) {
        position = y * rowstride;
        for (x = 0; x < width; x++) {
            a = buf_ptr[position + planestride * num_comp];
            if ((a + 1) & 0xfe) {
                a ^= 0xff;
                for (comp_num = 0; comp_num < spot_start; comp_num++) {
                    comp = buf_ptr[position + planestride * comp_num];
                    tmp = ((0xff - comp) * a) + 0x80;
                    comp += (tmp + (tmp >> 8)) >> 8;
                    buf_ptr[position + planestride * comp_num] = comp;
                }
                for (comp_num = spot_start; comp_num < num_comp; comp_num++) {
                    comp = buf_ptr[position + planestride * comp_num];
                    tmp = ((-comp) * a) + 0x80;
                    comp += (tmp + (tmp >> 8)) >> 8;
                    buf_ptr[position + planestride * comp_num] = comp;
                }
            } else if (a == 0) {
                for (comp_num = 0; comp_num < spot_start; comp_num++) {
                    buf_ptr[position + planestride * comp_num] = 0xff;
                }
                for (comp_num = spot_start; comp_num < num_comp; comp_num++) {
                    buf_ptr[position + planestride * comp_num] = 0;
                }
            }
            position += 1;
        }
    }
}

static void
pdf14_blend_image_mixed_buffer16(byte* buf_ptr_, int width, int height, int rowstride,
    int planestride, int num_comp, int spot_start)
{
    uint16_t* buf_ptr = (uint16_t*)(void*)buf_ptr_;
    int x, y;
    int position;
    int comp, a;
    int tmp, comp_num;

    /* planestride and rowstride are in bytes, and we want them in shorts */
    planestride >>= 1;
    rowstride >>= 1;

    /* Note that the input here is native endian, and the output must be in big endian! */
    for (y = 0; y < height; y++) {
        position = y * rowstride;
        for (x = 0; x < width; x++) {
            /* composite RGBA (or CMYKA, etc.) pixel with over solid background */
            a = buf_ptr[position + planestride * num_comp];
            if (a == 0) {
                for (comp_num = 0; comp_num < spot_start; comp_num++) {
                    buf_ptr[position + planestride * comp_num] = 0xffff;
                }
                for (comp_num = spot_start; comp_num < num_comp; comp_num++) {
                    buf_ptr[position + planestride * comp_num] = 0;
                }
            } else if (a == 0xffff) {
#if ARCH_IS_BIG_ENDIAN
#else
                /* Convert from native -> big endian */
                for (comp_num = 0; comp_num < num_comp; comp_num++) {
                    comp = buf_ptr[position + planestride * comp_num];
                    ((byte*)&buf_ptr[position + planestride * comp_num])[0] = comp >> 8;
                    ((byte*)&buf_ptr[position + planestride * comp_num])[1] = comp;
                }
#endif
            } else {
                a ^= 0xffff;
                a += a >> 15; /* a is now 0 to 0x10000 */
                a >>= 1; /* We can only use 15 bits as bg-comp has a sign bit we can't lose */
                for (comp_num = 0; comp_num < spot_start; comp_num++) {
                    comp = buf_ptr[position + planestride * comp_num];
                    tmp = ((0xffff - comp) * a) + 0x4000;
                    comp += (tmp >> 15); /* Errors in bit 16 upwards will be ignored */
                    /* Store as big endian */
                    ((byte*)&buf_ptr[position + planestride * comp_num])[0] = comp >> 8;
                    ((byte*)&buf_ptr[position + planestride * comp_num])[1] = comp;
                }
                for (comp_num = spot_start; comp_num < num_comp; comp_num++) {
                    comp = buf_ptr[position + planestride * comp_num];
                    tmp = ((0 - comp) * a) + 0x4000;
                    comp += (tmp >> 15); /* Errors in bit 16 upwards will be ignored */
                    /* Store as big endian */
                    ((byte*)&buf_ptr[position + planestride * comp_num])[0] = comp >> 8;
                    ((byte*)&buf_ptr[position + planestride * comp_num])[1] = comp;
                }
            }
            position += 1;
        }
    }
}

static pdf14_buf*
insert_empty_planes(pdf14_ctx* ctx, pdf14_buf** src_buf, int num_new_planes, int insert_index)
{
    int planestride = (*src_buf)->planestride;
    int src_n_planes = (*src_buf)->n_planes;
    int src_n_chan = (*src_buf)->n_chan;
    int des_n_planes = src_n_planes + num_new_planes;
    int des_n_chan = src_n_chan + num_new_planes;
    byte *src_ptr = (*src_buf)->data;
    byte* des_ptr;
    byte *des_data;
    bool deep = ctx->deep;

    des_data = gs_alloc_bytes(ctx->memory,
        (size_t)planestride * des_n_planes + CAL_SLOP,
        "insert_empty_planes");
    if (des_data == NULL)
        return NULL;

    des_ptr = des_data;

    /* First copy portion prior to insert point */
    memcpy(des_ptr, src_ptr, (planestride * insert_index) << deep);

    /* New planes */
    des_ptr += (planestride * insert_index) << deep;
    src_ptr += (planestride * insert_index) << deep;
    memset(des_ptr, 0, (planestride * num_new_planes) << deep);

    /* Extra planes (i.e. doc spots, tags) */
    des_ptr += (planestride * num_new_planes) << deep;
    memcpy(des_ptr, src_ptr, (planestride * (src_n_planes - insert_index)) << deep);

    /* Set up buffer structure */
    gs_free_object(ctx->memory, (*src_buf)->data, "insert_empty_planes");
    (*src_buf)->n_planes = des_n_planes;
    (*src_buf)->n_chan = des_n_chan;
    (*src_buf)->data = des_data;

    return *src_buf;
}

static int
pdf14_put_blended_image_cmykspot(gx_device* dev, gx_device* target,
    gs_gstate* pgs, pdf14_buf* buf, int planestride_in,
    int rowstride_in, int x0, int y0, int width, int height,
    int num_comp, uint16_t bg, bool has_tags, gs_int_rect rect_in,
    gs_separations* pseparations, bool deep)
{
    pdf14_device* pdev = (pdf14_device*)dev;
    int code = 0;
    int y;
    int num_rows_left;
    int i;
    gs_int_rect rect = rect_in;
    int planestride = planestride_in;
    int rowstride = rowstride_in;
    byte* buf_ptr = NULL;
    cmm_profile_t* src_profile = buf->group_color_info->icc_profile;
    cmm_profile_t* des_profile = NULL;
    cmm_dev_profile_t* dev_target_profile;
    cmm_dev_profile_t* pdf14dev_profile;
    bool color_mismatch = false;
    bool supports_alpha = false;
    const byte* buf_ptrs[GS_CLIENT_COLOR_MAX_COMPONENTS];
    int alpha_offset = num_comp;
    int tag_offset = has_tags ? num_comp + 1 : 0;
    bool keep_native = pdev->overprint_sim && pdev->devn_params.page_spot_colors > 0;
    gs_color_space *pcs;
    gs_image1_t image;
    gx_image_enum_common_t *info;
    gx_image_plane_t planes[GS_IMAGE_MAX_COMPONENTS];
    pdf14_buf *cm_result = NULL;
    bool did_alloc;

    /* Check if group color space is CMYK based */
    code = dev_proc(target, get_profile)(target, &dev_target_profile);
    if (code < 0)
        return code;
    if (dev_target_profile == NULL)
        return gs_throw_code(gs_error_Fatal);

    if (src_profile == NULL) {
        code = dev_proc(dev, get_profile)(dev, &pdf14dev_profile);
        if (code < 0) {
            return code;
        }
        src_profile = pdf14dev_profile->device_profile[GS_DEFAULT_DEVICE_PROFILE];
    }

    /* If we have spot colors and are doing overprint simulation and the source
       space is not CMYK due to a blending color space being used, then convert
       base colors to CMYK so that we can properly blend the spot colors */
    if (keep_native && src_profile->data_cs != gsCMYK) {

        cm_result = pdf14_transform_color_buffer_no_matte(pgs, pdev->ctx, (gx_device *)dev, buf,
            buf->data, src_profile, pgs->icc_manager->default_cmyk, 0, 0, buf->rect.q.x,
            buf->rect.q.y, &did_alloc, buf->deep, false);
        if (cm_result == NULL)
            return_error(gs_error_VMerror);

        /* Update */
        buf = cm_result;
        src_profile = pgs->icc_manager->default_cmyk;
        num_comp = buf->n_chan - 1;
        bg = 0;
        tag_offset = has_tags ? num_comp + 1 : 0;
        alpha_offset = num_comp;

#if RAW_DUMP
        buf_ptr = buf->data + (rect.p.y - buf->rect.p.y) * buf->rowstride + ((rect.p.x - buf->rect.p.x) << deep);
        dump_raw_buffer(target->memory, height, width, buf->n_planes, planestride, rowstride,
            "post_to_cmyk_for_spot_blend", buf_ptr, deep);
        global_index++;
#endif
    }

    /* Fix order map if needed */
    for (i = 0; i < num_comp; i++) {
        pdev->devn_params.separation_order_map[i] = i;
    }

    /* Check if we have a color conversion issue */
    des_profile = dev_target_profile->device_profile[GS_DEFAULT_DEVICE_PROFILE];
    if (pdev->using_blend_cs || !gsicc_profiles_equal(des_profile, src_profile))
        color_mismatch = true;

    /* Check if target supports alpha */
    supports_alpha = dev_proc(target, dev_spec_op)(target, gxdso_supports_alpha, NULL, 0);
    code = 0;

    buf_ptr = buf->data + (rect.p.y - buf->rect.p.y) * buf->rowstride + ((rect.p.x - buf->rect.p.x) << deep);

    /* Note. The logic below will need a little rework if we ever
       have a device that has tags and alpha support */
    if (supports_alpha) {

        /* If doing simulated overprint, Bring the spot color channels into
           CMYK. Data is planar and 16 bit data in native format. */
        if (pdev->overprint_sim && pdev->devn_params.page_spot_colors > 0) {
            cmyk_composite_map cmyk_map[GX_DEVICE_MAX_SEPARATIONS];  /* Fracs */

            /* In the clist case, we need to get equiv spots out of the pseudo-band. */
            if (pdev->pclist_device != NULL) {
                gx_device_clist_reader *pcrdev = (gx_device_clist_reader *)(pdev->pclist_device);

                code = clist_read_op_equiv_cmyk_colors(pcrdev, &(pdev->op_pequiv_cmyk_colors));
                if (code < 0)
                    return code;
            }
            build_cmyk_map(dev, num_comp, &(pdev->op_pequiv_cmyk_colors), cmyk_map);

            /* Now we go to big endian */
            pdf14_spots_to_cmyk(buf_ptr, width, height, rowstride,
                planestride, num_comp, src_profile->num_comps,
                tag_offset, cmyk_map, true, deep);

            /* Reset buffer information. We have CMYK+alpha and maybe tags */
            buf->n_chan = buf->n_chan - buf->num_spots;
            buf->n_planes = buf->n_planes - buf->num_spots;
            buf->num_spots = 0;
            num_comp = buf->n_chan - 1;
            tag_offset = has_tags ? buf->n_planes - 1 : 0; /* Tags at end */
        }

        if (!color_mismatch) {
            for (i = 0; i < buf->n_planes; i++)
                buf_ptrs[i] = buf_ptr + i * planestride;
            code = dev_proc(target, put_image) (target, target, buf_ptrs, num_comp,
                rect.p.x, rect.p.y, width, height,
                rowstride, alpha_offset, tag_offset);
            /* Right now code has number of rows written */
        } else {
            /* In this case, just color convert and maintain alpha.
               This is a case where we either either blend in the
               right color space and have no alpha for the output
               device or hand back the wrong color space with
               alpha data.  We choose the later. */
            code = pdf14_put_image_color_convert(pdev, pgs, src_profile,
                        dev_target_profile, &buf, &buf_ptr, false, rect.p.x,
                        rect.p.y, width, height);
            if (code < 0)
                return code;

            /* reset */
            rowstride = buf->rowstride;
            planestride = buf->planestride;
            num_comp = buf->n_chan - 1;
            alpha_offset = num_comp;
            tag_offset = buf->has_tags ? buf->n_chan : 0;

            /* And then out */
            for (i = 0; i < buf->n_planes; i++)
                buf_ptrs[i] = buf_ptr + i * planestride;
            code = dev_proc(target, put_image) (target, target, buf_ptrs, num_comp,
                rect.p.x, rect.p.y, width, height, rowstride, alpha_offset,
                tag_offset);
            /* Right now code has number of rows written.  Writing continues below */
        }
    } else {
        /* Device could not handle the alpha data (we actually don't have
           a device that does spot colorants and has an alpha channel so
           the above code is untested.  Go ahead and preblend now and then
           color convert if needed */
#if RAW_DUMP
           /* Dump before and after the blend to make sure we are doing that ok */
        dump_raw_buffer(target->memory, height, width, buf->n_planes, planestride, rowstride,
            "pre_put_image_blend_image", buf_ptr, deep);
        global_index++;
#endif

        /* Delay the baking to big endian if we have to do spots to CMYK still.  We will take
           care of the conversion at that point */
        if (color_mismatch && (src_profile->data_cs == gsRGB || src_profile->data_cs == gsGRAY)) {
            if (deep) {
                pdf14_blend_image_mixed_buffer16(buf_ptr, width, height, rowstride,
                    planestride, num_comp, src_profile->num_comps);
            } else {
                pdf14_blend_image_mixed_buffer(buf_ptr, width, height, rowstride,
                    planestride, num_comp, src_profile->num_comps);
            }
        } else {
            if (deep) {
                gx_blend_image_buffer16(buf_ptr, width, height, rowstride,
                    planestride, num_comp, bg, keep_native);
            } else {
                gx_blend_image_buffer(buf_ptr, width, height, rowstride,
                    planestride, num_comp, bg >> 8);
            }
        }

#if RAW_DUMP
        dump_raw_buffer_be(target->memory, height, width, buf->n_planes, planestride, rowstride,
            "post_put_image_blend_image", buf_ptr, deep);
        global_index++;
#endif

        /* If doing simulated overprint, Bring the spot color channels into
           CMYK. Data is planar and 16 bit data is still in native format. */
        if (pdev->overprint_sim && pdev->devn_params.page_spot_colors > 0) {
            cmyk_composite_map cmyk_map[GX_DEVICE_MAX_SEPARATIONS];  /* Fracs */

            /* In the clist case, we need to get equiv spots out of the
               pseudo-band. */
            if (pdev->pclist_device != NULL) {
                gx_device_clist_reader *pcrdev = (gx_device_clist_reader *)(pdev->pclist_device);
                code = clist_read_op_equiv_cmyk_colors(pcrdev, &(pdev->op_pequiv_cmyk_colors));
                if (code < 0)
                    return code;
            }

            build_cmyk_map(dev, num_comp, &(pdev->op_pequiv_cmyk_colors), cmyk_map);
            pdf14_spots_to_cmyk(buf_ptr, width, height, rowstride,
                planestride, num_comp, src_profile->num_comps,
                tag_offset, cmyk_map, false, deep);

            /* Reset buffer information. We have CMYK and maybe tags */
            num_comp = 4;
            alpha_offset = 0;
            buf->n_chan = buf->n_chan - buf->num_spots - 1;     /* No spots or alpha */
            buf->n_planes = buf->n_planes - buf->num_spots - 1; /* No spots or alpha */
            tag_offset = has_tags ? buf->n_chan : 0;      /* Tags at end */
            buf->num_spots = 0;

#if RAW_DUMP
            dump_raw_buffer(target->memory, height, width, buf->n_planes, planestride, rowstride,
                "post_put_image_spot_to_cmyk", buf_ptr, deep);
            global_index++;
#endif
        }

        /* Map to the destination color space */
        if (color_mismatch) {
            code = pdf14_put_image_color_convert(pdev, pgs, src_profile, dev_target_profile,
                &buf, &buf_ptr, true, rect.p.x, rect.p.y, width, height);
            if (code < 0)
                return code;

            /* reset */
            rowstride = buf->rowstride;
            planestride = buf->planestride;
            num_comp = buf->n_chan;
            tag_offset = buf->has_tags ? buf->n_chan : 0;
        }

        /* We may need to pad the buffers to ensure that any additional spot
           channels that are not created by the ICC color conversion (or
           non-conversion if this is not an NCLR profile) get placed properly.
           It is up to the target device to
           handle these planes how it sees fit based upon the image data
           and/or any tags plane during any put image call.  We *could*
           do something here to possibly communicate through the put_image
           call where the page related spots start, but that would/could
           be confusing, especially for long term maintenance. Easier just
           to have put_image hand all the data */
        if (dev_target_profile->spotnames != NULL &&
            dev_target_profile->spotnames->count > des_profile->num_comps) {
            int num_new_planes = dev_target_profile->spotnames->count - des_profile->num_comps;
            int insert_index = des_profile->num_comps;
            pdf14_buf* result;

            result = insert_empty_planes(pdev->ctx, &buf, num_new_planes, insert_index);
            if (result == NULL)
                return_error(gs_error_VMerror);

            num_comp = buf->n_chan;
            tag_offset = buf->has_tags ? buf->n_chan : 0;
            buf_ptr = buf->data + (rect.p.y - buf->rect.p.y) * buf->rowstride + ((rect.p.x - buf->rect.p.x) << deep);
        }
#if RAW_DUMP
        /* Dump after the CS transform */
        dump_raw_buffer_be(target->memory, height, width, buf->n_planes, planestride, rowstride,
            "post_put_image_color_convert", buf_ptr, deep);
        global_index++;
        /* clist_band_count++; */
#endif

        /* Try put_image again. This can occur if the
           target, like psdcmyk and tiffsep, support put_image */
        alpha_offset = 0;
        for (i = 0; i < buf->n_planes; i++)
            buf_ptrs[i] = buf_ptr + i * planestride;
        code = dev_proc(target, put_image) (target, target, buf_ptrs, num_comp,
            rect.p.x, rect.p.y, width, height,
            rowstride, alpha_offset, tag_offset);
    }

    /* Put image was succesful.  We processed some or all of the rows.
       Continue until we are done */
    if (code > 0) {
        num_rows_left = height - code;
        while (num_rows_left > 0) {
            code = dev_proc(target, put_image) (target, target, buf_ptrs, num_comp,
                rect.p.x, rect.p.y + code, width, num_rows_left, rowstride,
                alpha_offset, tag_offset);
            if (code < 0) {
                return code;
            }
            num_rows_left = num_rows_left - code;
        }
        return 0;
    }

    /* Sep devices all support put_image (tiffsep and psdcmyk)
       as well as those devices that support alpha (pngalpha)
       If we are here, then we are doing an overprint simulation
       on some other device. Image data is aleady blended and in
       device color space. */
    code = gs_cspace_build_ICC(&pcs, NULL, pgs->memory);
    if (code < 0)
        return code;

    /* Already in destination CS */
    pcs->cmm_icc_profile_data = des_profile;

    /* pcs takes a reference to the profile data it just retrieved. */
    gsicc_adjust_profile_rc(pcs->cmm_icc_profile_data, 1, "pdf14_put_blended_image_cmykspot");
    gsicc_set_icc_range(&(pcs->cmm_icc_profile_data));
    gs_image_t_init_adjust(&image, pcs, false);
    image.ImageMatrix.xx = (float)width;
    image.ImageMatrix.yy = (float)height;
    image.Width = width;
    image.Height = height;
    image.BitsPerComponent = deep ? 16 : 8;
    image.ColorSpace = pcs;
    image.format = gs_image_format_component_planar;

    ctm_only_writable(pgs).xx = (float)width;
    ctm_only_writable(pgs).xy = 0;
    ctm_only_writable(pgs).yx = 0;
    ctm_only_writable(pgs).yy = (float)height;
    ctm_only_writable(pgs).tx = (float)rect.p.x;
    ctm_only_writable(pgs).ty = (float)rect.p.y;
    code = dev_proc(target, begin_typed_image) (target,
        pgs, NULL, (gs_image_common_t *)&image,
        NULL, NULL, NULL, pgs->memory, &info);
    if (code < 0) {
        rc_decrement_only_cs(pcs, "pdf14_put_blended_image_cmykspot");
        return code;
    }
#if RAW_DUMP
    /* Dump the current buffer to see what we have. */
    dump_raw_buffer(pdev->ctx->memory,
        pdev->ctx->stack->rect.q.y - pdev->ctx->stack->rect.p.y,
        pdev->ctx->stack->rect.q.x - pdev->ctx->stack->rect.p.x,
        pdev->ctx->stack->n_planes,
        pdev->ctx->stack->planestride, pdev->ctx->stack->rowstride,
        "put_image_final_big", pdev->ctx->stack->data, deep);
    dump_raw_buffer(pdev->ctx->memory,
        height, width, buf->n_planes,
        pdev->ctx->stack->planestride, pdev->ctx->stack->rowstride,
        "put_image_final_small", buf_ptr, deep);
    global_index++;
    clist_band_count++;
#endif

    for (i = 0; i < num_comp; i++) {
        planes[i].data = buf_ptr + i * planestride;
        planes[i].data_x = 0;
        planes[i].raster = buf->rowstride;
    }

    for (y = 0; y < height; y++) {
        int rows_used;

        info->procs->plane_data(info, (const gx_image_plane_t*) &planes, 1, &rows_used);

        for (i = 0; i < num_comp; i++) {
            planes[i].data += buf->rowstride;
        }
    }
    info->procs->end_image(info, true);

    /* This will also decrement the profile */
    rc_decrement_only_cs(pcs, "pdf14_put_blended_image_cmykspot");
    return code;
}

/**
 * pdf14_cmykspot_put_image: Put rendered image to target device.
 * @pdev: The PDF 1.4 rendering device.
 * @pgs: State for image draw operation.
 * @target: The target device.
 *
 * Puts the rendered image in @pdev's buffer to @target. This is called
 * as part of the sequence of popping the PDF 1.4 device filter.
 *
 * Return code: negative on error.
 **/
static	int
pdf14_cmykspot_put_image(gx_device *dev, gs_gstate *pgs, gx_device *target)
{
    pdf14_device *pdev = (pdf14_device *)dev;
    pdf14_buf *buf = pdev->ctx->stack;
    gs_int_rect rect;
    int x1, y1, width, height;
    gs_devn_params *pdevn_params = &pdev->devn_params;
    gs_separations *pseparations = &pdevn_params->separations;
    int planestride;
    int rowstride;
    bool deep = pdev->ctx->deep;
    uint16_t bg;
    int num_comp;

    /* Nothing was ever drawn. */
    if (buf == NULL)
        return 0;

    bg = buf->group_color_info->isadditive ? 65535 : 0;
    num_comp = buf->n_chan - 1;
    rect = buf->rect;
    planestride = buf->planestride;
    rowstride = buf->rowstride;

    /* Make sure that this is the only item on the stack. Fuzzing revealed a
       potential problem. Bug 694190 */
    if (buf->saved != NULL) {
        return gs_throw(gs_error_unknownerror, "PDF14 device push/pop out of sync");
    }
    if_debug0m('v', dev->memory, "[v]pdf14_cmykspot_put_image\n");
    rect_intersect(rect, buf->dirty);
    x1 = min(pdev->width, rect.q.x);
    y1 = min(pdev->height, rect.q.y);
    width = x1 - rect.p.x;
    height = y1 - rect.p.y;
    if (width <= 0 || height <= 0 || buf->data == NULL)
        return 0;

#if RAW_DUMP
    /* Dump the current buffer to see what we have. */
    dump_raw_buffer(pdev->ctx->memory,
                    pdev->ctx->stack->rect.q.y-pdev->ctx->stack->rect.p.y,
                    pdev->ctx->stack->rect.q.x-pdev->ctx->stack->rect.p.x,
                    pdev->ctx->stack->n_planes,
                    pdev->ctx->stack->planestride, pdev->ctx->stack->rowstride,
                    "CMYK_SPOT_PUTIMAGE", pdev->ctx->stack->data,
                    pdev->ctx->stack->deep);

    global_index++;
    clist_band_count++;
#endif

    return pdf14_put_blended_image_cmykspot(dev, target, pgs,
                      buf, planestride, rowstride,
                      rect.p.x, rect.p.y, width, height, num_comp, bg,
                      buf->has_tags, rect, pseparations, deep);
}

/**
 * pdf14_custom_put_image: Put rendered image to target device.
 * @pdev: The PDF 1.4 rendering device.
 * @pgs: State for image draw operation.
 * @target: The target device.
 *
 * Puts the rendered image in @pdev's buffer to @target. This is called
 * as part of the sequence of popping the PDF 1.4 device filter.
 *
 * Return code: negative on error.
 **/
static	int
pdf14_custom_put_image(gx_device * dev, gs_gstate * pgs, gx_device * target)
{
    pdf14_device * pdev = (pdf14_device *)dev;
    pdf14_buf *buf = pdev->ctx->stack;
    bool deep = pdev->ctx->deep;
    gs_int_rect rect;
    int x0, y0;
    int planestride;
    int rowstride;
    int num_comp;
    uint16_t bg;
    int x1, y1, width, height;
    byte *buf_ptr;

    /* Nothing was ever drawn. */
    if (buf == NULL)
        return 0;

    bg = pdev->ctx->additive ? 0xffff : 0;
    num_comp = buf->n_chan - 1;
    rect = buf->rect;
    x0 = rect.p.x;
    y0 = rect.p.y;
    planestride = buf->planestride;
    rowstride = buf->rowstride;

    /* Make sure that this is the only item on the stack. Fuzzing revealed a
       potential problem. Bug 694190 */
    if (buf->saved != NULL) {
        return gs_throw(gs_error_unknownerror, "PDF14 device push/pop out of sync");
    }
    if_debug0m('v', dev->memory, "[v]pdf14_custom_put_image\n");
    rect_intersect(rect, buf->dirty);
    x1 = min(pdev->width, rect.q.x);
    y1 = min(pdev->height, rect.q.y);
    width = x1 - rect.p.x;
    height = y1 - rect.p.y;
    if (width <= 0 || height <= 0 || buf->data == NULL)
        return 0;
    buf_ptr = buf->data + (rect.p.y - buf->rect.p.y) * buf->rowstride + ((rect.p.x - buf->rect.p.x)<<deep);

    return gx_put_blended_image_custom(target, buf_ptr,
                      planestride, rowstride,
                      x0, y0, width, height, num_comp, bg, deep);
}

/* This is rather nasty: in the event we are interrupted (by an error) between a push and pop
 * of one or more groups, we have to cycle through any ICC profile changes since the push
 * putting everything back how it was, and cleaning up the reference counts.
 */
static void pdf14_cleanup_group_color_profiles (pdf14_device *pdev)
{
    if (pdev->ctx && pdev->ctx->stack) {
        pdf14_buf *buf, *next;

        for (buf = pdev->ctx->stack->saved; buf != NULL; buf = next) {
            pdf14_group_color_t *group_color_info = buf->group_color_info;
            next = buf->saved;
            while (group_color_info) {
               if (group_color_info->icc_profile != NULL) {
                   cmm_profile_t *group_profile;
                   gsicc_rendering_param_t render_cond;
                   cmm_dev_profile_t *dev_profile;
                   int code = dev_proc((gx_device *)pdev, get_profile)((gx_device *)pdev,  &dev_profile);

                   if (code >= 0) {
                       gsicc_extract_profile(GS_UNKNOWN_TAG, dev_profile, &group_profile,
                                             &render_cond);

                       gsicc_adjust_profile_rc(pdev->icc_struct->device_profile[GS_DEFAULT_DEVICE_PROFILE],
                                               -1, "pdf14_end_transparency_group");
                       pdev->icc_struct->device_profile[GS_DEFAULT_DEVICE_PROFILE] =
                           group_color_info->icc_profile;
                       group_color_info->icc_profile = NULL;
                   }
               }

               group_color_info = group_color_info->previous;
            }
        }
    }
}

static	int
pdf14_close(gx_device *dev)
{
    pdf14_device *pdev = (pdf14_device *)dev;

    pdf14_cleanup_group_color_profiles(pdev);

    if (pdev->ctx) {
        pdf14_ctx_free(pdev->ctx);
        pdev->ctx = NULL;
    }
    return 0;
}

/* This is called when something has gone wrong and the interpreter received a
   stop while in the middle of doing something with the PDF14 device.  We need
   to clean up and end this in a graceful manner */
static int
pdf14_discard_trans_layer(gx_device *dev, gs_gstate * pgs)
{
    pdf14_device *pdev = (pdf14_device *)dev;
    /* The things that need to be cleaned up */
    pdf14_ctx *ctx = pdev->ctx;
    pdf14_smaskcolor_t *smaskcolor = pdev->smaskcolor;
    pdf14_group_color_t *group_color = pdev->color_model_stack;

    /* Free up the smask color */
    if (smaskcolor != NULL) {
        smaskcolor->ref_count = 1;
        pdf14_decrement_smask_color(pgs, dev);
        pdev->smaskcolor = NULL;
    }

    /* Free up the nested color procs and decrement the profiles */
    if (group_color != NULL) {
        while (group_color->previous != NULL)
            pdf14_pop_group_color(dev, pgs);
        gs_free_object(dev->memory->stable_memory, group_color, "pdf14_discard_trans_layer");
        pdev->color_model_stack = NULL;
    }

    /* Start the context clean up */
    if (ctx != NULL) {
        pdf14_buf *buf, *next;
        pdf14_group_color_t *procs, *prev_procs;

        if (ctx->mask_stack != NULL) {
            pdf14_free_mask_stack(ctx, ctx->memory);
        }

        /* Now the stack of buffers */
        for (buf = ctx->stack; buf != NULL; buf = next) {
            next = buf->saved;

            gs_free_object(ctx->memory, buf->transfer_fn, "pdf14_discard_trans_layer");
            gs_free_object(ctx->memory, buf->matte, "pdf14_discard_trans_layer");
            gs_free_object(ctx->memory, buf->data, "pdf14_discard_trans_layer");
            gs_free_object(ctx->memory, buf->backdrop, "pdf14_discard_trans_layer");
            /* During the soft mask push, the mask_stack was copied (not moved) from
               the ctx to the tos mask_stack. We are done with this now so it is safe
               to free this one object */
            gs_free_object(ctx->memory, buf->mask_stack, "pdf14_discard_trans_layer");
            for (procs = buf->group_color_info; procs != NULL; procs = prev_procs) {
                prev_procs = procs->previous;
                gs_free_object(ctx->memory, procs, "pdf14_discard_trans_layer");
            }
            gs_free_object(ctx->memory, buf, "pdf14_discard_trans_layer");
        }
        /* Finally the context itself */
        gs_free_object(ctx->memory, ctx, "pdf14_discard_trans_layer");
        pdev->ctx = NULL;
    }
    return 0;
}

static	int
pdf14_output_page(gx_device * dev, int num_copies, int flush)
{
    pdf14_device * pdev = (pdf14_device *)dev;

    if (pdev->target != NULL)
        return (*dev_proc(pdev->target, output_page)) (pdev->target, num_copies, flush);
    return 0;
}

#define	COPY_PARAM(p) dev->p = target->p
#define	COPY_ARRAY_PARAM(p) memcpy(dev->p, target->p, sizeof(dev->p))

/*
 * Copy device parameters back from a target.  This copies all standard
 * parameters related to page size and resolution, but not any of the
 * color-related parameters, as the pdf14 device retains its own color
 * handling. This routine is parallel to gx_device_copy_params().
 */
static	void
gs_pdf14_device_copy_params(gx_device *dev, const gx_device *target)
{
    cmm_dev_profile_t *profile_targ;
    cmm_dev_profile_t *profile_dev14;
    pdf14_device *pdev = (pdf14_device*) dev;
    int k;

    COPY_PARAM(width);
    COPY_PARAM(height);
    COPY_ARRAY_PARAM(MediaSize);
    COPY_ARRAY_PARAM(ImagingBBox);
    COPY_PARAM(ImagingBBox_set);
    COPY_ARRAY_PARAM(HWResolution);
    COPY_ARRAY_PARAM(Margins);
    COPY_ARRAY_PARAM(HWMargins);
    COPY_PARAM(PageCount);
    COPY_PARAM(MaxPatternBitmap);
    COPY_PARAM(graphics_type_tag);
    COPY_PARAM(interpolate_control);
    memcpy(&(dev->space_params), &(target->space_params), sizeof(gdev_space_params));
    /* The PDF14 device copies only the default profile not the text etc.
       TODO: MJV.  It has to make its own device structure but
       can grab a copy of the profile.  This allows swapping of profiles
       in the PDF14 device without messing up the target device profile.
       Also if the device is using a blend color space it will grab that too */
    if (dev->icc_struct == NULL) {
        dev->icc_struct = gsicc_new_device_profile_array(dev);
        profile_dev14 = dev->icc_struct;
        dev_proc((gx_device *) target, get_profile)((gx_device *) target,
                                          &(profile_targ));

        for (k = 0; k < NUM_DEVICE_PROFILES; k++) {
            if (profile_targ->device_profile[k] != NULL) {
                gsicc_adjust_profile_rc(profile_targ->device_profile[k], 1, "gs_pdf14_device_copy_params");
            }
            if (profile_dev14->device_profile[k] != NULL) {
                gsicc_adjust_profile_rc(profile_dev14->device_profile[k], -1, "gs_pdf14_device_copy_params");
            }
            profile_dev14->device_profile[k] = profile_targ->device_profile[k];
            profile_dev14->rendercond[k] = profile_targ->rendercond[k];
        }

        dev->icc_struct->devicegraytok = profile_targ->devicegraytok;
        dev->icc_struct->graydetection = profile_targ->graydetection;
        dev->icc_struct->pageneutralcolor = profile_targ->pageneutralcolor;
        dev->icc_struct->supports_devn = profile_targ->supports_devn;
        dev->icc_struct->usefastcolor = profile_targ->usefastcolor;
        dev->icc_struct->blacktext = profile_targ->blacktext;

        if (pdev->using_blend_cs) {
            /* Swap the device profile and the blend profile. */
            gsicc_adjust_profile_rc(profile_targ->device_profile[GS_DEFAULT_DEVICE_PROFILE],
                                    1, "gs_pdf14_device_copy_params");
            gsicc_adjust_profile_rc(profile_targ->blend_profile, 1, "gs_pdf14_device_copy_params");
            gsicc_adjust_profile_rc(profile_dev14->device_profile[GS_DEFAULT_DEVICE_PROFILE],
                                    -1, "gs_pdf14_device_copy_params");
            gsicc_adjust_profile_rc(profile_dev14->blend_profile, -1, "gs_pdf14_device_copy_params");
            profile_dev14->blend_profile = profile_targ->device_profile[GS_DEFAULT_DEVICE_PROFILE];
            profile_dev14->device_profile[GS_DEFAULT_DEVICE_PROFILE] = profile_targ->blend_profile;
        }
        profile_dev14->overprint_control = profile_targ->overprint_control;
    }
#undef COPY_ARRAY_PARAM
#undef COPY_PARAM
}

/*
 * This is a forwarding version of the put_params device proc.  It is only
 * used when the PDF 1.4 compositor devices are closed.  The routine will
 * check if the target device has closed and, if so, close itself.  The routine
 * also sync the device parameters.
 */
static	int
pdf14_forward_put_params(gx_device * dev, gs_param_list	* plist)
{
    pdf14_device * pdev = (pdf14_device *)dev;
    gx_device * tdev = pdev->target;
    bool was_open = tdev->is_open;
    int code = 0;

    if (tdev != 0 && (code = dev_proc(tdev, put_params)(tdev, plist)) >= 0) {
        gx_device_decache_colors(dev);
        if (!tdev->is_open) {
            code = gs_closedevice(dev);
            if (code == 0)
                code = was_open ? 1 : 0;   /* target device closed */
        }
        gx_device_copy_params(dev, tdev);
    }
    return code;
}

/* Function prototypes */
int put_param_pdf14_spot_names(gx_device * pdev,
                gs_separations * pseparations, gs_param_list * plist);
#define PDF14NumSpotColorsParamName "PDF14NumSpotColors"

/*
 * The put_params method for the PDF 1.4 device will check if the
 * target device has closed and, if so, close itself.  Note:  This routine is
 * currently being used by both the pdf14_clist_device and the pdf_device.
 * Please make sure that any changes are either applicable to both devices
 * or clone the routine for each device.
 */
static	int
pdf14_put_params(gx_device * dev, gs_param_list	* plist)
{
    pdf14_device * pdev = (pdf14_device *)dev;
    gx_device * tdev = pdev->target;
    bool was_open = tdev->is_open;
    int code = 0;

    if (tdev != 0 && (code = dev_proc(tdev, put_params)(tdev, plist)) >= 0) {
        gx_device_decache_colors(dev);
        if (!tdev->is_open) {
            code = gs_closedevice(dev);
            if (code == 0)
                code = was_open ? 1 : 0;   /* target device closed */
        }
        gs_pdf14_device_copy_params(dev, tdev);
    }
    return code;
}

/*
 * Copy marking related parameters into the PDF 1.4 device structure for use
 * by pdf14_fill_rectangle.
 */
static	void
pdf14_set_marking_params(gx_device *dev, const gs_gstate *pgs)
{
    pdf14_device * pdev = (pdf14_device *)dev;

    if (pgs->alphaisshape) {
        pdev->opacity = 1.0;
        if (pgs->is_fill_color) {
            pdev->shape = pgs->fillconstantalpha;
        } else {
            pdev->shape = pgs->strokeconstantalpha;
        }
    } else {
        pdev->shape = 1.0;
        if (pgs->is_fill_color) {
            pdev->opacity = pgs->fillconstantalpha;
        } else {
            pdev->opacity = pgs->strokeconstantalpha;
        }
    }
    pdev->alpha = pdev->opacity * pdev->shape;
    pdev->blend_mode = pgs->blend_mode;
    if (pdev->icc_struct->overprint_control != gs_overprint_control_disable) {
        pdev->overprint = pgs->overprint;
        pdev->stroke_overprint = pgs->stroke_overprint;
    } else {
        pdev->overprint = false;
        pdev->stroke_overprint = false;
    }

    pdev->fillconstantalpha = pgs->fillconstantalpha;
    pdev->strokeconstantalpha = pgs->strokeconstantalpha;

    if_debug6m('v', dev->memory,
               "[v]set_marking_params, opacity = %g, shape = %g, bm = %d, op = %d, eop = %d seop = %d\n",
               pdev->opacity, pdev->shape, pgs->blend_mode, pgs->overprint, pdev->effective_overprint_mode,
               pdev->stroke_effective_op_mode);
}

static  void
update_lop_for_pdf14(gs_gstate *pgs, const gx_drawing_color *pdcolor)
{
    bool hastrans = false;

    /* We'd really rather not have to set the pdf14 bit in the lop, as this
     * makes other operations much slower. We have no option however, if the
     * current colour involves transparency, or if it's anything other than
     * a completely solid (or transparent) operation in the normal blend mode.
     */
    if (pdcolor != NULL)
    {
        if (gx_dc_is_pattern1_color(pdcolor) &&
            gx_pattern1_get_transptr(pdcolor) != NULL) {
            hastrans = true;
        } else if (gx_dc_is_pattern2_color(pdcolor)) {
            /* FIXME: Here we assume that ALL type 2 patterns are
             * transparent - this test could be better. */
            hastrans = true;
        }
    }
    /* The only idempotent blend modes are Normal, Darken and Lighten.
       This appears to be the only place where this test is done so
       not adding a is_idempotent method */
    if ((pgs->blend_mode != BLEND_MODE_Normal &&
        pgs->blend_mode != BLEND_MODE_Darken &&
        pgs->blend_mode != BLEND_MODE_Lighten) ||
        (pgs->fillconstantalpha != 1.0) ||
        (pgs->strokeconstantalpha != 1.0) ||
        (hastrans))
    {
        /*
         * The blend operations are not idempotent.  Force non-idempotent
         * filling and stroking operations.
         */
        pgs->log_op |= lop_pdf14;
    }
}

static int
push_shfill_group(pdf14_clist_device *pdev,
                  gs_gstate *pgs,
                  gs_fixed_rect *box)
{
    gs_transparency_group_params_t params = { 0 };
    int code;
    gs_rect cb;
    gs_gstate fudged_pgs = *pgs;

    params.shade_group = true;

    /* gs_begin_transparency_group takes a bbox that it then
     * transforms by ctm. Our bbox has already been transformed,
     * so clear out the ctm. */
    fudged_pgs.ctm.xx = 1.0;
    fudged_pgs.ctm.xy = 0;
    fudged_pgs.ctm.yx = 0;
    fudged_pgs.ctm.yy = 1.0;
    fudged_pgs.ctm.tx = 0;
    fudged_pgs.ctm.ty = 0;
    cb.p.x = fixed2int_pixround(box->p.x);
    cb.p.y = fixed2int_pixround(box->p.y);
    cb.q.x = fixed2int_pixround(box->q.x);
    cb.q.y = fixed2int_pixround(box->q.y);

    params.Isolated = false;
    params.Knockout = true;
    params.page_group = false;
    params.group_opacity = fudged_pgs.fillconstantalpha;
    params.group_shape = 1.0;
    code = gs_begin_transparency_group(&fudged_pgs, &params, &cb, PDF14_BEGIN_TRANS_GROUP);

    /* We have the group handle the blendmode and the opacity,
     * and continue with the existing graphics state reset
     * to normal, opaque operation. We could do it the other
     * way around, but this way means that if we push a knockout
     * group for a stroke, and then the code calls back into
     * the fill operation as part of doing the stroking, we don't
     * push another one. */
    gs_setblendmode(pgs, BLEND_MODE_Normal);
    gs_setfillconstantalpha(pgs, 1.0);
    gs_setstrokeconstantalpha(pgs, 1.0);
    if (pdev) {
        code = pdf14_clist_update_params(pdev, pgs, false, NULL);
        if (code < 0)
            return code;
    }

    return code;
}

static int
pop_shfill_group(gs_gstate *pgs)
{
     return gs_end_transparency_group(pgs);
}

static int
pdf14_fill_path(gx_device *dev,	const gs_gstate *pgs,
                           gx_path *ppath, const gx_fill_params *params,
                           const gx_drawing_color *pdcolor,
                           const gx_clip_path *pcpath)
{
    gs_gstate new_pgs = *pgs;
    int code = 0;
    gs_pattern2_instance_t *pinst = NULL;
    int push_group = 0;

    code = pdf14_initialize_ctx(dev, dev->color_info.num_components,
        dev->color_info.polarity != GX_CINFO_POLARITY_SUBTRACTIVE, pgs);
    if (code < 0)
        return code;

    if (pdcolor == NULL)
       return_error(gs_error_unknownerror);	/* color must be defined */
    ((pdf14_device *)dev)->op_state = pgs->is_fill_color ? PDF14_OP_STATE_FILL : PDF14_OP_STATE_STROKE;
    if (gx_dc_is_pattern1_color(pdcolor)){
        if( gx_pattern1_get_transptr(pdcolor) != NULL ||
            gx_pattern1_clist_has_trans(pdcolor) ){
            /* In this case, we need to push a transparency group
               and tile the pattern color, which is stored in
               a pdf14 device buffer in the ctile object memember
               variable ttrans */
#if RAW_DUMP
            /* Since we do not get a put_image to view what
               we have do it now */
            if (gx_pattern1_get_transptr(pdcolor) != NULL) {
                const pdf14_device * ppatdev14 = (const pdf14_device *)
                                pdcolor->colors.pattern.p_tile->ttrans->pdev14;
                if (ppatdev14 != NULL) {  /* can occur during clist reading */
                    byte *buf_ptr = ppatdev14->ctx->stack->data  +
                        ppatdev14->ctx->stack->rect.p.y *
                        ppatdev14->ctx->stack->rowstride +
                        ppatdev14->ctx->stack->rect.p.x;
                    dump_raw_buffer(ppatdev14->ctx->memory,
                                    (ppatdev14->ctx->stack->rect.q.y -
                                     ppatdev14->ctx->stack->rect.p.y),
                                    (ppatdev14->ctx->stack->rect.q.x -
                                     ppatdev14->ctx->stack->rect.p.x),
                                    ppatdev14->ctx->stack->n_planes,
                                    ppatdev14->ctx->stack->planestride,
                                    ppatdev14->ctx->stack->rowstride,
                                    "Pattern_Fill", buf_ptr,
                                    ppatdev14->ctx->stack->deep);
                    global_index++;
                } else {
                     gx_pattern_trans_t *patt_trans =
                                        pdcolor->colors.pattern.p_tile->ttrans;
                     dump_raw_buffer(patt_trans->mem,
                                     patt_trans->rect.q.y-patt_trans->rect.p.y,
                                     patt_trans->rect.q.x-patt_trans->rect.p.x,
                                     patt_trans->n_chan,
                                     patt_trans->planestride, patt_trans->rowstride,
                                     "Pattern_Fill_clist",
                                     patt_trans->transbytes +
                                         patt_trans->rect.p.y * patt_trans->rowstride +
                                         (patt_trans->rect.p.x<<patt_trans->deep),
                                     patt_trans->deep);
                    global_index++;
                }
            }
#endif
            pdf14_set_marking_params(dev, &new_pgs);
            code = pdf14_tile_pattern_fill(dev, &new_pgs, ppath,
                params, pdcolor, pcpath);
            new_pgs.trans_device = NULL;
            new_pgs.has_transparency = false;
            return code;
        }
    }
    if (gx_dc_is_pattern2_color(pdcolor) ||
        pdcolor->type == &gx_dc_devn_masked) {
        /* Non-idempotent blends require a transparency
         * group to be pushed because shadings might
         * paint several pixels twice. */
        push_group = pgs->fillconstantalpha != 1.0 ||
               !blend_is_idempotent(gs_currentblendmode(pgs));
        pinst =
            (gs_pattern2_instance_t *)pdcolor->ccolor.pattern;
        pinst->saved->has_transparency = true;
        /* The transparency color space operations are driven
           by the pdf14 clist writer device.  */
        pinst->saved->trans_device = dev;
    }
    if (push_group) {
        gs_fixed_rect box;
        if (pcpath)
            gx_cpath_outer_box(pcpath, &box);
        else
            (*dev_proc(dev, get_clipping_box)) (dev, &box);
        if (ppath) {
            gs_fixed_rect path_box;

            gx_path_bbox(ppath, &path_box);
            if (box.p.x < path_box.p.x)
                box.p.x = path_box.p.x;
            if (box.p.y < path_box.p.y)
                box.p.y = path_box.p.y;
            if (box.q.x > path_box.q.x)
                box.q.x = path_box.q.x;
            if (box.q.y > path_box.q.y)
                box.q.y = path_box.q.y;
        }
        /* Group alpha set from fill value. push_shfill_group does reset to 1.0 */
        code = push_shfill_group(NULL, &new_pgs, &box);
    } else
        update_lop_for_pdf14(&new_pgs, pdcolor);
    pdf14_set_marking_params(dev, &new_pgs);
    if (code >= 0) {
        new_pgs.trans_device = dev;
        new_pgs.has_transparency = true;
        /* ppath can permissibly be NULL here, if we want to have a
         * shading or a pattern fill the clipping path. This upsets
         * coverity, which is not smart enough to realise that the
         * validity of a NULL ppath depends on the type of pdcolor.
         * We'll mark it as a false positive. */
        code = gx_default_fill_path(dev, &new_pgs, ppath, params, pdcolor, pcpath);
        new_pgs.trans_device = NULL;
        new_pgs.has_transparency = false;
    }
    if (code >= 0 && push_group) {
        code = pop_shfill_group(&new_pgs);
        pdf14_set_marking_params(dev, pgs);
    }
    if (pinst != NULL){
        pinst->saved->trans_device = NULL;
    }
    return code;
}

static	int
pdf14_stroke_path(gx_device *dev, const	gs_gstate	*pgs,
                             gx_path *ppath, const gx_stroke_params *params,
                             const gx_drawing_color *pdcolor,
                             const gx_clip_path *pcpath)
{
    gs_gstate new_pgs = *pgs;
    int push_group = 0;
    int code = 0;

    if (pdcolor == NULL)
       return_error(gs_error_unknownerror);	/* color must be defined */

    code = pdf14_initialize_ctx(dev, dev->color_info.num_components,
        dev->color_info.polarity != GX_CINFO_POLARITY_SUBTRACTIVE, pgs);
    if (code < 0)
        return code;

    if (gx_dc_is_pattern2_color(pdcolor)) {
        /* Non-idempotent blends require a transparency
         * group to be pushed because shadings might
         * paint several pixels twice. */
        push_group = pgs->strokeconstantalpha != 1.0 ||
               !blend_is_idempotent(gs_currentblendmode(pgs));
    }
    if (push_group) {
        gs_fixed_rect box;
        if (pcpath)
            gx_cpath_outer_box(pcpath, &box);
        else
            (*dev_proc(dev, get_clipping_box)) (dev, &box);

        /* For fill_path, we accept ppath == NULL to mean
         * fill the entire clipping region. That makes no
         * sense for stroke_path, hence ppath is always non
         * NULL here. */
        {
            gs_fixed_rect path_box;
            gs_fixed_point expansion;

            gx_path_bbox(ppath, &path_box);
            /* Expand the path bounding box by the scaled line width. */
            if (gx_stroke_path_expansion(pgs, ppath, &expansion) < 0) {
                /* The expansion is so large it caused a limitcheck. */
                path_box.p.x = path_box.p.y = min_fixed;
                path_box.q.x = path_box.q.y = max_fixed;
            } else {
                expansion.x += pgs->fill_adjust.x;
                expansion.y += pgs->fill_adjust.y;
                /*
                 * It's theoretically possible for the following computations to
                 * overflow, so we need to check for this.
                 */
                path_box.p.x = (path_box.p.x < min_fixed + expansion.x ? min_fixed :
                                path_box.p.x - expansion.x);
                path_box.p.y = (path_box.p.y < min_fixed + expansion.y ? min_fixed :
                                path_box.p.y - expansion.y);
                path_box.q.x = (path_box.q.x > max_fixed - expansion.x ? max_fixed :
                                path_box.q.x + expansion.x);
                path_box.q.y = (path_box.q.y > max_fixed - expansion.y ? max_fixed :
                                path_box.q.y + expansion.y);
            }
            if (box.p.x < path_box.p.x)
                box.p.x = path_box.p.x;
            if (box.p.y < path_box.p.y)
                box.p.y = path_box.p.y;
            if (box.q.x > path_box.q.x)
                box.q.x = path_box.q.x;
            if (box.q.y > path_box.q.y)
                box.q.y = path_box.q.y;
        }
        /* Group alpha set from fill value. push_shfill_group does reset to 1.0 */
        new_pgs.fillconstantalpha = new_pgs.strokeconstantalpha;
        code = push_shfill_group(NULL, &new_pgs, &box);
    } else
        update_lop_for_pdf14(&new_pgs, pdcolor);
    pdf14_set_marking_params(dev, &new_pgs);
    if (code >= 0) {
        PDF14_OP_FS_STATE save_op_state = ((pdf14_device *)dev)->op_state;

        ((pdf14_device*)dev)->op_state = PDF14_OP_STATE_STROKE;
        code = gx_default_stroke_path(dev, &new_pgs, ppath, params, pdcolor, pcpath);
        ((pdf14_device*)dev)->op_state = save_op_state;
    }
    if (code >= 0 && push_group) {
        code = pop_shfill_group(&new_pgs);
        pdf14_set_marking_params(dev, pgs);
    }

    return code;
}

static int
pdf14_fill_stroke_path(gx_device *dev, const gs_gstate *cpgs, gx_path *ppath,
    const gx_fill_params *fill_params, const gx_drawing_color *pdcolor_fill,
    const gx_stroke_params *stroke_params, const gx_drawing_color *pdcolor_stroke,
    const gx_clip_path *pcpath)
{
    union {
        const gs_gstate *cpgs;
        gs_gstate *pgs;
    } const_breaker;
    gs_gstate *pgs;
    int code, code2;
    gs_transparency_group_params_t params = { 0 };
    gs_fixed_rect clip_bbox;
    gs_rect bbox, group_stroke_box;
    gs_fixed_rect path_bbox;
    int expansion_code;
    gs_fixed_point expansion;
    pdf14_device *p14dev = (pdf14_device *)dev;
    float stroke_alpha = cpgs->strokeconstantalpha;
    float fill_alpha = cpgs->fillconstantalpha;
    gs_blend_mode_t blend_mode = cpgs->blend_mode;
    PDF14_OP_FS_STATE save_op_state = p14dev->op_state;

    /* Break const just once, neatly */
    const_breaker.cpgs = cpgs;
    pgs = const_breaker.pgs;

    if ((pgs->fillconstantalpha == 0.0 && pgs->strokeconstantalpha == 0.0) ||
        (pgs->ctm.xx == 0.0 && pgs->ctm.xy == 0.0 && pgs->ctm.yx == 0.0 && pgs->ctm.yy == 0.0))
        return 0;

    code = pdf14_initialize_ctx(dev, dev->color_info.num_components,
        dev->color_info.polarity != GX_CINFO_POLARITY_SUBTRACTIVE, cpgs);
    if (code < 0)
        return code;

    code = gx_curr_fixed_bbox(pgs, &clip_bbox, NO_PATH);
    if (code < 0 && code != gs_error_unknownerror)
        return code;
    if (code == gs_error_unknownerror) {
        /* didn't get clip box from gx_curr_fixed_bbox */
        clip_bbox.p.x = clip_bbox.p.y = 0;
        clip_bbox.q.x = int2fixed(dev->width);
        clip_bbox.q.y = int2fixed(dev->height);
    }
    if (pcpath)
        rect_intersect(clip_bbox, pcpath->outer_box);

    /* expand the ppath using stroke expansion rule, then intersect it */
    code = gx_path_bbox(ppath, &path_bbox);
    if (code == gs_error_nocurrentpoint && ppath->segments->contents.subpath_first == 0)
        return 0;		/* ignore empty path */
    if (code < 0)
        return code;
    expansion_code = gx_stroke_path_expansion(pgs, ppath, &expansion);
    if (expansion_code >= 0) {
        path_bbox.p.x -= expansion.x;
        path_bbox.p.y -= expansion.y;
        path_bbox.q.x += expansion.x;
        path_bbox.q.y += expansion.y;
    }
    rect_intersect(path_bbox, clip_bbox);
    bbox.p.x = fixed2float(path_bbox.p.x);
    bbox.p.y = fixed2float(path_bbox.p.y);
    bbox.q.x = fixed2float(path_bbox.q.x);
    bbox.q.y = fixed2float(path_bbox.q.y);

    code = gs_bbox_transform_inverse(&bbox, &ctm_only(pgs), &group_stroke_box);
    if (code < 0)
        return code;
    if (p14dev->overprint != pgs->overprint || p14dev->stroke_overprint != pgs->stroke_overprint) {
        p14dev->overprint = pgs->overprint;
        p14dev->stroke_overprint = pgs->stroke_overprint;
    }
    /* See if overprint is enabled for both stroke and fill AND if ca == CA */
    if (fill_alpha == stroke_alpha &&
        p14dev->overprint && p14dev->stroke_overprint &&
        dev->color_info.polarity == GX_CINFO_POLARITY_SUBTRACTIVE) {
        /* Push a non-isolated non-knockout group with alpha = 1.0 and
           compatible overprint mode.  Group will be composited with
           original alpha and blend mode */
        params.Isolated = false;
        params.group_color_type = UNKNOWN;
        params.Knockout = false;
        params.page_group = false;
        params.group_opacity = 1.0;
        params.group_shape = fill_alpha;

        /* non-isolated non-knockout group pushed with original alpha and blend mode */
        code = pdf14_begin_transparency_group(dev, &params,
                                              &group_stroke_box, pgs, dev->memory);
        if (code < 0)
            return code;

        /* Change fill alpha to 1.0 and blend mode to compatible overprint for actual drawing */
        (void)gs_setfillconstantalpha(pgs, 1.0);
        (void)gs_setblendmode(pgs, BLEND_MODE_CompatibleOverprint); /* Can never fail */

        p14dev->op_state = PDF14_OP_STATE_FILL;
        code = pdf14_fill_path(dev, pgs, ppath, fill_params, pdcolor_fill, pcpath);
        if (code < 0)
            goto cleanup;

        (void)gs_setstrokeconstantalpha(pgs, 1.0);
        gs_swapcolors_quick(pgs);	/* flips stroke_color_index (to stroke) */
        p14dev->op_state = PDF14_OP_STATE_STROKE;
        code = pdf14_stroke_path(dev, pgs, ppath, stroke_params, pdcolor_stroke, pcpath);
        gs_swapcolors_quick(pgs);	/* this flips pgs->stroke_color_index back as well */
        if (code < 0)
            goto cleanup;       /* bail out (with colors swapped back to fill) */

    } else {
        /* Push a non-isolated knockout group. Do not change the alpha or
            blend modes. Note: we need to draw those that have alpha = 0 */
        params.Isolated = false;
        params.group_color_type = UNKNOWN;
        params.Knockout = true;
        params.page_group = false;
        params.group_shape = 1.0;
        params.group_opacity = 1.0;

        /* non-isolated knockout group is pushed with alpha = 1.0 and Normal blend mode */
        (void)gs_setblendmode(pgs, BLEND_MODE_Normal); /* Can never fail */
        code = pdf14_begin_transparency_group(dev, &params, &group_stroke_box,
                                              pgs, dev->memory);

        /* restore blend mode for actual drawing in the group */
        (void)gs_setblendmode(pgs, blend_mode); /* Can never fail */

        /* If we are in an overprint situation, set the blend mode to compatible
            overprint */
        if ((p14dev->icc_struct->overprint_control != gs_overprint_control_disable) && pgs->overprint &&
            dev->color_info.polarity == GX_CINFO_POLARITY_SUBTRACTIVE)
            (void)gs_setblendmode(pgs, BLEND_MODE_CompatibleOverprint); /* Can never fail */
        code = pdf14_fill_path(dev, pgs, ppath, fill_params, pdcolor_fill, pcpath);
        if ((p14dev->icc_struct->overprint_control != gs_overprint_control_disable) && pgs->overprint &&
            dev->color_info.polarity == GX_CINFO_POLARITY_SUBTRACTIVE)
            (void)gs_setblendmode(pgs, blend_mode); /* Can never fail */
        if (code < 0)
            goto cleanup;

        /* Note that the stroke can end up doing fill methods */
        (void)gs_setfillconstantalpha(pgs, stroke_alpha);

        gs_swapcolors_quick(pgs);
        p14dev->op_state = PDF14_OP_STATE_STROKE;
        if ((p14dev->icc_struct->overprint_control != gs_overprint_control_disable) && pgs->stroke_overprint &&
            dev->color_info.polarity == GX_CINFO_POLARITY_SUBTRACTIVE)
            (void)gs_setblendmode(pgs, BLEND_MODE_CompatibleOverprint); /* Can never fail */
        code = pdf14_stroke_path(dev, pgs, ppath, stroke_params, pdcolor_stroke, pcpath);
        /* Don't need to restore blendmode here, as it will be restored below. */
        gs_swapcolors_quick(pgs);
        if (code < 0)
            goto cleanup;
        /* Bug 703324 we need to reset the fill constant alpha in the graphics
         * state to the correct saved value. We also need to reset the 'opacity' member of the
         * device, because some device methods (eg fill_masked_image) don't take a graphics
         * state pointer as a parameter and so are unable to set the opacity value themselves.
         * We therefore need to make sure it is set according to the current fill state.
         */
        (void)gs_setfillconstantalpha(pgs, fill_alpha);
        pdf14_set_marking_params(dev, pgs);
    }

cleanup:
    /* Restore the state */
    p14dev->op_state = save_op_state;
    (void)gs_setblendmode(pgs, blend_mode); /* Can never fail */
    (void)gs_setstrokeconstantalpha(pgs, stroke_alpha);
    (void)gs_setfillconstantalpha(pgs, fill_alpha);

    code2 = pdf14_end_transparency_group(dev, pgs);
    if (code2 < 0) {
        /* At this point things have gone very wrong. We should just shut down */
        code = gs_abort_pdf14trans_device(pgs);
        return code2;
    }
    return code;
}

static int
pdf14_copy_alpha(gx_device * dev, const byte * data, int data_x,
           int aa_raster, gx_bitmap_id id, int x, int y, int w, int h,
                      gx_color_index color, int depth)
{
    return pdf14_copy_alpha_color(dev, data, data_x, aa_raster, id, x, y, w, h,
                                  color, NULL, depth, false);
}

static int
pdf14_copy_alpha_hl_color(gx_device * dev, const byte * data, int data_x,
           int aa_raster, gx_bitmap_id id, int x, int y, int w, int h,
                      const gx_drawing_color *pdcolor, int depth)
{
    return pdf14_copy_alpha_color(dev, data, data_x, aa_raster, id, x, y, w, h,
                                  0, pdcolor, depth, true);
}

static int
do_pdf14_copy_alpha_color(gx_device * dev, const byte * data, int data_x,
                          int aa_raster, gx_bitmap_id id, int x, int y,
                          int w, int h, gx_color_index color,
                          const gx_device_color *pdc, int depth, bool devn)
{
    const byte *aa_row;
    pdf14_device *pdev = (pdf14_device *)dev;
    pdf14_buf *buf = pdev->ctx->stack;
    int i, j, k;
    byte *line, *dst_ptr;
    byte src[PDF14_MAX_PLANES];
    byte dst[PDF14_MAX_PLANES] = { 0 };
    gs_blend_mode_t blend_mode = pdev->blend_mode;
    bool additive = pdev->ctx->additive;
    int rowstride = buf->rowstride;
    int planestride = buf->planestride;
    gs_graphics_type_tag_t curr_tag = GS_UNKNOWN_TAG;  /* Quiet compiler */
    bool has_alpha_g = buf->has_alpha_g;
    bool has_shape = buf->has_shape;
    bool has_tags = buf->has_tags;
    bool knockout = buf->knockout;
    bool tag_blend = blend_mode == BLEND_MODE_Normal ||
        blend_mode == BLEND_MODE_Compatible ||
        blend_mode == BLEND_MODE_CompatibleOverprint;
    int num_chan = buf->n_chan;
    int num_comp = num_chan - 1;
    int shape_off = num_chan * planestride;
    int alpha_g_off = shape_off + (has_shape ? planestride : 0);
    int tag_off = alpha_g_off + (has_alpha_g ? planestride : 0);
    bool overprint = pdev->op_state == PDF14_OP_STATE_FILL ? pdev->overprint : pdev->stroke_overprint;
    gx_color_index drawn_comps = pdev->op_state == PDF14_OP_STATE_FILL ?
                                 pdev->drawn_comps_fill : pdev->drawn_comps_stroke;
    gx_color_index comps;
    byte shape = 0; /* Quiet compiler. */
    byte src_alpha;
    int alpha2_aa, alpha_aa, sx;
    int alpha_aa_act;
    int xoff;
    gx_color_index mask = ((gx_color_index)1 << 8) - 1;
    int shift = 8;

    if (buf->data == NULL)
        return 0;
    aa_row = data;
    if (has_tags) {
        curr_tag = (color >> (num_comp*8)) & 0xff;
    }

    if (devn) {
        if (additive) {
            for (j = 0; j < num_comp; j++) {
                src[j] = ((pdc->colors.devn.values[j]) >> shift & mask);
            }
        } else {
            for (j = 0; j < num_comp; j++) {
                src[j] = 255 - ((pdc->colors.devn.values[j]) >> shift & mask);
            }
        }
    } else
        pdev->pdf14_procs->unpack_color(num_comp, color, pdev, src);
    src_alpha = src[num_comp] = (byte)floor (255 * pdev->alpha + 0.5);
    if (has_shape)
        shape = (byte)floor (255 * pdev->shape + 0.5);
    /* Limit the area we write to the bounding rectangle for this buffer */
    if (x < buf->rect.p.x) {
        xoff = data_x + buf->rect.p.x - x;
        w += x - buf->rect.p.x;
        x = buf->rect.p.x;
    } else {
        xoff = data_x;
    }
    if (y < buf->rect.p.y) {
      h += y - buf->rect.p.y;
      aa_row -= (y - buf->rect.p.y) * aa_raster;
      y = buf->rect.p.y;
    }
    if (x + w > buf->rect.q.x) w = buf->rect.q.x - x;
    if (y + h > buf->rect.q.y) h = buf->rect.q.y - y;
    /* Update the dirty rectangle. */
    if (x < buf->dirty.p.x) buf->dirty.p.x = x;
    if (y < buf->dirty.p.y) buf->dirty.p.y = y;
    if (x + w > buf->dirty.q.x) buf->dirty.q.x = x + w;
    if (y + h > buf->dirty.q.y) buf->dirty.q.y = y + h;
    line = buf->data + (x - buf->rect.p.x) + (y - buf->rect.p.y) * rowstride;

    for (j = 0; j < h; ++j, aa_row += aa_raster) {
        dst_ptr = line;
        sx = xoff;
        for (i = 0; i < w; ++i, ++sx) {
            /* Complement the components for subtractive color spaces */
            if (additive) {
                for (k = 0; k < num_chan; ++k)		/* num_chan includes alpha */
                    dst[k] = dst_ptr[k * planestride];
            } else { /* Complement the components for subtractive color spaces */
                for (k = 0; k < num_comp; ++k)
                    dst[k] = 255 - dst_ptr[k * planestride];
                dst[num_comp] = dst_ptr[num_comp * planestride];	/* alpha */
            }
            /* Get the aa alpha from the buffer */
            switch(depth)
            {
            case 2:  /* map 0 - 3 to 0 - 255 */
                alpha_aa = ((aa_row[sx >> 2] >> ((3 - (sx & 3)) << 1)) & 3) * 85;
                break;
            case 4:
                alpha2_aa = aa_row[sx >> 1];
                alpha_aa = (sx & 1 ? alpha2_aa & 0xf : alpha2_aa >> 4) * 17;
                break;
            case 8:
                alpha_aa = aa_row[sx];
                break;
            default:
                return_error(gs_error_rangecheck);
            }
            if (alpha_aa != 0) {  /* This does happen */
                if (alpha_aa != 255) {
                    /* We have an alpha value from aa */
                    alpha_aa_act = alpha_aa;
                    if (src_alpha != 255) {
                        /* Need to combine it with the existing alpha */
                        int tmp = src_alpha * alpha_aa_act + 0x80;
                        alpha_aa_act = (tmp + (tmp >> 8)) >> 8;
                    }
                    /* Set our source alpha value appropriately */
                    src[num_comp] = alpha_aa_act;
                } else {
                    /* We may have to reset this is it was changed as we
                       moved across the row */
                    src[num_comp] = src_alpha;
                }
                if (knockout) {
                    if (buf->isolated) {
                        art_pdf_knockoutisolated_group_8(dst, src, num_comp);
                    } else {
                        art_pdf_composite_knockout_8(dst, src, num_comp,
                            blend_mode, pdev->blend_procs, pdev);
                    }
                } else {
                    art_pdf_composite_pixel_alpha_8(dst, src, num_comp, blend_mode, num_comp,
                                                    pdev->blend_procs, pdev);
                }
                /* Complement the results for subtractive color spaces */
                if (additive) {
                    for (k = 0; k < num_chan; ++k)
                        dst_ptr[k * planestride] = dst[k];
                } else {
                    if (overprint && dst_ptr[num_comp * planestride] != 0) {
                        for (k = 0, comps = drawn_comps; comps != 0;
                                ++k, comps >>= 1) {
                            if ((comps & 0x1) != 0) {
                                dst_ptr[k * planestride] = 255 - dst[k];
                            }
                        }
                        /* The alpha channel */
                        dst_ptr[num_comp * planestride] = dst[num_comp];
                    } else {
                        for (k = 0; k < num_comp; ++k)
                            dst_ptr[k * planestride] = 255 - dst[k];
                        /* The alpha channel */
                        dst_ptr[num_comp * planestride] = dst[num_comp];
                    }
                }
                if (has_alpha_g) {
                    int tmp = (255 - dst_ptr[alpha_g_off]) * (255 - src[num_comp]) + 0x80;
                    dst_ptr[alpha_g_off] = 255 - ((tmp + (tmp >> 8)) >> 8);
                }
                if (has_shape) {
                    int tmp = (255 - dst_ptr[shape_off]) * (255 - shape) + 0x80;
                    dst_ptr[shape_off] = 255 - ((tmp + (tmp >> 8)) >> 8);
                }
                if (has_tags) {
                    /* If alpha is 100% then set to curr_tag, else or */
                    /* other than Normal BM, we always OR */
                    if (src[num_comp] == 255 && tag_blend) {
                        dst_ptr[tag_off] = curr_tag;
                    } else {
                        dst_ptr[tag_off] |= curr_tag;
                    }
                }
            }
            ++dst_ptr;
        }
        line += rowstride;
    }
    return 0;
}

static int
do_pdf14_copy_alpha_color_16(gx_device * dev, const byte * data, int data_x,
                             int aa_raster, gx_bitmap_id id, int x, int y,
                             int w, int h, gx_color_index color,
                             const gx_device_color *pdc, int depth, bool devn)
{
    const byte *aa_row;
    pdf14_device *pdev = (pdf14_device *)dev;
    pdf14_buf *buf = pdev->ctx->stack;
    int i, j, k;
    byte *line;
    uint16_t *dst_ptr;
    uint16_t src[PDF14_MAX_PLANES];
    uint16_t dst[PDF14_MAX_PLANES] = { 0 };
    gs_blend_mode_t blend_mode = pdev->blend_mode;
    bool tag_blend = blend_mode == BLEND_MODE_Normal ||
        blend_mode == BLEND_MODE_Compatible ||
        blend_mode == BLEND_MODE_CompatibleOverprint;
    bool additive = pdev->ctx->additive;
    int rowstride = buf->rowstride;
    int planestride = buf->planestride;
    gs_graphics_type_tag_t curr_tag = GS_UNKNOWN_TAG;  /* Quiet compiler */
    bool has_alpha_g = buf->has_alpha_g;
    bool has_shape = buf->has_shape;
    bool has_tags = buf->has_tags;
    bool knockout = buf->knockout;
    int num_chan = buf->n_chan;
    int num_comp = num_chan - 1;
    int shape_off = num_chan * planestride;
    int alpha_g_off = shape_off + (has_shape ? planestride : 0);
    int tag_off = alpha_g_off + (has_alpha_g ? planestride : 0);
    bool overprint = pdev->op_state == PDF14_OP_STATE_FILL ? pdev->overprint : pdev->stroke_overprint;
    gx_color_index drawn_comps = pdev->op_state == PDF14_OP_STATE_FILL ?
        pdev->drawn_comps_fill : pdev->drawn_comps_stroke;
    gx_color_index comps;
    uint16_t shape = 0; /* Quiet compiler. */
    uint16_t src_alpha;
    int alpha2_aa, alpha_aa, sx;
    int alpha_aa_act;
    int xoff;

    if (buf->data == NULL)
        return 0;
    aa_row = data;
    if (has_tags) {
        curr_tag = (color >> (num_comp*16)) & 0xff;
    }

    if (devn) {
        if (additive) {
            for (j = 0; j < num_comp; j++) {
                src[j] = pdc->colors.devn.values[j];
            }
        } else {
            for (j = 0; j < num_comp; j++) {
                src[j] = 65535 - pdc->colors.devn.values[j];
            }
        }
    } else
        pdev->pdf14_procs->unpack_color16(num_comp, color, pdev, src);
    src_alpha = src[num_comp] = (uint16_t)floor (65535 * pdev->alpha + 0.5);
    if (has_shape)
        shape = (uint16_t)floor (65535 * pdev->shape + 0.5);
    /* Limit the area we write to the bounding rectangle for this buffer */
    if (x < buf->rect.p.x) {
        xoff = data_x + buf->rect.p.x - x;
        w += x - buf->rect.p.x;
        x = buf->rect.p.x;
    } else {
        xoff = data_x;
    }
    if (y < buf->rect.p.y) {
      h += y - buf->rect.p.y;
      aa_row -= (y - buf->rect.p.y) * aa_raster;
      y = buf->rect.p.y;
    }
    if (x + w > buf->rect.q.x) w = buf->rect.q.x - x;
    if (y + h > buf->rect.q.y) h = buf->rect.q.y - y;
    /* Update the dirty rectangle. */
    if (x < buf->dirty.p.x) buf->dirty.p.x = x;
    if (y < buf->dirty.p.y) buf->dirty.p.y = y;
    if (x + w > buf->dirty.q.x) buf->dirty.q.x = x + w;
    if (y + h > buf->dirty.q.y) buf->dirty.q.y = y + h;
    line = buf->data + (x - buf->rect.p.x)*2 + (y - buf->rect.p.y) * rowstride;

    planestride >>= 1;
    rowstride   >>= 1;
    alpha_g_off >>= 1;
    shape_off   >>= 1;
    tag_off     >>= 1;
    for (j = 0; j < h; ++j, aa_row += aa_raster) {
        dst_ptr = (uint16_t *)(void *)line;
        sx = xoff;
        for (i = 0; i < w; ++i, ++sx) {
            /* Complement the components for subtractive color spaces */
            if (additive) {
                for (k = 0; k < num_chan; ++k)		/* num_chan includes alpha */
                    dst[k] = dst_ptr[k * planestride];
            } else { /* Complement the components for subtractive color spaces */
                for (k = 0; k < num_comp; ++k)
                    dst[k] = 65535 - dst_ptr[k * planestride];
                dst[num_comp] = dst_ptr[num_comp * planestride];	/* alpha */
            }
            /* Get the aa alpha from the buffer */
            switch(depth)
            {
            case 2:  /* map 0 - 3 to 0 - 255 */
                alpha_aa = ((aa_row[sx >> 2] >> ((3 - (sx & 3)) << 1)) & 3) * 85;
                break;
            case 4:
                alpha2_aa = aa_row[sx >> 1];
                alpha_aa = (sx & 1 ? alpha2_aa & 0xf : alpha2_aa >> 4) * 17;
                break;
            case 8:
                alpha_aa = aa_row[sx];
                break;
            default:
                return_error(gs_error_rangecheck);
            }
            if (alpha_aa != 0) {  /* This does happen */
                if (alpha_aa != 255) {
                    /* We have an alpha value from aa */
                    alpha_aa_act = alpha_aa * 0x101;
                    if (src_alpha != 65535) {
                        /* Need to combine it with the existing alpha */
                        int tmp = src_alpha * alpha_aa_act + 0x8000;
                        alpha_aa_act = (tmp + (tmp >> 16)) >> 16;
                    }
                    /* Set our source alpha value appropriately */
                    src[num_comp] = alpha_aa_act;
                } else {
                    /* We may have to reset this is it was changed as we
                       moved across the row */
                    src[num_comp] = src_alpha;
                }
                if (knockout) {
                    if (buf->isolated) {
                        art_pdf_knockoutisolated_group_16(dst, src, num_comp);
                    } else {
                        art_pdf_composite_knockout_16(dst, src, num_comp,
                            blend_mode, pdev->blend_procs, pdev);
                    }
                } else {
                    art_pdf_composite_pixel_alpha_16(dst, src, num_comp, blend_mode, num_comp,
                                                     pdev->blend_procs, pdev);
                }
                /* Complement the results for subtractive color spaces */
                if (additive) {
                    for (k = 0; k < num_chan; ++k)
                        dst_ptr[k * planestride] = dst[k];
                } else {
                    if (overprint && dst_ptr[num_comp * planestride] != 0) {
                        for (k = 0, comps = drawn_comps; comps != 0;
                                ++k, comps >>= 1) {
                            if ((comps & 0x1) != 0) {
                                dst_ptr[k * planestride] = 65535 - dst[k];
                            }
                        }
                        /* The alpha channel */
                        dst_ptr[num_comp * planestride] = dst[num_comp];
                    } else {
                        for (k = 0; k < num_comp; ++k)
                            dst_ptr[k * planestride] = 65535 - dst[k];
                        /* The alpha channel */
                        dst_ptr[num_comp * planestride] = dst[num_comp];
                    }
                }
                if (has_alpha_g) {
                    int tmp = (65535 - dst_ptr[alpha_g_off]) * (65535 - src[num_comp]) + 0x8000;
                    dst_ptr[alpha_g_off] = 65535 - ((tmp + (tmp >> 16)) >> 16);
                }
                if (has_shape) {
                    int tmp = (65535 - dst_ptr[shape_off]) * (65535 - shape) + 0x8000;
                    dst_ptr[shape_off] = 65535 - ((tmp + (tmp >> 16)) >> 16);
                }
                if (has_tags) {
                    /* If alpha is 100% then set to curr_tag, else or */
                    /* other than Normal BM, we always OR */
                    if (src[num_comp] == 65535 && tag_blend) {
                        dst_ptr[tag_off] = curr_tag;
                    } else {
                        dst_ptr[tag_off] |= curr_tag;
                    }
                }
            }
            ++dst_ptr;
        }
        line += rowstride;
    }
    return 0;
}

static int
pdf14_copy_alpha_color(gx_device * dev, const byte * data, int data_x,
           int aa_raster, gx_bitmap_id id, int x, int y, int w, int h,
                      gx_color_index color, const gx_device_color *pdc,
                      int depth, bool devn)
{
    bool deep = device_is_deep(dev);
    int code;

    code = pdf14_initialize_ctx(dev, dev->color_info.num_components,
        dev->color_info.polarity != GX_CINFO_POLARITY_SUBTRACTIVE, NULL);
    if (code < 0)
        return code;

    if (deep)
        return do_pdf14_copy_alpha_color_16(dev, data, data_x, aa_raster,
                                            id, x, y, w, h,
                                            color, pdc, depth, devn);
    else
        return do_pdf14_copy_alpha_color(dev, data, data_x, aa_raster,
                                         id, x, y, w, h,
                                         color, pdc, depth, devn);
}

static	int
pdf14_fill_mask(gx_device * orig_dev,
                     const byte * data, int dx, int raster, gx_bitmap_id id,
                     int x, int y, int w, int h,
                     const gx_drawing_color * pdcolor, int depth,
                     gs_logical_operation_t lop, const gx_clip_path * pcpath)
{
    gx_device *dev;
    pdf14_device *p14dev = (pdf14_device *)orig_dev;
    gx_device_clip cdev;
    gx_color_tile *ptile = NULL;
    int code = 0;
    gs_int_rect group_rect;
    gx_pattern_trans_t *fill_trans_buffer = NULL;
    bool has_pattern_trans = false;
    cmm_dev_profile_t *dev_profile;

    if (pdcolor == NULL)
        return_error(gs_error_unknownerror);	/* color must be defined */

    code = pdf14_initialize_ctx(orig_dev, orig_dev->color_info.num_components,
        orig_dev->color_info.polarity != GX_CINFO_POLARITY_SUBTRACTIVE, NULL);
    if (code < 0)
        return code;

    /* If we are doing a fill with a pattern that has a transparency then
       go ahead and do a push and a pop of the transparency group */
    if (gx_dc_is_pattern1_color(pdcolor)) {
        if( gx_pattern1_get_transptr(pdcolor) != NULL) {
            ptile = pdcolor->colors.pattern.p_tile;
            /* Set up things in the ptile so that we get the proper
               blending etc */
            /* Set the blending procs and the is_additive setting based
               upon the number of channels */
            if (ptile->ttrans->n_chan-1 < 4) {
                ptile->ttrans->blending_procs = &rgb_blending_procs;
                ptile->ttrans->is_additive = true;
            } else {
                ptile->ttrans->blending_procs = &cmyk_blending_procs;
                ptile->ttrans->is_additive = false;
            }
            /* Set the procs so that we use the proper filling method. */
            gx_set_pattern_procs_trans((gx_device_color*) pdcolor);
            /* Based upon if the tiles overlap pick the type of rect
               fill that we will want to use */
            if (ptile->has_overlap) {
                /* This one does blending since there is tile overlap */
                ptile->ttrans->pat_trans_fill = &tile_rect_trans_blend;
            } else {
                /* This one does no blending since there is no tile overlap */
                ptile->ttrans->pat_trans_fill = &tile_rect_trans_simple;
            }
            /* Push the group */
            group_rect.p.x = x;
            group_rect.p.y = max(0,y);
            group_rect.q.x = x + w;
            group_rect.q.y = y + h;
            if (!(w <= 0 || h <= 0)) {

                pdf14_group_color_t *group_color_info = pdf14_clone_group_color_info((gx_device *) p14dev, p14dev->ctx->stack->group_color_info);
                if (group_color_info == NULL)
                    return gs_error_VMerror;

                code = pdf14_push_transparency_group(p14dev->ctx, &group_rect,
                     1, 0, 65535, 65535, 65535, ptile->blending_mode, 0, 0,
                     ptile->ttrans->n_chan-1, false, false, NULL, NULL,
                     group_color_info, NULL, NULL);
                if (code < 0)
                    return code;
                /* Set up the output buffer information now that we have
                   pushed the group */
                fill_trans_buffer = new_pattern_trans_buff(p14dev->memory);
                pdf14_get_buffer_information((gx_device *) p14dev,
                                              fill_trans_buffer, NULL, false);
                /* Store this in the appropriate place in pdcolor.  This
                   is released later after the mask fill */
                ptile->ttrans->fill_trans_buffer = fill_trans_buffer;
                has_pattern_trans = true;
            }
        }
    }
    if (pcpath != 0) {
        gx_make_clip_device_on_stack(&cdev, pcpath, orig_dev);
        dev = (gx_device *) & cdev;
    } else
        dev = orig_dev;
    if (depth > 1) {
        /****** CAN'T DO ROP OR HALFTONE WITH ALPHA ******/
        code = (*dev_proc(dev, copy_alpha))
            (dev, data, dx, raster, id, x, y, w, h,
             gx_dc_pure_color(pdcolor), depth);
    } else {
        code = pdcolor->type->fill_masked(pdcolor, data, dx, raster, id,
                                          x, y, w, h, dev, lop, false);
    }
    if (has_pattern_trans) {
        if (code >= 0)
            code = dev_proc(dev, get_profile)(dev,  &dev_profile);
        if (code >= 0)
            code = pdf14_pop_transparency_group(NULL, p14dev->ctx,
                                                p14dev->blend_procs,
                                                p14dev->color_info.num_components,
                                                dev_profile->device_profile[GS_DEFAULT_DEVICE_PROFILE],
                                                orig_dev);
        gs_free_object(p14dev->memory, ptile->ttrans->fill_trans_buffer,
                       "pdf14_fill_mask");
        ptile->ttrans->fill_trans_buffer = NULL;  /* Avoid GC issues */
    }
    return code;
}



/* Used for filling rects when we are doing a fill with a pattern that
   has transparency */
static	int
pdf14_tile_pattern_fill(gx_device * pdev, const gs_gstate * pgs,
                        gx_path * ppath, const gx_fill_params * params,
                        const gx_device_color * pdevc,
                        const gx_clip_path * pcpath)
{
    int code;
    gs_gstate *pgs_noconst = (gs_gstate *)pgs; /* Break const. */
    gs_fixed_rect clip_box;
    gs_fixed_rect outer_box;
    pdf14_device * p14dev = (pdf14_device *)pdev;
    gs_int_rect rect;
    gx_clip_rect *curr_clip_rect;
    gx_color_tile *ptile = NULL;
    int k;
    gx_pattern_trans_t *fill_trans_buffer = NULL;
    gs_int_point phase;  /* Needed during clist rendering for band offset */
    int n_chan_tile;
    gx_clip_path cpath_intersection;
    gx_path path_ttrans;
    gs_blend_mode_t blend_mode;
    pdf14_group_color_t *group_color_info;

    if (ppath == NULL)
        return_error(gs_error_unknownerror);	/* should not happen */
    if (pcpath != NULL) {
        code = gx_cpath_init_local_shared_nested(&cpath_intersection, pcpath, ppath->memory, 1);
    } else {
        (*dev_proc(pdev, get_clipping_box)) (pdev, &clip_box);
        gx_cpath_init_local(&cpath_intersection, ppath->memory);
        code = gx_cpath_from_rectangle(&cpath_intersection, &clip_box);
    }
    if (code < 0)
        return code;
    code = gx_cpath_intersect_with_params(&cpath_intersection, ppath,
                                          params->rule, pgs_noconst, params);
    if (code < 0)
        return code;
    /* One (common) case worth optimising for is where we have a pattern that
     * is positioned such that only one repeat of the tile is actually
     * visible. In this case, we can restrict the size of the blending group
     * we need to produce to be that of the actual area of the tile that is
     * used. */
    ptile = pdevc->colors.pattern.p_tile;
    if (ptile->ttrans != NULL)
    {
        if ((cpath_intersection.outer_box.p.x < 0) ||
            (cpath_intersection.outer_box.p.y < 0) ||
            (cpath_intersection.outer_box.q.x > int2fixed(ptile->ttrans->width)) ||
            (cpath_intersection.outer_box.q.y > int2fixed(ptile->ttrans->height)))
        {
            /* More than one repeat of the tile would be visible, so we can't
             * use the optimisation here. (Actually, this test isn't quite
             * right - it actually tests whether more than the '0th' repeat
             * of the tile is visible. A better test would test if just one
             * repeat of the tile was visible, irrespective of which one.
             * This is (hopefully) relatively rare, and would make the code
             * below more complex too, so we're ignoring that for now. If it
             * becomes evident that it's a case that matters we can revisit
             * it.) */
        } else {
            /* Only the 0th repeat is visible. Restrict the size further to
             * just the used area of that patch. */
            gx_path_init_local(&path_ttrans, ppath->memory);
            code = gx_path_add_rectangle(&path_ttrans,
                                         int2fixed(ptile->ttrans->rect.p.x),
                                         int2fixed(ptile->ttrans->rect.p.y),
                                         int2fixed(ptile->ttrans->rect.q.x),
                                         int2fixed(ptile->ttrans->rect.q.y));
            if (code < 0)
                return code;
            code = gx_cpath_intersect(&cpath_intersection, &path_ttrans,
                                      params->rule, pgs_noconst);
            gx_path_free(&path_ttrans, "pdf14_tile_pattern_fill(path_ttrans)");
            if (code < 0)
                return code;
        }
    }
    /* Now let us push a transparency group into which we are
     * going to tile the pattern.  */
    if (ppath != NULL) {
        pdf14_device save_pdf14_dev;		/* save area for p14dev */

        gx_cpath_outer_box(&cpath_intersection, &outer_box);
        rect.p.x = fixed2int(outer_box.p.x);
        rect.p.y = fixed2int(outer_box.p.y);
        rect.q.x = fixed2int_ceiling(outer_box.q.x);
        rect.q.y = fixed2int_ceiling(outer_box.q.y);

        /* The color space of this group must be the same as that of the
           tile.  Then when we pop the group, if there is a mismatch between
           the tile color space and the current context we will do the proper
           conversion.  In this way, we ensure that if the tile has any overlapping
           occuring it will be blended in the proper manner i.e in the tile
           underlying color space. */
        if (ptile->cdev == NULL) {
            if (ptile->ttrans == NULL)
                return_error(gs_error_unknownerror);	/* should not happen */
            n_chan_tile = ptile->ttrans->n_chan;
        } else {
            n_chan_tile = ptile->cdev->common.color_info.num_components+1;
        }
        blend_mode = ptile->blending_mode;
        memcpy(&save_pdf14_dev, p14dev, sizeof(pdf14_device));

        group_color_info = pdf14_clone_group_color_info(pdev, p14dev->ctx->stack->group_color_info);
        if (group_color_info == NULL)
            return gs_error_VMerror;

        code = pdf14_push_transparency_group(p14dev->ctx, &rect, 1, 0, (uint16_t)floor(65535 * p14dev->alpha + 0.5),
                                             (uint16_t)floor(65535 * p14dev->shape + 0.5), (uint16_t)floor(65535 * p14dev->opacity + 0.5),
                                              blend_mode, 0, 0, n_chan_tile - 1, false, false,
                                              NULL, NULL, group_color_info, pgs_noconst, pdev);
        if (code < 0)
            return code;

        /* Set the blending procs and the is_additive setting based
           upon the number of channels */
        if (ptile->cdev == NULL) {
            if (n_chan_tile-1 < 4) {
                ptile->ttrans->blending_procs = &rgb_blending_procs;
                ptile->ttrans->is_additive = true;
            } else {
                ptile->ttrans->blending_procs = &cmyk_blending_procs;
                ptile->ttrans->is_additive = false;
            }
        }
        /* Now lets go through the rect list and fill with the pattern */
        /* First get the buffer that we will be filling */
        if (ptile->cdev == NULL) {
            fill_trans_buffer = new_pattern_trans_buff(pgs->memory);
            pdf14_get_buffer_information(pdev, fill_trans_buffer, NULL, false);
            /* Based upon if the tiles overlap pick the type of rect fill that we will
               want to use */
            if (ptile->has_overlap) {
                /* This one does blending since there is tile overlap */
                ptile->ttrans->pat_trans_fill = &tile_rect_trans_blend;
            } else {
                /* This one does no blending since there is no tile overlap */
                ptile->ttrans->pat_trans_fill = &tile_rect_trans_simple;
            }
            /* fill the rectangles */
            phase.x = pdevc->phase.x;
            phase.y = pdevc->phase.y;
            if (cpath_intersection.rect_list->list.head != NULL){
                curr_clip_rect = cpath_intersection.rect_list->list.head->next;
                for( k = 0; k < cpath_intersection.rect_list->list.count && code >= 0; k++){
                    if_debug5m('v', pgs->memory,
                               "[v]pdf14_tile_pattern_fill, (%d, %d), %d x %d pat_id %d \n",
                               curr_clip_rect->xmin, curr_clip_rect->ymin,
                               curr_clip_rect->xmax-curr_clip_rect->xmin,
                               curr_clip_rect->ymax-curr_clip_rect->ymin, (int)ptile->id);
                    code = gx_trans_pattern_fill_rect(curr_clip_rect->xmin, curr_clip_rect->ymin,
                                                      curr_clip_rect->xmax, curr_clip_rect->ymax, ptile,
                                                      fill_trans_buffer, phase, pdev, pdevc, 1);
                    curr_clip_rect = curr_clip_rect->next;
                }
            } else if (cpath_intersection.rect_list->list.count == 1) {
                /* The case when there is just a single rect */
                if_debug5m('v', pgs->memory,
                           "[v]pdf14_tile_pattern_fill, (%d, %d), %d x %d pat_id %d \n",
                           cpath_intersection.rect_list->list.single.xmin,
                           cpath_intersection.rect_list->list.single.ymin,
                           cpath_intersection.rect_list->list.single.xmax-
                              cpath_intersection.rect_list->list.single.xmin,
                           cpath_intersection.rect_list->list.single.ymax-
                              cpath_intersection.rect_list->list.single.ymin,
                           (int)ptile->id);
                code = gx_trans_pattern_fill_rect(cpath_intersection.rect_list->list.single.xmin,
                                                  cpath_intersection.rect_list->list.single.ymin,
                                                  cpath_intersection.rect_list->list.single.xmax,
                                                  cpath_intersection.rect_list->list.single.ymax,
                                                  ptile, fill_trans_buffer, phase, pdev, pdevc, 1);
            }
        } else {
            /* Clist pattern with transparency.  Create a clip device from our
               cpath_intersection.  The above non-clist case could probably be
               done this way too, which will reduce the amount of code here.
               That is for another day though due to time constraints*/
            gx_device *dev;
            gx_device_clip clipdev;

            gx_make_clip_device_on_stack(&clipdev, &cpath_intersection, pdev);
            dev = (gx_device *)&clipdev;
            phase.x = pdevc->phase.x;
            phase.y = pdevc->phase.y;
            code = gx_trans_pattern_fill_rect(rect.p.x, rect.p.y, rect.q.x, rect.q.y,
                                              ptile, fill_trans_buffer, phase,
                                              dev, pdevc, 1);

        }
        /* We're done drawing with the pattern, remove the reference to the
         * pattern device
         */
        p14dev->pclist_device = NULL;
        if (code < 0)
            return code;

        /* free our buffer object */
        if (fill_trans_buffer != NULL) {
            gs_free_object(pgs->memory, fill_trans_buffer, "pdf14_tile_pattern_fill");
            ptile->ttrans->fill_trans_buffer = NULL;  /* Avoid GC issues */
        }
        /* pop our transparency group which will force the blending.
           This was all needed for Bug 693498 */
        code = pdf14_pop_transparency_group(pgs_noconst, p14dev->ctx,
                                            p14dev->blend_procs,
                                            p14dev->color_info.num_components,
                                            p14dev->icc_struct->device_profile[GS_DEFAULT_DEVICE_PROFILE],
                                            pdev);
        memcpy(p14dev, &save_pdf14_dev, sizeof(pdf14_device));
        p14dev->pclist_device = NULL;
    }
    gx_cpath_free(&cpath_intersection, "pdf14_tile_pattern_fill");
    return code;
}

/* Useful function that should probably go elsewhere.
 * Call this function to find the topmost pdf14 device in the device chain,
 * or NULL if there is not one.
 */
static pdf14_device *find_pdf14_device(gx_device *dev)
{
    pdf14_device *pdev;

    if (dev_proc(dev, dev_spec_op)(dev, gxdso_is_pdf14_device, &pdev, sizeof(pdev)) <= 0)
        return NULL;
    return pdev;
}

/* Imager render for pattern transparency filling.  This is just here to catch
   the final flush, at which time we will pop the group and reset a few items */
static	int
pdf14_pattern_trans_render(gx_image_enum * penum, const byte * buffer, int data_x,
                    uint w, int h, gx_device * dev)
{
    int code;
    pdf14_device * p14dev;
    const gs_gstate * pgs = penum->pgs;
    gx_device_color * pdcolor = (penum->icolor1);
    gx_color_tile *ptile = pdcolor->colors.pattern.p_tile;

    /* Pass along to the original renderer */
    code = (ptile->ttrans->image_render)(penum, buffer, data_x, w, h, dev);
    if (code < 0)
        return code;
    /* On our final time through here, go ahead and pop the transparency
       group and reset the procs in the device color. And free the fill
       trans buffer object */
    if (h == 0 && ptile->trans_group_popped == false) {
        p14dev = find_pdf14_device(dev);

        if (p14dev->pclist_device == NULL) {
            /* Used if we are on clist writing phase.  Would only
               occur if we somehow failed in high level clist
               image writing */
            code = gs_end_transparency_group((gs_gstate *) pgs);
        } else {
            /* Used if we are on clist reading phase.  If we had high level
               image in clist */
            cmm_dev_profile_t *dev_profile;
            code = dev_proc(dev, get_profile)(dev,  &dev_profile);
            if (code < 0)
                return code;

            if_debug2m('v', p14dev->ctx->memory,
                      "[v*] Popping trans group pattern fill, uid = %ld id = %ld \n",
                       ptile->uid.id, ptile->id);
            code = pdf14_pop_transparency_group(NULL, p14dev->ctx, p14dev->blend_procs,
                    p14dev->color_info.num_components,
                    dev_profile->device_profile[GS_DEFAULT_DEVICE_PROFILE],
                    (gx_device *) p14dev);
        }
        pdcolor->colors.pattern.p_tile->trans_group_popped = true;
        gs_free_object(pgs->memory, ptile->ttrans->fill_trans_buffer,
                       "pdf14_pattern_trans_render");
        ptile->ttrans->fill_trans_buffer = NULL;  /* Avoid GC issues */
    }
    return code;
}

/* This function is used to get things in place for filling a mask image
   with a pattern that has transparency.  It is used by pdf14_begin_type_image
   and pdf14_clist_begin_type_image */
static int
pdf14_patt_trans_image_fill(gx_device * dev, const gs_gstate * pgs,
                           const gs_matrix *pmat, const gs_image_common_t *pic,
                           const gs_int_rect * prect,
                           const gx_drawing_color * pdcolor,
                           const gx_clip_path * pcpath, gs_memory_t * mem,
                           gx_image_enum_common_t ** pinfo)
{
    const gs_image_t *pim = (const gs_image_t *)pic;
    pdf14_device * p14dev = (pdf14_device *)dev;
    gx_color_tile *ptile;
    int code;
    gs_int_rect group_rect;
    gx_image_enum *penum;
    gs_rect bbox_in, bbox_out;
    gx_pattern_trans_t *fill_trans_buffer;

    ptile = pdcolor->colors.pattern.p_tile;
    /* Set up things in the ptile so that we get the proper
       blending etc */
    /* Set the blending procs and the is_additive setting based
       upon the number of channels */
    if (ptile->ttrans->n_chan-1 < 4) {
        ptile->ttrans->blending_procs = &rgb_blending_procs;
        ptile->ttrans->is_additive = true;
    } else {
        ptile->ttrans->blending_procs = &cmyk_blending_procs;
        ptile->ttrans->is_additive = false;
    }
    /* Set the blending mode in the ptile based upon the current
       setting in the gs_gstate */
    ptile->blending_mode = pgs->blend_mode;
    /* Based upon if the tiles overlap pick the type of rect
       fill that we will want to use */
    if (ptile->has_overlap) {
        /* This one does blending since there is tile overlap */
        ptile->ttrans->pat_trans_fill = &tile_rect_trans_blend;
    } else {
        /* This one does no blending since there is no tile overlap */
        ptile->ttrans->pat_trans_fill = &tile_rect_trans_simple;
    }
    /* Set the procs so that we use the proper filling method. */
    gx_set_pattern_procs_trans((gx_device_color*) pdcolor);
    /* Let the imaging stuff get set up */
    code = gx_default_begin_typed_image(dev, pgs, pmat, pic,
                            prect, pdcolor,pcpath, mem, pinfo);
    if (code < 0)
        return code;
    /* Now Push the group */
    /* First apply the inverse of the image matrix to our
       image size to get our bounding box. */
    bbox_in.p.x = 0;
    bbox_in.p.y = 0;
    bbox_in.q.x = pim->Width;
    bbox_in.q.y = pim->Height;
    code = gs_bbox_transform_inverse(&bbox_in, &(pim->ImageMatrix),
                                &bbox_out);
    if (code < 0)
        return code;
    /* That in turn will get hit by the matrix in the gs_gstate */
    code = compute_group_device_int_rect(p14dev, &group_rect,
                                            &bbox_out, (gs_gstate *)pgs);
    if (code < 0)
        return code;
    if (!(pim->Width == 0 || pim->Height == 0)) {
        if_debug2m('v', p14dev->ctx->memory,
                   "[v*] Pushing trans group patt_trans_image_fill, uid = %ld id = %ld \n",
                   ptile->uid.id, ptile->id);

        code = pdf14_push_transparency_group(p14dev->ctx, &group_rect, 1, 0, 65535, 65535,
                                             65535, pgs->blend_mode, 0, 0,
                                             ptile->ttrans->n_chan-1, false, false,
                                             NULL, NULL, NULL, (gs_gstate *)pgs, dev);

        /* Set up the output buffer information now that we have
           pushed the group */
        fill_trans_buffer = new_pattern_trans_buff(pgs->memory);
        pdf14_get_buffer_information(dev, fill_trans_buffer, NULL, false);

        /* Store this in the appropriate place in pdcolor.  This
           is released later in pdf14_pattern_trans_render when
           we are all done with the mask fill */
        ptile->ttrans->fill_trans_buffer = fill_trans_buffer;

        /* Change the renderer to handle this case so we can catch the
           end.  We will then pop the group and reset the pdcolor proc.
           Keep the base renderer also. */
        penum = (gx_image_enum *) *pinfo;
        ptile->ttrans->image_render = penum->render;
        penum->render = &pdf14_pattern_trans_render;
        ptile->trans_group_popped = false;
    }
    return code;
}

static	int
pdf14_begin_typed_image(gx_device * dev, const gs_gstate * pgs,
                           const gs_matrix *pmat, const gs_image_common_t *pic,
                           const gs_int_rect * prect,
                           const gx_drawing_color * pdcolor,
                           const gx_clip_path * pcpath, gs_memory_t * mem,
                           gx_image_enum_common_t ** pinfo)
{
    const gs_image_t *pim = (const gs_image_t *)pic;
    int code;

    code = pdf14_initialize_ctx(dev, dev->color_info.num_components,
        dev->color_info.polarity != GX_CINFO_POLARITY_SUBTRACTIVE, pgs);
    if (code < 0)
        return code;

    /* If we are filling an image mask with a pattern that has a transparency
       then we need to do some special handling */
    if (pim->ImageMask) {
        if (pdcolor != NULL && gx_dc_is_pattern1_color(pdcolor)) {
            if( gx_pattern1_get_transptr(pdcolor) != NULL){
                /* If we are in a final run through here for this case then
                   go ahead and push the transparency group.   Also, update
                   the proc for the pattern color so that we used the
                   appropriate fill operation.  Note that the group
                   is popped and the proc will be reset when we flush the
                   image data.  This is handled in a special pdf14 image
                   renderer which will end up installed for this case.
                   Detect setting of begin_image to gx_no_begin_image.
                   (final recursive call) */
                if (dev_proc(dev, begin_typed_image) != gx_default_begin_typed_image) {
                    code = pdf14_patt_trans_image_fill(dev, pgs, pmat, pic,
                                                prect, pdcolor, pcpath, mem,
                                                pinfo);
                    return code;
                }
            }
        }
    }
    pdf14_set_marking_params(dev, pgs);
    return gx_default_begin_typed_image(dev, pgs, pmat, pic, prect, pdcolor,
                                        pcpath, mem, pinfo);
}

static	void
pdf14_set_params(gs_gstate * pgs,
                 gx_device * dev,
                 const gs_pdf14trans_params_t * pparams)
{
    if_debug0m('v', dev->memory, "[v]pdf14_set_params\n");
    if (pparams->changed & PDF14_SET_BLEND_MODE)
        pgs->blend_mode = pparams->blend_mode;
    if (pparams->changed & PDF14_SET_TEXT_KNOCKOUT)
        pgs->text_knockout = pparams->text_knockout;
    if (pparams->changed & PDF14_SET_AIS)
        pgs->alphaisshape = pparams->ais;
    if (pparams->changed & PDF14_SET_OVERPRINT)
        pgs->overprint = pparams->overprint;
    if (pparams->changed & PDF14_SET_STROKEOVERPRINT)
        pgs->stroke_overprint = pparams->stroke_overprint;
    if (pparams->changed & PDF14_SET_FILLCONSTANTALPHA)
        pgs->fillconstantalpha = pparams->fillconstantalpha;
    if (pparams->changed & PDF14_SET_STROKECONSTANTALPHA)
        pgs->strokeconstantalpha = pparams->strokeconstantalpha;
    if (pparams->changed & PDF_SET_FILLSTROKE_STATE) {
        gs_swapcolors_quick(pgs);
        if (pparams->op_fs_state == PDF14_OP_STATE_STROKE)
            pgs->is_fill_color = false;
        else
            pgs->is_fill_color = true;
    }
    pdf14_set_marking_params(dev, pgs);
}

/*
 * This open_device method for the PDF 1.4 compositor devices is only used
 * when these devices are disabled.  This routine is about as close to
 * a pure "forwarding" open_device operation as is possible. Its only
 * significant function is to ensure that the is_open field of the
 * PDF 1.4 compositor devices matches that of the target device.
 *
 * We assume this procedure is called only if the device is not already
 * open, and that gs_opendevice will take care of the is_open flag.
 */
static	int
pdf14_forward_open_device(gx_device * dev)
{
    gx_device_forward * pdev = (gx_device_forward *)dev;
    gx_device * tdev = pdev->target;
    int code;

    /* The PDF 1.4 compositing devices must have a target */
    if (tdev == 0)
        return_error(gs_error_unknownerror);
    if ((code = gs_opendevice(tdev)) >= 0)
        gx_device_copy_params(dev, tdev);
    return code;
}

/*
 * Convert all device procs to be 'forwarding'.  The caller is responsible
 * for setting any device procs that should not be forwarded.
 */
static	void
pdf14_forward_device_procs(gx_device * dev)
{
    gx_device_forward *pdev = (gx_device_forward *)dev;
    pdf14_device *p14dev = (pdf14_device*)dev;

    /* If doing simulated overprint with spot colors
       then makes sure to reset devn setting */
    if (p14dev->overprint_sim &&
        p14dev->color_info.num_components > 4)
        p14dev->icc_struct->supports_devn =
            p14dev->target_support_devn;

    /*
     * We are using gx_device_forward_fill_in_procs to set the various procs.
     * This will ensure that any new device procs are also set.  However that
     * routine only changes procs which are NULL.  Thus we start by setting all
     * procs to NULL.
     */
    memset(&(pdev->procs), 0, size_of(pdev->procs));
    gx_device_forward_fill_in_procs(pdev);
    /*
     * gx_device_forward_fill_in_procs does not forward all procs.
     * Set the remainding procs to also forward.
     */
    set_dev_proc(dev, close_device, gx_forward_close_device);
    set_dev_proc(dev, fill_rectangle, gx_forward_fill_rectangle);
    set_dev_proc(dev, fill_rectangle_hl_color, gx_forward_fill_rectangle_hl_color);
    set_dev_proc(dev, copy_mono, gx_forward_copy_mono);
    set_dev_proc(dev, copy_color, gx_forward_copy_color);
    set_dev_proc(dev, get_page_device, gx_forward_get_page_device);
    set_dev_proc(dev, strip_tile_rectangle, gx_forward_strip_tile_rectangle);
    set_dev_proc(dev, copy_alpha, gx_forward_copy_alpha);
    set_dev_proc(dev, get_profile, gx_forward_get_profile);
    set_dev_proc(dev, set_graphics_type_tag, gx_forward_set_graphics_type_tag);
    /* These are forwarding devices with minor tweaks. */
    set_dev_proc(dev, open_device, pdf14_forward_open_device);
    set_dev_proc(dev, put_params, pdf14_forward_put_params);
}

/*
 * Disable the PDF 1.4 compositor device.  Once created, the PDF 1.4
 * compositor device is never removed.  (We do not have a remove compositor
 * method.)  However it is no-op'ed when the PDF 1.4 device is popped.  This
 * routine implements that action.
 */
int
pdf14_disable_device(gx_device * dev)
{
    gx_device_forward * pdev = (gx_device_forward *)dev;

    if_debug0m('v', dev->memory, "[v]pdf14_disable_device\n");
    dev->color_info = pdev->target->color_info;
    pdf14_forward_device_procs(dev);
    set_dev_proc(dev, composite, pdf14_forward_composite);
    return 0;
}

/*
 * The default color space for PDF 1.4 blend modes is based upon the process
 * color model of the output device.
 */
static	pdf14_default_colorspace_t
pdf14_determine_default_blend_cs(gx_device * pdev, bool use_pdf14_accum,
                                 bool *using_blend_cs)
{
    /* If a blend color space was specified, then go ahead and use that to
       define the default color space for the blend modes.  Only Gray, RGB
       or CMYK blend color spaces are allowed.  Note we do not allow this
       setting if we are dealing with a separation device. */
    cmm_dev_profile_t *dev_profile;
    int code = dev_proc(pdev, get_profile)(pdev, &dev_profile);
    bool valid_blend_cs = false;
    *using_blend_cs = false;

    /* Make sure any specified blend color space is valid along with other cond */
    if (code == 0 && dev_profile->blend_profile != NULL && !use_pdf14_accum) {
        if (!dev_profile->blend_profile->isdevlink &&
            !dev_profile->blend_profile->islab &&
            (dev_profile->blend_profile->data_cs == gsGRAY ||
             dev_profile->blend_profile->data_cs == gsRGB ||
             dev_profile->blend_profile->data_cs == gsCMYK)) {
            /* Also, do not allow the use of the blend space when we are pushing
               a pattern pdf14 device.  Those should inherit from the parent */
            if (!(gx_device_is_pattern_clist(pdev) ||
                  gx_device_is_pattern_accum(pdev))) {
                valid_blend_cs = true;
            }
        }
    }

    /* If num components is one, just go ahead and use gray.  This avoids
       issues with additive/subtractive mono color devices  */
    if (pdev->color_info.polarity == GX_CINFO_POLARITY_ADDITIVE ||
        pdev->color_info.num_components == 1) {
        /*
        * Note:  We do not allow the SeparationOrder device parameter for
        * additive devices.  Thus we always have 1 colorant for DeviceGray
        * and 3 colorants for DeviceRGB.
        */
        if (valid_blend_cs) {
            *using_blend_cs = true;
            switch (dev_profile->blend_profile->num_comps) {
            case 1:
                return PDF14_DeviceGray;
            case 3:
                return PDF14_DeviceRGB;
            case 4:
                return PDF14_DeviceCMYK;
            }
        }
        if (pdev->color_info.num_components == 1)
            return PDF14_DeviceGray;
        else
            return PDF14_DeviceRGB;
    } else {
        /*
         * Check if the device is CMYK only or CMYK plus spot colors. Note
         * the CMYK plus spot colors will not support the blend color space
         */
        int i, output_comp_num, num_cmyk_used = 0, num_cmyk = 0;
#if CUSTOM_BLENDING_MODE == ALWAYS_USE_CUSTOM_BLENDING
        return PDF14_DeviceCustom;
#endif
        /*
         * Count the number of CMYK process components supported by the output
         * device.
         */
        for (i = 0; i < 4; i++) {
            const char * pcomp_name = (const char *)DeviceCMYKComponents[i];

            output_comp_num = dev_proc(pdev, get_color_comp_index)
                (pdev, pcomp_name, strlen(pcomp_name), NO_COMP_NAME_TYPE_OP);
            if (output_comp_num >= 0) {
                num_cmyk++;
                if (output_comp_num != GX_DEVICE_COLOR_MAX_COMPONENTS)
                    num_cmyk_used++;
            }
        }
        /*
         * Check if the device supports only CMYK.  Otherewise we assume that
         * the output device supports spot colors.  Note:  This algorithm can
         * be fooled if the SeparationOrder device parameter is being used by
         * the output device device to only select CMYK.
         */
        if (num_cmyk_used == 4 && pdev->color_info.num_components == 4
            && pdev->color_info.max_components == 4) {
            if (valid_blend_cs) {
                *using_blend_cs = true;
                switch (dev_profile->blend_profile->num_comps) {
                case 1:
                    return PDF14_DeviceGray;
                case 3:
                    return PDF14_DeviceRGB;
                case 4:
                    return PDF14_DeviceCMYK;
                }
            }
            return PDF14_DeviceCMYK;
        }
        /*
         * Check if we should use the 'custom' PDF 1.4 compositor device.
         * This device is only needed for those devices which do not support
         * a basic CMYK process color model.
         */
#if CUSTOM_BLENDING_MODE == AUTO_USE_CUSTOM_BLENDING
        if (num_cmyk != 4)
            return PDF14_DeviceCustom;
#endif
        /*
         * Otherewise we use a CMYK plus spot colors for blending.
         */
        return PDF14_DeviceCMYKspot;
    }
}

/*
 * the PDF 1.4 transparency spec says that color space for blending
 * operations can be based upon either a color space specified in the
 * group or a default value based upon the output device.  We are
 * currently only using a color space based upon the device.
 */
static	int
get_pdf14_device_proto(gx_device       *dev,
                       pdf14_device    *pdevproto,
                       gs_gstate       *pgs,
                 const gs_pdf14trans_t *pdf14pct,
                       bool             use_pdf14_accum)
{
    bool using_blend_cs;
    pdf14_default_colorspace_t dev_cs =
                pdf14_determine_default_blend_cs(dev, use_pdf14_accum,
                                                 &using_blend_cs);
    bool deep = device_is_deep(dev);
    int num_spots = pdf14pct->params.num_spot_colors;

    /* overprint overide */
    if (pdf14pct->params.overprint_sim_push) {
        using_blend_cs = false;
        if (pdf14pct->params.num_spot_colors_int > 0) {
            dev_cs = PDF14_DeviceCMYKspot;
            num_spots = pdf14pct->params.num_spot_colors_int;
        } else
            dev_cs = PDF14_DeviceCMYK;
    }

    switch (dev_cs) {
        case PDF14_DeviceGray:
            *pdevproto = gs_pdf14_Gray_device;
            pdevproto->color_info.max_components = 1;
            pdevproto->color_info.num_components =
                                    pdevproto->color_info.max_components;
            pdevproto->color_info.depth = 8<<deep;
            pdevproto->color_info.max_gray = deep ? 65535 : 255;
            pdevproto->color_info.gray_index = 0; /* Avoid halftoning */
            pdevproto->color_info.dither_grays = deep ? 65536 : 256;
            pdevproto->sep_device = false;
            break;
        case PDF14_DeviceRGB:
            *pdevproto = gs_pdf14_RGB_device;
            pdevproto->color_info.depth = 24<<deep;
            pdevproto->color_info.max_gray = deep ? 65535 : 255;
            pdevproto->color_info.dither_grays = deep ? 65536 : 256;
            pdevproto->sep_device = false;
            break;
        case PDF14_DeviceCMYK:
            *pdevproto = gs_pdf14_CMYK_device;
            pdevproto->color_info.depth = 32<<deep;
            pdevproto->color_info.max_gray = deep ? 65535 : 255;
            pdevproto->color_info.dither_grays = deep ? 65536 : 256;
            pdevproto->sep_device = false;
            break;
        case PDF14_DeviceCMYKspot:
            *pdevproto = gs_pdf14_CMYKspot_device;
            /* Need to figure out how we want to handle the device profile
               for this case */
            /*
             * The number of components for the PDF14 device is the sum
             * of the process components and the number of spot colors
             * for the page.
             */
            if (num_spots >= 0) {
                pdevproto->devn_params.page_spot_colors = num_spots;
                pdevproto->color_info.num_components =
                    pdevproto->devn_params.num_std_colorant_names + num_spots;
                if (pdevproto->color_info.num_components > GS_CLIENT_COLOR_MAX_COMPONENTS)
                    pdevproto->color_info.num_components = GS_CLIENT_COLOR_MAX_COMPONENTS;
                pdevproto->color_info.depth =
                                    pdevproto->color_info.num_components * (8<<deep);
                pdevproto->sep_device = true;
            }
            break;
        case PDF14_DeviceCustom:
            /*
             * We are using the output device's process color model.  The
             * color_info for the PDF 1.4 compositing device needs to match
             * the output device.
             */
            *pdevproto = gs_pdf14_custom_device;
            pdevproto->color_info = dev->color_info;
            /* The pdf14 device has to be 8 (or 16) bit continuous tone. Force it */
            pdevproto->color_info.depth =
                       pdevproto->color_info.num_components * (8<<deep);
            pdevproto->color_info.max_gray = deep ? 65535 : 255;
            pdevproto->color_info.max_color = deep ? 65535 : 255;
            pdevproto->color_info.dither_grays = deep ? 65536 : 256;
            pdevproto->color_info.dither_colors = deep ? 65536 : 256;
            break;
        default:			/* Should not occur */
            return_error(gs_error_rangecheck);
    }
    pdevproto->initialize_device_procs((gx_device *)pdevproto);
    pdevproto->using_blend_cs = using_blend_cs;
    pdevproto->overprint_sim = pdf14pct->params.overprint_sim_push;
    return 0;
}

/* When playing back the clist, we need to know if the buffer device is compatible */
/* with the pdf14 compositor that was used when writing the clist. Colorspace and  */
/* depth are critical since these must match when reading back colors.             */
bool
pdf14_ok_to_optimize(gx_device *dev)
{
    bool using_blend_cs;
    pdf14_default_colorspace_t pdf14_cs =
        pdf14_determine_default_blend_cs(dev, false, &using_blend_cs);
    gsicc_colorbuffer_t dev_icc_cs;
    bool ok = false;
    int tag_depth = device_encodes_tags(dev) ? 8 : 0;
    cmm_dev_profile_t *dev_profile;
    int code = dev_proc(dev, get_profile)(dev,  &dev_profile);
    bool deep = device_is_deep(dev);

    if (code < 0)
        return false;

    check_device_compatible_encoding(dev);

    if (dev->color_info.separable_and_linear != GX_CINFO_SEP_LIN_STANDARD)
        return false;

    dev_icc_cs = dev_profile->device_profile[GS_DEFAULT_DEVICE_PROFILE]->data_cs;
    /* If the outputprofile is not "standard" then colors converted to device color */
    /* during clist writing won't match the colors written for the pdf14 clist dev  */
    if (!(dev_icc_cs == gsGRAY || dev_icc_cs == gsRGB || dev_icc_cs == gsCMYK))
        return false;                           /* can't handle funky output profiles */

    switch (pdf14_cs) {
        case PDF14_DeviceGray:
            ok = dev->color_info.max_gray == (deep ? 65535 : 255) && dev->color_info.depth == (8<<deep) + tag_depth;
            break;
        case PDF14_DeviceRGB:
            ok = dev->color_info.max_color == (deep ? 65535: 255) && dev->color_info.depth == (24<<deep) + tag_depth;
            break;
        case PDF14_DeviceCMYK:
            ok = dev->color_info.max_color == (deep ? 65535 : 255) && dev->color_info.depth == (32<<deep) + tag_depth;
            break;
        case PDF14_DeviceCMYKspot:
            ok = false;			/* punt for this case */
            break;
        case PDF14_DeviceCustom:
            /*
             * We are using the output device's process color model.  The
             * color_info for the PDF 1.4 compositing device needs to match
             * the output device, but it may not have been contone.
             */
            ok = dev->color_info.depth == dev->color_info.num_components * (8<<deep) + tag_depth;
            break;
        default:			/* Should not occur */
            ok = false;
    }
    return ok;
}

/*
 * Recreate the PDF 1.4 compositor device.  Once created, the PDF 1.4
 * compositor device is never removed.  (We do not have a remove compositor
 * method.)  However it is no-op'ed when the PDF 1.4 device is popped.  This
 * routine will re-enable the compositor if the PDF 1.4 device is pushed
 * again.
 */
static	int
pdf14_recreate_device(gs_memory_t *mem,	gs_gstate	* pgs,
                gx_device * dev, const gs_pdf14trans_t * pdf14pct)
{
    pdf14_device * pdev = (pdf14_device *)dev;
    gx_device * target = pdev->target;
    pdf14_device dev_proto;
    bool has_tags = device_encodes_tags(dev);
    int code;
    bool deep = device_is_deep(dev);

    if_debug0m('v', dev->memory, "[v]pdf14_recreate_device\n");

    /*
     * We will not use the entire prototype device but we will set the
     * color related info and the device procs to match the prototype.
     */
    code = get_pdf14_device_proto(target, &dev_proto, pgs,
                                  pdf14pct, false);
    if (code < 0)
        return code;
    pdev->color_info = dev_proto.color_info;
    pdev->pad = target->pad;
    pdev->log2_align_mod = target->log2_align_mod;

    if (pdf14pct->params.overprint_sim_push && pdf14pct->params.num_spot_colors_int > 0 && !target->is_planar)
        pdev->is_planar = true;
    else
        pdev->is_planar = target->is_planar;

    pdev->procs = dev_proto.procs;
    if (deep) {
        set_dev_proc(pdev, encode_color, pdf14_encode_color16);
        set_dev_proc(pdev, decode_color, pdf14_decode_color16);
    }
    if (has_tags) {
        set_dev_proc(pdev, encode_color, deep ? pdf14_encode_color16_tag : pdf14_encode_color_tag);
        pdev->color_info.comp_shift[pdev->color_info.num_components] = pdev->color_info.depth;
        pdev->color_info.depth += 8;
    }
    pdev->color_info.separable_and_linear = GX_CINFO_SEP_LIN_STANDARD;
    gx_device_fill_in_procs((gx_device *)pdev);
    pdev->save_get_cmap_procs = pgs->get_cmap_procs;
    pgs->get_cmap_procs = pdf14_get_cmap_procs;
    gx_set_cmap_procs(pgs, (gx_device *)pdev);
    check_device_separable(dev);
    return dev_proc(pdev, open_device)(dev);
}

/*
 * Implement the various operations that can be specified via the PDF 1.4
 * create compositor request.
 */
static	int
gx_update_pdf14_compositor(gx_device * pdev, gs_gstate * pgs,
    const gs_pdf14trans_t * pdf14pct, gs_memory_t * mem )
{
    pdf14_device *p14dev = (pdf14_device *)pdev;
    gs_pdf14trans_params_t params = pdf14pct->params;
    int code = 0;

    params.idle = pdf14pct->idle;
    switch (params.pdf14_op) {
        default:			/* Should not occur. */
            break;
        case PDF14_PUSH_DEVICE:
            if (!(params.is_pattern)) {
                p14dev->blend_mode = 0;
                p14dev->opacity = p14dev->shape = 0.0;
                pdf14_recreate_device(mem, pgs, pdev, pdf14pct);
            }
            break;
        case PDF14_ABORT_DEVICE:
            /* Something has gone very wrong.  Let transparency device clean up
               what ever it has allocated and then we are shutting it down */
            code = gx_abort_trans_device(pgs, pdev);
            if (p14dev->free_devicen) {
                devn_free_params(pdev);
            }
            pdf14_disable_device(pdev);
            pdf14_close(pdev);
            break;
        case PDF14_POP_DEVICE:
            if (!(params.is_pattern)) {
                if_debug0m('v', pdev->memory,
                           "[v]gx_update_pdf14_compositor(PDF14_POP_DEVICE)\n");
                pgs->get_cmap_procs = p14dev->save_get_cmap_procs;
                gx_set_cmap_procs(pgs, p14dev->target);
                /* Send image out raster data to output device */
                {
                    /* Make a copy so we can change the ROP */
                    gs_gstate new_pgs = *pgs;

                    /* We don't use the gs_gstate log_op since this is for the */
                    /* clist playback. Putting the image (band in the case of the */
                    /* clist) only needs to use the default ROP to copy the data  */
                    new_pgs.log_op = rop3_default;
                    code = p14dev->pdf14_procs->put_image(pdev, &new_pgs, p14dev->target);
                }
                /* Before we disable the device release any deviceN structures.
                    free_devicen is set if the pdf14 device had inherited its
                    deviceN parameters from the target clist device.  In this
                    case they should not be freed */
                if (p14dev->free_devicen) {
                    devn_free_params(pdev);
                }
                pdf14_disable_device(pdev);
                pdf14_close(pdev);
            }
            break;
        case PDF14_BEGIN_TRANS_PAGE_GROUP:
        case PDF14_BEGIN_TRANS_GROUP:
            if (p14dev->smask_constructed || p14dev->depth_within_smask)
                p14dev->depth_within_smask++;
            p14dev->smask_constructed = 0;
            code = gx_begin_transparency_group(pgs, pdev, &params);
            break;
        case PDF14_END_TRANS_GROUP:
            code = gx_end_transparency_group(pgs, pdev);
            if (p14dev->depth_within_smask)
                p14dev->depth_within_smask--;
            break;
        case PDF14_BEGIN_TRANS_TEXT_GROUP:
            if (p14dev->text_group == PDF14_TEXTGROUP_BT_PUSHED) {
                p14dev->text_group = PDF14_TEXTGROUP_MISSING_ET;
                emprintf(p14dev->memory, "Warning: Text group pushed but no ET found\n");
            } else
                p14dev->text_group = PDF14_TEXTGROUP_BT_NOT_PUSHED;
            break;
        case PDF14_END_TRANS_TEXT_GROUP:
            if (p14dev->text_group == PDF14_TEXTGROUP_BT_PUSHED)
                code = gx_end_transparency_group(pgs, pdev);
            p14dev->text_group = PDF14_TEXTGROUP_NO_BT; /* Hit ET */
            break;
        case PDF14_BEGIN_TRANS_MASK:
            code = gx_begin_transparency_mask(pgs, pdev, &params);
            if (code >= 0)
                p14dev->in_smask_construction++;
            break;
        case PDF14_END_TRANS_MASK:
            code = gx_end_transparency_mask(pgs, pdev, &params);
            if (code >= 0) {
                p14dev->in_smask_construction--;
                if (p14dev->in_smask_construction < 0)
                    p14dev->in_smask_construction = 0;
                if (p14dev->in_smask_construction == 0)
                    p14dev->smask_constructed = 1;
            }
            break;
        case PDF14_SET_BLEND_PARAMS:
            pdf14_set_params(pgs, pdev, &pdf14pct->params);
            break;
        case PDF14_PUSH_TRANS_STATE:
            code = gx_push_transparency_state(pgs, pdev);
            break;
        case PDF14_POP_TRANS_STATE:
            code = gx_pop_transparency_state(pgs, pdev);
            break;
        case PDF14_PUSH_SMASK_COLOR:
            code = pdf14_increment_smask_color(pgs, pdev);
            break;
        case PDF14_POP_SMASK_COLOR:
            code = pdf14_decrement_smask_color(pgs, pdev);
            break;
    }
    return code;
}

/*
 * The PDF 1.4 compositor is never removed.  (We do not have a 'remove
 * compositor' method.  However the compositor is disabled when we are not
 * doing a page which uses PDF 1.4 transparency.  This routine is only active
 * when the PDF 1.4 compositor is 'disabled'.  It checks for reenabling the
 * PDF 1.4 compositor.  Otherwise it simply passes create compositor requests
 * to the target.
 */
static	int
pdf14_forward_composite(gx_device * dev, gx_device * * pcdev,
        const gs_composite_t * pct, gs_gstate * pgs,
        gs_memory_t * mem, gx_device *cdev)
{
    pdf14_device *pdev = (pdf14_device *)dev;
    gx_device * tdev = pdev->target;
    int code;

    *pcdev = dev;
    if (gs_is_pdf14trans_compositor(pct)) {
        const gs_pdf14trans_t * pdf14pct = (const gs_pdf14trans_t *) pct;

        if (pdf14pct->params.pdf14_op == PDF14_PUSH_DEVICE)
            return gx_update_pdf14_compositor(dev, pgs, pdf14pct, mem);
        return 0;
    }
    code = dev_proc(tdev, composite)(tdev, pcdev, pct, pgs, mem, cdev);
    if (code == 1) {
        /* We have created a new compositor that wrapped tdev. This means
         * that our target should be updated to point to that. */
        gx_device_set_target((gx_device_forward *)pdev, *pcdev);
        code = 0; /* We have not created a new compositor that wrapped dev. */
    }
    return code;
}

/*
 * The PDF 1.4 compositor can be handled directly, so just set *pcdev = dev
 * and return. Since the gs_pdf14_device only supports the high-level routines
 * of the interface, don't bother trying to handle any other compositor.
 */
static int
pdf14_composite(gx_device * dev, gx_device * * pcdev,
        const gs_composite_t * pct, gs_gstate * pgs,
        gs_memory_t * mem, gx_device *cdev)
{
    pdf14_device *p14dev = (pdf14_device *)dev;
    if (gs_is_pdf14trans_compositor(pct)) {
        const gs_pdf14trans_t * pdf14pct = (const gs_pdf14trans_t *) pct;
        *pcdev = dev;
        /* cdev, may be the clist reader device which may contain information that
           we will need related to the ICC color spaces that define transparency
           groups.  We want this propogated through all the pdf14 functions.  Store
           a pointer to it in the pdf14 device */
        p14dev->pclist_device = cdev;
        return gx_update_pdf14_compositor(dev, pgs, pdf14pct, mem);
    } else if (gs_is_overprint_compositor(pct)) {
                /* If we had an overprint compositer action, then the
                   color components that were drawn should be updated.
                   The overprint compositor logic and its interactions
                   with the clist is a little odd as it passes uninitialized
                   values around a fair amount.  Hence the forced assignement here.
                   See gx_spot_colors_set_overprint in gscspace for issues... */
                const gs_overprint_t * op_pct = (const gs_overprint_t *) pct;
                gx_color_index drawn_comps;

                p14dev->op_state = op_pct->params.op_state;


                if (p14dev->op_state == PDF14_OP_STATE_NONE) {
                    if (op_pct->params.retain_any_comps) {
                        drawn_comps = op_pct->params.drawn_comps;
                    } else {
                        /* Draw everything. If this parameter was not set, clist does
                           not fill it in.  */
                        drawn_comps = ((gx_color_index)1 << (p14dev->color_info.num_components)) - (gx_color_index)1;
                    }

                    if (op_pct->params.is_fill_color) {
                        p14dev->effective_overprint_mode = op_pct->params.effective_opm;
                        p14dev->drawn_comps_fill = drawn_comps;
                    } else {
                        p14dev->stroke_effective_op_mode = op_pct->params.effective_opm;
                        p14dev->drawn_comps_stroke = drawn_comps;
                    }

                }
                *pcdev = dev;
                return 0;
    } else
        return gx_no_composite(dev, pcdev, pct, pgs, mem, cdev);
}

static int
pdf14_push_text_group(gx_device *dev, gs_gstate *pgs,
                      gs_blend_mode_t blend_mode, float opacity,
                      float shape, bool is_clist)
{
    int code;
    gs_transparency_group_params_t params = { 0 };
    gs_rect bbox = { 0 }; /* Bounding box is set by parent */
    pdf14_clist_device * pdev = (pdf14_clist_device *)dev;
    float alpha = pgs->fillconstantalpha;

    /* Push a non-isolated knock-out group making sure the opacity and blend
       mode are correct */
    params.Isolated = false;
    params.Knockout = true;
    params.page_group = false;
    params.text_group = PDF14_TEXTGROUP_BT_PUSHED;
    params.group_opacity = 1.0;
    params.group_shape = 1.0;

    gs_setfillconstantalpha(pgs, 1.0);
    gs_setblendmode(pgs, BLEND_MODE_Normal);

    if (is_clist) {
        code = pdf14_clist_update_params(pdev, pgs, false, NULL);
        if (code < 0)
            return code;
    }

    code = gs_begin_transparency_group(pgs, &params, &bbox, PDF14_BEGIN_TRANS_GROUP);
    gs_setfillconstantalpha(pgs, alpha);
    gs_setblendmode(pgs, blend_mode);
    if (code < 0)
        return code;

    if (is_clist) {
        code = pdf14_clist_update_params(pdev, pgs, false, NULL);
    }
    return code;
}

static	int
pdf14_text_begin(gx_device * dev, gs_gstate * pgs,
                 const gs_text_params_t * text, gs_font * font,
                 const gx_clip_path * pcpath,
                 gs_text_enum_t ** ppenum)
{
    int code;
    gs_text_enum_t *penum;
    gs_blend_mode_t blend_mode = gs_currentblendmode(pgs);
    float opacity = pgs->fillconstantalpha;
    float shape = 1.0;
    bool blend_issue = !(blend_mode == BLEND_MODE_Normal || blend_mode == BLEND_MODE_Compatible || blend_mode == BLEND_MODE_CompatibleOverprint);
    pdf14_device *pdev = (pdf14_device*)dev;
    bool draw = !(text->operation & TEXT_DO_NONE);
    uint text_mode = gs_currenttextrenderingmode(pgs);
    bool text_stroke = (text_mode == 1 || text_mode == 2 || text_mode == 5 || text_mode == 6);
    bool text_fill = (text_mode == 0 || text_mode == 2 || text_mode == 4 || text_mode == 6);

    code = pdf14_initialize_ctx(dev, dev->color_info.num_components,
        dev->color_info.polarity != GX_CINFO_POLARITY_SUBTRACTIVE, (const gs_gstate*) pgs);
    if (code < 0)
        return code;

    if_debug0m('v', pgs->memory, "[v]pdf14_text_begin\n");
    pdf14_set_marking_params(dev, pgs);
    code = gx_default_text_begin(dev, pgs, text, font, pcpath, &penum);
    if (code < 0)
        return code;

    /* We may need to push a non-isolated transparency group if the following
       is true.
       1) We are not currently in one that we pushed for text and we are in
          a BT/ET pair.  This is determined by looking at the pdf14 text_group.
       2) The blend mode is not Normal or the opacity is not 1.0
       3) Text knockout is set to true
       4) We are actually doing a text drawing

       Special note:  If text-knockout is set to false while we are within a
       BT ET pair, we should pop the group.  I need to create a test file for
       this case.  */

       /* Catch case where we already pushed a group and are trying to push another one.
       In that case, we will pop the current one first, as we don't want to be left
       with it. Note that if we have a BT and no other BTs or ETs then this issue
       will not be caught until we do the put_image and notice that the stack is not
       empty. */
    if (pdev->text_group == PDF14_TEXTGROUP_MISSING_ET) {
        code = gs_end_transparency_group(pgs);
        if (code < 0)
            return code;
        pdev->text_group = PDF14_TEXTGROUP_BT_NOT_PUSHED;
    }

    if (gs_currenttextknockout(pgs) && (blend_issue ||
         (pgs->fillconstantalpha != 1.0 && text_fill) ||
         (pgs->strokeconstantalpha != 1.0 && text_stroke)) &&
         text_mode != 3 && /* don't bother with invisible text */
         pdev->text_group == PDF14_TEXTGROUP_BT_NOT_PUSHED)
        if (draw) {
            code = pdf14_push_text_group(dev, pgs, blend_mode, opacity, shape,
                false);
        }
    *ppenum = (gs_text_enum_t *)penum;
    return code;
}

static	int
pdf14_initialize_device(gx_device *new_dev)
{
    pdf14_device *pdev = (pdf14_device*)new_dev;

    pdev->ctx = NULL;
    pdev->color_model_stack = NULL;
    pdev->smaskcolor = NULL;

    return 0;
}

/*
 * Implement copy_mono by filling lots of small rectangles.
 */
static int
pdf14_copy_mono(gx_device * dev,
               const byte * base, int sourcex, int sraster, gx_bitmap_id id,
        int x, int y, int w, int h, gx_color_index zero, gx_color_index one)
{
    const byte *sptr;
    const byte *line;
    int sbit, first_bit;
    int code, sbyte, bit, count;
    int run_length, startx, current_bit, bit_value;
    gx_color_index current_color;

    fit_copy(dev, base, sourcex, sraster, id, x, y, w, h);
    line = base + (sourcex >> 3);
    sbit = sourcex & 7;
    first_bit = 7 - sbit;

    /* Loop through the height of the specified area. */
    while (h-- > 0) {
        /* Set up for the start of each line of the area. */
        sptr = line;
        sbyte = *sptr++;
        bit = first_bit;
        count = w;
        run_length = 0;
        startx = x;
        current_bit = 0;
        current_color = zero;

        /* Loop across each pixel of a line. */
        do {
            bit_value = (sbyte >> bit) & 1;
            if (bit_value == current_bit) {
                /* The value did not change, simply increment our run length */
                run_length++;
            } else {
                /* The value changed, fill the current rectangle. */
                if (run_length != 0) {
                    if (current_color != gx_no_color_index) {
                        code = (*dev_proc(dev, fill_rectangle))
                                (dev, startx, y, run_length, 1, current_color);
                        if (code < 0)
                            return code;
                    }
                    startx += run_length;
                }
                run_length = 1;
                current_color = bit_value ? one : zero;
                current_bit = bit_value;
            }
            /* Move to the next input bit. */
            if (bit == 0) {
                bit = 7;
                sbyte = *sptr++;
            }
            else
                bit--;
        } while (--count > 0);
        /* Fill the last rectangle in the line. */
        if (run_length != 0 && current_color != gx_no_color_index) {
            code = (*dev_proc(dev, fill_rectangle))
                        (dev, startx, y, run_length, 1, current_color);
            if (code < 0)
                return code;
        }
        /* Move to the next line */
        line += sraster;
        y++;
    }
    return 0;
}

/* Added to avoid having to go back and forth between fixed and int
   in some of the internal methods used for dealing with tiling
   and devn colors */
static int
pdf14_fill_rectangle_devn(gx_device *dev, int x, int y, int w, int h,
    const gx_drawing_color *pdcolor)
{
    pdf14_device *pdev = (pdf14_device *)dev;
    pdf14_buf *buf;
    int code;

    fit_fill_xywh(dev, x, y, w, h);
    if (w <= 0 || h <= 0)
        return 0;

    code = pdf14_initialize_ctx(dev, dev->color_info.num_components,
        dev->color_info.polarity != GX_CINFO_POLARITY_SUBTRACTIVE, NULL);
    if (code < 0)
        return code;
    buf = pdev->ctx->stack;

    if (buf->knockout)
        return pdf14_mark_fill_rectangle_ko_simple(dev, x, y, w, h, 0, pdcolor,
            true);
    else
        return pdf14_mark_fill_rectangle(dev, x, y, w, h, 0, pdcolor, true);
}

/* Step through and do rect fills with the devn colors as
   we hit each transition in the bitmap. It is possible
   that one of the colors is not devn, but is pure and
   is set to gx_no_color_index. This type of mix happens
   for example from tile_clip_fill_rectangle_hl_color */
static int
pdf14_copy_mono_devn(gx_device *dev,
    const byte *base, int sourcex, int sraster,
    int x, int y, int w, int h, const gx_drawing_color *pdcolor0,
    const gx_drawing_color *pdcolor1)
{
    const byte *sptr;
    const byte *line;
    int sbit, first_bit;
    int code, sbyte, bit, count;
    int run_length, startx, current_bit, bit_value;
    const gx_drawing_color *current_color;

    if ((x | y) < 0) {
        if (x < 0) {
            w += x;
            sourcex -= x;
            x = 0;
        }
        if (y < 0) {
            h += y;
            base -= (int)(y * sraster);
            y = 0;
        }
    }
    if (w > (dev)->width - x)
        w = (dev)->width - x;
    if (h > (dev)->height - y)
        h = (dev)->height - y;
    if (w <= 0 || h <= 0)
        return 0;

    line = base + (sourcex >> 3);
    sbit = sourcex & 7;
    first_bit = 7 - sbit;

    /* Loop through the height of the specified area. */
    while (h-- > 0) {
        /* Set up for the start of each line of the area. */
        sptr = line;
        sbyte = *sptr++;
        bit = first_bit;
        count = w;
        run_length = 0;
        startx = x;
        current_bit = 0;
        current_color = pdcolor0;

        /* Loop across each pixel of a line. */
        do {
            bit_value = (sbyte >> bit) & 1;
            if (bit_value == current_bit) {
                /* The value did not change, simply increment our run length */
                run_length++;
            } else {
                /* The value changed, fill the current rectangle. */
                if (run_length != 0) {
                    if (current_color->type != gx_dc_type_pure &&
                        current_color->colors.pure != gx_no_color_index) {
                        code = pdf14_fill_rectangle_devn(dev, startx, y,
                            run_length, 1, current_color);
                        if (code < 0)
                            return code;
                    }
                    startx += run_length;
                }
                run_length = 1;
                current_color = bit_value ? pdcolor1 : pdcolor0;
                current_bit = bit_value;
            }

            /* Move to the next input bit. */
            if (bit == 0) {
                bit = 7;
                sbyte = *sptr++;
            } else
                bit--;
        } while (--count > 0);

        /* Fill the last rectangle in the line. */
        if (run_length != 0 && current_color->type != gx_dc_type_pure &&
            current_color->colors.pure != gx_no_color_index) {
            code = pdf14_fill_rectangle_devn(dev, startx, y,
                run_length, 1, current_color);
            if (code < 0)
                return code;
        }
        /* Move to the next line */
        line += sraster;
        y++;
    }
    return 0;
}

/* Step through the tiles doing essentially copy_mono but with devn colors */
static int
pdf14_impl_strip_tile_rectangle_devn(gx_device *dev, const gx_strip_bitmap *tiles,
    int x, int y, int w, int h, const gx_drawing_color *pdcolor0,
    const gx_drawing_color *pdcolor1, int px, int py)
{   /* Fill the rectangle in chunks. */
    int width = tiles->size.x;
    int height = tiles->size.y;
    int raster = tiles->raster;
    int rwidth = tiles->rep_width;
    int rheight = tiles->rep_height;
    int shift = tiles->shift;

    if (rwidth == 0 || rheight == 0)
        return_error(gs_error_unregistered);
    fit_fill_xy(dev, x, y, w, h);

     {
        int xoff = (shift == 0 ? px :
                px + (y + py) / rheight * tiles->rep_shift);
        int irx = ((rwidth & (rwidth - 1)) == 0 ? /* power of 2 */
            (x + xoff) & (rwidth - 1) :
            (x + xoff) % rwidth);
        int ry = ((rheight & (rheight - 1)) == 0 ? /* power of 2 */
            (y + py) & (rheight - 1) :
            (y + py) % rheight);
        int icw = width - irx;
        int ch = height - ry;
        byte *row = tiles->data + ry * raster;
        int code = 0;

        if (ch >= h) {      /* Shallow operation */
            if (icw >= w) { /* Just one (partial) tile to transfer. */
                code = pdf14_copy_mono_devn(dev, row, irx, raster, x, y,
                    w, h, pdcolor0, pdcolor1);
                if (code < 0)
                    return_error(code);
            } else {
                int ex = x + w;
                int fex = ex - width;
                int cx = x + icw;

                code = pdf14_copy_mono_devn(dev, row, irx, raster,
                    x, y, icw, h, pdcolor0, pdcolor1);
                if (code < 0)
                    return_error(code);

                while (cx <= fex) {
                    code = pdf14_copy_mono_devn(dev, row, 0, raster, cx, y,
                        width, h, pdcolor0, pdcolor1);
                    if (code < 0)
                        return_error(code);
                    cx += width;
                }
                if (cx < ex) {
                    code = pdf14_copy_mono_devn(dev, row, 0, raster, cx, y,
                        ex - cx, h, pdcolor0, pdcolor1);
                    if (code < 0)
                        return_error(code);
                }
            }
        } else if (icw >= w && shift == 0) {
            /* Narrow operation, no shift */
            int ey = y + h;
            int fey = ey - height;
            int cy = y + ch;

            code = pdf14_copy_mono_devn(dev, row, irx, raster,
                x, y, w, ch, pdcolor0, pdcolor1);
            if (code < 0)
                return_error(code);
            row = tiles->data;
            do {
                ch = (cy > fey ? ey - cy : height);
                code = pdf14_copy_mono_devn(dev, row, irx, raster,
                    x, cy, w, ch, pdcolor0, pdcolor1);
                if (code < 0)
                    return_error(code);
            } while ((cy += ch) < ey);
        } else {
            /* Full operation.  If shift != 0, some scan lines */
            /* may be narrow.  We could test shift == 0 in advance */
            /* and use a slightly faster loop, but right now */
            /* we don't bother. */
            int ex = x + w, ey = y + h;
            int fex = ex - width, fey = ey - height;
            int cx, cy;

            for (cy = y;;) {
                if (icw >= w) {
                    code = pdf14_copy_mono_devn(dev, row, irx, raster,
                        x, cy, w, ch, pdcolor0, pdcolor1);
                    if (code < 0)
                        return_error(code);
                } else {
                    code = pdf14_copy_mono_devn(dev, row, irx, raster,
                        x, cy, icw, ch, pdcolor0, pdcolor1);
                    if (code < 0)
                        return_error(code);
                    cx = x + icw;
                    while (cx <= fex) {
                        code = pdf14_copy_mono_devn(dev, row, 0, raster,
                            cx, cy, width, ch, pdcolor0, pdcolor1);
                        if (code < 0)
                            return_error(code);
                        cx += width;
                    }
                    if (cx < ex) {
                        code = pdf14_copy_mono_devn(dev, row, 0, raster,
                            cx, cy, ex - cx, ch, pdcolor0, pdcolor1);
                        if (code < 0)
                            return_error(code);
                    }
                }
                if ((cy += ch) >= ey)
                    break;
                ch = (cy > fey ? ey - cy : height);
                if ((irx += shift) >= rwidth)
                    irx -= rwidth;
                icw = width - irx;
                row = tiles->data;
            }
        }
    }
    return 0;
}

/* pdf14 device supports devn */
static int
pdf14_strip_tile_rect_devn(gx_device *dev, const gx_strip_bitmap *tiles,
    int x, int y, int w, int h,
    const gx_drawing_color *pdcolor0,
    const gx_drawing_color *pdcolor1, int px, int py)
{
    pdf14_device *pdev = (pdf14_device *)dev;
    pdf14_buf *buf;
    int num_comp;
    int k;
    bool same = false;
    int code;

    code = pdf14_initialize_ctx(dev, dev->color_info.num_components,
        dev->color_info.polarity != GX_CINFO_POLARITY_SUBTRACTIVE, NULL);
    if (code < 0)
        return code;
    buf = pdev->ctx->stack;
    num_comp = buf->n_chan - 1;

    /* if color0 is identical to color1, do rect fill */
    if (pdcolor0->type == gx_dc_type_devn && pdcolor1->type == gx_dc_type_devn) {
        same = true;
        for (k = 0; k < num_comp; k++) {
            if (pdcolor0->colors.devn.values[k] != pdcolor1->colors.devn.values[k]) {
                same = false;
                break;
            }
        }
    }

    if (same) {
        code = pdf14_fill_rectangle_devn(dev, x, y, w, h, pdcolor0);
    } else {
        /* Go through the tile stepping using code stolen from
           gx_default_strip_tile_rectangle and call the rect fills
           using code stolen from pdf14_copy_mono but using devn
           colors */
        code = pdf14_impl_strip_tile_rectangle_devn(dev, tiles,
            x, y, w, h, pdcolor0, pdcolor1, px, py);
    }
    return code;
}

/* Used in a few odd cases where the target device is planar and we have
   a planar tile (pattern) and we are copying it into place here */
static int
pdf14_copy_planes(gx_device * dev, const byte * data, int data_x, int raster,
                  gx_bitmap_id id, int x, int y, int w, int h, int plane_height)
{
    pdf14_device *pdev = (pdf14_device *)dev;
#if RAW_DUMP
    pdf14_ctx *ctx = pdev->ctx;
#endif
    pdf14_buf *buf = pdev->ctx->stack;
    int xo = x;
    int yo = y;
    pdf14_buf fake_tos;
    int deep = pdev->ctx->deep;

    fit_fill_xywh(dev, x, y, w, h);
    if (w <= 0 || h <= 0)
        return 0;

    fake_tos.deep = deep;
    fake_tos.alpha = (uint16_t)(0xffff * pdev->alpha + 0.5);
    fake_tos.backdrop = NULL;
    fake_tos.blend_mode = pdev->blend_mode;
    fake_tos.color_space = buf->color_space;
    fake_tos.data = (byte *)data + ((data_x - (x - xo))<<deep) - (y - yo) * raster; /* Nasty, cast away of const */
    fake_tos.dirty.p.x = x;
    fake_tos.dirty.p.y = y;
    fake_tos.dirty.q.x = x + w;
    fake_tos.dirty.q.y = y + h;
    fake_tos.has_alpha_g = 0;
    fake_tos.has_shape = 0;
    fake_tos.has_tags = 0;
    fake_tos.idle = false;
    fake_tos.isolated = false;
    fake_tos.knockout = false;
    fake_tos.mask_id = 0;
    fake_tos.mask_stack = NULL;
    fake_tos.matte = NULL;
    fake_tos.matte_num_comps = 0;
    fake_tos.memory = dev->memory;
    fake_tos.n_chan = dev->color_info.num_components;
    fake_tos.n_planes = dev->color_info.num_components;
    fake_tos.num_spots = 0;
    fake_tos.group_color_info = NULL;
    fake_tos.planestride = raster * plane_height;
    fake_tos.rect.p.x = x;
    fake_tos.rect.p.y = y;
    fake_tos.rect.q.x = x + w;
    fake_tos.rect.q.y = y + h;
    fake_tos.rowstride = raster;
    fake_tos.saved = NULL;
    fake_tos.shape = 0xffff;
    fake_tos.SMask_SubType = TRANSPARENCY_MASK_Alpha;
    fake_tos.transfer_fn = NULL;
    pdf14_compose_alphaless_group(&fake_tos, buf, x, x+w, y, y+h,
                                  pdev->ctx->memory, dev);
    return 0;
}

static int
pdf14_fill_rectangle_hl_color(gx_device *dev, const gs_fixed_rect *rect,
    const gs_gstate *pgs, const gx_drawing_color *pdcolor,
    const gx_clip_path *pcpath)
{
    pdf14_device *pdev = (pdf14_device *)dev;
    pdf14_buf* buf;
    int code;
    int x = fixed2int(rect->p.x);
    int y = fixed2int(rect->p.y);
    int w = fixed2int(rect->q.x) - x;
    int h = fixed2int(rect->q.y) - y;

    fit_fill_xywh(dev, x, y, w, h);
    if (w <= 0 || h <= 0)
        return 0;

    code = pdf14_initialize_ctx(dev, dev->color_info.num_components,
        dev->color_info.polarity != GX_CINFO_POLARITY_SUBTRACTIVE, pgs);
    if (code < 0)
        return code;
    buf = pdev->ctx->stack;

    if (buf->knockout)
        return pdf14_mark_fill_rectangle_ko_simple(dev, x, y, w, h, 0, pdcolor,
                                                   true);
    else
        return pdf14_mark_fill_rectangle(dev, x, y, w, h, 0, pdcolor, true);
}

static	int
pdf14_fill_rectangle(gx_device * dev,
                    int x, int y, int w, int h, gx_color_index color)
{
    pdf14_device *pdev = (pdf14_device *)dev;
    pdf14_buf *buf;
    int code;

    fit_fill_xywh(dev, x, y, w, h);
    if (w <= 0 || h <= 0)
        return 0;

    code = pdf14_initialize_ctx(dev, dev->color_info.num_components,
            dev->color_info.polarity != GX_CINFO_POLARITY_SUBTRACTIVE, NULL);
    if (code < 0)
        return code;

    buf = pdev->ctx->stack;

    if (buf->knockout)
        return pdf14_mark_fill_rectangle_ko_simple(dev, x, y, w, h, color, NULL,
                                                   false);
    else
        return pdf14_mark_fill_rectangle(dev, x, y, w, h, color, NULL, false);
}

static int
pdf14_compute_group_device_int_rect(const gs_matrix *ctm,
                                    const gs_rect *pbbox, gs_int_rect *rect)
{
    gs_rect dev_bbox;
    int code;

    code = gs_bbox_transform(pbbox, ctm, &dev_bbox);
    if (code < 0)
        return code;
    rect->p.x = (int)floor(dev_bbox.p.x);
    rect->p.y = (int)floor(dev_bbox.p.y);
    rect->q.x = (int)ceil(dev_bbox.q.x);
    rect->q.y = (int)ceil(dev_bbox.q.y);
    return 0;
}

static	int
compute_group_device_int_rect(pdf14_device *pdev, gs_int_rect *rect,
                              const gs_rect *pbbox, gs_gstate *pgs)
{
    int code = pdf14_compute_group_device_int_rect(&ctm_only(pgs), pbbox, rect);

    if (code < 0)
        return code;
    rect_intersect(*rect, pdev->ctx->rect);
    /* Make sure the rectangle is not anomalous (q < p) -- see gsrect.h */
    if (rect->q.x < rect->p.x)
        rect->q.x = rect->p.x;
    if (rect->q.y < rect->p.y)
        rect->q.y = rect->p.y;
    return 0;
}

static	int
pdf14_begin_transparency_group(gx_device* dev,
    const gs_transparency_group_params_t* ptgp,
    const gs_rect* pbbox,
    gs_gstate* pgs, gs_memory_t* mem)
{
    pdf14_device* pdev = (pdf14_device*)dev;
    float alpha = ptgp->group_opacity * ptgp->group_shape;
    gs_int_rect rect;
    int code;
    bool isolated = ptgp->Isolated;
    gs_transparency_color_t group_color_type;
    cmm_profile_t* group_profile;
    cmm_profile_t* tos_profile;
    gsicc_rendering_param_t render_cond;
    cmm_dev_profile_t* dev_profile;
    bool cm_back_drop = false;
    bool new_icc = false;
    pdf14_group_color_t* group_color_info;

    code = dev_proc(dev, get_profile)(dev, &dev_profile);
    if (code < 0)
        return code;
    gsicc_extract_profile(GS_UNKNOWN_TAG, dev_profile, &tos_profile, &render_cond);

    if (ptgp->text_group == PDF14_TEXTGROUP_BT_PUSHED) {
        pdev->text_group = PDF14_TEXTGROUP_BT_PUSHED;  /* For immediate mode and clist reading */
    }

    if (ptgp->text_group == PDF14_TEXTGROUP_BT_PUSHED)
        rect = pdev->ctx->rect; /* Use parent group for text_group. */
    else
        code = compute_group_device_int_rect(pdev, &rect, pbbox, pgs);

    if (code < 0)
        return code;
    if_debug5m('v', pdev->memory,
        "[v]pdf14_begin_transparency_group, I = %d, K = %d, alpha = %g, bm = %d page_group = %d\n",
        ptgp->Isolated, ptgp->Knockout, (double)alpha, pgs->blend_mode, ptgp->page_group);

    /* If the group color is unknown then use the current device profile. */
    if (ptgp->group_color_type == UNKNOWN) {
        group_color_type = ICC;
        group_profile = tos_profile;
    }
    else {
        group_color_type = ptgp->group_color_type;
        group_profile = ptgp->iccprofile;
    }

    /* We have to handle case where the profile is in the clist */
    if (group_profile == NULL && pdev->pclist_device != NULL) {
        /* Get the serialized data from the clist. */
        gx_device_clist_reader* pcrdev = (gx_device_clist_reader*)(pdev->pclist_device);
        group_profile = gsicc_read_serial_icc((gx_device*)pcrdev, ptgp->icc_hashcode);
        if (group_profile == NULL)
            return gs_throw(gs_error_unknownerror, "ICC data not found in clist");
        /* Keep a pointer to the clist device */
        group_profile->dev = (gx_device*)pcrdev;
        new_icc = true;
    }
    if (group_profile != NULL) {
        /* If we have a non-isolated group and the color space is different,
            we will need to CM the backdrop. */
        if (!gsicc_profiles_equal(group_profile, tos_profile)) {
            cm_back_drop = true;
        }
    }

    /* Always create the base color group information as it is only through
       groups that we can have a color space change.  This will survive
       the life of the context. */
    if (pdev->ctx->base_color == NULL) {
        pdev->ctx->base_color = pdf14_make_base_group_color(dev);
    }

    /* If this is not the page group and we don't yet have a group, we need
       to create a buffer for the whole page so that we can handle stuff drawn
       outside this current group (e.g. two non inclusive groups drawn independently) */
    if (pdev->ctx->stack == NULL && !ptgp->page_group) {
        code = pdf14_initialize_ctx(dev, dev->color_info.num_components,
            dev->color_info.polarity != GX_CINFO_POLARITY_SUBTRACTIVE, NULL);
        if (code < 0)
            return code;
        pdev->ctx->stack->isolated = true;
    }

    group_color_info = pdf14_push_color_model(dev, group_color_type, ptgp->icc_hashcode,
        group_profile, false);
    if (group_color_info == NULL)
        return gs_error_VMerror;
    if_debug0m('v', dev->memory, "[v]Transparency group color space update\n");

    code = pdf14_push_transparency_group(pdev->ctx, &rect, isolated, ptgp->Knockout,
                                        (uint16_t)floor (65535 * alpha + 0.5),
                                        (uint16_t)floor(65535 * ptgp->group_shape + 0.5),
                                        (uint16_t)floor(65535 * ptgp->group_opacity + 0.5),
                                        pgs->blend_mode, ptgp->idle,
                                         ptgp->mask_id, pdev->color_info.num_components,
                                         cm_back_drop, ptgp->shade_group,
                                         group_profile, tos_profile, group_color_info, pgs, dev);
    if (new_icc)
        gsicc_adjust_profile_rc(group_profile, -1, "pdf14_begin_transparency_group");
    return code;
}

static void
pdf14_pop_color_model(gx_device* dev, pdf14_group_color_t* group_color)
{
    pdf14_device* pdev = (pdf14_device*)dev;

    if (group_color != NULL &&
        !(group_color->group_color_mapping_procs == NULL &&
            group_color->group_color_comp_index == NULL)) {
        set_dev_proc(pdev, get_color_mapping_procs, group_color->group_color_mapping_procs);
        set_dev_proc(pdev, get_color_comp_index, group_color->group_color_comp_index);
        pdev->color_info.polarity = group_color->polarity;
        pdev->color_info.num_components = group_color->num_components;
        pdev->blend_procs = group_color->blend_procs;
        pdev->ctx->additive = group_color->isadditive;
        pdev->pdf14_procs = group_color->unpack_procs;
        pdev->color_info.depth = group_color->depth;
        pdev->color_info.max_color = group_color->max_color;
        pdev->color_info.max_gray = group_color->max_gray;
        memcpy(&(pdev->color_info.comp_bits), &(group_color->comp_bits),
            GX_DEVICE_COLOR_MAX_COMPONENTS);
        memcpy(&(pdev->color_info.comp_shift), &(group_color->comp_shift),
            GX_DEVICE_COLOR_MAX_COMPONENTS);
        if (group_color->icc_profile != NULL) {
            /* make sure to decrement the device profile.  If it was allocated
               with the push then it will be freed. */
            gsicc_adjust_profile_rc(pdev->icc_struct->device_profile[GS_DEFAULT_DEVICE_PROFILE],
                                    -1, "pdf14_pop_color_model");
            pdev->icc_struct->device_profile[GS_DEFAULT_DEVICE_PROFILE] =
                                    group_color->icc_profile;

            gsicc_adjust_profile_rc(pdev->icc_struct->device_profile[GS_DEFAULT_DEVICE_PROFILE],
                                    1, "pdf14_pop_color_model");
        }
    }
}

static	int
pdf14_end_transparency_group(gx_device* dev, gs_gstate* pgs)
{
    pdf14_device* pdev = (pdf14_device*)dev;
    int code;
    cmm_profile_t* group_profile;
    gsicc_rendering_param_t render_cond;
    cmm_dev_profile_t* dev_profile;

    code = dev_proc(dev, get_profile)(dev, &dev_profile);
    if (code < 0)
        return code;

    gsicc_extract_profile(GS_UNKNOWN_TAG, dev_profile, &group_profile,
        &render_cond);
    if_debug0m('v', dev->memory, "[v]pdf14_end_transparency_group\n");

    code = pdf14_pop_transparency_group(pgs, pdev->ctx, pdev->blend_procs,
        pdev->color_info.num_components, group_profile, (gx_device*)pdev);
    if (code < 0)
        return code;
#ifdef DEBUG
    pdf14_debug_mask_stack_state(pdev->ctx);
#endif
    /* If this group is the base group, then restore the color model
       of the device at this time.  Note that during the actual device pop
       we will need to use the profile of the buffer not the pdf14 device
       as the source color space */
    if (pdev->ctx->stack->group_popped) {
        pdf14_pop_color_model(dev, pdev->ctx->base_color);
    } else {
        pdf14_pop_color_model(dev, pdev->ctx->stack->group_color_info);
    }

    return code;
}

static pdf14_group_color_t*
pdf14_push_color_model(gx_device *dev, gs_transparency_color_t group_color_type,
                        int64_t icc_hashcode, cmm_profile_t *iccprofile,
                        bool is_mask)
{
    pdf14_device *pdevproto = NULL;
    pdf14_device *pdev = (pdf14_device *)dev;
    const pdf14_procs_t *new_14procs = NULL;
    pdf14_group_color_t *group_color;
    gx_color_polarity_t new_polarity;
    uchar new_num_comps;
    bool new_additive;
    gx_device_clist_reader *pcrdev;
    byte comp_bits[GX_DEVICE_COLOR_MAX_COMPONENTS];
    byte comp_shift[GX_DEVICE_COLOR_MAX_COMPONENTS];
    int k;
    bool has_tags = device_encodes_tags(dev);
    bool deep = pdev->ctx->deep;

    if_debug0m('v', dev->memory, "[v]pdf14_push_color_model\n");

    group_color = gs_alloc_struct(dev->memory->stable_memory,
                               pdf14_group_color_t, &st_pdf14_clr,
                               "pdf14_push_color_model");
    if (group_color == NULL)
        return NULL;

    memset(group_color, 0, sizeof(pdf14_group_color_t));

    switch (group_color_type) {
        case GRAY_SCALE:
            new_polarity = GX_CINFO_POLARITY_ADDITIVE;
            new_num_comps = 1;
            pdevproto = (pdf14_device *)&gs_pdf14_Gray_device;
            new_additive = true;
            new_14procs = &gray_pdf14_procs;
            comp_bits[0] = 8<<deep;
            comp_shift[0] = 0;
            break;
        case DEVICE_RGB:
        case CIE_XYZ:
            new_polarity = GX_CINFO_POLARITY_ADDITIVE;
            new_num_comps = 3;
            pdevproto = (pdf14_device *)&gs_pdf14_RGB_device;
            new_additive = true;
            new_14procs = &rgb_pdf14_procs;
            for (k = 0; k < 3; k++) {
                comp_bits[k] = 8<<deep;
                comp_shift[k] = (2 - k) * (8<<deep);
            }
            break;
        case DEVICE_CMYK:
            new_polarity = GX_CINFO_POLARITY_SUBTRACTIVE;
            new_num_comps = 4;
            pdevproto = (pdf14_device *)&gs_pdf14_CMYK_device;
            new_additive = false;
            /* This is needed due to the mismatched compressed encode decode
                between the device procs and the pdf14 procs */
            if (dev->color_info.num_components > 4){
                new_14procs = &cmykspot_pdf14_procs;
            } else {
                new_14procs = &cmyk_pdf14_procs;
            }
            for (k = 0; k < 4; k++) {
                comp_bits[k] = 8<<deep;
                comp_shift[k] = (3 - k) * (8<<deep);
            }
            break;
        case ICC:
            /* If we are coming from the clist reader, then we need to get
                the ICC data now  */
            if (iccprofile == NULL && pdev->pclist_device != NULL) {
                /* Get the serialized data from the clist.  Not the whole
                    profile. */
                pcrdev = (gx_device_clist_reader *)(pdev->pclist_device);
                iccprofile = gsicc_read_serial_icc((gx_device *) pcrdev,
                                                    icc_hashcode);
                if (iccprofile == NULL)
                    return NULL;
                /* Keep a pointer to the clist device */
                iccprofile->dev = (gx_device *) pcrdev;
            } else {
                /* Go ahead and rc increment right now.  This way when
                    we pop, we will make sure to decrement and avoid a
                    leak for the above profile that we just created.  This
                    goes with the assignment to the device's profile.
                    Note that we still do the increment for the group_color
                    assignment below. */
                if (iccprofile == NULL)
                    return NULL;
                gsicc_adjust_profile_rc(iccprofile, 1, "pdf14_push_color_model");
            }
            new_num_comps = iccprofile->num_comps;
            if (new_num_comps == 4) {
                new_additive = false;
                new_polarity = GX_CINFO_POLARITY_SUBTRACTIVE;
            } else {
                new_additive = true;
                new_polarity = GX_CINFO_POLARITY_ADDITIVE;
            }
            switch (new_num_comps) {
                case 1:
                    if (pdev->sep_device && !is_mask) {
                        pdevproto = (pdf14_device *)&gs_pdf14_Grayspot_device;
                        new_14procs = &grayspot_pdf14_procs;
                    } else {
                        pdevproto = (pdf14_device *)&gs_pdf14_Gray_device;
                        new_14procs = &gray_pdf14_procs;
                    }
                    comp_bits[0] = 8<<deep;
                    comp_shift[0] = 0;
                    break;
                case 3:
                    if (pdev->sep_device) {
                        pdevproto = (pdf14_device *)&gs_pdf14_RGBspot_device;
                        new_14procs = &rgbspot_pdf14_procs;
                    }
                    else {
                        pdevproto = (pdf14_device *)&gs_pdf14_RGB_device;
                        new_14procs = &rgb_pdf14_procs;
                    }
                    for (k = 0; k < 3; k++) {
                        comp_bits[k] = 8<<deep;
                        comp_shift[k] = (2 - k) * (8<<deep);
                    }
                    break;
                case 4:
                    if (pdev->sep_device) {
                        pdevproto = (pdf14_device *)&gs_pdf14_CMYKspot_device;
                        new_14procs = &cmykspot_pdf14_procs;
                    } else {
                        pdevproto = (pdf14_device *)&gs_pdf14_CMYK_device;
                        new_14procs = &cmyk_pdf14_procs;
                    }
                    for (k = 0; k < 4; k++) {
                        comp_bits[k] = 8<<deep;
                        comp_shift[k] = (3 - k) * (8<<deep);
                    }
                    break;
                default:
                    return NULL;
                    break;
            }
            break;
        default:
            return NULL;
            break;
    }

    if (group_color_type == ICC && iccprofile != NULL) {
        group_color->icc_profile = iccprofile;
        gsicc_adjust_profile_rc(iccprofile, 1, "pdf14_push_color_model");
    }

    /* If we are a sep device and this is not a softmask, ensure we maintain the
       spot colorants and know how to index into them */
    if (pdev->sep_device && !is_mask) {
        int num_spots = dev->color_info.num_components -
            dev->icc_struct->device_profile[GS_DEFAULT_DEVICE_PROFILE]->num_comps;

        if (num_spots > 0) {
            new_num_comps += num_spots;
            for (k = 0; k < new_num_comps; k++) {
                comp_bits[k] = 8<<deep;
                comp_shift[k] = (new_num_comps - k - 1) * (8<<deep);
            }
        }
    }

    /* Set device values now and store settings in group_color.  Then they
       are available when we pop the previous group */
    if_debug2m('v', pdev->memory,
                "[v]pdf14_push_color_model, num_components_old = %d num_components_new = %d\n",
                pdev->color_info.num_components,new_num_comps);
    {
        gx_device local_device;

        local_device.initialize_device_procs = pdevproto->initialize_device_procs;
        local_device.initialize_device_procs((gx_device *)&local_device);
        set_dev_proc(pdev, get_color_mapping_procs, local_device.procs.get_color_mapping_procs);
        set_dev_proc(pdev, get_color_comp_index, local_device.procs.get_color_comp_index);
    }
    group_color->blend_procs = pdev->blend_procs = pdevproto->blend_procs;
    group_color->polarity = pdev->color_info.polarity = new_polarity;
    group_color->num_components = pdev->color_info.num_components = new_num_comps;
    group_color->isadditive = pdev->ctx->additive = new_additive;
    group_color->unpack_procs = pdev->pdf14_procs = new_14procs;
    pdev->color_info.depth = new_num_comps * (8<<deep);
    memset(&(pdev->color_info.comp_bits), 0, GX_DEVICE_COLOR_MAX_COMPONENTS);
    memset(&(pdev->color_info.comp_shift), 0, GX_DEVICE_COLOR_MAX_COMPONENTS);
    memcpy(&(pdev->color_info.comp_bits), comp_bits, new_num_comps);
    memcpy(&(pdev->color_info.comp_shift), comp_shift, new_num_comps);
    if (has_tags) {
        pdev->color_info.comp_shift[pdev->color_info.num_components] = pdev->color_info.depth;
        pdev->color_info.depth += 8;
    }
    group_color->max_color = pdev->color_info.max_color = deep ? 65535 : 255;
    group_color->max_gray = pdev->color_info.max_gray = deep ? 65535 : 255;
    group_color->depth = pdev->color_info.depth;
    group_color->decode = dev_proc(pdev, decode_color);
    group_color->encode = dev_proc(pdev, encode_color);
    group_color->group_color_mapping_procs = dev_proc(pdev, get_color_mapping_procs);
    group_color->group_color_comp_index = dev_proc(pdev, get_color_comp_index);
    memcpy(&(group_color->comp_bits), &(pdev->color_info.comp_bits),
        GX_DEVICE_COLOR_MAX_COMPONENTS);
    memcpy(&(group_color->comp_shift), &(pdev->color_info.comp_shift),
        GX_DEVICE_COLOR_MAX_COMPONENTS);
    group_color->get_cmap_procs = pdf14_get_cmap_procs;

    /* If the CS was ICC based, we need to update the device ICC profile
        in the ICC manager, since that is the profile that is used for the
        PDF14 device */
    if (group_color_type == ICC && iccprofile != NULL) {
        /* iccprofile was incremented above if we had not just created it.
           When we do the pop we will decrement and if we just created it, it
           will be destroyed */
        gsicc_adjust_profile_rc(dev->icc_struct->device_profile[GS_DEFAULT_DEVICE_PROFILE], -1, "pdf14_push_color_model");
        dev->icc_struct->device_profile[GS_DEFAULT_DEVICE_PROFILE] = iccprofile;
    }
    return group_color;
}

static int
pdf14_clist_push_color_model(gx_device *dev, gx_device* cdev, gs_gstate *pgs,
                             const gs_pdf14trans_t *pdf14pct, gs_memory_t* mem,
                             bool is_mask)
{
    pdf14_device* pdev = (pdf14_device*)dev;
    pdf14_group_color_t* new_group_color;
    gsicc_rendering_param_t render_cond;
    cmm_dev_profile_t* dev_profile;
    pdf14_device* pdevproto;
    gx_device_clist_writer* cldev = (gx_device_clist_writer*)pdev->pclist_device;
    const pdf14_procs_t* new_14procs;
    bool update_color_info;
    gx_color_polarity_t new_polarity;
    int new_num_comps;
    bool new_additive = false;
    byte new_depth;
    byte comp_bits[GX_DEVICE_COLOR_MAX_COMPONENTS];
    byte comp_shift[GX_DEVICE_COLOR_MAX_COMPONENTS];
    int k;
    bool has_tags = device_encodes_tags(dev);
    bool deep = device_is_deep(dev);
    gs_transparency_color_t group_color_type = pdf14pct->params.group_color_type;
    cmm_profile_t *new_profile = pdf14pct->params.iccprofile;
    cmm_profile_t *old_profile = NULL;

    dev_proc(dev, get_profile)(dev, &dev_profile);
    gsicc_extract_profile(GS_UNKNOWN_TAG, dev_profile, &old_profile,
        &render_cond);
    if_debug0m('v', dev->memory, "[v]pdf14_clist_push_color_model\n");

    /* Allocate a new one */
    new_group_color = gs_alloc_struct(dev->memory->stable_memory, pdf14_group_color_t,
        &st_pdf14_clr, "pdf14_clist_push_color_model");

    /* Link to old one */
    new_group_color->previous = pdev->color_model_stack;

    /* Reassign new one to dev */
    pdev->color_model_stack = new_group_color;

    /* Initialize with values */
    new_group_color->get_cmap_procs = pgs->get_cmap_procs;
    new_group_color->group_color_mapping_procs =
        dev_proc(pdev, get_color_mapping_procs);
    new_group_color->group_color_comp_index =
        dev_proc(pdev, get_color_comp_index);
    new_group_color->blend_procs = pdev->blend_procs;
    new_group_color->polarity = pdev->color_info.polarity;
    new_group_color->num_components = pdev->color_info.num_components;
    new_group_color->unpack_procs = pdev->pdf14_procs;
    new_group_color->depth = pdev->color_info.depth;
    new_group_color->max_color = pdev->color_info.max_color;
    new_group_color->max_gray = pdev->color_info.max_gray;
    new_group_color->decode = dev_proc(pdev, decode_color);
    new_group_color->encode = dev_proc(pdev, encode_color);
    memcpy(&(new_group_color->comp_bits), &(pdev->color_info.comp_bits),
        GX_DEVICE_COLOR_MAX_COMPONENTS);
    memcpy(&(new_group_color->comp_shift), &(pdev->color_info.comp_shift),
        GX_DEVICE_COLOR_MAX_COMPONENTS);

    if (new_profile == NULL)
        new_group_color->icc_profile = NULL;

    /* isadditive is only used in ctx */
    if (pdev->ctx) {
        new_group_color->isadditive = pdev->ctx->additive;
    }

    memset(comp_bits, 0, GX_DEVICE_COLOR_MAX_COMPONENTS);
    memset(comp_shift, 0, GX_DEVICE_COLOR_MAX_COMPONENTS);

    if (group_color_type == ICC && new_profile == NULL)
        return gs_throw(gs_error_undefinedresult, "Missing ICC data");
    if_debug0m('v', cldev->memory, "[v]pdf14_clist_push_color_model\n");
    /* Check if we need to alter the device procs at this stage.  Many of the procs
       are based upon the color space of the device.  We want to remain in the
       color space defined by the color space of the soft mask or transparency
       group as opposed to the device color space. Later, when we pop the softmask
       we will collapse it to a single band and then compose with it to the device
       color space (or the parent layer space).  In the case where we pop an
       isolated transparency group, we will do the blending in the proper color
       space and then transform the data when we pop the group.  Remember that only
       isolated groups can have color spaces that are different than their parent. */
    update_color_info = false;
    switch (group_color_type) {
    case GRAY_SCALE:
        if (pdev->color_info.num_components != 1) {
            update_color_info = true;
            new_polarity = GX_CINFO_POLARITY_ADDITIVE;
            new_num_comps = 1;
            pdevproto = (pdf14_device*)&gs_pdf14_Gray_device;
            new_additive = true;
            new_14procs = &gray_pdf14_procs;
            new_depth = 8 << deep;
            comp_bits[0] = 8 << deep;
            comp_shift[0] = 0;
        }
        break;
    case DEVICE_RGB:
    case CIE_XYZ:
        if (pdev->color_info.num_components != 3) {
            update_color_info = true;
            new_polarity = GX_CINFO_POLARITY_ADDITIVE;
            new_num_comps = 3;
            pdevproto = (pdf14_device*)&gs_pdf14_RGB_device;
            new_additive = true;
            new_14procs = &rgb_pdf14_procs;
            new_depth = 24 << deep;
            for (k = 0; k < 3; k++) {
                comp_bits[k] = 8 << deep;
                comp_shift[k] = (2 - k) * (8 << deep);
            }
        }
        break;
    case DEVICE_CMYK:
        if (pdev->color_info.num_components != 4) {
            update_color_info = true;
            new_polarity = GX_CINFO_POLARITY_SUBTRACTIVE;
            new_num_comps = 4;
            pdevproto = (pdf14_device*)&gs_pdf14_CMYK_device;
            new_additive = false;
            /* This is needed due to the mismatched compressed encode decode
               between the device procs and the pdf14 procs */
            if (dev->color_info.num_components > 4) {
                new_14procs = &cmykspot_pdf14_procs;
            }
            else {
                new_14procs = &cmyk_pdf14_procs;
            }
            new_depth = 32 << deep;
            for (k = 0; k < 4; k++) {
                comp_bits[k] = 8 << deep;
                comp_shift[k] = (3 - k) * (8 << deep);
            }
        }
        break;
    case ICC:
        /* Check if the profile is different. */
        if (!gsicc_profiles_equal(old_profile, new_profile)) {
            update_color_info = true;
            new_num_comps = new_profile->num_comps;
            new_depth = new_profile->num_comps * (8 << deep);
            switch (new_num_comps) {
            case 1:
                if (pdev->sep_device && !is_mask) {
                    pdevproto = (pdf14_device*)&gs_pdf14_Grayspot_device;
                    new_14procs = &grayspot_pdf14_procs;
                }
                else {
                    pdevproto = (pdf14_device*)&gs_pdf14_Gray_device;
                    new_14procs = &gray_pdf14_procs;
                }
                new_polarity = GX_CINFO_POLARITY_ADDITIVE;
                new_additive = true;
                comp_bits[0] = 8 << deep;
                comp_shift[0] = 0;
                break;
            case 3:
                if (pdev->sep_device) {
                    pdevproto = (pdf14_device*)&gs_pdf14_RGBspot_device;
                    new_14procs = &rgbspot_pdf14_procs;
                }
                else {
                    pdevproto = (pdf14_device*)&gs_pdf14_RGB_device;
                    new_14procs = &rgb_pdf14_procs;
                }
                new_polarity = GX_CINFO_POLARITY_ADDITIVE;
                new_additive = true;
                for (k = 0; k < 3; k++) {
                    comp_bits[k] = 8 << deep;
                    comp_shift[k] = (2 - k) * (8 << deep);
                }
                break;
            case 4:
                if (pdev->sep_device) {
                    pdevproto = (pdf14_device*)&gs_pdf14_CMYKspot_device;
                    new_14procs = &cmykspot_pdf14_procs;
                }
                else {
                    pdevproto = (pdf14_device*)&gs_pdf14_CMYK_device;
                    new_14procs = &cmyk_pdf14_procs;
                }
                new_polarity = GX_CINFO_POLARITY_SUBTRACTIVE;
                new_additive = false;
                for (k = 0; k < 4; k++) {
                    comp_bits[k] = 8 << deep;
                    comp_shift[k] = (3 - k) * (8 << deep);
                }
                break;
            default:
                return gs_throw(gs_error_undefinedresult,
                    "ICC Number of colorants illegal");
            }
        }
        break;
    case UNKNOWN:
        return 0;
        break;
    default:
        return_error(gs_error_rangecheck);
        break;
    }

    if (!update_color_info) {
        /* Profile not updated */
        new_group_color->icc_profile = NULL;
        if_debug0m('v', pdev->memory, "[v]procs not updated\n");
        return 0;
    }

    if (pdev->sep_device && !is_mask) {
        int num_spots;

        if (old_profile == NULL)
            return_error(gs_error_undefined);

        num_spots = pdev->color_info.num_components - old_profile->num_comps;

        if (num_spots > 0) {
            new_num_comps += num_spots;
            for (k = 0; k < new_num_comps; k++) {
                comp_bits[k] = 8 << deep;
                comp_shift[k] = (new_num_comps - k - 1) * (8 << deep);
            }
            new_depth = (8 << deep) * new_num_comps;
        }
    }
    if (has_tags) {
        new_depth += 8;
    }
    if_debug2m('v', pdev->memory,
        "[v]pdf14_clist_push_color_model, num_components_old = %d num_components_new = %d\n",
        pdev->color_info.num_components, new_num_comps);
    /* Set new information in the device */
    {
        gx_device local_device;

        local_device.initialize_device_procs = pdevproto->initialize_device_procs;
        local_device.initialize_device_procs((gx_device *)&local_device);
        set_dev_proc(pdev, get_color_mapping_procs, local_device.procs.get_color_mapping_procs);
        set_dev_proc(pdev, get_color_comp_index, local_device.procs.get_color_comp_index);
    }
    pdev->blend_procs = pdevproto->blend_procs;
    pdev->color_info.polarity = new_polarity;
    pdev->color_info.num_components = new_num_comps;
    pdev->color_info.max_color = deep ? 65535 : 255;
    pdev->color_info.max_gray = deep ? 65535 : 255;
    pdev->pdf14_procs = new_14procs;
    pdev->color_info.depth = new_depth;
    memset(&(pdev->color_info.comp_bits), 0, GX_DEVICE_COLOR_MAX_COMPONENTS);
    memset(&(pdev->color_info.comp_shift), 0, GX_DEVICE_COLOR_MAX_COMPONENTS);
    memcpy(&(pdev->color_info.comp_bits), comp_bits, new_num_comps);
    memcpy(&(pdev->color_info.comp_shift), comp_shift, new_num_comps);
    pdev->color_info.comp_shift[new_num_comps] = new_depth - 8;	/* in case we has_tags is set */

    /* If we have a compressed color codec, and we are doing a soft mask
       push operation then go ahead and update the color encode and
       decode for the pdf14 device to not used compressed color
       encoding while in the soft mask.  We will just check for gray
       and compressed.  Note that we probably don't have_tags if we
       are dealing with compressed color.  But is is possible so
       we add it in to catch for future use. */
    cldev->clist_color_info.depth = pdev->color_info.depth;
    cldev->clist_color_info.polarity = pdev->color_info.polarity;
    cldev->clist_color_info.num_components = pdev->color_info.num_components;
    cldev->clist_color_info.max_color = pdev->color_info.max_color;
    cldev->clist_color_info.max_gray = pdev->color_info.max_gray;
    /* For the ICC profiles, we want to update the ICC profile for the
       device.  We store the original in group_color.
       That will be stored in the clist and restored during the reading phase. */
    if (group_color_type == ICC) {
        gsicc_adjust_profile_rc(new_profile, 1, "pdf14_clist_push_color_model");
        new_group_color->icc_profile =
            dev->icc_struct->device_profile[GS_DEFAULT_DEVICE_PROFILE];
        dev->icc_struct->device_profile[GS_DEFAULT_DEVICE_PROFILE] = new_profile;
    }
    if (pdev->ctx) {
        pdev->ctx->additive = new_additive;
    }
    return 1;  /* Lets us detect that we did do an update */
}

static int
pdf14_clist_pop_color_model(gx_device *dev, gs_gstate *pgs)
{

    pdf14_device *pdev = (pdf14_device *)dev;
    pdf14_group_color_t *group_color = pdev->color_model_stack;
    gx_device_clist_writer * cldev = (gx_device_clist_writer *)pdev->pclist_device;

    if (group_color == NULL)
        return_error(gs_error_unknownerror);  /* Unmatched group pop */

    if_debug0m('v', pdev->memory, "[v]pdf14_clist_pop_color_model\n");
    /* The color procs are always pushed.  Simply restore them. */
    if (group_color->group_color_mapping_procs == NULL &&
        group_color->group_color_comp_index == NULL) {
        if_debug0m('v', dev->memory, "[v]pdf14_clist_pop_color_model ERROR \n");
    } else {
        if_debug2m('v', pdev->memory,
                   "[v]pdf14_clist_pop_color_model, num_components_old = %d num_components_new = %d\n",
                   pdev->color_info.num_components,group_color->num_components);
        pgs->get_cmap_procs = group_color->get_cmap_procs;
        gx_set_cmap_procs(pgs, dev);
        set_dev_proc(pdev, get_color_mapping_procs, group_color->group_color_mapping_procs);
        set_dev_proc(pdev, get_color_comp_index, group_color->group_color_comp_index);
        pdev->color_info.polarity = group_color->polarity;
        pdev->color_info.depth = group_color->depth;
        pdev->color_info.num_components = group_color->num_components;
        pdev->blend_procs = group_color->blend_procs;
        pdev->pdf14_procs = group_color->unpack_procs;
        pdev->color_info.max_color = group_color->max_color;
        pdev->color_info.max_gray = group_color->max_gray;
        set_dev_proc(pdev, encode_color, group_color->encode);
        set_dev_proc(pdev, decode_color, group_color->decode);
        memcpy(&(pdev->color_info.comp_bits),&(group_color->comp_bits),
                            GX_DEVICE_COLOR_MAX_COMPONENTS);
        memcpy(&(pdev->color_info.comp_shift),&(group_color->comp_shift),
                            GX_DEVICE_COLOR_MAX_COMPONENTS);
        /* clist writer fill rect has no access to gs_gstate */
        /* and it forwards the target device.  this information */
        /* is passed along to use in this case */
        cldev->clist_color_info.depth = pdev->color_info.depth;
        cldev->clist_color_info.polarity = pdev->color_info.polarity;
        cldev->clist_color_info.num_components = pdev->color_info.num_components;
        cldev->clist_color_info.max_color = pdev->color_info.max_color;
        cldev->clist_color_info.max_gray = pdev->color_info.max_gray;
        memcpy(&(cldev->clist_color_info.comp_bits),&(group_color->comp_bits),
               GX_DEVICE_COLOR_MAX_COMPONENTS);
        memcpy(&(cldev->clist_color_info.comp_shift),&(group_color->comp_shift),
               GX_DEVICE_COLOR_MAX_COMPONENTS);
        if (pdev->ctx){
            pdev->ctx->additive = group_color->isadditive;
        }
       /* The device profile must be restored. */
        if (group_color->icc_profile != NULL) {
            gsicc_adjust_profile_rc(dev->icc_struct->device_profile[GS_DEFAULT_DEVICE_PROFILE],
                                    -1, "pdf14_clist_pop_color_model");
            dev->icc_struct->device_profile[GS_DEFAULT_DEVICE_PROFILE] = group_color->icc_profile;
        }
        if_debug0m('v', dev->memory, "[v]procs updated\n");
    }
   pdf14_pop_group_color(dev, pgs);
    return 0;
}

/* When a transparency group is popped, the parent colorprocs must be restored.
   Since the color mapping procs are all based upon the device, we must have a
   nested list based upon the transparency group color space.  This nesting
   must be outside the nested ctx structures to allow the nesting for the
   clist writer */
static void
pdf14_pop_group_color(gx_device *dev, const gs_gstate *pgs)
{
    pdf14_device *pdev = (pdf14_device *)dev;
    pdf14_group_color_t *group_color = pdev->color_model_stack;

    if_debug0m('v', dev->memory, "[v]pdf14_pop_group_color\n");

    /* Update the link */
    pdev->color_model_stack = group_color->previous;

    /* Free the old one */
    gs_free_object(dev->memory->stable_memory, group_color, "pdf14_clr_free");
}

static	int
pdf14_begin_transparency_mask(gx_device	*dev,
                              const gx_transparency_mask_params_t *ptmp,
                              const gs_rect *pbbox,
                              gs_gstate *pgs, gs_memory_t *mem)
{
    pdf14_device *pdev = (pdf14_device *)dev;
    uint16_t bg_alpha = 0;   /* By default the background alpha (area outside mask) is zero */
    byte *transfer_fn;
    gs_int_rect rect;
    int code;
    int group_color_numcomps;
    gs_transparency_color_t group_color_type;
    bool deep = device_is_deep(dev);
    pdf14_group_color_t* group_color_info;

    code = pdf14_initialize_ctx(dev, dev->color_info.num_components,
        dev->color_info.polarity != GX_CINFO_POLARITY_SUBTRACTIVE, (const gs_gstate*)pgs);
    if (code < 0)
        return code;

    if (ptmp->subtype == TRANSPARENCY_MASK_None) {
        pdf14_ctx *ctx = pdev->ctx;

        /* free up any maskbuf on the current tos */
        if (ctx->mask_stack) {
            if (ctx->mask_stack->rc_mask->mask_buf != NULL ) {
                pdf14_buf_free(ctx->mask_stack->rc_mask->mask_buf);
                ctx->mask_stack->rc_mask->mask_buf = NULL;
            }
        }
        return 0;
    }
    transfer_fn = (byte *)gs_alloc_bytes(pdev->ctx->memory, (256+deep)<<deep,
                                         "pdf14_begin_transparency_mask");
    if (transfer_fn == NULL)
        return_error(gs_error_VMerror);
    code = compute_group_device_int_rect(pdev, &rect, pbbox, pgs);
    if (code < 0)
        return code;
    /* If we have background components the background alpha may be nonzero */
    if (ptmp->Background_components)
        bg_alpha = (int)(65535 * ptmp->GrayBackground + 0.5);
    if_debug1m('v', dev->memory,
               "pdf14_begin_transparency_mask, bg_alpha = %d\n", bg_alpha);
    memcpy(transfer_fn, ptmp->transfer_fn, (256+deep)<<deep);
   /* If the group color is unknown, then we must use the previous group color
       space or the device process color space */
    if (ptmp->group_color_type == UNKNOWN){
        if (pdev->ctx->stack){
            /* Use previous group color space */
            group_color_numcomps = pdev->ctx->stack->n_chan-1;  /* Remove alpha */
        } else {
            /* Use process color space */
            group_color_numcomps = pdev->color_info.num_components;
        }
        switch (group_color_numcomps) {
            case 1:
                group_color_type = GRAY_SCALE;
                break;
            case 3:
                group_color_type = DEVICE_RGB;
                break;
            case 4:
                group_color_type = DEVICE_CMYK;
            break;
            default:
                /* We can end up here if we are in a deviceN color space and
                   we have a sep output device */
                group_color_type = DEVICEN;
            break;
         }
    } else {
        group_color_type = ptmp->group_color_type;
        group_color_numcomps = ptmp->group_color_numcomps;
    }

    group_color_info = pdf14_push_color_model(dev, group_color_type, ptmp->icc_hashcode,
                                               ptmp->iccprofile, true);
    if (group_color_info == NULL)
        return gs_error_VMerror;

    /* Note that the soft mask always follows the group color requirements even
       when we have a separable device */
    code = pdf14_push_transparency_mask(pdev->ctx, &rect, bg_alpha,
                                        transfer_fn, ptmp->function_is_identity,
                                        ptmp->idle, ptmp->replacing,
                                        ptmp->mask_id, ptmp->subtype,
                                        group_color_numcomps,
                                        ptmp->Background_components,
                                        ptmp->Background,
                                        ptmp->Matte_components,
                                        ptmp->Matte,
                                        ptmp->GrayBackground,
                                        group_color_info);
    if (code < 0)
        return code;

    return 0;
}

static	int
pdf14_end_transparency_mask(gx_device *dev, gs_gstate *pgs)
{
    pdf14_device *pdev = (pdf14_device *)dev;
    pdf14_group_color_t *group_color;
    int ok;

    if_debug0m('v', dev->memory, "pdf14_end_transparency_mask\n");
    ok = pdf14_pop_transparency_mask(pdev->ctx, pgs, dev);
#ifdef DEBUG
    pdf14_debug_mask_stack_state(pdev->ctx);
#endif

    /* May need to reset some color stuff related
     * to a mismatch between the Smask color space
     * and the Smask blending space */
    if (pdev->ctx->stack != NULL ) {
        group_color = pdev->ctx->stack->group_color_info;
        if (!(group_color->group_color_mapping_procs == NULL &&
            group_color->group_color_comp_index == NULL)) {
            pgs->get_cmap_procs = group_color->get_cmap_procs;
            gx_set_cmap_procs(pgs, dev);
            set_dev_proc(pdev, get_color_mapping_procs, group_color->group_color_mapping_procs);
            set_dev_proc(pdev, get_color_comp_index, group_color->group_color_comp_index);
            pdev->color_info.polarity = group_color->polarity;
            pdev->color_info.num_components = group_color->num_components;
            pdev->color_info.depth = group_color->depth;
            pdev->blend_procs = group_color->blend_procs;
            pdev->ctx->additive = group_color->isadditive;
            pdev->pdf14_procs = group_color->unpack_procs;
            pdev->color_info.max_color = group_color->max_color;
            pdev->color_info.max_gray = group_color->max_gray;
            set_dev_proc(pdev, encode_color, group_color->encode);
            set_dev_proc(pdev, decode_color, group_color->decode);
            memcpy(&(pdev->color_info.comp_bits),&(group_color->comp_bits),
                                GX_DEVICE_COLOR_MAX_COMPONENTS);
            memcpy(&(pdev->color_info.comp_shift),&(group_color->comp_shift),
                                GX_DEVICE_COLOR_MAX_COMPONENTS);
            /* Take care of the ICC profile */
            if (group_color->icc_profile != NULL) {
                gsicc_adjust_profile_rc(dev->icc_struct->device_profile[GS_DEFAULT_DEVICE_PROFILE],
                                        -1, "pdf14_end_transparency_mask");
                dev->icc_struct->device_profile[GS_DEFAULT_DEVICE_PROFILE] = group_color->icc_profile;
                gsicc_adjust_profile_rc(dev->icc_struct->device_profile[GS_DEFAULT_DEVICE_PROFILE],
                                         1, "pdf14_end_transparency_mask");
            }
        }
    }
    return ok;
}

static	int
do_mark_fill_rectangle_ko_simple(gx_device *dev, int x, int y, int w, int h,
                                 gx_color_index color,
                                 const gx_device_color *pdc, bool devn)
{
    pdf14_device *pdev = (pdf14_device *)dev;
    pdf14_buf *buf = pdev->ctx->stack;
    gs_blend_mode_t blend_mode = pdev->blend_mode;
    bool tag_blend = blend_mode == BLEND_MODE_Normal ||
        blend_mode == BLEND_MODE_Compatible ||
        blend_mode == BLEND_MODE_CompatibleOverprint;
    int i, j, k;
    byte *bline, *bg_ptr, *line, *dst_ptr;
    byte src[PDF14_MAX_PLANES];
    byte dst[PDF14_MAX_PLANES] = { 0 };
    byte dst2[PDF14_MAX_PLANES] = { 0 };
    int rowstride = buf->rowstride;
    int planestride = buf->planestride;
    int num_chan = buf->n_chan;
    int num_comp = num_chan - 1;
    int shape_off = num_chan * planestride;
    bool has_shape = buf->has_shape;
    bool has_alpha_g = buf->has_alpha_g;
    int alpha_g_off = shape_off + (has_shape ? planestride : 0);
    int tag_off = shape_off + (has_alpha_g ? planestride : 0) +
                              (has_shape ? planestride : 0);
    bool has_tags = buf->has_tags;
    bool additive = pdev->ctx->additive;
    gs_graphics_type_tag_t curr_tag = GS_UNKNOWN_TAG;  /* Quiet compiler */
    gx_color_index mask = ((gx_color_index)1 << 8) - 1;
    int shift = 8;
    byte shape = 0; /* Quiet compiler. */
    byte src_alpha;
    bool overprint = pdev->op_state == PDF14_OP_STATE_FILL ? pdev->overprint : pdev->stroke_overprint;
    gx_color_index drawn_comps = pdev->op_state == PDF14_OP_STATE_FILL ?
        pdev->drawn_comps_fill : pdev->drawn_comps_stroke;
    gx_color_index comps;
    bool has_backdrop = buf->backdrop != NULL;

    if (buf->data == NULL)
        return 0;
#if 0
    if (sizeof(color) <= sizeof(ulong))
        if_debug6m('v', dev->memory,
                   "[v]pdf14_mark_fill_rectangle_ko_simple, (%d, %d), %d x %d color = %lx, nc %d,\n",
                   x, y, w, h, (ulong)color, num_chan);
    else
        if_debug7m('v', dev->memory,
                   "[v]pdf14_mark_fill_rectangle_ko_simple, (%d, %d), %d x %d color = %8lx%08lx, nc %d,\n",
                   x, y, w, h,
                   (ulong)(color >> 8*(sizeof(color) - sizeof(ulong))), (ulong)color,
                   num_chan);
#endif
    /*
     * Unpack the gx_color_index values.  Complement the components for subtractive
     * color spaces.
     */
    if (devn) {
        if (additive) {
            for (j = 0; j < num_comp; j++) {
                src[j] = ((pdc->colors.devn.values[j]) >> shift & mask);
            }
        } else {
            for (j = 0; j < num_comp; j++) {
                src[j] = 255 - ((pdc->colors.devn.values[j]) >> shift & mask);
            }
        }
    } else
        pdev->pdf14_procs->unpack_color(num_comp, color, pdev, src);

    src_alpha = src[num_comp] = (byte)floor (255 * pdev->alpha + 0.5);
    if (has_shape) {
        shape = (byte)floor (255 * pdev->shape + 0.5);
    } else {
        shape_off = 0;
    }
    if (has_tags) {
        curr_tag = (color >> (num_comp*8)) & 0xff;
    } else {
        tag_off = 0;
    }
    if (!has_alpha_g)
        alpha_g_off = 0;
    src_alpha = 255 - src_alpha;
    shape = 255 - shape;

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
    /* Update the dirty rectangle with the mark. */
    if (x < buf->dirty.p.x) buf->dirty.p.x = x;
    if (y < buf->dirty.p.y) buf->dirty.p.y = y;
    if (x + w > buf->dirty.q.x) buf->dirty.q.x = x + w;
    if (y + h > buf->dirty.q.y) buf->dirty.q.y = y + h;

    /* composite with backdrop only. */
    if (has_backdrop)
        bline = buf->backdrop + (x - buf->rect.p.x) + (y - buf->rect.p.y) * rowstride;
    else
        bline = NULL;

    line = buf->data + (x - buf->rect.p.x) + (y - buf->rect.p.y) * rowstride;

    for (j = 0; j < h; ++j) {
        bg_ptr = bline;
        dst_ptr = line;
        for (i = 0; i < w; ++i) {
            /* Complement the components for subtractive color spaces */
            if (has_backdrop) {
                if (additive) {
                    for (k = 0; k < num_chan; ++k)
                        dst[k] = bg_ptr[k * planestride];
                } else {
                    for (k = 0; k < num_comp; ++k)
                        dst2[k] = dst[k] = 255 - bg_ptr[k * planestride];
                }
                dst2[num_comp] = dst[num_comp] = bg_ptr[num_comp * planestride];	/* alpha doesn't invert */
            }
            if (buf->isolated || !has_backdrop) {
                art_pdf_knockoutisolated_group_8(dst, src, num_comp);
            } else {
                art_pdf_composite_knockout_8(dst, src, num_comp,
                                            blend_mode, pdev->blend_procs, pdev);
            }
            /* Complement the results for subtractive color spaces */
            if (additive) {
                if (!overprint) {
                    for (k = 0; k < num_chan; ++k)
                        dst_ptr[k * planestride] = dst[k];
                } else {
                    /* Hybrid additive with subtractive spots */
                    /* We may have to do the compatible overprint blending */
                    if (!buf->isolated && drawn_comps != (((size_t)1 << (size_t)dev->color_info.num_components) - (size_t)1)) {
                        art_pdf_composite_knockout_8(dst2, src, num_comp,
                            blend_mode, pdev->blend_procs, pdev);
                    }
                    for (k = 0, comps = drawn_comps; k < num_comp; ++k, comps >>= 1) {
                        if ((comps & 0x1) != 0) {
                            dst_ptr[k * planestride] = dst[k];
                        } else {
                            /* Compatible overprint blend result. */
                            dst_ptr[k * planestride] = dst2[k];
                        }
                    }
                    dst_ptr[num_comp * planestride] = dst[num_comp];    /* alpha */
                }
            } else {
                if (overprint) {
                    /* We may have to do the compatible overprint blending */
                    if (!buf->isolated && drawn_comps != (( (size_t) 1 << (size_t) dev->color_info.num_components)-(size_t) 1)) {
                        art_pdf_composite_knockout_8(dst2, src, num_comp,
                            blend_mode, pdev->blend_procs, pdev);
                    }
                    for (k = 0, comps = drawn_comps; k < num_comp; ++k, comps >>= 1) {
                        if ((comps & 0x1) != 0) {
                            dst_ptr[k * planestride] = 255 - dst[k];
                        } else {
                            /* Compatible overprint blend result. */
                            dst_ptr[k * planestride] = 255 - dst2[k];
                        }
                    }
                } else {
                    for (k = 0; k < num_comp; ++k)
                        dst_ptr[k * planestride] = 255 - dst[k];
                }
                dst_ptr[num_comp * planestride] = dst[num_comp];
            }
            if (tag_off) {
                /* FIXME: As we are knocking out, possibly, we should be
                 * always overwriting tag values here? */
                /* If src alpha is 100% then set to curr_tag, else or */
                /* other than Normal BM, we always OR */
                if (src[num_comp] == 255 && tag_blend) {
                    dst_ptr[tag_off] = curr_tag;
                } else {
                    dst_ptr[tag_off] |= curr_tag;
                }
            }
            /* Knockout group alpha and shape too */
            if (alpha_g_off)
                dst_ptr[alpha_g_off] = 255 - src_alpha;
            if (shape_off)
                dst_ptr[shape_off] = 255 - shape;
            ++dst_ptr;
            if (has_backdrop)
                ++bg_ptr;
        }
        bline += rowstride;
        line += rowstride;
    }
#if 0
/* #if RAW_DUMP */
    /* Dump the current buffer to see what we have. */
    dump_raw_buffer(pdev->ctx->memory,
                    pdev->ctx->stack->rect.q.y-pdev->ctx->stack->rect.p.y,
                    pdev->ctx->stack->rect.q.x-pdev->ctx->stack->rect.p.x,
                    pdev->ctx->stack->n_planes,
                    pdev->ctx->stack->planestride, pdev->ctx->stack->rowstride,
                    "Draw_Rect_KO", pdev->ctx->stack->data,
                    pdev->ctx->stack->deep);
    global_index++;
#endif
    return 0;
}

static	int
do_mark_fill_rectangle_ko_simple16(gx_device *dev, int x, int y, int w, int h,
                                   gx_color_index color,
                                   const gx_device_color *pdc, bool devn)
{
    pdf14_device *pdev = (pdf14_device *)dev;
    pdf14_buf *buf = pdev->ctx->stack;
    gs_blend_mode_t blend_mode = pdev->blend_mode;
    bool tag_blend = blend_mode == BLEND_MODE_Normal ||
        blend_mode == BLEND_MODE_Compatible ||
        blend_mode == BLEND_MODE_CompatibleOverprint;
    int i, j, k;
    uint16_t *bline, *bg_ptr, *line, *dst_ptr;
    uint16_t src[PDF14_MAX_PLANES];
    uint16_t dst[PDF14_MAX_PLANES] = { 0 };
    uint16_t dst2[PDF14_MAX_PLANES] = { 0 };
    int rowstride = buf->rowstride;
    int planestride = buf->planestride;
    int num_chan = buf->n_chan;
    int num_comp = num_chan - 1;
    int shape_off = num_chan * planestride;
    bool has_shape = buf->has_shape;
    bool has_alpha_g = buf->has_alpha_g;
    int alpha_g_off = shape_off + (has_shape ? planestride : 0);
    int tag_off = shape_off + (has_alpha_g ? planestride : 0) +
                              (has_shape ? planestride : 0);
    bool has_tags = buf->has_tags;
    bool additive = pdev->ctx->additive;
    gs_graphics_type_tag_t curr_tag = GS_UNKNOWN_TAG;  /* Quiet compiler */
    uint16_t shape = 0; /* Quiet compiler. */
    uint16_t src_alpha;
    bool overprint = pdev->op_state == PDF14_OP_STATE_FILL ? pdev->overprint : pdev->stroke_overprint;
    gx_color_index drawn_comps = pdev->op_state == PDF14_OP_STATE_FILL ?
        pdev->drawn_comps_fill : pdev->drawn_comps_stroke;
    gx_color_index comps;
    bool has_backdrop = buf->backdrop != NULL;

    if (buf->data == NULL)
        return 0;
#if 0
    if (sizeof(color) <= sizeof(ulong))
        if_debug6m('v', dev->memory,
                   "[v]pdf14_mark_fill_rectangle_ko_simple16, (%d, %d), %d x %d color = %lx, nc %d,\n",
                   x, y, w, h, (ulong)color, num_chan);
    else
        if_debug7m('v', dev->memory,
                   "[v]pdf14_mark_fill_rectangle_ko_simple16, (%d, %d), %d x %d color = %8lx%08lx, nc %d,\n",
                   x, y, w, h,
                   (ulong)(color >> 8*(sizeof(color) - sizeof(ulong))), (ulong)color,
                   num_chan);
#endif
    /*
     * Unpack the gx_color_index values.  Complement the components for subtractive
     * color spaces.
     */
    if (devn) {
        if (additive) {
            for (j = 0; j < num_comp; j++) {
                src[j] = pdc->colors.devn.values[j];
            }
        } else {
            for (j = 0; j < num_comp; j++) {
                src[j] = 65535 - pdc->colors.devn.values[j];
            }
        }
    } else
        pdev->pdf14_procs->unpack_color16(num_comp, color, pdev, src);

    src_alpha = src[num_comp] = (uint16_t)floor (65535 * pdev->alpha + 0.5);
    if (has_shape) {
        shape = (uint16_t)floor (65535 * pdev->shape + 0.5);
    } else {
        shape_off = 0;
    }
    if (has_tags) {
        curr_tag = (color >> (num_comp*16)) & 0xff;
    } else {
        tag_off = 0;
    }
    if (!has_alpha_g)
        alpha_g_off = 0;
    src_alpha = 65535 - src_alpha;
    shape = 65535 - shape;

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
    /* Update the dirty rectangle with the mark. */
    if (x < buf->dirty.p.x) buf->dirty.p.x = x;
    if (y < buf->dirty.p.y) buf->dirty.p.y = y;
    if (x + w > buf->dirty.q.x) buf->dirty.q.x = x + w;
    if (y + h > buf->dirty.q.y) buf->dirty.q.y = y + h;


    /* composite with backdrop only. */
    if (has_backdrop)
        bline = (uint16_t*)(void*)(buf->backdrop + (x - buf->rect.p.x) * 2 + (y - buf->rect.p.y) * rowstride);
    else
        bline = NULL;

    line = (uint16_t *)(void *)(buf->data + (x - buf->rect.p.x)*2 + (y - buf->rect.p.y) * rowstride);
    planestride >>= 1;
    rowstride >>= 1;
    alpha_g_off >>= 1;
    shape_off >>= 1;
    tag_off >>= 1;

    for (j = 0; j < h; ++j) {
        bg_ptr = bline;
        dst_ptr = line;
        for (i = 0; i < w; ++i) {
            /* Complement the components for subtractive color spaces */
            if (has_backdrop) {
                if (additive) {
                    for (k = 0; k < num_chan; ++k)
                        dst[k] = bg_ptr[k * planestride];
                } else {
                    for (k = 0; k < num_comp; ++k)
                        dst2[k] = dst[k] = 65535 - bg_ptr[k * planestride];
                }
                dst2[num_comp] = dst[num_comp] = bg_ptr[num_comp * planestride];	/* alpha doesn't invert */
            }
            if (buf->isolated || !has_backdrop) {
                art_pdf_knockoutisolated_group_16(dst, src, num_comp);
            } else {
                art_pdf_composite_knockout_16(dst, src, num_comp,
                                              blend_mode, pdev->blend_procs, pdev);
            }
            /* Complement the results for subtractive color spaces */
            if (additive) {
                if (!overprint) {
                    for (k = 0; k < num_chan; ++k)
                        dst_ptr[k * planestride] = dst[k];
                } else {
                    /* Hybrid additive with subtractive spots */
                    /* We may have to do the compatible overprint blending */
                    if (!buf->isolated && drawn_comps != (((size_t)1 << (size_t)dev->color_info.num_components) - (size_t)1)) {
                        art_pdf_composite_knockout_16(dst2, src, num_comp,
                            blend_mode, pdev->blend_procs, pdev);
                    }
                    for (k = 0, comps = drawn_comps; k < num_comp; ++k, comps >>= 1) {
                        if ((comps & 0x1) != 0) {
                            dst_ptr[k * planestride] = dst[k];
                        } else {
                            /* Compatible overprint blend result. */
                            dst_ptr[k * planestride] = dst2[k];
                        }
                    }
                    dst_ptr[num_comp * planestride] = dst[num_comp];    /* alpha */
                }
            } else {
                if (overprint) {
                    /* We may have to do the compatible overprint blending */
                    if (!buf->isolated && drawn_comps != (((size_t)1 << (size_t)dev->color_info.num_components) - (size_t)1)) {
                        art_pdf_composite_knockout_16(dst2, src, num_comp,
                            blend_mode, pdev->blend_procs, pdev);
                    }
                    for (k = 0, comps = drawn_comps; k < num_comp; ++k, comps >>= 1) {
                        if ((comps & 0x1) != 0) {
                            dst_ptr[k * planestride] = 65535 - dst[k];
                        } else {
                            /* Compatible overprint blend result. */
                            dst_ptr[k * planestride] = 65535 - dst2[k];
                        }
                    }
                } else {
                    for (k = 0; k < num_comp; ++k)
                        dst_ptr[k * planestride] = 65535 - dst[k];
                }
                dst_ptr[num_comp * planestride] = dst[num_comp];
            }
            if (tag_off) {
                /* FIXME: As we are knocking out, possibly, we should be
                 * always overwriting tag values here? */
                /* If src alpha is 100% then set to curr_tag, else or */
                /* other than Normal BM, we always OR */
                if (src[num_comp] == 65535 && tag_blend) {
                    dst_ptr[tag_off] = curr_tag;
                } else {
                    dst_ptr[tag_off] |= curr_tag;
                }
            }
            /* Knockout group alpha and shape too */
            if (alpha_g_off)
                dst_ptr[alpha_g_off] = 65535 - src_alpha;
            if (shape_off)
                dst_ptr[shape_off] = 65535 - shape;
            ++dst_ptr;
            if (has_backdrop)
                ++bg_ptr;
        }
        bline += rowstride;
        line += rowstride;
    }
#if 0
/* #if RAW_DUMP */
    /* Dump the current buffer to see what we have. */
    dump_raw_buffer(pdev->ctx->memory,
                    pdev->ctx->stack->rect.q.y-pdev->ctx->stack->rect.p.y,
                    pdev->ctx->stack->rect.q.x-pdev->ctx->stack->rect.p.x,
                    pdev->ctx->stack->n_planes,
                    pdev->ctx->stack->planestride, pdev->ctx->stack->rowstride,
                    "Draw_Rect_KO", pdev->ctx->stack->data,
                    pdev->ctx->stack->deep);
    global_index++;
#endif
    return 0;
}

static	int
pdf14_mark_fill_rectangle_ko_simple(gx_device *	dev, int x, int y, int w, int h,
                                    gx_color_index color,
                                    const gx_device_color *pdc, bool devn)
{
    pdf14_device *pdev = (pdf14_device *)dev;
    pdf14_buf *buf = pdev->ctx->stack;

    if (buf->deep)
        return do_mark_fill_rectangle_ko_simple16(dev, x, y, w, h, color, pdc, devn);
    else
        return do_mark_fill_rectangle_ko_simple(dev, x, y, w, h, color, pdc, devn);
}

/**
 * Here we have logic to override the cmap_procs with versions that
 * do not apply the transfer function. These copies should track the
 * versions in gxcmap.c.
 **/
static	cmap_proc_gray(pdf14_cmap_gray_direct);
static	cmap_proc_rgb(pdf14_cmap_rgb_direct);
static	cmap_proc_cmyk(pdf14_cmap_cmyk_direct);
static	cmap_proc_separation(pdf14_cmap_separation_direct);
static	cmap_proc_devicen(pdf14_cmap_devicen_direct);
static	cmap_proc_is_halftoned(pdf14_cmap_is_halftoned);

static	const gx_color_map_procs pdf14_cmap_many = {
     pdf14_cmap_gray_direct,
     pdf14_cmap_rgb_direct,
     pdf14_cmap_cmyk_direct,
     pdf14_cmap_separation_direct,
     pdf14_cmap_devicen_direct,
     pdf14_cmap_is_halftoned
    };

/**
 * Note: copied from gxcmap.c because it's inlined.
 **/
static	inline void
map_components_to_colorants(const frac * pcc,
                            const gs_devicen_color_map * pcolor_component_map,
                            frac * plist)
{
    int i = pcolor_component_map->num_colorants - 1;
    int pos;

    /* Clear all output colorants first */
    for (; i >= 0; i--) {
        plist[i] = frac_0;
    }
    /* Map color components into output list */
    for (i = pcolor_component_map->num_components - 1; i >= 0; i--) {
        pos = pcolor_component_map->color_map[i];
        if (pos >= 0)
            plist[pos] = pcc[i];
    }
}

/* See Section 7.6.4 of PDF 1.7 spec */
static inline bool
pdf14_state_opaque(gx_device *pdev, const gs_gstate *pgs)
{
    if (pgs->fillconstantalpha != 1.0 ||
        pgs->strokeconstantalpha != 1.0 ||
        !(pgs->blend_mode == BLEND_MODE_Normal ||
          pgs->blend_mode == BLEND_MODE_CompatibleOverprint))
        return 0;

    /* We can only be opaque if we're not in an SMask. */
    return dev_proc(pdev, dev_spec_op)(pdev,
                                       gxdso_in_smask,
                                       NULL, 0) != 1;
}

static	void
pdf14_cmap_gray_direct(frac gray, gx_device_color * pdc, const gs_gstate * pgs,
                       gx_device * dev, gs_color_select_t select)
{
    int i,ncomps;
    frac cm_comps[GX_DEVICE_COLOR_MAX_COMPONENTS];
    gx_color_value cv[GX_DEVICE_COLOR_MAX_COMPONENTS];
    gx_color_index color;
    gx_device *trans_device;
    const gx_device *map_dev;
    const gx_cm_color_map_procs *procs;

    /* If trans device is set, we need to use its procs. */
    if (pgs->trans_device != NULL) {
        trans_device = pgs->trans_device;
    } else {
        trans_device = dev;
    }
    ncomps = trans_device->color_info.num_components;

    /* map to the color model */
    procs = dev_proc(trans_device, get_color_mapping_procs)(trans_device, &map_dev);
    procs->map_gray(map_dev, gray, cm_comps);

    if (pdf14_state_opaque(trans_device, pgs)) {
        for (i = 0; i < ncomps; i++)
            cv[i] = frac2cv(gx_map_color_frac(pgs, cm_comps[i], effective_transfer[i]));
    } else {
        for (i = 0; i < ncomps; i++)
            cv[i] = frac2cv(cm_comps[i]);
    }

    /* If output device supports devn, we need to make sure we send it the
       proper color type.  We now support Gray + spots as devn colors */
    if (dev_proc(trans_device, dev_spec_op)(trans_device, gxdso_supports_devn, NULL, 0)) {
        for (i = 0; i < ncomps; i++)
            pdc->colors.devn.values[i] = cv[i];
        pdc->type = gx_dc_type_devn;
    } else {
        /* encode as a color index */
        color = dev_proc(trans_device, encode_color)(trans_device, cv);
        /* check if the encoding was successful; we presume failure is rare */
        if (color != gx_no_color_index)
            color_set_pure(pdc, color);
    }
}

static	void
pdf14_cmap_rgb_direct(frac r, frac g, frac b, gx_device_color *	pdc,
     const gs_gstate * pgs, gx_device * dev, gs_color_select_t select)
{
    int i,ncomps;
    frac cm_comps[GX_DEVICE_COLOR_MAX_COMPONENTS];
    gx_color_value cv[GX_DEVICE_COLOR_MAX_COMPONENTS];
    gx_color_index color;
    gx_device *trans_device;
    const gx_device *map_dev;
    const gx_cm_color_map_procs *procs;

    /* If trans device is set, we need to use its procs. */
    if (pgs->trans_device != NULL){
        trans_device = pgs->trans_device;
    } else {
        trans_device = dev;
    }
    ncomps = trans_device->color_info.num_components;
    /* map to the color model */
    procs = dev_proc(trans_device, get_color_mapping_procs)(trans_device, &map_dev);
    procs->map_rgb(map_dev, pgs, r, g, b, cm_comps);

    if (pdf14_state_opaque(trans_device, pgs)) {
        for (i = 0; i < ncomps; i++)
            cv[i] = frac2cv(gx_map_color_frac(pgs, cm_comps[i], effective_transfer[i]));
    } else {
        for (i = 0; i < ncomps; i++)
            cv[i] = frac2cv(cm_comps[i]);
    }

    /* If output device supports devn, we need to make sure we send it the
       proper color type.  We now support RGB + spots as devn colors */
    if (dev_proc(trans_device, dev_spec_op)(trans_device, gxdso_supports_devn, NULL, 0)) {
        for (i = 0; i < ncomps; i++)
            pdc->colors.devn.values[i] = cv[i];
        pdc->type = gx_dc_type_devn;
    } else {
        /* encode as a color index */
        color = dev_proc(trans_device, encode_color)(trans_device, cv);
        /* check if the encoding was successful; we presume failure is rare */
        if (color != gx_no_color_index)
            color_set_pure(pdc, color);
    }
}

static	void
pdf14_cmap_cmyk_direct(frac c, frac m, frac y, frac k, gx_device_color * pdc,
     const gs_gstate * pgs, gx_device * dev, gs_color_select_t select,
     const gs_color_space *pcs)
{
    int i, ncomps;
    frac cm_comps[GX_DEVICE_COLOR_MAX_COMPONENTS];
    gx_color_value cv[GX_DEVICE_COLOR_MAX_COMPONENTS];
    gx_color_index color;
    gx_device *trans_device;
    const gx_device *map_dev;
    const gx_cm_color_map_procs *procs;


    /* If trans device is set, we need to use its procs. */
    if (pgs->trans_device != NULL){
        trans_device = pgs->trans_device;
    } else {
        trans_device = dev;
    }
    ncomps = trans_device->color_info.num_components;

    /* Map to the color model. Transfer function is only used
       if we are drawing with an opaque color. */
    procs = dev_proc(trans_device, get_color_mapping_procs)(trans_device, &map_dev);
    procs->map_cmyk(map_dev, c, m, y, k, cm_comps);

    if (pdf14_state_opaque(trans_device, pgs)) {
        for (i = 0; i < ncomps; i++)
            cv[i] = frac2cv(gx_map_color_frac(pgs, cm_comps[i], effective_transfer[i]));
    } else {
        for (i = 0; i < ncomps; i++)
            cv[i] = frac2cv(cm_comps[i]);
    }

    /* if output device supports devn, we need to make sure we send it the
       proper color type */
    if (dev_proc(trans_device, dev_spec_op)(trans_device, gxdso_supports_devn, NULL, 0)) {
        for (i = 0; i < ncomps; i++)
            pdc->colors.devn.values[i] = cv[i];
        pdc->type = gx_dc_type_devn;
    } else {
        /* encode as a color index */
        color = dev_proc(trans_device, encode_color)(trans_device, cv);
        /* check if the encoding was successful; we presume failure is rare */
        if (color != gx_no_color_index)
            color_set_pure(pdc, color);
    }
}

static int
pdf14_get_num_spots(gx_device * dev)
{
    cmm_dev_profile_t *dev_profile;
    cmm_profile_t *icc_profile;
    gsicc_rendering_param_t render_cond;

    dev_proc(dev, get_profile)(dev, &dev_profile);
    gsicc_extract_profile(GS_UNKNOWN_TAG, dev_profile, &icc_profile,
        &render_cond);
    return dev->color_info.num_components - icc_profile->num_comps;
}

static	void
pdf14_cmap_separation_direct(frac all, gx_device_color * pdc, const gs_gstate * pgs,
                 gx_device * dev, gs_color_select_t select, const gs_color_space *pcs)
{
    int i, ncomps = dev->color_info.num_components;
    int num_spots = pdf14_get_num_spots(dev);
    bool additive = dev->color_info.polarity == GX_CINFO_POLARITY_ADDITIVE;
    frac cm_comps[GX_DEVICE_COLOR_MAX_COMPONENTS];
    gx_color_value cv[GX_DEVICE_COLOR_MAX_COMPONENTS];
    gx_color_index color;

    if (pgs->color_component_map.sep_type == SEP_ALL) {
        frac comp_value = all;

        /*
         * Invert the photometric interpretation for additive
         * color spaces because separations are always subtractive.
         */
        if (additive)
            comp_value = frac_1 - comp_value;
        /* Use the "all" value for all components */
        i = pgs->color_component_map.num_colorants - 1;
        for (; i >= 0; i--)
            cm_comps[i] = comp_value;
    } else {
        frac comp_value[GX_DEVICE_COLOR_MAX_COMPONENTS];

        /* map to the color model */
        for (i = pgs->color_component_map.num_components - 1; i >= 0; i--)
            comp_value[i] = all;
        map_components_to_colorants(comp_value, &(pgs->color_component_map), cm_comps);
    }
    /* apply the transfer function(s); convert to color values */
    if (additive) {
        for (i = 0; i < ncomps; i++)
            cv[i] = frac2cv(gx_map_color_frac(pgs, cm_comps[i], effective_transfer[i]));
        /* We are in an additive mode (blend space) and drawing with a sep color
        into a sep device.  Make sure we are drawing "white" with the process
        colorants, but only if we are not in an ALL case */
        if (pgs->color_component_map.sep_type != SEP_ALL)
            for (i = 0; i < ncomps - num_spots; i++)
                cv[i] = gx_max_color_value;
    } else
        for (i = 0; i < ncomps; i++)
            cv[i] = frac2cv(frac_1 - gx_map_color_frac(pgs, (frac)(frac_1 - cm_comps[i]), effective_transfer[i]));


    /* if output device supports devn, we need to make sure we send it the
       proper color type */
    if (dev_proc(dev, dev_spec_op)(dev, gxdso_supports_devn, NULL, 0)) {
        for (i = 0; i < ncomps; i++)
            pdc->colors.devn.values[i] = cv[i];
        pdc->type = gx_dc_type_devn;
    } else {
        /* encode as a color index */
        color = dev_proc(dev, encode_color)(dev, cv);
        /* check if the encoding was successful; we presume failure is rare */
        if (color != gx_no_color_index)
            color_set_pure(pdc, color);
    }
}

static	void
pdf14_cmap_devicen_direct(const	frac * pcc,
    gx_device_color * pdc, const gs_gstate * pgs, gx_device * dev,
    gs_color_select_t select, const gs_color_space *pcs)
{
    int i, ncomps = dev->color_info.num_components;
    int num_spots = pdf14_get_num_spots(dev);
    frac cm_comps[GX_DEVICE_COLOR_MAX_COMPONENTS];
    gx_color_value cv[GX_DEVICE_COLOR_MAX_COMPONENTS];
    gx_color_index color;
    gx_device *trans_device;

     /*  We may be coming from the clist writer which often forwards us the
         target device. If this occurs we actually need to get to the color
         space defined by the transparency group and we use the operators
         defined by the transparency device to do the job.
       */
    if (pgs->trans_device != NULL){
        trans_device = pgs->trans_device;
    } else {
        trans_device = dev;
    }
    ncomps = trans_device->color_info.num_components;
    /* map to the color model */
    map_components_to_colorants(pcc, &(pgs->color_component_map), cm_comps);;
    /* apply the transfer function(s); convert to color values */
    if (trans_device->color_info.polarity == GX_CINFO_POLARITY_ADDITIVE) {
        for (i = 0; i < ncomps; i++)
            cv[i] = frac2cv(gx_map_color_frac(pgs, cm_comps[i], effective_transfer[i]));
        /* We are in an additive mode (blend space) and drawing with a sep color
        into a sep device.  Make sure we are drawing "white" with the process
        colorants */
        for (i = 0; i < ncomps - num_spots; i++)
            cv[i] = gx_max_color_value;
    } else
        for (i = 0; i < ncomps; i++)
            cv[i] = frac2cv(frac_1 - gx_map_color_frac(pgs, (frac)(frac_1 - cm_comps[i]), effective_transfer[i]));
    /* if output device supports devn, we need to make sure we send it the
       proper color type */
    if (dev_proc(trans_device, dev_spec_op)(trans_device, gxdso_supports_devn, NULL, 0)) {
        for (i = 0; i < ncomps; i++)
            pdc->colors.devn.values[i] = cv[i];
        pdc->type = gx_dc_type_devn;
    } else {
    /* encode as a color index */
        color = dev_proc(trans_device, encode_color)(trans_device, cv);
        /* check if the encoding was successful; we presume failure is rare */
        if (color != gx_no_color_index)
            color_set_pure(pdc, color);
    }
}

static	bool
pdf14_cmap_is_halftoned(const gs_gstate * pgs, gx_device * dev)
{
    return false;
}

static	const gx_color_map_procs *
pdf14_get_cmap_procs(const gs_gstate *pgs, const gx_device * dev)
{
    /* The pdf14 marking device itself is always continuous tone. */
    return &pdf14_cmap_many;
}

static int
pdf14_dev_spec_op(gx_device *pdev, int dev_spec_op,
                  void *data, int size)
{
    pdf14_device * p14dev = (pdf14_device *)pdev;

    if (dev_spec_op == gxdso_supports_pattern_transparency)
        return 1;
    if (dev_spec_op == gxdso_pattern_shfill_doesnt_need_path)
        return 1;
    if (dev_spec_op == gxdso_is_pdf14_device) {
        if (data != NULL && size == sizeof(gx_device *))
            *(gx_device **)data = pdev;
        return 1;
    }
    if (dev_spec_op == gxdso_device_child) {
        pdf14_device *dev = (pdf14_device *)pdev;
        gxdso_device_child_request *d = (gxdso_device_child_request *)data;
        if (d->target == pdev) {
            d->target = dev->target;
            return 1;
        }
    }
    if (dev_spec_op == gxdso_supports_devn
     || dev_spec_op == gxdso_skip_icc_component_validation) {
        cmm_dev_profile_t *dev_profile;
        int code;
        code = dev_proc(pdev, get_profile)((gx_device*) pdev, &dev_profile);
        if (code == 0) {
            return dev_profile->supports_devn;
        } else {
            return 0;
        }
    }
    if (dev_spec_op == gxdso_pdf14_sep_device) {
        pdf14_device* dev = (pdf14_device*)pdev;

        if (strcmp(dev->dname, "pdf14cmykspot") == 0 ||
            strcmp(dev->dname, "pdf14clistcmykspot") == 0)
            return 1;
        return 0;
    }
    if (dev_spec_op == gxdso_is_encoding_direct)
        return 1;

    /* We don't want to pass on these spec_ops either, because the child might respond
     * with an inappropriate response when the PDF14 device is active. For example; the
     * JPEG passthrough will give utterly wrong results if we pass that to a device which
     * supports JPEG passthrough, because the pdf14 device needs to render the image.
     */
    if (dev_spec_op == gxdso_in_pattern_accumulator)
        return 0;
    if (dev_spec_op == gxdso_copy_color_is_fast)
        return 0;
    if(dev_spec_op == gxdso_pattern_handles_clip_path)
        return 0;
    if(dev_spec_op == gxdso_supports_hlcolor)
        return 0;
    if(dev_spec_op == gxdso_pattern_can_accum)
        return 0;
    if(dev_spec_op == gxdso_JPEG_passthrough_query)
        return 0;
    if (dev_spec_op == gxdso_overprint_active) {
        if (p14dev->pclist_device != NULL) {
            return dev_proc(p14dev->pclist_device, dev_spec_op)(p14dev->pclist_device, dev_spec_op, data, size);
        } else {
            return p14dev->overprint || p14dev->stroke_overprint;
        }
    }
    if (dev_spec_op == gxdso_in_smask_construction)
        return p14dev->in_smask_construction > 0;
    if (dev_spec_op == gxdso_in_smask)
        return p14dev->in_smask_construction > 0 || p14dev->depth_within_smask;
    if (dev_spec_op == gxdso_device_insert_child) {
        gx_device *tdev = p14dev->target;
        p14dev->target = (gx_device *)data;
        rc_increment(p14dev->target);
        rc_decrement_only(tdev, "pdf14_dev_spec_op");
        return 0;
    }

     return dev_proc(p14dev->target, dev_spec_op)(p14dev->target, dev_spec_op, data, size);
}

/* Needed to set color monitoring in the target device's profile */
int
gs_pdf14_device_color_mon_set(gx_device *pdev, bool monitoring)
{
    pdf14_device * p14dev = (pdf14_device *)pdev;
    gx_device *targ = p14dev->target;
    cmm_dev_profile_t *dev_profile;
    int code = dev_proc(targ, get_profile)((gx_device*) targ, &dev_profile);

    if (code == 0)
        dev_profile->pageneutralcolor = monitoring;
    return code;
}

static int
gs_pdf14_device_push(gs_memory_t *mem, gs_gstate * pgs,
        gx_device ** pdev, gx_device * target, const gs_pdf14trans_t * pdf14pct)
{
    pdf14_device dev_proto;
    pdf14_device * p14dev;
    int code;
    bool has_tags;
    cmm_profile_t *icc_profile;
    gsicc_rendering_param_t render_cond;
    cmm_dev_profile_t *dev_profile;
    uchar k;
    int max_bitmap;
    bool use_pdf14_accum = false;
    bool deep;

    /* Guard against later seg faults, this should not be possible */
    if (target == NULL)
        return gs_throw_code(gs_error_Fatal);

    has_tags = device_encodes_tags(target);
    deep = device_is_deep(target);
    max_bitmap = target->space_params.MaxBitmap == 0 ? MAX_BITMAP :
                                 target->space_params.MaxBitmap;
    /* If the device is not a printer class device, it won't support saved-pages */
    /* and so we may need to make a clist device in order to prevent very large  */
    /* or high resolution pages from having allocation problems.                 */
    /* We use MaxBitmap to decide when a clist is needed.*/
    if (dev_proc(target, dev_spec_op)(target, gxdso_supports_saved_pages, NULL, 0) <= 0 &&
        gx_device_is_pattern_clist(target) == 0 &&
        gx_device_is_pattern_accum(target) == 0 &&
        gs_device_is_memory(target) == 0) {

        uint32_t pdf14_trans_buffer_size =
              (ESTIMATED_PDF14_ROW_SPACE(max(1, target->width),
                                         target->color_info.num_components,
                                         deep ? 16 : 8) >> 3);

        if (target->height < max_ulong / pdf14_trans_buffer_size)
                pdf14_trans_buffer_size *= target->height;
        else
                max_bitmap = 0;     /* Force decision to clist */
        if (pdf14_trans_buffer_size > max_bitmap)
            use_pdf14_accum = true;
    }
    code = dev_proc(target, get_profile)(target,  &dev_profile);
    if (code < 0)
        return code;
    gsicc_extract_profile(GS_UNKNOWN_TAG, dev_profile, &icc_profile,
                          &render_cond);
    if_debug0m('v', mem, "[v]gs_pdf14_device_push\n");

    code = get_pdf14_device_proto(target, &dev_proto, pgs,
                                  pdf14pct, use_pdf14_accum);
    if (code < 0)
        return code;
    code = gs_copydevice((gx_device **) &p14dev,
                         (const gx_device *) &dev_proto, mem);
    if (code < 0)
        return code;

    gs_pdf14_device_copy_params((gx_device *)p14dev, target);
    gx_device_set_target((gx_device_forward *)p14dev, target);
    p14dev->pad = target->pad;
    p14dev->log2_align_mod = target->log2_align_mod;
    if (pdf14pct->params.overprint_sim_push && pdf14pct->params.num_spot_colors_int > 0 && !target->is_planar)
        p14dev->is_planar = true;
    else
        p14dev->is_planar = target->is_planar;

    p14dev->alpha = 1.0;
    p14dev->shape = 1.0;
    p14dev->opacity = 1.0;
    p14dev->fillconstantalpha = 1.0;
    p14dev->strokeconstantalpha = 1.0;

    /* Simulated overprint case.  We have to use CMYK-based profile.  Also if the target
       profile is NCLR, we are going to use a pdf14 device that is CMYK based and
       do the mapping to the NCLR profile when the put_image occurs */
    if ((p14dev->overprint_sim && icc_profile->data_cs != gsCMYK) ||
        icc_profile->data_cs == gsNCHANNEL) {
        gsicc_adjust_profile_rc(pgs->icc_manager->default_cmyk, 1, "gs_pdf14_device_push");
        gsicc_adjust_profile_rc(p14dev->icc_struct->device_profile[GS_DEFAULT_DEVICE_PROFILE],
            -1, "gs_pdf14_device_push");
        p14dev->icc_struct->device_profile[GS_DEFAULT_DEVICE_PROFILE] = pgs->icc_manager->default_cmyk;
    } else {
        /* If the target profile was CIELAB (and we are not using a blend CS),
           then overide with default RGB for proper blending.  During put_image
           we will convert from RGB to CIELAB.  Need to check that we have a
           default profile, which will not be the case if we are coming from the clist reader */
        if ((icc_profile->data_cs == gsCIELAB || icc_profile->islab)
            && pgs->icc_manager->default_rgb != NULL && !p14dev->using_blend_cs) {
            gsicc_adjust_profile_rc(pgs->icc_manager->default_rgb, 1, "gs_pdf14_device_push");
            gsicc_adjust_profile_rc(p14dev->icc_struct->device_profile[GS_DEFAULT_DEVICE_PROFILE],
                -1, "gs_pdf14_device_push");
            p14dev->icc_struct->device_profile[GS_DEFAULT_DEVICE_PROFILE] = pgs->icc_manager->default_rgb;
        }
    }

    if (pdf14pct->params.overprint_sim_push &&
        pdf14pct->params.num_spot_colors_int > 0) {
        p14dev->procs.update_spot_equivalent_colors = pdf14_update_spot_equivalent_colors;
        p14dev->procs.ret_devn_params = pdf14_ret_devn_params;
        p14dev->op_pequiv_cmyk_colors.all_color_info_valid = false;
        p14dev->target_support_devn = p14dev->icc_struct->supports_devn;
        p14dev->icc_struct->supports_devn = true;  /* Reset when pdf14 device is disabled */
    }

    /* The number of color planes should not exceed that of the target.
       Unless we are using a blend CS */
    if (!(p14dev->using_blend_cs || p14dev->overprint_sim)) {
        if (p14dev->color_info.num_components > target->color_info.num_components)
            p14dev->color_info.num_components = target->color_info.num_components;
        if (p14dev->color_info.max_components > target->color_info.max_components)
            p14dev->color_info.max_components = target->color_info.max_components;
    }
    p14dev->color_info.depth = p14dev->color_info.num_components * (8<<deep);
    /* If we have a tag device then go ahead and do a special encoder
       decoder for the pdf14 device to make sure we maintain this
       information in the encoded color information.  We could use
       the target device's methods but the PDF14 device has to maintain
       8 bit color always and we could run into other issues if the number
       of colorants became large.  If we need to do compressed color with
       tags that will be a special project at that time */
    if (deep) {
        set_dev_proc(p14dev, encode_color, pdf14_encode_color16);
        set_dev_proc(p14dev, decode_color, pdf14_decode_color16);
    }
    if (has_tags) {
        set_dev_proc(p14dev, encode_color, deep ? pdf14_encode_color16_tag : pdf14_encode_color_tag);
        p14dev->color_info.comp_shift[p14dev->color_info.num_components] = p14dev->color_info.depth;
        p14dev->color_info.depth += 8;
    }

    /* by definition pdf14_encode _is_ standard */
    p14dev->color_info.separable_and_linear = GX_CINFO_SEP_LIN_STANDARD;
    gx_device_fill_in_procs((gx_device *)p14dev);
    p14dev->save_get_cmap_procs = pgs->get_cmap_procs;
    pgs->get_cmap_procs = pdf14_get_cmap_procs;
    gx_set_cmap_procs(pgs, (gx_device *)p14dev);

    /* Components shift, etc have to be based upon 8 (or 16) bit */
    for (k = 0; k < p14dev->color_info.num_components; k++) {
        p14dev->color_info.comp_bits[k] = 8<<deep;
        p14dev->color_info.comp_shift[k] =
                            (p14dev->color_info.num_components - 1 - k) * (8<<deep);
    }
    if (use_pdf14_accum) {
        /* we will disable this device later, but we don't want to allocate large buffers */
        p14dev->width = 1;
        p14dev->height = 1;
    }

    p14dev->op_state = pgs->is_fill_color ? PDF14_OP_STATE_FILL : PDF14_OP_STATE_NONE;
    code = dev_proc((gx_device *) p14dev, open_device) ((gx_device *) p14dev);
    *pdev = (gx_device *) p14dev;
    pdf14_set_marking_params((gx_device *)p14dev, pgs);
    p14dev->color_model_stack = NULL;

    /* In case we have alphabits set */
    p14dev->color_info.anti_alias = target->color_info.anti_alias;

    if (pdf14pct->params.is_pattern) {
        code = pdf14_initialize_ctx((gx_device*)p14dev,
            p14dev->color_info.num_components,
            p14dev->color_info.polarity != GX_CINFO_POLARITY_SUBTRACTIVE, (const gs_gstate*) pgs);
        if (code < 0)
            return code;
    }

    /* We should never go into this when using a blend color space */
    if (use_pdf14_accum) {
        const gx_device_pdf14_accum *accum_proto = NULL;
        gx_device *new_target = NULL;
        gx_device_color pdcolor;
        frac pconc_white = frac_1;
        bool UsePlanarBuffer = false;

        if_debug0m('v', mem, "[v]gs_pdf14_device_push: Inserting clist device.\n");

        /* get the prototype for the accumulator device based on colorspace */
        switch (target->color_info.max_components) {	/* use max_components in case is devn device */
            case 1:
                accum_proto = &pdf14_accum_Gray;
                break;
            case 3:
                accum_proto = &pdf14_accum_RGB;
                break;
            case 4:
                accum_proto = &pdf14_accum_CMYK;
                break;
            default:
                accum_proto = &pdf14_accum_CMYKspot;
                UsePlanarBuffer = true;
        }
        if (accum_proto == NULL ||
            (code = gs_copydevice(&new_target, (gx_device *)accum_proto, mem->stable_memory)) < 0)
            goto no_clist_accum;

        ((gx_device_pdf14_accum *)new_target)->save_p14dev = (gx_device *)p14dev;  /* non-clist p14dev */
        /* Fill in values from the target device before opening */
        new_target->color_info = p14dev->color_info;
        ((gx_device_pdf14_accum *)new_target)->devn_params = p14dev->devn_params;
        new_target->color_info.separable_and_linear = GX_CINFO_SEP_LIN;
        set_linear_color_bits_mask_shift(new_target);
        gs_pdf14_device_copy_params(new_target, target);
        ((gx_device_pdf14_accum *)new_target)->page_uses_transparency = true;
        gx_device_fill_in_procs(new_target);

        memcpy(&(new_target->space_params), &(target->space_params), sizeof(gdev_space_params));
        max_bitmap = max(target->space_params.MaxBitmap, target->space_params.BufferSpace);
        ((gx_device_pdf14_accum *)new_target)->space_params.BufferSpace = max_bitmap;

        new_target->PageHandlerPushed = true;
        new_target->ObjectHandlerPushed = true;

        /* UsePlanarBuffer is true in case this is CMYKspot */
        if ((code = gdev_prn_open_planar(new_target, UsePlanarBuffer)) < 0 ||
             !PRINTER_IS_CLIST((gx_device_printer *)new_target)) {
            gs_free_object(mem->stable_memory, new_target, "pdf14-accum");
            goto no_clist_accum;
        }
        /* Do the initial fillpage into the pdf14-accum device we just created */
        dev_proc(new_target, set_graphics_type_tag)((gx_device *)new_target, GS_UNTOUCHED_TAG);
        if ((code = gx_remap_concrete_DGray(gs_currentcolorspace_inline((gs_gstate *)pgs),
                                            &pconc_white,
                                            &pdcolor, pgs, new_target, gs_color_select_all,
                                            dev_profile)) < 0)
            goto no_clist_accum;

        (*dev_proc(new_target, fillpage))(new_target, pgs, &pdcolor);
        code = clist_composite(new_target, pdev, (gs_composite_t *)pdf14pct, pgs, mem, NULL);
        if (code < 0)
            goto no_clist_accum;

        pdf14_disable_device((gx_device *)p14dev);           /* make the non-clist device forward */
        pdf14_close((gx_device *)p14dev);                    /* and free up the little memory it had */
    }
    return code;

no_clist_accum:
        /* FIXME: We allocated a really small p14dev, but that won't work */
    return gs_throw_code(gs_error_Fatal); /* punt for now */
}

/*
 * In a modest violation of good coding practice, the gs_composite_common
 * fields are "known" to be simple (contain no pointers to garbage
 * collected memory), and we also know the gs_pdf14trans_params_t structure
 * to be simple, so we just create a trivial structure descriptor for the
 * entire gs_pdf14trans_s structure.
 */
#define	private_st_gs_pdf14trans_t()\
  gs_private_st_ptrs2(st_pdf14trans, gs_pdf14trans_t, "gs_pdf14trans_t",\
      st_pdf14trans_enum_ptrs, st_pdf14trans_reloc_ptrs, params.transfer_function, params.iccprofile)

/* GC descriptor for gs_pdf14trans_t */
private_st_gs_pdf14trans_t();

/*
 * Check for equality of two PDF 1.4 transparency compositor objects.
 *
 * We are currently always indicating that PDF 1.4 transparency compositors are
 * equal.  Two transparency compositors may have teh same data but still
 * represent separate actions.  (E.g. two PDF14_BEGIN_TRANS_GROUP compositor
 * operations in a row mean that we are creating a group inside of a group.
 */
static	bool
c_pdf14trans_equal(const gs_composite_t	* pct0,	const gs_composite_t * pct1)
{
    return false;
}

#ifdef DEBUG
static const char * pdf14_opcode_names[] = PDF14_OPCODE_NAMES;
#endif

#define put_value(dp, value)\
    BEGIN\
        memcpy(dp, &value, sizeof(value));\
        dp += sizeof(value);\
    END

static inline int
c_pdf14trans_write_ctm(byte **ppbuf, const gs_pdf14trans_params_t *pparams)
{
    /* Note: We can't skip writing CTM if it is equal to pgs->ctm,
       because clist writer may skip this command for some bands.
       For a better result we need individual CTM for each band.
     */
    byte *pbuf = *ppbuf;
    int len, code;

    len = cmd_write_ctm_return_length_nodevice(&pparams->ctm);
    pbuf--; /* For cmd_write_ctm. */
    code = cmd_write_ctm(&pparams->ctm, pbuf, len);
    if (code < 0)
        return code;
    pbuf += len + 1;
    *ppbuf = pbuf;
    return 0;
}

/*
 * Convert a PDF 1.4 transparency compositor to string form for use by the command
 * list device. This is also where we update the pdf14_needed. When set the clist
 * painting procs will update the trans_bbox state for bands that are affected.
*/
static	int
c_pdf14trans_write(const gs_composite_t	* pct, byte * data, uint * psize,
                   gx_device_clist_writer *cdev)
{
    const gs_pdf14trans_params_t * pparams = &((const gs_pdf14trans_t *)pct)->params;
    int need, avail = *psize;
    byte buf[MAX_CLIST_TRANSPARENCY_BUFFER_SIZE]; /* Must be large enough
        to fit the data written below. We don't implement a dynamic check for
        the buffer owerflow, assuming that the consistency is verified in the
        coding phase. See the definition of MAX_CLIST_TRANSPARENCY_BUFFER_SIZE. */
    byte * pbuf = buf;
    int opcode = pparams->pdf14_op;
    int mask_size = 0;
    uint mask_id = 0;
    int code;
    bool found_icc;
    int64_t hashcode = 0;
    cmm_profile_t *icc_profile;
    gsicc_rendering_param_t render_cond;
    cmm_dev_profile_t *dev_profile;
    /* We maintain and update working copies until we actually write the clist */
    int pdf14_needed = cdev->pdf14_needed;
    int trans_group_level = cdev->pdf14_trans_group_level;
    int smask_level = cdev->pdf14_smask_level;
    bool deep = device_is_deep((gx_device *)cdev);

    code = dev_proc((gx_device *) cdev, get_profile)((gx_device *) cdev,
                                                     &dev_profile);
    if (code < 0)
        return code;
    gsicc_extract_profile(GS_UNKNOWN_TAG, dev_profile, &icc_profile,
                          &render_cond);
    *pbuf++ = opcode;			/* 1 byte */
    if (trans_group_level < 0 && opcode != PDF14_PUSH_DEVICE)
        return_error(gs_error_unregistered);	/* prevent spurious transparency ops (Bug 702327) */

    switch (opcode) {
        default:			/* Should not occur. */
            break;
        case PDF14_PUSH_DEVICE:
            trans_group_level = 0;
            cdev->pdf14_smask_level = 0;
            cdev->page_pdf14_needed = false;
            put_value(pbuf, pparams->num_spot_colors);
            put_value(pbuf, pparams->num_spot_colors_int);
            put_value(pbuf, pparams->overprint_sim_push);
            put_value(pbuf, pparams->is_pattern);

            /* If we happen to be going to a color space like CIELAB then
               we are going to do our blending in default RGB and convert
               to CIELAB at the end.  To do this, we need to store the
               default RGB profile in the clist so that we can grab it
               later on during the clist read back and put image command */
            if (icc_profile->data_cs == gsCIELAB || icc_profile->islab) {
                /* Get the default RGB profile.  Set the device hash code
                   so that we can extract it during the put_image operation. */
                cdev->trans_dev_icc_hash = gsicc_get_hash(pparams->iccprofile);
                found_icc =
                    clist_icc_searchtable(cdev, gsicc_get_hash(pparams->iccprofile));
                if (!found_icc) {
                    /* Add it to the table */
                    clist_icc_addentry(cdev, gsicc_get_hash(pparams->iccprofile),
                                       pparams->iccprofile);
                }
            }
            break;
        case PDF14_POP_DEVICE:
            pdf14_needed = false;		/* reset pdf14_needed */
            trans_group_level = -1;		/* reset so we need to PUSH_DEVICE next */
            smask_level = 0;
            put_value(pbuf, pparams->is_pattern);
            break;
        case PDF14_END_TRANS_GROUP:
        case PDF14_END_TRANS_TEXT_GROUP:
            trans_group_level--;	/* if now at page level, pdf14_needed will be updated */
            if (smask_level == 0 && trans_group_level == 0)
                pdf14_needed = cdev->page_pdf14_needed;
            break;			/* No data */
        case PDF14_BEGIN_TRANS_PAGE_GROUP:
        case PDF14_BEGIN_TRANS_GROUP:
            pdf14_needed = true;		/* the compositor will be needed while reading */
            trans_group_level++;
            code = c_pdf14trans_write_ctm(&pbuf, pparams);
            if (code < 0)
                return code;
            *pbuf++ = (pparams->Isolated & 1) + ((pparams->Knockout & 1) << 1);
            *pbuf++ = pparams->blend_mode;
            *pbuf++ = pparams->group_color_type;
            *pbuf++ = pparams->page_group;
            put_value(pbuf, pparams->group_color_numcomps);
            put_value(pbuf, pparams->opacity);
            put_value(pbuf, pparams->shape);
            put_value(pbuf, pparams->bbox);
            put_value(pbuf, pparams->shade_group);
            put_value(pbuf, pparams->text_group);
            mask_id = pparams->mask_id;
            put_value(pbuf, mask_id);
            /* Color space information maybe ICC based
               in this case we need to store the ICC
               profile or the ID if it is cached already */
            if (pparams->group_color_type == ICC) {
                /* Check if it is already in the ICC clist table */
                hashcode = gsicc_get_hash(pparams->iccprofile);
                found_icc = clist_icc_searchtable(cdev, hashcode);
                if (!found_icc) {
                    /* Add it to the table */
                    clist_icc_addentry(cdev, hashcode, pparams->iccprofile);
                    put_value(pbuf, hashcode);
                } else {
                    /* It will be in the clist. Just write out the hashcode */
                    put_value(pbuf, hashcode);
                }
            } else {
                put_value(pbuf, hashcode);
            }
            break;
        case PDF14_BEGIN_TRANS_MASK:
            if (pparams->subtype != TRANSPARENCY_MASK_None) {
                pdf14_needed = true;		/* the compositor will be needed while reading */
                smask_level++;
            }
            code = c_pdf14trans_write_ctm(&pbuf, pparams);
            if (code < 0)
                return code;
            put_value(pbuf, pparams->subtype);
            *pbuf++ = pparams->group_color_type;
            put_value(pbuf, pparams->group_color_numcomps);
            *pbuf++ = pparams->replacing;
            *pbuf++ = (pparams->function_is_identity) | (deep<<1);
            *pbuf++ = pparams->Background_components;
            *pbuf++ = pparams->Matte_components;
            put_value(pbuf, pparams->bbox);
            mask_id = pparams->mask_id;
            put_value(pbuf, mask_id);
            if (pparams->Background_components) {
                const int l = sizeof(pparams->Background[0]) * pparams->Background_components;

                memcpy(pbuf, pparams->Background, l);
                pbuf += l;
                memcpy(pbuf, &pparams->GrayBackground, sizeof(pparams->GrayBackground));
                pbuf += sizeof(pparams->GrayBackground);
            }
            if (pparams->Matte_components) {
                const int m = sizeof(pparams->Matte[0]) * pparams->Matte_components;

                memcpy(pbuf, pparams->Matte, m);
                pbuf += m;
            }
            if (!pparams->function_is_identity)
                mask_size = (256+deep)<<deep;
            /* Color space information may be ICC based
               in this case we need to store the ICC
               profile or the ID if it is cached already */
            if (pparams->group_color_type == ICC) {
                /* Check if it is already in the ICC clist table */
                hashcode = gsicc_get_hash(pparams->iccprofile);
                found_icc = clist_icc_searchtable(cdev, hashcode);
                if (!found_icc) {
                    /* Add it to the table */
                    clist_icc_addentry(cdev, hashcode, pparams->iccprofile);
                    put_value(pbuf, hashcode);
                } else {
                    /* It will be in the clist. Just write out the hashcode */
                    put_value(pbuf, hashcode);
                }
            } else {
                put_value(pbuf, hashcode);
            }
            break;
        case PDF14_END_TRANS_MASK:
            smask_level--;
            if (smask_level == 0 && trans_group_level == 0)
                pdf14_needed = cdev->page_pdf14_needed;
            break;
        case PDF14_SET_BLEND_PARAMS:
            if (pparams->blend_mode != BLEND_MODE_Normal || pparams->opacity != 1.0 ||
                pparams->shape != 1.0)
                pdf14_needed = true;		/* the compositor will be needed while reading */
            else if (smask_level == 0 && trans_group_level == 0)
                pdf14_needed = false;		/* At page level, set back to false */
            if (smask_level == 0 && trans_group_level == 0)
                cdev->page_pdf14_needed = pdf14_needed;         /* save for after popping to page level */
            /* Changed is now two bytes due to overprint stroke fill. Write as int */
            put_value(pbuf, pparams->changed);
            if (pparams->changed & PDF14_SET_BLEND_MODE)
                *pbuf++ = pparams->blend_mode;
            if (pparams->changed & PDF14_SET_TEXT_KNOCKOUT)
                *pbuf++ = pparams->text_knockout;
            if (pparams->changed & PDF14_SET_AIS)
                put_value(pbuf, pparams->ais);
            if (pparams->changed & PDF14_SET_OVERPRINT)
                put_value(pbuf, pparams->overprint);
            if (pparams->changed & PDF14_SET_STROKEOVERPRINT)
                put_value(pbuf, pparams->stroke_overprint);
            if (pparams->changed & PDF14_SET_FILLCONSTANTALPHA)
                put_value(pbuf, pparams->fillconstantalpha);
            if (pparams->changed & PDF14_SET_STROKECONSTANTALPHA)
                put_value(pbuf, pparams->strokeconstantalpha);
            if (pparams->changed & PDF_SET_FILLSTROKE_STATE)
                put_value(pbuf, pparams->op_fs_state);
            break;
        case PDF14_PUSH_TRANS_STATE:
            break;
        case PDF14_POP_TRANS_STATE:
            break;
        case PDF14_PUSH_SMASK_COLOR:
            return 0;   /* We really should never be here */
            break;
        case PDF14_POP_SMASK_COLOR:
            return 0;   /* We really should never be here */
            break;
    }

    /* check for fit */
    need = (pbuf - buf) + mask_size;
    *psize = need;
    if (need > avail) {
        if (avail)
            return_error(gs_error_rangecheck);
        else
            return gs_error_rangecheck;
    }

    /* If we are writing more than the maximum ever expected,
     * return a rangecheck error. Second check is for Coverity
     */
    if ((need + 3 > MAX_CLIST_COMPOSITOR_SIZE) ||
        (need + 3 - mask_size > MAX_CLIST_TRANSPARENCY_BUFFER_SIZE) )
        return_error(gs_error_rangecheck);

    /* Copy our serialized data into the output buffer */
    memcpy(data, buf, need - mask_size);
    if (mask_size)	/* Include the transfer mask data if present */
        memcpy(data + need - mask_size, pparams->transfer_fn, mask_size);
    if_debug3m('v', cdev->memory,
               "[v] c_pdf14trans_write: opcode = %s mask_id=%d need = %d\n",
               pdf14_opcode_names[opcode], mask_id, need);
    cdev->pdf14_needed = pdf14_needed;          /* all OK to update */
    cdev->pdf14_trans_group_level = trans_group_level;
    cdev->pdf14_smask_level = smask_level;
    return 0;
}

#undef put_value

/* Function prototypes */
static int gs_create_pdf14trans( gs_composite_t ** ppct,
                const gs_pdf14trans_params_t * pparams,
                gs_memory_t * mem );

#define	read_value(dp, value)\
    BEGIN\
        memcpy(&value, dp, sizeof(value));\
        dp += sizeof(value);\
    END

/*
 * Convert the string representation of the PDF 1.4 transparency parameter
 * into the full compositor.
 */
static	int
c_pdf14trans_read(gs_composite_t * * ppct, const byte *	data,
                                uint size, gs_memory_t * mem )
{
    gs_pdf14trans_params_t params = {0};
    const byte * start = data;
    int used, code = 0;
    bool deep;

    if (size < 1)
        return_error(gs_error_rangecheck);

    /* Read PDF 1.4 compositor data from the clist */
    params.pdf14_op = *data++;
    if_debug2m('v', mem, "[v] c_pdf14trans_read: opcode = %s  avail = %d",
               pdf14_opcode_names[params.pdf14_op], size);
    memset(&params.ctm, 0, sizeof(params.ctm));
    switch (params.pdf14_op) {
        default:			/* Should not occur. */
            break;
        case PDF14_PUSH_DEVICE:
            read_value(data, params.num_spot_colors);
            read_value(data, params.num_spot_colors_int);
            read_value(data, params.overprint_sim_push);
            read_value(data, params.is_pattern);
            break;
        case PDF14_ABORT_DEVICE:
            break;
        case PDF14_POP_DEVICE:
            read_value(data, params.is_pattern);
            break;
        case PDF14_END_TRANS_GROUP:
        case PDF14_END_TRANS_TEXT_GROUP:
#ifdef DEBUG
            code += 0; /* A good place for a breakpoint. */
#endif
            break;			/* No data */
        case PDF14_PUSH_TRANS_STATE:
            break;
        case PDF14_POP_TRANS_STATE:
            break;
        case PDF14_BEGIN_TRANS_PAGE_GROUP:
        case PDF14_BEGIN_TRANS_GROUP:
            /*
             * We are currently not using the bbox or the colorspace so they were
             * not placed in the clist
             */
            data = cmd_read_matrix(&params.ctm, data);
            params.Isolated = (*data) & 1;
            params.Knockout = (*data++ >> 1) & 1;
            params.blend_mode = *data++;
            params.group_color_type = *data++;  /* Trans group color */
            params.page_group = *data++;
            read_value(data,params.group_color_numcomps);  /* color group size */
            read_value(data, params.opacity);
            read_value(data, params.shape);
            read_value(data, params.bbox);
            read_value(data, params.shade_group);
            read_value(data, params.text_group);
            read_value(data, params.mask_id);
            read_value(data, params.icc_hash);
            break;
        case PDF14_BEGIN_TRANS_MASK:
                /* This is the largest transparency parameter at this time (potentially
                 * 1531 bytes in size if Background_components =
                 * GS_CLIENT_COLOR_MAX_COMPONENTS and Matte_components =
                 * GS_CLIENT_COLOR_MAX_COMPONENTS and we have a transfer function as well).
                 *
                 * NOTE:
                 * The clist reader must be able to handle this sized device.
                 * If any changes are made here the #define MAX_CLIST_COMPOSITOR_SIZE
                 * may also need to be changed correspondingly (defined in gstparam.h)
                 * Also... if another compositor param should exceed this size, this
                 * same condition applies.
                 */
            data = cmd_read_matrix(&params.ctm, data);
            read_value(data, params.subtype);
            params.group_color_type = *data++;
            read_value(data, params.group_color_numcomps);
            params.replacing = *data++;
            params.function_is_identity = *data & 1;
            deep = (*data++)>>1;
            params.Background_components = *data++;
            params.Matte_components = *data++;
            read_value(data, params.bbox);
            read_value(data, params.mask_id);
            if (params.Background_components) {
                const int l = sizeof(params.Background[0]) * params.Background_components;

                memcpy(params.Background, data, l);
                data += l;
                memcpy(&params.GrayBackground, data, sizeof(params.GrayBackground));
                data += sizeof(params.GrayBackground);
            }
            if (params.Matte_components) {
                const int m = sizeof(params.Matte[0]) * params.Matte_components;

                memcpy(params.Matte, data, m);
                data += m;
            }
            read_value(data, params.icc_hash);
            if (params.function_is_identity) {
                int i;

                if (deep) {
                    for (i = 0; i < MASK_TRANSFER_FUNCTION_SIZE; i++)
                        ((uint16_t *)params.transfer_fn)[i] = i*0x10000/MASK_TRANSFER_FUNCTION_SIZE;
                    ((uint16_t *)params.transfer_fn)[i] = 0xffff;
                } else {
                    for (i = 0; i < MASK_TRANSFER_FUNCTION_SIZE; i++) {
                        params.transfer_fn[i] = (byte)floor(i *
                            (255.0 / (MASK_TRANSFER_FUNCTION_SIZE - 1)) + 0.5);
                    }
                }
            } else {
                memcpy(params.transfer_fn, data, (256+deep)<<deep);
                data += (256+deep)<<deep;
            }
            break;
        case PDF14_END_TRANS_MASK:
            break;
        case PDF14_PUSH_SMASK_COLOR:
            return 0;
            break;
        case PDF14_POP_SMASK_COLOR:
            return 0;
            break;
        case PDF14_SET_BLEND_PARAMS:
            read_value(data, params.changed);
            if (params.changed & PDF14_SET_BLEND_MODE)
                params.blend_mode = *data++;
            if (params.changed & PDF14_SET_TEXT_KNOCKOUT)
                params.text_knockout = *data++;
            if (params.changed & PDF14_SET_AIS)
                read_value(data, params.ais);
            if (params.changed & PDF14_SET_OVERPRINT)
                read_value(data, params.overprint);
            if (params.changed & PDF14_SET_STROKEOVERPRINT)
                read_value(data, params.stroke_overprint);
            if (params.changed & PDF14_SET_FILLCONSTANTALPHA)
                read_value(data, params.fillconstantalpha);
            if (params.changed & PDF14_SET_STROKECONSTANTALPHA)
                read_value(data, params.strokeconstantalpha);
            if (params.changed & PDF_SET_FILLSTROKE_STATE)
                read_value(data, params.op_fs_state);
            break;
    }
    code = gs_create_pdf14trans(ppct, &params, mem);
    if (code < 0)
        return code;
    used = data - start;
    if_debug2m('v', mem, " mask_id=%d used = %d\n", params.mask_id, used);

    /* If we read more than the maximum expected, return a rangecheck error */
    if ( used + 3 > MAX_CLIST_COMPOSITOR_SIZE )
        return_error(gs_error_rangecheck);
    else
        return used;
}

/*
 * Adjust the compositor's CTM.
 */
static int
c_pdf14trans_adjust_ctm(gs_composite_t * pct0, int x0, int y0, gs_gstate *pgs)
{
    gs_pdf14trans_t *pct = (gs_pdf14trans_t *)pct0;
    gs_matrix mat = pct->params.ctm;

    if_debug6m('L', pgs->memory, " [%g %g %g %g %g %g]\n",
               mat.xx, mat.xy, mat.yx, mat.yy,
               mat.tx, mat.ty);
    mat.tx -= x0;
    mat.ty -= y0;
    gs_gstate_setmatrix(pgs, &mat);
    return 0;
}

/*
 * Create a PDF 1.4 transparency compositor.
 *
 * Note that this routine will be called only if the device is not already
 * a PDF 1.4 transparency compositor.
 * Return an error if it is not a PDF14_PUSH_DEVICE operation.
 */
static	int
c_pdf14trans_create_default_compositor(const gs_composite_t * pct,
    gx_device ** pp14dev, gx_device * tdev, gs_gstate * pgs,
    gs_memory_t * mem)
{
    const gs_pdf14trans_t * pdf14pct = (const gs_pdf14trans_t *) pct;
    int code = 0;

    /*
     * We only handle the push operation.  All other operations are ignored.
     * The other operations will be handled by the composite routine
     * for the PDF 1.4 compositing device.
     */
    switch (pdf14pct->params.pdf14_op) {
        case PDF14_PUSH_DEVICE:
            code = gs_pdf14_device_push(mem, pgs, pp14dev, tdev, pdf14pct);
            /* Change (non-error) code to 1 to indicate that we created
             * a device. */
            if (code >= 0)
                code = 1;
            break;
        default:
	    /* No other compositor actions are allowed if this isn't a pdf14 compositor */
            *pp14dev = NULL;
	    return_error(gs_error_unregistered);
    }
    return code;
}

/*
 * Find an opening compositor op.
 */
static gs_compositor_closing_state
find_opening_op(int opening_op, gs_composite_t **ppcte,
                gs_compositor_closing_state return_code)
{
    /* Assuming a right *BEGIN* - *END* operation balance. */
    gs_composite_t *pcte = *ppcte;

    for (;;) {
        if (pcte->type->comp_id == GX_COMPOSITOR_PDF14_TRANS) {
            gs_pdf14trans_t *pct = (gs_pdf14trans_t *)pcte;
            int op = pct->params.pdf14_op;

            *ppcte = pcte;
            if (op == opening_op)
                return return_code;
            if (op != PDF14_SET_BLEND_PARAMS) {
                if (opening_op == PDF14_BEGIN_TRANS_MASK)
                    return COMP_ENQUEUE;
                if (opening_op == PDF14_BEGIN_TRANS_GROUP || opening_op == PDF14_BEGIN_TRANS_PAGE_GROUP) {
                    if (op != PDF14_BEGIN_TRANS_MASK && op != PDF14_END_TRANS_MASK)
                        return COMP_ENQUEUE;
                }
                if (opening_op == PDF14_PUSH_DEVICE) {
                    if (op != PDF14_BEGIN_TRANS_MASK && op != PDF14_END_TRANS_MASK &&
                        op != PDF14_BEGIN_TRANS_GROUP && op != PDF14_BEGIN_TRANS_PAGE_GROUP && op != PDF14_END_TRANS_GROUP &&
                        op != PDF14_END_TRANS_TEXT_GROUP)
                        return COMP_ENQUEUE;
                }
            }
        } else
            return COMP_ENQUEUE;
        pcte = pcte->prev;
        if (pcte == NULL)
            return COMP_EXEC_QUEUE; /* Not in queue. */
    }
}

/*
 * Find an opening compositor op.
 */
static gs_compositor_closing_state
find_same_op(const gs_composite_t *composite_action, int my_op, gs_composite_t **ppcte)
{
    const gs_pdf14trans_t *pct0 = (gs_pdf14trans_t *)composite_action;
    gs_composite_t *pct = *ppcte;

    for (;;) {
        if (pct->type->comp_id == GX_COMPOSITOR_PDF14_TRANS) {
            gs_pdf14trans_t *pct_pdf14 = (gs_pdf14trans_t *)pct;

            *ppcte = pct;
            if (pct_pdf14->params.pdf14_op != my_op)
                return COMP_ENQUEUE;
            if (pct_pdf14->params.csel == pct0->params.csel) {
                /* If the new parameters completely replace the old ones
                   then remove the old one from the queu */
                if ((pct_pdf14->params.changed & pct0->params.changed) ==
                    pct_pdf14->params.changed) {
                    return COMP_REPLACE_CURR;
                } else {
                    return COMP_ENQUEUE;
                }
            }
        } else
            return COMP_ENQUEUE;
        pct = pct->prev;
        if (pct == NULL)
            return COMP_ENQUEUE; /* Not in queue. */
    }
}

/*
 * Check for closing compositor.
 */
static gs_compositor_closing_state
c_pdf14trans_is_closing(const gs_composite_t * composite_action, gs_composite_t ** ppcte,
                        gx_device *dev)
{
    gs_pdf14trans_t *pct0 = (gs_pdf14trans_t *)composite_action;
    int op0 = pct0->params.pdf14_op;

    switch (op0) {
        default: return_error(gs_error_unregistered); /* Must not happen. */
        case PDF14_PUSH_DEVICE:
            return COMP_ENQUEUE;
        case PDF14_ABORT_DEVICE:
            return COMP_ENQUEUE;
        case PDF14_POP_DEVICE:
            if (*ppcte == NULL)
                return COMP_ENQUEUE;
            else {
                gs_compositor_closing_state state = find_opening_op(PDF14_PUSH_DEVICE, ppcte, COMP_EXEC_IDLE);

                if (state == COMP_EXEC_IDLE)
                    return COMP_DROP_QUEUE;
                return state;
            }
        case PDF14_BEGIN_TRANS_PAGE_GROUP:
        case PDF14_BEGIN_TRANS_GROUP:
            return COMP_ENQUEUE;
        case PDF14_END_TRANS_GROUP:
        case PDF14_END_TRANS_TEXT_GROUP:
            if (*ppcte == NULL)
                return COMP_EXEC_QUEUE;
            return find_opening_op(PDF14_BEGIN_TRANS_GROUP, ppcte, COMP_MARK_IDLE);
        case PDF14_BEGIN_TRANS_MASK:
            return COMP_ENQUEUE;
        case PDF14_PUSH_TRANS_STATE:
            return COMP_ENQUEUE;
        case PDF14_POP_TRANS_STATE:
            return COMP_ENQUEUE;
        case PDF14_PUSH_SMASK_COLOR:
            return COMP_ENQUEUE;
            break;
        case PDF14_POP_SMASK_COLOR:
            return COMP_ENQUEUE;
            break;
        case PDF14_END_TRANS_MASK:
            if (*ppcte == NULL)
                return COMP_EXEC_QUEUE;
            return find_opening_op(PDF14_BEGIN_TRANS_MASK, ppcte, COMP_MARK_IDLE);
        case PDF14_SET_BLEND_PARAMS:
            if (*ppcte == NULL)
                return COMP_ENQUEUE;
            /* hack : ignore csel - here it is always zero : */
            return find_same_op(composite_action, PDF14_SET_BLEND_PARAMS, ppcte);
    }
}

/*
 * Check whether a next operation is friendly to the compositor.
 */
static bool
c_pdf14trans_is_friendly(const gs_composite_t * composite_action, byte cmd0, byte cmd1)
{
    gs_pdf14trans_t *pct0 = (gs_pdf14trans_t *)composite_action;
    int op0 = pct0->params.pdf14_op;

    if (op0 == PDF14_PUSH_DEVICE || op0 == PDF14_END_TRANS_GROUP ||
        op0 == PDF14_END_TRANS_TEXT_GROUP) {
        /* Halftone commands are always passed to the target printer device,
           because transparency buffers are always contone.
           So we're safe to execute them before queued transparency compositors. */
        if (cmd0 == cmd_opv_extend && (cmd1 == cmd_opv_ext_put_halftone ||
                                       cmd1 == cmd_opv_ext_put_ht_seg))
            return true;
        if (cmd0 == cmd_opv_set_misc && (cmd1 >> 6) == (cmd_set_misc_map >> 6))
            return true;
    }
    return false;
}

static composite_create_default_compositor_proc(c_pdf14trans_create_default_compositor);
static composite_equal_proc(c_pdf14trans_equal);
static composite_write_proc(c_pdf14trans_write);
static composite_read_proc(c_pdf14trans_read);
static composite_adjust_ctm_proc(c_pdf14trans_adjust_ctm);
static composite_is_closing_proc(c_pdf14trans_is_closing);
static composite_is_friendly_proc(c_pdf14trans_is_friendly);
static composite_clist_write_update(c_pdf14trans_clist_write_update);
static composite_clist_read_update(c_pdf14trans_clist_read_update);
static composite_get_cropping_proc(c_pdf14trans_get_cropping);

/*
 * Methods for the PDF 1.4 transparency compositor
 *
 * Note:  We have two set of methods.  They are the same except for the
 * composite_clist_write_update method.  Once the clist write device is created,
 * we use the second set of procedures.  This prevents the creation of multiple
 * PDF 1.4 clist write compositor devices being chained together.
 */
const gs_composite_type_t   gs_composite_pdf14trans_type = {
    GX_COMPOSITOR_PDF14_TRANS,
    {
        c_pdf14trans_create_default_compositor, /* procs.create_default_compositor */
        c_pdf14trans_equal,                      /* procs.equal */
        c_pdf14trans_write,                      /* procs.write */
        c_pdf14trans_read,                       /* procs.read */
        c_pdf14trans_adjust_ctm,		 /* procs.adjust_ctm */
        c_pdf14trans_is_closing,                 /* procs.is_closing */
        c_pdf14trans_is_friendly,                /* procs.is_friendly */
                /* Create a PDF 1.4 clist write device */
        c_pdf14trans_clist_write_update,   /* procs.composite_clist_write_update */
        c_pdf14trans_clist_read_update,	   /* procs.composite_clist_read_update */
        c_pdf14trans_get_cropping	   /* procs.composite_get_cropping */
    }                                            /* procs */
};

const gs_composite_type_t   gs_composite_pdf14trans_no_clist_writer_type = {
    GX_COMPOSITOR_PDF14_TRANS,
    {
        c_pdf14trans_create_default_compositor, /* procs.create_default_compositor */
        c_pdf14trans_equal,                      /* procs.equal */
        c_pdf14trans_write,                      /* procs.write */
        c_pdf14trans_read,                       /* procs.read */
        c_pdf14trans_adjust_ctm,		 /* procs.adjust_ctm */
        c_pdf14trans_is_closing,                 /* procs.is_closing */
        c_pdf14trans_is_friendly,                /* procs.is_friendly */
                /* The PDF 1.4 clist writer already exists, Do not create it. */
        gx_default_composite_clist_write_update, /* procs.composite_clist_write_update */
        c_pdf14trans_clist_read_update,	   /* procs.composite_clist_read_update */
        c_pdf14trans_get_cropping	   /* procs.composite_get_cropping */
    }                                            /* procs */
};

/*
 * Verify that a compositor data structure is for the PDF 1.4 compositor.
 */
int
gs_is_pdf14trans_compositor(const gs_composite_t * pct)
{
    return (pct->type == &gs_composite_pdf14trans_type
                || pct->type == &gs_composite_pdf14trans_no_clist_writer_type);
}

/*
 * Create a PDF 1.4 transparency compositor data structure.
 */
static int
gs_create_pdf14trans(
    gs_composite_t **               ppct,
    const gs_pdf14trans_params_t *  pparams,
    gs_memory_t *                   mem )
{
    gs_pdf14trans_t *                pct;

    pct = gs_alloc_struct(mem, gs_pdf14trans_t, &st_pdf14trans,
                             "gs_create_pdf14trans");
    if (pct == NULL)
        return_error(gs_error_VMerror);
    pct->type = &gs_composite_pdf14trans_type;
    pct->id = gs_next_ids(mem, 1);
    pct->params = *pparams;
    pct->idle = false;
    *ppct = (gs_composite_t *)pct;
    return 0;
}

/*
 * Send a PDF 1.4 transparency compositor action to the specified device.
 */
int
send_pdf14trans(gs_gstate	* pgs, gx_device * dev,
    gx_device * * pcdev, gs_pdf14trans_params_t * pparams, gs_memory_t * mem)
{
    gs_composite_t * pct = NULL;
    int code;

    pparams->ctm = ctm_only(pgs);
    code = gs_create_pdf14trans(&pct, pparams, mem);
    if (code < 0)
        return code;
    code = dev_proc(dev, composite) (dev, pcdev, pct, pgs, mem, NULL);
    if (code == gs_error_handled)
        code = 0;

    gs_free_object(pgs->memory, pct, "send_pdf14trans");

    return code;
}

/* ------------- PDF 1.4 transparency device for clist writing ------------- */

/*
 * The PDF 1.4 transparency compositor device may have a different process
 * color model than the output device.  If we are banding then we need to
 * create two compositor devices.  The output side (clist reader) needs a
 * compositor to actually composite the output.  We also need a compositor
 * device before the clist writer.  This is needed to provide a process color
 * model which matches the PDF 1.4 blending space.
 *
 * This section provides support for this device.
 */

/*
 * Define the default pre-clist (clist writer) PDF 1.4 compositing device.
 * We actually use the same structure for both the clist writer and reader
 * devices.  However we use separate names to identify the routines for each
 * device.
 */

static	dev_proc_composite(pdf14_clist_composite);
static	dev_proc_composite(pdf14_clist_forward_composite);
static	dev_proc_fill_path(pdf14_clist_fill_path);
static	dev_proc_stroke_path(pdf14_clist_stroke_path);
static	dev_proc_fill_stroke_path(pdf14_clist_fill_stroke_path);
static	dev_proc_text_begin(pdf14_clist_text_begin);
static	dev_proc_begin_typed_image(pdf14_clist_begin_typed_image);
static  dev_proc_copy_planes(pdf14_clist_copy_planes);

static void
pdf14_clist_init_procs(gx_device *dev,
                       dev_proc_get_color_mapping_procs(get_color_mapping_procs),
                       dev_proc_get_color_comp_index(get_color_comp_index))
{
    set_dev_proc(dev, get_initial_matrix, gx_forward_get_initial_matrix);
    set_dev_proc(dev, sync_output, gx_forward_sync_output);
    set_dev_proc(dev, output_page, gx_forward_output_page);
    set_dev_proc(dev, close_device, gx_forward_close_device);
    set_dev_proc(dev, map_rgb_color, pdf14_encode_color);
    set_dev_proc(dev, map_color_rgb, pdf14_decode_color);
    set_dev_proc(dev, fill_rectangle, gx_forward_fill_rectangle);
    set_dev_proc(dev, copy_mono, gx_forward_copy_mono);
    set_dev_proc(dev, copy_color, gx_forward_copy_color);
    set_dev_proc(dev, get_params, gx_forward_get_params);
    set_dev_proc(dev, put_params, pdf14_put_params);
    set_dev_proc(dev, map_cmyk_color, pdf14_encode_color);
    set_dev_proc(dev, get_page_device, gx_forward_get_page_device);
    set_dev_proc(dev, copy_alpha, gx_forward_copy_alpha);
    set_dev_proc(dev, fill_path, pdf14_clist_fill_path);
    set_dev_proc(dev, stroke_path, pdf14_clist_stroke_path);
    set_dev_proc(dev, fill_mask, gx_forward_fill_mask);
    set_dev_proc(dev, fill_trapezoid, gx_forward_fill_trapezoid);
    set_dev_proc(dev, fill_parallelogram, gx_forward_fill_parallelogram);
    set_dev_proc(dev, fill_triangle, gx_forward_fill_triangle);
    set_dev_proc(dev, draw_thin_line, gx_forward_draw_thin_line);
    set_dev_proc(dev, strip_tile_rectangle, gx_forward_strip_tile_rectangle);
    set_dev_proc(dev, strip_copy_rop2, gx_forward_strip_copy_rop2);
    set_dev_proc(dev, get_clipping_box, gx_forward_get_clipping_box);
    set_dev_proc(dev, begin_typed_image, pdf14_clist_begin_typed_image);
    set_dev_proc(dev, get_bits_rectangle, gx_forward_get_bits_rectangle);
    set_dev_proc(dev, composite, pdf14_clist_composite);
    set_dev_proc(dev, get_hardware_params, gx_forward_get_hardware_params);
    set_dev_proc(dev, text_begin, pdf14_clist_text_begin);
    set_dev_proc(dev, begin_transparency_group, pdf14_begin_transparency_group);
    set_dev_proc(dev, end_transparency_group, pdf14_end_transparency_group);
    set_dev_proc(dev, begin_transparency_mask, pdf14_begin_transparency_mask);
    set_dev_proc(dev, end_transparency_mask, pdf14_end_transparency_mask);
    set_dev_proc(dev, discard_transparency_layer, gx_default_discard_transparency_layer);
    set_dev_proc(dev, get_color_mapping_procs, get_color_mapping_procs);
    set_dev_proc(dev, get_color_comp_index, get_color_comp_index);
    set_dev_proc(dev, encode_color, pdf14_encode_color);
    set_dev_proc(dev, decode_color, pdf14_decode_color);
    set_dev_proc(dev, fill_rectangle_hl_color, gx_forward_fill_rectangle_hl_color);
    set_dev_proc(dev, update_spot_equivalent_colors, gx_forward_update_spot_equivalent_colors);
    set_dev_proc(dev, ret_devn_params, gx_forward_ret_devn_params);
    set_dev_proc(dev, fillpage, gx_forward_fillpage);
    set_dev_proc(dev, push_transparency_state, pdf14_push_transparency_state);
    set_dev_proc(dev, pop_transparency_state, pdf14_pop_transparency_state);
    set_dev_proc(dev, dev_spec_op, pdf14_dev_spec_op);
    set_dev_proc(dev, copy_planes, pdf14_clist_copy_planes);
    set_dev_proc(dev, set_graphics_type_tag, gx_forward_set_graphics_type_tag);
    set_dev_proc(dev, copy_alpha_hl_color, gx_forward_copy_alpha_hl_color);
    set_dev_proc(dev, fill_stroke_path, pdf14_clist_fill_stroke_path);
}

static void
pdf14_clist_Gray_initialize_device_procs(gx_device *dev)
{
    pdf14_clist_init_procs(dev,
                           gx_default_DevGray_get_color_mapping_procs,
                           gx_default_DevGray_get_color_comp_index);
}

static void
pdf14_clist_RGB_initialize_device_procs(gx_device *dev)
{
    pdf14_clist_init_procs(dev,
                           gx_default_DevRGB_get_color_mapping_procs,
                           gx_default_DevRGB_get_color_comp_index);
}

static void
pdf14_clist_CMYK_initialize_device_procs(gx_device *dev)
{
    pdf14_clist_init_procs(dev,
                           gx_default_DevCMYK_get_color_mapping_procs,
                           gx_default_DevCMYK_get_color_comp_index);
}

static void
pdf14_clist_CMYKspot_initialize_device_procs(gx_device *dev)
{
    pdf14_clist_init_procs(dev,
                           pdf14_cmykspot_get_color_mapping_procs,
                           pdf14_cmykspot_get_color_comp_index);
}

#if 0 /* NOT USED */
static void
pdf14_clist_RGBspot_initialize_device_procs(gx_device *dev)
{
    pdf14_clist_init_procs(dev,
                           pdf14_rgbspot_get_color_mapping_procs,
                           pdf14_rgbspot_get_color_comp_index);
}

static int
pdf14_clist_Grayspot_initialize_device_procs(gx_device *dev)
{
    pdf14_clist_init_procs(dev,
                           pdf14_grayspot_get_color_mapping_procs,
                           pdf14_grayspot_get_color_comp_index);
}
#endif  /* NOT USED */

const pdf14_clist_device pdf14_clist_Gray_device = {
    std_device_color_stype_body(pdf14_clist_device,
                                pdf14_clist_Gray_initialize_device_procs,
                                "pdf14clistgray",
                                &st_pdf14_device,
                                XSIZE, YSIZE, X_DPI, Y_DPI, 8, 255, 256),
    { 0 },			/* Procs */
    NULL,			/* target */
    { 0 },			/* devn_params - not used */
    &gray_pdf14_procs,
    &gray_blending_procs
};

const pdf14_clist_device pdf14_clist_RGB_device	= {
    std_device_color_stype_body(pdf14_clist_device,
                                pdf14_clist_RGB_initialize_device_procs,
                                "pdf14clistRGB",
                                &st_pdf14_device,
                                XSIZE, YSIZE, X_DPI, Y_DPI, 24, 255, 256),
    { 0 },			/* Procs */
    NULL,			/* target */
    { 0 },			/* devn_params - not used */
    &rgb_pdf14_procs,
    &rgb_blending_procs
};

const pdf14_clist_device pdf14_clist_CMYK_device = {
    std_device_std_color_full_body_type(pdf14_clist_device,
                                        pdf14_clist_CMYK_initialize_device_procs,
                                        "pdf14clistcmyk",
                                        &st_pdf14_device,
                                        XSIZE, YSIZE, X_DPI, Y_DPI, 32,
                                        0, 0, 0, 0, 0, 0),
    { 0 },			/* Procs */
    NULL,			/* target */
    { 0 },			/* devn_params - not used */
    &cmyk_pdf14_procs,
    &cmyk_blending_procs
};

const pdf14_clist_device pdf14_clist_CMYKspot_device = {
    std_device_part1_(pdf14_device,
                      pdf14_clist_CMYKspot_initialize_device_procs,
                      "pdf14clistcmykspot",
                      &st_pdf14_device,
                      open_init_closed),
    dci_values(GX_DEVICE_COLOR_MAX_COMPONENTS,64,255,255,256,256),
    std_device_part2_(XSIZE, YSIZE, X_DPI, Y_DPI),
    offset_margin_values(0, 0, 0, 0, 0, 0),
    std_device_part3_(),
    { 0 },			/* Procs */
    NULL,			/* target */
    /* DeviceN parameters */
    { 8,			/* Not used - Bits per color */
      DeviceCMYKComponents,	/* Names of color model colorants */
      4,			/* Number colorants for CMYK */
      0,			/* MaxSeparations has not been specified */
      -1,			/* PageSpotColors has not been specified */
      {0},			/* SeparationNames */
      0,			/* SeparationOrder names */
      {0, 1, 2, 3, 4, 5, 6, 7 }	/* Initial component SeparationOrder */
    },
    &cmykspot_pdf14_procs,
    &cmyk_blending_procs
};

const pdf14_clist_device pdf14_clist_custom_device = {
    std_device_part1_(pdf14_device,
                      pdf14_clist_CMYKspot_initialize_device_procs,
                      "pdf14clistcustom",
                      &st_pdf14_device,
                      open_init_closed),
    dci_values(GX_DEVICE_COLOR_MAX_COMPONENTS,64,255,255,256,256),
    std_device_part2_(XSIZE, YSIZE, X_DPI, Y_DPI),
    offset_margin_values(0, 0, 0, 0, 0, 0),
    std_device_part3_(),
    { 0 },			/* Procs */
    NULL,			/* target */
    /* DeviceN parameters */
    { 8,			/* Not used - Bits per color */
      DeviceCMYKComponents,	/* Names of color model colorants */
      4,			/* Number colorants for CMYK */
      0,			/* MaxSeparations has not been specified */
      -1,			/* PageSpotColors has not been specified */
      {0},			/* SeparationNames */
      0,			/* SeparationOrder names */
      {0, 1, 2, 3, 4, 5, 6, 7 }	/* Initial component SeparationOrder */
    },
    &custom_pdf14_procs,
    &custom_blending_procs
};

/*
 * the PDF 1.4 transparency spec says that color space for blending
 * operations can be based upon either a color space specified in the
 * group or a default value based upon the output device.  We are
 * currently only using a color space based upon the device.
 */
static	int
get_pdf14_clist_device_proto(gx_device          *dev,
                             pdf14_clist_device *pdevproto,
                             gs_gstate          *pgs,
                       const gs_pdf14trans_t    *pdf14pct,
                             bool                use_pdf14_accum)
{
    bool using_blend_cs;
    pdf14_default_colorspace_t dev_cs =
                pdf14_determine_default_blend_cs(dev, use_pdf14_accum,
                                                 &using_blend_cs);
    bool has_tags = device_encodes_tags(dev);
    bool deep = device_is_deep(dev);
    int num_spots = pdf14pct->params.num_spot_colors;

    /* overprint overide */
    if (pdf14pct->params.overprint_sim_push) {
        using_blend_cs = false;
        if (pdf14pct->params.num_spot_colors_int > 0) {
            dev_cs = PDF14_DeviceCMYKspot;
            num_spots = pdf14pct->params.num_spot_colors_int;
        } else
            dev_cs = PDF14_DeviceCMYK;
    }

    switch (dev_cs) {
        case PDF14_DeviceGray:
           /* We want gray to be single channel.  Low level
               initialization of gray device prototype is
               peculiar in that in dci_std_color_num_components
               the comment is
              "A device is monochrome only if it is bi-level"
              Here we want monochrome anytime we have a gray device.
              To avoid breaking things elsewhere, we will overide
              the prototype intialization here */
            *pdevproto = pdf14_clist_Gray_device;
            pdevproto->color_info.max_components = 1;
            pdevproto->color_info.num_components =
                                    pdevproto->color_info.max_components;
            pdevproto->color_info.max_gray = deep ? 65535 : 255;
            pdevproto->color_info.gray_index = 0; /* Avoid halftoning */
            pdevproto->color_info.dither_grays = pdevproto->color_info.max_gray+1;
            pdevproto->color_info.anti_alias = dev->color_info.anti_alias;
            pdevproto->color_info.depth = deep ? 16 : 8;
            pdevproto->sep_device = false;
            break;
        case PDF14_DeviceRGB:
            *pdevproto = pdf14_clist_RGB_device;
            pdevproto->color_info.anti_alias = dev->color_info.anti_alias;
            pdevproto->sep_device = false;
            if (deep) {
                pdevproto->color_info.depth = 3*16;
                pdevproto->color_info.max_color = 65535;
                pdevproto->color_info.max_gray = 65535;
                pdevproto->color_info.dither_colors = 65536;
                pdevproto->color_info.dither_grays = 65536;
            }
            break;
        case PDF14_DeviceCMYK:
            *pdevproto = pdf14_clist_CMYK_device;
            pdevproto->color_info.anti_alias = dev->color_info.anti_alias;
            pdevproto->sep_device = false;
            if (deep) {
                pdevproto->color_info.depth = 4*16;
                pdevproto->color_info.max_color = 65535;
                pdevproto->color_info.max_gray = 65535;
                pdevproto->color_info.dither_colors = 65536;
                pdevproto->color_info.dither_grays = 65536;
            }
            break;
        case PDF14_DeviceCMYKspot:
            *pdevproto = pdf14_clist_CMYKspot_device;
            /*
             * The number of components for the PDF14 device is the sum
             * of the process components and the number of spot colors
             * for the page. If we are using an NCLR ICC profile at
             * the output device, those spot colors are skipped. They
             * do not appear in the transparency buffer, but appear
             * during put image transform of the page group to the target
             * color space.
             */
            if (num_spots >= 0) {
                pdevproto->devn_params.page_spot_colors = num_spots;
                pdevproto->color_info.num_components =
                    pdevproto->devn_params.num_std_colorant_names + num_spots;
                if (pdevproto->color_info.num_components >
                              pdevproto->color_info.max_components)
                    pdevproto->color_info.num_components =
                              pdevproto->color_info.max_components;
                pdevproto->color_info.depth =
                              pdevproto->color_info.num_components * (8<<deep);
                if (deep && has_tags)
                    pdevproto->color_info.depth -= 8;
            }
            pdevproto->color_info.anti_alias = dev->color_info.anti_alias;
            pdevproto->sep_device = true;
            break;
        case PDF14_DeviceCustom:
            /*
             * We are using the output device's process color model.  The
             * color_info for the PDF 1.4 compositing device needs to match
             * the output device.
             */
            *pdevproto = pdf14_clist_custom_device;
            pdevproto->color_info = dev->color_info;
            /* The pdf14 device has to be 8 (or 16) bit continuous tone. Force it */
            pdevproto->color_info.depth =
                pdevproto->color_info.num_components * (8<<deep);
            pdevproto->color_info.max_gray = deep ? 65535 : 255;
            pdevproto->color_info.max_color = deep ? 65535 : 255;
            pdevproto->color_info.dither_grays = deep ? 65536 : 256;
            pdevproto->color_info.dither_colors = deep ? 65536 : 256;
            pdevproto->color_info.anti_alias = dev->color_info.anti_alias;
            break;
        default:			/* Should not occur */
            return_error(gs_error_rangecheck);
    }
    pdevproto->overprint_sim = pdf14pct->params.overprint_sim_push;
    pdevproto->using_blend_cs = using_blend_cs;
    return 0;
}

static	int
pdf14_create_clist_device(gs_memory_t *mem, gs_gstate * pgs,
                                gx_device ** ppdev, gx_device * target,
                                const gs_pdf14trans_t * pdf14pct)
{
    pdf14_clist_device dev_proto;
    pdf14_clist_device * pdev;
    int code;
    bool has_tags = device_encodes_tags(target);
    cmm_profile_t *target_profile;
    gsicc_rendering_param_t render_cond;
    cmm_dev_profile_t *dev_profile;
    uchar k;
    bool deep = device_is_deep(target);
    cmm_profile_t *icc_profile;


    code = dev_proc(target, get_profile)(target,  &dev_profile);
    if (code < 0)
        return code;
    gsicc_extract_profile(GS_UNKNOWN_TAG, dev_profile, &target_profile,
                          &render_cond);
    if_debug0m('v', pgs->memory, "[v]pdf14_create_clist_device\n");
    code = get_pdf14_clist_device_proto(target, &dev_proto,
                                        pgs, pdf14pct, false);
    if (code < 0)
        return code;
    code = gs_copydevice((gx_device **) &pdev,
                         (const gx_device *) &dev_proto, mem);
    if (code < 0)
        return code;

    /* If we are not using a blending color space, the number of color planes
       should not exceed that of the target */
    if (!(pdev->using_blend_cs || pdev->overprint_sim)) {
        if (pdev->color_info.num_components > target->color_info.num_components)
            pdev->color_info.num_components = target->color_info.num_components;
        if (pdev->color_info.max_components > target->color_info.max_components)
            pdev->color_info.max_components = target->color_info.max_components;
    }
    pdev->color_info.depth = pdev->color_info.num_components * (8<<deep);
    pdev->pad = target->pad;
    pdev->log2_align_mod = target->log2_align_mod;

    if (pdf14pct->params.overprint_sim_push && pdf14pct->params.num_spot_colors_int > 0 && !target->is_planar)
        pdev->is_planar = true;
    else
        pdev->is_planar = target->is_planar;

    pdev->op_state = pgs->is_fill_color ? PDF14_OP_STATE_FILL : PDF14_OP_STATE_NONE;

    if (deep) {
        set_dev_proc(pdev, encode_color, pdf14_encode_color16);
        set_dev_proc(pdev, decode_color, pdf14_decode_color16);
    }
    /* If we have a tag device then go ahead and do a special encoder decoder
       for the pdf14 device to make sure we maintain this information in the
       encoded color information.  We could use the target device's methods but
       the PDF14 device has to maintain 8 bit color always and we could run
       into other issues if the number of colorants became large.  If we need to
       do compressed color with tags that will be a special project at that time */
    if (has_tags) {
        set_dev_proc(pdev, encode_color, pdf14_encode_color_tag);
        pdev->color_info.comp_shift[pdev->color_info.num_components] = pdev->color_info.depth;
        pdev->color_info.depth += 8;
    }
    pdev->color_info.separable_and_linear = GX_CINFO_SEP_LIN_STANDARD;	/* this is the standard */
    gx_device_fill_in_procs((gx_device *)pdev);
    gs_pdf14_device_copy_params((gx_device *)pdev, target);
    gx_device_set_target((gx_device_forward *)pdev, target);

    /* Components shift, etc have to be based upon 8 bit */
    for (k = 0; k < pdev->color_info.num_components; k++) {
        pdev->color_info.comp_bits[k] = 8<<deep;
        pdev->color_info.comp_shift[k] = (pdev->color_info.num_components - 1 - k) * (8<<deep);
    }
    code = dev_proc((gx_device *) pdev, open_device) ((gx_device *) pdev);
    pdev->pclist_device = target;

    code = dev_proc(target, get_profile)(target, &dev_profile);
    if (code < 0)
        return code;
    gsicc_extract_profile(GS_UNKNOWN_TAG, dev_profile, &icc_profile,
        &render_cond);
    if_debug0m('v', mem, "[v]pdf14_create_clist_device\n");

    /* Simulated overprint case.  We have to use CMYK-based profile
       Also if the target profile is NCLR, we are going to use a pdf14
       device that is CMYK based and do the mapping to the NCLR profile
       when the put_image occurs */
    if ((pdev->overprint_sim && icc_profile->data_cs != gsCMYK) ||
         icc_profile->data_cs == gsNCHANNEL) {
        gsicc_adjust_profile_rc(pgs->icc_manager->default_cmyk, 1, "pdf14_create_clist_device");
        gsicc_adjust_profile_rc(pdev->icc_struct->device_profile[GS_DEFAULT_DEVICE_PROFILE],
            -1, "pdf14_create_clist_device");
        pdev->icc_struct->device_profile[GS_DEFAULT_DEVICE_PROFILE] = pgs->icc_manager->default_cmyk;
    } else {
        /* If the target profile was CIELAB, then overide with default RGB for
           proper blending.  During put_image we will convert from RGB to
           CIELAB */
        if ((target_profile->data_cs == gsCIELAB || target_profile->islab) &&
            !pdev->using_blend_cs) {
            rc_assign(pdev->icc_struct->device_profile[GS_DEFAULT_DEVICE_PROFILE],
                pgs->icc_manager->default_rgb, "pdf14_create_clist_device");
        }
    }

    if (pdf14pct->params.overprint_sim_push &&
        pdf14pct->params.num_spot_colors_int > 0) {
        pdev->procs.update_spot_equivalent_colors = pdf14_update_spot_equivalent_colors;
        pdev->procs.ret_devn_params = pdf14_ret_devn_params;
        pdev->op_pequiv_cmyk_colors.all_color_info_valid = false;
        pdev->target_support_devn = pdev->icc_struct->supports_devn;
        pdev->icc_struct->supports_devn = true;  /* Reset when pdf14 device is disabled */
    }

    pdev->my_encode_color = dev_proc(pdev, encode_color);
    pdev->my_decode_color = dev_proc(pdev, decode_color);
    pdev->my_get_color_mapping_procs = dev_proc(pdev, get_color_mapping_procs);
    pdev->my_get_color_comp_index = dev_proc(pdev, get_color_comp_index);
    pdev->color_info.separable_and_linear =
        target->color_info.separable_and_linear;
    *ppdev = (gx_device *) pdev;
    return code;
}

/*
 * Disable the PDF 1.4 clist compositor device.  Once created, the PDF 1.4
 * compositor device is never removed.  (We do not have a remove compositor
 * method.)  However it is no-op'ed when the PDF 1.4 device is popped.  This
 * routine implements that action.
 */
static	int
pdf14_disable_clist_device(gs_memory_t *mem, gs_gstate * pgs,
                                gx_device * dev)
{
    gx_device_forward * pdev = (gx_device_forward *)dev;
    gx_device * target = pdev->target;

    if_debug0m('v', pgs->memory, "[v]pdf14_disable_clist_device\n");

    /*
     * To disable the action of this device, we forward all device
     * procedures to the target except the composite and copy
     * the target's color_info.
     */
    dev->color_info = target->color_info;
    pdf14_forward_device_procs(dev);
    set_dev_proc(dev, composite, pdf14_clist_forward_composite);
    return 0;
}

/*
 * Recreate the PDF 1.4 clist compositor device.  Once created, the PDF 1.4
 * compositor device is never removed.  (We do not have a remove compositor
 * method.)  However it is no-op'ed when the PDF 1.4 device is popped.  This
 * routine will re-enable the compositor if the PDF 1.4 device is pushed
 * again.
 */
static	int
pdf14_recreate_clist_device(gs_memory_t	*mem, gs_gstate *	pgs,
                gx_device * dev, const gs_pdf14trans_t * pdf14pct)
{
    pdf14_clist_device * pdev = (pdf14_clist_device *)dev;
    gx_device * target = pdev->target;
    pdf14_clist_device dev_proto;
    int code;

    if_debug0m('v', pgs->memory, "[v]pdf14_recreate_clist_device\n");
    /*
     * We will not use the entire prototype device but we will set the
     * color related info to match the prototype.
     */
    code = get_pdf14_clist_device_proto(target, &dev_proto,
                                        pgs, pdf14pct, false);
    if (code < 0)
        return code;
    pdev->color_info = dev_proto.color_info;
    pdev->procs = dev_proto.procs;
    pdev->pad = target->pad;
    pdev->log2_align_mod = target->log2_align_mod;

    if (pdf14pct->params.overprint_sim_push && pdf14pct->params.num_spot_colors_int > 0 && !target->is_planar)
        pdev->is_planar = true;
    else
        pdev->is_planar = target->is_planar;

    pdev->color_info.separable_and_linear = GX_CINFO_SEP_LIN_STANDARD;
    gx_device_fill_in_procs((gx_device *)pdev);
    pdev->save_get_cmap_procs = pgs->get_cmap_procs;
    pgs->get_cmap_procs = pdf14_get_cmap_procs;
    gx_set_cmap_procs(pgs, (gx_device *)pdev);
    check_device_separable((gx_device *)pdev);
    return code;
}

/*
 * devicen params
 */
gs_devn_params *
pdf14_ret_devn_params(gx_device *pdev)
{
    pdf14_device *p14dev = (pdf14_device *)pdev;

    return &(p14dev->devn_params);
}

/*
 * devicen params
 */
gs_devn_params *
pdf14_accum_ret_devn_params(gx_device *pdev)
{
    gx_device_pdf14_accum *p14dev = (gx_device_pdf14_accum *)pdev;

    return &(p14dev->devn_params);
}

static int
pdf14_accum_get_color_comp_index(gx_device * dev,
    const char * pname, int name_size, int component_type)
{
    pdf14_device *p14dev = (pdf14_device *)(((gx_device_pdf14_accum *)dev)->save_p14dev);
    gx_device *target = p14dev->target;
    int colorant_number = devn_get_color_comp_index(dev,
                &(((gx_device_pdf14_accum *)dev)->devn_params),
                &(((gx_device_pdf14_accum *)dev)->equiv_cmyk_colors),
                pname, name_size, component_type, ENABLE_AUTO_SPOT_COLORS);

    if (target != NULL)
        /* colorant_number returned here _should_ be the same as from above */
        colorant_number = (*dev_proc(target, get_color_comp_index))
                              (target, (const char *)pname, name_size, component_type);
    return colorant_number;
}

/*
 * The following procedures are used to map the standard color spaces into
 * the separation color components for the pdf14_accum device.
 */
static void
pdf14_accum_gray_cs_to_cmyk_cm(const gx_device * dev, frac gray, frac out[])
{
    int * map =
      (int *)(&((gx_device_pdf14_accum *) dev)->devn_params.separation_order_map);

    gray_cs_to_devn_cm(dev, map, gray, out);
}

static void
pdf14_accum_rgb_cs_to_cmyk_cm(const gx_device * dev,
    const gs_gstate *pgs, frac r, frac g, frac b, frac out[])
{
    int * map =
      (int *)(&((gx_device_pdf14_accum *) dev)->devn_params.separation_order_map);

    rgb_cs_to_devn_cm(dev, map, pgs, r, g, b, out);
}

static void
pdf14_accum_cmyk_cs_to_cmyk_cm(const gx_device * dev,
    frac c, frac m, frac y, frac k, frac out[])
{
    const int * map =
      (int *)(&((gx_device_pdf14_accum *) dev)->devn_params.separation_order_map);

    cmyk_cs_to_devn_cm(dev, map, c, m, y, k, out);
}

static const gx_cm_color_map_procs pdf14_accum_cm_procs = {
    pdf14_accum_gray_cs_to_cmyk_cm,
    pdf14_accum_rgb_cs_to_cmyk_cm,
    pdf14_accum_cmyk_cs_to_cmyk_cm
};

static const gx_cm_color_map_procs *
pdf14_accum_get_color_mapping_procs(const gx_device * dev, const gx_device **map_dev)
{
    *map_dev = dev;
    return &pdf14_accum_cm_procs;
}

/*
 *  Device proc for updating the equivalent CMYK color for spot colors.
 */
static int
pdf14_accum_update_spot_equivalent_colors(gx_device * dev, const gs_gstate * pgs)
{
    gx_device_pdf14_accum *pdev = (gx_device_pdf14_accum *)dev;
    gx_device *tdev = ((pdf14_device *)(pdev->save_p14dev))->target;
    int code = update_spot_equivalent_cmyk_colors(dev, pgs, &pdev->devn_params,
                                              &pdev->equiv_cmyk_colors);

    if (code >= 0 && tdev != NULL)
        code = dev_proc(tdev, update_spot_equivalent_colors)(tdev, pgs);
    return code;
}

/* Used when doing overprint simulation and have spot colors */
static int
pdf14_update_spot_equivalent_colors(gx_device *dev, const gs_gstate *pgs)
{
    pdf14_device *pdev = (pdf14_device *)dev;
    const gs_color_space *pcs;
    int code;

    /* Make sure we are not All or None */
    pcs = gs_currentcolorspace_inline(pgs);
    if (pcs != NULL && pcs->type->index == gs_color_space_index_Separation &&
        pcs->params.separation.sep_type != SEP_OTHER)
            return 0;

    code = update_spot_equivalent_cmyk_colors(dev, pgs, &pdev->devn_params,
        &pdev->op_pequiv_cmyk_colors);
    return code;
}

/*
 * Retrieve a list of spot color names for the PDF14 device.
 */
int
put_param_pdf14_spot_names(gx_device * pdev,
                gs_separations * pseparations, gs_param_list * plist)
{
    int code, num_spot_colors, i;
    gs_param_string str;

    /* Check if the given keyname is present. */
    code = param_read_int(plist, PDF14NumSpotColorsParamName,
                                                &num_spot_colors);
    switch (code) {
        default:
            param_signal_error(plist, PDF14NumSpotColorsParamName, code);
            break;
        case 1:
            return 0;
        case 0:
            if (num_spot_colors < 1 ||
                num_spot_colors > GX_DEVICE_COLOR_MAX_COMPONENTS)
                return_error(gs_error_rangecheck);
            for (i = 0; i < num_spot_colors; i++) {
                char buff[20];
                byte * sep_name;

                gs_sprintf(buff, "PDF14SpotName_%d", i);
                code = param_read_string(plist, buff, &str);
                switch (code) {
                    default:
                        param_signal_error(plist, buff, code);
                        break;
                    case 0:
                        sep_name = gs_alloc_bytes(pdev->memory,
                                str.size, "put_param_pdf14_spot_names");
                        memcpy(sep_name, str.data, str.size);
                        pseparations->names[i].size = str.size;
                        pseparations->names[i].data = sep_name;
                }
            }
            pseparations->num_separations = num_spot_colors;
            break;
    }
    return 0;;
}

/*
 * This procedure will have information from the PDF 1.4 clist writing
 * clist compositior device.  This is information output the compressed
 * color list info which is needed for the support of spot colors in
 * PDF 1.4 compositing.  This info needs to be passed to the PDF 1.4
 * clist reading compositor.  However this device is not created until
 * the clist is read.  To get this info to that device, we have to
 * temporarily store that info in the output device.  This routine saves
 * that info in the output device.
 */
int
pdf14_put_devn_params(gx_device * pdev, gs_devn_params * pdevn_params,
                                        gs_param_list * plist)
{
    int code;
    code = put_param_pdf14_spot_names(pdev,
                       &pdevn_params->pdf14_separations, plist);
    return code;
}

/*
 * When we are banding, we have two PDF 1.4 compositor devices.  One for
 * when we are creating the clist.  The second is for imaging the data from
 * the clist.  This routine is part of the clist writing PDF 1.4 device.
 * This routine is only called once the PDF 1.4 clist write compositor already
 * exists.
 */
static	int
pdf14_clist_composite(gx_device	* dev, gx_device ** pcdev,
    const gs_composite_t * pct, gs_gstate * pgs, gs_memory_t * mem,
    gx_device *cdev)
{
    pdf14_clist_device * pdev = (pdf14_clist_device *)dev;
    int code, is_pdf14_compositor;
    const gs_pdf14trans_t * pdf14pct = (const gs_pdf14trans_t *) pct;
    bool deep = device_is_deep(dev);

    /* We only handle a few PDF 1.4 transparency operations */
    if ((is_pdf14_compositor = gs_is_pdf14trans_compositor(pct)) != 0) {
        switch (pdf14pct->params.pdf14_op) {
            case PDF14_PUSH_DEVICE:
                /* Re-activate the PDF 1.4 compositor */
                pdev->saved_target_color_info = pdev->target->color_info;
                pdev->target->color_info = pdev->color_info;
                pdev->saved_target_encode_color = dev_proc(pdev->target, encode_color);
                pdev->saved_target_decode_color = dev_proc(pdev->target, decode_color);
                set_dev_proc(pdev->target, encode_color, pdev->my_encode_color);
                set_dev_proc(pdev, encode_color, pdev->my_encode_color);
                set_dev_proc(pdev->target, decode_color, pdev->my_decode_color);
                set_dev_proc(pdev, decode_color, pdev->my_decode_color);
                pdev->saved_target_get_color_mapping_procs =
                                        dev_proc(pdev->target, get_color_mapping_procs);
                pdev->saved_target_get_color_comp_index =
                                        dev_proc(pdev->target, get_color_comp_index);
                set_dev_proc(pdev->target, get_color_mapping_procs, pdev->my_get_color_mapping_procs);
                set_dev_proc(pdev, get_color_mapping_procs, pdev->my_get_color_mapping_procs);
                set_dev_proc(pdev->target, get_color_comp_index, pdev->my_get_color_comp_index);
                set_dev_proc(pdev, get_color_comp_index, pdev->my_get_color_comp_index);
                pdev->save_get_cmap_procs = pgs->get_cmap_procs;
                pgs->get_cmap_procs = pdf14_get_cmap_procs;
                gx_set_cmap_procs(pgs, dev);
                code = pdf14_recreate_clist_device(mem, pgs, dev, pdf14pct);
                pdev->blend_mode = pdev->text_knockout = 0;
                pdev->opacity = pdev->shape = 0.0;
                if (code < 0)
                    return code;
                /*
                 * This routine is part of the PDF 1.4 clist write device.
                 * Change the compositor procs to not create another since we
                 * do not need to create a chain of identical devices.
                 */
                {
                    gs_pdf14trans_t pctemp = *pdf14pct;

                    pctemp.type = &gs_composite_pdf14trans_no_clist_writer_type;
                    code = dev_proc(pdev->target, composite)
                                (pdev->target, pcdev, (gs_composite_t *)&pctemp, pgs, mem, cdev);
                    /* We should never have created a new device here. */
                    assert(code != 1);
                    return code;
                }
            case PDF14_POP_DEVICE:
            {
                gx_device *clistdev = pdev->target;

                /* Find the clist device */
                while (1) {
                    gxdso_device_child_request req;
                    /* Ignore any errors here, that's expected as non-clist
                     * devices don't implement it. */
                    code = dev_proc(clistdev, dev_spec_op)(clistdev, gxdso_is_clist_device, NULL, 0);
                    if (code == 1)
                        break;
                    req.n = 0;
                    req.target = clistdev;
                    code = dev_proc(clistdev, dev_spec_op)(clistdev, gxdso_device_child, &req, sizeof(req));
                    if (code < 0)
                        return code;
                    clistdev = req.target;
                }

                /* If we have overprint simulation spot color information, store
                   it in a pseudo-band of the clist */
                if (pdev->overprint_sim &&
                    pdev->devn_params.page_spot_colors > 0) {
                    code = clist_write_op_equiv_cmyk_colors((gx_device_clist_writer *)clistdev,
                        &pdev->op_pequiv_cmyk_colors);
                    if (code < 0)
                        return code;
                }

                /* If we hit an error during an SMask, we need to undo the color
                 * swapping before continuing. pdf14_decrement_smask_color() checks
                 * for itself if it needs to take action.
                 */
                pdf14_decrement_smask_color(pgs, dev);
                /* Restore the color_info for the clist device */
                clistdev->color_info = pdev->saved_target_color_info;
                set_dev_proc(clistdev, encode_color, pdev->saved_target_encode_color);
                set_dev_proc(clistdev, decode_color, pdev->saved_target_decode_color);
                set_dev_proc(clistdev, get_color_mapping_procs, pdev->saved_target_get_color_mapping_procs);
                set_dev_proc(clistdev, get_color_comp_index, pdev->saved_target_get_color_comp_index);
                pgs->get_cmap_procs = pdev->save_get_cmap_procs;
                gx_set_cmap_procs(pgs, clistdev);
                gx_device_decache_colors(clistdev);
                /* Disable the PDF 1.4 compositor */
                pdf14_disable_clist_device(mem, pgs, dev);
                /*
                 * Make sure that the transfer funtions, etc. are current.
                 */
                code = cmd_put_color_mapping((gx_device_clist_writer *)clistdev, pgs);
                if (code < 0)
                    return code;
                break;
            }
            case PDF14_BEGIN_TRANS_PAGE_GROUP:
            case PDF14_BEGIN_TRANS_GROUP:
                if (pdev->smask_constructed || pdev->depth_within_smask)
                    pdev->depth_within_smask++;
                pdev->smask_constructed = 0;
                /*
                 * Keep track of any changes made in the blending parameters.
                   These need to be written out in the same bands as the group
                   information is written.  Hence the passing of the dimensions
                   for the group. */
                code = pdf14_clist_update_params(pdev, pgs, true,
                                                 (gs_pdf14trans_params_t *)&(pdf14pct->params));
                if (code < 0)
                    return code;
                if (pdf14pct->params.Background_components != 0 &&
                    pdf14pct->params.Background_components !=
                    pdev->color_info.num_components)
                    return_error(gs_error_rangecheck);

                /* We need to update the clist writer device procs based upon the
                   the group color space. This ensures the proper color data is
                   written out to the device. For simplicity, the list item is
                   created even if the color space did not change */
                code = pdf14_clist_push_color_model(dev, cdev, pgs, pdf14pct, mem, false);
                if (code < 0)
                    return code;

                break;
            case PDF14_BEGIN_TRANS_MASK:
                /* We need to update the clist writer device procs based upon the
                   the group color space.  For simplicity, the list item is created
                   even if the color space did not change */
                /* First store the current ones */
                if (pdf14pct->params.subtype == TRANSPARENCY_MASK_None)
                    break;

                /* Update the color settings of the clist writer.  Store information in stack */
                code = pdf14_clist_push_color_model(dev, cdev, pgs, pdf14pct, mem, true);
                if (code < 0)
                    return code;

                /* Also, if the BC is a value that may end up as something other
                  than transparent. We must use the parent colors bounding box in
                  determining the range of bands in which this mask can affect.
                  So, if needed change the masks bounding box at this time */
                pdev->in_smask_construction++;
                break;
            case PDF14_BEGIN_TRANS_TEXT_GROUP:
                if (pdev->text_group == PDF14_TEXTGROUP_BT_PUSHED) {
                    emprintf(pdev->memory, "Warning: Text group pushed but no ET found\n");
                    pdev->text_group = PDF14_TEXTGROUP_MISSING_ET;
                } else
                    pdev->text_group = PDF14_TEXTGROUP_BT_NOT_PUSHED;
                *pcdev = dev;
                return 0; /* Never put into clist. Only used during writing */
            case PDF14_END_TRANS_TEXT_GROUP:
                if (pdev->text_group != PDF14_TEXTGROUP_BT_PUSHED) {
                    *pcdev = dev;
                    return 0; /* Avoids spurious ET calls in interpreter */
                }
                pdev->text_group = PDF14_TEXTGROUP_NO_BT; /* These can't be nested */
                code = pdf14_clist_pop_color_model(dev, pgs);
                if (code < 0)
                    return code;
                break;
            case PDF14_END_TRANS_MASK:
                pdev->in_smask_construction--;
                if (pdev->in_smask_construction < 0)
                    pdev->in_smask_construction = 0;
                if (pdev->in_smask_construction == 0)
                    pdev->smask_constructed = 1;
                /* fallthrough */
            case PDF14_END_TRANS_GROUP:
                /* We need to update the clist writer device procs based upon the
                   the group color space. */
                code = pdf14_clist_pop_color_model(dev, pgs);
                if (pdev->depth_within_smask)
                    pdev->depth_within_smask--;
                if (code < 0)
                    return code;
                break;
            case PDF14_PUSH_TRANS_STATE:
                break;
            case PDF14_POP_TRANS_STATE:
                break;
            case PDF14_PUSH_SMASK_COLOR:
                code = pdf14_increment_smask_color(pgs,dev);
                *pcdev = dev;
                return code;  /* Note, this are NOT put in the clist */
                break;
            case PDF14_POP_SMASK_COLOR:
                code = pdf14_decrement_smask_color(pgs,dev);
                *pcdev = dev;
                return code;  /* Note, this are NOT put in the clist */
                break;
            case PDF14_SET_BLEND_PARAMS:
                /* If there is a change we go ahead and apply it to the target */
                code = pdf14_clist_update_params(pdev, pgs, false,
                                                 (gs_pdf14trans_params_t *)&(pdf14pct->params));
                *pcdev = dev;
                return code;
                break;
            case PDF14_ABORT_DEVICE:
                code = gx_abort_trans_device(pgs, dev);
                if (pdev->free_devicen) {
                    devn_free_params(dev);
                }
                pdf14_disable_device(dev);
                pdf14_close(dev);
                *pcdev = dev;
                return code;
                break;
            default:
                break;		/* Pass remaining ops to target */
        }
    }
    code = dev_proc(pdev->target, composite)
                        (pdev->target, pcdev, pct, pgs, mem, cdev);
    /* If we were accumulating into a pdf14-clist-accum device, */
    /* we now have to render the page into it's target device */
    if (is_pdf14_compositor && pdf14pct->params.pdf14_op == PDF14_POP_DEVICE &&
        pdev->target->stype == &st_gx_devn_accum_device) {

        int i, y, rows_used;
        byte *linebuf;
        byte *actual_data;
        gx_device_pdf14_accum *tdev = (gx_device_pdf14_accum *)(pdev->target);     /* the printer class clist device used to accumulate */
        /* get the target device we want to send the image to */
        gx_device *target = ((pdf14_device *)(tdev->save_p14dev))->target;
        gs_image1_t image;
        gs_color_space *pcs;
        gx_image_enum_common_t *info;
        gx_image_plane_t planes;
        gsicc_rendering_param_t render_cond;
        cmm_dev_profile_t *dev_profile;
        bool save_planar = pdev->is_planar;
        gs_devn_params *target_devn_params = dev_proc(target, ret_devn_params)(target);
        int save_num_separations;
        gs_int_rect rect;

        pdev->is_planar = false;		/* so gx_device_raster is for entire chunky pixel line */
        linebuf = gs_alloc_bytes(mem, gx_device_raster((gx_device *)pdev, true), "pdf14-clist_accum pop dev");
        pdev->is_planar = save_planar;

        /* As long as we don't have spot colors, we can use ICC colorspace, but spot
         * colors do require devn support
         */
        if (tdev->color_info.num_components <= 4 ||
             dev_proc(target, dev_spec_op)(target, gxdso_supports_devn, NULL, 0) <= 0) {
            /*
             * Set color space in preparation for sending an image.
             */
            code = gs_cspace_build_ICC(&pcs, NULL, pgs->memory);
            if (code < 0)
                goto put_accum_error;

            /* Need to set this to avoid color management during the
               image color render operation.  Exception is for the special case
               when the destination was CIELAB.  Then we need to convert from
               default RGB to CIELAB in the put image operation.  That will happen
               here as we should have set the profile for the pdf14 device to RGB
               and the target will be CIELAB */
            code = dev_proc(dev, get_profile)(dev,  &dev_profile);
            if (code < 0)
                goto put_accum_error;
            gsicc_extract_profile(GS_UNKNOWN_TAG, dev_profile,
                                  &(pcs->cmm_icc_profile_data), &render_cond);
            /* pcs takes a reference to the profile data it just retrieved. */
            gsicc_adjust_profile_rc(pcs->cmm_icc_profile_data, 1, "pdf14_clist_composite");
            gsicc_set_icc_range(&(pcs->cmm_icc_profile_data));
        } else {
             /* DeviceN case -- need to handle spot colors */
            code = gs_cspace_new_DeviceN(&pcs, tdev->color_info.num_components,
                                         gs_currentcolorspace(pgs), pgs->memory);
            if (code < 0)
                goto put_accum_error;
            /* set up a usable DeviceN space with info from the tdev->devn_params */
            pcs->params.device_n.use_alt_cspace = false;

            if ((code = pcs->type->install_cspace(pcs, pgs)) < 0) {
                goto put_accum_error;
            }
            /* One last thing -- we need to fudge the pgs->color_component_map */
            for (i=0; i < tdev->color_info.num_components; i++)
                pgs->color_component_map.color_map[i] = i;	/* enable all components in normal order */
            /* copy devn_params that were accumulated into the target device's devn_params */
            target_devn_params->bitspercomponent = tdev->devn_params.bitspercomponent;
            target_devn_params->std_colorant_names = tdev->devn_params.std_colorant_names;
            target_devn_params->num_std_colorant_names = tdev->devn_params.num_std_colorant_names;
            target_devn_params->max_separations = tdev->devn_params.max_separations;
            target_devn_params->page_spot_colors = tdev->devn_params.page_spot_colors;
            target_devn_params->num_separation_order_names = tdev->devn_params.num_separation_order_names;
            target_devn_params->separations = tdev->devn_params.separations;
            memcpy(target_devn_params->separation_order_map, tdev->devn_params.separation_order_map,
                   sizeof(gs_separation_map));
            target_devn_params->pdf14_separations = tdev->devn_params.pdf14_separations;
        }
        if (linebuf == NULL) {
            code = gs_error_VMerror;
            goto put_accum_error;
        }
        gs_image_t_init_adjust(&image, pcs, false);
        image.ImageMatrix.xx = (float)pdev->width;
        image.ImageMatrix.yy = (float)pdev->height;
        image.Width = pdev->width;
        image.Height = pdev->height;
        image.BitsPerComponent = 8<<deep;
        ctm_only_writable(pgs).xx = (float)pdev->width;
        ctm_only_writable(pgs).xy = 0;
        ctm_only_writable(pgs).yx = 0;
        ctm_only_writable(pgs).yy = (float)pdev->height;
        ctm_only_writable(pgs).tx = 0.0;
        ctm_only_writable(pgs).ty = 0.0;
        code = dev_proc(target, begin_typed_image) (target,
                                                    pgs, NULL,
                                                    (gs_image_common_t *)&image,
                                                    NULL, NULL, NULL,
                                                    pgs->memory, &info);
        if (code < 0)
            goto put_accum_error;
        rect.p.x = 0;
        rect.q.x = tdev->width;
        for (y=0; y < tdev->height; y++) {
            gs_get_bits_params_t params;

            params.options = (GB_ALIGN_ANY |
                              (GB_RETURN_COPY | GB_RETURN_POINTER) |
                              GB_OFFSET_0 |
                              GB_RASTER_STANDARD | GB_PACKING_CHUNKY |
                              GB_COLORS_NATIVE | GB_ALPHA_NONE);
            params.x_offset = 0;
            params.raster = bitmap_raster(dev->width * dev->color_info.depth);
            params.data[0] = linebuf;
            rect.p.y = y;
            rect.q.y = y+1;
            code = dev_proc(tdev, get_bits_rectangle)((gx_device *)tdev,
                                                      &rect, &params);
            if (code < 0)
                goto put_accum_error;
            actual_data = params.data[0];
            planes.data = actual_data;
            planes.data_x = 0;
            planes.raster = tdev->width * tdev->color_info.num_components;
            if ((code = info->procs->plane_data(info, &planes, 1, &rows_used)) < 0)
                goto put_accum_error;
        }
        code = info->procs->end_image(info, true);

put_accum_error:
        gs_free_object(pdev->memory, linebuf, "pdf14_put_image");
        /* This will also decrement the device profile */
        rc_decrement_only_cs(pcs, "pdf14_put_image");
        dev_proc(tdev, close_device)((gx_device *)tdev);	/* frees the prn_device memory */
        /* Now unhook the clist device and hook to the original so we can clean up */
        gx_device_set_target((gx_device_forward *)pdev,
                             ((gx_device_pdf14_accum *)(pdev->target))->save_p14dev);
        pdev->pclist_device = pdev->target;
        *pcdev = pdev->target;			    /* pass upwards to switch devices */
        pdev->color_info = target->color_info;      /* same as in pdf14_disable_clist */
        if (target_devn_params != NULL) {
            /* prevent devn_free_params from freeing names still in use by target device */
            save_num_separations = tdev->devn_params.separations.num_separations;
            tdev->devn_params.separations.num_separations = 0;
        }
        gs_free_object(tdev->memory, tdev, "popdevice pdf14-accum");
        if (target_devn_params != NULL) {
            target_devn_params->separations.num_separations = save_num_separations;
        }
        return code;		/* DON'T perform set_target */
    }
    if (code == 1) {
        /* We just wrapped pdev->target, so we need to update that.*/
        gx_device_set_target((gx_device_forward *)pdev, *pcdev);
        code = 0; /* We did not wrap dev. */
    }
    *pcdev = dev;
    return code;
}

/*
 * The PDF 1.4 clist compositor is never removed.  (We do not have a 'remove
 * compositor' method.  However the compositor is disabled when we are not
 * doing a page which uses PDF 1.4 transparency.  This routine is only active
 * when the PDF 1.4 compositor is 'disabled'.  It checks for reenabling the
 * PDF 1.4 compositor.  Otherwise it simply passes create compositor requests
 * to the targer.
 */
static	int
pdf14_clist_forward_composite(gx_device	* dev, gx_device * * pcdev,
        const gs_composite_t * pct, gs_gstate * pgs,
        gs_memory_t * mem, gx_device *cdev)
{
    pdf14_device *pdev = (pdf14_device *)dev;
    gx_device * tdev = pdev->target;
    gx_device * ndev;
    int code;

    *pcdev = dev;
    if (gs_is_pdf14trans_compositor(pct)) {
        const gs_pdf14trans_t * pdf14pct = (const gs_pdf14trans_t *) pct;

        if (pdf14pct->params.pdf14_op == PDF14_PUSH_DEVICE)
            return pdf14_clist_composite(dev, &ndev, pct, pgs, mem, cdev);
        return 0;
    }
    code = dev_proc(tdev, composite)(tdev, &ndev, pct, pgs, mem, cdev);
    if (code == 1) {
        /* We just wrapped tdev, so update our target. */
        gx_device_set_target((gx_device_forward *)pdev, ndev);
        code = 0; /* We did not wrap dev. */
    }
    return code;
}

/*
 * If any of the PDF 1.4 transparency blending parameters have changed, we
 * need to send them to the PDF 1.4 compositor on the output side of the clist.
 */
static	int
pdf14_clist_update_params(pdf14_clist_device * pdev, const gs_gstate * pgs,
                          bool crop_blend_params,
                          gs_pdf14trans_params_t *group_params)
{
    gs_pdf14trans_params_t params = { 0 };
    gx_device * pcdev;
    int changed = 0;
    int code = 0;
    gs_composite_t *pct_new = NULL;

    params.crop_blend_params = crop_blend_params;

    params.pdf14_op = PDF14_SET_BLEND_PARAMS;
    if (pgs->blend_mode != pdev->blend_mode) {
        changed |= PDF14_SET_BLEND_MODE;
        params.blend_mode = pdev->blend_mode = pgs->blend_mode;
    }
    if (pgs->text_knockout != pdev->text_knockout) {
        changed |= PDF14_SET_TEXT_KNOCKOUT;
        params.text_knockout = pdev->text_knockout = pgs->text_knockout;
    }
    if (pgs->alphaisshape != pdev->ais) {
        changed |= PDF14_SET_AIS;
        params.ais = pdev->ais = pgs->alphaisshape;
    }
    if (pgs->overprint != pdev->overprint) {
        changed |= PDF14_SET_OVERPRINT;
        params.overprint = pdev->overprint = pgs->overprint;
    }
    if (pgs->stroke_overprint != pdev->stroke_overprint) {
        changed |= PDF14_SET_STROKEOVERPRINT;
        params.stroke_overprint = pdev->stroke_overprint = pgs->stroke_overprint;
    }
    if (pgs->fillconstantalpha != pdev->fillconstantalpha) {
        changed |= PDF14_SET_FILLCONSTANTALPHA;
        params.fillconstantalpha = pdev->fillconstantalpha = pgs->fillconstantalpha;
    }
    if (pgs->strokeconstantalpha != pdev->strokeconstantalpha) {
        changed |= PDF14_SET_STROKECONSTANTALPHA;
        params.strokeconstantalpha = pdev->strokeconstantalpha = pgs->strokeconstantalpha;
    }
    if ((pgs->is_fill_color && pdev->op_state != PDF14_OP_STATE_FILL)) {
        changed |= PDF_SET_FILLSTROKE_STATE;
        params.op_fs_state = pdev->op_state = PDF14_OP_STATE_FILL;
    }
    if ((!pgs->is_fill_color && pdev->op_state != PDF14_OP_STATE_STROKE)) {
        changed |= PDF_SET_FILLSTROKE_STATE;
        params.op_fs_state = pdev->op_state = PDF14_OP_STATE_STROKE;
    }
    if (crop_blend_params) {
        params.ctm = group_params->ctm;
        params.bbox = group_params->bbox;
    }
    params.changed = changed;
    /* Avoid recursion when we have a PDF14_SET_BLEND_PARAMS forced and apply
       now to the target.  Otherwise we send the compositor action
       to the pdf14 device at this time.  This is due to the fact that we
       need to often perform this operation when we are already starting to
       do a compositor action */
    if (changed != 0) {
        code = gs_create_pdf14trans(&pct_new, &params, pgs->memory);
        if (code < 0) return code;
        code = dev_proc(pdev->target, composite)
                    (pdev->target, &pcdev, pct_new, (gs_gstate *)pgs, pgs->memory, NULL);
        gs_free_object(pgs->memory, pct_new, "pdf14_clist_update_params");
    }
    return code;
}

/*
 * fill_path routine for the PDF 1.4 transaprency compositor device for
 * writing the clist.
 */
static	int
pdf14_clist_fill_path(gx_device	*dev, const gs_gstate *pgs,
                           gx_path *ppath, const gx_fill_params *params,
                           const gx_drawing_color *pdcolor,
                           const gx_clip_path *pcpath)
{
    pdf14_clist_device * pdev = (pdf14_clist_device *)dev;
    gs_gstate new_pgs = *pgs;
    int code;
    gs_pattern2_instance_t *pinst = NULL;
    gx_device_forward * fdev = (gx_device_forward *)dev;
    cmm_dev_profile_t *dev_profile, *fwd_profile;
    gsicc_rendering_param_t render_cond;
    cmm_profile_t *icc_profile_fwd, *icc_profile_dev;
    int push_group = 0;

    code = dev_proc(dev, get_profile)(dev,  &dev_profile);
    if (code < 0)
        return code;
    code = dev_proc(fdev->target, get_profile)(fdev->target,  &fwd_profile);
    if (code < 0)
        return code;

    gsicc_extract_profile(GS_UNKNOWN_TAG, fwd_profile, &icc_profile_fwd,
                          &render_cond);
    gsicc_extract_profile(GS_UNKNOWN_TAG, dev_profile, &icc_profile_dev,
                          &render_cond);

    /*
     * Ensure that that the PDF 1.4 reading compositor will have the current
     * blending parameters.  This is needed since the fill_rectangle routines
     * do not have access to the gs_gstate.  Thus we have to pass any
     * changes explictly.
     */
    code = pdf14_clist_update_params(pdev, pgs, false, NULL);
    if (code < 0)
        return code;
    /* If we are doing a shading fill and we are in a transparency group of a
       different color space, then we do not want to do the shading in the
       device color space. It must occur in the source space.  To handle it in
       the device space would require knowing all the nested transparency group
       color space as well as the transparency.  Some of the shading code ignores
       this, so we have to pass on the clist_writer device to enable proper
       mapping to the transparency group color space. */

    if (pdcolor != NULL && gx_dc_is_pattern2_color(pdcolor)) {
        /* Non-idempotent blends require a transparency
         * group to be pushed because shadings might
         * paint several pixels twice. */
        push_group = pgs->fillconstantalpha != 1.0 ||
               !blend_is_idempotent(gs_currentblendmode(pgs));
        pinst =
            (gs_pattern2_instance_t *)pdcolor->ccolor.pattern;
           pinst->saved->has_transparency = true;
           /* The transparency color space operations are driven by the pdf14
              clist writer device.  */
           pinst->saved->trans_device = dev;
    }
    if (push_group) {
        gs_fixed_rect box;
        if (pcpath)
            gx_cpath_outer_box(pcpath, &box);
        else
            (*dev_proc(dev, get_clipping_box)) (dev, &box);
        if (ppath) {
            gs_fixed_rect path_box;

            gx_path_bbox(ppath, &path_box);
            if (box.p.x < path_box.p.x)
                box.p.x = path_box.p.x;
            if (box.p.y < path_box.p.y)
                box.p.y = path_box.p.y;
            if (box.q.x > path_box.q.x)
                box.q.x = path_box.q.x;
            if (box.q.y > path_box.q.y)
                box.q.y = path_box.q.y;
        }
        /* Group alpha set from fill value. push_shfill_group does reset to 1.0 */
        code = push_shfill_group(pdev, &new_pgs, &box);
    } else
        update_lop_for_pdf14(&new_pgs, pdcolor);
    if (code >= 0) {
        new_pgs.trans_device = dev;
        new_pgs.has_transparency = true;
        code = gx_forward_fill_path(dev, &new_pgs, ppath, params, pdcolor, pcpath);
        new_pgs.trans_device = NULL;
        new_pgs.has_transparency = false;
    }
    if (code >= 0 && push_group) {
        code = pop_shfill_group(&new_pgs);
        if (code >= 0)
            code = pdf14_clist_update_params(pdev, pgs, false, NULL);
    }
    if (pinst != NULL){
        pinst->saved->trans_device = NULL;
    }
    return code;
}

/*
 * stroke_path routine for the PDF 1.4 transparency compositor device for
 * writing the clist.
 */
static	int
pdf14_clist_stroke_path(gx_device *dev,	const gs_gstate *pgs,
                             gx_path *ppath, const gx_stroke_params *params,
                             const gx_drawing_color *pdcolor,
                             const gx_clip_path *pcpath)
{
    pdf14_clist_device * pdev = (pdf14_clist_device *)dev;
    gs_gstate new_pgs = *pgs;
    int code = 0;
    gs_pattern2_instance_t *pinst = NULL;
    int push_group = 0;

    /*
     * Ensure that that the PDF 1.4 reading compositor will have the current
     * blending parameters.  This is needed since the fill_rectangle routines
     * do not have access to the gs_gstate.  Thus we have to pass any
     * changes explictly.
     */
    code = pdf14_clist_update_params(pdev, pgs, false, NULL);
    if (code < 0)
        return code;
    /* If we are doing a shading stroke and we are in a transparency group of a
       different color space, then we need to get the proper device information
       passed along so that we use the correct color procs and colorinfo about
       the transparency device and not the final target device */
    if (pdcolor != NULL && gx_dc_is_pattern2_color(pdcolor)) {
        /* Non-idempotent blends require a transparency
         * group to be pushed because shadings might
         * paint several pixels twice. */
        push_group = pgs->strokeconstantalpha != 1.0 ||
               !blend_is_idempotent(gs_currentblendmode(pgs));
        if (pdev->color_model_stack != NULL) {
            pinst =
                (gs_pattern2_instance_t *)pdcolor->ccolor.pattern;
            pinst->saved->has_transparency = true;
            /* The transparency color space operations are driven
              by the pdf14 clist writer device.  */
            pinst->saved->trans_device = dev;
        }
    }
    if (push_group) {
        gs_fixed_rect box;
        if (pcpath)
            gx_cpath_outer_box(pcpath, &box);
        else
            (*dev_proc(dev, get_clipping_box)) (dev, &box);
        if (ppath) {
            gs_fixed_rect path_box;
            gs_fixed_point expansion;

            gx_path_bbox(ppath, &path_box);
            /* Expand the path bounding box by the scaled line width. */
            if (gx_stroke_path_expansion(pgs, ppath, &expansion) < 0) {
                /* The expansion is so large it caused a limitcheck. */
                path_box.p.x = path_box.p.y = min_fixed;
                path_box.q.x = path_box.q.y = max_fixed;
            } else {
                expansion.x += pgs->fill_adjust.x;
                expansion.y += pgs->fill_adjust.y;
                /*
                 * It's theoretically possible for the following computations to
                 * overflow, so we need to check for this.
                 */
                path_box.p.x = (path_box.p.x < min_fixed + expansion.x ? min_fixed :
                                path_box.p.x - expansion.x);
                path_box.p.y = (path_box.p.y < min_fixed + expansion.y ? min_fixed :
                                path_box.p.y - expansion.y);
                path_box.q.x = (path_box.q.x > max_fixed - expansion.x ? max_fixed :
                                path_box.q.x + expansion.x);
                path_box.q.y = (path_box.q.y > max_fixed - expansion.y ? max_fixed :
                                path_box.q.y + expansion.y);
            }
            if (box.p.x < path_box.p.x)
                box.p.x = path_box.p.x;
            if (box.p.y < path_box.p.y)
                box.p.y = path_box.p.y;
            if (box.q.x > path_box.q.x)
                box.q.x = path_box.q.x;
            if (box.q.y > path_box.q.y)
                box.q.y = path_box.q.y;
        }
        /* Group alpha set from fill value. push_shfill_group does reset to 1.0 */
        new_pgs.fillconstantalpha = new_pgs.strokeconstantalpha;
        code = push_shfill_group(pdev, &new_pgs, &box);
    } else
        update_lop_for_pdf14(&new_pgs, pdcolor);

    if (code >= 0) {
        new_pgs.trans_device = dev;
        new_pgs.has_transparency = true;
        code = gx_forward_stroke_path(dev, &new_pgs, ppath, params, pdcolor, pcpath);
        new_pgs.trans_device = NULL;
        new_pgs.has_transparency = false;
    }
    if (code >= 0 && push_group) {
        code = pop_shfill_group(&new_pgs);
        if (code >= 0)
            code = pdf14_clist_update_params(pdev, pgs, false, NULL);
    }
    if (pinst != NULL)
        pinst->saved->trans_device = NULL;
    return code;
}

/* Set up work for doing shading patterns in fill stroke through
   the clist.  We have to do all the dirty work now since we are
   going through the default fill and stroke operations individually */
static int
pdf14_clist_fill_stroke_path_pattern_setup(gx_device* dev, const gs_gstate* cpgs, gx_path* ppath,
    const gx_fill_params* params_fill, const gx_drawing_color* pdevc_fill,
    const gx_stroke_params* params_stroke, const gx_drawing_color* pdevc_stroke,
    const gx_clip_path* pcpath)
{
    union {
        const gs_gstate *cpgs;
        gs_gstate *pgs;
    } const_breaker;
    gs_gstate *pgs;
    int code, code2;
    gs_transparency_group_params_t params = { 0 };
    gs_fixed_rect clip_bbox;
    gs_rect bbox, group_stroke_box;
    float fill_alpha;
    float stroke_alpha;
    gs_blend_mode_t blend_mode;
    gs_fixed_rect path_bbox;
    int expansion_code;
    gs_fixed_point expansion;

    /* Break const just once, neatly */
    const_breaker.cpgs = cpgs;
    pgs = const_breaker.pgs;

    fill_alpha = pgs->fillconstantalpha;
    stroke_alpha = pgs->strokeconstantalpha;
    blend_mode = pgs->blend_mode;

    code = gx_curr_fixed_bbox(pgs, &clip_bbox, NO_PATH);
    if (code < 0 && code != gs_error_unknownerror)
        return code;
    if (code == gs_error_unknownerror) {
        /* didn't get clip box from gx_curr_fixed_bbox */
        clip_bbox.p.x = clip_bbox.p.y = 0;
        clip_bbox.q.x = int2fixed(dev->width);
        clip_bbox.q.y = int2fixed(dev->height);
    }
    if (pcpath)
        rect_intersect(clip_bbox, pcpath->outer_box);

    /* expand the ppath using stroke expansion rule, then intersect it */
    code = gx_path_bbox(ppath, &path_bbox);
    if (code == gs_error_nocurrentpoint && ppath->segments->contents.subpath_first == 0)
        return 0;		/* ignore empty path */
    if (code < 0)
        return code;
    expansion_code = gx_stroke_path_expansion(pgs, ppath, &expansion);
    if (expansion_code >= 0) {
        path_bbox.p.x -= expansion.x;
        path_bbox.p.y -= expansion.y;
        path_bbox.q.x += expansion.x;
        path_bbox.q.y += expansion.y;
    }
    rect_intersect(path_bbox, clip_bbox);
    bbox.p.x = fixed2float(path_bbox.p.x);
    bbox.p.y = fixed2float(path_bbox.p.y);
    bbox.q.x = fixed2float(path_bbox.q.x);
    bbox.q.y = fixed2float(path_bbox.q.y);

    code = gs_bbox_transform_inverse(&bbox, &ctm_only(pgs), &group_stroke_box);
    if (code < 0)
        return code;

    /* See if overprint is enabled for both stroke and fill AND if ca == CA */
    if (pgs->fillconstantalpha == pgs->strokeconstantalpha &&
        pgs->overprint && pgs->stroke_overprint &&
        (dev->icc_struct->overprint_control != gs_overprint_control_disable) &&
        dev->color_info.polarity == GX_CINFO_POLARITY_SUBTRACTIVE) {

        params.Isolated = false;
        params.group_color_type = UNKNOWN;
        params.Knockout = false;
        params.page_group = false;
        params.group_opacity = fill_alpha;
        params.group_shape = 1.0;

        /* non-isolated non-knockout group pushed with original alpha and blend mode */
        code = gs_begin_transparency_group(pgs, &params, &group_stroke_box, PDF14_BEGIN_TRANS_GROUP);
        if (code < 0)
            return code;

        /* Set alpha to 1.0 and compatible overprint mode for actual drawings */
        (void)gs_setfillconstantalpha(pgs, 1.0);
        (void)gs_setstrokeconstantalpha(pgs, 1.0);
        (void)gs_setblendmode(pgs, BLEND_MODE_CompatibleOverprint); /* Can never fail */

        code = pdf14_clist_fill_path(dev, pgs, ppath, params_fill, pdevc_fill, pcpath);
        if (code < 0)
            goto cleanup;

        code = pdf14_clist_stroke_path(dev, pgs, ppath, params_stroke, pdevc_stroke, pcpath);
        if (code < 0)
            goto cleanup;

    } else {
        /* Push a non-isolated knockout group. Do not change the alpha or
           blend modes */
        params.Isolated = false;
        params.group_color_type = UNKNOWN;
        params.Knockout = true;
        params.page_group = false;
        params.group_opacity = 1.0;
        params.group_shape = 1.0;

        /* non-isolated knockout group is pushed with alpha = 1.0 and Normal blend mode */
        (void)gs_setfillconstantalpha(pgs, 1.0);
        (void)gs_setblendmode(pgs, BLEND_MODE_Normal); /* Can never fail */
        code = gs_begin_transparency_group(pgs, &params, &group_stroke_box, PDF14_BEGIN_TRANS_GROUP);

        /* restore blend mode for actual drawing in the group */
        (void)gs_setblendmode(pgs, blend_mode); /* Can never fail */

        if (fill_alpha > 0.0) {
            (void)gs_setfillconstantalpha(pgs, fill_alpha);

            /* If we are in an overprint situation, set the blend mode to compatible
               overprint */
            if ((dev->icc_struct->overprint_control != gs_overprint_control_disable) &&
                pgs->overprint &&
                dev->color_info.polarity == GX_CINFO_POLARITY_SUBTRACTIVE)
                (void)gs_setblendmode(pgs, BLEND_MODE_CompatibleOverprint); /* Can never fail */

            code = pdf14_clist_fill_path(dev, pgs, ppath, params_fill, pdevc_fill, pcpath);
            if (code < 0)
                goto cleanup;

            if ((dev->icc_struct->overprint_control != gs_overprint_control_disable) &&
                pgs->overprint &&
                dev->color_info.polarity == GX_CINFO_POLARITY_SUBTRACTIVE)
                (void)gs_setblendmode(pgs, blend_mode); /* Can never fail */
        }

        if (stroke_alpha > 0.0) {
            /* Note that the stroke can end up looking like a fill here */
            (void)gs_setstrokeconstantalpha(pgs, stroke_alpha);
            (void)gs_setfillconstantalpha(pgs, stroke_alpha);

            if (pgs->overprint && dev->color_info.polarity == GX_CINFO_POLARITY_SUBTRACTIVE)
                (void)gs_setblendmode(pgs, BLEND_MODE_CompatibleOverprint); /* Can never fail */

            code = pdf14_clist_stroke_path(dev, pgs, ppath, params_stroke, pdevc_stroke, pcpath);
            if (code < 0)
                goto cleanup;

            if ((dev->icc_struct->overprint_control != gs_overprint_control_disable) &&
                pgs->overprint &&
                dev->color_info.polarity == GX_CINFO_POLARITY_SUBTRACTIVE)
                (void)gs_setblendmode(pgs, blend_mode); /* Can never fail */
        }
    }

cleanup:
    /* Now during the pop do the compositing with alpha of 1.0 and normal blend */
    (void)gs_setfillconstantalpha(pgs, 1.0);
    (void)gs_setstrokeconstantalpha(pgs, 1.0);
    (void)gs_setblendmode(pgs, BLEND_MODE_Normal); /* Can never fail */

    /* Restore where we were. If an error occured while in the group push
       return that error code but try to do the cleanup */
    code2 = gs_end_transparency_group(pgs);
    if (code2 < 0) {
        /* At this point things have gone very wrong. We should just shut down */
        code = gs_abort_pdf14trans_device(pgs);
        return code2;
    }

    /* Restore if there were any changes */
    (void)gs_setfillconstantalpha(pgs, fill_alpha);
    (void)gs_setstrokeconstantalpha(pgs, stroke_alpha);
    (void)gs_setblendmode(pgs, blend_mode); /* Can never fail */

    return code;
}

/*
 * fill_path routine for the PDF 1.4 transaprency compositor device for
 * writing the clist.
 */
static	int
pdf14_clist_fill_stroke_path(gx_device	*dev, const gs_gstate *pgs, gx_path *ppath,
                             const gx_fill_params *params_fill, const gx_drawing_color *pdevc_fill,
                             const gx_stroke_params *params_stroke, const gx_drawing_color *pdevc_stroke,
                             const gx_clip_path *pcpath)
{
    pdf14_clist_device * pdev = (pdf14_clist_device *)dev;
    gs_gstate new_pgs = *pgs;
    int code;

    if ((pgs->fillconstantalpha == 0.0 && pgs->strokeconstantalpha == 0.0) ||
        (pgs->ctm.xx == 0.0 && pgs->ctm.xy == 0.0 && pgs->ctm.yx == 0.0 && pgs->ctm.yy == 0.0))
        return 0;

    /*
     * Ensure that that the PDF 1.4 reading compositor will have the current
     * blending parameters.  This is needed since the fill_rectangle routines
     * do not have access to the gs_gstate.  Thus we have to pass any
     * changes explictly.
     */
    code = pdf14_clist_update_params(pdev, pgs, false, NULL);
    if (code < 0)
        return code;
    /* If we are doing a shading fill or stroke, the clist can't
       deal with this and end up in the pdf_fill_stroke operation.
       We will need to break up the fill stroke now and do
       the appropriate group pushes and set up. */

    if ((pdevc_fill != NULL && gx_dc_is_pattern2_color(pdevc_fill)) ||
        (pdevc_stroke != NULL && gx_dc_is_pattern2_color(pdevc_stroke))) {
        return pdf14_clist_fill_stroke_path_pattern_setup(dev, pgs, ppath,
            params_fill, pdevc_fill, params_stroke, pdevc_stroke, pcpath);
    }
    update_lop_for_pdf14(&new_pgs, pdevc_fill);
    new_pgs.trans_device = dev;
    new_pgs.has_transparency = true;
    code = gx_forward_fill_stroke_path(dev, &new_pgs, ppath, params_fill, pdevc_fill,
                                       params_stroke, pdevc_stroke, pcpath);
    new_pgs.trans_device = NULL;
    new_pgs.has_transparency = false;
    return code;
}

/*
 * text_begin routine for the PDF 1.4 transaprency compositor device for
 * writing the clist.
 */
static	int
pdf14_clist_text_begin(gx_device * dev,	gs_gstate	* pgs,
                 const gs_text_params_t * text, gs_font * font,
                 const gx_clip_path * pcpath,
                 gs_text_enum_t ** ppenum)
{
    pdf14_clist_device * pdev = (pdf14_clist_device *)dev;
    gs_text_enum_t *penum;
    int code;
    gs_blend_mode_t blend_mode = gs_currentblendmode(pgs);
    float opacity = pgs->fillconstantalpha;
    float shape = 1.0;
    bool blend_issue = !(blend_mode == BLEND_MODE_Normal || blend_mode == BLEND_MODE_Compatible || blend_mode == BLEND_MODE_CompatibleOverprint);
    bool draw = !(text->operation & TEXT_DO_NONE);
    uint text_mode = gs_currenttextrenderingmode(pgs);
    bool text_stroke = (text_mode == 1 || text_mode == 2 || text_mode == 5 || text_mode == 6);
    bool text_fill = (text_mode == 0 || text_mode == 2 || text_mode == 4 || text_mode == 6);

    if_debug0m('v', pgs->memory, "[v]pdf14_clist_text_begin\n");
    /*
     * Ensure that that the PDF 1.4 reading compositor will have the current
     * blending parameters.  This is needed since the fill_rectangle routines
     * do not have access to the gs_gstate.  Thus we have to pass any
     * changes explictly.
     */
    code = pdf14_clist_update_params(pdev, pgs, false, NULL);
    if (code < 0)
        return code;
    /* Pass text_begin to the target */
    code = gx_forward_text_begin(dev, pgs, text, font,
                                 pcpath, &penum);
    if (code < 0)
        return code;

   /* Catch case where we already pushed a group and are trying to push another one.
   In that case, we will pop the current one first, as we don't want to be left
   with it. Note that if we have a BT and no other BTs or ETs then this issue
   will not be caught until we do the put_image and notice that the stack is not
   empty. */
    if (pdev->text_group == PDF14_TEXTGROUP_MISSING_ET) {
        code = gs_end_transparency_group(pgs);
        if (code < 0)
            return code;
        pdev->text_group = PDF14_TEXTGROUP_BT_NOT_PUSHED;
    }

    /* We may need to push a non-isolated transparency group if the following
    is true.
    1) We are not currently in one that we pushed for text.  This is
    is determined by looking at the pdf14 device.
    2) The blend mode is not Normal or the opacity is not 1.0
    3) Text knockout is set to true
    4) And we are actually drawing text
    */

    if (gs_currenttextknockout(pgs) && (blend_issue ||
        (pgs->fillconstantalpha != 1.0 && text_fill) ||
        (pgs->strokeconstantalpha != 1.0 && text_stroke)) &&
        text_mode != 3 && /* don't bother with invisible text */
        pdev->text_group == PDF14_TEXTGROUP_BT_NOT_PUSHED) {
        if (draw) {
            code = pdf14_push_text_group(dev, pgs, blend_mode, opacity, shape, true);
            if (code == 0)
                pdev->text_group = PDF14_TEXTGROUP_BT_PUSHED;  /* Needed during clist writing */
        }
    }
    *ppenum = (gs_text_enum_t *)penum;
    return code;
}

static	int
pdf14_clist_begin_typed_image(gx_device	* dev, const gs_gstate * pgs,
                           const gs_matrix *pmat, const gs_image_common_t *pic,
                           const gs_int_rect * prect,
                           const gx_drawing_color * pdcolor,
                           const gx_clip_path * pcpath, gs_memory_t * mem,
                           gx_image_enum_common_t ** pinfo)
{
    pdf14_clist_device * pdev = (pdf14_clist_device *)dev;
    int code;
    gs_gstate * pgs_noconst = (gs_gstate *)pgs; /* Break 'const'. */
    const gs_image_t *pim = (const gs_image_t *)pic;
    gx_image_enum *penum;
    gx_color_tile *ptile;
    gs_rect bbox_in, bbox_out;
    gs_transparency_group_params_t tgp;
    /*
     * Ensure that that the PDF 1.4 reading compositor will have the current
     * blending parameters.  This is needed since the fill_rectangle routines
     * do not have access to the gs_gstate.  Thus we have to pass any
     * changes explictly.
     */
    code = pdf14_clist_update_params(pdev, pgs, false, NULL);
    if (code < 0)
        return code;
    /* Pass image to the target */
    /* Do a quick change to the gs_gstate so that if we can return with -1 in
       case the clist writer cannot handle this image itself.  In such a case,
       we want to make sure we dont use the target device.  I don't necc. like
       doing it this way.  Probably need to go back and do something a bit
       more elegant. */
    pgs_noconst->has_transparency = true;
    pgs_noconst->trans_device = dev;

    /* If we are filling an image mask with a pattern that has a transparency
       then we need to do some special handling */
    if (pim->ImageMask) {
        if (pdcolor != NULL && gx_dc_is_pattern1_color(pdcolor)) {
            if( gx_pattern1_get_transptr(pdcolor) != NULL){
                if (dev_proc(dev, begin_typed_image) != pdf14_clist_begin_typed_image) {
                    ptile = pdcolor->colors.pattern.p_tile;
                    /* Set up things in the ptile so that we get the proper
                       blending etc */
                    /* Set the blending procs and the is_additive setting based
                       upon the number of channels */
                    if (ptile->ttrans->n_chan-1 < 4) {
                        ptile->ttrans->blending_procs = &rgb_blending_procs;
                        ptile->ttrans->is_additive = true;
                    } else {
                        ptile->ttrans->blending_procs = &cmyk_blending_procs;
                        ptile->ttrans->is_additive = false;
                    }
                    /* Set the blending mode in the ptile based upon the current
                       setting in the gs_gstate */
                    ptile->blending_mode = pgs->blend_mode;
                    /* Set the procs so that we use the proper filling method. */
                    /* Let the imaging stuff get set up */
                    code = gx_default_begin_typed_image(dev, pgs, pmat, pic,
                                                        prect, pdcolor,
                                                        pcpath, mem, pinfo);
                    if (code < 0)
                        return code;

                    penum = (gx_image_enum *) *pinfo;
                    /* Apply inverse of the image matrix to our
                       image size to get our bounding box. */
                    bbox_in.p.x = 0;
                    bbox_in.p.y = 0;
                    bbox_in.q.x = pim->Width;
                    bbox_in.q.y = pim->Height;
                    code = gs_bbox_transform_inverse(&bbox_in, &(pim->ImageMatrix),
                                                     &bbox_out);
                    if (code < 0) return code;
                    /* Set up a compositor action for pushing the group */
                    if_debug0m('v', pgs->memory, "[v]Pushing special trans group for image\n");
                    tgp.Isolated = true;
                    tgp.Knockout = false;
                    tgp.page_group = false;
                    tgp.mask_id = 0;
                    tgp.image_with_SMask = false;
                    tgp.idle = false;
                    tgp.iccprofile = NULL;
                    tgp.icc_hashcode = 0;
                    tgp.group_color_numcomps = ptile->ttrans->n_chan-1;
                    tgp.ColorSpace = NULL;
                    tgp.text_group = 0;
                    tgp.group_opacity = pgs->fillconstantalpha;
                    tgp.group_shape = 1.0;
                    /* This will handle the compositor command */
                    gs_begin_transparency_group((gs_gstate *) pgs_noconst, &tgp,
                                                &bbox_out, PDF14_BEGIN_TRANS_GROUP);
                    ptile->ttrans->image_render = penum->render;
                    penum->render = &pdf14_pattern_trans_render;
                    ptile->trans_group_popped = false;
                    pgs_noconst->has_transparency = false;
                    pgs_noconst->trans_device = NULL;
                    return code;
                }
            }
        }
    }
    /* This basically tries high level images for clist. If that fails
       then we do the default */
    code = gx_forward_begin_typed_image(dev, pgs, pmat,
                            pic, prect, pdcolor, pcpath, mem, pinfo);
    if (code < 0){
        code = gx_default_begin_typed_image(dev, pgs, pmat, pic, prect,
                                        pdcolor, pcpath, mem, pinfo);
        pgs_noconst->has_transparency = false;
        pgs_noconst->trans_device = NULL;
        return code;
    } else {
        pgs_noconst->has_transparency = false;
        pgs_noconst->trans_device = NULL;
        return code;
    }
}

static int
pdf14_clist_copy_planes(gx_device * dev, const byte * data, int data_x, int raster,
                  gx_bitmap_id id, int x, int y, int w, int h, int plane_height)
{
    int code;

    code = gx_forward_copy_planes(dev, data, data_x, raster, id,
                                  x, y, w, h, plane_height);
    return code;
}

static int
gs_pdf14_clist_device_push(gs_memory_t *mem, gs_gstate *pgs, gx_device **pcdev,
                           gx_device *dev, const gs_pdf14trans_t *pdf14pct)
{
    int code;
    pdf14_clist_device *p14dev;
    gx_device_clist_writer * const cdev = &((gx_device_clist *)dev)->writer;

    code = pdf14_create_clist_device(mem, pgs, pcdev, dev, pdf14pct);
    if (code < 0)
        return code;
    /*
     * Set the color_info of the clist device to match the compositing
     * device.  We will restore it when the compositor is popped.
     * See pdf14_clist_composite for the restore.  Do the
     * same with the gs_gstate's get_cmap_procs.  We do not want
     * the gs_gstate to use transfer functions on our color values.
     * The transfer functions will be applied at the end after we
     * have done our PDF 1.4 blend operations.
     */
    p14dev = (pdf14_clist_device *)(*pcdev);
    p14dev->saved_target_color_info = dev->color_info;
    dev->color_info = (*pcdev)->color_info;
    /* Make sure that we keep the anti-alias information though */
    dev->color_info.anti_alias = p14dev->saved_target_color_info.anti_alias;
    p14dev->color_info.anti_alias = dev->color_info.anti_alias;

    /* adjust the clist_color_info now */
    cdev->clist_color_info.depth = p14dev->color_info.depth;
    cdev->clist_color_info.polarity = p14dev->color_info.polarity;
    cdev->clist_color_info.num_components = p14dev->color_info.num_components;
    cdev->clist_color_info.max_color = p14dev->color_info.max_color;
    cdev->clist_color_info.max_gray = p14dev->color_info.max_gray;

    p14dev->saved_target_encode_color = dev_proc(dev, encode_color);
    p14dev->saved_target_decode_color = dev_proc(dev, decode_color);
    set_dev_proc(dev, encode_color, p14dev->my_encode_color);
    set_dev_proc(p14dev, encode_color, p14dev->my_encode_color);
    set_dev_proc(dev, decode_color, p14dev->my_decode_color);
    set_dev_proc(p14dev, decode_color, p14dev->my_decode_color);
    p14dev->saved_target_get_color_mapping_procs =
                              dev_proc(dev, get_color_mapping_procs);
    p14dev->saved_target_get_color_comp_index =
                              dev_proc(dev, get_color_comp_index);
    set_dev_proc(dev, get_color_mapping_procs, p14dev->my_get_color_mapping_procs);
    set_dev_proc(p14dev, get_color_mapping_procs, p14dev->my_get_color_mapping_procs);
    set_dev_proc(dev, get_color_comp_index, p14dev->my_get_color_comp_index);
    set_dev_proc(p14dev, get_color_comp_index, p14dev->my_get_color_comp_index);
    p14dev->save_get_cmap_procs = pgs->get_cmap_procs;
    pgs->get_cmap_procs = pdf14_get_cmap_procs;
    gx_set_cmap_procs(pgs, dev);
    return code;
}
/*
 * When we push a PDF 1.4 transparency compositor onto the clist, we also need
 * to create a compositing device for clist writing.  The primary purpose of
 * this device is to provide support for the process color model in which
 * the PDF 1.4 transparency is done.  (This may differ from the process color
 * model of the output device.)  The actual work of compositing the image is
 * done on the output (reader) side of the clist.
 */
static	int
c_pdf14trans_clist_write_update(const gs_composite_t * pcte, gx_device * dev,
                gx_device ** pcdev, gs_gstate * pgs, gs_memory_t * mem)
{
    gx_device_clist_writer * const cdev = &((gx_device_clist *)dev)->writer;
    const gs_pdf14trans_t * pdf14pct = (const gs_pdf14trans_t *) pcte;
    int code = 0;

    /* We only handle the push/pop operations */
    switch (pdf14pct->params.pdf14_op) {
        case PDF14_PUSH_DEVICE:
            code = gs_pdf14_clist_device_push(mem, pgs, pcdev, dev, pdf14pct);
            /* Change (non-error) code to 1 to indicate that we created
             * a device. */
            if (code >= 0)
                code = 1;
            return code;

        case PDF14_POP_DEVICE:
            code = clist_writer_check_empty_cropping_stack(cdev);
            break;

        case PDF14_BEGIN_TRANS_PAGE_GROUP:
        case PDF14_BEGIN_TRANS_GROUP:
            {	/* HACK: store mask_id into our params for subsequent
                   calls of c_pdf14trans_write. To do this we must
                   break const. */
                gs_pdf14trans_t * pdf14pct_noconst;

                pdf14pct_noconst = (gs_pdf14trans_t *) pcte;
                /* What ever the current mask ID is, that is the
                   softmask group through which this transparency
                   group must be rendered. Store it now. */
                pdf14pct_noconst->params.mask_id = cdev->mask_id;
                if_debug1m('v', pgs->memory,
                           "[v]c_pdf14trans_clist_write_update group mask_id=%d \n",
                           cdev->mask_id);
            }
            break;
        case PDF14_END_TRANS_GROUP:
        case PDF14_END_TRANS_TEXT_GROUP:
            code = 0; /* A place for breakpoint. */
            break;
        case PDF14_BEGIN_TRANS_MASK:
            /* A new mask has been started */
            cdev->mask_id = ++cdev->mask_id_count;
            /* replacing is set everytime that we
               have a zpushtransparencymaskgroup */
            {	/* HACK: store mask_id into our params for subsequent
                   calls of c_pdf14trans_write. To do this we must
                   break const. */
                gs_pdf14trans_t * pdf14pct_noconst;

                pdf14pct_noconst = (gs_pdf14trans_t *) pcte;
                pdf14pct_noconst->params.mask_id = cdev->mask_id;
                if_debug1m('v', pgs->memory,
                           "[v]c_pdf14trans_clist_write_update mask mask_id=%d \n",
                           cdev->mask_id);
            }
            break;
        case PDF14_END_TRANS_MASK:
            code = 0; /* A place for breakpoint. */
            break;
        case PDF14_PUSH_TRANS_STATE:
            code = 0; /* A place for breakpoint. */
            break;
        case PDF14_POP_TRANS_STATE:
            code = 0; /* A place for breakpoint. */
            break;
        case PDF14_ABORT_DEVICE:
            code = 0;
            break;
        case PDF14_PUSH_SMASK_COLOR:
            *pcdev = dev;
            return 0;
            break;
        case PDF14_POP_SMASK_COLOR:
            *pcdev = dev;
            return 0;
            break;
        default:
            break;		/* do nothing for remaining ops */
    }
    *pcdev = dev;
    if (code < 0)
        return code;
    /* See c_pdf14trans_write, c_pdf14trans_adjust_ctm, and
       apply_composite. */
    code = gs_gstate_setmatrix(&cdev->gs_gstate, &pdf14pct->params.ctm);
    /* Wrote an extra ctm. */
    cmd_clear_known(cdev, ctm_known);

    return code;
}

/*
 * When we push a PDF 1.4 transparency compositor, we need to make the clist
 * device color_info data match the compositing device.  We need to do this
 * since the PDF 1.4 transparency compositing device may use a different
 * process color model than the output device.  We do not need to modify the
 * color related device procs since the compositing device has its own.  We
 * restore the color_info data when the transparency device is popped.
 */
static	int
c_pdf14trans_clist_read_update(gs_composite_t *	pcte, gx_device	* cdev,
                gx_device * tdev, gs_gstate * pgs, gs_memory_t * mem)
{
    pdf14_device * p14dev = (pdf14_device *)tdev;
    gs_pdf14trans_t * pdf14pct = (gs_pdf14trans_t *) pcte;
    gs_devn_params * pclist_devn_params;
    gx_device_clist_reader *pcrdev = (gx_device_clist_reader *)cdev;
    cmm_profile_t *cl_icc_profile, *p14_icc_profile;
    gsicc_rendering_param_t render_cond;
    cmm_dev_profile_t *dev_profile;

    dev_proc(cdev, get_profile)(cdev,  &dev_profile);
    gsicc_extract_profile(GS_UNKNOWN_TAG, dev_profile, &cl_icc_profile,
                          &render_cond);

    /* If we are using the blending color space, then be sure to use that. */
    if (p14dev->using_blend_cs && dev_profile->blend_profile != NULL)
        cl_icc_profile = dev_profile->blend_profile;

    dev_proc(p14dev, get_profile)((gx_device *)p14dev,  &dev_profile);
    gsicc_extract_profile(GS_UNKNOWN_TAG, dev_profile, &p14_icc_profile,
                          &render_cond);
    /*
     * We only handle the push/pop operations. Save and restore the color_info
     * field for the clist device.  This is needed since the process color
     * model of the clist device needs to match the PDF 1.4 compositing
     * device.
     */
    switch (pdf14pct->params.pdf14_op) {
    case PDF14_PUSH_DEVICE:
        /* Overprint simulation sets the profile at prototype creation, as does
           when the target profile is NCLR. Match the logic in gs_pdf14_device_push */
        if (!((p14dev->overprint_sim && cl_icc_profile->data_cs != gsCMYK) ||
            cl_icc_profile->data_cs == gsNCHANNEL)) {
            gsicc_adjust_profile_rc(cl_icc_profile, 1, "c_pdf14trans_clist_read_update");
            gsicc_adjust_profile_rc(p14dev->icc_struct->device_profile[GS_DEFAULT_DEVICE_PROFILE],
                -1, "c_pdf14trans_clist_read_update");
            p14dev->icc_struct->device_profile[GS_DEFAULT_DEVICE_PROFILE] = cl_icc_profile;
        }
            /*
             * If we are blending using spot colors (i.e. the output device
             * supports spot colors) then we need to transfer
             * color info from the clist PDF 1.4 compositing reader device
             * to the clist writer PDF 1.4 compositing device.
             * This info was transfered from that device to the output
             * device as a set of device parameters.  However the clist
             * reader PDF 1.4 compositing device did not exist when the
             * device parameters were read from the clist.  So that info
             * was buffered into the output device.
             */
            pclist_devn_params = dev_proc(cdev, ret_devn_params)(cdev);
            if (pclist_devn_params != NULL && pclist_devn_params->page_spot_colors != 0) {
                int num_comp = p14dev->color_info.num_components;
                /*
                 * The number of components for the PDF14 device is the sum
                 * of the process components and the number of spot colors
                 * for the page.  If the color capabilities of the parent
                 * device (which coming into this are the same as the p14dev)
                 * are smaller than the number of page spot colors then
                 * use that for the number of components. Otherwise use
                 * the page_spot_colors.
                 */
                p14dev->devn_params.page_spot_colors =
                    pclist_devn_params->page_spot_colors;
                if (num_comp < p14dev->devn_params.page_spot_colors + 4 ) {
                    p14dev->color_info.num_components = num_comp;
                } else {
                    /* if page_spot_colors < 0, this will be wrong, so don't update num_components */
                    if (p14dev->devn_params.page_spot_colors >= 0) {
                        p14dev->color_info.num_components =
                            p14dev->devn_params.num_std_colorant_names +
                            p14dev->devn_params.page_spot_colors;
                    }
                }
                /* limit the num_components to the max. */
                if (p14dev->color_info.num_components > p14dev->color_info.max_components)
                    p14dev->color_info.num_components = p14dev->color_info.max_components;
                /* Transfer the data for the spot color names
                   But we have to free what may be there before we do this */
                devn_free_params((gx_device*) p14dev);
                p14dev->devn_params.separations =
                    pclist_devn_params->pdf14_separations;
                p14dev->free_devicen = false;  /* to avoid freeing the clist ones */
                if (num_comp != p14dev->color_info.num_components) {
                    /* When the pdf14 device is opened it creates a context
                       and some soft mask related objects.  The push device
                       compositor action will have already created these but
                       they are the wrong size.  We must destroy them though
                       before reopening the device */
                    if (p14dev->ctx != NULL) {
                        pdf14_ctx_free(p14dev->ctx);
                        p14dev->ctx = NULL;
                    }
                    dev_proc(tdev, open_device) (tdev);
                }
            }
            /* Check if we need to swap out the ICC profile for the pdf14
               device.  This will occur if our source profile for our device
               happens to be something like CIELAB.  Then we will blend in
               RGB (unless a trans group is specified) */
            if (cl_icc_profile->data_cs == gsCIELAB || cl_icc_profile->islab) {
                gsicc_adjust_profile_rc(p14dev->icc_struct->device_profile[GS_DEFAULT_DEVICE_PROFILE],
                                        -1, "c_pdf14trans_clist_read_update");
                /* Initial ref count from gsicc_read_serial_icc() is 1, which is what we want */
                p14dev->icc_struct->device_profile[GS_DEFAULT_DEVICE_PROFILE] =
                                        gsicc_read_serial_icc(cdev, pcrdev->trans_dev_icc_hash);
                /* Keep a pointer to the clist device */
                p14dev->icc_struct->device_profile[GS_DEFAULT_DEVICE_PROFILE]->dev = (gx_device *) cdev;
            }
            break;

        case PDF14_POP_DEVICE:
#	    if 0 /* Disabled because *p14dev has no forwarding methods during
                    the clist playback. This code is not executed while clist
                    writing. */
            cdev->color_info = p14dev->saved_target_color_info;
#	    endif
           break;

        default:
            break;		/* do nothing for remaining ops */
    }

    return 0;
}

/*
 * Get cropping for the compositor command.
 */
static	int
c_pdf14trans_get_cropping(const gs_composite_t *pcte, int *ry, int *rheight,
                          int cropping_min, int cropping_max)
{
    gs_pdf14trans_t * pdf14pct = (gs_pdf14trans_t *) pcte;
    switch (pdf14pct->params.pdf14_op) {
        case PDF14_PUSH_DEVICE: return ALLBANDS; /* Applies to all bands. */
        case PDF14_POP_DEVICE:  return ALLBANDS; /* Applies to all bands. */
        case PDF14_ABORT_DEVICE: return ALLBANDS; /* Applies to all bands */
        case PDF14_BEGIN_TRANS_PAGE_GROUP:
        case PDF14_BEGIN_TRANS_GROUP:
            {	gs_int_rect rect;

                /* Text group always uses parents size*/
                if (pdf14pct->params.text_group == PDF14_TEXTGROUP_BT_PUSHED) {
                    *ry = cropping_min;
                    *rheight = cropping_max - *ry;
                } else {
                    pdf14_compute_group_device_int_rect(&pdf14pct->params.ctm,
                        &pdf14pct->params.bbox, &rect);
                    /* We have to crop this by the parent object.   */
                    *ry = max(rect.p.y, cropping_min);
                    *rheight = min(rect.q.y, cropping_max) - *ry;
                }
                return PUSHCROP; /* Push cropping. */
            }
        case PDF14_BEGIN_TRANS_MASK:
            {	gs_int_rect rect;

                pdf14_compute_group_device_int_rect(&pdf14pct->params.ctm,
                                                    &pdf14pct->params.bbox, &rect);
                /* We have to crop this by the parent object and worry about the BC outside
                   the range, except for image SMask which don't affect areas outside the image.
                   The presence of a transfer function opens the possibility of issues with this */
                if (pdf14pct->params.mask_is_image || (pdf14pct->params.GrayBackground == 1.0 &&
                      pdf14pct->params.function_is_identity)) {
                    /* In this case there will not be a background effect to
                       worry about.  The mask will not have any effect outside
                       the bounding box.  This is NOT the default or common case. */
                    *ry = max(rect.p.y, cropping_min);
                    *rheight = min(rect.q.y, cropping_max) - *ry;
                    return PUSHCROP; /* Push cropping. */
                }  else {
                    /* We need to make the soft mask range as large as the parent
                       due to the fact that the background color can have an impact
                       OUTSIDE the bounding box of the soft mask */
                    *ry = cropping_min;
                    *rheight = cropping_max - cropping_min;
                    if (pdf14pct->params.subtype == TRANSPARENCY_MASK_None)
                        return SAMEAS_PUSHCROP_BUTNOPUSH;
                    else
                        return PUSHCROP; /* Push cropping. */
                }
            }
        case PDF14_END_TRANS_GROUP: return POPCROP; /* Pop cropping. */
        case PDF14_END_TRANS_TEXT_GROUP: return POPCROP; /* Pop cropping. */
        case PDF14_END_TRANS_MASK: return POPCROP;   /* Pop the cropping */
        case PDF14_PUSH_TRANS_STATE: return CURRBANDS;
        case PDF14_POP_TRANS_STATE: return CURRBANDS;
        case PDF14_SET_BLEND_PARAMS: return ALLBANDS;
        case PDF14_PUSH_SMASK_COLOR: return POPCROP; /* Pop cropping. */
        case PDF14_POP_SMASK_COLOR: return POPCROP;   /* Pop the cropping */
        case PDF14_BEGIN_TRANS_TEXT_GROUP: return ALLBANDS; /* should never occur */
    }
    return ALLBANDS;
}

/*
 * This routine will check to see if the color component name matches those
 * that are available amoung the current device's color components.  If the
 * color name is known to the output device then we add it to the list of
 * colorants for the PDF 1.4 transparency compositor.
 *
 * Notes:  There are currently three different versions of The PDF 1.4
 * transparency compositor device.  The choice of which one is being used
 * depends upon the process color model of the output device.  This procedure
 * is only used if the output (target) device uses a CMYK, or RGB or Gray
 * plus spot color process color model.
 *
 * Parameters:
 *   dev - pointer to device data structure.
 *   pname - pointer to name (zero termination not required)
 *   nlength - length of the name
 *   number of process colorants (either 1, 3, or 4)
 *
 * This routine returns a positive value (0 to n) which is the device colorant
 * number if the name is found.  It returns GX_DEVICE_COLOR_MAX_COMPONENTS if
 * the colorant is not being used due to a SeparationOrder device parameter.
 * It returns a negative value if not found.
 */
static int
pdf14_spot_get_color_comp_index(gx_device *dev, const char *pname,
    int name_size, int component_type, int num_process_colors)
{
    pdf14_device *pdev = (pdf14_device *)dev;
    gx_device *tdev = pdev->target;
    gs_devn_params *pdevn_params = &pdev->devn_params;
    gs_separations *pseparations;
    int comp_index;
    dev_proc_get_color_comp_index(*target_get_color_comp_index);
    int offset = 4 - num_process_colors;

    while (tdev->child) {
        tdev = tdev->child;
    }
    /* If something has gone wrong and this is no longer the pdf14 compositor, */
    /* get the devn_params from the target to avoid accessing using the wrong  */
    /* pointer. Bug 696372.                                                    */
    if (tdev == (gx_device *)pdev)
        pdevn_params = dev_proc(pdev, ret_devn_params)(dev);
    pseparations = &pdevn_params->separations;
    /* If num_process_colors is 3 or 1 (RGB or Gray) then we are in a situation
     * where we are in a blend color space that is RGB or Gray based and we
     * have a spot colorant.  If the spot colorant name is Cyan, Magenta
     * Yellow or Black, then we should use the alternate tint transform */
    if (num_process_colors < 4) {
        int k;
        for (k = 0; k < 4; k++) {
            if (strncmp(pname, pdev->devn_params.std_colorant_names[k], name_size) == 0)
                return -1;
        }
    }

    target_get_color_comp_index = dev_proc(tdev, get_color_comp_index);

    /* The pdf14_clist_composite may have set the color procs.
       We need the real target procs, but not if we are doing simulated
       overprint */
    if (target_get_color_comp_index == pdf14_cmykspot_get_color_comp_index &&
        !pdev->overprint_sim)
        target_get_color_comp_index =
            ((pdf14_clist_device *)pdev)->saved_target_get_color_comp_index;
    /*
    * If this is not a separation name then simply forward it to the target
    * device or return -1 if we are doing overprint simulation.
    * The halftone setup expects this.
    */
    if (!pdev->overprint_sim && (component_type == NO_COMP_NAME_TYPE_HT ||
        component_type == NO_COMP_NAME_TYPE_OP)) {
        return  (*target_get_color_comp_index)(tdev, pname, name_size, component_type);
    }
    if (pdev->overprint_sim && component_type == NO_COMP_NAME_TYPE_HT) {
        return -1;
    }

    /*
    * Check if the component is in either the process color model list
    * or in the SeparationNames list.
    */
    comp_index = check_pcm_and_separation_names(dev, pdevn_params, pname,
        name_size, component_type);
    /*
    * Return the colorant number if we know this name.  Note adjustment for
    * compensating of blend color space.
    */
    if (comp_index >= 0)
        return comp_index - offset;

    /* Only worry about the target if we are not doing an overprint simulation */
    if (!pdev->overprint_sim) {
        /*
        * If we do not know this color, check if the output (target) device does.
        * Note that if the target device has ENABLE_AUTO_SPOT_COLORS this will add
        * the colorant so we will only get < 0 returned when we hit the max. for
        * the target device.
        */
        comp_index = (*target_get_color_comp_index)(tdev, pname, name_size, component_type);
        /*
        * Ignore color if unknown to the output device or if color is not being
        * imaged due to the SeparationOrder device parameter.
        */
        if (comp_index < 0 || comp_index == GX_DEVICE_COLOR_MAX_COMPONENTS)
            return comp_index - offset;
    }

    /*
    * This is a new colorant.  Add it to our list of colorants.
    * The limit accounts for the number of process colors (at least 4).
    */
    if ((pseparations->num_separations + 1) <
            (GX_DEVICE_COLOR_MAX_COMPONENTS - max(num_process_colors, 4))) {
        int sep_num = pseparations->num_separations++;
        int color_component_number;
        byte * sep_name;

        sep_name = gs_alloc_bytes(dev->memory->stable_memory,
            name_size, "pdf14_spot_get_color_comp_index");
        if (sep_name == NULL) {
            pseparations->num_separations--;	/* we didn't add it */
            return -1;
        }
        memcpy(sep_name, pname, name_size);
        pseparations->names[sep_num].size = name_size;
        pseparations->names[sep_num].data = sep_name;
        color_component_number = sep_num + num_process_colors;
        if (color_component_number >= dev->color_info.max_components)
            color_component_number = GX_DEVICE_COLOR_MAX_COMPONENTS;
        else
            pdevn_params->separation_order_map[color_component_number] =
            color_component_number;

        /* Indicate that we need to find equivalent CMYK color. */
        pdev->op_pequiv_cmyk_colors.color[sep_num].color_info_valid = false;
        pdev->op_pequiv_cmyk_colors.all_color_info_valid = false;

        return color_component_number;
    }

    return GX_DEVICE_COLOR_MAX_COMPONENTS;
}


/* CMYK process + spots */
static int
pdf14_cmykspot_get_color_comp_index(gx_device * dev, const char * pname,
    int name_size, int component_type)
{
    return pdf14_spot_get_color_comp_index(dev, pname, name_size, component_type, 4);
}

/* RGB process + spots */
static int
pdf14_rgbspot_get_color_comp_index(gx_device * dev, const char * pname,
    int name_size, int component_type)
{
    return pdf14_spot_get_color_comp_index(dev, pname, name_size, component_type, 3);
}

/* Gray process + spots */
static int
pdf14_grayspot_get_color_comp_index(gx_device * dev, const char * pname,
    int name_size, int component_type)
{
    return pdf14_spot_get_color_comp_index(dev, pname, name_size, component_type, 1);
}

/* These functions keep track of when we are dealing with soft masks.
   In such a case, we set the default color profiles to ones that ensure
   proper soft mask rendering. */
static int
pdf14_increment_smask_color(gs_gstate * pgs, gx_device * dev)
{
    pdf14_device * pdev = (pdf14_device *) dev;
    pdf14_smaskcolor_t *result;
    gsicc_smask_t *smask_profiles = pgs->icc_manager->smask_profiles;
    int k;

    /* See if we have profiles already in place.   Note we also have to
       worry about a corner case where this device does not have a
       smaskcolor stucture to store the profiles AND the profiles were
       already swapped out in the icc_manager.  This can occur when we
       pushed a transparency mask and then inside the mask we have a pattern
       which also has a transparency mask.   The state of the icc_manager
       is that it already has done the swap and there is no need to fool
       with any of this while dealing with the soft mask within the pattern */
    if (pdev->smaskcolor == NULL && pgs->icc_manager->smask_profiles != NULL &&
        pgs->icc_manager->smask_profiles->swapped) {
            return 0;
    }
    if (pdev->smaskcolor != NULL) {
        pdev->smaskcolor->ref_count++;
        if_debug1m(gs_debug_flag_icc, dev->memory,
                   "[icc] Increment smask color now %d\n",
                   pdev->smaskcolor->ref_count);
    } else {
        /* Allocate and swap out the current profiles.  The softmask
           profiles should already be in place */
        result = gs_alloc_struct(pdev->memory->stable_memory, pdf14_smaskcolor_t,
                                &st_pdf14_smaskcolor,
                                "pdf14_increment_smask_color");
        if (result == NULL)
            return gs_error_VMerror;

        result->profiles = gsicc_new_iccsmask(pdev->memory->stable_memory);
        if (result->profiles == NULL)
            return gs_error_VMerror;

        pdev->smaskcolor = result;

        result->profiles->smask_gray = pgs->icc_manager->default_gray;
        result->profiles->smask_rgb = pgs->icc_manager->default_rgb;
        result->profiles->smask_cmyk = pgs->icc_manager->default_cmyk;
        pgs->icc_manager->default_gray = smask_profiles->smask_gray;
        gsicc_adjust_profile_rc(pgs->icc_manager->default_gray, 1, "pdf14_increment_smask_color");
        pgs->icc_manager->default_rgb = smask_profiles->smask_rgb;
        gsicc_adjust_profile_rc(pgs->icc_manager->default_rgb, 1, "pdf14_increment_smask_color");
        pgs->icc_manager->default_cmyk = smask_profiles->smask_cmyk;
        gsicc_adjust_profile_rc(pgs->icc_manager->default_cmyk, 1, "pdf14_increment_smask_color");
        pgs->icc_manager->smask_profiles->swapped = true;
        if_debug0m(gs_debug_flag_icc, pgs->memory,
                   "[icc] Initial creation of smask color. Ref count 1\n");
        pdev->smaskcolor->ref_count = 1;
        /* We also need to update the profile that is currently in the
           color spaces of the graphic state.  Otherwise this can be
           referenced, which will result in a mismatch.  What we want to do
           is see if it was the original default and only swap in that case. */
        for (k = 0; k < 2; k++) {
            gs_color_space *pcs     = pgs->color[k].color_space;
            cmm_profile_t  *profile = pcs->cmm_icc_profile_data;
            if (profile != NULL) {
                switch(profile->data_cs) {
                    case gsGRAY:
                        if (profile->hashcode ==
                            result->profiles->smask_gray->hashcode) {
                                profile = pgs->icc_manager->default_gray;
                        }
                        break;
                    case gsRGB:
                        if (profile->hashcode ==
                            result->profiles->smask_rgb->hashcode) {
                                profile = pgs->icc_manager->default_rgb;
                        }
                        break;
                    case gsCMYK:
                        if (profile->hashcode ==
                            result->profiles->smask_cmyk->hashcode) {
                                profile = pgs->icc_manager->default_cmyk;
                        }
                        break;
                    default:

                        break;
                }
                if (pcs->cmm_icc_profile_data != profile) {
                    gsicc_adjust_profile_rc(profile, 1, "pdf14_increment_smask_color");
                    gsicc_adjust_profile_rc(pcs->cmm_icc_profile_data, -1, "pdf14_increment_smask_color");
                    pcs->cmm_icc_profile_data = profile;
                }
            }
        }
    }
    return 0;
}

static int
pdf14_decrement_smask_color(gs_gstate * pgs, gx_device * dev)
{
    pdf14_device * pdev = (pdf14_device *) dev;
    pdf14_smaskcolor_t *smaskcolor = pdev->smaskcolor;
    gsicc_manager_t *icc_manager = pgs->icc_manager;
    int k;

    /* See comment in pdf14_increment_smask_color to understand this one */
    if (pdev->smaskcolor == NULL && pgs->icc_manager->smask_profiles != NULL &&
        pgs->icc_manager->smask_profiles->swapped) {
            return 0;
    }
    if (smaskcolor != NULL) {
        smaskcolor->ref_count--;
        if_debug1m(gs_debug_flag_icc, pgs->memory,
                   "[icc] Decrement smask color.  Now %d\n",
                   smaskcolor->ref_count);
        if (smaskcolor->ref_count == 0) {
            if_debug0m(gs_debug_flag_icc, pgs->memory, "[icc] Reset smask color.\n");
            /* Lets return the profiles and clean up */
            /* First see if we need to "reset" the profiles that are in
               the graphic state */
            if_debug0m(gs_debug_flag_icc, pgs->memory, "[icc] Reseting graphic state color spaces\n");
            for (k = 0; k < 2; k++) {
                gs_color_space *pcs = pgs->color[k].color_space;
                cmm_profile_t  *profile = pcs->cmm_icc_profile_data;
                if (profile != NULL) {
                    switch(profile->data_cs) {
                        case gsGRAY:
                            if (profile->hashcode ==
                                pgs->icc_manager->default_gray->hashcode) {
                                    profile =
                                        smaskcolor->profiles->smask_gray;
                            }
                            break;
                        case gsRGB:
                            if (profile->hashcode ==
                                pgs->icc_manager->default_rgb->hashcode) {
                                    profile =
                                        smaskcolor->profiles->smask_rgb;
                            }
                            break;
                        case gsCMYK:
                            if (profile->hashcode ==
                                pgs->icc_manager->default_cmyk->hashcode) {
                                    profile =
                                        smaskcolor->profiles->smask_cmyk;
                            }
                            break;
                        default:

                            break;
                    }
                    if (pcs->cmm_icc_profile_data != profile) {
                        gsicc_adjust_profile_rc(profile, 1, "pdf14_decrement_smask_color");
                        gsicc_adjust_profile_rc(pcs->cmm_icc_profile_data, -1, "pdf14_decrement_smask_color");
                        pcs->cmm_icc_profile_data = profile;
                    }
                }
            }

            gsicc_adjust_profile_rc(icc_manager->default_gray, -1, "pdf14_decrement_smask_color");
            icc_manager->default_gray = smaskcolor->profiles->smask_gray;
            gsicc_adjust_profile_rc(icc_manager->default_rgb, -1, "pdf14_decrement_smask_color");
            icc_manager->default_rgb = smaskcolor->profiles->smask_rgb;
            gsicc_adjust_profile_rc(icc_manager->default_cmyk, -1, "pdf14_decrement_smask_color");
            icc_manager->default_cmyk = smaskcolor->profiles->smask_cmyk;
            icc_manager->smask_profiles->swapped = false;
            /* We didn't increment the reference count when we assigned these
             * so NULL them to avoid decrementing when smaskcolor is freed
             */
            smaskcolor->profiles->smask_gray =
              smaskcolor->profiles->smask_rgb =
              smaskcolor->profiles->smask_cmyk = NULL;

            pdf14_free_smask_color(pdev);
        }
    }
    return 0;
}

static void
pdf14_free_smask_color(pdf14_device * pdev)
{
    if (pdev->smaskcolor != NULL) {
        if ( pdev->smaskcolor->profiles != NULL) {
            /* Do not decrement the profiles - the references were moved
               here and moved back again, so the ref counts don't change
             */
            gs_free_object(pdev->memory->stable_memory, pdev->smaskcolor->profiles,
                        "pdf14_free_smask_color");
        }
        gs_free_object(pdev->memory->stable_memory, pdev->smaskcolor, "pdf14_free_smask_color");
        pdev->smaskcolor = NULL;
    }
}

static void
pdf14_device_finalize(const gs_memory_t *cmem, void *vptr)
{
    gx_device * const dev = (gx_device *)vptr;
    pdf14_device * pdev = (pdf14_device *)dev;

    pdf14_cleanup_group_color_profiles (pdev);

    if (pdev->ctx) {
        pdf14_ctx_free(pdev->ctx);
        pdev->ctx = NULL;
    }

    while (pdev->color_model_stack) {
        pdf14_pop_group_color(dev, NULL);
    }
    gx_device_finalize(cmem, vptr);
}

#if DUMP_MASK_STACK

static void
dump_mask_stack(pdf14_mask_t *mask_stack)
{
    pdf14_mask_t *curr_mask = mask_stack;
    int level = 0;

    while (curr_mask != NULL) {
        if_debug1m('v', curr_mask->memory, "[v]mask_level, %d\n", level);
        if_debug1m('v', curr_mask->memory, "[v]mask_buf, %p\n", curr_mask->rc_mask->mask_buf);
        if_debug1m('v', curr_mask->memory, "[v]rc_count, %ld\n", curr_mask->rc_mask->rc.ref_count);
        level++;
        curr_mask = curr_mask->previous;
    }
}

/* A function to display the current state of the mask stack */
static void
pdf14_debug_mask_stack_state(pdf14_ctx *ctx)
{
    if_debug1m('v', ctx->memory, "[v]ctx_maskstack, %p\n", ctx->mask_stack);
    if (ctx->mask_stack != NULL) {
        dump_mask_stack(ctx->mask_stack);
    }
    if_debug1m('v', ctx->memory, "[v]ctx_stack, %p\n", ctx->stack);
    if (ctx->stack != NULL) {
        if_debug1m('v', ctx->memory, "[v]ctx_stack_maskstack, %p\n", ctx->stack->mask_stack);
        if (ctx->stack->mask_stack != NULL) {
            dump_mask_stack(ctx->stack->mask_stack);
        }
    }
}

#else

#ifdef DEBUG
static void
pdf14_debug_mask_stack_state(pdf14_ctx *ctx)
{
    return;
}
#endif

#endif /* DUMP_MASK_STACK */
