/*
  Copyright (C) 2001-2003 artofcode LLC.
  
  This software is provided AS-IS with no warranty, either express or
  implied.
  
  This software is distributed under license and may not be copied,
  modified or distributed except as expressly authorized under the terms
  of the license contained in the file LICENSE in this distribution.
  
  For more information about licensing, please refer to
  http://www.ghostscript.com/licensing/. For information on
  commercial licensing, go to http://www.artifex.com/licensing/ or
  contact Artifex Software, Inc., 101 Lucas Valley Road #110,
  San Rafael, CA  94903, U.S.A., +1(415)492-9861.

  Author: Raph Levien <raph@artofcode.com>
*/
/* $Id$ */
/* Device filter implementing PDF 1.4 imaging model */

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
#include "gzstate.h"
#include "gdevp14.h"
#include "gsovrc.h"

#define DUMP_TO_PNG

#ifdef DUMP_TO_PNG
#include "png_.h"
#endif

# define INCR(v) DO_NOTHING

/* Buffer stack data structure */

#define PDF14_MAX_PLANES 16

typedef struct pdf14_buf_s pdf14_buf;
typedef struct pdf14_ctx_s pdf14_ctx;

struct pdf14_buf_s {
    pdf14_buf *saved;

    bool isolated;
    bool knockout;
    byte alpha;
    byte shape;
    gs_blend_mode_t blend_mode;

    bool has_alpha_g;
    bool has_shape;

    gs_int_rect rect;
    /* Note: the traditional GS name for rowstride is "raster" */

    /* Data is stored in planar format. Order of planes is: pixel values,
       alpha, shape if present, alpha_g if present. */

    int rowstride;
    int planestride;
    int n_chan; /* number of pixel planes including alpha */
    int n_planes; /* total number of planes including alpha, shape, alpha_g */
    byte *data;

    gs_int_rect bbox;
};

struct pdf14_ctx_s {
    pdf14_buf *stack;
    pdf14_buf *maskbuf;
    gs_memory_t *memory;
    gs_int_rect rect;
    int n_chan;
};

/* GC procedures for buffer stack */

private
ENUM_PTRS_WITH(pdf14_buf_enum_ptrs, pdf14_buf *buf)
    return 0;
    case 0: return ENUM_OBJ(buf->saved);
    case 1: return ENUM_OBJ(buf->data);
ENUM_PTRS_END

private
RELOC_PTRS_WITH(pdf14_buf_reloc_ptrs, pdf14_buf *buf)
{
    RELOC_VAR(buf->saved);
    RELOC_VAR(buf->data);
}
RELOC_PTRS_END

gs_private_st_composite(st_pdf14_buf, pdf14_buf, "pdf14_buf",
			pdf14_buf_enum_ptrs, pdf14_buf_reloc_ptrs);

gs_private_st_ptrs2(st_pdf14_ctx, pdf14_ctx, "pdf14_ctx",
		    pdf14_ctx_enum_ptrs, pdf14_ctx_reloc_ptrs,
		    stack, maskbuf);

/* ------ The device descriptors ------ */

/*
 * Default X and Y resolution.
 */
#define X_DPI 72
#define Y_DPI 72

private int pdf14_open(gx_device * pdev);
private dev_proc_close_device(pdf14_close);
private int pdf14_output_page(gx_device * pdev, int num_copies, int flush);
private dev_proc_fill_rectangle(pdf14_fill_rectangle);
private dev_proc_fill_path(pdf14_fill_path);
private dev_proc_stroke_path(pdf14_stroke_path);
private dev_proc_begin_typed_image(pdf14_begin_typed_image);
private dev_proc_text_begin(pdf14_text_begin);
private dev_proc_create_compositor(pdf14_create_compositor);
private dev_proc_begin_transparency_group(pdf14_begin_transparency_group);
private dev_proc_end_transparency_group(pdf14_end_transparency_group);
private dev_proc_begin_transparency_mask(pdf14_begin_transparency_mask);
private dev_proc_end_transparency_mask(pdf14_end_transparency_mask);

#define XSIZE (int)(8.5 * X_DPI)	/* 8.5 x 11 inch page, by default */
#define YSIZE (int)(11 * Y_DPI)

/* 24-bit color. */

private const gx_device_procs pdf14_procs =
{
	pdf14_open,			/* open */
	NULL,	/* get_initial_matrix */
	NULL,	/* sync_output */
	pdf14_output_page,		/* output_page */
	pdf14_close,			/* close */
	gx_default_rgb_map_rgb_color,
	gx_default_rgb_map_color_rgb,
	pdf14_fill_rectangle,	/* fill_rectangle */
	NULL,	/* tile_rectangle */
	NULL,	/* copy_mono */
	NULL,	/* copy_color */
	NULL,	/* draw_line */
	NULL,	/* get_bits */
	NULL,   /* get_params */
	NULL,   /* put_params */
	NULL,	/* map_cmyk_color */
	NULL,	/* get_xfont_procs */
	NULL,	/* get_xfont_device */
	NULL,	/* map_rgb_alpha_color */
#if 0
	gx_page_device_get_page_device,	/* get_page_device */
#else
	NULL,   /* get_page_device */
#endif
	NULL,	/* get_alpha_bits */
	NULL,	/* copy_alpha */
	NULL,	/* get_band */
	NULL,	/* copy_rop */
	pdf14_fill_path,		/* fill_path */
	pdf14_stroke_path,	/* stroke_path */
	NULL,	/* fill_mask */
	NULL,	/* fill_trapezoid */
	NULL,	/* fill_parallelogram */
	NULL,	/* fill_triangle */
	NULL,	/* draw_thin_line */
	NULL,	/* begin_image */
	NULL,	/* image_data */
	NULL,	/* end_image */
	NULL,	/* strip_tile_rectangle */
	NULL,	/* strip_copy_rop, */
	NULL,	/* get_clipping_box */
	pdf14_begin_typed_image,	/* begin_typed_image */
	NULL,	/* get_bits_rectangle */
	NULL,	/* map_color_rgb_alpha */
	pdf14_create_compositor,	/* create_compositor */
	NULL,	/* get_hardware_params */
	pdf14_text_begin,	/* text_begin */
	NULL,	/* finish_copydevice */
	pdf14_begin_transparency_group,
	pdf14_end_transparency_group,
	pdf14_begin_transparency_mask,
	pdf14_end_transparency_mask
};

typedef struct pdf14_device_s {
    gx_device_common;

    pdf14_ctx *ctx;
    gx_device *target;

    const gx_color_map_procs *(*save_get_cmap_procs)(const gs_imager_state *,
						     const gx_device *);
} pdf14_device;

gs_private_st_composite_use_final(st_pdf14_device, pdf14_device, "pdf14_device",
				  pdf14_device_enum_ptrs, pdf14_device_reloc_ptrs,
			  gx_device_finalize);

const pdf14_device gs_pdf14_device = {
    std_device_color_stype_body(pdf14_device, &pdf14_procs, "pdf14",
				&st_pdf14_device,
				XSIZE, YSIZE, X_DPI, Y_DPI, 24, 255, 0),
    { 0 }
};

/* GC procedures */
private 
ENUM_PTRS_WITH(pdf14_device_enum_ptrs, pdf14_device *pdev) return 0;
case 0: return ENUM_OBJ(pdev->ctx);
case 1: ENUM_RETURN(gx_device_enum_ptr(pdev->target));
ENUM_PTRS_END
private RELOC_PTRS_WITH(pdf14_device_reloc_ptrs, pdf14_device *pdev)
{
    RELOC_VAR(pdev->ctx);
    pdev->target = gx_device_reloc_ptr(pdev->target, gcst);
}
RELOC_PTRS_END

/* ------ The device descriptors for the marking device ------ */

private dev_proc_fill_rectangle(pdf14_mark_fill_rectangle);
private dev_proc_fill_rectangle(pdf14_mark_fill_rectangle_ko_simple);

private const gx_device_procs pdf14_mark_procs =
{
	NULL,	/* open */
	NULL,	/* get_initial_matrix */
	NULL,	/* sync_output */
	NULL,	/* output_page */
	NULL,	/* close */
	gx_default_rgb_map_rgb_color,
	gx_default_rgb_map_color_rgb,
	NULL,	/* fill_rectangle */
	NULL,	/* tile_rectangle */
	NULL,	/* copy_mono */
	NULL,	/* copy_color */
	NULL,	/* draw_line */
	NULL,	/* get_bits */
	NULL,   /* get_params */
	NULL,   /* put_params */
	NULL,	/* map_cmyk_color */
	NULL,	/* get_xfont_procs */
	NULL,	/* get_xfont_device */
	NULL,	/* map_rgb_alpha_color */
#if 0
	gx_page_device_get_page_device,	/* get_page_device */
#else
	NULL,   /* get_page_device */
#endif
	NULL,	/* get_alpha_bits */
	NULL,	/* copy_alpha */
	NULL,	/* get_band */
	NULL,	/* copy_rop */
	NULL,	/* fill_path */
	NULL,	/* stroke_path */
	NULL,	/* fill_mask */
	NULL,	/* fill_trapezoid */
	NULL,	/* fill_parallelogram */
	NULL,	/* fill_triangle */
	NULL,	/* draw_thin_line */
	NULL,	/* begin_image */
	NULL,	/* image_data */
	NULL,	/* end_image */
	NULL,	/* strip_tile_rectangle */
	NULL,	/* strip_copy_rop, */
	NULL,	/* get_clipping_box */
	NULL,	/* begin_typed_image */
	NULL,	/* get_bits_rectangle */
	NULL,	/* map_color_rgb_alpha */
	NULL,	/* create_compositor */
	NULL,	/* get_hardware_params */
	NULL,	/* text_begin */
	NULL	/* finish_copydevice */
};

typedef struct pdf14_mark_device_s {
    gx_device_common;

    pdf14_device *pdf14_dev;
    float opacity;
    float shape;
    float alpha; /* alpha = opacity * shape */
    gs_blend_mode_t blend_mode;
} pdf14_mark_device;

gs_private_st_simple_final(st_pdf14_mark_device, pdf14_mark_device,
			   "pdf14_mark_device", gx_device_finalize);

const pdf14_mark_device gs_pdf14_mark_device = {
    std_device_color_stype_body(pdf14_mark_device, &pdf14_mark_procs,
				"pdf14_mark",
				&st_pdf14_mark_device,
				XSIZE, YSIZE, X_DPI, Y_DPI, 24, 255, 0),
    { 0 }
};

private const gx_color_map_procs *
    pdf14_get_cmap_procs(const gs_imager_state *, const gx_device *);

/* ------ Private definitions ------ */

/**
 * pdf14_buf_new: Allocate a new PDF 1.4 buffer.
 * @n_chan: Number of pixel channels including alpha.
 *
 * Return value: Newly allocated buffer, or NULL on failure.
 **/
private pdf14_buf *
pdf14_buf_new(gs_int_rect *rect, bool has_alpha_g, bool has_shape,
	       int n_chan,
	       gs_memory_t *memory)
{
    pdf14_buf *result;
    int rowstride = (rect->q.x - rect->p.x + 3) & -4;
    int height = (rect->q.y - rect->p.y);
    int n_planes = n_chan + (has_shape ? 1 : 0) + (has_alpha_g ? 1 : 0);
    int planestride;
    double dsize = (((double) rowstride) * height) * n_planes;

    if (dsize > (double)max_uint)
      return NULL;

    result = gs_alloc_struct(memory, pdf14_buf, &st_pdf14_buf,
			     "pdf14_buf_new");
    if (result == NULL)
	return result;

    result->isolated = false;
    result->knockout = false;
    result->has_alpha_g = has_alpha_g;
    result->has_shape = has_shape;
    result->rect = *rect;
    result->n_chan = n_chan;
    result->n_planes = n_planes;
    result->rowstride = rowstride;
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
    result->bbox.p.x = max_int;
    result->bbox.p.y = max_int;
    result->bbox.q.x = min_int;
    result->bbox.q.y = min_int;
    return result;
}

private void
pdf14_buf_free(pdf14_buf *buf, gs_memory_t *memory)
{
    gs_free_object(memory, buf->data, "pdf14_buf_free");
    gs_free_object(memory, buf, "pdf14_buf_free");
}

private pdf14_ctx *
pdf14_ctx_new(gs_int_rect *rect, int n_chan, gs_memory_t *memory)
{
    pdf14_ctx *result;
    pdf14_buf *buf;

    result = gs_alloc_struct(memory, pdf14_ctx, &st_pdf14_ctx,
			     "pdf14_ctx_new");
    if (result == NULL)
	return result;

    buf = pdf14_buf_new(rect, false, false, n_chan, memory);
    if (buf == NULL) {
	gs_free_object(memory, result, "pdf14_ctx_new");
	return NULL;
    }
    if_debug3('v', "[v]base buf: %d x %d, %d channels\n",
	      buf->rect.q.x, buf->rect.q.y, buf->n_chan);
    memset(buf->data, 0, buf->planestride * buf->n_planes);
    buf->saved = NULL;
    result->stack = buf;
    result->maskbuf = NULL;
    result->n_chan = n_chan;
    result->memory = memory;
    result->rect = *rect;
    return result;
}

private void
pdf14_ctx_free(pdf14_ctx *ctx)
{
    pdf14_buf *buf, *next;

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
private pdf14_buf *
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

private int
pdf14_push_transparency_group(pdf14_ctx *ctx, gs_int_rect *rect,
			      bool isolated, bool knockout,
			      byte alpha, byte shape,
			      gs_blend_mode_t blend_mode)
{
    pdf14_buf *tos = ctx->stack;
    pdf14_buf *buf, *backdrop;
    bool has_shape;

    /* todo: fix this hack, which makes all knockout groups isolated.
       For the vast majority of files, there won't be any visible
       effects, but it still isn't correct. The pixel compositing code
       for non-isolated knockout groups gets pretty hairy, which is
       why this is here. */
    if (knockout) isolated = true;

    has_shape = tos->has_shape || tos->knockout;

    buf = pdf14_buf_new(rect, !isolated, has_shape, ctx->n_chan, ctx->memory);
    if_debug3('v', "[v]push buf: %d x %d, %d channels\n", buf->rect.p.x, buf->rect.p.y, buf->n_chan);
    if (buf == NULL)
	return_error(gs_error_VMerror);
    buf->isolated = isolated;
    buf->knockout = knockout;
    buf->alpha = alpha;
    buf->shape = shape;
    buf->blend_mode = blend_mode;

    buf->saved = tos;
    ctx->stack = buf;

    backdrop = pdf14_find_backdrop_buf(ctx);
    if (backdrop == NULL) {
	memset(buf->data, 0, buf->planestride * (buf->n_chan +
						 (buf->has_shape ? 1 : 0)));
    } else {
	/* make copy of backdrop for compositing */
	byte *buf_plane = buf->data;
	byte *tos_plane = tos->data + buf->rect.p.x - tos->rect.p.x +
	    (buf->rect.p.y - tos->rect.p.y) * tos->rowstride;
	int width = buf->rect.q.x - buf->rect.p.x;
	int y0 = buf->rect.p.y;
	int y1 = buf->rect.q.y;
	int i;
	int n_chan_copy = buf->n_chan + (tos->has_shape ? 1 : 0);

	for (i = 0; i < n_chan_copy; i++) {
	    byte *buf_ptr = buf_plane;
	    byte *tos_ptr = tos_plane;
	    int y;

	    for (y = y0; y < y1; ++y) {
		memcpy (buf_ptr, tos_ptr, width); 
		buf_ptr += buf->rowstride;
		tos_ptr += tos->rowstride;
	    }
	    buf_plane += buf->planestride;
	    tos_plane += tos->planestride;
	}
	if (has_shape && !tos->has_shape)
	    memset (buf_plane, 0, buf->planestride);
    }

    return 0;
}

private int
pdf14_pop_transparency_group(pdf14_ctx *ctx)
{
    pdf14_buf *tos = ctx->stack;
    pdf14_buf *nos = tos->saved;
    pdf14_buf *maskbuf = ctx->maskbuf;
    int n_chan = ctx->n_chan;
    byte alpha = tos->alpha;
    byte shape = tos->shape;
    byte blend_mode = tos->blend_mode;
    int x0 = tos->rect.p.x;
    int y0 = tos->rect.p.y;
    int x1 = tos->rect.q.x;
    int y1 = tos->rect.q.y;
    byte *tos_ptr = tos->data;
    byte *nos_ptr = nos->data + x0 - nos->rect.p.x +
	(y0 - nos->rect.p.y) * nos->rowstride;
    byte *mask_ptr = NULL;
    int tos_planestride = tos->planestride;
    int nos_planestride = nos->planestride;
    int mask_planestride;
    byte mask_bg_alpha;
    int width = x1 - x0;
    int x, y;
    int i;
    byte tos_pixel[PDF14_MAX_PLANES];
    byte nos_pixel[PDF14_MAX_PLANES];
    bool tos_isolated = tos->isolated;
    bool nos_knockout = nos->knockout;
    byte *nos_alpha_g_ptr;
    int tos_shape_offset = n_chan * tos_planestride;
    int tos_alpha_g_offset = tos_shape_offset +
	(tos->has_shape ? tos_planestride : 0);
    int nos_shape_offset = n_chan * nos_planestride;
    bool nos_has_shape = nos->has_shape;

    if (nos == NULL)
	return_error(gs_error_rangecheck);

    rect_merge(nos->bbox, tos->bbox);

    if (nos->has_alpha_g)
	nos_alpha_g_ptr = nos_ptr + n_chan * nos_planestride;
    else
	nos_alpha_g_ptr = NULL;

    if (maskbuf != NULL) {
	mask_ptr = maskbuf->data + x0 - maskbuf->rect.p.x +
	    (y0 - maskbuf->rect.p.y) * maskbuf->rowstride;
	mask_planestride = maskbuf->planestride;
	mask_bg_alpha = maskbuf->alpha;
    }

    for (y = y0; y < y1; ++y) {
	for (x = 0; x < width; ++x) {
	    byte pix_alpha = alpha;
	    for (i = 0; i < n_chan; ++i) {
		tos_pixel[i] = tos_ptr[x + i * tos_planestride];
		nos_pixel[i] = nos_ptr[x + i * nos_planestride];
	    }

	    if (mask_ptr != NULL) {
		int mask_alpha = mask_ptr[x + 3 * mask_planestride];
		int tmp;
		byte mask;

		if (mask_alpha == 255)
		    mask = mask_ptr[x]; /* todo: rgba->mask */
		else if (mask_alpha == 0)
		    mask = mask_bg_alpha;
		else {
		    int t2 = (mask_ptr[x] - mask_bg_alpha) * mask_alpha + 0x80;
		    mask = mask_bg_alpha + ((t2 + (t2 >> 8)) >> 8);
		}
		tmp = pix_alpha * mask + 0x80;
		pix_alpha = (tmp + (tmp >> 8)) >> 8;
	    }

	    if (nos_knockout) {
		byte *nos_shape_ptr = nos_has_shape ?
		    &nos_ptr[x + nos_shape_offset] : NULL;
		byte tos_shape = tos_ptr[x + tos_shape_offset];

		art_pdf_composite_knockout_isolated_8(nos_pixel,
						      nos_shape_ptr,
						      tos_pixel,
						      n_chan - 1,
						      tos_shape,
						      pix_alpha, shape);
	    } else if (tos_isolated) {
		art_pdf_composite_group_8(nos_pixel, nos_alpha_g_ptr,
					  tos_pixel,
					  n_chan - 1,
					  pix_alpha, blend_mode);
	    } else {
		byte tos_alpha_g = tos_ptr[x + tos_alpha_g_offset];
		art_pdf_recomposite_group_8(nos_pixel, nos_alpha_g_ptr,
					    tos_pixel, tos_alpha_g,
					    n_chan - 1,
					    pix_alpha, blend_mode);
	    }
	    if (nos_has_shape) {
		nos_ptr[x + nos_shape_offset] =
		    art_pdf_union_mul_8 (nos_ptr[x + nos_shape_offset],
					 tos_ptr[x + tos_shape_offset],
					 shape);
	    }
	    
	    for (i = 0; i < n_chan; ++i) {
		nos_ptr[x + i * nos_planestride] = nos_pixel[i];
	    }
	    if (nos_alpha_g_ptr != NULL)
		++nos_alpha_g_ptr;
	}
	tos_ptr += tos->rowstride;
	nos_ptr += nos->rowstride;
	if (nos_alpha_g_ptr != NULL)
	    nos_alpha_g_ptr += nos->rowstride - width;
	if (mask_ptr != NULL)
	    mask_ptr += maskbuf->rowstride;
    }

    ctx->stack = nos;
    if_debug0('v', "[v]pop buf\n");
    pdf14_buf_free(tos, ctx->memory);
    if (maskbuf != NULL) {
	pdf14_buf_free(maskbuf, ctx->memory);
	ctx->maskbuf = NULL;
    }
    return 0;
}

private int
pdf14_push_transparency_mask(pdf14_ctx *ctx, gs_int_rect *rect, byte bg_alpha)
{
    pdf14_buf *buf;

    buf = pdf14_buf_new(rect, false, false, ctx->n_chan, ctx->memory);
    if (buf == NULL)
	return_error(gs_error_VMerror);

    /* fill in, but these values aren't really used */
    buf->isolated = true;
    buf->knockout = false;
    buf->alpha = bg_alpha;
    buf->shape = 0xff;
    buf->blend_mode = BLEND_MODE_Normal;

    buf->saved = ctx->stack;
    ctx->stack = buf;
    memset(buf->data, 0, buf->planestride * buf->n_chan);
    return 0;
}

private int
pdf14_pop_transparency_mask(pdf14_ctx *ctx)
{
    pdf14_buf *tos = ctx->stack;

    ctx->stack = tos->saved;
    ctx->maskbuf = tos;
    return 0;
}

private int
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
    pdev->ctx = pdf14_ctx_new(&rect, 4, dev->memory);
    if (pdev->ctx == NULL)
	return_error(gs_error_VMerror);

    return 0;
}

#ifdef DUMP_TO_PNG
/* Dumps a planar RGBA image to a PNG file. */
private int
dump_planar_rgba(const byte *buf, int width, int height, int rowstride, int planestride)
{
    int rowbytes = width << 2;
    byte *row = gs_malloc(rowbytes, 1, "png raster buffer");
    png_struct *png_ptr =
    png_create_write_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
    png_info *info_ptr =
    png_create_info_struct(png_ptr);
    const char *software_key = "Software";
    char software_text[256];
    png_text text_png;
    const byte *buf_ptr = buf;
    FILE *file;
    int code;
    int y;

	file = fopen ("c:\\temp\\tmp.png", "wb");

    if_debug0('v', "[v]pnga_output_page\n");

    if (row == 0 || png_ptr == 0 || info_ptr == 0) {
	code = gs_note_error(gs_error_VMerror);
	goto done;
    }
    /* set error handling */
    if (setjmp(png_ptr->jmpbuf)) {
	/* If we get here, we had a problem reading the file */
	code = gs_note_error(gs_error_VMerror);
	goto done;
    }

    code = 0;			/* for normal path */
    /* set up the output control */
    png_init_io(png_ptr, file);

    /* set the file information here */
    info_ptr->width = width;
    info_ptr->height = height;
    /* resolution is in pixels per meter vs. dpi */
    info_ptr->x_pixels_per_unit =
	(png_uint_32) (96.0 * (100.0 / 2.54));
    info_ptr->y_pixels_per_unit =
	(png_uint_32) (96.0 * (100.0 / 2.54));
    info_ptr->phys_unit_type = PNG_RESOLUTION_METER;
    info_ptr->valid |= PNG_INFO_pHYs;

    /* At present, only supporting 32-bit rgba */
    info_ptr->bit_depth = 8;
    info_ptr->color_type = PNG_COLOR_TYPE_RGB_ALPHA;

    /* add comment */
    sprintf(software_text, "%s %d.%02d", gs_product,
	    (int)(gs_revision / 100), (int)(gs_revision % 100));
    text_png.compression = -1;	/* uncompressed */
    text_png.key = (char *)software_key;	/* not const, unfortunately */
    text_png.text = software_text;
    text_png.text_length = strlen(software_text);
    info_ptr->text = &text_png;
    info_ptr->num_text = 1;

    /* write the file information */
    png_write_info(png_ptr, info_ptr);

    /* don't write the comments twice */
    info_ptr->num_text = 0;
    info_ptr->text = NULL;

    /* Write the contents of the image. */
    for (y = 0; y < height; ++y) {
	int x;

	for (x = 0; x < width; ++x) {
	    row[(x << 2)] = buf_ptr[x];
	    row[(x << 2) + 1] = buf_ptr[x + planestride];
	    row[(x << 2) + 2] = buf_ptr[x + planestride * 2];
	    row[(x << 2) + 3] = buf_ptr[x + planestride * 3];
	}
	png_write_row(png_ptr, row);
	buf_ptr += rowstride;
    }

    /* write the rest of the file */
    png_write_end(png_ptr, info_ptr);

  done:
    /* free the structures */
    png_destroy_write_struct(&png_ptr, &info_ptr);
    gs_free(row, rowbytes, 1, "png raster buffer");

    fclose (file);
    return code;
}
#endif


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
private int
pdf14_put_image(pdf14_device *pdev, gs_state *pgs, gx_device *target)
{
    int code;
    gs_image1_t image;
    gs_matrix pmat;
    gx_image_enum_common_t *info;
    int width = pdev->width;
    int height = pdev->height;
    gs_imager_state *pis = (gs_imager_state *)pgs;
    int y;
    pdf14_buf *buf = pdev->ctx->stack;

    int planestride = buf->planestride;
    byte *buf_ptr = buf->data;
    byte *linebuf;
    gs_color_space cs;

#ifdef DUMP_TO_PNG
    dump_planar_rgba(buf_ptr, width, height,
		     buf->rowstride, buf->planestride);
#endif

#if 0
    /* Set graphics state device to target, so that image can set up
       the color mapping properly. */
    rc_increment(pdev);
    gs_setdevice_no_init(pgs, target);
#endif

    /* Set color space to RGB, in preparation for sending an RGB image. */
    gs_setrgbcolor(pgs, 0, 0, 0);

    gs_cspace_init_DeviceRGB(&cs);
    gx_set_dev_color(pgs);
    gs_image_t_init_adjust(&image, &cs, false);
    image.ImageMatrix.xx = (float)width;
    image.ImageMatrix.yy = (float)height;
    image.Width = width;
    image.Height = height;
    image.BitsPerComponent = 8;
    pmat.xx = (float)width;
    pmat.xy = 0;
    pmat.yx = 0;
    pmat.yy = (float)height;
    pmat.tx = 0;
    pmat.ty = 0;
    code = dev_proc(target, begin_typed_image) (target,
						pis, &pmat,
						(gs_image_common_t *)&image,
						NULL, NULL, NULL,
						pgs->memory, &info);
    if (code < 0)
	return code;

    linebuf = gs_alloc_bytes(pdev->memory, width * 3, "pdf14_put_image");
    for (y = 0; y < height; y++) {
	gx_image_plane_t planes;
	int x;
	int rows_used;

	for (x = 0; x < width; x++) {
	    const byte bg_r = 0xff, bg_g = 0xff, bg_b = 0xff;
	    byte r, g, b, a;
	    int tmp;

	    /* composite RGBA pixel with over solid background */
	    a = buf_ptr[x + planestride * 3];

	    if ((a + 1) & 0xfe) {
		r = buf_ptr[x];
		g = buf_ptr[x + planestride];
		b = buf_ptr[x + planestride * 2];
		a ^= 0xff;

		tmp = ((bg_r - r) * a) + 0x80;
		r += (tmp + (tmp >> 8)) >> 8;
		linebuf[x * 3] = r;

		tmp = ((bg_g - g) * a) + 0x80;
		g += (tmp + (tmp >> 8)) >> 8;
		linebuf[x * 3 + 1] = g;

		tmp = ((bg_b - b) * a) + 0x80;
		b += (tmp + (tmp >> 8)) >> 8;
		linebuf[x * 3 + 2] = b;
	    } else if (a == 0) {
		linebuf[x * 3] = bg_r;
		linebuf[x * 3 + 1] = bg_g;
		linebuf[x * 3 + 2] = bg_b;
	    } else {
		r = buf_ptr[x];
		g = buf_ptr[x + planestride];
		b = buf_ptr[x + planestride * 2];
		linebuf[x * 3 + 0] = r;
		linebuf[x * 3 + 1] = g;
		linebuf[x * 3 + 2] = b;
	    }
	}

	planes.data = linebuf;
	planes.data_x = 0;
	planes.raster = width * 3;
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

    return code;
}

private int
pdf14_close(gx_device *dev)
{
    pdf14_device *pdev = (pdf14_device *)dev;

    if (pdev->ctx)
	pdf14_ctx_free(pdev->ctx);
    return 0;
}

private int
pdf14_output_page(gx_device *dev, int num_copies, int flush)
{
    /* todo: actually forward page */

    /* Hmm, actually I think the filter should be popped before the
       output_page operation. In that case, this should probably
       rangecheck. */
    return 0;
}

private void
pdf14_finalize(gx_device *dev)
{
    if_debug1('v', "[v]finalizing %lx\n", dev);
}

#define COPY_PARAM(p) dev->p = target->p
#define COPY_ARRAY_PARAM(p) memcpy(dev->p, target->p, sizeof(dev->p))

/*
 * Copy device parameters back from a target.  This copies all standard
 * parameters related to page size and resolution, but not any of the
 * color-related parameters, as the pdf14 device retains its own color
 * handling. This routine is parallel to gx_device_copy_params().
 */
private void
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
#undef COPY_ARRAY_PARAM
#undef COPY_PARAM
}

/**
 * pdf14_get_marking_device: Obtain a marking device.
 * @dev: Original device.
 * @pis: Imager state.
 *
 * The current implementation creates a marking device each time this
 * routine is called. A potential optimization is to cache a single
 * instance in the original device.
 *
 * Return value: Marking device, or NULL on error.
 **/
private gx_device *
pdf14_get_marking_device(gx_device *dev, const gs_imager_state *pis)
{
    pdf14_device *pdev = (pdf14_device *)dev;
    pdf14_buf *buf = pdev->ctx->stack;
    pdf14_mark_device *mdev;
    int code = gs_copydevice((gx_device **)&mdev,
			     (const gx_device *)&gs_pdf14_mark_device,
			     dev->memory);

    if (code < 0)
	return NULL;

    gx_device_fill_in_procs((gx_device *)mdev);
    mdev->pdf14_dev = pdev;
    mdev->opacity = pis->opacity.alpha;
    mdev->shape = pis->shape.alpha;
    mdev->alpha = pis->opacity.alpha * pis->shape.alpha;
    mdev->blend_mode = pis->blend_mode;

    if (buf->knockout) {
	fill_dev_proc((gx_device *)mdev, fill_rectangle,
		      pdf14_mark_fill_rectangle_ko_simple);
    } else {
	fill_dev_proc((gx_device *)mdev, fill_rectangle,
		      pdf14_mark_fill_rectangle);
    }

    if_debug1('v', "[v]creating %lx\n", mdev);
    gs_pdf14_device_copy_params((gx_device *)mdev, dev);
    mdev->finalize = pdf14_finalize;
    return (gx_device *)mdev;
}

private void
pdf14_release_marking_device(gx_device *marking_dev)
{
    rc_decrement_only(marking_dev, "pdf14_release_marking_device");
}

private int
pdf14_fill_path(gx_device *dev, const gs_imager_state *pis,
			   gx_path *ppath, const gx_fill_params *params,
			   const gx_drawing_color *pdcolor,
			   const gx_clip_path *pcpath)
{
    int code;
    gx_device *mdev = pdf14_get_marking_device(dev, pis);
    gs_imager_state new_is = *pis;

    if (mdev == 0)
	return_error(gs_error_VMerror);
    new_is.log_op |= lop_pdf14;
    code = gx_default_fill_path(mdev, &new_is, ppath, params, pdcolor, pcpath);
    pdf14_release_marking_device(mdev);

    return code;
}

private int
pdf14_stroke_path(gx_device *dev, const gs_imager_state *pis,
			     gx_path *ppath, const gx_stroke_params *params,
			     const gx_drawing_color *pdcolor,
			     const gx_clip_path *pcpath)
{
    int code;
    gx_device *mdev = pdf14_get_marking_device(dev, pis);
    gs_imager_state new_is = *pis;

    if (mdev == 0)
	return_error(gs_error_VMerror);
    new_is.log_op |= lop_pdf14;
    code = gx_default_stroke_path(mdev, &new_is, ppath, params, pdcolor,
				  pcpath);
    pdf14_release_marking_device(mdev);
    return code;
}

private int
pdf14_begin_typed_image(gx_device * dev, const gs_imager_state * pis,
			   const gs_matrix *pmat, const gs_image_common_t *pic,
			   const gs_int_rect * prect,
			   const gx_drawing_color * pdcolor,
			   const gx_clip_path * pcpath, gs_memory_t * mem,
			   gx_image_enum_common_t ** pinfo)
{
    gx_device *mdev = pdf14_get_marking_device(dev, pis);
    int code;

    if (mdev == 0)
	return_error(gs_error_VMerror);

    code = gx_default_begin_typed_image(mdev, pis, pmat, pic, prect, pdcolor,
					pcpath, mem, pinfo);

    /* We need to free the marking device on end of image. This probably
       means implementing our own image enum, which primarily forwards
       requests, but also frees the marking device on end_image. For
       now, we'll just leak this - it will get cleaned up by the GC. */
#if 0
    pdf14_release_marking_device(mdev);
#endif

    return code;
}

/*
 * The overprint compositor can be handled directly, so just set *pcdev = dev
 * and return. Since the gs_pdf14_device only supports the high-level routines
 * of the interface, don't bother trying to handle any other compositor.
 */
private int
pdf14_create_compositor(
    gx_device *             dev,
    gx_device **            pcdev,
    const gs_composite_t *  pct,
    const gs_imager_state * pis,
    gs_memory_t *           mem )
{
    if (gs_is_overprint_compositor(pct)) {
        *pcdev = dev;
        return 0;
    } else
        return gx_no_create_compositor(dev, pcdev, pct, pis, mem);
}

private int
pdf14_text_begin(gx_device * dev, gs_imager_state * pis,
		 const gs_text_params_t * text, gs_font * font,
		 gx_path * path, const gx_device_color * pdcolor,
		 const gx_clip_path * pcpath, gs_memory_t * memory,
		 gs_text_enum_t ** ppenum)
{
    int code;
    gx_device *mdev = pdf14_get_marking_device(dev, pis);
    gs_text_enum_t *penum;
    if (mdev == 0)
	return_error(gs_error_VMerror);
    if_debug0('v', "[v]pdf14_text_begin\n");
    code = gx_default_text_begin(mdev, pis, text, font, path, pdcolor, pcpath,
				 memory, &penum);
    if (code < 0)
	return code;
    rc_decrement_only(mdev, "pdf14_text_begin");
    *ppenum = (gs_text_enum_t *)penum;
    return code;
}

private int
pdf14_fill_rectangle(gx_device * dev,
		    int x, int y, int w, int h, gx_color_index color)
{
    if_debug4('v', "[v]pdf14_fill_rectangle, (%d, %d), %d x %d\n", x, y, w, h);
    return 0;
}


private int
pdf14_begin_transparency_group(gx_device *dev,
			      const gs_transparency_group_params_t *ptgp,
			      const gs_rect *pbbox,
			      gs_imager_state *pis,
			      gs_transparency_state_t **ppts,
			      gs_memory_t *mem)
{
    pdf14_device *pdev = (pdf14_device *)dev;
    double alpha = pis->opacity.alpha * pis->shape.alpha;
    int code;

    if_debug4('v', "[v]begin_transparency_group, I = %d, K = %d, alpha = %g, bm = %d\n",
	      ptgp->Isolated, ptgp->Knockout, alpha, pis->blend_mode);

    code = pdf14_push_transparency_group(pdev->ctx, &pdev->ctx->rect,
					 ptgp->Isolated, ptgp->Knockout,
					 (byte)floor (255 * alpha + 0.5),
					 (byte)floor (255 * pis->shape.alpha + 0.5),
					 pis->blend_mode);
    return code;
}

private int
pdf14_end_transparency_group(gx_device *dev,
			      gs_imager_state *pis,
			      gs_transparency_state_t **ppts)
{
    pdf14_device *pdev = (pdf14_device *)dev;
    int code;

    if_debug0('v', "[v]end_transparency_group\n");
    code = pdf14_pop_transparency_group(pdev->ctx);
    return code;
}

private int
pdf14_begin_transparency_mask(gx_device *dev,
			      const gs_transparency_mask_params_t *ptmp,
			      const gs_rect *pbbox,
			      gs_imager_state *pis,
			      gs_transparency_state_t **ppts,
			      gs_memory_t *mem)
{
    pdf14_device *pdev = (pdf14_device *)dev;
    byte bg_alpha = 0;

    if (ptmp->has_Background)
	bg_alpha = (int)(255 * ptmp->Background[0] + 0.5);
    if_debug1('v', "begin transparency mask, bg_alpha = %d!\n", bg_alpha);
    return pdf14_push_transparency_mask(pdev->ctx, &pdev->ctx->rect, bg_alpha);
}

private int
pdf14_end_transparency_mask(gx_device *dev,
			  gs_transparency_mask_t **pptm)
{
    pdf14_device *pdev = (pdf14_device *)dev;

    if_debug0('v', "end transparency mask!\n");
    return pdf14_pop_transparency_mask(pdev->ctx);
}

private int
pdf14_mark_fill_rectangle(gx_device * dev,
			 int x, int y, int w, int h, gx_color_index color)
{
    pdf14_mark_device *mdev = (pdf14_mark_device *)dev;
    pdf14_device *pdev = (pdf14_device *)mdev->pdf14_dev;
    pdf14_buf *buf = pdev->ctx->stack;
    int i, j, k;
    byte *line, *dst_ptr;
    byte src[PDF14_MAX_PLANES];
    byte dst[PDF14_MAX_PLANES];
    gs_blend_mode_t blend_mode = mdev->blend_mode;
    int rowstride = buf->rowstride;
    int planestride = buf->planestride;
    bool has_alpha_g = buf->has_alpha_g;
    bool has_shape = buf->has_shape;
    int shape_off = buf->n_chan * planestride;
    int alpha_g_off = shape_off + (has_shape ? planestride : 0);
    byte shape;

    src[0] = color >> 16;
    src[1] = (color >> 8) & 0xff;
    src[2] = color & 0xff;
    src[3] = (byte)floor (255 * mdev->alpha + 0.5);
    if (has_shape)
	shape = (byte)floor (255 * mdev->shape + 0.5);

    if (x < buf->rect.p.x) x = buf->rect.p.x;
    if (y < buf->rect.p.x) y = buf->rect.p.y;
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
	    for (k = 0; k < 4; ++k)
		dst[k] = dst_ptr[k * planestride];
	    art_pdf_composite_pixel_alpha_8(dst, src, 3, blend_mode);
	    for (k = 0; k < 4; ++k)
		dst_ptr[k * planestride] = dst[k];
	    if (has_alpha_g) {
		int tmp = (255 - dst_ptr[alpha_g_off]) * (255 - src[3]) + 0x80;
		dst_ptr[alpha_g_off] = 255 - ((tmp + (tmp >> 8)) >> 8);
	    }
	    if (has_shape) {
		int tmp = (255 - dst_ptr[shape_off]) * (255 - shape) + 0x80;
		dst_ptr[shape_off] = 255 - ((tmp + (tmp >> 8)) >> 8);
	    }
	    ++dst_ptr;
	}
	line += rowstride;
    }
    return 0;
}

private int
pdf14_mark_fill_rectangle_ko_simple(gx_device * dev,
				   int x, int y, int w, int h, gx_color_index color)
{
    pdf14_mark_device *mdev = (pdf14_mark_device *)dev;
    pdf14_device *pdev = (pdf14_device *)mdev->pdf14_dev;
    pdf14_buf *buf = pdev->ctx->stack;
    int i, j, k;
    byte *line, *dst_ptr;
    byte src[PDF14_MAX_PLANES];
    byte dst[PDF14_MAX_PLANES];
    int rowstride = buf->rowstride;
    int planestride = buf->planestride;
    int shape_off = buf->n_chan * planestride;
    bool has_shape = buf->has_shape;
    byte opacity;

    src[0] = color >> 16;
    src[1] = (color >> 8) & 0xff;
    src[2] = color & 0xff;
    src[3] = (byte)floor (255 * mdev->shape + 0.5);
    opacity = (byte)floor (255 * mdev->opacity + 0.5);

    if (x < buf->rect.p.x) x = buf->rect.p.x;
    if (y < buf->rect.p.x) y = buf->rect.p.y;
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
	    for (k = 0; k < 4; ++k)
		dst[k] = dst_ptr[k * planestride];
	    art_pdf_composite_knockout_simple_8(dst, has_shape ? dst_ptr + shape_off : NULL,
						src, 3, opacity);
	    for (k = 0; k < 4; ++k)
		dst_ptr[k * planestride] = dst[k];
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

private cmap_proc_gray(pdf14_cmap_gray_direct);
private cmap_proc_rgb(pdf14_cmap_rgb_direct);
private cmap_proc_cmyk(pdf14_cmap_cmyk_direct);
private cmap_proc_rgb_alpha(pdf14_cmap_rgb_alpha_direct);
private cmap_proc_separation(pdf14_cmap_separation_direct);
private cmap_proc_devicen(pdf14_cmap_devicen_direct);

private const gx_color_map_procs pdf14_cmap_many = {
     pdf14_cmap_gray_direct,
     pdf14_cmap_rgb_direct,
     pdf14_cmap_cmyk_direct,
     pdf14_cmap_rgb_alpha_direct,
     pdf14_cmap_separation_direct,
     pdf14_cmap_devicen_direct
    };

/**
 * Note: copied from gxcmap.c because it's inlined.
 **/
private inline void
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

private void
pdf14_cmap_gray_direct(frac gray, gx_device_color * pdc, const gs_imager_state * pis,
		 gx_device * dev, gs_color_select_t select)
{
    int i, ncomps = dev->color_info.num_components;
    frac cm_comps[GX_DEVICE_COLOR_MAX_COMPONENTS];
    gx_color_value cv[GX_DEVICE_COLOR_MAX_COMPONENTS];
    gx_color_index color;

    /* map to the color model */
    dev_proc(dev, get_color_mapping_procs)(dev)->map_gray(dev, gray, cm_comps);

    for (i = 0; i < ncomps; i++)
        cv[i] = frac2cv(cm_comps[i]);

    /* encode as a color index */
    color = dev_proc(dev, encode_color)(dev, cv);

    /* check if the encoding was successful; we presume failure is rare */
    if (color != gx_no_color_index)
        color_set_pure(pdc, color);
}


private void
pdf14_cmap_rgb_direct(frac r, frac g, frac b, gx_device_color * pdc,
     const gs_imager_state * pis, gx_device * dev, gs_color_select_t select)
{
    int i, ncomps = dev->color_info.num_components;
    frac cm_comps[GX_DEVICE_COLOR_MAX_COMPONENTS];
    gx_color_value cv[GX_DEVICE_COLOR_MAX_COMPONENTS];
    gx_color_index color;

    /* map to the color model */
    dev_proc(dev, get_color_mapping_procs)(dev)->map_rgb(dev, pis, r, g, b, cm_comps);

    for (i = 0; i < ncomps; i++)
        cv[i] = frac2cv(cm_comps[i]);

    /* encode as a color index */
    color = dev_proc(dev, encode_color)(dev, cv);

    /* check if the encoding was successful; we presume failure is rare */
    if (color != gx_no_color_index)
        color_set_pure(pdc, color);
}

private void
pdf14_cmap_cmyk_direct(frac c, frac m, frac y, frac k, gx_device_color * pdc,
     const gs_imager_state * pis, gx_device * dev, gs_color_select_t select)
{
    int i, ncomps = dev->color_info.num_components;
    frac cm_comps[GX_DEVICE_COLOR_MAX_COMPONENTS];
    gx_color_value cv[GX_DEVICE_COLOR_MAX_COMPONENTS];
    gx_color_index color;

    /* map to the color model */
    dev_proc(dev, get_color_mapping_procs)(dev)->map_cmyk(dev, c, m, y, k, cm_comps);

    for (i = 0; i < ncomps; i++)
        cv[i] = frac2cv(cm_comps[i]);

    color = dev_proc(dev, encode_color)(dev, cv);
    if (color != gx_no_color_index) 
	color_set_pure(pdc, color);
}

private void
pdf14_cmap_rgb_alpha_direct(frac r, frac g, frac b, frac alpha, gx_device_color * pdc,
     const gs_imager_state * pis, gx_device * dev, gs_color_select_t select)
{
    int i, ncomps = dev->color_info.num_components;
    frac cm_comps[GX_DEVICE_COLOR_MAX_COMPONENTS];
    gx_color_value cv_alpha, cv[GX_DEVICE_COLOR_MAX_COMPONENTS];
    gx_color_index color;

    /* map to the color model */
    dev_proc(dev, get_color_mapping_procs)(dev)->map_rgb(dev, pis, r, g, b, cm_comps);

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
    if (dev_proc(dev, map_rgb_alpha_color) != gx_default_map_rgb_alpha_color &&
         (cv_alpha = frac2cv(alpha)) != gx_max_color_value)
        color = dev_proc(dev, map_rgb_alpha_color)(dev, cv[0], cv[1], cv[2], cv_alpha);
    else
        color = dev_proc(dev, encode_color)(dev, cv);

    /* check if the encoding was successful; we presume failure is rare */
    if (color != gx_no_color_index)
        color_set_pure(pdc, color);
}


private void
pdf14_cmap_separation_direct(frac all, gx_device_color * pdc, const gs_imager_state * pis,
		 gx_device * dev, gs_color_select_t select)
{
    int i;
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

    /* encode as a color index */
    color = dev_proc(dev, encode_color)(dev, cv);

    /* check if the encoding was successful; we presume failure is rare */
    if (color != gx_no_color_index)
        color_set_pure(pdc, color);
}


private void
pdf14_cmap_devicen_direct(const frac * pcc, 
    gx_device_color * pdc, const gs_imager_state * pis, gx_device * dev,
    gs_color_select_t select)
{
    frac cm_comps[GX_DEVICE_COLOR_MAX_COMPONENTS];
    gx_color_value cv[GX_DEVICE_COLOR_MAX_COMPONENTS];
    gx_color_index color;

    /* map to the color model */
    map_components_to_colorants(pcc, &(pis->color_component_map), cm_comps);;

    /* encode as a color index */
    color = dev_proc(dev, encode_color)(dev, cv);

    /* check if the encoding was successful; we presume failure is rare */
    if (color != gx_no_color_index)
        color_set_pure(pdc, color);
}

private const gx_color_map_procs *
pdf14_get_cmap_procs(const gs_imager_state *pis, const gx_device * dev)
{
    /* The pdf14 marking device itself is always continuous tone. */
    return &pdf14_cmap_many;
}

private int
gs_pdf14_device_filter_push(gs_device_filter_t *self, gs_memory_t *mem,
			    gs_state *pgs, gx_device **pdev, gx_device *target)
{
    pdf14_device *p14dev;
    int code;

    code = gs_copydevice((gx_device **) &p14dev,
			 (const gx_device *) &gs_pdf14_device,
			 mem);
    if (code < 0)
	return code;

    gx_device_fill_in_procs((gx_device *)p14dev);

    gs_pdf14_device_copy_params((gx_device *)p14dev, target);

    rc_assign(p14dev->target, target, "gs_pdf14_device_filter_push");

    p14dev->save_get_cmap_procs = pgs->get_cmap_procs;
    pgs->get_cmap_procs = pdf14_get_cmap_procs;

    code = dev_proc((gx_device *) p14dev, open_device) ((gx_device *) p14dev);
    *pdev = (gx_device *) p14dev;
    return code;
}

private int
gs_pdf14_device_filter_prepop(gs_device_filter_t *self, gs_memory_t *mem,
			   gs_state *pgs, gx_device *dev)
{
    pdf14_device *p14dev = (pdf14_device *)dev;

    pgs->get_cmap_procs = p14dev->save_get_cmap_procs;
    return 0;
}

private int
gs_pdf14_device_filter_postpop(gs_device_filter_t *self, gs_memory_t *mem,
			   gs_state *pgs, gx_device *dev)
{
    pdf14_device *p14dev = (pdf14_device *)dev;
    gx_device *target = pgs->device;
    int code;

    code = pdf14_put_image(p14dev, pgs, target);
    if (code < 0)
	return code;

    code = dev_proc(dev, close_device) (dev);
    if (code < 0)
	return code;

    rc_decrement(p14dev->target, "gs_pdf14_device_filter_pop");

    gs_free_object(mem, self, "gs_pdf14_device_filter_pop");
    return 0;
}

int
gs_pdf14_device_filter(gs_device_filter_t **pdf, int depth, gs_memory_t *mem)
{
    gs_device_filter_t *df;

    df = gs_alloc_struct(mem, gs_device_filter_t,
			 &st_gs_device_filter, "gs_pdf14_device_filter");
    if (df == 0)
	return_error(gs_error_VMerror);
    df->push = gs_pdf14_device_filter_push;
    df->prepop = gs_pdf14_device_filter_prepop;
    df->postpop = gs_pdf14_device_filter_postpop;
    *pdf = df;
    return 0;
}
