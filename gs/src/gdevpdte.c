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
/* Encoding-based (Type 1/2/42) text processing for pdfwrite. */
#include "math_.h"
#include "memory_.h"
#include "gx.h"
#include "gserrors.h"
#include "gxfcmap.h"
#include "gxfont.h"
#include "gxfont0.h"
#include "gxfont0c.h"
#include "gxpath.h"		/* for getting current point */
#include "gdevpsf.h"
#include "gdevpdfx.h"
#include "gdevpdfg.h"
#include "gdevpdtx.h"
#include "gdevpdtd.h"
#include "gdevpdtf.h"
#include "gdevpdts.h"
#include "gdevpdtt.h"

/****************************************************************
 **************** PATCH *****************************************
 ****************************************************************/
#if 1

int
pdf_process_string(pdf_text_enum_t *penum, gs_string *pstr,
		   const gs_matrix *pfmat, bool encoded,
		   pdf_text_process_state_t *pts, int *pindex)
{
    return -1;
}

int
process_plain_text(gs_text_enum_t *pte, const void *vdata, void *vbuf,
		   uint size)
{
    return -1;
}

#else

/*
 * Internal procedure to process a string in a non-composite font.
 * Doesn't use or set pte->{data,size,index}; may use/set pte->xy_index;
 * may set pte->returned.total_width.
 */
private int process_text_return_width(const gs_text_enum_t *pte,
				      gs_font *font, pdf_font_t *pdfont,
				      const gs_matrix *pfmat,
				      const gs_const_string *pstr,
				      int *pindex, gs_point *pdpt);
private int process_text_add_width(gs_text_enum_t *pte,
				   gs_font *font, const gs_matrix *pfmat,
				   const pdf_text_process_state_t *ppts,
				   const gs_const_string *pstr,
				   int *pindex, gs_point *pdpt);
int
pdf_process_string(pdf_text_enum_t *penum, gs_string *pstr,
		   const gs_matrix *pfmat, bool encoded,
		   pdf_text_process_state_t *pts, int *pindex)
{
    gs_text_enum_t *const pte = (gs_text_enum_t *)penum;
    gx_device_pdf *const pdev = (gx_device_pdf *)penum->dev;
    gs_font *font;
    const gs_text_params_t *text = &pte->text;
    int i;
    int code = 0, mask;
    gs_point width_pt;

    font = penum->current_font;
    if (pfmat == 0)
	pfmat = &font->FontMatrix;
    if (text->operation & TEXT_RETURN_WIDTH)
	gx_path_current_point(pte->path, &penum->origin);

    switch (font->FontType) {
    case ft_TrueType:
    case ft_encrypted:
    case ft_encrypted2:
	break;
    default:
	return_error(gs_error_rangecheck);
    }

    code = pdf_update_text_state(pts, penum, pfmat);
    if (code > 0) {
	/* Try not to emulate ADD_TO_WIDTH if we don't have to. */
	if (code & TEXT_ADD_TO_SPACE_WIDTH) {
	    if (!memchr(pstr->data, pte->text.space.s_char, pstr->size))
		code &= ~TEXT_ADD_TO_SPACE_WIDTH;
	}
    }
    if (code < 0)
	return code;
    mask = code;

    /* Check that all characters can be encoded. */

    for (i = 0; i < pstr->size; ++i) {
	int chr = pstr->data[i];
	pdf_font_resource_t *pdfont = pts->in.font;
	int code =
	    (encoded ? chr :
	     pdf_encode_char(pdev, chr, (gs_font_base *)font, pdfont));

	if (code < 0)
	    return code;
	/*
	 * For incrementally loaded fonts, expand FirstChar..LastChar
	 * if needed.
	 */
	if (code < pdfont->u.simple.FirstChar)
	    pdfont->u.simple.FirstChar = code;
	if (code > pdfont->u.simple.LastChar)
	    pdfont->u.simple.LastChar = code;
	pstr->data[i] = (byte)code;
    }

    /* Bring the text-related parameters in the output up to date. */
    code = pdf_write_text_process_state(pdev, pte, pts,
					(gs_const_string *)pstr);
    if (code < 0)
	return code;

    if (text->operation & TEXT_REPLACE_WIDTHS) {
	gs_point w;
	gs_matrix tmat;

	w.x = w.y = 0;
	tmat = pts->text_matrix;
	for (i = 0; i < pstr->size; *pindex = ++i, pte->xy_index++) {
	    gs_point d, dpt;

	    gs_text_replaced_width(&pte->text, pte->xy_index, &d);
	    w.x += d.x, w.y += d.y;
	    gs_distance_transform(d.x, d.y, &ctm_only(pte->pis), &dpt);
	    if (text->operation & TEXT_DO_DRAW) {
		code = pdf_append_chars(pdev, pstr->data + i, 1, dpt.x, dpt.y);
		if (code < 0)
		    return code;
	    }
	    tmat.tx += dpt.x;
	    tmat.ty += dpt.y;
	    if (i + 1 < pstr->size) {
		code = pdf_set_text_matrix(pdev, &tmat);
		if (code < 0)
		    return code;
	    }
	}
	pte->returned.total_width = w;
	if (text->operation & TEXT_RETURN_WIDTH)
	    code = gx_path_add_point(pte->path, float2fixed(tmat.tx),
				     float2fixed(tmat.ty));
	return code;
    }

    /*
     * The only operations left to handle are TEXT_DO_DRAW and
     * TEXT_RETURN_WIDTH.
     */
    if (mask == 0) {
	/*
	 * If any character has real_width != Width, we have to process
	 * the string character-by-character.  process_text_return_width
	 * will tell us what we need to know.
	 */
	int save_index = *pindex;

	if (!(text->operation & (TEXT_DO_DRAW | TEXT_RETURN_WIDTH)))
	    return 0;
	code = process_text_return_width(pte, font, pts->pdfont, pfmat,
					 (gs_const_string *)pstr,
					 pindex, &width_pt);
	if (code < 0)
	    return code;
	if (code == 0) {
	    /* No characters with redefined widths -- the fast case. */
	    if (text->operation & TEXT_DO_DRAW) {
		code = pdf_append_chars(pdev, pstr->data, pstr->size,
					width_pt.x, width_pt.y);
		if (code < 0)
		    return code;
	    }
	} else {
	    /* Use the slow case.  Set mask to any non-zero value. */
	    mask = TEXT_RETURN_WIDTH;
	    *pindex = save_index;
	}
    }
    if (mask) {
	/*
	 * Cancel the word and character spacing, since we're going
	 * to emulate them.
	 */
	pts->words = pts->chars = 0;
	code = pdf_write_text_process_state(pdev, pte, pts,
					    (gs_const_string *)pstr);
	if (code < 0)
	    return code;
	code = process_text_add_width(pte, font, pfmat, pts,
				      (gs_const_string *)pstr,
				      pindex, &width_pt);
	if (code < 0)
	    return code;
    }


    /* Finally, return the total width if requested. */
    if (!(text->operation & TEXT_RETURN_WIDTH))
	return 0;
    pte->returned.total_width = width_pt;
    gs_distance_transform(width_pt.x, width_pt.y, &ctm_only(pte->pis),
			  &width_pt);
    return gx_path_add_point(pte->path,
			     penum->origin.x + float2fixed(width_pt.x),
			     penum->origin.y + float2fixed(width_pt.y));
}

/*
 * Compute the total text width (in user space).  Return 1 if any
 * character had real_width != Width, otherwise 0.
 */
private int
process_text_return_width(const gs_text_enum_t *pte, gs_font *font,
			  pdf_font_t *pdfont, const gs_matrix *pfmat,
			  const gs_const_string *pstr,
			  int *pindex, gs_point *pdpt)
{
    int i, w;
    gs_matrix smat;
    gs_point dpt;
    int num_spaces = 0;
    int space_char =
	(pte->text.operation & TEXT_ADD_TO_SPACE_WIDTH ?
	 pte->text.space.s_char : -1);
    int widths_differ = 0;

    for (i = *pindex, w = 0; i < pstr->size; ++i) {
	pdf_glyph_widths_t cw;
	int code = pdf_char_widths(pdfont, pstr->data[i], font, &cw);

	if (code < 0)
	    return code;
	w += cw.real_width;
	if (cw.real_width != cw.Width)
	    widths_differ = 1;
	if (pstr->data[i] == space_char)
	    ++num_spaces;
    }
    pdf_font_orig_matrix(font, &smat);
    gs_distance_transform(w / 1000.0 / smat.xx, 0.0, pfmat, &dpt);
    if (pte->text.operation & TEXT_ADD_TO_ALL_WIDTHS) {
	int num_chars = pstr->size - pte->index;

	dpt.x += pte->text.delta_all.x * num_chars;
	dpt.y += pte->text.delta_all.y * num_chars;
    }
    if (pte->text.operation & TEXT_ADD_TO_SPACE_WIDTH) {
	dpt.x += pte->text.delta_space.x * num_spaces;
	dpt.y += pte->text.delta_space.y * num_spaces;
    }
    *pindex = i;
    *pdpt = dpt;
    return widths_differ;
}
/*
 * Emulate TEXT_ADD_TO_ALL_WIDTHS and/or TEXT_ADD_TO_SPACE_WIDTH.
 * We know that the Tw and Tc values are zero.
 */
private int
process_text_add_width(gs_text_enum_t *pte, gs_font *font,
		       const gs_matrix *pfmat,
		       const pdf_text_process_state_t *ppts,
		       const gs_const_string *pstr,
		       int *pindex, gs_point *pdpt)
{
    gx_device_pdf *const pdev = (gx_device_pdf *)pte->dev;
    int i, w;
    gs_matrix smat;
    double scale;
    gs_point dpt;
	gs_matrix tmat;
    int space_char =
	(pte->text.operation & TEXT_ADD_TO_SPACE_WIDTH ?
	 pte->text.space.s_char : -1);
    int code = 0;
    bool move = false;

    pdf_font_orig_matrix(font, &smat);
    scale = 0.001 / smat.xx;
    dpt.x = dpt.y = 0;
    tmat = ppts->text_matrix;
    for (i = *pindex, w = 0; i < pstr->size; ++i) {
	pdf_glyph_widths_t cw;
	int code = pdf_char_widths(ppts->pdfont, pstr->data[i], font, &cw);
	gs_point wpt;

	if (code < 0)
	    break;
	if (move) {
	    gs_point mpt;

	    gs_distance_transform(dpt.x, dpt.y, &ctm_only(pte->pis), &mpt);
	    tmat.tx = ppts->text_matrix.tx + mpt.x;
	    tmat.ty = ppts->text_matrix.ty + mpt.y;
	    code = pdf_set_text_matrix(pdev, &tmat);
	    if (code < 0)
		break;
	    move = false;
	}
	gs_distance_transform(cw.real_width * scale, 0.0, pfmat, &wpt);
	if (pte->text.operation & TEXT_DO_DRAW) {
	    code = pdf_append_chars(pdev, &pstr->data[i], 1, wpt.x, wpt.y);
	    if (code < 0)
		break;
	}
	dpt.x += wpt.x, dpt.y += wpt.y;
	if (cw.real_width != cw.Width)
	    move = true;
	if (pte->text.operation & TEXT_ADD_TO_ALL_WIDTHS) {
	    dpt.x += pte->text.delta_all.x;
	    dpt.y += pte->text.delta_all.y;
	    move = true;
	}
	if (pstr->data[i] == space_char) {
	    dpt.x += pte->text.delta_space.x;
	    dpt.y += pte->text.delta_space.y;
	    move = true;
	}
    }
    *pindex = i;		/* only do the part we haven't done yet */
    *pdpt = dpt;
    return code;
}

/* ---------------- Type 1 or TrueType font ---------------- */

/*
 * Process a text string in an ordinary font.
 */
int
process_plain_text(gs_text_enum_t *pte, const void *vdata, void *vbuf,
		   uint size)
{
    byte *const buf = vbuf;
    uint operation = pte->text.operation;
    pdf_text_enum_t *const penum = (pdf_text_enum_t *)pte;
    int index = 0, code;
    gs_string str;
    bool encoded;
    pdf_text_process_state_t text_state;

    if (operation & (TEXT_FROM_STRING | TEXT_FROM_BYTES)) {
	memcpy(buf, (const byte *)vdata + pte->index, size);
	encoded = false;
    } else if (operation & (TEXT_FROM_CHARS | TEXT_FROM_SINGLE_CHAR)) {
	/* Check that all chars fit in a single byte. */
	const gs_char *const cdata = vdata;
	int i;

	size /= sizeof(gs_char);
	for (i = 0; i < size; ++i) {
	    gs_char chr = cdata[pte->index + i];

	    if (chr & ~0xff)
		return_error(gs_error_rangecheck);
	    buf[i] = (byte)chr;
	}
	encoded = false;
    } else if (operation & (TEXT_FROM_GLYPHS | TEXT_FROM_SINGLE_GLYPH)) {
	/*
	 * Reverse-map the glyphs into the encoding.  Eventually, assign
	 * them to empty slots if missing.
	 */
	const gs_glyph *const gdata = vdata;
	gs_font *font = pte->current_font;
	int i;

	size /= sizeof(gs_glyph);
	code = pdf_update_text_state(&text_state, penum, &font->FontMatrix);
	if (code < 0)
	    return code;
	for (i = 0; i < size; ++i) {
	    gs_glyph glyph = gdata[pte->index + i];
	    int chr = pdf_encode_glyph((gx_device_pdf *)pte->dev, -1, glyph,
				       (gs_font_base *)font,
				       text_state.values.pdfont);

	    if (chr < 0)
		return_error(gs_error_rangecheck);
	    buf[i] = (byte)chr;
	}
	encoded = true;
    } else
	return_error(gs_error_rangecheck);
    str.data = buf;
    if (size > 1 && (pte->text.operation & TEXT_INTERVENE)) {
	/* Just do one character. */
	str.size = 1;
	code = pdf_process_string(penum, &str, NULL, encoded, &text_state,
				  &index);
	if (code >= 0) {
	    pte->returned.current_char = buf[0];
	    code = TEXT_PROCESS_INTERVENE;
	}
    } else {
	str.size = size;
	code = pdf_process_string(penum, &str, NULL, encoded, &text_state,
				  &index);
    }
    pte->index += index;
    return code;
}

/****************************************************************
 **************** PATCH *****************************************
 ****************************************************************/
#endif
