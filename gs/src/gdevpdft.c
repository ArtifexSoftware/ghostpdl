/* Copyright (C) 1996, 2000 Aladdin Enterprises.  All rights reserved.
  
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
*/

/*$Id$ */
/* Text handling for PDF-writing driver. */
#include "math_.h"
#include "memory_.h"
#include "string_.h"
#include "gx.h"
#include "gserrors.h"
#include "gsmatrix.h"
#include "gsutil.h"		/* for bytes_compare */
#include "gxfixed.h"		/* for gxfcache.h */
#include "gxfont.h"
#include "gxfont0.h"
#include "gxfont1.h"
#include "gxfont42.h"
#include "gxfcache.h"		/* for orig_fonts list */
#include "gxfcid.h"
#include "gxpath.h"		/* for getting current point */
#include "gdevpdfx.h"
#include "gdevpdff.h"
#include "gdevpdfg.h"
#include "scommon.h"

/*
 * We have to handle re-encoded Type 1 fonts, because TeX output makes
 * constant use of them.  We have several alternatives for how to handle
 * characters whose encoding doesn't match their encoding in the base font's
 * built-in encoding.  If a character's glyph doesn't match the character's
 * glyph in the encoding built up so far, we check if the font has that
 * glyph at all; if not, we fall back to a bitmap.  Otherwise, we use one or
 * both of the following algorithms:
 *
 *      1. If this is the first time a character at this position has been
 *      seen, assign its glyph to that position in the encoding.
 *      We do this step if the device parameter ReAssignCharacters is true.
 *      (This is the default.)
 *
 *      2. If the glyph is present in the encoding at some other position,
 *      substitute that position for the character; otherwise, assign the
 *      glyph to an unoccupied (.notdef) position.
 *      We do this step if the device parameter ReEncodeCharacters is true.
 *      (This is the default.)
 *
 *      3. Finally, fall back to using a bitmap.
 *
 * If it is essential that all strings in the output contain exactly the
 * same character codes as the input, set ReEncodeCharacters to false.  If
 * it is important that strings be searchable, but some non-searchable
 * strings can be tolerated, the defaults are appropriate.  If searchability
 * is not important, set ReAssignCharacters to false.
 * 
 * Since the chars_used bit vector keeps track of characters by their
 * position in the Encoding, not by their glyph identifier, it can't record
 * use of unencoded characters.  Therefore, if we ever use an unencoded
 * character in an embedded font, we force writing the entire font rather
 * than a subset.  This is inelegant but (for the moment) too awkward to
 * fix.
 *
 * Acrobat Reader 3's Print function has a bug that make re-encoded
 * characters print as blank if the font is substituted (not embedded or one
 * of the base 14).  To work around this bug, when CompatibilityLevel <= 1.2,
 * for non-embedded non-base fonts, no substitutions or re-encodings are
 * allowed.
 */

/* Forward references */
private int encoding_find_glyph(P3(gs_font_base *bfont, gs_glyph font_glyph,
				   gs_encoding_index_t index));
private int pdf_set_font_and_size(P3(gx_device_pdf * pdev, pdf_font_t * font,
				     floatp size));
private int pdf_set_text_matrix(P2(gx_device_pdf * pdev,
				   const gs_matrix * pmat));
private int pdf_append_chars(P3(gx_device_pdf * pdev, const byte * str,
				uint size));

/* Define the text enumerator. */
typedef struct pdf_text_enum_s {
    gs_text_enum_common;
    gs_text_enum_t *pte_default;
    gs_fixed_point origin;
} pdf_text_enum_t;
extern_st(st_gs_text_enum);
gs_private_st_suffix_add1(st_pdf_text_enum, pdf_text_enum_t, "pdf_text_enum_t",
  pdf_text_enum_enum_ptrs, pdf_text_enum_reloc_ptrs, st_gs_text_enum,
  pte_default);

/* Define the auxiliary procedures for text processing. */
private int
pdf_text_resync(gs_text_enum_t *pte, const gs_text_enum_t *pfrom)
{
    pdf_text_enum_t *const penum = (pdf_text_enum_t *)pte;

    if ((pte->text.operation ^ pfrom->text.operation) & ~TEXT_FROM_ANY)
	return_error(gs_error_rangecheck);
    if (penum->pte_default) {
	int code = gs_text_resync(penum->pte_default, pfrom);

	if (code < 0)
	    return code;
    }
    pte->text = pfrom->text;
    gs_text_enum_copy_dynamic(pte, pfrom, false);
    return 0;
}
private bool
pdf_text_is_width_only(const gs_text_enum_t *pte)
{
    const pdf_text_enum_t *const penum = (const pdf_text_enum_t *)pte;

    if (penum->pte_default)
	return gs_text_is_width_only(penum->pte_default);
    return false;
}
private int
pdf_text_current_width(const gs_text_enum_t *pte, gs_point *pwidth)
{
    const pdf_text_enum_t *const penum = (const pdf_text_enum_t *)pte;

    if (penum->pte_default)
	return gs_text_current_width(penum->pte_default, pwidth);
    return_error(gs_error_rangecheck); /* can't happen */
}
private int
pdf_text_set_cache(gs_text_enum_t *pte, const double *pw,
		   gs_text_cache_control_t control)
{
    pdf_text_enum_t *const penum = (pdf_text_enum_t *)pte;

    if (penum->pte_default)
	return gs_text_set_cache(penum->pte_default, pw, control);
    return_error(gs_error_rangecheck); /* can't happen */
}
private int
pdf_text_retry(gs_text_enum_t *pte)
{
    pdf_text_enum_t *const penum = (pdf_text_enum_t *)pte;

    if (penum->pte_default)
	return gs_text_retry(penum->pte_default);
    return_error(gs_error_rangecheck); /* can't happen */
}
private void
pdf_text_release(gs_text_enum_t *pte, client_name_t cname)
{
    pdf_text_enum_t *const penum = (pdf_text_enum_t *)pte;

    if (penum->pte_default) {
	gs_text_release(penum->pte_default, cname);
	penum->pte_default = 0;
    }
    gx_default_text_release(pte, cname);
}

/* Begin processing text. */
private text_enum_proc_process(pdf_text_process);
private const gs_text_enum_procs_t pdf_text_procs = {
    pdf_text_resync, pdf_text_process,
    pdf_text_is_width_only, pdf_text_current_width,
    pdf_text_set_cache, pdf_text_retry,
    pdf_text_release
};
int
gdev_pdf_text_begin(gx_device * dev, gs_imager_state * pis,
		    const gs_text_params_t *text, gs_font * font,
		    gx_path * path, const gx_device_color * pdcolor,
		    const gx_clip_path * pcpath,
		    gs_memory_t * mem, gs_text_enum_t ** ppte)
{
    gx_device_pdf *const pdev = (gx_device_pdf *)dev;
    pdf_text_enum_t *penum;
    gs_fixed_point cpt;
    int code;

    /* Track the dominant text rotation. */
    {
	gs_matrix tmat;
	int i;

	gs_matrix_multiply(&font->FontMatrix, &ctm_only(pis), &tmat);
	if (is_xxyy(&tmat))
	    i = (tmat.xx >= 0 ? 0 : 2);
	else if (is_xyyx(&tmat))
	    i = (tmat.xy >= 0 ? 1 : 3);
	else
	    i = 4;
	pdf_current_page(pdev)->text_rotation.counts[i] += text->size;
    }

    code = pdf_prepare_fill(pdev, pis);
    if (code < 0)
	return code;

    if ((text->operation & TEXT_DO_ANY_CHARPATH) !=0 ||
	gx_path_current_point(path, &cpt) < 0
	)
	return gx_default_text_begin(dev, pis, text, font, path, pdcolor,
				     pcpath, mem, ppte);

    /*
     * Set the clipping path and drawing color.  We set both the fill
     * and stroke color, because we don't know whether the fonts will
     * be filled or stroked, and we can't set a color while we are in
     * text mode.  (This is a consequence of the implementation, not a
     * limitation of PDF.)
     */

    if (pdf_must_put_clip_path(pdev, pcpath)) {
	int code = pdf_open_page(pdev, PDF_IN_STREAM);

	if (code < 0)
	    return code;
	pdf_put_clip_path(pdev, pcpath);
    }

    if ((code = pdf_set_drawing_color(pdev, pdcolor, &pdev->stroke_color,
				      &psdf_set_stroke_color_commands)) < 0 ||
	(code = pdf_set_drawing_color(pdev, pdcolor, &pdev->fill_color,
				      &psdf_set_fill_color_commands)) < 0
	)
	return code;

    /* Allocate and initialize the enumerator. */

    rc_alloc_struct_1(penum, pdf_text_enum_t, &st_pdf_text_enum, mem,
		      return_error(gs_error_VMerror), "gdev_pdf_text_begin");
    penum->rc.free = rc_free_text_enum;
    penum->pte_default = 0; 
    code = gs_text_enum_init((gs_text_enum_t *)penum, &pdf_text_procs,
			     dev, pis, text, font, path, pdcolor, pcpath, mem);
    if (code < 0) {
	gs_free_object(mem, penum, "gdev_pdf_text_begin");
	return code;
    }

    *ppte = (gs_text_enum_t *)penum;

    return 0;
}

/*
 * Continue processing text.  Per the check in pdf_text_begin, we know the
 * operation is not a charpath, but it could be anything else.
 */

/*
 * Define quantities derived from the current font and CTM, used within
 * the text processing loop.
 */
typedef struct pdf_text_process_state_s {
    float chars;		/* scaled character spacing (Tc) */
    float words;		/* scaled word spacing (Tw) */
    float size;			/* font size for Tf */
    gs_matrix text_matrix;	/* normalized FontMatrix * CTM for Tm */
    int mode;			/* render mode (Tr) */
    gs_font *font;
    pdf_font_t *pdfont;
} pdf_text_process_state_t;

/*
 * Declare the procedures for processing different species of text.
 * These procedures may, but need not, copy pte->text into buf
 * (a caller-supplied buffer large enough to hold the string).
 */
#define PROCESS_TEXT_PROC(proc)\
  int proc(P4(gs_text_enum_t *pte, const void *vdata, void *vbuf, uint size))
private PROCESS_TEXT_PROC(process_plain_text);
private PROCESS_TEXT_PROC(process_composite_text);
private PROCESS_TEXT_PROC(process_cmap_text);
private PROCESS_TEXT_PROC(process_cid_text);

/* Other forward declarations */
private int pdf_process_string(P6(pdf_text_enum_t *penum, gs_string *pstr,
				  const gs_matrix *pfmat, bool encoded,
				  pdf_text_process_state_t *pts, int *pindex));
private int pdf_update_text_state(P3(pdf_text_process_state_t *ppts,
				     const pdf_text_enum_t *penum,
				     const gs_matrix *pfmat));
private int pdf_encode_char(P4(gx_device_pdf *pdev, int chr,
			       gs_font_base *bfont, pdf_font_t *ppf));
private int pdf_encode_glyph(P5(gx_device_pdf *pdev, int chr, gs_glyph glyph,
				gs_font_base *bfont, pdf_font_t *ppf));
private int pdf_write_text_process_state(P4(gx_device_pdf *pdev,
			const gs_text_enum_t *pte,	/* for pdcolor, pis */
			const pdf_text_process_state_t *ppts,
			const gs_const_string *pstr));
private int process_text_return_width(P7(const gs_text_enum_t *pte,
					 gs_font *font, pdf_font_t *pdfont,
					 const gs_matrix *pfmat,
					 const gs_const_string *pstr,
					 int *pindex, gs_point *pdpt));
private int process_text_add_width(P7(gs_text_enum_t *pte,
				      gs_font *font, const gs_matrix *pfmat,
				      const pdf_text_process_state_t *ppts,
				      const gs_const_string *pstr,
				      int *pindex, gs_point *pdpt));

/*
 * Continue processing text.  This is the 'process' procedure in the text
 * enumerator.
 */
private int
pdf_text_process(gs_text_enum_t *pte)
{
    pdf_text_enum_t *const penum = (pdf_text_enum_t *)pte;
    uint operation = pte->text.operation;
    const void *vdata;
    uint size = pte->text.size - pte->index;
    gs_text_enum_t *pte_default = penum->pte_default;
    PROCESS_TEXT_PROC((*process));
    int code;
#define BUF_SIZE 100		/* arbitrary > 0 */

    /*
     * If we fell back to the default implementation, continue using it.
     */
 top:
    if (pte_default) {
	code = gs_text_process(pte_default);
	gs_text_enum_copy_dynamic(pte, pte_default, true);
	if (code)
	    return code;
	gs_text_release(pte_default, "pdf_text_process");
	penum->pte_default = 0;
	return 0;
    }
    {
	gs_font *font = pte->current_font;

	switch (font->FontType) {
	case ft_CID_encrypted:
	case ft_CID_TrueType:
	    process = process_cid_text;
	    break;
	case ft_encrypted:
	case ft_encrypted2:
	case ft_TrueType:
	    /* The data may be either glyphs or characters. */
	    process = process_plain_text;
	    break;
	case ft_composite:
	    process =
		(((gs_font_type0 *)font)->data.FMapType == fmap_CMap ?
		 process_cmap_text :
		 process_composite_text);
	    break;
	default:
	    return_error(gs_error_rangecheck);
	}
    }

    /*
     * We want to process the entire string in a single call, but we may
     * need to modify it.  Copy it to a buffer.  Note that it may consist
     * of bytes, gs_chars, or gs_glyphs.
     */

    if (operation & (TEXT_FROM_STRING | TEXT_FROM_BYTES))
	vdata = pte->text.data.bytes;
    else if (operation & TEXT_FROM_CHARS)
	vdata = pte->text.data.chars, size *= sizeof(gs_char);
    else if (operation & TEXT_FROM_SINGLE_CHAR)
	vdata = &pte->text.data.d_char, size = sizeof(gs_char);
    else if (operation & TEXT_FROM_GLYPHS)
	vdata = pte->text.data.glyphs, size *= sizeof(gs_glyph);
    else if (operation & TEXT_FROM_SINGLE_GLYPH)
	vdata = &pte->text.data.d_glyph, size = sizeof(gs_glyph);
    else
	return_error(gs_error_rangecheck);

    if (size <= BUF_SIZE) {
	/* Use a union to ensure alignment. */
	union bu_ {
	    byte bytes[BUF_SIZE];
	    gs_char chars[BUF_SIZE / sizeof(gs_char)];
	    gs_glyph glyphs[BUF_SIZE / sizeof(gs_glyph)];
	} buf;

	code = process(pte, vdata, buf.bytes, size);
    } else {
	byte *buf = gs_alloc_string(pte->memory, size, "pdf_text_process");

	if (buf == 0)
	    return_error(gs_error_VMerror);
	code = process(pte, vdata, buf, size);
	gs_free_string(pte->memory, buf, size, "pdf_text_process");
    }

    if (code < 0) {
	/* Fall back to the default implementation. */
	code = gx_default_text_begin(pte->dev, pte->pis, &pte->text,
				     pte->current_font,
				     pte->path, pte->pdcolor, pte->pcpath,
				     pte->memory, &pte_default);
	if (code < 0)
	    return code;
	penum->pte_default = pte_default;
	gs_text_enum_copy_dynamic(pte_default, pte, false);
	goto top;
    }
    return code;
}

/*
 * Process a text string in an ordinary font.
 */
private int
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
				       text_state.pdfont);

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
	if (code >= 0)
	    code = TEXT_PROCESS_INTERVENE;
    } else {
	str.size = size;
	code = pdf_process_string(penum, &str, NULL, encoded, &text_state,
				  &index);
    }
    pte->index += index;
    return code;
}

/*
 * Process a text string in a composite font with FMapType != 9 (CMap).
 */
private int
process_composite_text(gs_text_enum_t *pte, const void *vdata, void *vbuf,
		       uint size)
{
    byte *const buf = vbuf;
    pdf_text_enum_t *const penum = (pdf_text_enum_t *)pte;
    int index = 0, code;
    gs_string str;
    pdf_text_process_state_t text_state;
    pdf_text_enum_t curr, prev;
    gs_point total_width;

    str.data = buf;
    if (pte->text.operation &
	(TEXT_FROM_ANY - (TEXT_FROM_STRING | TEXT_FROM_BYTES))
	)
	return_error(gs_error_rangecheck);
    if (pte->text.operation & TEXT_INTERVENE) {
	/* Not implemented. (PostScript doesn't even allow this case.) */
	return_error(gs_error_rangecheck);
    }
    total_width.x = total_width.y = 0;
    curr = *penum;
    prev = curr;
    prev.current_font = 0;
    /* Scan runs of characters in the same leaf font. */
    for ( ; ; ) {
	int font_code, buf_index;
	gs_font *new_font;

	for (buf_index = 0; ; ++buf_index) {
	    gs_char chr;
	    gs_glyph glyph;

	    gs_text_enum_copy_dynamic((gs_text_enum_t *)&prev,
				      (gs_text_enum_t *)&curr, false);
	    font_code = pte->orig_font->procs.next_char_glyph
		((gs_text_enum_t *)&curr, &chr, &glyph);
	    /*
	     * We check for a font change by comparing the current
	     * font, rather than testing the return code, because
	     * it makes the control structure a little simpler.
	     */
	    switch (font_code) {
	    case 0:		/* no font change */
	    case 1:		/* font change */
		new_font = curr.fstack.items[curr.fstack.depth].font;
		if (new_font != prev.current_font)
		    break;
		if (chr != (byte)chr)	/* probably can't happen */
		    return_error(gs_error_rangecheck);
		buf[buf_index] = (byte)chr;
		continue;
	    case 2:		/* end of string */
		break;
	    default:	/* error */
		return font_code;
	    }
	    break;
	}
	str.size = buf_index;
	gs_text_enum_copy_dynamic((gs_text_enum_t *)&curr,
				  (gs_text_enum_t *)&prev, false);
	if (buf_index) {
	    /* buf_index == 0 is only possible the very first time. */
	    /*
	     * The FontMatrix of leaf descendant fonts is not updated
	     * by scalefont.  Compute the effective FontMatrix now.
	     */
	    const gs_matrix *psmat =
		&curr.fstack.items[curr.fstack.depth - 1].font->FontMatrix;
	    gs_matrix fmat;

	    gs_matrix_multiply(&curr.current_font->FontMatrix, psmat,
			       &fmat);
	    code = pdf_process_string(&curr, &str, &fmat, false, &text_state,
				      &index);
	    if (code < 0)
		return code;
	    gs_text_enum_copy_dynamic(pte, (gs_text_enum_t *)&curr, true);
	    if (pte->text.operation & TEXT_RETURN_WIDTH) {
		pte->returned.total_width.x = total_width.x +=
		    curr.returned.total_width.x;
		pte->returned.total_width.y = total_width.y +=
		    curr.returned.total_width.y;
	    }
	}
	if (font_code == 2)
	    break;
	curr.current_font = new_font;
    }
    return code;
}

/*
 * Process a text string in a composite font with FMapType == 9 (CMap).
 */
private int
process_cmap_text(gs_text_enum_t *pte, const void *vdata, void *vbuf,
		  uint size)
{
    /****** NOT IMPLEMENTED YET ******/
    return_error(gs_error_rangecheck);
}

/*
 * Process a text string in a CIDFont.
 */
private int
process_cid_text(gs_text_enum_t *pte, const void *vdata, void *vbuf,
		 uint size)
{
    /****** NOT IMPLEMENTED YET ******/
    return_error(gs_error_rangecheck);
}

/*
 * Internal procedure to process a string in a non-composite font.
 * Doesn't use or set pte->{data,size,index}; may use/set pte->xy_index;
 * may set pte->returned.total_width.
 */
private int
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
	pdf_font_t *pdfont = pts->pdfont;
	int code =
	    (encoded ? chr :
	     pdf_encode_char(pdev, chr, (gs_font_base *)font, pdfont));

	if (code < 0)
	    return code;
	/*
	 * For incrementally loaded fonts, expand FirstChar..LastChar
	 * if needed.
	 */
	if (code < pdfont->FirstChar)
	    pdfont->FirstChar = code;
	if (code > pdfont->LastChar)
	    pdfont->LastChar = code;
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

	    if (text->operation & TEXT_DO_DRAW) {
		code = pdf_append_chars(pdev, pstr->data + i, 1);
		if (code < 0)
		    return code;
	    }
	    gs_text_replaced_width(&pte->text, pte->xy_index, &d);
	    w.x += d.x, w.y += d.y;
	    gs_distance_transform(d.x, d.y, &ctm_only(pte->pis), &dpt);
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

    /* Write out the characters, unless we have to emulate ADD_TO_WIDTH. */
    if (mask == 0 && (text->operation & TEXT_DO_DRAW)) {
	code = pdf_append_chars(pdev, pstr->data, pstr->size);
	if (code < 0)
	    return code;
    }

    /*
     * If we don't need the total width, and don't have to emulate
     * ADD_TO_WIDTH, return now.  If the widths are available directly from
     * the font, place the characters and/or compute and return the total
     * width now.  Otherwise, call the default implementation.
     */
    {
	gs_point dpt;

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
					  pindex, &dpt);
	    if (code < 0)
		return code;
	    if (!(text->operation & TEXT_RETURN_WIDTH))
		return 0;
	} else if (!(text->operation & TEXT_RETURN_WIDTH))
	    return 0;
	else {
	    code = process_text_return_width(pte, font, pts->pdfont, pfmat,
					     (gs_const_string *)pstr,
					     pindex, &dpt);
	    if (code < 0)
		return code;
	}
	pte->returned.total_width = dpt;
	gs_distance_transform(dpt.x, dpt.y, &ctm_only(pte->pis), &dpt);
	return gx_path_add_point(pte->path,
				 penum->origin.x + float2fixed(dpt.x),
				 penum->origin.y + float2fixed(dpt.y));
    }
}

/*
 * Compute the cached values in the text processing state from the text
 * parameters, current_font, and pis->ctm.  Return either an error code (<
 * 0) or a mask of operation attributes that the caller must emulate.
 * Currently the only such attributes are TEXT_ADD_TO_ALL_WIDTHS and
 * TEXT_ADD_TO_SPACE_WIDTH.
 */
private int
transform_delta_inverse(const gs_point *pdelta, const gs_matrix *pmat,
			gs_point *ppt)
{
    int code = gs_distance_transform_inverse(pdelta->x, pdelta->y, pmat, ppt);
    gs_point delta;

    if (code < 0)
	return code;
    if (ppt->y == 0)
	return 0;
    /* Check for numerical fuzz. */
    code = gs_distance_transform(ppt->x, 0.0, pmat, &delta);
    if (code < 0)
	return 0;		/* punt */
    if (fabs(delta.x - pdelta->x) < 0.01 && fabs(delta.y - pdelta->y) < 0.01) {
	/* Close enough to y == 0: device space error < 0.01 pixel. */
	ppt->y = 0;
    }
    return 0;
}
private int
pdf_update_text_state(pdf_text_process_state_t *ppts,
		      const pdf_text_enum_t *penum, const gs_matrix *pfmat)
{
    gx_device_pdf *const pdev = (gx_device_pdf *)penum->dev;
    gs_font *font = penum->current_font;
    pdf_font_t *ppf;
    gs_fixed_point cpt;
    gs_matrix orig_matrix, smat, tmat;
    double
	sx = pdev->HWResolution[0] / 72.0,
	sy = pdev->HWResolution[1] / 72.0;
    float chars = 0, words = 0, size;
    int mask = 0;
    int code = gx_path_current_point(penum->path, &cpt);

    if (code < 0)
	return code;

    /* PDF always uses 1000 units per em for font metrics. */
    switch (font->FontType) {
    case ft_TrueType:
    case ft_CID_TrueType:
	/* The TrueType FontMatrix is 1 unit per em, which is what we want. */
	gs_make_identity(&orig_matrix);
	break;
    case ft_encrypted:
    case ft_encrypted2:
    case ft_CID_encrypted:
	gs_make_scaling(0.001, 0.001, &orig_matrix);
	break;
    default:
	return_error(gs_error_rangecheck);
    }
    DISCARD(pdf_find_orig_font(pdev, font, &orig_matrix));

    /* Compute the scaling matrix and combined matrix. */

    gs_matrix_invert(&orig_matrix, &smat);
    gs_matrix_multiply(&smat, pfmat, &smat);
    tmat = ctm_only(penum->pis);
    tmat.tx = tmat.ty = 0;
    gs_matrix_multiply(&smat, &tmat, &tmat);

    /* Try to find a reasonable size value.  This isn't necessary, */
    /* but it's worth a little effort. */

    size = fabs(tmat.yy) / sy;
    if (size < 0.01)
	size = fabs(tmat.xx) / sx;
    if (size < 0.01)
	size = 1;

    /* Check for spacing parameters we can handle, and transform them. */

    if (penum->text.operation & TEXT_ADD_TO_ALL_WIDTHS) {
	gs_point pt;

	code = transform_delta_inverse(&penum->text.delta_all, &smat, &pt);
	if (code >= 0 && pt.y == 0)
	    chars = pt.x * size;
	else
	    mask |= TEXT_ADD_TO_ALL_WIDTHS;
    }

    if (penum->text.operation & TEXT_ADD_TO_SPACE_WIDTH) {
	gs_point pt;

	code = transform_delta_inverse(&penum->text.delta_space, &smat, &pt);
	if (code >= 0 && pt.y == 0 && penum->text.space.s_char == 32)
	    words = pt.x * size;
	else
	    mask |= TEXT_ADD_TO_SPACE_WIDTH;
    }

    /* Find or create the font resource. */

    ppf = (pdf_font_t *)
	pdf_find_resource_by_gs_id(pdev, resourceFont, font->id);
    /* Check for the possibility that the base font has been freed. */
    if (ppf && ppf->FontDescriptor->written)
	ppf = 0;
    if (ppf == 0 || ppf->font == 0) {
	code = pdf_create_pdf_font(pdev, font, &orig_matrix, &ppf);
	if (code < 0)
	    return code;
    }

    /* Store the updated values. */

    tmat.xx /= size;
    tmat.xy /= size;
    tmat.yx /= size;
    tmat.yy /= size;
    tmat.tx += fixed2float(cpt.x);
    tmat.ty += fixed2float(cpt.y);

    ppts->chars = chars;
    ppts->words = words;
    ppts->size = size;
    ppts->text_matrix = tmat;
    ppts->mode = (font->PaintType == 0 ? 0 : 1);
    ppts->font = font;
    ppts->pdfont = ppf;

    return mask;
}

/*
 * For a given character, check whether the encoding of bfont (the current
 * font) is compatible with that of the underlying unscaled, possibly
 * standard, base font, and if not, whether we can re-encode the character
 * using the base font's encoding.  Return the (possibly re-encoded)
 * character if successful.
 */
inline private void
record_used(pdf_font_descriptor_t *pfd, int c)
{
    pfd->chars_used.data[c >> 3] |= 1 << (c & 7);
}
#define BASE_ENCODING(ppf)\
  (ppf->BaseEncoding != ENCODING_INDEX_UNKNOWN ? ppf->BaseEncoding :\
   ppf->index >= 0 ? pdf_standard_fonts[ppf->index].base_encoding :\
   ENCODING_INDEX_UNKNOWN)

private int
pdf_encode_char(gx_device_pdf *pdev, int chr, gs_font_base *bfont,
		pdf_font_t *ppf)
{
    pdf_font_descriptor_t *const pfd = ppf->FontDescriptor;
    /*
     * bfont is the current font in which the text is being shown.
     * ei is its encoding_index.
     */
    gs_encoding_index_t ei = bfont->encoding_index;
    /*
     * base_font is the font that underlies this PDF font (i.e., this PDF
     * font is base_font plus some possible Encoding and Widths differences,
     * and possibly a different FontMatrix).  base_font is 0 iff this PDF
     * font is one of the standard 14 (i.e., ppf->index >= 0).  bei is the
     * index of the BaseEncoding (explicit or, for the standard fonts,
     * implicit) that will be written in the PDF file: it is not necessarily
     * the same as base_font->encoding_index, or even
     * base_font->nearest_encoding_index.
     */
    gs_font *base_font = pfd->base_font;
    bool have_font = base_font != 0 && base_font->FontType != ft_composite;
    bool is_standard = ppf->index >= 0;
    gs_encoding_index_t bei = BASE_ENCODING(ppf);
    pdf_encoding_element_t *pdiff = ppf->Differences;
    /*
     * If set, font_glyph is the glyph currently associated with chr in
     * base_font + bei + diffs; glyph is the glyph corresponding to chr in
     * bfont.
     */
    gs_glyph font_glyph, glyph;
#define IS_USED(c)\
  (((pfd)->chars_used.data[(c) >> 3] & (1 << ((c) & 7))) != 0)

    if (ei == bei && ei != ENCODING_INDEX_UNKNOWN && pdiff == 0) {
	/*
	 * Just note that the character has been used with its original
	 * encoding.
	 */
	record_used(pfd, chr);
	return chr;
    }
    if (!is_standard && !have_font)
	return_error(gs_error_undefined); /* can't encode */

#define ENCODE_NO_DIFF(ch)\
   (bei != ENCODING_INDEX_UNKNOWN ?\
    bfont->procs.callbacks.known_encode((gs_char)(ch), bei) :\
    /* have_font */ bfont->procs.encode_char(base_font, ch, GLYPH_SPACE_NAME))
#define HAS_DIFF(ch) (pdiff != 0 && pdiff[ch].str.data != 0)
#define ENCODE_DIFF(ch) (pdiff[ch].glyph)
#define ENCODE(ch)\
  (HAS_DIFF(ch) ? ENCODE_DIFF(ch) : ENCODE_NO_DIFF(ch))

    font_glyph = ENCODE(chr);
    glyph =
	(ei == ENCODING_INDEX_UNKNOWN ?
	 bfont->procs.encode_char((gs_font *)bfont, chr, GLYPH_SPACE_NAME) :
	 bfont->procs.callbacks.known_encode(chr, ei));
    if (glyph == font_glyph) {
	record_used(pfd, chr);
	return chr;
    }

    return pdf_encode_glyph(pdev, chr, glyph, bfont, ppf);
}
/*
 * Encode a glyph that doesn't appear in the default place in an encoding.
 * chr will be -1 if we are encoding a glyph for glyphshow.
 */
private int
pdf_encode_glyph(gx_device_pdf *pdev, int chr, gs_glyph glyph,
		 gs_font_base *bfont, pdf_font_t *ppf)
{
    pdf_font_descriptor_t *const pfd = ppf->FontDescriptor;
    pdf_encoding_element_t *pdiff = ppf->Differences;
    gs_font *base_font = pfd->base_font;
    gs_encoding_index_t bei = BASE_ENCODING(ppf);

    if (ppf->index < 0 && pfd->FontFile_id == 0 &&
	pdev->CompatibilityLevel <= 1.2
	) {
	/*
	 * Work around the bug in Acrobat Reader 3's Print function
	 * that makes re-encoded characters in substituted fonts
	 * print as blank.
	 */
	return_error(gs_error_undefined);
    }

    /*
     * TrueType fonts don't allow Differences in the encoding, but we might
     * be able to re-encode the character if it appears elsewhere in the
     * encoding.
     */
    if (bfont->FontType == ft_TrueType) {
	if (pdev->ReEncodeCharacters) {
	    int c;

	    for (c = 0; c < 256; ++c)
		if (ENCODE_NO_DIFF(c) == glyph) {
		    record_used(pfd, c);
		    return c;
		}
	}
	return_error(gs_error_undefined);
    }

    /*
     * If the font isn't going to be embedded, check whether this glyph is
     * available in the base font's glyph set at all.
     */
    if (pfd->FontFile_id == 0) {
	switch (bei) {
	case ENCODING_INDEX_STANDARD:
	case ENCODING_INDEX_ISOLATIN1:
	case ENCODING_INDEX_WINANSI:
	case ENCODING_INDEX_MACROMAN:
	    if (encoding_find_glyph(bfont, glyph, ENCODING_INDEX_ALOGLYPH) < 0
	    /*
	     * One would expect that the standard Latin character set in PDF
	     * 1.3, which was released after PostScript 3, would be the full
	     * 315-character PostScript 3 set.  However, the PDF 1.3
	     * reference manual clearly specifies that the PDF 1.3 Latin
	     * character set is the smaller PostScript Level 2 set,
	     * and we have verified that this is the case in Acrobat 4.
	     * Therefore, we have commented out the second part of the
	     * conditional below.
	     */
#if 0
		&&
		(pdev->CompatibilityLevel < 1.3 ||
		 encoding_find_glyph(bfont, glyph, ENCODING_INDEX_ALXGLYPH) < 0)
#endif
		)
		return_error(gs_error_undefined);
	default:
	    break;
	}
    }

    if (pdev->ReAssignCharacters && chr >= 0) {
	/*
	 * If this is the first time we've seen this character,
	 * assign the glyph to its position in the encoding.
	 */
	if (!HAS_DIFF(chr) && !IS_USED(chr)) {
	    int code =
		pdf_add_encoding_difference(pdev, ppf, chr, bfont, glyph);

	    if (code >= 0) {
		/*
		 * As noted in the comments at the beginning of this file,
		 * we don't have any way to record, for the purpose of
		 * subsetting, the fact that a particular unencoded glyph
		 * was used.  If this glyph doesn't appear anywhere else in
		 * the base encoding, fall back to writing the entire font.
		 */
		int c;

		for (c = 0; c < 256; ++c)
		    if (ENCODE_NO_DIFF(c) == glyph)
			break;
		if (c < 256)	/* found */
		    record_used(pfd, c);
		else {		/* not found */
		    if (pfd->do_subset == FONT_SUBSET_YES)
			return_error(gs_error_undefined);
		    pfd->do_subset = FONT_SUBSET_NO;
		}
		return chr;
	    }
	}
    }

    if (pdev->ReEncodeCharacters || chr < 0) {
	/*
	 * Look for the glyph at some other position in the encoding.
	 */
	int c, code;

	for (c = 0; c < 256; ++c) {
	    if (HAS_DIFF(c)) {
		if (ENCODE_DIFF(c) == glyph)
		    return c;
	    } else if (ENCODE_NO_DIFF(c) == glyph) {
		record_used(pfd, c);
		return c;
	    }
	}
	/*
	 * The glyph isn't encoded anywhere.  Look for a
	 * never-referenced .notdef position where we can put it.
	 */
	for (c = 0; c < 256; ++c) {
	    gs_const_string gnstr;
	    gs_glyph font_glyph;

	    if (HAS_DIFF(c) || IS_USED(c))
		continue; /* slot already referenced */
	    font_glyph = ENCODE_NO_DIFF(c);
	    if (font_glyph == gs_no_glyph)
		break;
	    gnstr.data = (const byte *)
		bfont->procs.callbacks.glyph_name(font_glyph, &gnstr.size);
	    if (gnstr.size == 7 && !memcmp(gnstr.data, ".notdef", 7))
		break;
	}
	if (c == 256)	/* no .notdef positions left */
	    return_error(gs_error_undefined);
	code = pdf_add_encoding_difference(pdev, ppf, c, bfont, glyph);
	if (code < 0)
	    return code;
	/* See under ReAssignCharacters above regarding the following: */
	if (pfd->do_subset == FONT_SUBSET_YES)
	    return_error(gs_error_undefined);
	pfd->do_subset = FONT_SUBSET_NO;
	return c;
    }

    return_error(gs_error_undefined);

#undef IS_USED
#undef ENCODE_NO_DIFF
#undef HAS_DIFF
#undef ENCODE_DIFF
#undef ENCODE
}

/*
 * Write out commands to make the output state match the processing state.
 */
private int
pdf_write_text_process_state(gx_device_pdf *pdev,
			     const gs_text_enum_t *pte,	/* for pdcolor, pis */
			     const pdf_text_process_state_t *ppts,
			     const gs_const_string *pstr)
{
    int code;

    pdf_set_font_and_size(pdev, ppts->pdfont, ppts->size);
    code = pdf_set_text_matrix(pdev, &ppts->text_matrix);
    if (code < 0)
	return code;

    if (pdev->text.character_spacing != ppts->chars &&
	pstr->size + pdev->text.buffer_count > 1
	) {
	code = pdf_open_page(pdev, PDF_IN_TEXT);
	if (code < 0)
	    return code;
	pprintg1(pdev->strm, "%g Tc\n", ppts->chars);
	pdev->text.character_spacing = ppts->chars;
    }

    if (pdev->text.word_spacing != ppts->words &&
	(memchr(pstr->data, 32, pstr->size) ||
	 memchr(pdev->text.buffer, 32, pdev->text.buffer_count))
	) {
	code = pdf_open_page(pdev, PDF_IN_TEXT);
	if (code < 0)
	    return code;
	pprintg1(pdev->strm, "%g Tw\n", ppts->words);
	pdev->text.word_spacing = ppts->words;
    }

    if (pdev->text.render_mode != ppts->mode) {
	code = pdf_open_page(pdev, PDF_IN_TEXT);
	if (code < 0)
	    return code;
	pprintd1(pdev->strm, "%d Tr\n", ppts->mode);
	if (ppts->mode) {
	    /* Also write all the parameters for stroking. */
	    gs_imager_state *pis = pte->pis;
	    float save_width = pis->line_params.half_width;

	    pis->line_params.half_width = ppts->font->StrokeWidth / 2;
	    code = pdf_prepare_stroke(pdev, pis);
	    if (code >= 0) {
		/*
		 * See stream_to_text in gdevpdfu.c re the computation of
		 * the scaling value.
		 */
		double scale = 72.0 / pdev->HWResolution[1];

		code = gdev_vector_prepare_stroke((gx_device_vector *)pdev,
						  pis, NULL, NULL, scale);
	    }
	    pis->line_params.half_width = save_width;
	    if (code < 0)
		return code;
	}
	pdev->text.render_mode = ppts->mode;
    }

    return 0;
}

/* Compute the total text width (in user space). */
private int
process_text_return_width(const gs_text_enum_t *pte, gs_font *font,
			  pdf_font_t *pdfont, const gs_matrix *pfmat,
			  const gs_const_string *pstr,
			  int *pindex, gs_point *pdpt)
{
    int i, w;
    double scale = (font->FontType == ft_TrueType ? 0.001 : 1.0);
    gs_point dpt;
    int num_spaces = 0;
    int space_char =
	(pte->text.operation & TEXT_ADD_TO_SPACE_WIDTH ?
	 pte->text.space.s_char : -1);

    for (i = *pindex, w = 0; i < pstr->size; ++i) {
	int cw;
	int code = pdf_char_width(pdfont, pstr->data[i], font, &cw);

	if (code < 0)
	    return code;
	w += cw;
	if (pstr->data[i] == space_char)
	    ++num_spaces;
    }
    gs_distance_transform(w * scale, 0.0, pfmat, &dpt);
    if (pte->text.operation & TEXT_ADD_TO_ALL_WIDTHS) {
	int num_chars = pstr->size - pte->index;

	dpt.x += pte->text.delta_all.x * num_chars;
	dpt.y += pte->text.delta_all.y * num_chars;
    }
    if (pte->text.operation & TEXT_ADD_TO_SPACE_WIDTH) {
	dpt.x += pte->text.delta_space.x * num_spaces;
	dpt.y += pte->text.delta_space.y * num_spaces;
    }
    *pdpt = dpt;
    return 0;
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
    double scale = (font->FontType == ft_TrueType ? 0.001 : 1.0);
    gs_point dpt;
	gs_matrix tmat;
    int space_char =
	(pte->text.operation & TEXT_ADD_TO_SPACE_WIDTH ?
	 pte->text.space.s_char : -1);
    int code = 0;
    bool move = false;

    dpt.x = dpt.y = 0;
    tmat = ppts->text_matrix;
    for (i = *pindex, w = 0; i < pstr->size; ++i) {
	int cw;
	int code = pdf_char_width(ppts->pdfont, pstr->data[i], font, &cw);
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
	if (pte->text.operation & TEXT_DO_DRAW) {
	    code = pdf_append_chars(pdev, &pstr->data[i], 1);
	    if (code < 0)
		break;
	}
	gs_distance_transform(cw * scale, 0.0, pfmat, &wpt);
	dpt.x += wpt.x, dpt.y += wpt.y;
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


/* ---------------- Text and font utilities ---------------- */

/* Forward declarations */
private int assign_char_code(P1(gx_device_pdf * pdev));

/*
 * Try to find a glyph in a (pseudo-)encoding.  If present, return the
 * index (character code); if absent, return -1.
 */
private int
encoding_find_glyph(gs_font_base *bfont, gs_glyph font_glyph,
		    gs_encoding_index_t index)
{
    int ch;
    gs_glyph glyph;

    for (ch = 0;
	 (glyph = bfont->procs.callbacks.known_encode((gs_char)ch, index)) !=
	     gs_no_glyph;
	 ++ch)
	if (glyph == font_glyph)
	    return ch;
    return -1;
}

/*
 * Set the current font and size, writing a Tf command if needed.
 */
private int
pdf_set_font_and_size(gx_device_pdf * pdev, pdf_font_t * font, floatp size)
{
    if (font != pdev->text.font || size != pdev->text.size) {
	int code = pdf_open_page(pdev, PDF_IN_TEXT);
	stream *s = pdev->strm;

	if (code < 0)
	    return code;
	pprints1(s, "/%s ", font->frname);
	pprintg1(s, "%g Tf\n", size);
	pdev->text.font = font;
	pdev->text.size = size;
    }
    font->used_on_page = true;
    return 0;
}

/*
 * Set the text matrix for writing text.
 * The translation component of the matrix is the text origin.
 * If the non-translation components of the matrix differ from the
 * current ones, write a Tm command; if there is only a Y translation
 * and it matches the leading, set use_leading so the next text string
 * will be written with ' rather than Tj; otherwise, write either a TL
 * command or a Tj command using space pseudo-characters.
 */
private int
set_text_distance(gs_point *pdist, const gs_point *ppt, const gs_matrix *pmat)
{
    double rounded;

    gs_distance_transform_inverse(pmat->tx - ppt->x, pmat->ty - ppt->y,
				  pmat, pdist);
    /* If the distance is very close to integers, round it. */
    if (fabs(pdist->x - (rounded = floor(pdist->x + 0.5))) < 0.0005)
	pdist->x = rounded;
    if (fabs(pdist->y - (rounded = floor(pdist->y + 0.5))) < 0.0005)
	pdist->y = rounded;
    return 0;
}
private int
pdf_set_text_matrix(gx_device_pdf * pdev, const gs_matrix * pmat)
{
    stream *s = pdev->strm;
    double sx = 72.0 / pdev->HWResolution[0],
	sy = 72.0 / pdev->HWResolution[1];
    int code;

    if (pmat->xx == pdev->text.matrix.xx &&
	pmat->xy == pdev->text.matrix.xy &&
	pmat->yx == pdev->text.matrix.yx &&
	pmat->yy == pdev->text.matrix.yy &&
    /*
     * If we aren't already in text context, BT will reset
     * the text matrix.
     */
	(pdev->context == PDF_IN_TEXT || pdev->context == PDF_IN_STRING)
	) {
	/* Use leading, Td or a pseudo-character. */
	gs_point dist;

	set_text_distance(&dist, &pdev->text.current, pmat);
	if (dist.y == 0 && dist.x >= X_SPACE_MIN &&
	    dist.x <= X_SPACE_MAX &&
	    pdev->text.font != 0 &&
	    PDF_FONT_IS_SYNTHESIZED(pdev->text.font)
	    ) {			/* Use a pseudo-character. */
	    int dx = (int)dist.x;
	    int dx_i = dx - X_SPACE_MIN;
	    byte space_char = pdev->text.font->spaces[dx_i];

	    if (space_char == 0) {
		if (pdev->text.font != pdev->open_font)
		    goto not_spaces;
		code = assign_char_code(pdev);
		if (code <= 0)
		    goto not_spaces;
		space_char = pdev->open_font->spaces[dx_i] = (byte)code;
		if (pdev->space_char_ids[dx_i] == 0) {
		    /* Create the space char_proc now. */
		    char spstr[3 + 14 + 1];
		    stream *s;

		    sprintf(spstr, "%d 0 0 0 0 0 d1\n", dx);
		    pdev->space_char_ids[dx_i] = pdf_begin_separate(pdev);
		    s = pdev->strm;
		    pprintd1(s, "<</Length %d>>\nstream\n", strlen(spstr));
		    pprints1(s, "%sendstream\n", spstr);
		    pdf_end_separate(pdev);
		}
	    }
	    pdf_append_chars(pdev, &space_char, 1);
	    pdev->text.current.x += dx * pmat->xx;
	    pdev->text.use_leading = false;
	    return 0;
	}
      not_spaces:
	code = pdf_open_page(pdev, PDF_IN_TEXT);
	if (code < 0)
	    return code;
	if (dist.x == 0 && dist.y < 0) {
	    /* Use TL, if needed, + '. */
	    float dist_y = (float)-dist.y;

	    if (fabs(pdev->text.leading - dist_y) > 0.0005) {
		pprintg1(s, "%g TL\n", dist_y);
		pdev->text.leading = dist_y;
	    }
	    pdev->text.use_leading = true;
	} else {
	    /* Use Td. */
	    set_text_distance(&dist, &pdev->text.line_start, pmat);
	    pprintg2(s, "%g %g Td\n", dist.x, dist.y);
	    pdev->text.use_leading = false;
	}
    } else {			/* Use Tm. */
	code = pdf_open_page(pdev, PDF_IN_TEXT);
	if (code < 0)
	    return code;
	/*
	 * See stream_to_text in gdevpdf.c for why we need the following
	 * matrix adjustments.
	 */
	pprintg6(pdev->strm, "%g %g %g %g %g %g Tm\n",
		 pmat->xx * sx, pmat->xy * sy,
		 pmat->yx * sx, pmat->yy * sy,
		 pmat->tx * sx, pmat->ty * sy);
	pdev->text.matrix = *pmat;
	pdev->text.use_leading = false;
    }
    pdev->text.line_start.x = pmat->tx;
    pdev->text.line_start.y = pmat->ty;
    pdev->text.current.x = pmat->tx;
    pdev->text.current.y = pmat->ty;
    return 0;
}

/* Append characters to a string being accumulated. */
private int
pdf_append_chars(gx_device_pdf * pdev, const byte * str, uint size)
{
    const byte *p = str;
    uint left = size;

    while (left)
	if (pdev->text.buffer_count == max_text_buffer) {
	    int code = pdf_open_page(pdev, PDF_IN_TEXT);

	    if (code < 0)
		return code;
	} else {
	    int code = pdf_open_page(pdev, PDF_IN_STRING);
	    uint copy;

	    if (code < 0)
		return code;
	    copy = min(max_text_buffer - pdev->text.buffer_count, left);
	    memcpy(pdev->text.buffer + pdev->text.buffer_count, p, copy);
	    pdev->text.buffer_count += copy;
	    p += copy;
	    left -= copy;
	}
    return 0;
}

/* ---------------- Synthesized fonts ---------------- */

/* Assign a code for a char_proc. */
private int
assign_char_code(gx_device_pdf * pdev)
{
    pdf_font_t *font = pdev->open_font;

    if (pdev->embedded_encoding_id == 0) {
	long id = pdf_begin_separate(pdev);
	stream *s = pdev->strm;
	int i;

	/*
	 * Even though the PDF reference documentation says that a
	 * BaseEncoding key is required unless the encoding is
	 * "based on the base font's encoding" (and there is no base
	 * font in this case), Acrobat 2.1 gives an error if the
	 * BaseEncoding key is present.
	 */
	pputs(s, "<</Type/Encoding/Differences[0");
	for (i = 0; i < 256; ++i) {
	    if (!(i & 15))
		pputs(s, "\n");
	    pprintd1(s, "/a%d", i);
	}
	pputs(s, "\n] >>\n");
	pdf_end_separate(pdev);
	pdev->embedded_encoding_id = id;
    }
    if (font == 0 || font->num_chars == 256 || !pdev->use_open_font) {
	/* Start a new synthesized font. */
	int code = pdf_alloc_font(pdev, gs_no_id, &font, NULL, NULL);
	char *pc;

	if (code < 0)
	    return code;
	if (pdev->open_font == 0)
	    memset(font->frname, 0, sizeof(font->frname));
	else
	    strcpy(font->frname, pdev->open_font->frname);
	for (pc = font->frname; *pc == 'Z'; ++pc)
	    *pc = '@';
	if ((*pc)++ == 0)
	    *pc = 'A', pc[1] = 0;
	pdev->open_font = font;
	pdev->use_open_font = true;
    }
    return font->num_chars++;
}

/* Begin a CharProc for a synthesized (bitmap) font. */
int
pdf_begin_char_proc(gx_device_pdf * pdev, int w, int h, int x_width,
  int y_offset, gs_id id, pdf_char_proc_t ** ppcp, pdf_stream_position_t * ppos)
{
    pdf_resource_t *pres;
    pdf_char_proc_t *pcp;
    int char_code = assign_char_code(pdev);
    pdf_font_t *font = pdev->open_font;
    int code;

    if (char_code < 0)
	return char_code;
    code = pdf_begin_resource(pdev, resourceCharProc, id, &pres);
    if (code < 0)
	return code;
    pcp = (pdf_char_proc_t *) pres;
    pcp->font = font;
    pcp->char_next = font->char_procs;
    font->char_procs = pcp;
    pcp->char_code = char_code;
    pcp->width = w;
    pcp->height = h;
    pcp->x_width = x_width;
    pcp->y_offset = y_offset;
    font->max_y_offset = max(font->max_y_offset, h + (h >> 2));
    *ppcp = pcp;
    {
	stream *s = pdev->strm;

	/*
	 * The resource file is positionable, so rather than use an
	 * object reference for the length, we'll go back and fill it in
	 * at the end of the definition.  Take 10K as the longest
	 * definition we can handle.
	 */
	pputs(s, "<</Length     >>\nstream\n");
	ppos->start_pos = stell(s);
    }
    return 0;
}

/* End a CharProc. */
int
pdf_end_char_proc(gx_device_pdf * pdev, pdf_stream_position_t * ppos)
{
    stream *s = pdev->strm;
    long start_pos = ppos->start_pos;
    long end_pos = stell(s);
    long length = end_pos - start_pos;

    if (length > 9999)
	return_error(gs_error_limitcheck);
    sseek(s, start_pos - 14);
    pprintd1(s, "%d", length);
    sseek(s, end_pos);
    pputs(s, "endstream\n");
    pdf_end_separate(pdev);
    return 0;
}

/* Put out a reference to an image as a character in a synthesized font. */
int
pdf_do_char_image(gx_device_pdf * pdev, const pdf_char_proc_t * pcp,
		  const gs_matrix * pimat)
{
    pdf_set_font_and_size(pdev, pcp->font, 1.0);
    {
	gs_matrix tmat;

	tmat = *pimat;
	tmat.ty -= pcp->y_offset;
	pdf_set_text_matrix(pdev, &tmat);
    }
    pdf_append_chars(pdev, &pcp->char_code, 1);
    pdev->text.current.x += pcp->x_width * pdev->text.matrix.xx;
    return 0;
}
