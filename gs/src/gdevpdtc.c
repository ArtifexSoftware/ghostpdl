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
/* Composite and CID-based text processing for pdfwrite. */
#include "memory_.h"
#include "gx.h"
#include "gserrors.h"
#include "gxfcmap.h"
#include "gxfont.h"
#include "gxfont0.h"
#include "gxfont0c.h"
#include "gzpath.h"
#include "gxchar.h"
#include "gdevpsf.h"
#include "gdevpdfx.h"
#include "gdevpdtx.h"
#include "gdevpdtd.h"
#include "gdevpdtf.h"
#include "gdevpdts.h"
#include "gdevpdtt.h"

/* ---------------- Non-CMap-based composite font ---------------- */

/*
 * Process a text string in a composite font with FMapType != 9 (CMap).
 */
int
process_composite_text(gs_text_enum_t *pte, void *vbuf, uint bsize)
{
    byte *const buf = vbuf;
    pdf_text_enum_t *const penum = (pdf_text_enum_t *)pte;
    int code = 0;
    gs_string str;
    pdf_text_process_state_t text_state;
    pdf_text_enum_t curr, prev, out;
    gs_point total_width;
    const gs_matrix *psmat = 0;
    gs_font *prev_font = 0;
    gs_char chr, char_code, space_char = ~0;
    int buf_index = 0;
    bool return_width = (penum->text.operation & TEXT_RETURN_WIDTH);

    str.data = buf;
    if (return_width) {
	code = gx_path_current_point(penum->path, &penum->origin);
	if (code < 0)
	    return code;
    }
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
    out = curr;
    out.current_font = 0;
    /* Scan runs of characters in the same leaf font. */
    for ( ; ; ) {
	int font_code;
	gs_font *new_font = 0;

	gs_text_enum_copy_dynamic((gs_text_enum_t *)&out,
				  (gs_text_enum_t *)&curr, false);
	for (;;) {
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
		curr.returned.current_char = chr;
		char_code = gx_current_char((gs_text_enum_t *)&curr);
		new_font = curr.fstack.items[curr.fstack.depth].font;
		if (new_font != prev_font)
		    break;
		if (chr != (byte)chr)	/* probably can't happen */
		    return_error(gs_error_rangecheck);
		if (buf_index >= bsize)
		    return_error(gs_error_unregistered); /* Must not happen. */
		buf[buf_index] = (byte)chr;
		buf_index++;
		prev_font = new_font;
		psmat = &curr.fstack.items[curr.fstack.depth - 1].font->FontMatrix;
		if (pte->text.space.s_char == char_code)
		    space_char = chr;
		continue;
	    case 2:		/* end of string */
		break;
	    default:	/* error */
		return font_code;
	    }
	    break;
	}
	str.size = buf_index;
	if (buf_index) {
	    /* buf_index == 0 is only possible the very first time. */
	    /*
	     * The FontMatrix of leaf descendant fonts is not updated
	     * by scalefont.  Compute the effective FontMatrix now.
	     */
	    gs_matrix fmat;

	    /* set up the base font : */
	    out.fstack.depth = 0;
	    out.fstack.items[out.fstack.depth].font = out.current_font = prev_font;

	    /* Provide the decoded space character : */
	    out.text.space.s_char = space_char;

	    gs_matrix_multiply(&prev_font->FontMatrix, psmat, &fmat);
	    code = pdf_encode_process_string(&out, &str, &fmat, &text_state);
	    if (code < 0)
		return code;
	    curr.xy_index = out.xy_index; /* pdf_encode_process_string advanced it. */
	    gs_text_enum_copy_dynamic(pte, (gs_text_enum_t *)&prev, true);
	    if (return_width) {
		pte->returned.total_width.x = total_width.x +=
		    out.returned.total_width.x;
		pte->returned.total_width.y = total_width.y +=
		    out.returned.total_width.y;
	    }
	}
	if (font_code == 2)
	    break;
	buf[0] = (byte)chr;
	buf_index = 1;
	space_char = (pte->text.space.s_char == char_code ? chr : ~0);
	psmat = &curr.fstack.items[curr.fstack.depth - 1].font->FontMatrix;
	prev_font = new_font;
    }
    if (!return_width)
	return 0;
    return gx_path_add_point(penum->path,
			     penum->origin.x + float2fixed(total_width.x),
			     penum->origin.y + float2fixed(total_width.y));
}

/* ---------------- CMap-based composite font ---------------- */

/*
 * Process a text string in a composite font with FMapType == 9 (CMap).
 */
private const char *const standard_cmap_names[] = {
    /* The following were added in PDF 1.4. */
    "GBKp-EUC-H", "GBKp-EUC-V",
    "GBK2K-H", "GBK2K-V",
    "HKscs-B5-H", "HKscs-B5-V",
#define END_PDF14_CMAP_NAMES_INDEX 6

    "Identity-H", "Identity-V",

    "GB-EUC-H", "GB-EUC-V",
    "GBpc-EUC-H", "GBpc-EUC-V",
    "GBK-EUC-H", "GBK-EUC-V",
    "UniGB-UCS2-H", "UniGB-UCS2-V",

    "B5pc-H", "B5pc-V",
    "ETen-B5-H", "ETen-B5-V",
    "ETenms-B5-H", "ETenms-B5-V",
    "CNS-EUC-H", "CNS-EUC-V",
    "UniCNS-UCS2-H", "UniCNS-UCS2-V",

    "83pv-RKSJ-H",
    "90ms-RKSJ-H", "90ms-RKSJ-V",
    "90msp-RKSJ-H", "90msp-RKSJ-V",
    "90pv-RKSJ-H",
    "Add-RKSJ-H", "Add-RKSJ-V",
    "EUC-H", "EUC-V",
    "Ext-RKSJ-H", "Ext-RKSJ-V",
    "H", "V",
    "UniJIS-UCS2-H", "UniJIS-UCS2-V",
    "UniJIS-UCS2-HW-H", "UniJIS-UCS2-HW-V",

    "KSC-EUC-H", "KSC-EUC-V",
    "KSCms-UHC-H", "KSCms-UHC-V",
    "KSCms-UHC-HW-H", "KSCms-UHC-HW-V",
    "KSCpc-EUC-H",
    "UniKS-UCS2-H", "UniKS-UCS2-V",

    0
};

private int
attach_cmap_resource(gx_device_pdf *pdev, pdf_font_resource_t *pdfont, 
		const gs_cmap_t *pcmap, int font_index_only)
{
    const char *const *pcmn =
	standard_cmap_names +
	(pdev->CompatibilityLevel < 1.4 ? END_PDF14_CMAP_NAMES_INDEX : 0);
    bool is_identity = false;
    pdf_resource_t *pcmres = 0;	/* CMap */
    int code;

    /*
     * If the CMap isn't standard, write it out if necessary.
     */
    for (; *pcmn != 0; ++pcmn)
	if (pcmap->CMapName.size == strlen(*pcmn) &&
	    !memcmp(*pcmn, pcmap->CMapName.data, pcmap->CMapName.size))
	    break;
    if (*pcmn == 0) {
	/* 
	 * PScript5.dll Version 5.2 creates identity CMaps with
	 * instandard name. Check this specially here
	 * and later replace with a standard name.
	 * This is a temporary fix for SF bug #615994 "CMAP is corrupt".
	 */
	is_identity = gs_cmap_is_identity(pcmap, font_index_only);
    }
    if (*pcmn == 0 && !is_identity) {		/* not standard */
	pcmres = pdf_find_resource_by_gs_id(pdev, resourceCMap, pcmap->id + font_index_only);
	if (pcmres == 0) {
	    /* Create and write the CMap object. */
	    code = pdf_cmap_alloc(pdev, pcmap, &pcmres, font_index_only);
	    if (code < 0)
		return code;
	}
    }
    if (pcmap->from_Unicode) {
	gs_cmap_ranges_enum_t renum;

	gs_cmap_ranges_enum_init(pcmap, &renum);
	if (gs_cmap_enum_next_range(&renum) == 0 && renum.range.size == 2 &&
	    gs_cmap_enum_next_range(&renum) == 1) {
	    /*
	     * Exactly one code space range, of size 2.  Add an identity
	     * ToUnicode CMap.
	     */
	    if (!pdev->Identity_ToUnicode_CMaps[pcmap->WMode]) {
		/* Create and write an identity ToUnicode CMap now. */
		gs_cmap_t *pidcmap;

		code = gs_cmap_create_char_identity(&pidcmap, 2, pcmap->WMode,
						    pdev->memory);
		if (code < 0)
		    return code;
		pidcmap->CMapType = 2;	/* per PDF Reference */
		code = pdf_cmap_alloc(pdev, pidcmap,
				&pdev->Identity_ToUnicode_CMaps[pcmap->WMode], -1);
		if (code < 0)
		    return code;
	    }
	    pdfont->res_ToUnicode = pdev->Identity_ToUnicode_CMaps[pcmap->WMode];
	}
    }
    if (pcmres || is_identity) {
	uint size = pcmap->CMapName.size;
	byte *chars = gs_alloc_string(pdev->pdf_memory, size,
				      "pdf_font_resource_t(CMapName)");

	if (chars == 0)
	    return_error(gs_error_VMerror);
	memcpy(chars, pcmap->CMapName.data, size);
	if (is_identity)
	    strcpy(pdfont->u.type0.Encoding_name, 
		    (pcmap->WMode ? "/Identity-V" : "/Identity-H"));
	else
	    sprintf(pdfont->u.type0.Encoding_name, "%ld 0 R",
		    pdf_resource_id(pcmres));
	pdfont->u.type0.CMapName.data = chars;
	pdfont->u.type0.CMapName.size = size;
    } else {
	sprintf(pdfont->u.type0.Encoding_name, "/%s", *pcmn);
	pdfont->u.type0.CMapName.data = (const byte *)*pcmn;
	pdfont->u.type0.CMapName.size = strlen(*pcmn);
	pdfont->u.type0.cmap_is_standard = true;
    }
    pdfont->u.type0.WMode = pcmap->WMode;
    return 0;
}

/* Record widths and CID => GID mappings. */
private int
scan_cmap_text(pdf_text_enum_t *pte)
{
    gx_device_pdf *pdev = (gx_device_pdf *)pte->dev;
    gs_font_type0 *const font = (gs_font_type0 *)pte->current_font; /* Type 0, fmap_CMap */
    gs_text_enum_t scan = *(gs_text_enum_t *)pte;
    int wmode = font->WMode, code;
    gs_font *subfont = NULL;
    bool done = false;

    pte->returned.total_width.x = pte->returned.total_width.y = 0;;
    do {
	uint index = scan.index;
	gs_const_string str;
	pdf_text_process_state_t text_state;
	pdf_font_resource_t *pdfont;
	gs_point wxy;

	code = gx_path_current_point(pte->path, &pte->origin);
	if (code < 0)
	    return code;
        for ( ; ; ) {
	    gs_char chr;
	    gs_glyph glyph;
	    pdf_font_resource_t *pdsubf;
	    pdf_font_descriptor_t *pfd;
	    byte *glyph_usage;
	    double *real_widths, *w, *v;
	    int char_cache_size, width_cache_size;
	    uint cid;

	    code = font->procs.next_char_glyph(&scan, &chr, &glyph);
	    if (code == 2) {		/* end of string */
		done = true;
		break;
	    }
	    if (code < 0)
		return code;
	    if (subfont != NULL && subfont != scan.fstack.items[scan.fstack.depth].font)
		break;
	    subfont = scan.fstack.items[scan.fstack.depth].font;
	    switch (subfont->FontType) {
	    case ft_CID_encrypted:
	    case ft_CID_TrueType:
		break;
	    default:
		/* An unsupported case, fall back to default implementation. */
		return_error(gs_error_rangecheck);
	    }
	    if (glyph == GS_NO_GLYPH)
		glyph = GS_MIN_CID_GLYPH;
	    cid = glyph - GS_MIN_CID_GLYPH;
	    code = pdf_obtain_cidfont_resource(pdev, subfont, &pdsubf, &glyph, 1);
	    if (code < 0)
		return code;
	    code = pdf_attached_font_resource(pdev, (gs_font *)subfont, &pdsubf, 
				       &glyph_usage, &real_widths, &char_cache_size, &width_cache_size);
	    if (code < 0)
		return code;
	    code = pdf_obtain_parent_type0_font_resource(pdev, pdsubf, &pdfont);
	    if (code < 0)
		return code;
	    if (!pdfont->u.type0.Encoding_name[0]) {
		/*
		 * If pdfont->u.type0.Encoding_name is set, 
		 * a CMap resource is already attached.
		 * See attach_cmap_resource.
		 */
		code = attach_cmap_resource(pdev, pdfont, font->data.CMap, 
			scan.fstack.items[scan.fstack.depth].index);
		if (code < 0)
		    return code;
	    }
	    pfd = pdsubf->FontDescriptor;
	    code = pdf_obtain_cidfont_widths_arrays(pdev, pdsubf, wmode, &w, &v);
	    if (code < 0)
		return code;
	    /* We can't check pdsubf->used[cid >> 3] here,
	       because it mixed data for different values of WMode. 
	       Perhaps pdf_font_used_glyph returns fast with reused glyphs.
	     */
	    code = pdf_font_used_glyph(pfd, glyph, (gs_font_base *)subfont);
	    if (code < 0)
		return code;
	    if (cid < 0 || cid >= char_cache_size || cid >= width_cache_size)
		return_error(gs_error_unregistered); /* Must not happen */
	    if (cid >= pdsubf->count)
		cid = 0, code = 1; /* undefined CID */
	    if (code == 0 /* just copied */ || pdsubf->Widths[cid] == 0) {
		pdf_glyph_widths_t widths;

		code = pdf_glyph_widths(pdsubf, wmode, glyph, (gs_font *)subfont, &widths);
		if (code < 0)
		    return code;
		if (code == 0) { /* OK to cache */
		    if (cid > pdsubf->count)
			return_error(gs_error_unregistered); /* Must not happen. */
		    w[cid] = widths.Width.w;
		    if (v != NULL) {
			v[cid * 2 + 0] = widths.Width.v.x;
			v[cid * 2 + 1] = widths.Width.v.y;
		    }
		    real_widths[cid] = widths.real_width.w;
		}
		if (pdsubf->u.cidfont.CIDToGIDMap != 0) {
		    gs_font_cid2 *subfont2 = (gs_font_cid2 *)subfont;

		    pdsubf->u.cidfont.CIDToGIDMap[cid] =
			subfont2->cidata.CIDMap_proc(subfont2, glyph);
		}
	    }
	    pdsubf->used[cid >> 3] |= 0x80 >> (cid & 7);
	}
	pdf_set_text_wmode(pdev, font->WMode);
	code = pdf_update_text_state(&text_state, (pdf_text_enum_t *)pte, pdfont,
				     &font->FontMatrix);
	if (code < 0)
	    return code;
	str.data = scan.text.data.bytes + index;
	str.size = scan.index - index;
	code = process_text_modify_width((pdf_text_enum_t *)pte, (gs_font *)font,
			      &text_state, &str, &wxy);
	if (code < 0)
	    return code;
	pte->index = scan.index;
	pte->xy_index = scan.xy_index;

	code = gx_path_add_point(pte->path,
			     pte->origin.x + float2fixed(wxy.x),
			     pte->origin.y + float2fixed(wxy.y));
	if (code < 0)
	    return code;
    } while (!done);
    pte->index = scan.index;
    pte->xy_index = scan.xy_index;
    return 0;
}

int
process_cmap_text(gs_text_enum_t *pte, void *vbuf, uint bsize)
{
    if (pte->text.operation &
	(TEXT_FROM_ANY - (TEXT_FROM_STRING | TEXT_FROM_BYTES))
	)
	return_error(gs_error_rangecheck);
    if (pte->text.operation & TEXT_INTERVENE) {
	/* Not implemented.  (PostScript doesn't allow TEXT_INTERVENE.) */
	return_error(gs_error_rangecheck);
    }
    return scan_cmap_text((pdf_text_enum_t *)pte);
}

/* ---------------- CIDFont ---------------- */

/*
 * Process a text string in a CIDFont.  (Only glyphshow is supported.)
 */
int
process_cid_text(gs_text_enum_t *pte, void *vbuf, uint bsize)
{
    uint operation = pte->text.operation;
    gs_text_enum_t save;
    gs_font *scaled_font = pte->current_font; /* CIDFont */
    gs_font *font;		/* unscaled font (CIDFont) */
    const gs_glyph *glyphs;
    gs_matrix scale_matrix;
    pdf_font_resource_t *pdsubf; /* CIDFont */
    gs_font_type0 *font0 = NULL;
    uint size;
    int code;

    if (operation & TEXT_FROM_GLYPHS) {
	glyphs = pte->text.data.glyphs;
	size = pte->text.size - pte->index;
    } else if (operation & TEXT_FROM_SINGLE_GLYPH) {
	glyphs = &pte->text.data.d_glyph;
	size = 1;
    } else
	return_error(gs_error_rangecheck);

    /*
     * PDF doesn't support glyphshow directly: we need to create a Type 0
     * font with an Identity CMap.  Make sure all the glyph numbers fit
     * into 16 bits.  (Eventually we should support wider glyphs too,
     * but this would require a different CMap.)
     */
    if (bsize < size * 2)
	return_error(gs_error_unregistered); /* Must not happen. */
    {
	int i;
	byte *pchars = vbuf;

	for (i = 0; i < size; ++i) {
	    ulong gnum = glyphs[i] - GS_MIN_CID_GLYPH;

	    if (gnum & ~0xffffL)
		return_error(gs_error_rangecheck);
	    *pchars++ = (byte)(gnum >> 8);
	    *pchars++ = (byte)gnum;
	}
    }

    /* Find the original (unscaled) version of this font. */

    for (font = scaled_font; font->base != font; )
	font = font->base;
    /* Compute the scaling matrix. */
    gs_matrix_invert(&font->FontMatrix, &scale_matrix);
    gs_matrix_multiply(&scale_matrix, &scaled_font->FontMatrix, &scale_matrix);

    /* Find or create the CIDFont resource. */

    code = pdf_obtain_font_resource(pte, NULL, &pdsubf);
    if (code < 0)
	return code;

    /* Create the CMap and Type 0 font if they don't exist already. */

    if (pdsubf->u.cidfont.glyphshow_font_id != 0)
 	font0 = (gs_font_type0 *)gs_find_font_by_id(font->dir, 
 		    pdsubf->u.cidfont.glyphshow_font_id);
    if (font0 == NULL) {
  	code = gs_font_type0_from_cidfont(&font0, font, font->WMode,
 					  &scale_matrix, font->memory);
	if (code < 0)
	    return code;
 	pdsubf->u.cidfont.glyphshow_font_id = font0->id;
    }

    /* Now handle the glyphshow as a show in the Type 0 font. */

    save = *pte;
    pte->current_font = pte->orig_font = (gs_font *)font0;
    /* Patch the operation temporarily for init_fstack. */
    pte->text.operation = (operation & ~TEXT_FROM_ANY) | TEXT_FROM_BYTES;
    /* Patch the data for process_cmap_text. */
    pte->text.data.bytes = vbuf;
    pte->text.size = size * 2;
    pte->index = 0;
    gs_type0_init_fstack(pte, pte->current_font);
    code = process_cmap_text(pte, vbuf, bsize);
    pte->current_font = scaled_font;
    pte->orig_font = save.orig_font;
    pte->text = save.text;
    pte->index = save.index + pte->index / 2;
    pte->fstack = save.fstack;
    return code;
}
