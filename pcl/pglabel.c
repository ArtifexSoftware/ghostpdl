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
#include "gsline.h"

/* Define a bogus pl_font_t for the stick font. */
/* Eventually this will be a real one.... */
#define STICK_FONT_TYPEFACE 48
private pl_font_t stick_font;

/* ------ Font selection ------- */

/* Recompute the current font if necessary. */
private int
hpgl_recompute_font(hpgl_state_t *pgls)
{	pcl_font_selection_t *pfs =
	  &pgls->g.font_selection[pgls->g.font_selected];

	if ( (pfs->params.typeface_family & 0xfff) == STICK_FONT_TYPEFACE )
	  { /* Select the stick font. */
	    pfs->font = &stick_font;
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
hpgl_get_carriage_return_pos(const hpgl_state_t *pgls, gs_point *pt)
{
	*pt = pgls->g.carriage_return_pos;
        return 0;
}

private int
hpgl_set_carriage_return_pos(hpgl_state_t *pgls, const gs_point *pt)
{
	pgls->g.carriage_return_pos = *pt;
	return 0;
}

private int
hpgl_get_current_cell_width(const hpgl_state_t *pgls, hpgl_real_t *width)
{
	const pcl_font_selection_t *pfs = 
	  &pgls->g.font_selection[pgls->g.font_selected];

	*width =
	  (pfs->params.proportional_spacing ?
	   points_2_plu(pfs->params.height_4ths / 4.0 /****** WRONG ******/) :
	   1.5 * inches_2_plu(pl_fp_pitch_cp(&pfs->params) / 7200.0));
	return 0;
}

private int
hpgl_get_current_cell_height(const hpgl_state_t *pgls, hpgl_real_t *height)
{
	const pcl_font_selection_t *pfs = 
	  &pgls->g.font_selection[pgls->g.font_selected];

	*height =
	  (pfs->params.proportional_spacing ?
	   1.33 * points_2_plu(pfs->params.height_4ths /4.0) :
	   2.0 * inches_2_plu(pl_fp_pitch_cp(&pfs->params) / 7200.0)
	     /****** WRONG ******/);
	return 0;
}

/* Reposition the cursor.  This does all the work for CP, and is also */
/* used to handle control characters within LB. */
private int
hpgl_move_cursor_by_characters(hpgl_state_t *pgls, hpgl_real_t spaces,
  hpgl_real_t lines, bool do_CR)
{
	hpgl_pen_state_t saved_pen_state;	

	/* HAS ***HACK **** HACK **** HACK */
	/* save the current render mode and temporarily set it to
           vector mode. This allows the HPGL/2 graphics not to apply
           the character ctm for the label position move.  This must
           be handled more "pleasantly" in the future */

	hpgl_rendering_mode_t rm = pgls->g.current_render_mode;

	pgls->g.current_render_mode = hpgl_rm_vector;

	/* CP does its work with the pen up -- we restore the current
           state at the end of the routine.  We will take advantage of
           PR so save the current relative state as well */
	hpgl_save_pen_state(pgls,  
			    &saved_pen_state, 
			    hpgl_pen_down | hpgl_pen_relative);

	{
	  hpgl_real_t width, height;
	  hpgl_args_t args;
	  gs_point pt, crpt, relpos; /* current pos and carriage return pos */

	  /* get the character cell height and width */
	  hpgl_call(hpgl_get_current_cell_width(pgls, &width));
	  hpgl_call(hpgl_get_current_cell_height(pgls, &height));
	  
	  /* get the current position and carriage return position */
	  hpgl_call(hpgl_get_current_position(pgls, &pt));
	  hpgl_call(hpgl_get_carriage_return_pos(pgls, &crpt));
	  
	  /* calculate the next label position in relative coordinates.  If
             CR then the X position is simply the x coordinate of the
             current carriage return point.  We use relative coordinates for
             the movement in either case. HAS the extra space calculations
             are incorrect */
	  if (do_CR)
	    relpos.x = -(pt.x - crpt.x);
	  else
	    relpos.x = ((spaces + pgls->g.character.extra_space.x) * width);

	  /* the y coordinate is independant of CR/LF status */
	  relpos.y = -((lines + pgls->g.character.extra_space.y) * height);

	  /* move to the current position */
	  hpgl_args_setup(&args);
	  hpgl_PU(&args, pgls);

	  /* a relative move to the new position */
	  hpgl_args_set_real2(&args, relpos.x, relpos.y);
	  hpgl_PR(&args, pgls);

	  /* update the carriage return point - the Y is the Y of
             the current position and the X is the X of the current
             carriage return point position */
	  hpgl_call(hpgl_get_current_position(pgls, &pt));
	  hpgl_call(hpgl_get_carriage_return_pos(pgls, &crpt));
	  crpt.y = pt.y;

	  /* set the value in the state */
	  hpgl_call(hpgl_set_carriage_return_pos(pgls, &crpt));

	}

	/* put the pen back the way it was, of course we do not want
           to restore the pen's position.  And restore the render mode. */
	hpgl_restore_pen_state(pgls, 
			       &saved_pen_state, 
			       hpgl_pen_down | hpgl_pen_relative);
	pgls->g.current_render_mode = rm;
	return 0;
}

/* CP [spaces,lines]; HAS -- an arcane implementation, but it is late */
 int
hpgl_CP(hpgl_args_t *pargs, hpgl_state_t *pgls)
{	
	hpgl_real_t spaces = 0.0;
	hpgl_real_t lines = 1.0;
	bool crlf = false;

	if ( hpgl_arg_c_real(pargs, &spaces) )
	  { 
	    if ( !hpgl_arg_c_real(pargs, &lines) )
	      return e_Range;
	  }
	else
	  {
	    /* if there are no arguments a carriage return and line feed
	       is executed */
	    crlf = true;
	  }
	return hpgl_move_cursor_by_characters(pgls, spaces, lines, crlf);
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
hpgl_print_char(hpgl_state_t *pgls, hpgl_character_point *character)
{
	hpgl_rendering_mode_t rm = pgls->g.current_render_mode;
	/*
	 * The default stick font size is 32x32 units.
	 * Scale the data according to the current font height.
	 * (Eventually we must take SI/SR into account.)
	 */
	const pcl_font_selection_t *pfs = 
	  &pgls->g.font_selection[pgls->g.font_selected];
	double scale = (pfs->params.height_4ths / 4.0) / 32;

	/* clear the current path, if there is one */
	hpgl_call(hpgl_draw_current_path(pgls, hpgl_rm_vector));

	/* Set up the graphics state. */
	pgls->g.current_render_mode = hpgl_rm_character;
	hpgl_call(hpgl_set_graphics_state(pgls, hpgl_rm_character));

	/* Always use round caps and joins. */
	gs_setlinecap(pgls->pgs, gs_cap_round);
	gs_setlinejoin(pgls->pgs, gs_join_round);

	/* all character data is absolute */
	pgls->g.relative = false;
	while (character->operation != hpgl_char_end)
	  {
	    hpgl_args_t args;
	    /* setup the arguments */
	    hpgl_args_setup(&args);
	    /* all character operations supply numeric arguments except: */
	    if (character->operation != hpgl_char_pen_down_no_args)
	      {
		hpgl_args_add_real(&args, character->vertex.x * scale);
		hpgl_args_add_real(&args, character->vertex.y * scale);
	      }

	    switch (character->operation)
	      {
	      case hpgl_char_pen_up : 
		hpgl_call(hpgl_PU(&args, pgls));
		break;
	      case hpgl_char_pen_down : 
	      case hpgl_char_pen_down_no_args :
		hpgl_call(hpgl_PD(&args, pgls));
		break;
	      case hpgl_char_pen_relative :
		hpgl_call(hpgl_PR(&args, pgls));
		break;
	      case hpgl_char_arc_relative : /* HAS not yet supported */;
	      default :
		dprintf("unsupported character operation\n");
	      }
	    character++;
	  }
	hpgl_call(hpgl_draw_current_path(pgls, hpgl_rm_character));
	pgls->g.current_render_mode = rm;
	return 0;
}

 
/* return relative coordinates to compensate for origin placement -- LO */
 private int
hpgl_get_character_origin_offset(const hpgl_state_t *pgls,
  hpgl_real_t label_length, gs_point *offset)
{
	hpgl_real_t width, height, offset_magnitude;

	/* HAS need to account for vertical labels correctly.  The
           label length is always parallel to the x-axis wrt LO.  */
	
	offset->x = 0.0;
	offset->y = 0.0;
	
	/* HAS just height is used as we do not yet support vertical
           labels correctly */
  	hpgl_call(hpgl_get_current_cell_width(pgls, &width));
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
	    offset_magnitude = 0.33 * width;
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
	hpgl_args_t args;
	hpgl_pen_state_t pen_state;

	/* Compute the label length.  (Future performance improvement: */
	/* only do this if we need it to compute the offset.) */
	{
	  hpgl_real_t label_length = 0.0;
	  hpgl_real_t height, width;
	  int save_index = pgls->g.font_selected;
	  int i = 0;

	  /* get the character cell height and width */
cell:	  hpgl_call(hpgl_get_current_cell_width(pgls, &width));
	  hpgl_call(hpgl_get_current_cell_height(pgls, &height));
	  for ( ; i < pgls->g.label.char_count; ++i )
	    {
	      /* HAS missing logic here. */
	      byte ch = pgls->g.label.buffer[i];
	      if ( pgls->g.transparent_data )
		label_length += width;
	      else
		switch (ch) 
		{
		case BS :
		  label_length -= width;
		  if ( label_length < 0.0 ) 
		    label_length = 0.0;
		  break;
		case LF :
		  break;
		case CR :
		  break;
		case FF :
		  break;
		case HT :
		  break;
		case SI :
		  pgls->g.font_selected = 0;
shift:		  hpgl_call(hpgl_recompute_font(pgls));
		  goto cell;
		case SO :
		  pgls->g.font_selected = 1;
		  goto shift;
		default :
		  label_length += width;
		  break;
		}
	    }
	  if ( save_index != pgls->g.font_selected )
	    {
	      pgls->g.font_selected = save_index;
	      hpgl_call(hpgl_recompute_font(pgls));
	    }
	  hpgl_call(hpgl_get_character_origin_offset(pgls, label_length, &offset));
	}

	hpgl_save_pen_state(pgls, &pen_state, hpgl_pen_relative);
	hpgl_args_set_real2(&args, -offset.x, -offset.y);
	hpgl_PR(&args, pgls);
	hpgl_restore_pen_state(pgls, &pen_state, hpgl_pen_relative);

	{
	  int i;
	  for ( i = 0; i < pgls->g.label.char_count; ++i )
	    { byte ch = pgls->g.label.buffer[i];
	      hpgl_pen_state_t saved_pen_state;

	      /* we need to keep this pen position, as updating the label
		 origin is relative to the current origin vs. the pen's
		 position after the character is rendered */
	      if ( ch < 0x20 && !pgls->g.transparent_data )
		{ hpgl_real_t spaces = 1, lines = 0;
		  bool do_CR = false;

		  switch (ch)
		    {
		    case BS :
		      spaces = -1;
		      break;
		    case LF :
		      spaces = 0, lines = 1;
		      break;
		    case CR :
		      spaces = 0, lines = 0;
		      do_CR = true;
		      break;
		    case FF :
		      /* does nothing */
		      spaces = 0, lines = 0;
		      break;
		    case HT :
		      /* appears to expand to 5 spaces */
		      spaces = 5, lines = 0;
		      break;
		    case SI :
		      pgls->g.font_selected = 0;
		      hpgl_call(hpgl_recompute_font(pgls));
		      continue;
		    case SO :
		      pgls->g.font_selected = 1;
		      hpgl_call(hpgl_recompute_font(pgls));
		      continue;
		    default :
		      goto print;
		    }
		  hpgl_move_cursor_by_characters(pgls, spaces, lines, do_CR);
		  continue;
		}
print:	      hpgl_save_pen_state(pgls, &saved_pen_state, hpgl_pen_all); 
	      if ( !isprint(ch) )
		ch = ' ';	/* ascii is all we have right now */
	      hpgl_call(hpgl_print_char(pgls, hpgl_ascii_char_set[ch - 0x20]));
	      hpgl_restore_pen_state(pgls, &saved_pen_state, hpgl_pen_all); 
	      hpgl_call(hpgl_move_cursor_by_characters(pgls, 1, 0, false));
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
	    gs_point pt;
	    /* HAS need to figure out what gets saved here -- relative,
	       pen_down, position ?? */
	    hpgl_call(hpgl_clear_current_path(pgls));
	    /* set the carriage return point and initialize the character
	       buffer first time only */
	    hpgl_call(hpgl_get_current_position(pgls, &pt));
	    hpgl_call(hpgl_set_carriage_return_pos(pgls, &pt));
	    hpgl_call(hpgl_init_label_buffer(pgls));
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

	    if ( ch == CR && !pgls->g.transparent_data )
	      {
		hpgl_call(hpgl_process_buffer(pgls));
		hpgl_call(hpgl_destroy_label_buffer(pgls));
		hpgl_call(hpgl_init_label_buffer(pgls));
	      }
	    else
	      hpgl_call(hpgl_buffer_char(pgls, ch));
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
	  /*
	   * Fill in some of the the stick font characteristics.
	   * Many more will be required later....
	   */
	stick_font.scaling_technology = plfst_TrueType;	/****** WRONG ******/
	stick_font.font_type = plft_7bit_printable;	/****** WRONG ******/
	stick_font.params.proportional_spacing = false;
	/* The basic cell size for the stick font is 32 plu. */
	pl_fp_set_pitch_cp(&stick_font.params, plu_2_inches(32) * 7200);
	stick_font.params.typeface_family = STICK_FONT_TYPEFACE;
	return 0;
}
const pcl_init_t pglabel_init = {
  pglabel_do_init, 0
};
