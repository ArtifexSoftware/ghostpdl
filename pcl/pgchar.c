/* Copyright (C) 1996, 1997 Aladdin Enterprises.  All rights reserved.
   Unauthorized use, copying, and/or distribution prohibited.
 */

/* pgchar.c */
/* HP-GL/2 character commands */
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

/* ------ Internal procedures ------ */

/* Define font parameters (AD, SD). */
private int
hpgl_font_definition(hpgl_args_t *pargs, hpgl_state_t *pgls, int index)
{	/*
	 * Since these commands take an arbitrary number of arguments,
	 * we reset the argument bookkeeping after each group.
	 * We reset phase to 1 after seeing the first pair,
	 * so we can tell whether there were any arguments at all.
	 */
	pcl_font_selection_t *pfs = &pgls->g.font_selection[index];
#define pfp (&pfs->params)
	int kind;

	for ( ; hpgl_arg_c_int(pargs, &kind); pargs->phase = 1 )
	  switch ( kind )
	    {
	    case 1:		/* symbol set */
	      { int32 sset;
	        if ( !hpgl_arg_int(pargs, &sset) )
		  return e_Range;
		pfp->symbol_set = (uint)sset;
	      }
	      break;
	    case 2:		/* spacing */
	      { int spacing;
	        if ( !hpgl_arg_c_int(pargs, &spacing) || (spacing & ~1) )
		  return e_Range;
		pfp->proportional_spacing = spacing;
	      }
	      break;
	    case 3:		/* pitch */
	      { hpgl_real_t pitch;
	        if ( !hpgl_arg_c_real(pargs, &pitch) || pitch < 0 )
		  return e_Range;
		pfp->pitch_100ths = (uint)(pitch * 100);
	      }
	      break;
	    case 4:		/* height */
	      { hpgl_real_t height;
	        if ( !hpgl_arg_c_real(pargs, &height) || height < 0 )
		  return e_Range;
		pfp->height_4ths = (uint)(height * 4);
	      }
	      break;
	    case 5:		/* posture */
	      { int posture;
	        if ( !hpgl_arg_c_int(pargs, &posture) ||
		     posture < 0 || posture > 2
		   )
		  return e_Range;
		pfp->style = posture;
	      }
	      break;
	    case 6:		/* stroke weight */
	      { int weight;
	        if ( !hpgl_arg_c_int(pargs, &weight) ||
		     weight < -7 || (weight > 7 && weight != 9999)
		   )
		  return e_Range;
		pfp->stroke_weight = weight;
	      }
	      break;
	    case 7:		/* typeface */
	      { int32 face;
	        if ( !hpgl_arg_int(pargs, &face) )
		  return e_Range;
		pfp->typeface_family = (uint)face;
	      }
	      break;
	    default:
	      return e_Range;
	    }
	/* If there were no arguments at all, default all values. */
	if ( !pargs->phase )
	  hpgl_default_font_params(pfs);
	return 0;
}
/* Define label drawing direction (DI, DR). */
private int
hpgl_label_direction(hpgl_args_t *pargs, hpgl_state_t *pgls, bool relative)
{	hpgl_real_t run = 1, rise = 0;

	if ( hpgl_arg_c_real(pargs, &run) )
	  { double hyp;
	    if ( !hpgl_arg_c_real(pargs, &rise) || (run == 0 && rise == 0) )
	      return e_Range;
	    hyp = hypot(run, rise);
	    run /= hyp;
	    rise /= hyp;
	  }
	pgls->g.character.direction.x = run;
	pgls->g.character.direction.y = rise;
	pgls->g.character.direction_relative = relative;
	return 0;
}

/* Select font (FI, FN). */
private int
hpgl_select_font(hpgl_args_t *pargs, hpgl_state_t *pgls, int index)
{	int32 id;

	if ( !hpgl_arg_c_int(pargs, &id) || id < 0 )
	  return e_Range;
	return e_Unimplemented;
}

/* Set character size (SI, SR). */
private int
hpgl_character_size(hpgl_args_t *pargs, hpgl_state_t *pgls, bool relative)
{	hpgl_real_t width, height;

	if ( hpgl_arg_c_real(pargs, &width) )
	  { if ( !hpgl_arg_c_real(pargs, &height) )
	      return e_Range;
	  }
	return e_Unimplemented;
}

/* ------ Commands ------ */

/* AD [kind,value...]; */
 int
hpgl_AD(hpgl_args_t *pargs, hpgl_state_t *pgls)
{	
	return hpgl_font_definition(pargs, pgls, 1);
}

/* CF [mode[,pen]]; */
 int
hpgl_CF(hpgl_args_t *pargs, hpgl_state_t *pgls)
{	int mode = 0;
	int32 pen = 0;
	if ( hpgl_arg_c_int(pargs, &mode) )
	  { if ( mode & ~3 )
	      return e_Range;
	    if ( hpgl_arg_int(pargs, &pen) )
	      { /**** MUST CHANGE FOR COLOR ****/
		if ( pen & ~1 )
		  return e_Range;
	      }
	  }
	return e_Unimplemented;
}

private int
hpgl_get_carriage_return_pos(hpgl_state_t *pgls, gs_point *pt)
{
	*pt = pgls->g.carriage_return_pos;
        return 0;
}

private int
hpgl_set_carriage_return_pos(hpgl_state_t *pgls, gs_point *pt)
{
	pgls->g.carriage_return_pos = *pt;
	return 0;
}

private int
hpgl_get_current_cell_width(hpgl_state_t *pgls, hpgl_real_t *width)
{
	pcl_font_selection_t *pfs = 
	  &pgls->g.font_selection[pgls->g.font_selected];
	*width = inches_2_plu((1.0 / (pfs->params.pitch_100ths / 
				100.0)));
	*width *= 1.5;
	return 0;
}

 private int
hpgl_get_current_cell_height(hpgl_state_t *pgls, hpgl_real_t *height)
{
	pcl_font_selection_t *pfs = 
	  &pgls->g.font_selection[pgls->g.font_selected];
	*height = points_2_plu((pfs->params.height_4ths) /4.0);
	*height *= 1.33;
	return 0;
}

/* CP [spaces,lines]; HAS -- an arcane implementation, but it is late */
 int
hpgl_CP(hpgl_args_t *pargs, hpgl_state_t *pgls)
{	
	hpgl_real_t spaces = 0.0;
	hpgl_real_t lines = 1.0;
	hpgl_pen_state_t saved_pen_state;	
	bool crlf = false;

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

	if ( hpgl_arg_c_real(pargs, &spaces) )
	  { 
	    if ( !hpgl_arg_c_real(pargs, &lines) )
	      return e_Range;
	  }
	else

	  /* if there are no arguments a carriage return and line feed
	  is executed */
	  crlf = true;

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
	  
	  /* calculate the next label position in relative
             coordinates.  If CR/LF the calculate the y coordinate and
             the X position is simply the x coordinate of the current
             carriage return point.  We use relative coordinates for
             the movement in either case. HAS the extra space
             calculations are incorrect */
	  if (crlf)
	    relpos.x = -(pt.x - crpt.x);
	  else
	    relpos.x = ((spaces + pgls->g.character.extra_space.x) * width);

	  /* the y coordinate is independant of CR/LF status */
	  relpos.y = -((lines + pgls->g.character.extra_space.y) * height);

	  /* move to the current position */
	  hpgl_args_setup(&args);
	  hpgl_PU(&args, pgls);

	  /* a relative move to the new position */
	  hpgl_args_set_real(&args, relpos.x);
	  hpgl_args_add_real(&args, relpos.y);
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

/* DI [run,rise]; */
 int
hpgl_DI(hpgl_args_t *pargs, hpgl_state_t *pgls)
{	
	return hpgl_label_direction(pargs, pgls, false);
}

/* DR [run,rise]; */
 int
hpgl_DR(hpgl_args_t *pargs, hpgl_state_t *pgls)
{	
	return hpgl_label_direction(pargs, pgls, true);
}

/* DT terminator[,mode]; */
 int
hpgl_DT(hpgl_args_t *pargs, hpgl_state_t *pgls)
{	const byte *p = pargs->source.ptr;
	const byte *rlimit = pargs->source.limit;
	byte ch = (byte)pargs->phase;
	int mode = 1;

	/* We use phase to remember the terminator character */
	/* in case we had to restart execution. */
	if ( p >= rlimit )
	  return e_NeedData;
	if ( !ch )
	  switch ( (ch = *++p) )
	    {
	    case ';':
	      pargs->source.ptr = p;
	      pgls->g.label.terminator = 3;
	      pgls->g.label.print_terminator = false;
	      return 0;
	    case 0: case 5: case 27:
	      return e_Range;
	    default:
	      if ( p >= rlimit )
		return e_NeedData;
	      if ( *++p ==',' )
		{ pargs->source.ptr = p;
		  pargs->phase = ch;
		}
	    }
	if ( hpgl_arg_c_int(pargs, &mode) && (mode & ~1) )
	  return e_Range;
	pgls->g.label.terminator = ch;
	pgls->g.label.print_terminator = !mode;
	return 0;
}

/* DV [path[,line]]; */
 int
hpgl_DV(hpgl_args_t *pargs, hpgl_state_t *pgls)
{	int path = 0, line = 0;

	hpgl_arg_c_int(pargs, &path);
	hpgl_arg_c_int(pargs, &line);
	if ( (path & ~3) | (line & ~1) )
	  return e_Range;
	pgls->g.character.text_path = path;
	pgls->g.character.reverse_line_feed = line;
	return 0;
}

/* ES [width[,height]]; */
 int
hpgl_ES(hpgl_args_t *pargs, hpgl_state_t *pgls)
{	hpgl_real_t width = 0, height = 0;

	hpgl_arg_c_real(pargs, &width);
	hpgl_arg_c_real(pargs, &height);
	pgls->g.character.extra_space.x = width;
	pgls->g.character.extra_space.y = height;
	return 0;
}

/* FI fontid; */
 int
hpgl_FI(hpgl_args_t *pargs, hpgl_state_t *pgls)
{	
	return hpgl_select_font(pargs, pgls, 0);
}

/* FN fontid; */
 int
hpgl_FN(hpgl_args_t *pargs, hpgl_state_t *pgls)
{	
	return hpgl_select_font(pargs, pgls, 1);
}

/* update the length of the current label */
 private int
hpgl_update_label_length(hpgl_state_t *pgls, byte ch)
{

	hpgl_real_t height, width;
	/* get the character cell height and width */
  	hpgl_call(hpgl_get_current_cell_width(pgls, &width));
	hpgl_call(hpgl_get_current_cell_height(pgls, &height));
	/* HAS missing logic here. */
	switch (ch) 
	  {
	  case BS :
	    pgls->g.label.length -= width;
	    if ( pgls->g.label.length < 0.0 ) 
	      pgls->g.label.length = 0.0;
	    break;
	  case LF :
	    break;
	  case CR :
	    break;
	  default :
	    pgls->g.label.length += width;
	    break;
	  }
	return 0;
}

/* updates the current cursor position, which is the same as the
   current pen postion */

 private int
hpgl_update_label_pos(hpgl_state_t *pgls, byte ch)
{
	int spaces = 1, lines = 0;
  	hpgl_args_t args;
	/* HAS missing logic here. */
	switch (ch)
	  {
	  case BS :
	    spaces = -1;
	    break;
	  case LF :
	    spaces = 0;
	    lines = 1;
	    break;
	  case CR :
	    /* do a CR/LF and a -1 line move */
	    hpgl_args_setup(&args);
	    hpgl_CP(&args, pgls);
	    spaces = 0;
	    lines = -1;
	    break;
	  default :
	    break;
	  }
	hpgl_args_set_real(&args, spaces);
	hpgl_args_add_real(&args, lines);
	hpgl_CP(&args, pgls);
	return 0;
}

/* build the path and render it */
 private int
hpgl_print_char(hpgl_state_t *pgls,  hpgl_character_point *character)
{
	hpgl_rendering_mode_t rm = pgls->g.current_render_mode;

	/* clear the current path, if there is one */
	hpgl_call(hpgl_draw_current_path(pgls, hpgl_rm_vector));

	pgls->g.current_render_mode = hpgl_rm_character;
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
		hpgl_args_add_real(&args, character->vertex.x);
		hpgl_args_add_real(&args, character->vertex.y);
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


/* initialize the character buffer, setting state pointers for the
   beginning of the character buffer and the current character within
   the buffer to position 0. */
 private int
hpgl_init_label_buffer(hpgl_state_t *pgls)
{
	return (((pgls->g.label.char_count = 0,
		  pgls->g.label.length = 0.0,
		  pgls->g.label.buffer_size = hpgl_char_count,
		  pgls->g.label.line_ptr =
		  pgls->g.label.char_ptr =
		  gs_alloc_bytes(pgls->memory, 
				 hpgl_char_count,
				 "hpgl_character_data")) == 0) ? 
		e_Memory : 
		0);
}

/* double the size of the current buffer after filling it */
 private int
hpgl_resize_label_buffer(hpgl_state_t *pgls)
{
	byte *new_mem;

	/* resize the buffer */
	if ( (new_mem = gs_resize_object(pgls->memory, 
					 pgls->g.label.line_ptr,
					 pgls->g.label.buffer_size << 1,
					 "hpgl_resize_label_buffer")) == 0 )
	  return e_Memory;
	
	/* set the new buffer size and new memory location */
	pgls->g.label.line_ptr = new_mem;
	pgls->g.label.buffer_size <<= 1;
	
	return 0;
}


/* initializes the character buffer */
 private int
hpgl_destroy_label_buffer(hpgl_state_t *pgls)
{
	gs_free_object(pgls->memory, 
		       pgls->g.label.line_ptr,
		       "hpgl_destroy_label_buffer");
	
	pgls->g.label.char_count = 0;
	pgls->g.label.buffer_size = 0;
	pgls->g.label.line_ptr = 0;
	pgls->g.label.char_ptr = 0;
	pgls->g.label.length = 0.0;
	return 0;
}

/* places a single character in the line buffer */
 private int
hpgl_buffer_char(hpgl_state_t *pgls, byte ch)
{
	/* check if there is room for the new character and resize if
           necessary */
	if ( pgls->g.label.buffer_size == pgls->g.label.char_count )
	  hpgl_call(hpgl_resize_label_buffer(pgls));
	/* store the character */
	*(pgls->g.label.char_ptr++) = ch;
	pgls->g.label.char_count++;
	/* update the current buffers length */
	hpgl_call(hpgl_update_label_length(pgls, ch));
	return 0;
}

 private int
hpgl_process_char(hpgl_state_t *pgls, byte ch)
{
	hpgl_pen_state_t saved_pen_state;
	/* we need to keep this pen position, as updating the label
           origin is relative to the current origin vs. the pen's
           position after the character is rendered */
	hpgl_save_pen_state(pgls, &saved_pen_state, hpgl_pen_all); 
	if ( isprint(ch) )
	  /* ascii is all we have right now */
	  hpgl_call(hpgl_print_char(pgls, hpgl_ascii_char_set[ch - 0x20]));
	hpgl_restore_pen_state(pgls, &saved_pen_state, hpgl_pen_all); 
	hpgl_call(hpgl_update_label_pos(pgls, ch));
	return 0;
}

 
/* return relative coordinates to compensate for origin placement -- LO */
 private int
hpgl_get_character_origin_offset(hpgl_state_t *pgls, gs_point *offset)
{
	hpgl_real_t width, height, label_length, offset_magnitude;

	/* HAS need to account for vertical labels correctly.  The
           label lengthis always parallel to the x-axis wrt LO.  */
	
	label_length = pgls->g.label.length;
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
	    offset_magnitude = 33 * width;
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
		offset->y += offset_magnitude;
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
	const byte *chp = pgls->g.label.line_ptr;
	gs_point offset;
	hpgl_args_t args;
	hpgl_pen_state_t pen_state;

	hpgl_call(hpgl_get_character_origin_offset(pgls, &offset));
	hpgl_save_pen_state(pgls, &pen_state, hpgl_pen_relative);
	hpgl_args_set_real(&args, -offset.x);
	hpgl_args_add_real(&args, -offset.y);
	hpgl_PR(&args, pgls);
	hpgl_restore_pen_state(pgls, &pen_state, hpgl_pen_relative);

        /* process each character updating the current count of
           buffered characters and the current character pointer */
	for (; pgls->g.label.char_count; pgls->g.label.char_count--)
	  hpgl_call(hpgl_process_char(pgls, *(chp++)));
	return 0;
}	    

/* LB ..text..terminator */
 int
hpgl_LB(hpgl_args_t *pargs, hpgl_state_t *pgls)
{	const byte *p = pargs->source.ptr;
	const byte *rlimit = pargs->source.limit;

	/* HAS need to figure out what gets saved here -- relative,
           pen_down, position ?? */
	hpgl_call(hpgl_clear_current_path(pgls));
	/* set the carriage return point and initialize the character
           buffer first time only */
	if ( pargs->phase == 0 )
	  {
	    gs_point pt;
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
		if ( pgls->g.label.print_terminator )
		    hpgl_call(hpgl_buffer_char(pgls, ch));
		hpgl_call(hpgl_process_buffer(pgls));
		hpgl_call(hpgl_destroy_label_buffer(pgls));
		pargs->source.ptr = p;
		return 0;
	      }

	    hpgl_call(hpgl_buffer_char(pgls, ch));

	    /* process the buffer for a carriage return so that we can
               treat the label origin correctly, and initialize a new
               buffer */

	    if ( ch == CR )
	      {
		hpgl_call(hpgl_process_buffer(pgls));
		hpgl_call(hpgl_destroy_label_buffer(pgls));
		hpgl_call(hpgl_init_label_buffer(pgls));
	      }
	  }
	pargs->source.ptr = p;
	return e_NeedData;
}

/* LO; */
 int
hpgl_LO(hpgl_args_t *pargs, hpgl_state_t *pgls)
{	int origin = 1;

	hpgl_arg_c_int(pargs, &origin);
	if ( origin < 1 || origin == 10 || origin == 20 || origin > 21 )
	  return e_Range;
	pgls->g.label.origin = origin;
	return 0;
}

/* SA; */
 int
hpgl_SA(hpgl_args_t *pargs, hpgl_state_t *pgls)
{	
	pgls->g.font_selected = 1;
	pgls->g.font = 0;	/* recompute from params */
	return 0;
}

/* SB [mode]; */
 int
hpgl_SB(hpgl_args_t *pargs, hpgl_state_t *pgls)
{	int mode = 0;

	if ( hpgl_arg_c_int(pargs, &mode) && (mode & ~1) )
	  return e_Range;
	pgls->g.bitmap_fonts_allowed = mode;
	/**** RESELECT FONT IF NO LONGER ALLOWED ****/
	return 0;
}

/* SD [kind,value...]; */
 int
hpgl_SD(hpgl_args_t *pargs, hpgl_state_t *pgls)
{	
	return hpgl_font_definition(pargs, pgls, 0);
}

/* SI [width,height]; */
 int
hpgl_SI(hpgl_args_t *pargs, hpgl_state_t *pgls)
{	
	return hpgl_character_size(pargs, pgls, false);
}

/* SL [slant]; */
 int
hpgl_SL(hpgl_args_t *pargs, hpgl_state_t *pgls)
{	hpgl_real_t slant = 0;

	hpgl_arg_c_real(pargs, &slant);
	pgls->g.character.slant = slant;
	return 0;
}

/* SR [width,height]; */
 int
hpgl_SR(hpgl_args_t *pargs, hpgl_state_t *pgls)
{	
	return hpgl_character_size(pargs, pgls, true);
}

/* SS; */
 int
hpgl_SS(hpgl_args_t *pargs, hpgl_state_t *pgls)
{	
	pgls->g.font_selected = 0;
	pgls->g.font = 0;	/* recompute from params */
	return 0;
}

/* TD [mode]; */
 int
hpgl_TD(hpgl_args_t *pargs, hpgl_state_t *pgls)
{	int mode = 0;

	if ( hpgl_arg_c_int(pargs, &mode) && (mode & ~1) )
	  return e_Range;
	pgls->g.transparent_data = mode;
	return 0;
}

/* Initialization */
private int
pgchar_do_init(gs_memory_t *mem)
{		/* Register commands */
	DEFINE_HPGL_COMMANDS
	  HPGL_COMMAND('A', 'D', hpgl_AD, 0),		/* kind/value pairs */
	  HPGL_COMMAND('C', 'F', hpgl_CF, 0),
	  HPGL_COMMAND('C', 'P', hpgl_CP, hpgl_cdf_lost_mode_cleared),
	  HPGL_COMMAND('D', 'I', hpgl_DI, 0),
	  HPGL_COMMAND('D', 'R', hpgl_DR, 0),
	  /* DT has special argument parsing, so it must handle skipping */
	  /* in polygon mode itself. */
	  HPGL_COMMAND('D', 'T', hpgl_DT, hpgl_cdf_polygon),
	  HPGL_COMMAND('D', 'V', hpgl_DV, 0),
	  HPGL_COMMAND('E', 'S', hpgl_ES, 0),
	  HPGL_COMMAND('F', 'I', hpgl_FI, 0),
	  HPGL_COMMAND('F', 'N', hpgl_FN, 0),
	  /* LB also has special argument parsing. */
	  HPGL_COMMAND('L', 'B', hpgl_LB, hpgl_cdf_polygon|hpgl_cdf_lost_mode_cleared),
	  HPGL_COMMAND('L', 'O', hpgl_LO, 0),
	  HPGL_COMMAND('S', 'A', hpgl_SA, 0),
	  HPGL_COMMAND('S', 'B', hpgl_SB, 0),
	  HPGL_COMMAND('S', 'D', hpgl_SD, 0),		/* kind/value pairs */
	  HPGL_COMMAND('S', 'I', hpgl_SI, 0),
	  HPGL_COMMAND('S', 'L', hpgl_SL, 0),
	  HPGL_COMMAND('S', 'R', hpgl_SR, 0),
	  HPGL_COMMAND('S', 'S', hpgl_SS, 0),
	  HPGL_COMMAND('T', 'D', hpgl_TD, 0),
	END_HPGL_COMMANDS
	return 0;
}
const pcl_init_t pgchar_init = {
  pgchar_do_init, 0
};
