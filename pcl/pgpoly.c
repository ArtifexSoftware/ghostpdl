/* Copyright (C) 1996 Aladdin Enterprises.  All rights reserved.
   Unauthorized use, copying, and/or distribution prohibited.
 */

/* pgpoly.c */
/* HP-GL/2 polygon commands */
#include "std.h"
#include "pgmand.h"
#include "pgdraw.h"
#include "pggeom.h"

/* ------ Internal procedures ------ */

/* Define fill/edge and absolute/relative flags. */
#define DO_EDGE 1
#define DO_RELATIVE 2

/* Build a rectangle in polygon mode used by (EA, ER, RA, RR). */
private int
hpgl_rectangle(hpgl_args_t *pargs, hpgl_state_t *pgls, int flags)
{	hpgl_real_t x2, y2;
	if ( !hpgl_arg_units(pargs, &x2) || !hpgl_arg_units(pargs, &y2) )
	  return e_Range;

	if ( flags & DO_RELATIVE )
	  {
	    x2 += pgls->g.pos.x;
	    y2 += pgls->g.pos.y;
	  }
	      
	hpgl_args_setup(pargs);
	/* enter polygon mode. */
	hpgl_call(hpgl_PM(pargs, pgls));

	/* do the rectangle */
	{
	  hpgl_real_t x1 = pgls->g.pos.x; 
	  hpgl_real_t y1 = pgls->g.pos.y;

	  hpgl_args_set_real(pargs, x1);
	  hpgl_args_add_real(pargs, y2);
	  hpgl_call(hpgl_PD(pargs, pgls));

	  hpgl_args_set_real(pargs, x2); 
	  hpgl_args_add_real(pargs, y2);
	  hpgl_call(hpgl_PD(pargs, pgls));

	  hpgl_args_set_real(pargs, x2); 
	  hpgl_args_add_real(pargs, y1);
	  hpgl_call(hpgl_PD(pargs, pgls));

	  hpgl_args_set_real(pargs, x1);
	  hpgl_args_add_real(pargs, y1);
	  hpgl_call(hpgl_PD(pargs, pgls));
	}

	/* exit polygon mode PM2 */
	hpgl_args_set_int(pargs,2);
	hpgl_call(hpgl_PM(pargs, pgls));
	return 0;
}

/* Fill or edge a wedge (EW, WG). */
private int
hpgl_wedge(hpgl_args_t *pargs, hpgl_state_t *pgls)
{	hpgl_real_t radius, start, sweep, chord = 5;

	if ( !hpgl_arg_units(pargs, &radius) ||
	     !hpgl_arg_c_real(pargs, &start) ||
	     !hpgl_arg_c_real(pargs, &sweep) || sweep < -360 || sweep > 360 ||
	     (hpgl_arg_c_real(pargs, &chord) && (chord < 0.5 || chord > 180))
	   )
	  return e_Range;


	/* enter polygon mode */
	hpgl_args_setup(pargs);
	hpgl_call(hpgl_PM(pargs, pgls));

	/* draw the 2 lines and the arc using 3 point this does seem
           convoluted but it does guarantee that the endpoint lines
           for the vectors and the arc endpoints are coincident. */
	{
	  hpgl_real_t x1, y1, x2, y2, x3, y3;
	  hpgl_real_t startx = pgls->g.pos.x;
	  hpgl_real_t starty = pgls->g.pos.y;

	  hpgl_compute_vector_endpoints(radius, pgls->g.pos.x, pgls->g.pos.y,
				        start, &x1, &y1);

	  hpgl_compute_vector_endpoints(radius, pgls->g.pos.x, pgls->g.pos.y,
				        (start+sweep)/2.0, &x2, &y2);

	  hpgl_compute_vector_endpoints(radius, pgls->g.pos.x, pgls->g.pos.y,
				        (start+sweep), &x3, &y3);

	  hpgl_args_set_real(pargs, x1);
	  hpgl_args_add_real(pargs, y1);
	  hpgl_call(hpgl_PD(pargs, pgls));
	  
	  hpgl_args_set_real(pargs, x2);
	  hpgl_args_add_real(pargs, y2);
	  hpgl_args_add_real(pargs, x3);
	  hpgl_args_add_real(pargs, y3);
	  hpgl_args_add_real(pargs, chord);
	  hpgl_call(hpgl_AT(pargs, pgls));

	  hpgl_args_set_real(pargs, startx);
	  hpgl_args_add_real(pargs, starty);

	  hpgl_call(hpgl_PD(pargs, pgls));
	}
	  
	/* exit polygon mode */
	hpgl_args_set_int(pargs,2);
	hpgl_call(hpgl_PM(pargs, pgls));

	return 0;
}

/* ------ Commands ------ */

/* EA x,y; */
int
hpgl_EA(hpgl_args_t *pargs, hpgl_state_t *pgls)
{	
	
	hpgl_save_pen_state(pgls);
	hpgl_call(hpgl_rectangle(pargs, pgls, DO_EDGE));
	hpgl_call(hpgl_draw_current_path(pgls, hpgl_rm_vector));
	hpgl_restore_pen_state(pgls);
	return 0;
}

/* EP; */
int
hpgl_EP(hpgl_args_t *pargs, hpgl_state_t *pgls)
{	
	hpgl_save_pen_state(pgls);
	hpgl_call(hpgl_draw_current_path(pgls, hpgl_rm_vector));
	hpgl_restore_pen_state(pgls);
	return 0;
}

/* ER dx,dy; */
int
hpgl_ER(hpgl_args_t *pargs, hpgl_state_t *pgls)
{	
	hpgl_save_pen_state(pgls);
	hpgl_call(hpgl_rectangle(pargs, pgls, DO_RELATIVE));
	hpgl_call(hpgl_draw_current_path(pgls, hpgl_rm_vector));
	hpgl_restore_pen_state(pgls);
	return 0;
}

/* EW radius,astart,asweep[,achord]; */
int
hpgl_EW(hpgl_args_t *pargs, hpgl_state_t *pgls)
{	
	hpgl_save_pen_state(pgls);
	hpgl_call(hpgl_wedge(pargs, pgls));
	hpgl_call(hpgl_draw_current_path(pgls, hpgl_rm_vector));
	hpgl_restore_pen_state(pgls);
	return 0;
}

/* FP method; */
/* FP; */
int
hpgl_FP(hpgl_args_t *pargs, hpgl_state_t *pgls)
{	int method = 0;

	if ( hpgl_arg_c_int(pargs, &method) && (method & ~1) )
	  return e_Range;

	hpgl_call(hpgl_draw_current_path(pgls, hpgl_rm_polygon));

	return 0;
}

/* PM op; */
int
hpgl_PM(hpgl_args_t *pargs, hpgl_state_t *pgls)
{	int op;
  
	if ( hpgl_arg_c_int(pargs, &op) == 0 )
	  {
	    hpgl_args_setup(pargs);
	    hpgl_args_add_int(pargs, 0);
	    hpgl_PM(pargs, pgls);
	    return 0;
	  }

	if (op < 0 || op > 2) return e_Range;

	switch( op )
	  {
	  case 0 : 
	    /* clear the current path if there is one */
	    hpgl_draw_current_path(pgls, hpgl_rm_vector);
#ifdef NOPE
	    hpgl_call(hpgl_clear_current_path(pgls));
	    /* a side affect of polygon mode is to add the current
               state position to the polygon buffer.  We do a PU to
               guarantee the first point is a moveto.  HAS not sure if
               this is correct. */
#endif
	    hpgl_args_set_real(pargs, pgls->g.pos.x); 
	    hpgl_args_add_real(pargs, pgls->g.pos.y);
	    hpgl_call(hpgl_PU(pargs, pgls));

	    pgls->g.polygon_mode = true;
	    break;
	  case 1 :
	    hpgl_call(hpgl_close_current_path(pgls));
	    /* remain in poly mode, this shouldn't be necessary */
	    pgls->g.polygon_mode = true;
	    break;
	  case 2 :
	    hpgl_call(hpgl_close_current_path(pgls));
	    /* return to vector mode */
	    pgls->g.polygon_mode = false;
	  }    
	return 0;
}

/* RA x,y; */
int
hpgl_RA(hpgl_args_t *pargs, hpgl_state_t *pgls)
{	
	hpgl_save_pen_state(pgls);
	hpgl_call(hpgl_rectangle(pargs, pgls, 0));
	hpgl_restore_pen_state(pgls);
	return 0;
}

/* RR dx,dy; */
int
hpgl_RR(hpgl_args_t *pargs, hpgl_state_t *pgls)
{		
	hpgl_save_pen_state(pgls);
	hpgl_call(hpgl_rectangle(pargs, pgls, DO_RELATIVE));
	hpgl_call(hpgl_draw_current_path(pgls, hpgl_rm_polygon));
	hpgl_restore_pen_state(pgls);
	return 0;
}

/* WG radius,astart,asweep[,achord]; */
int
hpgl_WG(hpgl_args_t *pargs, hpgl_state_t *pgls)
{	
	hpgl_save_pen_state(pgls);
	hpgl_call(hpgl_wedge(pargs, pgls));
	hpgl_call(hpgl_draw_current_path(pgls, hpgl_rm_polygon));
	hpgl_restore_pen_state(pgls);
	return 0;
}

/* Initialization */
private int
pgpoly_do_init(gs_memory_t *mem)
{		/* Register commands */
	DEFINE_HPGL_COMMANDS
	  HPGL_COMMAND('E', 'A', hpgl_EA),
	  HPGL_COMMAND('E', 'P', hpgl_EP),
	  HPGL_COMMAND('E', 'R', hpgl_ER),
	  HPGL_COMMAND('E', 'W', hpgl_EW),
	  HPGL_COMMAND('F', 'P', hpgl_FP),
	  HPGL_POLY_COMMAND('P', 'M', hpgl_PM),
	  HPGL_COMMAND('R', 'A', hpgl_RA),
	  HPGL_COMMAND('R', 'R', hpgl_RR),
	  HPGL_COMMAND('W', 'G', hpgl_WG),
	END_HPGL_COMMANDS
	return 0;
}
const pcl_init_t pgpoly_init = {
  pgpoly_do_init, 0
};
