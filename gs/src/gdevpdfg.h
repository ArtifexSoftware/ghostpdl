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
/* Internal graphics interfaces for PDF-writing driver. */

#ifndef gdevpdfg_INCLUDED
#  define gdevpdfg_INCLUDED

#include "gscspace.h"		/* for gs_separation_name */

/* ---------------- Exported by gdevpdfc.c ---------------- */

/* Define standard and short color space names. */
/*
 * Based on Adobe's published PDF documentation, it appears that the
 * abbreviations for the calibrated color spaces were introduced in
 * PDF 1.1 (added to Table 7.3) and removed in PDF 1.2 (Table 8.1)!
 */
typedef struct pdf_color_space_names_s {
    const char *DeviceCMYK;
    const char *DeviceGray;
    const char *DeviceRGB;
    const char *Indexed;
} pdf_color_space_names_t;
#define PDF_COLOR_SPACE_NAMES\
  "/DeviceCMYK", "/DeviceGray", "/DeviceRGB", "/Indexed"
#define PDF_COLOR_SPACE_NAMES_SHORT\
  "/CMYK", "/G", "/RGB", "/I"
extern const pdf_color_space_names_t
    pdf_color_space_names,
    pdf_color_space_names_short;

/* Define an abstract color space type. */
#ifndef gs_color_space_DEFINED
#  define gs_color_space_DEFINED
typedef struct gs_color_space_s gs_color_space;
#endif

/*
 * Define a ColorSpace resource.  We need to retain the range scaling
 * information (if any).
 */
typedef struct pdf_color_space_s pdf_color_space_t;
struct pdf_color_space_s {
    pdf_resource_common(pdf_color_space_t);
    const gs_range_t *ranges;
};
/* The descriptor is public because it is for a resource type. */
#define public_st_pdf_color_space()  /* in gdevpdfc.c */\
  gs_public_st_suffix_add1(st_pdf_color_space, pdf_color_space_t,\
    "pdf_color_space_t", pdf_color_space_enum_ptrs,\
    pdf_color_space_reloc_ptrs, st_pdf_resource, ranges)

/*
 * Create a local Device{Gray,RGB,CMYK} color space corresponding to the
 * given number of components.
 */
int pdf_cspace_init_Device(gs_color_space *pcs, int num_components);

/*
 * Create a PDF color space corresponding to a PostScript color space.
 * For parameterless color spaces, set *pvalue to a (literal) string with
 * the color space name; for other color spaces, create a cos_array_t if
 * necessary and set *pvalue to refer to it.  In the latter case, if
 * by_name is true, return a string /Rxxxx rather than a reference to
 * the actual object.
 *
 * If ppranges is not NULL, then if the domain of the color space had
 * to be scaled (to convert a CIEBased space to ICCBased), store a pointer
 * to the ranges in *ppranges, otherwise set *ppranges to 0.
 */
int pdf_color_space(gx_device_pdf *pdev, cos_value_t *pvalue,
		    const gs_range_t **ppranges,
		    const gs_color_space *pcs,
		    const pdf_color_space_names_t *pcsn,
		    bool by_name);

/* Create colored and uncolored Pattern color spaces. */
int pdf_cs_Pattern_colored(gx_device_pdf *pdev, cos_value_t *pvalue);
int pdf_cs_Pattern_uncolored(gx_device_pdf *pdev, cos_value_t *pvalue);

/* Set the ProcSets bits corresponding to an image color space. */
void pdf_color_space_procsets(gx_device_pdf *pdev,
			      const gs_color_space *pcs);

/* ---------------- Exported by gdevpdfg.c ---------------- */

/* Reset the graphics state parameters to initial values. */
void pdf_reset_graphics(gx_device_pdf *pdev);

/* Set the fill or stroke color. */
/* pdecolor is &pdev->fill_color or &pdev->stroke_color. */
int pdf_set_pure_color(gx_device_pdf *pdev, gx_color_index color,
		       gx_device_color_saved * psc,
		       const psdf_set_color_commands_t *ppscc);
int pdf_set_drawing_color(gx_device_pdf *pdev,
			  const gx_drawing_color *pdc,
			  gx_device_color_saved * psc,
			  const psdf_set_color_commands_t *ppscc);

/*
 * Bring the graphics state up to date for a drawing operation.
 * (Text uses either fill or stroke.)
 */
int pdf_prepare_fill(gx_device_pdf *pdev, const gs_imager_state *pis);
int pdf_prepare_stroke(gx_device_pdf *pdev, const gs_imager_state *pis);
int pdf_prepare_image(gx_device_pdf *pdev, const gs_imager_state *pis);
int pdf_prepare_imagemask(gx_device_pdf *pdev, const gs_imager_state *pis,
			  const gx_drawing_color *pdcolor);
int pdf_save_viewer_state(gx_device_pdf *pdev, stream *s);
int pdf_restore_viewer_state(gx_device_pdf *pdev, stream *s);

/*
 * Convert a string into cos name.
 */
int pdf_string_to_cos_name(gx_device_pdf *pdev, const byte *str, uint len, 
		       cos_value_t *pvalue);

/* ---------------- Exported by gdevpdfj.c ---------------- */

/* ------ Image stream dictionaries ------ */

/* Define the long and short versions of the keys in an image dictionary, */
/* and other strings for images. */
typedef struct pdf_image_names_s {
    pdf_color_space_names_t color_spaces;
    pdf_filter_names_t filter_names;
    const char *BitsPerComponent;
    const char *ColorSpace;
    const char *Decode;
    const char *Height;
    const char *ImageMask;
    const char *Interpolate;
    const char *Width;
} pdf_image_names_t;
#define PDF_IMAGE_PARAM_NAMES\
    "/BitsPerComponent", "/ColorSpace", "/Decode",\
    "/Height", "/ImageMask", "/Interpolate", "/Width"
#define PDF_IMAGE_PARAM_NAMES_SHORT\
    "/BPC", "/CS", "/D", "/H", "/IM", "/I", "/W"
extern const pdf_image_names_t pdf_image_names_full, pdf_image_names_short;

/* Store the values of image parameters other than filters. */
/* pdev is used only for updating procsets. */
/* pcsvalue is not used for masks. */
int pdf_put_image_values(cos_dict_t *pcd, gx_device_pdf *pdev,
			 const gs_pixel_image_t *pic,
			 const pdf_image_names_t *pin,
			 const cos_value_t *pcsvalue);

/* Store filters for an image. */
/* Currently this only saves parameters for CCITTFaxDecode. */
int pdf_put_image_filters(cos_dict_t *pcd, gx_device_pdf *pdev,
			  const psdf_binary_writer * pbw,
			  const pdf_image_names_t *pin);

/* ------ Image writing ------ */

/* Define the maximum size of an in-line image. */
#define MAX_INLINE_IMAGE_BYTES 4000

/*
 * Fill in the image parameters for a device space bitmap.
 * PDF images are always specified top-to-bottom.
 * data_h is the actual number of data rows, which may be less than h.
 */
void pdf_make_bitmap_matrix(gs_matrix * pmat, int x, int y, int w, int h,
			    int h_actual);

/* Put out the gsave and matrix for an image. */
void pdf_put_image_matrix(gx_device_pdf * pdev, const gs_matrix * pmat,
			  floatp y_scale);

/* Put out a reference to an image resource. */
int pdf_do_image(gx_device_pdf * pdev, const pdf_resource_t * pres,
		 const gs_matrix * pimat, bool in_contents);

/* Define the structure for writing an image. */
typedef struct pdf_image_writer_s {
    psdf_binary_writer binary[3];
    int alt_writer_count; /* no. of active elements in writer[] (1,2,3) */
    const pdf_image_names_t *pin;
    pdf_resource_t *pres;	/* XObject resource iff not in-line */
    int height;			/* initially specified image height */
    cos_stream_t *data;
    const char *end_string;	/* string to write after EI if in-line */
    cos_dict_t *named;		/* named dictionary from NI */
} pdf_image_writer;
extern_st(st_pdf_image_writer);	/* public for gdevpdfi.c */
#define public_st_pdf_image_writer() /* in gdevpdfj.c */\
  gs_public_st_composite(st_pdf_image_writer, pdf_image_writer,\
    "pdf_image_writer", pdf_image_writer_enum_ptrs, pdf_image_writer_reloc_ptrs)
#define pdf_image_writer_max_ptrs (psdf_binary_writer_max_ptrs * 3 + 3)

/*
 * Begin writing an image, creating the resource if not in-line, and setting
 * up the binary writer.  If pnamed != 0, it is a dictionary object created
 * by a NI pdfmark.
 */
int pdf_begin_write_image(gx_device_pdf * pdev, pdf_image_writer * piw,
			  gx_bitmap_id id, int w, int h,
			  cos_dict_t *pnamed, bool in_line,
			  int alt_writer_count);

/* Begin writing the image data, setting up the dictionary and filters. */
int pdf_begin_image_data(gx_device_pdf * pdev, pdf_image_writer * piw,
			 const gs_pixel_image_t * pim,
			 const cos_value_t *pcsvalue, 
			 int alt_writer_index);

/* Copy the data for a mask or monobit bitmap. */
int pdf_copy_mask_bits(stream *s, const byte *base, int sourcex,
		       int raster, int w, int h, byte invert);

/* Copy the data for a colored image (device pixels). */
int pdf_copy_color_bits(stream *s, const byte *base, int sourcex,
			int raster, int w, int h, int bytes_per_pixel);

/* Complete image data. */
int
pdf_complete_image_data(gx_device_pdf *pdev, pdf_image_writer *piw, int data_h,
			int width, int bits_per_pixel);

/* Finish writing the binary image data. */
int pdf_end_image_binary(gx_device_pdf *pdev, pdf_image_writer *piw,
			 int data_h);

/*
 * Finish writing an image.  If in-line, write the BI/dict/ID/data/EI and
 * return 1; if a resource, write the resource definition and return 0.
 */
int pdf_end_write_image(gx_device_pdf * pdev, pdf_image_writer * piw);

/*
 *  Make alternative stream for image compression choice.
 */
int pdf_make_alt_stream(gx_device_pdf * pdev, psdf_binary_writer * piw);

/* 
 * End binary with choosing image compression. 
 */
int pdf_choose_compression(pdf_image_writer * piw, bool end_binary);

/* ---------------- Exported by gdevpdfv.c ---------------- */

/* Write a color value. */
int pdf_put_drawing_color(gx_device_pdf *pdev, const gx_drawing_color *pdc,
			  const psdf_set_color_commands_t *ppscc);

#endif /* gdevpdfg_INCLUDED */
