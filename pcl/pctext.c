/* Copyright (C) 1996, 1997 Aladdin Enterprises.  All rights reserved.
 * Unauthorized use, copying, and/or distribution prohibited.
 */

/* pctext.c */
/* PCL5 text printing commands */
#include "std.h"
#include "stdio_.h"
#include "plvalue.h"
#include "pcommand.h"
#include "pcstate.h"
#include "pcdraw.h"
#include "pcfont.h"
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
	const pcl_font_selection_t *pfs =
	  &pcls->font_selection[pcls->font_selected];
	const pl_symbol_map_t *psm = pfs->map;
	uint first_code, last_code;

	/* if there is not symbol map we assume the the character
           implicitly indexes the font.  */
	if ( psm == 0 ) 
	  {
	    if ( (pfs->font->scaling_technology == plfst_TrueType) &&
		 (pfs->font->storage == pcds_internal) )
	      return chr + 0xf000;
	    return chr;
	  }
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
	    {
	      *pindex = size;
	      *pchr = *p;
	      return 0;
	    }
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
	/* get symbol table value */
	*pchr = pcl_map_symbol(*pchr, penum);
	/* don't need to look up undefined values */
	if ( *pchr == 0xffff )
	  return 0;
	{
	  const pl_font_t *plfont =
	    (const pl_font_t *)(gs_currentfont(penum->pgs)->client_data);
	  gs_matrix ignore_mat;
	  if ( !pl_font_includes_char(plfont, NULL, &ignore_mat, *pchr)
	       )
	    /* HAS. we should differentiate between unmapped
	       characters and characters not found in the font to
	       avoid extra work in pcl_show_chars() */
	    *pchr = 0xffff;
	}
	return 0;
}


private int
pcl_char_width(gs_show_enum *penum, gs_state *pgs, byte chr, gs_point *ppt)
{	int code = gs_stringwidth_n_init(penum, pgs, &chr, 1);
	if ( code < 0 || (code = gs_show_next(penum)) < 0 )
	  return code;
	gs_show_width(penum, ppt);
	return 0;
}

private void
pcl_set_gs_font(pcl_state_t *pcls)
{
	gs_font *pfont = (gs_font *)pcls->font->pfont;
	pfont->procs.next_char = pcl_next_char_proc;
	gs_setfont(pcls->pgs, pfont);
}

/* restores a font after symbol set substitution */
private int
pcl_restore_font(pcl_state_t *pcls, 
		 pcl_font_selection_t *original_font)
{
	pcl_font_selection_t *pfp = &pcls->font_selection[pcls->font_selected];
	int code;
	pcl_decache_font(pcls, pcls->font_selected);
	pfp = original_font;
	code = pcl_recompute_font(pcls);
	if ( code < 0 )
	  return code;
	pcl_set_gs_font(pcls);
	return 0;
}


private int
pcl_show_chars(gs_show_enum *penum, pcl_state_t *pcls, const gs_point *pscale,
  const byte *str, uint size, gs_point *last_width)
{	gs_state *pgs = pcls->pgs;
	floatp limit = (pcl_right_margin(pcls) + 0.5) / pscale->x;
	gs_rop3_t rop = gs_currentrasterop(pgs);
	floatp gray = gs_currentgray(pgs);
	pcl_font_selection_t *pfp = &pcls->font_selection[pcls->font_selected];
	gs_rop3_t effective_rop =
	  (gray == 1.0 ? rop3_know_T_1(rop) :
	   gray == 0.0 ? rop3_know_T_0(rop) : rop);
	bool source_transparent = gs_currentsourcetransparent(pgs);
	bool opaque = !source_transparent &&
	  rop3_know_S_0(effective_rop) != rop3_D;
	bool wrap = pcls->end_of_line_wrap;
	uint i, ci;
	gs_char chr, mapped_chr;
	gs_const_string gstr;
	gstr.data = str;
	gstr.size = size;
	for ( i = ci = 0;
	      !pcl_next_char(&gstr, &i, pcls->text_parsing_method, &chr);
	      ci = i
	      )
	  { gs_point width;
	    gs_point move, start;
	    bool substitute_font = false;
	    pcl_font_selection_t saved_font;
	    int code;
	    bool have_char = true;
	    gs_currentpoint(pgs, &start);
	    move = start;
	    gs_show_n_init(penum, pgs, str + ci, i - ci);
	    pcl_next_char_proc(penum, &mapped_chr);
	    /* HAS not good because mapped 0xffff can mean undefined
               entry in symbol table or "glyph not found in
               font". Check symbol table first?  (HAS) We only
               substitute for unbound fonts */
	    if ( mapped_chr == 0xffff )
	      {
		uint mapped_chr2 = (uint)pcl_map_symbol(chr, penum);
		saved_font = *pfp;
		if ( !pfp->map )
		  have_char = false;
		else
		  {
		    pcl_decache_font(pcls, pcls->font_selected);
		    /* select courier */
		    code = pcl_recompute_substitute_font(pcls, mapped_chr2);
		    /* character not in the font repoitore? */
		    if ( code < 0 )
		      {
			*pfp = saved_font;
			code = pcl_recompute_font(pcls);
			if ( code < 0 )
			  return code;
			have_char = false;
			substitute_font = false;
		      }
		    else
		      {
			/* found a font with the glyph */
			substitute_font = true;
			have_char = true;
		      }
		    pcl_set_gs_font(pcls);
		    code = gs_show_n_init(penum, pgs, str + ci, i - ci);
		    if ( code < 0 )
		      return code;
		    code = pcl_next_char_proc(penum, &mapped_chr);
		    if ( code < 0 )
		      return code;
		  }
	      }

	    /* use the character width for proportionally spaced
               printable characters otherwise use hmi, with one
               exception: if hmi is set explicitly the space character
               uses scaled hmi FTS panel 41 */
	    {
	      floatp scaled_hmi = pcl_hmi(pcls) / pscale->x;
	      if ( pfp->params.proportional_spacing  && have_char )
		{
		  if ( str[ci] == ' ' && (pcls->hmi_set == hmi_set_explicitly) )
		    width.x = scaled_hmi;
		  else
		    {
		      code = pcl_char_width(penum, pgs, chr, &width);
		      if ( code < 0 )
			return code;
		    }
		}
	      else
		width.x = scaled_hmi;
	    }
	    move.x += width.x;
	    if ( (move.x > limit) && pcls->check_right_margin )
	      {
		if ( wrap )
		  {
		    gs_point new_line_coord;
		    pcl_do_CR(pcls);
		    pcl_do_LF(pcls);
		    pcls->check_right_margin = true; /* CR/LF reset this */
		    gs_moveto(pgs, pcls->cap.x / pscale->x,
			      pcls->cap.y / pscale->y);
		    gs_currentpoint(pgs, &new_line_coord);
		    /* move - start is the advance width */
		    move.x = (move.x - start.x) + new_line_coord.x;
		    move.y = new_line_coord.y;
		  }
		else
		  {
		    if ( substitute_font )
		      {
			code = pcl_restore_font(pcls, &saved_font);
			if ( code < 0 )
			  return code;
		      }
		    /* crossed the right margin, nothing else to do */
		    return 0;
		  }
	      }

	    if ( opaque )
	      { /*
		 * We have to handle bitmap and outline characters
		 * differently.  For outlines, we construct the path,
		 * then use eofill to process the inside and outside
		 * separately.  For bitmaps, we do the rendering
		 * ourselves, replacing the BuildChar procedure.
		 * What a nuisance!
		 */
		gs_font *pfont = gs_currentfont(penum->pgs);
		const pl_font_t *plfont =
		  (const pl_font_t *)(pfont->client_data);
		gs_rop3_t background_rop = rop3_know_S_1(effective_rop);

		if ( plfont->scaling_technology == plfst_bitmap )
		  { /*
		     * It's too much work to handle all cases right.
		     * Do the right thing in the two common ones:
		     * where the background is unchanged, or where
		     * the foreground doesn't depend on the previous
		     * state of the destination.
		     */
		    gs_point pt;

		    gs_gsave(pgs);
		    gs_setfilladjust(pgs, 0.0, 0.0);
		    /*
		     * Here's where we take the shortcut: if the
		     * current RasterOp is such that the background is
		     * actually affected, affect the entire bounding
		     * box of the character, not just the part outside
		     * the glyph.
		     */
		    if ( background_rop != rop3_D )
		      { /*
			 * Fill the bounding box.  We get this by
			 * actually digging into the font structure.
			 * Someday this should probably be a pl library
			 * procedure.
			 */
			gs_char chr;
			gs_glyph glyph;
			const byte *cdata;

			gs_show_n_init(penum, pgs, str + ci, i - ci);
			pcl_next_char_proc(penum, &chr);
			glyph = (*pfont->procs.encode_char)
			  (penum, pfont, &chr);
			cdata = pl_font_lookup_glyph(plfont, glyph)->data;
			if ( cdata != 0 )
			  { const byte *params = cdata + 6;
			  int lsb = pl_get_int16(params);
			  int ascent = pl_get_int16(params + 2);
			  int width = pl_get_uint16(params + 4);
			  int height = pl_get_uint16(params + 6);
			  gs_rect bbox;

			  gs_currentpoint(pgs, &pt);
			  bbox.p.x = pt.x + lsb;
			  bbox.p.y = pt.y - ascent;
			  bbox.q.x = bbox.p.x + width;
			  bbox.q.y = bbox.p.y + height;
			  gs_gsave(pgs);
			  gs_setrasterop(pgs, background_rop);
			  gs_rectfill(pgs, &bbox, 1);
			  gs_grestore(pgs);
			  }
		      }
		    /* Now do the foreground. */
		    gs_show_n_init(penum, pgs, str + ci, i - ci);
		    code = gs_show_next(penum);
		    if ( code < 0 )
		      return code;
		    gs_currentpoint(pgs, &pt);
		    gs_grestore(pgs);
		    gs_moveto(pgs, pt.x, pt.y);
		  }
		else
		  { gs_point pt;
		    gs_rect bbox;

		    gs_gsave(pgs);
		    gs_currentpoint(pgs, &pt);
		    gs_newpath(pgs);
		    gs_moveto(pgs, pt.x, pt.y);
		    /* Don't allow pixels to be processed twice. */
		    gs_setfilladjust(pgs, 0.0, 0.0);
		    gs_charpath_n_init(penum, pgs, str + ci, i - ci, true);
		    code = gs_show_next(penum);
		    if ( code < 0 )
		      { gs_grestore(pgs);
		        return code;
		      }
		    gs_currentpoint(pgs, &pt); /* save end point */
		    /* Handle the outside of the path. */
		    gs_gsave(pgs);
		    gs_pathbbox(pgs, &bbox);
		    gs_rectappend(pgs, &bbox, 1);
		    gs_setrasterop(pgs, background_rop);
		    gs_eofill(pgs);
		    gs_grestore(pgs);
		    /* Handle the inside of the path. */
		    gs_fill(pgs);
		    gs_grestore(pgs);
		    gs_moveto(pgs, pt.x, pt.y);
		  }
	      }
	    else
	      { 
		if ( have_char )
		  {
		    gs_matrix fmat;
		    gs_currentmatrix(pgs, &fmat);
		    if ( (pcls->text_path == -1) && ((i - ci) == 2) )
		      {
			gs_rotate(pgs, 90);
			move.x += width.x; /* hack */
		      }
		    code = gs_show_n_init(penum, pgs, str + ci, i - ci);
		    if ( code < 0 )
		      return code;
		    code = gs_show_next(penum);
		    if ( code < 0 )
		      return code;
		    gs_setmatrix(pgs, &fmat);
		  }
		gs_moveto(pgs, move.x, move.y);
	      }
	    if ( substitute_font )
	      {
		code = pcl_restore_font(pcls, &saved_font);
		if ( code < 0 ) 
		  return code;
		substitute_font = false;
	      }

	    {
	      /* set the last character width for backspace.  Could be
                 moved outside the loop HAS */
	      gs_point last_pt;
	      gs_currentpoint(pgs, &last_pt);
	      last_width->x = last_pt.x - start.x;
	      last_width->y = last_pt.y - start.y;
	    }
	  }
	return 0;
}

int
pcl_text(const byte *str, uint size, pcl_state_t *pcls)
{	gs_memory_t *mem = pcls->memory;
	gs_state *pgs = pcls->pgs;
	gs_show_enum *penum;
       	gs_matrix user_ctm, font_ctm;
	gs_point last_width;
	gs_point scale;
	int scale_sign;
	int code;
	if ( pcls->font == 0 )
	  { code = pcl_recompute_font(pcls);
	    if ( code < 0 )
	      return code;
	  }
	/**** WE COULD CACHE MORE AND DO LESS SETUP HERE ****/
	pcl_set_graphics_state(pcls, true);
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
	code = pcl_set_drawing_color(pcls, pcls->pattern_type,
				     &pcls->current_pattern_id);
	if ( code < 0 )
	  return code;
	pcl_set_gs_font(pcls);
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
	    pcl_font_selection_t *pfp = &pcls->font_selection[pcls->font_selected];
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
	/* Print remaining characters.  Enter here in font space. */
	code = pcl_show_chars(penum, pcls, &scale, str, size, &last_width);
	if ( code < 0 )
	  goto x;
	gs_setmatrix(pgs, &user_ctm);
	{
	  gs_point pt;
	  gs_currentpoint(pgs, &pt);
	  pcls->cap.x = pt.x;
	  pcls->cap.y = pt.y;
	}
	pcls->last_width.x = last_width.x * scale.x;
	pcls->last_width.y = last_width.y * scale.y;
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
