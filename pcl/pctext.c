/* Copyright (C) 1996, 1997, 1998 Aladdin Enterprises.  All rights
   reserved.  Unauthorized use, copying, and/or distribution
   prohibited.  */

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
private const pcl_text_parsing_method_t pcl_tpm_0 = pcl_tpm_0_data,
                                        pcl_tpm_21 = pcl_tpm_21_data,
                                        pcl_tpm_31 = pcl_tpm_31_data,
                                        pcl_tpm_38 = pcl_tpm_38_data;

/* pseudo-"dots" (actually 1/300" units) used in underline only */
#define	dots(n)	    ((float)(7200 / 300 * n))


/*
 * Get next character procedure, to be passed to the graphic library font
 * machinery. Always assumes two bytes are in the string.
 */
  private int
get_gs_next_char(
    gs_text_enum_t *    penum,
    gs_char *           pchr,
    gs_glyph *          pglyph
)
{
    const byte *        pb = penum->text.data.bytes;
    if ( (penum->index / 2 ) == penum->text.size )
	return 2;
    *pglyph = gs_no_glyph;
    *pchr = (((uint)pb[penum->index]) << 8) + pb[penum->index+1];
    penum->index += 2;
    return 0;
}

/*
 * Install a font in the graphic state.
 */
  private void
set_gs_font(
    pcl_state_t *   pcs
)
{
    gs_font *       pfont = (gs_font *)pcs->font->pfont;

    pfont->procs.next_char_glyph = get_gs_next_char;
    gs_setfont(pcs->pgs, pfont);
}

/*
 * Mapping via symbol sets. Note that this applies only to unbound fonts.
 */
  private gs_char
map_symbol(
    const pl_font_t *       pfont,
    const pl_symbol_map_t * psm,
    gs_char                 chr
)
{
    uint                    first_code, last_code;

    /*
     * If there is no symbol map we assume the the character
     * implicitly indexes the font.
     */
    if (psm == 0) {
	if ( (pfont->scaling_technology == plfst_TrueType) &&
	     (pfont->storage == pcds_internal)               )
	    return chr + 0xf000;
	return chr;
    }

    first_code = pl_get_uint16(psm->first_code);
    last_code = pl_get_uint16(psm->last_code);

    /*
     * If chr is double-byte but the symbol map is only single-byte,
     * just return chr (it is not clear this make any sense - jan, I
     * agree - henry).  
     */
    if ((chr < first_code) || (chr > last_code))
	return ((last_code <= 0xff) && (chr > 0xff) ? chr : 0xffff);
    else {
	pl_glyph_vocabulary_t fgv =  /* font glyph vocabulary */
	    (pl_glyph_vocabulary_t)(~pfont->character_complement[7] & 07);
	gs_char code = psm->codes[chr - first_code];

	/* simple common case - the glyph vocabs match */
	if ( pl_symbol_map_vocabulary(psm) == fgv )
	    return code;
	/* font wants unicode and we have mapped an msl symbol set */
	if ( ( pl_symbol_map_vocabulary(psm) == plgv_MSL ) &&
	     ( fgv == plgv_Unicode ) ) {
	    if ( psm->mapping_type != PLGV_M2U_MAPPING )
		/* font selection should not have given us this */
		return_error(gs_error_invalidfont);
	    else
		return pl_map_MSL_to_Unicode(code,
		         (psm->id[0] << 8) + psm->id[1]);
	}
	/* font wants msl coding we have mapped unicode */
	if ( ( pl_symbol_map_vocabulary(psm) == plgv_Unicode ) &&
	     ( fgv == plgv_MSL ) ) {
	    if ( psm->mapping_type != PLGV_U2M_MAPPING )
		/* symbol set doesn't support the mapping - should not happen */
		return_error(gs_error_invalidfont);
	    else
		return pl_map_Unicode_to_MSL(code,
        		 (psm->id[0] << 8) + psm->id[1]);
	} else
            return 0xffff;
    }
}

/*
 * Check if a character code is considered "printable" by given symbol set.
 */
  private bool
is_printable(
    const pl_symbol_map_t * psm,
    gs_char                 chr
)
{
    if ((psm == 0) || (psm->type >= 2))
        return true;
    else if (psm->type == 1)
        chr &= 0x7f;
    return (chr >= ' ') && (chr <= '\177');
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
  private int
get_next_char(
    pcl_state_t *   pcs,
    const byte **   ppb,
    uint *          plen,
    gs_char *       pchr,
    bool *          pis_space,
    bool            literal,
    gs_point *      pwidth
)
{
    const byte *    pb = *ppb;
    int             len = *plen;
    gs_char         chr;
    if (len <= 0)
        return 2;
    chr = *pb++;
    len--;
    if (pcl_char_is_2_byte(chr, pcs->text_parsing_method) && (len > 0)) {
        chr = (chr << 8) + *pb++;
        len--;
        *pis_space = false;
    } else
	*pis_space = (chr == ' ' &&  pcs->font->storage == pcds_internal);
    *ppb = pb;
    *plen = len;

    /* check if the code is considered "printable" in the current symbol set */
    if (!is_printable(pcs->map, chr)) {
        *pis_space = literal;
        *pchr = 0xffff;
        return 0;
    }

    /* map the symbol. If it fails to map, quit now */
    chr = map_symbol(pcs->font, pcs->map, chr);
    *pchr = chr;
    if (chr == 0xffff) {
        *pis_space = true;
        return 0;
    }

    /* check if the character is in the font and get the character
       width at the same time */
    if ( pl_font_char_width(pcs->font, chr, pwidth) == 0 )
	return 0;
    /*
     * The character is not in the font.
     */
    *pis_space = true;
    *pchr = 0xffff;
    return 0;
}

/*
 * Draw the foreground of a character. For transparent text this is the only
 * part that must be drawn.
 */
  private int
show_char_foreground(
    const pcl_state_t * pcs,
    const char *        pbuff
)
{
    gs_text_enum_t *   penum;
    int                code = gs_show_begin(pcs->pgs, pbuff, 1, pcs->memory, &penum);

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
  private int
show_char_background(
    pcl_state_t *       pcs,
    const char *        pbuff
)
{
    gs_text_enum_t *    penum;
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
	gs_char         chr = (((uint)pbuff[0]) << 8) + pbuff[1];
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
        gs_image_cleanup(pen);
        gs_free_object(gs_state_memory(pgs), pen, "bitmap font background");

    } else {
	gs_rect     bbox;

        /* clear the path; start the new one from the current point */
	gs_newpath(pgs);
	gs_moveto(pgs, pt.x, pt.y);

        /* get the character path */
	gs_charpath_begin(pgs, pbuff, 1, true, pcs->memory, &penum);
	if ((code = gs_text_process(penum)) >= 0) {

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
 private floatp
pcl_get_width(pcl_state_t *pcs, gs_point *advance_vector, const gs_point *pscale, gs_char chr, bool is_space)
{
    pcl_font_selection_t *  pfp = &(pcs->font_selection[pcs->font_selected]);
    floatp width;
    if (chr != 0xffff) { 
	if (!pfp->params.proportional_spacing || is_space)
	    width = pcl_hmi(pcs);
	else {
	    if ( pcs->font->scaling_technology == plfst_TrueType ) {
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

  private int
pcl_show_chars_slow(
    pcl_state_t *           pcs,
    const gs_point *        pscale,
    const byte *            str,
    uint                    size,
    bool                    literal
)
{
    gs_state *              pgs = pcs->pgs;
    char                    buff[2];
    floatp                  rmargin = pcs->margins.right;
    floatp                  page_size = pcs->xfm_state.pd_size.x;
    bool                    opaque = !pcs->source_transparent;
    bool                    wrap = pcs->end_of_line_wrap;
    bool                    is_space = false;
    bool                    use_rmargin = (pcs->cap.x <= rmargin);
    gs_char                 chr;
    int                     code = 0;
    floatp                  width;
    gs_point                cpt;
    gs_point                advance_vector;

    cpt.x = pcs->cap.x;
    cpt.y = pcs->cap.y;
    while (get_next_char(pcs, &str, &size, &chr, &is_space, literal, &advance_vector) == 0) {
        floatp  tmp_x;

        /* check if a character was found */
        buff[0] = (chr >> 8);
        buff[1] = (chr & 0xff);
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
        if (pcs->last_was_BS)
            tmp_x += (pcs->last_width - width) / 2;
        gs_moveto(pgs, tmp_x / pscale->x, cpt.y / pscale->y);

        if (chr != 0xffff) {

            /* if source is opaque, show and opaque background */
            if (opaque)
                code = show_char_background(pcs, buff);
            if (code >= 0)
                code = show_char_foreground(pcs, buff);
            if (code < 0)
                break;
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

  private int
pcl_show_chars_fast(
    pcl_state_t *           pcs,
    const gs_point *        pscale,
    const byte *            str,
    uint                    size,
    bool                    literal
)
{
    gs_state *              pgs = pcs->pgs;
    floatp                  rmargin = pcs->margins.right;
    floatp                  page_size = pcs->xfm_state.pd_size.x;
    bool                    is_space = false;
    int                     code = 0;
    gs_char                 chr;
    floatp                  width;
    gs_point                cpt;
    gs_point                advance_vector;
    char                    *xyshow_buffp;
    float                   *xyshow_fvalsp;
    int                     xyshow_index;
    gs_text_enum_t          *penum;

    cpt.x = pcs->cap.x;
    cpt.y = pcs->cap.y;
    xyshow_index = 0;

    /* allocate arrays of coordinates and buffer for xyshow.  Note we
       are really doing xshow, the y displacement is alway 0.0 but the
       graphics library only has an interface for xyshow. */
    xyshow_fvalsp = (float *)gs_alloc_byte_array(pcs->memory, size, sizeof(float) * 2, "pcl_show_chars_fast");
    if ( xyshow_fvalsp == 0 )
	return_error(e_Memory);
    xyshow_buffp = (char *)gs_alloc_byte_array(pcs->memory, size, sizeof(char) * 2, "pcl_show_chars_fast");
    if ( xyshow_buffp == 0 ) {
	gs_free_object(pcs->memory, xyshow_fvalsp, "pcl_show_chars_fast" );
	return_error(e_Memory);
    }
    
    while (get_next_char(pcs, &str, &size, &chr, &is_space, literal, &advance_vector) == 0) {
        /* check if a character was found.  For an undefined character
           backup the index to accumulate hmi in the last characters */
	xyshow_buffp[xyshow_index * 2] = (chr >> 8);
	xyshow_buffp[xyshow_index * 2 + 1] = (chr & 0xff);
	/* round width to integral pcl current units */
	width = (pcl_get_width(pcs, &advance_vector, pscale, chr, is_space));
	/*
         * A special case occurs in the non-wrap situation when the current
         * position exactly equals the current margin. In that case, no
         * character is printed.
         */
	if (cpt.x == rmargin)
	    break;
	else if (cpt.x >= page_size) {
	    cpt.x = page_size;
	    break;
	}

	/* store the x displacement - accumulate the displacement for
           undefined characters which pcl treats as spaces.  We don't
           have to set the y displacement at position (xyshow_index *
           2 + 1) because it is always zero and the entire array was
           initialized to zero above. */
	xyshow_fvalsp[xyshow_index * 2] = (width / pscale->x);
	xyshow_fvalsp[xyshow_index * 2 + 1] = 0.0;
	xyshow_index++;

	/* the pcl cap moves the width of the character */
	cpt.x += width;
	/* check for crossing rmargin or exceeding page boundary */
	if (cpt.x > rmargin) {
	    cpt.x = rmargin;
	    break;
	} else if (cpt.x >= page_size) {
	    cpt.x = page_size;
	    break;
	}
    }
    /* finally move back to the original point, the start of the string */
    gs_moveto(pgs, pcs->cap.x / pscale->x, pcs->cap.y / pscale->y);

    /* start the "show" - we pass the same array of floats for
       parameters 4 and 5 - the graphics library does the right thing */
    code = gs_xyshow_begin(pcs->pgs, xyshow_buffp, xyshow_index,
			   xyshow_fvalsp, xyshow_fvalsp, xyshow_index * 2,
			   pcs->memory, &penum);
    if (code >= 0)
        code = gs_text_process(penum);
    gs_text_release(penum, "pcl_show_chars_fast");
    /* record the last width */
    pcs->last_width = width;
    /* update the current position in pcl's state */
    pcs->cap.x = cpt.x;
    pcs->cap.y = cpt.y;
    /* free everything */
    gs_free_object(pcs->memory, xyshow_fvalsp, "pcl_show_chars_fast" );
    gs_free_object(pcs->memory, xyshow_buffp, "pcl_show_chars_fast" );
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
        if (pfp->params.proportional_spacing) {
	    scale.x = scale.y = pfp->params.height_4ths
                                 * 0.25 * inch2coord(1.0 / ppi);
	} else
	    scale.x = scale.y = pl_fp_pitch_cp(&pfp->params)
	                         * (100.0 / pl_fp_pitch_cp(&pfp->font->params))
	                         * inch2coord(1.0 / (100.0 * ppi));
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
    /* Print remaining characters, restore the ctm */
    if ( (pcs->cap.x <= pcs->margins.right) && 
	 (pcs->source_transparent) &&
	 (!pcs->last_was_BS) &&
	 (!pcs->end_of_line_wrap) )
	code = pcl_show_chars_fast(pcs, &scale, str, size, literal);
    else 
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
    return pcl_text((const byte *)&(pargs->command), 1, pcs, false);
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
  private int
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
  private int
pcl_enable_underline(
    pcl_args_t *    pargs,
    pcl_state_t *   pcs
)
{
    int             type = int_arg(pargs);

    /* ignore command if underlining is already enabled */
    if (pcs->underline_enabled)
        return 0;

    if (type == 0) {
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
  private int
pcl_disable_underline(
    pcl_args_t *    pargs,
    pcl_state_t *   pcs
)
{	
    pcl_break_underline(pcs);
    pcs->underline_enabled = false;
    return 0;
}

/* (From PCL5 Comparison Guide, p. 1-56) */

/*
 * ESC & t <method> P
 *
 * Select the text parsing method.
 */
  private int
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
  private int
pcl_text_path_direction(
    pcl_args_t *    pargs,
    pcl_state_t *   pcs
)
{
    int             direction = int_arg(pargs);

    switch (direction) {

      case 0:
      case -1:
	break;

      default:
	return e_Range;
    }

    pcs->text_path = direction;
    return e_Unimplemented;
}

/* ------ Initialization ------ */
  private int
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

  private void
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
