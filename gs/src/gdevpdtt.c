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
/* Text processing for pdfwrite. */
#include "math_.h"
#include "string_.h"
#include "gx.h"
#include "gserrors.h"
#include "gxfcache.h"		/* for orig_fonts list */
#include "gxfont.h"
#include "gxfont0.h"
#include "gxpath.h"		/* for getting current point */
#include "gdevpdfx.h"
#include "gdevpdfg.h"
#include "gdevpdtx.h"
#include "gdevpdtd.h"
#include "gdevpdtf.h"
#include "gdevpdts.h"
#include "gdevpdtt.h"

/* ================ Text enumerator ================ */

/* GC descriptor */
private_st_pdf_text_enum();

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

    if (!(text->operation & TEXT_DO_DRAW) || path == 0 ||
	gx_path_current_point(path, &cpt) < 0
	)
	return gx_default_text_begin(dev, pis, text, font, path, pdcolor,
				     pcpath, mem, ppte);

    code = pdf_prepare_fill(pdev, pis);
    if (code < 0)
	return code;

    if (text->operation & TEXT_DO_DRAW) {
	/*
	 * Set the clipping path and drawing color.  We set both the fill
	 * and stroke color, because we don't know whether the fonts will be
	 * filled or stroked, and we can't set a color while we are in text
	 * mode.  (This is a consequence of the implementation, not a
	 * limitation of PDF.)
	 */

	if (pdf_must_put_clip_path(pdev, pcpath)) {
	    int code = pdf_open_page(pdev, PDF_IN_STREAM);

	    if (code < 0)
		return code;
	    pdf_put_clip_path(pdev, pcpath);
	}

	if ((code =
	     pdf_set_drawing_color(pdev, pdcolor, &pdev->stroke_color,
				   &psdf_set_stroke_color_commands)) < 0 ||
	    (code =
	     pdf_set_drawing_color(pdev, pdcolor, &pdev->fill_color,
				   &psdf_set_fill_color_commands)) < 0
	    )
	    return code;
    }

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

/* ================ Process text ================ */

/* ---------------- Internal utilities ---------------- */

/*
 * Compute and return the orig_matrix of a font.
 */
private double
font_orig_scale(const gs_font *font)
{
    switch (font->FontType) {
    case ft_composite:		/* subfonts have their own FontMatrix */
    case ft_TrueType:
    case ft_CID_TrueType:
	/* The TrueType FontMatrix is 1 unit per em, which is what we want. */
	return 1.0;
    case ft_encrypted:
    case ft_encrypted2:
    case ft_CID_encrypted:
	/*
         * Type 1 fonts are supposed to use a standard FontMatrix of
         * [0.001 0 0 0.001 0 0], with a 1000-unit cell.  However,
         * Windows NT 4.0 creates Type 1 fonts, apparently derived from
         * TrueType fonts, that use a 2048-unit cell and corresponding
         * FontMatrix.  Also, some PS programs perform font scaling by
         * replacing FontMatrix like this :
         *
         *   /f12 /Times-Roman findfont
         *   copyfont	  % (remove FID)
         *   dup /FontMatrix [0.012 0 0 0.012 0 0] put
         *   definefont
         *   /f12 1 selectfont
         *
         * Such fonts are their own "base font", but the orig_matrix
         * must still be set to 0.001, not 0.012 .
         *
         * Detect and correct for this here.
	 */
	{
	    const gs_font *base_font = font;

	    while (base_font->base != base_font)
		base_font = base_font->base;
	    if (base_font->FontMatrix.xx == 1.0/2048 &&
		base_font->FontMatrix.xy == 0 &&
		base_font->FontMatrix.yx == 0 &&
		base_font->FontMatrix.yy == 1.0/2048
		)
		return 1.0/2048;
	    else
		return 0.001;
	}
	break;
    default:
	return 0;		/* error */
    }
}
int
pdf_font_orig_matrix(const gs_font *font, gs_matrix *pmat)
{
    double scale = font_orig_scale(font);

    if (scale == 0)
	return_error(gs_error_rangecheck);
    gs_make_scaling(scale, scale, pmat);
    return 0;
}

/*
 * Find or create a font resource object for a gs_font.  Return 1 iff the
 * font was newly created.  This procedure is only intended to be called
 * from a few places in the text code.
 */
int
pdf_make_font_resource(gx_device_pdf *pdev, gs_font *font,
		       pdf_font_resource_t **ppdfont)
{
    int index = -1;
    int BaseEncoding = ENCODING_INDEX_UNKNOWN;
    int same = 0, base_same = 0;
    pdf_font_embed_t embed =
	pdf_font_embed_status(pdev, font, &index, &same);
    pdf_font_descriptor_t *pfd = 0;
    pdf_resource_type_t rtype;
    int (*font_alloc)(gx_device_pdf *, pdf_font_resource_t **,
		      gs_id, pdf_font_descriptor_t *);
    gs_font *base_font = font;
    gs_font *below;
    pdf_font_resource_t *pdfont;
    pdf_standard_font_t *const psfa =
	pdev->text->outline_fonts->standard_fonts;
    int code = 0;
#define BASE_UID(fnt) (&((const gs_font_base *)(fnt))->UID)

    /* Find the "lowest" base font that has the same outlines. */
    while ((below = base_font->base) != base_font &&
	   base_font->procs.same_font(base_font, below,
				      FONT_SAME_OUTLINES))
	base_font = below;
    /*
     * set_base is the head of a logical loop; we return here if we
     * decide to change the base_font to one registered as a resource.
     */
 set_base:
    if (base_font == font)
	base_same = same;
    else
	embed = pdf_font_embed_status(pdev, base_font, &index, &base_same);
    if (embed == FONT_EMBED_STANDARD) {
	pdf_standard_font_t *psf = &psfa[index];

	if (!psf->pdfont) {
	    code = pdf_font_std_alloc(pdev, &psf->pdfont, base_font->id,
				      (gs_font_base *)base_font, index);
	    if (code < 0)
		return code;
	    code = 1;
	}
	*ppdfont = psf->pdfont;
	return code;
    } else if (embed == FONT_EMBED_YES &&
	       base_font->FontType != ft_composite &&
	       uid_is_valid(BASE_UID(base_font)) &&
	       !base_font->is_resource
	       ) {
	/*
	 * The base font has a UID, but it isn't a resource.  Look for a
	 * resource with the same UID, in the hope that that will be
	 * more authoritative.
	 */
	gs_font *orig = base_font->dir->orig_fonts;

	for (; orig; orig = orig->next)
	    if (orig != base_font && orig->FontType == base_font->FontType &&
		orig->is_resource &&
		uid_equal(BASE_UID(base_font), BASE_UID(orig))
		) {
		/* Use this as the base font instead. */
		base_font = orig;
		/*
		 * Recompute the embedding status of the base font.  This
		 * can't lead to a loop, because base_font->is_resource is
		 * now known to be true.
		 */
		goto set_base;
	    }
    }

    /* See if we already have a font resource for this base font. */
    /* Composite fonts don't have descriptors. */
    /****** WRONG -- MUST ACCOMMODATE MULTIPLE RESOURCES ******/

    switch (font->FontType) {
    case ft_CID_encrypted:
    case ft_CID_TrueType:
	rtype = resourceCIDFont;
	font_alloc = pdf_font_cidfont_alloc;
	break;
    case ft_encrypted:
    case ft_encrypted2:
    case ft_TrueType:
	rtype = resourceFont;
	font_alloc = pdf_font_simple_alloc;
	break;
    case ft_composite:
	pdfont = (pdf_font_resource_t *)
	    pdf_find_resource_by_gs_id(pdev, resourceFont, font->id);
	if (pdfont == 0) {
	    gs_font_type0 *const pfont = (gs_font_type0 *)base_font;
	    gs_font *subfont = pfont->data.FDepVector[0];
	    pdf_font_resource_t *pdsubf;

	    code = pdf_make_font_resource(pdev, subfont, &pdsubf);
	    if (code < 0)
		return code;
	    code = pdf_font_type0_alloc(pdev, &pdfont, base_font->id, pdsubf);
	    if (code < 0)
		return code;
	    code = 1;
	}
	*ppdfont = pdfont;
	return code;
    default:
	return_error(gs_error_invalidfont);
    }
    pdfont = (pdf_font_resource_t *)
	pdf_find_resource_by_gs_id(pdev, rtype, base_font->id);
    if (pdfont != 0) {
	*ppdfont = pdfont;
	return 0;
    }

    /* Create an appropriate font resource and descriptor. */

    if (embed == FONT_EMBED_YES) {
	/*
	 * HACK: Acrobat Reader 3 has a bug that makes cmap formats 4
	 * and 6 not work in embedded TrueType fonts.  Consequently, it
	 * can only handle embedded TrueType fonts if all the glyphs
	 * referenced by the Encoding have numbers 0-255.  Check for
	 * this now.
	 */
	if (font->FontType == ft_TrueType &&
	    pdev->CompatibilityLevel <= 1.2
	    ) {
	    int i;

	    for (i = 0; i <= 0xff; ++i) {
		gs_glyph glyph =
		    font->procs.encode_char(font, (gs_char)i,
					    GLYPH_SPACE_INDEX);

		if (glyph == gs_no_glyph ||
		    (glyph >= gs_min_cid_glyph &&
		     glyph <= gs_min_cid_glyph + 0xff)
		    )
		    continue;
		/* Can't embed, punt. */
		return_error(gs_error_rangecheck);
	    }
	}
    } else {			/* embed == FONT_EMBED_NO (STD not possible) */
	/*
	 * Per the PDF 1.3 documentation, there are only 3 BaseEncoding
	 * values allowed for non-embedded fonts.  Pick one here.
	 */
	BaseEncoding =
	    ((const gs_font_base *)base_font)->nearest_encoding_index;
	switch (BaseEncoding) {
	default:
	    BaseEncoding = ENCODING_INDEX_WINANSI;
	case ENCODING_INDEX_WINANSI:
	case ENCODING_INDEX_MACROMAN:
	case ENCODING_INDEX_MACEXPERT:
	    break;
	}
    }

    if ((code = pdf_font_descriptor_alloc(pdev, &pfd,
					  (gs_font_base *)base_font,
					  embed == FONT_EMBED_YES)) < 0 ||
	(code = font_alloc(pdev, &pdfont, base_font->id, pfd)) < 0
	)
	return code;
    code = 1;

    if (rtype == resourceFont)	/* i.e. not CIDFont */
	pdfont->u.simple.BaseEncoding = BaseEncoding;

    if (~same & FONT_SAME_METRICS) {
	/*
	 * Contrary to the PDF 1.3 documentation, FirstChar and
	 * LastChar are *not* simply a way to strip off initial and
	 * final entries in the Widths array that are equal to
	 * MissingWidth.  Acrobat Reader assumes that characters
	 * with codes less than FirstChar or greater than LastChar
	 * are undefined, without bothering to consult the Encoding.
	 * Therefore, the implicit value of MissingWidth is pretty
	 * useless, because there must be explicit Width entries for
	 * every character in the font that is ever used.
	 * Furthermore, if there are several subsets of the same
	 * font in a document, it appears to be random as to which
	 * one Acrobat Reader uses to decide what the FirstChar and
	 * LastChar values are.  Therefore, we must write the Widths
	 * array for the entire font even for subsets.
	 */
	/****** NYI ******/
	/*
	 * If the font is being downloaded incrementally, the range we
	 * determine here will be too small.  The character encoding
	 * loop in pdf_process_string takes care of expanding it.
	 */
	/******
	       pdf_find_char_range(font, &pdfont->u.simple.FirstChar,
	       &pdfont->u.simple.LastChar);
	 ******/
    }

    *ppdfont = pdfont;
    return 1;
}

/*
 * Compute the cached values in the text processing state from the text
 * parameters, current_font, and pis->ctm.  Return either an error code (<
 * 0) or a mask of operation attributes that the caller must emulate.
 * Currently the only such attributes are TEXT_ADD_TO_ALL_WIDTHS and
 * TEXT_ADD_TO_SPACE_WIDTH.  Note that this procedure fills in all the
 * values in ppts->values, not just the ones that need to be set now.
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
int
pdf_update_text_state(pdf_text_process_state_t *ppts,
		      const pdf_text_enum_t *penum,
		      pdf_font_resource_t *pdfont, const gs_matrix *pfmat)
{
    gx_device_pdf *const pdev = (gx_device_pdf *)penum->dev;
    gs_font *font = penum->current_font;
    gs_fixed_point cpt;
    gs_matrix orig_matrix, smat, tmat;
    double
	sx = pdev->HWResolution[0] / 72.0,
	sy = pdev->HWResolution[1] / 72.0;
    float size;
    int mask = 0;
    int code = gx_path_current_point(penum->path, &cpt);

    if (code < 0)
	return code;

    /* Get the original matrix from the base font, if available. */

    {
	gs_font_base *cfont = pdf_font_resource_font(pdfont);

	if (cfont != 0)
	    pdf_font_orig_matrix((gs_font *)font, &orig_matrix);
	else
	    pdf_font_orig_matrix(font, &orig_matrix);
    }

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

    ppts->members = TEXT_STATE_SET_FONT_AND_SIZE |
	TEXT_STATE_SET_MATRIX | TEXT_STATE_SET_RENDER_MODE;

    if (penum->text.operation & TEXT_ADD_TO_ALL_WIDTHS) {
	gs_point pt;

	code = transform_delta_inverse(&penum->text.delta_all, &smat, &pt);
	if (code >= 0 && pt.y == 0) {
	    ppts->values.character_spacing = pt.x * size;
	    ppts->members |= TEXT_STATE_SET_CHARACTER_SPACING;
	} else
	    mask |= TEXT_ADD_TO_ALL_WIDTHS;
    }

    if (penum->text.operation & TEXT_ADD_TO_SPACE_WIDTH) {
	gs_point pt;

	code = transform_delta_inverse(&penum->text.delta_space, &smat, &pt);
	if (code >= 0 && pt.y == 0 && penum->text.space.s_char == 32) {
	    ppts->values.word_spacing = pt.x * size;
	    ppts->members |= TEXT_STATE_SET_WORD_SPACING;
	} else
	    mask |= TEXT_ADD_TO_SPACE_WIDTH;
    }

    /* Store the updated values. */

    tmat.xx /= size;
    tmat.xy /= size;
    tmat.yx /= size;
    tmat.yy /= size;
    tmat.tx += fixed2float(cpt.x);
    tmat.ty += fixed2float(cpt.y);

    ppts->values.pdfont = pdfont;
    ppts->values.size = size;
    ppts->values.matrix = tmat;
    ppts->values.render_mode = (font->PaintType == 0 ? 0 : 1);
    ppts->font = font;

    return mask;
}

/*
 * Set up commands to make the output state match the processing state.
 * General graphics state commands are written now; text state commands
 * are written later.  Update ppts->values to reflect all current values.
 */
private double
font_matrix_scaling(const gs_font *font)
{
    return fabs((font->FontMatrix.yy != 0 ? font->FontMatrix.yy :
		 font->FontMatrix.yx));
}
int
pdf_set_text_process_state(gx_device_pdf *pdev,
			   const gs_text_enum_t *pte,	/* for pdcolor, pis */
			   pdf_text_process_state_t *ppts,
			   const gs_const_string *pstr)
{
    /*
     * Setting the stroke parameters may exit text mode, causing the
     * settings of the text parameters to be lost.  Therefore, we set the
     * stroke parameters first.
     */
    if (pdf_render_mode_uses_stroke(pdev, &ppts->values)) {
	/* Write all the parameters for stroking. */
	gs_imager_state *pis = pte->pis;
	float save_width = pis->line_params.half_width;
	const gs_font *font = ppts->font;
	double scaled_width = font->StrokeWidth;
	int code;

	/* Note that we compute pis->line_params.half_width in device space,
	 * even though it logically represents a value in user space.  
	 * The 'scale' value compensates for this.
	 */
	scaled_width *= font_matrix_scaling(font);
	scaled_width *= min(hypot(pte->pis->ctm.xx, pte->pis->ctm.yx) / 
                                pdev->HWResolution[0] * pdev->HWResolution[1],
                            hypot(pte->pis->ctm.xy, pte->pis->ctm.yy));
	pis->line_params.half_width = scaled_width / 2;
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

    /* Now set all the other parameters. */

    return pdf_set_text_state_values(pdev, &ppts->values, ppts->members);
}

/*
 * Get the widths (unmodified and possibly modified) of a glyph in a (base)
 * font.  Return 1 if the width was defaulted to MissingWidth.
 */
private int store_glyph_width(int *pwidth, int wmode, double scale,
			      const gs_glyph_info_t *pinfo);
int
pdf_glyph_widths(pdf_font_resource_t *pdfont, gs_glyph glyph,
		 gs_font_base *font, pdf_glyph_widths_t *pwidths)
{
    int wmode = font->WMode;
    gs_glyph_info_t info;
    /*
     * orig_scale is 1.0 for TrueType, 0.001 or 1.0/2048 for Type 1.
     */
    double scale = font_orig_scale((const gs_font *)pdf_font_resource_font(pdfont)) * 1000.0;
    int code;

    if (glyph != gs_no_glyph &&
	(code = font->procs.glyph_info((gs_font *)font, glyph, NULL,
				       (GLYPH_INFO_WIDTH0 << wmode) |
				       GLYPH_INFO_OUTLINE_WIDTHS,
				       &info)) >= 0 &&
	(code = store_glyph_width(&pwidths->Width, wmode, scale, &info)) >= 0 &&
	(/*
	  * Only ask for modified widths if they are different, i.e.,
	  * if GLYPH_INFO_OUTLINE_WIDTHS was set in the response.
	  */
	 (info.members & GLYPH_INFO_OUTLINE_WIDTHS) == 0 ||
	 (code = font->procs.glyph_info((gs_font *)font, glyph, NULL,
					GLYPH_INFO_WIDTH0 << wmode,
					&info)) >= 0) &&
	(code = store_glyph_width(&pwidths->real_width, wmode, scale, &info)) >= 0
	) {
	/*
	 * If the character is .notdef, don't cache the width,
	 * just in case this is an incrementally defined font.
	 */
	return (gs_font_glyph_is_notdef(font, glyph) ? 1 : 0);
    } else {
	/* Try for MissingWidth. */
	gs_point scale2;
	const gs_point *pscale = 0;
	gs_font_info_t finfo;

	if (scale != 1)
	    scale2.x = scale2.y = scale, pscale = &scale2;
	code = font->procs.font_info((gs_font *)font, pscale,
				     FONT_INFO_MISSING_WIDTH, &finfo);
	if (code < 0)
	    return code;
	pwidths->Width = pwidths->real_width = finfo.MissingWidth;
	/*
	 * Don't mark the width as known, just in case this is an
	 * incrementally defined font.
	 */
	return 1;
    }
}
private int
store_glyph_width(int *pwidth, int wmode, double scale,
		  const gs_glyph_info_t *pinfo)
{
    double w, v;

    if (wmode && (w = pinfo->width[wmode].y) != 0)
	v = pinfo->width[wmode].x;
    else
	w = pinfo->width[wmode].x, v = pinfo->width[wmode].y;
    if (v != 0)
	return_error(gs_error_rangecheck);
    *pwidth = (int)(w * scale);
    return 0;
}

/* ---------------- Main entry ---------------- */

/*
 * Fall back to the default text processing code when needed.
 */
int
pdf_default_text_begin(gs_text_enum_t *pte, const gs_text_params_t *text,
		       gs_text_enum_t **ppte)
{
    return gx_default_text_begin(pte->dev, pte->pis, text, pte->current_font,
				 pte->path, pte->pdcolor, pte->pcpath,
				 pte->memory, ppte);
}

/*
 * Continue processing text.  This is the 'process' procedure in the text
 * enumerator.  Per the check in pdf_text_begin, we know the operation is
 * not a charpath, but it could be anything else.
 */
int
pdf_text_process(gs_text_enum_t *pte)
{
    pdf_text_enum_t *const penum = (pdf_text_enum_t *)pte;
    uint operation = pte->text.operation;
    const void *vdata;
    uint size = pte->text.size - pte->index;
    gs_text_enum_t *pte_default;
    PROCESS_TEXT_PROC((*process));
    int code = -1;		/* to force default implementation */
#define BUF_SIZE 100		/* arbitrary > 0 */

    /*
     * If we fell back to the default implementation, continue using it.
     */
 top:
    pte_default = penum->pte_default;
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
	    goto skip;
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
	goto skip;

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

 skip:
    if (code < 0) {
	/* Fall back to the default implementation. */
	code = pdf_default_text_begin(pte, &pte->text, &pte_default);
	if (code < 0)
	    return code;
	penum->pte_default = pte_default;
	gs_text_enum_copy_dynamic(pte_default, pte, false);
    }
    /* The 'process' procedure might also have set pte_default itself. */
    if (penum->pte_default && !code)
	goto top;
    return code;
}
