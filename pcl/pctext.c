/* Portions Copyright (C) 2001 artofcode LLC.
   Portions Copyright (C) 1996, 2001 Artifex Software Inc.
   Portions Copyright (C) 1988, 2000 Aladdin Enterprises.
   This software is based in part on the work of the Independent JPEG Group.
   All Rights Reserved.

   This software is distributed under license and may not be copied, modified
   or distributed except as expressly authorized under the terms of that
   license.  Refer to licensing information at http://www.artifex.com/ or
   contact Artifex Software, Inc., 101 Lucas Valley Road #110,
   San Rafael, CA  94903, (415)492-9861, for further information. */
/*$Id$ */

/* pctext.c -  PCL5 text printing commands */

#include "math_.h"
#include "gx.h"
#include "gsimage.h"
#include "plvalue.h"
#include "plvocab.h"
#include "pcommand.h"
#include "pcstate.h"
#include "pcdraw.h"
#include "pcfont.h"
#include "pcursor.h"
#include "pcpage.h"
#include "pcfrgrnd.h"
#include "gdebug.h"
#include "gscoord.h"
#include "gsline.h"
#include "gspaint.h"
#include "gspath.h"
#include "gspath2.h"
#include "gsrop.h"
#include "gsstate.h"
#include "gxchar.h"
#include "gxfont.h"		/* for setting next_char proc */
#include "gxstate.h"

/* Define the text parsing methods. */
static const pcl_text_parsing_method_t pcl_tpm_0 = pcl_tpm_0_data,
                                        pcl_tpm_21 = pcl_tpm_21_data,
                                        pcl_tpm_31 = pcl_tpm_31_data,
                                        pcl_tpm_38 = pcl_tpm_38_data;

/* pseudo-"dots" (actually 1/300" units) used in underline only */
#define	dots(n)	    ((float)(7200 / 300 * n))


/*
 * Install a font in the graphic state.
 */
  static void
set_gs_font(
    pcl_state_t *   pcs
)
{
    gs_font *       pfont = (gs_font *)pcs->font->pfont;
    gs_setfont(pcs->pgs, pfont);
    /* font scaling is reflected directly in the ctm */
    pfont->FontMatrix = pfont->orig_FontMatrix;
}

/* uncomment the following definition to treat map type 0 as defined
   in the specification.  The default is to use the behavior we have
   observed on several HP devices.  Map type 0 is treated as map type
   1. */

/* #define USE_MAP_TYPE_IN_SPECIFICATION */

/*
 * Check if a character code is considered "printable" by given symbol set.
 */
  static bool
is_printable(
    const pcl_state_t * pcs,
    gs_char                 chr,
    bool                    literal
)
{
    int map_type;
    bool printable;

    if (literal) /* transparent data */
	printable = true;
    else {
	if (pcs->map != 0) {
	    /* PCL TRM 10-7 
	     * symbol map type overrides, font map type
	     */
	    map_type = pcs->map->type;
	}
	else {
	    /* PCL TRM 11-18 */
	    map_type = pcs->font->font_type;
	}

#ifndef USE_MAP_TYPE_IN_SPECIFICATION
        if ( map_type == 0 )
            map_type = 1;
#endif /* USE_MAP_TYPE_IN_SPECIFICATION */

	if ( map_type == 0 )
	    printable = (chr >= ' ') && (chr <= '\177'); 
	else if ( map_type == 1 ) {
	    chr &= 0x7f;
	    printable = (chr >= ' '); /* 0-31 and 128-159 are not printable */
	}
	else if ( map_type >= 2 ) {
	    /* 2 is correct but will force all types above 2 here */
	    if ( (chr == 0) || (chr == '\033') || 
		 ((chr >= '\007') && (chr <= '\017')) )
		printable = false;
	    else
		printable = true;
	}
    }
    return printable; 
}

/*
 * Retrieve the next character identifier from a string.
 *
 * Both the string pointer and the length are modified.
 *
 * The final operand is true if the text was provided via the literal
 * (transparent) text command: ESC & p <nbytes> X. This distinction is
 * important for characters that are not considered printable by the
 * current symbol set. Normally, such characters are ignored. But if they 
 * resulted from the literal (transparent) text command, they are handled as
 * spaces. Characters that are mapped by the symbol set but are not in a font
 * are always dealt with as space characters.
 *
 * The special handling provided for the character code 32 below is not,
 * in fact, correct. PCL fonts may map non-space characters to code 32, and
 * if this is done no special handling is provided for this code; in PCL,
 * a space character is a character not present in the font. Unfortunately,
 * some of the resident fonts used have explicit space characters, and to
 * handle the hmi properly when these fonts are used, this code must handle
 * fonts that have actual characters at code 32 improperly.
 *
 * Returns 0 on success, 2 if the string is exhausted. Note that it is not an
 * error for the string to end in the middle of a 2-byte sequence.
 */
  static int
get_next_char(
    pcl_state_t *   pcs,
    const byte **   ppb,
    uint *          plen,
    gs_char *       pchr,
    gs_char *       porig_char,
    bool *          pis_space,
    bool            literal,
    gs_point *      pwidth
)
{
    const byte *    pb = *ppb;
    int             len = *plen;
    pl_font_t *     plfont = pcs->font;
    gs_char         chr;
    if (len <= 0)
        return 2;
    *pis_space = false;
    chr = *pb++;
    len--;
    if (pcl_char_is_2_byte(chr, pcs->text_parsing_method) && (len > 0)) {
        chr = (chr << 8) + *pb++;
        len--;
    }
    *ppb = pb;
    *plen = len;
    *porig_char = chr;
    /* check if the code is considered "printable" in the current symbol set */
    if (!is_printable(pcs, chr, literal)) {
        *pis_space = literal;
        *pchr = 0xffff;
        return 0;
    }

    /* map the symbol. If it fails to map, quit now.  Unless we have a
       galley character */
    chr = pl_map_symbol(pcs->map, chr,
                        plfont->storage == pcds_internal,
                        pl_complement_to_vocab(plfont->character_complement) == plgv_MSL, false);
    *pchr = chr;
    if (chr == 0xffff) {
        *pis_space = true;
        return 0;
     }

    /* For internal fonts we simulate the font missing the space
       character here.  The character complement is checked to see if
       the the font is Western Latin and Unicode 0020.  We could
       also check for an MSL space here but we know the internal
       reportoire will never contain an MSL font that requires
       simulating a missing space character. */
    if (plfont->storage == pcds_internal && 
        chr == 0x0020 &&
        plfont->character_complement[5] == 0x3f &&
        pl_complement_to_vocab(plfont->character_complement) == plgv_Unicode) {
        *pis_space = true;
        *pchr = 0xffff;
        return 0;
     }

    /* check if the character is in the font and get the character
       width at the same time */
    if ( *pis_space == false )
        if ( pl_font_char_width(plfont, (void *)(pcs->pgs), chr, pwidth) == 0 )
            return 0;
    /*
     * If we get to this point deem the character an undefined
     * character - a space in pcl.
     */
    *pis_space = true;
    *pchr = 0xffff;
    return 0;
}

/*
 * Draw the foreground of a character. For transparent text this is the only
 * part that must be drawn.
 */
static int
show_char_foreground(
    const pcl_state_t * pcs,
    const gs_char *        pbuff
)
{
    int code = 0;
    gs_text_enum_t *penum;
    pl_font_t *plfont = pcs->font;
    gs_font *pfont = plfont->pfont;
    gs_text_params_t text;

    /* set vertical writing if -1 which requires double bytes or 1 */
    if ((pcs->text_path == -1 && ((pbuff[0] & 0xff00) != 0)) ||
        (pcs->text_path == 1))
    	pfont->WMode = 1;
    else
        pfont->WMode = 0;
    text.operation = TEXT_FROM_CHARS | TEXT_DO_DRAW | TEXT_RETURN_WIDTH;
    text.data.chars = pbuff;
    text.size = 1;
    code = gs_text_begin(pcs->pgs, &text, pcs->memory, &penum);
    if (code >= 0)
        code = gs_text_process(penum);
    gs_text_release(penum, "show_char_foreground");

    return code;
}

/*
 * draw the opaque background of a character.
 *
 * In the graphic library, characters are masks, hence they are always
 * transparent. Not so in PCL, where characters may be either opaque or
 * transparent.
 *
 * To deal with this dichotomy, opaque characters are rendered as a pair of
 * masks. One is the normal character mask; the other is the bounding box of
 * the character less the character itself.
 *
 * The manner in which the second mask is formed varies based on the font type.
 * For bitmap fonts, the inverse mask is formed as an imagemask object, with
 * inverted polarity. For scalable fonts (which have only provided a path),
 * the inverse is formed by adding the bounding box rectangle as a path to
 * the character path, and using eofill on the resultant path.
 *
 * Special handling is required to achieve the desired raster operation on the
 * "background" mask. From the point of view of the graphic library, the
 * background mask is a normal mask, and hence would utiltise the S = 0
 * portion of the current logical operation (recall that rop's are expressed
 * in an additive sense). The desired effect is, however, the S = 1 portion
 * of the current rop, so the current rop must be inverted in the sense of the
 * source to achive the desired result. In principle, the S = 1 porition of
 * the background rop should be set to the no-op rop, but this is not necessary
 * as the source is a mask.
 *
 * An additional complication arises from the specification provided by HP for
 * handling the source opaque, pattern transparent situation. In this case,
 * the pattern affects only for the foreground pixels of the source; the back-
 * ground must be rendered as a solid, opaque white.
 */
  static int
show_char_background(
    pcl_state_t *       pcs,
    const gs_char *        pbuff
)
{
    gs_state *          pgs = pcs->pgs;
    gs_rop3_t           rop = (gs_rop3_t)(pcs->logical_op);
    const pl_font_t *   plfont = pcs->font;
    gs_font *           pfont = plfont->pfont;
    gs_point            pt;
    int                 code = 0;

    /* save the graphic state and set the background raster operation */
    pcl_gsave(pcs);
    if (pcs->pattern_transparent)
        pcl_set_drawing_color(pcs, pcl_pattern_solid_white, 0, false);
    gs_setrasterop(pgs, (gs_rop3_t)rop3_know_S_1((int)rop));
    gs_currentpoint(pgs, &pt);

    if (plfont->scaling_technology == plfst_bitmap) {
	gs_char         chr = pbuff[0];
	gs_glyph        glyph = pfont->procs.encode_char(pfont, chr, gs_no_glyph);
	const byte *    cdata = pl_font_lookup_glyph(plfont, glyph)->data;
	int             nbytes;
        uint            used;
        gs_image_enum * pen = 0;
        gs_image1_t     mask;

        /* empty characters have no background */
	if (cdata == 0) {
            pcl_grestore(pcs);
            return 0;
        }

        /* allocate the image enumerator */
        pen = gs_image_enum_alloc(gs_state_memory(pgs), "bitmap font background");
        if (pen == 0) {
            pcl_grestore(pcs);
            return e_Memory;
        }

        /* translate the origin to the ul corner of the image */
	pt.x += (float)pl_get_int16(cdata + 6);
	pt.y -= (float)pl_get_int16(cdata + 8);
        gs_translate(pgs, pt.x, pt.y);

        /* set up and render the image mask */
        gs_image_t_init_mask(&mask, false);
        mask.adjust = false;
        mask.Width = pl_get_uint16(cdata + 10);
        mask.Height = pl_get_uint16(cdata + 12);
        nbytes = ((mask.Width + 7) / 8) * mask.Height;
        gs_image_init(pen, &mask, false, pgs);
        code = gs_image_next(pen, cdata + 16, nbytes, &used);

        /* clean up */
        gs_image_cleanup(pen, pgs);
        gs_free_object(gs_state_memory(pgs), pen, "bitmap font background");

    } else {
        gs_text_params_t text;
	gs_rect     bbox;
        gs_text_enum_t *    penum;

        /* clear the path; start the new one from the current point */
	gs_newpath(pgs);
	gs_moveto(pgs, pt.x, pt.y);
        text.data.chars = pbuff;
        text.size = 1;
        text.operation = TEXT_FROM_CHARS | TEXT_DO_TRUE_CHARPATH | TEXT_RETURN_WIDTH;
        code = gs_text_begin(pgs, &text, pcs->memory, &penum);
        if (code >= 0)
            code = gs_text_process(penum);
        if (code >= 0) {
	    /* append the characters bounding box and use eofill */
	    gs_pathbbox(pgs, &bbox);
	    gs_rectappend(pgs, &bbox, 1);
	    gs_eofill(pgs);
        }
	gs_text_release(penum, "show_char_background");
    }

    pcl_grestore(pcs);
    return code;
}

/*
 * get the advance width.
 */
 static floatp
pcl_get_width(pcl_state_t *pcs, gs_point *advance_vector, const gs_point *pscale, gs_char chr, bool is_space)
{
    pcl_font_selection_t *  pfp = &(pcs->font_selection[pcs->font_selected]);
    floatp width;
    if (chr != 0xffff) { 
	if (!pfp->params.proportional_spacing || is_space)
	    width = pcl_hmi(pcs);
	else {
	    if (pcs->font->scaling_technology == plfst_TrueType ||
                pcs->font->scaling_technology == plfst_MicroType) {
		floatp tmp;
		tmp = pscale->x / (floatp)pcs->uom_cp + 0.5;
		tmp -= fmod(tmp, (floatp)1.0);
		tmp *= (floatp)pcs->uom_cp;
		width = advance_vector->x * tmp;

	    } else
		width = advance_vector->x * pscale->x;
	    width += (floatp)pcs->uom_cp / 2.0;
	    width -= fmod(width, (floatp)pcs->uom_cp);
	}
    } else if (is_space)
	width = pcl_hmi(pcs);
    else
	width = 0.0;
    /* round to nearest integral pcl units */
    return width;
}

/*
 * Show a string of characters.  Provide a general purpose function
 * that can be used in all cases (pcl_show_chars_slow) and a faster
 * function (pcl_show_chars_fast) that can be used for most
 * circumstances.  The latter algorithm can print strings of
 * characters the slow algorithm only prints one character at a time.
 *
 * As is the case for other parts of this code, this code is made more complex
 * by the PostScript-centric nature of the the graphics library, and by a
 * long standing flaw in the PostScript view of fonts. Specifically, the
 * initial introduction of Encoding arrays into PostScript fonts, followed by
 * composite font mechanism, very much confused the concepts of font and text
 * parsing method.
 *
 * A font is an object which accepts a character identifier and returns a
 * "rendering" of that character (which may be a bitmap, may be a path, may
 * be an advance vector, or may be some combination of the above); it may also
 * in some cases apply this rendering to the graphic state (which may include
 * modifying the output). Whether or not a font caches or expects its client
 * to handle caching is a separate issue; there are good areguments for either
 * approach.
 *
 * A text parsing method is an object that accepts a character string and
 * returns one or more character identifiers. A text parsing method is, in
 * principle, completely independent of a font, though for historical reasons
 * the two concepts are often linked at the application level.
 *
 * Because of the PostScript origins of the graphic library, its font interface
 * handles both text parsing and rendering. To achieve flexibility, the client
 * may provide a "get next character" procedure for the graphic library font
 * machinery to work with, but this flexibility is not sufficient for PCL, as
 * the latter potentially needs to perform additional operations on each
 * character. Hence, PCL will not ask the font machiner to render more than
 * one character at a time.
 *
 * Complicating this picture is the nature of memory management in the graphic
 * library. The show class operators in PostScript generally take a string as
 * an operand. PostScript strings have the "getinterval" property: one string
 * may be part of another string. Hence strings cannot have headers. In a
 * relocating memory systems, this implies that strings must be dealt with
 * separately from other objects: they must be allocated as strings. In the
 * case of PCL, this is not necessarily the case (see, for example, the case
 * of transparent mode, below).
 *
 * The original implementation of this routine ignored this distinction and
 * could, in principle, have failed if re-location was enabled. It was also
 * rather hard to read, because it parsed the input string (at least) twice:
 * once the find the character so that PCL-specific actions could be taken,
 * then again via the font machiner.
 *
 *
 * The final operand is true if the text was provided via the literal
 * (transparent) text command: ESC & p <nbytes> X. This distinction is
 * important for characters that are not mapped by the current symbol set.  */

  static int
pcl_show_chars_slow(
    pcl_state_t *           pcs,
    const gs_point *        pscale,
    const byte *            str,
    uint                    size,
    bool                    literal
)
{
    gs_state *              pgs = pcs->pgs;
    gs_char                 buff[1];
    floatp                  rmargin = pcs->margins.right;
    floatp                  page_size = pcs->xfm_state.pd_size.x;
    bool                    source_opaque = !pcs->source_transparent;
    bool                    invisible_pattern = is_invisible_pattern(pcs);
    bool                    wrap = pcs->end_of_line_wrap;
    bool                    is_space = false;
    bool                    use_rmargin = (pcs->cap.x <= rmargin);
    gs_char                 chr, orig_chr;
    int                     code = 0;
    floatp                  width;
    gs_point                cpt;
    gs_point                advance_vector;

    cpt.x = pcs->cap.x;
    cpt.y = pcs->cap.y;

    while (get_next_char(pcs, &str, &size, &chr, &orig_chr, &is_space, literal, &advance_vector) == 0) {
        floatp  tmp_x;

        /* check if a character was found */
        buff[0] = chr;
	/* round width to integral pcl current units */
	width = (pcl_get_width(pcs, &advance_vector, pscale, chr, is_space));

        /*
         * Check for transitions of the left margin; this check is
         * disabled if the immediately preceding character was a back-space.
         * A move beyond the "right" logical page edge is also considered
         * a margin transition.
         *
         * A little-known feature of PCL is the effect of the line-wrap
         * command on the interpretation of the right margin. If line
         * wrap is in effect, a transition of the left margin will cause
         * a <cr><lf> operation BEFORE the current character is printed. If
         * line-wrap is not in effect, a transition of the right margin will
         * stop printing AFTER the current character is printed.
         *
         * A special case occurs in the non-wrap situation when the current
         * position exactly equals the current margin. In that case, no
         * character is printed.
         */
        if (!pcs->last_was_BS) {
            if (wrap) {
                if ( (use_rmargin && (cpt.x + width > rmargin)) ||
                     (cpt.x + width > page_size)                   ) {
                    /* update the current position for the benefit of
                       CR so it knows where to draw underlines if
                       needed then restore the position.  NB don't
                       like this business of mixing floats and ints -
                       (pcs->cap and cpt). throughout this
                       function. */
                    pcs->cap.x = cpt.x;
                    pcs->cap.y = cpt.y;
	            pcl_do_CR(pcs);
                    pcl_do_LF(pcs);
                    cpt.x = pcs->cap.x;
                    cpt.y = pcs->cap.y;
                    use_rmargin = true;
		}
            } else {
                if (use_rmargin && (cpt.x == rmargin))
                    break;
                else if (cpt.x >= page_size) {
                    cpt.x = page_size;
                    break;
                }
            }
	}

        /*
         * If the immediately preceding character was a BS, the code will
         * center the current character on top of the preceding one. After
         * the character is printed, the current point is returned to the
         * prior point.
         */
        tmp_x = cpt.x;
        if (pcs->last_was_BS) {
            /* hack alert.  It seems if the last width is large, we
             use the horizontal dimension of the page as a guess, the
             centering is replaced by returning to the zero
             coordinate.  It would take quite a bit of time to
             investigate what the hp is doing in this pathological
             case, so we have not done a detailed analysis.  This
             solution prints the tests we have correctly. */
            if (pcs->last_width > pcs->xfm_state.pd_size.x)
                tmp_x = 0;
            else
                tmp_x += (pcs->last_width - width) / 2; 
        }
        gs_moveto(pgs, tmp_x / pscale->x, cpt.y / pscale->y);

        if (chr != 0xffff) {
            /* if source is opaque, show and opaque background */
            if (source_opaque)
                code = show_char_background(pcs, buff);
            if (code >= 0)
            if (!invisible_pattern)
                    code = show_char_foreground(pcs, buff);
            if (code < 0)
                break;
            /* NB WRONG - */
            pcl_mark_page_for_current_pos(pcs);
        }

        /* 
         * Check again for the first character following a back-space. if
         * this is the case, go back to the original position.
         */
        if (pcs->last_was_BS) {
            cpt.x += pcs->last_width;
            pcs->last_was_BS = false;
        } else
            cpt.x += width;

        /* check for going beyond the margin if not wrapping */
        if (!wrap) {
	    if (use_rmargin && (cpt.x > rmargin)) {
                cpt.x = rmargin;
                break;
            } else if (cpt.x >= page_size) {
                cpt.x = page_size;
                break;
	    }
	}
    }

    /* record the last width */
    pcs->last_width = width;

    /* update the current position */
    pcs->cap.x = cpt.x;
    pcs->cap.y = cpt.y;

    return code;
}

/*
 * Set up to handle a string of text.
 *
 * The final operand is true if the text was provided via the literal
 * (transparent) text command: ESC & p <nbytes> X. This distinction is
 * important for characters that are not mapped by the current symbol set.
 */
  int
pcl_text(
    const byte *    str,
    uint            size,
    pcl_state_t *   pcs,
    bool            literal
)
{
    gs_state *      pgs = pcs->pgs;
    gs_matrix       user_ctm;
    gs_point        scale;
    int             scale_sign;
    int             code;

    /* rtl files can have text in them - we don't print any characters
       in rtl */
    if (pcs->personality == rtl)
	return 0;
    /* set up the current font and HMI */
    if ((pcs->font == 0) && ((code = pcl_recompute_font(pcs)) < 0))
        return code;

    /* set up the graphic state */
    code = pcl_set_drawing_color( pcs,
                                  pcs->pattern_type,
                                  pcs->current_pattern_id,
                                  false
                                  );
    if (code >= 0)
        code = pcl_set_graphics_state(pcs);
    if (code < 0)
	return code;
    set_gs_font(pcs);

    /* set up the font transformation */
    if (pcs->font->scaling_technology == plfst_bitmap) {
	scale.x = pcl_coord_scale / pcs->font->resolution.x;
	scale.y = pcl_coord_scale / pcs->font->resolution.y;

	/*
	 * Bitmap fonts use an inverted coordinate system,
	 * the same as the usual PCL system.
	 */
	scale_sign = 1;
    } else {
	/*
	 * Outline fonts are 1-point; the font height is given in
	 * (quarter-)points.  However, if the font is fixed-width,
	 * it must be scaled by pitch, not by height, relative to
	 * the nominal pitch of the outline.
	 */
	pcl_font_selection_t *  pfp = &pcs->font_selection[pcs->font_selected];
	/* AGFA madness - 72.307 points per inch for intellifonts */
	floatp ppi = (pfp->font->scaling_technology == plfst_Intellifont) ? 72.307 : 72.0;
        if (pfp->font->params.proportional_spacing) {
	    scale.x = scale.y = pfp->params.height_4ths
	                         * 0.25 * 7200.0 / ppi;
	} else {
	    scale.x = scale.y = pl_fp_pitch_cp(&pfp->params)
	                         * (1000.0 / pl_fp_pitch_cp(&pfp->font->params))
	                         * (7200.0 / (100.0 * ppi));

            /* hack for a scalable lineprinter font.  If a real
               lineprinter bitmap font is available it will be handled
               by the bitmap scaling case above */
            if (pfp->font->params.typeface_family == 0) {
                scale.x = scale.y = 850.0;
            }

	}
	/*
	 * Scalable fonts use an upright coordinate system,
	 * the opposite from the usual PCL system.
	 */
	scale_sign = -1;
    }

    /*
     * If floating underline is on, since we're about to print a real
     * character, track the best-underline position.
     * XXX Until we have the font's design value for underline position,
     * use 0.2 em.  This is enough to almost clear descenders in typical
     * fonts; it's also large enough for us to check that the mechanism
     * works.
     */
    if (pcs->underline_enabled && pcs->underline_floating) {
	float   yu = scale.y / 5.0;

	if (yu > pcs->underline_position)
	    pcs->underline_position = yu;
    }

    /*
     * XXX I'm using the more general, slower approach rather than
     * just multiplying/dividing by scale factors, in order to keep it
     * correct through orientation changes.  Various parts of this should
     * be cleaned up when performance time rolls around.
     */
    gs_currentmatrix(pgs, &user_ctm);

    /* possibly invert text because HP coordinate system is inverted */
    scale.y *= scale_sign;
    gs_scale(pgs, scale.x, scale.y);

    /* it is not clear if vertical substitutes are allowed in mode -1 */ 
    if (pcs->text_path != 0)
        pcs->font->allow_vertical_substitutes = true;
    else
        pcs->font->allow_vertical_substitutes = false;


    /* Print remaining characters, restore the ctm */
    code = pcl_show_chars_slow(pcs, &scale, str, size, literal);
    gs_setmatrix(pgs, &user_ctm);
    if (code > 0)		/* shouldn't happen */
	code = gs_note_error(gs_error_invalidfont);
    return code;
}

/*
 * Individual non-command/control characters
 */
  int
pcl_plain_char(
    pcl_args_t *    pargs,
    pcl_state_t *   pcs
)
{
    return pcl_text((const byte *)&(pargs->command), 1, pcs, pcs->display_functions);
}

/*
 * draw underline up to current point, adjust status
 */
  void
pcl_do_underline(
    pcl_state_t *   pcs
)
{
    if (pcs->underline_start.x != pcs->cap.x) {
	gs_state *  pgs = pcs->pgs;
        float       y = pcs->underline_start.y + pcs->underline_position;
        int         code;

        /* save the grapics state */
        pcl_gsave(pcs);

        code = pcl_set_drawing_color( pcs,
                                      pcs->pattern_type,
                                      pcs->current_pattern_id,
                                      false
                                      );
        if (code >= 0)
	    code = pcl_set_graphics_state(pcs);
        if (code < 0)
            return;

	/*
         * TRM says (8-34) that underline is 3 dots.  In a victory for
         * common sense, it's not.  Rather, it's 0.01" (which *is* 3 dots
         * at 300 dpi only)
         */
	gs_setlinewidth(pgs, dots(3));
	gs_moveto(pgs, pcs->underline_start.x, y);
	gs_lineto(pgs, pcs->cap.x, y);
	gs_stroke(pgs);
        
        pcl_grestore(pcs);
    }

    /*
     * Fixed underline is 5 "dots" (actually 5/300") down.  Floating
     * will be determined on the fly.
     */
    pcs->underline_start = pcs->cap;
    pcs->underline_position = pcs->underline_floating ? 0.0 : dots(5);
}

/* ------ Commands ------ */

/*
 * ESC & p <count> X
 *
 * Unparsed text command
 */
  static int
pcl_transparent_mode(
    pcl_args_t *    pargs,
    pcl_state_t *   pcs
)
{
    return pcl_text(arg_data(pargs), uint_arg(pargs), pcs, true);
}

/*
 * ESC & d <0|3> D
 *
 * Enable floating or fixed-depth underlining.
 *
 * NB: If underlining is already enabled, this command is ignored. Underlining
 *     must be specifically disabled to switch from fixed to floating.
 */
  static int
pcl_enable_underline(
    pcl_args_t *    pargs,
    pcl_state_t *   pcs
)
{
    int             type = int_arg(pargs);

    /* ignore command if underlining is already enabled */
    if (pcs->underline_enabled)
        return 0;

    if ((type == 0) || (type == 1)) {
	pcs->underline_floating = false;
	pcs->underline_position = dots(5);
    } else if (type == 3) {
	pcs->underline_floating = true;
	pcs->underline_position = 0.0;
    } else
	return 0;

    pcs->underline_enabled = true;
    pcs->underline_start = pcs->cap;
    return 0;
}

/*
 * ESC & d @
 *
 * Disable underlining
 */
  static int
pcl_disable_underline(
    pcl_args_t *    pargs,
    pcl_state_t *   pcs
)
{	
    /* apparently disabling underlining has the side effect of
       flushing any pending underlines.  This side effect is not
       documented */
    if (pcs->underline_enabled == true) {
        pcl_do_underline(pcs);
        pcs->underline_enabled = false;
    }
    return 0;
}

/* (From PCL5 Comparison Guide, p. 1-56) */

/*
 * ESC & t <method> P
 *
 * Select the text parsing method.
 */
  static int
pcl_text_parsing_method(
    pcl_args_t *    pargs,
    pcl_state_t *   pcs
)
{
    switch (int_arg(pargs)) {

      case 0: case 1:
        pcs->text_parsing_method = &pcl_tpm_0;
        break;

      case 21:
        pcs->text_parsing_method = &pcl_tpm_21;
        break;

      case 31:
        pcs->text_parsing_method = &pcl_tpm_31;
        break;

      case 38:
        pcs->text_parsing_method = &pcl_tpm_38;
        break;

      default:
        return e_Range;
    }

    return 0;
}

/* (From PCL5 Comparison Guide, p. 1-57) */

/*
 * ESC & c <direction> T
 *
 * Set the text path direction - not yet implemented.
 */
  static int
pcl_text_path_direction(
    pcl_args_t *    pargs,
    pcl_state_t *   pcs
)
{
    int             direction = int_arg(pargs);

    switch (direction) {

    case 0:
    case 1:
    case -1:
	break;

    default:
	return e_Range;
    }

    pcs->text_path = direction;
    return 0;
}

/* ------ Initialization ------ */
  static int
pctext_do_registration(
    pcl_parser_state_t *pcl_parser_state,
    gs_memory_t *   mem
)
{
    /* Register commands */
    DEFINE_CONTROL(0, "(plain char)", pcl_plain_char);

    DEFINE_CLASS('&')
    {
        'p', 'X',
	PCL_COMMAND("Transparent Mode", pcl_transparent_mode, pca_bytes)
    },
    {
        'd', 'D',
        PCL_COMMAND( "Enable Underline",
                     pcl_enable_underline,
		     pca_neg_ignore | pca_big_ignore
                     )
    },
    {
        'd', '@',
	PCL_COMMAND( "Disable Underline",
                     pcl_disable_underline,
		     pca_neg_ignore | pca_big_ignore
                     )
    },
    END_CLASS

    DEFINE_CLASS('&')
    {
        't', 'P',
        PCL_COMMAND( "Text Parsing Method",
                     pcl_text_parsing_method,
		     pca_neg_error | pca_big_error
                     )
    },
    {
        'c', 'T',
        PCL_COMMAND( "Text Path Direction",
                     pcl_text_path_direction,
		     pca_neg_ok | pca_big_error
                     )
    },
    END_CLASS

    DEFINE_CONTROL(1, "(plain char)", pcl_plain_char);	/* default "command" */

    return 0;
}

  static void
pctext_do_reset(
    pcl_state_t *       pcs,
    pcl_reset_type_t    type
)
{
    static  const uint  mask = (  pcl_reset_initial
                                | pcl_reset_printer
                                | pcl_reset_overlay );

    if ((type & mask) != 0) {
        pcs->underline_enabled = false;
	pcs->last_was_BS = false;
	pcs->last_width = inch2coord(1.0 / 10.0);
	pcs->text_parsing_method = &pcl_tpm_0;
	pcs->text_path = 0;
    }
}

const pcl_init_t    pctext_init = { pctext_do_registration, pctext_do_reset, 0 };
