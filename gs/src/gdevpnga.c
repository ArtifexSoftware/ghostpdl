/*
  Copyright (C) 2001 artofcode LLC.
  
  This file is part of AFPL Ghostscript.
  
  AFPL Ghostscript is distributed with NO WARRANTY OF ANY KIND.  No author or
  distributor accepts any responsibility for the consequences of using it, or
  for whether it serves any particular purpose or works at all, unless he or
  she says so in writing.  Refer to the Aladdin Free Public License (the
  "License") for full details.
  
  Every copy of AFPL Ghostscript must include a copy of the License, normally
  in a plain ASCII text file named PUBLIC.  The License grants you the right
  to copy, modify and redistribute AFPL Ghostscript, but only under certain
  conditions described in the License.  Among other things, the License
  requires that the copyright notice and this notice be preserved on all
  copies.

  Author: Raph Levien <raph@artofcode.com>
*/
/*$Id$ */
/* Test driver for PDF 1.4 transparency stuff */

#include "gdevprn.h"
#include "gdevpccm.h"
#include "gscdefs.h"
#include "gsdevice.h"
#include "gdevmem.h"
#include "gxblend.h"
#include "gxtext.h"

# define INCR(v) DO_NOTHING

#define PNG_INTERNAL
/*
 * libpng versions 1.0.3 and later allow disabling access to the stdxxx
 * files while retaining support for FILE * I/O.
 */
#define PNG_NO_CONSOLE_IO
/*
 * Earlier libpng versions require disabling FILE * I/O altogether.
 * This produces a compiler warning about no prototype for png_init_io.
 * The right thing will happen at link time, since the library itself
 * is compiled with stdio support.  Unfortunately, we can't do this
 * conditionally depending on PNG_LIBPNG_VER, because this is defined
 * in png.h.
 */
/*#define PNG_NO_STDIO*/
#include "png_.h"

/* ------ The device descriptors ------ */

/*
 * Default X and Y resolution.
 */
#define X_DPI 72
#define Y_DPI 72

private int pnga_open(gx_device * pdev);
private dev_proc_close_device(pnga_close);
private int pnga_output_page(gx_device * pdev, int num_copies, int flush);
private dev_proc_fill_rectangle(pnga_fill_rectangle);
private dev_proc_fill_path(pnga_fill_path);
private dev_proc_stroke_path(pnga_stroke_path);
private dev_proc_text_begin(pnga_text_begin);

#define XSIZE (8.5 * X_DPI)	/* 8.5 x 11 inch page, by default */
#define YSIZE (11 * Y_DPI)

/* 24-bit color. */

private const gx_device_procs pnga_procs =
{
	pnga_open,			/* open */
	NULL,	/* get_initial_matrix */
	NULL,	/* sync_output */
	pnga_output_page,		/* output_page */
	pnga_close,			/* close */
	gx_default_rgb_map_rgb_color,
	gx_default_rgb_map_color_rgb,
	pnga_fill_rectangle,	/* fill_rectangle */
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
	pnga_fill_path,		/* fill_path */
	pnga_stroke_path,	/* stroke_path */
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
	pnga_text_begin,	/* text_begin */
	NULL	/* finish_copydevice */
};

typedef struct pnga_device_s {
    gx_device_common;

    int rowstride;
    char *data;
} pnga_device;

gs_private_st_composite_use_final(st_pnga_device, pnga_device, "pnga_device",
				  pnga_device_enum_ptrs, pnga_device_reloc_ptrs,
				  gx_device_finalize);

const gx_device_printer gs_pnga_device = {
    std_device_color_stype_body(pnga_device, &pnga_procs, "pnga",
				&st_pnga_device,
				XSIZE, YSIZE, X_DPI, Y_DPI, 24, 255, 0),
    { 0 }
};

#if 0
prn_device(pnga_procs, "pnga",
	   DEFAULT_WIDTH_10THS, DEFAULT_HEIGHT_10THS,
	   X_DPI, Y_DPI,
	   0, 0, 0, 0,		/* margins */
	   24, pnga_print_page);
#endif

/* GC procedures */
private 
ENUM_PTRS_WITH(pnga_device_enum_ptrs, pnga_device *pdev) return 0;
case 0: return ENUM_OBJ(pdev->data);
ENUM_PTRS_END
private RELOC_PTRS_WITH(pnga_device_reloc_ptrs, pnga_device *pdev)
{
    RELOC_VAR(pdev->data);
}
RELOC_PTRS_END

/* ------ The device descriptors for the marking device ------ */

private dev_proc_fill_rectangle(pnga_mark_fill_rectangle);

private const gx_device_procs pnga_mark_procs =
{
	NULL,	/* open */
	NULL,	/* get_initial_matrix */
	NULL,	/* sync_output */
	NULL,	/* output_page */
	NULL,	/* close */
	gx_default_rgb_map_rgb_color,
	gx_default_rgb_map_color_rgb,
	pnga_mark_fill_rectangle,	/* fill_rectangle */
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

typedef struct pnga_mark_device_s {
    gx_device_common;

    pnga_device *pnga_dev;
    float alpha;
    gs_blend_mode_t blend_mode;
} pnga_mark_device;

gs_private_st_simple_final(st_pnga_mark_device, pnga_mark_device,
			   "pnga_mark_device", gx_device_finalize);

const gx_device_printer gs_pnga_mark_device = {
    std_device_color_stype_body(pnga_mark_device, &pnga_mark_procs,
				"pnga_mark",
				&st_pnga_mark_device,
				XSIZE, YSIZE, X_DPI, Y_DPI, 24, 255, 0),
    { 0 }
};

typedef struct pnga_text_enum_s {
    gs_text_enum_common;
    gs_text_enum_t *target_enum;
} pnga_text_enum_t;
extern_st(st_gs_text_enum);
gs_private_st_suffix_add1(st_pnga_text_enum, pnga_text_enum_t,
			  "pnga_text_enum_t", pnga_text_enum_enum_ptrs,
			  pnga_text_enum_reloc_ptrs, st_gs_text_enum,
			  target_enum);

/* ------ Private definitions ------ */

private int
pnga_open(gx_device * dev)
{
    pnga_device * pdev = (pnga_device *)dev;
    int rowstride;

    if_debug2('v', "[v]pnga_open: width = %d, height = %d\n",
	     dev->width, dev->height);
    rowstride = dev->width * 3 << 2;
    pdev->rowstride = rowstride;
    pdev->data = gs_alloc_bytes(dev->memory, rowstride * dev->height, "pnga_open");
    memset(pdev->data, 0, rowstride * dev->height);
    return 0;
}

private int
pnga_close(gx_device * dev)
{
    pnga_device * pdev = (pnga_device *)dev;

    if (pdev->data)
	gs_free_object(dev->memory, pdev->data, "pnga_close");
    return 0;
}

private int
pnga_output_page(gx_device * dev, int num_copies, int flush)
{
    pnga_device * pdev = (pnga_device *)dev;
    gs_memory_t *mem = dev->memory;
    int rowstride = pdev->rowstride;

    /* PNG structures */
    byte *row = gs_alloc_bytes(mem, rowstride, "png raster buffer");
    png_struct *png_ptr =
    png_create_write_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
    png_info *info_ptr =
    png_create_info_struct(png_ptr);
    int height = dev->height;
    int depth = dev->color_info.depth;
    int y;
    int code;			/* return code */
    const char *software_key = "Software";
    char software_text[256];
    png_text text_png;
    FILE *file;

    file = fopen ("/tmp/tmp.png", "wb"); /* todo: suck from OutputFile */

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
    info_ptr->width = dev->width;
    info_ptr->height = dev->height;
    /* resolution is in pixels per meter vs. dpi */
    info_ptr->x_pixels_per_unit =
	(png_uint_32) (dev->HWResolution[0] * (100.0 / 2.54));
    info_ptr->y_pixels_per_unit =
	(png_uint_32) (dev->HWResolution[1] * (100.0 / 2.54));
    info_ptr->phys_unit_type = PNG_RESOLUTION_METER;
    info_ptr->valid |= PNG_INFO_pHYs;

    /* At present, only supporting 32-bit rgba */
    info_ptr->bit_depth = 8;
    info_ptr->color_type = PNG_COLOR_TYPE_RGB_ALPHA;

    /* set the palette if there is one */
    if (info_ptr->color_type == PNG_COLOR_TYPE_PALETTE) {
	int i;
	int num_colors = 1 << depth;
	gx_color_value rgb[3];

	info_ptr->palette =
	    (void *)gs_alloc_bytes(mem, 256 * sizeof(png_color),
				   "png palette");
	if (info_ptr->palette == 0) {
	    code = gs_note_error(gs_error_VMerror);
	    goto done;
	}
	info_ptr->num_palette = num_colors;
	info_ptr->valid |= PNG_INFO_PLTE;
	for (i = 0; i < num_colors; i++) {
	    (*dev_proc(dev, map_color_rgb)) ((gx_device *) dev,
					      (gx_color_index) i, rgb);
	    info_ptr->palette[i].red = gx_color_value_to_byte(rgb[0]);
	    info_ptr->palette[i].green = gx_color_value_to_byte(rgb[1]);
	    info_ptr->palette[i].blue = gx_color_value_to_byte(rgb[2]);
	}
    }
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
    for (y = 0; y < height; y++) {
	byte *rowptr;
	rowptr = pdev->data + y * rowstride;
	png_write_rows(png_ptr, &rowptr, 1);
    }

    /* write the rest of the file */
    png_write_end(png_ptr, info_ptr);

    /* if you alloced the palette, free it here */
    gs_free_object(mem, info_ptr->palette, "png palette");

  done:
    /* free the structures */
    png_destroy_write_struct(&png_ptr, &info_ptr);
    gs_free_object(mem, row, "png raster buffer");

    fclose (file);

    return code;
}

private void
pnga_finalize(gx_device *dev)
{
    dlprintf1("finalizing %x\n", dev);
}

/**
 * pnga_get_marking_device: Obtain a marking device.
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
pnga_get_marking_device(gx_device *dev, const gs_imager_state *pis)
{
    pnga_mark_device *mdev;
    int code = gs_copydevice((gx_device **)&mdev,
			     (const gx_device *)&gs_pnga_mark_device,
			     dev->memory);

    if (code < 0)
	return NULL;

    gx_device_fill_in_procs((gx_device *)mdev);
    mdev->pnga_dev = (pnga_device *)dev;
    mdev->alpha = pis->opacity.alpha * pis->shape.alpha;
    mdev->blend_mode = pis->blend_mode;
    dlprintf1("creating %x\n", mdev);
    mdev->finalize = pnga_finalize;
    return (gx_device *)mdev;
}

private void
pnga_release_marking_device(gx_device *marking_dev)
{
    rc_decrement_only(marking_dev, "pnga_release_marking_device");
}

private int
pnga_fill_path(gx_device *dev, const gs_imager_state *pis,
			   gx_path *ppath, const gx_fill_params *params,
			   const gx_drawing_color *pdcolor,
			   const gx_clip_path *pcpath)
{
    int code;
    gx_device *mdev = pnga_get_marking_device(dev, pis);

    if (mdev == 0)
	return_error(gs_error_VMerror);
    code = gx_default_fill_path(mdev, pis, ppath, params, pdcolor, pcpath);
    pnga_release_marking_device(mdev);
    return code;
}

private int
pnga_stroke_path(gx_device *dev, const gs_imager_state *pis,
			     gx_path *ppath, const gx_stroke_params *params,
			     const gx_drawing_color *pdcolor,
			     const gx_clip_path *pcpath)
{
    int code;
    gx_device *mdev = pnga_get_marking_device(dev, pis);

    if (mdev == 0)
	return_error(gs_error_VMerror);
    code = gx_default_stroke_path(mdev, pis, ppath, params, pdcolor, pcpath);
    pnga_release_marking_device(mdev);
    return code;
}

private int
pnga_text_resync(gs_text_enum_t *pte, const gs_text_enum_t *pfrom)
{
    pnga_text_enum_t *const penum = (pnga_text_enum_t *)pte;

    if ((pte->text.operation ^ pfrom->text.operation) & ~TEXT_FROM_ANY)
	return_error(gs_error_rangecheck);
    if (penum->target_enum) {
	int code = gs_text_resync(penum->target_enum, pfrom);

	if (code < 0)
	    return code;
    }
    pte->text = pfrom->text;
    gs_text_enum_copy_dynamic(pte, pfrom, false);
    return 0;
}

private int
pnga_text_process(gs_text_enum_t *pte)
{
    pnga_text_enum_t *const penum = (pnga_text_enum_t *)pte;
    int code;

    code = gs_text_process(penum->target_enum);
    gs_text_enum_copy_dynamic(pte, penum->target_enum, true);
    return code;
}

private bool
pnga_text_is_width_only(const gs_text_enum_t *pte)
{
    const pnga_text_enum_t *const penum = (const pnga_text_enum_t *)pte;

    if (penum->target_enum)
	return gs_text_is_width_only(penum->target_enum);
    return false;
}

private int
pnga_text_current_width(const gs_text_enum_t *pte, gs_point *pwidth)
{
    const pnga_text_enum_t *const penum = (const pnga_text_enum_t *)pte;

    if (penum->target_enum)
	return gs_text_current_width(penum->target_enum, pwidth);
    return_error(gs_error_rangecheck); /* can't happen */
}

private int
pnga_text_set_cache(gs_text_enum_t *pte, const double *pw,
		   gs_text_cache_control_t control)
{
    pnga_text_enum_t *const penum = (pnga_text_enum_t *)pte;

    if (penum->target_enum)
	return gs_text_set_cache(penum->target_enum, pw, control);
    return_error(gs_error_rangecheck); /* can't happen */
}

private int
pnga_text_retry(gs_text_enum_t *pte)
{
    pnga_text_enum_t *const penum = (pnga_text_enum_t *)pte;

    if (penum->target_enum)
	return gs_text_retry(penum->target_enum);
    return_error(gs_error_rangecheck); /* can't happen */
}

private void
pnga_text_release(gs_text_enum_t *pte, client_name_t cname)
{
    pnga_text_enum_t *const penum = (pnga_text_enum_t *)pte;

    if (penum->target_enum) {
	gs_text_release(penum->target_enum, cname);
	penum->target_enum = 0;
    }
    gx_default_text_release(pte, cname);
}

private const gs_text_enum_procs_t pnga_text_procs = {
    pnga_text_resync, pnga_text_process,
    pnga_text_is_width_only, pnga_text_current_width,
    pnga_text_set_cache, pnga_text_retry,
    pnga_text_release
};

private int
pnga_text_begin(gx_device * dev, gs_imager_state * pis,
		 const gs_text_params_t * text, gs_font * font,
		 gx_path * path, const gx_device_color * pdcolor,
		 const gx_clip_path * pcpath, gs_memory_t * memory,
		 gs_text_enum_t ** ppenum)
{
    int code;
    gx_device *mdev = pnga_get_marking_device(dev, pis);
    pnga_text_enum_t *penum;
    gs_text_enum_t *target_enum;

    if (mdev == 0)
	return_error(gs_error_VMerror);
    if_debug0('v', "[v]pnga_text_begin\n");
    code = gx_default_text_begin(mdev, pis, text, font, path, pdcolor, pcpath,
				 memory, &target_enum);

    rc_alloc_struct_1(penum, pnga_text_enum_t, &st_pnga_text_enum, memory,
		      return_error(gs_error_VMerror), "pnga_text_begin");
    penum->rc.free = rc_free_text_enum;
    penum->target_enum = target_enum;
    code = gs_text_enum_init((gs_text_enum_t *)penum, &pnga_text_procs,
			     dev, pis, text, font, path, pdcolor, pcpath,
			     memory);
    if (code < 0) {
	gs_free_object(memory, penum, "pnga_text_begin");
	return code;
    }
    *ppenum = (gs_text_enum_t *)penum;
    rc_decrement_only(mdev, "pnga_text_begin");
    return code;
}

private int
pnga_fill_rectangle(gx_device * dev,
		    int x, int y, int w, int h, gx_color_index color)
{
    if_debug4('v', "[v]pnga_fill_rectangle, (%d, %d), %d x %d\n", x, y, w, h);
    return 0;
}

private int
pnga_mark_fill_rectangle(gx_device * dev,
		    int x, int y, int w, int h, gx_color_index color)
{
    pnga_mark_device * mdev = (pnga_mark_device *)dev;
    pnga_device * pdev = (pnga_device *)mdev->pnga_dev;
    int i, j;
    uchar *line, *dst_ptr;
    int rowstride;
    byte src[4];
    gs_blend_mode_t blend_mode = mdev->blend_mode;

#if 0
    fprintf (stderr, "pnga_fill_rectangle, (%d, %d) %d x %d: %x\n",
	     x, y, w, h, color);
#endif
    rowstride = pdev->rowstride;
    line = (uchar *)(pdev->data + y * pdev->rowstride + x * 4);

    src[0] = color >> 16;
    src[1] = (color >> 8) & 0xff;
    src[2] = color & 0xff;
    src[3] = floor (255 * mdev->alpha + 0.5);

    for (j = 0; j < h; j++) {
	dst_ptr = line;
	for (i = 0; i < w; i++) {
	    art_pdf_composite_pixel_alpha_8(dst_ptr, src, 3, blend_mode);
	    dst_ptr += 4;
	}
	line += rowstride;
    }
    return 0;
}

