/* Copyright (C) 1997 Aladdin Enterprises.  All rights reserved.
   Unauthorized use, copying, and/or distribution prohibited.
 */

/* pglabel.c */
/* HP-GL/2 label commands */
#include "math_.h"
#include "ctype_.h"
#include "stdio_.h"		/* for gdebug.h */
#include "gdebug.h"
#include "pgmand.h"
#include "pginit.h"
#include "pgfont.h"
#include "pgdraw.h"
#include "pggeom.h"
#include "pgmisc.h"
#include "pcfsel.h"
#include "gschar.h"
#include "gscoord.h"
#include "gsline.h"
#include "gspath.h"
#include "gsutil.h"
#include "gxfont.h"

#define STICK_FONT_TYPEFACE 48

/* ------ Font selection ------- */

/* Select primary (0) or alternate (1) font. */
#define hpgl_select_font(pgls, i)\
  do {\
    if ( (pgls)->g.font_selected != (i) )\
      (pgls)->g.font_selected = (i), (pgls)->g.font = 0;\
  } while (0)

/* Ensure a font is available. */
#define hpgl_ensure_font(pgls)\
  do {\
    if ( (pgls)->g.font == 0 )\
      hpgl_call(hpgl_recompute_font(pgls));\
  } while (0)

/* Recompute the current font if necessary. */
private int
hpgl_recompute_font(hpgl_state_t *pgls)
{	pcl_font_selection_t *pfs =
	  &pgls->g.font_selection[pgls->g.font_selected];

	if ( (pfs->params.typeface_family & 0xfff) == STICK_FONT_TYPEFACE )
	  { /* Select the stick font. */
	    pl_font_t *font = &pgls->g.stick_font[pgls->g.font_selected];

	    /* Create a gs_font if none has been created yet. */
	    if ( font->pfont == 0 )
	      { gs_font_base *pfont =
		  gs_alloc_struct(pgls->memory, gs_font_base, &st_gs_font_base,
				  "stick font");
	        int code;

	        if ( pfont == 0 )
		  return_error(e_Memory);
		code = pl_fill_in_font((gs_font *)pfont, font, pgls->font_dir,
				       pgls->memory);
		if ( code < 0 )
		  return code;
		hpgl_fill_in_stick_font(pfont, gs_next_ids(1));
		font->pfont = (gs_font *)pfont;
		font->scaling_technology = plfst_TrueType;/****** WRONG ******/
		font->font_type = plft_7bit_printable;	/****** WRONG ******/
	      }
	    /*
	     * The stick font is protean: set its proportional spacing,
	     * style, and stroke weight parameters to the requested ones.
	     * We could fill in some of the other characteristics earlier,
	     * but it's simpler to do it here.
	     */
	    font->params = pfs->params;
	    font->params.typeface_family = STICK_FONT_TYPEFACE;
	    pfs->font = font;
	    pfs->map = 0;	/****** WRONG ******/
	  }
	else
	  { int code = pcl_reselect_font(pfs, pgls /****** NOTA BENE ******/);

	    if ( code < 0 )
	      return code;
	  }
	pgls->g.font = pfs->font;
	pgls->g.map = pfs->map;
	return 0;
}

/* ------ Position management ------ */

private int
hpgl_get_char_width(const hpgl_state_t *pgls, uint ch, hpgl_real_t *width)
{
	const pcl_font_selection_t *pfs = 
	  &pgls->g.font_selection[pgls->g.font_selected];

	if ( pfs->params.proportional_spacing ) {
	  gs_point gs_width;
	  gs_matrix mat;

	  gs_make_identity(&mat);
	  if ( pl_font_char_width(pfs->font, pfs->map, &mat, ch, &gs_width) >= 0 ) {
	    *width = gs_width.x * points_2_plu(pfs->params.height_4ths / 4.0);
	    return 0;
	  }
	}
	*width = inches_2_plu(pl_fp_pitch_cp(&pfs->params) / 7200.0);
	return 0;
}
private int
hpgl_get_current_cell_height(const hpgl_state_t *pgls, hpgl_real_t *height)
{
	const pcl_font_selection_t *pfs = 
	  &pgls->g.font_selection[pgls->g.font_selected];

	*height =
	  (pfs->params.proportional_spacing ?
	   points_2_plu(pfs->params.height_4ths /4.0) :
	   1.667 * inches_2_plu(pl_fp_pitch_cp(&pfs->params) / 7200.0)
	     /****** WRONG ******/);
	return 0;
}

/* Reposition the cursor.  This does all the work for CP, and is also */
/* used to handle some control characters within LB. */
private int
hpgl_move_cursor_by_characters(hpgl_state_t *pgls, hpgl_real_t spaces,
  hpgl_real_t lines)
{
	/* HAS ***HACK **** HACK **** HACK */
	/* save the current render mode and temporarily set it to
           vector mode. This allows the HPGL/2 graphics not to apply
           the character ctm for the label position move.  This must
           be handled more "pleasantly" in the future */

	hpgl_rendering_mode_t rm = pgls->g.current_render_mode;

	hpgl_ensure_font(pgls);
	pgls->g.current_render_mode = hpgl_rm_vector;

	{
	  double dx = 0, dy = 0;

	  /* calculate the next label position in relative coordinates. */
	  if ( spaces ) {
	    hpgl_real_t width;
	    hpgl_call(hpgl_get_char_width(pgls, ' ', &width));
	    dx = ((width + pgls->g.character.extra_space.x) * spaces);
	  }
	  if ( lines ) {
	    hpgl_real_t height;
	    hpgl_call(hpgl_get_current_cell_height(pgls, &height));
	    dy = ((height + pgls->g.character.extra_space.y) * lines *
		  pgls->g.character.line_feed_direction);
	  }

	  /* Permute the offsets according to the text path. */
	  switch ( pgls->g.character.text_path )
	    {
	    case 0: break;
	    case 1: { double t = dx; dx = dy; dy = -t; } break;
	    case 2: dx = -dx; dy = -dy; break;
	    case 3: { double t = dx; dx = -dy; dy = t; } break;
	    }

	  /* a relative move to the new position */
	  hpgl_call(hpgl_add_point_to_path(pgls, dx, dy,
					   hpgl_plot_move_relative));

	  if ( lines != 0 )
	    { /* update the position of the carriage return point */
	      if ( pgls->g.character.text_path & 1 )
		pgls->g.carriage_return_pos.x += dx;
	      else
		pgls->g.carriage_return_pos.y += dy;
	    }
	}

	pgls->g.current_render_mode = rm;
	return 0;
}

/* Execute a CR for CP or LB. */
private int
hpgl_do_CR(hpgl_state_t *pgls)
{	return hpgl_add_point_to_path(pgls, pgls->g.carriage_return_pos.x,
				      pgls->g.carriage_return_pos.y,
				      hpgl_plot_move_absolute);
}

/* CP [spaces,lines]; */
/* CP [;] */
 int
hpgl_CP(hpgl_args_t *pargs, hpgl_state_t *pgls)
{	
	hpgl_real_t spaces, lines;

	if ( hpgl_arg_c_real(pargs, &spaces) )
	  { 
	    if ( !hpgl_arg_c_real(pargs, &lines) )
	      return e_Range;
	  }
	else
	  {
	    /* if there are no arguments a carriage return and line feed
	       is executed */
	    hpgl_call(hpgl_do_CR(pgls));
	    spaces = 0, lines = -1;
	  }
	return hpgl_move_cursor_by_characters(pgls, spaces, lines);
}

/* ------ Label buffer management ------ */

/* initialize the character buffer, setting state pointers for the
   beginning of the character buffer and the current character within
   the buffer to position 0. */
 private int
hpgl_init_label_buffer(hpgl_state_t *pgls)
{
	pgls->g.label.char_count = 0;
	pgls->g.label.buffer_size = hpgl_char_count;
	return ((pgls->g.label.buffer =
		   gs_alloc_bytes(pgls->memory, hpgl_char_count,
				  "hpgl_init_label_buffer")) == 0 ?
		e_Memory :
		0);
}

/* double the size of the current buffer */
 private int
hpgl_resize_label_buffer(hpgl_state_t *pgls)
{
	uint new_size = pgls->g.label.buffer_size << 1;
	byte *new_mem;

	if ( (new_mem = gs_resize_object(pgls->memory, 
					 pgls->g.label.buffer, new_size,
					 "hpgl_resize_label_buffer")) == 0 )
	  return e_Memory;
	pgls->g.label.buffer = new_mem;
	pgls->g.label.buffer_size = new_size;
	return 0;
}

/* release the character buffer */
 private int
hpgl_destroy_label_buffer(hpgl_state_t *pgls)
{
	gs_free_object(pgls->memory, pgls->g.label.buffer,
		       "hpgl_destroy_label_buffer");
	pgls->g.label.char_count = 0;
	pgls->g.label.buffer_size = 0;
	pgls->g.label.buffer = 0;
	return 0;
}

/* add a single character to the line buffer */
 private int
hpgl_buffer_char(hpgl_state_t *pgls, byte ch)
{
	/* check if there is room for the new character and resize if
           necessary */
	if ( pgls->g.label.buffer_size == pgls->g.label.char_count )
	  hpgl_call(hpgl_resize_label_buffer(pgls));
	/* store the character */
	pgls->g.label.buffer[pgls->g.label.char_count++] = ch;
	return 0;
}

/* ------ LB command ------ */

/* build the path and render it */
 private int
hpgl_print_char(hpgl_state_t *pgls, uint ch)
{
	hpgl_rendering_mode_t rm = pgls->g.current_render_mode;
	const pcl_font_selection_t *pfs = 
	  &pgls->g.font_selection[pgls->g.font_selected];
	gs_point pos;

	/* Set up the graphics state. */
	pgls->g.current_render_mode = hpgl_rm_character;

	pos = pgls->g.pos;
	/* Reset the 'have path' flag so that the CTM gets reset.
	   This is a hack; doing things properly requires a more subtle
	   approach to handling pen up/down and path construction. */
	hpgl_call(hpgl_clear_current_path(pgls));
	/*
	 * All character data is relative, but we have to start at
	 * the right place.
	 */
	hpgl_call(hpgl_add_point_to_path(pgls, pos.x, pos.y,
					 hpgl_plot_move_absolute));
	/*
	 * HACK: we know that the drawing machinery only sets the CTM
	 * once, at the beginning of the path.  We now impose the scale
	 * (and other transformations) on the CTM so that we can add
	 * the symbol outline based on a 1x1-unit cell.
	 */
	{ gs_state *pgs = pgls->pgs;
	  gs_font *pfont = pgls->g.font->pfont;
	  gs_matrix save_ctm;
	  float save_width = pgls->g.pen.width[pgls->g.pen.selected];
	  bool save_relative = pgls->g.pen.width_relative;
	  gs_point scale;
	  int weight = pfs->params.stroke_weight;
	  gs_show_enum *penum =
	    gs_show_enum_alloc(pgls->memory, pgs, "hpgl_print_char");
	  byte str[1];
	  int code;
	  gs_point start_pt, end_pt;
	  hpgl_real_t width;

	  if ( penum == 0 )
	    return_error(e_Memory);
	  gs_currentmatrix(pgs, &save_ctm);
	  /* Handle size. */
	  if ( !pgls->g.character.size_set )
	    scale.x = scale.y = points_2_plu(pfs->params.height_4ths / 4.0);
	  else
	    { /*
	       * HACK: hpgl_set_ctm took P1/P2 into account unless
	       * an absolute character size is in effect.
	       ****** THIS IS WRONG FOR DI/DR ******
	       */
	      scale = pgls->g.character.size;
	      if ( pgls->g.character.size_relative )
		scale.x *= pgls->g.P2.x - pgls->g.P1.x,
		  scale.y *= pgls->g.P2.y - pgls->g.P1.y;
	      if ( pgls->g.bitmap_fonts_allowed ) /* no mirroring */
		scale.x = fabs(scale.x), scale.y = fabs(scale.y);
	    }
	  gs_scale(pgs, scale.x, scale.y);
	  /* Handle rotation. */
	  { double run = pgls->g.character.direction.x,
	      rise = pgls->g.character.direction.y;

	    if ( pgls->g.character.direction_relative )
	      run *= pgls->g.P2.x - pgls->g.P1.x,
		rise *= pgls->g.P2.y - pgls->g.P1.y;
	    if ( run < 0 || rise != 0 )
	      { double denom = hypot(run, rise);
	        gs_matrix rmat;

		/*
		 ****** IF bitmap_fonts_allowed, ROTATE TO NEAREST
		 ****** MULTIPLE OF 90, BUT ADVANCE CORRECTLY (!)
		 */
		gs_make_identity(&rmat);
		rmat.xx = run / denom;
		rmat.xy = rise / denom;
		rmat.yx = -rmat.xy;
		rmat.yy = rmat.xx;
		gs_concat(pgs, &rmat);
	      }
	  }
	  /* Handle slant. */
	  if ( pgls->g.character.slant && !pgls->g.bitmap_fonts_allowed )
	    { gs_matrix smat;

	      gs_make_identity(&smat);
	      smat.yx = pgls->g.character.slant;
	      gs_concat(pgs, &smat);
	    }
	  gs_setfont(pgs, pfont);
	  /*
	   * Adjust the initial position of the character according to
	   * the text path.
	   */
	  gs_currentpoint(pgs, &start_pt);
	  if ( pgls->g.character.text_path == 2 )
	    { hpgl_get_char_width(pgls, ch, &width);
	      start_pt.x -= width / scale.y;
	      gs_moveto(pgs, start_pt.x, start_pt.y);
	    }
	  /*
	   * If we're using a stroked font, patch the pen width to reflect
	   * the stroke weight.  Note that when the font's build_char
	   * procedure calls stroke, the CTM is still scaled.
	   ****** WHAT IF scale.x != scale.y? ******
	   */
	  if ( pfont->PaintType != 0 )
	    { if ( weight == 9999 )
	        pgls->g.pen.width[pgls->g.pen.selected] /= scale.y;
	      else
		{ pgls->g.pen.width[pgls->g.pen.selected] =
		    0.05 + weight * 0.005; /* adhoc */
		  pgls->g.pen.width_relative = true;
		}
	      gs_setlinewidth(pgs, pgls->g.pen.width[pgls->g.pen.selected]);
	    }
	  str[0] = ch;
	  /*
	   * Currently this code is very inefficient, because it always
	   * does a charpath followed by a GL/2 fill and therefore can't
	   * take advantage of the character cache.  We should detect the
	   * common case (no vector fills, edging, or other cases that
	   * can't be implemented directly by the library) and use show
	   * instead of charpath + fill.
	   */
	  if ( (code = gs_charpath_n_init(penum, pgs, str, 1, true)) < 0 ||
	       (code = gs_show_next(penum)) != 0
	     )
	    { gs_show_enum_release(penum, pgls->memory);
	      return (code < 0 ? code : gs_error_Fatal);
	    }
	  gs_free_object(pgls->memory, penum, "hpgl_print_char");
	  gs_currentpoint(pgs, &end_pt);
	  /*
	   * Adjust the final position according to the text path.
	   */
	  switch ( pgls->g.character.text_path )
	    {
	    case 0:
	      break;
	    case 1:
	      /* Nominal character height is 1 in current coordinates. */
	      hpgl_call(gs_moveto(pgs, start_pt.x, end_pt.y - 1));
	      break;
	    case 2:
	      hpgl_call(gs_moveto(pgs, start_pt.x, start_pt.y));
	      break;
	    case 3:
	      /* see above */
	      hpgl_call(gs_moveto(pgs, start_pt.x, end_pt.y + 1));
	      break;
	    }
	  gs_setmatrix(pgs, &save_ctm);
	  gs_currentpoint(pgs, &end_pt);
	  hpgl_call(hpgl_draw_current_path(pgls, hpgl_rm_character));
	  pgls->g.pen.width[pgls->g.pen.selected] = save_width;
	  pgls->g.pen.width_relative = save_relative;
	  hpgl_call(hpgl_add_point_to_path(pgls, end_pt.x, end_pt.y,
					   hpgl_plot_move_absolute));
	}
	pgls->g.current_render_mode = rm;

	return 0;
}

 
/* return relative coordinates to compensate for origin placement -- LO */
 private int
hpgl_get_character_origin_offset(const hpgl_state_t *pgls,
  hpgl_real_t label_length, gs_point *offset)
{
	hpgl_real_t height;

	/* HAS need to account for vertical labels correctly.  The
           label length is always parallel to the x-axis wrt LO.  */
	
	offset->x = 0.0;
	offset->y = 0.0;
	
	/* HAS just height is used as we do not yet support vertical
           labels correctly */
	hpgl_call(hpgl_get_current_cell_height(pgls, &height));

	/* HAS no support for label 21 yet */
	if ( pgls->g.label.origin == 21 )
	  {
	    dprintf("no label 21 support yet, LO offsets are 0\n");
	    return 0;
	  }
	    
	/* HAS does not yet handle 21. */
	switch(pgls->g.label.origin % 10)
	  {
	  case 1:
	    break;
	  case 2:
	    offset->y = .5 * height;
	    break;
	  case 3:
	    offset->y = height;
	    break;
	  case 4:
	    offset->x = .5 * label_length;
	    break;
	  case 5:
	    offset->x = .5 * label_length;
	    offset->y = .5 * height;
	    break;
	  case 6:
	    offset->x = .5 * label_length;
	    offset->y = height;
	    break;
	  case 7:
	    offset->x = label_length;
	    break;
	  case 8:
	    offset->x = label_length;
	    offset->y = .5 * height;
	    break;
	  case 9:
	    offset->x = label_length;
	    offset->y = height;
	    break;
	  default:
	    dprintf("unknown label parameter");

	  }

	/* stickfonts are offset by 16 grid units or .33 times the
           point size.  HAS need to support other font types. */
	
	if ( pgls->g.label.origin > 10 )
	  {
	    hpgl_real_t offset_magnitude = 0.33 * height;

	    switch (pgls->g.label.origin)
	      {
	      case 11:
		offset->x -= offset_magnitude;
		offset->y -= offset_magnitude;
		break;
	      case 12:
		offset->x -= offset_magnitude;
		break;
	      case 13:
		offset->x -= offset_magnitude;
		offset->y += offset_magnitude;
		break;
	      case 14:
		offset->y -= offset_magnitude;
		break;
	      case 15:
		/* HAS I don't think 5 & 15 are different but I need
                   to check */
		break;
	      case 16:
		offset->y += offset_magnitude;
		break;
	      case 17:
		offset->x += offset_magnitude;
		offset->y -= offset_magnitude;
		break;
	      case 18:
		offset->x += offset_magnitude;
		break;
	      case 19:
		offset->x += offset_magnitude;
		offset->y += offset_magnitude;
		break;
	      case 21:
		dprintf("label 21 not currently supported\n");
	      default:
		dprintf("unknown label parameter");
		  
	      }
	  }
	return 0;
}
			  
/* prints a line of characters.  HAS comment this. */
 private int
hpgl_process_buffer(hpgl_state_t *pgls)
{
	gs_point offset;
	hpgl_real_t label_length = 0.0;

	/*
	 * NOTE: the two loops below must be consistent with each other!
	 */

	/* Compute the label length if the label origin needs it. */
	if ( pgls->g.label.origin % 10 >= 4 )
	{
	  hpgl_real_t width = 0.0;
	  int save_index = pgls->g.font_selected;
	  int i = 0;

	  while ( i < pgls->g.label.char_count )
	    {
	      /* HAS missing logic here. */
	      byte ch = pgls->g.label.buffer[i++];

	      if ( ch < 0x20 && !pgls->g.transparent_data )
		switch (ch) 
		{
		case BS :
		  if ( width == 0.0 ) /* BS as first char of string */
		    { hpgl_ensure_font(pgls);
		      hpgl_call(hpgl_get_char_width(pgls, ' ', &width));
		    }
		  label_length -= width;
		  if ( label_length < 0.0 ) 
		    label_length = 0.0;
		  continue;
		case LF :
		  continue;
		case CR :
		  continue;
		case FF :
		  continue;
		case HT :
		  hpgl_ensure_font(pgls);
		  hpgl_call(hpgl_get_char_width(pgls, ' ', &width));
		  label_length += width * 5;
		  continue;
		case SI :
		  hpgl_select_font(pgls, 0);
		  continue;
		case SO :
		  hpgl_select_font(pgls, 1);
		  continue;
		default :
		  break;
		}
	      hpgl_ensure_font(pgls);
	      hpgl_call(hpgl_get_char_width(pgls, ch, &width));
	      label_length += width;
	    }
	  hpgl_select_font(pgls, save_index);
	}
	hpgl_call(hpgl_get_character_origin_offset(pgls, label_length, &offset));
	hpgl_call(hpgl_add_point_to_path(pgls, -offset.x, -offset.y,
					 hpgl_plot_move_relative));

	{
	  int i;
	  for ( i = 0; i < pgls->g.label.char_count; ++i )
	    { byte ch = pgls->g.label.buffer[i];

	      if ( ch < 0x20 && !pgls->g.transparent_data )
		{ hpgl_real_t spaces = 1, lines = 0;

		  switch (ch)
		    {
		    case BS :
		      spaces = -1;
		      break;
		    case LF :
		      spaces = 0, lines = -1;
		      break;
		    case CR :
		      hpgl_call(hpgl_do_CR(pgls));
		      continue;
		    case FF :
		      /* does nothing */
		      spaces = 0, lines = 0;
		      break;
		    case HT :
		      /* appears to expand to 5 spaces */
		      spaces = 5, lines = 0;
		      break;
		    case SI :
		      hpgl_select_font(pgls, 0);
		      continue;
		    case SO :
		      hpgl_select_font(pgls, 2);
		      continue;
		    default :
		      goto print;
		    }
		  hpgl_ensure_font(pgls);
		  hpgl_move_cursor_by_characters(pgls, spaces, lines);
		  continue;
		}
print:	      hpgl_ensure_font(pgls);
	      hpgl_call(hpgl_print_char(pgls, ch));
	    }
	}

	pgls->g.label.char_count = 0;
	return 0;
}	    

/* LB ..text..terminator */
 int
hpgl_LB(hpgl_args_t *pargs, hpgl_state_t *pgls)
{	const byte *p = pargs->source.ptr;
	const byte *rlimit = pargs->source.limit;
	bool print_terminator = pgls->g.label.print_terminator;

	if ( pargs->phase == 0 )
	  {
	    hpgl_call(hpgl_draw_current_path(pgls, hpgl_rm_vector));
	    /* initialize the character buffer first time only */
	    hpgl_call(hpgl_init_label_buffer(pgls));
	    pgls->g.label.initial_pos = pgls->g.pos;
	    pargs->phase = 1;
	  }

	while ( p < rlimit )
	  { byte ch = *++p;
	    if_debug1('I',
		      (ch == '\\' ? " \\%c" : ch >= 33 && ch <= 126 ? " %c" :
		       " \\%03o"),
		      ch);
	    if ( ch == pgls->g.label.terminator )
	      {
		if ( !print_terminator )
		  {
		    hpgl_call(hpgl_process_buffer(pgls));
		    hpgl_call(hpgl_destroy_label_buffer(pgls));
		    pargs->source.ptr = p;
		    /*
		     * Depending on the DV/LO combination, conditionally
		     * restore the initial position, per TRM 23-78.
		     */
		    { static const byte can_concat[22] = {
		        0, 9, 1, 3, 8, 0, 2, 12, 4, 6,
		        0, 9, 1, 3, 8, 0, 2, 12, 4, 6,
			0, 9
		      };
		      if ( !(can_concat[pgls->g.label.origin] &
			     (1 << pgls->g.character.text_path))
		         )
			/* HAS not sure if this is correct.  But
                           restoring Y causes problems printing
                           captions in the FTS. */
			hpgl_call(hpgl_add_point_to_path(pgls,
					pgls->g.label.initial_pos.x,
					pgls->g.pos.y,
					hpgl_plot_move_absolute));
		    }
		    return 0;
		  }
		/*
		 * Process the character in the ordinary way, then come here
		 * again.  We do things this way to simplify the case where
		 * the terminator is a control character.
		 */
		--p;
		print_terminator = false;
	      }

	    /* process the buffer for a carriage return so that we can
               treat the label origin correctly, and initialize a new
               buffer */

	    hpgl_call(hpgl_buffer_char(pgls, ch));
	    if ( ch == CR && !pgls->g.transparent_data )
	      {
		hpgl_call(hpgl_process_buffer(pgls));
		hpgl_call(hpgl_destroy_label_buffer(pgls));
		hpgl_call(hpgl_init_label_buffer(pgls));
	      }
	  }
	pargs->source.ptr = p;
	return e_NeedData;
}

/* Initialization */
private int
pglabel_do_init(gs_memory_t *mem)
{		/* Register commands */
	DEFINE_HPGL_COMMANDS
	  HPGL_COMMAND('C', 'P', hpgl_CP, hpgl_cdf_lost_mode_cleared),
	  /* LB also has special argument parsing. */
	  HPGL_COMMAND('L', 'B', hpgl_LB, hpgl_cdf_polygon|hpgl_cdf_lost_mode_cleared),
	END_HPGL_COMMANDS
	return 0;
}
const pcl_init_t pglabel_init = {
  pglabel_do_init, 0
};
