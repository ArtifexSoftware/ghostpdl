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
/* BaseFont implementation for pdfwrite */
#include "memory_.h"
#include <stdlib.h>		/* for rand() */
#include "gx.h"
#include "gserrors.h"
#include "gsutil.h"		/* for bytes_compare */
#include "gxfcid.h"
#include "gxfcopy.h"
#include "gxfont.h"		/* for gxfont42.h */
#include "gxfont42.h"
#include "gdevpsf.h"
#include "gdevpdfx.h"
#include "gdevpdtb.h"

/*
 * Adobe's Distiller Parameters documentation for Acrobat Distiller 5
 * says that all fonts other than Type 1 are always subsetted; the
 * documentation for Distiller 4 says that fonts other than Type 1 and
 * TrueType are subsetted.  We do the latter, except that we always
 * subset large TrueType fonts.
 */
#define MAX_NO_SUBSET_GLYPHS 500	/* arbitrary */

/* ---------------- Definitions ---------------- */

struct pdf_base_font_s {
    /*
     * For the standard 14 fonts, copied == complete is a complete copy
     * of the font, and DO_SUBSET = NO.
     *
     * For fonts that MAY be subsetted, copied is a partial copy,
     * complete is a complete copy, and DO_SUBSET = UNKNOWN until
     * pdf_font_do_subset is called.
     *
     * For fonts that MUST be subsetted, copied == complete is a partial
     * copy, and DO_SUBSET = YES.
     */
    gs_font_base *copied;
    gs_font_base *complete;
    enum {
	DO_SUBSET_UNKNOWN = 0,
	DO_SUBSET_NO,
	DO_SUBSET_YES
    } do_subset;
    bool is_complete;
    /*
     * For CIDFonts, which are always subsetted, num_glyphs is CIDCount.
     * For optionally subsetted fonts, num_glyphs is the count of glyphs
     * in the font when originally copied.  Note that if the font is
     * downloaded incrementally, num_glyphs may be 0.
     */
    int num_glyphs;
    byte *CIDSet;		/* for CIDFonts */
    long FontFile_id;		/* non-0 iff the font is embedded */
    gs_string font_name;
    bool written;
};
BASIC_PTRS(pdf_base_font_ptrs) {
    GC_OBJ_ELT(pdf_base_font_t, copied),
    GC_OBJ_ELT(pdf_base_font_t, complete),
    GC_OBJ_ELT(pdf_base_font_t, CIDSet),
    GC_STRING_ELT(pdf_base_font_t, font_name)
};
gs_private_st_basic(st_pdf_base_font, pdf_base_font_t, "pdf_base_font_t",
		    pdf_base_font_ptrs, pdf_base_font_data);

/* ---------------- Private ---------------- */

#define SUBSET_PREFIX_SIZE 7	/* XXXXXX+ */

/*
 * Determine whether a font is a subset font by examining the name.
 */
private bool
pdf_has_subset_prefix(const byte *str, uint size)
{
    int i;

    if (size < SUBSET_PREFIX_SIZE || str[SUBSET_PREFIX_SIZE - 1] != '+')
	return false;
    for (i = 0; i < SUBSET_PREFIX_SIZE - 1; ++i)
	if ((uint)(str[i] - 'A') >= 26)
	    return false;
    return true;
}

/*
 * Add the XXXXXX+ prefix for a subset font.
 */
private int
pdf_add_subset_prefix(const gx_device_pdf *pdev, gs_string *pstr)
{
    uint size = pstr->size;
    byte *data = gs_resize_string(pdev->pdf_memory, pstr->data, size,
				  size + SUBSET_PREFIX_SIZE,
				  "pdf_add_subset_prefix");
    ulong v = (ulong)(pdev->random_offset + rand());
    int i;

    if (data == 0)
	return_error(gs_error_VMerror);
    memmove(data + SUBSET_PREFIX_SIZE, data, size);
    for (i = 0; i < SUBSET_PREFIX_SIZE - 1; ++i, v /= 26)
	data[i] = 'A' + (v % 26);
    data[SUBSET_PREFIX_SIZE - 1] = '+';
    pstr->data = data;
    pstr->size = size + SUBSET_PREFIX_SIZE;
    return 0;
}

/* Begin writing FontFile* data. */
private int
pdf_begin_fontfile(gx_device_pdf *pdev, long FontFile_id,
		   const char *entries, long len1, pdf_data_writer_t *pdw)
{
    stream *s;

    pdf_open_separate(pdev, FontFile_id);
    s = pdev->strm;
    stream_puts(s, "<<");
    if (entries)
	stream_puts(pdev->strm, entries);
    if (len1 >= 0)
	pprintld1(pdev->strm, "/Length1 %ld", len1);
    return pdf_begin_data_stream(pdev, pdw, DATA_STREAM_BINARY |
				 (pdev->CompressFonts ?
				  DATA_STREAM_COMPRESS : 0));
}

/* Finish writing FontFile* data. */
private int
pdf_end_fontfile(gx_device_pdf *pdev, pdf_data_writer_t *pdw)
{
    return pdf_end_data(pdw);
}

/* ---------------- Public ---------------- */

/*
 * Allocate and initialize a base font structure, making the required
 * stable copy/ies of the gs_font.  Note that this removes any XXXXXX+
 * font name prefix from the copy.  If do_complete is true, the copy is
 * a complete one, and adding glyphs or Encoding entries is not allowed.
 */
int
pdf_base_font_alloc(gx_device_pdf *pdev, pdf_base_font_t **ppbfont,
		    gs_font_base *font, bool do_complete)
{
    gs_memory_t *mem = pdev->pdf_memory;
    gs_font *copied;
    gs_font *complete;
    pdf_base_font_t *pbfont =
	gs_alloc_struct(mem, pdf_base_font_t,
			&st_pdf_base_font, "pdf_base_font_alloc");
    const gs_font_name *pfname =
	(font->key_name.size != 0 ? &font->key_name : &font->font_name);
    gs_const_string font_name;
    char fnbuf[3 + sizeof(long) / 3 + 1]; /* .F#######\0 */
    int code;

    if (pbfont == 0)
	return_error(gs_error_VMerror);
    code = gs_copy_font((gs_font *)font, mem, &copied);
    if (code < 0)
	goto fail;
    memset(pbfont, 0, sizeof(*pbfont));
    switch (font->FontType) {
    case ft_encrypted:
    case ft_encrypted2:
	pbfont->do_subset = (do_complete ? DO_SUBSET_NO : DO_SUBSET_UNKNOWN);
	/* We will count the number of glyphs below. */
	pbfont->num_glyphs = -1;
	break;
    case ft_TrueType:
	pbfont->num_glyphs = ((gs_font_type42 *)font)->data.numGlyphs;
	pbfont->do_subset =
	    (pbfont->num_glyphs <= MAX_NO_SUBSET_GLYPHS ?
	     DO_SUBSET_UNKNOWN : DO_SUBSET_YES);
	break;
    case ft_CID_encrypted:
	pbfont->num_glyphs = ((gs_font_cid0 *)font)->cidata.common.CIDCount;
	goto cid;
    case ft_CID_TrueType:
	pbfont->num_glyphs = ((gs_font_cid2 *)font)->cidata.common.CIDCount;
    cid:
	pbfont->do_subset = DO_SUBSET_YES;
	pbfont->CIDSet =
	    gs_alloc_bytes(mem, (pbfont->num_glyphs + 7) / 8,
			   "pdf_base_font_alloc(CIDSet)");
	if (pbfont->CIDSet == 0) {
	    code = gs_note_error(gs_error_VMerror);
	    goto fail;
	}
	memset(pbfont->CIDSet, 0, (pbfont->num_glyphs + 7) / 8);
	break;
    default:
	code = gs_note_error(gs_error_rangecheck);
	goto fail;
    }
    if (pbfont->do_subset != DO_SUBSET_YES) {
	/* The only possibly non-subsetted fonts are Type 1/2 and Type 42. */
	if (do_complete)
	    complete = copied, code = 0;
	else
	    code = gs_copy_font((gs_font *)font, mem, &complete);
	if (code >= 0)
	    code = gs_copy_font_complete((gs_font *)font, complete);
	if (pbfont->num_glyphs < 0) { /* Type 1 */
	    int index, count;
	    gs_glyph glyph;

	    for (index = 0, count = 0;
		 (font->procs.enumerate_glyph((gs_font *)font, &index,
					      GLYPH_SPACE_NAME, &glyph),
		  index != 0);
		 )
		++count;
	    pbfont->num_glyphs = count;
	}
    } else
	complete = copied;
    pbfont->copied = (gs_font_base *)copied;
    pbfont->complete = (gs_font_base *)complete;
    pbfont->is_complete = do_complete;
    if (pfname->size > 0) {
	font_name.data = pfname->chars;
	font_name.size = pfname->size;
	while (pdf_has_subset_prefix(font_name.data, font_name.size)) {
	    /* Strip off an existing subset prefix. */
	    font_name.data += SUBSET_PREFIX_SIZE;
	    font_name.size -= SUBSET_PREFIX_SIZE;
	}
    } else {
	sprintf(fnbuf, ".F%lx", (ulong)copied);
	font_name.data = (byte *)fnbuf;
	font_name.size = strlen(fnbuf);
    }
    pbfont->font_name.data =
	gs_alloc_string(mem, font_name.size, "pdf_base_font_alloc(font_name)");
    if (pbfont->font_name.data == 0)
	goto fail;
    memcpy(pbfont->font_name.data, font_name.data, font_name.size);
    pbfont->font_name.size = font_name.size;
    *ppbfont = pbfont;
    return 0;
 fail:
    gs_free_object(mem, pbfont, "pdf_base_font_alloc");
    return code;
}

/*
 * Return a reference to the name of a base font.  This name is guaranteed
 * not to have a XXXXXX+ prefix.  The client may change the name at will,
 * but must not add a XXXXXX+ prefix.
 */
gs_string *
pdf_base_font_name(pdf_base_font_t *pbfont)
{
    return &pbfont->font_name;
}

/*
 * Return the (copied, subset) font associated with a base font.
 * This procedure probably shouldn't exist....
 */
gs_font_base *
pdf_base_font_font(const pdf_base_font_t *pbfont)
{
    return pbfont->copied;
}

/*
 * Copy a glyph (presumably one that was just used) into a saved base
 * font.  Note that it is the client's responsibility to determine that
 * the source font is compatible with the target font.  (Normally they
 * will be the same.)
 */
int
pdf_base_font_copy_glyph(pdf_base_font_t *pbfont, gs_glyph glyph,
			 gs_font_base *font)
{
    int code =
	gs_copy_glyph_options((gs_font *)font, glyph,
			      (gs_font *)pbfont->copied,
			      (pbfont->is_complete ? COPY_GLYPH_NO_NEW : 0));

    if (code < 0)
	return code;
    if (pbfont->CIDSet != 0 &&
	(uint)(glyph - GS_MIN_CID_GLYPH) < pbfont->num_glyphs
	) {
	uint cid = glyph - GS_MIN_CID_GLYPH;

	pbfont->CIDSet[cid >> 3] |= 0x80 >> (cid & 7);
    }
    return 0;
}

/*
 * Determine whether a font should be subsetted.  Note that if the font is
 * subsetted, this procedure modifies the copied font by adding the XXXXXX+
 * font name prefix and clearing the UID.
 */
bool
pdf_do_subset_font(gx_device_pdf *pdev, pdf_base_font_t *pbfont)
{
    gs_font_base *copied = pbfont->copied;

    /*
     * If the decision has not been made already, determine whether or not
     * to subset the font.
     */
    if (pbfont->do_subset == DO_SUBSET_UNKNOWN) {
	int max_pct = pdev->params.MaxSubsetPct;
	bool do_subset = pdev->params.SubsetFonts && max_pct > 0;

	if (do_subset && max_pct < 100) {
	    /* We want to subset iff used <= total * MaxSubsetPct / 100. */
	    do_subset = false;
	    if (max_pct > 0) {
		int max_subset_used = pbfont->num_glyphs * max_pct / 100;
		int used, index;
		gs_glyph ignore_glyph;

		do_subset = true;
		for (index = 0, used = 0;
		     (copied->procs.enumerate_glyph((gs_font *)copied,
						&index, GLYPH_SPACE_INDEX,
						&ignore_glyph), index != 0);
		     )
		    if (++used > max_subset_used) {
			do_subset = false;
			break;
		    }
	    }
	}
	pbfont->do_subset = (do_subset ? DO_SUBSET_YES : DO_SUBSET_NO);
    }
    /*
     * Adjust the FontName and UID according to the subsetting decision.
     */
    if (pbfont->do_subset == DO_SUBSET_YES &&
	!pdf_has_subset_prefix(pbfont->font_name.data, pbfont->font_name.size)
	) {
	/*
	 * We have no way to report an error if pdf_add_subset_prefix
	 * doesn't have enough room to add the subset prefix, but the
	 * output is still OK if this happens.
	 */
	DISCARD(pdf_add_subset_prefix(pdev, &pbfont->font_name));
	/* Don't write a UID for subset fonts. */
	uid_set_invalid(&copied->UID);
    }
    return (pbfont->do_subset == DO_SUBSET_YES);
}

/*
 * Write the FontFile entry for an embedded font, /FontFile<n> # # R.
 */
int
pdf_write_FontFile_entry(gx_device_pdf *pdev, pdf_base_font_t *pbfont)
{
    stream *s = pdev->strm;
    const char *FontFile_key;

    switch (pbfont->copied->FontType) {
    case ft_TrueType:
    case ft_CID_TrueType:
	FontFile_key = "/FontFile2";
	break;
    default:			/* Type 1/2, CIDFontType 0 */
	FontFile_key = "/FontFile3";
    }
    if (pbfont->FontFile_id == 0)
	pbfont->FontFile_id = pdf_obj_ref(pdev);
    stream_puts(s, FontFile_key);
    pprintld1(s, " %ld 0 R", pbfont->FontFile_id);
    return 0;
}

/*
 * Write an embedded font.
 */
int
pdf_write_embedded_font(gx_device_pdf *pdev, pdf_base_font_t *pbfont)
{
    bool do_subset = pdf_do_subset_font(pdev, pbfont);
    gs_font_base *out_font =
	(do_subset ? pbfont->copied : pbfont->complete);
    long FontFile_id;
    gs_const_string fnstr;
    pdf_data_writer_t writer;
    int code;

    if (pbfont->written)
	return 0;		/* already written */
    if (pbfont->FontFile_id == 0)
	pbfont->FontFile_id = pdf_obj_ref(pdev);
    FontFile_id = pbfont->FontFile_id;
    fnstr.data = pbfont->font_name.data;
    fnstr.size = pbfont->font_name.size;

    /* Now write the font (or subset). */
    switch (out_font->FontType) {

    case ft_composite:
	/* Nothing to embed -- the descendant fonts do it all. */
	code = 0;
	break;

    case ft_encrypted:
    case ft_encrypted2:
	/*
	 * Since we only support PDF 1.2 and later, always write Type 1
	 * fonts as Type1C (Type 2).  Acrobat Reader apparently doesn't
	 * accept CFF fonts with Type 1 CharStrings, so we need to convert
	 * them.  Also remove lenIV, so Type 2 fonts will compress better.
	 */
#define TYPE2_OPTIONS (WRITE_TYPE2_NO_LENIV | WRITE_TYPE2_CHARSTRINGS)
	code = pdf_begin_fontfile(pdev, FontFile_id, "/Subtype/Type1C", -1L,
				  &writer);
	if (code < 0)
	    return code;
	code = psf_write_type2_font(writer.binary.strm,
				    (gs_font_type1 *)out_font,
				    TYPE2_OPTIONS |
			(pdev->CompatibilityLevel < 1.3 ? WRITE_TYPE2_AR3 : 0),
				    NULL, 0, &fnstr);
	goto finish;

    case ft_TrueType: {
#define TRUETYPE_OPTIONS (WRITE_TRUETYPE_NAME | WRITE_TRUETYPE_HVMTX)
	/****** WHEN SHOULD WE USE WRITE_TRUETYPE_CMAP? ******/
	/* Acrobat Reader 3 doesn't handle cmap format 6 correctly. */
	const int options = TRUETYPE_OPTIONS |
	    (pdev->CompatibilityLevel <= 1.2 ?
	     WRITE_TRUETYPE_NO_TRIMMED_TABLE : 0);
	stream poss;

	swrite_position_only(&poss);
	code = psf_write_truetype_font(&poss, (gs_font_type42 *)out_font,
				       options, NULL, 0, &fnstr);
	if (code < 0)
	    return code;
	code = pdf_begin_fontfile(pdev, FontFile_id, NULL, stell(&poss),
				  &writer);
	if (code < 0)
	    return code;
	code = psf_write_truetype_font(writer.binary.strm,
				       (gs_font_type42 *)out_font,
				       options, NULL, 0, &fnstr);
	goto finish;
    }

    case ft_CID_encrypted:
	code = pdf_begin_fontfile(pdev, FontFile_id, "/Subtype/CIDFontType0C",
				  -1L, &writer);
	if (code < 0)
	    return code;
	code = psf_write_cid0_font(writer.binary.strm,
				   (gs_font_cid0 *)out_font, TYPE2_OPTIONS,
				   NULL, 0, &fnstr);
	goto finish;

    case ft_CID_TrueType:
	/* CIDFontType 2 fonts don't use cmap, name, OS/2, or post. */
#define CID2_OPTIONS WRITE_TRUETYPE_HVMTX
	code = pdf_begin_fontfile(pdev, FontFile_id, NULL, -1L, &writer);
	if (code < 0)
	    return code;
	code = psf_write_cid2_font(writer.binary.strm,
				   (gs_font_cid2 *)out_font,
				   CID2_OPTIONS, NULL, 0, &fnstr);
    finish:
	pdf_end_fontfile(pdev, &writer);
	break;

    default:
	code = gs_note_error(gs_error_rangecheck);
    }

    pbfont->written = true;
    return code;
}

/*
 * Write the CharSet for a subsetted font, as a PDF string.
 */
int
pdf_write_CharSet(gx_device_pdf *pdev, pdf_base_font_t *pbfont)
{
    stream *s = pdev->strm;
    gs_font_base *font = pbfont->copied;
    int index;
    gs_glyph glyph;

    stream_puts(s, "(");
    for (index = 0;
	 (font->procs.enumerate_glyph((gs_font *)font, &index,
				      GLYPH_SPACE_NAME, &glyph),
	  index != 0);
	 ) {
	gs_const_string gstr;
	int code = font->procs.glyph_name((gs_font *)font, glyph, &gstr);

	/* Don't include .notdef. */
	if (code >= 0 &&
	    bytes_compare(gstr.data, gstr.size, (const byte *)".notdef", 7)
	    )
	    pdf_put_name(pdev, gstr.data, gstr.size);
    }
    stream_puts(s, ")");
    return 0;
}

/*
 * Write the CIDSet object for a subsetted CIDFont.
 */
int
pdf_write_CIDSet(gx_device_pdf *pdev, pdf_base_font_t *pbfont,
		 long *pcidset_id)
{
    pdf_data_writer_t writer;
    long cidset_id = pdf_begin_separate(pdev);
    int code = pdf_begin_data(pdev, &writer);

    if (code >= 0) {
	stream_write(writer.binary.strm, pbfont->CIDSet,
		     (pbfont->num_glyphs + 7) / 8);
	code = pdf_end_data(&writer);
	if (code >= 0) {
	    *pcidset_id = cidset_id;
	    return code;
	}
    }
    /* code < 0 */
    pdf_end_separate(pdev);
    return code;
}
