/* Copyright (C) 2002 Aladdin Enterprises.  All rights reserved.
  
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
/* Internal text processing interface for pdfwrite */

#ifndef gdevpdtt_INCLUDED
#  define gdevpdtt_INCLUDED

/*
 * This file is only used internally to define the interface between
 * gdevpdtt.c, gdevpdtc.c, and gdevpdte.c.
 */

/* ---------------- Types and structures ---------------- */

/* Define the text enumerator. */
typedef struct pdf_text_enum_s {
    gs_text_enum_common;
    gs_text_enum_t *pte_default;
    gs_fixed_point origin;
} pdf_text_enum_t;
#define private_st_pdf_text_enum()\
  extern_st(st_gs_text_enum);\
  gs_private_st_suffix_add1(st_pdf_text_enum, pdf_text_enum_t,\
    "pdf_text_enum_t", pdf_text_enum_enum_ptrs, pdf_text_enum_reloc_ptrs,\
    st_gs_text_enum, pte_default)

/*
 * Define quantities derived from the current font and CTM, used within
 * the text processing loop.  NOTE: This structure has no GC descriptor
 * and must therefore only be used locally (allocated on the C stack).
 */
typedef struct pdf_text_process_state_s {
    pdf_text_state_values_t values;
    int members;		/* which values need to be set */
    gs_font *font;
} pdf_text_process_state_t;

/*
 * Define the structure used to return glyph width information.
 */
typedef struct pdf_glyph_widths_s {
    int Width;			/* unmodified, for Widths */
    int real_width;		/* possibly modified, for rendering */
} pdf_glyph_widths_t;

/* ---------------- Procedures ---------------- */

/*
 * Declare the procedures for processing different species of text.
 * These procedures may, but need not, copy pte->text into buf
 * (a caller-supplied buffer large enough to hold the string).
 */
#define PROCESS_TEXT_PROC(proc)\
  int proc(gs_text_enum_t *pte, const void *vdata, void *vbuf, uint size)

/* ------ gdevpdtt.c ------ */

/*
 * Compute and return the orig_matrix of a font.
 */
int pdf_font_orig_matrix(const gs_font *font, gs_matrix *pmat);

/*
 * Find or create the font resource for a gs_font.  Return 1 if the font
 * was newly created.
 */
int pdf_make_font_resource(gx_device_pdf *pdev, gs_font *font,
			   pdf_font_resource_t **ppdfont);

/*
 * Compute the cached values in the text processing state from the text
 * parameters, current_font, and pis->ctm.  Return either an error code (<
 * 0) or a mask of operation attributes that the caller must emulate.
 * Currently the only such attributes are TEXT_ADD_TO_ALL_WIDTHS and
 * TEXT_ADD_TO_SPACE_WIDTH.  Note that this procedure fills in all the
 * values in ppts->values, not just the ones that need to be set now.
 */
int pdf_update_text_state(pdf_text_process_state_t *ppts,
			  const pdf_text_enum_t *penum,
			  pdf_font_resource_t *pdfont, const gs_matrix *pfmat);

/*
 * Set up commands to make the output state match the processing state.
 * General graphics state commands are written now; text state commands
 * are written later.  Update ppts->values to reflect all current values.
 */
int pdf_set_text_process_state(gx_device_pdf *pdev,
			const gs_text_enum_t *pte, /* for pdcolor, pis */
			       pdf_text_process_state_t *ppts,
			       const gs_const_string *pstr);

/*
 * Get the widths (unmodified and possibly modified) of a glyph in a (base)
 * font.  Return 1 if the width should not be cached.
 */
int pdf_glyph_widths(pdf_font_resource_t *pdfont, gs_glyph glyph,
		     gs_font_base *font, pdf_glyph_widths_t *pwidths);

/*
 * Fall back to the default text processing code when needed.
 */
int pdf_default_text_begin(gs_text_enum_t *pte, const gs_text_params_t *text,
			   gs_text_enum_t **ppte);

/* ------ gdevpdtc.c ------ */

PROCESS_TEXT_PROC(process_composite_text);
PROCESS_TEXT_PROC(process_cmap_text);
PROCESS_TEXT_PROC(process_cid_text);

/* ------ gdevpdte.c ------ */

PROCESS_TEXT_PROC(process_plain_text);

/*
 * Internal procedure to process a string in a simple font.
 * Doesn't use or set penum->{data,size,index}; may use/set penum->xy_index;
 * may set penum->returned.total_width.  Sets ppts->values.
 *
 * Note that the caller is responsible for re-encoding the string, if
 * necessary; for adding Encoding entries in pdfont; and for copying any
 * necessary glyphs.  penum->current_font provides the gs_font for getting
 * glyph metrics, but this font's Encoding is not used.
 */
int pdf_process_string(pdf_text_enum_t *penum, gs_string *pstr,
		       pdf_font_resource_t *pdfont, const gs_matrix *pfmat,
		       pdf_text_process_state_t *ppts, int *pindex);

#endif /* gdevpdtt_INCLUDED */
