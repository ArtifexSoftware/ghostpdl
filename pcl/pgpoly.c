/* Copyright (C) 1996, 1997 Aladdin Enterprises.  All rights reserved.
   Unauthorized use, copying, and/or distribution prohibited.
 */

/* pgpoly.c */
/* HP-GL/2 polygon commands */
#include "std.h"
#include "pgmand.h"
#include "pgdraw.h"
#include "pggeom.h"
#include "pgmisc.h"
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
	  gs_point p1, p2;

	  hpgl_call(gs_point_transform(x1, y1, &pgls->g.polygon.ctm, &p1));
	  hpgl_call(gs_point_transform(x2, y2, &pgls->g.polygon.ctm, &p2));

	  hpgl_call(hpgl_add_point_to_path(pgls, p1.x, p1.y, hpgl_plot_move_absolute));
	  hpgl_call(hpgl_add_point_to_path(pgls, p2.x, p1.y, hpgl_plot_draw_absolute));
	  hpgl_call(hpgl_add_point_to_path(pgls, p2.x, p2.y, hpgl_plot_draw_absolute));
	  hpgl_call(hpgl_add_point_to_path(pgls, p1.x, p2.y, hpgl_plot_draw_absolute));
	  /* polygons are implicitly closed */
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
	  gs_point p1, p2, p3;

	  hpgl_compute_vector_endpoints(radius, pgls->g.pos.x, pgls->g.pos.y,
				        start, &x1, &y1);

	  hpgl_compute_vector_endpoints(radius, pgls->g.pos.x, pgls->g.pos.y,
				        (start + (sweep / 2.0)), &x2, &y2);

	  hpgl_compute_vector_endpoints(radius, pgls->g.pos.x, pgls->g.pos.y,
				        (start + sweep), &x3, &y3);

	  hpgl_call(gs_point_transform(x1, y1, &pgls->g.polygon.ctm, &p1));
	  hpgl_call(gs_point_transform(x2, y2, &pgls->g.polygon.ctm, &p2));
	  hpgl_call(gs_point_transform(x3, y3, &pgls->g.polygon.ctm, &p3));

	  hpgl_call(hpgl_add_point_to_path(pgls, pgls->g.pos.x, pgls->g.pos.y,
					   hpgl_plot_move_absolute));
	  hpgl_call(hpgl_add_point_to_path(pgls, p1.x, p1.y,
					   hpgl_plot_draw_absolute));
	  hpgl_call(hpgl_add_arc_3point_to_path(pgls, 
						p1.x, p1.y, 
						p2.x, p2.y,
						p3.x, p3.y, chord,
						hpgl_plot_draw_absolute));
	}
	  
	/* exit polygon mode, this should close the path and the wedge
           is complete */
	hpgl_args_set_int(pargs,2);
	hpgl_call(hpgl_PM(pargs, pgls));

	return 0;
}

/* ------ Commands ------ */

/* EA x,y; */
int
hpgl_EA(hpgl_args_t *pargs, hpgl_state_t *pgls)
{	
	
	hpgl_call(hpgl_rectangle(pargs, pgls, DO_EDGE));
	hpgl_call(hpgl_copy_polygon_buffer_to_current_path(pgls));
	hpgl_call(hpgl_draw_current_path(pgls, hpgl_rm_vector));
	return 0;
}

/* EP; */
int
hpgl_EP(hpgl_args_t *pargs, hpgl_state_t *pgls)
{	
	/* preserve the current path and copy the polygon buffer to
           the current path */
	hpgl_call(hpgl_gsave(pgls));
	hpgl_call(hpgl_copy_polygon_buffer_to_current_path(pgls));
	hpgl_call(hpgl_draw_current_path(pgls, hpgl_rm_vector));
	hpgl_call(hpgl_grestore(pgls));
	return 0;
}

/* ER dx,dy; */
int
hpgl_ER(hpgl_args_t *pargs, hpgl_state_t *pgls)
{	
	hpgl_call(hpgl_rectangle(pargs, pgls, DO_RELATIVE));
	hpgl_call(hpgl_copy_polygon_buffer_to_current_path(pgls));
	hpgl_call(hpgl_draw_current_path(pgls, hpgl_rm_vector));
	return 0;
}

/* EW radius,astart,asweep[,achord]; */
int
hpgl_EW(hpgl_args_t *pargs, hpgl_state_t *pgls)
{	
	hpgl_call(hpgl_wedge(pargs, pgls));
	hpgl_call(hpgl_copy_polygon_buffer_to_current_path(pgls));
	hpgl_call(hpgl_draw_current_path(pgls, hpgl_rm_vector));
	return 0;
}

 private hpgl_rendering_mode_t
hpgl_get_poly_render_mode(hpgl_state_t *pgls)
{
	return (((pgls->g.fill.type == hpgl_fill_hatch) || 
		(pgls->g.fill.type == hpgl_fill_crosshatch)) ?
		hpgl_rm_clip_and_fill_polygon :
		hpgl_rm_polygon);
}

/* FP method; */
/* FP; */
int
hpgl_FP(hpgl_args_t *pargs, hpgl_state_t *pgls)
{	int method = 0;

	if ( hpgl_arg_c_int(pargs, &method) && (method & ~1) )
	  return e_Range;

	pgls->g.fill_type = (method == 0) ? 
	  hpgl_even_odd_rule : hpgl_winding_number_rule;
	/* preserve the current path and copy the polygon buffer to
           the current path */
	hpgl_call(hpgl_gsave(pgls));
	hpgl_call(hpgl_copy_polygon_buffer_to_current_path(pgls));
	hpgl_call(hpgl_draw_current_path(pgls, 
					 hpgl_get_poly_render_mode(pgls)));
	hpgl_call(hpgl_grestore(pgls));
	return 0;
}

/* polygon points are rotated separately (ARGH) */
 int private
hpgl_set_polygon_ctm(hpgl_state_t *pgls)
{
	gs_matrix mat;
	gs_make_identity(&mat); 
	pgls->g.polygon.ctm = mat;
	return 0;
}

/* PM op; */
int
hpgl_PM(hpgl_args_t *pargs, hpgl_state_t *pgls)
{	int op;

	if ( hpgl_arg_c_int(pargs, &op) == 0 )    
	  op = 0;

	switch( op )
	  {
	  case 0 : 
	    /* clear the current path if there is one */
	    hpgl_call(hpgl_draw_current_path(pgls, hpgl_rm_vector));
	    pgls->g.polygon_mode = true;
	    /* save the pen state, to be restored by PM2 */
	    hpgl_save_pen_state(pgls, 
				&pgls->g.polygon.pen_state,
				hpgl_pen_all);
	    hpgl_call(hpgl_set_polygon_ctm(pgls));
	    break;
	  case 1 :
	    hpgl_call(hpgl_close_current_path(pgls));
	    /* remain in poly mode, this shouldn't be necessary */
	    pgls->g.polygon_mode = true;
	    break;
	  case 2 :
	    hpgl_call(hpgl_close_current_path(pgls));
	    /* make a copy of the path and clear the current path */
	    hpgl_call(hpgl_copy_current_path_to_polygon_buffer(pgls));
	    hpgl_call(hpgl_clear_current_path(pgls));
	    /* return to vector mode */
	    pgls->g.polygon_mode = false;
	    /* restore the pen state */
	    hpgl_restore_pen_state(pgls,
				   &pgls->g.polygon.pen_state,
				   hpgl_pen_all);
	    break;
	  default:
	    return e_Range;
	  }    
	return 0;
}

/* RA x,y; */
int
hpgl_RA(hpgl_args_t *pargs, hpgl_state_t *pgls)
{	
	hpgl_call(hpgl_rectangle(pargs, pgls, 0));
	hpgl_call(hpgl_copy_polygon_buffer_to_current_path(pgls));
	hpgl_call(hpgl_draw_current_path(pgls, 
					 hpgl_get_poly_render_mode(pgls)));
	return 0;
}

/* RR dx,dy; */
int
hpgl_RR(hpgl_args_t *pargs, hpgl_state_t *pgls)
{		
	hpgl_call(hpgl_rectangle(pargs, pgls, DO_RELATIVE));
	hpgl_call(hpgl_copy_polygon_buffer_to_current_path(pgls));
	hpgl_call(hpgl_draw_current_path(pgls,
					 hpgl_get_poly_render_mode(pgls)));
	return 0;
}

/* WG radius,astart,asweep[,achord]; */
int
hpgl_WG(hpgl_args_t *pargs, hpgl_state_t *pgls)
{	
	hpgl_call(hpgl_wedge(pargs, pgls));
	hpgl_call(hpgl_copy_polygon_buffer_to_current_path(pgls));
	hpgl_call(hpgl_draw_current_path(pgls,
					 hpgl_get_poly_render_mode(pgls)));
	return 0;
}

/* Initialization */
private int
pgpoly_do_init(gs_memory_t *mem)
{		/* Register commands */
	DEFINE_HPGL_COMMANDS
	  HPGL_COMMAND('E', 'A', hpgl_EA, hpgl_cdf_lost_mode_cleared),
	  HPGL_COMMAND('E', 'P', hpgl_EP, 0),
	  HPGL_COMMAND('E', 'R', hpgl_ER, hpgl_cdf_lost_mode_cleared),
	  HPGL_COMMAND('E', 'W', hpgl_EW, hpgl_cdf_lost_mode_cleared),
	  HPGL_COMMAND('F', 'P', hpgl_FP, 0),
	  HPGL_COMMAND('P', 'M', hpgl_PM, hpgl_cdf_polygon|hpgl_cdf_lost_mode_cleared),
	  HPGL_COMMAND('R', 'A', hpgl_RA, hpgl_cdf_lost_mode_cleared),
	  HPGL_COMMAND('R', 'R', hpgl_RR, hpgl_cdf_lost_mode_cleared),
	  HPGL_COMMAND('W', 'G', hpgl_WG, hpgl_cdf_lost_mode_cleared),
	END_HPGL_COMMANDS
	return 0;
}
const pcl_init_t pgpoly_init = {
  pgpoly_do_init, 0
};
