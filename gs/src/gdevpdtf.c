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
/* Font and CMap resource implementation for pdfwrite text */
#include "memory_.h"
#include "gx.h"
#include "gserrors.h"
#include "gsutil.h"		/* for bytes_compare */
#include "gxfcache.h"		/* for orig_fonts list */
#include "gxfont.h"
#include "gxfont1.h"
#include "gxfcid.h"
#include "gxfcmap.h"
#include "gdevpdfx.h"
#include "gdevpdtd.h"
#include "gdevpdtf.h"
#include "gdevpdtw.h"

/* GC descriptors */
public_st_pdf_font_resource();
private_st_pdf_encoding1();
private_st_pdf_encoding_element();
private_st_pdf_standard_font();
private_st_pdf_standard_font_element();
private_st_pdf_outline_fonts();

/* ---------------- Private ---------------- */

private
ENUM_PTRS_WITH(pdf_font_resource_enum_ptrs, pdf_font_resource_t *pdfont) return 0;
case 0: return ENUM_STRING(&pdfont->BaseFont);
case 1: ENUM_RETURN(pdfont->FontDescriptor);
case 2: ENUM_RETURN(pdfont->Widths);
case 3: ENUM_RETURN(pdfont->real_widths);
case 4: ENUM_RETURN(pdfont->ToUnicode);
case 5: switch (pdfont->FontType) {
 case ft_composite:
     ENUM_RETURN(pdfont->u.type0.DescendantFont);
 case ft_CID_encrypted:
 case ft_CID_TrueType:
     ENUM_RETURN(pdfont->u.cidfont.glyphshow_font);
 default:
     ENUM_RETURN(pdfont->u.simple.Encoding);
case 6: switch (pdfont->FontType) {
 case ft_composite:
     return (pdfont->u.type0.cmap_is_standard ? 0 :
	     ENUM_CONST_STRING(&pdfont->u.type0.CMapName));
 case ft_CID_TrueType:
     ENUM_RETURN(pdfont->u.cidfont.CIDToGIDMap);
 case ft_user_defined:
     ENUM_RETURN(pdfont->u.simple.s.type3.char_procs);
 default:
     return 0;
}
}
ENUM_PTRS_END
private
RELOC_PTRS_WITH(pdf_font_resource_reloc_ptrs, pdf_font_resource_t *pdfont)
{
    RELOC_STRING_VAR(pdfont->BaseFont);
    RELOC_VAR(pdfont->FontDescriptor);
    RELOC_VAR(pdfont->Widths);
    RELOC_VAR(pdfont->real_widths);
    RELOC_VAR(pdfont->ToUnicode);
    switch (pdfont->FontType) {
    case ft_composite:
	if (!pdfont->u.type0.cmap_is_standard)
	    RELOC_CONST_STRING_VAR(pdfont->u.type0.CMapName);
	RELOC_VAR(pdfont->u.type0.DescendantFont);
	break;
    case ft_CID_TrueType:
	RELOC_VAR(pdfont->u.cidfont.CIDToGIDMap);
	/* falls through */
    case ft_CID_encrypted:
	RELOC_VAR(pdfont->u.cidfont.glyphshow_font);
	break;
    default:
	RELOC_VAR(pdfont->u.simple.Encoding);
	/* falls through */
    case ft_user_defined:
	RELOC_VAR(pdfont->u.simple.s.type3.char_procs);
	break;
    }
}
RELOC_PTRS_END

/* Define the 14 standard built-in fonts. */
typedef struct pdf_standard_font_info_s {
    const char *fname;
    gs_encoding_index_t base_encoding;
} pdf_standard_font_info_t;
private const pdf_standard_font_info_t pdf_standard_font_info[] = {
#define m(name, enc) {name, enc},
    pdf_do_standard_fonts(m)
#undef m
    {0}
};

/*
 * Allocate and (minimally) initialize a font resource.
 */
private int
font_resource_alloc(gx_device_pdf *pdev, pdf_font_resource_t **ppfres,
		    pdf_resource_type_t rtype, gs_id rid, font_type ftype,
		    int chars_count,
		    pdf_font_write_contents_proc_t write_contents)
{
    gs_memory_t *mem = pdev->pdf_memory;
    pdf_font_resource_t *pfres;
    int *widths = 0;
    int *real_widths = 0;
    byte *used = 0;
    int code;

    if (chars_count != 0) {
	uint size = (chars_count + 7) / 8;

	widths = (void *)gs_alloc_byte_array(mem, chars_count, sizeof(int),
					     "font_resource_alloc(Widths)");
	real_widths =
	    (void *)gs_alloc_byte_array(mem, chars_count, sizeof(int),
					"font_resource_alloc(real_widths)");
	used = gs_alloc_bytes(mem, size, "font_resource_alloc(used)");
	if (widths == 0 || real_widths == 0 || used == 0) {
	    code = gs_note_error(gs_error_VMerror);
	    goto fail;
	}
	memset(used, 0, size);
    }
    code = pdf_alloc_resource(pdev, rtype, rid, (pdf_resource_t **)&pfres, 0L);
    if (code < 0)
	goto fail;
    memset((byte *)pfres + sizeof(pdf_resource_t), 0,
	   sizeof(*pfres) - sizeof(pdf_resource_t));
    pfres->FontType = ftype;
    pfres->count = chars_count;
    pfres->Widths = widths;
    pfres->real_widths = real_widths;
    pfres->used = used;
    pfres->write_contents = write_contents;
    *ppfres = pfres;
    return 0;
 fail:
    gs_free_object(mem, used, "font_resource_alloc(used)");
    gs_free_object(mem, real_widths, "font_resource_alloc(real_widths)");
    gs_free_object(mem, widths, "font_resource_alloc(Widths)");
    return code;
}
private int
font_resource_simple_alloc(gx_device_pdf *pdev, pdf_font_resource_t **ppfres,
			   gs_id rid, font_type ftype, int chars_count,
			   pdf_font_write_contents_proc_t write_contents)
{
    pdf_font_resource_t *pfres;
    int code = font_resource_alloc(pdev, &pfres, resourceFont, rid, ftype,
				   chars_count, write_contents);

    if (code < 0)
	return code;
    pfres->u.simple.LastChar = -1;
    pfres->u.simple.BaseEncoding = -1;
    *ppfres = pfres;
    return 0;
}

/*
 * Record whether a Type 1 or Type 2 font is a Multiple Master instance.
 */
private void
set_is_MM_instance(pdf_font_resource_t *pdfont, const gs_font_base *pfont)
{
    switch (pfont->FontType) {
    case ft_encrypted:
    case ft_encrypted2:
	pdfont->u.simple.s.type1.is_MM_instance =
	    ((const gs_font_type1 *)pfont)->data.WeightVector.count > 0;
    default:
	break;
    }
}

/* ---------------- Font resources ---------------- */

/*
 * Allocate and initialize bookkeeping for outline fonts.
 */
pdf_outline_fonts_t *
pdf_outline_fonts_alloc(gs_memory_t *mem)
{
    pdf_outline_fonts_t *pofs =
	gs_alloc_struct(mem, pdf_outline_fonts_t, &st_pdf_outline_fonts,
			"pdf_outline_fonts_alloc(outline_fonts)");
    pdf_standard_font_t *ppsf =
	gs_alloc_struct_array(mem, PDF_NUM_STANDARD_FONTS,
			      pdf_standard_font_t,
			      &st_pdf_standard_font_element,
			      "pdf_outline_fonts_alloc(standard_fonts)");

    if (pofs == 0 || ppsf == 0)
	return 0;
    memset(ppsf, 0, PDF_NUM_STANDARD_FONTS * sizeof(*ppsf));
    memset(pofs, 0, sizeof(*pofs));
    pofs->standard_fonts = ppsf;
    return pofs;
}

/* Get the object ID of a font resource. */
long
pdf_font_id(const pdf_font_resource_t *pdfont)
{
    return pdf_resource_id((const pdf_resource_t *)pdfont);
}

/*
 * Return the (copied, subset) font associated with a font resource.
 * If this font resource doesn't have a descriptor (Type 0, Type 3, or
 * base 14), return 0.
 */
gs_font_base *
pdf_font_resource_font(const pdf_font_resource_t *pdfont)
{
    if (pdfont->FontDescriptor == 0)
	return 0;
    return pdf_font_descriptor_font(pdfont->FontDescriptor);
}

/* Return the index of a standard font name, or -1 if missing. */
private int
pdf_find_standard_font(const byte *str, uint size)
{
    const pdf_standard_font_info_t *ppsf;

    for (ppsf = pdf_standard_font_info; ppsf->fname; ++ppsf)
	if (strlen(ppsf->fname) == size &&
	    !strncmp(ppsf->fname, (const char *)str, size)
	    )
	    return ppsf - pdf_standard_font_info;
    return -1;
}

/*
 * If there is a standard font with the same appearance (CharStrings,
 * Private, WeightVector) as the given font, set *psame to the mask of
 * identical properties, and return the standard-font index; otherwise,
 * set *psame to 0 and return -1.
 */
private int
find_std_appearance(const gx_device_pdf *pdev, const gs_font_base *bfont,
		    int mask, int *psame)
{
    bool has_uid = uid_is_UniqueID(&bfont->UID) && bfont->UID.id != 0;
    const pdf_standard_font_t *psf = pdev->text->outline_fonts->standard_fonts;
    int i;

    mask |= FONT_SAME_OUTLINES;
    for (i = 0; i < PDF_NUM_STANDARD_FONTS; ++psf, ++i) {
	if (has_uid) {
	    if (!uid_equal(&bfont->UID, &psf->uid))
		continue;
	    if (!psf->font) {
		/*
		 * Identity of UIDs is supposed to guarantee that the
		 * fonts have the same outlines and metrics.
		 */
		*psame = FONT_SAME_OUTLINES | FONT_SAME_METRICS;
		return i;
	    }
	}
	if (psf->font) {
	    int same = *psame =
		bfont->procs.same_font((const gs_font *)bfont,
				       (const gs_font *)psf->font,
				       mask);

	    if (same & FONT_SAME_OUTLINES)
		return i;
	}
    }
    *psame = 0;
    return -1;
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
	    pdf_standard_font_t *psf;

	    if (i >= 0 && (psf = &pdev->text->outline_fonts->standard_fonts[i])->font == 0) {
		psf->font = obfont;
		psf->orig_matrix = obfont->FontMatrix;
		psf->uid = obfont->UID;
		found = true;
	    }
	}
    }
    return found;
}

/*
 * Find the original (unscaled) standard font corresponding to an
 * arbitrary font, if any.  Return its index in standard_fonts, or -1.
 */
int
pdf_find_orig_font(gx_device_pdf *pdev, gs_font *font, gs_matrix *pfmat)
{
    bool scan = true;
    int i;

    if (font->FontType == ft_composite)
	return -1;
    for (;; font = font->base) {
	gs_font_base *bfont = (gs_font_base *)font;
	int same;

	/* Look for a standard font with the same appearance. */
	i = find_std_appearance(pdev, bfont, 0, &same);
	if (i >= 0)
	    break;
	if (scan) {
	    /* Scan for fonts with any of the standard names that */
	    /* have a UID. */
	    bool found = scan_for_standard_fonts(pdev, font->dir);

	    scan = false;
	    if (found) {
		i = find_std_appearance(pdev, bfont, 0, &same);
		if (i >= 0)
		    break;
	    }
	}
	if (font->base == font)
	    return -1;
    }
    *pfmat = pdev->text->outline_fonts->standard_fonts[i].orig_matrix;
    return i;
}

/*
 * Determine the embedding status of a font.  If the font is in the base
 * 14, store its index (0..13) in *pindex and its similarity to the base
 * font (as determined by the font's same_font procedure) in *psame.
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
private bool
embed_as_standard(gx_device_pdf *pdev, const gs_font *font, int index,
		  int *psame)
{
    if (font->is_resource) {
	*psame = ~0;
	return true;
    } else if (font->FontType != ft_composite &&
	       find_std_appearance(pdev, (const gs_font_base *)font, -1,
				   psame) == index)
	return true;
    return false;
}
pdf_font_embed_t
pdf_font_embed_status(gx_device_pdf *pdev, gs_font *font, int *pindex,
		      int *psame)
{
    const byte *chars = font->font_name.chars;
    uint size = font->font_name.size;
    int index = pdf_find_standard_font(chars, size);
    int discard_same;

    /*
     * The behavior of Acrobat Distiller changed between 3.0 (PDF 1.2),
     * which will never embed the base 14 fonts, and 4.0 (PDF 1.3), which
     * doesn't treat them any differently from any other fonts.  However,
     * if any of the base 14 fonts is not embedded, it still requires
     * special treatment.
     */
    if (pindex)
	*pindex = index;
    if (psame == 0)
	psame = &discard_same;
    *psame = 0;
    if (pdev->CompatibilityLevel < 1.3) {
	if (index >= 0 && embed_as_standard(pdev, font, index, psame))
	    return FONT_EMBED_STANDARD;
    }
    /* Check the Embed lists. */
    if (!embed_list_includes(&pdev->params.NeverEmbed, chars, size)) {
	if (pdev->params.EmbedAllFonts || font_is_symbolic(font) ||
	    embed_list_includes(&pdev->params.AlwaysEmbed, chars, size))
	    return FONT_EMBED_YES;
    }
    if (index >= 0 && embed_as_standard(pdev, font, index, psame))
	return FONT_EMBED_STANDARD;
    return FONT_EMBED_NO;
}

/*
 * Compute the BaseFont of a font according to the algorithm described
 * in gdevpdtf.h.
 */
int
pdf_compute_BaseFont(gx_device_pdf *pdev, pdf_font_resource_t *pdfont)
{
    pdf_font_resource_t *pdsubf = pdfont;
    gs_string fname;
    uint size, extra = 0;
    byte *data;

    if (pdfont->FontDescriptor == 0) {
	/* Type 3 font, or has its BaseFont computed in some other way. */
	return 0;
    }
    if (pdfont->FontType == ft_composite) {
	int code;

	pdsubf = pdfont->u.type0.DescendantFont;
	code = pdf_compute_BaseFont(pdev, pdsubf);
	if (code < 0)
	    return code;
	if (pdsubf->FontType == ft_CID_encrypted)
	    extra = 1 + pdfont->u.type0.CMapName.size;
    }
    fname = *pdf_font_descriptor_base_name(pdsubf->FontDescriptor);
    size = fname.size;
    data = gs_alloc_string(pdev->pdf_memory, size + extra,
			   "pdf_compute_BaseFont");
    if (data == 0)
	return_error(gs_error_VMerror);
    memcpy(data, fname.data, size);
    switch (pdfont->FontType) {
    case ft_composite:
	if (extra) {
	    data[size] = '-';
	    memcpy(data + size + 1, pdfont->u.type0.CMapName.data, extra - 1);
	}
	break;
    case ft_encrypted:
    case ft_encrypted2: {
	/****** if not embedded & MM: ******/
	/* Replace spaces by underscores in the base name. */
	uint i;

	for (i = 0; i < size; ++i)
	    if (data[i] == ' ')
		data[i] = '_';
	break;
    }
    case ft_TrueType:
    case ft_CID_TrueType: {
	/* Remove spaces from the base name. */
	uint i, j;

	for (i = j = 0; i < size; ++i)
	    if (data[i] != ' ')
		data[j++] = data[i];
	data = gs_resize_string(pdev->pdf_memory, data, i, j,
				"pdf_compute_BaseFont");
	size = j;
	break;
    }
    default:
	break;
    }
    pdfont->BaseFont.data = fname.data = data;
    pdfont->BaseFont.size = fname.size = size;
    *pdf_font_descriptor_name(pdsubf->FontDescriptor) = fname;
    return 0;
}

/* ------ Type 0 ------ */

/* Allocate a Type 0 font resource. */
int
pdf_font_type0_alloc(gx_device_pdf *pdev, pdf_font_resource_t **ppfres,
		     gs_id rid, pdf_font_resource_t *DescendantFont)
{
    int code = font_resource_alloc(pdev, ppfres, resourceFont, rid,
				   ft_composite, 0, pdf_write_contents_type0);

    if (code >= 0) {
	(*ppfres)->u.type0.DescendantFont = DescendantFont;
	code = pdf_compute_BaseFont(pdev, *ppfres);
    }
    return code;    
}

/* ------ Type 3 ------ */

/* Allocate a Type 3 font resource. */
int
pdf_font_type3_alloc(gx_device_pdf *pdev, pdf_font_resource_t **ppfres,
		     pdf_font_write_contents_proc_t write_contents)
{
    return font_resource_simple_alloc(pdev, ppfres, gs_no_id, ft_user_defined,
				      256, write_contents);
}

/* ------ Standard (base 14) Type 1 or TrueType ------ */

/* Allocate a standard (base 14) font resource. */
int
pdf_font_std_alloc(gx_device_pdf *pdev, pdf_font_resource_t **ppfres,
		   gs_id rid, gs_font_base *pfont)
{
    int code = font_resource_alloc(pdev, ppfres, resourceFont, rid,
				   pfont->FontType, 0, pdf_write_contents_std);

    if (code < 0)
	return code;
    set_is_MM_instance(*ppfres, pfont);
    /****** SET BaseFont ******/
    return 0;
}

/* ------ Other Type 1 or TrueType ------ */

/* Allocate a Type 1 or TrueType font resource. */
int
pdf_font_simple_alloc(gx_device_pdf *pdev, pdf_font_resource_t **ppfres,
		      gs_id rid, pdf_font_descriptor_t *pfd)
{
    pdf_encoding_element_t *Encoding =
	gs_alloc_struct_array(pdev->pdf_memory, 256, pdf_encoding_element_t,
			      &st_pdf_encoding_element,
			      "pdf_font_simple_alloc(Encoding)");
    pdf_font_resource_t *pdfont;
    int i, code;

    if (Encoding == 0)
	return_error(gs_error_VMerror);
    code = font_resource_simple_alloc(pdev, &pdfont, rid,
				      pdf_font_descriptor_FontType(pfd),
				      256, pdf_write_contents_simple);

    if (code < 0) {
	gs_free_object(pdev->pdf_memory, Encoding,
		       "pdf_font_simple_alloc(Encoding)");
	return code;
    }
    pdfont->FontDescriptor = pfd;
    memset(Encoding, 0, 256 * sizeof(*Encoding));
    for (i = 0; i < 256; ++i)
	Encoding[i].glyph = GS_NO_GLYPH;
    pdfont->u.simple.Encoding = Encoding;
    set_is_MM_instance(pdfont, pdf_font_descriptor_font(pfd));
    *ppfres = pdfont;
    return pdf_compute_BaseFont(pdev, pdfont);
}

/* ------ CID-keyed ------ */

/* Allocate a CIDFont resource. */
int
pdf_font_cidfont_alloc(gx_device_pdf *pdev, pdf_font_resource_t **ppfres,
		       gs_id rid, pdf_font_descriptor_t *pfd)
{
    font_type FontType = pdf_font_descriptor_FontType(pfd);
    gs_font_base *font = pdf_font_descriptor_font(pfd);
    int chars_count;
    int code;
    pdf_font_write_contents_proc_t write_contents;
    const gs_cid_system_info_t *pcidsi;
    ushort *map = 0;
    pdf_font_resource_t *pdfont;

    switch (FontType) {
    case ft_CID_encrypted:
	chars_count = ((const gs_font_cid0 *)font)->cidata.common.CIDCount;
	pcidsi = &((const gs_font_cid0 *)font)->cidata.common.CIDSystemInfo;
	write_contents = pdf_write_contents_cid0;
	break;
    case ft_CID_TrueType:
	chars_count = ((const gs_font_cid2 *)font)->cidata.common.CIDCount;
	pcidsi = &((const gs_font_cid2 *)font)->cidata.common.CIDSystemInfo;
	map = (void *)gs_alloc_byte_array(pdev->pdf_memory, chars_count,
					  sizeof(*map), "CIDToGIDMap");
	if (map == 0)
	    return_error(gs_error_VMerror);
	memset(map, 0, chars_count * sizeof(*map));
	write_contents = pdf_write_contents_cid2;
	break;
    default:
	return_error(gs_error_rangecheck);
    }
    code = font_resource_alloc(pdev, &pdfont, resourceCIDFont, rid, FontType,
			       chars_count, write_contents);
    if (code < 0)
	return code;
    pdfont->FontDescriptor = pfd;
    pdfont->u.cidfont.CIDToGIDMap = map;
    /*
     * Write the CIDSystemInfo now, so we don't try to access it after
     * the font may no longer be available.
     */
    {
	long cidsi_id = pdf_begin_separate(pdev);

	pdf_write_cid_system_info(pdev, pcidsi);
	pdf_end_separate(pdev);
	pdfont->u.cidfont.CIDSystemInfo_id = cidsi_id;
    }
    *ppfres = pdfont;
    return pdf_compute_BaseFont(pdev, pdfont);
}

/* ---------------- CMap resources ---------------- */

/*
 * Allocate a CMap resource.
 */
int
pdf_cmap_alloc(gx_device_pdf *pdev, const gs_cmap_t *pcmap,
	       pdf_resource_t **ppres)
{
    /*
     * We don't store any of the contents of the CMap: instead, we write
     * it out immediately and just save the id.  Since some CMaps are very
     * large, we should wait, and only write the entries actually used.
     * This is a project for some future date....
     */
    int code = pdf_alloc_resource(pdev, resourceCMap, pcmap->id, ppres, 0L);

    if (code < 0)
	return code;
    return pdf_write_cmap(pdev, pcmap, *ppres);
}
