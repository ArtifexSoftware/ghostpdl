/* Copyright (C) 1996, 1997, 1998 Aladdin Enterprises.  All rights
   reserved.  Unauthorized use, copying, and/or distribution
   prohibited.  */

/* pgvector.c */
/* HP-GL/2 vector commands */

#include "stdio_.h"		/* for gdebug.h */
#include "gdebug.h"
#include "pgmand.h"
#include "pggeom.h"
#include "pgdraw.h"
#include "pgmisc.h"
#include "gspath.h"
#include "gscoord.h"
#include "math_.h"

/* ------ Internal procedures ------ */

/* Draw an arc (AA, AR). */
 private int
hpgl_arc(hpgl_args_t *pargs, hpgl_state_t *pgls, bool relative)
{	
	hpgl_real_t x_center, y_center, sweep, x_current, y_current, chord_angle = 5;
	hpgl_real_t radius, start_angle;

	if ( !hpgl_arg_units(pargs, &x_center) ||
	     !hpgl_arg_units(pargs, &y_center) ||
	     !hpgl_arg_c_real(pargs, &sweep)
	     )
	  return e_Range;

	hpgl_arg_c_real(pargs, &chord_angle);

	x_current = pgls->g.pos.x;
	y_current = pgls->g.pos.y;

	if ( relative )
	  {
	    x_center += x_current;
	    y_center += y_current;
	  }

	radius =
	  hpgl_compute_distance(x_current, y_current, x_center, y_center);

	start_angle = radians_to_degrees *
	  hpgl_compute_angle(x_current - x_center, y_current - y_center);

	hpgl_call(hpgl_add_arc_to_path(pgls, x_center, y_center,
				       radius, start_angle, sweep,
				       (sweep < 0.0 ) ?
				       -chord_angle : chord_angle, false,
				       pgls->g.move_or_draw, true));
	
	pgls->g.carriage_return_pos = pgls->g.pos;
	return 0;
}

/* Draw a 3-point arc (AT, RT). */
 private int
hpgl_arc_3_point(hpgl_args_t *pargs, hpgl_state_t *pgls, bool relative)
{	
	hpgl_real_t x_start = pgls->g.pos.x, y_start = pgls->g.pos.y;
	hpgl_real_t x_inter, y_inter, x_end, y_end;
	hpgl_real_t chord_angle = 5;

	if ( !hpgl_arg_units(pargs, &x_inter) ||
	     !hpgl_arg_units(pargs, &y_inter) ||
	     !hpgl_arg_units(pargs, &x_end) ||
	     !hpgl_arg_units(pargs, &y_end)
	   )
	  return e_Range;

	hpgl_arg_c_real(pargs, &chord_angle);

	if ( relative )
	  {
	    x_inter += x_start;
	    y_inter += y_start;
	    x_end += x_start;
	    y_end += y_start;
	  }
	
	hpgl_call(hpgl_add_arc_3point_to_path(pgls,
					      x_start, y_start,
					      x_inter, y_inter,
					      x_end, y_end, chord_angle,
					      pgls->g.move_or_draw));
	pgls->g.carriage_return_pos = pgls->g.pos;
	return 0;
}

/* Draw a Bezier (BR, BZ). */
int
hpgl_bezier(hpgl_args_t *pargs, hpgl_state_t *pgls, bool relative)
{	
	hpgl_real_t x_start, y_start;
	/*
	 * Since these commands take an arbitrary number of arguments,
	 * we reset the argument bookkeeping after each group.
	 */
	for ( ; ; )
	  {
	    hpgl_real_t coords[6];
	    int i;

	    for ( i = 0; i < 6 && hpgl_arg_units(pargs, &coords[i]); ++i )
	      ;
	    switch ( i )
	      {
	      case 0:		/* done */
		pgls->g.carriage_return_pos = pgls->g.pos;
		/* draw the path */
		if ( !pgls->g.polygon_mode )
		  hpgl_call(hpgl_draw_current_path(pgls, hpgl_rm_vector));
		return 0;
	      case 6:
		break;
	      default:
		return e_Range;
	      }

	    x_start = pgls->g.pos.x;
	    y_start = pgls->g.pos.y;

	    if ( relative )
	      hpgl_call(hpgl_add_bezier_to_path(pgls, x_start, y_start,
						x_start + coords[0],
						y_start + coords[1],
						x_start + coords[2],
						y_start + coords[3],
						x_start + coords[4],
						y_start + coords[5],
						pgls->g.move_or_draw));
	    else
	      hpgl_call(hpgl_add_bezier_to_path(pgls, x_start, y_start,
						coords[0], coords[1],
						coords[2], coords[3],
						coords[4], coords[5],
						pgls->g.move_or_draw));
	    /* Prepare for the next set of points. */
	    hpgl_args_init(pargs);
	  }
}

/* Plot points, symbols, or lines (PA, PD, PR, PU). */
int
hpgl_plot(hpgl_args_t *pargs, hpgl_state_t *pgls, hpgl_plot_function_t func)
{	

	/*
	 * Since these commands take an arbitrary number of arguments,
	 * we reset the argument bookkeeping after each group.
	 */

	hpgl_real_t x, y;
	if ( hpgl_plot_is_move(func) )
	  hpgl_call(hpgl_close_path(pgls));

	while ( hpgl_arg_units(pargs, &x) && hpgl_arg_units(pargs, &y) )
	  {
	    pargs->phase = 1;	/* we have arguments */
	    hpgl_call(hpgl_add_point_to_path(pgls, x, y, func, true));
	    /* Prepare for the next set of points. */
	    if ( pgls->g.symbol_mode != 0 )
	      hpgl_call(hpgl_print_symbol_mode_char(pgls));
	    hpgl_args_init(pargs);
	  }

	/* check for no argument case */
	if ( !pargs->phase)
	  {
	    if ( hpgl_plot_is_relative(func) )
	      /*hpgl_call(hpgl_add_point_to_path(pgls, 0.0, 0.0, func, true)) */;
	    else
	      {
		gs_point cur_point;
		hpgl_call(hpgl_get_current_position(pgls, &cur_point));
		hpgl_call(hpgl_add_point_to_path(pgls, cur_point.x,
						 cur_point.y, func, true));
	      }
	  }
	if ( pgls->g.symbol_mode != 0 )
	  hpgl_call(hpgl_print_symbol_mode_char(pgls));
	pgls->g.carriage_return_pos = pgls->g.pos;
	return 0;
}

/* ------ Commands ------ */
 private int
hpgl_draw_arc(hpgl_state_t *pgls)
{
	if ( !pgls->g.polygon_mode )
	  hpgl_call(hpgl_draw_current_path(pgls, hpgl_rm_vector));
	return 0;
}

/* AA xcenter,ycenter,sweep[,chord]; */
 int
hpgl_AA(hpgl_args_t *pargs, hpgl_state_t *pgls)
{	
        hpgl_call(hpgl_arc(pargs, pgls, false));
	hpgl_call(hpgl_draw_arc(pgls));
	return 0;
}

/* AR xcenter,ycenter,sweep[,chord]; */
 int
hpgl_AR(hpgl_args_t *pargs, hpgl_state_t *pgls)
{	
        hpgl_call(hpgl_arc(pargs, pgls, true));
	hpgl_call(hpgl_draw_arc(pgls));
	return 0;
}

/* AT xinter,yinter,xend,yend[,chord]; */
 int
hpgl_AT(hpgl_args_t *pargs, hpgl_state_t *pgls)
{	
	hpgl_call(hpgl_arc_3_point(pargs, pgls, false));
	hpgl_call(hpgl_draw_arc(pgls));
	return 0;
}

/* BR x1,y1,x2,y2,x3,y3...; */
 int
hpgl_BR(hpgl_args_t *pargs, hpgl_state_t *pgls)
{	return hpgl_bezier(pargs, pgls, true);
}

/* BZ x1,y1,x2,y2,x3,y3...; */
int
hpgl_BZ(hpgl_args_t *pargs, hpgl_state_t *pgls)
{	return hpgl_bezier(pargs, pgls, false);
}

/* CI radius[,chord]; */
int
hpgl_CI(hpgl_args_t *pargs, hpgl_state_t *pgls)
{	
	hpgl_real_t radius, chord = 5;
	bool reset_ctm = true;
	gs_point pos = pgls->g.pos; /* center */

	if ( !hpgl_arg_units(pargs, &radius) )
	  return e_Range;
	hpgl_arg_c_real(pargs, &chord);
	/* draw the arc/circle */
	hpgl_call(hpgl_add_arc_to_path(pgls, pgls->g.pos.x, pgls->g.pos.y,
				       radius, 0.0, 360.0, chord, true,
				       hpgl_plot_draw_absolute, reset_ctm));
	if ( !pgls->g.polygon_mode )
	  hpgl_call(hpgl_draw_arc(pgls));
	/* move back to the center */
	hpgl_call(hpgl_add_point_to_path(pgls, pos.x, pos.y,
					 hpgl_plot_move_absolute, true));
	/* restore the ctm - i.e. the ctm with picture frame scaling */
	hpgl_set_ctm(pgls);
	return 0;

}

/* PA x,y...; */
int
hpgl_PA(hpgl_args_t *pargs, hpgl_state_t *pgls)
{	
	pgls->g.relative_coords = hpgl_plot_absolute;
	return hpgl_plot(pargs, pgls,
			 pgls->g.move_or_draw | hpgl_plot_absolute);
}

/* PD (d)x,(d)y...; */
int
hpgl_PD(hpgl_args_t *pargs, hpgl_state_t *pgls)
{	
	pgls->g.move_or_draw = hpgl_plot_draw;
	return hpgl_plot(pargs, pgls,
			 hpgl_plot_draw | pgls->g.relative_coords);
}

/* PE (flag|value|coord)*; */
/*
 * We record the state of the command in the 'phase' as follows:
 */
enum {
	pe_pen_up = 1,	/* next coordinate are pen-up move */
	pe_absolute = 2,	/* next coordinates are absolute */
	pe_7bit = 4,		/* use 7-bit encoding */
};
#define pe_fbits_shift 3	/* ... + # of fraction bits << this much */
private bool pe_args(P3(hpgl_args_t *, int32 *, int));
int
hpgl_PE(hpgl_args_t *pargs, hpgl_state_t *pgls)
{	/*
	 * To simplify the control structure here, we require that
	 * the input buffer be large enough to hold 2 coordinates
	 * in the base 32 encoding, i.e., 14 bytes.  We're counting on
	 * there not being any whitespace within coordinate values....
	 */
	const byte *p = pargs->source.ptr;
	const byte *rlimit = pargs->source.limit;

	while ( p < rlimit )
	  { byte ch = *(pargs->source.ptr = ++p);
	  switch ( ch & 127 /* per spec */ )
	    {
	    case ';':
	      pgls->g.carriage_return_pos = pgls->g.pos;
	      return 0;
	    case ':':
	      if_debug0('I', "\n  :");
	      { int32 pen;
	      if ( !pe_args(pargs, &pen, 1) )
		{ pargs->source.ptr = p - 1;
		break;
		}
	      {
		hpgl_args_t args;
		hpgl_args_set_int(&args, pen);
		/* Note SP is illegal in polygon mode we must handle that here */
		if ( !pgls->g.polygon_mode )
		  hpgl_call(hpgl_SP(&args, pgls));
	      }
	      }
	      p = pargs->source.ptr;
	      continue;
	    case '<':
	      if_debug0('I', "\n  <");
	      pargs->phase |= pe_pen_up;
	      continue;
	    case '>':
	      if_debug0('I', "\n  >");
	      { int32 fbits;
	      if ( !pe_args(pargs, &fbits, 1) )
		{ pargs->source.ptr = p - 1;
		break;
		}
	      if ( fbits < -26 || fbits > 26 )
		return e_Range;
	      pargs->phase =
		(pargs->phase & (-1 << pe_fbits_shift)) +
		(fbits << pe_fbits_shift);
	      }
	      p = pargs->source.ptr;
	      continue;
	    case '=':
	      if_debug0('I', "  =");
	      pargs->phase |= pe_absolute;
	      continue;
	    case '7':
	      if_debug0('I', "\n  7");
	      pargs->phase |= pe_7bit;
	      continue;
	    case ESC:
		/*
		 * This is something of a hack.  Apparently we're supposed
		 * to parse all PCL commands embedded in the GL/2 stream,
		 * and simply ignore everything except the 3 that end GL/2
		 * mode.  Instead, we simply stop parsing PE arguments as
		 * soon as we see an ESC.
		 */
		if ( ch == ESC ) /* (might be ESC+128) */
		  { pargs->source.ptr = p - 1; /* rescan ESC */
		    pgls->g.carriage_return_pos = pgls->g.pos;
		    return 0;
		  }
		/* falls through */
	    default:
	      if ( (ch & 127) <= 32 || (ch & 127) == 127 )
		continue;
	      pargs->source.ptr = p - 1;
	      {
		int32 xy[2];
		hpgl_plot_function_t func;

		if ( !pe_args(pargs, xy, 2) )
		  break;
		func =
		  (pargs->phase & pe_pen_up ? hpgl_plot_move :
		   hpgl_plot_draw) |
		  (pargs->phase & pe_absolute ? hpgl_plot_absolute :
		   hpgl_plot_relative);
		hpgl_call(hpgl_add_point_to_path(pgls, (floatp)xy[0],
						 (floatp)xy[1], func, true));
	      }
	      pargs->phase &= ~(pe_pen_up | pe_absolute);
	      p = pargs->source.ptr;
	      continue;
	    }
	  break;
	  }
	return e_NeedData;
}
/* Get an encoded value from the input.  Return false if we ran out of */
/* input data.  Ignore syntax errors (!). */
private bool
pe_args(hpgl_args_t *pargs, int32 *pvalues, int count)
{	const byte *p = pargs->source.ptr;
	const byte *rlimit = pargs->source.limit;
	int i;

	for ( i = 0; i < count; ++i )
	  { int32 value = 0;
	    int shift = 0;

	    for ( ; ; )
	      { int ch;

	      if ( p >= rlimit )
		return false;
	      ch = *++p;
	      if ( (ch & 127) <= 32 || (ch & 127) == 127 )
		continue;
	      if ( pargs->phase & pe_7bit )
		{ ch -= 63;
		if ( ch & ~63 )
		  goto syntax_error;
		value += (int32)(ch & 31) << shift;
		shift += 5;
		if ( ch & 32 )
		  break;
		}
	      else
		{ ch -= 63;
		if ( ch & ~191 )
		  goto syntax_error;
		value += (int32)(ch & 63) << shift;
		shift += 6;
		if ( ch & 128 )
		  break;
		}
	      }
	    pvalues[i] = (value & 1 ? -(value >> 1) : value >> 1);
	    if_debug2('I', "  [%ld,%d]", (long)pvalues[i],
		      (int)arith_rshift(pargs->phase, pe_fbits_shift));
	  }
	pargs->source.ptr = p;
	return true;
syntax_error:
	/* Just ignore everything we've parsed up to this point. */
	pargs->source.ptr = p;
	return false;
}

/* PR dx,dy...; */
int
hpgl_PR(hpgl_args_t *pargs, hpgl_state_t *pgls)
{	
	pgls->g.relative_coords = hpgl_plot_relative;
	return hpgl_plot(pargs, pgls,
			 pgls->g.move_or_draw | hpgl_plot_relative);

}

/* PU (d)x,(d)y...; */
int
hpgl_PU(hpgl_args_t *pargs, hpgl_state_t *pgls)
{	
	pgls->g.move_or_draw = hpgl_plot_move;
	return hpgl_plot(pargs, pgls,
			 hpgl_plot_move | pgls->g.relative_coords);
}

/* RT xinter,yinter,xend,yend[,chord]; */
int
hpgl_RT(hpgl_args_t *pargs, hpgl_state_t *pgls)
{	
	hpgl_call(hpgl_arc_3_point(pargs, pgls, true));
	hpgl_call(hpgl_draw_arc(pgls));
	return 0;
}

/* Initialization */
private int
pgvector_do_init(gs_memory_t *mem)
{	
	/* Register commands */
	DEFINE_HPGL_COMMANDS
		HPGL_COMMAND('A', 'A', hpgl_AA, hpgl_cdf_polygon|hpgl_cdf_lost_mode_cleared),
		HPGL_COMMAND('A', 'R', hpgl_AR, hpgl_cdf_polygon|hpgl_cdf_lost_mode_cleared),
		HPGL_COMMAND('A', 'T', hpgl_AT, hpgl_cdf_polygon|hpgl_cdf_lost_mode_cleared),
		HPGL_COMMAND('B', 'R', hpgl_BR, hpgl_cdf_polygon),	/* argument pattern can repeat */
		HPGL_COMMAND('B', 'Z', hpgl_BZ, hpgl_cdf_polygon),	/* argument pattern can repeat */
		HPGL_COMMAND('C', 'I', hpgl_CI, hpgl_cdf_polygon|hpgl_cdf_lost_mode_cleared),
		HPGL_COMMAND('P', 'A', hpgl_PA, hpgl_cdf_polygon),	/* argument pattern can repeat */
		HPGL_COMMAND('P', 'D', hpgl_PD, hpgl_cdf_polygon),	/* argument pattern can repeat */
		HPGL_COMMAND('P', 'E', hpgl_PE, hpgl_cdf_polygon),
		HPGL_COMMAND('P', 'R', hpgl_PR, hpgl_cdf_polygon|hpgl_cdf_lost_mode_cleared),	/* argument pattern can repeat */
		HPGL_COMMAND('P', 'U', hpgl_PU, hpgl_cdf_polygon),	/* argument pattern can repeat */
		HPGL_COMMAND('R', 'T', hpgl_RT, hpgl_cdf_polygon|hpgl_cdf_lost_mode_cleared),
	END_HPGL_COMMANDS
	return 0;
}
const pcl_init_t pgvector_init = {
	pgvector_do_init, 0
};
