/* Copyright (C) 1999 Aladdin Enterprises.  All rights reserved.
 * This software is licensed to a single customer by Artifex Software Inc.
 * under the terms of a specific OEM agreement.
 */

/*$RCSfile$ $Revision$ */
/* Font handling for pdfwrite driver. */
#include "math_.h"
#include "memory_.h"
#include "string_.h"
#include "gx.h"
#include "gserrors.h"
#include "gsmalloc.h"		/* for patching font memory */
#include "gsmatrix.h"
#include "gspath.h"
#include "gsutil.h"		/* for bytes_compare */
#include "gxfixed.h"		/* for gxfcache.h */
#include "gxfont.h"
#include "gxfcache.h"		/* for orig_fonts list */
#include "gxpath.h"		/* for getting current point */
#include "gdevpdfx.h"
#include "scommon.h"

/* GC descriptors */
private_st_pdf_font_descriptor();
private_st_pdf_encoding_element();
private ENUM_PTRS_WITH(pdf_encoding_elt_enum_ptrs, pdf_encoding_element_t *pe) {
    uint count = size / (uint)sizeof(*pe);

    if (index >= count)
	return 0;
    return ENUM_CONST_STRING(&pe[index].str);
} ENUM_PTRS_END
private RELOC_PTRS_WITH(pdf_encoding_elt_reloc_ptrs, pdf_encoding_element_t *pe)
    uint count = size / (uint)sizeof(*pe);
    uint i;

    for (i = 0; i < count; ++i)
	RELOC_CONST_STRING_VAR(pe[i].str);
RELOC_PTRS_END

/* Define the 14 standard built-in fonts. */
const pdf_standard_font_t pdf_standard_fonts[] = {
#define m(name, enc) {name, enc},
    pdf_do_std_fonts(m)
#undef m
    {0}
};

/* ---------------- Standard fonts ---------------- */

/* Return the index of a standard font name, or -1 if missing. */
private int
pdf_find_standard_font(const byte *str, uint size)
{
    const pdf_standard_font_t *ppsf;

    for (ppsf = pdf_standard_fonts; ppsf->fname; ++ppsf)
	if (strlen(ppsf->fname) == size &&
	    !strncmp(ppsf->fname, (const char *)str, size)
	    )
	    return ppsf - pdf_standard_fonts;
    return -1;
}

/*
 * Return the index of a standard font with the same appearance
 * (CharStrings, Private, WeightVector) as a given font, or -1 if missing.
 */
private int
find_std_appearance(const gx_device_pdf *pdev, const gs_font_base *bfont,
		    int *psame)
{
    bool has_uid = uid_is_UniqueID(&bfont->UID) && bfont->UID.id != 0;
    const pdf_std_font_t *psf = pdev->std_fonts;
    int i;

    for (i = 0; i < PDF_NUM_STD_FONTS; ++psf, ++i) {
	if (has_uid) {
	    if (uid_equal(&bfont->UID, &psf->uid)) {
		return i;
	    }
	} else if (psf->font) {
	    int same = *psame =
		bfont->procs.same_font((const gs_font *)bfont, psf->font,
				       FONT_SAME_OUTLINES | FONT_SAME_METRICS |
				       FONT_SAME_ENCODING);

	    if (same & FONT_SAME_OUTLINES)
		return i;
	}
    }
    return -1;
}

/*
 * We register the fonts in pdev->std_fonts so that the pointers can
 * be weak (get set to 0 when the font is freed).
 */
private GS_NOTIFY_PROC(pdf_std_font_notify_proc);
typedef struct pdf_std_font_notify_s {
    gx_device_pdf *pdev;
    int index;			/* in std_fonts */
    gs_font *font;	/* for checking */
} pdf_std_font_notify_t;
gs_private_st_ptrs2(st_pdf_std_font_notify, pdf_std_font_notify_t,
		    "pdf_std_font_notify_t",
		    pdf_std_font_notify_enum_ptrs,
		    pdf_std_font_notify_reloc_ptrs,
		    pdev, font);
private int
pdf_std_font_notify_proc(void *vpsfn /*proc_data*/, void *event_data)
{
    pdf_std_font_notify_t *const psfn = vpsfn;
    gx_device_pdf *const pdev = psfn->pdev;
    gs_font *const font = psfn->font;

    if (event_data)
	return 0;		/* unknown event */
    if_debug3('_',
	      "[_]  notify 0x%lx: gs_font 0x%lx, index=%d\n",
	      (ulong)psfn, (ulong)font, psfn->index);
#ifdef DEBUG
    if (pdev->std_fonts[psfn->index].font != font)
	lprintf3("pdf_std_font_notify font = 0x%lx, std_fonts[%d] = 0x%lx\n",
		 (ulong)font, psfn->index,
		 (ulong)pdev->std_fonts[psfn->index].font);
    else
#endif
	pdev->std_fonts[psfn->index].font = 0;
    gs_font_notify_unregister(font, pdf_std_font_notify_proc, vpsfn);
    gs_free_object(pdev->pdf_memory, vpsfn, "pdf_std_font_notify_proc");
    return 0;
}

/*
 * Scan a font directory for standard fonts.  Return true if any new ones
 * were found.
 */
private bool
scan_for_standard_fonts(gx_device_pdf *pdev, const gs_font_dir *dir)
{
    bool found = false;
    gs_font *orig = dir->orig_fonts;

    for (; orig; orig = orig->next) {
	gs_font_base *obfont;

	if (orig->FontType == ft_composite || !orig->is_resource)
	    continue;
	obfont = (gs_font_base *)orig;
	if (uid_is_UniqueID(&obfont->UID)) {
	    /* Is it one of the standard fonts? */
	    int i = pdf_find_standard_font(orig->key_name.chars,
					   orig->key_name.size);

	    if (i >= 0 && pdev->std_fonts[i].font == 0) {
		pdf_std_font_notify_t *psfn =
		    gs_alloc_struct(pdev->pdf_memory, pdf_std_font_notify_t,
				    &st_pdf_std_font_notify,
				    "scan_for_standard_fonts");

		if (psfn == 0)
		    continue;	/* can't register */
		psfn->pdev = pdev;
		psfn->index = i;
		psfn->font = orig;
		if_debug3('_', "[_]register 0x%lx: gs_font 0x%lx, index=%d\n",
			  (ulong)psfn, (ulong)orig, i);
		gs_font_notify_register(orig, pdf_std_font_notify_proc, psfn);
		pdev->std_fonts[i].uid = obfont->UID;
		pdev->std_fonts[i].orig_matrix = obfont->FontMatrix;
		pdev->std_fonts[i].font = orig;
		found = true;
	    }
	}
    }
    return found;
}

/*
 * Find the original (unscaled) standard font corresponding to an
 * arbitrary font, if any.
 */
bool
pdf_find_orig_font(gx_device_pdf *pdev, gs_font *font, gs_const_string *pfname,
		   gs_matrix *pfmat)
{
    bool scan = true;
    int i;

    if (font->FontType == ft_composite)
	return false;
    for (;; font = font->base) {
	gs_font_base *bfont = (gs_font_base *)font;
	int same;

	/* Look for a standard font with the same appearance. */
	i = find_std_appearance(pdev, bfont, &same);
	if (i >= 0)
	    goto ret;
	if (scan) {
	    /* Scan for fonts with any of the standard names that */
	    /* have a UID. */
	    bool found = scan_for_standard_fonts(pdev, font->dir);

	    scan = false;
	    if (found) {
		i = find_std_appearance(pdev, bfont, &same);
		if (i >= 0)
		    goto ret;
	    }
	}
	if (font->base == font)
	    return false;
    }
 ret:
    pfname->data = (const byte *)pdf_standard_fonts[i].fname;
    pfname->size = strlen((const char *)pfname->data);
    *pfmat = pdev->std_fonts[i].orig_matrix;
    return true;
}

/*
 * Determine the embedding status of a font.  If the font is in the base
 * 14, store its index (0..13) in *pindex, otherwise store -1 there.
 */
private bool
font_is_symbolic(const gs_font *font)
{
    if (font->FontType == ft_composite)
	return true;		/* arbitrary */
    switch (((const gs_font_base *)font)->nearest_encoding_index) {
    case ENCODING_INDEX_STANDARD:
    case ENCODING_INDEX_ISOLATIN1:
    case ENCODING_INDEX_WINANSI:
    case ENCODING_INDEX_MACROMAN:
	return false;
    default:
	return true;
    }
}
private bool
embed_list_includes(const gs_param_string_array *psa, const byte *chars,
		    uint size)
{
    uint i;

    for (i = 0; i < psa->size; ++i)
	if (!bytes_compare(psa->data[i].data, psa->data[i].size, chars, size))
	    return true;
    return false;
}
pdf_font_embed_t
pdf_font_embed_status(gx_device_pdf *pdev, gs_font *font, int *pindex,
		      int *psame)
{
    const byte *chars = font->font_name.chars;
    uint size = font->font_name.size;
    /* Check whether the font is in the base 14. */
    int index = pdf_find_standard_font(chars, size);

    if (index >= 0) {
	*pindex = index;
	if (font->is_resource) {
	    *psame = ~0;
	    return FONT_EMBED_BASE14;
	} else if (font->FontType != ft_composite &&
		   find_std_appearance(pdev, (gs_font_base *)font,
				       psame) == index)
	    return FONT_EMBED_BASE14;
    }
    *pindex = -1;
    /* Check the Embed lists. */
    if (embed_list_includes(&pdev->params.NeverEmbed, chars, size))
	return FONT_EMBED_NO;
    if (pdev->params.EmbedAllFonts || font_is_symbolic(font) ||
	embed_list_includes(&pdev->params.AlwaysEmbed, chars, size))
	return FONT_EMBED_YES;
    return FONT_EMBED_UNKNOWN;
}

/* ---------------- Everything else ---------------- */

/* Allocate a font resource. */
int
pdf_alloc_font(gx_device_pdf *pdev, gs_id rid, pdf_font_t **ppfres,
	       bool with_descriptor)
{
    gs_memory_t *mem = pdev->v_memory;
    pdf_font_descriptor_t *pfd = 0;
    int code;
    pdf_font_t *pfres;

    if (with_descriptor) {
	pfd = gs_alloc_struct(mem, pdf_font_descriptor_t,
			      &st_pdf_font_descriptor,
			      "pdf_alloc_font(descriptor)");
	if (pfd == 0)
	    return_error(gs_error_VMerror);
	memset(pfd, 0, sizeof(*pfd));
    }
    code = pdf_alloc_resource(pdev, resourceFont, rid,
			      (pdf_resource_t **)ppfres, 0L);
    if (code < 0) {
	gs_free_object(mem, pfd, "pdf_alloc_font(descriptor)");
	return code;
    }
    pfres = *ppfres;
    pfres->rid = rid;
    memset((byte *)pfres + sizeof(pdf_resource_t), 0,
	   sizeof(*pfres) - sizeof(pdf_resource_t));
    sprintf(pfres->frname, "R%ld", pfres->object->id);
    pfres->index = -1;
    pfres->BaseEncoding = ENCODING_INDEX_UNKNOWN; /* -1 */
    pfres->differences = 0;
    pfres->descriptor = pfd;
    pfres->char_procs = 0;
    pfres->skip = false;
    return 0;
}

/* Add an encoding difference to a font. */
int
pdf_add_encoding_difference(gx_device_pdf *pdev, pdf_font_t *ppf, int chr,
			    const gs_font_base *bfont, gs_glyph glyph)
{
    pdf_encoding_element_t *pdiff = ppf->differences;

    if (pdiff == 0) {
	pdiff = gs_alloc_struct_array(pdev->pdf_memory, 256,
				      pdf_encoding_element_t,
				      &st_pdf_encoding_element,
				      "differences");
	if (pdiff == 0)
	    return_error(gs_error_VMerror);
	memset(pdiff, 0, sizeof(pdf_encoding_element_t) * 256);
	ppf->differences = pdiff;
    }
    pdiff[chr].glyph = glyph;
    pdiff[chr].str.data = (const byte *)
	bfont->procs.callbacks.glyph_name(glyph, &pdiff[chr].str.size);
    return 0;
}

/* Get the width of a given character in a (base) font. */
int
pdf_char_width(pdf_font_t *ppf, int ch, gs_font *font,
	       int *pwidth /* may be NULL */)
{
    if (ch < 0 || ch > 255)
	return_error(gs_error_rangecheck);
    if (!(ppf->widths_known[ch >> 3] & (1 << (ch & 7)))) {
	gs_font_base *bfont = (gs_font_base *)font;
	gs_glyph glyph = bfont->procs.encode_char(font, (gs_char)ch,
						  GLYPH_SPACE_INDEX);
	int wmode = font->WMode;
	gs_glyph_info_t info;
	double w, v;
	int code;

	if (glyph != gs_no_glyph &&
	    (code = font->procs.glyph_info(font, glyph, NULL,
					   GLYPH_INFO_WIDTH0 << wmode,
					   &info)) >= 0
	    ) {
	    if (wmode && (w = info.width[wmode].y) != 0)
		v = info.width[wmode].x;
	    else
		w = info.width[wmode].x, v = info.width[wmode].y;
	    if (v != 0)
		return_error(gs_error_rangecheck);
	    if (font->FontType == ft_TrueType) {
		/* TrueType fonts have 1 unit per em, we want 1000. */
		w *= 1000;
	    }
	    ppf->Widths[ch] = (int)w;
	} else {
	    /* Try for MissingWidth. */
	    static const gs_point tt_scale = {1000, 1000};
	    const gs_point *pscale = 0;
	    gs_font_info_t finfo;

	    if (font->FontType == ft_TrueType) {
		/* TrueType fonts have 1 unit per em, we want 1000. */
		pscale = &tt_scale;
	    }
	    code = font->procs.font_info(font, pscale, FONT_INFO_MISSING_WIDTH,
					 &finfo);
	    if (code < 0)
		return code;
	    ppf->Widths[ch] = finfo.MissingWidth;
	}
	ppf->widths_known[ch >> 3] |= 1 << (ch & 7);
    }
    if (pwidth)
	*pwidth = ppf->Widths[ch];
    return 0;
}

/* Compute the FontDescriptor for a font. */
private int
font_char_bbox(gs_rect *pbox, gs_font *font, int ch, const gs_matrix *pmat)
{
    gs_glyph glyph;
    gs_glyph_info_t info;
    int code;

    glyph = font->procs.encode_char(font, (gs_char)ch, GLYPH_SPACE_INDEX);
    if (glyph == gs_no_glyph)
	return gs_error_undefined;
    code = font->procs.glyph_info(font, glyph, pmat, GLYPH_INFO_BBOX, &info);
    if (code < 0)
	return code;
    *pbox = info.bbox;
    return 0;
}
int
pdf_compute_font_descriptor(gx_device_pdf *pdev, pdf_font_descriptor_t *pfd,
			    gs_font *font, const byte *used /*[32]*/)
{
    gs_font_base *bfont = (gs_font_base *)font;
    gs_glyph glyph, notdef;
    int index;
    int wmode = font->WMode;
    pdf_font_descriptor_t desc;
    gs_matrix smat;
    gs_matrix *pmat = NULL;
    int fixed_width = 0;
    int code;

    memset(&desc, 0, sizeof(desc));
    desc.FontBBox.p.x = desc.FontBBox.p.y = max_int;
    desc.FontBBox.q.x = desc.FontBBox.q.y = min_int;
    /*
     * Embedded TrueType fonts use a 1000-unit character space, but the
     * font itself uses a 1-unit space.  Compensate for this here.
     */
    if (font->FontType == ft_TrueType) {
	gs_make_scaling(1000.0, 1000.0, &smat);
	pmat = &smat;
    }
    /*
     * Scan the entire glyph space to compute Ascent, Descent, FontBBox,
     * and the fixed width if any.
     */
    notdef = gs_no_glyph;
    for (index = 0;
	 (code = font->procs.enumerate_glyph(font, &index, GLYPH_SPACE_INDEX, &glyph)) >= 0 &&
	     index != 0;
	 ) {
	gs_glyph_info_t info;
	gs_const_string gnstr;

	code = font->procs.glyph_info(font, glyph, pmat,
				      (GLYPH_INFO_WIDTH0 << wmode) |
				      GLYPH_INFO_BBOX | GLYPH_INFO_NUM_PIECES,
				      &info);
	if (code < 0)
	    return code;
	if (notdef == gs_no_glyph) {
	    gnstr.data = (const byte *)
		bfont->procs.callbacks.glyph_name(glyph, &gnstr.size);
	    if (gnstr.size == 7 && !memcmp(gnstr.data, ".notdef", 7)) {
		notdef = glyph;
		desc.MissingWidth = info.width[wmode].x;
	    }
	}
	rect_merge(desc.FontBBox, info.bbox);
	if (!info.num_pieces)
	    desc.Ascent = max(desc.Ascent, info.bbox.q.y);
	if (info.width[wmode].y != 0)
	    fixed_width = min_int;
	else if (fixed_width == 0)
	    fixed_width = info.width[wmode].x;
	else if (info.width[wmode].x != fixed_width)
	    fixed_width = min_int;
    }
    if (code < 0)
	return code;
    if (desc.Ascent == 0)
	desc.Ascent = desc.FontBBox.q.y;
    desc.Descent = desc.FontBBox.p.y;
    if (fixed_width > 0) {
	desc.Flags |= FONT_IS_FIXED_WIDTH;
	desc.AvgWidth = desc.MaxWidth = desc.MissingWidth = fixed_width;
    }
    if (font_is_symbolic(font))
	desc.Flags |= FONT_IS_SYMBOLIC;
    else {
	/*
	 * Look at various specific characters to guess at the remaining
	 * descriptor values (CapHeight, ItalicAngle, StemV, and flags
	 * SERIF, SCRIPT, ITALIC, ALL_CAPS, and SMALL_CAPS).  The
	 * algorithms are pretty crude.
	 */
	/*
	 * Look at the glyphs for the lower-case letters.  If they are
	 * all missing, this is an all-cap font; if any is present, check
	 * the relative heights to determine whether this is a small-cap font.
	 */
	bool small_present = false;
	int ch;
	int height = min_int, x_height = min_int, descent = max_int;
	int cap_height = 0;
	gs_rect bbox, bbox2;

	desc.Flags |= FONT_IS_ADOBE_ROMAN; /* required if not symbolic */
	for (ch = 'a'; ch <= 'z'; ++ch) {
	    int y0, y1;

	    code = font_char_bbox(&bbox, font, ch, pmat);
	    if (code < 0)
		continue;
	    small_present = true;
	    y0 = (int)bbox.p.y;
	    y1 = (int)bbox.q.y;
	    switch (ch) {
	    case 'b': case 'd': case 'f': case 'h':
	    case 'k': case 'l': case 't': /* ascender */
		height = max(height, y1);
	    case 'i':		/* anomalous ascent */
		break;
	    case 'j':		/* descender with anomalous ascent */
		descent = min(descent, y0);
		break;
	    case 'g': case 'p': case 'q': case 'y': /* descender */
		descent = min(descent, y0);
	    default:		/* no ascender or descender */
		x_height = max(x_height, y1);		
	    }
	    desc.XHeight = (int)x_height;
	}
	if (!small_present)
	    desc.Flags |= FONT_IS_ALL_CAPS;
	else if (descent > desc.Descent / 3 || x_height > height * 0.8)
	    desc.Flags |= FONT_IS_SMALL_CAPS;
	for (ch = 'A'; ch <= 'Z'; ++ch) {
	    code = font_char_bbox(&bbox, font, ch, pmat);
	    if (code < 0)
		continue;
	    cap_height = max(cap_height, (int)bbox.q.y);
	}
	desc.CapHeight = cap_height;
	/*
	 * Look at various glyphs to determine ItalicAngle, StemV,
	 * SERIF, SCRIPT, and ITALIC.
	 */
	if ((code = font_char_bbox(&bbox, font, ':', pmat)) >= 0 &&
	    (code = font_char_bbox(&bbox2, font, '.', pmat)) >= 0
	    ) {
	    /* Calculate the dominant angle. */
	    int angle = 
		(int)(atan2((bbox.q.y - bbox.p.y) - (bbox2.q.y - bbox2.p.y),
			    (bbox.q.x - bbox.p.x) - (bbox2.q.x - bbox2.p.x)) *
		      radians_to_degrees) - 90;

	    /* Normalize to [-90..90]. */
	    while (angle > 90)
		angle -= 180;
	    while (angle < -90)
		angle += 180;
	    if (angle < -30)
		angle = -30;
	    else if (angle > 30)
		angle = 30;
	    /*
	     * For script or embellished fonts, we can get an angle that is
	     * slightly off from zero even for non-italic fonts.
	     * Compensate for this now.
	     */
	    if (angle <= 2 && angle >= -2)
		angle = 0;
	    desc.ItalicAngle = angle;
	}
	if (desc.ItalicAngle)
	    desc.Flags |= FONT_IS_ITALIC;
	if (code >= 0) {
	    double wdot = bbox2.q.x - bbox2.p.x;

	    if ((code = font_char_bbox(&bbox2, font, 'I', pmat)) >= 0) {
		/* Calculate the serif and stem widths. */
		double wcolon = bbox.q.x - bbox.p.x;
		double wI = bbox2.q.x - bbox2.p.x;
		double serif_width = (wI - wcolon) / 2;
		double stem_v = wdot;

		desc.StemV = (int)max(stem_v, 0);
		if (serif_width < 0 || stem_v < 0)
		    desc.Flags |= FONT_IS_SCRIPT;
		else if (serif_width != 0)
		    desc.Flags |= FONT_IS_SERIF;
	    }
	}
    }
    if (desc.CapHeight == 0)
	desc.CapHeight = desc.Ascent;
    if (desc.StemV == 0)
	desc.StemV = (int)(desc.FontBBox.q.x * 0.15);
    *pfd = desc;
    return 0;
}

/* ---------------- Writing font resources ---------------- */

/*
 * Determine whether a font is a subset font by examining the name.
 */
private bool
has_subset_prefix(const byte *str, uint size)
{
    int i;

    if (size < 7 || str[6] != '+')
	return false;
    for (i = 0; i < 6; ++i)
	if ((uint)(str[i] - 'A') >= 26)
	    return false;
    return true;
}

/*
 * Make the prefix for a subset font from the font's resource ID.
 */
private void
make_subset_prefix(byte *str, ulong id)
{
    int i;
    ulong v;

    for (i = 0, v = id * 987654321; i < 6; ++i, v /= 26)
	str[i] = 'A' + (v % 26);
    str[6] = '+';
}

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

/* Write the FontFile data for an embedded Type 1 font. */
private int
pdf_embed_font_type1(gx_device_pdf *pdev, pdf_font_descriptor_t *pfd,
		     gs_font_type1 *font, gs_glyph subset_glyphs[256],
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
#define TYPE1_OPTIONS (WRITE_TYPE1_EEXEC | WRITE_TYPE1_EEXEC_MARK)
    code = psdf_write_type1_font(&poss, font, TYPE1_OPTIONS,
				 subset_glyphs, subset_size, pfname, lengths);
    if (code < 0)
	return code;
    pdf_open_separate(pdev, pfd->FontFile_id);
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

	psdf_write_type1_font(writer.strm, font, TYPE1_OPTIONS,
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
    psdf_write_type1_font(writer.strm, font, TYPE1_OPTIONS,
			  subset_glyphs, subset_size, pfname,
			  lengths /*ignored*/);
#endif
#undef TYPE1_OPTIONS
    psdf_end_binary(&writer);
    pdf_end_fontfile(pdev, start, length_id);
    return 0;
}

/* Write the FontFile2 data for an embedded TrueType font. */
private int
pdf_embed_font_type42(gx_device_pdf *pdev, pdf_font_descriptor_t *pfd,
		      gs_font_type42 *font, gs_glyph subset_glyphs[256],
		      uint subset_size, const gs_const_string *pfname)
{
    stream poss;
    int length;
    int code;
    long length_id;
    long start;
    psdf_binary_writer writer;
    /* Acrobat Reader 3 doesn't handle cmap format 6 correctly. */
    const int options = WRITE_TRUETYPE_CMAP | WRITE_TRUETYPE_NAME |
	(pdev->CompatibilityLevel <= 1.2 ?
	 WRITE_TRUETYPE_NO_TRIMMED_TABLE : 0);

    swrite_position_only(&poss);
    code = psdf_write_truetype_font(&poss, font, options,
				    subset_glyphs, subset_size, pfname);
    if (code < 0)
	return code;
    length = stell(&poss);
    pdf_open_separate(pdev, pfd->FontFile_id);
    pdf_begin_fontfile(pdev, &length_id);
    pprintd1(pdev->strm, "/Length1 %d>>stream\n", length);
    start = pdf_stell(pdev);
    code = psdf_begin_binary((gx_device_psdf *)pdev, &writer);
    if (code < 0)
	return code;
    psdf_write_truetype_font(writer.strm, font, options,
			     subset_glyphs, subset_size, pfname);
    psdf_end_binary(&writer);
    pdf_end_fontfile(pdev, start, length_id);
    return 0;
}


/* Write out the Widths for an embedded or synthesized font. */
private int
pdf_write_widths(gx_device_pdf *pdev, int first, int last,
		 const int widths[256])
{
    stream *s = pdev->strm;
    int i;

    pprintd2(s, "/FirstChar %d/LastChar %d/Widths[", first, last);
    for (i = first; i <= last; ++i)
	pprintd1(s, (i & 15 ? " %d" : ("\n%d")), widths[i]);
    pputs(s, "]\n");
    return 0;
}

/* Write a FontBBox dictionary element. */
private int
pdf_write_font_bbox(gx_device_pdf *pdev, const gs_int_rect *pbox)
{
    stream *s = pdev->strm;

    pprintd4(s, "/FontBBox[%d %d %d %d]",
	     pbox->p.x, pbox->p.y, pbox->q.x, pbox->q.y);
    return 0;
}

/* Write a synthesized bitmap font resource. */
private int
pdf_write_synthesized_type3(gx_device_pdf *pdev, const pdf_font_t *pef)
{
    stream *s;
    gs_int_rect bbox;
    int widths[256];

    memset(&bbox, 0, sizeof(bbox));
    memset(widths, 0, sizeof(widths));
    pdf_open_separate(pdev, pef->object->id);
    s = pdev->strm;
    pprints1(s, "<</Type/Font/Name/%s/Subtype/Type3", pef->frname);
    pprintld1(s, "/Encoding %ld 0 R/CharProcs", pdev->embedded_encoding_id);

    /* Write the CharProcs. */
    {
	const pdf_char_proc_t *pcp;
	int w;

	pputs(s, "<<");
	/* Write real characters. */
	for (pcp = pef->char_procs; pcp; pcp = pcp->char_next) {
	    bbox.p.y = min(bbox.p.y, pcp->y_offset);
	    bbox.q.x = max(bbox.q.x, pcp->width);
	    bbox.q.y = max(bbox.q.y, pcp->height + pcp->y_offset);
	    widths[pcp->char_code] = pcp->x_width;
	    pprintld2(s, "/a%ld\n%ld 0 R", (long)pcp->char_code,
		      pcp->object->id);
	}
	/* Write space characters. */
	for (w = 0; w < countof(pef->spaces); ++w) {
	    byte ch = pef->spaces[w];

	    if (ch) {
		pprintld2(s, "/a%ld\n%ld 0 R", (long)ch,
			  pdev->space_char_ids[w]);
		widths[ch] = w + X_SPACE_MIN;
	    }
	}
	pputs(s, ">>");
    }

    pdf_write_font_bbox(pdev, &bbox);
    pputs(s, "/FontMatrix[1 0 0 1 0 0]");
    pdf_write_widths(pdev, 0, pef->num_chars - 1, widths);
    pputs(s, ">>\n");
    pdf_end_separate(pdev);
    return 0;
}

/* Write a font descriptor. */
private int
pdf_write_font_descriptor(gx_device_pdf *pdev,
			  const pdf_font_descriptor_t *pfd, int Flags,
			  const gs_const_string *pfname,
			  const char *FontFile_key)
{
#define DESC_INT(str, memb)\
 {str, gs_param_type_int, offset_of(pdf_font_descriptor_t, memb)}
    static const gs_param_item_t required_items[] = {
	DESC_INT("Ascent", Ascent),
	DESC_INT("CapHeight", CapHeight),
	DESC_INT("Descent", Descent),
	DESC_INT("ItalicAngle", ItalicAngle),
	DESC_INT("StemV", StemV),
	gs_param_item_end
    };
    static const gs_param_item_t optional_items[] = {
	DESC_INT("AvgWidth", AvgWidth),
	DESC_INT("Leading", Leading),
	DESC_INT("MaxWidth", MaxWidth),
	DESC_INT("MissingWidth", MissingWidth),
	DESC_INT("StemH", StemH),
	DESC_INT("XHeight", XHeight),
	gs_param_item_end
    };
#undef DESC_INT
    param_printer_params_t params;
    static const param_printer_params_t ppp_defaults = {
	param_printer_params_default_values
    };
    printer_param_list_t rlist;
    gs_param_list *const plist = (gs_param_list *)&rlist;
    
    pdf_font_descriptor_t defaults;
    stream *s;
    int code;

    pdf_open_separate(pdev, pfd->id);
    params = ppp_defaults;
    s = pdev->strm;
    code = s_init_param_printer(&rlist, &params, s);
    pputs(s, "<</Type/FontDescriptor/FontName");
    pdf_put_name(pdev, pfname->data, pfname->size);
    gs_param_write_items(plist, pfd, NULL, required_items);
    param_write_int(plist, "Flags", &Flags);
    pdf_write_font_bbox(pdev, &pfd->FontBBox);
    memset(&defaults, 0, sizeof(defaults));
    gs_param_write_items(plist, pfd, &defaults, optional_items);
    s_release_param_printer(&rlist);
    if (pfd->FontFile_id) {
	pputs(s, FontFile_key);
	pprintld1(s, " %ld 0 R", pfd->FontFile_id);
    }
    pputs(s, ">>\n");
    pdf_end_separate(pdev);
    return 0;
}

/*
 * Write a Type 1 or TrueType font resource, including Widths, Encoding,
 * and/or FontDescriptor.
 */
private int
pdf_write_font_resource(gx_device_pdf *pdev, const pdf_font_t *pef,
			const gs_const_string *pfname)
{
    stream *s;
    const pdf_font_descriptor_t *pfd = pef->descriptor;
    const char *FontFile_key;
    int Flags;
    int code;

    pdf_open_separate(pdev, pef->object->id);
    s = pdev->strm;
    if (pfd)
	Flags = pfd->Flags;
    switch (pef->FontType) {
    case ft_encrypted:
	pputs(s, "<</Subtype/Type1/BaseFont");
	pdf_put_name(pdev, pfname->data, pfname->size);
	FontFile_key = "/FontFile";
	break;
    case ft_TrueType:
	pputs(s, "<</Subtype/TrueType/BaseFont");
	/****** WHAT ABOUT STYLE INFO? ******/
	pdf_put_name(pdev, pfname->data, pfname->size);
	FontFile_key = "/FontFile2";
	/*
	 * Hack: make all embedded subset TrueType fonts "symbolic" to work
	 * around undocumented assumptions in Acrobat Reader.
	 */
	if (has_subset_prefix(pfname->data, pfname->size))
	    Flags = (Flags & ~(FONT_IS_ADOBE_ROMAN)) |
		FONT_IS_SYMBOLIC;
	break;
    default:
	return_error(gs_error_rangecheck);
    }
    pprintld1(s, "/Type/Font/Name/R%ld", pef->object->id);
    if (pfd)
	pprintld1(s, "/FontDescriptor %ld 0 R", pfd->id);
    if (pef->index < 0 || pef->differences) {
	/*
	 * Among the many problems in the PDF 1.3 documentation is the
	 * omission of the following interesting fact: FirstChar and
	 * LastChar are *not* simply a way to strip off initial and final
	 * entries in the Widths array that are equal to MissingWidth.
	 * Acrobat Reader assumes that characters with codes less than
	 * FirstChar or greater than LastChar are undefined, without
	 * bothering to consult the Encoding.  Therefore, the implicit value
	 * of MissingWidth is pretty useless, because there must be explicit
	 * Width entries for every character in the font that is every used.
	 */
	int first = 0, last = 248;
	byte first_byte, last_byte;

	while (first <= last && (first_byte = pef->chars_used[first >> 3]) == 0)
	    first += 8;
	while (last >= first && (last_byte = pef->chars_used[last >> 3]) == 0)
	    last -= 8;
	/*
	 * At this point, either first > last, which indicates that none of
	 * the characters in the font were used (unusual to say the least,
	 * but must be handled correctly), or first_byte and last_byte are
	 * both non-zero.
	 */
	if (first > last) {
	    /*
	     * None of the characters in the font were used.  This is
	     * unusual, to say the least, but must be handled correctly.
	     */
	    first = last = 0;
	} else {
	    while (!(first_byte & 1))
		first_byte >>= 1, ++first;
	    for (last += 7; !(last_byte & 0x80);)
		last_byte <<= 1, --last;
	}
	pdf_write_widths(pdev, first, last, pef->Widths);
    }
    if (pef->BaseEncoding >= 0 || pef->differences) {
	long diff_id = pdf_obj_ref(pdev);

	pprintld1(s, "/Encoding %ld 0 R>>\n", diff_id);
	pdf_end_separate(pdev);
	pdf_open_separate(pdev, diff_id);
	s = pdev->strm;
	pputs(s, "<</Type/Encoding");
	if (pef->BaseEncoding >= 0) {
	    static const char *const encoding_names[] = {
		KNOWN_REAL_ENCODING_NAMES
	    };

	    pprints1(s, "/BaseEncoding/%s", encoding_names[pef->BaseEncoding]);
	}
	if (pef->differences) {
	    int prev = 256;
	    int i;

	    pputs(s, "/Differences[");
	    for (i = 0; i < 256; ++i)
		if (pef->differences[i].str.data != 0) {
		    if (i != prev + 1)
			pprintd1(s, "\n%d", i);
		    pdf_put_name(pdev,
				 pef->differences[i].str.data,
				 pef->differences[i].str.size);
		    prev = i;
		}
	    pputs(s, "]");
	}
    }
    pputs(s, ">>\n");
    pdf_end_separate(pdev);
    if (pfd) {
	code = pdf_write_font_descriptor(pdev, pfd, Flags, pfname, FontFile_key);
	if (code < 0)
	    return code;
    }
    return 0;
}

/*
 * Write the FontFile* data for an embedded font.
 * Return a rangecheck error if the font can't be embedded.  */
private int
pdf_write_embedded_font(gx_device_pdf *pdev, pdf_font_descriptor_t *pfd,
			pdf_font_t *ppf, gs_font *font)
{
    byte fnchars[MAX_PDF_FONT_NAME];
    uint fnsize = font->font_name.size;
    gs_const_string font_name;
    bool do_subset = pdev->params.SubsetFonts && pdev->params.MaxSubsetPct > 0;
    long subset_id = pfd->FontFile_id;
    gs_glyph subset_glyphs[256];
    gs_glyph *glyph_subset = 0;
    uint subset_size = 0;

    /* Determine whether to subset the font. */
    if (do_subset) {
	int used, i, total, index;
	gs_glyph ignore_glyph;

	for (i = 0, used = 0; i < 256/8; ++i)
	    used += byte_count_bits[ppf->chars_used[i]];
	for (index = 0, total = 0;
	     (font->procs.enumerate_glyph(font, &index, GLYPH_SPACE_INDEX,
					  &ignore_glyph), index != 0);
	     )
	    ++total;
	if ((double)used / total >= pdev->params.MaxSubsetPct / 100.0)
	    do_subset = false;
	else {
	    subset_size = psdf_subset_glyphs(subset_glyphs, font,
					     ppf->chars_used);
	    glyph_subset = subset_glyphs;
	}
    }

    /* Generate an appropriate font name. */
    if (has_subset_prefix(font->font_name.chars, fnsize)) {
	/*
	 * This font is already a subset.  Replace the prefix with a newly
	 * generated one, to ensure no conflicts.
	 */
	memcpy(fnchars + 7, font->font_name.chars + 7, fnsize - 7);
	make_subset_prefix(fnchars, subset_id);
    } else if (do_subset) {
	memcpy(fnchars + 7, font->font_name.chars, fnsize);
	make_subset_prefix(fnchars, subset_id);
	fnsize += 7;
    } else
	memcpy(fnchars, font->font_name.chars, fnsize);    
    font_name.data = fnchars;
    font_name.size = fnsize;
    pdf_write_font_resource(pdev, ppf, &font_name);

    /* Finally, write the font (or subset). */
    switch (font->FontType) {
    case ft_encrypted:
	return pdf_embed_font_type1(pdev, pfd, (gs_font_type1 *)font,
				    glyph_subset, subset_size, &font_name);
    case ft_TrueType:
	return pdf_embed_font_type42(pdev, pfd, (gs_font_type42 *)font,
				     glyph_subset, subset_size, &font_name);
    default:
	return_error(gs_error_rangecheck);
    }
}

/* Register a font for eventual writing (embedded or not). */
private GS_NOTIFY_PROC(pdf_font_notify_proc);
typedef struct pdf_font_notify_s {
    gx_device_pdf *pdev;
    pdf_font_t *pdfont;
} pdf_font_notify_t;
gs_private_st_ptrs2(st_pdf_font_notify, pdf_font_notify_t, "pdf_font_notify_t",
		    pdf_font_notify_enum_ptrs, pdf_font_notify_reloc_ptrs,
		    pdev, pdfont);
int
pdf_register_font(gx_device_pdf *pdev, gs_font *font, pdf_font_t *ppf)
{
    /*
     * Note that we do this allocation in font->memory, not pdev->pdf_memory.
     */
    gs_memory_t *fn_memory = gs_memory_stable(font->memory);
    pdf_font_notify_t *pfn =
	gs_alloc_struct(fn_memory, pdf_font_notify_t,
			&st_pdf_font_notify, "pdf_register_font");

    if (pfn == 0)
	return_error(gs_error_VMerror);
    pfn->pdev = pdev;
    pfn->pdfont = ppf;
    if_debug3('_', "[_]register 0x%lx: pdf_font_t 0x%lx, gs_font 0x%lx\n",
	      (ulong)pfn, (ulong)ppf, (ulong)font);
    ppf->font = font;
    return gs_font_notify_register(font, pdf_font_notify_proc, pfn);
}
private int
pdf_font_notify_proc(void *vpfn /*proc_data*/, void *event_data)
{
    pdf_font_notify_t *const pfn = vpfn;
    gx_device_pdf *const pdev = pfn->pdev;
    pdf_font_t *const ppf = pfn->pdfont;
    gs_font *const font = ppf->font;
    gs_memory_t *fn_memory = gs_memory_stable(font->memory);
    gs_memory_t *save_memory = font->memory;
    int code;
    pdf_font_descriptor_t fdesc;

    if (event_data)
	return 0;		/* unknown event */
    if_debug4('_',
	      "[_]  notify 0x%lx: pdf_font_t 0x%lx, gs_font 0x%lx, skip=%d\n",
	      (ulong)pfn, (ulong)ppf, (ulong)font, ppf->skip);
    /*
     * HACK: temporarily patch the font's memory to one that we know is
     * available even during GC or restore.  (Eventually we need to fix
     * this by prohibiting implementations of font_info and glyph_info
     * from doing any allocations.)
     */
    font->memory = &gs_memory_default;
    code = pdf_compute_font_descriptor(pdev, &fdesc, font, NULL);
    gs_font_notify_unregister(font, pdf_font_notify_proc, vpfn);
    gs_free_object(fn_memory, vpfn, "pdf_font_notify_proc");
    ppf->font = 0;
    /*
     * Mark the font as written so that pdf_write_font_resources won't try
     * to write it a second time, but don't attempt to free it:
     * pdf_close_page will need to write it in the list of the page's used
     * resources.  pdf_close will take care of freeing the structure.
     */
    ppf->skip = true;
    if (code >= 0) {
	if (ppf->descriptor && ppf->descriptor->FontFile_id) {
	    /* This is an embedded font. */
	    gs_matrix save_mat;

	    save_mat = font->FontMatrix;
	    font->FontMatrix = ppf->orig_matrix;
	    fdesc.FontFile_id = ppf->descriptor->FontFile_id;
	    code = pdf_write_embedded_font(pdev, &fdesc, ppf, font);
	    font->FontMatrix = save_mat;
	} else {
	    gs_const_string font_name;

	    font_name.data = font->font_name.chars;
	    font_name.size = font->font_name.size;
	    pdf_write_font_resource(pdev, ppf, &font_name);
	}
    }
    font->memory = save_memory;
    return code;
}

/* Write out the font resources when wrapping up the output. */
int
pdf_write_font_resources(gx_device_pdf *pdev)
{
    int j;
    pdf_font_t *pef;
    pdf_font_t *next;

    for (j = 0; j < NUM_RESOURCE_CHAINS; ++j)
	for (pef = (pdf_font_t *)pdev->resources[resourceFont].chains[j];
	     pef != 0; pef = next
	     ) {
	    next = pef->next;	/* notify_proc unlinks the font */
	    if (PDF_FONT_IS_SYNTHESIZED(pef))
		pdf_write_synthesized_type3(pdev, pef);
	    else if (!pef->skip) {
		pdf_font_notify_t fn;

		fn.pdev = pdev;
		fn.pdfont = pef;
		pdf_font_notify_proc(&fn, NULL);
	    }
	}
    return 0;
}
