/* Copyright (C) 1996 Aladdin Enterprises.  All rights reserved.
 * Unauthorized use, copying, and/or distribution prohibited.
 */

/* pcfont.c */
/* PCL5 font selection and text printing commands */
#include "std.h"
#include "pcommand.h"
#include "pcstate.h"
#include "pldict.h"
#include "plvalue.h"
#include "pcfont.h"
#include "pcdraw.h"
#include "gsmatrix.h"
#include "gschar.h"
#include "gscoord.h"
#include "gsfont.h"
#include "gspath.h"
#include "gsstate.h"

/* XXX allow us to compile, but mark the points we'll need to fix later */
#define	e_Screwup	e_Unimplemented

/* Internal routines */

/* Clear the font pointer cache after changing a font parameter. */
private void
decache_font(pcl_state_t *pcls, int set)
{	pcls->font_selection[set].font = 0;
	if ( pcls->font_selected == set )
	  pcls->font = 0;
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


/* Compute a font's score against selection parameters.  TRM 8-27. */
private void
score_match(const pcl_state_t *pcls, const pcl_font_selection_t *pfs,
    const pl_font_t *fp, match_score_t score)
{
	int tscore;

	/* 1.  Symbol set. */
	if ( pl_font_is_bound(fp) )
	  score[score_symbol_set] =
	      pfs->params.symbol_set == fp->params.symbol_set;
	else
	  {
	    /* XXX Decide whether unbound font supports the requested
	     * symbol set.  This requires the big ugly table.  */
	    score[score_symbol_set] = 37;	/* plug */
	  }

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
	score[score_typeface] =
	    pfs->params.typeface_family == fp->params.typeface_family;

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
private int
recompute_font(pcl_state_t *pcls)
{	int set = pcls->font_selected;
	pcl_font_selection_t *pfs = &pcls->font_selection[set];

	if ( pfs->font == 0 )
	  { pl_dict_enum_t dictp;
	    gs_const_string key;
	    void *value;
	    pl_font_t *best_font;
	    match_score_t best_match;

	    /* Be sure we've got at least one font. */
	    pl_dict_enum_begin(&pcls->soft_fonts, &dictp);
	    if ( !pl_dict_enum_next(&dictp, &key, &value) )
		return e_Screwup;
	    best_font = (pl_font_t *)value;
	    score_match(pcls, pfs, best_font, best_match);

	    while ( pl_dict_enum_next(&dictp, &key, &value) )
	      { pl_font_t *fp = (pl_font_t *)value;
		match_score_t match;
		score_index_t i;

		score_match(pcls, pfs, fp, match);
		for (i=0; i<score_limit; i++)
		  if ( match[i] != best_match[i] )
		    {
		      if ( match[i] > best_match[i] )
			{
			  best_font = fp;
			  memcpy((void*)best_match, (void*)match,
			      sizeof(match));
			}
		      break;
		    }
	      }
	    pfs->font = best_font;
	  }
	pcls->font = pfs->font;
	return 0;
}

/* Commands */

int
pcl_text(const byte *str, uint size, pcl_state_t *pcls)
{	gs_memory_t *mem = pcls->memory;
	gs_state *pgs = pcls->pgs;
	gs_show_enum *penum;
       	gs_matrix user_ctm, font_ctm;
	pcl_font_selection_t *pfp;
	coord_point initial;
	int code = 0;

	if ( pcls->font == 0 )
	  { int code = recompute_font(pcls);
	    if ( code < 0 )
	      return code;
	  }
	pfp = &pcls->font_selection[pcls->font_selected];
	/**** WE COULD CACHE MORE AND DO LESS SETUP HERE ****/
	pcl_set_ctm(pcls, true);
	gs_setfont(pgs, (gs_font *)pcls->font->pfont);
	penum = gs_show_enum_alloc(mem, pgs, "pcl_plain_char");
	if ( penum == 0 )
	  return_error(e_Memory);
	gs_moveto(pgs, (float)pcls->cap.x, (float)pcls->cap.y);
	{ float scale_x, scale_y;

	  if ( pcls->font->scaling_technology == plfst_bitmap )
	    {
	      scale_x = pcl_coord_scale / pcls->font->resolution.x;
	      scale_y = pcl_coord_scale / pcls->font->resolution.y;
	    }
	  else
	    {
	      if ( pfp->params.proportional_spacing )
		scale_x = scale_y = pfp->params.height_4ths * 25.0;
	      else
		/* XXX We need to ferret out the design factor that will
		 * give us the width of a character.  For now, wire in the
		 * assumption that it's 60% of height--this gives the
		 * common equivalence of 10 pt == 12 pitch, 12 pt == 10
		 * pitch.  */
		scale_x = scale_y = 7200.0 / 0.6 / pfp->params.pitch_100ths;
	    }
	  /* Keep copies of both the user-space CTM and the font-space
	   * (font scale factors * user space) CTM because we flip back and
	   * forth to deal with effect of past backspace and holding info
	   * for future backspace.
	   * XXX I'm using the more general, slower approach rather than
	   * just multiplying/dividing by scale factors. */
	  gs_currentmatrix(pgs, &user_ctm);
	  /* invert text because HP coordinate system is inverted */
	  gs_scale(pgs, scale_x, -scale_y);
	  gs_currentmatrix(pgs, &font_ctm);
	}

	/* Overstrike-center if needed.  Enter here in font space. */
	if ( pcls->last_was_BS )
	  { coord_point prev = pcls->cap;
	    coord_point this_width, delta_BS;

	    /* XXX Need width of new character in *user* space. */
	    this_width = pcls->last_width;	/* XXX DUMMY! */

	    delta_BS = this_width;	/* in case just 1 char */
	    initial.x = prev.x + (pcls->last_width.x - this_width.x) / 2;
	    initial.y = prev.y + (pcls->last_width.y - this_width.y) / 2;
	    if ( initial.x != 0 || initial.y != 0 )
	      {
		gs_setmatrix(pgs, &user_ctm);
		gs_moveto(pgs, initial.x, initial.y);
		gs_setmatrix(pgs, &font_ctm);
	      }
	    if ( (code = gs_show_n_init(penum, pgs, str, 1)) < 0 )
	      goto x;
	    if ( (code = gs_show_next(penum)) != 0 )
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
		if ( (code = gs_show_n_init(penum, pgs, str, size - 1)) < 0 )
		  goto x;
		if ( (code = gs_show_next(penum)) != 0)
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
	    if ( (code = gs_show_n_init(penum, pgs, str, 1)) < 0 )
	      goto x;
	    if ( (code = gs_show_next(penum)) != 0)
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
int /* individual non-command/control characters */
pcl_plain_char(pcl_args_t *pargs, pcl_state_t *pcls)
{	byte str[1];
	str[0] = pargs->command;
	return pcl_text(str, 1, pcls);
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
	    decache_font(pcls, set);
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
	    decache_font(pcls, set);
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

	if ( pitch_100ths > 65535 )
	  return e_Range;
	if ( pitch_100ths != pcls->font_selection[set].params.pitch_100ths )
	  {
	    pcls->font_selection[set].params.pitch_100ths = pitch_100ths;
	    decache_font(pcls, set);
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

	if ( height_4ths >= 4000 )
	  return e_Range;
	if ( height_4ths != pcls->font_selection[set].params.height_4ths )
	  {
	    pcls->font_selection[set].params.height_4ths = height_4ths;
	    decache_font(pcls, set);
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
	    decache_font(pcls, set);
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
	    decache_font(pcls, set);
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
	    decache_font(pcls, set);
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
	if ( pl_font_is_bound(fp) )
	    pfs->params.symbol_set = fp->params.symbol_set;
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
	decache_font(pcls, set);
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
	    pcls->floating_underline = false;
	    break;
	  case 3:
	    pcls->floating_underline = true;
	    break;
	  default:
	    return 0;
	  }
	pcls->underline_enabled = true;
	return 0;
}

private int /* ESC & d @ */
pcl_disable_underline(pcl_args_t *pargs, pcl_state_t *pcls)
{	pcls->underline_enabled = false;
	return 0;
}

private int /* SO */
pcl_SO(pcl_args_t *pargs, pcl_state_t *pcls)
{	if ( pcls->font_selected != 1 )
	  { pcls->font_selected = 1;
	    pcls->font = pcls->font_selection[1].font;
	  }
	return 0;
}

private int /* SI */
pcl_SI(pcl_args_t *pargs, pcl_state_t *pcls)
{	if ( pcls->font_selected != 0 )
	  { pcls->font_selected = 0;
	    pcls->font = pcls->font_selection[0].font;
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
	DEFINE_CONTROL(0, pcl_plain_char);
	{ int chr;
	  /*
	   * The H-P manual only talks about A through Z, but it appears
	   * that characters from A through ^ (94) can be used.
	   */
	  for ( chr = 'A'; chr <= '^'; ++chr )
	    if ( chr != 'X' )
	      { DEFINE_CLASS_COMMAND_ARGS('(', 0, chr, pcl_primary_symbol_set, pca_neg_error|pca_big_error);
	        DEFINE_CLASS_COMMAND_ARGS(')', 0, chr, pcl_secondary_symbol_set, pca_neg_error|pca_big_error);
	      }
	}
	DEFINE_CLASS('(')
	  {'s', 'P', {pcl_primary_spacing, pca_neg_ignore|pca_big_ignore}},
	  {'s', 'H', {pcl_primary_pitch, pca_neg_error|pca_big_error}},
	  {'s', 'V', {pcl_primary_height, pca_neg_error|pca_big_error}},
	  {'s', 'S', {pcl_primary_style, pca_neg_error|pca_big_clamp}},
	  {'s', 'B', {pcl_primary_stroke_weight, pca_neg_ok|pca_big_error}},
	  {'s', 'T', {pcl_primary_typeface, pca_neg_error|pca_big_ok}},
	  {0, 'X', {pcl_primary_font_selection_id, pca_neg_error|pca_big_error}},
	  {0, '@', {pcl_select_default_font_primary, pca_neg_error|pca_big_error}},
	END_CLASS
	DEFINE_CLASS(')')
	  {'s', 'P', {pcl_secondary_spacing, pca_neg_ignore|pca_big_ignore}},
	  {'s', 'H', {pcl_secondary_pitch, pca_neg_error|pca_big_error}},
	  {'s', 'V', {pcl_secondary_height, pca_neg_error|pca_big_error}},
	  {'s', 'S', {pcl_secondary_style, pca_neg_error|pca_big_clamp}},
	  {'s', 'B', {pcl_secondary_stroke_weight, pca_neg_ok|pca_big_error}},
	  {'s', 'T', {pcl_secondary_typeface, pca_neg_error|pca_big_ok}},
	  {0, 'X', {pcl_secondary_font_selection_id, pca_neg_error|pca_big_error}},
	  {0, '@', {pcl_select_default_font_secondary, pca_neg_error|pca_big_error}},
	END_CLASS
	DEFINE_CLASS('&')
	  {'p', 'X', {pcl_transparent_mode, pca_bytes}},
	  {'d', 'D', {pcl_enable_underline, pca_neg_ignore|pca_big_ignore}},
	  {'d', '@', {pcl_disable_underline, pca_neg_ignore|pca_big_ignore}},
	END_CLASS
	DEFINE_CONTROL(SO, pcl_SO)
	DEFINE_CONTROL(SI, pcl_SI)
	DEFINE_CLASS('&')
	  {'t', 'P', {pcl_text_parsing_method, pca_neg_error|pca_big_error}},
	  {'c', 'T', {pcl_text_path_direction, pca_neg_ok|pca_big_error}},
	  {'k', 'S', {pcl_set_pitch_mode, pca_neg_error|pca_big_error}},
	END_CLASS
	DEFINE_CONTROL(1, pcl_plain_char);	/* default "command" */
	return 0;
}
private void
pcfont_do_reset(pcl_state_t *pcls, pcl_reset_type_t type)
{	if ( type & (pcl_reset_initial | pcl_reset_printer) )
	  { pcls->font_selection[0].params.symbol_set = 277;	/* Roman-8 */
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
	if ( type & pcl_reset_initial )
	    pl_dict_init(&pcls->hard_fonts, pcls->memory, pl_free_font);

}
const pcl_init_t pcfont_init = {
  pcfont_do_init, pcfont_do_reset
};
