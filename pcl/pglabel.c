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
#include "gscoord.h"
#include "gsline.h"

#define STICK_FONT_TYPEFACE 48

/* ------ Font selection ------- */

/* Recompute the current font if necessary. */
private int
hpgl_recompute_font(hpgl_state_t *pgls)
{	pcl_font_selection_t *pfs =
	  &pgls->g.font_selection[pgls->g.font_selected];

	{ int code = pcl_reselect_font(pfs, pgls /****** NOTA BENE ******/);
	  if ( code < 0 )
	    return code;
	}
	/*
	 ****** HACK: ignore the results of pcl_reselect font;
	 ****** always use the stick font.
	 */
#if 0
	if ( (pfs->params.typeface_family & 0xfff) == STICK_FONT_TYPEFACE )
#endif
	  { /* Select the stick font. */
	    pl_font_t *font = &pgls->g.stick_font[pgls->g.font_selected];

	    /*
	     * The stick font is protean: set its proportional spacing,
	     * style, and stroke weight parameters to the requested ones.
	     * We could fill in some of the other characteristics earlier,
	     * but it's simpler to do it here.
	     */
	    font->scaling_technology = plfst_TrueType;	/****** WRONG ******/
	    font->font_type = plft_7bit_printable;	/****** WRONG ******/
	    font->params = pfs->params;
	    font->params.typeface_family = STICK_FONT_TYPEFACE;
	    pfs->font = font;
	    pfs->map = 0;	/****** WRONG ******/
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
	floatp wtemp;

	*width =
	  (pfs->params.proportional_spacing ?
	   /****** HACK: only works for stick font ******/
	   (ch < 0x20 || ch > 0x7e || ch == ' ' ? 0.667 :
	    (hpgl_stick_char_width(ch, &wtemp), wtemp))
	     * points_2_plu(pfs->params.height_4ths / 4.0) :
	   inches_2_plu(pl_fp_pitch_cp(&pfs->params) / 7200.0));
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
/* used to handle control characters within LB. */
private int
hpgl_move_cursor_by_characters(hpgl_state_t *pgls, hpgl_real_t spaces,
  hpgl_real_t lines, bool do_CR)
{
	/* HAS ***HACK **** HACK **** HACK */
	/* save the current render mode and temporarily set it to
           vector mode. This allows the HPGL/2 graphics not to apply
           the character ctm for the label position move.  This must
           be handled more "pleasantly" in the future */

	hpgl_rendering_mode_t rm = pgls->g.current_render_mode;

	pgls->g.current_render_mode = hpgl_rm_vector;

	{
	  hpgl_real_t width, height;
	  gs_point pt, relpos;

	  /* get the character cell height and width */
	  hpgl_call(hpgl_get_char_width(pgls, ' ', &width));
	  hpgl_call(hpgl_get_current_cell_height(pgls, &height));
	  
	  /* get the current position */
	  hpgl_call(hpgl_get_current_position(pgls, &pt));
	  
	  /* calculate the next label position in relative coordinates.  If
             CR then the X position is simply the x coordinate of the
             current carriage return point.  We use relative coordinates for
             the movement in either case. */
	  if (do_CR)
	    relpos.x = -(pt.x - pgls->g.carriage_return_pos.x);
	  else
	    relpos.x = ((width + pgls->g.character.extra_space.x) * spaces);

	  /* the y coordinate is independant of CR/LF status */
	  relpos.y = -((height + pgls->g.character.extra_space.y) * lines);

	  /* a relative move to the new position */
	  hpgl_call(hpgl_add_point_to_path(pgls, relpos.x, relpos.y,
					   hpgl_plot_move_relative));

	  if ( lines != 0 )
	    { /* update the Y position of the carriage return point */
	      hpgl_call(hpgl_get_current_position(pgls, &pt));
	      pgls->g.carriage_return_pos.y = pt.y;
	    }
	}

	pgls->g.current_render_mode = rm;
	return 0;
}

/* CP [spaces,lines]; */
/* CP [;] */
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
hpgl_print_char(hpgl_state_t *pgls, uint ch)
{
	hpgl_rendering_mode_t rm = pgls->g.current_render_mode;
	/*
	 * The default stick font size is "32x32 units", but this is
	 * meaningless if the size of the "units" aren't specified,
	 * and the TRM gives no clue about this.  Instead, since our
	 * stick font is based on a 1x1 cell, simply use points_2_plu;
	 * but according to TRM 23-18, the cell is 2/3 of the point size.
	 * Scale the data according to the current font height.
	 * (Eventually we must take DI/DR/SI/SL/SR into account.)
	 */
	const pcl_font_selection_t *pfs = 
	  &pgls->g.font_selection[pgls->g.font_selected];
	double scale = points_2_plu(pfs->params.height_4ths / 4.0) * 2/3;
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
	 * on the CTM so that we can add the symbol outline based on
	 * a 1x1-unit cell.
	 */
	{ gs_matrix save_ctm;

	  gs_currentmatrix(pgls->pgs, &save_ctm);
	  gs_scale(pgls->pgs, scale, scale);
	  hpgl_stick_append_char(pgls, ch);
	  gs_setmatrix(pgls->pgs, &save_ctm);
	}
	/*
	 * Since we're using the stroked font, patch the pen width
	 * to reflect the stroke weight.
	 */
	{ float save_width = pgls->g.pen.width[pgls->g.pen.selected];
	  bool save_relative = pgls->g.pen.width_relative;
	  int weight = pfs->params.stroke_weight;

	  if ( weight != 9999 )
	    { pgls->g.pen.width[pgls->g.pen.selected] =
		(0.05 + weight * 0.005) * scale; /* adhoc */
	      pgls->g.pen.width_relative = true;
	    }
	  hpgl_call(hpgl_draw_current_path(pgls, hpgl_rm_character));
	  pgls->g.pen.width[pgls->g.pen.selected] = save_width;
	  pgls->g.pen.width_relative = save_relative;
	}
	pgls->g.current_render_mode = rm;

	/* Advance by the character escapement. */
	{ hpgl_real_t dx;

	  hpgl_call(hpgl_get_char_width(pgls, ch, &dx));
	  hpgl_call(hpgl_add_point_to_path(pgls, pos.x + dx, pos.y,
					   hpgl_plot_move_absolute));
	}

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

	      if ( ch >= 0x20 || pgls->g.transparent_data )
		{
		  hpgl_call(hpgl_get_char_width(pgls, ch, &width));
		  label_length += width;
		}
	      else
		switch (ch) 
		{
		case BS :
		  if ( width == 0.0 ) /* BS as first char of string */
		    hpgl_call(hpgl_get_char_width(pgls, ' ', &width));
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
		  if ( pgls->g.font_selected == 0 )
		    break;
		  pgls->g.font_selected = 0;
shift:		  hpgl_call(hpgl_recompute_font(pgls));
		  break;
		case SO :
		  if ( pgls->g.font_selected == 1 )
		    break;
		  pgls->g.font_selected = 1;
		  goto shift;
		default :
		  break;
		}
	    }
	  if ( save_index != pgls->g.font_selected )
	    {
	      pgls->g.font_selected = save_index;
	      hpgl_call(hpgl_recompute_font(pgls));
	    }
	}
	hpgl_call(hpgl_get_character_origin_offset(pgls, label_length, &offset));
	hpgl_call(hpgl_add_point_to_path(pgls, -offset.x, -offset.y,
					 hpgl_plot_move_relative));

	{
	  int i;
	  for ( i = 0; i < pgls->g.label.char_count; ++i )
	    { byte ch = pgls->g.label.buffer[i];

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
		      if ( pgls->g.font_selected != 0 )
			{
			  pgls->g.font_selected = 0;
			  hpgl_call(hpgl_recompute_font(pgls));
			}
		      continue;
		    case SO :
		      if ( pgls->g.font_selected != 1 )
			{
			  pgls->g.font_selected = 1;
			  hpgl_call(hpgl_recompute_font(pgls));
			}
		      continue;
		    default :
		      goto print;
		    }
		  hpgl_move_cursor_by_characters(pgls, spaces, lines, do_CR);
		  continue;
		}
print:	      hpgl_call(hpgl_print_char(pgls, ch));
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
	    /* Make sure the font is current. */
	    if ( pgls->g.font == 0 )
	      hpgl_call(hpgl_recompute_font(pgls));
	    /* initialize the character buffer first time only */
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
