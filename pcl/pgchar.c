/* Copyright (C) 1996 Aladdin Enterprises.  All rights reserved.
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
	  { double hypot;
	    if ( !hpgl_arg_c_real(pargs, &rise) || (run == 0 && rise == 0) )
	      return e_Range;
	    hypot = sqrt(run * run + rise * rise);
	    run /= hypot;
	    rise /= hypot;
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
hpgl_get_current_cell_width(hpgl_state_t *pgls, hpgl_real_t *width)
{
  	/* HAS - just supports current font */
	*width = inches_2_plu((1.0 / 
			       (pgls->g.font_selection[0].params.pitch_100ths / 
				100.0)));
	/* HAS NO spec here */
	*width *= 1.5;
	return 0;
}

private int
hpgl_get_current_cell_height(hpgl_state_t *pgls, hpgl_real_t *height)
{
  	/* HAS - just supports current font */
	*height = points_2_plu((pgls->g.font_selection[0].params.height_4ths) /4.0);
	/* HAS NO spec here */
	*height *= 2.0;
	return 0;
}

/* CP [spaces,lines]; HAS -- an arcane implementation, but it is late */
 int
hpgl_CP(hpgl_args_t *pargs, hpgl_state_t *pgls)
{	
	hpgl_real_t spaces = 0.0;
	hpgl_real_t lines = 1.0;
	
	/* CP does its work with the pen up -- we restore the current
           state at the end of the routine */
	hpgl_save_pen_down_state(pgls);
	/* We will take advantage of PR so save the current relative
           state as well */
	hpgl_save_pen_relative_state(pgls);

	if ( hpgl_arg_c_real(pargs, &spaces) )
	  { if ( !hpgl_arg_c_real(pargs, &lines) )
	      return e_Range;
	  }

	{

	  hpgl_real_t width, height;
	  hpgl_args_t args;

	  /* get the character cell height and width */
	  hpgl_call(hpgl_get_current_cell_width(pgls, &width));
	  hpgl_call(hpgl_get_current_cell_height(pgls, &height));

	  /* set the pen up */
	  hpgl_args_set_real(&args, pgls->g.pos.x);
	  hpgl_args_add_real(&args, pgls->g.pos.y);
	  hpgl_PU(&args, pgls);
	  
	  /* relative move to new position, not forgetting ES */
	  hpgl_args_set_real(&args, ((spaces + pgls->g.character.extra_space.x) * 
				     width));
	  hpgl_args_add_real(&args, -((lines + pgls->g.character.extra_space.y) * 
				      height));
	  hpgl_PR(&args, pgls);

	  /* save the current position in GL/2's state */
	  hpgl_call(hpgl_set_current_position(pgls, (gs_point *) NULL));

	}
	/* put the pen back the way it was, of course we do not want
           to restore the pen's position */
	hpgl_restore_pen_relative_state(pgls);
	hpgl_restore_pen_down_state(pgls);
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
	return hpgl_select_font(pargs, pgls, 0);
}

/* updates the current cursor position, which is the same as the
   current pen postion */

static int
hpgl_update_label_pos(hpgl_state_t *pgls, byte ch)
{
	int spaces = 1, lines = 0;
  	hpgl_args_t args;
	/* HAS missing logic here. */
	switch (ch) {
	case BS :
	  spaces = -1;
	  break;
	case LF :
	  spaces = 0;
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
static int
hpgl_print_char(hpgl_state_t *pgls,  hpgl_character_point *character)
{
	hpgl_rendering_mode_t rm = pgls->g.current_render_mode;

	/* set the current gs point to the origin */
	hpgl_call(hpgl_set_current_position(pgls, (gs_point *)NULL));

	/* clear the current path, if there is one */
	hpgl_call(hpgl_draw_current_path(pgls, hpgl_rm_vector));

	pgls->g.current_render_mode = hpgl_rm_character;

	while (character->operation != hpgl_char_end)
	  {
	    hpgl_args_t args;
	    /* setup the arguments */
	    hpgl_args_setup(&args);
	    /* all operations supply arguments except: */
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
	    
 static int
hpgl_process_char(hpgl_state_t *pgls, byte ch)
{
  
	if ( isprint(ch) )
	  /* ascii is all we have right now */
	  hpgl_call(hpgl_print_char(pgls, hpgl_ascii_char_set[ch - 0x20]));

	hpgl_call(hpgl_update_label_pos(pgls, ch));

	return 0;
}

/* LB ..text..terminator */
 int
hpgl_LB(hpgl_args_t *pargs, hpgl_state_t *pgls)
{	const byte *p = pargs->source.ptr;
	const byte *rlimit = pargs->source.limit;

	/* we use the hpgl drawing machinery to stroke the glyph so we
           need to save the state of the pen and restore it after the
           glyph is printed.  HAS needs work */
	bool rel  = pgls->g.relative;
	bool down = pgls->g.pen_down;
	gs_point pos = pgls->g.pos;

	while ( p < rlimit )
	  { byte ch = *++p;
	    if_debug1('I',
		      (ch == '\\' ? " \\%c" : ch >= 33 && ch <= 126 ? " %c" :
		       " \\%03o"),
		      ch);
	    if ( ch == pgls->g.label.terminator )
	      { 
		if ( pgls->g.label.print_terminator )
		    hpgl_call(hpgl_process_char(pgls, ch));

		/* restore pen's state */
		pgls->g.relative = rel;
		pgls->g.pen_down = down;
		pgls->g.pos = pos;
		pargs->source.ptr = p;
		return 0;
	      }
	    hpgl_call(hpgl_process_char(pgls, ch));
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
	  HPGL_COMMAND('A', 'D', hpgl_AD),		/* kind/value pairs */
	  HPGL_COMMAND('C', 'F', hpgl_CF),
	  HPGL_COMMAND('C', 'P', hpgl_CP),
	  HPGL_COMMAND('D', 'I', hpgl_DI),
	  HPGL_COMMAND('D', 'R', hpgl_DR),
	  /* DT has special argument parsing, so it must handle skipping */
	  /* in polygon mode itself. */
	  HPGL_POLY_COMMAND('D', 'T', hpgl_DT),
	  HPGL_COMMAND('D', 'V', hpgl_DV),
	  HPGL_COMMAND('E', 'S', hpgl_ES),
	  HPGL_COMMAND('F', 'I', hpgl_FI),
	  HPGL_COMMAND('F', 'N', hpgl_FN),
	  /* LB also has special argument parsing. */
	  HPGL_POLY_COMMAND('L', 'B', hpgl_LB),
	  HPGL_COMMAND('L', 'O', hpgl_LO),
	  HPGL_COMMAND('S', 'A', hpgl_SA),
	  HPGL_COMMAND('S', 'B', hpgl_SB),
	  HPGL_COMMAND('S', 'D', hpgl_SD),		/* kind/value pairs */
	  HPGL_COMMAND('S', 'I', hpgl_SI),
	  HPGL_COMMAND('S', 'L', hpgl_SL),
	  HPGL_COMMAND('S', 'R', hpgl_SR),
	  HPGL_COMMAND('S', 'S', hpgl_SS),
	  HPGL_COMMAND('T', 'D', hpgl_TD),
	END_HPGL_COMMANDS
	return 0;
}
const pcl_init_t pgchar_init = {
  pgchar_do_init, 0
};
