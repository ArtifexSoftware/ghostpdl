/* Copyright (C) 1996, 2000 Aladdin Enterprises.  All rights reserved.
  
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
/* PDF-writing driver */
#include "fcntl_.h"
#include "memory_.h"
#include "string_.h"
#include "time_.h"
#include "unistd_.h"
#include "gx.h"
#include "gp.h"			/* for gp_get_realtime */
#include "gserrors.h"
#include "gxdevice.h"
#include "gdevpdfx.h"
#include "gdevpdfg.h"		/* only for pdf_reset_graphics */
#include "gdevpdfo.h"
#include "gdevpdt.h"

/* Define the default language level and PDF compatibility level. */
/* Acrobat 4 (PDF 1.3) is the default. */
#define PSDF_VERSION_INITIAL psdf_version_ll3
#define PDF_COMPATIBILITY_LEVEL_INITIAL 1.3

/* Define the size of internal stream buffers. */
/* (This is not a limitation, it only affects performance.) */
#define sbuf_size 512

/* GC descriptors */
private_st_pdf_page();
gs_private_st_element(st_pdf_page_element, pdf_page_t, "pdf_page_t[]",
		      pdf_page_elt_enum_ptrs, pdf_page_elt_reloc_ptrs,
		      st_pdf_page);
private_st_device_pdfwrite();
private_st_pdf_substream_save();
private_st_pdf_substream_save_element();

/* GC procedures */
private 
ENUM_PTRS_WITH(device_pdfwrite_enum_ptrs, gx_device_pdf *pdev)
{
    index -= gx_device_pdf_num_ptrs + gx_device_pdf_num_strings;
    if (index < NUM_RESOURCE_TYPES * NUM_RESOURCE_CHAINS)
	ENUM_RETURN(pdev->resources[index / NUM_RESOURCE_CHAINS].chains[index % NUM_RESOURCE_CHAINS]);
    index -= NUM_RESOURCE_TYPES * NUM_RESOURCE_CHAINS;
    if (index <= pdev->outline_depth)
	ENUM_RETURN(pdev->outline_levels[index].first.action);
    index -= pdev->outline_depth + 1;
    if (index <= pdev->outline_depth)
	ENUM_RETURN(pdev->outline_levels[index].last.action);
    index -= pdev->outline_depth + 1;
    ENUM_PREFIX(st_device_psdf, 0);
}
#define e1(i,elt) ENUM_PTR(i, gx_device_pdf, elt);
gx_device_pdf_do_ptrs(e1)
#undef e1
#define e1(i,elt) ENUM_STRING_PTR(i + gx_device_pdf_num_ptrs, gx_device_pdf, elt);
gx_device_pdf_do_strings(e1)
#undef e1
ENUM_PTRS_END
private RELOC_PTRS_WITH(device_pdfwrite_reloc_ptrs, gx_device_pdf *pdev)
{
    RELOC_PREFIX(st_device_psdf);
#define r1(i,elt) RELOC_PTR(gx_device_pdf,elt);
    gx_device_pdf_do_ptrs(r1)
#undef r1
#define r1(i,elt) RELOC_STRING_PTR(gx_device_pdf,elt);
	gx_device_pdf_do_strings(r1)
#undef r1
    {
	int i, j;

	for (i = 0; i < NUM_RESOURCE_TYPES; ++i)
	    for (j = 0; j < NUM_RESOURCE_CHAINS; ++j)
		RELOC_PTR(gx_device_pdf, resources[i].chains[j]);
	for (i = 0; i <= pdev->outline_depth; ++i) {
	    RELOC_PTR(gx_device_pdf, outline_levels[i].first.action);
	    RELOC_PTR(gx_device_pdf, outline_levels[i].last.action);
	}
    }
}
RELOC_PTRS_END
/* Even though device_pdfwrite_finalize is the same as gx_device_finalize, */
/* we need to implement it separately because st_composite_final */
/* declares all 3 procedures as private. */
private void
device_pdfwrite_finalize(void *vpdev)
{
    gx_device_finalize(vpdev);
}

/* Driver procedures */
private dev_proc_open_device(pdf_open);
private dev_proc_output_page(pdf_output_page);
private dev_proc_close_device(pdf_close);
/* Driver procedures defined in other files are declared in gdevpdfx.h. */

#ifndef X_DPI
#  define X_DPI 720
#endif
#ifndef Y_DPI
#  define Y_DPI 720
#endif

const gx_device_pdf gs_pdfwrite_device =
{std_device_dci_type_body(gx_device_pdf, 0, "pdfwrite",
			  &st_device_pdfwrite,
			  DEFAULT_WIDTH_10THS * X_DPI / 10,
			  DEFAULT_HEIGHT_10THS * Y_DPI / 10,
			  X_DPI, Y_DPI,
			  3, 24, 255, 255, 256, 256),
 {pdf_open,
  gx_upright_get_initial_matrix,
  NULL,				/* sync_output */
  pdf_output_page,
  pdf_close,
  gx_default_rgb_map_rgb_color,
  gx_default_rgb_map_color_rgb,
  gdev_pdf_fill_rectangle,
  NULL,				/* tile_rectangle */
  gdev_pdf_copy_mono,
  gdev_pdf_copy_color,
  NULL,				/* draw_line */
  psdf_get_bits,		/* get_bits */
  gdev_pdf_get_params,
  gdev_pdf_put_params,
  NULL,				/* map_cmyk_color */
  NULL,				/* get_xfont_procs */
  NULL,				/* get_xfont_device */
  NULL,				/* map_rgb_alpha_color */
  gx_page_device_get_page_device,
  NULL,				/* get_alpha_bits */
  NULL,				/* copy_alpha */
  NULL,				/* get_band */
  NULL,				/* copy_rop */
  gdev_pdf_fill_path,
  gdev_pdf_stroke_path,
  gdev_pdf_fill_mask,
  NULL,				/* fill_trapezoid */
  NULL,				/* fill_parallelogram */
  NULL,				/* fill_triangle */
  NULL,				/* draw_thin_line */
  NULL,				/* begin_image */
  NULL,				/* image_data */
  NULL,				/* end_image */
  gdev_pdf_strip_tile_rectangle,
  NULL,				/* strip_copy_rop */
  NULL,				/* get_clipping_box */
  gdev_pdf_begin_typed_image,
  psdf_get_bits_rectangle,	/* get_bits_rectangle */
  NULL,				/* map_color_rgb_alpha */
  psdf_create_compositor,	/* create_compositor */
  NULL,				/* get_hardware_params */
  gdev_pdf_text_begin,
  NULL,				/* finish_copydevice */
  NULL,				/* begin_transparency_group */
  NULL,				/* end_transparency_group */
  NULL,				/* begin_transparency_mask */
  NULL,				/* end_transparency_mask */
  NULL,				/* discard_transparency_layer */
  NULL,				/* get_color_mapping_procs */
  NULL,				/* get_color_comp_index */
  NULL,				/* encode_color */
  NULL,				/* decode_color */
  gdev_pdf_pattern_manage 	/* pattern_manage */
 },
 psdf_initial_values(PSDF_VERSION_INITIAL, 0 /*false */ ),  /* (!ASCII85EncodePages) */
 PDF_COMPATIBILITY_LEVEL_INITIAL,  /* CompatibilityLevel */
 -1,				/* EndPage */
 1,				/* StartPage */
 1 /*true*/,			/* Optimize */
 0 /*false*/,			/* ParseDSCCommentsForDocInfo */
 1 /*true*/,			/* ParseDSCComments */
 0 /*false*/,			/* EmitDSCWarnings */
 0 /*false*/,			/* CreateJobTicket */
 0 /*false*/,			/* PreserveEPSInfo */
 1 /*true*/,			/* AutoPositionEPSFiles */
 1 /*true*/,			/* PreserveCopyPage */
 0 /*false*/,			/* UsePrologue */
 0,				/* OffOptimizations */
 1 /*true*/,			/* ReAssignCharacters */
 1 /*true*/,			/* ReEncodeCharacters */
 1,				/* FirstObjectNumber */
 1 /*true*/,			/* CompressFonts */
 0 /*false*/,			/* is_EPS */
 {-1, -1},			/* doc_dsc_info */
 {-1, -1},			/* page_dsc_info */
 0 /*false*/,			/* fill_overprint */
 0 /*false*/,			/* stroke_overprint */
 0,				/* overprint_mode */
 gs_no_id,			/* halftone_id */
 {gs_no_id, gs_no_id, gs_no_id, gs_no_id}, /* transfer_ids */
 0,				/* transfer_not_identity */
 gs_no_id,			/* black_generation_id */
 gs_no_id,			/* undercolor_removal_id */
 pdf_compress_none,		/* compression */
 {{0}},				/* xref */
 {{0}},				/* asides */
 {{0}},				/* streams */
 {{0}},				/* pictures */
 0,				/* next_id */
 0,				/* Catalog */
 0,				/* Info */
 0,				/* Pages */
 0,				/* outlines_id */
 0,				/* next_page */
 0,				/* contents_id */
 PDF_IN_NONE,			/* context */
 0,				/* contents_length_id */
 0,				/* contents_pos */
 NoMarks,			/* procsets */
 0,				/* text */
 {{0}},				/* text_rotation */
 0,				/* pages */
 0,				/* num_pages */
 1,				/* used_mask */
 {
     {
	 {0}}},			/* resources */
 {0},				/* cs_Patterns */
 {0},				/* Identity_ToUnicode_CMaps */
 0,				/* last_resource */
 {
     {
	 {0}}},			/* outline_levels */
 0,				/* outline_depth */
 0,				/* closed_outline_depth */
 0,				/* outlines_open */
 0,				/* articles */
 0,				/* Dests */
 0,				/* global_named_objects */
 0,				/* local_named_objects */
 0,				/* NI_stack */
 0,				/* Namespace_stack */
 0,				/* open_graphics */
 0,				/* font_cache */
 {0, 0},			/* char_width */
 0,				/* clip_path */
 0,                             /* PageLabels */
 -1,                            /* PageLabels_current_page */
 0,                             /* PageLabels_current_label */
 0,				/* */
 {				/* vgstack[2] */
    {0}, {0}
 },
 0,				/* vgstack_depth */
 0,				/* vgstack_bottom */
 {0},				/* vg_initial */
 false,				/* vg_initial_set */
 0,				/* sbstack_size */
 0,				/* sbstack_depth */
 0,				/* sbstack */
 0,				/* substream_Resources */
 1,				/* pcm_color_info_index == DeviceRGB */
 false,				/* skip_colors */
 false				/* AR4_save_bug */
};

/* ---------------- Device open/close ---------------- */

/* Close and remove temporary files. */
private int
pdf_close_temp_file(gx_device_pdf *pdev, pdf_temp_file_t *ptf, int code)
{
    int err = 0;
    FILE *file = ptf->file;

    /*
     * ptf->strm == 0 or ptf->file == 0 is only possible if this procedure
     * is called to clean up during initialization failure, but ptf->strm
     * might not be open if it was finalized before the device was closed.
     */
    if (ptf->strm) {
	if (s_is_valid(ptf->strm)) {
	    sflush(ptf->strm);
	    /* Prevent freeing the stream from closing the file. */
	    ptf->strm->file = 0;
	} else
	    ptf->file = file = 0;	/* file was closed by finalization */
	gs_free_object(pdev->pdf_memory, ptf->strm_buf,
		       "pdf_close_temp_file(strm_buf)");
	ptf->strm_buf = 0;
	gs_free_object(pdev->pdf_memory, ptf->strm,
		       "pdf_close_temp_file(strm)");
	ptf->strm = 0;
    }
    if (file) {
	err = ferror(file) | fclose(file);
	unlink(ptf->file_name);
	ptf->file = 0;
    }
    ptf->save_strm = 0;
    return
	(code < 0 ? code : err != 0 ? gs_note_error(gs_error_ioerror) : code);
}
private int
pdf_close_files(gx_device_pdf * pdev, int code)
{
    code = pdf_close_temp_file(pdev, &pdev->pictures, code);
    code = pdf_close_temp_file(pdev, &pdev->streams, code);
    code = pdf_close_temp_file(pdev, &pdev->asides, code);
    return pdf_close_temp_file(pdev, &pdev->xref, code);
}

/* Reset the state of the current page. */
private void
pdf_reset_page(gx_device_pdf * pdev)
{
    pdev->page_dsc_info = gs_pdfwrite_device.page_dsc_info;
    pdev->contents_id = 0;
    pdf_reset_graphics(pdev);
    pdev->procsets = NoMarks;
    memset(pdev->cs_Patterns, 0, sizeof(pdev->cs_Patterns));	/* simplest to create for each page */
    pdf_reset_text_page(pdev->text);
}

/* Open a temporary file, with or without a stream. */
private int
pdf_open_temp_file(gx_device_pdf *pdev, pdf_temp_file_t *ptf)
{
    char fmode[4];

    strcpy(fmode, "w+");
    strcat(fmode, gp_fmode_binary_suffix);
    ptf->file =
	gp_open_scratch_file(gp_scratch_file_name_prefix,
			     ptf->file_name, fmode);
    if (ptf->file == 0)
	return_error(gs_error_invalidfileaccess);
    return 0;
}
private int
pdf_open_temp_stream(gx_device_pdf *pdev, pdf_temp_file_t *ptf)
{
    int code = pdf_open_temp_file(pdev, ptf);

    if (code < 0)
	return code;
    ptf->strm = s_alloc(pdev->pdf_memory, "pdf_open_temp_stream(strm)");
    if (ptf->strm == 0)
	return_error(gs_error_VMerror);
    ptf->strm_buf = gs_alloc_bytes(pdev->pdf_memory, sbuf_size,
				   "pdf_open_temp_stream(strm_buf)");
    if (ptf->strm_buf == 0) {
	gs_free_object(pdev->pdf_memory, ptf->strm,
		       "pdf_open_temp_stream(strm)");
	ptf->strm = 0;
	return_error(gs_error_VMerror);
    }
    swrite_file(ptf->strm, ptf->file, ptf->strm_buf, sbuf_size);
    return 0;
}

/* Initialize the IDs allocated at startup. */
void
pdf_initialize_ids(gx_device_pdf * pdev)
{
    gs_param_string nstr;

    pdev->next_id = pdev->FirstObjectNumber;

    /* Initialize the Catalog. */

    param_string_from_string(nstr, "{Catalog}");
    pdf_create_named_dict(pdev, &nstr, &pdev->Catalog, 0L);

    /* Initialize the Info dictionary. */

    param_string_from_string(nstr, "{DocInfo}");
    pdf_create_named_dict(pdev, &nstr, &pdev->Info, 0L);
    {
	char buf[PDF_MAX_PRODUCER];

	pdf_store_default_Producer(buf);
	cos_dict_put_c_key_string(pdev->Info, "/Producer", (byte *)buf,
				  strlen(buf));
    }
    /*
     * Acrobat Distiller sets CreationDate and ModDate to the current
     * date and time, rather than (for example) %%CreationDate from the
     * PostScript file.  We think this is wrong, but we do the same.
     */
    {
	struct tm tms;
	time_t t;
	char buf[1+2+4+2+2+2+2+2+2+1+1]; /* (D:yyyymmddhhmmss)\0 */

	time(&t);
	tms = *localtime(&t);
	sprintf(buf,
		"(D:%04d%02d%02d%02d%02d%02d)",
		tms.tm_year + 1900, tms.tm_mon + 1, tms.tm_mday,
		tms.tm_hour, tms.tm_min, tms.tm_sec);
	cos_dict_put_c_key_string(pdev->Info, "/CreationDate", (byte *)buf,
				  strlen(buf));
	cos_dict_put_c_key_string(pdev->Info, "/ModDate", (byte *)buf,
				  strlen(buf));
    }

    /* Allocate the root of the pages tree. */

    pdf_create_named_dict(pdev, NULL, &pdev->Pages, 0L);
}

#ifdef __DECC
/* The ansi alias rules are violated in this next routine.  Tell the compiler
   to ignore this.
 */
#pragma optimize save
#pragma optimize ansi_alias=off
#endif
/* Update the color mapping procedures after setting ProcessColorModel. */
void
pdf_set_process_color_model(gx_device_pdf * pdev, int index)
{

    static gx_device_color_info pcm_color_info[] = {
	dci_values(1, 8, 255, 0, 256, 0),
	dci_values(3, 24, 255, 255, 256, 256),
	dci_values(4, 32, 255, 255, 256, 256)
    };

    pcm_color_info[0].separable_and_linear = GX_CINFO_SEP_LIN;
    pcm_color_info[1].separable_and_linear = GX_CINFO_SEP_LIN;
    pcm_color_info[2].separable_and_linear = GX_CINFO_SEP_LIN;
    pdev->color_info = pcm_color_info[index];
    pdev->pcm_color_info_index = index;
    /*
     * The conversion from PS to PDF should be transparent as possible.
     * Particularly it should not change representation of colors.
     * Perhaps due to historical reasons the source color information
     * sometimes isn't accessible from device methods, and
     * therefore they perform a mapping of colors to 
     * an output color model. Here we handle some color models,
     * which were selected almost due to antique reasons.
     */
    if (!strcmp(pdev->color_info.cm_name, "DeviceGray")) {
	set_dev_proc(pdev, map_rgb_color, gx_default_gray_map_rgb_color);
	set_dev_proc(pdev, map_color_rgb, gx_default_gray_map_color_rgb);
	set_dev_proc(pdev, map_cmyk_color, NULL);
        set_dev_proc(pdev, get_color_mapping_procs, gx_default_DevGray_get_color_mapping_procs);
        set_dev_proc(pdev, get_color_comp_index, gx_default_DevGray_get_color_comp_index);
        set_dev_proc(pdev, encode_color, gx_default_gray_encode);
        set_dev_proc(pdev, decode_color, gx_default_decode_color);
    } else if (!strcmp(pdev->color_info.cm_name, "DeviceRGB")) {
	set_dev_proc(pdev, map_rgb_color, gx_default_rgb_map_rgb_color);
	set_dev_proc(pdev, map_color_rgb, gx_default_rgb_map_color_rgb);
	set_dev_proc(pdev, map_cmyk_color, NULL);
        set_dev_proc(pdev, get_color_mapping_procs, gx_default_DevRGB_get_color_mapping_procs);
        set_dev_proc(pdev, get_color_comp_index, gx_default_DevRGB_get_color_comp_index);
        set_dev_proc(pdev, encode_color, gx_default_rgb_map_rgb_color);
        set_dev_proc(pdev, decode_color, gx_default_rgb_map_color_rgb);
    } else if (!strcmp(pdev->color_info.cm_name, "DeviceCMYK")) {
	set_dev_proc(pdev, map_rgb_color, NULL);
	set_dev_proc(pdev, map_color_rgb, cmyk_8bit_map_color_rgb);
       /* possible problems with aliassing on next statement */
	set_dev_proc(pdev, map_cmyk_color, cmyk_8bit_map_cmyk_color);
        set_dev_proc(pdev, get_color_mapping_procs, gx_default_DevCMYK_get_color_mapping_procs);
        set_dev_proc(pdev, get_color_comp_index, gx_default_DevCMYK_get_color_comp_index);
        set_dev_proc(pdev, encode_color, cmyk_8bit_map_cmyk_color);
        set_dev_proc(pdev, decode_color, cmyk_8bit_map_color_rgb);
    } else {	/* can't happen - see the call from gdev_pdf_put_params. */
 	DO_NOTHING;
    }
}
#ifdef __DECC
#pragma optimize restore
#endif

/*
 * Reset the text state parameters to initial values.
 */
void
pdf_reset_text(gx_device_pdf * pdev)
{
    pdf_reset_text_state(pdev->text);
}

/* Open the device. */
private int
pdf_open(gx_device * dev)
{
    gx_device_pdf *const pdev = (gx_device_pdf *) dev;
    gs_memory_t *mem = pdev->pdf_memory = gs_memory_stable(pdev->memory);
    int code;

    if ((code = pdf_open_temp_file(pdev, &pdev->xref)) < 0 ||
	(code = pdf_open_temp_stream(pdev, &pdev->asides)) < 0 ||
	(code = pdf_open_temp_stream(pdev, &pdev->streams)) < 0 ||
	(code = pdf_open_temp_stream(pdev, &pdev->pictures)) < 0
	)
	goto fail;
    code = gdev_vector_open_file((gx_device_vector *) pdev, sbuf_size);
    if (code < 0)
	goto fail;
    gdev_vector_init((gx_device_vector *) pdev);
    pdev->vec_procs = &pdf_vector_procs;
    pdev->fill_options = pdev->stroke_options = gx_path_type_optimize;
    /* Set in_page so the vector routines won't try to call */
    /* any vector implementation procedures. */
    pdev->in_page = true;
    /*
     * pdf_initialize_ids allocates some (global) named objects, so we must
     * initialize the named objects dictionary now.
     */
    pdev->local_named_objects =
	pdev->global_named_objects =
	cos_dict_alloc(pdev, "pdf_open(global_named_objects)");
    /* Initialize internal structures that don't have IDs. */
    pdev->NI_stack = cos_array_alloc(pdev, "pdf_open(NI stack)");
    pdev->Namespace_stack = cos_array_alloc(pdev, "pdf_open(Namespace stack)");
    pdf_initialize_ids(pdev);
    /* Now create a new dictionary for the local named objects. */
    pdev->local_named_objects =
	cos_dict_alloc(pdev, "pdf_open(local_named_objects)");
    pdev->outlines_id = 0;
    pdev->next_page = 0;
    pdev->text = pdf_text_data_alloc(mem);
    pdev->sbstack_size = count_of(pdev->vgstack); /* Overestimated a few. */
    pdev->sbstack = gs_alloc_struct_array(mem, pdev->sbstack_size, pdf_substream_save,
				 &st_pdf_substream_save_element, "pdf_open");
    pdev->pages =
	gs_alloc_struct_array(mem, initial_num_pages, pdf_page_t,
			      &st_pdf_page_element, "pdf_open(pages)");
    if (pdev->text == 0 || pdev->pages == 0 || pdev->sbstack == 0) {
	code = gs_error_VMerror;
	goto fail;
    }
    memset(pdev->sbstack, 0, pdev->sbstack_size * sizeof(pdf_substream_save));
    memset(pdev->pages, 0, initial_num_pages * sizeof(pdf_page_t));
    pdev->num_pages = initial_num_pages;
    {
	int i, j;

	for (i = 0; i < NUM_RESOURCE_TYPES; ++i)
	    for (j = 0; j < NUM_RESOURCE_CHAINS; ++j)
		pdev->resources[i].chains[j] = 0;
    }
    pdev->outline_levels[0].first.id = 0;
    pdev->outline_levels[0].left = max_int;
    pdev->outline_levels[0].first.action = 0;
    pdev->outline_levels[0].last.action = 0;
    pdev->outline_depth = 0;
    pdev->closed_outline_depth = 0;
    pdev->outlines_open = 0;
    pdev->articles = 0;
    pdev->Dests = 0;
    /* {global,local}_named_objects was initialized above */
    pdev->PageLabels = 0;
    pdev->PageLabels_current_page = 0;
    pdev->PageLabels_current_label = 0;
    pdev->open_graphics = 0;
    pdev->pte = NULL;
    pdf_reset_page(pdev);
    return 0;
  fail:
    return pdf_close_files(pdev, code);
}

/* Detect I/O errors. */
private int
pdf_ferror(gx_device_pdf *pdev)
{
    fflush(pdev->file);
    fflush(pdev->xref.file);
    sflush(pdev->strm);
    sflush(pdev->asides.strm);
    sflush(pdev->streams.strm);
    sflush(pdev->pictures.strm);
    return ferror(pdev->file) || ferror(pdev->xref.file) ||
	ferror(pdev->asides.file) || ferror(pdev->streams.file) ||
	ferror(pdev->pictures.file);
}

/* Compute the dominant text orientation of a page. */
private int
pdf_dominant_rotation(const pdf_text_rotation_t *ptr)
{
    int i, imax = -1;
    long max_count = -1;
    static const int angles[] = { pdf_text_rotation_angle_values };

    for (i = 0; i < countof(ptr->counts); ++i) {
	long count = ptr->counts[i];

	if (count > max_count)
	    imax = i, max_count = count;
    }
    return (imax < 0 ? imax : angles[imax]);
}

/* Print a Rotate command, if requested and possible. */
private void
pdf_print_orientation(gx_device_pdf * pdev, pdf_page_t *page)
{
    stream *s = pdev->strm;
    int dsc_orientation = -1;
    const pdf_page_dsc_info_t *ppdi;

    if (pdev->params.AutoRotatePages == arp_None)
	return; /* Not requested. */

    ppdi = (page != NULL ? &page->dsc_info : &pdev->doc_dsc_info);

    /* Determine DSC orientation : */
    if (ppdi->viewing_orientation >= 0)
	dsc_orientation = ppdi->viewing_orientation;
    else if (ppdi->orientation >= 0)
	dsc_orientation = ppdi->orientation;
    if ((page == NULL && pdev->params.AutoRotatePages == arp_All) || /* document */
        (page != NULL && page->text_rotation.Rotate >= 0) || /* page */
	dsc_orientation >= 0 /* have DSC */) {
        const pdf_text_rotation_t *ptr = 
	    (page != NULL ? &page->text_rotation : &pdev->text_rotation);
	int angle = -1;
	const gs_point *pbox = &(page != NULL ? page : &pdev->pages[0])->MediaBox;

	if (dsc_orientation >= 0 && pbox->x > pbox->y) {
	    /* The page is in landscape format. Adjust the rotation accordingly. */
	    dsc_orientation ^= 1;
	}

	/* Combine DSC rotation with text rotation : */
	if (dsc_orientation == 0) {
	    if (ptr->Rotate == 0 || ptr->Rotate == 180)
		angle = ptr->Rotate;
	} else if (dsc_orientation == 1) {
	    if (ptr->Rotate == 90 || ptr->Rotate == 270)
		angle = ptr->Rotate;
	}

	/* If not combinable, prefer text rotation : */
	if (angle < 0) {
	    if (ptr->Rotate >= 0)
		angle = ptr->Rotate;
	    else
		angle = dsc_orientation * 90;
	}

	/* If got some, write it out : */
	if (angle >= 0)
	    pprintd1(s, "/Rotate %d", angle);
    }
}


/* Close the current page. */
private int
pdf_close_page(gx_device_pdf * pdev)
{
    int page_num = ++(pdev->next_page);
    pdf_page_t *page;
    int code;

    /*
     * If the very first page is blank, we need to open the document
     * before doing anything else.
     */

    pdf_open_document(pdev);
    pdf_close_contents(pdev, true);

    /*
     * We can't write the page object or the annotations array yet, because
     * later pdfmarks might add elements to them.  Write the other objects
     * that the page references, and record what we'll need later.
     *
     * Start by making sure the pages array element exists.
     */

    pdf_page_id(pdev, page_num);
    page = &pdev->pages[page_num - 1];
    page->MediaBox.x = pdev->MediaSize[0];
    page->MediaBox.y = pdev->MediaSize[1];
    page->contents_id = pdev->contents_id;
    /* pdf_store_page_resources sets procsets, resource_ids[]. */
    code = pdf_store_page_resources(pdev, page);
    if (code < 0)
	return code;

    /* Write the Functions. */

    pdf_write_resource_objects(pdev, resourceFunction);
    pdf_free_resource_objects(pdev, resourceFunction);

    /* Close use of text on the page. */

    pdf_close_text_page(pdev);

    /* Accumulate text rotation. */

    page->text_rotation.Rotate =
	(pdev->params.AutoRotatePages == arp_PageByPage ?
	 pdf_dominant_rotation(&page->text_rotation) : -1);
    {
	int i;

	for (i = 0; i < countof(page->text_rotation.counts); ++i)
	    pdev->text_rotation.counts[i] += page->text_rotation.counts[i];
    }

    /* Record information from DSC comments. */

    page->dsc_info = pdev->page_dsc_info;
    if (page->dsc_info.orientation < 0)
	page->dsc_info.orientation = pdev->doc_dsc_info.orientation;
    if (page->dsc_info.bounding_box.p.x >= page->dsc_info.bounding_box.q.x ||
	page->dsc_info.bounding_box.p.y >= page->dsc_info.bounding_box.q.y
	)
	page->dsc_info.bounding_box = pdev->doc_dsc_info.bounding_box;

    /* Finish up. */

    pdf_reset_page(pdev);
    return (pdf_ferror(pdev) ? gs_note_error(gs_error_ioerror) : 0);
}

/* Write the page object. */
private double
round_box_coord(floatp xy)
{
    return (int)(xy * 100 + 0.5) / 100.0;
}
private int
pdf_write_page(gx_device_pdf *pdev, int page_num)
{
    long page_id = pdf_page_id(pdev, page_num);
    pdf_page_t *page = &pdev->pages[page_num - 1];
    stream *s;

    pdf_open_obj(pdev, page_id);
    s = pdev->strm;
    pprintg2(s, "<</Type/Page/MediaBox [0 0 %g %g]\n",
	     round_box_coord(page->MediaBox.x),
	     round_box_coord(page->MediaBox.y));
    pdf_print_orientation(pdev, page);
    pprintld1(s, "/Parent %ld 0 R\n", pdev->Pages->id);
    stream_puts(s, "/Resources<</ProcSet[/PDF");
    if (page->procsets & ImageB)
	stream_puts(s, " /ImageB");
    if (page->procsets & ImageC)
	stream_puts(s, " /ImageC");
    if (page->procsets & ImageI)
	stream_puts(s, " /ImageI");
    if (page->procsets & Text)
	stream_puts(s, " /Text");
    stream_puts(s, "]\n");
    {
	int i;

	for (i = 0; i < countof(page->resource_ids); ++i)
	    if (page->resource_ids[i]) {
		stream_puts(s, pdf_resource_type_names[i]);
		pprintld1(s, " %ld 0 R\n", page->resource_ids[i]);
	    }
    }
    stream_puts(s, ">>\n");

    /* Write the annotations array if any. */

    if (page->Annots) {
	stream_puts(s, "/Annots");
	COS_WRITE(page->Annots, pdev);
	COS_FREE(page->Annots, "pdf_write_page(Annots)");
	page->Annots = 0;
    }
    /*
     * The PDF documentation allows, and this code formerly emitted,
     * a Contents entry whose value was an empty array.  Acrobat Reader
     * 3 and 4 accept this, but Acrobat Reader 5.0 rejects it.
     * Fortunately, the Contents entry is optional.
     */
    if (page->contents_id != 0)
	pprintld1(s, "/Contents %ld 0 R\n", page->contents_id);

    /* Write any elements stored by pdfmarks. */

    cos_dict_elements_write(page->Page, pdev);

    stream_puts(s, ">>\n");
    pdf_end_obj(pdev);
    return 0;
}

/* Wrap up ("output") a page. */
private int
pdf_output_page(gx_device * dev, int num_copies, int flush)
{
    gx_device_pdf *const pdev = (gx_device_pdf *) dev;
    int code = pdf_close_page(pdev);

    return (code < 0 ? code :
	    pdf_ferror(pdev) ? gs_note_error(gs_error_ioerror) :
	    gx_finish_output_page(dev, num_copies, flush));
}

/* Close the device. */
private int
pdf_close(gx_device * dev)
{
    gx_device_pdf *const pdev = (gx_device_pdf *) dev;
    gs_memory_t *mem = pdev->pdf_memory;
    stream *s;
    FILE *tfile = pdev->xref.file;
    long xref;
    long resource_pos;
    long Catalog_id = pdev->Catalog->id, Info_id = pdev->Info->id,
	Pages_id = pdev->Pages->id;
    long Threads_id = 0;
    bool partial_page = (pdev->contents_id != 0 && pdev->next_page != 0);
    int code = 0, code1;

    /*
     * If this is an EPS file, or if the file didn't end with a showpage for
     * some other reason, or if the file has produced no marks at all, we
     * need to tidy up a little so as not to produce illegal PDF.  However,
     * if there is at least one complete page, we discard any leftover
     * marks.
     */
    if (pdev->next_page == 0) {
	code = pdf_open_page(pdev, PDF_IN_STREAM);

	if (code < 0)
	    return code;
    }
    if (pdev->contents_id != 0)
	pdf_close_page(pdev);

    /* Write the page objects. */

    {
	int i;

	for (i = 1; i <= pdev->next_page; ++i)
	    pdf_write_page(pdev, i);
    }

    /* Write the font resources and related resources. */

    code1 = pdf_close_text_document(pdev);
    if (code >= 0)
	code = code1;
    code1 = pdf_write_resource_objects(pdev, resourceCMap);
    if (code >= 0)
	code = code1;
    code1 = pdf_free_resource_objects(pdev, resourceCMap);
    if (code >= 0)
	code = code1;

    /* Create the Pages tree. */

    pdf_open_obj(pdev, Pages_id);
    s = pdev->strm;
    stream_puts(s, "<< /Type /Pages /Kids [\n");
    /* Omit the last page if it was incomplete. */
    if (partial_page)
	--(pdev->next_page);
    {
	int i;

	for (i = 0; i < pdev->next_page; ++i)
	    pprintld1(s, "%ld 0 R\n", pdev->pages[i].Page->id);
    }
    pprintd1(s, "] /Count %d\n", pdev->next_page);
    pdev->text_rotation.Rotate = pdf_dominant_rotation(&pdev->text_rotation);
    pdf_print_orientation(pdev, NULL);
    cos_dict_elements_write(pdev->Pages, pdev);
    stream_puts(s, ">>\n");
    pdf_end_obj(pdev);

    /* Close outlines and articles. */

    if (pdev->outlines_id != 0) {
	/* depth > 0 is only possible for an incomplete outline tree. */
	while (pdev->outline_depth > 0)
	    pdfmark_close_outline(pdev);
	pdfmark_close_outline(pdev);
	pdf_open_obj(pdev, pdev->outlines_id);
	pprintd1(s, "<< /Count %d", pdev->outlines_open);
	pprintld2(s, " /First %ld 0 R /Last %ld 0 R >>\n",
		  pdev->outline_levels[0].first.id,
		  pdev->outline_levels[0].last.id);
	pdf_end_obj(pdev);
    }
    if (pdev->articles != 0) {
	pdf_article_t *part;

	/* Write the remaining information for each article. */
	for (part = pdev->articles; part != 0; part = part->next)
	    pdfmark_write_article(pdev, part);
    }

    /* Write named destinations.  (We can't free them yet.) */

    if (pdev->Dests)
	COS_WRITE_OBJECT(pdev->Dests, pdev);

    /* Write the PageLabel array */
    pdfmark_end_pagelabels(pdev);
    if (pdev->PageLabels) {
	COS_WRITE_OBJECT(pdev->PageLabels, pdev);
    }

    /* Write the Catalog. */

    /*
     * The PDF specification requires Threads to be an indirect object.
     * Write the threads now, if any.
     */
    if (pdev->articles != 0) {
	pdf_article_t *part;

	Threads_id = pdf_begin_obj(pdev);
	s = pdev->strm;
	stream_puts(s, "[ ");
	while ((part = pdev->articles) != 0) {
	    pdev->articles = part->next;
	    pprintld1(s, "%ld 0 R\n", part->contents->id);
	    COS_FREE(part->contents, "pdf_close(article contents)");
	    gs_free_object(mem, part, "pdf_close(article)");
	}
	stream_puts(s, "]\n");
	pdf_end_obj(pdev);
    }
    pdf_open_obj(pdev, Catalog_id);
    s = pdev->strm;
    stream_puts(s, "<<");
    pprintld1(s, "/Type /Catalog /Pages %ld 0 R\n", Pages_id);
    if (pdev->outlines_id != 0)
	pprintld1(s, "/Outlines %ld 0 R\n", pdev->outlines_id);
    if (Threads_id)
	pprintld1(s, "/Threads %ld 0 R\n", Threads_id);
    if (pdev->Dests)
	pprintld1(s, "/Dests %ld 0 R\n", pdev->Dests->id);
    if (pdev->PageLabels)
	pprintld1(s, "/PageLabels << /Nums  %ld 0 R >>\n", 
                  pdev->PageLabels->id);
    cos_dict_elements_write(pdev->Catalog, pdev);
    stream_puts(s, ">>\n");
    pdf_end_obj(pdev);
    if (pdev->Dests) {
	COS_FREE(pdev->Dests, "pdf_close(Dests)");
	pdev->Dests = 0;
    }
    if (pdev->PageLabels) {
	COS_FREE(pdev->PageLabels, "pdf_close(PageLabels)");
	pdev->PageLabels = 0;
        pdev->PageLabels_current_label = 0;
    }

    /* Prevent writing special named objects twice. */

    pdev->Catalog->id = 0;
    /*pdev->Info->id = 0;*/	/* Info should get written */
    pdev->Pages->id = 0;
    {
	int i;

	for (i = 0; i < pdev->num_pages; ++i)
	    if (pdev->pages[i].Page)
		pdev->pages[i].Page->id = 0;
    }

    /*
     * Write the definitions of the named objects.
     * Note that this includes Form XObjects created by BP/EP, named PS
     * XObjects, and images named by NI.
     */

    do {
	cos_dict_objects_write(pdev->local_named_objects, pdev);
    } while (pdf_pop_namespace(pdev) >= 0);
    cos_dict_objects_write(pdev->global_named_objects, pdev);

    /* Copy the resources into the main file. */

    s = pdev->strm;
    resource_pos = stell(s);
    sflush(pdev->asides.strm);
    {
	FILE *rfile = pdev->asides.file;
	long res_end = ftell(rfile);

	fseek(rfile, 0L, SEEK_SET);
	pdf_copy_data(s, rfile, res_end);
    }

    /* Write the cross-reference section. */

    xref = pdf_stell(pdev);
    if (pdev->FirstObjectNumber == 1)
	pprintld1(s, "xref\n0 %ld\n0000000000 65535 f \n",
		  pdev->next_id);
    else
	pprintld2(s, "xref\n0 1\n0000000000 65535 f \n%ld %ld\n",
		  pdev->FirstObjectNumber,
		  pdev->next_id - pdev->FirstObjectNumber);
    fseek(tfile, 0L, SEEK_SET);
    {
	long i;

	for (i = pdev->FirstObjectNumber; i < pdev->next_id; ++i) {
	    ulong pos;
	    char str[21];

	    fread(&pos, sizeof(pos), 1, tfile);
	    if (pos & ASIDES_BASE_POSITION)
		pos += resource_pos - ASIDES_BASE_POSITION;
	    sprintf(str, "%010ld 00000 n \n", pos);
	    stream_puts(s, str);
	}
    }

    /* Write the trailer. */

    stream_puts(s, "trailer\n");
    pprintld3(s, "<< /Size %ld /Root %ld 0 R /Info %ld 0 R\n",
	      pdev->next_id, Catalog_id, Info_id);
    stream_puts(s, ">>\n");
    pprintld1(s, "startxref\n%ld\n%%%%EOF\n", xref);

    /* Release the resource records. */

    {
	pdf_resource_t *pres;
	pdf_resource_t *prev;

	for (prev = pdev->last_resource; (pres = prev) != 0;) {
	    prev = pres->prev;
	    gs_free_object(mem, pres, "pdf_resource_t");
	}
	pdev->last_resource = 0;
    }

    /* Free named objects. */

    cos_dict_objects_delete(pdev->local_named_objects);
    COS_FREE(pdev->local_named_objects, "pdf_close(local_named_objects)");
    pdev->local_named_objects = 0;
    cos_dict_objects_delete(pdev->global_named_objects);
    COS_FREE(pdev->global_named_objects, "pdf_close(global_named_objects)");
    pdev->global_named_objects = 0;

    /* Wrap up. */

    gs_free_object(mem, pdev->pages, "pages");
    pdev->pages = 0;
    pdev->num_pages = 0;

    code1 = gdev_vector_close_file((gx_device_vector *) pdev);
    if (code >= 0)
	code = code1;
    return pdf_close_files(pdev, code);
}
