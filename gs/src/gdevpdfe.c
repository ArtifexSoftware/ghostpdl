/* Copyright (C) 2000 Aladdin Enterprises.  All rights reserved.

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
/* Embedded font writing for pdfwrite driver. */
#include "memory_.h"
#include "string_.h"
#include "gx.h"
#include "gserrors.h"
#include "gsmatrix.h"
#include "gxfcid.h"
#include "gxfont.h"
#include "gxfont0.h"
#include "gdevpdfx.h"
#include "gdevpdff.h"
#include "gdevpsf.h"
#include "scommon.h"

/* ---------------- Utilities ---------------- */

/* Begin writing FontFile* data. */
private int
pdf_begin_fontfile(gx_device_pdf *pdev, long *plength_id)
{
    stream *s;

    *plength_id = pdf_obj_ref(pdev);
    s = pdev->strm;
    pputs(s, "<<");
    if (!pdev->binary_ok)
	pputs(s, "/Filter/ASCII85Decode");
    pprintld1(s, "/Length %ld 0 R", *plength_id);
    return 0;
}

/* Finish writing FontFile* data. */
private int
pdf_end_fontfile(gx_device_pdf *pdev, long start, long length_id)
{
    stream *s = pdev->strm;
    long length;

    pputs(s, "\n");
    length = pdf_stell(pdev) - start;
    pputs(s, "endstream\n");
    pdf_end_separate(pdev);
    pdf_open_separate(pdev, length_id);
    pprintld1(pdev->strm, "%ld\n", length);
    pdf_end_separate(pdev);
    return 0;
}

/* ---------------- Individual font types ---------------- */

/* ------ Type 1 family ------ */

/*
 * Acrobat Reader apparently doesn't accept CFF fonts with Type 1
 * CharStrings, so we need to convert them.
 */
#define TYPE2_OPTIONS WRITE_TYPE2_CHARSTRINGS

/* Write the FontFile[3] data for an embedded Type 1, Type 2, or */
/* CIDFontType 0 font. */
private int
pdf_embed_font_as_type1(gx_device_pdf *pdev, gs_font_type1 *font,
			long FontFile_id, gs_glyph subset_glyphs[256],
			uint subset_size, const gs_const_string *pfname)
{
    stream poss;
    int lengths[3];
    int code;
    long length_id;
    long start;
    psdf_binary_writer writer;

    swrite_position_only(&poss);
    /*
     * We omit the 512 zeros and the cleartomark, and set Length3 to 0.
     * Note that the interpreter adds them implicitly (per documentation),
     * so we must set MARK so that the encrypted portion pushes a mark on
     * the stack.
     */
#define TYPE1_OPTIONS 0/*(WRITE_TYPE1_EEXEC | WRITE_TYPE1_EEXEC_MARK)*/
    code = psf_write_type1_font(&poss, font, TYPE1_OPTIONS,
				 subset_glyphs, subset_size, pfname, lengths);
    if (code < 0)
	return code;
    pdf_open_separate(pdev, FontFile_id);
    pdf_begin_fontfile(pdev, &length_id);
    pprintd2(pdev->strm, "/Length1 %d/Length2 %d/Length3 0>>stream\n",
	     lengths[0], lengths[1]);
    start = pdf_stell(pdev);
    code = psdf_begin_binary((gx_device_psdf *)pdev, &writer);
    if (code < 0)
	return code;
#ifdef DEBUG
    {
	int check_lengths[3];

	psf_write_type1_font(writer.strm, font, TYPE1_OPTIONS,
			      subset_glyphs, subset_size, pfname,
			      check_lengths);
	if (writer.strm == pdev->strm &&
	    (check_lengths[0] != lengths[0] ||
	     check_lengths[1] != lengths[1] ||
	     check_lengths[2] != lengths[2])
	    ) {
	    lprintf7("Type 1 font id %ld, lengths mismatch: (%d,%d,%d) != (%d,%d,%d)\n",
		     ((gs_font *)font)->id, lengths[0], lengths[1], lengths[2],
		     check_lengths[0], check_lengths[1], check_lengths[2]);
	}
    }
#else
    psf_write_type1_font(writer.strm, font, TYPE1_OPTIONS,
			  subset_glyphs, subset_size, pfname,
			  lengths /*ignored*/);
#endif
#undef TYPE1_OPTIONS
    psdf_end_binary(&writer);
    pdf_end_fontfile(pdev, start, length_id);
    return 0;
}

/* Embed a font as Type 2. */
private int
pdf_embed_font_as_type2(gx_device_pdf *pdev, gs_font_type1 *font,
			long FontFile_id, gs_glyph subset_glyphs[256],
			uint subset_size, const gs_const_string *pfname)
{
    stream poss;
    int code;
    long length_id;
    long start;
    psdf_binary_writer writer;
    int options = TYPE2_OPTIONS |
	(pdev->CompatibilityLevel < 1.3 ? WRITE_TYPE2_AR3 : 0);

    swrite_position_only(&poss);
    code = psf_write_type2_font(&poss, font, options,
				 subset_glyphs, subset_size, pfname);
    if (code < 0)
	return code;
    pdf_open_separate(pdev, FontFile_id);
    pdf_begin_fontfile(pdev, &length_id);
    pprintld1(pdev->strm, "/Length1 %ld/Subtype/Type1C>>stream\n",
	      (long)stell(&poss));
    start = pdf_stell(pdev);
    code = psdf_begin_binary((gx_device_psdf *)pdev, &writer);
    if (code < 0)
	return code;
    code = psf_write_type2_font(writer.strm, font, options,
				 subset_glyphs, subset_size, pfname);
    psdf_end_binary(&writer);
    pdf_end_fontfile(pdev, start, length_id);
    return 0;
}

/* Embed a Type 1 or Type 2 font. */
private int
pdf_embed_font_type1(gx_device_pdf *pdev, gs_font_type1 *font,
		     long FontFile_id, gs_glyph subset_glyphs[256],
		     uint subset_size, const gs_const_string *pfname)
{
    switch (((const gs_font *)font)->FontType) {
    case ft_encrypted:
	if (pdev->CompatibilityLevel < 1.2)
	    return pdf_embed_font_as_type1(pdev, font, FontFile_id,
					   subset_glyphs, subset_size, pfname);
	/* For PDF 1.2 and later, write Type 1 fonts as Type1C. */
    case ft_encrypted2:
	return pdf_embed_font_as_type2(pdev, font, FontFile_id,
				       subset_glyphs, subset_size, pfname);
    default:
	return_error(gs_error_rangecheck);
    }
}

/* Embed a CIDFontType0 font. */
private int
pdf_embed_font_cid0(gx_device_pdf *pdev, gs_font_cid0 *font,
		    long FontFile_id, const byte *subset_cids,
		    uint subset_size, const gs_const_string *pfname)
{
    stream poss;
    int code;
    long length_id;
    long start;
    psdf_binary_writer writer;

    if (pdev->CompatibilityLevel < 1.2)
	return_error(gs_error_rangecheck);
    /*
     * It would be wonderful if we could avoid duplicating nearly all of
     * pdf_embed_font_as_type2, but we don't see how to do it.
     */
    swrite_position_only(&poss);
    code = psf_write_cid0_font(&poss, font, TYPE2_OPTIONS,
			       subset_cids, subset_size, pfname);
    if (code < 0)
	return code;
    pdf_open_separate(pdev, FontFile_id);
    pdf_begin_fontfile(pdev, &length_id);
    pprintld1(pdev->strm, "/Length1 %ld/Subtype/CIDFontType0C>>stream\n",
	      (long)stell(&poss));
    start = pdf_stell(pdev);
    code = psdf_begin_binary((gx_device_psdf *)pdev, &writer);
    if (code < 0)
	return code;
    code = psf_write_cid0_font(writer.strm, font, TYPE2_OPTIONS,
			       subset_cids, subset_size, pfname);
    psdf_end_binary(&writer);
    pdf_end_fontfile(pdev, start, length_id);
    return 0;
}

/* ------ TrueType family ------ */

/* Write the FontFile2 data for an embedded TrueType or CIDFontType 2 font. */
private int
pdf_embed_TrueType(gx_device_pdf *pdev, gs_font_type42 *font,
		   long FontFile_id, gs_glyph subset_glyphs[256],
		   uint subset_size, const gs_const_string *pfname,
		   int options)
{
    stream poss;
    int length;
    int code;
    long length_id;
    long start;
    psdf_binary_writer writer;

    swrite_position_only(&poss);
    code = psf_write_truetype_font(&poss, font, options,
				    subset_glyphs, subset_size, pfname);
    if (code < 0)
	return code;
    length = stell(&poss);
    pdf_open_separate(pdev, FontFile_id);
    pdf_begin_fontfile(pdev, &length_id);
    pprintd1(pdev->strm, "/Length1 %d>>stream\n", length);
    start = pdf_stell(pdev);
    code = psdf_begin_binary((gx_device_psdf *)pdev, &writer);
    if (code < 0)
	return code;
    psf_write_truetype_font(writer.strm, font, options,
			     subset_glyphs, subset_size, pfname);
    psdf_end_binary(&writer);
    pdf_end_fontfile(pdev, start, length_id);
    return 0;
}

/* Embed a TrueType font. */
private int
pdf_embed_font_type42(gx_device_pdf *pdev, gs_font_type42 *font,
		      long FontFile_id, gs_glyph subset_glyphs[256],
		      uint subset_size, const gs_const_string *pfname)
{
    /* Acrobat Reader 3 doesn't handle cmap format 6 correctly. */
    const int options = WRITE_TRUETYPE_CMAP | WRITE_TRUETYPE_NAME |
	(pdev->CompatibilityLevel <= 1.2 ?
	 WRITE_TRUETYPE_NO_TRIMMED_TABLE : 0);

    return pdf_embed_TrueType(pdev, font, FontFile_id, subset_glyphs,
			      subset_size, pfname, options);
}

/* Embed a CIDFontType2 font. */
private int
pdf_embed_font_cid2(gx_device_pdf *pdev, gs_font_cid2 *font,
		    long FontFile_id, gs_glyph subset_glyphs[256],
		    uint subset_size, const gs_const_string *pfname)
{
    /* CIDFontType 2 fonts don't use cmap, name, OS/2, or post. */

    return pdf_embed_TrueType(pdev, (gs_font_type42 *)font, FontFile_id,
			      subset_glyphs, subset_size, pfname, 0);
}

/* ---------------- Entry point ---------------- */

/*
 * Write the FontDescriptor and FontFile* data for an embedded font.
 * Return a rangecheck error if the font can't be embedded.
 */
int
pdf_write_embedded_font(gx_device_pdf *pdev, pdf_font_descriptor_t *pfd)
{
    gs_font *font = pfd->base_font;
    gs_const_string font_name;
    byte *fnchars = pfd->FontName.chars;
    uint fnsize = pfd->FontName.size;
    bool do_subset = pfd->subset_ok && pdev->params.SubsetFonts &&
	pdev->params.MaxSubsetPct > 0;
    long FontFile_id = pfd->FontFile_id;
    gs_glyph subset_glyphs[256];
    gs_glyph *glyph_subset = 0;
    uint subset_size = 0;
    gs_matrix save_mat;
    int code;

    /* Determine whether to subset the font. */
    if (do_subset) {
	int used, i, total, index;
	gs_glyph ignore_glyph;

	for (i = 0, used = 0; i < pfd->chars_used.size; ++i)
	    used += byte_count_bits[pfd->chars_used.data[i]];
	for (index = 0, total = 0;
	     (font->procs.enumerate_glyph(font, &index, GLYPH_SPACE_INDEX,
					  &ignore_glyph), index != 0);
	     )
	    ++total;
	if ((double)used / total > pdev->params.MaxSubsetPct / 100.0)
	    do_subset = false;
	else {
	    subset_size = psf_subset_glyphs(subset_glyphs, font,
					    pfd->chars_used.data);
	    glyph_subset = subset_glyphs;
	}
    }

    /* Generate an appropriate font name. */
    if (pdf_has_subset_prefix(fnchars, fnsize)) {
	/* Strip off any existing subset prefix. */
	fnsize -= SUBSET_PREFIX_SIZE;
	memmove(fnchars, fnchars + SUBSET_PREFIX_SIZE, fnsize);
    }
    if (do_subset) {
	memmove(fnchars + SUBSET_PREFIX_SIZE, fnchars, fnsize);
	pdf_make_subset_prefix(fnchars, FontFile_id);
	fnsize += SUBSET_PREFIX_SIZE;
    }
    font_name.data = fnchars;
    font_name.size = pfd->FontName.size = fnsize;
    code = pdf_write_FontDescriptor(pdev, pfd);
    if (code >= 0) {
	pfd->written = true;

	/*
	 * Finally, write the font (or subset), using the original
	 * (unscaled) FontMatrix.
	 */
	save_mat = font->FontMatrix;
	font->FontMatrix = pfd->orig_matrix;
	switch (font->FontType) {
	case ft_composite:
	    /* Nothing to embed -- the descendant fonts do it all. */
	    break;
	case ft_encrypted:
	case ft_encrypted2:
	    code = pdf_embed_font_type1(pdev, (gs_font_type1 *)font,
					FontFile_id, glyph_subset,
					subset_size, &font_name);
	    break;
	case ft_CID_encrypted:
	    code = pdf_embed_font_cid0(pdev, (gs_font_cid0 *)font,
				       FontFile_id, pfd->chars_used.data,
				       pfd->chars_used.size, &font_name);
	    break;
	case ft_CID_TrueType:
	    /****** WRONG, SUBSET IS DEFINED BY BIT VECTOR ******/
	    code = pdf_embed_font_cid2(pdev, (gs_font_cid2 *)font,
				       FontFile_id, glyph_subset,
				       subset_size, &font_name);
	    break;
	case ft_TrueType:
	    code = pdf_embed_font_type42(pdev, (gs_font_type42 *)font,
					 FontFile_id, glyph_subset,
					 subset_size, &font_name);
	    break;
	default:
	    code = gs_note_error(gs_error_rangecheck);
	}
	font->FontMatrix = save_mat;
    }
    return code;
}
