/* Copyright (C) 1996, 1997 Aladdin Enterprises.  All rights reserved.
 * Unauthorized use, copying, and/or distribution prohibited.
 */

/* pcfont.c */
/* PCL5 font selection and text printing commands */
#include "memory_.h"
/* XXX #include tangle:  We need pl_load_tt_font, which is only supplied
 * by plfont.h if stdio.h is included, but pcstate.h back-door includes
 * plfont.h, so we have to pull in stdio.h here even though it should only
 * be down local to the font-init-hack code. */
#include "stdio_.h"
#include "pcommand.h"
#include "plvalue.h"
#include "pcstate.h"
#include "pcfont.h"
#include "pcdraw.h"
#include "pcsymbol.h"
#include "gsmatrix.h"
#include "gscoord.h"
#include "gspaint.h"
#include "gspath.h"
#include "gsstate.h"
#include "gsutil.h"
#include "gxfixed.h"
#include "gxchar.h"
#include "gxfont.h"
#include "gxstate.h"

/*
 * It appears that the default symbol set in the LJ 6MP is PC-8,
 * not Roman-8.  Eventually we will have to deal with this in a more
 * systematic way, but for now, we just define it at compile time.
 */
#define ROMAN_8_SYMBOL_SET 277
/*#define DEFAULT_SYMBOL_SET 277*/	/* Roman-8 */
#define DEFAULT_SYMBOL_SET 341		/* PC-8 */

/* pseudo-"dots" (actually 1/300" units) used in underline only */
#define	dots(n)	((float)(7200 / 300 * n))


/*
 * Decache the HMI after resetting the font.
 */
#define pcl_decache_hmi(pcls)\
  do {\
    if ( pcls->hmi_set == hmi_set_from_font )\
      pcls->hmi_set = hmi_not_set;\
  } while (0)\


/* Clear the font pointer cache after changing a font parameter.  set
 * indicates which font (0/1 for primary/secondary).  -1 means both. */
void
pcl_decache_font(pcl_state_t *pcls, int set)
{	
	if ( set < 0 )
	  {
	    pcl_decache_font(pcls, 0);
	    pcl_decache_font(pcls, 1);
	  }
	else
	  {
	    pcls->font_selection[set].font = NULL;
	    if ( pcls->font_selected == set )
	      {
	        pcls->font = NULL;
		pcls->map = NULL;
		pcl_decache_hmi(pcls);
	      }
	  }
}


/* Vector for scoring how well a font matches selection criteria.  It
 * would be nice to do this with a single scalar, but pitch/height in
 * particular require too much info.  Elements of the vector correspond
 * to the selection criteria, in order - TRM Ch 8. */
typedef enum {
  score_first = 0,
  score_symbol_set=score_first,
  score_spacing,
  score_pitch,
  score_height,
  score_style,
  score_weight,
  score_typeface,
  score_location,
  score_orientation,
  score_limit,
} score_index_t;
typedef	int match_score_t[score_limit];


/* Decide whether an unbound font supports a symbol set (mostly convenient
 * setup for pcl_check_symbol_support(). */
private bool
check_support(const pcl_state_t *pcls, uint symbol_set, pl_font_t *fp,
    pl_symbol_map_t **mapp)
{
	pl_glyph_vocabulary_t gv = ~fp->character_complement[7] & 07;
	byte id[2];
	pl_symbol_map_t *map;

	id[0] = symbol_set >> 8;
	id[1] = symbol_set;
	if ( (map = pcl_find_symbol_map(pcls, id, gv)) == NULL )
	  return false;
	if ( pcl_check_symbol_support(map->character_requirements,
	    fp->character_complement) )
	  {
	    *mapp = map;
	    return true;
	  }
	else
	  return false;
}


/* Compute a font's score against selection parameters.  TRM 8-27.
 * Also set *mapp to the symbol map to be used if this font wins. */
private void
score_match(const pcl_state_t *pcls, const pcl_font_selection_t *pfs,
    const pl_font_t *fp, pl_symbol_map_t **mapp, match_score_t score)
{
	int tscore;

	/* 1.  Symbol set.  2 for exact match or full support, 1 for
	 * default supported, 0 for no luck. */
	if ( pl_font_is_bound(fp) )
	  {
	    score[score_symbol_set] =
		pfs->params.symbol_set == fp->params.symbol_set? 2:
		    (fp->params.symbol_set == pcl_default_symbol_set_value);
	    *mapp = NULL;	/* bound fonts have no map */
	  }
	else
	  score[score_symbol_set] =
	    check_support(pcls, pfs->params.symbol_set, fp, mapp)? 2:
	      check_support(pcls, pcl_default_symbol_set_value, fp, mapp);

	/* 2.  Spacing. */
	score[score_spacing] =
	  pfs->params.proportional_spacing == fp->params.proportional_spacing;

	/* 3.  Pitch. */
	/* Need to score this so that (1) exact match is highest, (2) any
	 * higher-than-requested pitch is better than any lower-than-
	 * requested pitch, (3) within these categories, nearer is better. */
	if ( pfs->params.proportional_spacing )
	    score[score_pitch] = 0;	/* should not influence score */
	else
	  { /* The magic hex constants here allow 24 bits + sign to resolve
	     * the meaning of "close".  They are deliberately NOT #defined
	     * elsewhere because they have no meaning outside this block.  */
	    if ( pl_font_is_scalable(fp) )
	      /* scalable; match is effectively exact */
	      score[score_pitch] = 0x2000000;
	    else
	      { int delta = fp->params.pitch_100ths - pfs->params.pitch_100ths;

		/* If within one unit, call it exact; otherwise give
		 * preference to higher pitch than requested. */
		if ( abs(delta) <= 1 )
		  score[score_pitch] = 0x2000000;
		else if ( delta > 0 )
		  score[score_pitch] = 0x2000000 - delta;
		else
		  score[score_pitch] = 0x1000000 + delta;
	      }
	  }

	/* 4.  Height. */
	/* Closest match scores highest (no preference for + or -). Otherwise
	 * similar to pitch, and again, values assigned have no meaning out-
	 * side this code. */
	if ( pl_font_is_scalable(fp) )
	  score[score_height] = 0x1000000;
	else
	  { int delta = abs(pfs->params.height_4ths - fp->params.height_4ths);

	    /* As before, allow one unit of error to appear "exact".  */
	    if ( delta <= 1 )
	      delta = 0;
	    score[score_height] = 0x1000000 - delta;
	  }

	/* 5.  Style. */
	score[score_style] = pfs->params.style == fp->params.style;

	/* 6.  Stroke Weight. */
	/* If not exact match, prefer closest value away from 0 (more
	 * extreme).  If none, then prefer closest value nearer 0.  Once
	 * again, the actual values assigned here have no meaning outside
	 * this chunk of code. */
	{ /* Worst cases (font value toward zero from request) 0..13.
	   * Nearest more extreme: 14..21.  21 is exact.  */
	  int fwt = fp->params.stroke_weight;
	  int pwt = pfs->params.stroke_weight;
	  int delta = pwt - fwt;

	  /* With a little time, this could be collapsed. */
	  if ( pwt >= 0 )
	    if ( fwt >= pwt )
	      tscore = 21 + delta;
	    else
	      tscore = 14 - delta;
	  else
	    if ( fwt <= pwt )
	      tscore = 21 - delta;
	    else
	      tscore = 14 + delta;
	  score[score_weight] = tscore;
	}

	/* 7.  Typeface family. */
	{ uint diff = pfs->params.typeface_family ^ fp->params.typeface_family;

	  /* Give partial credit for the same typeface from a different */
	  /* vendor. */
	  score[score_typeface] =
	    (diff == 0 ? 2 : !(diff & 0xfff) ? 1 : 0);
	}

	/* 8. Location. */
	/* Rearrange the value from "storage" into priority for us.  We
	 * only care that we get the priority sequence: soft-font, cartridge,
	 * SIMM, and internal, and that we order at least 2 cartridges.  (TRM
	 * implies that SIMMs are *not* to be ordered.)  Once again, values
	 * assigned here have no external meaning. */
	/* 1 bit at bottom for SIMM _vs_ internal, then cartridge-number
	 * bits, then soft-font bit at top. */
	if ( fp->storage & pcds_downloaded )
	  tscore = 1 << (pcds_cartridge_max + 1);
	else if ( fp->storage & pcds_all_cartridges )
	  tscore = (fp->storage & pcds_all_cartridges) >>
	      (pcds_cartridge_shift - 1);
	else
	  /* no priority among SIMMs, and internal is 0 */
	  tscore = (fp->storage & pcds_all_simms)? 1: 0;
	score[score_location] = tscore;

	/* 9.  Orientation */
	if ( fp->scaling_technology != plfst_bitmap )
	  score[score_orientation] = 1;
	else
	  { pcl_font_header_t *fhp = (pcl_font_header_t *)(fp->header);

	    score[score_orientation] = fhp?
		fhp->Orientation == pcls->orientation:
		0;
	  }
}


/* Recompute the current font from the descriptive parameters. */
/* We export this so we can call it for setting the HMI. */
int
pcl_recompute_font(pcl_state_t *pcls)
{	int set = pcls->font_selected;
	pcl_font_selection_t *pfs = &pcls->font_selection[set];

	if ( pfs->font == 0 )
	  { pl_dict_enum_t dictp;
	    gs_const_string key;
	    void *value;
	    pl_font_t *best_font = 0;
	    pl_symbol_map_t *mapp, *best_map;
	    match_score_t best_match;

	    /* Initialize the best match to be worse than any real font. */
	    best_match[0] = -1;
	    pl_dict_enum_begin(&pcls->soft_fonts, &dictp);
	    while ( pl_dict_enum_next(&dictp, &key, &value) )
	      { pl_font_t *fp = (pl_font_t *)value;
		match_score_t match;
		score_index_t i;

		score_match(pcls, pfs, fp, &mapp, match);
		for (i=0; i<score_limit; i++)
		  if ( match[i] != best_match[i] )
		    {
		      if ( match[i] > best_match[i] )
			{
			  best_font = fp;
			  best_map = mapp;
			  memcpy((void*)best_match, (void*)match,
			      sizeof(match));
			}
		      break;
		    }
	      }
	    if ( best_font == 0 )
	      return e_Unimplemented;	/* no fonts */
	    pfs->font = best_font;
	    pfs->map = best_map;
	  }
	pcls->font = pfs->font;
	pcls->map = pfs->map;
	pfs->selected_by_id = false;
	return 0;
}

/* Next-character procedures, with symbol mapping. */
private gs_char
pcl_map_symbol(uint chr, const gs_show_enum *penum)
{	pcl_state_t *pcls = gs_state_client_data(penum->pgs);
	const pl_symbol_map_t *psm =
	  pcls->font_selection[pcls->font_selected].map;
	uint first_code;

	if ( psm == 0 )
	  return chr;
	first_code = pl_get_uint16(psm->first_code);
	if ( chr < first_code || chr > pl_get_uint16(psm->last_code) )
	  return 0xffff;
	return psm->codes[chr - first_code];
}
private int
pcl_next_char_8(gs_show_enum *penum, gs_char *pchr)
{	if ( penum->index == penum->str.size )
	  return 2;
	*pchr = pcl_map_symbol(penum->str.data[penum->index++], penum);
	return 0;
}

/* Show a string, possibly substituting a modified HMI for all characters */
/* or only the space character. */
typedef enum {
  char_adjust_none,
  char_adjust_space,
  char_adjust_all
} pcl_char_adjust_t;
private int
pcl_char_width(gs_show_enum *penum, gs_state *pgs, byte chr, gs_point *ppt)
{	int code = gs_stringwidth_n_init(penum, pgs, &chr, 1);
	if ( code < 0 || (code = gs_show_next(penum)) < 0 )
	  return code;
	gs_show_width(penum, ppt);
	return 0;
}
private int
pcl_show_chars(gs_show_enum *penum, pcl_state_t *pcls, const gs_point *pscale,
  pcl_char_adjust_t adjust, const char *str, uint size)
{	gs_state *pgs = pcls->pgs;
	int code;
	floatp scaled_hmi = pcl_hmi(pcls) / pscale->x;
	floatp diff;

	/* Compute any width adjustment. */
	switch ( adjust )
	  {
	  case char_adjust_none:
	    break;
	  case char_adjust_space:
	    { gs_point space_width;

	      /* Find the actual width of the space character. */
	      pcl_char_width(penum, pgs, ' ', &space_width);
	      diff = scaled_hmi - space_width.x;
	    }
	    break;
	  case char_adjust_all:
	    { gs_point char_width;

	      /* Find some character that is defined in the font. */
	      /****** WRONG -- SHOULD GET IT SOMEWHERE ELSE ******/
	      { byte chr;
	        for ( chr = ' '; ; ++chr )
		  { pcl_char_width(penum, pgs, chr, &char_width);
		    if ( char_width.x != 0 )
		      break;
		  }
		diff = scaled_hmi - char_width.x;
	      }
	    }
	    break;
	  }
	if ( pcls->end_of_line_wrap )
	  { /*
	     * If end-of-line wrap is enabled, we must render one character
	     * at a time.  (It's a good thing this is used primarily for
	     * diagnostics.)
	     */
	    int i;
	    for ( i = 0; i < size; ++i )
	      { byte chr;
	        gs_point width, pt;
		floatp move;

		chr = str[i];
	        if ( adjust == char_adjust_all ||
		     (chr == ' ' && adjust == char_adjust_space)
		   )
		  { width.x = scaled_hmi;
		    move = diff;
		  }
		else
		  { pcl_char_width(penum, pgs, chr, &width);
		    move = 0.0;
		  }
		gs_currentpoint(pgs, &pt);
		/* Compensate for the fact that everything is scaled. */
		/* We should really handle this by scaling the font.... */
		if ( (pt.x + width.x) * pscale->x > pcls->right_margin )
		  { pcl_do_CR(pcls);
		    pcl_do_LF(pcls);
		    gs_moveto(pgs, pcls->cap.x / pscale->x,
			      pcls->cap.y / pscale->y);
		  }
		gs_show_n_init(penum, pgs, str + i, 1);
		gs_show_next(penum);
		if ( move != 0 )
		  gs_rmoveto(pgs, move, 0.0);
	      }
	  }
	else switch ( adjust )
	  {
	  case char_adjust_none:
	    code = gs_show_n_init(penum, pgs, str, size);
	    break;
	  case char_adjust_space:
	    code = gs_widthshow_n_init(penum, pgs, diff, 0.0, ' ', str, size);
	    break;
	  case char_adjust_all:
	    code = gs_ashow_n_init(penum, pgs, diff, 0.0, str, size);
	    break;
	  }
	if ( code < 0 )
	  return code;
	return gs_show_next(penum);
}

/* Commands */

int
pcl_text(const byte *str, uint size, pcl_state_t *pcls)
{	gs_memory_t *mem = pcls->memory;
	gs_state *pgs = pcls->pgs;
	gs_show_enum *penum;
       	gs_matrix user_ctm, font_ctm, font_only_mat;
	pcl_font_selection_t *pfp;
	coord_point initial;
	gs_point gs_width;
	gs_point scale;
	int scale_sign;
	pcl_char_adjust_t adjust = char_adjust_none;
	int code = 0;

	if ( pcls->font == 0 )
	  { code = pcl_recompute_font(pcls);
	    if ( code < 0 )
	      return code;
	  }
	pfp = &pcls->font_selection[pcls->font_selected];
	/**** WE COULD CACHE MORE AND DO LESS SETUP HERE ****/
	pcl_set_graphics_state(pcls, true);
	code = pcl_set_drawing_color(pcls, pcls->pattern_type,
				     &pcls->current_pattern_id);
	if ( code < 0 )
	  return code;
	{ gs_font *pfont = (gs_font *)pcls->font->pfont;
	  /*
	   * Set up for single-byte character parsing.  Eventually we will
	   * use the text parsing method to generalize this....
	   */
	  pfont->procs.next_char = pcl_next_char_8;
	  gs_setfont(pgs, pfont);
	}
	penum = gs_show_enum_alloc(mem, pgs, "pcl_plain_char");
	if ( penum == 0 )
	  return_error(e_Memory);
	gs_moveto(pgs, (floatp)pcls->cap.x, (floatp)pcls->cap.y);

	if ( pcls->font->scaling_technology == plfst_bitmap )
	  {
	    scale.x = pcl_coord_scale / pcls->font->resolution.x;
	    scale.y = pcl_coord_scale / pcls->font->resolution.y;
	    /*
	     * Bitmap fonts use an inverted coordinate system,
	     * the same as the usual PCL system.
	     */
	    scale_sign = 1;
	  }
	else
	  {
	    /*
	     * Outline fonts are 1-point; the font height is given in
	     * (quarter-)points.
	     */
	    scale.x = scale.y = pfp->params.height_4ths * 0.25
	      * inch2coord(1.0/72);
	    /*
	     * Scalable fonts use an upright coordinate system,
	     * the opposite from the usual PCL system.
	     */
	    scale_sign = -1;
	  }

	/* If floating underline is on, since we're about to print a real
	 * character, track the best-underline position.
	 * XXX Until we have the font's design value for underline position,
	 * use 0.2 em.  This is enough to almost clear descenders in typical
	 * fonts; it's also large enough for us to check that the mechanism
	 * works. */
	if ( pcls->underline_enabled && pcls->underline_floating )
	  {
	    float yu = scale.y / 5.0;
	    if ( yu > pcls->underline_position )
	      pcls->underline_position = yu;
	  }

	/* Keep copies of both the user-space CTM and the font-space
	 * (font scale factors * user space) CTM because we flip back and
	 * forth to deal with effect of past backspace and holding info
	 * for future backspace.
	 * XXX I'm using the more general, slower approach rather than
	 * just multiplying/dividing by scale factors, in order to keep it
	 * correct through orientation changes.  Various parts of this should
	 * be cleaned up when performance time rolls around. */
	gs_currentmatrix(pgs, &user_ctm);
	/* possibly invert text because HP coordinate system is inverted */
	scale.y *= scale_sign;
	gs_scale(pgs, scale.x, scale.y);
	gs_currentmatrix(pgs, &font_ctm);

	/*
	 * If this is a fixed-pitch font and we have an explicitly set HMI,
	 * override the HMI.
	 */
	if ( !pcls->font->params.proportional_spacing &&
	     pcls->hmi_set == hmi_set_explicitly
	   )
	  adjust = char_adjust_all;
	else
	  { /*
	     * If the string has any spaces and the font doesn't define the
	     * space character, we have to do something special.
	     */
	    uint i;
	    for ( i = 0; i < size; ++i )
	      if ( str[i] == ' ' )
		{ if ( !pl_font_includes_char(pcls->font, pcls->map, &font_ctm, ' ') )
		    adjust = char_adjust_space;
	          break;
		}
	  }

	/* Overstrike-center if needed.  Enter here in font space. */
	if ( pcls->last_was_BS )
	  {
	    coord_point prev = pcls->cap;
	    coord_point this_width, delta_BS;

	    /* Scale the width only by the font part of transformation so
	     * it ends up in PCL coordinates. */
	    gs_make_scaling(scale.x, scale.y, &font_only_mat);
	    if ( (code = pl_font_char_width(pcls->font, pcls->map,
		&font_only_mat, str[0], &gs_width)) != 0 )
	      goto x;

	    this_width.x = gs_width.x;	/* XXX just swapping types??? */
	    this_width.y = gs_width.y;
	    delta_BS = this_width;	/* in case just 1 char */
	    initial.x = prev.x + (pcls->last_width.x - this_width.x) / 2;
	    initial.y = prev.y + (pcls->last_width.y - this_width.y) / 2;
	    if ( initial.x != 0 || initial.y != 0 )
	      {
		gs_setmatrix(pgs, &user_ctm);
		gs_moveto(pgs, initial.x, initial.y);
		gs_setmatrix(pgs, &font_ctm);
	      }
	    if ( (code = pcl_show_chars(penum, pcls, &scale, adjust, str, 1)) != 0 )
	      goto x;
	    str++;  size--;
	    /* Now reposition to "un-backspace", *regardless* of the
	     * escapement of the character we just printed. */
	    pcls->cap.x = prev.x + pcls->last_width.x;
	    pcls->cap.y = prev.y + pcls->last_width.y;
	    gs_setmatrix(pgs, &user_ctm);
	    gs_moveto(pgs, pcls->cap.x, pcls->cap.y);
	    gs_setmatrix(pgs, &font_ctm);
	    pcls->last_width = delta_BS;
	  }

	/* Print remaining characters.  Enter here in font space. */
	if ( size > 0 )
	  { gs_point pt;
	    if ( size > 1 )
	      {
		if ( (code = pcl_show_chars(penum, pcls, &scale, adjust, str, size - 1)) != 0 )
		  goto x;
		str += size - 1;
		size = 1;
	      }

	    /* Print remaining characters.  Note that pcls->cap may be out
	     * of sync here. */
	    gs_setmatrix(pgs, &user_ctm);
	    gs_currentpoint(pgs, &pt);
	    initial.x = pt.x;
	    initial.y = pt.y;
	    gs_setmatrix(pgs, &font_ctm);
	    if ( (code = pcl_show_chars(penum, pcls, &scale, adjust, str, 1)) != 0 )
	      goto x;
	    gs_setmatrix(pgs, &user_ctm);
	    gs_currentpoint(pgs, &pt);
	    pcls->cap.x = pt.x;
	    pcls->cap.y = pt.y;
	    pcls->last_width.x = pcls->cap.x - initial.x;
	    pcls->last_width.y = pcls->cap.y - initial.y;
	  }
x:	if ( code > 0 )		/* shouldn't happen */
	  code = gs_note_error(gs_error_invalidfont);
	if ( code < 0 )
	  gs_show_enum_release(penum, mem);
	else
	  gs_free_object(mem, penum, "pcl_plain_char");
	pcls->have_page = true;
	pcls->last_was_BS = false;
	return code;
}


/* individual non-command/control characters */
int
pcl_plain_char(pcl_args_t *pargs, pcl_state_t *pcls)
{	byte str[1];
	str[0] = pargs->command;
	return pcl_text(str, 1, pcls);
}


/* draw underline up to current point, adjust status */
void
pcl_do_underline(pcl_state_t *pcls)
{
	if ( pcls->underline_start.x != pcls->cap.x ||
	     pcls->underline_start.y != pcls->cap.y )
	  {
	    gs_state *pgs = pcls->pgs;
	    float y = pcls->underline_start.y + pcls->underline_position;

	    pcl_set_graphics_state(pcls, true);
	    /* TRM says (8-34) that underline is 3 dots.  In a victory for
	     * common sense, it's not.  Rather, it's 0.01" (which *is* 3 dots
	     * at 300 dpi only)  */
	    gs_setlinewidth(pgs, dots(3));
	    gs_moveto(pgs, pcls->underline_start.x, y);
	    gs_lineto(pgs, pcls->cap.x, y);
	    gs_stroke(pgs);
	  }
	/* Fixed underline is 5 "dots" (actually 5/300") down.  Floating
	 * will be determined on the fly. */
	pcls->underline_position = pcls->underline_floating? 0.0: dots(5);
}

/* The font parameter commands all come in primary and secondary variants. */

private int
pcl_symbol_set(pcl_args_t *pargs, pcl_state_t *pcls, int set)
{	uint num = uint_arg(pargs);
	uint symset;

	if ( num > 1023 )
	  return e_Range;
	/* The following algorithm is from Appendix C of the */
	/* PCL5 Comparison Guide. */
	symset = (num << 5) + pargs->command - 64;
	if ( symset != pcls->font_selection[set].params.symbol_set )
	  {
	    pcls->font_selection[set].params.symbol_set = symset;
	    pcl_decache_font(pcls, set);
	  }
	return 0;
}
private int /* ESC ( <> others */
pcl_primary_symbol_set(pcl_args_t *pargs, pcl_state_t *pcls)
{	return pcl_symbol_set(pargs, pcls, 0);
}
private int /* ESC ) <> others */
pcl_secondary_symbol_set(pcl_args_t *pargs, pcl_state_t *pcls)
{	return pcl_symbol_set(pargs, pcls, 1);
}

private int
pcl_spacing(pcl_args_t *pargs, pcl_state_t *pcls, int set)
{	uint spacing = uint_arg(pargs);

	if ( spacing > 1 )
	  return 0;
	if ( spacing != pcls->font_selection[set].params.proportional_spacing )
	  {
	    pcls->font_selection[set].params.proportional_spacing = spacing;
	    pcl_decache_font(pcls, set);
	  }
	return 0;
}
private int /* ESC ( s <prop_bool> P */
pcl_primary_spacing(pcl_args_t *pargs, pcl_state_t *pcls)
{	return pcl_spacing(pargs, pcls, 0);
}
private int /* ESC ) s <prop_bool> P */
pcl_secondary_spacing(pcl_args_t *pargs, pcl_state_t *pcls)
{	return pcl_spacing(pargs, pcls, 1);
}

private int
pcl_pitch(pcl_args_t *pargs, pcl_state_t *pcls, int set)
{	uint pitch_100ths = (uint)(float_arg(pargs) * 100 + 0.5);
	pcl_font_selection_t *pfs = &pcls->font_selection[set];

	if ( pitch_100ths > 65535 )
	  return e_Range;
	if ( pitch_100ths != pfs->params.pitch_100ths )
	  {
	    pfs->params.pitch_100ths = pitch_100ths;
	    if ( !pfs->selected_by_id )
	      pcl_decache_font(pcls, set);
	  }
	return 0;
}
private int /* ESC ( s <pitch> H */
pcl_primary_pitch(pcl_args_t *pargs, pcl_state_t *pcls)
{	return pcl_pitch(pargs, pcls, 0);
}
private int /* ESC ) s <pitch> H */
pcl_secondary_pitch(pcl_args_t *pargs, pcl_state_t *pcls)
{	return pcl_pitch(pargs, pcls, 1);
}

private int
pcl_height(pcl_args_t *pargs, pcl_state_t *pcls, int set)
{	uint height_4ths = (uint)(float_arg(pargs) * 4 + 0.5);
	pcl_font_selection_t *pfs = &pcls->font_selection[set];

	if ( height_4ths >= 4000 )
	  return e_Range;
	if ( height_4ths != pfs->params.height_4ths )
	  {
	    pfs->params.height_4ths = height_4ths;
	    if ( !pfs->selected_by_id )
	      pcl_decache_font(pcls, set);
	  }
	return 0;
}
private int /* ESC ( s <height> V */
pcl_primary_height(pcl_args_t *pargs, pcl_state_t *pcls)
{	return pcl_height(pargs, pcls, 0);
}
private int /* ESC ) s <height> V */
pcl_secondary_height(pcl_args_t *pargs, pcl_state_t *pcls)
{	return pcl_height(pargs, pcls, 1);
}

private int
pcl_style(pcl_args_t *pargs, pcl_state_t *pcls, int set)
{	uint style = uint_arg(pargs);

	if ( style != pcls->font_selection[set].params.style )
	  {
	    pcls->font_selection[set].params.style = style;
	    pcl_decache_font(pcls, set);
	  }
	return 0;
}
private int /* ESC ( s <style> S */
pcl_primary_style(pcl_args_t *pargs, pcl_state_t *pcls)
{	return pcl_style(pargs, pcls, 0);
}
private int /* ESC ) s <style> S */
pcl_secondary_style(pcl_args_t *pargs, pcl_state_t *pcls)
{	return pal_style(pargs, pcls, 1);
}

private int
pcl_stroke_weight(pcl_args_t *pargs, pcl_state_t *pcls, int set)
{	int weight = int_arg(pargs);

	if ( weight < -7 )
	  weight = -7;
	else if ( weight > 7 )
	  weight = 7;
	if ( weight != pcls->font_selection[set].params.stroke_weight )
	  {
	    pcls->font_selection[set].params.stroke_weight = weight;
	    pcl_decache_font(pcls, set);
	  }
	return 0;
}
private int /* ESC ( s <weight> B */
pcl_primary_stroke_weight(pcl_args_t *pargs, pcl_state_t *pcls)
{	return pcl_stroke_weight(pargs, pcls, 0);
}
private int /* ESC ) s <weight> B */
pcl_secondary_stroke_weight(pcl_args_t *pargs, pcl_state_t *pcls)
{	return pcl_stroke_weight(pargs, pcls, 1);
}

private int
pcl_typeface(pcl_args_t *pargs, pcl_state_t *pcls, int set)
{	uint typeface = uint_arg(pargs);

	if ( typeface != pcls->font_selection[set].params.typeface_family )
	  {
	    pcls->font_selection[set].params.typeface_family = typeface;
	    pcl_decache_font(pcls, set);
	  }
	return 0;
}
private int /* ESC ( s <typeface> T */
pcl_primary_typeface(pcl_args_t *pargs, pcl_state_t *pcls)
{	return pcl_typeface(pargs, pcls, 0);
}
private int /* ESC ) s <typeface> T */
pcl_secondary_typeface(pcl_args_t *pargs, pcl_state_t *pcls)
{	return pcl_typeface(pargs, pcls, 1);
}

private int
pcl_font_selection_id(pcl_args_t *pargs, pcl_state_t *pcls, int set)
{	uint id = uint_arg(pargs);
	byte id_key[2];
	void *value;
	pcl_font_selection_t *pfs;
	pl_font_t *fp;

	id_key[0] = id >> 8;
	id_key[1] = (byte)id;
	if ( !pl_dict_find(&pcls->soft_fonts, id_key, 2, &value) )
	  return 0;		/* font not found */
	fp = (pl_font_t *)value;
	pfs = &pcls->font_selection[set];

	/* Transfer parameters from the selected font into the selection
	 * parameters, being careful with the softer parameters. */
	pfs->font = fp;
	pfs->selected_by_id = true;
	pfs->map = 0;
	if ( pl_font_is_bound(fp) )
	  pfs->params.symbol_set = fp->params.symbol_set;
	else
	  { if ( check_support(pcls, pfs->params.symbol_set, fp, &pfs->map) )
	      DO_NOTHING;
	    else if ( check_support(pcls, pcl_default_symbol_set_value,
				    fp, &pfs->map)
		    )
	      DO_NOTHING;
	    else
	      { /*
		 * This font doesn't support the required symbol set.
		 * Punt -- map 1-for-1.
		 */
	      }
	  }
		 
	pfs->params.proportional_spacing = fp->params.proportional_spacing;
	if ( !pfs->params.proportional_spacing && !pl_font_is_scalable(fp) )
	  pfs->params.pitch_100ths = fp->params.pitch_100ths;
	if ( !pl_font_is_scalable(fp) )
	  pfs->params.height_4ths = fp->params.height_4ths;
	pfs->params.style = fp->params.style;
	pfs->params.stroke_weight = fp->params.stroke_weight;
	pfs->params.typeface_family = fp->params.typeface_family;
	if ( pcls->font_selected == set )
	  pcls->font = fp;
	pcl_decache_hmi(pcls);
	return 0;
}
private int /* ESC ( <font_id> X */
pcl_primary_font_selection_id(pcl_args_t *pargs, pcl_state_t *pcls)
{	return pcl_font_selection_id(pargs, pcls, 0);
}
private int /* ESC ) <font_id> X */
pcl_secondary_font_selection_id(pcl_args_t *pargs, pcl_state_t *pcls)
{	return pcl_font_selection_id(pargs, pcls, 1);
}

private int
pcl_select_default_font(pcl_args_t *pargs, pcl_state_t *pcls, int set)
{	if ( int_arg(pargs) != 3 )
	  return e_Range;
	pcl_decache_font(pcls, set);
	return e_Unimplemented;
}
private int /* ESC ( 3 @ */
pcl_select_default_font_primary(pcl_args_t *pargs, pcl_state_t *pcls)
{	return pcl_select_default_font(pargs, pcls, 0);
}
private int /* ESC ) 3 @ */
pcl_select_default_font_secondary(pcl_args_t *pargs, pcl_state_t *pcls)
{	return pcl_select_default_font(pargs, pcls, 1);
}

private int /* ESC & p <count> X */
pcl_transparent_mode(pcl_args_t *pargs, pcl_state_t *pcls)
{	return pcl_text(arg_data(pargs), uint_arg(pargs), pcls);
}

private int /* ESC & d <0|3> D */
pcl_enable_underline(pcl_args_t *pargs, pcl_state_t *pcls)
{	switch ( int_arg(pargs) )
	  {
	  case 0:
	    pcls->underline_floating = false;
	    pcls->underline_position = dots(5);
	    break;
	  case 3:
	    pcls->underline_floating = true;
	    pcls->underline_position = 0.0;
	    break;
	  default:
	    return 0;
	  }
	pcls->underline_enabled = true;
	pcls->underline_start = pcls->cap;
	return 0;
}

private int /* ESC & d @ */
pcl_disable_underline(pcl_args_t *pargs, pcl_state_t *pcls)
{	
	pcl_break_underline(pcls);
	pcls->underline_enabled = false;
	return 0;
}

private int /* SO */
pcl_SO(pcl_args_t *pargs, pcl_state_t *pcls)
{	if ( pcls->font_selected != 1 )
	  { pcls->font_selected = 1;
	    pcls->font = pcls->font_selection[1].font;
	    pcl_decache_hmi(pcls);
	  }
	return 0;
}

private int /* SI */
pcl_SI(pcl_args_t *pargs, pcl_state_t *pcls)
{	if ( pcls->font_selected != 0 )
	  { pcls->font_selected = 0;
	    pcls->font = pcls->font_selection[0].font;
	    pcl_decache_hmi(pcls);
	  }
	return 0;
}

/* (From PCL5 Comparison Guide, p. 1-56) */
private int /* ESC & t <method> P */
pcl_text_parsing_method(pcl_args_t *pargs, pcl_state_t *pcls)
{	int method = int_arg(pargs);
	switch ( method )
	  {
	  case 0: case 1:
	  case 21:
	  case 31:
	  case 38:
	    break;
	  default:
	    return e_Range;
	  }
	return e_Unimplemented;
}

/* (From PCL5 Comparison Guide, p. 1-57) */
private int /* ESC & c <direction> T */
pcl_text_path_direction(pcl_args_t *pargs, pcl_state_t *pcls)
{	int direction = int_arg(pargs);
	switch ( direction )
	  {
	  case 0: case -1:
	    break;
	  default:
	    return e_Range;
	  }
	return e_Unimplemented;
}

/* This command is listed only on p. A-7 of the PCL5 comparison guide. */
/* It is not referenced or documented anywhere! */
private int /* ESC & k <mode> S */
pcl_set_pitch_mode(pcl_args_t *pargs, pcl_state_t *pcls)
{	int mode = int_arg(pargs);
	switch ( mode )
	  {
	  case 0: case 2: case 4:
	    break;
	  default:
	    return e_Range;
	  }
	return e_Unimplemented;
}

/* Initialization */
private int
pcfont_do_init(gs_memory_t *mem)
{		/* Register commands */
	DEFINE_CONTROL(0, "(plain char)", pcl_plain_char);
	{ int chr;
	  /*
	   * The H-P manual only talks about A through Z, but it appears
	   * that characters from A through ^ (94) can be used.
	   */
	  for ( chr = 'A'; chr <= '^'; ++chr )
	    if ( chr != 'X' )
	      { DEFINE_CLASS_COMMAND_ARGS('(', 0, chr, "Primary Symbol Set",
					  pcl_primary_symbol_set,
					  pca_neg_error|pca_big_error);
	        DEFINE_CLASS_COMMAND_ARGS(')', 0, chr, "Secondary Symbol Set",
					  pcl_secondary_symbol_set,
					  pca_neg_error|pca_big_error);
	      }
	}
	DEFINE_CLASS('(')
	  {'s', 'P',
	     PCL_COMMAND("Primary Spacing", pcl_primary_spacing,
			 pca_neg_ignore|pca_big_ignore)},
	  {'s', 'H',
	     PCL_COMMAND("Primary Pitch", pcl_primary_pitch,
			 pca_neg_error|pca_big_error)},
	  {'s', 'V',
	     PCL_COMMAND("Primary Height", pcl_primary_height,
			 pca_neg_error|pca_big_error)},
	  {'s', 'S',
	     PCL_COMMAND("Primary Style", pcl_primary_style,
			 pca_neg_error|pca_big_clamp)},
	  {'s', 'B',
	     PCL_COMMAND("Primary Stroke Weight", pcl_primary_stroke_weight,
			 pca_neg_ok|pca_big_error)},
	  {'s', 'T',
	     PCL_COMMAND("Primary Typeface", pcl_primary_typeface,
			 pca_neg_error|pca_big_ok)},
	  {0, 'X',
	     PCL_COMMAND("Primary Font Selection ID",
			 pcl_primary_font_selection_id,
			 pca_neg_error|pca_big_error)},
	  {0, '@',
	     PCL_COMMAND("Default Font Primary",
			 pcl_select_default_font_primary,
			 pca_neg_error|pca_big_error)},
	END_CLASS
	DEFINE_CLASS(')')
	  {'s', 'P',
	     PCL_COMMAND("Secondary Spacing", pcl_secondary_spacing,
			 pca_neg_ignore|pca_big_ignore)},
	  {'s', 'H',
	     PCL_COMMAND("Secondary Pitch", pcl_secondary_pitch,
			 pca_neg_error|pca_big_error)},
	  {'s', 'V',
	     PCL_COMMAND("Secondary Height", pcl_secondary_height,
			 pca_neg_error|pca_big_error)},
	  {'s', 'S',
	     PCL_COMMAND("Secondary Style", pcl_secondary_style,
			 pca_neg_error|pca_big_clamp)},
	  {'s', 'B',
	     PCL_COMMAND("Secondary Stroke Weight",
			 pcl_secondary_stroke_weight,
			 pca_neg_ok|pca_big_error)},
	  {'s', 'T',
	     PCL_COMMAND("Secondary Typeface", pcl_secondary_typeface,
			 pca_neg_error|pca_big_ok)},
	  {0, 'X',
	     PCL_COMMAND("Secondary Font Selection ID",
			 pcl_secondary_font_selection_id,
			 pca_neg_error|pca_big_error)},
	  {0, '@',
	     PCL_COMMAND("Default Font Secondary",
			 pcl_select_default_font_secondary,
			 pca_neg_error|pca_big_error)},
	END_CLASS
	DEFINE_CLASS('&')
	  {'p', 'X',
	     PCL_COMMAND("Transparent Mode", pcl_transparent_mode,
			 pca_bytes)},
	  {'d', 'D',
	     PCL_COMMAND("Enable Underline", pcl_enable_underline,
			 pca_neg_ignore|pca_big_ignore)},
	  {'d', '@',
	     PCL_COMMAND("Disable Underline", pcl_disable_underline,
			 pca_neg_ignore|pca_big_ignore)},
	END_CLASS
	DEFINE_CONTROL(SO, "SO", pcl_SO)
	DEFINE_CONTROL(SI, "SI", pcl_SI)
	DEFINE_CLASS('&')
	  {'t', 'P',
	     PCL_COMMAND("Text Parsing Method", pcl_text_parsing_method,
			 pca_neg_error|pca_big_error)},
	  {'c', 'T',
	     PCL_COMMAND("Text Path Direction", pcl_text_path_direction,
			 pca_neg_ok|pca_big_error)},
	  {'k', 'S',
	     PCL_COMMAND("Set Pitch Mode", pcl_set_pitch_mode,
			 pca_neg_error|pca_big_error)},
	END_CLASS
	DEFINE_CONTROL(1, "(plain char)", pcl_plain_char);	/* default "command" */
	return 0;
}
private void
pcfont_do_reset(pcl_state_t *pcls, pcl_reset_type_t type)
{	if ( type & (pcl_reset_initial | pcl_reset_printer) )
	  { pcls->font_selection[0].params.symbol_set =
	      DEFAULT_SYMBOL_SET;
	    pcls->font_selection[0].params.proportional_spacing = false;
	    pcls->font_selection[0].params.pitch_100ths = 10*100;
	    pcls->font_selection[0].params.height_4ths = 12*4;
	    pcls->font_selection[0].params.style = 0;
	    pcls->font_selection[0].params.stroke_weight = 0;
	    pcls->font_selection[0].params.typeface_family = 3;	/* Courier */
	    pcls->font_selection[0].font = 0;		/* not looked up yet */
	    pcls->font_selection[1] = pcls->font_selection[0];
	    pcls->font_selected = 0;
	    pcls->font = 0;
	    pcls->underline_enabled = false;
	    pcls->last_was_BS = false;
	    pcls->last_width.x = pcls->hmi;
	    pcls->last_width.y = 0;
	  }

}

/* Load some built-in fonts.  This must be done at initialization time, but
 * after the state and memory are set up.  Return an indication of whether
 * at least one font was successfully loaded.
 * XXX The existing code is more than a bit of a hack; however it does
 * allow us to see some text.  Approach: expect to find some fonts in the
 * current directory (wherever the interpreter is executed) with names
 * *.ttf.  Load whichever ones are found in the table below.  Probably
 * very little of this code can be salvaged for later.
 */
bool
pcl_load_built_in_fonts(pcl_state_t *pcls, const char *prefixes[])
{	const char **pprefix;
#include "string_.h"
#include "gp.h"
	typedef struct font_hack {
	  const char *ext_name;
	  pl_font_params_t params;
	  byte character_complement[8];
	} font_hack_t;
	static const font_hack_t hack_table[] = {
	    /*
	     * Symbol sets, typeface family values, and character complements
	     * are faked; they do not (necessarily) match the actual fonts.
	     */
#define C(b) ((byte)((b) ^ 0xff))
#define cc_alphabetic\
  { C(0), C(0), C(0), C(0), C(0xff), C(0xc0), C(0), C(plgv_Unicode) }
#define cc_symbol\
  { C(0), C(0), C(0), C(4), C(0), C(0), C(0), C(plgv_Unicode) }
#define cc_dingbats\
  { C(0), C(0), C(0), C(1), C(0), C(0), C(0), C(plgv_Unicode) }
	    {"arial",	{0, 1, 0, 0, 0, 0, 16602}, cc_alphabetic },
	    {"arialbd",	{0, 1, 0, 0, 0, 3, 16602}, cc_alphabetic },
	    {"arialbi",	{0, 1, 0, 0, 1, 3, 16602}, cc_alphabetic },
	    {"ariali",	{0, 1, 0, 0, 1, 0, 16602}, cc_alphabetic },
	    {"cour",	{0, 0, 0, 0, 0, 0,  4099}, cc_alphabetic },
	    {"courbd",	{0, 0, 0, 0, 0, 3,  4099}, cc_alphabetic },
	    {"courbi",	{0, 0, 0, 0, 1, 3,  4099}, cc_alphabetic },
	    {"couri",	{0, 0, 0, 0, 1, 0,  4099}, cc_alphabetic },
	    {"times",	{0, 1, 0, 0, 0, 0, 16901}, cc_alphabetic },
	    {"timesbd",	{0, 1, 0, 0, 0, 3, 16901}, cc_alphabetic },
	    {"timesbi",	{0, 1, 0, 0, 1, 3, 16901}, cc_alphabetic },
	    {"timesi",	{0, 1, 0, 0, 1, 0, 16901}, cc_alphabetic },
	    {"symbol",	{621,1,0, 0, 0, 0, 16686}, cc_symbol },
	    {"wingding",{2730,1,0,0, 0, 0, 19114}, cc_dingbats },
	    {NULL,	{0, 0, 0, 0, 0, 0, 0} }
#undef C
#undef cc_alphabetic
#undef cc_symbol
#undef cc_dingbats
	};
	const font_hack_t *hackp;
	byte font_found[countof(hack_table)];
	bool found_some = false;
	byte key[3];

	/* This initialization was moved from the do_reset procedures to
	 * this point to localize the font initialization in the state
	 * and have it in a place where memory allocation is safe. */
	pcls->font_dir = gs_font_dir_alloc(pcls->memory);
	pl_dict_init(&pcls->built_in_fonts, pcls->memory, pl_free_font);
	pl_dict_init(&pcls->soft_fonts, pcls->memory, pl_free_font);
	pl_dict_set_parent(&pcls->soft_fonts, &pcls->built_in_fonts);

	    /* Load only those fonts we know about. */
	memset(font_found, 0, sizeof(font_found));
	/* We use a 3-byte key so the built-in fonts don't have */
	/* IDs that could conflict with user-defined ones. */
	key[0] = key[1] = key[2] = 0;
	for ( pprefix = prefixes; *pprefix != 0; ++pprefix )
	  for ( hackp = hack_table; hackp->ext_name != 0; ++hackp )
	    if ( !font_found[hackp - hack_table] )
	      { char fname[150];
	        FILE *fnp;
		pl_font_t *plfont;

	        strcpy(fname, *pprefix);
		strcat(fname, hackp->ext_name);
		strcat(fname, ".ttf");
		if ( (fnp=fopen(fname, gp_fmode_rb)) == NULL )
		  continue;
		if ( pl_load_tt_font(fnp, pcls->font_dir, pcls->memory,
				     gs_next_ids(1), &plfont) < 0 )
		  {
#ifdef DEBUG
		    lprintf1("Failed loading font %s\n", fname);
#endif
		    continue;
		  }
#ifdef DEBUG
		dprintf1("Loaded %s\n", fname);
#endif
		plfont->storage = pcds_internal;
		if ( hackp->params.symbol_set != 0 )
		  plfont->font_type = plft_8bit;
		/* Don't smash the pitch_100ths, which was obtained
		 * from the actual font. */
		{ uint save_pitch = plfont->params.pitch_100ths;
		  plfont->params = hackp->params;
		  plfont->params.pitch_100ths = save_pitch;
		}
		memcpy(plfont->character_complement,
		       hackp->character_complement, 8);
		pl_dict_put(&pcls->built_in_fonts, key, sizeof(key), plfont);
		key[sizeof(key) - 1]++;
		font_found[hackp - hack_table] = 1;
		found_some = true;
	    }
	return found_some;
}

const pcl_init_t pcfont_init = {
  pcfont_do_init, pcfont_do_reset
};
