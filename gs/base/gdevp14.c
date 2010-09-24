/* Copyright (C) 2001-2006 Artifex Software, Inc.
   All Rights Reserved.
  
   This software is provided AS-IS with no warranty, either express or
   implied.

   This software is distributed under license and may not be copied, modified
   or distributed except as expressly authorized under the terms of that
   license.  Refer to licensing information at http://www.artifex.com/
   or contact Artifex Software, Inc.,  7 Mt. Lassen Drive - Suite A-134,
   San Rafael, CA  94903, U.S.A., +1(415)492-9861, for further information.
*/
/* $Id$	*/
/* Compositing devices for implementing	PDF 1.4	imaging	model */

#include "math_.h"
#include "memory_.h"
#include "gx.h"
#include "gserrors.h"
#include "gscdefs.h"
#include "gxdevice.h"
#include "gsdevice.h"
#include "gsstruct.h"
#include "gxistate.h"
#include "gxdcolor.h"
#include "gxiparam.h"
#include "gstparam.h"
#include "gxblend.h"
#include "gxtext.h"
#include "gsdfilt.h"
#include "gsimage.h"
#include "gsrect.h"
#include "gscoord.h"
#include "gzstate.h"
#include "gdevdevn.h"
#include "gdevp14.h"
#include "gsovrc.h"
#include "gxcmap.h"
#include "gscolor1.h"
#include "gstrans.h"
#include "gsutil.h"
#include "gxcldev.h"
#include "gxclpath.h"
#include "gxdcconv.h"
#include "vdtrace.h"
#include "gscolorbuffer.h"
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

#if RAW_DUMP
unsigned int global_index = 0;
unsigned int clist_band_count = 0;
#endif

/* Static prototypes */
/* Used for filling rects when we are doing a fill with a pattern that
   has transparency */
static int
pdf14_tile_pattern_fill(gx_device * pdev, const gs_imager_state * pis,
		     gx_path * ppath, const gx_fill_params * params,
		 const gx_device_color * pdevc, const gx_clip_path * pcpath);
static pdf14_mask_t *pdf14_mask_element_new(gs_memory_t *memory);
static void pdf14_free_smask_color(pdf14_device * pdev); 
static	int compute_group_device_int_rect(pdf14_device *pdev, gs_int_rect *rect, 
                              const gs_rect *pbbox, gs_imager_state *pis);


/* Functions for dealing with soft mask color */
static int pdf14_decrement_smask_color(gs_imager_state * pis, gx_device * dev);
static int pdf14_increment_smask_color(gs_imager_state * pis, gx_device * dev);

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
void pdf14_cmyk_cs_to_cmyk_cm(gx_device *, frac, frac, frac, frac, frac *);
int gs_pdf14_device_push(gs_memory_t *, gs_imager_state *, gx_device **,
			gx_device *, const gs_pdf14trans_t *);
static int pdf14_tile_pattern_fill(gx_device * pdev,
                const gs_imager_state * pis, gx_path * ppath,
                const gx_fill_params * params,
                const gx_device_color * pdevc, const gx_clip_path * pcpath);
static pdf14_mask_t * pdf14_mask_element_new(gs_memory_t * memory);

/* Buffer stack	data structure */
gs_private_st_ptrs5(st_pdf14_buf, pdf14_buf, "pdf14_buf",
		    pdf14_buf_enum_ptrs, pdf14_buf_reloc_ptrs,
		    saved, data, transfer_fn, maskbuf, parent_color_info_procs);

gs_private_st_ptrs2(st_pdf14_ctx, pdf14_ctx, "pdf14_ctx",
		    pdf14_ctx_enum_ptrs, pdf14_ctx_reloc_ptrs,
		    stack, maskbuf);

gs_private_st_ptrs1(st_pdf14_clr, pdf14_parent_color_t, "pdf14_clr",
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

static	int pdf14_open(gx_device * pdev);
static	dev_proc_close_device(pdf14_close);
static	int pdf14_output_page(gx_device	* pdev,	int num_copies,	int flush);
static	dev_proc_put_params(pdf14_put_params);
static	dev_proc_get_color_comp_index(pdf14_cmykspot_get_color_comp_index);
static	dev_proc_get_color_mapping_procs(pdf14_cmykspot_get_color_mapping_procs);
dev_proc_encode_color(pdf14_encode_color);
dev_proc_encode_color(pdf14_encode_color_tag);
dev_proc_encode_color(pdf14_compressed_encode_color);
dev_proc_decode_color(pdf14_decode_color);
dev_proc_decode_color(pdf14_compressed_decode_color);
static	dev_proc_fill_rectangle(pdf14_fill_rectangle);
static	dev_proc_fill_rectangle(pdf14_mark_fill_rectangle);
static	dev_proc_fill_rectangle(pdf14_mark_fill_rectangle_ko_simple);
static	dev_proc_fill_path(pdf14_fill_path);
static  dev_proc_copy_mono(pdf14_copy_mono);
static	dev_proc_fill_mask(pdf14_fill_mask);
static	dev_proc_stroke_path(pdf14_stroke_path);
static	dev_proc_begin_typed_image(pdf14_begin_typed_image);
static	dev_proc_text_begin(pdf14_text_begin);
static	dev_proc_create_compositor(pdf14_create_compositor);
static	dev_proc_create_compositor(pdf14_forward_create_compositor);
static	dev_proc_begin_transparency_group(pdf14_begin_transparency_group);
static	dev_proc_end_transparency_group(pdf14_end_transparency_group);
static	dev_proc_begin_transparency_mask(pdf14_begin_transparency_mask);
static	dev_proc_end_transparency_mask(pdf14_end_transparency_mask);
static  dev_proc_pattern_manage(pdf14_pattern_manage);
static int pdf14_clist_get_param_compressed_color_list(pdf14_device * p14dev);
static	dev_proc_push_transparency_state(pdf14_push_transparency_state);
static	dev_proc_pop_transparency_state(pdf14_pop_transparency_state);

static	const gx_color_map_procs *
    pdf14_get_cmap_procs(const gs_imager_state *, const gx_device *);
static	const gx_color_map_procs *
    pdf14_get_cmap_procs_group(const gs_imager_state *, const gx_device *);

#define	XSIZE (int)(8.5	* X_DPI)	/* 8.5 x 11 inch page, by default */
#define	YSIZE (int)(11 * Y_DPI)

/* 24-bit color. */

#define	pdf14_dev_procs(get_color_mapping_procs, get_color_comp_index, encode_color, decode_color) \
{\
	pdf14_open,			/* open */\
	NULL,				/* get_initial_matrix */\
	NULL,				/* sync_output */\
	pdf14_output_page,		/* output_page */\
	pdf14_close,			/* close */\
	encode_color,			/* rgb_map_rgb_color */\
	decode_color,			/* gx_default_rgb_map_color_rgb */\
	pdf14_fill_rectangle,		/* fill_rectangle */\
	NULL,				/* tile_rectangle */\
	pdf14_copy_mono,		/* copy_mono */\
	NULL,				/* copy_color */\
	NULL,				/* draw_line */\
	NULL,				/* get_bits */\
	NULL,				/* get_params */\
	pdf14_put_params,		/* put_params */\
	NULL,				/* map_cmyk_color */\
	NULL,				/* get_xfont_procs */\
	NULL,				/* get_xfont_device */\
	NULL,				/* map_rgb_alpha_color */\
	NULL,				/* get_page_device */\
	NULL,				/* get_alpha_bits */\
	NULL,				/* copy_alpha */\
	NULL,				/* get_band */\
	NULL,				/* copy_rop */\
	pdf14_fill_path,		/* fill_path */\
	pdf14_stroke_path,		/* stroke_path */\
	pdf14_fill_mask,		/* fill_mask */\
	NULL,				/* fill_trapezoid */\
	NULL,				/* fill_parallelogram */\
	NULL,				/* fill_triangle */\
	NULL,				/* draw_thin_line */\
	NULL,				/* begin_image */\
	NULL,				/* image_data */\
	NULL,				/* end_image */\
	NULL,				/* strip_tile_rectangle */\
	NULL,				/* strip_copy_rop, */\
	NULL,				/* get_clipping_box */\
	pdf14_begin_typed_image,	/* begin_typed_image */\
	NULL,				/* get_bits_rectangle */\
	NULL,				/* map_color_rgb_alpha */\
	pdf14_create_compositor,	/* create_compositor */\
	NULL,				/* get_hardware_params */\
	pdf14_text_begin,		/* text_begin */\
	NULL,				/* finish_copydevice */\
	pdf14_begin_transparency_group,\
	pdf14_end_transparency_group,\
	pdf14_begin_transparency_mask,\
	pdf14_end_transparency_mask,\
	NULL,				/* discard_transparency_layer */\
	get_color_mapping_procs,	/* get_color_mapping_procs */\
	get_color_comp_index,		/* get_color_comp_index */\
	encode_color,			/* encode_color */\
	decode_color,			/* decode_color */\
	pdf14_pattern_manage,		/* pattern_manage */\
	NULL,				/* fill_rectangle_hl_color */\
	NULL,				/* include_color_space */\
	NULL,				/* fill_linear_color_scanline */\
	NULL,				/* fill_linear_color_trapezoid */\
	NULL,				/* fill_linear_color_triangle */\
	gx_forward_update_spot_equivalent_colors,	/* update spot */\
        NULL,                           /* DevN params */\
        NULL,                           /* fill page */\
        pdf14_push_transparency_state,\
        pdf14_pop_transparency_state\
}

static	const gx_device_procs pdf14_Gray_procs =
	pdf14_dev_procs(gx_default_DevGray_get_color_mapping_procs,
			gx_default_DevGray_get_color_comp_index,
		       	pdf14_encode_color, pdf14_decode_color);

static	const gx_device_procs pdf14_RGB_procs =
	pdf14_dev_procs(gx_default_DevRGB_get_color_mapping_procs,
			gx_default_DevRGB_get_color_comp_index,
		       	pdf14_encode_color, pdf14_decode_color);

static	const gx_device_procs pdf14_CMYK_procs =
	pdf14_dev_procs(gx_default_DevCMYK_get_color_mapping_procs,
			gx_default_DevCMYK_get_color_comp_index,
		       	pdf14_encode_color, pdf14_decode_color);

#if USE_COMPRESSED_ENCODING
static	const gx_device_procs pdf14_CMYKspot_procs =
	pdf14_dev_procs(pdf14_cmykspot_get_color_mapping_procs,
			pdf14_cmykspot_get_color_comp_index,
		       	pdf14_compressed_encode_color, pdf14_compressed_decode_color);
#else
static	const gx_device_procs pdf14_CMYKspot_procs =
	pdf14_dev_procs(pdf14_cmykspot_get_color_mapping_procs,
			pdf14_cmykspot_get_color_comp_index,
		       	pdf14_encode_color, pdf14_decode_color);
#endif

static	const gx_device_procs pdf14_custom_procs =
	pdf14_dev_procs(gx_forward_get_color_mapping_procs,
			gx_forward_get_color_comp_index,
		       	gx_forward_encode_color,
		       	gx_forward_decode_color);

gs_private_st_composite_use_final(st_pdf14_device, pdf14_device, "pdf14_device",
				  pdf14_device_enum_ptrs, pdf14_device_reloc_ptrs,
			  gx_device_finalize);

static int pdf14_put_image(gx_device * dev, gs_imager_state * pis,
	       						gx_device * target);
static int pdf14_cmykspot_put_image(gx_device * dev, gs_imager_state * pis,
	       						gx_device * target);
static int pdf14_custom_put_image(gx_device * dev, gs_imager_state * pis,
	       						gx_device * target);

/* Used to alter device color mapping procs based upon group or softmask color space */
static int pdf14_update_device_color_procs(gx_device *dev,
			      gs_transparency_color_t group_color, int64_t icc_hashcode,
			      gs_imager_state *pis, cmm_profile_t *iccprofile);


/* Used to alter device color mapping procs based upon group or softmask color space */
/* Uses color procs stack so that it can be used with clist writer */
static int
pdf14_update_device_color_procs_push_c(gx_device *dev,
			      gs_transparency_color_t group_color, int64_t icc_hashcode,
			      gs_imager_state *pis, cmm_profile_t *iccprofile);

static int
pdf14_update_device_color_procs_pop_c(gx_device *dev,gs_imager_state *pis);


static void pdf14_push_parent_color(gx_device *dev, const gs_imager_state *pis);
static void pdf14_pop_parent_color(gx_device *dev, const gs_imager_state *pis);

static const pdf14_procs_t gray_pdf14_procs = {
    pdf14_unpack_additive,
    pdf14_put_image
};

static const pdf14_procs_t rgb_pdf14_procs = {
    pdf14_unpack_additive,
    pdf14_put_image
};

static const pdf14_procs_t cmyk_pdf14_procs = {
    pdf14_unpack_subtractive,
    pdf14_put_image
};

static const pdf14_procs_t cmykspot_pdf14_procs = {
    pdf14_unpack_compressed,
    pdf14_cmykspot_put_image
};

static const pdf14_procs_t custom_pdf14_procs = {
    pdf14_unpack_custom,
    pdf14_custom_put_image
};

static const pdf14_nonseparable_blending_procs_t gray_blending_procs = {
    art_blend_luminosity_custom_8,
    art_blend_saturation_custom_8
};

static const pdf14_nonseparable_blending_procs_t rgb_blending_procs = {
    art_blend_luminosity_rgb_8,
    art_blend_saturation_rgb_8
};

static const pdf14_nonseparable_blending_procs_t cmyk_blending_procs = {
    art_blend_luminosity_cmyk_8,
    art_blend_saturation_cmyk_8
};

static const pdf14_nonseparable_blending_procs_t custom_blending_procs = {
    art_blend_luminosity_custom_8,
    art_blend_saturation_custom_8
};

const pdf14_device gs_pdf14_Gray_device	= {
    std_device_std_color_full_body_type(pdf14_device, &pdf14_Gray_procs, "pdf14gray",
				&st_pdf14_device,
				XSIZE, YSIZE, X_DPI, Y_DPI, 8, 
	                        0, 0, 0, 0, 0, 0),
    { 0 },			/* Procs */
    NULL,			/* target */
    { 0 },			/* devn_params - not used */
    &gray_pdf14_procs,
    &gray_blending_procs
};

const pdf14_device gs_pdf14_RGB_device = {
    std_device_color_stype_body(pdf14_device, &pdf14_RGB_procs, "pdf14RGB",
				&st_pdf14_device,
				XSIZE, YSIZE, X_DPI, Y_DPI, 24, 255, 256),
    { 0 },			/* Procs */
    NULL,			/* target */
    { 0 },			/* devn_params - not used */
    &rgb_pdf14_procs,
    &rgb_blending_procs
};

const pdf14_device gs_pdf14_CMYK_device	= {
    std_device_std_color_full_body_type(pdf14_device, &pdf14_CMYK_procs,
	    "pdf14cmyk", &st_pdf14_device, XSIZE, YSIZE, X_DPI, Y_DPI, 32,
	    0, 0, 0, 0, 0, 0),
    { 0 },			/* Procs */
    NULL,			/* target */
    { 0 },			/* devn_params - not used */
    &cmyk_pdf14_procs,
    &cmyk_blending_procs
};

const pdf14_device gs_pdf14_CMYKspot_device	= {
    std_device_part1_(pdf14_device, &pdf14_CMYKspot_procs, "pdf14cmykspot", 
                        &st_pdf14_device, open_init_closed),
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
    std_device_part1_(pdf14_device, &pdf14_custom_procs, "pdf14custom", 
                        &st_pdf14_device, open_init_closed),
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
case 1: return ENUM_OBJ(pdev->trans_group_parent_cmap_procs);
case 2: return ENUM_OBJ(pdev->smaskcolor);
case 3:	ENUM_RETURN(gx_device_enum_ptr(pdev->target));
case 4: ENUM_RETURN(pdev->devn_params.compressed_color_list);
ENUM_PTRS_END

static	RELOC_PTRS_WITH(pdf14_device_reloc_ptrs, pdf14_device *pdev)
{
    {
	int i;

	for (i = 0; i < pdev->devn_params.separations.num_separations; ++i) {
	    RELOC_PTR(pdf14_device, devn_params.separations.names[i].data);
	}
    }
    RELOC_PTR(pdf14_device, devn_params.compressed_color_list);
    RELOC_VAR(pdev->ctx);
    RELOC_VAR(pdev->smaskcolor);
    RELOC_VAR(pdev->trans_group_parent_cmap_procs);
    pdev->target = gx_device_reloc_ptr(pdev->target, gcst);
}
RELOC_PTRS_END

/* ------ Private definitions ------ */

/**
 * pdf14_buf_new: Allocate a new PDF 1.4 buffer.
 * @n_chan: Number of pixel channels including alpha.
 *
 * Return value: Newly allocated buffer, or NULL on failure.
 **/
static	pdf14_buf *
pdf14_buf_new(gs_int_rect *rect, bool has_tags, bool has_alpha_g, 
              bool has_shape, bool idle, int n_chan, gs_memory_t *memory)
{

	/* Note that alpha_g is the alpha for the GROUP */
	/* This is distinct from the alpha that may also exist */
	/* for the objects within the group.  Hence it can introduce */
	/* yet another plane */

    pdf14_buf *result;
    pdf14_parent_color_t *new_parent_color;
    int rowstride = (rect->q.x - rect->p.x + 3) & -4;
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
    result->maskbuf = NULL;
    result->idle = idle;
    result->mask_id = 0;
    new_parent_color = gs_alloc_struct(memory, pdf14_parent_color_t, &st_pdf14_clr,
					        "pdf14_buf_new");
    result->parent_color_info_procs = new_parent_color;
    result->parent_color_info_procs->get_cmap_procs = NULL;
    result->parent_color_info_procs->parent_color_mapping_procs = NULL;
    result->parent_color_info_procs->parent_color_comp_index = NULL;
    result->parent_color_info_procs->icc_profile = NULL;
    result->parent_color_info_procs->previous = NULL;
    if (height <= 0 || idle) {
	/* Empty clipping - will skip all drawings. */
	result->planestride = 0;
	result->data = 0;
    } else {
	planestride = rowstride * height;
	result->planestride = planestride;
	result->data = gs_alloc_bytes(memory, planestride * n_planes,
					"pdf14_buf_new");
	if (result->data == NULL) {
	    gs_free_object(memory, result, "pdf_buf_new");
	    return NULL;
	}
	if (has_alpha_g) {
	    int alpha_g_plane = n_chan + (has_shape ? 1 : 0);
	    memset (result->data + alpha_g_plane * planestride, 0, planestride);
	}
        if (has_tags) {
            int tags_plane = n_chan + (has_shape ? 1 : 0) + (has_alpha_g ? 1 : 0);
            memset (result->data + tags_plane * planestride, 
                    GS_UNTOUCHED_TAG, planestride);
        }
    }
    /* Initialize bbox with the reversed rectangle for further accumulation : */
    result->bbox.p.x = rect->q.x;
    result->bbox.p.y = rect->q.y;
    result->bbox.q.x = rect->p.x;
    result->bbox.q.y = rect->p.y;
    return result;
}

static	void
pdf14_buf_free(pdf14_buf *buf, gs_memory_t *memory)
{
    gs_free_object(memory, buf->maskbuf, "pdf14_buf_free");
    gs_free_object(memory, buf->transfer_fn, "pdf14_buf_free");
    gs_free_object(memory, buf->data, "pdf14_buf_free");
    gs_free_object(memory, buf->parent_color_info_procs, "pdf14_buf_free");
    gs_free_object(memory, buf, "pdf14_buf_free");
}


static void
rc_pdf14_maskbuf_free(gs_memory_t * mem, void *ptr_in, client_name_t cname)
{
    /* Ending the mask buffer. */
    pdf14_rcmask_t *rcmask = (pdf14_rcmask_t * ) ptr_in;
    /* free the pdf14 buffer. */
    if ( rcmask->mask_buf != NULL ){
        pdf14_buf_free(rcmask->mask_buf, mem);
    }
    gs_free_object(mem, rcmask, "rc_pdf14_maskbuf_free");
}

static	pdf14_rcmask_t *
pdf14_rcmask_new(gs_memory_t *memory)
{
    pdf14_rcmask_t *result;

    result = gs_alloc_struct(memory, pdf14_rcmask_t, &st_pdf14_rcmask,
			     "pdf14_maskbuf_new");
    if ( result == NULL )
        return(NULL);
    rc_init_free(result, memory, 1, rc_pdf14_maskbuf_free);
    result->mask_buf = NULL;
    result->memory = memory;
    return(result);
}

static	pdf14_ctx *
pdf14_ctx_new(gs_int_rect *rect, int n_chan, bool additive, gs_memory_t	*memory)
{
    pdf14_ctx *result;
    pdf14_buf *buf;
    bool has_tags;

    has_tags = (memory->gs_lib_ctx->BITTAG != GS_DEVICE_DOESNT_SUPPORT_TAGS);

    result = gs_alloc_struct(memory, pdf14_ctx, &st_pdf14_ctx,
			     "pdf14_ctx_new");
    if (result == NULL)
	return result;
    /* Note:  buffer creation expects alpha to be in number of channels */
    buf = pdf14_buf_new(rect, has_tags, false, false, false, n_chan+1, memory);
    if (buf == NULL) {
	gs_free_object(memory, result, "pdf14_ctx_new");
	return NULL;
    }
    if_debug4('v', "[v]base buf: %d x %d, %d color channels, %d planes \n",
	      buf->rect.q.x, buf->rect.q.y, buf->n_chan, buf->n_planes);
    if (buf->data != NULL) {
        if (buf->has_tags) {
	    memset(buf->data, 0, buf->planestride * (buf->n_planes-1));
        } else {
	    memset(buf->data, 0, buf->planestride * buf->n_planes);
        }
    }
    buf->saved = NULL;
    result->stack = buf;
    result->maskbuf = pdf14_mask_element_new(memory);
    result->n_chan = n_chan;
    result->memory = memory;
    result->rect = *rect;
    result->additive = additive;
    return result;
}

static	void
pdf14_ctx_free(pdf14_ctx *ctx)
{
    pdf14_buf *buf, *next;

    if (ctx->maskbuf) {
	/* A mask was created but was not used in this band. */
        rc_decrement(ctx->maskbuf->rc_mask, "pdf14_ctx_free");
        gs_free_object(ctx->memory,ctx->maskbuf,"pdf14_ctx_free");
    }
    for (buf = ctx->stack; buf != NULL; buf = next) {
	next = buf->saved;
	pdf14_buf_free(buf, ctx->memory);
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
pdf14_find_backdrop_buf(pdf14_ctx *ctx)
{
    pdf14_buf *buf = ctx->stack;

    while (buf != NULL) {
	if (buf->isolated) return NULL;
	if (!buf->knockout) return buf->saved;
	buf = buf->saved;
    }
    /* this really shouldn't happen, as bottom-most buf should be
       non-knockout */
    return NULL;
}

static	int
pdf14_push_transparency_group(pdf14_ctx	*ctx, gs_int_rect *rect,
			      bool isolated, bool knockout,
			      byte alpha, byte shape,
			      gs_blend_mode_t blend_mode, bool idle,
			      uint mask_id, int numcomps)
{
    pdf14_buf *tos = ctx->stack;
    pdf14_buf *buf, *backdrop;
    bool has_shape, has_tags;

    if_debug1('v', "[v]pdf14_push_transparency_group, idle = %d\n", idle);
    /* todo: fix this hack, which makes all knockout groups isolated.
       For the vast majority of files, there won't be any visible
       effects, but it still isn't correct. The pixel compositing code
       for non-isolated knockout groups gets pretty hairy, which is
       why this is here. */
    if (knockout) 
	isolated = true;
    has_shape = tos->has_shape || tos->knockout;
    has_tags = tos->has_tags;
    /* We need to create this based upon the size of
    the color space + an alpha channel. NOT the device size
    or the previous ctx size. */
    /* The second parameter in pdf14_buf_new decides if we should
       add a GROUP alpha channel to the buffer.  If it is NOT isolated, then this
       buffer will be added.  If it is isolated, then the buffer will not be added.
       I question the redundancy here of the alpha and the group alpha channel, 
       but that will need to be looked at later. */
    buf = pdf14_buf_new(rect, has_tags, !isolated, has_shape, idle, 
                        numcomps+1, ctx->memory);
    if_debug4('v', "[v]base buf: %d x %d, %d color channels, %d planes \n",
	      buf->rect.q.x, buf->rect.q.y, buf->n_chan, buf->n_planes);
    if (buf == NULL)
	return_error(gs_error_VMerror);
    buf->isolated = isolated;
    buf->knockout = knockout;
    buf->alpha = alpha;
    buf->shape = shape;
    buf->blend_mode = blend_mode;
    buf->mask_id = mask_id;
    buf->maskbuf = ctx->maskbuf; /* Save becasuse the group rendering may set up 
				    another (nested) mask. */
    ctx->maskbuf = NULL; /* Clean the mask field for rendering this group. 
			    See pdf14_pop_transparency_group how to handle it. */
    buf->saved = tos;
    ctx->stack = buf;
    if (buf->data == NULL)
	return 0;
    if (idle)
	return 0;
    backdrop = pdf14_find_backdrop_buf(ctx);
    if (backdrop == NULL)
	memset(buf->data, 0, buf->planestride * (buf->n_chan +
						 (buf->has_shape ? 1 : 0)));
    else 
	pdf14_preserve_backdrop(buf, tos, has_shape);
#if RAW_DUMP
  
    /* Dump the current buffer to see what we have. */

    dump_raw_buffer(ctx->stack->rect.q.y-ctx->stack->rect.p.y, 
                ctx->stack->rowstride, ctx->stack->n_planes,
                ctx->stack->planestride, ctx->stack->rowstride, 
                "TransGroupPush",ctx->stack->data);

    global_index++;
#endif
    return 0;
}

static	int
pdf14_pop_transparency_group(gs_imager_state *pis, pdf14_ctx *ctx,
    const pdf14_nonseparable_blending_procs_t * pblend_procs, 
    int curr_num_color_comp, cmm_profile_t *curr_icc_profile, gx_device *dev)
{
    pdf14_buf *tos = ctx->stack;
    pdf14_buf *nos = tos->saved;
    pdf14_mask_t *mask_stack = tos->maskbuf;
    pdf14_buf *maskbuf;
    int x0, x1, y0, y1;
    byte *new_data_buf = NULL;
    int num_noncolor_planes, new_num_planes;
    int num_cols, num_rows, num_newcolor_planes;
    bool icc_match;

    gsicc_rendering_param_t rendering_params;
    gsicc_link_t *icc_link;
    gsicc_bufferdesc_t input_buff_desc;
    gsicc_bufferdesc_t output_buff_desc;
    if ( mask_stack == NULL) {
        maskbuf = NULL;
    } else {
        maskbuf = mask_stack->rc_mask->mask_buf;
    }
    if (nos == NULL)
	return_error(gs_error_rangecheck);
    y0 = max(tos->rect.p.y, nos->rect.p.y);
    y1 = min(tos->rect.q.y, nos->rect.q.y);
    x0 = max(tos->rect.p.x, nos->rect.p.x);
    x1 = min(tos->rect.q.x, nos->rect.q.x);
    if (ctx->maskbuf) {
	/* This can occur when we have a situation where we are ending out of 
           a group that has internal to it a soft mask and another group.  
           The soft mask left over from the previous trans group pop is put 
           into ctx->masbuf, since it is still active if another trans group
           push occurs to use it.  If one does not occur, but instead we find 
           ourselves popping from a parent group, then this softmask is no 
           longer needed.  We will rc_decrement and set it to NULL. */
        rc_decrement(ctx->maskbuf->rc_mask, "pdf14_pop_transparency_group");
        if (ctx->maskbuf->rc_mask == NULL ){
            gs_free_object(ctx->memory, ctx->maskbuf, "pdf14_pop_transparency_group");
        }
	ctx->maskbuf = NULL;
    }
    ctx->maskbuf = mask_stack;  /* Restore the mask saved by pdf14_push_transparency_group. */
    tos->maskbuf = NULL;        /* Clean the pointer sinse the mask ownership is now passed to ctx. */
    if (tos->idle)
	goto exit;
    if (maskbuf != NULL && maskbuf->data == NULL && maskbuf->alpha == 255)
	goto exit;
    if (maskbuf != NULL) {
	y0 = max(y0, maskbuf->rect.p.y);
	y1 = min(y1, maskbuf->rect.q.y);
	x0 = max(x0, maskbuf->rect.p.x);
	x1 = min(x1, maskbuf->rect.q.x);
    }
#if RAW_DUMP
    /* Dump the current buffer to see what we have. */
    dump_raw_buffer(ctx->stack->rect.q.y-ctx->stack->rect.p.y, 
                ctx->stack->rowstride, ctx->stack->n_planes,
                ctx->stack->planestride, ctx->stack->rowstride, 
                "Trans_Group_Pop",ctx->stack->data);
#endif
/* Note currently if a pattern space has transparency, the ICC profile is not used
   for blending purposes.  Instead we rely upon the gray, rgb, or cmyk parent space.
   This is partially due to the fact that pdf14_pop_transparency_group and
   pdf14_push_transparnecy_group have no real ICC interaction and those are the
   operations called in the tile transparency code.  Instead we may want to
   look at pdf14_begin_transparency_group and pdf14_end_transparency group which
   is where all the ICC information is handled.  We will return to look at that later */
    if ( nos->parent_color_info_procs->icc_profile != NULL ) {
        icc_match = (nos->parent_color_info_procs->icc_profile->hashcode != 
                        curr_icc_profile->hashcode);
    } else {
        /* Let the other tests make the decision if we need to transform */
        icc_match = false;
    }
    /* If the color spaces are different and we actually did do a swap of 
       the procs for color */
    if ( (nos->parent_color_info_procs->num_components != curr_num_color_comp && 
        nos->parent_color_info_procs->parent_color_mapping_procs != NULL) 
                    || icc_match ) {
        if (x0 < x1 && y0 < y1) {
            /* The NOS blending color space is different than that of the
               TOS.  It is necessary to transform the TOS buffer data to the
               color space of the NOS prior to doing the pdf14_compose_group 
               operation.  */
            num_noncolor_planes = tos->n_planes - curr_num_color_comp;
            num_newcolor_planes = nos->parent_color_info_procs->num_components;
            new_num_planes = num_noncolor_planes + num_newcolor_planes;
            /* See if we are doing ICC based conversion */
            if (nos->parent_color_info_procs->icc_profile != NULL && 
                curr_icc_profile != NULL) {                
                /* Use the ICC color management for buffer color conversion */
                /* Define the rendering intents */
                rendering_params.black_point_comp = BP_ON;
                rendering_params.object_type = GS_IMAGE_TAG;
                rendering_params.rendering_intent = gsPERCEPTUAL;
                /* Request the ICC link for the transform that we will need to use */
                /* Note that if pis is NULL we assume the same color space.  This
                   is due to a call to pop the group from fill_mask when filling
                   with a mask with transparency.  In that case, the parent
                   and the child will have the same color space anyway */
                icc_link = gsicc_get_link_profile(pis, dev, curr_icc_profile, 
                    nos->parent_color_info_procs->icc_profile,
                    &rendering_params, pis->memory, false);
                /* If the link is the identity, then we don't need to do 
                   any color conversions */
                if ( !(icc_link->is_identity) ) {
                    /* Before we do any allocations check if we can get away with
                       reusing the existing buffer if it is the same size ( if it is
                       smaller go ahead and allocate).  We could reuse it in this
                       case too.  We need to do a bit of testing to determine what
                       would be best.  */
                    if( num_newcolor_planes != curr_num_color_comp ) {
                        /* Different size.  We will need to allocate */
                        new_data_buf = gs_alloc_bytes(ctx->memory, 
                                            tos->planestride*new_num_planes,
					        "pdf14_buf_new");
            if (new_data_buf == NULL)	    
                return_error(gs_error_VMerror);
                        /* Copy over the noncolor planes. */
                        memcpy(new_data_buf + tos->planestride * num_newcolor_planes, 
                            tos->data + tos->planestride * curr_num_color_comp, 
                            tos->planestride * num_noncolor_planes);
                    } else {
                        /* In place color conversion! */
                        new_data_buf = tos->data;
                    }
                    /* Set up the buffer descriptors. Note that pdf14 always has
                       the alpha channels at the back end (last planes).  
                       We will just handle that here and let the CMM know 
                       nothing about it */
                    num_rows = tos->rect.q.y - tos->rect.p.y;
                    num_cols = tos->rect.q.x - tos->rect.p.x;
                    gsicc_init_buffer(&input_buff_desc, curr_num_color_comp, 1,
                                  false, false, true, tos->planestride, tos->rowstride,
                                  num_rows, num_cols);
                    gsicc_init_buffer(&output_buff_desc, 
                        nos->parent_color_info_procs->num_components, 1,
                                  false, false, true, tos->planestride, 
                                  tos->rowstride, num_rows, num_cols);
                    /* Transform the data */
                    gscms_transform_color_buffer(icc_link, &input_buff_desc, 
                                        &output_buff_desc, tos->data, 
                                        new_data_buf);
                }
                /* Release the link */
                gsicc_release_link(icc_link);
                /* free the old object if the color spaces were different sizes */
                if( num_newcolor_planes != curr_num_color_comp ) {
                    gs_free_object(ctx->memory, tos->data, "pdf14_buf_free");
                    tos->data = new_data_buf;
                }
            } else {
                /* Non ICC based transform */
                new_data_buf = gs_alloc_bytes(ctx->memory, 
                                    tos->planestride*new_num_planes,"pdf14_buf_new");
                if (new_data_buf == NULL)	    
                    return_error(gs_error_VMerror);
                gs_transform_color_buffer_generic(tos->data, tos->rowstride, 
                            tos->planestride,curr_num_color_comp, tos->rect,
                            new_data_buf, num_newcolor_planes, num_noncolor_planes);
             /* Free the old object */
              gs_free_object(ctx->memory, tos->data, "pdf14_buf_free");
                 tos->data = new_data_buf;
            }
             /* Adjust the plane and channel size now */
             tos->n_chan = nos->n_chan;
             tos->n_planes = nos->n_planes;
#if RAW_DUMP
			/* Dump the current buffer to see what we have. */
			dump_raw_buffer(ctx->stack->rect.q.y-ctx->stack->rect.p.y, 
						ctx->stack->rowstride, ctx->stack->n_planes,
						ctx->stack->planestride, ctx->stack->rowstride, 
						"Trans_Group_ColorConv",ctx->stack->data);
#endif
             /* compose */
             pdf14_compose_group(tos, nos, maskbuf, x0, x1, y0, y1, nos->n_chan, 
                 nos->parent_color_info_procs->isadditive, 
                 nos->parent_color_info_procs->parent_blending_procs);
        }
    } else {
        /* Group color spaces are the same.  No color conversions needed */
        if (x0 < x1 && y0 < y1)
	    pdf14_compose_group(tos, nos, maskbuf, x0, x1, y0, y1,nos->n_chan, 
                                ctx->additive, pblend_procs);
    }
exit:
    ctx->stack = nos;
    {	/* If this group is one for an image with soft mask,
	   the containing group's mask was saved in maskbuf->maskbuf.
	   Set up it now to the transparensy context as undiong 
	   the save action done in pdf14_push_transparency_mask
	   when replacing==false; */
	/* ctx->maskbuf = (maskbuf != NULL ? maskbuf->maskbuf : NULL); */
    }
    if_debug1('v', "[v]pop buf, idle=%d\n", tos->idle);
    pdf14_buf_free(tos, ctx->memory);
    return 0;
}

/*
 * Create a transparency mask that will be used as the mask for
 * the next transparency group that is created afterwards.
 * The sequence of calls is:
 * push_mask, draw the mask, pop_mask, push_group, draw the group, pop_group
 */
static	int
pdf14_push_transparency_mask(pdf14_ctx *ctx, gs_int_rect *rect,	byte bg_alpha,
			     byte *transfer_fn, bool idle, bool replacing,
			     uint mask_id, gs_transparency_mask_subtype_t subtype, 
                             bool SMask_is_CIE, int numcomps,
                             int Background_components,
                             const float Background[],
                             const float GrayBackground)
{
    pdf14_buf *buf;
    unsigned char *curr_ptr, gray;
    
    if_debug2('v', "[v]pdf14_push_transparency_mask, idle=%d, replacing=%d\n", 
                    idle, replacing);
#if 0
    if (replacing && ctx->maskbuf != NULL) {
	if (ctx->maskbuf->maskbuf != NULL) {
	    /* Pass here when a mask of an image
	       is being replaced with the containing group's mask.
	       It looks as the image's mask covers some band,
	       but the image itself does not.
	       We believe it must not happen,
	       but provide a cleanup for safety.
	    */
	    pdf14_buf_free(ctx->maskbuf->maskbuf, ctx->memory);
	}
	pdf14_buf_free(ctx->maskbuf, ctx->memory);
	ctx->maskbuf = NULL;
    }
#endif
    /* An optimization to consider is that if the SubType is Alpha
       then we really should only be allocating the alpha band and
       only draw with that channel.  Current architecture makes that 
       a bit tricky.  We need to create this based upon the size of
       the color space + an alpha channel. NOT the device size
       or the previous ctx size */
    /* A mask doesnt worry about tags */
    buf = pdf14_buf_new(rect, false, false, false, idle, numcomps+1, 
                        ctx->memory);
    if (buf == NULL)
	return_error(gs_error_VMerror);
    buf->alpha = bg_alpha;
    /* fill in, but these values aren't really used */
    buf->isolated = true;
    buf->knockout = false;
    buf->shape = 0xff;
    buf->blend_mode = BLEND_MODE_Normal;
    buf->transfer_fn = transfer_fn;
    buf->mask_id = mask_id;
    {	/* If replacing=false, we start the mask for an image with SMask.
	   In this case the image's SMask temporary replaces the 
	   mask of the containing group. Save the containing droup's mask 
           in buf->maskbuf */
	buf->maskbuf = ctx->maskbuf;
        if (buf->maskbuf){
            rc_increment(buf->maskbuf->rc_mask);
        }
    }
#if RAW_DUMP
    /* Dump the current buffer to see what we have. */
    if (ctx->stack->planestride > 0 ){
        dump_raw_buffer(ctx->stack->rect.q.y-ctx->stack->rect.p.y, 
                    ctx->stack->rowstride, ctx->stack->n_planes,
                    ctx->stack->planestride, ctx->stack->rowstride, 
                    "Raw_Buf_PreSmask",ctx->stack->data);
        global_index++;
    }
#endif
    buf->saved = ctx->stack;
    ctx->stack = buf;
    /* Soft Mask related information so we know how to 
       compute luminosity when we pop the soft mask */
    buf->SMask_is_CIE = SMask_is_CIE;
    buf->SMask_SubType = subtype;
    if (buf->data != NULL){
        /* We need to initialize it to the BC if it existed */
        /* According to the spec, the CS has to be the same */
        /* If the back ground component is black, then don't bother 
           with this.  Since we are forcing the rendering to gray
           earlier now, go ahead and just use the GrayBackGround color 
           directly. */
        if ( Background_components && GrayBackground != 0.0 ) {
            curr_ptr = buf->data;
            gray = (unsigned char) (255.0 * GrayBackground);
            memset(curr_ptr, gray, buf->planestride); 
                curr_ptr +=  buf->planestride;
            /* If we have a background component that was not black, then we 
               need to set the alpha for this mask as if we had drawn in the
               entire soft mask buffer */
	    memset(curr_ptr, 255, buf->planestride *(buf->n_chan - 1));
        } else {
            /* Compose mask with opaque background */
	    memset(buf->data, 0, buf->planestride * buf->n_chan);
        }
    }
    return 0;
}

static	int
pdf14_pop_transparency_mask(pdf14_ctx *ctx, gs_imager_state *pis, gx_device *dev)
{
    pdf14_buf *tos = ctx->stack;
    byte *new_data_buf;
    int icc_match;
    cmm_profile_t *des_profile = tos->parent_color_info_procs->icc_profile; /* If set, this should be a gray profile */
    cmm_profile_t *src_profile = dev->device_icc_profile;    
    gsicc_rendering_param_t rendering_params;
    gsicc_link_t *icc_link;

    /* icc_match == -1 means old non-icc code.
       icc_match == 0 means use icc code 
       icc_match == 1 mean no conversion needed */
    if ( des_profile != NULL && src_profile != NULL ) {
        icc_match = (des_profile->hashcode ==  src_profile->hashcode);
    } else {
        icc_match = -1;
    }
    if_debug1('v', "[v]pdf14_pop_transparency_mask, idle=%d\n", tos->idle);
    ctx->stack = tos->saved;
    tos->saved = NULL;  /* To avoid issues with GC */
    if (tos->maskbuf) {
        /* During the soft mask push, the maskbuf was copied (not moved) from the ctx to the tos maskbuf. We 
           are done with this now so it is safe to just set to NULL.  However, before we do that we must perform 
           rc decrement to match the increment that occured was made.  Also, if this is the last ref
           count of the rc_mask, we should free the buffer now since no other groups need it. */
        rc_decrement(tos->maskbuf->rc_mask, "pdf14_pop_transparency_mask(tos->maskbuf->rc_mask)");
        if (tos->maskbuf->rc_mask) {
            if (tos->maskbuf->rc_mask->rc.ref_count == 1){
                rc_decrement(tos->maskbuf->rc_mask, 
                            "pdf14_pop_transparency_mask(tos->maskbuf->rc_mask)");
            }
        }
	tos->maskbuf = NULL;
    }
    if (tos->data == NULL ) {
        /* This can occur in clist rendering if the soft mask does
           not intersect the current band.  It would be nice to
           catch this earlier and just avoid creating the structure
           to begin with.  For now we need to delete the structure
           that was created.  Only delete if the alpha value is not
           255 */
        if (tos->alpha == 255) {
	    pdf14_buf_free(tos, ctx->memory);
	    ctx->maskbuf = NULL;
        } else {
            /* Assign as mask buffer */
            ctx->maskbuf = pdf14_mask_element_new(ctx->memory);
            ctx->maskbuf->rc_mask->mask_buf = tos;
        }
    } else {
        /* If we are already in the source space then there is no reason 
           to do the transformation */
        /* Lets get this to a monochrome buffer and map it to a luminance only value */
        /* This will reduce our memory.  We won't reuse the existing one, due */
        /* Due to the fact that on certain systems we may have issues recovering */
        /* the data after a resize */
	new_data_buf = gs_alloc_bytes(ctx->memory, tos->planestride,
					"pdf14_buf_new");
        if (new_data_buf == NULL)	    
            return_error(gs_error_VMerror);
        /* Initialize with 0.  Need to do this since in Smask_Luminosity_Mapping 
           we won't be filling everything during the remap if it had not been 
           written into by the PDF14 fill rect */
        memset(new_data_buf, 0, tos->planestride);
        /* If the subtype was alpha, then just grab the alpha channel now
           and we are all done */
        if (tos->SMask_SubType == TRANSPARENCY_MASK_Alpha) {
            smask_copy(tos->rect.q.y - tos->rect.p.y,
                       tos->rect.q.x - tos->rect.p.x, 
                       tos->rowstride, 
                       (tos->data)+tos->planestride, new_data_buf);
        } else {
            if ( icc_match == 1 || tos->n_chan == 2) {
                /* There is no need to convert.  Data is already gray scale.
                   We just need to copy the gray plane */
                smask_copy(tos->rect.q.y - tos->rect.p.y,
                           tos->rect.q.x - tos->rect.p.x, 
                           tos->rowstride, tos->data, new_data_buf);
            } else {
                if ( icc_match == -1 ) {
                    /* The slow old fashioned way */
                    smask_luminosity_mapping(tos->rect.q.y - tos->rect.p.y ,
                        tos->rect.q.x - tos->rect.p.x,tos->n_chan, 
                        tos->rowstride, tos->planestride, 
                        tos->data,  new_data_buf, ctx->additive,
                tos->SMask_is_CIE, tos->SMask_SubType); 
                } else {
                    /* ICC case where we use the CMM */
                    /* Request the ICC link for the transform that we will need to use */
                    rendering_params.black_point_comp = BP_OFF;
                    rendering_params.object_type = GS_IMAGE_TAG;
                    rendering_params.rendering_intent = gsPERCEPTUAL;
                    icc_link = gsicc_get_link_profile(pis, dev, des_profile, 
                        src_profile, &rendering_params, pis->memory, false);
                    smask_icc(tos->rect.q.y - tos->rect.p.y ,
                                        tos->rect.q.x - tos->rect.p.x,tos->n_chan, 
                                        tos->rowstride, tos->planestride, 
                                        tos->data, new_data_buf, icc_link);
                    /* Release the link */
                    gsicc_release_link(icc_link);
                }
            }
        }
         /* Free the old object, NULL test was above */
          gs_free_object(ctx->memory, tos->data, "pdf14_buf_free");
             tos->data = new_data_buf;
         /* Data is single channel now */
         tos->n_chan = 1;
         tos->n_planes = 1;
        /* Assign as reference counted mask buffer */
        ctx->maskbuf = pdf14_mask_element_new(ctx->memory);
        ctx->maskbuf->rc_mask->mask_buf = tos;
     }
    return 0;
}

static pdf14_mask_t *
pdf14_mask_element_new(gs_memory_t *memory)
{
    pdf14_mask_t *result;

    result = gs_alloc_struct(memory, pdf14_mask_t, &st_pdf14_mask,
			     "pdf14_mask_element_new");
    /* Get the reference counted mask */
    result->rc_mask = pdf14_rcmask_new(memory);
    result->previous = NULL;
    result->memory = memory;
    return(result);
}

static int
pdf14_push_transparency_state(gx_device *dev, gs_imager_state *pis)
{
    /* We need to push the current soft mask.  We need to
       be able to recover it if we draw a new one and
       then obtain a Q operation ( a pop ) */
    
    pdf14_device *pdev = (pdf14_device *)dev;
    pdf14_ctx *ctx = pdev->ctx;
    pdf14_mask_t *new_mask;

    if_debug0('v', "pdf14_push_transparency_state\n");
    /* We need to push the current mask buffer   */
    /* Allocate a new element for the stack.
       Don't do anything if there is no mask present.*/
    if ( ctx->maskbuf != NULL ) { 
        new_mask = pdf14_mask_element_new(ctx->memory);
        /* Duplicate and make the link */
        new_mask->rc_mask = ctx->maskbuf->rc_mask;
        rc_increment(new_mask->rc_mask);
        ctx->maskbuf->previous = new_mask;
    }
 return(0);
}

static int
pdf14_pop_transparency_state(gx_device *dev, gs_imager_state *pis)
{

    /* Pop the soft mask.  It is no longer needed. Likely due to 
       a Q that has occurred. */
    pdf14_device *pdev = (pdf14_device *)dev;
    pdf14_ctx *ctx = pdev->ctx;
    pdf14_mask_t *old_mask;

    if_debug0('v', "pdf14_pop_transparency_state\n");
    /* rc decrement the current link after we break it from
       the list, then free the stack element.  Don't do
       anything if there is no mask present. */
    if ( ctx->maskbuf != NULL ) {
        old_mask = ctx->maskbuf;
        ctx->maskbuf = ctx->maskbuf->previous;
        if (old_mask->rc_mask) {
            rc_decrement(old_mask->rc_mask, "pdf14_pop_transparency_state");
        }
        gs_free_object(old_mask->memory, old_mask, "pdf14_pop_transparency_state");
    }
 return(0);
}

static	int
pdf14_open(gx_device *dev)
{
    pdf14_device *pdev = (pdf14_device *)dev;
    gs_int_rect rect;

    if_debug2('v', "[v]pdf14_open: width = %d, height = %d\n",
	     dev->width, dev->height);
    rect.p.x = 0;
    rect.p.y = 0;
    rect.q.x = dev->width;
    rect.q.y = dev->height;
    pdev->ctx = pdf14_ctx_new(&rect, dev->color_info.num_components,
	pdev->color_info.polarity != GX_CINFO_POLARITY_SUBTRACTIVE, dev->memory);
    if (pdev->ctx == NULL)
	return_error(gs_error_VMerror);
    return 0;
}

static const gx_cm_color_map_procs pdf14_DeviceCMYKspot_procs = {
    pdf14_gray_cs_to_cmyk_cm, pdf14_rgb_cs_to_cmyk_cm, pdf14_cmyk_cs_to_cmyk_cm
};

static const gx_cm_color_map_procs *
pdf14_cmykspot_get_color_mapping_procs(const gx_device * dev)
{
    return &pdf14_DeviceCMYKspot_procs;
}


/* Used to passed along information about the buffer created by the
   pdf14 device.  This is used by the pattern accumulator when the
   pattern contains transparency */
int 
pdf14_get_buffer_information(const gx_device * dev, gx_pattern_trans_t *transbuff)
{

    const pdf14_device * pdev = (pdf14_device *)dev;
    pdf14_buf *buf;
    gs_int_rect rect;
    int x1,y1,width,height;

    if ( pdev->ctx == NULL){
        return 0;  /* this can occur if the pattern is a clist */
    }
    buf = pdev->ctx->stack;
    rect = buf->rect;
    rect_intersect(rect, buf->bbox);
    x1 = min(pdev->width, rect.q.x);
    y1 = min(pdev->height, rect.q.y);
    width = x1 - rect.p.x;
    height = y1 - rect.p.y;
    if (width <= 0 || height <= 0 || buf->data == NULL)
	return 0;
    transbuff->pdev14 = dev;
    transbuff->n_chan = buf->n_chan;
    transbuff->planestride = buf->planestride;
    transbuff->rowstride = buf->rowstride;
    transbuff->transbytes = buf->data;
    transbuff->has_shape = buf->has_shape;
    transbuff->width = width;
    transbuff->height = height;
    transbuff->rect = rect;
#if RAW_DUMP
    /* Dump the buffer that should be going into the pattern */;
        dump_raw_buffer(height, width, buf->n_chan,
                    pdev->ctx->stack->planestride, pdev->ctx->stack->rowstride, 
                    "pdf14_pattern_buff",buf->data);
        global_index++;
#endif
    return(0);
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
pdf14_put_image(gx_device * dev, gs_imager_state * pis, gx_device * target)
{
    const pdf14_device * pdev = (pdf14_device *)dev;
    int code;
    gs_image1_t image;
    gx_image_enum_common_t *info;
    pdf14_buf *buf = pdev->ctx->stack;
    gs_int_rect rect = buf->rect;
    int y;
    int num_comp = buf->n_chan - 1;
    byte *linebuf;
    gs_color_space *pcs;
    const byte bg = pdev->ctx->additive ? 255 : 0;
    int x1, y1, width, height;
    byte *buf_ptr;
    bool data_blended = false;
    int num_rows_left;

    if_debug0('v', "[v]pdf14_put_image\n");
    rect_intersect(rect, buf->bbox);
    x1 = min(pdev->width, rect.q.x);
    y1 = min(pdev->height, rect.q.y);
    width = x1 - rect.p.x;
    height = y1 - rect.p.y;
#ifdef DUMP_TO_PNG
    dump_planar_rgba(pdev->memory, buf);
#endif
    if (width <= 0 || height <= 0 || buf->data == NULL)
	return 0;
    buf_ptr = buf->data + rect.p.y * buf->rowstride + rect.p.x;
    /* See if the target device has a put_image command.  If
       yes then see if it can handle the image data directly.
       If it cannot, then we will need to use the begin_typed_image
       interface, which cannot pass along tag nor alpha data to 
       the target device */
    if (target->procs.put_image != NULL) {
        /* See if the target device can handle the data in its current 
           form with the alpha component */
        int alpha_offset = num_comp;
        int tag_offset = buf->has_tags ? num_comp+1 : 0;
        code = dev_proc(target, put_image) (target, buf_ptr, num_comp,
                                            rect.p.x, rect.p.y, width, height,
                                            buf->rowstride, buf->planestride,
                                            num_comp,tag_offset);
        if (code == 0) {
            /* Device could not handle the alpha data.  Go ahead and 
               preblend now. Note that if we do this, and we end up in the
               default below, we only need to repack in chunky not blend */
#if RAW_DUMP
            /* Dump before and after the blend to make sure we are doing that ok */
            dump_raw_buffer(height, width, buf->n_planes,
                        pdev->ctx->stack->planestride, pdev->ctx->stack->rowstride, 
                        "pre_final_blend",buf_ptr);
            global_index++;
#endif
            gx_blend_image_buffer(buf_ptr, width, height, buf->rowstride,
                                  buf->planestride, num_comp, bg);
#if RAW_DUMP
            /* Dump before and after the blend to make sure we are doing that ok */
            dump_raw_buffer(height, width, buf->n_planes,
                        pdev->ctx->stack->planestride, pdev->ctx->stack->rowstride, 
                        "post_final_blend",buf_ptr);
            global_index++;
            clist_band_count++;
#endif
            data_blended = true;
            /* Try again now */
            alpha_offset = 0;
            code = dev_proc(target, put_image) (target, buf_ptr, num_comp,
                                                rect.p.x, rect.p.y, width, height,
                                                buf->rowstride, buf->planestride,
                                                alpha_offset, tag_offset);
        }
        if (code > 0) {
            /* We processed some or all of the rows.  Continue until we are done */
            num_rows_left = height - code;
            while (num_rows_left > 0) {
                code = dev_proc(target, put_image) (target, buf_ptr, buf->n_planes,
                                                    rect.p.x, rect.p.y+code, width, 
                                                    num_rows_left, buf->rowstride, 
                                                    buf->planestride,
                                                    alpha_offset, tag_offset);
                num_rows_left = num_rows_left - code;
            }
            return 0;
        }
    }
#if 0
    /* Set graphics state device to target, so that image can set up
       the color mapping properly. */
    rc_increment(pdev);
    gs_setdevice_no_init(pgs, target);
#endif
    /*
     * Set color space to either Gray, RGB, or CMYK in preparation for sending
     * an image.
     */
    switch (num_comp) {
	case 1:				/* DeviceGray */
	    pcs = gs_cspace_new_DeviceGray(pis->memory);
	    break;
	case 3:				/* DeviceRGB */
	    pcs = gs_cspace_new_DeviceRGB(pis->memory);
	    break;
	case 4:				/* DeviceCMYK */
	    pcs = gs_cspace_new_DeviceCMYK(pis->memory);
	    break;
	default:			/* Should never occur */
	    return_error(gs_error_rangecheck);
	    break;
    }
    if (pcs == NULL)
	return_error(gs_error_VMerror);
    /* Need to set this to avoid color management during the 
       image color render operation */
    pcs->cmm_icc_profile_data = dev->device_icc_profile;
    rc_increment(dev->device_icc_profile);
    gs_image_t_init_adjust(&image, pcs, false);
    image.ImageMatrix.xx = (float)width;
    image.ImageMatrix.yy = (float)height;
    image.Width = width;
    image.Height = height;
    image.BitsPerComponent = 8;
    ctm_only_writable(pis).xx = (float)width;
    ctm_only_writable(pis).xy = 0;
    ctm_only_writable(pis).yx = 0;
    ctm_only_writable(pis).yy = (float)height;
    ctm_only_writable(pis).tx = (float)rect.p.x;
    ctm_only_writable(pis).ty = (float)rect.p.y;
    code = dev_proc(target, begin_typed_image) (target,
						pis, NULL,
						(gs_image_common_t *)&image,
						NULL, NULL, NULL,
						pis->memory, &info);
    if (code < 0) {
	rc_decrement_only_cs(pcs, "pdf14_put_image");
	return code;
    }
#if RAW_DUMP
    /* Dump the current buffer to see what we have. */
    dump_raw_buffer(pdev->ctx->stack->rect.q.y-pdev->ctx->stack->rect.p.y, 
                pdev->ctx->stack->rect.q.x-pdev->ctx->stack->rect.p.x, 
				pdev->ctx->stack->n_planes,
                pdev->ctx->stack->planestride, pdev->ctx->stack->rowstride, 
                "pdF14_putimage",pdev->ctx->stack->data);
    dump_raw_buffer(height, width, num_comp+1,
                pdev->ctx->stack->planestride, pdev->ctx->stack->rowstride, 
                "PDF14_PUTIMAGE_SMALL",buf_ptr);
    global_index++;
    if (!data_blended) {
        clist_band_count++;
    }
#endif
    linebuf = gs_alloc_bytes(pdev->memory, width * num_comp, "pdf14_put_image");
    for (y = 0; y < height; y++) {
	gx_image_plane_t planes;
	int rows_used,k,x;

        if (data_blended) {
            for (x = 0; x < width; x++) {
	        for (k = 0; k < num_comp; k++) {
		    linebuf[x * num_comp + k] = buf_ptr[x + buf->planestride * k];
	        }
            }
        } else {
	    gx_build_blended_image_row(buf_ptr, y, buf->planestride, width, 
                                       num_comp, bg, linebuf);
        }
	planes.data = linebuf;
	planes.data_x = 0;
	planes.raster = width * num_comp;
	info->procs->plane_data(info, &planes, 1, &rows_used);
	/* todo: check return value */
	buf_ptr += buf->rowstride;
    }
    gs_free_object(pdev->memory, linebuf, "pdf14_put_image");
    info->procs->end_image(info, true);
#if 0
    /* Restore device in graphics state.*/
    gs_setdevice_no_init(pgs, (gx_device*) pdev);
    rc_decrement_only(pdev, "pdf_14_put_image");
#endif
    /* This will also decrement the device profile */
    rc_decrement_only_cs(pcs, "pdf14_put_image");
    return code;
}

/**
 * pdf14_cmykspot_put_image: Put rendered image to target device.
 * @pdev: The PDF 1.4 rendering device.
 * @pis: State for image draw operation.
 * @target: The target device.
 *
 * Puts the rendered image in @pdev's buffer to @target. This is called
 * as part of the sequence of popping the PDF 1.4 device filter.
 *
 * Return code: negative on error.
 **/
static	int
pdf14_cmykspot_put_image(gx_device * dev, gs_imager_state * pis, gx_device * target)
{
    pdf14_device * pdev = (pdf14_device *)dev;
    pdf14_buf *buf = pdev->ctx->stack;
    gs_int_rect rect = buf->rect;
    int x1, y1, width, height;
    gs_devn_params * pdevn_params = &pdev->devn_params;
    gs_separations * pseparations = &pdevn_params->separations;
    int planestride = buf->planestride;
    int rowstride = buf->rowstride;
    const byte bg = pdev->ctx->additive ? gx_max_color_value : 0;
    int num_comp = buf->n_chan - 1;
    byte *buf_ptr;

    if_debug0('v', "[v]pdf14_cmykspot_put_image\n");
    rect_intersect(rect, buf->bbox);
    x1 = min(pdev->width, rect.q.x);
    y1 = min(pdev->height, rect.q.y);
    width = x1 - rect.p.x;
    height = y1 - rect.p.y;
    if (width <= 0 || height <= 0 || buf->data == NULL)
	return 0;
    buf_ptr = buf->data + rect.p.y * buf->rowstride + rect.p.x;
#if RAW_DUMP
    /* Dump the current buffer to see what we have. */
    dump_raw_buffer(pdev->ctx->stack->rect.q.y-pdev->ctx->stack->rect.p.y, 
                pdev->ctx->stack->rect.q.x-pdev->ctx->stack->rect.p.x, 
				pdev->ctx->stack->n_planes,
                pdev->ctx->stack->planestride, pdev->ctx->stack->rowstride, 
                "CMYK_SPOT_PUTIMAGE",pdev->ctx->stack->data);

    global_index++;
    clist_band_count++;
#endif
    return gx_put_blended_image_cmykspot(target, buf_ptr, planestride, rowstride,
		      rect.p.x, rect.p.y, width, height, num_comp, bg, pseparations);
}

/**
 * pdf14_custom_put_image: Put rendered image to target device.
 * @pdev: The PDF 1.4 rendering device.
 * @pis: State for image draw operation.
 * @target: The target device.
 *
 * Puts the rendered image in @pdev's buffer to @target. This is called
 * as part of the sequence of popping the PDF 1.4 device filter.
 *
 * Return code: negative on error.
 **/
static	int
pdf14_custom_put_image(gx_device * dev, gs_imager_state * pis, gx_device * target)
{
    pdf14_device * pdev = (pdf14_device *)dev;
    pdf14_buf *buf = pdev->ctx->stack;
    gs_int_rect rect = buf->rect;
    int x0 = rect.p.x, y0 = rect.p.y;
    int planestride = buf->planestride;
    int rowstride = buf->rowstride;
    int num_comp = buf->n_chan - 1;
    const byte bg = pdev->ctx->additive ? gx_max_color_value : 0;
    int x1, y1, width, height;
    byte *buf_ptr;

    if_debug0('v', "[v]pdf14_custom_put_image\n");
    rect_intersect(rect, buf->bbox);
    x1 = min(pdev->width, rect.q.x);
    y1 = min(pdev->height, rect.q.y);
    width = x1 - rect.p.x;
    height = y1 - rect.p.y;
    if (width <= 0 || height <= 0 || buf->data == NULL)
	return 0;
    buf_ptr = buf->data + rect.p.y * buf->rowstride + rect.p.x;

    return gx_put_blended_image_custom(target, buf_ptr, 
		      planestride, rowstride,
		      x0, y0, width, height, num_comp, bg);
}

static	int
pdf14_close(gx_device *dev)
{
    pdf14_device *pdev = (pdf14_device *)dev;

    if (pdev->ctx) {
	pdf14_ctx_free(pdev->ctx);
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
	COPY_PARAM(width);
	COPY_PARAM(height);
	COPY_ARRAY_PARAM(MediaSize);
	COPY_ARRAY_PARAM(ImagingBBox);
	COPY_PARAM(ImagingBBox_set);
	COPY_ARRAY_PARAM(HWResolution);
	COPY_ARRAY_PARAM(MarginsHWResolution);
	COPY_ARRAY_PARAM(Margins);
	COPY_ARRAY_PARAM(HWMargins);
	COPY_PARAM(PageCount);
	COPY_PARAM(MaxPatternBitmap);
        /* Don't let put params blow away a group color space */
        if (dev->device_icc_profile == NULL) {
            COPY_PARAM(device_icc_profile);
            rc_increment(dev->device_icc_profile);
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
int put_param_compressed_color_list_elem(gx_device * pdev,
    gs_param_list * plist, compressed_color_list_t ** pret_comp_list,
    char * keyname, int num_comps);
int put_param_pdf14_spot_names(gx_device * pdev,
		gs_separations * pseparations, gs_param_list * plist);
#define PDF14CompressedColorListParamName "PDF14CompressedColorList"
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
#if 0
    put_param_compressed_color_list_elem(pdev, plist, &pret_comp_list,
		  PDF14CompressedColorListParamName, TOP_ENCODED_LEVEL);
    put_param_pdf14_spot_names(dev, &pdev->devn_params, plist);
#endif
    return code;
}

/*
 * Copy marking related parameters into the PDF 1.4 device structure for use
 * by pdf14_fill_rrectangle.
 */
static	void
pdf14_set_marking_params(gx_device *dev, const gs_imager_state *pis)
{
    pdf14_device * pdev = (pdf14_device *)dev;

    pdev->opacity = pis->opacity.alpha;
    pdev->shape = pis->shape.alpha;
    pdev->alpha = pis->opacity.alpha * pis->shape.alpha;
    pdev->blend_mode = pis->blend_mode;
    pdev->overprint = pis->overprint;
    pdev->overprint_mode = pis->overprint_mode;

    if_debug3('v', "[v]set_marking_params, opacity = %g, shape = %g, bm = %d\n",
	      pdev->opacity, pdev->shape, pis->blend_mode);
}

static  void
update_lop_for_pdf14(gs_imager_state *pis)
{
    if ((pis->alpha != 0xFFFF) ||
        (pis->blend_mode != BLEND_MODE_Normal) ||
        (pis->opacity.alpha != 1.0) ||
        (pis->shape.alpha != 1.0))
    {
        /*
         * The blend operations are not idempotent.  Force non-idempotent
         * filling and stroking operations.
         */
        pis->log_op |= lop_pdf14;
    }
}

static	int
pdf14_fill_path(gx_device *dev,	const gs_imager_state *pis,
			   gx_path *ppath, const gx_fill_params *params,
			   const gx_drawing_color *pdcolor,
			   const gx_clip_path *pcpath)
{
    gs_imager_state new_is = *pis;
    int code;
    gs_pattern2_instance_t *pinst = NULL;

    if (pdcolor != NULL && gx_dc_is_pattern1_color(pdcolor)){
        if( gx_pattern1_get_transptr(pdcolor) != NULL){
            /* In this case, we need to push a transparency group 
               and tile the pattern color, which is stored in 
               a pdf14 device buffer in the ctile object memember
               variable ttrans */
#if RAW_DUMP
            /* Since we do not get a put_image to view what
               we have do it now */
            pdf14_device * ppatdev14 = pdcolor->colors.pattern.p_tile->ttrans->pdev14;
            if (ppatdev14 != NULL) {  /* can occur during clist reading */
                dump_raw_buffer(ppatdev14->ctx->stack->rect.q.y-ppatdev14->ctx->stack->rect.p.y, 
                            ppatdev14->ctx->stack->rect.q.x-ppatdev14->ctx->stack->rect.p.x, 
			    ppatdev14->ctx->stack->n_planes, ppatdev14->ctx->stack->planestride, 
                            ppatdev14->ctx->stack->rowstride, 
                            "Pattern_Fill",ppatdev14->ctx->stack->data);
                global_index++;
            } else {
                 gx_pattern_trans_t *patt_trans = pdcolor->colors.pattern.p_tile->ttrans;
                 dump_raw_buffer(patt_trans->rect.q.y-patt_trans->rect.p.y, 
                            patt_trans->rect.q.x-patt_trans->rect.p.x, 
				            patt_trans->n_chan,
                            patt_trans->planestride, patt_trans->rowstride, 
                            "Pattern_Fill_clist",patt_trans->transbytes);
                global_index++;
            }
#endif
            code = pdf14_tile_pattern_fill(dev, &new_is, ppath, 
                params, pdcolor, pcpath);
            new_is.trans_device = NULL;
            new_is.has_transparency = false;
            return code;
        }
    }
   if (pdcolor != NULL && gx_dc_is_pattern2_color(pdcolor)) {
 	pinst =
	    (gs_pattern2_instance_t *)pdcolor->ccolor.pattern;
           pinst->saved->has_transparency = true;
           /* The transparency color space operations are driven
              by the pdf14 clist writer device.  */
           pinst->saved->trans_device = dev;
    }
    update_lop_for_pdf14(&new_is);
    pdf14_set_marking_params(dev, pis);
    new_is.trans_device = dev;
    new_is.has_transparency = true;
    code = gx_default_fill_path(dev, &new_is, ppath, params, pdcolor, pcpath);
    new_is.trans_device = NULL;
    new_is.has_transparency = false;
    if (pinst != NULL){
        pinst->saved->trans_device = NULL;
    }
    return code;
}

static	int
pdf14_stroke_path(gx_device *dev, const	gs_imager_state	*pis,
			     gx_path *ppath, const gx_stroke_params *params,
			     const gx_drawing_color *pdcolor,
			     const gx_clip_path *pcpath)
{
    gs_imager_state new_is = *pis;

    update_lop_for_pdf14(&new_is);
    pdf14_set_marking_params(dev, pis);
    return gx_default_stroke_path(dev, &new_is, ppath, params, pdcolor,
				  pcpath);
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

    /* If we are doing a fill with a pattern that has a transparency then
       go ahead and do a push and a pop of the transparency group */
    if (pdcolor != NULL && gx_dc_is_pattern1_color(pdcolor)) {
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
                code = pdf14_push_transparency_group(p14dev->ctx, &group_rect,
	             1, 0, 255,255, ptile->ttrans->blending_mode, 0, 0, ptile->ttrans->n_chan-1);
                /* Fix the reversed bbox. Not clear on why the push group does that */
                p14dev->ctx->stack->bbox.p.x = p14dev->ctx->rect.p.x;
                p14dev->ctx->stack->bbox.p.y = p14dev->ctx->rect.p.y;
                p14dev->ctx->stack->bbox.q.x = p14dev->ctx->rect.q.x;
                p14dev->ctx->stack->bbox.q.y = p14dev->ctx->rect.q.y;
                /* Set up the output buffer information now that we have
                   pushed the group */
                fill_trans_buffer = new_pattern_trans_buff(p14dev->memory);
                pdf14_get_buffer_information((gx_device *) p14dev, fill_trans_buffer);
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
            code = pdf14_pop_transparency_group(NULL, p14dev->ctx, p14dev->blend_procs, 
                                p14dev->color_info.num_components, 
                                p14dev->device_icc_profile, orig_dev);
            gs_free_object(p14dev->memory, ptile->ttrans->fill_trans_buffer, 
                           "pdf14_fill_mask");
            ptile->ttrans->fill_trans_buffer = NULL;  /* Avoid GC issues */
    }
    return code;
}

/* Used for filling rects when we are doing a fill with a pattern that
   has transparency */
static	int
pdf14_tile_pattern_fill(gx_device * pdev, const gs_imager_state * pis,
		     gx_path * ppath, const gx_fill_params * params,
		 const gx_device_color * pdevc, const gx_clip_path * pcpath)
{
    int code = 0;
    gs_imager_state *pis_noconst = (gs_imager_state *)pis; /* Break const. */
    gs_fixed_rect clip_box;
    gs_fixed_rect outer_box;
    pdf14_device * p14dev = (pdf14_device *)pdev;
    gs_int_rect rect;
    gx_clip_rect *curr_clip_rect;
    gx_color_tile *ptile;
    int k;
    gx_pattern_trans_t *fill_trans_buffer;
    int ok;
    gs_int_point phase;  /* Needed during clist rendering for band offset */

    gx_clip_path cpath_intersection;
    if (pcpath != NULL) {
        code = gx_cpath_init_local_shared(&cpath_intersection, pcpath, pdev->memory);
        if (code < 0)
	    return code;
    } else {
        (*dev_proc(pdev, get_clipping_box)) (pdev, &clip_box);
        gx_cpath_init_local(&cpath_intersection, ppath->memory);
        code = gx_cpath_from_rectangle(&cpath_intersection, &clip_box);
    }
    if (ppath != NULL)
        code = gx_cpath_intersect_with_params(&cpath_intersection, ppath, params->rule, 
	        pis_noconst, params);

  /* Now let us push a transparency group into which we are
     going to tile the pattern.  */
    if (ppath != NULL && code >= 0) {
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
        ptile = pdevc->colors.pattern.p_tile;
        code = pdf14_push_transparency_group(p14dev->ctx, &rect,
					 1, 0, 255,255,
					 pis->blend_mode, 0,
					 0, ptile->ttrans->n_chan-1);

        /* Set the blending procs and the is_additive setting based 
           upon the number of channels */
        if (ptile->ttrans->n_chan-1 < 4){
            ptile->ttrans->blending_procs = &rgb_blending_procs;
            ptile->ttrans->is_additive = true;
        } else {
            ptile->ttrans->blending_procs = &cmyk_blending_procs;
            ptile->ttrans->is_additive = false;
        }
        /* Fix the reversed bbox. Not clear on why the push group does that */
        p14dev->ctx->stack->bbox.p.x = p14dev->ctx->rect.p.x;
        p14dev->ctx->stack->bbox.p.y = p14dev->ctx->rect.p.y;
        p14dev->ctx->stack->bbox.q.x = p14dev->ctx->rect.q.x;
        p14dev->ctx->stack->bbox.q.y = p14dev->ctx->rect.q.y;
        /* Now lets go through the rect list and fill with the pattern */
        /* First get the buffer that we will be filling */
        fill_trans_buffer = new_pattern_trans_buff(pis->memory);
        pdf14_get_buffer_information(pdev, fill_trans_buffer);
        /* Set the blending mode in the ptile based upon the current 
           setting in the imager state */
        ptile->ttrans->blending_mode = pis->blend_mode;
        /* fill the rectangles */
        phase.x = pdevc->phase.x;
        phase.y = pdevc->phase.y;
        /* Based upon if the tiles overlap pick the type of rect fill that we will
           want to use */
        if (ptile->has_overlap) {
            /* This one does blending since there is tile overlap */
            ptile->ttrans->pat_trans_fill = &tile_rect_trans_blend;
        } else {
            /* This one does no blending since there is no tile overlap */
            ptile->ttrans->pat_trans_fill = &tile_rect_trans_simple;
        }
        if (cpath_intersection.rect_list->list.head != NULL){
            curr_clip_rect = cpath_intersection.rect_list->list.head->next;
            for( k = 0; k< cpath_intersection.rect_list->list.count; k++){  
	        if_debug5('v', "[v]pdf14_tile_pattern_fill, (%d, %d), %d x %d pat_id %d \n", 
		            curr_clip_rect->xmin, curr_clip_rect->ymin, 
                            curr_clip_rect->xmax-curr_clip_rect->xmin, 
                            curr_clip_rect->ymax-curr_clip_rect->ymin, ptile->id);
                ok = gx_trans_pattern_fill_rect(curr_clip_rect->xmin, curr_clip_rect->ymin, 
                    curr_clip_rect->xmax, curr_clip_rect->ymax, ptile, fill_trans_buffer, phase);
                curr_clip_rect = curr_clip_rect->next;
            }
        } else if (cpath_intersection.rect_list->list.count == 1) {
            /* The case when there is just a single rect */
            if_debug5('v', "[v]pdf14_tile_pattern_fill, (%d, %d), %d x %d pat_id %d \n", 
                cpath_intersection.rect_list->list.single.xmin, cpath_intersection.rect_list->list.single.ymin, 
                        cpath_intersection.rect_list->list.single.xmax-cpath_intersection.rect_list->list.single.xmin, 
                        cpath_intersection.rect_list->list.single.ymax-cpath_intersection.rect_list->list.single.ymin, 
                        ptile->id);
            ok = gx_trans_pattern_fill_rect(cpath_intersection.rect_list->list.single.xmin, 
                                            cpath_intersection.rect_list->list.single.ymin, 
                                            cpath_intersection.rect_list->list.single.xmax, 
                                            cpath_intersection.rect_list->list.single.ymax, 
                                            ptile, fill_trans_buffer, phase);
        }
        /* free our buffer object */
        gs_free_object(pis->memory, fill_trans_buffer, "pdf14_tile_pattern_fill");
        ptile->ttrans->fill_trans_buffer = NULL;  /* Avoid GC issues */
        /* pop our transparency group which will force the blending */
        code = pdf14_pop_transparency_group(pis, p14dev->ctx, p14dev->blend_procs, 
                            p14dev->color_info.num_components, 
                            p14dev->device_icc_profile, pdev);
    }
    return(code);
}

/* Imager render for pattern transparency filling.  This is just here to catch
   the final flush, at which time we will pop the group and reset a few items */
static	int
pdf14_pattern_trans_render(gx_image_enum * penum, const byte * buffer, int data_x,
		    uint w, int h, gx_device * dev)
{
    int code;
    pdf14_device * p14dev = (pdf14_device *)dev;
    const gs_imager_state * pis = penum->pis;
    gx_device_color * pdcolor = (penum->icolor1);
    gx_color_tile *ptile = pdcolor->colors.pattern.p_tile;

    /* Pass along to the original renderer */
    code = (ptile->ttrans->image_render)(penum, buffer, data_x, w, h, dev);
    /* On our final time through here, go ahead and pop the transparency 
       group and reset the procs in the device color. And free the fill
       trans buffer object */
    if ( h == 0 && ptile->trans_group_popped == false) {
        if (pis->is_gstate) {
            /* Used if we are on clist writing phase.  Would only
               occur if we somehow failed in high level clist 
               image writing */
            code = gs_end_transparency_group((gs_state *) pis);
        } else {
            /* Used if we are on clist reading phase.  If we had high level
               image in clist */
            code = pdf14_pop_transparency_group(NULL, p14dev->ctx, p14dev->blend_procs, 
                    p14dev->color_info.num_components, 
                    p14dev->device_icc_profile, (gx_device *) p14dev);
        }
        pdcolor->colors.pattern.p_tile->trans_group_popped = true;
        gs_free_object(pis->memory, ptile->ttrans->fill_trans_buffer, 
                       "pdf14_pattern_trans_render");
        ptile->ttrans->fill_trans_buffer = NULL;  /* Avoid GC issues */
    }
    return(code);
}

/* This function is used to get things in place for filling a mask image
   with a pattern that has transparency.  It is used by pdf14_begin_type_image
   and pdf14_clist_begin_type_image */
static int
pdf14_patt_trans_image_fill(gx_device * dev, const gs_imager_state * pis,
			   const gs_matrix *pmat, const gs_image_common_t *pic,
			   const gs_int_rect * prect,
			   const gx_drawing_color * pdcolor,
			   const gx_clip_path * pcpath, gs_memory_t * mem,
			   gx_image_enum_common_t ** pinfo)
{
    const gs_image_t *pim = (const gs_image_t *)pic;
    pdf14_device * p14dev = (pdf14_device *)dev;
    gx_color_tile *ptile;
    int code = 0;
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
       setting in the imager state */
    ptile->ttrans->blending_mode = pis->blend_mode;
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
    code = gx_default_begin_typed_image(dev, pis, pmat, pic, 
                            prect, pdcolor,pcpath, mem, pinfo);
    /* Now Push the group */
    /* First apply the inverse of the image matrix to our 
       image size to get our bounding box. */
    bbox_in.p.x = 0;
    bbox_in.p.y = 0;
    bbox_in.q.x = pim->Width;
    bbox_in.q.y = pim->Height;
    code = gs_bbox_transform_inverse(&bbox_in, &(pim->ImageMatrix), 
                                &bbox_out);
    if (code < 0) return code;
    /* That in turn will get hit by the matrix in the imager state */
    code = compute_group_device_int_rect(p14dev, &group_rect, 
                                            &bbox_out, pis);
    if (!(pim->Width == 0 || pim->Height == 0)) {
        code = pdf14_push_transparency_group(p14dev->ctx, &group_rect,
	     1, 0, 255,255,
	     pis->blend_mode, 0,
	     0, ptile->ttrans->n_chan-1);
        /* Fix the reversed bbox. Not clear on why the push group does that */
        p14dev->ctx->stack->bbox.p.x = p14dev->ctx->rect.p.x;
        p14dev->ctx->stack->bbox.p.y = p14dev->ctx->rect.p.y;
        p14dev->ctx->stack->bbox.q.x = p14dev->ctx->rect.q.x;
        p14dev->ctx->stack->bbox.q.y = p14dev->ctx->rect.q.y;
        /* Set up the output buffer information now that we have
           pushed the group */
        fill_trans_buffer = new_pattern_trans_buff(pis->memory);
        pdf14_get_buffer_information(dev, fill_trans_buffer);
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
pdf14_begin_typed_image(gx_device * dev, const gs_imager_state * pis,
			   const gs_matrix *pmat, const gs_image_common_t *pic,
			   const gs_int_rect * prect,
			   const gx_drawing_color * pdcolor,
			   const gx_clip_path * pcpath, gs_memory_t * mem,
			   gx_image_enum_common_t ** pinfo)
{
    const gs_image_t *pim = (const gs_image_t *)pic;
    int code;

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
                if (dev->procs.begin_image != gx_default_begin_image) {
                    code = pdf14_patt_trans_image_fill(dev, pis, pmat, pic,
                                                prect, pdcolor, pcpath, mem,
			                        pinfo);
                    return code;
                }
            }
        }
    }
    pdf14_set_marking_params(dev, pis);
    return gx_default_begin_typed_image(dev, pis, pmat, pic, prect, pdcolor,
					pcpath, mem, pinfo);
}

static	void
pdf14_set_params(gs_imager_state * pis,	gx_device * dev,
				const gs_pdf14trans_params_t * pparams)
{
    if_debug0('v', "[v]pdf14_set_params\n");
    if (pparams->changed & PDF14_SET_BLEND_MODE)
	pis->blend_mode = pparams->blend_mode;
    if (pparams->changed & PDF14_SET_TEXT_KNOCKOUT)
	pis->text_knockout = pparams->text_knockout;
    if (pparams->changed & PDF14_SET_SHAPE_ALPHA)
	pis->shape.alpha = pparams->shape.alpha;
    if (pparams->changed & PDF14_SET_OPACITY_ALPHA)
	pis->opacity.alpha = pparams->opacity.alpha;
    if (pparams->changed & PDF14_SET_OVERPRINT)
	pis->overprint = pparams->overprint;
    if (pparams->changed & PDF14_SET_OVERPRINT_MODE)
	pis->overprint_mode = pparams->overprint_mode;
    pdf14_set_marking_params(dev, pis);
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
    int code = 0;

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
    gx_device_forward * pdev = (gx_device_forward *)dev;

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
    set_dev_proc(dev, tile_rectangle, gx_forward_tile_rectangle);
    set_dev_proc(dev, copy_mono, gx_forward_copy_mono);
    set_dev_proc(dev, copy_color, gx_forward_copy_color);
    set_dev_proc(dev, get_page_device, gx_forward_get_page_device);
    set_dev_proc(dev, strip_tile_rectangle, gx_forward_strip_tile_rectangle);
    set_dev_proc(dev, copy_alpha, gx_forward_copy_alpha);
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

    if_debug0('v', "[v]pdf14_disable_device\n");
    dev->color_info = pdev->target->color_info;
    pdf14_forward_device_procs(dev);
    set_dev_proc(dev, create_compositor, pdf14_forward_create_compositor);
    return 0;
}

/*
 * The default color space for PDF 1.4 blend modes is based upon the process
 * color model of the output device.
 */
static	pdf14_default_colorspace_t
pdf14_determine_default_blend_cs(gx_device * pdev)
{
    /* If num components is one, just go ahead and use gray.  This avoids
       issues with additive/subtractive mono color devices  */
    if (pdev->color_info.polarity == GX_CINFO_POLARITY_ADDITIVE || 
        pdev->color_info.num_components == 1)
	/*
	 * Note:  We do not allow the SeparationOrder device parameter for
	 * additive devices.  Thus we always have 1 colorant for DeviceGray
	 * and 3 colorants for DeviceRGB. 
	 */
         if (pdev->color_info.num_components == 1)
             return PDF14_DeviceGray;
         else
            return PDF14_DeviceRGB;
    else {
	/*
	 * Check if the device is CMYK only or CMYK plus spot colors.
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
		(pdev, pcomp_name, strlen(pcomp_name), NO_COMP_NAME_TYPE);
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
	    && pdev->color_info.max_components == 4)
	    return PDF14_DeviceCMYK;
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
get_pdf14_device_proto(gx_device * dev, pdf14_device ** pdevproto,
	pdf14_device * ptempdevproto, gs_imager_state * pis,
	const gs_pdf14trans_t * pdf14pct)
{
    pdf14_default_colorspace_t dev_cs =
		pdf14_determine_default_blend_cs(dev);

    switch (dev_cs) {
	case PDF14_DeviceGray:
	    *pdevproto = (pdf14_device *)&gs_pdf14_Gray_device;

            /* We want gray to be single channel.  Low level 
               initialization of gray device prototype is 
               peculiar in that in dci_std_color_num_components
               the comment is
              "A device is monochrome only if it is bi-level"

              Here we want monochrome anytime we have a gray device.
              To avoid breaking things elsewhere, we will overide
              the prototype intialization here */
               
            *ptempdevproto = **pdevproto;
            ptempdevproto->color_info.max_components = 1;
            ptempdevproto->color_info.num_components = ptempdevproto->color_info.max_components;
            if (dev->color_info.icc_profile == '\0') {
                strcpy(&(ptempdevproto->color_info.icc_profile[0]), DEFAULT_GRAY_ICC);
            } else {
                strcpy(&(ptempdevproto->color_info.icc_profile[0]), dev->color_info.icc_profile);
            }
            *pdevproto = ptempdevproto;
	    break;
	case PDF14_DeviceRGB:
	    *pdevproto = (pdf14_device *)&gs_pdf14_RGB_device;
            *ptempdevproto = **pdevproto;
            /* PDF14 Device should inherent the ICC color info of the parent.
               If the parent does not have one, (and it should) then use
               the default. */
            if (dev->color_info.icc_profile == '\0') {
                strcpy(&(ptempdevproto->color_info.icc_profile[0]), DEFAULT_RGB_ICC);
            } else {
                strcpy(&(ptempdevproto->color_info.icc_profile[0]), dev->color_info.icc_profile);
            }
            *pdevproto = ptempdevproto;
	    break;
	case PDF14_DeviceCMYK:
	    *pdevproto = (pdf14_device *)&gs_pdf14_CMYK_device;
            *ptempdevproto = **pdevproto;
            if (dev->color_info.icc_profile == '\0') {
                strcpy(&(ptempdevproto->color_info.icc_profile[0]), DEFAULT_CMYK_ICC);
            } else {
                strcpy(&(ptempdevproto->color_info.icc_profile[0]), dev->color_info.icc_profile);
            }
            *pdevproto = ptempdevproto;
	    break;
	case PDF14_DeviceCMYKspot:
	    *pdevproto = (pdf14_device *)&gs_pdf14_CMYKspot_device;
            /* Need to figure out how we want to handle the device profile
               for this case */
	    /*
	     * The number of components for the PDF14 device is the sum
	     * of the process components and the number of spot colors
	     * for the page.
	     */
	    if (pdf14pct->params.num_spot_colors >= 0) {
	        *ptempdevproto = **pdevproto;
	        ptempdevproto->devn_params.page_spot_colors =
	    	    pdf14pct->params.num_spot_colors;
	        ptempdevproto->color_info.num_components =
	    	    ptempdevproto->devn_params.num_std_colorant_names +
	    	    pdf14pct->params.num_spot_colors;
	        if (ptempdevproto->color_info.num_components >
	    		ptempdevproto->color_info.max_components)
	    	    ptempdevproto->color_info.num_components =
		        ptempdevproto->color_info.max_components;
		*pdevproto = ptempdevproto;
	    }
            if (dev->color_info.icc_profile == '\0') {
                strcpy(&(ptempdevproto->color_info.icc_profile[0]), DEFAULT_CMYK_ICC);
            } else {
                strcpy(&(ptempdevproto->color_info.icc_profile[0]), dev->color_info.icc_profile);
            }
	    break;
	case PDF14_DeviceCustom:
	    /*
	     * We are using the output device's process color model.  The
	     * color_info for the PDF 1.4 compositing device needs to match
	     * the output device.
	     */
	    *ptempdevproto = gs_pdf14_custom_device;
	    ptempdevproto->color_info = dev->color_info;
            /* The pdf14 device has to be 8 bit continuous tone. Force it */
            ptempdevproto->color_info.depth = 
                ptempdevproto->color_info.num_components * 8;
            ptempdevproto->color_info.max_gray = 255; 
            ptempdevproto->color_info.max_color = 255;
            ptempdevproto->color_info.dither_grays = 256;
            ptempdevproto->color_info.dither_colors = 256;
            
            *pdevproto = ptempdevproto;
            if (dev->color_info.icc_profile == '\0') {
                strcpy(&(ptempdevproto->color_info.icc_profile[0]), DEFAULT_CMYK_ICC);
            } else {
                strcpy(&(ptempdevproto->color_info.icc_profile[0]), dev->color_info.icc_profile);
            }
	    break;
	default:			/* Should not occur */
	    return_error(gs_error_rangecheck);
    }
    return 0;
}

/*
 * Recreate the PDF 1.4 compositor device.  Once created, the PDF 1.4
 * compositor device is never removed.  (We do not have a remove compositor
 * method.)  However it is no-op'ed when the PDF 1.4 device is popped.  This
 * routine will re-enable the compositor if the PDF 1.4 device is pushed
 * again.
 */
static	int
pdf14_recreate_device(gs_memory_t *mem,	gs_imager_state	* pis,
		gx_device * dev, const gs_pdf14trans_t * pdf14pct)
{
    pdf14_device * pdev = (pdf14_device *)dev;
    gx_device * target = pdev->target;
    pdf14_device * dev_proto;
    pdf14_device temp_dev_proto;
    int code;

    if_debug0('v', "[v]pdf14_recreate_device\n");

    /*
     * We will not use the entire prototype device but we will set the
     * color related info and the device procs to match the prototype.
     */
    code = get_pdf14_device_proto(target, &dev_proto,
		   		 &temp_dev_proto, pis, pdf14pct);
    if (code < 0)
	return code;
    pdev->color_info = dev_proto->color_info;
    pdev->procs = dev_proto->procs;
    dev->static_procs = dev_proto->static_procs;
    gx_device_set_procs(dev);
    gx_device_fill_in_procs(dev);
    check_device_separable(dev);
    return dev_proc(pdev, open_device)(dev);
}

/*
 * Implement the various operations that can be specified via the PDF 1.4
 * create compositor request.
 */
static	int
gx_update_pdf14_compositor(gx_device * pdev, gs_imager_state * pis,
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
	    p14dev->blend_mode = 0;
	    p14dev->opacity = p14dev->shape = 0.0;
	    pdf14_recreate_device(mem, pis, pdev, pdf14pct);
	    break;
	case PDF14_POP_DEVICE:
	    if_debug0('v', "[v]gx_update_pdf14_compositor(PDF14_POP_DEVICE)\n");
	    pis->get_cmap_procs = p14dev->save_get_cmap_procs;
	    gx_set_cmap_procs(pis, p14dev->target);
	    /* Send image out raster data to output device */
            {
		/* Make a copy so we can change the ROP */
		gs_imager_state new_is = *pis;

		/* We don't use the imager state log_op since this is for the */
		/* clist playback. Putting the image (band in the case of the */
		/* clist) only needs to use the default ROP to copy the data  */
		new_is.log_op = rop3_default;
		p14dev->pdf14_procs->put_image(pdev, &new_is, p14dev->target);
	    }
	    pdf14_disable_device(pdev);
	    pdf14_close(pdev);
	    break;
	case PDF14_BEGIN_TRANS_GROUP:
	    code = gx_begin_transparency_group(pis, pdev, &params);
	    break;
	case PDF14_END_TRANS_GROUP:
	    code = gx_end_transparency_group(pis, pdev);
	    break;
	case PDF14_BEGIN_TRANS_MASK:
	    code = gx_begin_transparency_mask(pis, pdev, &params);
	    break;
	case PDF14_END_TRANS_MASK:
	    code = gx_end_transparency_mask(pis, pdev, &params);
	    break;
	case PDF14_SET_BLEND_PARAMS:
	    pdf14_set_params(pis, pdev, &pdf14pct->params);
	    break;
        case PDF14_PUSH_TRANS_STATE:
            code = gx_push_transparency_state(pis, pdev);
            break;
        case PDF14_POP_TRANS_STATE:
            code = gx_pop_transparency_state(pis, pdev);
            break;
        case PDF14_PUSH_SMASK_COLOR:
            code = pdf14_increment_smask_color(pis, pdev);
            break;
        case PDF14_POP_SMASK_COLOR:
            code = pdf14_decrement_smask_color(pis, pdev);
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
 * to the targer.
 */
static	int
pdf14_forward_create_compositor(gx_device * dev, gx_device * * pcdev,
	const gs_composite_t * pct, gs_imager_state * pis,
	gs_memory_t * mem, gx_device *cdev)
{
    pdf14_device *pdev = (pdf14_device *)dev;
    gx_device * tdev = pdev->target;
    gx_device * ndev;
    int code = 0;

    *pcdev = dev;
    if (gs_is_pdf14trans_compositor(pct)) {
	const gs_pdf14trans_t * pdf14pct = (const gs_pdf14trans_t *) pct;

	if (pdf14pct->params.pdf14_op == PDF14_PUSH_DEVICE)
	    return gx_update_pdf14_compositor(dev, pis, pdf14pct, mem);
	return 0;
    }
    code = dev_proc(tdev, create_compositor)(tdev, &ndev, pct, pis, mem, cdev);
    if (code < 0)
	return code;
    pdev->target = ndev;
    return 0;
}

/*
 * The PDF 1.4 compositor can be handled directly, so just set *pcdev = dev
 * and return. Since the gs_pdf14_device only supports the high-level routines
 * of the interface, don't bother trying to handle any other compositor.
 */
static	int
pdf14_create_compositor(gx_device * dev, gx_device * * pcdev,
	const gs_composite_t * pct, gs_imager_state * pis,
	gs_memory_t * mem, gx_device *cdev)
{
	pdf14_device *p14dev = (pdf14_device *)dev;
	if (gs_is_pdf14trans_compositor(pct)) {
	const gs_pdf14trans_t * pdf14pct = (const gs_pdf14trans_t *) pct;
	*pcdev = dev;
        /* cdev, may be the clist reader device which may contain
           information that we will need related to the ICC color
           spaces that define transparency groups.  We want this
           propogated through all the pdf14 functions.  Store 
           a pointer to it in the pdf14 device */
        p14dev->pclist_device = cdev;
	return gx_update_pdf14_compositor(dev, pis, pdf14pct, mem);
    } else if (gs_is_overprint_compositor(pct)) {
		/* If we had an overprint compositer action, then the 
		   color components that were drawn should be updated.
		   The overprint compositor logic and its interactions
		   with the clist is a little odd as it passes uninitialized
		   values around a fair amount.  Hence the forced assignement here.
		   See gx_spot_colors_set_overprint in gscspace for issues... */
		const gs_overprint_t * op_pct = (const gs_overprint_t *) pct;
		if (op_pct->params.retain_any_comps && !op_pct->params.retain_spot_comps) {
                    p14dev->drawn_comps = op_pct->params.drawn_comps;
		} else {
                    /* Draw everything. If this parameter was not set, clist does not fill it in.  */
                    p14dev->drawn_comps = ( (gx_color_index) 1 << (p14dev->color_info.num_components)) - (gx_color_index) 1; 
		}
		*pcdev = dev;
		return 0;
    } else
	return gx_no_create_compositor(dev, pcdev, pct, pis, mem, cdev);
}

static	int
pdf14_text_begin(gx_device * dev, gs_imager_state * pis,
		 const gs_text_params_t * text, gs_font * font,
		 gx_path * path, const gx_device_color * pdcolor,
		 const gx_clip_path * pcpath, gs_memory_t * memory,
		 gs_text_enum_t ** ppenum)
{
    int code;
    gs_text_enum_t *penum;

    if_debug0('v', "[v]pdf14_text_begin\n");
    pdf14_set_marking_params(dev, pis);
    code = gx_default_text_begin(dev, pis, text, font, path, pdcolor, pcpath,
				 memory, &penum);
    if (code < 0)
	return code;
    *ppenum = (gs_text_enum_t *)penum;
    return code;
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

    /* Loop through the height of the specfied area. */
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

static	int
pdf14_fill_rectangle(gx_device * dev,
		    int x, int y, int w, int h, gx_color_index color)
{
    pdf14_device *pdev = (pdf14_device *)dev;
    pdf14_buf *buf = pdev->ctx->stack;

    fit_fill_xywh(dev, x, y, w, h);
    if (w <= 0 || h <= 0)
	return 0;
    if (buf->knockout)
	return pdf14_mark_fill_rectangle_ko_simple(dev, x, y, w, h, color);
    else
	return pdf14_mark_fill_rectangle(dev, x, y, w, h, color);
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
compute_group_device_int_rect(pdf14_device *pdev, gs_int_rect *rect, const gs_rect *pbbox, gs_imager_state *pis)
{
    int code = pdf14_compute_group_device_int_rect(&ctm_only(pis), pbbox, rect);

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
pdf14_begin_transparency_group(gx_device *dev,
			      const gs_transparency_group_params_t *ptgp,
			      const gs_rect *pbbox,
			      gs_imager_state *pis,
			      gs_transparency_state_t **ppts,
			      gs_memory_t *mem)
{
    pdf14_device *pdev = (pdf14_device *)dev;
    double alpha = pis->opacity.alpha * pis->shape.alpha;
    gs_int_rect rect;
    int code;
    bool isolated;
    bool sep_target = (strcmp(pdev->dname, "pdf14cmykspot") == 0);
    int group_color_numcomps;
    gs_transparency_color_t group_color;
    cmm_profile_t *curr_profile;

    /* If the target device supports separations, then 
       we should should NOT create the group.  The exception to this 
       rule would be if we just popped a transparency mask */

    code = compute_group_device_int_rect(pdev, &rect, pbbox, pis);

    if (code < 0)
	return code;
    if_debug4('v', "[v]pdf14_begin_transparency_group, I = %d, K = %d, alpha = %g, bm = %d\n",
	      ptgp->Isolated, ptgp->Knockout, alpha, pis->blend_mode);
    /* If the group color is unknown then use the current device profile.  Note
       that if we have a sep device there may not be a profile */
    if (ptgp->group_color == UNKNOWN){
        if (pdev->ctx->stack){
            /* Use previous group color space */
            group_color_numcomps = pdev->ctx->stack->n_chan-1;  /* Remove alpha */
        } else {
            /* Use process color space */
            group_color_numcomps = pdev->color_info.num_components;
        }
       /* If we are not going out to a sep device then use
           the ICC color space defined by the device profile 
           in the ICC manager.  This is reset for each
           group push. */
        if (group_color_numcomps < 5 ) {
            group_color = ICC;
            curr_profile = dev->device_icc_profile;
        } else {
                /* We can end up here if we are in
                   a deviceN color space and 
                   we have a sep output device */
                group_color = DEVICEN;
            curr_profile = NULL;
         }  
    } else {
        group_color_numcomps = ptgp->group_color_numcomps;
        group_color = ptgp->group_color;
        curr_profile = ptgp->iccprofile;
    }
    /* If needed, update the color mapping procs. But only if we dont have a sep device.
        The exception would be if we are in doing the group for a soft mask */
    if (!sep_target) {
        if (curr_profile != NULL) {
            /* If the cs is different then force isolation */
            if ( curr_profile->hashcode == 
                dev->device_icc_profile->hashcode) {
                    /* cs same.  use parameter setting */
                    isolated = ptgp->Isolated;
            } else {
                /* blend cs different.  force isolation. other
                   option  would be to ignore the color space */
                isolated = true;
            }
        } else {
            if (group_color_numcomps == pdev->color_info.num_components) {
                 /* cs same.  use parameter setting */
                isolated = ptgp->Isolated;
            } else {
                /* blend cs different.  force isolation. other
                   option  would be to ignore the color space */
                isolated = true;
            }
        }
        code = pdf14_update_device_color_procs(dev, group_color, ptgp->icc_hashcode, pis, curr_profile);
        if_debug0('v', "[v]Transparency group color space update\n");
    } else {
        /* Sep devices always blend in the components */
        isolated = ptgp->Isolated;
        group_color_numcomps = pdev->color_info.num_components;
    }

    if (code < 0)
	return code;
    code = pdf14_push_transparency_group(pdev->ctx, &rect,
					 isolated, ptgp->Knockout,
					 (byte)floor (255 * alpha + 0.5),
					 (byte)floor (255 * pis->shape.alpha + 0.5),
					 pis->blend_mode, ptgp->idle,
					 ptgp->mask_id,group_color_numcomps);
    return code;
}

static	int
pdf14_end_transparency_group(gx_device *dev,
			      gs_imager_state *pis,
			      gs_transparency_state_t **ppts)
{
    pdf14_device *pdev = (pdf14_device *)dev;
    int code;
    pdf14_parent_color_t *parent_color;
    cmm_profile_t *icc_profile = dev->device_icc_profile;

    if_debug0('v', "[v]pdf14_end_transparency_group\n");
    vd_get_dc('c');
    vd_set_shift(0, 0);
    vd_set_scale(0.01);
    vd_set_origin(0, 0);
    vd_erase(RGB(192, 192, 192));
    code = pdf14_pop_transparency_group(pis, pdev->ctx, pdev->blend_procs,
                                pdev->color_info.num_components, icc_profile,
                                (gx_device *) pdev);
   /* May need to reset some color stuff related
     * to a mismatch between the parents color space
     * and the group blending space */
    parent_color = pdev->ctx->stack->parent_color_info_procs;
	if (!(parent_color->parent_color_mapping_procs == NULL && 
		parent_color->parent_color_comp_index == NULL)) {
	    pis->get_cmap_procs = parent_color->get_cmap_procs;
	    gx_set_cmap_procs(pis, dev);
	    pdev->procs.get_color_mapping_procs = parent_color->parent_color_mapping_procs;
	    pdev->procs.get_color_comp_index = parent_color->parent_color_comp_index;
	    pdev->color_info.polarity = parent_color->polarity;
	    pdev->color_info.num_components = parent_color->num_components;
	    pdev->blend_procs = parent_color->parent_blending_procs;
	    pdev->ctx->additive = parent_color->isadditive;
	    pdev->pdf14_procs = parent_color->unpack_procs;
            pdev->color_info.depth = parent_color->depth;
        pdev->color_info.max_color = parent_color->max_color;
        pdev->color_info.max_gray = parent_color->max_gray;
        memcpy(&(pdev->color_info.comp_bits),&(parent_color->comp_bits),
                            GX_DEVICE_COLOR_MAX_COMPONENTS);
        memcpy(&(pdev->color_info.comp_shift),&(parent_color->comp_shift),
                            GX_DEVICE_COLOR_MAX_COMPONENTS);
	    parent_color->get_cmap_procs = NULL;
	    parent_color->parent_color_comp_index = NULL;
	    parent_color->parent_color_mapping_procs = NULL;
        if (parent_color->icc_profile != NULL) {
            dev->device_icc_profile = parent_color->icc_profile;
            rc_decrement(parent_color->icc_profile,"pdf14_end_transparency_group");
            parent_color->icc_profile = NULL;
	}
    }
    vd_release_dc;
    return code;
}

static int
pdf14_update_device_color_procs(gx_device *dev,
			      gs_transparency_color_t group_color, int64_t icc_hashcode,
			      gs_imager_state *pis, cmm_profile_t *iccprofile)
{
    pdf14_device *pdevproto = NULL;
    pdf14_device *pdev = (pdf14_device *)dev;
    const pdf14_procs_t *new_14procs = NULL;
    pdf14_parent_color_t *parent_color_info;
    gx_color_polarity_t new_polarity;
    int new_num_comps;
    bool new_additive;
    byte new_depth;
    gx_device_clist_reader *pcrdev;
    byte comp_bits[] = {0,0,0,0};
    byte comp_shift[] = {0,0,0,0};
    int k;
    bool has_tags = (dev->memory->gs_lib_ctx->BITTAG != 
                        GS_DEVICE_DOESNT_SUPPORT_TAGS);

    if (pdev->ctx->stack != NULL){
        parent_color_info = pdev->ctx->stack->parent_color_info_procs;
    } else {
        /* This should not occur */
        return_error(gs_error_undefined);
    }
    if_debug0('v', "[v]pdf14_update_device_color_procs\n");

    /* Update the device procs at this
       stage.  Many of the procs are based upon the color
       space of the device.  We want to remain in 
       the color space defined by the color space of
       the soft mask or transparency group as opposed to the 
       device color space.  
       Later, when we pop the softmask we will collapse it
       to a single band and then compose with it
       to the device color space (or the parent layer
       space).  In the case where we pop an isolated transparency 
       group, we will do the blending in the proper color
       space and then transform the data when we pop the group. 
       Remember that only isolated groups can have color spaces
       that are different than their parent. */
	parent_color_info->get_cmap_procs = NULL;
        parent_color_info->parent_color_mapping_procs = NULL;
        parent_color_info->parent_color_comp_index = NULL;
        switch (group_color) {
            case GRAY_SCALE:
                new_polarity = GX_CINFO_POLARITY_ADDITIVE;
                new_num_comps = 1;
                pdevproto = (pdf14_device *)&gs_pdf14_Gray_device;
                new_additive = true;
                new_14procs = &gray_pdf14_procs;
                new_depth = 8;
                comp_bits[0] = 8;
                comp_shift[0] = 0; 
                break;
            case DEVICE_RGB:			 	
            case CIE_XYZ:				
                new_polarity = GX_CINFO_POLARITY_ADDITIVE;
                new_num_comps = 3;
                pdevproto = (pdf14_device *)&gs_pdf14_RGB_device;
                new_additive = true;
                new_14procs = &rgb_pdf14_procs;
                new_depth = 24;
                for (k = 0; k < 3; k++) {
                    comp_bits[k] = 8;
                    comp_shift[k] = (2-k)*8; 
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
                new_depth = 32;
                for (k = 0; k < 4; k++) {
                    comp_bits[k] = 8;
                    comp_shift[k] = (3-k)*8; 
                }
                break;
            case ICC:
                /* If we are coming from the clist reader, then we need to get the ICC data now  */
                if (iccprofile == NULL && pdev->pclist_device != NULL) {
                    /* Get the serialized data from the clist.  Not the whole profile. */
                    pcrdev = (gx_device_clist_reader *)(pdev->pclist_device);
                    iccprofile = gsicc_read_serial_icc((gx_device *) pcrdev, icc_hashcode);
                    if (iccprofile == NULL)
                        return gs_rethrow(-1,"ICC data not found in clist");
                    /* Keep a pointer to the clist device */
                    iccprofile->dev = (gx_device *) pcrdev;
                } 
                new_num_comps = iccprofile->num_comps;
                new_depth = new_num_comps * 8;
                if (new_num_comps == 4) {
                    new_additive = false;
                    new_polarity = GX_CINFO_POLARITY_SUBTRACTIVE;
                } else {
                    new_additive = true;
                    new_polarity = GX_CINFO_POLARITY_ADDITIVE;
                }
                switch (new_num_comps) {
                    case 1:
                        pdevproto = (pdf14_device *)&gs_pdf14_Gray_device;
                        new_14procs = &gray_pdf14_procs;
                        comp_bits[0] = 8;
                        comp_shift[0] = 0; 
                        break;
                    case 3:
                        pdevproto = (pdf14_device *)&gs_pdf14_RGB_device;
                        new_14procs = &rgb_pdf14_procs;
                        for (k = 0; k < 3; k++) {
                            comp_bits[k] = 8;
                            comp_shift[k] = (2-k)*8; 
                        }
                        break;
                   case 4:
                        pdevproto = (pdf14_device *)&gs_pdf14_CMYK_device;
                        new_14procs = &cmyk_pdf14_procs;
                        for (k = 0; k < 4; k++) {
                            comp_bits[k] = 8;
                            comp_shift[k] = (3-k)*8; 
                        }
                        break;
                }
                break;
            default:			
	        return_error(gs_error_rangecheck);
	        break;
         }    
        if_debug2('v', "[v]pdf14_update_device_color_procs,num_components_old = %d num_components_new = %d\n", 
            pdev->color_info.num_components,new_num_comps);

        /* Save the old information */
        parent_color_info->get_cmap_procs = pis->get_cmap_procs;
        parent_color_info->parent_color_mapping_procs = 
            pdev->procs.get_color_mapping_procs;
        parent_color_info->parent_color_comp_index = 
            pdev->procs.get_color_comp_index;
        parent_color_info->parent_blending_procs = pdev->blend_procs;
        parent_color_info->polarity = pdev->color_info.polarity;
        parent_color_info->num_components = pdev->color_info.num_components;
        parent_color_info->isadditive = pdev->ctx->additive;
        parent_color_info->unpack_procs = pdev->pdf14_procs;
        parent_color_info->depth = pdev->color_info.depth;
        memcpy(&(parent_color_info->comp_bits),&(pdev->color_info.comp_bits),
                            GX_DEVICE_COLOR_MAX_COMPONENTS);
        memcpy(&(parent_color_info->comp_shift),&(pdev->color_info.comp_shift),
                            GX_DEVICE_COLOR_MAX_COMPONENTS);
        parent_color_info->max_color = pdev->color_info.max_color;
        parent_color_info->max_gray = pdev->color_info.max_gray;

        /* Don't increment the space since we are going to remove it from the 
           ICC manager anyway.  */
        if (group_color == ICC && iccprofile != NULL) {
            parent_color_info->icc_profile = dev->device_icc_profile;
        }

        /* Set new information */
        pis->get_cmap_procs = pdf14_get_cmap_procs_group;
        gx_set_cmap_procs(pis, dev);
        pdev->procs.get_color_mapping_procs = 
            pdevproto->static_procs->get_color_mapping_procs;
        pdev->procs.get_color_comp_index = 
            pdevproto->static_procs->get_color_comp_index;
        pdev->blend_procs = pdevproto->blend_procs;
        pdev->color_info.polarity = new_polarity;
        pdev->color_info.num_components = new_num_comps;
        pdev->ctx->additive = new_additive; 
        pdev->pdf14_procs = new_14procs;
        if (has_tags) {
            new_depth += 8;
        }
        pdev->color_info.depth = new_depth;
        memset(&(pdev->color_info.comp_bits),0,GX_DEVICE_COLOR_MAX_COMPONENTS);
        memset(&(pdev->color_info.comp_shift),0,GX_DEVICE_COLOR_MAX_COMPONENTS);
        memcpy(&(pdev->color_info.comp_bits),comp_bits,4);
        memcpy(&(pdev->color_info.comp_shift),comp_shift,4);
        pdev->color_info.max_color = 255;
        pdev->color_info.max_gray = 255;
        /* If the CS was ICC based, we need to update the device ICC profile 
           in the ICC manager, since that is the profile that is used for the
           PDF14 device */
        if (group_color == ICC && iccprofile != NULL) {
            dev->device_icc_profile = iccprofile;
            rc_increment(parent_color_info->icc_profile);
        }
        return(1);  /* Lets us detect that we did do an update */
}

/* A new version that works with the color_procs stack 
   for transparency groups */
static int
pdf14_update_device_color_procs_push_c(gx_device *dev,
			      gs_transparency_color_t group_color, int64_t icc_hashcode,
			      gs_imager_state *pis, cmm_profile_t *icc_profile)
{
    pdf14_device *pdevproto;
    pdf14_device *pdev = (pdf14_device *)dev;
    gx_device_clist_writer * cldev = (gx_device_clist_writer *)pdev->pclist_device;
    const pdf14_procs_t *new_14procs;
    bool update_color_info;
    gx_color_polarity_t new_polarity;
    int new_num_comps;
    bool new_additive;
    byte new_depth;
    byte comp_bits[] = {0,0,0,0};
    byte comp_shift[] = {0,0,0,0};
    int k;
    bool has_tags = (dev->memory->gs_lib_ctx->BITTAG != 
                        GS_DEVICE_DOESNT_SUPPORT_TAGS);

    if (group_color == ICC && icc_profile == NULL) {
        return gs_rethrow(gs_error_undefinedresult, 
            "Missing ICC data");
    }
    if_debug0('v', "[v]pdf14_update_device_color_procs_push_c\n");
   /* Check if we need to alter the device procs at this
       stage.  Many of the procs are based upon the color
       space of the device.  We want to remain in 
       the color space defined by the color space of
       the soft mask or transparency group as opposed to the 
       device color space.  
       Later, when we pop the softmask we will collapse it
       to a single band and then compose with it
       to the device color space (or the parent layer
       space).  In the case where we pop an isolated transparency 
       group, we will do the blending in the proper color
       space and then transform the data when we pop the group. 
       Remember that only isolated groups can have color spaces
       that are different than their parent. */
        update_color_info = false;
        switch (group_color) {
            case GRAY_SCALE:
                  if (pdev->color_info.num_components != 1){ 
                    update_color_info = true;
                    new_polarity = GX_CINFO_POLARITY_ADDITIVE;
                    new_num_comps = 1;
                    pdevproto = (pdf14_device *)&gs_pdf14_Gray_device;
                    new_additive = true;
                    new_14procs = &gray_pdf14_procs;
                    new_depth = 8;
                    comp_bits[0] = 8;
                    comp_shift[0] = 0; 
                }
                break;
            case DEVICE_RGB:			 	
            case CIE_XYZ:				
                if (pdev->color_info.num_components != 3){ 
                    update_color_info = true;
                    new_polarity = GX_CINFO_POLARITY_ADDITIVE;
                    new_num_comps = 3;
                    pdevproto = (pdf14_device *)&gs_pdf14_RGB_device;
                    new_additive = true;
                    new_14procs = &rgb_pdf14_procs;
                    new_depth = 24;
                    for (k = 0; k < 3; k++) {
                        comp_bits[k] = 8;
                        comp_shift[k] = (2-k)*8; 
                    }
                }
                break; 
            case DEVICE_CMYK:				
                if (pdev->color_info.num_components != 4){ 
                    update_color_info = true;
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
                    new_depth = 32;
                    for (k = 0; k < 4; k++) {
                        comp_bits[k] = 8;
                        comp_shift[k] = (3-k)*8; 
                    }
                }
                break;
            case ICC:
                /* Check if the profile is different. */
                if (dev->device_icc_profile->hashcode != 
                                        icc_profile->hashcode) {
                    update_color_info = true;
                    new_num_comps = icc_profile->num_comps;
                    new_depth = icc_profile->num_comps*8;
                    switch (new_num_comps) {
                    case 1:
                        new_polarity = GX_CINFO_POLARITY_ADDITIVE;
                        new_additive = true;
                        pdevproto = (pdf14_device *)&gs_pdf14_Gray_device;
                        new_14procs = &gray_pdf14_procs;
                        comp_bits[0] = 8;
                        comp_shift[0] = 0; 
                        break;
                    case 3:
                        new_polarity = GX_CINFO_POLARITY_ADDITIVE;
                        new_additive = true;
                        pdevproto = (pdf14_device *)&gs_pdf14_RGB_device;
                        new_14procs = &rgb_pdf14_procs;
                        for (k = 0; k < 3; k++) {
                            comp_bits[k] = 8;
                            comp_shift[k] = (2-k)*8; 
                        }
                        break;
                    case 4:
                        new_polarity = GX_CINFO_POLARITY_SUBTRACTIVE;
                        new_additive = false;
                        pdevproto = (pdf14_device *)&gs_pdf14_CMYK_device;
                        new_14procs = &cmyk_pdf14_procs;
                        for (k = 0; k < 4; k++) {
                            comp_bits[k] = 8;
                            comp_shift[k] = (3-k)*8; 
                        }
                        break;
            default:			
                        return gs_rethrow(gs_error_undefinedresult, 
                            "ICC Number of colorants illegal");
                    }
                }
                break;
            default:			
	        return_error(gs_error_rangecheck);
	        break;
         }    
         if (update_color_info){
             if (has_tags) {
                 new_depth += 8;
             }
            if_debug2('v', "[v]pdf14_update_device_color_procs_push_c,num_components_old = %d num_components_new = %d\n", 
                pdev->color_info.num_components,new_num_comps);
            /* Set new information in the device */
            pis->get_cmap_procs = pdf14_get_cmap_procs_group;
            gx_set_cmap_procs(pis, dev);
            pdev->procs.get_color_mapping_procs = 
                pdevproto->static_procs->get_color_mapping_procs;
            pdev->procs.get_color_comp_index = 
                pdevproto->static_procs->get_color_comp_index;
            pdev->blend_procs = pdevproto->blend_procs;
            pdev->color_info.polarity = new_polarity;
            pdev->color_info.num_components = new_num_comps;
            pdev->color_info.max_color = 255;
            pdev->color_info.max_gray = 255;
            pdev->pdf14_procs = new_14procs;
            pdev->color_info.depth = new_depth;
            memset(&(pdev->color_info.comp_bits),0,GX_DEVICE_COLOR_MAX_COMPONENTS);
            memset(&(pdev->color_info.comp_shift),0,GX_DEVICE_COLOR_MAX_COMPONENTS);
            memcpy(&(pdev->color_info.comp_bits),comp_bits,4);
            memcpy(&(pdev->color_info.comp_shift),comp_shift,4);
            cldev->clist_color_info.depth = pdev->color_info.depth;
            cldev->clist_color_info.polarity = pdev->color_info.polarity;
            cldev->clist_color_info.num_components = pdev->color_info.num_components;
            cldev->clist_color_info.max_color = pdev->color_info.max_color;
            cldev->clist_color_info.max_gray = pdev->color_info.max_gray;
            /* For the ICC profiles, we want to update the ICC profile for the
               device in the ICC manager.  We already stored in in pdf14_parent_color_t.
               That will be stored in the clist and restored during the reading phase. */
           if (group_color == ICC) {
                dev->device_icc_profile = icc_profile;
            }
            if (pdev->ctx) {
               pdev->ctx->additive = new_additive; 
            }
            return(1);  /* Lets us detect that we did do an update */
         }
         if_debug0('v', "[v]procs not updated\n");
         return 0;
}

static int
pdf14_update_device_color_procs_pop_c(gx_device *dev,gs_imager_state *pis)
{

    pdf14_device *pdev = (pdf14_device *)dev;
    pdf14_parent_color_t *parent_color = pdev->trans_group_parent_cmap_procs;
    gx_device_clist_writer * cldev = (gx_device_clist_writer *)pdev->pclist_device;

    if_debug0('v', "[v]pdf14_update_device_color_procs_pop_c\n");
    /* The color procs are always pushed.  Simply restore them. */
    if (!(parent_color->parent_color_mapping_procs == NULL && 
        parent_color->parent_color_comp_index == NULL)) {
        if_debug2('v', "[v]pdf14_update_device_color_procs_pop_c,num_components_old = %d num_components_new = %d\n", 
            pdev->color_info.num_components,parent_color->num_components);
        pis->get_cmap_procs = parent_color->get_cmap_procs;
        gx_set_cmap_procs(pis, dev);
        pdev->procs.get_color_mapping_procs = parent_color->parent_color_mapping_procs;
        pdev->procs.get_color_comp_index = parent_color->parent_color_comp_index;
        pdev->color_info.polarity = parent_color->polarity;
        pdev->color_info.depth = parent_color->depth;
        pdev->color_info.num_components = parent_color->num_components;
        pdev->blend_procs = parent_color->parent_blending_procs;
        pdev->pdf14_procs = parent_color->unpack_procs;
        pdev->color_info.max_color = parent_color->max_color;
        pdev->color_info.max_gray = parent_color->max_gray;
        memcpy(&(pdev->color_info.comp_bits),&(parent_color->comp_bits),
                            GX_DEVICE_COLOR_MAX_COMPONENTS);
        memcpy(&(pdev->color_info.comp_shift),&(parent_color->comp_shift),
                            GX_DEVICE_COLOR_MAX_COMPONENTS);
        /* clist writer fill rect has no access to imager state */
        /* and it forwards the target device.  this information */
        /* is passed along to use in this case */
        cldev->clist_color_info.depth = pdev->color_info.depth;
        cldev->clist_color_info.polarity = pdev->color_info.polarity;
        cldev->clist_color_info.num_components = pdev->color_info.num_components;
        cldev->clist_color_info.max_color = pdev->color_info.max_color;
        cldev->clist_color_info.max_gray = pdev->color_info.max_gray;
        memcpy(&(cldev->clist_color_info.comp_bits),&(parent_color->comp_bits),
                            GX_DEVICE_COLOR_MAX_COMPONENTS);
        memcpy(&(cldev->clist_color_info.comp_shift),&(parent_color->comp_shift),
                            GX_DEVICE_COLOR_MAX_COMPONENTS);
        if (pdev->ctx){
            pdev->ctx->additive = parent_color->isadditive;
        }
       /* The device profile must be restored.  No reference - count is done here 
          A match with pdf14_update_device_color_procs_push_c.  There functions
          are closely integrated with pdf14_pop_parent_color and pdf14_push_parent color.
          All four are used only on the clist writer side of the transparency code */
         dev->device_icc_profile = parent_color->icc_profile;
         if_debug0('v', "[v]procs updated\n");
    } else {
        if_debug0('v', "[v]pdf14_update_device_color_procs_pop_c ERROR \n");
    }
    return 0;
}

   /* When a transparency group is pushed, the parent colorprocs
      are initialized.  Since the color mapping procs are
      all based upon the device, we must have a nested list 
      based upon the transparency group color space.  This 
      nesting must be outside the nested ctx structures 
      to allow the nesting for the clist writer */
static void
pdf14_push_parent_color(gx_device *dev, const gs_imager_state *pis)
{

    pdf14_device *pdev = (pdf14_device *)dev;
    pdf14_parent_color_t *new_parent_color;

    if_debug0('v', "[v]pdf14_push_parent_color\n");
    /* Allocate a new one */
    new_parent_color = gs_alloc_struct(dev->memory, pdf14_parent_color_t, 
                                        &st_pdf14_clr,"pdf14_clr_new");
    /* Link to old one */
    new_parent_color->previous = pdev->trans_group_parent_cmap_procs;
    /* Reassign new one to dev */
    pdev->trans_group_parent_cmap_procs = new_parent_color;
    /* Initialize with values */
    new_parent_color->get_cmap_procs = pis->get_cmap_procs;
    new_parent_color->parent_color_mapping_procs = 
        pdev->procs.get_color_mapping_procs;
    new_parent_color->parent_color_comp_index = 
        pdev->procs.get_color_comp_index;
    new_parent_color->parent_blending_procs = pdev->blend_procs;
    new_parent_color->polarity = pdev->color_info.polarity;
    new_parent_color->num_components = pdev->color_info.num_components;
    new_parent_color->unpack_procs = pdev->pdf14_procs;
    new_parent_color->depth = pdev->color_info.depth;
    new_parent_color->max_color = pdev->color_info.max_color;
    new_parent_color->max_gray = pdev->color_info.max_gray;
    memcpy(&(new_parent_color->comp_bits),&(pdev->color_info.comp_bits),
                        GX_DEVICE_COLOR_MAX_COMPONENTS);
    memcpy(&(new_parent_color->comp_shift),&(pdev->color_info.comp_shift),
                        GX_DEVICE_COLOR_MAX_COMPONENTS);
    /* The ICC manager has the ICC profile for the device */
    new_parent_color->icc_profile = dev->device_icc_profile;
    rc_increment(dev->device_icc_profile);
    /* isadditive is only used in ctx */
    if (pdev->ctx) {
        new_parent_color->isadditive = pdev->ctx->additive;
    }
}

   /* When a transparency group is popped, the parent colorprocs
      must be restored.  Since the color mapping procs are
      all based upon the device, we must have a nested list 
      based upon the transparency group color space.  This 
      nesting must be outside the nested ctx structures 
      to allow the nesting for the clist writer */
static void
pdf14_pop_parent_color(gx_device *dev, const gs_imager_state *pis)
{
    pdf14_device *pdev = (pdf14_device *)dev;
    pdf14_parent_color_t *old_parent_color_info = pdev->trans_group_parent_cmap_procs;

     if_debug0('v', "[v]pdf14_pop_parent_color\n");
     /* We need to compliment pdf14_push_parent color */
     if (old_parent_color_info->icc_profile != NULL)
        rc_decrement(old_parent_color_info->icc_profile, "pdf14_pop_parent_color");
   /* Update the link */
    pdev->trans_group_parent_cmap_procs = old_parent_color_info->previous;
    /* Free the old one */
    gs_free_object(dev->memory, old_parent_color_info, "pdf14_clr_free");
}

static	int
pdf14_begin_transparency_mask(gx_device	*dev,
			      const gx_transparency_mask_params_t *ptmp,
			      const gs_rect *pbbox,
			      gs_imager_state *pis,
			      gs_transparency_state_t **ppts,
			      gs_memory_t *mem)
{
    pdf14_device *pdev = (pdf14_device *)dev;
    byte bg_alpha = 255;
    byte *transfer_fn = (byte *)gs_alloc_bytes(pdev->ctx->memory, 256,
					       "pdf14_begin_transparency_mask");
    gs_int_rect rect;
    int code;
    int group_color_numcomps;
    gs_transparency_color_t group_color; 

    if (transfer_fn == NULL)
	return_error(gs_error_VMerror);
    code = compute_group_device_int_rect(pdev, &rect, pbbox, pis);
    if (code < 0)
	return code;
    if (ptmp->Background_components)
	bg_alpha = (int)(255 * ptmp->GrayBackground + 0.5);
    if_debug1('v', "pdf14_begin_transparency_mask, bg_alpha = %d\n", bg_alpha);
    memcpy(transfer_fn, ptmp->transfer_fn, size_of(ptmp->transfer_fn));
   /* If the group color is unknown, then we must use the previous group color
       space or the device process color space */
    if (ptmp->group_color == UNKNOWN){
        if (pdev->ctx->stack){
            /* Use previous group color space */
            group_color_numcomps = pdev->ctx->stack->n_chan-1;  /* Remove alpha */
        } else {
            /* Use process color space */
            group_color_numcomps = pdev->color_info.num_components;
        }
        switch (group_color_numcomps) {
            case 1:				
                group_color = GRAY_SCALE;       
                break;
            case 3:				
                group_color = DEVICE_RGB;       
                break;
            case 4:				
                group_color = DEVICE_CMYK;       
            break;
            default:
                
                /* We can end up here if we are in
                   a deviceN color space and 
                   we have a sep output device */
                group_color = DEVICEN;
            break;
         }  
    } else {

        group_color = ptmp->group_color;
        group_color_numcomps = ptmp->group_color_numcomps;
    }
    /* Always update the color mapping procs.  Otherwise we end up
       fowarding to the target device. */
    code = pdf14_update_device_color_procs(dev, group_color, ptmp->icc_hashcode, pis, ptmp->iccprofile);
    if (code < 0)
	return code;
    /* Note that the soft mask always follows the group color requirements even
       when we have a separable device */
    return pdf14_push_transparency_mask(pdev->ctx, &rect, bg_alpha,
					transfer_fn, ptmp->idle, ptmp->replacing,
					ptmp->mask_id, ptmp->subtype, 
                                        ptmp->SMask_is_CIE, group_color_numcomps,
                                        ptmp->Background_components, ptmp->Background,
                                        ptmp->GrayBackground);
}

static	int
pdf14_end_transparency_mask(gx_device *dev, gs_imager_state *pis,
			  gs_transparency_mask_t **pptm)
{
    pdf14_device *pdev = (pdf14_device *)dev;
    pdf14_parent_color_t *parent_color;
    int ok;

    if_debug0('v', "pdf14_end_transparency_mask\n");
    ok = pdf14_pop_transparency_mask(pdev->ctx, pis, dev);
    /* May need to reset some color stuff related
     * to a mismatch between the Smask color space
     * and the Smask blending space */
    if (pdev->ctx->stack != NULL ) {
        parent_color = pdev->ctx->stack->parent_color_info_procs;
        if (!(parent_color->parent_color_mapping_procs == NULL && 
            parent_color->parent_color_comp_index == NULL)) {
            pis->get_cmap_procs = parent_color->get_cmap_procs;
                gx_set_cmap_procs(pis, dev);
                pdev->procs.get_color_mapping_procs = parent_color->parent_color_mapping_procs;
                pdev->procs.get_color_comp_index = parent_color->parent_color_comp_index;
                pdev->color_info.polarity = parent_color->polarity;
                pdev->color_info.num_components = parent_color->num_components;
                pdev->color_info.depth = parent_color->depth;
                pdev->blend_procs = parent_color->parent_blending_procs;
                pdev->ctx->additive = parent_color->isadditive;
                pdev->pdf14_procs = parent_color->unpack_procs;
            pdev->color_info.max_color = parent_color->max_color;
            pdev->color_info.max_gray = parent_color->max_gray;
                parent_color->get_cmap_procs = NULL;
                parent_color->parent_color_comp_index = NULL;
                parent_color->parent_color_mapping_procs = NULL;
            memcpy(&(pdev->color_info.comp_bits),&(parent_color->comp_bits),
                                GX_DEVICE_COLOR_MAX_COMPONENTS);
            memcpy(&(pdev->color_info.comp_shift),&(parent_color->comp_shift),
                                GX_DEVICE_COLOR_MAX_COMPONENTS);
            /* Take care of the ICC profile */
            if (parent_color->icc_profile != NULL) {
                dev->device_icc_profile = parent_color->icc_profile;
                rc_decrement(parent_color->icc_profile,"pdf14_end_transparency_mask");
                parent_color->icc_profile = NULL;
        }
    }
    }
    return ok;
}

static	int
pdf14_mark_fill_rectangle(gx_device * dev,
			 int x, int y, int w, int h, gx_color_index color)
{
    pdf14_device *pdev = (pdf14_device *)dev;
    pdf14_buf *buf = pdev->ctx->stack;
    int i, j, k;
    byte *line, *dst_ptr;
    byte src[PDF14_MAX_PLANES];
    byte dst[PDF14_MAX_PLANES];
    gs_blend_mode_t blend_mode = pdev->blend_mode;
    bool additive = pdev->ctx->additive;
    int rowstride = buf->rowstride;
    int planestride = buf->planestride;
    gs_object_tag_type_t curr_tag;
    bool has_alpha_g = buf->has_alpha_g;
    bool has_shape = buf->has_shape;
    bool has_tags = buf->has_tags;
    int num_chan = buf->n_chan;
    int num_comp = num_chan - 1;
    int shape_off = num_chan * planestride;
    int alpha_g_off = shape_off + (has_shape ? planestride : 0);
    int tag_off = alpha_g_off + (has_tags ? planestride : 0);
    bool overprint = pdev->overprint;
    gx_color_index drawn_comps = pdev->drawn_comps;
    gx_color_index comps;
    byte shape = 0; /* Quiet compiler. */
    byte src_alpha;

    if (buf->data == NULL)
	return 0;
    /* NB: gx_color_index is 4 or 8 bytes */
#if 0
    if (sizeof(color) <= sizeof(ulong))
	if_debug8('v', "[v]pdf14_mark_fill_rectangle, (%d, %d), %d x %d color = %lx  bm %d, nc %d, overprint %d\n", 
		    x, y, w, h, (ulong)color, blend_mode, num_chan, overprint);
    else
	if_debug9('v', "[v]pdf14_mark_fill_rectangle, (%d, %d), %d x %d color = %08lx%08lx  bm %d, nc %d, overprint %d\n", 
		    x, y, w, h, 
		    (ulong)(color >> 8*(sizeof(color) - sizeof(ulong))), (ulong)color, 
		    blend_mode, num_chan, overprint);
#endif
    /*
     * Unpack the gx_color_index values.  Complement the components for subtractive
     * color spaces.
     */
    if (has_tags) {
        curr_tag = (color >> (num_comp*8)) & 0xff; 
    }
    pdev->pdf14_procs->unpack_color(num_comp, color, pdev, src);
    src_alpha = src[num_comp] = (byte)floor (255 * pdev->alpha + 0.5);
    if (has_shape)
	shape = (byte)floor (255 * pdev->shape + 0.5);
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
    if (x < buf->bbox.p.x) buf->bbox.p.x = x;
    if (y < buf->bbox.p.y) buf->bbox.p.y = y;
    if (x + w > buf->bbox.q.x) buf->bbox.q.x = x + w;
    if (y + h > buf->bbox.q.y) buf->bbox.q.y = y + h;
    line = buf->data + (x - buf->rect.p.x) + (y - buf->rect.p.y) * rowstride;
    for (j = 0; j < h; ++j) {
	dst_ptr = line;
	for (i = 0; i < w; ++i) {
	    /* Complement the components for subtractive color spaces */
	    if (additive) {
		for (k = 0; k < num_chan; ++k)
		    dst[k] = dst_ptr[k * planestride];
	    }
	    else { /* Complement the components for subtractive color spaces */
		for (k = 0; k < num_comp; ++k)
		    dst[k] = 255 - dst_ptr[k * planestride];
		dst[num_comp] = dst_ptr[num_comp * planestride];
	    }
	    art_pdf_composite_pixel_alpha_8(dst, src, num_comp,
			   		 blend_mode, pdev->blend_procs);
	    /* Complement the results for subtractive color spaces */
	    if (additive) {
			for (k = 0; k < num_chan; ++k)
				dst_ptr[k * planestride] = dst[k];
		} else {
			if (overprint) {
				for (k = 0, comps = drawn_comps; comps != 0; ++k, comps >>= 1) {
					if ((comps & 0x1) != 0) {
						dst_ptr[k * planestride] = 255 - dst[k];
					}
				}
				/* The alpha channel */
				dst_ptr[num_comp * planestride] = dst[num_comp];
			} else {
				for (k = 0; k < num_comp; ++k)
					dst_ptr[k * planestride] = 255 - dst[k];
				dst_ptr[num_comp * planestride] = dst[num_comp];
			}
	    }
	    if (has_alpha_g) {
		int tmp = (255 - dst_ptr[alpha_g_off]) * (255 - src_alpha) + 0x80;
		dst_ptr[alpha_g_off] = 255 - ((tmp + (tmp >> 8)) >> 8);
	    }
	    if (has_shape) {
		int tmp = (255 - dst_ptr[shape_off]) * (255 - shape) + 0x80;
		dst_ptr[shape_off] = 255 - ((tmp + (tmp >> 8)) >> 8);
	    }
            if (has_tags) {
                /* If alpha is 100% then set to pure path, else or */
                if (dst[num_comp] == 255) { 
                    dst_ptr[tag_off] = curr_tag;
                } else {
                    dst_ptr[tag_off] = ( dst_ptr[tag_off] |curr_tag ) & ~GS_UNTOUCHED_TAG;
                }
            }
	    ++dst_ptr;
	}
	line += rowstride;
    }
#if 0
/* #if RAW_DUMP */
    /* Dump the current buffer to see what we have. */
	
	if(global_index/10.0 == (int) (global_index/10.0) )
		dump_raw_buffer(pdev->ctx->stack->rect.q.y-pdev->ctx->stack->rect.p.y, 
					pdev->ctx->stack->rect.q.x-pdev->ctx->stack->rect.p.x, 
					pdev->ctx->stack->n_planes,
					pdev->ctx->stack->planestride, pdev->ctx->stack->rowstride, 
					"Draw_Rect",pdev->ctx->stack->data);

    global_index++;
#endif
    return 0;
}

static	int
pdf14_mark_fill_rectangle_ko_simple(gx_device *	dev,
				   int x, int y, int w, int h, gx_color_index color)
{
    pdf14_device *pdev = (pdf14_device *)dev;
    pdf14_buf *buf = pdev->ctx->stack;
    int i, j, k;
    byte *line, *dst_ptr;
    byte src[PDF14_MAX_PLANES];
    byte dst[PDF14_MAX_PLANES];
    int rowstride = buf->rowstride;
    int planestride = buf->planestride;
    int num_chan = buf->n_chan;
    int num_comp = num_chan - 1;
    int shape_off = num_chan * planestride;
    bool has_shape = buf->has_shape;
    bool has_alpha_g = buf->has_alpha_g;
    int tag_off = shape_off + (has_alpha_g ? planestride : 0) +
                  (has_shape ? planestride : 0);
    bool has_tags = buf->has_tags;
    bool additive = pdev->ctx->additive;
    gs_object_tag_type_t curr_tag;

    if (buf->data == NULL)
	return 0;

    if (sizeof(color) <= sizeof(ulong))
	if_debug6('v', "[v]pdf14_mark_fill_rectangle_ko_simple, (%d, %d), %d x %d color = %lx, nc %d,\n", 
		    x, y, w, h, (ulong)color, num_chan);
    else
	if_debug7('v', "[v]pdf14_mark_fill_rectangle_ko_simple, (%d, %d), %d x %d color = %8lx%08lx, nc %d,\n", 
		    x, y, w, h, 
		    (ulong)(color >> 8*(sizeof(color) - sizeof(ulong))), (ulong)color, 
		    num_chan);

    /*
     * Unpack the gx_color_index values.  Complement the components for subtractive
     * color spaces.
     */
    if (has_tags) {
        curr_tag = (color >> (num_comp*8)) & 0xff; 
    }
    pdev->pdf14_procs->unpack_color(num_comp, color, pdev, src);
    src[num_comp] = (byte)floor (255 * pdev->alpha + 0.5);
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

    if (x < buf->bbox.p.x) buf->bbox.p.x = x;
    if (y < buf->bbox.p.y) buf->bbox.p.y = y;
    if (x + w > buf->bbox.q.x) buf->bbox.q.x = x + w;
    if (y + h > buf->bbox.q.y) buf->bbox.q.y = y + h;

    line = buf->data + (x - buf->rect.p.x) + (y - buf->rect.p.y) * rowstride;

    for (j = 0; j < h; ++j) {
	dst_ptr = line;
	for (i = 0; i < w; ++i) {
	    /* Complement the components for subtractive color spaces */
	    if (additive) {
		for (k = 0; k < num_chan; ++k)
		    dst[k] = dst_ptr[k * planestride];
	    }
	    else {
		for (k = 0; k < num_comp; ++k)
		    dst[k] = 255 - dst_ptr[k * planestride];
		dst[num_comp] = dst_ptr[num_comp * planestride];
	    }
	    art_pdf_composite_knockout_simple_8(dst,
		has_shape ? dst_ptr + shape_off : NULL, 
                has_tags ? dst_ptr + tag_off : NULL,     
                src, curr_tag, num_comp, 255);
            /* ToDo:  Review use of shape and opacity above.   */ 
	    /* Complement the results for subtractive color spaces */
	    if (additive) {
		for (k = 0; k < num_chan; ++k)
		    dst_ptr[k * planestride] = dst[k];
	    }
	    else {
		for (k = 0; k < num_comp; ++k)
		    dst_ptr[k * planestride] = 255 - dst[k];
		dst_ptr[num_comp * planestride] = dst[num_comp];
	    }
	    ++dst_ptr;
	}
	line += rowstride;
    }
    return 0;
}


/**
 * Here we have logic to override the cmap_procs with versions that
 * do not apply the transfer function. These copies should track the
 * versions in gxcmap.c.
 **/

static	cmap_proc_gray(pdf14_cmap_gray_direct);
static	cmap_proc_rgb(pdf14_cmap_rgb_direct);
static	cmap_proc_cmyk(pdf14_cmap_cmyk_direct);
static	cmap_proc_gray(pdf14_cmap_gray_direct_group);
static	cmap_proc_rgb(pdf14_cmap_rgb_direct_group);
static	cmap_proc_cmyk(pdf14_cmap_cmyk_direct_group);
static	cmap_proc_rgb_alpha(pdf14_cmap_rgb_alpha_direct);
static	cmap_proc_separation(pdf14_cmap_separation_direct);
static	cmap_proc_devicen(pdf14_cmap_devicen_direct);
static	cmap_proc_is_halftoned(pdf14_cmap_is_halftoned);

static	const gx_color_map_procs pdf14_cmap_many = {
     pdf14_cmap_gray_direct,
     pdf14_cmap_rgb_direct,
     pdf14_cmap_cmyk_direct,
     pdf14_cmap_rgb_alpha_direct,
     pdf14_cmap_separation_direct,
     pdf14_cmap_devicen_direct,
     pdf14_cmap_is_halftoned
    };

static	const gx_color_map_procs pdf14_cmap_many_group = {
     pdf14_cmap_gray_direct_group,
     pdf14_cmap_rgb_direct_group,
     pdf14_cmap_cmyk_direct_group,
     pdf14_cmap_rgb_alpha_direct,
     pdf14_cmap_separation_direct,
     pdf14_cmap_devicen_direct,
     pdf14_cmap_is_halftoned
    };

/**
 * Note: copied from gxcmap.c because it's inlined.
 **/
static	inline void
map_components_to_colorants(const frac * pcc,
	const gs_devicen_color_map * pcolor_component_map, frac * plist)
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

static	void
pdf14_cmap_gray_direct(frac gray, gx_device_color * pdc, const gs_imager_state * pis,
		 gx_device * dev, gs_color_select_t select)
{
    int i,ncomps; 
    frac cm_comps[GX_DEVICE_COLOR_MAX_COMPONENTS];
    gx_color_value cv[GX_DEVICE_COLOR_MAX_COMPONENTS];
    gx_color_index color;
    gx_device *trans_device;

    /* If trans device is set, we need to use its procs. */
    if (pis->trans_device != NULL){
        trans_device = pis->trans_device;
    } else {
        trans_device = dev;
    }
    ncomps = trans_device->color_info.num_components;
    /* map to the color model */
    dev_proc(trans_device, get_color_mapping_procs)(trans_device)->map_gray(trans_device, gray, cm_comps);
    for (i = 0; i < ncomps; i++)
	cv[i] = frac2cv(cm_comps[i]);
    /* encode as a color index */
    color = dev_proc(trans_device, encode_color)(trans_device, cv);
    /* check if the encoding was successful; we presume failure is rare */
    if (color != gx_no_color_index)
	color_set_pure(pdc, color);
}

static	void
pdf14_cmap_rgb_direct(frac r, frac g, frac b, gx_device_color *	pdc,
     const gs_imager_state * pis, gx_device * dev, gs_color_select_t select)
{
    int i,ncomps; 
    frac cm_comps[GX_DEVICE_COLOR_MAX_COMPONENTS];
    gx_color_value cv[GX_DEVICE_COLOR_MAX_COMPONENTS];
    gx_color_index color;
    gx_device *trans_device;

    /* If trans device is set, we need to use its procs. */
    if (pis->trans_device != NULL){
        trans_device = pis->trans_device;
    } else {
        trans_device = dev;
    }
    ncomps = trans_device->color_info.num_components;
    /* map to the color model */
    dev_proc(trans_device, get_color_mapping_procs)(trans_device)->map_rgb(trans_device, pis, r, g, b, cm_comps);
    for (i = 0; i < ncomps; i++)
	cv[i] = frac2cv(cm_comps[i]);
    /* encode as a color index */
    color = dev_proc(trans_device, encode_color)(trans_device, cv);
    /* check if the encoding was successful; we presume failure is rare */
    if (color != gx_no_color_index)
	color_set_pure(pdc, color);
}

static	void
pdf14_cmap_cmyk_direct(frac c, frac m, frac y, frac k, gx_device_color * pdc,
     const gs_imager_state * pis, gx_device * dev, gs_color_select_t select)
{
    int i,ncomps; 
    frac cm_comps[GX_DEVICE_COLOR_MAX_COMPONENTS];
    gx_color_value cv[GX_DEVICE_COLOR_MAX_COMPONENTS];
    gx_color_index color;
    gx_device *trans_device;

    /* If trans device is set, we need to use its procs. */
    if (pis->trans_device != NULL){
        trans_device = pis->trans_device;
    } else {
        trans_device = dev;
    }
    ncomps = trans_device->color_info.num_components;
    /* map to the color model */
    dev_proc(trans_device, get_color_mapping_procs)(trans_device)->map_cmyk(trans_device, c, m, y, k, cm_comps);
    for (i = 0; i < ncomps; i++)
	cv[i] = frac2cv(cm_comps[i]);
    color = dev_proc(trans_device, encode_color)(trans_device, cv);
    if (color != gx_no_color_index) 
	color_set_pure(pdc, color);
}

/* color mapping for when we have an smask or a isolated transparency group with another color space */
static	void
pdf14_cmap_gray_direct_group(frac gray, gx_device_color * pdc, const gs_imager_state * pis,
		 gx_device * dev, gs_color_select_t select)
{
    int i, ncomps = dev->color_info.num_components;
    frac cm_comps[GX_DEVICE_COLOR_MAX_COMPONENTS];
    gx_color_value cv[GX_DEVICE_COLOR_MAX_COMPONENTS];
    gx_color_index color;
    gx_device *trans_device;

     /*  We may be coming from the clist writer 
         which often forwards us the target device.
         If this occurs we actually need to get to
         the color space defined by the transparency group
         and we use the operators defined by the transparency device 
         to do the job.  
       */
    if (pis->trans_device != NULL){
        trans_device = pis->trans_device;
    } else {
        trans_device = dev;
    }
    ncomps = trans_device->color_info.num_components;
   /* If we are doing concretization of colors in an SMask or isolated group 
       then just return the color as is */
   if (ncomps == 1 ) {
	cv[0] = frac2cv(gray);
        /* encode as a color index */
        color = dev_proc(trans_device, encode_color)(trans_device, cv);
        /* check if the encoding was successful; we presume failure is rare */
         if (color != gx_no_color_index)
	    color_set_pure(pdc, color);
    } else {
        /* map to the color model */
        dev_proc(trans_device, get_color_mapping_procs)(trans_device)->map_gray(trans_device, gray, cm_comps);
        for (i = 0; i < ncomps; i++)
	    cv[i] = frac2cv(cm_comps[i]);
        /* encode as a color index */
        color = dev_proc(trans_device, encode_color)(trans_device, cv);
        /* check if the encoding was successful; we presume failure is rare */
        if (color != gx_no_color_index)
	    color_set_pure(pdc, color);
    }
}

/* color mapping for when we have an smask or a isolated transparency group with another color space */
static	void
pdf14_cmap_rgb_direct_group(frac r, frac g, frac b, gx_device_color *	pdc,
     const gs_imager_state * pis, gx_device * dev, gs_color_select_t select)
{
    int i, ncomps = dev->color_info.num_components;
    frac cm_comps[GX_DEVICE_COLOR_MAX_COMPONENTS];
    gx_color_value cv[GX_DEVICE_COLOR_MAX_COMPONENTS];
    gx_color_index color;
    gx_device *trans_device;

     /*  We may be coming from the clist writer 
         which often forwards us the target device.
         If this occurs we actually need to get to
         the color space defined by the transparency group
         and we use the operators defined by the transparency device 
         to do the job.  
       */
    if (pis->trans_device != NULL){
        trans_device = pis->trans_device;
    } else {
        trans_device = dev;
    }
    ncomps = trans_device->color_info.num_components;
    if ( ncomps == 3 ){
        cv[0] = frac2cv(r);
        cv[1] = frac2cv(g);
        cv[2] = frac2cv(b);
        /* encode as a color index */
        color = dev_proc(trans_device, encode_color)(trans_device, cv);
       /* check if the encoding was successful; we presume failure is rare */
         if (color != gx_no_color_index)
        color_set_pure(pdc, color);    
    } else {
        /* map to the device color model */
        /* We can end up here, if for example we had a DeviceN
           color space with a CIE based alternate space and
           a output device that was  RGB but a blending
           space that was CMYK.  The proper way to solve this
           is to introduce another color space for the graphic
           state that has its own Joint CIE Cache between the
           source and a CMYK CRD (the transparency color space).
           The problem is that we can
           only have one CRD, which is defined by the output
           device.  We will fix these issues with the
           new ICC base color architecture. */
        dev_proc(trans_device, get_color_mapping_procs)(trans_device)->map_rgb(trans_device, pis, r, g, b, cm_comps);
        for (i = 0; i < ncomps; i++)
            cv[i] = frac2cv(cm_comps[i]);
        /* encode as a color index */
        color = dev_proc(trans_device, encode_color)(trans_device, cv);
        /* check if the encoding was successful; we presume failure is rare */
        if (color != gx_no_color_index)
            color_set_pure(pdc, color);
    }
}

/* color mapping for when we have an smask or a isolated transparency group with another color space */
static	void
pdf14_cmap_cmyk_direct_group(frac c, frac m, frac y, frac k, gx_device_color * pdc,
     const gs_imager_state * pis, gx_device * dev, gs_color_select_t select)
{
    int i, ncomps = dev->color_info.num_components;
    frac cm_comps[GX_DEVICE_COLOR_MAX_COMPONENTS];
    gx_color_value cv[GX_DEVICE_COLOR_MAX_COMPONENTS];
    gx_color_index color;
    gx_device *trans_device;

     /*  We may be coming from the clist writer 
         which often forwards us the target device.
         If this occurs we actually need to get to
         the color space defined by the transparency group
         and we use the operators defined by the transparency device 
         to do the job.  
       */
    if (pis->trans_device != NULL){
        trans_device = pis->trans_device;
    } else {
        trans_device = dev;
    }
    ncomps = trans_device->color_info.num_components;
    if (ncomps == 4 ){
        cv[0] = frac2cv(c);
        cv[1] = frac2cv(m);
        cv[2] = frac2cv(y);
        cv[3] = frac2cv(k);
         /* encode as a color index */
        color = dev_proc(trans_device, encode_color)(trans_device, cv);
        /* check if the encoding was successful; we presume failure is rare */
        if (color != gx_no_color_index)
	    color_set_pure(pdc, color); 
    } else {
        /* map to the color model */
        dev_proc(trans_device, get_color_mapping_procs)(trans_device)->map_cmyk(trans_device, c, m, y, k, cm_comps);
        for (i = 0; i < ncomps; i++)
	    cv[i] = frac2cv(cm_comps[i]);
        color = dev_proc(trans_device, encode_color)(trans_device, cv);
        if (color != gx_no_color_index) 
	    color_set_pure(pdc, color);
    }
}

static	void
pdf14_cmap_rgb_alpha_direct(frac r, frac g, frac b, frac alpha,	gx_device_color	* pdc,
     const gs_imager_state * pis, gx_device * dev, gs_color_select_t select)
{
    int i, ncomps;
    frac cm_comps[GX_DEVICE_COLOR_MAX_COMPONENTS];
    gx_color_value cv[GX_DEVICE_COLOR_MAX_COMPONENTS];
    gx_color_index color;
    gx_device *trans_device;

     /*  We may be coming from the clist writer 
         which often forwards us the target device.
         If this occurs we actually need to get to
         the color space defined by the transparency group
         and we use the operators defined by the transparency device 
         to do the job.  
       */
    if (pis->trans_device != NULL){
        trans_device = pis->trans_device;
    } else {
        trans_device = dev;
    }
    ncomps = trans_device->color_info.num_components;
    /* map to the color model */
    dev_proc(trans_device, get_color_mapping_procs)(trans_device)->map_rgb(trans_device, pis, r, g, b, cm_comps);
    /* pre-multiply to account for the alpha weighting */
    if (alpha != frac_1) {
#ifdef PREMULTIPLY_TOWARDS_WHITE
	frac alpha_bias = frac_1 - alpha;
#else
	frac alpha_bias = 0;
#endif
	for (i = 0; i < ncomps; i++)
	    cm_comps[i] = (frac)((long)cm_comps[i] * alpha) / frac_1 + alpha_bias;
    }

    for (i = 0; i < ncomps; i++)
	cv[i] = frac2cv(cm_comps[i]);
    /* encode as a color index */
   /* if (dev_proc(dev, map_rgb_alpha_color) != gx_default_map_rgb_alpha_color &&
	 (cv_alpha = frac2cv(alpha)) != gx_max_color_value)
    else
	color = dev_proc(dev, encode_color)(dev, cv);  */
    color = dev_proc(trans_device, encode_color)(trans_device, cv); 
    /* check if the encoding was successful; we presume failure is rare */
    if (color != gx_no_color_index)
	color_set_pure(pdc, color);
}

static	void
pdf14_cmap_separation_direct(frac all, gx_device_color * pdc, const gs_imager_state * pis,
		 gx_device * dev, gs_color_select_t select)
{
    int i, ncomps = dev->color_info.num_components;
    bool additive = dev->color_info.polarity == GX_CINFO_POLARITY_ADDITIVE;
    frac comp_value = all;
    frac cm_comps[GX_DEVICE_COLOR_MAX_COMPONENTS];
    gx_color_value cv[GX_DEVICE_COLOR_MAX_COMPONENTS];
    gx_color_index color;

    if (pis->color_component_map.sep_type == SEP_ALL) {
	/*
	 * Invert the photometric interpretation for additive
	 * color spaces because separations are always subtractive.
	 */
	if (additive)
	    comp_value = frac_1 - comp_value;
	/* Use the "all" value for all components */
	i = pis->color_component_map.num_colorants - 1;
	for (; i >= 0; i--)
	    cm_comps[i] = comp_value;
    }
    else {
	/* map to the color model */
	map_components_to_colorants(&comp_value, &(pis->color_component_map), cm_comps);
    }
    /* apply the transfer function(s); convert to color values */
    if (additive)
	for (i = 0; i < ncomps; i++)
	    cv[i] = frac2cv(gx_map_color_frac(pis,
				cm_comps[i], effective_transfer[i]));
    else
	for (i = 0; i < ncomps; i++)
	    cv[i] = frac2cv(frac_1 - gx_map_color_frac(pis,
			(frac)(frac_1 - cm_comps[i]), effective_transfer[i]));
    /* encode as a color index */
    color = dev_proc(dev, encode_color)(dev, cv);
    /* check if the encoding was successful; we presume failure is rare */
    if (color != gx_no_color_index)
	color_set_pure(pdc, color);
}

static	void
pdf14_cmap_devicen_direct(const	frac * pcc, 
    gx_device_color * pdc, const gs_imager_state * pis, gx_device * dev,
    gs_color_select_t select)
{
    int i, ncomps = dev->color_info.num_components;
    frac cm_comps[GX_DEVICE_COLOR_MAX_COMPONENTS];
    gx_color_value cv[GX_DEVICE_COLOR_MAX_COMPONENTS];
    gx_color_index color;
    gx_device *trans_device;

     /*  We may be coming from the clist writer 
         which often forwards us the target device.
         If this occurs we actually need to get to
         the color space defined by the transparency group
         and we use the operators defined by the transparency device 
         to do the job.  
       */
    if (pis->trans_device != NULL){
        trans_device = pis->trans_device;
    } else {
        trans_device = dev;
    }
    ncomps = trans_device->color_info.num_components;
    /* map to the color model */
    map_components_to_colorants(pcc, &(pis->color_component_map), cm_comps);;
    /* apply the transfer function(s); convert to color values */
    if (trans_device->color_info.polarity == GX_CINFO_POLARITY_ADDITIVE)
	for (i = 0; i < ncomps; i++)
	    cv[i] = frac2cv(gx_map_color_frac(pis,
				cm_comps[i], effective_transfer[i]));
    else
	for (i = 0; i < ncomps; i++)
	    cv[i] = frac2cv(frac_1 - gx_map_color_frac(pis,
			(frac)(frac_1 - cm_comps[i]), effective_transfer[i]));
    /* encode as a color index */
    color = dev_proc(trans_device, encode_color)(trans_device, cv);
    /* check if the encoding was successful; we presume failure is rare */
    if (color != gx_no_color_index)
	color_set_pure(pdc, color);
}

static	bool
pdf14_cmap_is_halftoned(const gs_imager_state *	pis, gx_device * dev)
{
    return false;
}

static	const gx_color_map_procs *
pdf14_get_cmap_procs(const gs_imager_state *pis, const gx_device * dev)
{
    /* The pdf14 marking device itself is always continuous tone. */
    return &pdf14_cmap_many;
}

static	const gx_color_map_procs *
pdf14_get_cmap_procs_group(const gs_imager_state *pis, const gx_device * dev)
{
    /* The pdf14 marking device itself is always continuous tone. */
    return &pdf14_cmap_many_group;
}


static int 
pdf14_pattern_manage(gx_device *pdev, gx_bitmap_id id,
		gs_pattern1_instance_t *pinst, pattern_manage_t function)
{
    if (function == pattern_manage__shfill_doesnt_need_path)
	return 1;
    return gx_default_pattern_manage(pdev, id, pinst, function);
}

int
gs_pdf14_device_push(gs_memory_t *mem, gs_imager_state * pis,
	gx_device ** pdev, gx_device * target, const gs_pdf14trans_t * pdf14pct)
{
    pdf14_device * dev_proto;
    pdf14_device * p14dev, temp_dev_proto;
    int code;
    bool has_tags = (mem->gs_lib_ctx->BITTAG != GS_DEVICE_DOESNT_SUPPORT_TAGS);

    if_debug0('v', "[v]gs_pdf14_device_push\n");
    code = get_pdf14_device_proto(target, &dev_proto,
		   		 &temp_dev_proto, pis, pdf14pct);
    if (code < 0)
	return code;
    code = gs_copydevice((gx_device **) &p14dev,
			 (const gx_device *) dev_proto, mem);
    if (code < 0)
	return code;
    gs_pdf14_device_copy_params((gx_device *)p14dev, target);
    rc_assign(p14dev->target, target, "gs_pdf14_device_push");
    /* If we have a tag device then go ahead and do a special encoder
       decoder for the pdf14 device to make sure we maintain this
       information in the encoded color information.  We could use
       the target device's methods but the PDF14 device has to maintain
       8 bit color always and we could run into other issues if the number
       of colorants became large.  If we need to do compressed color with
       tags that will be a special project at that time */
    if (has_tags) {
        p14dev->procs.encode_color = pdf14_encode_color_tag;
        p14dev->color_info.depth += 8;
    }
    check_device_separable((gx_device *)p14dev);
    gx_device_fill_in_procs((gx_device *)p14dev);
    p14dev->save_get_cmap_procs = pis->get_cmap_procs;
    pis->get_cmap_procs = pdf14_get_cmap_procs;
    gx_set_cmap_procs(pis, (gx_device *)p14dev);
    code = dev_proc((gx_device *) p14dev, open_device) ((gx_device *) p14dev);
    *pdev = (gx_device *) p14dev;
    pdf14_set_marking_params((gx_device *)p14dev, pis);
    p14dev->trans_group_parent_cmap_procs = NULL;
#if RAW_DUMP
    /* Dump the current buffer to see what we have. */
    dump_raw_buffer(p14dev->ctx->stack->rect.q.y-p14dev->ctx->stack->rect.p.y, 
                p14dev->ctx->stack->rect.q.x-p14dev->ctx->stack->rect.p.x, 
				p14dev->ctx->stack->n_planes,
                p14dev->ctx->stack->planestride, p14dev->ctx->stack->rowstride, 
                "Device_Push",p14dev->ctx->stack->data);

    global_index++;
#endif
    return code;
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

#define	put_value(dp, value)\
    BEGIN\
	memcpy(dp, &value, sizeof(value));\
	dp += sizeof(value);\
    END

static inline int
c_pdf14trans_write_ctm(byte **ppbuf, const gs_pdf14trans_params_t *pparams)
{
    /* Note: We can't skip writing CTM if it is equal to pis->ctm,
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
 * list device.
 */
static	int
c_pdf14trans_write(const gs_composite_t	* pct, byte * data, uint * psize, gx_device_clist_writer *cdev)
{
    const gs_pdf14trans_params_t * pparams = &((const gs_pdf14trans_t *)pct)->params;
    int need, avail = *psize;
    byte buf[MAX_CLIST_TRANSPARENCY_BUFFER_SIZE]; /* Must be large enough 
    	to fit the data written below. We don't implement a dynamic check for the buffer owerflow,
	assuming that the consistency is verified in the coding phase. 
	See the definition of MAX_CLIST_TRANSPARENCY_BUFFER_SIZE. */
    byte * pbuf = buf;
    int opcode = pparams->pdf14_op;
    int mask_size = 0;
    uint mask_id = 0;
    int code;
    bool found_icc;
    int64_t hashcode = 0;

    *pbuf++ = opcode;			/* 1 byte */
    switch (opcode) {
	default:			/* Should not occur. */
	    break;
	case PDF14_PUSH_DEVICE:
	    put_value(pbuf, pparams->num_spot_colors);	    
	    break;
	case PDF14_POP_DEVICE:
	    code = 0;
	    break;
	case PDF14_END_TRANS_GROUP:
	    break;			/* No data */
	case PDF14_BEGIN_TRANS_GROUP:
	    code = c_pdf14trans_write_ctm(&pbuf, pparams);
	    if (code < 0)
		return code;
	    *pbuf++ = (pparams->Isolated & 1) + ((pparams->Knockout & 1) << 1);
	    *pbuf++ = pparams->blend_mode;
	    *pbuf++ = pparams->group_color;
	    put_value(pbuf, pparams->group_color_numcomps);
	    put_value(pbuf, pparams->opacity.alpha);
	    put_value(pbuf, pparams->shape.alpha);
	    put_value(pbuf, pparams->bbox);
	    mask_id = pparams->mask_id;
	    put_value(pbuf, pparams->mask_id);
            /* Color space information maybe ICC based 
               in this case we need to store the ICC
               profile or the ID if it is cached already */
            if (pparams->group_color == ICC) {                   
                /* Check if it is already in the ICC clist table */
                hashcode = pparams->iccprofile->hashcode;
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
	    code = c_pdf14trans_write_ctm(&pbuf, pparams);
	    if (code < 0)
		return code;
	    put_value(pbuf, pparams->subtype);
	    *pbuf++ = pparams->group_color;
	    put_value(pbuf, pparams->group_color_numcomps);
	    *pbuf++ = pparams->replacing;
	    *pbuf++ = pparams->function_is_identity;
	    *pbuf++ = pparams->Background_components;
	    put_value(pbuf, pparams->bbox);
	    mask_id = pparams->mask_id;
	    put_value(pbuf, pparams->mask_id);
	    if (pparams->Background_components) {
		const int l = sizeof(pparams->Background[0]) * pparams->Background_components;

		memcpy(pbuf, pparams->Background, l);
		pbuf += l;
		memcpy(pbuf, &pparams->GrayBackground, sizeof(pparams->GrayBackground));
		pbuf += sizeof(pparams->GrayBackground);
	    }
	    if (!pparams->function_is_identity)
		mask_size = sizeof(pparams->transfer_fn);
            /* Color space information may be ICC based 
               in this case we need to store the ICC
               profile or the ID if it is cached already */
            if (pparams->group_color == ICC) {                   
                /* Check if it is already in the ICC clist table */
                hashcode = pparams->iccprofile->hashcode;
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
	    break;
	case PDF14_SET_BLEND_PARAMS:
	    *pbuf++ = pparams->changed;
	    if (pparams->changed & PDF14_SET_BLEND_MODE)
		*pbuf++ = pparams->blend_mode;
	    if (pparams->changed & PDF14_SET_TEXT_KNOCKOUT)
		*pbuf++ = pparams->text_knockout;
	    if (pparams->changed & PDF14_SET_OPACITY_ALPHA)
		put_value(pbuf, pparams->opacity.alpha);
	    if (pparams->changed & PDF14_SET_SHAPE_ALPHA)
		put_value(pbuf, pparams->shape.alpha);
		if (pparams->changed & PDF14_SET_OVERPRINT)
		put_value(pbuf, pparams->overprint);
	    if (pparams->changed & PDF14_SET_OVERPRINT_MODE)
		put_value(pbuf, pparams->overprint_mode);
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
     * return a rangecheck error.
     */
    if ( need + 3 > (MAX_CLIST_COMPOSITOR_SIZE) )
	return_error(gs_error_rangecheck);

    /* Copy our serialized data into the output buffer */
    memcpy(data, buf, need - mask_size);
    if (mask_size)	/* Include the transfer mask data if present */
	memcpy(data + need - mask_size, pparams->transfer_fn, mask_size);
    if_debug3('v', "[v] c_pdf14trans_write: opcode = %s mask_id=%d need = %d\n",
				pdf14_opcode_names[opcode], mask_id, need);
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

    if (size < 1)
	return_error(gs_error_rangecheck);

    /* Read PDF 1.4 compositor data from the clist */
    params.pdf14_op = *data++;
    if_debug2('v', "[v] c_pdf14trans_read: opcode = %s  avail = %d",
				pdf14_opcode_names[params.pdf14_op], size);
    memset(&params.ctm, 0, sizeof(params.ctm));
    switch (params.pdf14_op) {
	default:			/* Should not occur. */
	    break;
	case PDF14_PUSH_DEVICE:
	    read_value(data, params.num_spot_colors);	    
	    break;
	case PDF14_POP_DEVICE:
	    code += 0; /* A good place for a breakpoint. */
	    break;
	case PDF14_END_TRANS_GROUP:
	    code += 0; /* A good place for a breakpoint. */
	    break;			/* No data */
        case PDF14_PUSH_TRANS_STATE:
            break;
        case PDF14_POP_TRANS_STATE:
            break;
	case PDF14_BEGIN_TRANS_GROUP:
	    /*
	     * We are currently not using the bbox or the colorspace so they were
	     * not placed in the clist
	     */
	    data = cmd_read_matrix(&params.ctm, data);
	    params.Isolated = (*data) & 1;
	    params.Knockout = (*data++ >> 1) & 1;
	    params.blend_mode = *data++;
	    params.group_color = *data++;  /* Trans group color */
            read_value(data,params.group_color_numcomps);  /* color group size */
	    read_value(data, params.opacity.alpha);
	    read_value(data, params.shape.alpha);
	    read_value(data, params.bbox);
	    read_value(data, params.mask_id);
            read_value(data, params.icc_hash);
	    break;
	case PDF14_BEGIN_TRANS_MASK:
		/* This is the largest transparency parameter at this time (potentially
		 * 1275 bytes in size if Background_components = 
		 * GS_CLIENT_COLOR_MAX_COMPONENTS and we have a transfer function
		 * as well).
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
            params.group_color = *data++;
	    read_value(data, params.group_color_numcomps);
	    params.replacing = *data++;
	    params.function_is_identity = *data++;
	    params.Background_components = *data++;
	    read_value(data, params.bbox);
	    read_value(data, params.mask_id);
	    if (params.Background_components) {
		const int l = sizeof(params.Background[0]) * params.Background_components;

		memcpy(params.Background, data, l);
		data += l;
		memcpy(&params.GrayBackground, data, sizeof(params.GrayBackground));
		data += sizeof(params.GrayBackground);
	    }
            read_value(data, params.icc_hash);
	    if (params.function_is_identity) {
		int i;

		for (i = 0; i < MASK_TRANSFER_FUNCTION_SIZE; i++) {
		    params.transfer_fn[i] = (byte)floor(i *
			(255.0 / (MASK_TRANSFER_FUNCTION_SIZE - 1)) + 0.5);
		}
	    } else {
		read_value(data, params.transfer_fn);
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
	    params.changed = *data++;
	    if (params.changed & PDF14_SET_BLEND_MODE)
		params.blend_mode = *data++;
	    if (params.changed & PDF14_SET_TEXT_KNOCKOUT)
		params.text_knockout = *data++;
	    if (params.changed & PDF14_SET_OPACITY_ALPHA)
		read_value(data, params.opacity.alpha);
	    if (params.changed & PDF14_SET_SHAPE_ALPHA)
		read_value(data, params.shape.alpha);
		if (params.changed & PDF14_SET_OVERPRINT)
		read_value(data, params.overprint);
	    if (params.changed & PDF14_SET_OVERPRINT_MODE)
		read_value(data, params.overprint_mode);
	    break;
    }
    code = gs_create_pdf14trans(ppct, &params, mem);
    if (code < 0)
	return code;
    used = data - start;
    if_debug2('v', " mask_id=%d used = %d\n", params.mask_id, used);
    
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
c_pdf14trans_adjust_ctm(gs_composite_t * pct0, int x0, int y0, gs_imager_state *pis)
{
    gs_pdf14trans_t *pct = (gs_pdf14trans_t *)pct0;
    gs_matrix mat = pct->params.ctm;

    if_debug6('L', " [%g %g %g %g %g %g]\n",
	      mat.xx, mat.xy, mat.yx, mat.yy,
	      mat.tx, mat.ty);
    mat.tx -= x0;
    mat.ty -= y0;
    gs_imager_setmatrix(pis, &mat);
    return 0;
}

/*
 * Create a PDF 1.4 transparency compositor.
 *
 * Note that this routine will be called only if the device is not already
 * a PDF 1.4 transparency compositor.
 */
static	int
c_pdf14trans_create_default_compositor(const gs_composite_t * pct,
    gx_device ** pp14dev, gx_device * tdev, gs_imager_state * pis,
    gs_memory_t * mem)
{
    const gs_pdf14trans_t * pdf14pct = (const gs_pdf14trans_t *) pct;
    gx_device * p14dev = NULL;
    int code = 0;

    /*
     * We only handle the push operation.  All other operations are ignored.
     * The other operations will be handled by the create_compositor routine
     * for the PDF 1.4 compositing device.
     */
    switch (pdf14pct->params.pdf14_op) {
	case PDF14_PUSH_DEVICE:
	    code = gs_pdf14_device_push(mem, pis, &p14dev, tdev, pdf14pct);
	    *pp14dev = p14dev;
	    break;
	default:
	    *pp14dev = tdev;
	    break;
    }
    return code;
}

/*
 * Find an opening compositor op.
 */
static int
find_opening_op(int opening_op, gs_composite_t **ppcte, int return_code)
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
		    return 0;
		if (opening_op == PDF14_BEGIN_TRANS_GROUP) {
		    if (op != PDF14_BEGIN_TRANS_MASK && op != PDF14_END_TRANS_MASK)
			return 0;
		}
		if (opening_op == PDF14_PUSH_DEVICE) {
		    if (op != PDF14_BEGIN_TRANS_MASK && op != PDF14_END_TRANS_MASK &&
			op != PDF14_BEGIN_TRANS_GROUP && op != PDF14_END_TRANS_GROUP)
			return 0;
		}
	    }
	} else
	    return 0;
	pcte = pcte->prev;
	if (pcte == NULL)
	    return 2; /* Not in queue. */
    }
}

/*
 * Find an opening compositor op.
 */
static int
find_same_op(const gs_composite_t *this, int my_op, gs_composite_t **ppcte)
{
    const gs_pdf14trans_t *pct0 = (gs_pdf14trans_t *)this;
    gs_composite_t *pct = *ppcte;

    for (;;) {
	if (pct->type->comp_id == GX_COMPOSITOR_PDF14_TRANS) {
	    gs_pdf14trans_t *pct_pdf14 = (gs_pdf14trans_t *)pct;
	    
	    *ppcte = pct;
	    if (pct_pdf14->params.pdf14_op != my_op)
		return 0;
	    if (pct_pdf14->params.csel == pct0->params.csel)
		return 4;
	} else
	    return 0;
	pct = pct->prev;
	if (pct == NULL)
	    return 0; /* Not in queue. */
    }
}

/*
 * Check for closing compositor.
 */
static int
c_pdf14trans_is_closing(const gs_composite_t * this, gs_composite_t ** ppcte, gx_device *dev)
{
    gs_pdf14trans_t *pct0 = (gs_pdf14trans_t *)this;
    int op0 = pct0->params.pdf14_op;

    switch (op0) {
	default: return_error(gs_error_unregistered); /* Must not happen. */
	case PDF14_PUSH_DEVICE: 
	    return 0;
	case PDF14_POP_DEVICE:
	    if (*ppcte == NULL)
		return 0;
	    else {	
		int code = find_opening_op(PDF14_PUSH_DEVICE, ppcte, 1);

		if (code == 1)
		    return 5;
		return code;
	    }
	case PDF14_BEGIN_TRANS_GROUP: 
	    return 0;
	case PDF14_END_TRANS_GROUP:
	    if (*ppcte == NULL)
		return 2;
	    return find_opening_op(PDF14_BEGIN_TRANS_GROUP, ppcte, 6);
	case PDF14_BEGIN_TRANS_MASK: 
	    return 0;
        case PDF14_PUSH_TRANS_STATE:
            return 0;
        case PDF14_POP_TRANS_STATE:
            return 0;
        case PDF14_PUSH_SMASK_COLOR:
            return 0;
            break;
        case PDF14_POP_SMASK_COLOR:
            return 0;
            break;
	case PDF14_END_TRANS_MASK: 
	    if (*ppcte == NULL)
		return 2;
	    return find_opening_op(PDF14_BEGIN_TRANS_MASK, ppcte, 6);
	case PDF14_SET_BLEND_PARAMS: 
	    if (*ppcte == NULL)
		return 0;
	    /* hack : ignore csel - here it is always zero : */
	    return find_same_op(this, PDF14_SET_BLEND_PARAMS, ppcte);
    }
}

/*
 * Check whether a next operation is friendly to the compositor.
 */
static bool
c_pdf14trans_is_friendly(const gs_composite_t * this, byte cmd0, byte cmd1)
{
    gs_pdf14trans_t *pct0 = (gs_pdf14trans_t *)this;
    int op0 = pct0->params.pdf14_op;

    if (op0 == PDF14_PUSH_DEVICE || op0 == PDF14_END_TRANS_GROUP) {
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
	c_pdf14trans_clist_read_update,	   /* procs.composite_clist_reade_update */
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
	c_pdf14trans_clist_read_update,	   /* procs.composite_clist_reade_update */
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
send_pdf14trans(gs_imager_state	* pis, gx_device * dev,
    gx_device * * pcdev, gs_pdf14trans_params_t * pparams, gs_memory_t * mem)
{
    gs_composite_t * pct = NULL;
    int code;

    pparams->ctm = ctm_only(pis);
    code = gs_create_pdf14trans(&pct, pparams, mem);
    if (code < 0)
	return code;
    code = dev_proc(dev, create_compositor) (dev, pcdev, pct, pis, mem, NULL);

    gs_free_object(pis->memory, pct, "send_pdf14trans");

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

#define	pdf14_clist_procs(get_color_mapping_procs, get_color_comp_index,\
						encode_color, decode_color) \
{\
	NULL,				/* open */\
	gx_forward_get_initial_matrix,	/* get_initial_matrix */\
	gx_forward_sync_output,		/* sync_output */\
	gx_forward_output_page,		/* output_page */\
	gx_forward_close_device,	/* close_device */\
	encode_color,			/* rgb_map_rgb_color */\
	decode_color,			/* map_color_rgb */\
	gx_forward_fill_rectangle,	/* fill_rectangle */\
	gx_forward_tile_rectangle,	/* tile_rectangle */\
	gx_forward_copy_mono,		/* copy_mono */\
	gx_forward_copy_color,		/* copy_color */\
	NULL		,		/* draw_line - obsolete */\
	gx_forward_get_bits,		/* get_bits */\
	gx_forward_get_params,		/* get_params */\
	pdf14_put_params,		/* put_params */\
	encode_color,			/* map_cmyk_color */\
	gx_forward_get_xfont_procs,	/* get_xfont_procs */\
	gx_forward_get_xfont_device,	/* get_xfont_device */\
	NULL,				/* map_rgb_alpha_color */\
	gx_forward_get_page_device,	/* get_page_device */\
	gx_forward_get_alpha_bits,	/* get_alpha_bits */\
	NULL,				/* copy_alpha */\
	gx_forward_get_band,		/* get_band */\
	gx_forward_copy_rop,		/* copy_rop */\
	pdf14_clist_fill_path,		/* fill_path */\
	pdf14_clist_stroke_path,		/* stroke_path */\
	gx_forward_fill_mask,		/* fill_mask */\
	gx_forward_fill_trapezoid,	/* fill_trapezoid */\
	gx_forward_fill_parallelogram,	/* fill_parallelogram */\
	gx_forward_fill_triangle,	/* fill_triangle */\
	gx_forward_draw_thin_line,	/* draw_thin_line */\
	pdf14_clist_begin_image,	/* begin_image */\
	gx_forward_image_data,		/* image_data */\
	gx_forward_end_image,		/* end_image */\
	gx_forward_strip_tile_rectangle, /* strip_tile_rectangle */\
	gx_forward_strip_copy_rop,	/* strip_copy_rop, */\
	gx_forward_get_clipping_box,	/* get_clipping_box */\
	pdf14_clist_begin_typed_image,	/* begin_typed_image */\
	gx_forward_get_bits_rectangle,	/* get_bits_rectangle */\
	NULL,				/* map_color_rgb_alpha */\
	pdf14_clist_create_compositor,	/* create_compositor */\
	gx_forward_get_hardware_params,	/* get_hardware_params */\
	pdf14_clist_text_begin,		/* text_begin */\
	NULL,				/* finish_copydevice */\
	pdf14_begin_transparency_group,\
	pdf14_end_transparency_group,\
	pdf14_begin_transparency_mask,\
	pdf14_end_transparency_mask,\
	NULL,				/* discard_transparency_layer */\
	get_color_mapping_procs,	/* get_color_mapping_procs */\
	get_color_comp_index,		/* get_color_comp_index */\
	encode_color,			/* encode_color */\
	decode_color,			/* decode_color */\
	clist_pattern_manage,	/* pattern_manage */\
	NULL,				/* fill_rectangle_hl_color */\
	NULL,				/* include_color_space */\
	NULL,				/* fill_linear_color_scanline */\
	NULL,				/* fill_linear_color_trapezoid */\
	NULL,				/* fill_linear_color_triangle */\
	gx_forward_update_spot_equivalent_colors,	/* update spot */\
	gx_forward_ret_devn_params,	/* gx_forward_ret_devn_params */\
	gx_forward_fillpage,\
        pdf14_push_transparency_state,\
        pdf14_pop_transparency_state\
}

static	dev_proc_create_compositor(pdf14_clist_create_compositor);
static	dev_proc_create_compositor(pdf14_clist_forward_create_compositor);
static	dev_proc_fill_path(pdf14_clist_fill_path);
static	dev_proc_stroke_path(pdf14_clist_stroke_path);
static	dev_proc_text_begin(pdf14_clist_text_begin);
static	dev_proc_begin_image(pdf14_clist_begin_image);
static	dev_proc_begin_typed_image(pdf14_clist_begin_typed_image);

static	const gx_device_procs pdf14_clist_Gray_procs =
	pdf14_clist_procs(gx_default_DevGray_get_color_mapping_procs,
			gx_default_DevGray_get_color_comp_index,
			pdf14_encode_color,
			pdf14_decode_color);

static	const gx_device_procs pdf14_clist_RGB_procs =
	pdf14_clist_procs(gx_default_DevRGB_get_color_mapping_procs,
			gx_default_DevRGB_get_color_comp_index,
			pdf14_encode_color,
			pdf14_decode_color);

static	const gx_device_procs pdf14_clist_CMYK_procs =
	pdf14_clist_procs(gx_default_DevCMYK_get_color_mapping_procs,
			gx_default_DevCMYK_get_color_comp_index,
			pdf14_encode_color, pdf14_decode_color);

static	const gx_device_procs pdf14_clist_CMYKspot_procs =
	pdf14_clist_procs(pdf14_cmykspot_get_color_mapping_procs,
			pdf14_cmykspot_get_color_comp_index,
			pdf14_compressed_encode_color,
			pdf14_compressed_decode_color);

static	const gx_device_procs pdf14_clist_custom_procs =
	pdf14_clist_procs(gx_forward_get_color_mapping_procs,
			gx_forward_get_color_comp_index,
			gx_forward_encode_color,
			gx_forward_decode_color);

const pdf14_clist_device pdf14_clist_Gray_device = {
    std_device_color_stype_body(pdf14_clist_device, &pdf14_clist_Gray_procs,
			"pdf14clistgray", &st_pdf14_device,
			XSIZE, YSIZE, X_DPI, Y_DPI, 8, 255, 256),
    { 0 },			/* Procs */
    NULL,			/* target */
    { 0 },			/* devn_params - not used */
    &gray_pdf14_procs,
    &gray_blending_procs
};

const pdf14_clist_device pdf14_clist_RGB_device	= {
    std_device_color_stype_body(pdf14_clist_device, &pdf14_clist_RGB_procs,
			"pdf14clistRGB", &st_pdf14_device,
			XSIZE, YSIZE, X_DPI, Y_DPI, 24, 255, 256),
    { 0 },			/* Procs */
    NULL,			/* target */
    { 0 },			/* devn_params - not used */
    &rgb_pdf14_procs,
    &rgb_blending_procs
};

const pdf14_clist_device pdf14_clist_CMYK_device = {
    std_device_std_color_full_body_type(pdf14_clist_device,
			&pdf14_clist_CMYK_procs, "pdf14clistcmyk",
			&st_pdf14_device, XSIZE, YSIZE, X_DPI, Y_DPI, 32,
			0, 0, 0, 0, 0, 0),
    { 0 },			/* Procs */
    NULL,			/* target */
    { 0 },			/* devn_params - not used */
    &cmyk_pdf14_procs,
    &cmyk_blending_procs
};

const pdf14_clist_device pdf14_clist_CMYKspot_device = {
    std_device_part1_(pdf14_device, &pdf14_clist_CMYKspot_procs, "pdf14clistcmykspot", &st_pdf14_device, open_init_closed),
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
    std_device_part1_(pdf14_device, &pdf14_clist_CMYKspot_procs, "pdf14clistcustom", &st_pdf14_device, open_init_closed),
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
get_pdf14_clist_device_proto(gx_device * dev, pdf14_clist_device ** pdevproto,
	pdf14_clist_device * ptempdevproto, gs_imager_state * pis,
	const gs_pdf14trans_t * pdf14pct)
{
    pdf14_default_colorspace_t dev_cs =
		pdf14_determine_default_blend_cs(dev);

    switch (dev_cs) {
	case PDF14_DeviceGray:
	    *pdevproto = (pdf14_clist_device *)&pdf14_clist_Gray_device;
           /* We want gray to be single channel.  Low level 
               initialization of gray device prototype is 
               peculiar in that in dci_std_color_num_components
               the comment is
              "A device is monochrome only if it is bi-level"
              Here we want monochrome anytime we have a gray device.
              To avoid breaking things elsewhere, we will overide
              the prototype intialization here */
            *ptempdevproto = **pdevproto;
            ptempdevproto->color_info.max_components = 1;
            ptempdevproto->color_info.num_components = ptempdevproto->color_info.max_components;
            if (dev->color_info.icc_profile == '\0') {
                strcpy(&(ptempdevproto->color_info.icc_profile[0]), DEFAULT_GRAY_ICC);
            } else {
                strcpy(&(ptempdevproto->color_info.icc_profile[0]), dev->color_info.icc_profile);
            }
            *pdevproto = ptempdevproto;
	    break;
	case PDF14_DeviceRGB:
	    *pdevproto = (pdf14_clist_device *)&pdf14_clist_RGB_device;
            *ptempdevproto = **pdevproto;
            if (dev->color_info.icc_profile == '\0') {
                strcpy(&(ptempdevproto->color_info.icc_profile[0]), DEFAULT_RGB_ICC);
            } else {
                strcpy(&(ptempdevproto->color_info.icc_profile[0]), dev->color_info.icc_profile);
            }
            *pdevproto = ptempdevproto;
	    break;
	case PDF14_DeviceCMYK:
	    *pdevproto = (pdf14_clist_device *)&pdf14_clist_CMYK_device;
            *ptempdevproto = **pdevproto;
            if (dev->color_info.icc_profile == '\0') {
                strcpy(&(ptempdevproto->color_info.icc_profile[0]), DEFAULT_CMYK_ICC);
            } else {
                strcpy(&(ptempdevproto->color_info.icc_profile[0]), dev->color_info.icc_profile);
            }
            *pdevproto = ptempdevproto;
	    break;
	case PDF14_DeviceCMYKspot:
	    *pdevproto = (pdf14_clist_device *)&pdf14_clist_CMYKspot_device;
            *ptempdevproto = **pdevproto;
	    /*
	     * The number of components for the PDF14 device is the sum
	     * of the process components and the number of spot colors
	     * for the page.
	     */
	    if (pdf14pct->params.num_spot_colors >= 0) {
	        ptempdevproto->devn_params.page_spot_colors =
	    	    pdf14pct->params.num_spot_colors;
	        ptempdevproto->color_info.num_components =
	    	    ptempdevproto->devn_params.num_std_colorant_names +
	    	    pdf14pct->params.num_spot_colors;
	        if (ptempdevproto->color_info.num_components >
	    		ptempdevproto->color_info.max_components)
	    	    ptempdevproto->color_info.num_components =
	        	ptempdevproto->color_info.max_components;
	    }
            if (dev->color_info.icc_profile == '\0') {
                strcpy(&(ptempdevproto->color_info.icc_profile[0]), DEFAULT_CMYK_ICC);
            } else {
                strcpy(&(ptempdevproto->color_info.icc_profile[0]), dev->color_info.icc_profile);
            }
            *pdevproto = ptempdevproto;
	    break;
	case PDF14_DeviceCustom:
	    /*
	     * We are using the output device's process color model.  The
	     * color_info for the PDF 1.4 compositing device needs to match
	     * the output device.
	     */
	    *ptempdevproto = pdf14_clist_custom_device;
	    ptempdevproto->color_info = dev->color_info;
            /* The pdf14 device has to be 8 bit continuous tone. Force it */
            ptempdevproto->color_info.depth = 
                ptempdevproto->color_info.num_components * 8;
            ptempdevproto->color_info.max_gray = 255; 
            ptempdevproto->color_info.max_color = 255;
            ptempdevproto->color_info.dither_grays = 256;
            ptempdevproto->color_info.dither_colors = 256;
            if (dev->color_info.icc_profile == '\0') {
                strcpy(&(ptempdevproto->color_info.icc_profile[0]), DEFAULT_CMYK_ICC);
            } else {
                strcpy(&(ptempdevproto->color_info.icc_profile[0]), dev->color_info.icc_profile);
            }
	    *pdevproto = ptempdevproto;
	    break;
	default:			/* Should not occur */
	    return_error(gs_error_rangecheck);
    }
    return 0;
}

static	int
pdf14_create_clist_device(gs_memory_t *mem, gs_imager_state * pis,
				gx_device ** ppdev, gx_device * target,
				const gs_pdf14trans_t * pdf14pct)
{
    pdf14_clist_device * dev_proto;
    pdf14_clist_device * pdev, temp_dev_proto;
    int code;
    bool has_tags = (mem->gs_lib_ctx->BITTAG != GS_DEVICE_DOESNT_SUPPORT_TAGS);

    if_debug0('v', "[v]pdf14_create_clist_device\n");
    code = get_pdf14_clist_device_proto(target, &dev_proto,
		   		 &temp_dev_proto, pis, pdf14pct);
    if (code < 0)
	return code;
    code = gs_copydevice((gx_device **) &pdev,
			 (const gx_device *) dev_proto, mem);
    if (code < 0)
	return code;
    /* If we have a tag device then go ahead and do a special encoder
       decoder for the pdf14 device to make sure we maintain this
       information in the encoded color information.  We could use
       the target device's methods but the PDF14 device has to maintain
       8 bit color always and we could run into other issues if the number
       of colorants became large.  If we need to do compressed color with
       tags that will be a special project at that time */
    if (has_tags) {
        pdev->procs.encode_color = pdf14_encode_color_tag;
        pdev->color_info.depth += 8;
    }
    check_device_separable((gx_device *)pdev);
    gx_device_fill_in_procs((gx_device *)pdev);
    gs_pdf14_device_copy_params((gx_device *)pdev, target);
    rc_assign(pdev->target, target, "pdf14_create_clist_device");
    code = dev_proc((gx_device *) pdev, open_device) ((gx_device *) pdev);
    pdev->pclist_device = target;

    pdev->my_encode_color = pdev->procs.encode_color;
    pdev->my_decode_color = pdev->procs.decode_color;
    pdev->my_get_color_mapping_procs = pdev->procs.get_color_mapping_procs;
    pdev->my_get_color_comp_index = pdev->procs.get_color_comp_index;
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
pdf14_disable_clist_device(gs_memory_t *mem, gs_imager_state * pis,
				gx_device * dev)
{
    gx_device_forward * pdev = (gx_device_forward *)dev;
    gx_device * target = pdev->target;

    if_debug0('v', "[v]pdf14_disable_clist_device\n");

    /*
     * To disable the action of this device, we forward all device
     * procedures to the target except the create_compositor and copy
     * the target's color_info.
     */
    dev->color_info = target->color_info;
    pdf14_forward_device_procs(dev);
    set_dev_proc(dev, create_compositor, pdf14_clist_forward_create_compositor);
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
pdf14_recreate_clist_device(gs_memory_t	*mem, gs_imager_state *	pis,
		gx_device * dev, const gs_pdf14trans_t * pdf14pct)
{
    pdf14_clist_device * pdev = (pdf14_clist_device *)dev;
    gx_device * target = pdev->target;
    pdf14_clist_device * dev_proto;
    pdf14_clist_device temp_dev_proto;
    int code;

    if_debug0('v', "[v]pdf14_recreate_clist_device\n");
    /*
     * We will not use the entire prototype device but we will set the
     * color related info to match the prototype.
     */
    code = get_pdf14_clist_device_proto(target, &dev_proto,
		   		 &temp_dev_proto, pis, pdf14pct);
    if (code < 0)
	return code;
    pdev->color_info = dev_proto->color_info;
    pdev->procs = dev_proto->procs;
    gx_device_fill_in_procs(dev);
    check_device_separable((gx_device *)pdev);
    return code;
}

/*
 * Key names are normally C const strings.  However we need to create temp
 * parameter key names.  They only need to have a short life.  We need to
 * create a parameter list with the key names.  Then we will put the parameters
 * into the clist.  That process will create a permanent copy of the key
 * name.  At that point we can release our temp key names.
 */
typedef struct keyname_link_list_s {
	struct keyname_link_list_s * next;
	char * key_name;
    } keyname_link_list_t;

/*
 * The GC description for the keyname link list is being included for
 * completeness.  Since this structure is only temporary, this structure
 * should never be exposed to the GC.
 */
gs_private_st_ptrs2(st_keyname_link_list, keyname_link_list_t,
	       		"keyname_link_list", keyname_link_list_enum_ptrs,
		       	keyname_link_list_reloc_ptrs, next, key_name);

/* See comments before the definition of keyname_link_list_t */
static int
free_temp_keyname_list(gs_memory_t * mem, keyname_link_list_t * plist)
{
    keyname_link_list_t * pthis_elem;

    while (plist != NULL) {
	pthis_elem = plist;
	plist = plist->next;
	gs_free_object(mem, (byte *)pthis_elem, "free_temp_keyname_list");
    }
    return 0;
}

/* Put a data value into our 'string' */
#define put_data(pdata, value, count)\
    for(j = 0; j < count; j++)\
	*pdata++ = (byte)((value) >> (j * 8))

/*
 * Convert a compressed color list element into a set of device parameters.
 * Note:  This routine recursively calls itself.  As a result it can create
 * mulitple device parameters.  The parameters are 'strings'.  Actually the
 * data is stored in the strings as binary data.
 *
 * See comments before the definition of keyname_link_list_t
 */
static int
get_param_compressed_color_list_elem(pdf14_clist_device * pdev,
       	gs_param_list * plist, compressed_color_list_t * pcomp_list,
       	char * keyname, keyname_link_list_t ** pkeyname_list)
{
    int max_list_elem_size =
	    6 + NUM_ENCODE_LIST_ITEMS * sizeof(comp_bit_map_list_t);
    int code, i, j;
    byte * pdata;
    gs_param_string str;

    if (pcomp_list == NULL)	/* Exit if we don not have a list. */
	return 0;

    /* Allocate a string for temp data */
    pdata = gs_alloc_bytes(pdev->memory, max_list_elem_size,
		   		 "convert_compressed_color_list_elem");
    str.data = (const byte *)pdata;
    str.persistent = false;

    put_data(pdata, pcomp_list->num_sub_level_ptrs, 2);
    put_data(pdata, pcomp_list->first_bit_map, 2);

    /* . */
    for (i = pcomp_list->first_bit_map; i < NUM_ENCODE_LIST_ITEMS; i++) {
	put_data(pdata, pcomp_list->u.comp_data[i].num_comp, 2);
	put_data(pdata, pcomp_list->u.comp_data[i].num_non_solid_comp, 2);
	put_data(pdata, pcomp_list->u.comp_data[i].solid_not_100, 1);
        put_data(pdata, pcomp_list->u.comp_data[i].colorants, 
        			sizeof(pcomp_list->u.comp_data[i].colorants));
	if (pcomp_list->u.comp_data[i].num_comp !=
		       	pcomp_list->u.comp_data[i].num_non_solid_comp) {
            put_data(pdata, pcomp_list->u.comp_data[i].solid_colorants, 
        	sizeof(pcomp_list->u.comp_data[i].solid_colorants));
	}
    }
    str.size = pdata - str.data;
    code = param_write_string(plist, keyname, &str);
    gs_free_object(pdev->memory, (byte *)str.data,
		    "convert_compressed_color_list_elem");

    /* Convert the sub levels. */
    for (i = 0; i < pcomp_list->num_sub_level_ptrs; i++) {
	/*
	 * We generate a keyname for the sub level elements based upon
	 * the keyname for the current level.  See comments before the
	 * definition of keyname_link_list_t for comments about the lifetime
	 * of the keynames.
	 */
    	/* Allocate a string for the keyname */
	char * keyname_buf = (char *)gs_alloc_bytes(pdev->memory,
		strlen(keyname) + 10, "convert_compressed_color_list_elem");
	/*
	 * Allocate a link list element so we can keep track of the memory
	 * allocated to hold the keynames.
	 */
       	keyname_link_list_t * pkeyname_list_elem = 
	    gs_alloc_struct(pdev->memory, keyname_link_list_t,
		&st_keyname_link_list, "convert_compressed_color_list_elem");
	pkeyname_list_elem->next = *pkeyname_list;
	pkeyname_list_elem->key_name = keyname_buf;
	*pkeyname_list = pkeyname_list_elem;
	sprintf(keyname_buf, "%s_%d", keyname, i);
	get_param_compressed_color_list_elem(pdev, plist,
		       		pcomp_list->u.sub_level_ptrs[i], keyname_buf,
			       	pkeyname_list);
    }

    return 0;;
}
#undef put_data

/* Get data value from our 'string' */
#define get_data(pdata, value, count)\
    j = count - 1;\
    value = pdata[j--];\
    for(; j >= 0; j--)\
	value = (value << 8) | pdata[j];\
    pdata += count

/*
 * Retrieve a compressed color list from a set of device parameters.
 * Note:  This routine recursively calls itself.  As a result it can process
 * mulitple device parameters and create the entire compressed color list.
 * The parameters are 'strings'.  Actually the data is stored in the strings
 * as binary data.
 */
int
put_param_compressed_color_list_elem(gx_device * pdev,
    gs_param_list * plist, compressed_color_list_t ** pret_comp_list,
    char * keyname, int num_comps)
{
    int code, i, j;
    byte * pdata;
    gs_param_string str;
    compressed_color_list_t * pcomp_list;

    /* Check if the given keyname is present. */
    code = param_read_string(plist, keyname, &str);
    switch (code) {
      case 0:
	  break;	/* We have the given keyname, continue. */
      default:
	  param_signal_error(plist, keyname, code);
      case 1:
	  *pret_comp_list = NULL;
	  return 0;
    }
    /* Allocate a compressed color list element. */
    pdata = (byte *)str.data;
    pcomp_list = alloc_compressed_color_list_elem(pdev->memory, num_comps);
    get_data(pdata, pcomp_list->num_sub_level_ptrs, 2);
    get_data(pdata, pcomp_list->first_bit_map, 2);

    /* Read the bit maps */
    for (i = pcomp_list->first_bit_map; i < NUM_ENCODE_LIST_ITEMS; i++) {
	get_data(pdata, pcomp_list->u.comp_data[i].num_comp, 2);
	get_data(pdata, pcomp_list->u.comp_data[i].num_non_solid_comp, 2);
	get_data(pdata, pcomp_list->u.comp_data[i].solid_not_100, 1);
        get_data(pdata, pcomp_list->u.comp_data[i].colorants, 
        			sizeof(pcomp_list->u.comp_data[i].colorants));
	if (pcomp_list->u.comp_data[i].num_comp !=
		       	pcomp_list->u.comp_data[i].num_non_solid_comp) {
            get_data(pdata, pcomp_list->u.comp_data[i].solid_colorants, 
        		sizeof(pcomp_list->u.comp_data[i].solid_colorants));
	}
    }

    /* Get the sub levels. */
    for (i = 0; i < pcomp_list->num_sub_level_ptrs; i++) {
	char buff[50];
        compressed_color_list_t * sub_list_ptr;

	sprintf(buff, "%s_%d", keyname, i);
	put_param_compressed_color_list_elem(pdev, plist,
		       			&sub_list_ptr, buff, num_comps - 1);
	pcomp_list->u.sub_level_ptrs[i] = sub_list_ptr;
    }

    *pret_comp_list = pcomp_list;
    return 0;;
}
#undef get_data

/*
 * Convert a list of spot color names into a set of device parameters.
 * This is done to transfer information from the PDf14 clist writer
 * compositing device to the PDF14 clist reader compositing device.
 *
 * See comments before the definition of keyname_link_list_t
 */
static int
get_param_spot_color_names(pdf14_clist_device * pdev,
       	gs_param_list * plist, keyname_link_list_t ** pkeyname_list)
{
    int code, i;
    gs_param_string str;
    gs_separations * separations = &pdev->devn_params.separations;
    int num_spot_colors = separations->num_separations;

    if (num_spot_colors == 0)
	return 0;

    code = param_write_int(plist, PDF14NumSpotColorsParamName,
		   				 &num_spot_colors);
    for (i = 0; i < num_spot_colors; i++) {
	/*
	 * We generate a keyname for the spot color based upon the
	 * spot color number.  See comments before the definition of
	 * keyname_link_list_t for comments about the lifetime of the keynames.
	 */
    	/* Allocate a string for the keyname */
	char * keyname_buf = (char *)gs_alloc_bytes(pdev->memory,
		strlen("PDF14SpotName_") + 10, "get_param_spot_color_names");
	/*
	 * Allocate a link list element so we can keep track of the memory
	 * allocated to hold the keynames.
	 */
       	keyname_link_list_t * pkeyname_list_elem = 
	    gs_alloc_struct(pdev->memory, keyname_link_list_t,
		&st_keyname_link_list, "get_param_spot_color_names");
	pkeyname_list_elem->next = *pkeyname_list;
	pkeyname_list_elem->key_name = keyname_buf;
	*pkeyname_list = pkeyname_list_elem;
	sprintf(keyname_buf, "PDF14SpotName_%d", i);
	str.size = separations->names[i].size;
	str.data = separations->names[i].data;
	str.persistent = false;
	code = param_write_string(plist, keyname_buf, &str);
    }
    return 0;;
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

		sprintf(buff, "PDF14SpotName_%d", i);
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

static int
pdf14_clist_get_param_compressed_color_list(pdf14_device * p14dev)
{
    gx_device_clist_writer * cldev = (gx_device_clist_writer *)p14dev->pclist_device;
    gs_c_param_list param_list;
    keyname_link_list_t * pkeyname_list_head = NULL;
    int code;

    /*
     * If a put_params call fails, the device will be left in a closed
     * state, but higher-level code won't notice this fact.  We flag this by
     * setting permanent_error, which prevents writing to the command list.
     */

    if (cldev->permanent_error)
	return cldev->permanent_error;
    gs_c_param_list_write(&param_list, p14dev->memory);
    code = get_param_compressed_color_list_elem(p14dev,
		(gs_param_list *)&param_list,
		p14dev->devn_params.compressed_color_list,
       		(char *)PDF14CompressedColorListParamName, &pkeyname_list_head);
    get_param_spot_color_names(p14dev, (gs_param_list *)&param_list,
		   	 &pkeyname_list_head);
    if (code >= 0) {
        gx_device * tdev = p14dev->target;

	gs_c_param_list_read(&param_list);
	/* put_param_compressed_color_list_elem(p14dev,
		        (gs_param_list *)&param_list,
		        &pret_comp_list, "PDF14CompressedColorList",
		        TOP_ENCODED_LEVEL);
	 put_param_pdf14_spot_names(p14dev, (gs_param_list *)&param_list); */
#if 1
	code = dev_proc(tdev, put_params)(tdev, (gs_param_list *)&param_list);
#else
	/*
	 * This call will put the compressed color list info into the
	 * clist.  However there are two problems.  The info goes into
	 * the list at the end of the list.
	 */
	code = cmd_put_params(cldev, (gs_param_list *)&param_list );
#endif
    }
    gs_c_param_list_release(&param_list);
    free_temp_keyname_list(p14dev->memory, pkeyname_list_head);

    return code;
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
    int code = put_param_compressed_color_list_elem(pdev, plist,
	    &pdevn_params->pdf14_compressed_color_list,
	    (char *)PDF14CompressedColorListParamName, TOP_ENCODED_LEVEL);
    if (code >= 0)
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
pdf14_clist_create_compositor(gx_device	* dev, gx_device ** pcdev,
    const gs_composite_t * pct, gs_imager_state * pis, gs_memory_t * mem, gx_device *cdev)
{
    pdf14_clist_device * pdev = (pdf14_clist_device *)dev;
    int code;
    bool sep_target;

    /* We only handle a few PDF 1.4 transparency operations 4 */
    if (gs_is_pdf14trans_compositor(pct)) {
	const gs_pdf14trans_t * pdf14pct = (const gs_pdf14trans_t *) pct;

	switch (pdf14pct->params.pdf14_op) {
	    case PDF14_PUSH_DEVICE:
		/* Re-activate the PDF 1.4 compositor */
		pdev->saved_target_color_info = pdev->target->color_info;
		pdev->target->color_info = pdev->color_info;
		pdev->saved_target_encode_color = pdev->target->procs.encode_color;
		pdev->saved_target_decode_color = pdev->target->procs.decode_color;
		pdev->target->procs.encode_color = pdev->procs.encode_color = pdev->my_encode_color;
		pdev->target->procs.decode_color = pdev->procs.decode_color = pdev->my_decode_color;
		pdev->saved_target_get_color_mapping_procs = pdev->target->procs.get_color_mapping_procs;
		pdev->saved_target_get_color_comp_index = pdev->target->procs.get_color_comp_index;
		pdev->target->procs.get_color_mapping_procs = pdev->procs.get_color_mapping_procs = pdev->my_get_color_mapping_procs;
		pdev->target->procs.get_color_comp_index = pdev->procs.get_color_comp_index = pdev->my_get_color_comp_index;
		pdev->save_get_cmap_procs = pis->get_cmap_procs;
		pis->get_cmap_procs = pdf14_get_cmap_procs;
		gx_set_cmap_procs(pis, dev);
		code = pdf14_recreate_clist_device(mem, pis, dev, pdf14pct);
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
		    gs_composite_t pctemp = *pct;

		    pctemp.type = &gs_composite_pdf14trans_no_clist_writer_type;
		    code = dev_proc(pdev->target, create_compositor)
				(pdev->target, pcdev, &pctemp, pis, mem, cdev);
		    *pcdev = dev;
		    return code;
		}
	    case PDF14_POP_DEVICE:
		/* Restore the color_info for the clist device */
		pdev->target->color_info = pdev->saved_target_color_info;
		pdev->target->procs.encode_color = pdev->saved_target_encode_color;
		pdev->target->procs.decode_color = pdev->saved_target_decode_color;
		pdev->target->procs.get_color_mapping_procs = pdev->saved_target_get_color_mapping_procs;
		pdev->target->procs.get_color_comp_index = pdev->saved_target_get_color_comp_index;
		pis->get_cmap_procs = pdev->save_get_cmap_procs;
		gx_set_cmap_procs(pis, pdev->target);
		/*
		 * For spot colors we use a 'compressed encoding' for
		 * gx_color_index values.  Send the related data struct
		 * to the clist.
		 */
	        pdf14_clist_get_param_compressed_color_list(pdev);
		/* Disable the PDF 1.4 compositor */
		pdf14_disable_clist_device(mem, pis, dev);
		/*
		 * Make sure that the transfer funtions, etc. are current.
		 */
		code = cmd_put_color_mapping(
			(gx_device_clist_writer *)(pdev->target), pis);
		if (code < 0)
		    return code;
		break;
	    case PDF14_BEGIN_TRANS_GROUP:
		/*
		 * Keep track of any changes made in the blending parameters.
		 */
		pdev->text_knockout = pdf14pct->params.Knockout;
		pdev->blend_mode = pdf14pct->params.blend_mode;
		pdev->opacity = pdf14pct->params.opacity.alpha;
		pdev->shape = pdf14pct->params.shape.alpha;
		if (pdf14pct->params.Background_components != 0 && 
		    pdf14pct->params.Background_components != pdev->color_info.num_components)
		    return_error(gs_error_rangecheck);
                /* We need to update the clist writer device procs based upon the
                   the group color space.  For simplicity, the list item is created even if the
                   color space did not change */
                /* First store the current ones */
                pdf14_push_parent_color(dev, pis);
                /* Now update the device procs. Not 
                   if we have a sep target though */
                sep_target = (strcmp(pdev->dname, "pdf14clistcustom") == 0) || 
                    (strcmp(pdev->dname, "pdf14clistcmykspot") == 0);
                if (!sep_target)
                   code = pdf14_update_device_color_procs_push_c(dev,
                                  pdf14pct->params.group_color,
			          pdf14pct->params.icc_hash, pis,
                                  pdf14pct->params.iccprofile);
               /* Note that our initial device buffer may have had a different color space
                   than the first transparency group.  In such a case, we really should force
                   this first group to be isolated, anytime that the parent color space is 
                   different than its child group, it should be isolated, per the PDF spec */
                /* MJV */
             /*   if (code == 1){

                    isolated = true;
                    if_debug0('v', "[v]Transparency group color space change\n");

                } else {

                    isolated = ptgp->Isolated;

                } */
		break;
	    case PDF14_BEGIN_TRANS_MASK:

                /* We need to update the clist writer device procs based upon the
                   the group color space.  For simplicity, the list item is created even if the
                   color space did not change */
                /* First store the current ones */
                pdf14_push_parent_color(dev, pis);
                /* Now update the device procs */
               code = pdf14_update_device_color_procs_push_c(dev,
                                  pdf14pct->params.group_color,
			          pdf14pct->params.icc_hash, pis,
                                  pdf14pct->params.iccprofile);
               /* Also, if the BC is a value that may end up as
                  something other than transparent. We must
                  use the parent colors bounding box in
                  determining the range of bands in which
                  this mask can affect.  So, if needed change
                  the masks bounding box at this time */
		break;
            /* When we get a trans group pop, we need to update the color mapping 
                    procs if it was not a sep device */
	    case PDF14_END_TRANS_GROUP:
               /* We need to update the clist writer device procs based upon the
                   the group color space. */
                /* First restore our procs */
                sep_target = (strcmp(pdev->dname, "pdf14clistcustom") == 0) || 
                    (strcmp(pdev->dname, "pdf14clistcmykspot") == 0);
                if (!sep_target)
               code = pdf14_update_device_color_procs_pop_c(dev,pis);
                /* We always do this push and pop */
                pdf14_pop_parent_color(dev, pis);
                break;
	    case PDF14_END_TRANS_MASK:
                /* We need to update the clist writer device procs based upon the
                   the group color space. */
                /* First restore our procs */
               code = pdf14_update_device_color_procs_pop_c(dev,pis);
                /* Now pop the old one */
                pdf14_pop_parent_color(dev, pis);
                break;
            case PDF14_PUSH_TRANS_STATE:
                break;
            case PDF14_POP_TRANS_STATE:
                break;
            case PDF14_PUSH_SMASK_COLOR:
                code = pdf14_increment_smask_color(pis,dev);
                *pcdev = dev;
                return code;  /* Note, this are NOT put in the clist */
                break;
            case PDF14_POP_SMASK_COLOR:
                code = pdf14_decrement_smask_color(pis,dev);
                *pcdev = dev;
                return code;  /* Note, this are NOT put in the clist */
                break;
            default:
		break;		/* Pass remaining ops to target */
	}
    }
    code = dev_proc(pdev->target, create_compositor)
			(pdev->target, pcdev, pct, pis, mem, cdev);
    if (*pcdev != pdev->target)
	rc_assign(pdev->target, *pcdev, "pdf14_clist_create_compositor");
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
pdf14_clist_forward_create_compositor(gx_device	* dev, gx_device * * pcdev,
	const gs_composite_t * pct, gs_imager_state * pis,
	gs_memory_t * mem, gx_device *cdev)
{
    pdf14_device *pdev = (pdf14_device *)dev;
    gx_device * tdev = pdev->target;
    gx_device * ndev;
    int code = 0;

    *pcdev = dev;
    if (gs_is_pdf14trans_compositor(pct)) {
	const gs_pdf14trans_t * pdf14pct = (const gs_pdf14trans_t *) pct;

	if (pdf14pct->params.pdf14_op == PDF14_PUSH_DEVICE)
	    return pdf14_clist_create_compositor(dev, &ndev, pct, pis, mem, cdev);
	return 0;
    }
    code = dev_proc(tdev, create_compositor)(tdev, &ndev, pct, pis, mem, cdev);
    if (code < 0)
	return code;
    pdev->target = ndev;
    return 0;
}

/*
 * If any of the PDF 1.4 transparency blending parameters have changed, we
 * need to send them to the PDF 1.4 compositor on the output side of the clist.
 */
static	int
pdf14_clist_update_params(pdf14_clist_device * pdev, const gs_imager_state * pis)
{
    gs_pdf14trans_params_t params = { 0 };
    gx_device * pcdev;
    int changed = 0;
    int code = 0;

    params.pdf14_op = PDF14_SET_BLEND_PARAMS;
    if (pis->blend_mode != pdev->blend_mode) {
	changed |= PDF14_SET_BLEND_MODE;
	params.blend_mode = pdev->blend_mode = pis->blend_mode;
    }
    if (pis->text_knockout != pdev->text_knockout) {
	changed |= PDF14_SET_TEXT_KNOCKOUT;
	params.text_knockout = pdev->text_knockout = pis->text_knockout;
    }
    if (pis->shape.alpha != pdev->shape) {
	changed |= PDF14_SET_SHAPE_ALPHA;
	params.shape.alpha = pdev->shape = pis->shape.alpha;
    }
    if (pis->opacity.alpha != pdev->opacity) {
	changed |= PDF14_SET_OPACITY_ALPHA;
	params.opacity.alpha = pdev->opacity = pis->opacity.alpha;
    }
    if (pis->overprint != pdev->overprint) {
	changed |= PDF14_SET_OVERPRINT;
	params.overprint = pdev->overprint = pis->overprint;
    }
    if (pis->overprint_mode != pdev->overprint_mode) {
	changed |= PDF14_SET_OVERPRINT_MODE;
	params.overprint_mode = pdev->overprint_mode = pis->overprint_mode;
    }


    /*
     * Put parameters into a compositor parameter and then call the
     * create_compositor.  This will pass the data through the clist
     * to the PDF 1.4 transparency output device.  Note:  This action
     * never creates a new PDF 1.4 compositor and it does not change
     * the imager state.
     */
    if (changed != 0) {
	params.changed = changed;
	code = send_pdf14trans((gs_imager_state *)pis, (gx_device *)pdev,
					&pcdev, &params, pis->memory);
    }
    return code;
}


/*
 * fill_path routine for the PDF 1.4 transaprency compositor device for
 * writing the clist.
 */
static	int
pdf14_clist_fill_path(gx_device	*dev, const gs_imager_state *pis,
			   gx_path *ppath, const gx_fill_params *params,
			   const gx_drawing_color *pdcolor,
			   const gx_clip_path *pcpath)
{
    pdf14_clist_device * pdev = (pdf14_clist_device *)dev;
    gs_imager_state new_is = *pis;
    int code;
    gs_pattern2_instance_t *pinst = NULL;

    /*
     * Ensure that that the PDF 1.4 reading compositor will have the current
     * blending parameters.  This is needed since the fill_rectangle routines
     * do not have access to the imager state.  Thus we have to pass any
     * changes explictly.
     */
    code = pdf14_clist_update_params(pdev, pis);
    if (code < 0)
	return code;
    /* If we are doing a shading fill and we are in a tranparency
       group of a different color space, then we do not want to 
       do the shading in the device color space. It must occur in
       the source space.  To handle it in the device space would 
       require knowing all the nested transparency group color spaces
       as well as the transparency.  Some of the shading code
       ignores this, so we have to pass on the clist_writer device
       to enable proper mapping to the transparency group color space. */
    if (pdcolor != NULL && gx_dc_is_pattern2_color(pdcolor) && pdev->trans_group_parent_cmap_procs != NULL) {
 	pinst =
	    (gs_pattern2_instance_t *)pdcolor->ccolor.pattern;
           pinst->saved->has_transparency = true;
           /* The transparency color space operations are driven
              by the pdf14 clist writer device.  */
           pinst->saved->trans_device = dev;
    }
    update_lop_for_pdf14(&new_is);
    new_is.trans_device = dev;
    new_is.has_transparency = true;
    code = gx_forward_fill_path(dev, &new_is, ppath, params, pdcolor, pcpath);
    new_is.trans_device = NULL;
    new_is.has_transparency = false;
    if (pinst != NULL){
        pinst->saved->trans_device = NULL;
    }
    return code;
}

/*
 * stroke_path routine for the PDF 1.4 transaprency compositor device for
 * writing the clist.
 */
static	int
pdf14_clist_stroke_path(gx_device *dev,	const gs_imager_state *pis,
			     gx_path *ppath, const gx_stroke_params *params,
			     const gx_drawing_color *pdcolor,
			     const gx_clip_path *pcpath)
{
    pdf14_clist_device * pdev = (pdf14_clist_device *)dev;
    gs_imager_state new_is = *pis;
    int code;
    gs_pattern2_instance_t *pinst = NULL;

    /*
     * Ensure that that the PDF 1.4 reading compositor will have the current
     * blending parameters.  This is needed since the fill_rectangle routines
     * do not have access to the imager state.  Thus we have to pass any
     * changes explictly.
     */
    code = pdf14_clist_update_params(pdev, pis);
    if (code < 0)
	return code;
    /* If we are doing a shading stroke and we are in a tranparency
       group of a different color space, then we need to get the
       proper device information passed along so that we use the
       correct color procs and colorinfo about the transparency device
       and not the final target device */
    if (pdcolor != NULL && gx_dc_is_pattern2_color(pdcolor) && pdev->trans_group_parent_cmap_procs != NULL) {
 	pinst =
	    (gs_pattern2_instance_t *)pdcolor->ccolor.pattern;
           pinst->saved->has_transparency = true;
           /* The transparency color space operations are driven
              by the pdf14 clist writer device.  */
           pinst->saved->trans_device = dev;
    }

    update_lop_for_pdf14(&new_is);
    new_is.trans_device = dev;
    new_is.has_transparency = true;
    code = gx_forward_stroke_path(dev, &new_is, ppath, params, pdcolor, pcpath);
    new_is.trans_device = NULL;
    new_is.has_transparency = false;
    if (pinst != NULL){
        pinst->saved->trans_device = NULL;
    }
    return code;
}

/*
 * text_begin routine for the PDF 1.4 transaprency compositor device for
 * writing the clist.
 */
static	int
pdf14_clist_text_begin(gx_device * dev,	gs_imager_state	* pis,
		 const gs_text_params_t * text, gs_font * font,
		 gx_path * path, const gx_device_color * pdcolor,
		 const gx_clip_path * pcpath, gs_memory_t * memory,
		 gs_text_enum_t ** ppenum)
{
    pdf14_clist_device * pdev = (pdf14_clist_device *)dev;
    gs_text_enum_t *penum;
    int code;

    /*
     * Ensure that that the PDF 1.4 reading compositor will have the current
     * blending parameters.  This is needed since the fill_rectangle routines
     * do not have access to the imager state.  Thus we have to pass any
     * changes explictly.
     */
    code = pdf14_clist_update_params(pdev, pis);
    if (code < 0)
	return code;
    /* Pass text_begin to the target */
    code = gx_forward_text_begin(dev, pis, text, font, path,
				pdcolor, pcpath, memory, &penum);
    if (code < 0)
	return code;
    *ppenum = (gs_text_enum_t *)penum;
    return code;
}

static	int
pdf14_clist_begin_image(gx_device * dev,
		       const gs_imager_state * pis, const gs_image_t * pim,
		       gs_image_format_t format, const gs_int_rect * prect,
		       const gx_drawing_color * pdcolor,
		       const gx_clip_path * pcpath,
		       gs_memory_t * memory, gx_image_enum_common_t ** pinfo)
{
    pdf14_clist_device * pdev = (pdf14_clist_device *)dev;
    int code;

    /*
     * Ensure that that the PDF 1.4 reading compositor will have the current
     * blending parameters.  This is needed since the fill_rectangle routines
     * do not have access to the imager state.  Thus we have to pass any
     * changes explictly.
     */
    code = pdf14_clist_update_params(pdev, pis);
    if (code < 0)
	return code;
    /* Pass image to the target */
    code = gx_forward_begin_image(dev, pis, pim, format, prect,
					pdcolor, pcpath, memory, pinfo);
    if (code < 0)
        return gx_default_begin_image(dev, pis, pim, format, prect,
					pdcolor, pcpath, memory, pinfo);
    else return code;

}

static	int
pdf14_clist_begin_typed_image(gx_device	* dev, const gs_imager_state * pis,
			   const gs_matrix *pmat, const gs_image_common_t *pic,
			   const gs_int_rect * prect,
			   const gx_drawing_color * pdcolor,
			   const gx_clip_path * pcpath, gs_memory_t * mem,
			   gx_image_enum_common_t ** pinfo)
{
    pdf14_clist_device * pdev = (pdf14_clist_device *)dev;
    int code = 0;
    gs_imager_state * pis_noconst = (gs_imager_state *)pis; /* Break 'const'. */
    const gs_image_t *pim = (const gs_image_t *)pic;
    gx_image_enum *penum;
    gx_color_tile *ptile;
    gs_rect bbox_in, bbox_out;
    gs_transparency_group_params_t tgp;
    /*
     * Ensure that that the PDF 1.4 reading compositor will have the current
     * blending parameters.  This is needed since the fill_rectangle routines
     * do not have access to the imager state.  Thus we have to pass any
     * changes explictly.
     */
    code = pdf14_clist_update_params(pdev, pis);
    if (code < 0)
	return code;
    /* Pass image to the target */
    /* Do a quick change to the imager state so that if we can return with -1 in
       case the clist writer cannot handle this image itself.  In such a case,
       we want to make sure we dont use the target device.  I don't necc. like
       doing it this way.  Probably need to go back and do something a bit
       more elegant. */
    pis_noconst->has_transparency = true;
    pis_noconst->trans_device = dev;

    /* If we are filling an image mask with a pattern that has a transparency
       then we need to do some special handling */
    if (pim->ImageMask) {
        if (pdcolor != NULL && gx_dc_is_pattern1_color(pdcolor)) {
            if( gx_pattern1_get_transptr(pdcolor) != NULL){
                 if (dev->procs.begin_image != pdf14_clist_begin_image) {
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
                       setting in the imager state */
                    ptile->ttrans->blending_mode = pis->blend_mode;
                    /* Set the procs so that we use the proper filling method. */
                    /* Let the imaging stuff get set up */
                    code = gx_default_begin_typed_image(dev, pis, pmat, pic, 
                                            prect, pdcolor,pcpath, mem, pinfo);
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
                    if_debug0('v', "[v]Pushing special trans group for image\n");
                    tgp.Isolated = true;
                    tgp.Knockout = false;
                    tgp.mask_id = 0;
                    tgp.image_with_SMask = false;
                    tgp.idle = false;
                    tgp.iccprofile = NULL;
                    tgp.icc_hashcode = 0;
                    tgp.group_color_numcomps = ptile->ttrans->n_chan-1;
                    tgp.ColorSpace = NULL;
                    /* This will handle the compositor command */
                    gs_begin_transparency_group((gs_state *) pis_noconst, &tgp, 
                                                &bbox_out);
                    ptile->ttrans->image_render = penum->render;
                    penum->render = &pdf14_pattern_trans_render;
                    ptile->trans_group_popped = false;
                    pis_noconst->has_transparency = false;
                    pis_noconst->trans_device = NULL;
                    return code;
                }
            }
        }
    }
    /* This basically tries high level images for clist. If that fails
       then we do the default */
    code = gx_forward_begin_typed_image(dev, pis, pmat,
			    pic, prect, pdcolor, pcpath, mem, pinfo);  
    if (code < 0){
        code = gx_default_begin_typed_image(dev, pis, pmat, pic, prect,
					pdcolor, pcpath, mem, pinfo);
        pis_noconst->has_transparency = false;
        pis_noconst->trans_device = NULL;
        return code;
    } else {
        pis_noconst->has_transparency = false;
        pis_noconst->trans_device = NULL;
        return code;
    }
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
		gx_device ** pcdev, gs_imager_state * pis, gs_memory_t * mem)
{
    gx_device_clist_writer * const cdev = &((gx_device_clist *)dev)->writer;
    const gs_pdf14trans_t * pdf14pct = (const gs_pdf14trans_t *) pcte;
    pdf14_clist_device * p14dev;
    int code = 0;

    p14dev = (pdf14_clist_device *)(*pcdev);
    /* We only handle the push/pop operations */
    switch (pdf14pct->params.pdf14_op) {
	case PDF14_PUSH_DEVICE:
	    code = pdf14_create_clist_device(mem, pis, pcdev, dev, pdf14pct);
	    /*
	     * Set the color_info of the clist device to match the compositing
	     * device.  We will restore it when the compositor is popped.
	     * See pdf14_clist_create_compositor for the restore.  Do the
	     * same with the imager state's get_cmap_procs.  We do not want
	     * the imager state to use transfer functions on our color values.
	     * The transfer functions will be applied at the end after we
	     * have done our PDF 1.4 blend operations.
	     */

            /* 
	     * Also if the bit depth is not 8 per channel we need to adjust 
             * as all the pdf14 compositing code is for 8 bits per channel.
             * The clist writer device uses this information to make sure
             * the proper bit depth is written. If we are using compressed
             * color enconding, the color is written in a gx_color_index
             * even for more than 8 components.
	     */
	    p14dev = (pdf14_clist_device *)(*pcdev);
            if (cdev->clist_color_info.num_components * 8 != cdev->clist_color_info.depth &&
			p14dev->my_encode_color != pdf14_compressed_encode_color)
                cdev->clist_color_info.depth = cdev->clist_color_info.num_components * 8;

	    p14dev->saved_target_color_info = dev->color_info;
	    dev->color_info = (*pcdev)->color_info;
	    p14dev->saved_target_encode_color = dev->procs.encode_color;
	    p14dev->saved_target_decode_color = dev->procs.decode_color;
	    dev->procs.encode_color = p14dev->procs.encode_color = p14dev->my_encode_color;
	    dev->procs.decode_color = p14dev->procs.decode_color = p14dev->my_decode_color;
	    p14dev->saved_target_get_color_mapping_procs = dev->procs.get_color_mapping_procs;
	    p14dev->saved_target_get_color_comp_index = dev->procs.get_color_comp_index;
	    dev->procs.get_color_mapping_procs = p14dev->procs.get_color_mapping_procs = p14dev->my_get_color_mapping_procs;
	    dev->procs.get_color_comp_index = p14dev->procs.get_color_comp_index = p14dev->my_get_color_comp_index;
	    p14dev->save_get_cmap_procs = pis->get_cmap_procs;
	    pis->get_cmap_procs = pdf14_get_cmap_procs;
	    gx_set_cmap_procs(pis, dev);
	    return code;

	case PDF14_POP_DEVICE:
#	    if 0 /* Disabled because pdf14_clist_create_compositor does so. */
	    /*
	     * Ensure that the tranfer functions, etc.  are current before we
	     * dump our transparency image to the output device.
	     */
            if (pis->dev_ht)
                code = cmd_put_halftone((gx_device_clist_writer *)
                           (((pdf14_clist_device *)dev)->target), pis->dev_ht);
#	    else 
	    code = 0;
#	    endif
	    code = clist_writer_check_empty_cropping_stack(cdev);
	    break;

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

                if_debug1('v', "[v]c_pdf14trans_clist_write_update group mask_id=%d \n", cdev->mask_id);
	    }

	    break;

	case PDF14_END_TRANS_GROUP:
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

		if_debug1('v', "[v]c_pdf14trans_clist_write_update mask mask_id=%d \n", cdev->mask_id);
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
        case PDF14_PUSH_SMASK_COLOR:
            return 0;
            break;
        case PDF14_POP_SMASK_COLOR:
            return 0;
            break;
	default:
	    break;		/* do nothing for remaining ops */
    }
    *pcdev = dev;
    if (code < 0)
	return code;

    /* See c_pdf14trans_write, c_pdf14trans_adjust_ctm, and
       apply_create_compositor. */
    code = gs_imager_setmatrix(&cdev->imager_state, &pdf14pct->params.ctm);
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
		gx_device * tdev, gs_imager_state * pis, gs_memory_t * mem)
{
    pdf14_device * p14dev = (pdf14_device *)tdev;
    gs_pdf14trans_t * pdf14pct = (gs_pdf14trans_t *) pcte;
    gs_devn_params * pclist_devn_params;

    /*
     * We only handle the push/pop operations. Save and restore the color_info
     * field for the clist device.  (This is needed since the process color
     * model of the clist device needs to match the PDF 1.4 compositing
     * device.
     */
    switch (pdf14pct->params.pdf14_op) {
	case PDF14_PUSH_DEVICE:
#	    if 0 /* Disabled because *p14dev has no forwarding methods during the clist playback. 
		    This code is not executed while clist writing. */
	    p14dev->saved_target_color_info = cdev->color_info;
	    cdev->color_info = p14dev->color_info;
	     */
#	    endif
	    /*
	     * If we are blending using spot colors (i.e. the output device
	     * supports spot colors) then we need to transfer compressed
	     * color info from the clist PDF 1.4 compositing reader device
	     * to the clist writer PDF 1.4 compositing device.
	     * This info was transfered from that device to the output
	     * device as a set of device parameters.  However the clist
	     * reader PDF 1.4 compositing device did not exist when the
	     * device parameters were read from the clist.  So that info
	     * was buffered into the output device.
	     */
	    pclist_devn_params = dev_proc(cdev, ret_devn_params)(cdev);
	    if (pclist_devn_params != NULL) {
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
		    p14dev->color_info.num_components =
		        p14dev->devn_params.num_std_colorant_names +
		        p14dev->devn_params.page_spot_colors;
                }
 		/* Transfer the data for the compressed color encoding. */
		/* free_compressed_color_list(p14dev->memory,
			p14dev->devn_params.compressed_color_list); */
		p14dev->devn_params.compressed_color_list =
		    pclist_devn_params->pdf14_compressed_color_list;
		/* free_separation_names(p14dev->memory,
				 &p14dev->devn_params.separations); */
		p14dev->devn_params.separations =
		    pclist_devn_params->pdf14_separations;
		if ( num_comp != p14dev->color_info.num_components) {
		    dev_proc(tdev, open_device) (tdev);
		    dev_proc(tdev, open_device) (tdev);
		}
	    }
	    break;

	case PDF14_POP_DEVICE:
#	    if 0 /* Disabled because *p14dev has no forwarding methods during the clist playback. 
		    This code is not executed while clist writing. */
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
c_pdf14trans_get_cropping(const gs_composite_t *pcte, int *ry, int *rheight, int cropping_min, int cropping_max)
{
    gs_pdf14trans_t * pdf14pct = (gs_pdf14trans_t *) pcte;
    switch (pdf14pct->params.pdf14_op) {
	case PDF14_PUSH_DEVICE: return 0; /* Applies to all bands. */
	case PDF14_POP_DEVICE:  return 0; /* Applies to all bands. */

	case PDF14_BEGIN_TRANS_GROUP:

	    {	gs_int_rect rect;
		int code;

		code = pdf14_compute_group_device_int_rect(&pdf14pct->params.ctm, &pdf14pct->params.bbox, &rect);

                /* We have to crop this by the parent object.   */

                *ry = max(rect.p.y, cropping_min);
                *rheight = min(rect.q.y, cropping_max) - *ry;
                return 1; /* Push cropping. */

            }

        case PDF14_BEGIN_TRANS_MASK:
	    {	gs_int_rect rect;
		int code;

		code = pdf14_compute_group_device_int_rect(&pdf14pct->params.ctm, &pdf14pct->params.bbox, &rect);

                /* We have to crop this by the parent object and worry about the BC outside the range */

                if ( pdf14pct->params.GrayBackground == 1.0 ) {

                    /* In this case there will not be a background effect to worry about.  The mask will
                       not have any effect outside the bounding box.  This is NOT the default or common case. */

                    *ry = max(rect.p.y, cropping_min);
		    *rheight = min(rect.q.y, cropping_max) - *ry;
		    return 1; /* Push cropping. */

                }  else {

                    /* We need to make the soft mask range as large as the parent due to the
                       fact that the background color can have an impact OUTSIDE the bounding
                       box of the soft mask */

                    *ry = cropping_min;
		    *rheight = cropping_max - cropping_min;
		    return 1; /* Push cropping. */

                }

	    }

	case PDF14_END_TRANS_GROUP: return 2; /* Pop cropping. */
	case PDF14_END_TRANS_MASK: return 2;   /* Pop the cropping */ 

        case PDF14_PUSH_TRANS_STATE: return 3;
        case PDF14_POP_TRANS_STATE: return 3;

	case PDF14_SET_BLEND_PARAMS: return 3;
	case PDF14_PUSH_SMASK_COLOR: return 2; /* Pop cropping. */
	case PDF14_POP_SMASK_COLOR: return 2;   /* Pop the cropping */ 

    }
    return 0;
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
 * is only used if the output (target) device uses a CMYK plus spot color
 * process color model.
 *
 * Parameters:
 *   dev - pointer to device data structure.
 *   pname - pointer to name (zero termination not required)
 *   nlength - length of the name
 *
 * This routine returns a positive value (0 to n) which is the device colorant
 * number if the name is found.  It returns GX_DEVICE_COLOR_MAX_COMPONENTS if
 * the colorant is not being used due to a SeparationOrder device parameter.
 * It returns a negative value if not found.
 */
static int
pdf14_cmykspot_get_color_comp_index(gx_device * dev, const char * pname,
				int name_size, int component_type)
{
    pdf14_device * pdev = (pdf14_device *) dev;
    gx_device * tdev = pdev->target;
    gs_devn_params * pdevn_params = &pdev->devn_params;
    gs_separations * pseparations = &pdevn_params->separations;
    int comp_index;
    dev_proc_get_color_comp_index(*target_get_color_comp_index) = dev_proc(tdev, get_color_comp_index);

    /* The pdf14_clist_create_compositor may have set the color procs. We need the real target procs */
    if (target_get_color_comp_index == pdf14_cmykspot_get_color_comp_index)
	target_get_color_comp_index = ((pdf14_clist_device *)pdev)->saved_target_get_color_comp_index;
    /*
     * If this is not a separation name then simply forward it to the target
     * device.
     */
    if (component_type == NO_COMP_NAME_TYPE)
	return  (*target_get_color_comp_index)(tdev, pname, name_size, component_type);
    /*
     * Check if the component is in either the process color model list
     * or in the SeparationNames list.
     */
    comp_index = check_pcm_and_separation_names(dev, pdevn_params,
					pname, name_size, component_type);
    /*
     * Return the colorant number if we know this name.
     */
    if (comp_index >= 0)
	return comp_index; 
    /*
     * If we do not know this color, check if the output (target) device does.
     */
    comp_index = (*target_get_color_comp_index)(tdev, pname, name_size, component_type);
    /*
     * Ignore color if unknown to the output device or if color is not being
     * imaged due to the SeparationOrder device parameter.
     */
    if (comp_index < 0 || comp_index == GX_DEVICE_COLOR_MAX_COMPONENTS)
	return comp_index; 

    /*
     * This is a new colorant.  Add it to our list of colorants.
     */
    if (pseparations->num_separations < GX_DEVICE_COLOR_MAX_COMPONENTS - 1) {
	int sep_num = pseparations->num_separations++;
        int color_component_number;
	byte * sep_name;

	sep_name = gs_alloc_bytes(dev->memory,
			name_size, "pdf14_cmykspot_get_color_comp_index");
	memcpy(sep_name, pname, name_size);
	pseparations->names[sep_num].size = name_size;
	pseparations->names[sep_num].data = sep_name;
	color_component_number = sep_num + pdevn_params->num_std_colorant_names;
	if (color_component_number >= dev->color_info.num_components)
	    color_component_number = GX_DEVICE_COLOR_MAX_COMPONENTS;
	else
	    pdevn_params->separation_order_map[color_component_number] =
					       color_component_number;
        return color_component_number;
    }

    return GX_DEVICE_COLOR_MAX_COMPONENTS;
}

/* These functions keep track of when we are dealing with soft masks.
   In such a case, we set the default color profiles to ones that ensure
   proper soft mask rendering. */
static int 
pdf14_increment_smask_color(gs_imager_state * pis, gx_device * dev)
{
    pdf14_device * pdev = (pdf14_device *) dev;
    pdf14_smaskcolor_t *result;
    gsicc_smask_t *smask_profiles = pis->icc_manager->smask_profiles;

    /* See if we have profiles already in place */
    if (pdev->smaskcolor != NULL) {
        pdev->smaskcolor->ref_count++;
    } else {
        /* Allocate and swap out the current profiles.  The softmask
           profiles should already b in place */
        result = gs_alloc_struct(pdev->memory, pdf14_smaskcolor_t, 
                                &st_pdf14_smaskcolor, 
                                "pdf14_increment_smask_color");
        if (result == NULL ) return(-1);
        result->profiles = gsicc_new_iccsmask(pdev->memory);
        pdev->smaskcolor = result;
        /* Theoretically there should not be any reason to change ref counts 
            on the profiles for a well-formed PDF with clean soft mask groups.
           The only issue could be if the graphic state is popped while we
           are still within a softmask group. */
        result->profiles->smask_gray = pis->icc_manager->default_gray;
        result->profiles->smask_rgb = pis->icc_manager->default_rgb;
        result->profiles->smask_cmyk = pis->icc_manager->default_cmyk;
        pis->icc_manager->default_gray = smask_profiles->smask_gray;
        pis->icc_manager->default_rgb = smask_profiles->smask_rgb;
        pis->icc_manager->default_cmyk = smask_profiles->smask_cmyk;
        pdev->smaskcolor->ref_count = 1;
    }
    return(0);
}

static int
pdf14_decrement_smask_color(gs_imager_state * pis, gx_device * dev)
{
    pdf14_device * pdev = (pdf14_device *) dev;
    pdf14_smaskcolor_t *smaskcolor = pdev->smaskcolor;
    gsicc_manager_t *icc_manager = pis->icc_manager;    
    
    if (smaskcolor != NULL) {
        smaskcolor->ref_count--;
        if (smaskcolor->ref_count == 0) {
            /* Lets return the profiles and clean up */
            /* Decrement handled in pdf14_free_smask_color */
            icc_manager->default_gray = smaskcolor->profiles->smask_gray;
            icc_manager->default_rgb = smaskcolor->profiles->smask_rgb;
            icc_manager->default_cmyk = smaskcolor->profiles->smask_cmyk;
            pdf14_free_smask_color(pdev); 
        }
    }
    return(0);
}

static void 
pdf14_free_smask_color(pdf14_device * pdev) 
{
    gsicc_smask_t *profiles;

    if (pdev->smaskcolor != NULL) {
        if ( pdev->smaskcolor->profiles != NULL) {
            profiles = pdev->smaskcolor->profiles;
            /* Do not decrement the softmask enties.  They will remain
               in the icc_manager softmask member.  They were not 
               incremented when moved here */
            gs_free_object(pdev->memory, pdev->smaskcolor->profiles,
                        "pdf14_free_smask_color");
        }
        gs_free_object(pdev->memory, pdev->smaskcolor, "pdf14_free_smask_color");
        pdev->smaskcolor = NULL;
    }
}

