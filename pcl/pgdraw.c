/* Copyright (C) 1996 Aladdin Enterprises.  All rights reserved.
   Unauthorized use, copying, and/or distribution prohibited.
 */

/* pgdraw.c */
/* HP-GL/2 line drawing/path building routines. */

#include "stdio_.h"		
#include "math_.h"
#include "gdebug.h"            
#include "gstypes.h"		/* for gsstate.h */
#include "gsmatrix.h"		/* for gsstate.h */
#include "gsmemory.h"		/* for gsstate.h */
#include "gsstate.h"
#include "gscoord.h"
#include "gspath.h"
#include "gspaint.h"
#include "gxfixed.h"
#include "gxpath.h"
#include "pgmand.h"
#include "pgdraw.h"
#include "pggeom.h"
#include "pcdraw.h"             /* included for setting pcl's ctm */

/* HAS: update ghostscript's gs and the current ctm to reflect hpgl's.
   embodied in hpgl_set_graphics_dash_state(),
   hpgl_set_graphics_line_attributes(), and hpgl_set_graphics_state() below.
   Note: For now the gs gstate is updated each time graphics are
   rendered and the ctm is updated each time a new path is started.
   Potential performance issue.  The design choice is based solely on
   ease of implementation.  */
 
private int
hpgl_set_graphics_dash_state(hpgl_state_t *pgls)
{
	bool adaptive = ( pgls->g.line.type < 0 );
	int entry = abs(pgls->g.line.type);
	hpgl_line_type_t *pat = ((adaptive) ?  
				 (&pgls->g.adaptive_line_type[entry - 1]) :
				 (&pgls->g.fixed_line_type[entry - 1]));
	
	float length = ((pgls->g.line.pattern_length_relative) ?
			(pgls->g.line.pattern_length * 
			 hpgl_compute_distance(pgls->g.P1.x,
					       pgls->g.P1.y,
					       pgls->g.P2.x,
					       pgls->g.P2.y)) :
			(mm_2_plu(pgls->g.line.pattern_length)));
	
	float pattern[20];
	int i;

	gs_setdashadapt(pgls->pgs, adaptive);
	
	if ( entry == 0 )
	  {
	    /* dot length NOTE this is in absolute 1/72" units not
               user space */
	    hpgl_call(gs_setdotlength(pgls->pgs, (0.00098), 
				      true));
	    return 0;
	  }
	/* HAS not correct as user space may not be PU */
	
	for ( i = 0; i < pat->count; i++ )
	  {
	    pattern[i] = length * pat->gap[i];
	  }
	
	/* HAS does not handle residual / offset yet.  It is
	   not clear where the calculation should take place */
	hpgl_call(gs_setdash(pgls->pgs, pattern, pat->count, 0));
	
	return 0;
}

/* set up joins, caps, miter limit, and line width */
private int
hpgl_set_graphics_line_attribute_state(hpgl_state_t *pgls,
				       hpgl_rendering_mode_t render_mode)
{

	  /* HAS *** We use a miter join instead of miter/beveled as I
             am not sure if the gs library supports it */
	  const gs_line_cap cap_map[] = {gs_cap_butt,      /* 0 not supported */
					 gs_cap_butt,      /* 1 butt end */
					 gs_cap_square,    /* 2 square end */
					 gs_cap_triangle,  /* 3 triangular end */
					 gs_cap_round};    /* 4 round cap */

	  const gs_line_join join_map[] = {gs_join_none,    /* 0 not supported */
					   gs_join_miter,    /* 1 mitered */
					   gs_join_miter,    /* 2 mitered/beveled */
					   gs_join_triangle, /* 3 triangular join */
					   gs_join_round,    /* 4 round join */
					   gs_join_bevel,    /* 5 bevel join */
					   gs_join_none};    /* 6 no join */

	  switch( render_mode )
	    {
	    case hpgl_rm_character:
	    case hpgl_rm_polygon:
	      hpgl_call(gs_setlinejoin(pgls->pgs, gs_cap_round));
	      hpgl_call(gs_setlinecap(pgls->pgs, gs_join_round));
	      break;
	    case hpgl_rm_vector:
vector:	      hpgl_call(gs_setlinejoin(pgls->pgs, cap_map[pgls->g.line.cap]));
	      hpgl_call(gs_setlinecap(pgls->pgs, join_map[pgls->g.line.join]));
	      break;
	    default:
	      /* shouldn't happen as we must have a mode to properly
                 parse an hpgl file. */
	      dprintf("warning no hpgl rendering mode set using vector mode\n");
	      goto vector;
	    }

	  /* HAS -- yuck need symbolic names for GL join types.  Set
	     miter limit !very large! if there is not bevel.  Miter is
	     also sensitive to render mode but I have not figured it
	     out completely. */
	  hpgl_call(gs_setmiterlimit(pgls->pgs, 
				     ( pgls->g.line.join == 1 ) ?
				     5000.0 :
				     pgls->g.miter_limit));
	  
	  hpgl_call(gs_setlinewidth(pgls->pgs, 
				    pgls->g.pen_width[pgls->g.pen]));
	  
	  return 0;
}

/* set up an hpgl clipping region wrt last IW command */
private int
hpgl_set_clipping_region(hpgl_state_t *pgls)
{
	gs_fixed_rect fixed_box;
	gs_rect float_box;
	gs_matrix mat;

	hpgl_call(gs_currentmatrix(pgls->pgs, &mat));

	hpgl_call(gs_bbox_transform(&pgls->g.window, 
				    &mat,
				    &float_box));

	/* HAS maybe a routine that does this?? */
	fixed_box.p.x = float2fixed(float_box.p.x);
	fixed_box.p.y = float2fixed(float_box.p.y);
	fixed_box.q.x = float2fixed(float_box.q.x);
	fixed_box.q.y = float2fixed(float_box.q.y);
	
	hpgl_call(gx_clip_to_rectangle(pgls->pgs, &fixed_box));
	return 0;
}

private int
hpgl_set_graphics_state(hpgl_state_t *pgls, hpgl_rendering_mode_t render_mode)
{
	/* do dash stuff */
	if ( !pgls->g.line.is_solid ) 
	  hpgl_call(hpgl_set_graphics_dash_state(pgls));

	/* joins, caps, and line width */
	hpgl_call(hpgl_set_graphics_line_attribute_state(pgls, render_mode));
	
	hpgl_call(hpgl_set_clipping_region(pgls));

	return 0;
}
/* change pcl's transformation.  HAS hpgl's coordinate system rotates
   with pcl's orientation but not pcl's current print direction. */
private int
hpgl_set_ctm(hpgl_state_t *pgls)
{
  	/* set the default matrix */
	pcl_set_ctm(pgls, false);
	
	/* scale for x and y plu's with a flip for y */
	hpgl_call(gs_scale(pgls->pgs, (7200.0/1016.0), -(7200.0 / 1016.0)));

	/* move the origin HAS *wrong* */
	hpgl_call(gs_translate(pgls->pgs, 
			       0, 
			       -(centipoints_2_plu(pgls->logical_page_height))));

	/* now move the origin to P1 */
	hpgl_call(gs_translate(pgls->pgs, pgls->g.P1.x, pgls->g.P1.y));

	/* set up scaling wrt plot size and picture frame size.  HAS
           we still have the line width issue when scaling is
           assymetric !!  */
	hpgl_call(gs_scale(pgls->pgs, 
			   (pgls->g.picture_frame.height /
			    pgls->g.plot_height),
			   (pgls->g.picture_frame.width /
			    pgls->g.plot_width)));
		     
	return 0;
}
int
hpgl_add_point_to_path(hpgl_state_t *pgls, floatp x, floatp y, 
		       int (*gs_func)(gs_state *pgs, floatp x, floatp y))
{	
	if ( !(pgls->g.have_path) ) 
	  {
	    /* initialize the first point so we can implicitly close
               the path when we stroke and set up the ctm */
	    pgls->g.first_point.x = x;
	    pgls->g.first_point.y = y;
	    /* indicate that there is a current path */
	    pgls->g.have_path = true;
	    /* initialize the current transformation matrix -- see
               notes above on performance */
	    hpgl_call(hpgl_set_ctm(pgls));
	    hpgl_call(gs_newpath(pgls->pgs));
	    hpgl_call(gs_moveto(pgls->pgs, x, y));
	    hpgl_call(hpgl_set_ctm(pgls)); /* HAS remove this */
	  }
	hpgl_call((*gs_func)(pgls->pgs, x, y));
	/* update hpgl's state position */
	pgls->g.pos.x = x;
	pgls->g.pos.y = y;
	return 0;
}

/* destroys the current path.  HAS probably don't need to create a new
   one also. */
int 
hpgl_clear_current_path(hpgl_state_t *pgls)
{
	hpgl_call(gs_newpath(pgls->pgs));
	return 0;
}

/* closes the current path, making the first point and last point coincident */
int 
hpgl_close_current_path(hpgl_state_t *pgls)
{
	hpgl_call(gs_closepath(pgls->pgs));
	return 0;
}

int
hpgl_add_arc_to_path(hpgl_state_t *pgls, floatp center_x, floatp center_y, 
		     floatp radius, floatp start_angle, floatp sweep_angle,
		     floatp chord_angle)
{
	int num_chords=
	  hpgl_compute_number_of_chords(sweep_angle * (180.0 / M_PI),
					chord_angle);
	floatp start_angle_radians = start_angle * (M_PI/180.0);
	floatp chord_angle_radians = chord_angle * (M_PI/180.0);
	int i;
	floatp arccoord_x, arccoord_y;
	  
	hpgl_compute_arc_coords(radius, center_x, center_y, 
				start_angle_radians, 
				&arccoord_x, &arccoord_y);
	hpgl_call(hpgl_add_point_to_path(pgls, arccoord_x, arccoord_y, 
					 (pgls->g.pen_down) ? 
					 gs_lineto : gs_moveto));
	num_chords--;
	/* HAS - pen up/down is invariant in the loop */
	for ( i = 0; i < num_chords; i++ ) 
	  {
	    start_angle_radians += chord_angle_radians;
	    hpgl_compute_arc_coords(radius, center_x, center_y, 
				    start_angle_radians, 
				    &arccoord_x, &arccoord_y);
	    hpgl_call(hpgl_add_point_to_path(pgls, arccoord_x, arccoord_y, 
					     ((pgls->g.pen_down) ? 
					      gs_lineto : gs_moveto)));
	  }
	return 0;
}

int
hpgl_add_bezier_to_path(hpgl_state_t *pgls, floatp x1, floatp y1, 
			floatp x2, floatp y2, floatp x3, floatp y3, 
			floatp x4, floatp y4)
{
	hpgl_call(hpgl_add_point_to_path(pgls, x1, x2, gs_moveto));
	/* HAS we may need to flatten this here */
	hpgl_call(gs_curveto(pgls->pgs, x2, y2, x3, y3, x4, y4));
	return 0;
}


/* HAS -- There will need to be compression phase here note that
   extraneous PU's do not result in separate subpaths.  */

/* Stroke the current path */
int
hpgl_draw_current_path(hpgl_state_t *pgls, hpgl_rendering_mode_t render_mode)
{
	/* get the last point in the current subpath, if there is no
	   current point than we have nothing to do. */

	gs_point pt;

	if ( !pgls->g.have_path ) return 0;

	hpgl_call(gs_currentpoint(pgls->pgs, &pt)); 

	/* If the first point is coincident with the final point the
           path is closed implicitly.  HAS -- add epsilon */

	if ( (pt.x == pgls->g.first_point.x) && 
	     (pt.y == pgls->g.first_point.y) ) 
	  {
	    hpgl_call(gs_closepath(pgls->pgs));
	  }

	hpgl_call(hpgl_set_graphics_state(pgls, render_mode));

	/* HAS - yes they are exclusive but the hpgl_call macro is
           kooky about "else" right now */
	if ( render_mode == hpgl_rm_polygon ) 
	  hpgl_call(gs_fill(pgls->pgs));

	if ( render_mode == hpgl_rm_vector )
	  hpgl_call(gs_stroke(pgls->pgs));

	pgls->g.have_path = false;
	/* the page has been marked */
	pgls->have_page = true;
	return 0;
}

int
hpgl_draw_line(hpgl_state_t *pgls, floatp x1, floatp y1, floatp x2, floatp y2)
{
	hpgl_call(hpgl_add_point_to_path(pgls, x1, y1, 
					 ((pgls->g.pen_down) ? 
					  gs_lineto : gs_moveto)));
	hpgl_call(hpgl_add_point_to_path(pgls, x2, y2, 
					 ((pgls->g.pen_down) ? 
					  gs_lineto : gs_moveto)));
	hpgl_call(hpgl_draw_current_path(pgls, hpgl_rm_vector));
	
	return 0;
}

int
hpgl_draw_dot(hpgl_state_t *pgls, floatp x1, floatp y1)
{
	hpgl_call(hpgl_add_point_to_path(pgls, x1, y1, 
					 ((pgls->g.pen_down) ? 
					  gs_lineto : gs_moveto)));
	hpgl_call(hpgl_draw_current_path(pgls, hpgl_rm_vector));

	return 0;
}
