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


/* Text handling for PDF-writing driver. */
#include "math_.h"
#include "memory_.h"
#include "string_.h"
#include "gx.h"
#include "gserrors.h"
#include "gsutil.h"		/* for bytes_compare */
#include "gdevpdfx.h"
#include "scommon.h"

/*
 * When a base font is selected, we have many alternatives for how to handle
 * characters whose encoding doesn't match their encoding in the base font's
 * built-in encoding.  If a character's glyph doesn't match the character's
 * glyph in the encoding built up so far, we check if the font has that
 * glyph at all; if not, we fall back to a bitmap.  Otherwise, we use one
 * or both of the following algorithms:
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
 *      3. Finally, fall back to using bitmaps (for the entire string).
 *
 * If it is essential that all strings in the output contain exactly the
 * same character codes as the input, set ReEncodeCharacters to false.  If
 * it is important that strings be searchable, but some non-searchable
 * strings can be tolerated, the defaults are appropriate.  If searchability
 * is not important, set ReAssignCharacters to false.
 */

/*
 * The show pseudo-parameter is currently the way that the PostScript code
 * passes show operations to the PDF writer.  It is a hack!  Its "value"
 * is the string being shown.  The following other parameters must be "set"
 * at the same time:
 *      /showX px
 *      /showY py
 *      /showValues [{cx cy char} {ax ay}] (optional)
 *      /showFontName /fontname
 *      /showMatrix [xx xy yx yy tx ty]
 *      /showEncoding [e0 .. e255] (optional)
 *      /showBaseEncoding [e0 ... e255] (optional)
 *      /showCharStrings << charnames => anything >> (optional)
 * Note that px/y and tx/y are floating point values in device space;
 * cx/y and ax/y are in text space.  The matrix is the concatenation of
 *      FontMatrix
 *      inverse of base FontMatrix
 *      CTM
 * This represents the transformation from a 1-unit-based character space
 * to device space.  The base encoding is StandardEncoding for all fonts
 * except Symbol and ZapfDingbats; the encodings are not passed if the
 * encoding is eq to the base encoding, since in this case no check for
 * whether the string is encodable in the base encoding is needed.
 */

/* Define the 14 standard built-in fonts. */
private const char *const standard_font_names[] =
{
    "Courier", "Courier-Bold", "Courier-Oblique", "Courier-BoldOblique",
"Helvetica", "Helvetica-Bold", "Helvetica-Oblique", "Helvetica-BoldOblique",
    "Symbol",
    "Times-Roman", "Times-Bold", "Times-Italic", "Times-BoldItalic",
    "ZapfDingbats",
    0
};

/* Define StandardEncoding. */
private const char notdef_string[] = ".notdef";

#define notdef notdef_string
private const char *const std_enc_strings[256] =
{
  /* \00x */
    notdef, notdef, notdef, notdef, notdef, notdef, notdef, notdef,
    notdef, notdef, notdef, notdef, notdef, notdef, notdef, notdef,
    notdef, notdef, notdef, notdef, notdef, notdef, notdef, notdef,
    notdef, notdef, notdef, notdef, notdef, notdef, notdef, notdef,
  /* \04x */
    "space", "exclam", "quotedbl", "numbersign",
    "dollar", "percent", "ampersand", "quoteright",
    "parenleft", "parenright", "asterisk", "plus",
    "comma", "hyphen", "period", "slash",
    "zero", "one", "two", "three",
    "four", "five", "six", "seven",
    "eight", "nine", "colon", "semicolon",
    "less", "equal", "greater", "question",
  /* \10x */
    "at", "A", "B", "C", "D", "E", "F", "G",
    "H", "I", "J", "K", "L", "M", "N", "O",
    "P", "Q", "R", "S", "T", "U", "V", "W",
    "X", "Y", "Z", "bracketleft",
    "backslash", "bracketright", "asciicircum", "underscore",
  /* \14x */
    "quoteleft", "a", "b", "c", "d", "e", "f", "g",
    "h", "i", "j", "k", "l", "m", "n", "o",
    "p", "q", "r", "s", "t", "u", "v", "w",
    "x", "y", "z", "braceleft", "bar", "braceright", "asciitilde", notdef,
  /* \20x */
    notdef, notdef, notdef, notdef, notdef, notdef, notdef, notdef,
    notdef, notdef, notdef, notdef, notdef, notdef, notdef, notdef,
    notdef, notdef, notdef, notdef, notdef, notdef, notdef, notdef,
    notdef, notdef, notdef, notdef, notdef, notdef, notdef, notdef,
  /* \24x */
    notdef, "exclamdown", "cent", "sterling",
    "fraction", "yen", "florin", "section",
    "currency", "quotesingle", "quotedblleft", "guillemotleft",
    "guilsinglleft", "guilsinglright", "fi", "fl",
    notdef, "endash", "dagger", "daggerdbl",
    "periodcentered", notdef, "paragraph", "bullet",
    "quotesinglbase", "quotedblbase", "quotedblright", "guillemotright",
    "ellipsis", "perthousand", notdef, "questiondown",
  /* \30x */
    notdef, "grave", "acute", "circumflex",
    "tilde", "macron", "breve", "dotaccent",
    "dieresis", notdef, "ring", "cedilla",
    notdef, "hungarumlaut", "ogonek", "caron",
    "emdash", notdef, notdef, notdef,
    notdef, notdef, notdef, notdef,
    notdef, notdef, notdef, notdef,
    notdef, notdef, notdef, notdef,
  /* \34x", */
    notdef, "AE", notdef, "ordfeminine", notdef, notdef, notdef, notdef,
    "Lslash", "Oslash", "OE", "ordmasculine", notdef, notdef, notdef, notdef,
    notdef, "ae", notdef, notdef, notdef, "dotlessi", notdef, notdef,
    "lslash", "oslash", "oe", "germandbls", notdef, notdef, notdef, notdef
};

#undef notdef

/* Forward references */
private int pdf_set_font_and_size(P3(gx_device_pdf * pdev, pdf_font * font,
				     floatp size));
private int pdf_set_text_matrix(P2(gx_device_pdf * pdev,
				   const gs_matrix * pmat));
private int pdf_append_chars(P3(gx_device_pdf * pdev, const byte * str,
				uint size));
private int pdf_check_encoding(P6(gx_device_pdf * pdev, gs_param_string * pstr,
				  gs_param_list * plist, byte * strbuf,
				  int max_strbuf, pdf_font * ppf));

/* Process a show operation. */
int
pdfshow_process(gx_device_pdf * pdev, gs_param_list * plist,
		const gs_param_string * pts)
{
    gs_param_string str, fnstr;
    gs_param_float_array va;
    float v_cx = 0, v_cy = 0, v_ax = 0, v_ay = 0, v_px, v_py;
    int v_cch = 32;
    gs_param_float_array ma;
    int code;
    pdf_font *ppf;
    stream *s;
    double sx = pdev->HWResolution[0] / 72.0, sy = pdev->HWResolution[1] / 72.0;
    float size;
    byte strbuf[200];

    str = *pts;
    if (str.size == 0)
	return 0;		/* nothing to do */
    /* Find or create the font resource. */
    if ((code = param_read_string(plist, "showFontName", &fnstr)))
	return_error(gs_error_rangecheck);
    {
	int i;


	for (i = 0; i < num_resource_chains; ++i)
	    for (ppf = (pdf_font *) pdev->resources[resourceFont].chains[i];
		 ppf != 0; ppf = ppf->next
		)
		if (!bytes_compare(ppf->fname.chars, ppf->fname.size,
				   fnstr.data, fnstr.size)
		    )
		    goto found;
      found:DO_NOTHING;
    }
    if (ppf == 0) {
	/*
	 * Currently, we only handle the built-in fonts.  By doing
	 * things in this order, we may wind up including some base
	 * fonts that aren't actually needed, but it's unlikely.
	 */
	const char *const *ppfn;

	for (ppfn = standard_font_names; *ppfn; ++ppfn)
	    if (strlen(*ppfn) == fnstr.size &&
		!strncmp(*ppfn, (const char *)fnstr.data, fnstr.size)
		)
		break;
	if (!*ppfn)
	    return_error(gs_error_undefined);
	/*
	 * Allocate the font resource, but don't write it yet,
	 * because we don't know yet whether it will need
	 * a modified Encoding.
	 */
	code = pdf_alloc_resource(pdev, resourceFont, gs_no_id,
				  (pdf_resource **) & ppf);
	if (code < 0)
	    return_error(gs_error_undefined);
	memcpy(ppf->fname.chars, fnstr.data, fnstr.size);
	ppf->fname.size = fnstr.size;
	ppf->frname[0] = 0;	/* not an embedded font */
	memset(ppf->chars_used, 0, sizeof(ppf->chars_used));
	ppf->differences = 0;
	ppf->num_chars = 0;	/* ditto */
	ppf->char_procs = 0;	/* for GC */
    }
    /* Check that all characters can be encoded. */
    code = pdf_check_encoding(pdev, &str, plist, strbuf, sizeof(strbuf), ppf);
    if (code < 0)
	return code;
    /* Read and check the rest of the parameters now. */
    if ((code = param_read_float(plist, "showX", &v_px)) ||
	(code = param_read_float(plist, "showY", &v_py)) ||
	(code = param_read_float_array(plist, "showMatrix", &ma)) ||
	ma.size != 6
	)
	return_error(gs_error_rangecheck);
    switch ((code = param_read_float_array(plist, "showValues", &va))) {
	case 1:
	    break;
	default:
	    return_error(code);
	case 0:
	    {
		const float *vp = va.data;

		switch (va.size) {
		    case 3:
			v_cx = vp[0], v_cy = vp[1], v_cch = (int)vp[2];
			break;
		    case 5:
			v_cx = vp[0], v_cy = vp[1], v_cch = (int)vp[2];
			vp += 3;
		    case 2:
			v_ax = vp[0], v_ay = vp[1];
			break;
		    default:
			return_error(gs_error_rangecheck);
		}
		if (v_cy != 0 || (v_cch != 32 && v_cx != 0) || v_ay != 0)
		    return_error(gs_error_undefined);
	    }
    }
    /* Try to find a reasonable size value.  This isn't necessary, */
    /* but it's worth the effort. */
    size = fabs(ma.data[3]) / sy;
    if (size < 0.01)
	size = fabs(ma.data[0]) / sx;
    if (size < 0.01)
	size = 1;
    /* We attempt to eliminate redundant parameter settings. */
    pdf_set_font_and_size(pdev, ppf, size);
    {
	float chars = v_ax * size;

	if (pdev->text.character_spacing != chars) {
	    code = pdf_open_contents(pdev, pdf_in_text);
	    if (code < 0)
		return code;
	    s = pdev->strm;
	    pprintg1(s, "%g Tc\n", chars);
	    pdev->text.character_spacing = chars;
	}
    }
    {
	float words = v_cx * size;

	if (pdev->text.word_spacing != words) {
	    code = pdf_open_contents(pdev, pdf_in_text);
	    if (code < 0)
		return code;
	    s = pdev->strm;
	    pprintg1(s, "%g Tw\n", words);
	    pdev->text.word_spacing = words;
	}
    }
    {
	gs_matrix tmat;

	tmat.xx = ma.data[0] / size;
	tmat.xy = ma.data[1] / size;
	tmat.yx = ma.data[2] / size;
	tmat.yy = ma.data[3] / size;
	tmat.tx = v_px + ma.data[4];
	tmat.ty = v_py + ma.data[5];
	pdf_set_text_matrix(pdev, &tmat);
    }

    /* If we re-encoded the string, pdf_check_encoding changed */
    /* str.data to strbuf. */
    return pdf_append_chars(pdev, str.data, str.size);
}

/* Create an encoding differences vector for a font. */
private int
pdf_create_differences(gx_device_pdf * pdev, pdf_font * ppf)
{
    gs_const_string *pdiff = ppf->differences;
    int i;

    if (pdiff != 0)
	return 0;
    ppf->diff_id = pdf_obj_ref(pdev);
    pdiff = gs_alloc_struct_array(pdev->pdf_memory, 256, gs_const_string,
				  &st_const_string_element,
				  "differences");
    if (pdiff == 0)
	return_error(gs_error_VMerror);
    for (i = 0; i < 256; ++i)
	pdiff[i].data = 0, pdiff[i].size = 0;
    ppf->differences = pdiff;
    return 0;
}

/*
 * Check whether the encodings are compatible, and if not,
 * whether we can re-encode the string using the base encoding.
 */
private int
pdf_check_encoding(gx_device_pdf * pdev, gs_param_string * pstr,
       gs_param_list * plist, byte * strbuf, int max_strbuf, pdf_font * ppf)
{
    gs_param_string_array ea, bea;
    int code;
    gs_const_string *pdiff = ppf->differences;
    uint i;

    ea.data = bea.data = 0;
    switch ((code = param_read_name_array(plist, "showEncoding", &ea))) {
	default:
	    return code;
	case 0:
	    if (ea.size != 256)
		return_error(gs_error_rangecheck);
	case 1:
	    DO_NOTHING;
    }
    switch ((code = param_read_name_array(plist, "showBaseEncoding", &bea))) {
	default:
	    return code;
	case 0:
	    if (bea.size != 256)
		return_error(gs_error_rangecheck);
	case 1:
	    DO_NOTHING;
    }

    if (ea.data == bea.data && pdiff == 0) {
	/*
	 * Just note the characters that have been used with their
	 * original encodings.
	 */
	for (i = 0; i < pstr->size; ++i) {
	    byte chr = pstr->data[i];

	    ppf->chars_used[chr >> 3] |= 1 << (chr & 7);
	}
	return 0;		/* encodings are the same */
    }
    for (i = 0; i < pstr->size; ++i) {
	byte chr = pstr->data[i];

#define set_encoded(sdata, ssize, ch)\
  (pdiff != 0 && pdiff[ch].data != 0 ?\
   (sdata = pdiff[ch].data, ssize = pdiff[ch].size) :\
   bea.data == 0 ?\
   (sdata = (const byte *)std_enc_strings[ch],\
    ssize = strlen((const char *)sdata)) :\
   (sdata = bea.data[ch].data, ssize = bea.data[ch].size))
	const byte *fedata;
	uint fesize;
	const byte *edata;
	uint esize;

	set_encoded(fedata, fesize, chr);
	if (ea.data == 0)
	    esize = strlen((const char *)
			   (edata = (const byte *)std_enc_strings[chr]));
	else
	    edata = ea.data[chr].data, esize = ea.data[chr].size;
	if (edata == fedata ||
	    (esize == fesize &&
	     !memcmp(edata, fedata, esize))
	    )
	    ppf->chars_used[chr >> 3] |= 1 << (chr & 7);
	else {
	    if (pdev->ReAssignCharacters) {
		/*
		 * If this is the first time we've seen this character,
		 * assign the glyph to its position in the encoding.
		 */
		if ((pdiff == 0 || pdiff[chr].data == 0) &&
		    !(ppf->chars_used[chr >> 3] & (1 << (chr & 7)))
		    ) {
		    int code = pdf_create_differences(pdev, ppf);

		    if (code < 0)
			return code;
		    pdiff = ppf->differences;
		    /*
		     * Since the entry in ea is the string for a name,
		     * or a const C string, we don't need to copy it.
		     */
		    pdiff[chr].data = edata;
		    pdiff[chr].size = esize;
		    continue;
		}
	    }
	    if (pdev->ReEncodeCharacters) {
		/*
		 * Look for the character at some other position in the
		 * encoding.
		 */
		int c;

		for (c = 0; c < 256; ++c) {
		    set_encoded(fedata, fesize, c);
		    if (esize == fesize && !memcmp(edata, fedata, esize))
			break;
		}
		if (c == 256) {
		    /*
		     * The character isn't encoded anywhere.  Look for a
		     * never-referenced .notdef position where we can put
		     * it.
		     */
		    int code;

		    for (c = 0; c < 256; ++c) {
			set_encoded(fedata, fesize, c);
			if (!bytes_compare(fedata, fesize,
					   (const byte *)".notdef", 7) &&
			    !(ppf->chars_used[c >> 3] & (1 << (c & 7)))
			    )
			    break;
		    }
		    if (c == 256)	/* no .notdef positions left */
			return_error(gs_error_undefined);
		    code = pdf_create_differences(pdev, ppf);
		    if (code < 0)
			return code;
		    ppf->differences[c].data = edata;
		    ppf->differences[c].size = esize;
		}
		/*
		 * It really simplifies things if we can buffer
		 * the entire string locally in one piece....
		 */
		if (pstr->data != strbuf) {
		    if (pstr->size > max_strbuf)
			return_error(gs_error_limitcheck);
		    memcpy(strbuf, pstr->data, pstr->size);
		    pstr->data = strbuf;
		}
		strbuf[i] = (byte) c;
		continue;
	    }
	    return_error(gs_error_undefined);
	}
    }
    return 0;
}

/* ---------------- Embedded fonts ---------------- */

/*
 * Set the current font and size, writing a Tf command if needed.
 */
private int
pdf_set_font_and_size(gx_device_pdf * pdev, pdf_font * font, floatp size)
{
    if (font != pdev->text.font || size != pdev->text.size) {
	int code = pdf_open_contents(pdev, pdf_in_text);
	stream *s = pdev->strm;

	if (code < 0)
	    return code;
	if (font->frname[0])
	    pprints1(s, "/%s ", font->frname);
	else
	    pprintld1(s, "/R%ld ", font->id);
	pprintg1(s, "%g Tf\n", size);
	pdev->text.font = font;
	pdev->text.size = size;
    }
    font->used_on_page = true;
    return 0;
}

/* Assign a code for a char_proc. */
private int
assign_char_code(gx_device_pdf * pdev)
{
    pdf_font *font = pdev->open_font;

    if (font == 0) {		/* This is the very first embedded font. */
	/* Create the (canned) Encoding array. */
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
    if (font == 0 || font->num_chars == 256) {	/* Start a new embedded font. */
	pdf_resource *pres;
	int code = pdf_alloc_resource(pdev, resourceFont, gs_no_id, &pres);
	char *pc;

	if (code < 0)
	    return code;
	font = (pdf_font *) pres;
	font->fname.size = 0;
	font->used_on_page = false;
	if (pdev->open_font == 0)
	    memset(font->frname, 0, sizeof(font->frname));
	else
	    strcpy(font->frname, pdev->open_font->frname);
	for (pc = font->frname; *pc == 'Z'; ++pc)
	    *pc = '@';
	if ((*pc)++ == 0)
	    *pc = 'A', pc[1] = 0;
	font->differences = 0;
	font->num_chars = 0;
	font->char_procs = 0;
	font->max_y_offset = 0;
	memset(font->spaces, 0, sizeof(font->spaces));
	pdev->open_font = font;
    }
    return font->num_chars++;
}

/*
 * Set the text matrix for writing text.
 * The translation component of the matrix is the text origin.
 * If the non-translation components of the matrix differ from the
 * current ones, write a Tm command; otherwise, write either a Td command
 * or a Tj command using space pseudo-characters.
 * Do not write a \n after the command.
 */
private int
set_text_distance(gs_point * pdist, const gs_point * ppt, const gs_matrix * pmat)
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
    double sx = 72.0 / pdev->HWResolution[0], sy = 72.0 / pdev->HWResolution[1];
    int code;

    if (pmat->xx == pdev->text.matrix.xx &&
	pmat->xy == pdev->text.matrix.xy &&
	pmat->yx == pdev->text.matrix.yx &&
	pmat->yy == pdev->text.matrix.yy &&
    /*
     * If we aren't already in text context, BT will reset
     * the text matrix.
     */
	(pdev->context == pdf_in_text || pdev->context == pdf_in_string)
	) {			/* Use Td or a pseudo-character. */
	gs_point dist;

	set_text_distance(&dist, &pdev->text.current, pmat);
	if (dist.y == 0 && dist.x >= x_space_min &&
	    dist.x <= x_space_max &&
	    pdev->text.font != 0 &&
	    font_is_embedded(pdev->text.font)
	    ) {			/* Use a pseudo-character. */
	    int dx = (int)dist.x;
	    int dx_i = dx - x_space_min;
	    byte space_char = pdev->text.font->spaces[dx_i];

	    if (space_char == 0) {
		if (pdev->text.font != pdev->open_font)
		    goto td;
		code = assign_char_code(pdev);
		if (code <= 0)
		    goto td;
		space_char =
		    pdev->open_font->spaces[dx_i] =
		    (byte) code;
		if (pdev->space_char_ids[dx_i] == 0) {	/* Create the space char_proc now. */
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
	    pdev->text.current.x += dist.x * pmat->xx;
	    return 0;
	}
      td:			/* Use Td. */
	code = pdf_open_page(pdev, pdf_in_text);
	if (code < 0)
	    return code;
	set_text_distance(&dist, &pdev->text.line_start, pmat);
	pprintg2(s, "%g %g Td", dist.x, dist.y);
    } else {			/* Use Tm. */
	code = pdf_open_page(pdev, pdf_in_text);
	if (code < 0)
	    return code;
	/*
	 * See stream_to_text in gdevpdf.c for why we need the following
	 * matrix adjustments.
	 */
	pprintg6(pdev->strm, "%g %g %g %g %g %g Tm",
		 pmat->xx * sx, pmat->xy * sy,
		 pmat->yx * sx, pmat->yy * sy,
		 pmat->tx * sx, pmat->ty * sy);
	pdev->text.matrix = *pmat;
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
	    int code = pdf_open_contents(pdev, pdf_in_text);

	    if (code < 0)
		return code;
	} else {
	    int code = pdf_open_contents(pdev, pdf_in_string);
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

/* Begin a CharProc for an embedded (bitmap) font. */
int
pdf_begin_char_proc(gx_device_pdf * pdev, int w, int h, int x_width,
  int y_offset, gs_id id, pdf_char_proc ** ppcp, pdf_stream_position * ppos)
{
    pdf_resource *pres;
    pdf_char_proc *pcp;
    int char_code = assign_char_code(pdev);
    pdf_font *font = pdev->open_font;
    int code;

    if (char_code < 0)
	return char_code;
    code = pdf_begin_resource(pdev, resourceCharProc, id, &pres);
    if (code < 0)
	return code;
    pcp = (pdf_char_proc *) pres;
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
pdf_end_char_proc(gx_device_pdf * pdev, pdf_stream_position * ppos)
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

/* Put out a reference to an image as a character in an embedded font. */
int
pdf_do_char_image(gx_device_pdf * pdev, const pdf_char_proc * pcp,
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
