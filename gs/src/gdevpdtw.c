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
/* Font resource writing for pdfwrite text */
#include "memory_.h"
#include "gx.h"
#include "gxfcmap.h"
#include "gxfont.h"
#include "gdevpsf.h"
#include "gdevpdfx.h"
#include "gdevpdfo.h"		/* for object->written */
#include "gdevpdtd.h"		/* for writing FontDescriptor */
#include "gdevpdtf.h"
#include "gdevpdti.h"		/* for writing bitmap fonts Encoding */
#include "gdevpdtw.h"

/* ================ Font resource writing ================ */

/* ---------------- Private ---------------- */

/* Write the Widths for a font. */
private int
pdf_write_Widths(gx_device_pdf *pdev, int first, int last, const double *widths)
{
    stream *s = pdev->strm;
    int i;

    if (first > last)
	first = last = 0;
    pprintd2(s, "/FirstChar %d/LastChar %d/Widths[", first, last);
    for (i = first; i <= last; ++i)
	pprintd1(s, (i & 15 ? " %d" : "\n%d"), (int)(widths[i] + 0.5));
    stream_puts(s, "]\n");
    return 0;
}

/* Check strings equality. */
private bool 
strings_equal(const gs_const_string *str0, const gs_const_string *str1)
{
    return str0->size == str1->size &&
	   !memcmp(str0->data, str1->data, str0->size);
}

/* Check if an encoding element differs from a standard one. */
private int
pdf_different_encoding_element(const pdf_font_resource_t *pdfont, int ch, int encoding_index)
{
    if (pdfont->u.simple.Encoding[ch].is_difference)
	return 1;
    else if (encoding_index >= 0) {
	gs_glyph glyph0 = gs_c_known_encode(ch, encoding_index);
	gs_glyph glyph1 = pdfont->u.simple.Encoding[ch].glyph;
	gs_const_string str;
	int code = gs_c_glyph_name(glyph0, &str);

	if (code < 0)
	    return code; /* Must not happen */
	if (glyph1 != GS_NO_GLYPH)
	    if (!strings_equal(&str, &pdfont->u.simple.Encoding[ch].str))
		return 1;
    }
    return 0;
}

/* Write the Subtype and Encoding for a simple font. */
private int
pdf_write_simple_contents(gx_device_pdf *pdev,
			  const pdf_font_resource_t *pdfont)
{
    stream *s = pdev->strm;
    static const char *const encoding_names[] = {
	KNOWN_REAL_ENCODING_NAMES
    };
    gs_encoding_index_t base_encoding = pdfont->u.simple.BaseEncoding;
    long diff_id = 0;
    int ch = (pdfont->u.simple.Encoding ? 0 : 256);
    int code = 0;

    for (; ch < 256; ++ch) {
	code = pdf_different_encoding_element(pdfont, ch, base_encoding);
	if (code < 0)
	    return code; /* Must not happen */
	if (code)
	    break;
    }
    if (ch < 256)
	pprintld1(s, "/Encoding %ld 0 R",
		  (diff_id = pdf_obj_ref(pdev)));
    else if (base_encoding > 0)
	pprints1(s, "/Encoding/%s", encoding_names[base_encoding]);
    pprints1(s, "/Subtype/%s>>\n",
	     (pdfont->FontType == ft_TrueType ? "TrueType" :
	      pdfont->u.simple.s.type1.is_MM_instance ? "MMType1" : "Type1"));
    pdf_end_separate(pdev);
    if (diff_id) {
	int prev = 256;

	pdf_open_separate(pdev, diff_id);
	s = pdev->strm;
	stream_puts(s, "<</Type/Encoding");
	if (base_encoding > 0)
	    pprints1(s, "/BaseEncoding/%s", encoding_names[base_encoding]);
	stream_puts(s, "/Differences[");
	for (; ch < 256; ++ch) {
	    code = pdf_different_encoding_element(pdfont, ch, base_encoding);
	    if (code < 0)
		return code; /* Must not happen */
	    if (code) {
		if (ch != prev + 1)
		    pprintd1(s, "\n%d", ch);
		pdf_put_name(pdev, pdfont->u.simple.Encoding[ch].str.data,
			     pdfont->u.simple.Encoding[ch].str.size);
		prev = ch;
	    }
	}
	stream_puts(s, "]>>\n");
	pdf_end_separate(pdev);
    }
    return 0;
}

/*
 * Write the [D]W[2] entries for a CIDFont.  *pdfont is known to be a
 * CIDFont (type 0 or 2).
 */
private int
pdf_write_CIDFont_widths(gx_device_pdf *pdev,
			 const pdf_font_resource_t *pdfont)
{
    /*
     * The values of the CIDFont width keys are as follows:
     *   DW = w (default 0)
     *   W = [{c [w ...] | cfirst clast w}*]
     *   DW2 = [vy w1y] (default [880 -1000])
     *   W2 = [{c [w1y vx vy ...] | cfirst clast w1y vx vy}*]
     * Currently we only write DW and W.
     */
    stream *s = pdev->strm;
    psf_glyph_enum_t genum;
    gs_glyph glyph;
    int dw = 0;

    psf_enumerate_bits_begin(&genum, NULL, pdfont->used, pdfont->count,
			     GLYPH_SPACE_INDEX);

    /* Use the most common width as DW. */

    {
	ushort counts[1001];
	int dw_count = 0, i;

	memset(counts, 0, sizeof(counts));
	while (!psf_enumerate_glyphs_next(&genum, &glyph)) {
	    int width = (int)(pdfont->Widths[glyph - GS_MIN_CID_GLYPH] + 0.5);

	    counts[min(width, countof(counts) - 1)]++;
	}
	for (i = 0; i < countof(counts); ++i)
	    if (counts[i] > dw_count)
		dw = i, dw_count = counts[i];
	if (dw != 0)
	    pprintd1(s, "/DW %d\n", dw);
    }

    /*
     * Now write all widths different from DW.  Currently we make no
     * attempt to optimize this: we write every width individually.
     */

    psf_enumerate_glyphs_reset(&genum);
    {
	int prev = -2;

	while (!psf_enumerate_glyphs_next(&genum, &glyph)) {
	    int cid = glyph - GS_MIN_CID_GLYPH;
	    int width = (int)(pdfont->Widths[cid] + 0.5);

	    if (cid == prev + 1)
		pprintd1(s, "\n%d", width);
	    else if (width == dw)
		continue;
	    else {
		if (prev >= 0)
		    stream_puts(s, "]\n");
		else
		    stream_puts(s, "/W[");
		pprintd2(s, "%d[%d", cid, width);
	    }
	    prev = cid;
	}
	if (prev >= 0)
	    stream_puts(s, "]]");
    }    

    return 0;
}

/* ---------------- Specific FontTypes ---------------- */

/* Write the contents of a Type 0 font resource. */
int
pdf_write_contents_type0(gx_device_pdf *pdev, pdf_font_resource_t *pdfont)
{
    stream *s = pdev->strm;

    /*
     * The Encoding name might be missing if an error occurred when
     * creating the font resource.
     */
    if (pdfont->u.type0.Encoding_name[0])
	pprints1(s, "/Encoding %s", pdfont->u.type0.Encoding_name);
    pprintld1(s, "/DescendantFonts[%ld 0 R]",
	      pdf_font_id(pdfont->u.type0.DescendantFont));
    stream_puts(s, "/Subtype/Type0>>\n");
    pdf_end_separate(pdev);
    return 0;
}

/*
 * Finish writing the contents of a Type 3 font resource (FontBBox, Widths,
 * Subtype).
 */
int
pdf_finish_write_contents_type3(gx_device_pdf *pdev,
				pdf_font_resource_t *pdfont)
{
    stream *s = pdev->strm;

    pdf_write_font_bbox(pdev, &pdfont->u.simple.s.type3.FontBBox);
    pdf_write_Widths(pdev, pdfont->u.simple.FirstChar, 
		    pdfont->u.simple.LastChar, pdfont->Widths);
    stream_puts(s, "/Subtype/Type3>>\n");
    pdf_end_separate(pdev);
    return 0;
}

/* Write the contents of a standard (base 14) font resource. */
int
pdf_write_contents_std(gx_device_pdf *pdev, pdf_font_resource_t *pdfont)
{
    return pdf_write_simple_contents(pdev, pdfont);
}

/* Write the contents of a simple (Type 1 or Type 42) font resource. */
int
pdf_write_contents_simple(gx_device_pdf *pdev, pdf_font_resource_t *pdfont)
{
    pdf_write_Widths(pdev, pdfont->u.simple.FirstChar,
		     pdfont->u.simple.LastChar, pdfont->Widths);
    return pdf_write_simple_contents(pdev, pdfont);
}

/* Write the contents of a CIDFont resource. */
private int
write_contents_cid_common(gx_device_pdf *pdev, pdf_font_resource_t *pdfont,
			  int subtype)
{
    /* Write [D]W[2], CIDSystemInfo, and Subtype, and close the object. */
    stream *s = pdev->strm;
    int code = pdf_write_CIDFont_widths(pdev, pdfont);

    if (code < 0)
	return code;
    if (pdfont->u.cidfont.CIDSystemInfo_id)
	pprintld1(s, "/CIDSystemInfo %ld 0 R",
		  pdfont->u.cidfont.CIDSystemInfo_id);
    pprintd1(s, "/Subtype/CIDFontType%d>>\n", subtype);
    pdf_end_separate(pdev);
    return 0;
}
int
pdf_write_contents_cid0(gx_device_pdf *pdev, pdf_font_resource_t *pdfont)
{
    return write_contents_cid_common(pdev, pdfont, 0);
}
int
pdf_write_contents_cid2(gx_device_pdf *pdev, pdf_font_resource_t *pdfont)
{
    int count = pdfont->count;
    long map_id = 0;
    psf_glyph_enum_t genum;
    gs_glyph glyph;
    int code;

    /* Check for the identity CIDMap. */
    psf_enumerate_bits_begin(&genum, NULL, pdfont->used, count,
			     GLYPH_SPACE_INDEX);
    while (!psf_enumerate_glyphs_next(&genum, &glyph)) {
	int cid = glyph - GS_MIN_CID_GLYPH;
	int gid = pdfont->u.cidfont.CIDToGIDMap[cid];

	if (gid != cid) {	/* non-identity map */
	    map_id = pdf_obj_ref(pdev);
	    pprintld1(pdev->strm, "/CIDToGIDMap %ld 0 R\n", map_id);
	    break;
	}
    }

    code = write_contents_cid_common(pdev, pdfont, 2);
    if (code < 0)
	return code;

    if (map_id) {
	pdf_data_writer_t writer;
	int i;

	pdf_open_separate(pdev, map_id);
	stream_puts(pdev->strm, "<<");
	pdf_begin_data(pdev, &writer);
	for (i = 0; i < count; ++i) {
	    uint gid = pdfont->u.cidfont.CIDToGIDMap[i];

	    stream_putc(writer.binary.strm, (byte)(gid >> 8));
	    stream_putc(writer.binary.strm, (byte)(gid));
	}
	code = pdf_end_data(&writer);
    }
    return code;
}

/* ---------------- External entries ---------------- */

/* Write a font resource. */
private int
pdf_write_font_resource(gx_device_pdf *pdev, pdf_font_resource_t *pdfont)
{
    stream *s;

    pdf_open_separate(pdev, pdf_font_id(pdfont));
    s = pdev->strm;
    stream_puts(s, "<<");
    if (pdfont->BaseFont.size > 0) {
	stream_puts(s, "/BaseFont");
	pdf_put_name(pdev, pdfont->BaseFont.data, pdfont->BaseFont.size);
    }
    if (pdfont->FontDescriptor)
	pprintld1(s, "/FontDescriptor %ld 0 R",
		  pdf_font_descriptor_id(pdfont->FontDescriptor));
    if (pdfont->ToUnicode)
	pprintld1(s, "/ToUnicode %ld 0 R",
		  pdf_resource_id((const pdf_resource_t *)pdfont->ToUnicode));
    if (pdev->CompatibilityLevel > 1.0)
	stream_puts(s, "/Type/Font\n");
    else
	pprintld1(s, "/Type/Font/Name/R%ld\n", pdf_font_id(pdfont));
    return pdfont->write_contents(pdev, pdfont);
}

/*
 * Close the text-related parts of a document, including writing out font
 * and related resources.
 */
private int
write_font_resources(gx_device_pdf *pdev, pdf_resource_list_t *prlist)
{
    int j;
    pdf_resource_t *pres;

    for (j = 0; j < NUM_RESOURCE_CHAINS; ++j)
	for (pres = prlist->chains[j]; pres != 0; pres = pres->next) {
	    pdf_font_resource_t *const pdfont = (pdf_font_resource_t *)pres;
	    int code = pdf_compute_BaseFont(pdev, pdfont);

	    if (code < 0)
		return code;
	    pdf_write_font_resource(pdev, pdfont);
	}
    return 0;
}
private int
finish_font_descriptors(gx_device_pdf *pdev,
			int (*finish_proc)(gx_device_pdf *,
					   pdf_font_descriptor_t *))
{
    int j;
    pdf_resource_t *pres;

    for (j = 0; j < NUM_RESOURCE_CHAINS; ++j)
	for (pres = pdev->resources[resourceFontDescriptor].chains[j];
	     pres != 0; pres = pres->next
	     ) {
	    int code = finish_proc(pdev, (pdf_font_descriptor_t *)pres);

	    if (code < 0)
		return code;
	}
    return 0;
}
int
pdf_close_text_document(gx_device_pdf *pdev)
{
    int code;

    /*
     * Finish the descriptors and write any embedded fonts, but don't
     * write the descriptors yet; then write the fonts; finally write
     * the descriptors.
     */

    if ((code = finish_font_descriptors(pdev, pdf_finish_FontDescriptor)) < 0 ||
	(code = write_font_resources(pdev, &pdev->resources[resourceCIDFont])) < 0 ||
	(code = write_font_resources(pdev, &pdev->resources[resourceFont])) < 0 ||
	(code = finish_font_descriptors(pdev, pdf_write_FontDescriptor)) < 0
	)
	return code;

    /* If required, write the Encoding for Type 3 bitmap fonts. */

    return pdf_write_bitmap_fonts_Encoding(pdev);
}

/* ================ CMap resource writing ================ */

/*
 * Write the CIDSystemInfo for a CIDFont or a CMap.
 */
int
pdf_write_cid_system_info(gx_device_pdf *pdev,
			  const gs_cid_system_info_t *pcidsi)
{
    stream *s = pdev->strm;

    stream_puts(s, "<<\n/Registry");
    s_write_ps_string(s, pcidsi->Registry.data, pcidsi->Registry.size,
		      PRINT_HEX_NOT_OK);
    stream_puts(s, "\n/Ordering");
    s_write_ps_string(s, pcidsi->Ordering.data, pcidsi->Ordering.size,
		      PRINT_HEX_NOT_OK);
    pprintd1(s, "\n/Supplement %d\n>>\n", pcidsi->Supplement);
    return 0;
}

/*
 * Write a CMap resource.  We pass the CMap object as well as the resource,
 * because we write CMaps when they are created.
 */
int
pdf_write_cmap(gx_device_pdf *pdev, const gs_cmap_t *pcmap,
	       pdf_resource_t *pres /*CMap*/)
{
    pdf_data_writer_t writer;
    stream *s;
    int code;

    if (pres->object->written)
	return 0;
    pdf_open_separate(pdev, pres->object->id);
    s = pdev->strm;
    pprintd1(s, "<</WMode %d/CMapName", pcmap->WMode);
    pdf_put_name(pdev, pcmap->CMapName.data, pcmap->CMapName.size);
    stream_puts(s, "/CIDSystemInfo");
    code = pdf_write_cid_system_info(pdev, pcmap->CIDSystemInfo);
    if (code < 0)
	return code;
    code = pdf_begin_data_stream(pdev, &writer,
				 DATA_STREAM_NOT_BINARY |
				 (pdev->CompressFonts ?
				  DATA_STREAM_COMPRESS : 0));
    if (code < 0)
	return code;
    code = psf_write_cmap(writer.binary.strm, pcmap,
			  pdf_put_name_chars_proc(pdev), NULL);
    if (code < 0)
	return code;
    code = pdf_end_data(&writer);
    if (code < 0)
	return code;
    pres->object->written = true;
    return 0;
}
