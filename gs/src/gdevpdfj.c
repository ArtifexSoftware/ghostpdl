/* Copyright (C) 2000 Aladdin Enterprises.  All rights reserved.
  
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
*/

/* $Id$ */
/* Image-writing utilities for pdfwrite driver */
#include "memory_.h"
#include "string_.h"
#include "gx.h"
#include "gserrors.h"
#include "gdevpdfx.h"
#include "gdevpdfg.h"
#include "gdevpdfo.h"
#include "gxcspace.h"
#include "gsiparm4.h"
#include "gdevpsds.h"

#define CHECK(expr)\
  BEGIN if ((code = (expr)) < 0) return code; END

/* GC descriptors */
public_st_pdf_image_writer();
private ENUM_PTRS_WITH(pdf_image_writer_enum_ptrs, pdf_image_writer *piw)
     index -= 3;
     if (index < psdf_binary_writer_max_ptrs * 3) {
	 gs_ptr_type_t ret =
	     ENUM_USING(st_psdf_binary_writer, &piw->binary[index % 3],
			sizeof(psdf_binary_writer), index / 3);

	 if (ret == 0)		/* don't stop early */
	     ENUM_RETURN(0);
	 return ret;
    }
    return 0;
case 0: ENUM_RETURN(piw->pres);
case 1: ENUM_RETURN(piw->data);
case 2: ENUM_RETURN(piw->named);
ENUM_PTRS_END
private RELOC_PTRS_WITH(pdf_image_writer_reloc_ptrs, pdf_image_writer *piw)
{
    int i;

    for (i = 0; i < piw->alt_writer_count; ++i)
	RELOC_USING(st_psdf_binary_writer, &piw->binary[i],
		    sizeof(psdf_binary_writer));
    RELOC_VAR(piw->pres);
    RELOC_VAR(piw->data);
    RELOC_VAR(piw->named);
}
RELOC_PTRS_END

/* ---------------- Image stream dictionaries ---------------- */

const pdf_image_names_t pdf_image_names_full = {
    { PDF_COLOR_SPACE_NAMES },
    { PDF_FILTER_NAMES },
    PDF_IMAGE_PARAM_NAMES
};
const pdf_image_names_t pdf_image_names_short = {
    { PDF_COLOR_SPACE_NAMES_SHORT },
    { PDF_FILTER_NAMES_SHORT },
    PDF_IMAGE_PARAM_NAMES_SHORT
};

/* Store the values of image parameters other than filters. */
/* pdev is used only for updating procsets. */
/* pcsvalue is not used for masks. */
private int
pdf_put_pixel_image_values(cos_dict_t *pcd, gx_device_pdf *pdev,
			   const gs_pixel_image_t *pim,
			   const gs_color_space *pcs,
			   const pdf_image_names_t *pin,
			   const cos_value_t *pcsvalue)
{
    int num_components;
    float indexed_decode[2];
    const float *default_decode = NULL;
    int code;

    if (pcs) {
	CHECK(cos_dict_put_c_key(pcd, pin->ColorSpace, pcsvalue));
	pdf_color_space_procsets(pdev, pcs);
	num_components = gs_color_space_num_components(pcs);
	if (gs_color_space_get_index(pcs) == gs_color_space_index_Indexed) {
	    indexed_decode[0] = 0;
	    indexed_decode[1] = (1 << pim->BitsPerComponent) - 1;
	    default_decode = indexed_decode;
	}
    } else
	num_components = 1;
    CHECK(cos_dict_put_c_key_int(pcd, pin->Width, pim->Width));
    CHECK(cos_dict_put_c_key_int(pcd, pin->Height, pim->Height));
    CHECK(cos_dict_put_c_key_int(pcd, pin->BitsPerComponent,
				 pim->BitsPerComponent));
    {
	int i;

	for (i = 0; i < num_components * 2; ++i)
	    if (pim->Decode[i] !=
		(default_decode ? default_decode[i] : i & 1)
		)
		break;
	if (i < num_components * 2) {
	    cos_array_t *pca =
		cos_array_alloc(pdev, "pdf_put_pixel_image_values(decode)");

	    if (pca == 0)
		return_error(gs_error_VMerror);
	    for (i = 0; i < num_components * 2; ++i)
		CHECK(cos_array_add_real(pca, pim->Decode[i]));
	    CHECK(cos_dict_put_c_key_object(pcd, pin->Decode,
					    COS_OBJECT(pca)));
	}
    }
    if (pim->Interpolate)
	CHECK(cos_dict_put_c_strings(pcd, pin->Interpolate, "true"));
    return 0;
}
int
pdf_put_image_values(cos_dict_t *pcd, gx_device_pdf *pdev,
		     const gs_pixel_image_t *pic,
		     const pdf_image_names_t *pin,
		     const cos_value_t *pcsvalue)
{
    const gs_color_space *pcs = pic->ColorSpace;
    int code;

    switch (pic->type->index) {
    case 1: {
	const gs_image1_t *pim = (const gs_image1_t *)pic;

	if (pim->ImageMask) {
	    CHECK(cos_dict_put_c_strings(pcd, pin->ImageMask, "true"));
	    pdev->procsets |= ImageB;
	    pcs = NULL;
	}
    }
	break;
    case 3: {
	/*
	 * Clients must treat this as a special case: they must call
	 * pdf_put_image_values for the MaskDict separately, and must
	 * add the Mask entry to the main image stream (dictionary).
	 */
	/*const gs_image3_t *pim = (const gs_image3_t *)pic;*/

	/* Masked images are only supported starting in PDF 1.3. */
	if (pdev->CompatibilityLevel < 1.3)
	    return_error(gs_error_rangecheck);
    }
	break;
    case 4: {
	const gs_image4_t *pim = (const gs_image4_t *)pic;
	int num_components = gs_color_space_num_components(pcs);
	cos_array_t *pca;
	int i;
    
	/* Masked images are only supported starting in PDF 1.3. */
	if (pdev->CompatibilityLevel < 1.3)
	    return_error(gs_error_rangecheck);
	pca = cos_array_alloc(pdev, "pdf_put_image_values(mask)");
	if (pca == 0)
	    return_error(gs_error_VMerror);
	for (i = 0; i < num_components; ++i) {
	    int lo, hi;

	    if (pim->MaskColor_is_range)
		lo = pim->MaskColor[i * 2], hi = pim->MaskColor[i * 2 + 1];
	    else
		lo = hi = pim->MaskColor[i];
	    CHECK(cos_array_add_int(pca, lo));
	    CHECK(cos_array_add_int(pca, hi));
	}
	CHECK(cos_dict_put_c_key_object(pcd, "/Mask", COS_OBJECT(pca)));
    }
	break;
    default:
	return_error(gs_error_rangecheck);
    }
    return pdf_put_pixel_image_values(pcd, pdev, pic, pcs, pin, pcsvalue);
}

/* Store filters for an image. */
/* Currently this only saves parameters for CCITTFaxDecode. */
int
pdf_put_image_filters(cos_dict_t *pcd, gx_device_pdf *pdev,
		      const psdf_binary_writer * pbw,
		      const pdf_image_names_t *pin)
{
    return pdf_put_filters(pcd, pdev, pbw->strm, &pin->filter_names);
}

/* ---------------- Image writing ---------------- */

/*
 * Fill in the image parameters for a device space bitmap.
 * PDF images are always specified top-to-bottom.
 * data_h is the actual number of data rows, which may be less than h.
 */
void
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

/*
 * Put out the gsave and matrix for an image.  y_scale adjusts the matrix
 * for images that end prematurely.
 */
void
pdf_put_image_matrix(gx_device_pdf * pdev, const gs_matrix * pmat,
		     floatp y_scale)
{
    gs_matrix imat;

    gs_matrix_translate(pmat, 0.0, 1.0 - y_scale, &imat);
    gs_matrix_scale(&imat, 1.0, y_scale, &imat);
    pdf_put_matrix(pdev, "q ", &imat, "cm\n");
}

/* Put out a reference to an image resource. */
int
pdf_do_image(gx_device_pdf * pdev, const pdf_resource_t * pres,
	     const gs_matrix * pimat, bool in_contents)
{
    if (in_contents) {
	int code = pdf_open_contents(pdev, PDF_IN_STREAM);

	if (code < 0)
	    return code;
    }
    if (pimat) {
	/* Adjust the matrix to account for short images. */
	const pdf_x_object_t *const pxo = (const pdf_x_object_t *)pres;
	double scale = (double)pxo->data_height / pxo->height;

	pdf_put_image_matrix(pdev, pimat, scale);
    }
    pprintld1(pdev->strm, "/R%ld Do\nQ\n", pdf_resource_id(pres));
    return 0;
}

/* ------ Begin / finish ------ */

/*
 * Begin writing an image, creating the resource if not in-line, and setting
 * up the binary writer.  If pnamed != 0, it is a stream object created by a
 * NI pdfmark.
 */
int
pdf_begin_write_image(gx_device_pdf * pdev, pdf_image_writer * piw,
		      gx_bitmap_id id, int w, int h, cos_dict_t *named,
		      bool in_line, int alt_writer_count)
{
    /* Patch pdev->strm so the right stream gets into the writer. */
    stream *save_strm = pdev->strm;
    int code;

    piw->alt_writer_count = alt_writer_count;
    if (in_line) {
	piw->pres = 0;
	piw->pin = &pdf_image_names_short;
	piw->data = cos_stream_alloc(pdev, "pdf_begin_image_data");
	if (piw->data == 0)
	    return_error(gs_error_VMerror);
	piw->end_string = " Q";
	piw->named = 0;		/* must have named == 0 */
    } else {
	pdf_x_object_t *pxo;
	cos_stream_t *pcos;

	/*
	 * Note that if named != 0, there are two objects with the same id
	 * while the image is being accumulated: named, and pres->object.
	 */
	code = pdf_alloc_resource(pdev, resourceXObject, id, &piw->pres,
				  (named ? named->id : 0L));
	if (code < 0)
	    return code;
	cos_become(piw->pres->object, cos_type_stream);
	piw->pres->rid = id;
	piw->pin = &pdf_image_names_full;
	pxo = (pdf_x_object_t *)piw->pres;
	pcos = (cos_stream_t *)pxo->object;
	CHECK(cos_dict_put_c_strings(cos_stream_dict(pcos), "/Subtype",
				     "/Image"));
	pxo->width = w;
	pxo->height = h;
	/* Initialize data_height for the benefit of copy_{mono,color}. */
	pxo->data_height = h;
	piw->data = pcos;
	piw->named = named;
    }
    pdev->strm = pdev->streams.strm;
    pdev->strm = cos_write_stream_alloc(piw->data, pdev, "pdf_begin_write_image");
    if (pdev->strm == 0)
	return_error(gs_error_VMerror);
    piw->height = h;
    code = psdf_begin_binary((gx_device_psdf *) pdev, &piw->binary[0]);
    piw->binary[0].target = NULL; /* We don't need target with cos_write_stream. */
    pdev->strm = save_strm;
    return code;
}

/*
 *  Make alternative stream for image compression choice.
 */
int
pdf_make_alt_stream(gx_device_pdf * pdev, psdf_binary_writer * pbw)
{
    cos_stream_t *pcos = cos_stream_alloc(pdev, "pdf_make_alt_stream");
    int code;

    if (pcos == 0)
        return_error(gs_error_VMerror);
    pcos->id = 0;
    CHECK(cos_dict_put_c_strings(cos_stream_dict(pcos), "/Subtype", "/Image"));
    pbw->strm = cos_write_stream_alloc(pcos, pdev, "pdf_make_alt_stream");
    if (pbw->strm == 0)
        return_error(gs_error_VMerror);
    pbw->dev = (gx_device_psdf *)pdev;
    pbw->memory = pdev->pdf_memory;
    return 0;
}

/* Begin writing the image data, setting up the dictionary and filters. */
int
pdf_begin_image_data(gx_device_pdf * pdev, pdf_image_writer * piw,
		     const gs_pixel_image_t * pim, const cos_value_t *pcsvalue,
		     int alt_writer_index)
{
    
    cos_stream_t *s = cos_write_stream_from_pipeline(piw->binary[alt_writer_index].strm);
    cos_dict_t *pcd = cos_stream_dict(s);
    int code = pdf_put_image_values(pcd, pdev, pim, piw->pin, pcsvalue);

    if (code >= 0)
	code = pdf_put_image_filters(pcd, pdev, &piw->binary[alt_writer_index], piw->pin);
    if (code < 0) {
	if (!piw->pres)
	    COS_FREE(piw->data, "pdf_begin_image_data");
	piw->data = 0;
    }
    return code;
}

/* Complete image data. */
int
pdf_complete_image_data(gx_device_pdf *pdev, pdf_image_writer *piw, int data_h,
			int width, int bits_per_pixel)
{
    if (data_h != piw->height) {
	if (piw->alt_writer_count > 1 ||
	    piw->binary[0].strm->procs.process == s_DCTE_template.process) {
	    /* 	Since DCTE can't safely close with incomplete data,
		we add stub data to complete the stream.
	    */
	    int bytes_per_line = (width * bits_per_pixel + 7) / 8;
	    int lines_left = piw->height - data_h;
	    byte buf[256];
	    const uint lb = sizeof(buf);
	    int i, l, status;
	    uint ignore;

	    memset(buf, 128, lb);
	    for (; lines_left; lines_left--) 
		for (i = 0; i < piw->alt_writer_count; i++) {
		    for (l = bytes_per_line; l > 0; l -= lb)
			if ((status = sputs(piw->binary[i].strm, buf, min(l, lb), 
					    &ignore)) < 0)
			    return_error(gs_error_ioerror);
		}
	}
    }
    return 0;
}

/* Finish writing the binary image data. */
int
pdf_end_image_binary(gx_device_pdf *pdev, pdf_image_writer *piw, int data_h)
{
    int code, code1 = 0;

    if (piw->alt_writer_count > 1)
	code = pdf_choose_compression(piw, true);
    else
	code = psdf_end_binary(&piw->binary[0]);
    /* If the image ended prematurely, update the Height. */
    if (data_h != piw->height)
	code1 = cos_dict_put_c_key_int(cos_stream_dict(piw->data),
				      piw->pin->Height, data_h);
    return code < 0 ? code : code1;
}

/*
 * Finish writing an image.  If in-line, write the BI/dict/ID/data/EI and
 * return 1; if a resource, write the resource definition and return 0.
 */
int
pdf_end_write_image(gx_device_pdf * pdev, pdf_image_writer * piw)
{
    pdf_resource_t *pres = piw->pres;

    if (pres) {			/* image resource */
	cos_object_t *const pco = pres->object;
	cos_stream_t *const pcs = (cos_stream_t *)pco;
	cos_dict_t *named = piw->named;

	if (named) {
	    /*
	     * This image was named by NI.  Copy any dictionary elements
	     * from the named dictionary to the image stream, and then
	     * associate the name with the stream.
	     */
	    int code = cos_dict_move_all(cos_stream_dict(pcs), named);

	    if (code < 0)
		return code;
	    pres->named = true;
	    /*
	     * We need to make the entry in the name dictionary point to
	     * the stream (pcs) rather than the object created by NI (named).
	     * Unfortunately, we no longer know what dictionary to use.
	     * Instead, overwrite the latter with the former's contents,
	     * and change the only relevant pointer.
	     */
	    *(cos_object_t *)named = *pco;
	    pres->object = COS_OBJECT(named);
	}
	else if (!pres->named) { /* named objects are written at the end */
	    cos_write_object(pco, pdev);
	    cos_release(pco, "pdf_end_write_image");
	}
	return 0;
    } else {			/* in-line image */
	stream *s = pdev->strm;

	stream_puts(s, "BI\n");
	cos_stream_elements_write(piw->data, pdev);
	stream_puts(s, (pdev->binary_ok ? "ID " : "ID\n"));
	cos_stream_contents_write(piw->data, pdev);
	pprints1(s, "\nEI%s\n", piw->end_string);
	COS_FREE(piw->data, "pdf_end_write_image");
	return 1;
    }
}

/* ------ Copy data ------ */

/* Copy the data for a mask or monobit bitmap. */
int
pdf_copy_mask_bits(stream *s, const byte *base, int sourcex, int raster,
		   int w, int h, byte invert)
{
    int yi;

    for (yi = 0; yi < h; ++yi) {
	const byte *data = base + yi * raster + (sourcex >> 3);
	int sbit = sourcex & 7;

	if (sbit == 0) {
	    int nbytes = (w + 7) >> 3;
	    int i;

	    for (i = 0; i < nbytes; ++data, ++i)
		sputc(s, *data ^ invert);
	} else {
	    int wleft = w;
	    int rbit = 8 - sbit;

	    for (; wleft + sbit > 8; ++data, wleft -= 8)
		sputc(s, ((*data << sbit) + (data[1] >> rbit)) ^ invert);
	    if (wleft > 0)
		sputc(s, ((*data << sbit) ^ invert) &
		      (byte) (0xff00 >> wleft));
	}
    }
    return 0;
}

/* Copy the data for a colored image (device pixels). */
int
pdf_copy_color_bits(stream *s, const byte *base, int sourcex, int raster,
		    int w, int h, int bytes_per_pixel)
{
    int yi;

    for (yi = 0; yi < h; ++yi) {
	uint ignore;

	sputs(s, base + sourcex * bytes_per_pixel + yi * raster,
	      w * bytes_per_pixel, &ignore);
    }
    return 0;
}

/* Choose image compression - auxiliary procs */
private inline bool much_bigger__DL(long l1, long l2)
{
    return l1 > 1024*1024 && l2 < l1 / 3;
}
private void
pdf_choose_compression_cos(pdf_image_writer *piw, cos_stream_t *s[2], bool force)
{   /*	Assume s[0] is Flate, s[1] is DCT, s[2] is chooser. */
    long l0, l1;
    int k0, k1;

    l0 = cos_stream_length(s[0]);
    l1 = cos_stream_length(s[1]);
    k0 = s_compr_chooser__get_choice(
	(stream_compr_chooser_state *)piw->binary[2].strm->state, force);
    if (k0 && l0 > 0 && l1 > 0)
	k0--;
    else if (much_bigger__DL(l0, l1))
	k0 = 0; 
    else if (much_bigger__DL(l1, l0) || force)
	k0 = 1; 
    else
       return;
    k1 = 1 - k0;
    s_close_filters(&piw->binary[k0].strm, piw->binary[k0].target);
    s[k0]->cos_procs->release((cos_object_t *)s[k0], "pdf_image_choose_filter");
    s[k0]->written = 1;
    piw->binary[0].strm = piw->binary[k1].strm;
    sclose(piw->binary[2].strm);
    piw->binary[1].strm = piw->binary[2].strm = 0; /* for GC */
    s[k1]->id = piw->pres->object->id;
    piw->pres->object = (cos_object_t *)s[k1];
    piw->data = s[k1];
    piw->alt_writer_count = 1;
}

/* End binary with choosing image compression. */
int
pdf_choose_compression(pdf_image_writer * piw, bool end_binary)
{
    cos_stream_t *s[2];
    s[0] = cos_write_stream_from_pipeline(piw->binary[0].strm);
    s[1] = cos_write_stream_from_pipeline(piw->binary[1].strm);
    if (end_binary) {
	int status;

    	status = s_close_filters(&piw->binary[0].strm, piw->binary[0].target);
	if (status < 0)
	    return status;
	status = s_close_filters(&piw->binary[1].strm, piw->binary[1].target);
	if (status < 0)
	    return status;
    }
    pdf_choose_compression_cos(piw, s, end_binary);
    return 0;
}
