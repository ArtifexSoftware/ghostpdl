/* Copyright (C) 1996, 2000 Aladdin Enterprises.  All rights reserved.

   This file is part of Aladdin Ghostscript.

   Aladdin Ghostscript is distributed with NO WARRANTY OF ANY KIND.  No author
   or distributor accepts any responsibility for the consequences of using it,
   or for whether it serves any particular purpose or works at all, unless he
   or she says so in writing.  Refer to the Aladdin Ghostscript Free Public
   License (the "License") for full details.

   Every copy of Aladdin Ghostscript must include a copy of the License,
   normally in a plain ASCII text file named PUBLIC.  The License grants you
   the right to copy, modify and redistribute Aladdin Ghostscript, but only
   under certain conditions described in the License.  Among other things, the
   License requires that the copyright notice and this notice be preserved on
   all copies.
 */

/*$Id$ */
/* Image handling for PDF-writing driver */
#include "math_.h"
#include "memory_.h"
#include "string_.h"
#include "jpeglib_.h"		/* for sdct.h */
#include "gx.h"
#include "gserrors.h"
#include "gsflip.h"
#include "gdevpdfx.h"
#include "gdevpdff.h"		/* for synthesized bitmap fonts */
#include "gdevpdfg.h"
#include "gdevpdfo.h"		/* for data stream */
#include "gxcspace.h"
#include "gscolor2.h"		/* for gscie.h */
#include "gscie.h"		/* requires gscspace.h */
#include "gsiparm3.h"
#include "gsiparm4.h"
#include "gxistate.h"
#include "strimpl.h"
#include "sa85x.h"
#include "scfx.h"
#include "sdct.h"
#include "slzwx.h"
#include "spngpx.h"
#include "srlx.h"
#include "szlibx.h"

/* We need this color space type for constructing temporary color spaces. */
extern const gs_color_space_type gs_color_space_type_Indexed;

#define MAX_INLINE_IMAGE_BYTES 4000

#define CHECK(expr)\
  BEGIN if ((code = (expr)) < 0) return code; END

/* ---------------- Utilities ---------------- */

/* ------ Images ------ */

/* Fill in the image parameters for a device space bitmap. */
/* PDF images are always specified top-to-bottom. */
/* data_h is the actual number of data rows, which may be less than h. */
private void
pdf_make_bitmap_matrix(gs_matrix * pmat, int x, int y, int w, int h,
		       int h_actual)
{
    pmat->xx = w;
    pmat->xy = 0;
    pmat->yx = 0;
    pmat->yy = -h_actual;
    pmat->tx = x;
    pmat->ty = y + h;
}
private void
pdf_make_bitmap_image(gs_image_t * pim, int x, int y, int w, int h)
{
    pim->Width = w;
    pim->Height = h;
    pdf_make_bitmap_matrix(&pim->ImageMatrix, x, y, w, h, h);
}

/* Put out the gsave and matrix for an image. */
private void
pdf_put_image_matrix(gx_device_pdf * pdev, const gs_matrix * pmat,
		     floatp y_scale)
{
    gs_matrix imat;

    gs_matrix_scale(pmat, 1.0, y_scale, &imat);
    pdf_put_matrix(pdev, "q ", &imat, "cm\n");
}

/* ------ Image writing ------ */

/* Forward references */
private image_enum_proc_plane_data(pdf_image_plane_data);
private image_enum_proc_end_image(pdf_image_end_image);

private const gx_image_enum_procs_t pdf_image_enum_procs = {
    pdf_image_plane_data,
    pdf_image_end_image
};

/* Put out a reference to an image resource. */
private int
pdf_do_image(gx_device_pdf * pdev, const pdf_resource_t * pres,
	     const gs_matrix * pimat, bool in_contents)
{
    if (in_contents) {
	int code = pdf_open_contents(pdev, PDF_IN_STREAM);

	if (code < 0)
	    return code;
    }
    if (pimat) {
	/* Scale the matrix to account for short images. */
	const pdf_x_object_t *const pxo = (const pdf_x_object_t *)pres;
	double scale = (double)pxo->data_height / pxo->height;

	pdf_put_image_matrix(pdev, pimat, scale);
    }
    pprintld1(pdev->strm, "/R%ld Do\nQ\n", pdf_resource_id(pres));
    return 0;
}

/* ---------------- Driver procedures ---------------- */

/* ------ Low-level calls ------ */

/* Copy a mask bitmap.  for_pattern = -1 means put the image in-line, */
/* 1 means put the image in a resource. */
private int
pdf_copy_mask_data(gx_device_pdf * pdev, const byte * base, int sourcex,
		   int raster, gx_bitmap_id id, int x, int y, int w, int h,
		   gs_image_t *pim, pdf_image_writer *piw,
		   int for_pattern)
{
    ulong nbytes;
    int code;
    const byte *row_base;
    int row_step;
    long pos;
    bool in_line;

    gs_image_t_init_mask(pim, true);
    pdf_make_bitmap_image(pim, x, y, w, h);
    nbytes = ((ulong)w * h + 7) / 8;

    if (for_pattern) {
	/*
	 * Patterns must be emitted in order of increasing user Y, i.e.,
	 * the opposite of PDF's standard image order.
	 */
	row_base = base + (h - 1) * raster;
	row_step = -raster;
	in_line = for_pattern < 0;
    } else {
	row_base = base;
	row_step = raster;
	in_line = nbytes <= MAX_INLINE_IMAGE_BYTES;
	pdf_put_image_matrix(pdev, &pim->ImageMatrix, 1.0);
	/*
	 * Check whether we've already made an XObject resource for this
	 * image.
	 */
	if (id != gx_no_bitmap_id) {
	    piw->pres = pdf_find_resource_by_gs_id(pdev, resourceXObject, id);
	    if (piw->pres)
		return 0;
	}
    }
    /*
     * We have to be able to control whether to put Pattern images in line,
     * to avoid trying to create an XObject resource while we're in the
     * middle of writing a Pattern resource.
     */
    if (for_pattern < 0)
	pputs(pdev->strm, "q ");
    if ((code = pdf_begin_write_image(pdev, piw, id, w, h, NULL, in_line)) < 0 ||
	(code = psdf_setup_image_filters((gx_device_psdf *) pdev, &piw->binary,
					 (gs_pixel_image_t *)pim, NULL, NULL)) < 0 ||
	(code = pdf_begin_image_data(pdev, piw, (const gs_pixel_image_t *)pim,
				     NULL)) < 0
	)
	return code;
    pos = stell(pdev->streams.strm);
    pdf_copy_mask_bits(piw->binary.strm, row_base, sourcex, row_step, w, h, 0);
    cos_stream_add_since(piw->data, pos);
    pdf_end_image_binary(pdev, piw, piw->height);
    return pdf_end_write_image(pdev, piw);
}

/* Copy a monochrome bitmap or mask. */
private int
pdf_copy_mono(gx_device_pdf *pdev,
	      const byte *base, int sourcex, int raster, gx_bitmap_id id,
	      int x, int y, int w, int h, gx_color_index zero,
	      gx_color_index one, const gx_clip_path *pcpath)
{
    int code;
    gs_color_space cs;
    cos_value_t cs_value;
    cos_value_t *pcsvalue;
    byte palette[sizeof(gx_color_index) * 2];
    gs_image_t image;
    pdf_image_writer writer;
    pdf_stream_position_t ipos;
    pdf_resource_t *pres = 0;
    byte invert = 0;
    long pos;

    /* Update clipping. */
    if (pdf_must_put_clip_path(pdev, pcpath)) {
	code = pdf_open_page(pdev, PDF_IN_STREAM);
	if (code < 0)
	    return code;
	pdf_put_clip_path(pdev, pcpath);
    }
    /* We have 3 cases: mask, inverse mask, and solid. */
    if (zero == gx_no_color_index) {
	if (one == gx_no_color_index)
	    return 0;
	/* If a mask has an id, assume it's a character. */
	if (id != gx_no_bitmap_id && sourcex == 0) {
	    pdf_set_pure_color(pdev, one, &pdev->fill_color,
			       &psdf_set_fill_color_commands);
	    pres = pdf_find_resource_by_gs_id(pdev, resourceCharProc, id);
	    if (pres == 0) {	/* Define the character in an embedded font. */
		pdf_char_proc_t *pcp;
		int y_offset;
		int max_y_offset =
		(pdev->open_font == 0 ? 0 :
		 pdev->open_font->max_y_offset);

		gs_image_t_init_mask(&image, false);
		invert = 0xff;
		pdf_make_bitmap_image(&image, x, y, w, h);
		y_offset =
		    image.ImageMatrix.ty - (int)(pdev->text.current.y + 0.5);
		if (x < pdev->text.current.x ||
		    y_offset < -max_y_offset || y_offset > max_y_offset
		    )
		    y_offset = 0;
		/*
		 * The Y axis of the text matrix is inverted,
		 * so we need to negate the Y offset appropriately.
		 */
		code = pdf_begin_char_proc(pdev, w, h, 0, y_offset, id,
					   &pcp, &ipos);
		if (code < 0)
		    return code;
		y_offset = -y_offset;
		pprintd3(pdev->strm, "0 0 0 %d %d %d d1\n", y_offset,
			 w, h + y_offset);
		pprintd3(pdev->strm, "%d 0 0 %d 0 %d cm\n", w, h,
			 y_offset);
		code = pdf_begin_write_image(pdev, &writer, gs_no_id, w, h, NULL, true);
		if (code < 0)
		    return code;
		pcp->rid = id;
		pres = (pdf_resource_t *) pcp;
		goto wr;
	    }
	    pdf_make_bitmap_matrix(&image.ImageMatrix, x, y, w, h, h);
	    goto rx;
	}
	pdf_set_pure_color(pdev, one, &pdev->fill_color,
			   &psdf_set_fill_color_commands);
	gs_image_t_init_mask(&image, false);
	invert = 0xff;
    } else if (one == gx_no_color_index) {
	gs_image_t_init_mask(&image, false);
	pdf_set_pure_color(pdev, zero, &pdev->fill_color,
			   &psdf_set_fill_color_commands);
    } else if (zero == pdev->black && one == pdev->white) {
	gs_cspace_init_DeviceGray(&cs);
	gs_image_t_init(&image, &cs);
    } else if (zero == pdev->white && one == pdev->black) {
	gs_cspace_init_DeviceGray(&cs);
	gs_image_t_init(&image, &cs);
	invert = 0xff;
    } else {
	/*
	 * We think this code is never executed when interpreting PostScript
	 * or PDF: the library never uses monobit non-mask images
	 * internally, and high-level images don't go through this code.
	 * However, we still want the code to work.
	 */
	gs_color_space cs_base;
	gx_color_index c[2];
	int i, j;
	int ncomp = pdev->color_info.num_components;
	byte *p;

	switch (ncomp) {
	case 1: gs_cspace_init_DeviceGray(&cs_base); break;
	case 3: gs_cspace_init_DeviceRGB(&cs_base); break;
	case 4: gs_cspace_init_DeviceCMYK(&cs_base); break;
	default: return_error(gs_error_rangecheck);
	}
	c[0] = psdf_adjust_color_index((gx_device_vector *)pdev, zero);
	c[1] = psdf_adjust_color_index((gx_device_vector *)pdev, one);
	gs_cspace_init(&cs, &gs_color_space_type_Indexed, NULL);
	cs.params.indexed.base_space = *(gs_direct_color_space *)&cs_base;
	cs.params.indexed.hival = 1;
	p = palette;
	for (i = 0; i < 2; ++i)
	    for (j = ncomp - 1; j >= 0; --j)
		*p++ = (byte)(c[i] >> (j * 8));
	cs.params.indexed.lookup.table.data = palette;
	cs.params.indexed.lookup.table.size = p - palette;
	cs.params.indexed.use_proc = false;
	gs_image_t_init(&image, &cs);
	image.BitsPerComponent = 1;
    }
    pdf_make_bitmap_image(&image, x, y, w, h);
    {
	ulong nbytes = (ulong) ((w + 7) >> 3) * h;
	bool in_line = nbytes <= MAX_INLINE_IMAGE_BYTES;

	if (in_line)
	    pdf_put_image_matrix(pdev, &image.ImageMatrix, 1.0);
	code = pdf_open_page(pdev, PDF_IN_STREAM);
	if (code < 0)
	    return code;
	code = pdf_begin_write_image(pdev, &writer, gs_no_id, w, h, NULL, in_line);
	if (code < 0)
	    return code;
    }
  wr:
    if (image.ImageMask)
	pcsvalue = NULL;
    else {
	code = pdf_color_space(pdev, &cs_value, &cs,
			       &writer.pin->color_spaces, true);
	if (code < 0)
	    return code;
	pcsvalue = &cs_value;
    }
    /*
     * There are 3 different cases at this point:
     *      - Writing an in-line image (pres == 0, writer.pres == 0);
     *      - Writing an XObject image (pres == 0, writer.pres != 0);
     *      - Writing the image for a CharProc (pres != 0).
     * We handle them with in-line code followed by a switch,
     * rather than making the shared code into a procedure,
     * simply because there would be an awful lot of parameters
     * that would need to be passed.
     */
    if (pres) {
	/* Always use CCITTFax 2-D for character bitmaps. */
	psdf_CFE_binary(&writer.binary, image.Width, image.Height, false);
    } else {
	/* Use the Distiller compression parameters. */
	psdf_setup_image_filters((gx_device_psdf *) pdev, &writer.binary,
				 (gs_pixel_image_t *)&image, NULL, NULL);
    }
    pdf_begin_image_data(pdev, &writer, (const gs_pixel_image_t *)&image,
			 pcsvalue);
    pos = stell(pdev->streams.strm);
    code = pdf_copy_mask_bits(writer.binary.strm, base, sourcex, raster,
			      w, h, invert);
    if (code < 0)
	return code;
    code = cos_stream_add_since(writer.data, pos);
    pdf_end_image_binary(pdev, &writer, writer.height);
    if (!pres) {
	switch ((code = pdf_end_write_image(pdev, &writer))) {
	    default:		/* error */
		return code;
	    case 1:
		return 0;
	    case 0:
		return pdf_do_image(pdev, writer.pres, &image.ImageMatrix,
				    true);
	}
    }
    writer.end_string = "";	/* no Q */
    switch ((code = pdf_end_write_image(pdev, &writer))) {
    default:		/* error */
	return code;
    case 0:			/* not possible */
	return_error(gs_error_Fatal);
    case 1:
	break;
    }
    code = pdf_end_char_proc(pdev, &ipos);
    if (code < 0)
	return code;
  rx:{
	gs_matrix imat;

	imat = image.ImageMatrix;
	imat.xx /= w;
	imat.xy /= h;
	imat.yx /= w;
	imat.yy /= h;
	return pdf_do_char_image(pdev, (const pdf_char_proc_t *)pres, &imat);
    }
}
int
gdev_pdf_copy_mono(gx_device * dev,
		   const byte * base, int sourcex, int raster, gx_bitmap_id id,
		   int x, int y, int w, int h, gx_color_index zero,
		   gx_color_index one)
{
    gx_device_pdf *pdev = (gx_device_pdf *) dev;

    if (w <= 0 || h <= 0)
	return 0;
    return pdf_copy_mono(pdev, base, sourcex, raster, id, x, y, w, h,
			 zero, one, NULL);
}

/* Copy a color bitmap.  for_pattern = -1 means put the image in-line, */
/* 1 means put the image in a resource. */
private int
pdf_copy_color_data(gx_device_pdf * pdev, const byte * base, int sourcex,
		    int raster, gx_bitmap_id id, int x, int y, int w, int h,
		    gs_image_t *pim, pdf_image_writer *piw,
		    int for_pattern)
{
    int depth = pdev->color_info.depth;
    int bytes_per_pixel = depth >> 3;
    gs_color_space cs;
    cos_value_t cs_value;
    ulong nbytes;
    int code;
    const byte *row_base;
    int row_step;
    long pos;
    bool in_line;

    switch(bytes_per_pixel) {
    case 3: gs_cspace_init_DeviceRGB(&cs); break;
    case 4: gs_cspace_init_DeviceCMYK(&cs); break;
    default: gs_cspace_init_DeviceGray(&cs);
    }
    gs_image_t_init(pim, &cs);
    pdf_make_bitmap_image(pim, x, y, w, h);
    pim->BitsPerComponent = 8;
    nbytes = (ulong)w * bytes_per_pixel * h;

    if (for_pattern) {
	/*
	 * Patterns must be emitted in order of increasing user Y, i.e.,
	 * the opposite of PDF's standard image order.
	 */
	row_base = base + (h - 1) * raster;
	row_step = -raster;
	in_line = for_pattern < 0;
    } else {
	row_base = base;
	row_step = raster;
	in_line = nbytes <= MAX_INLINE_IMAGE_BYTES;
	pdf_put_image_matrix(pdev, &pim->ImageMatrix, 1.0);
	/*
	 * Check whether we've already made an XObject resource for this
	 * image.
	 */
	if (id != gx_no_bitmap_id) {
	    piw->pres = pdf_find_resource_by_gs_id(pdev, resourceXObject, id);
	    if (piw->pres)
		return 0;
	}
    }
    /*
     * We have to be able to control whether to put Pattern images in line,
     * to avoid trying to create an XObject resource while we're in the
     * middle of writing a Pattern resource.
     */
    if (for_pattern < 0)
	pputs(pdev->strm, "q ");
    if ((code = pdf_begin_write_image(pdev, piw, id, w, h, NULL, in_line)) < 0 ||
	(code = pdf_color_space(pdev, &cs_value, &cs,
				&piw->pin->color_spaces, true)) < 0 ||
	(code = psdf_setup_image_filters((gx_device_psdf *) pdev, &piw->binary,
					 (gs_pixel_image_t *)pim, NULL, NULL)) < 0 ||
	(code = pdf_begin_image_data(pdev, piw, (const gs_pixel_image_t *)pim,
				     &cs_value)) < 0
	)
	return code;
    pos = stell(pdev->streams.strm);
    pdf_copy_color_bits(piw->binary.strm, row_base, sourcex, row_step, w, h,
			bytes_per_pixel);
    cos_stream_add_since(piw->data, pos);
    pdf_end_image_binary(pdev, piw, piw->height);
    return pdf_end_write_image(pdev, piw);
}

int
gdev_pdf_copy_color(gx_device * dev, const byte * base, int sourcex,
		    int raster, gx_bitmap_id id, int x, int y, int w, int h)
{
    gx_device_pdf *pdev = (gx_device_pdf *) dev;
    gs_image_t image;
    pdf_image_writer writer;
    int code;
    
    if (w <= 0 || h <= 0)
	return 0;
    code = pdf_open_page(pdev, PDF_IN_STREAM);
    if (code < 0)
	return code;
    /* Make sure we aren't being clipped. */
    pdf_put_clip_path(pdev, NULL);
    code = pdf_copy_color_data(pdev, base, sourcex, raster, id, x, y, w, h,
			       &image, &writer, 0);
    switch (code) {
	default:
	    return code;	/* error */
	case 1:
	    return 0;
	case 0:
	    return pdf_do_image(pdev, writer.pres, NULL, true);
    }
}

/* Fill a mask. */
int
gdev_pdf_fill_mask(gx_device * dev,
		 const byte * data, int data_x, int raster, gx_bitmap_id id,
		   int x, int y, int width, int height,
		   const gx_drawing_color * pdcolor, int depth,
		   gs_logical_operation_t lop, const gx_clip_path * pcpath)
{
    gx_device_pdf *pdev = (gx_device_pdf *) dev;

    if (width <= 0 || height <= 0)
	return 0;
    if (depth > 1 || !gx_dc_is_pure(pdcolor) != 0)
	return gx_default_fill_mask(dev, data, data_x, raster, id,
				    x, y, width, height, pdcolor, depth, lop,
				    pcpath);
    return pdf_copy_mono(pdev, data, data_x, raster, id, x, y, width, height,
			 gx_no_color_index, gx_dc_pure_color(pdcolor),
			 pcpath);
}

/* Tile with a bitmap.  This is important for pattern fills. */
int
gdev_pdf_strip_tile_rectangle(gx_device * dev, const gx_strip_bitmap * tiles,
			      int x, int y, int w, int h,
			      gx_color_index color0, gx_color_index color1,
			      int px, int py)
{
    gx_device_pdf *const pdev = (gx_device_pdf *) dev;
    int tw = tiles->rep_width, th = tiles->rep_height;
    double xscale = pdev->HWResolution[0] / 72.0,
	yscale = pdev->HWResolution[1] / 72.0;
    bool mask;
    int depth;
    int (*copy_data)(P12(gx_device_pdf *, const byte *, int, int,
			 gx_bitmap_id, int, int, int, int,
			 gs_image_t *, pdf_image_writer *, int));
    pdf_resource_t *pres;
    cos_value_t cs_value;
    int code;

    if (tiles->id == gx_no_bitmap_id || tiles->shift != 0 ||
	(w < tw && h < th) ||
	color0 != gx_no_color_index ||
	/* Pattern fills are only available starting in PDF 1.2. */
	pdev->CompatibilityLevel < 1.2
	)
	goto use_default;
    if (color1 != gx_no_color_index) {
	/* This is a mask pattern. */
	mask = true;
	depth = 1;
	copy_data = pdf_copy_mask_data;
	code = pdf_cs_Pattern_RGB(pdev, &cs_value);
    } else {
	/* This is a colored pattern. */
	mask = false;
	depth = pdev->color_info.depth;
	copy_data = pdf_copy_color_data;
	code = pdf_cs_Pattern(pdev, &cs_value);
    }
    if (code < 0)
	goto use_default;
    pres = pdf_find_resource_by_gs_id(pdev, resourcePattern, tiles->id);
    if (!pres) {
	/* Create the Pattern resource. */
	int code;
	long image_id, length_id, start, end;
	stream *s;
	gs_image_t image;
	pdf_image_writer writer;
	long image_bytes = ((long)tw * depth + 7) / 8 * th;
	bool in_line = image_bytes <= MAX_INLINE_IMAGE_BYTES;
	ulong tile_id =
	    (tw == tiles->size.x && th == tiles->size.y ? tiles->id :
	     gx_no_bitmap_id);

	if (in_line)
	    image_id = 0;
	else if (image_bytes > 65500) {
	    /*
	     * Acrobat Reader can't handle image Patterns with more than
	     * 64K of data.  :-(
	     */
	    goto use_default;
	} else {
	    /* Write out the image as an XObject resource now. */
	    code = copy_data(pdev, tiles->data, 0, tiles->raster,
			     tile_id, 0, 0, tw, th, &image, &writer, 1);
	    if (code < 0)
		goto use_default;
	    image_id = pdf_resource_id(writer.pres);
	}
	code = pdf_begin_resource(pdev, resourcePattern, tiles->id, &pres);
	if (code < 0)
	    goto use_default;
	s = pdev->strm;
	pprintd1(s, "/Type/Pattern/PatternType 1/PaintType %d/TilingType 1/Resources<<\n",
		 (mask ? 2 : 1));
	if (image_id)
	    pprintld2(s, "/XObject<</R%ld %ld 0 R>>", image_id, image_id);
	pprints1(s, "/ProcSet[/PDF/Image%s]>>\n", (mask ? "B" : "C"));
	/*
	 * Because of bugs in Acrobat Reader's Print function, we can't use
	 * the natural BBox and Step here: they have to be 1.
	 */
	pprintg2(s, "/Matrix[%g 0 0 %g 0 0]", tw / xscale, th / yscale);
	pputs(s, "/BBox[0 0 1 1]/XStep 1/YStep 1/Length ");
	if (image_id) {
	    char buf[MAX_REF_CHARS + 6 + 1]; /* +6 for /R# Do\n */

	    sprintf(buf, "/R%ld Do\n", image_id);
	    pprintd1(s, "%d>>stream\n", strlen(buf));
	    pprints1(s, "%sendstream\n", buf);
	    pdf_end_resource(pdev);
	} else {
	    length_id = pdf_obj_ref(pdev);
	    pprintld1(s, "%ld 0 R>>stream\n", length_id);
	    start = pdf_stell(pdev);
	    code = copy_data(pdev, tiles->data, 0, tiles->raster,
			     tile_id, 0, 0, tw, th, &image, &writer, -1);
	    switch (code) {
	    default:
		return code;	/* error */
	    case 1:
		break;
	    case 0:			/* not possible */
		return_error(gs_error_Fatal);
	    }
	    end = pdf_stell(pdev);
	    pputs(s, "\nendstream\n");
	    pdf_end_resource(pdev);
	    pdf_open_separate(pdev, length_id);
	    pprintld1(pdev->strm, "%ld\n", end - start);
	    pdf_end_separate(pdev);
	}
    }
    /* Fill the rectangle with the Pattern. */
    {
	int code = pdf_open_page(pdev, PDF_IN_STREAM);
	stream *s;

	if (code < 0)
	    goto use_default;
	/* Make sure we aren't being clipped. */
	pdf_put_clip_path(pdev, NULL);
	s = pdev->strm;
	/*
	 * Because of bugs in Acrobat Reader's Print function, we can't
	 * leave the CTM alone here: we have to reset it to the default.
	 */
	pprintg2(s, "q %g 0 0 %g 0 0 cm\n", xscale, yscale);
	cos_value_write(&cs_value, pdev);
	pputs(s, " cs");
	if (mask)
	    pprintd3(s, " %d %d %d", (int)(color1 >> 16),
		     (int)((color1 >> 8) & 0xff), (int)(color1 & 0xff));
	pprintld1(s, "/R%ld scn", pdf_resource_id(pres));
	pprintg4(s, " %g %g %g %g re f Q\n",
		 x / xscale, y / yscale, w / xscale, h / xscale);
    }
    return 0;
use_default:
    return gx_default_strip_tile_rectangle(dev, tiles, x, y, w, h,
					   color0, color1, px, py);
}

/* ------ High-level calls ------ */

/* Define the structure for keeping track of progress through an image. */
typedef struct pdf_image_enum_s {
    gx_image_enum_common;
    gs_memory_t *memory;
    int width;
    int bits_per_pixel;		/* bits per pixel (per plane) */
    int rows_left;
    pdf_image_writer writer;
    gs_matrix mat;
} pdf_image_enum;
gs_private_st_composite(st_pdf_image_enum, pdf_image_enum, "pdf_image_enum",
  pdf_image_enum_enum_ptrs, pdf_image_enum_reloc_ptrs);
/* GC procedures */
private ENUM_PTRS_WITH(pdf_image_enum_enum_ptrs, pdf_image_enum *pie)
    if (index < pdf_image_writer_max_ptrs) {
	gs_ptr_type_t ret =
	    ENUM_USING(st_pdf_image_writer, &pie->writer, sizeof(pie->writer),
		       index);

	if (ret == 0)		/* don't stop early */
	    ENUM_RETURN(0);
	return ret;
    }
    return ENUM_USING_PREFIX(st_gx_image_enum_common,
			     pdf_image_writer_max_ptrs);
ENUM_PTRS_END
private RELOC_PTRS_WITH(pdf_image_enum_reloc_ptrs, pdf_image_enum *pie)
{
    RELOC_USING(st_pdf_image_writer, &pie->writer, sizeof(pie->writer));
    RELOC_USING(st_gx_image_enum_common, vptr, size);
}
RELOC_PTRS_END

/* Start processing an image. */
int
gdev_pdf_begin_typed_image(gx_device * dev, const gs_imager_state * pis,
			   const gs_matrix *pmat, const gs_image_common_t *pic,
			   const gs_int_rect * prect,
			   const gx_drawing_color * pdcolor,
			   const gx_clip_path * pcpath, gs_memory_t * mem,
			   gx_image_enum_common_t ** pinfo)
{
    gx_device_pdf *pdev = (gx_device_pdf *) dev;
    const gs_pixel_image_t *pim;
    int code;
    pdf_image_enum *pie;
    gs_image_format_t format;
    const gs_color_space *pcs;
    cos_value_t cs_value;
    int num_components;
    bool is_mask, in_line;
    gs_int_rect rect;
    /*
     * We define this union because psdf_setup_image_filters may alter the
     * gs_pixel_image_t part, but pdf_begin_image_data must also have access
     * to the type-specific parameters.
     */
    union iu_ {
	gs_pixel_image_t pixel;	/* we may change some components */
	gs_image1_t type1;
	gs_image3_t type3;
	gs_image4_t type4;
    } image;
    ulong nbytes;
    int width, height;

    /* Check for the image types we can handle. */
    switch (pic->type->index) {
    case 1: {
	const gs_image_t *pim1 = (const gs_image_t *)pic;

	if (pim1->Alpha != gs_image_alpha_none)
	    goto nyi;
	is_mask = pim1->ImageMask;
	in_line = true;
	image.type1 = *pim1;
	break;
    }
    case 3: {
	const gs_image3_t *pim3 = (const gs_image3_t *)pic;

	if (pdev->CompatibilityLevel < 1.3)
	    goto nyi;
	if (prect && !(prect->p.x == 0 && prect->p.y == 0 &&
		       prect->q.x == pim3->Width &&
		       prect->q.y == pim3->Height))
	    goto nyi;
	/****** NYI ******/
	if (1) goto nyi;
	is_mask = in_line = false;
	image.type3 = *pim3;
	break;
    }
    case 4:
	if (pdev->CompatibilityLevel < 1.3)
	    goto nyi;
	is_mask = in_line = false;
	image.type4 = *(const gs_image4_t *)pic;
	break;
    default:
	goto nyi;
    }
    pim = (const gs_pixel_image_t *)pic;
    format = pim->format;
    switch (format) {
    case gs_image_format_chunky:
    case gs_image_format_component_planar:
	break;
    default:
	goto nyi;
    }
    pcs = pim->ColorSpace;
    num_components = (is_mask ? 1 : gs_color_space_num_components(pcs));

    code = pdf_open_page(pdev, PDF_IN_STREAM);
    if (code < 0)
	return code;
    if (is_mask) {
	code = pdf_set_drawing_color(pdev, pdcolor, &pdev->fill_color,
				     &psdf_set_fill_color_commands);
	if (code < 0)
	    goto nyi;
    }
    if (prect)
	rect = *prect;
    else {
	rect.p.x = rect.p.y = 0;
	rect.q.x = pim->Width, rect.q.y = pim->Height;
    }
    pie = gs_alloc_struct(mem, pdf_image_enum, &st_pdf_image_enum,
			  "pdf_begin_image");
    if (pie == 0)
	return_error(gs_error_VMerror);
    *pinfo = (gx_image_enum_common_t *) pie;
    gx_image_enum_common_init(*pinfo, (const gs_data_image_t *) pim,
			      &pdf_image_enum_procs, dev,
			      num_components, format);
    pie->memory = mem;
    width = rect.q.x - rect.p.x;
    pie->width = width;
    height = rect.q.y - rect.p.y;
    pie->bits_per_pixel =
	pim->BitsPerComponent * num_components / pie->num_planes;
    pie->rows_left = height;
    nbytes = (((ulong) pie->width * pie->bits_per_pixel + 7) >> 3) *
	pie->num_planes * pie->rows_left;
    in_line &= nbytes <= MAX_INLINE_IMAGE_BYTES;
    if (prect != 0 ||
	(is_mask ? pim->CombineWithColor :
	 pdf_color_space(pdev, &cs_value, pcs,
			 (in_line ? &pdf_color_space_names_short :
			  &pdf_color_space_names), true) < 0)
	) {
	gs_free_object(mem, pie, "pdf_begin_image");
	goto nyi;
    }
    pdf_put_clip_path(pdev, pcpath);
    if (pmat == 0)
	pmat = &ctm_only(pis);
    {
	gs_matrix mat;
	gs_matrix bmat;
	int code;

	pdf_make_bitmap_matrix(&bmat, -rect.p.x, -rect.p.y,
			       pim->Width, pim->Height, height);
	if ((code = gs_matrix_invert(&pim->ImageMatrix, &mat)) < 0 ||
	    (code = gs_matrix_multiply(&bmat, &mat, &mat)) < 0 ||
	    (code = gs_matrix_multiply(&mat, pmat, &pie->mat)) < 0
	    ) {
	    gs_free_object(mem, pie, "pdf_begin_image");
	    return code;
	}
    }
    if ((code = pdf_begin_write_image(pdev, &pie->writer, gs_no_id, width,
				      height, NULL, in_line)) < 0 ||
	/****** pctm IS WRONG ******/
	(code = psdf_setup_image_filters((gx_device_psdf *) pdev,
					 &pie->writer.binary, &image.pixel,
					 pmat, pis)) < 
	(code = pdf_begin_image_data(pdev, &pie->writer,
				     (const gs_pixel_image_t *)&image,
				     &cs_value)) < 0
	)
	return code;
    return 0;
 nyi:
    return gx_default_begin_typed_image(dev, pis, pmat, pic, prect, pdcolor,
					pcpath, mem, pinfo);
}

/* Process the next piece of an image. */
private int
pdf_image_plane_data(gx_image_enum_common_t * info,
		     const gx_image_plane_t * planes, int height,
		     int *rows_used)
{
    gx_device_pdf *pdev = (gx_device_pdf *)info->dev;
    pdf_image_enum *pie = (pdf_image_enum *) info;
    int h = height;
    int y;
    /****** DOESN'T HANDLE IMAGES WITH VARYING WIDTH PER PLANE ******/
    uint width_bits = pie->width * pie->plane_depths[0];
    /****** DOESN'T HANDLE NON-ZERO data_x CORRECTLY ******/
    uint bcount = (width_bits + 7) >> 3;
    uint ignore;
    int nplanes = pie->num_planes;
    stream *s = pdev->streams.strm;
    long pos = stell(s);
    int code;
    int status = 0;

    if (h > pie->rows_left)
	h = pie->rows_left;
    pie->rows_left -= h;
    for (y = 0; y < h; ++y) {
	if (nplanes > 1) {
	    /*
	     * We flip images in blocks, and each block except the last one
	     * must contain an integral number of pixels.  The easiest way
	     * to meet this condition is for all blocks except the last to
	     * be a multiple of 3 source bytes (guaranteeing an integral
	     * number of 1/2/4/8/12-bit samples), i.e., 3*nplanes flipped
	     * bytes.  This requires a buffer of at least
	     * 3*GS_IMAGE_MAX_COMPONENTS bytes.
	     */
	    int pi;
	    uint count = bcount;
	    uint offset = 0;
#define ROW_BYTES max(200 /*arbitrary*/, 3 * GS_IMAGE_MAX_COMPONENTS)
	    const byte *bit_planes[GS_IMAGE_MAX_COMPONENTS];
	    int block_bytes = ROW_BYTES / (3 * nplanes) * 3;
	    byte row[ROW_BYTES];

	    for (pi = 0; pi < nplanes; ++pi)
		bit_planes[pi] = planes[pi].data + planes[pi].raster * y;
	    while (count) {
		uint flip_count;
		uint flipped_count;

		if (count >= block_bytes) {
		    flip_count = block_bytes;
		    flipped_count = block_bytes * nplanes;
		} else {
		    flip_count = count;
		    flipped_count =
			(width_bits % (block_bytes * 8) * nplanes + 7) >> 3;
		}
		image_flip_planes(row, bit_planes, offset, flip_count,
				  nplanes, pie->plane_depths[0]);
		status = sputs(pie->writer.binary.strm, row, flipped_count,
			       &ignore);
		if (status < 0)
		    break;
		offset += flip_count;
		count -= flip_count;
	    }
	} else {
	    status = sputs(pie->writer.binary.strm,
			   planes[0].data + planes[0].raster * y, bcount,
			   &ignore);
	}
	if (status < 0)
	    break;
    }
    *rows_used = h;
    if (status < 0)
	return_error(gs_error_ioerror);
    code = cos_stream_add_since(pie->writer.data, pos);
    return (code < 0 ? code : !pie->rows_left);
#undef ROW_BYTES
}

/* Clean up by releasing the buffers. */
private int
pdf_image_end_image(gx_image_enum_common_t * info, bool draw_last)
{
    gx_device_pdf *pdev = (gx_device_pdf *)info->dev;
    pdf_image_enum *pie = (pdf_image_enum *)info;
    int height = pie->writer.height;
    int data_height = height - pie->rows_left;
    int code;

    if (pie->writer.pres)
	((pdf_x_object_t *)pie->writer.pres)->data_height = data_height;
    else
	pdf_put_image_matrix(pdev, &pie->mat,
			     (height == 0 || data_height == 0 ? 1.0 :
			      (double)data_height / height));
    code = pdf_end_image_binary(pdev, &pie->writer, data_height);
    if (code < 0)
	return code;
    code = pdf_end_write_image(pdev, &pie->writer);
    switch (code) {
    default:
	return code;	/* error */
    case 1:
	code = 0;
	break;
    case 0:
	code = pdf_do_image(pdev, pie->writer.pres, &pie->mat, true);
    }
    gs_free_object(pie->memory, pie, "pdf_end_image");
    return code;
}
