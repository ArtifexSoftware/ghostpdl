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

/*
 * Given a text string and a simple gs_font, process it in chunks
 * determined by pdf_encode_string.
 */
private int pdf_encode_string(gx_device_pdf *pdev, gs_font_base *font,
			      const gs_string *pstr, int *pindex,
			      pdf_font_resource_t **ppdfont);
private int pdf_process_string(pdf_text_enum_t *penum, gs_string *pstr,
			       pdf_font_resource_t *pdfont,
			       const gs_matrix *pfmat,
			       pdf_text_process_state_t *ppts, int *pindex);
int
pdf_encode_process_string(pdf_text_enum_t *penum, gs_string *pstr,
			  const gs_matrix *pfmat,
			  pdf_text_process_state_t *ppts, int *pindex)
{
    gx_device_pdf *const pdev = (gx_device_pdf *)penum->dev;
    gs_font_base *font;
    int code = 0;

    switch (penum->current_font->FontType) {
    case ft_TrueType:
    case ft_encrypted:
    case ft_encrypted2:
	break;
    default:
	return_error(gs_error_rangecheck);
    }
    font = (gs_font_base *)penum->current_font;

    while (*pindex < pstr->size) {
	int index = *pindex;
	uint end = pstr->size;
	pdf_font_resource_t *pdfont;

	code = pdf_encode_string(pdev, font, pstr, &index, &pdfont);
	if (index == *pindex)
	    break;
	pstr->size = index;
	code = pdf_process_string(penum, pstr, pdfont, pfmat, ppts, pindex);
	pstr->size = end;
	if (code < 0 || *pindex < index)
	    break;
    }
    return code;
}

/*
 * Given a text string and a simple gs_font, return a font resource suitable
 * for a prefix of the text string, possibly re-encoding the string.  This
 * may involve creating a font resource and/or adding glyphs and/or Encoding
 * entries to it.
 *
 * Note that this procedure is not guaranteed to handle the entire string,
 * because a single string in a single gs_font may require the use of
 * several different font resources to represent it if all the characters
 * cannot be encoded using a single font resource.
 *
 * Reads and sets *pindex; reads and may modify pstr->data[*pindex ..
 * pstr->size - 1]; sets *ppdfont.
 */
private int
pdf_encode_string(gx_device_pdf *pdev, gs_font_base *font,
		  const gs_string *pstr, int *pindex,
		  pdf_font_resource_t **ppdfont)
{
    gs_font *bfont = (gs_font *)font;
    pdf_font_resource_t *pdfont;
    gs_font_base *base_font;
    int code, i;

    /*
     * This crude version of the code simply uses pdf_make_font_resource,
     * and never re-encodes characters.
     */
    code = pdf_make_font_resource(pdev, bfont, &pdfont);
    if (code < 0)
	return code;
    base_font = pdf_font_resource_font(pdfont);

    for (i = *pindex; i < pstr->size; ++i) {
	int ch = pstr->data[i];
	pdf_encoding_element_t *pet = &pdfont->u.simple.Encoding[ch];
	gs_glyph glyph =
	    font->procs.encode_char((gs_font *)font, ch, GLYPH_SPACE_NAME);
	gs_const_string gnstr;

	if (glyph == GS_NO_GLYPH || glyph == pet->glyph)
	    continue;
	code = font->procs.glyph_name((gs_font *)font, glyph, &gnstr);
	if (code < 0)
	    break;		/* can't get name of glyph */
	/* The standard 14 fonts don't have a FontDescriptor. */
	code = (pdfont->base_font != 0 ?
		pdf_base_font_copy_glyph(pdfont->base_font, glyph, font) :
		pdf_font_used_glyph(pdfont->FontDescriptor, glyph, font));
	if (code < 0)
	    break;		/* can't copy the glyph */
	pet->glyph = glyph;
	pet->str = gnstr;
	/****** SET is_difference CORRECTLY ******/
	if (pdfont->FontDescriptor != 0)
	    pet->is_difference = true;
    }
    *pindex = i;
    *ppdfont = pdfont;
    return 0;
}

/*
 * Internal procedure to process a string in a non-composite font.
 * Doesn't use or set pte->{data,size,index}; may use/set pte->xy_index;
 * may set penum->returned.total_width.  Sets ppts->values.
 *
 * Note that the caller is responsible for re-encoding the string, if
 * necessary; for adding Encoding entries in pdfont; and for copying any
 * necessary glyphs.  penum->current_font provides the gs_font for getting
 * glyph metrics, but this font's Encoding is not used.
 */
private int process_text_return_width(const gs_text_params_t *ptp,
				      gs_font_base *font,
				      pdf_font_resource_t *pdfont,
				      const gs_matrix *pfmat,
				      const gs_const_string *pstr,
				      int *pindex, gs_point *pdpt);
private int process_text_add_width(gs_text_enum_t *pte,
				   gs_font_base *font, const gs_matrix *pfmat,
				   pdf_text_process_state_t *ppts,
				   const gs_const_string *pstr,
				   int *pindex, gs_point *pdpt);
private int
pdf_process_string(pdf_text_enum_t *penum, gs_string *pstr,
		   pdf_font_resource_t *pdfont, const gs_matrix *pfmat,
		   pdf_text_process_state_t *ppts, int *pindex)
{
    gs_text_enum_t *const pte = (gs_text_enum_t *)penum;
    gx_device_pdf *const pdev = (gx_device_pdf *)penum->dev;
    gs_font_base *font = (gs_font_base *)penum->current_font;
    const gs_text_params_t *text = &pte->text;
    int i;
    int code = 0, mask;
    gs_point width_pt;

    if (pfmat == 0)
	pfmat = &font->FontMatrix;
    if (text->operation & TEXT_RETURN_WIDTH)
	gx_path_current_point(pte->path, &penum->origin);

    /*
     * Note that pdf_update_text_state sets all the members of ppts->values
     * to their current values.
     */
    code = pdf_update_text_state(ppts, penum, pdfont, pfmat);
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

    /*
     * For incrementally loaded fonts, expand FirstChar..LastChar
     * if needed.
     */

    for (i = 0; i < pstr->size; ++i) {
	int chr = pstr->data[i];

	if (chr < pdfont->u.simple.FirstChar)
	    pdfont->u.simple.FirstChar = chr;
	if (chr > pdfont->u.simple.LastChar)
	    pdfont->u.simple.LastChar = chr;
    }

    /* Bring the text-related parameters in the output up to date. */
    code = pdf_set_text_process_state(pdev, pte, ppts,
				      (gs_const_string *)pstr);
    if (code < 0)
	return code;

    if (text->operation & TEXT_REPLACE_WIDTHS) {
	gs_point w;

	w.x = w.y = 0;
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
	    ppts->values.matrix.tx += dpt.x;
	    ppts->values.matrix.ty += dpt.y;
	    if (i + 1 < pstr->size) {
		code = pdf_set_text_state_values(pdev, &ppts->values,
						 TEXT_STATE_SET_MATRIX);
		if (code < 0)
		    return code;
	    }
	}
	pte->returned.total_width = w;
	if (text->operation & TEXT_RETURN_WIDTH)
	    code = gx_path_add_point(pte->path,
				     float2fixed(ppts->values.matrix.tx),
				     float2fixed(ppts->values.matrix.ty));
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
	code = process_text_return_width(&pte->text, font, pdfont, pfmat,
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
	ppts->members = TEXT_STATE_SET_CHARACTER_SPACING |
	    TEXT_STATE_SET_WORD_SPACING;
	ppts->values.character_spacing = ppts->values.word_spacing = 0;
	code = pdf_set_text_process_state(pdev, pte, ppts,
					  (gs_const_string *)pstr);
	if (code < 0)
	    return code;
	code = process_text_add_width(pte, font, pfmat, ppts,
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
 * Get the widths (unmodified and possibly modified) of a given character
 * in a simple font.  May add the widths to the widths cache (pdfont->Widths
 * and pdfont->real_widths).  Return 1 if the widths were not cached.
 */
private int
pdf_char_widths(pdf_font_resource_t *pdfont, int ch, gs_font_base *font,
		pdf_glyph_widths_t *pwidths /* may be NULL */)
{
    pdf_glyph_widths_t widths;
    int code;

    if (ch < 0 || ch > 255)
	return_error(gs_error_rangecheck);
    if (pwidths == 0)
	pwidths = &widths;
    if (pdfont->Widths[ch] == 0) {
	/* Might be an unused char, or just not cached. */
	gs_glyph glyph = pdfont->u.simple.Encoding[ch].glyph;

	code = pdf_glyph_widths(pdfont, glyph, font, pwidths);
	if (code < 0)
	    return code;
	if (code == 0) {
	    pdfont->Widths[ch] = pwidths->Width;
	    pdfont->real_widths[ch] = pwidths->real_width;
	}
	pdfont->used[ch >> 3] |= 0x80 >> (ch & 7);
    } else {
	pwidths->Width = pdfont->Widths[ch];
	pwidths->real_width = pdfont->real_widths[ch];
	code = 0;
    }
    return code;
}

/*
 * Compute the total text width (in user space).  Return 1 if any
 * character had real_width != Width, otherwise 0.
 */
private int
process_text_return_width(const gs_text_params_t *ptp, gs_font_base *font,
			  pdf_font_resource_t *pdfont, const gs_matrix *pfmat,
			  const gs_const_string *pstr,
			  int *pindex, gs_point *pdpt)
{
    int i, w;
    gs_matrix smat;
    gs_point dpt;
    int num_spaces = 0;
    int space_char =
	(ptp->operation & TEXT_ADD_TO_SPACE_WIDTH ?
	 ptp->space.s_char : -1);
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
    pdf_font_orig_matrix((gs_font *)font, &smat);
    gs_distance_transform(w / 1000.0 / smat.xx, 0.0, pfmat, &dpt);
    if (ptp->operation & TEXT_ADD_TO_ALL_WIDTHS) {
	int num_chars = pstr->size - *pindex;

	dpt.x += ptp->delta_all.x * num_chars;
	dpt.y += ptp->delta_all.y * num_chars;
    }
    if (ptp->operation & TEXT_ADD_TO_SPACE_WIDTH) {
	dpt.x += ptp->delta_space.x * num_spaces;
	dpt.y += ptp->delta_space.y * num_spaces;
    }
    *pindex = i;
    *pdpt = dpt;
    return widths_differ;
}
/*
 * Emulate TEXT_ADD_TO_ALL_WIDTHS and/or TEXT_ADD_TO_SPACE_WIDTH.
 * We know that the Tw and Tc values are zero.  Uses and updates
 * ppts->values.matrix; uses ppts->values.font.
 */
private int
process_text_add_width(gs_text_enum_t *pte, gs_font_base *font,
		       const gs_matrix *pfmat,
		       pdf_text_process_state_t *ppts,
		       const gs_const_string *pstr,
		       int *pindex, gs_point *pdpt)
{
    gx_device_pdf *const pdev = (gx_device_pdf *)pte->dev;
    int i, w;
    gs_matrix smat;
    double scale;
    gs_point dpt;
    int space_char =
	(pte->text.operation & TEXT_ADD_TO_SPACE_WIDTH ?
	 pte->text.space.s_char : -1);
    int code = 0;
    bool move = false;

    pdf_font_orig_matrix((gs_font *)font, &smat);
    scale = 0.001 / smat.xx;
    dpt.x = dpt.y = 0;
    for (i = *pindex, w = 0; i < pstr->size; ++i) {
	pdf_glyph_widths_t cw;
	int code = pdf_char_widths(ppts->values.pdfont, pstr->data[i], font,
				   &cw);
	gs_point wpt;

	if (code < 0)
	    break;
	if (move) {
	    gs_point mpt;

	    gs_distance_transform(dpt.x, dpt.y, &ctm_only(pte->pis), &mpt);
	    ppts->values.matrix.tx += mpt.x;
	    ppts->values.matrix.ty += mpt.y;
	    code = pdf_set_text_state_values(pdev, &ppts->values,
					     TEXT_STATE_SET_MATRIX);
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
    uint count;
    uint operation = pte->text.operation;
    pdf_text_enum_t *const penum = (pdf_text_enum_t *)pte;
    int index = 0, code;
    gs_string str;
    bool encoded;
    pdf_text_process_state_t text_state;

    if (operation & (TEXT_FROM_STRING | TEXT_FROM_BYTES)) {
	count = size;
	memcpy(buf, (const byte *)vdata + pte->index, size);
	encoded = false;
    } else if (operation & (TEXT_FROM_CHARS | TEXT_FROM_SINGLE_CHAR)) {
	/* Check that all chars fit in a single byte. */
	const gs_char *const cdata = vdata;
	int i;

	count = size / sizeof(gs_char);
	for (i = 0; i < count; ++i) {
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
#if 1				/* **************** */
	return_error(gs_error_rangecheck);
#else				/* **************** */
	const gs_glyph *const gdata = vdata;
	gs_font *font = pte->current_font;
	int i;

	count = size / sizeof(gs_glyph);
	code = pdf_update_text_state(&text_state, penum, &font->FontMatrix);
	if (code < 0)
	    return code;
	for (i = 0; i < count; ++i) {
	    gs_glyph glyph = gdata[pte->index + i];
	    int chr = pdf_encode_glyph((gx_device_pdf *)pte->dev, -1, glyph,
				       (gs_font_base *)font,
				       text_state.values.pdfont);

	    if (chr < 0)
		return_error(gs_error_rangecheck);
	    buf[i] = (byte)chr;
	}
	encoded = true;
#endif				/* **************** */
    } else
	return_error(gs_error_rangecheck);
    str.data = buf;
    if (count > 1 && (pte->text.operation & TEXT_INTERVENE)) {
	/* Just do one character. */
	str.size = 1;
	code = pdf_encode_process_string(penum, &str, NULL, &text_state,
					 &index);
	if (code >= 0) {
	    pte->returned.current_char = buf[0];
	    code = TEXT_PROCESS_INTERVENE;
	}
    } else {
	str.size = count;
	code = pdf_encode_process_string(penum, &str, NULL, &text_state,
					 &index);
    }
    pte->index += index;
    return code;
}

/****************************************************************
./src/gdevpdte.c: In function `process_plain_text':
./src/gdevpdte.c:413: warning: passing arg 3 of `pdf_update_text_state' from incompatible pointer type
./src/gdevpdte.c:413: too few arguments to function `pdf_update_text_state'
./src/gdevpdte.c:418: warning: implicit declaration of function `pdf_encode_glyph'
./src/gdevpdte.c:433: parse error before `,'
./src/gdevpdte.c:441: parse error before `,'
./src/gdevpdte.c:380: warning: `code' might be used uninitialized in this function
 ****************************************************************/
