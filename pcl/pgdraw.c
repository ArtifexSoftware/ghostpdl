/* Copyright (C) 1996, 1997 Aladdin Enterprises.  All rights reserved.
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
#include "pgmisc.h"
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

	/* handle the simple case (no dash) and return */
	if ( pgls->g.line.is_solid ) 
	  {
	    /* use a 0 count pattern to turn off dashing in case it is
               set */
	    hpgl_call(gs_setdash(pgls->pgs, pattern, 0, 0));
	    return 0;
	  }

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
	      hpgl_call(gs_setlinejoin(pgls->pgs, 
				       gs_join_round)); 
	      hpgl_call(gs_setlinecap(pgls->pgs, 
				      gs_cap_round));
	      break;
	    case hpgl_rm_vector:
vector:	      hpgl_call(gs_setlinejoin(pgls->pgs, 
				       join_map[pgls->g.line.join])); 
	      hpgl_call(gs_setlinecap(pgls->pgs, 
				      cap_map[pgls->g.line.cap]));
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
hpgl_set_plu_to_device_ctm(hpgl_state_t *pgls)
{
	pcl_set_ctm(pgls, false);
	/* translate the coordinate system to the anchor point */
	hpgl_call(gs_translate(pgls->pgs, 
			       pgls->g.picture_frame.anchor_point.x,
			       pgls->g.picture_frame.anchor_point.y));

	{
	  /* picture frame dimensinsions in plu */
	  hpgl_real_t pic_w = (pgls->g.picture_frame.width);
	  hpgl_real_t pic_h = (pgls->g.picture_frame.height);

	  /* move the origin */
	  hpgl_call(gs_translate(pgls->pgs, 0, (pic_h)));

	  hpgl_call(gs_rotate(pgls->pgs, pgls->g.rotation));

	  /* account for rotated coordinate system */
	  switch (pgls->g.rotation)
	    {
	    case 0 :
	      hpgl_call(gs_translate(pgls->pgs, 0, 0));
	      break;
	    case 90 : 
	      hpgl_call(gs_translate(pgls->pgs, 0, -(pic_h)));
	      break;
	    case 180 :
	      hpgl_call(gs_translate(pgls->pgs, -(pic_w), -(pic_h)));
	      break;
	    case 270 :
	      hpgl_call(gs_translate(pgls->pgs, -(pic_w), 0));
	      break;
	      }
	}
	/* scale to plotter units and a flip for y */
	hpgl_call(gs_scale(pgls->pgs, (7200.0/1016.0), -(7200.0/1016.0)));

	/* set up scaling wrt plot size and picture frame size.  HAS
	   we still have the line width issue when scaling is
	   assymetric !!  */

	/* if any of these are zero something is wrong */
        if ( (pgls->g.picture_frame.height == 0.0) ||
	     (pgls->g.picture_frame.width == 0.0) ||
	     (pgls->g.plot_width == 0.0) ||
	     (pgls->g.plot_height == 0.0) )
	  return 1;
	
	hpgl_call(gs_scale(pgls->pgs, 
			   (pgls->g.picture_frame.height /
			    pgls->g.plot_height),
			   (pgls->g.picture_frame.width /
			    pgls->g.plot_width)));

	return 0;


}

private int
hpgl_set_label_to_plu_ctm(hpgl_state_t *pgls)
{
	/* if we are not in character mode do nothing -- identity
           transformation */
        /* HAS font is selection not complete */
	hpgl_real_t height_points = 
	  pgls->g.font_selection[0].params.height_4ths / 4.0;
	hpgl_real_t width_points = 
	  1.0 / (pgls->g.font_selection[0].params.pitch_100ths / 100.0);
	/* HAS -- Only LO1 is currently supported -- need to add others */
	hpgl_call(gs_translate(pgls->pgs, pgls->g.pos.x, pgls->g.pos.y));
	/* HAS -- only support standard font */
	hpgl_call(gs_scale(pgls->pgs, 
			   points_2_plu(height_points),
			   inches_2_plu(width_points)));
	return 0;
}


private int
hpgl_set_user_units_to_plu_ctm(hpgl_state_t *pgls)
{

	hpgl_call(gs_translate(pgls->pgs, pgls->g.P1.x, -pgls->g.P1.y));
			       
	/* finally scale to user units.  HAS this only handles the
           simple scaling scale for the moment. */
	if ( pgls->g.scaling_type == hpgl_scaling_anisotropic )
	  {
	    floatp scale_x = (pgls->g.P2.x - pgls->g.P1.x) / 
	      (pgls->g.scaling_params.pmax.x - pgls->g.scaling_params.pmin.x);
	    floatp scale_y = (pgls->g.P2.y - pgls->g.P1.y) / 
	      (pgls->g.scaling_params.pmax.y - pgls->g.scaling_params.pmin.y);
	    hpgl_call(gs_scale(pgls->pgs, scale_x, scale_y));
	  }
	else if ( pgls->g.scaling_type != hpgl_scaling_none )
	  dprintf1("unsuported scaling type %d:\n", pgls->g.scaling_type);

	return 0;
}

/* set up ctm's.  Uses the current render mode to figure out which ctm
   is appropriate */
private int
hpgl_set_ctm(hpgl_state_t *pgls)
{
	/* convert pcl->device to plu->device */
	hpgl_call(hpgl_set_plu_to_device_ctm(pgls));

	/* concatenate on user units ctm or character ctm based on
           current mode */
	if ( pgls->g.current_render_mode == hpgl_rm_character )
	  hpgl_call(hpgl_set_label_to_plu_ctm(pgls));
	else
	  hpgl_call(hpgl_set_user_units_to_plu_ctm(pgls));

	return 0;
}
	      
/* maps current hpgl fill type to pcl pattern type.  */
private pcl_pattern_type_t
hpgl_map_fill_type(hpgl_state_t *pgls)
{
	switch (pgls->g.fill.type)
	  {
	  case hpgl_fill_solid : return pcpt_solid_black;
	  case hpgl_fill_solid2 : return pcpt_solid_black;
	  case hpgl_fill_pcl_crosshatch : return pcpt_cross_hatch;
	  case hpgl_fill_shaded : return pcpt_shading;
	  case hpgl_fill_pcl_user_defined : 
	    dprintf("No key mapping support flling back to solid");
	    break;
	  default : 
	    dprintf1("Unsupported fill type %d falling back to solid\n",
		     pgls->g.fill.type);
	  }

	return pcpt_solid_black;
}

/* HAS I don't much care for the idea of overloading pcl_id with
   shading and hatching information, but that appears to be the way
   pcl_set_drawing_color() is set up. */
private pcl_id_t *
hpgl_map_id_type(hpgl_state_t *pgls, pcl_id_t *id)
{
	switch (pgls->g.fill.type)
	  {
	  case hpgl_fill_solid : 
	  case hpgl_fill_solid2 :
	    id = NULL;
	    break;
	  case hpgl_fill_pcl_crosshatch : 
	    id_set_value(*id, pgls->g.fill.param.pattern_type);
	    break;
	  case hpgl_fill_shaded : 
	    id_set_value(*id, (int)((pgls->g.fill.param.shading) * 100.0));
	    break;
	  case hpgl_fill_pcl_user_defined : 
	    id = NULL;
	    dprintf("No key mapping support yet");
	    break;
	  default : 
	    id = NULL;
	    dprintf1("Unsupported fill type %d falling back to pcl solid\n",
		     pgls->g.fill.type);
	  }
	return id;
} 
 
private int
hpgl_set_drawing_color(hpgl_state_t *pgls)
{
	pcl_id_t pcl_id;

	hpgl_call(pcl_set_drawing_color(pgls, 
					hpgl_map_fill_type(pgls), 
					hpgl_map_id_type(pgls, &pcl_id)));
	return 0;
}

private int
hpgl_set_graphics_state(hpgl_state_t *pgls, hpgl_rendering_mode_t render_mode)
{
        /* HACK to reset the ctm.  Note that in character mode we
           concatenate the character matrix to the ctm, but we do not
           want the tranformation to apply to the line width, miter,
           etc. */
	hpgl_call(hpgl_set_ctm(pgls));

	/* do dash stuff. */
	hpgl_call(hpgl_set_graphics_dash_state(pgls));

	/* joins, caps, and line width. */
	hpgl_call(hpgl_set_graphics_line_attribute_state(pgls, render_mode));
	
	/* set up the hpgl fills. */
	hpgl_call(hpgl_set_drawing_color(pgls));

	/* set up a clipping region */
	hpgl_call(hpgl_set_clipping_region(pgls));

	return 0;
}

int
hpgl_get_current_position(hpgl_state_t *pgls, gs_point *pt)
{

	*pt = pgls->g.pos;
	return 0;
}					   

int
hpgl_set_current_position(hpgl_state_t *pgls, gs_point *pt)
{
	if ( pgls->g.relative )
	  {
	    pgls->g.pos.x += pt->x;
	    pgls->g.pos.y += pt->y;
	  }
	else
	  {
	    pgls->g.pos.x = pt->x;
	    pgls->g.pos.y = pt->y;
	  }
	return 0;
}

int
hpgl_add_point_to_path(hpgl_state_t *pgls, floatp x, floatp y, 
		       int (*gs_func)(gs_state *pgs, floatp x, floatp y))
{	
	gs_point point;
	point.x = x;
	point.y = y;

	if ( !(pgls->g.have_first_moveto) ) 
	  {
	    /* initialize the first point so we can implicitly close
               the path when we stroke and set up the ctm */
	    pgls->g.first_point = point;
	    /* initialize the current transformation matrix */
	    hpgl_call(hpgl_set_ctm(pgls));

	    hpgl_call(gs_newpath(pgls->pgs));

	    hpgl_call_check_lost(gs_moveto(pgls->pgs, point.x, point.y));
	    /* indicate that there is a current path */
	    /* we do not want to indicate a first moveto if we went
               into lost mode */
	    if (!hpgl_lost) pgls->g.have_first_moveto = true;
	  }

	hpgl_call_check_lost((*gs_func)(pgls->pgs, point.x, point.y));

	/* update hpgl's state position */
	hpgl_call(hpgl_set_current_position(pgls, &point));
	return 0;
}

/* destroys the current path.  HAS probably don't need to create a new
   one also. */
int 
hpgl_clear_current_path(hpgl_state_t *pgls)
{
	/* if a current path exists set the current state position */
	hpgl_call(gs_newpath(pgls->pgs));
	pgls->g.have_first_moveto = false;
	return 0;
}

/* closes the current path, making the first point and last point coincident */
int 
hpgl_close_current_path(hpgl_state_t *pgls)
{
	hpgl_call(gs_closepath(pgls->pgs));
	return 0;
}

/* converts pcl coordinate to device space and back to hpgl space */
int
hpgl_add_pcl_point_to_path(hpgl_state_t *pgls, gs_point *pcl_pt)
{
	gs_point dev_pt, hpgl_pt;
	hpgl_call(hpgl_clear_current_path(pgls));
	pcl_set_ctm(pgls, false);
	hpgl_call(gs_transform(pgls->pgs, pcl_pt->x, pcl_pt->y, &dev_pt));
	hpgl_call(hpgl_set_ctm(pgls));
	hpgl_call(gs_itransform(pgls->pgs, dev_pt.x, dev_pt.y, &hpgl_pt));
	hpgl_call(hpgl_add_point_to_path(pgls, hpgl_pt.x, hpgl_pt.y, gs_moveto));
	return 0;
}
int
hpgl_add_arc_to_path(hpgl_state_t *pgls, floatp center_x, floatp center_y, 
		     floatp radius, floatp start_angle, floatp sweep_angle,
		     floatp chord_angle)
{
	int num_chords=
	  hpgl_compute_number_of_chords(sweep_angle, chord_angle);
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
	hpgl_call(hpgl_add_point_to_path(pgls, x1, y1, gs_moveto));
	/* HAS we may need to flatten this here */
	hpgl_call(gs_curveto(pgls->pgs, x2, y2, x3, y3, x4, y4));
	/* set the state position */
	{
	  gs_point last_point;
	  last_point.x = x4;
	  last_point.y = y4;
	  hpgl_call(hpgl_set_current_position(pgls, (gs_point *)&last_point));
	}
	return 0;
}

/* an implicit gl/2 style closepath.  If the first and last point are
   the same the path gets closed */
private int
hpgl_close_path(hpgl_state_t *pgls)
{
	gs_point last_point;
	hpgl_call(hpgl_get_current_position(pgls, &last_point));

	if ( (pgls->g.first_point.x == last_point.x) &&
	     (pgls->g.first_point.y == last_point.y) )
	    hpgl_call(gs_closepath(pgls->pgs));

	return 0;
}
	
	  

/* HAS -- There will need to be compression phase here note that
   extraneous PU's do not result in separate subpaths.  */

/* Stroke the current path */
int
hpgl_draw_current_path(hpgl_state_t *pgls, hpgl_rendering_mode_t render_mode)
{
	if ( !pgls->g.have_first_moveto ) return 0;

	hpgl_call(hpgl_close_path(pgls));

	hpgl_call(hpgl_set_graphics_state(pgls, render_mode));

	/* we reset the ctm before stroking to preserve the line width
           information */
	hpgl_call(hpgl_set_plu_to_device_ctm(pgls));
	
	/* we fill polygons and stroke in character and vector mode */
	if ( render_mode == hpgl_rm_polygon ) 
	  hpgl_call(gs_fill(pgls->pgs));
	else
	  hpgl_call(gs_stroke(pgls->pgs));

	pgls->g.have_first_moveto = false;
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
