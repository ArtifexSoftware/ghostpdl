/* Copyright (C) 1996, 1997 Aladdin Enterprises.  All rights reserved.
 * Unauthorized use, copying, and/or distribution prohibited.
 */

/* pctext.c */
/* PCL5 text printing commands */
#include "std.h"
#include "plvalue.h"
#include "pcommand.h"
#include "pcstate.h"
#include "pcdraw.h"
#include "pcfont.h"
#include "gscoord.h"
#include "gsline.h"
#include "gspaint.h"
#include "gspath.h"
#include "gspath2.h"
#include "gxchar.h"
#include "gxfont.h"		/* for setting next_char proc */
#include "gxstate.h"

/* Define the text parsing methods. */
private const pcl_text_parsing_method_t
  pcl_tpm_0 = pcl_tpm_0_data,
  pcl_tpm_21 = pcl_tpm_21_data,
  pcl_tpm_31 = pcl_tpm_31_data,
  pcl_tpm_38 = pcl_tpm_38_data;

/* pseudo-"dots" (actually 1/300" units) used in underline only */
#define	dots(n)	((float)(7200 / 300 * n))

/* Next-character procedures, with symbol mapping. */
private gs_char
pcl_map_symbol(uint chr, const gs_show_enum *penum)
{	const pcl_state_t *pcls = gs_state_client_data(penum->pgs);
	const pl_symbol_map_t *psm =
	  pcls->font_selection[pcls->font_selected].map;
	uint first_code, last_code;

	if ( psm == 0 )
	  return chr;
	first_code = pl_get_uint16(psm->first_code);
	last_code = pl_get_uint16(psm->last_code);
	/* If chr is double-byte but the symbol map is only single-byte, */
	/* just return chr. */
	if ( chr < first_code || chr > last_code )
	  return (last_code <= 0xff && chr > 0xff ? chr : 0xffff);
	return psm->codes[chr - first_code];
}
/*
 * Parse one character from a string.  Return 2 if the string is exhausted,
 * 0 if a character was parsed, -1 if the string ended in the middle of
 * a double-byte character.
 */
int
pcl_next_char(const gs_const_string *pstr, uint *pindex,
  const pcl_text_parsing_method_t *tpm, gs_char *pchr)
{	uint i = *pindex;
	uint size = pstr->size;
	const byte *p;

	if ( i >= size )
	  return 2;
	p = pstr->data + i;
	if ( pcl_char_is_2_byte(*p, tpm) ) {
	  if ( i + 1 >= size )
	    return -1;
	  *pchr = (*p << 8) + p[1];
	  *pindex = i + 2;
	} else {
	  *pchr = *p;
	  *pindex = i + 1;
	}
	return 0;
}
private int
pcl_next_char_proc(gs_show_enum *penum, gs_char *pchr)
{	const pcl_state_t *pcls = gs_state_client_data(penum->pgs);
	int code = pcl_next_char(&penum->str, &penum->index,
				 pcls->text_parsing_method, pchr);

	if ( code )
	  return code;
	*pchr = pcl_map_symbol(*pchr, penum);
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
  pcl_char_adjust_t adjust, const byte *str, uint size)
{	gs_state *pgs = pcls->pgs;
	int code;
	floatp scaled_hmi = pcl_hmi(pcls) / pscale->x;
	floatp diff;
	floatp limit = (pcl_right_margin(pcls) + 0.5) / pscale->x;

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
	/*
	 * Note that TRM 24-13 states (badly) that wrapping only
	 * occurs when *crossing* the right margin: if the cursor
	 * is initially to the right of the margin, wrapping does
	 * not occur.
	 */
	if ( pcls->end_of_line_wrap && pcls->check_right_margin )
	  { /*
	     * If end-of-line wrap is enabled, we must render one character
	     * at a time.  (It's a good thing this is used primarily for
	     * diagnostics.)
	     */
	    uint i, ci;
	    gs_char chr;
	    gs_const_string gstr;

	    gstr.data = str;
	    gstr.size = size;
	    for ( i = ci = 0;
		  !pcl_next_char(&gstr, &i, pcls->text_parsing_method, &chr);
		  ci = i
		)
	      { gs_point width;
		floatp move;
		gs_point pt;

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
		/*
		 * Compensate for the fact that everything is scaled.
		 * We should really handle this by scaling the font....
		 * We need a little fuzz to handle rounding inaccuracies.
		 */
		if ( pt.x + width.x >= limit )
		  { pcl_do_CR(pcls);
		    pcl_do_LF(pcls);
		    pcls->check_right_margin = true; /* CR/LF reset this */
		    gs_moveto(pgs, pcls->cap.x / pscale->x,
			      pcls->cap.y / pscale->y);
		  }
		gs_show_n_init(penum, pgs, str + ci, 1);
		code = gs_show_next(penum);
		if ( code < 0 )
		  return code;
		if ( move != 0 )
		  gs_rmoveto(pgs, move, 0.0);
	      }
	    return 0;
	  }
	switch ( adjust )
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
	gs_const_string gstr;
	const pcl_text_parsing_method_t *tpm = pcls->text_parsing_method;
	uint index = 0;

	if ( pcls->font == 0 )
	  { code = pcl_recompute_font(pcls);
	    if ( code < 0 )
	      return code;
	  }
	pfp = &pcls->font_selection[pcls->font_selected];
	/**** WE COULD CACHE MORE AND DO LESS SETUP HERE ****/
	pcl_set_graphics_state(pcls, true);
	/* Clip to the margins. */
	{ gs_rect text_clip;

	  /*
	   * TRM 5-13 describes text clipping very badly.  Text is only
	   * clipped if it crosses the right margin in the course of
	   * display: it isn't clipped to the left margin, and it isn't
	   * clipped to the right margin if it starts to the right of
	   * the right margin.
	   */
	  if ( !pcls->within_text )
	    {
	      /* Decide now whether to clip and wrap. */
	      pcls->check_right_margin = pcls->cap.x < pcl_right_margin(pcls);
	      pcls->within_text = true;
	    }
	  gs_initclip(pgs);
	  gs_clippath(pgs);
	  gs_pathbbox(pgs, &text_clip);
	  gs_newpath(pgs);
	  text_clip.p.y = pcl_top_margin(pcls);
	  if ( pcls->check_right_margin )
	    text_clip.q.x = pcl_right_margin(pcls);
	  text_clip.q.y = pcl_top_margin(pcls) + pcl_text_length(pcls);
	  code = gs_rectclip(pgs, &text_clip, 1);
	  if ( code < 0 )
	    return code;
	}
	code = pcl_set_drawing_color(pcls, pcls->pattern_type,
				     &pcls->current_pattern_id);
	if ( code < 0 )
	  return code;
	{ gs_font *pfont = (gs_font *)pcls->font->pfont;

	  pfont->procs.next_char = pcl_next_char_proc;
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
	     * (quarter-)points.  However, if the font is fixed-width,
	     * it must be scaled by pitch, not by height, relative to
	     * the nominal pitch of the outline.
	     */
	    if ( pfp->params.proportional_spacing )
	      scale.x = scale.y = pfp->params.height_4ths * 0.25
		* inch2coord(1.0/72);
	    else
	      scale.x = scale.y = pl_fp_pitch_cp(&pfp->params)
	        * (100.0 / pl_fp_pitch_cp(&pfp->font->params))
	        * inch2coord(1.0/7200);
	    /*
	     * Scalable fonts use an upright coordinate system,
	     * the opposite from the usual PCL system.
	     */
	    scale_sign = -1;
	  }

	gstr.data = str;
	gstr.size = size;

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
	     * ******HACK******: The built-in fonts must not define the
	     * space character (required by Genoa FTS 0010-054).
	     */
	    uint i;
	    gs_char chr;

	    for ( i = 0; !pcl_next_char(&gstr, &i, tpm, &chr); )
	      if ( chr == ' ' )
		{ if ( (pcls->font->storage & ~pcds_downloaded) ||
		       !pl_font_includes_char(pcls->font, pcls->map, &font_ctm, ' ')
		     )
		    adjust = char_adjust_space;
	          break;
		}
	  }

	/* Overstrike-center if needed.  Enter here in font space. */
	if ( pcls->last_was_BS )
	  {
	    coord_point prev = pcls->cap;
	    coord_point this_width, delta_BS;
	    gs_char chr;

	    if ( pcl_next_char(&gstr, &index, tpm, &chr) != 0 )
	      return 0;
	    /* Scale the width only by the font part of transformation so
	     * it ends up in PCL coordinates. */
	    gs_make_scaling(scale.x, scale.y, &font_only_mat);
	    if ( (code = pl_font_char_width(pcls->font, pcls->map,
		&font_only_mat, chr, &gs_width)) != 0 )
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
	/****** FOLLOWING IS WRONG FOR DOUBLE-BYTE CHARS ******/
	if ( index < size )
	  { gs_point pt;
	    if ( size - index > 1 )
	      {
		if ( (code = pcl_show_chars(penum, pcls, &scale, adjust,
					    str + index,
					    size - index - 1)) != 0
		   )
		  goto x;
		index = size - 1;
	      }

	    /* Print remaining characters.  Note that pcls->cap may be out
	     * of sync here. */
	    gs_setmatrix(pgs, &user_ctm);
	    gs_currentpoint(pgs, &pt);
	    initial.x = pt.x;
	    initial.y = pt.y;
	    gs_setmatrix(pgs, &font_ctm);
	    if ( (code = pcl_show_chars(penum, pcls, &scale, adjust,
					str + index, size - index)) != 0
	       )
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
/****** PARSER MUST KNOW IF CHAR IS 2-BYTE ******/
int
pcl_plain_char(pcl_args_t *pargs, pcl_state_t *pcls)
{	uint chr = pargs->command;
	byte str[2];
	uint size;

	if ( chr > 0xff )
	  { /* Double-byte */
	    str[0] = chr >> 8;
	    str[1] = (byte)chr;
	    size = 2;
	  }
	else
	  { /* Single-byte */
	    str[0] = chr;
	    size = 1;
	  }
	return pcl_text(str, size, pcls);
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

/* ------ Commands ------ */

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

/* (From PCL5 Comparison Guide, p. 1-56) */
private int /* ESC & t <method> P */
pcl_text_parsing_method(pcl_args_t *pargs, pcl_state_t *pcls)
{	switch ( int_arg(pargs) )
	  {
	  case 0: case 1:
	    pcls->text_parsing_method = &pcl_tpm_0; break;
	  case 21:
	    pcls->text_parsing_method = &pcl_tpm_21; break;
	  case 31:
	    pcls->text_parsing_method = &pcl_tpm_31; break;
	  case 38:
	    pcls->text_parsing_method = &pcl_tpm_38; break;
	  default:
	    return e_Range;
	  }
	return 0;
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
	pcls->text_path = direction;
	return e_Unimplemented;
}

/* ------ Initialization ------ */
private int
pctext_do_init(gs_memory_t *mem)
{		/* Register commands */
	DEFINE_CONTROL(0, "(plain char)", pcl_plain_char);
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
	DEFINE_CLASS('&')
	  {'t', 'P',
	     PCL_COMMAND("Text Parsing Method", pcl_text_parsing_method,
			 pca_neg_error|pca_big_error)},
	  {'c', 'T',
	     PCL_COMMAND("Text Path Direction", pcl_text_path_direction,
			 pca_neg_ok|pca_big_error)},
	END_CLASS
	DEFINE_CONTROL(1, "(plain char)", pcl_plain_char);	/* default "command" */
	return 0;
}
private void
pctext_do_reset(pcl_state_t *pcls, pcl_reset_type_t type)
{	if ( type & (pcl_reset_initial | pcl_reset_printer) )
	  { pcls->underline_enabled = false;
	    pcls->last_was_BS = false;
	    pcls->within_text = false;
	    pcls->last_width.x = pcls->hmi;
	    pcls->last_width.y = 0;
	    pcls->text_parsing_method = &pcl_tpm_0;
	    pcls->text_path = 0;
	  }
}

const pcl_init_t pctext_init = {
  pctext_do_init, pctext_do_reset
};
