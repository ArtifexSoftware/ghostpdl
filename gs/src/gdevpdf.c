/* Copyright (C) 1996, 1997, 1998 Aladdin Enterprises.  All rights reserved.

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


/* PDF-writing driver */
#include "math_.h"
#include "memory_.h"
#include "string_.h"
#include "time_.h"
#include "gx.h"
#include "gp.h"
#include "gscdefs.h"
#include "gserrors.h"
#include "gxdevice.h"
#include "gxfixed.h"
#include "gxistate.h"
#include "gxpaint.h"
#include "gzpath.h"
#include "gzcpath.h"
#include "gdevpdfx.h"
#include "scanchar.h"
#include "strimpl.h"		/* for short-sighted compilers */
#include "scfx.h"		/* s_CFE_template is default */
#include "sstring.h"
#include "szlibx.h"

/*
 ****** DISABLE GC because we still allocate all temporary data
 ****** on the C heap.
 */
#define DISABLE_GC

/* Optionally substitute other filters for FlateEncode for debugging. */
#if 1
#  define compression_filter_name "FlateDecode"
#  define compression_filter_template s_zlibE_template
#  define compression_filter_state stream_zlib_state
#else
#  include "slzwx.h"
#  define compression_filter_name "LZWDecode"
#  define compression_filter_template s_LZWE_template
#  define compression_filter_state stream_LZW_state
#endif

/* Define the size of the internal stream buffer. */
/* (This is not a limitation, it only affects performance.) */
#define sbuf_size 512

/*
 * Define the offset that indicates that a file position is in the
 * resource file (rfile) rather than the main (contents) file.
 * Must be a power of 2, and larger than the largest possible output file.
 */
#define rfile_base_position min_long

/* GC descriptors */
private_st_device_pdfwrite();
private_st_pdf_resource();
private_st_pdf_font();
private_st_pdf_char_proc();

/* GC procedures */
#define pdev ((gx_device_pdf *)vptr)
private 
ENUM_PTRS_BEGIN(device_pdfwrite_enum_ptrs)
{
#ifdef DISABLE_GC		/* **************** */
    return 0;
#else /* **************** */
    index -= gx_device_pdf_num_ptrs + gx_device_pdf_num_strings;
    if (index < num_resource_types * num_resource_chains)
	ENUM_RETURN(pdev->resources[index / num_resource_chains].chains[index % num_resource_chains]);
    index -= num_resource_types;
    if (index < pdev->outline_depth)
	ENUM_RETURN_STRING_PTR(gx_device_pdf, outline_levels[index].first.action_string);
    index -= pdev->outline_depth;
    if (index < pdev->outline_depth)
	ENUM_RETURN_STRING_PTR(gx_device_pdf, outline_levels[index].last.action_string);
    index -= pdev->outline_depth;
    ENUM_PREFIX(st_device_psdf, 0);
#endif /* **************** */
}
#ifndef DISABLE_GC		/* **************** */
#define e1(i,elt) ENUM_PTR(i, gx_device_pdf, elt);
gx_device_pdf_do_ptrs(e1)
#undef e1
#define e1(i,elt) ENUM_STRING_PTR(i + gx_device_pdf_num_ptrs, gx_device_pdf, elt);
gx_device_pdf_do_strings(e1)
#undef e1
#endif /* **************** */
ENUM_PTRS_END
private RELOC_PTRS_BEGIN(device_pdfwrite_reloc_ptrs)
{
#ifndef DISABLE_GC		/* **************** */
    RELOC_PREFIX(st_device_psdf);
#define r1(i,elt) RELOC_PTR(gx_device_pdf,elt);
    gx_device_pdf_do_ptrs(r1)
#undef r1
#define r1(i,elt) RELOC_STRING_PTR(gx_device_pdf,elt);
	gx_device_pdf_do_strings(r1)
#undef r1
    {
	int i, j;

	for (i = 0; i < num_resource_types; ++i)
	    for (j = 0; j < num_resource_chains; ++j)
		RELOC_PTR(gx_device_pdf, resources[i].chains[j]);
	for (i = 0; i < pdev->outline_depth; ++i) {
	    RELOC_STRING_PTR(gx_device_pdf, outline_levels[i].first.action_string);
	    RELOC_STRING_PTR(gx_device_pdf, outline_levels[i].last.action_string);
	}
    }
#endif /* **************** */
}
RELOC_PTRS_END
#undef pdev
/* Even though device_pdfwrite_finalize is the same as gx_device_finalize, */
/* we need to implement it separately because st_composite_final */
/* declares all 3 procedures as private. */
private void
device_pdfwrite_finalize(void *vpdev)
{
    gx_device_finalize(vpdev);
}

/* Device procedures */
private dev_proc_open_device(pdf_open);
private dev_proc_output_page(pdf_output_page);
private dev_proc_close_device(pdf_close);
extern dev_proc_fill_rectangle(gdev_pdf_fill_rectangle);	/* in gdevpdfd.c */
extern dev_proc_copy_mono(gdev_pdf_copy_mono);	/* in gdevpdfi.c */
extern dev_proc_copy_color(gdev_pdf_copy_color);	/* in gdevpdfi.c */
extern dev_proc_get_params(gdev_pdf_get_params);	/* in gdevpdfp.c */
extern dev_proc_put_params(gdev_pdf_put_params);	/* in gdevpdfp.c */
extern dev_proc_fill_path(gdev_pdf_fill_path);	/* in gdevpdfd.c */
extern dev_proc_stroke_path(gdev_pdf_stroke_path);	/* in gdevpdfd.c */
extern dev_proc_fill_mask(gdev_pdf_fill_mask);	/* in gdevpdfi.c */
extern dev_proc_begin_image(gdev_pdf_begin_image);	/* in gdevpdfi.c */

#ifndef X_DPI
#  define X_DPI 720
#endif
#ifndef Y_DPI
#  define Y_DPI 720
#endif

const gx_device_pdf gs_pdfwrite_device =
{std_device_color_stype_body(gx_device_pdf, 0, "pdfwrite",
			     &st_device_pdfwrite,
			     DEFAULT_WIDTH_10THS * X_DPI / 10,
			     DEFAULT_HEIGHT_10THS * Y_DPI / 10,
			     X_DPI, Y_DPI, 24, 255, 255),
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
  NULL,				/* get_bits */
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
  gdev_pdf_begin_image,
  NULL,				/* image_data */
  NULL				/* end_image */
 },
 psdf_initial_values(psdf_version_level2, 0 /*false */ ),	/* (!ASCII85EncodePages) */
 1.2,				/* CompatibilityLevel */
 1 /*true */ ,			/* ReAssignCharacters */
 1 /*true */ ,			/* ReEncodeCharacters */
 1,				/* FirstObjectNumber */
 pdf_compress_none,		/* compression */
 {0},				/* tfname */
 0,				/* tfile */
 {0},				/* rfname */
 0,				/* rfile */
 0,				/* rstrm */
 0,				/* rstrmbuf */
 0,				/* rsave_strm */
 0,				/* open_font */
 0,				/* embedded_encoding_id */
 0,				/* next_id */
 0,				/* root_id */
 0,				/* info_id */
 0,				/* pages_id */
 0,				/* outlines_id */
 0,				/* next_page */
 0,				/* contents_id */
 pdf_in_none,			/* context */
 0,				/* contents_length_id */
 0,				/* contents_pos */
 NoMarks,			/* procsets */
 -1,				/* flatness */
 {gx_line_params_initial},	/* line_params */
 {pdf_text_state_default},	/* text */
 {0},				/* space_char_ids */
 0,				/* page_ids */
 0,				/* num_page_ids */
 0,				/* pages_referenced */
 {
     {
	 {0}}},			/* resources */
 0,				/* annots */
 0,				/* last_resource */
 {0, 0},			/* catalog_string */
 {0, 0},			/* pages_string */
 {0, 0},			/* page_string */
 {
     {
	 {0}}},			/* outline_levels */
 0,				/* outline_depth */
 0,				/* closed_outline_depth */
 0,				/* outlines_open */
 0,				/* articles */
 0,				/* named_dests */
 0,				/* named_objects */
 0				/* open_graphics */
};

/* ---------------- Utilities ---------------- */

/* ------ Document ------ */

/* Initialize the IDs allocated at startup. */
void
pdf_initialize_ids(gx_device_pdf * pdev)
{
    pdev->next_id = pdev->FirstObjectNumber;
    pdev->root_id = pdf_obj_ref(pdev);
    pdev->pages_id = pdf_obj_ref(pdev);
}

/* Open the document if necessary. */
void
pdf_open_document(gx_device_pdf * pdev)
{
    if (!is_in_document(pdev) && pdf_stell(pdev) == 0) {
	stream *s = pdev->strm;

	pprintd1(s, "%%PDF-1.%d\n",
		 (pdev->CompatibilityLevel >= 1.2 ? 2 : 1));
	pdev->binary_ok = !pdev->params.ASCII85EncodePages;
	if (pdev->binary_ok)
	    pputs(s, "%\307\354\217\242\n");
    }
    /*
     * Determine the compression method.  Currently this does nothing.
     * It also isn't clear whether the compression method can now be
     * changed in the course of the document.
     *
     * The following algorithm is per an update to TN # 5151 by
     * Adobe Developer Support.
     */
    if (!pdev->params.CompressPages)
	pdev->compression = pdf_compress_none;
    else if (pdev->CompatibilityLevel < 1.2)
	pdev->compression = pdf_compress_LZW;
    else if (pdev->params.UseFlateCompression)
	pdev->compression = pdf_compress_Flate;
    else
	pdev->compression = pdf_compress_LZW;
}

/* ------ Objects ------ */

/* Allocate an object ID. */
private long
pdf_next_id(gx_device_pdf * pdev)
{
    return (pdev->next_id)++;
}

/* Return the current position in the output. */
/* Note that this may be in either the main file or the resource file. */
long
pdf_stell(gx_device_pdf * pdev)
{
    stream *s = pdev->strm;
    long pos = stell(s);

    if (s == pdev->rstrm)
	pos += rfile_base_position;
    return pos;
}

/* Allocate an ID for a future object. */
long
pdf_obj_ref(gx_device_pdf * pdev)
{
    long id = pdf_next_id(pdev);
    long pos = pdf_stell(pdev);

    fwrite(&pos, sizeof(pos), 1, pdev->tfile);
    return id;
}

/* Begin an object, optionally allocating an ID. */
long
pdf_open_obj(gx_device_pdf * pdev, long id)
{
    stream *s = pdev->strm;

    if (id <= 0) {
	id = pdf_obj_ref(pdev);
    } else {
	long pos = pdf_stell(pdev);
	FILE *tfile = pdev->tfile;
	long tpos = ftell(tfile);

	fseek(tfile, (id - pdev->FirstObjectNumber) * sizeof(pos),
	      SEEK_SET);
	fwrite(&pos, sizeof(pos), 1, tfile);
	fseek(tfile, tpos, SEEK_SET);
    }
    pprintld1(s, "%ld 0 obj\n", id);
    return id;
}

/* End an object. */
int
pdf_end_obj(gx_device_pdf * pdev)
{
    pputs(pdev->strm, "endobj\n");
    return 0;
}

/* ------ Graphics ------ */

/* Reset the graphics state parameters to initial values. */
void
pdf_reset_graphics(gx_device_pdf * pdev)
{
    color_set_pure(&pdev->fill_color, 0);	/* black */
    color_set_pure(&pdev->stroke_color, 0);	/* ditto */
    pdev->flatness = -1;
    {
	static const gx_line_params lp_initial =
	{gx_line_params_initial};

	pdev->line_params = lp_initial;
    }
}

/* Set the fill or stroke color. */
int
pdf_set_color(gx_device_pdf * pdev, gx_color_index color,
	      gx_drawing_color * pdcolor, const char *rgs)
{
    if (gx_dc_pure_color(pdcolor) != color) {
	int code;

	/*
	 * In principle, we can set colors in either stream or text
	 * context.  However, since we currently enclose all text
	 * strings inside a gsave/grestore, this causes us to lose
	 * track of the color when we leave text context.  Therefore,
	 * we require stream context for setting colors.
	 */
#if 0
	switch (pdev->context) {
	    case pdf_in_stream:
	    case pdf_in_text:
		break;
	    case pdf_in_none:
		code = pdf_open_page(pdev, pdf_in_stream);
		goto open;
	    case pdf_in_string:
		code = pdf_open_page(pdev, pdf_in_text);
	      open:if (code < 0)
		    return code;
	}
#else
	code = pdf_open_page(pdev, pdf_in_stream);
	if (code < 0)
	    return code;
#endif
	color_set_pure(pdcolor, color);
	psdf_set_color((gx_device_vector *) pdev, pdcolor, rgs);
    }
    return 0;
}

/* Write matrix values. */
void
pdf_put_matrix(gx_device_pdf * pdev, const char *before,
	       const gs_matrix * pmat, const char *after)
{
    stream *s = pdev->strm;

    if (before)
	pputs(s, before);
    pprintg6(s, "%g %g %g %g %g %g ",
	     pmat->xx, pmat->xy, pmat->yx, pmat->yy, pmat->tx, pmat->ty);
    if (after)
	pputs(s, after);
}

/*
 * Write a name, with escapes for unusual characters.  In PDF 1.1, we have
 * no choice but to replace these characters with '?'; in PDF 1.2, we can
 * use an escape sequence for anything except a null <00>.
 */
void
pdf_put_name(const gx_device_pdf * pdev, const byte * nstr, uint size)
{
    stream *s = pdev->strm;
    uint i;
    bool escape = pdev->CompatibilityLevel >= 1.2;
    char hex[4];

    pputc(s, '/');
    for (i = 0; i < size; ++i) {
	uint c = nstr[i];

	switch (c) {
	    case '%':
	    case '(':
	    case ')':
	    case '<':
	    case '>':
	    case '[':
	    case ']':
	    case '{':
	    case '}':
		/* These characters are invalid in both 1.1 and 1.2, */
		/* but can be escaped in 1.2. */
		if (escape) {
		    sprintf(hex, "#%02x", c);
		    pputs(s, hex);
		    break;
		}
		/* falls through */
	    case 0:
		/* This is invalid in 1.1 and 1.2, and cannot be escaped. */
		pputc(s, '?');
		break;
	    case '/':
	    case '#':
		/* These are valid in 1.1, but must be escaped in 1.2. */
		if (escape) {
		    sprintf(hex, "#%02x", c);
		    pputs(s, hex);
		    break;
		}
		/* falls through */
	    default:
		pputc(s, c);
	}
    }
}

/*
 * Write a string in its shortest form ( () or <> ).  Note that
 * this form is different depending on whether binary data are allowed.
 * We wish PDF supported ASCII85 strings ( <~ ~> ), but it doesn't.
 */
void
pdf_put_string(const gx_device_pdf * pdev, const byte * str, uint size)
{
    psdf_write_string(pdev->strm, str, size,
		      (pdev->binary_ok ? print_binary_ok : 0));
}

/* Write a value, treating names specially. */
void
pdf_put_value(const gx_device_pdf * pdev, const byte * vstr, uint size)
{
    if (vstr[0] == '/')
	pdf_put_name(pdev, vstr + 1, size - 1);
    else
	pwrite(pdev->strm, vstr, size);
}

/* ------ Page contents ------ */

/* Handle transitions between contexts. */
private int
    none_to_stream(P1(gx_device_pdf *)), stream_to_text(P1(gx_device_pdf *)),
    string_to_text(P1(gx_device_pdf *)), text_to_stream(P1(gx_device_pdf *)),
    stream_to_none(P1(gx_device_pdf *));
typedef int (*context_proc) (P1(gx_device_pdf *));
private const context_proc context_procs[4][4] =
{
    {0, none_to_stream, none_to_stream, none_to_stream},
    {stream_to_none, 0, stream_to_text, stream_to_text},
    {text_to_stream, text_to_stream, 0, 0},
    {string_to_text, string_to_text, string_to_text, 0}
};

/* Enter stream context. */
private int
none_to_stream(gx_device_pdf * pdev)
{
    stream *s;

    if (pdev->contents_id != 0)
	return_error(gs_error_Fatal);	/* only 1 contents per page */
    pdev->contents_id = pdf_begin_obj(pdev);
    pdev->contents_length_id = pdf_obj_ref(pdev);
    s = pdev->strm;
    pprintld1(s, "<</Length %ld 0 R", pdev->contents_length_id);
    if (pdev->compression == pdf_compress_Flate)
	pprints1(s, "/Filter /%s", compression_filter_name);
    pputs(s, ">>\nstream\n");
    pdev->contents_pos = pdf_stell(pdev);
    if (pdev->compression == pdf_compress_Flate) {	/* Set up the Flate filter. */
	const stream_template *template = &compression_filter_template;
	stream *es = s_alloc(pdev->pdf_memory, "PDF compression stream");
	byte *buf = gs_alloc_bytes(pdev->pdf_memory, sbuf_size,
				   "PDF compression buffer");
	compression_filter_state *st =
	gs_alloc_struct(pdev->pdf_memory, compression_filter_state,
			template->stype, "PDF compression state");

	if (es == 0 || st == 0 || buf == 0)
	    return_error(gs_error_VMerror);
	s_std_init(es, buf, sbuf_size, &s_filter_write_procs,
		   s_mode_write);
	st->memory = pdev->pdf_memory;
	st->template = template;
	es->state = (stream_state *) st;
	es->procs.process = template->process;
	es->strm = s;
	(*template->set_defaults) ((stream_state *) st);
	(*template->init) ((stream_state *) st);
	pdev->strm = s = es;
    }
    /* Scale the coordinate system. */
    pprintg2(s, "%g 0 0 %g 0 0 cm\n",
	     72.0 / pdev->HWResolution[0], 72.0 / pdev->HWResolution[1]);
    /* Do a level of gsave for the clipping path. */
    pputs(s, "q\n");
    return pdf_in_stream;
}
/* Enter text context from stream context. */
private int
stream_to_text(gx_device_pdf * pdev)
{				/*
				 * Bizarrely enough, Acrobat Reader cares how the final font size is
				 * obtained -- the CTM (cm), text matrix (Tm), and font size (Tf)
				 * are *not* all equivalent.  In particular, it seems to use the
				 * product of the text matrix and font size to decide how to
				 * anti-alias characters.  Therefore, we have to temporarily patch
				 * the CTM so that the scale factors are unity.  What a nuisance!
				 */
    pprintg2(pdev->strm, "q %g 0 0 %g 0 0 cm BT\n",
	     pdev->HWResolution[0] / 72.0, pdev->HWResolution[1] / 72.0);
    pdev->procsets |= Text;
    gs_make_identity(&pdev->text.matrix);
    pdev->text.line_start.x = pdev->text.line_start.y = 0;
    pdev->text.buffer_count = 0;
    return pdf_in_text;
}
/* Exit string context to text context. */
private int
string_to_text(gx_device_pdf * pdev)
{
    pdf_put_string(pdev, pdev->text.buffer, pdev->text.buffer_count);
    pputs(pdev->strm, "Tj\n");
    pdev->text.buffer_count = 0;
    return pdf_in_text;
}
/* Exit text context to stream context. */
private int
text_to_stream(gx_device_pdf * pdev)
{
    pputs(pdev->strm, "ET Q\n");
    pdev->text.font = 0;	/* because of Q */
    return pdf_in_stream;
}
/* Exit stream context. */
private int
stream_to_none(gx_device_pdf * pdev)
{
    stream *s = pdev->strm;
    long length;

    if (pdev->compression == pdf_compress_Flate) {	/* Terminate the Flate filter. */
	stream *fs = s->strm;

	sclose(s);
	gs_free_object(pdev->pdf_memory, s->cbuf, "zlib buffer");
	gs_free_object(pdev->pdf_memory, s, "zlib stream");
	pdev->strm = s = fs;
    }
    length = pdf_stell(pdev) - pdev->contents_pos;
    pputs(s, "endstream\n");
    pdf_end_obj(pdev);
    pdf_open_obj(pdev, pdev->contents_length_id);
    pprintld1(s, "%ld\n", length);
    pdf_end_obj(pdev);
    return pdf_in_none;
}

/* Begin a page contents part. */
int
pdf_open_contents(gx_device_pdf * pdev, pdf_context context)
{
    int (*proc) (P1(gx_device_pdf *));

    while ((proc = context_procs[pdev->context][context]) != 0) {
	int code = (*proc) (pdev);

	if (code < 0)
	    return code;
	pdev->context = (pdf_context) code;
    }
    pdev->context = context;
    return 0;
}

/* Close the current contents part if we are in one. */
int
pdf_close_contents(gx_device_pdf * pdev, bool last)
{
    if (pdev->context == pdf_in_none)
	return 0;
    if (last) {			/* Exit from the clipping path gsave. */
	pdf_open_contents(pdev, pdf_in_stream);
	pputs(pdev->strm, "Q\n");
	pdev->text.font = 0;
    }
    return pdf_open_contents(pdev, pdf_in_none);
}

/* ------ Resources et al ------ */

/* Define the names of the resource types. */
private const char *const resource_names[] =
{pdf_resource_type_names};

/* Define the allocator descriptors for the resource types. */
extern const gs_memory_struct_type_t st_pdf_named_object;	/* in gdevpdfo.c */
private const gs_memory_struct_type_t *const resource_structs[] =
{pdf_resource_type_structs};

/* Find a resource of a given type by gs_id. */
pdf_resource *
pdf_find_resource_by_gs_id(gx_device_pdf * pdev, pdf_resource_type type,
			   gs_id rid)
{
    pdf_resource **pchain =
    &pdev->resources[type].chains[gs_id_hash(rid) % num_resource_chains];
    pdf_resource **pprev = pchain;
    pdf_resource *pres;

    for (; (pres = *pprev) != 0; pprev = &pres->next)
	if (pres->rid == rid) {
	    if (pprev != pchain) {
		*pprev = pres->next;
		pres->next = *pchain;
		*pchain = pres;
	    }
	    return pres;
	}
    return 0;
}

/* Begin an object logically separate from the contents. */
long
pdf_open_separate(gx_device_pdf * pdev, long id)
{
    pdf_open_document(pdev);
    pdev->rsave_strm = pdev->strm;
    pdev->strm = pdev->rstrm;
    return pdf_open_obj(pdev, id);
}

/* Begin an aside (resource, annotation, ...). */
private int
pdf_alloc_aside(gx_device_pdf * pdev, pdf_resource ** plist,
		const gs_memory_struct_type_t * pst, pdf_resource ** ppres)
{
    pdf_resource *pres;

    if (pst == NULL)
	pst = &st_pdf_resource;
    pres =
	gs_alloc_struct(pdev->pdf_memory, pdf_resource, pst, "begin_aside");
    if (pres == 0)
	return_error(gs_error_VMerror);
    pres->next = *plist;
    *plist = pres;
    pres->prev = pdev->last_resource;
    pdev->last_resource = pres;
    *ppres = pres;
    return 0;
}
int
pdf_begin_aside(gx_device_pdf * pdev, pdf_resource ** plist,
		const gs_memory_struct_type_t * pst, pdf_resource ** ppres)
{
    long id = pdf_begin_separate(pdev);
    int code;

    if (id < 0)
	return id;
    code = pdf_alloc_aside(pdev, plist, pst, ppres);
    if (code < 0)
	return code;
    (*ppres)->id = id;
    return 0;
}

/* Begin a resource of a given type. */
int
pdf_begin_resource(gx_device_pdf * pdev, pdf_resource_type type, gs_id rid,
		   pdf_resource ** ppres)
{
    int code = pdf_begin_aside(pdev,
       &pdev->resources[type].chains[gs_id_hash(rid) % num_resource_chains],
			       resource_structs[type], ppres);

    if (code < 0)
	return code;
    if (resource_names[type] != 0) {
	stream *s = pdev->strm;

	pprints1(s, "<< /Type /%s", resource_names[type]);
	pprintld1(s, " /Name /R%ld", (*ppres)->id);
    }
    return code;
}

/* Allocate a resource, but don't open the stream. */
int
pdf_alloc_resource(gx_device_pdf * pdev, pdf_resource_type type, gs_id rid,
		   pdf_resource ** ppres)
{
    int code = pdf_alloc_aside(pdev,
       &pdev->resources[type].chains[gs_id_hash(rid) % num_resource_chains],
			       resource_structs[type], ppres);

    if (code < 0)
	return code;
    (*ppres)->id = pdf_obj_ref(pdev);
    return 0;
}

/* End an aside or other separate object. */
int
pdf_end_aside(gx_device_pdf * pdev)
{
    int code = pdf_end_obj(pdev);

    pdev->strm = pdev->rsave_strm;
    return code;
}
/* End a resource. */
int
pdf_end_resource(gx_device_pdf * pdev)
{
    return pdf_end_aside(pdev);
}

/* ------ Pages ------ */

/* Reset the state of the current page. */
void
pdf_reset_page(gx_device_pdf * pdev, bool first_page)
{
    pdev->contents_id = 0;
    pdf_reset_graphics(pdev);
    pdev->procsets = NoMarks;
    {
	int i, j;

	for (i = 0; i < num_resource_types; ++i)
	    if (first_page ||
		!(i == resourceFont || resource_names[i] == 0)
		)
		for (j = 0; j < num_resource_chains; ++j)
		    pdev->resources[i].chains[j] = 0;
    }
    pdev->page_string.data = 0;
    {
	static const pdf_text_state text_default =
	{pdf_text_state_default};

	pdev->text = text_default;
    }
}

/* Get or assign the ID for a page. */
/* Returns 0 if the page number is out of range. */
long
pdf_page_id(gx_device_pdf * pdev, int page_num)
{
    long page_id;

    if (page_num >= pdev->num_page_ids) {	/* Grow the page_ids array. */
	uint new_num_ids =
	max(page_num + 10, pdev->num_page_ids << 1);

	/* resize_object for a byte array takes a new object size */
	/* in bytes.  This is a quirk of the API that we probably */
	/* won't ever be able to fix.... */
	long *new_ids = gs_resize_object(pdev->pdf_memory, pdev->page_ids,
					 new_num_ids * sizeof(long),
					 "pdf_page_id(resize page_ids)");

	if (new_ids == 0)
	    return 0;
	pdev->page_ids = new_ids;
	pdev->num_page_ids = new_num_ids;
    }
    if (page_num < 1)
	return 0;
    while (page_num > pdev->pages_referenced)
	pdev->page_ids[pdev->pages_referenced++] = 0;
    if ((page_id = pdev->page_ids[page_num - 1]) == 0)
	pdev->page_ids[page_num - 1] = page_id = pdf_obj_ref(pdev);
    return page_id;
}

/* Write saved page- or document-level information. */
int
pdf_write_saved_string(gx_device_pdf * pdev, gs_string * pstr)
{
    if (pstr->data != 0) {
	pwrite(pdev->strm, pstr->data, pstr->size);
	gs_free_string(pdev->pdf_memory, pstr->data, pstr->size,
		       "pdf_write_saved_string");
	pstr->data = 0;
    }
    return 0;
}

/* Open a page for writing. */
int
pdf_open_page(gx_device_pdf * pdev, pdf_context context)
{
    if (!is_in_page(pdev)) {
	if (pdf_page_id(pdev, pdev->next_page + 1) == 0)
	    return_error(gs_error_VMerror);
	pdf_open_document(pdev);
    }
    /* Note that context may be pdf_in_none here. */
    return pdf_open_contents(pdev, context);
}

/* Close the current page. */
private int
pdf_close_page(gx_device_pdf * pdev)
{
    stream *s;
    int page_num = ++(pdev->next_page);
    long page_id;

    /* If the very first page is blank, we need to open the document */
    /* before doing anything else. */
    pdf_open_document(pdev);
    pdf_close_contents(pdev, true);
    page_id = pdf_page_id(pdev, page_num);
    pdf_open_obj(pdev, page_id);
    s = pdev->strm;
    pprintd2(s, "<<\n/Type /Page\n/MediaBox [0 0 %d %d]\n",
	     (int)(pdev->MediaSize[0]), (int)(pdev->MediaSize[1]));
    pprintld1(s, "/Parent %ld 0 R\n", pdev->pages_id);
    pputs(s, "/Resources << /ProcSet [/PDF");
    if (pdev->procsets & ImageB)
	pputs(s, " /ImageB");
    if (pdev->procsets & ImageC)
	pputs(s, " /ImageC");
    if (pdev->procsets & ImageI)
	pputs(s, " /ImageI");
    if (pdev->procsets & Text)
	pputs(s, " /Text");
    pputs(s, "]\n");
    {
	int i;

	for (i = 0; i < num_resource_types; ++i)
	    if (!(i == resourceFont || resource_names[i] == 0)) {
		bool first = true;
		int j;
		const pdf_resource *pres;

		for (j = 0; j < num_resource_chains; ++j) {
		    for (pres = pdev->resources[i].chains[j];
			 pres != 0; pres = pres->next
			) {
			if (first)
			    pprints1(s, "/%s<<", resource_names[i]), first = false;
			pprintld2(s, "/R%ld\n%ld 0 R", pres->id, pres->id);
		    }
		    pdev->resources[i].chains[j] = 0;
		}
		if (!first)
		    pputs(s, ">>\n");
	    }
    }
    /* Put out references to just those fonts used on this page. */
    {
	bool first = true;
	int j;
	pdf_font *font;

	for (j = 0; j < num_resource_chains; ++j)
	    for (font = (pdf_font *) pdev->resources[resourceFont].chains[j];
		 font != 0; font = font->next
		)
		if (font->used_on_page) {
		    if (first)
			pputs(s, "/Font <<\n"), first = false;
		    if (font->frname[0])
			pprints1(s, "/%s", font->frname);
		    else
			pprintld1(s, "/R%ld", font->id);
		    pprintld1(s, " %ld 0 R\n", font->id);
		    font->used_on_page = false;
		}
	if (!first)
	    pputs(s, ">>\n");
    }
    pputs(s, ">>\n");
    if (pdev->contents_id == 0)
	pputs(s, "/Contents []\n");
    else
	pprintld1(s, "/Contents %ld 0 R\n", pdev->contents_id);
    pdf_write_saved_string(pdev, &pdev->page_string);
    {
	const pdf_resource *pres = pdev->annots;
	bool any = false;

	for (; pres != 0; pres = pres->next)
	    if (pres->rid == page_num - 1) {
		if (!any) {
		    pputs(s, "/Annots [\n");
		    any = true;
		}
		pprintld1(s, "%ld 0 R\n", pres->id);
	    }
	if (any)
	    pputs(s, "]\n");
    }
    pputs(s, ">>\n");
    pdf_end_obj(pdev);
    pdf_reset_page(pdev, false);
    return 0;
}

/* Write the default entries of the Info dictionary. */
int
pdf_write_default_info(gx_device_pdf * pdev)
{
    stream *s = pdev->strm;

    /* Reading the time without using time_t is a challenge.... */
    long t[2];			/* time_t can't be longer than 2 longs. */
    struct tm ltime;
    char buf[20];

    time((void *)t);
    ltime = *localtime((void *)t);
    sprintf(buf, "%04d%02d%02d%02d%02d%02d",
	    ltime.tm_year + 1900, ltime.tm_mon + 1, ltime.tm_mday,
	    ltime.tm_hour, ltime.tm_min, ltime.tm_sec);
    pprints1(s, "/CreationDate (D:%s)\n", buf);
    sprintf(buf, "%1.2f", gs_revision / 100.0);
    pprints2(s, "/Producer (%s %s)\n", gs_product, buf);
    return 0;
}

/* ---------------- Device open/close ---------------- */

/* Close and remove temporary files. */
private void
pdf_close_files(gx_device_pdf * pdev)
{
    gs_free_object(pdev->pdf_memory, pdev->rstrmbuf,
		   "pdf_close_files(rstrmbuf)");
    pdev->rstrmbuf = 0;
    gs_free_object(pdev->pdf_memory, pdev->rstrm,
		   "pdf_close_files(rstrm)");
    pdev->rstrm = 0;
    if (pdev->rfile != 0) {
	fclose(pdev->rfile);
	pdev->rfile = 0;
	unlink(pdev->rfname);
    }
    if (pdev->tfile != 0) {
	fclose(pdev->tfile);
	pdev->tfile = 0;
	unlink(pdev->tfname);
    }
}

/* Open the device. */
private int
pdf_open(gx_device * dev)
{
    gx_device_pdf *pdev = (gx_device_pdf *) dev;
    char fmode[4];
    int code;

    pdev->pdf_memory = &gs_memory_default;	/* as good as any */
    strcpy(fmode, "w+");
    strcat(fmode, gp_fmode_binary_suffix);
    pdev->tfile =
	gp_open_scratch_file(gp_scratch_file_name_prefix,
			     pdev->tfname, fmode);
    if (pdev->tfile == 0)
	return_error(gs_error_invalidfileaccess);
    pdev->rfile =
	gp_open_scratch_file(gp_scratch_file_name_prefix,
			     pdev->rfname, fmode);
    if (pdev->rfile == 0) {
	code = gs_note_error(gs_error_invalidfileaccess);
	goto fail;
    }
    pdev->rstrm = s_alloc(pdev->pdf_memory, "pdf_open(rstrm)");
    pdev->rstrmbuf = gs_alloc_bytes(pdev->pdf_memory, sbuf_size,
				    "pdf_open(rstrmbuf)");
    if (pdev->rstrm == 0 || pdev->rstrmbuf == 0) {
	code = gs_note_error(gs_error_VMerror);
	goto fail;
    }
    swrite_file(pdev->rstrm, pdev->rfile, pdev->rstrmbuf, sbuf_size);
    code = gdev_vector_open_file((gx_device_vector *) pdev, sbuf_size);
    if (code < 0)
	goto fail;
    gdev_vector_init((gx_device_vector *) pdev);
    /* Set in_page so the vector routines won't try to call */
    /* any vector implementation procedures. */
    pdev->in_page = true;
    pdf_initialize_ids(pdev);
    pdev->outlines_id = 0;
    pdev->next_page = 0;
    memset(pdev->space_char_ids, 0, sizeof(pdev->space_char_ids));
    pdev->page_ids = (void *)
	gs_alloc_byte_array(pdev->pdf_memory, initial_num_page_ids,
			    sizeof(*pdev->page_ids), "pdf_open(page_ids)");
    if (pdev->page_ids == 0) {
	code = gs_error_VMerror;
	goto fail;
    }
    pdev->num_page_ids = initial_num_page_ids;
    pdev->pages_referenced = 0;
    pdev->catalog_string.data = 0;
    pdev->pages_string.data = 0;
    pdev->outline_levels[0].first.id = 0;
    pdev->outline_levels[0].left = max_int;
    pdev->outline_depth = 0;
    pdev->closed_outline_depth = 0;
    pdev->outlines_open = 0;
    pdev->articles = 0;
    pdev->named_dests = 0;
    pdev->named_objects = 0;
    pdev->open_graphics = 0;
    pdf_reset_page(pdev, true);

    return 0;
  fail:
    pdf_close_files(pdev);
    return code;
}

/* Wrap up ("output") a page. */
private int
pdf_output_page(gx_device * dev, int num_copies, int flush)
{
    gx_device_pdf *pdev = (gx_device_pdf *) dev;
    int code = pdf_close_page(pdev);

    return (code < 0 ? code : gx_finish_output_page(dev, num_copies, flush));
}

/* Write out the CharProcs for an embedded font. */
/* We thought that Acrobat 2.x required this to be an indirect object, */
/* but we were wrong. */
private int
pdf_write_char_procs(gx_device_pdf * pdev, const pdf_font * pef,
		     gs_int_rect * pbbox, int widths[256])
{
    stream *s = pdev->strm;
    const pdf_char_proc *pcp;
    int w;

    pputs(s, "<<");
    /* Write real characters. */
    for (pcp = pef->char_procs; pcp; pcp = pcp->char_next) {
	pbbox->p.y = min(pbbox->p.y, pcp->y_offset);
	pbbox->q.x = max(pbbox->q.x, pcp->width);
	pbbox->q.y = max(pbbox->q.y, pcp->height + pcp->y_offset);
	widths[pcp->char_code] = pcp->x_width;
	pprintld2(s, "/a%ld\n%ld 0 R", (long)pcp->char_code, pcp->id);
    }
    /* Write space characters. */
    for (w = 0; w < countof(pef->spaces); ++w) {
	byte ch = pef->spaces[w];

	if (ch) {
	    pprintld2(s, "/a%ld\n%ld 0 R", (long)ch,
		      pdev->space_char_ids[w]);
	    widths[ch] = w + x_space_min;
	}
    }
    pputs(s, ">>");
    return 0;
}

/* Write out the Widths for an embedded font similarly. */
private int
pdf_write_widths(gx_device_pdf * pdev, const pdf_font * pef, int widths[256])
{
    stream *s = pdev->strm;
    int i;

    pputs(s, "[");
    for (i = 0; i < pef->num_chars; ++i)
	pprintd1(s, (i & 15 ? " %d" : ("\n%d")), widths[i]);
    pputs(s, "]");
    return 0;
}

/* Close the device. */
private int
pdf_close(gx_device * dev)
{
    gx_device_pdf *pdev = (gx_device_pdf *) dev;
    stream *s;
    FILE *tfile = pdev->tfile;
    long xref;
    long named_dests_id = 0;
    long resource_pos;

    /*
     * If this is an EPS file, or if the file has produced no marks
     * at all, we need to tidy up a little so as not to produce
     * illegal PDF.  We recognize EPS files as having some contents
     * but no showpage.
     */
    if (pdev->next_page == 0) {
	pdf_open_document(pdev);
	if (pdev->contents_id != 0) {
	    pdf_close_page(pdev);
	}
    }
    /*
     * Write out fonts.  For base fonts, write the encoding
     * differences.
     */

    {
	int j;
	const pdf_font *pef;

	s = pdev->strm;
	for (j = 0; j < num_resource_chains; ++j)
	    for (pef = (const pdf_font *)pdev->resources[resourceFont].chains[j];
		 pef != 0; pef = pef->next
		) {
		pdf_open_obj(pdev, pef->id);
		if (font_is_embedded(pef)) {
		    gs_int_rect bbox;
		    int widths[256];

		    memset(&bbox, 0, sizeof(bbox));
		    memset(widths, 0, sizeof(widths));
		    pprints1(s, "<</Type/Font/Name/%s/Subtype/Type3", pef->frname);
		    pprintld1(s, "/Encoding %ld 0 R", pdev->embedded_encoding_id);
		    pprintd1(s, "/FirstChar 0/LastChar %d/CharProcs",
			     pef->num_chars - 1);
		    pdf_write_char_procs(pdev, pef, &bbox, widths);
		    pprintd4(s, "/FontBBox[%d %d %d %d]/FontMatrix[1 0 0 1 0 0]/Widths",
			     bbox.p.x, bbox.p.y, bbox.q.x, bbox.q.y);
		    pdf_write_widths(pdev, pef, widths);
		    pputs(s, ">>\n");
		} else {
		    pprintld1(s, "<</Type/Font/Name/R%ld/Subtype/Type1/BaseFont",
			      pef->id);
		    pdf_put_name(pdev, pef->fname.chars, pef->fname.size);
		    if (pef->differences != 0)
			pprintld1(s, "/Encoding %ld 0 R", pef->diff_id);
		    pputs(s, ">>\n");
		    if (pef->differences != 0) {
			int prev = 256;
			int i;

			pdf_end_obj(pdev);
			pdf_open_obj(pdev, pef->diff_id);
			pputs(s, "<</Type/Encoding/Differences[");
			for (i = 0; i < 256; ++i)
			    if (pef->differences[i].data != 0) {
				if (i != prev + 1)
				    pprintd1(s, "\n%d", i);
				pdf_put_name(pdev, pef->differences[i].data,
					     pef->differences[i].size);
				prev = i;
			    }
			pputs(s, "]>>\n");
		    }
		}
		pdf_end_obj(pdev);
	    }
    }

    /* Create the root (Catalog). */

    pdf_open_obj(pdev, pdev->pages_id);
    pputs(s, "<< /Type /Pages /Kids [\n");
    {
	int i;

	for (i = 0; i < pdev->next_page; ++i)
	    pprintld1(s, "%ld 0 R\n", pdev->page_ids[i]);
    }
    pprintd1(s, "] /Count %d\n", pdev->next_page);
    pdf_write_saved_string(pdev, &pdev->pages_string);
    pputs(s, ">>\n");
    pdf_end_obj(pdev);
    if (pdev->outlines_id != 0) {
	pdfmark_close_outline(pdev);	/* depth must be zero! */
	pdf_open_obj(pdev, pdev->outlines_id);
	pprintd1(s, "<< /Count %d", pdev->outlines_open);
	pprintld2(s, " /First %ld 0 R /Last %ld 0 R >>\n",
		  pdev->outline_levels[0].first.id,
		  pdev->outline_levels[0].last.id);
	pdf_end_obj(pdev);
    }
    if (pdev->articles != 0) {
	pdf_article *part;

	/* Write the remaining information for each article. */
	for (part = pdev->articles; part != 0; part = part->next)
	    pdfmark_write_article(pdev, part);
    }
    if (pdev->named_dests != 0) {
	pdf_named_dest *pnd;

	named_dests_id = pdf_begin_obj(pdev);
	pputs(s, "<<\n");
	while ((pnd = pdev->named_dests) != 0) {
	    pdev->named_dests = pnd->next;
	    pdf_put_value(pdev, pnd->key.data, pnd->key.size);
	    pprints1(s, " %s\n", pnd->dest);
	    gs_free_string(pdev->pdf_memory, pnd->key.data, pnd->key.size,
			   "pdf_close(named_dest key)");
	    gs_free_object(pdev->pdf_memory, pnd, "pdf_close(named_dest)");
	}
	pputs(s, ">>\n");
	pdf_end_obj(pdev);
    }
    pdf_open_obj(pdev, pdev->root_id);
    pprintld1(s, "<< /Type /Catalog /Pages %ld 0 R\n", pdev->pages_id);
    if (pdev->outlines_id != 0)
	pprintld1(s, "/Outlines %ld 0 R\n", pdev->outlines_id);
    if (pdev->articles != 0) {
	pdf_article *part;

	pputs(s, "/Threads [ ");
	while ((part = pdev->articles) != 0) {
	    pdev->articles = part->next;
	    pprintld1(s, "%ld 0 R\n", part->id);
	    gs_free_string(pdev->pdf_memory, part->title.data,
			   part->title.size, "pdf_close(article title)");
	    gs_free_object(pdev->pdf_memory, part, "pdf_close(article)");
	}
	pputs(s, "]\n");
    }
    if (named_dests_id != 0)
	pprintld1(s, "/Dests %ld 0 R\n", named_dests_id);
    pdf_write_saved_string(pdev, &pdev->catalog_string);
    pputs(s, ">>\n");
    pdf_end_obj(pdev);

    /* Create the Info directory. */
    /* This is supposedly optional, but some readers may require it. */

    if (pdev->info_id == 0) {
	pdev->info_id = pdf_begin_obj(pdev);
	pputs(s, "<< ");
	pdf_write_default_info(pdev);
	pputs(s, ">>\n");
	pdf_end_obj(pdev);
    }
    /* Write the definitions of the named objects. */

    pdfmark_write_and_free_named(pdev, &pdev->named_objects);

    /* Copy the resources into the main file. */

    s = pdev->strm;
    resource_pos = stell(s);
    sflush(pdev->rstrm);
    {
	FILE *rfile = pdev->rfile;
	long res_end = ftell(rfile);
	byte buf[sbuf_size];

	fseek(rfile, 0L, SEEK_SET);
	while (res_end > 0) {
	    uint count = min(res_end, sbuf_size);

	    fread(buf, 1, sbuf_size, rfile);
	    pwrite(s, buf, count);
	    res_end -= count;
	}
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
	    if (pos & rfile_base_position)
		pos += resource_pos - rfile_base_position;
	    sprintf(str, "%010ld 00000 n \n", pos);
	    pputs(s, str);
	}
    }

    /* Write the trailer. */

    pputs(s, "trailer\n");
    pprintld3(s, "<< /Size %ld /Root %ld 0 R /Info %ld 0 R\n",
	      pdev->next_id, pdev->root_id, pdev->info_id);
    pputs(s, ">>\n");
    pprintld1(s, "startxref\n%ld\n%%%%EOF\n", xref);

    /* Release the resource records. */

    {
	pdf_resource *pres;
	pdf_resource *prev;

	for (prev = pdev->last_resource; (pres = prev) != 0;) {
	    prev = pres->prev;
	    gs_free_object(pdev->pdf_memory, pres, "pdf_resource");
	}
	pdev->last_resource = 0;
    }

    gs_free_object(pdev->pdf_memory, pdev->page_ids, "page_ids");
    pdev->page_ids = 0;
    pdev->num_page_ids = 0;

    gdev_vector_close_file((gx_device_vector *) pdev);
    pdf_close_files(pdev);
    return 0;
}
