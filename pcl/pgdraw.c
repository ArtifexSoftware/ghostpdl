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
#include "gxfarith.h"		/* for sincos */
#include "gxfixed.h"
#include "pgmand.h"
#include "pgdraw.h"
#include "pggeom.h"
#include "pgmisc.h"
#include "pcdraw.h"             /* included for setting pcl's ctm */

/* HAS: update ghostscript's gs and the current ctm to reflect hpgl's.
   embodied in hpgl_set_graphics_dash_state(),
   hpgl_set_graphics_line_attributes(), and hpgl_set_drawing_state() below.
   Note: For now the gs gstate is updated each time graphics are
   rendered and the ctm is updated each time a new path is started.
   Potential performance issue.  The design choice is based solely on
   ease of implementation.  */
/* ctm to translate from pcl space to plu space */
 int
hpgl_set_pcl_to_plu_ctm(hpgl_state_t *pgls)
{
	pcl_set_ctm(pgls, false);
	hpgl_call(gs_translate(pgls->pgs, 
			       pgls->g.picture_frame.anchor_point.x,
			       pgls->g.picture_frame.anchor_point.y));
	/* move the origin */
	hpgl_call(gs_translate(pgls->pgs, 0, pgls->g.picture_frame_height));
	/* scale to plotter units and a flip for y */
	hpgl_call(gs_scale(pgls->pgs, (7200.0/1016.0), -(7200.0/1016.0)));
	/* account for rotated coordinate system */
	hpgl_call(gs_rotate(pgls->pgs, pgls->g.rotation));
	{
	  hpgl_real_t fw_plu = (coord_2_plu(pgls->g.picture_frame_width));
	  hpgl_real_t fh_plu = (coord_2_plu(pgls->g.picture_frame_height));
	  switch (pgls->g.rotation)
	    {
	    case 0 :
	      hpgl_call(gs_translate(pgls->pgs, 0, 0));
	      break;
	    case 90 : 
	      hpgl_call(gs_translate(pgls->pgs, 0, -fw_plu));
	      break;
	    case 180 :
	      hpgl_call(gs_translate(pgls->pgs, -fw_plu, -fh_plu));
	      break;
	    case 270 :
	      hpgl_call(gs_translate(pgls->pgs, -fh_plu, 0));
	      break;
	    }
	}
	/* set up scaling wrt plot size and picture frame size.  HAS
	   we still have the line width issue when scaling is
	   assymetric !!  */
	/* if any of these are zero something is wrong */
        if ( (pgls->g.picture_frame_height == 0) ||
	     (pgls->g.picture_frame_width == 0) ||
	     (pgls->g.plot_width == 0) ||
	     (pgls->g.plot_height == 0) )
	  return 1;
	hpgl_call(gs_scale(pgls->pgs, 
			   ((hpgl_real_t)pgls->g.picture_frame_height /
			    (hpgl_real_t)pgls->g.plot_height),
			   ((hpgl_real_t)pgls->g.picture_frame_width /
			    (hpgl_real_t)pgls->g.plot_width)));
	return 0;
}

/* Set the CTM to map PLU to device units, regardless of scaling. */
/* (We need this for labels when scaling is on.) */
 int
hpgl_set_plu_ctm(hpgl_state_t *pgls)
{
	hpgl_call(hpgl_set_pcl_to_plu_ctm(pgls));
	return 0;
}

/* The CTM maps PLU to device units; adjust it to handle scaling, if any. */
 int
hpgl_compute_user_units_to_plu_ctm(const hpgl_state_t *pgls, gs_matrix *pmat)
{	floatp origin_x = pgls->g.P1.x, origin_y = pgls->g.P1.y;

	switch ( pgls->g.scaling_type )
	  {
	  case hpgl_scaling_none:
	    gs_make_identity(pmat);
	    break;
	  case hpgl_scaling_point_factor:
	    gs_make_translation(origin_x, origin_y, pmat);
	    gs_matrix_scale(pmat, pgls->g.scaling_params.factor.x,
			    pgls->g.scaling_params.factor.y, pmat);
	    gs_matrix_translate(pmat, -pgls->g.scaling_params.pmin.x,
				-pgls->g.scaling_params.pmin.y, pmat);
	    break;
	  default:
	  /*case hpgl_scaling_anisotropic:*/
	  /*case hpgl_scaling_isotropic:*/
	    {
	      floatp window_x = pgls->g.P2.x - origin_x,
		range_x = pgls->g.scaling_params.pmax.x -
		  pgls->g.scaling_params.pmin.x,
		scale_x = window_x / range_x;
	      floatp window_y = pgls->g.P2.y - origin_y,
		range_y = pgls->g.scaling_params.pmax.y -
		  pgls->g.scaling_params.pmin.y,
		scale_y = window_y / range_y;

	      if ( pgls->g.scaling_type == hpgl_scaling_isotropic )
		{ if ( scale_x > scale_y )
		    { /* Reduce the X scaling. */
		      origin_x += range_x * (scale_x - scale_y) *
			pgls->g.scaling_params.left;
		      scale_x = scale_y;
		    }
		  else if ( scale_y > scale_x )
		    { /* Reduce the Y scaling. */
		      origin_y += range_y * (scale_y - scale_x) *
			pgls->g.scaling_params.bottom;
		      scale_y = scale_x;
		    }
		}
	      gs_make_translation(origin_x, origin_y, pmat);
	      gs_matrix_scale(pmat, scale_x, scale_y, pmat);
	      gs_matrix_translate(pmat, -pgls->g.scaling_params.pmin.x,
				  -pgls->g.scaling_params.pmin.y, pmat);
	      break;
	    }
	  }
	return 0;

}
 private int
hpgl_set_user_units_to_plu_ctm(hpgl_state_t *pgls)
{
	if ( pgls->g.scaling_type != hpgl_scaling_none )
	  {
	    gs_matrix mat;

	    hpgl_call(hpgl_compute_user_units_to_plu_ctm(pgls, &mat));
	    hpgl_call(gs_concat(pgls->pgs, &mat));
	  }
	return 0;
}

/* set up ctm's.  Uses the current render mode to figure out which ctm
   is appropriate */
 int
hpgl_set_ctm(hpgl_state_t *pgls)
{
	hpgl_call(hpgl_set_plu_ctm(pgls));
	hpgl_call(hpgl_set_user_units_to_plu_ctm(pgls));
	return 0;
}

 private int
hpgl_set_graphics_dash_state(hpgl_state_t *pgls)
{
	bool adaptive = ( pgls->g.line.current.type < 0 );
	int entry = abs(pgls->g.line.current.type);
	const hpgl_line_type_t *pat;
	float length;
	float pattern[20];
	float offset;
	int count;
	int i;

	/* handle the simple case (no dash) and return */
	if ( pgls->g.line.current.is_solid ) 
	  {
	    /* use a 0 count pattern to turn off dashing in case it is
               set */
	    hpgl_call(gs_setdash(pgls->pgs, pattern, 0, 0));
	    return 0;
	  }
	
	/* Make sure we always draw dots. */
	hpgl_call(gs_setdotlength(pgls->pgs, 0.00098, true));
	if ( entry == 0 )
	  {
	    /* dot length NOTE this is in absolute 1/72" units not
               user space */
	    /* Use an adaptive pattern with an infinitely long segment length
	       to get the dots drawn just at the ends of lines. */
	    pattern[0] = 0;
	    pattern[1] = 1.0e6;	/* "infinity" */
	    hpgl_call(gs_setdash(pgls->pgs, pattern, 2, 0));
	    gs_setdashadapt(pgls->pgs, true);
	    return 0;
	  }

	pat = ((adaptive) ?  
	       (&pgls->g.adaptive_line_type[entry - 1]) :
	       (&pgls->g.fixed_line_type[entry - 1]));
	
	length = ((pgls->g.line.current.pattern_length_relative) ?
		  (pgls->g.line.current.pattern_length * 
		   hpgl_compute_distance(pgls->g.P1.x,
					 pgls->g.P1.y,
					 pgls->g.P2.x,
					 pgls->g.P2.y)) :
		  (mm_2_plu(pgls->g.line.current.pattern_length)));

	gs_setdashadapt(pgls->pgs, adaptive);
	/* HAS not correct as user space may not be PU */
	
	/*
	 * The graphics library interprets odd pattern counts differently
	 * from GL: if the pattern count is odd, we need to do something
	 * a little special.
	 */
	/* HAS does not handle residual / offset yet.  It is
	   not clear where the calculation should take place. */
	count = pat->count;
	for ( i = 0; i < count; i++ )
	  pattern[i] = length * pat->gap[i];
	offset = 0;
	if ( count & 1 )
	  {
	    /*
	     * Combine the last gap with the first one, and change the
	     * offset to compensate.  NOTE: this doesn't work right with
	     * adaptive line type: we may need to change the library to
	     * make this work properly.
	     */
	    --count;
	    pattern[0] += pattern[count];
	    offset += pattern[count];
	  }
	
	hpgl_call(gs_setdash(pgls->pgs, pattern, count, offset));
	
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

	  /* HP appears to use default line attributes if the the pen
             width is less than or equal to .35mm or 14.0 plu.  This
             is not documented PCLTRM.  Pen widths are maintained in
             plotter units */
	  if ( pgls->g.pen.width[pgls->g.pen.selected] <= 14.0 )
	    {
	      hpgl_call(gs_setlinejoin(pgls->pgs, 
				       gs_join_miter)); 
	      hpgl_call(gs_setlinecap(pgls->pgs, 
				      gs_cap_butt));
	      hpgl_call(gs_setlinewidth(pgls->pgs, 
					pgls->g.pen.width[pgls->g.pen.selected]));
	      hpgl_call(gs_setmiterlimit(pgls->pgs, 5.0));
	      return 0;
	    }

	  switch( render_mode )
	    {
	    case hpgl_rm_character:
	    case hpgl_rm_polygon:
	    case hpgl_rm_vector_fill:
	    case hpgl_rm_clip_and_fill_polygon:
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
#ifdef COMMENT
	  /* I do not remember the rational for the large miter */
	  hpgl_call(gs_setmiterlimit(pgls->pgs, 
				     ( pgls->g.line.join == 1 ) ?
				     5000.0 :
				     pgls->g.miter_limit));
#endif
	  hpgl_call(gs_setmiterlimit(pgls->pgs, pgls->g.miter_limit));

	  hpgl_call(gs_setlinewidth(pgls->pgs, 
				    pgls->g.pen.width[pgls->g.pen.selected]));
	  
	  return 0;
}

/* A bounding box for the current polygon -- used for HPGL/2 vector
   fills.  We expand the bounding box by the current line width to
   avoid overhanging lines. */

 private int
hpgl_polyfill_anchored_bbox(hpgl_state_t *pgls, gs_rect *bbox, 
			    hpgl_real_t spacing, hpgl_real_t direction)
{
  	/* get the bounding box we only need this for the upper right
           corner.  The lower left is supplied by AC */
	hpgl_call(gs_pathbbox(pgls->pgs, bbox));
	/* expand the box */

	{
#define sin_dir sincos.sin
#define cos_dir sincos.cos
	  gs_matrix mat, imat;
	  gs_sincos_t sincos;
	  gs_point fill_increment;
	  gs_point anchor = pgls->g.anchor_corner;
	  /* calculate integer number of fill lines between the origin
             of the bbox and the anchor corner in X and Y directions.
             Then use the coordinates of the n'th fill line as the
             anchor corner.  Also make sure anchor corner is in front of bbox */
#if 0
	  int fill_line_num;
#endif
	  hpgl_call(hpgl_compute_user_units_to_plu_ctm(pgls, &mat));
	  hpgl_call(gs_matrix_invert(&mat, &imat));
	  hpgl_call(gs_point_transform(pgls->g.anchor_corner.x, 
				       pgls->g.anchor_corner.y,
				       &imat,
				       &anchor));
	  gs_sincos_degrees(direction, &sincos);
	  /* HAS anchor in front of polygon origin is not documented */
	  bbox->p.x = min(anchor.x, bbox->p.x);
	  bbox->p.y = min(anchor.y, bbox->p.y);

#if 0
	  if ( sin_dir != 0 )
	    {
	      fill_increment.x = fabs(spacing / sin_dir);
	      /* move the anchor back if necessary.  note that we
                 treat x and y exclusively */
	      while ( bbox->p.x < anchor.x ) anchor.x -= fill_increment.x;
	      /* integer number of fill lines */

	      fill_line_num =
		(int)fabs(bbox->p.x - anchor.x) / fill_increment.x;
	      /* new origin at last fill line below origin */
	      bbox->p.x = fill_line_num * fill_increment.x;
	    }
	  /* same for y */
	  if ( cos_dir != 0 )
	    {
	      fill_increment.y = fabs(spacing / cos_dir);
	      while ( bbox->p.y < anchor.y ) anchor.y -= fill_increment.y;
	      fill_line_num =
		(int)fabs(bbox->p.y - anchor.y) / fill_increment.y;
	      bbox->p.y = fill_line_num * fill_increment.y;
	    }
	  bbox->p = anchor;

	{
	  hpgl_real_t half_width = pgls->g.pen.width[pgls->g.pen.selected] / 2.0;
	  bbox->p.x -= half_width;
	  bbox->p.y -= half_width;
	  bbox->q.x += half_width;
	  bbox->q.y += half_width;
	}
#endif
	}
	return 0;
}

/* set up an hpgl clipping region -- intersection of IW command and
   picture frame. */
 private int
hpgl_set_clipping_region(hpgl_state_t *pgls, hpgl_rendering_mode_t render_mode)
{
	/* if we are doing vector fill a clipping path has already
           been set up using the last polygon */
	if ( render_mode == hpgl_rm_vector_fill )
	  return 0;
	else
	  {
	    gs_fixed_rect fixed_box;
	    gs_rect pcl_clip_box;
	    gs_rect dev_clip_box; 
	    gs_matrix save_ctm;
	    gs_matrix pcl_ctm;

	    /* get pcl to device ctm and restore the current ctm */
	    hpgl_call(gs_currentmatrix(pgls->pgs, &save_ctm));
	    hpgl_call(pcl_set_ctm(pgls, false));
	    hpgl_call(gs_currentmatrix(pgls->pgs, &pcl_ctm));
	    hpgl_call(gs_setmatrix(pgls->pgs, &save_ctm));
	    
	    /* find the clipping region defined by the picture frame
               which is defined in pcl coordinates */
	    pcl_clip_box.p.x = pgls->g.picture_frame.anchor_point.x;
	    pcl_clip_box.p.y = pgls->g.picture_frame.anchor_point.y;
	    pcl_clip_box.q.x = pcl_clip_box.p.x + pgls->g.picture_frame_width;
	    pcl_clip_box.q.y = pcl_clip_box.p.y + pgls->g.picture_frame_height;
	    hpgl_call(gs_bbox_transform(&pcl_clip_box,
					&pcl_ctm,
					&dev_clip_box));

	    /* if the clipping window is active calculate the new clip
               box derived from IW and the intersection of the device
               space boxes replace the current box.  Note that IW
               coordinates are in current units and and the picture
               frame in pcl coordinates. */
	    if ( pgls->g.soft_clip_window.state == active )
	      {
		gs_rect dev_soft_window_box;
		gs_matrix ctm;
		hpgl_call(gs_currentmatrix(pgls->pgs, &ctm));
		hpgl_call(gs_bbox_transform(&pgls->g.soft_clip_window.rect, 
					    &ctm,
					    &dev_soft_window_box));
	        dev_clip_box.p.x = max(dev_clip_box.p.x, dev_soft_window_box.p.x);  
		dev_clip_box.p.y = max(dev_clip_box.p.y, dev_soft_window_box.p.y); 
	    	dev_clip_box.q.x = min(dev_clip_box.q.x, dev_soft_window_box.q.x); 
		dev_clip_box.q.y = min(dev_clip_box.q.y, dev_soft_window_box.q.y); 
	      }

	    /* convert intersection box to fixed point and clip */
	    fixed_box.p.x = float2fixed(dev_clip_box.p.x);
	    fixed_box.p.y = float2fixed(dev_clip_box.p.y);
	    fixed_box.q.x = float2fixed(dev_clip_box.q.x);
	    fixed_box.q.y = float2fixed(dev_clip_box.q.y);
	    hpgl_call(gx_clip_to_rectangle(pgls->pgs, &fixed_box));
	  } 
	return 0;
}

/* Plot one vector for vector fill all these use absolute coordinates. */
private int
hpgl_draw_vector_absolute(hpgl_state_t *pgls, hpgl_real_t x0, hpgl_real_t y0,
			   hpgl_real_t x1, hpgl_real_t y1)
{
	hpgl_call(hpgl_add_point_to_path(pgls, x0, y0, hpgl_plot_move_absolute, false));
	hpgl_call(hpgl_add_point_to_path(pgls, x1, y1, hpgl_plot_draw_absolute, false));
	return 0;      
}

/* HAS should replicate lines beginning at the anchor corner to +X and
   +Y.  Not quite right - anchor corner not yet supported.
   pgls->g.anchor_corner needs to be used to set dash offsets */
 private int
hpgl_polyfill(hpgl_state_t *pgls)
{
	hpgl_real_t diag_mag, startx, starty, endx, endy;
	gs_sincos_t sincos;
#define sin_dir sincos.sin
#define cos_dir sincos.cos
	gs_rect bbox;
	hpgl_pen_state_t saved_pen_state;
	bool cross = (pgls->g.fill.type == hpgl_fill_crosshatch);
	const hpgl_hatch_params_t *params =
	  (cross ? &pgls->g.fill.param.crosshatch : &pgls->g.fill.param.hatch);
	hpgl_real_t spacing = params->spacing;
	hpgl_real_t direction = params->angle;
	/* save the pen position */
	hpgl_save_pen_state(pgls, &saved_pen_state, hpgl_pen_pos);
	if ( spacing == 0 )
	  { /* Per TRM 22-12, use 1% of the P1/P2 distance. */
	    gs_matrix mat;

	    hpgl_compute_user_units_to_plu_ctm(pgls, &mat);
	    spacing = 
	      0.01 * hpgl_compute_distance(pgls->g.P1.x, pgls->g.P1.y,
					   pgls->g.P2.x, pgls->g.P2.y);
	    /****** WHAT IF ANISOTROPIC SCALING? ******/
	    spacing /= min(fabs(mat.xx), fabs(mat.yy));
	  }
	/* get the bounding box */
        hpgl_call(hpgl_polyfill_anchored_bbox(pgls, &bbox, spacing, direction));
	/* HAS calculate the offset for dashing - we do not need this
           for solid lines */
	/* HAS calculate the diagonals magnitude.  Note we clip this
           latter in the library.  If desired we could clip to the
           actual bbox here to save processing time.  For now we simply
           draw all fill lines using the diagonals magnitude */
	diag_mag = 
	  hpgl_compute_distance(bbox.p.x, bbox.p.y, bbox.q.x, bbox.q.y);

	/* if the line width exceeds the spacing we use the line width
           to avoid overlapping of the fill lines.  HAS this can be
           integrated with the logic above for spacing as not to
           duplicate alot of code. */
	{
	  gs_matrix mat;
	  hpgl_real_t line_width = pgls->g.pen.width[pgls->g.pen.selected];
	  hpgl_call(hpgl_compute_user_units_to_plu_ctm(pgls, &mat));
	  line_width /= min(fabs(mat.xx), fabs(mat.yy));
	  if ( line_width >= spacing )
	    {
	      hpgl_call(gs_setgray(pgls->pgs, 0.0));
	      hpgl_call(gs_fill(pgls->pgs));
	      return 0;
	    }
	}
	/* get rid of the current path */
	hpgl_call(hpgl_clear_current_path(pgls));
start:	gs_sincos_degrees(direction, &sincos);
	if ( sin_dir < 0 )
	  sin_dir = -sin_dir, cos_dir = -cos_dir; /* ensure y_inc >= 0 */
	/* Draw one vector regardless of direction. */
	startx = bbox.p.x; starty = bbox.p.y;
	endx = (diag_mag * cos_dir) + startx;
	endy = (diag_mag * sin_dir) + starty;
	hpgl_call(hpgl_draw_vector_absolute(pgls, startx, starty, endx, endy));
	/* Travel along +x using current spacing. */
	if ( sin_dir != 0 )
	  { hpgl_real_t x_fill_increment = fabs(spacing / sin_dir);

	    while ( endx += x_fill_increment,
		    (startx += x_fill_increment) <= bbox.q.x

		  )
	      hpgl_call(hpgl_draw_vector_absolute(pgls, startx, starty, endx, endy));
	    hpgl_call(hpgl_draw_current_path(pgls, hpgl_rm_vector_fill));
	  }
	/* Travel along +Y similarly. */
	if ( cos_dir != 0 )
	  { hpgl_real_t y_fill_increment = fabs(spacing / cos_dir);

	    /*
	     * If the slope is negative, we have to travel along the right
	     * edge of the box rather than the left edge.  Fortuitously,
	     * the X loop left everything set up exactly right for this case.
	     */
	    if ( cos_dir >= 0 )
	      { startx = bbox.p.x; starty = bbox.p.y;
	        endx = (diag_mag * cos_dir) + startx;
		endy = (diag_mag * sin_dir) + starty;
	      }
	    else
		starty -= y_fill_increment, endy -= y_fill_increment;

	    while ( endy += y_fill_increment,
		    (starty += y_fill_increment) <= bbox.q.y
		  )
	      hpgl_call(hpgl_draw_vector_absolute(pgls, startx, starty, endx, endy));
	    hpgl_call(hpgl_draw_current_path(pgls, hpgl_rm_vector_fill));
	  }
	if ( cross )
	  {
	    cross = false;
	    direction += 90;
	    goto start;
	  }
	hpgl_restore_pen_state(pgls, &saved_pen_state, hpgl_pen_pos);
	return 0;
#undef sin_dir
#undef cos_dir
}

/* HAS - probably not necessary and clip intersection with IW */
 private int
hpgl_polyfill_using_current_line_type(hpgl_state_t *pgls)
{
	/* gsave and grestore used to preserve the clipping region */
  	hpgl_call(hpgl_gsave(pgls));
	/* use the current path to set up a clipping path */
	/* beginning at the anchor corner replicate lines */
	hpgl_call(gs_clip(pgls->pgs));
	hpgl_call(hpgl_polyfill(pgls));
	hpgl_call(hpgl_grestore(pgls));
	hpgl_call(hpgl_clear_current_path(pgls));

	return 0;
}	      

/* HAS no color support yet!!!  maps hpgl/2 pen colors to pcl
   patterns.  Separate function as hpgl/2 color will be more
   elaborate. */
 private pcl_pattern_type_t
hpgl_map_pen_color(hpgl_state_t *pgls)
{
	/* HAS no color support yet.  Should use pcl_set_foreground_color(). */
	return pgls->g.pen.selected == 0 ? pcpt_solid_white : pcpt_solid_black;

}

/* maps current hpgl fill type to pcl pattern type.  */
/* HAS note that hpgl_map_fill_type() and hpgl_map_id() can/should be
   merged into one function of one albeit large switch statement.  It
   is two functions now for debugging purposes */
 private pcl_pattern_type_t
hpgl_map_fill_type(hpgl_state_t *pgls, hpgl_rendering_mode_t render_mode)
{
	switch ( render_mode )
	  {
	  case hpgl_rm_character:
	    switch (pgls->g.character.fill_mode)
	      {
	      case hpgl_char_solid_edge:
	      case hpgl_char_edge:	/* (value is irrelevant) */
		return pcpt_solid_black;
	      default:
		break;
	      }
	    /* fill like a polygon */
	  case hpgl_rm_polygon:
	    switch (pgls->g.fill.type)
	      {
	      case hpgl_fill_solid: 
	      case hpgl_fill_solid2: return pcpt_solid_black;
		/* should not hit next two cases in polygon mode and here
                   (i.e. implies hpgl_rm_vector_fill) */
	      case hpgl_fill_hatch: 
	      case hpgl_fill_crosshatch: return pcpt_user_defined;
	      case hpgl_fill_pcl_crosshatch: return pcpt_cross_hatch;
	      case hpgl_fill_shaded: return pcpt_shading;
	      case hpgl_fill_pcl_user_defined: return pcpt_user_defined;
	      default: break;
	      }
	    break;
	  case hpgl_rm_vector:
	  case hpgl_rm_vector_fill:
	    switch( pgls->g.screen.type )
	      {
	      case hpgl_screen_none: return hpgl_map_pen_color(pgls);
	      case hpgl_screen_shaded_fill: return pcpt_shading;
	      case hpgl_screen_hpgl_user_defined: return pcpt_user_defined;
	      case hpgl_screen_crosshatch: return pcpt_cross_hatch;
	      case hpgl_screen_pcl_user_defined: return pcpt_user_defined;
	      default: break;
	      }
	  default: break;
	  }
	/* If we are here the render mode or fill type is unknown/unsupported */
	return pcpt_solid_black;
}

private pcl_id_t *
hpgl_setget_pcl_id(pcl_id_t *id, uint val)
{
	id_set_value(*id, val);
	return(id);
}

/* HAS I don't much care for the idea of overloading pcl_id with
   shading and hatching information, but that appears to be the way
   pcl_set_drawing_color() is set up.  */
private pcl_id_t *
hpgl_map_id_type(hpgl_state_t *pgls, pcl_id_t *id, hpgl_rendering_mode_t render_mode)
{
	switch ( render_mode )
	  {
	  case hpgl_rm_character:
	    switch ( pgls->g.character.fill_mode )
	      {
	      case hpgl_char_solid_edge:
	      case hpgl_char_edge:	/* (value is irrelevant) */
		return NULL;
	      default:
		break;
	      }
	    /* fill like a polygon */
	  case hpgl_rm_polygon:
	    switch ( pgls->g.fill.type )
	      {
	      case hpgl_fill_solid: 
	      case hpgl_fill_solid2: return NULL;
		/* should not hit next two cases in polygon mode and here
                   (i.e. implies hpgl_rm_vector_fill) */
	      case hpgl_fill_hatch: 
	      case hpgl_fill_crosshatch: break;
	      case hpgl_fill_pcl_crosshatch: 
		return hpgl_setget_pcl_id(id, pgls->g.fill.param.pattern_type);
	      case hpgl_fill_shaded: 
		return
		  hpgl_setget_pcl_id(id, pgls->g.fill.param.shading);
	      case hpgl_fill_pcl_user_defined : /* HAS - not supported */
		return hpgl_setget_pcl_id(id, pgls->g.fill.param.pattern_id);
	      default: break;
	      }
	    break;
	  case hpgl_rm_vector:
	  case hpgl_rm_vector_fill:
	    switch( pgls->g.screen.type )
	      {
	      case hpgl_screen_none: return NULL;
	      case hpgl_screen_shaded_fill: 
		return
		  hpgl_setget_pcl_id(id, pgls->g.screen.param.shading);
	      case hpgl_screen_hpgl_user_defined: break; /* unsupported */
	      case hpgl_screen_crosshatch: 
		return 
		  hpgl_setget_pcl_id(id, pgls->g.screen.param.pattern_type);
	      case hpgl_screen_pcl_user_defined: /* unsupported */
		return hpgl_setget_pcl_id(id, pgls->g.screen.param.pattern_id);
	      default: break;
	      }
	    break;
	  default : break;
	  }
	/* implies something is unknown or unsupported */
	return NULL;
} 
 
 private int
hpgl_set_drawing_color(hpgl_state_t *pgls, hpgl_rendering_mode_t render_mode)
{
	pcl_id_t pcl_id;
	pl_dict_t pattern_dict;
	pcl_pattern_type_t pat_type;
	bool user_defined = ( (pgls->g.fill.type == hpgl_fill_hpgl_user_defined) ||
			      (pgls->g.screen.type == hpgl_screen_hpgl_user_defined) );
	if ( user_defined )
	  {
	    void *value; /* ignored */
	    id_set_value(pcl_id, pgls->g.raster_fill_index);
	    pattern_dict = pgls->g.raster_patterns;
	    if ( !pl_dict_find(&pattern_dict, id_key(pcl_id), 2, &value) )
	      pat_type = pcpt_solid_black;
	    else
	      pat_type = pcpt_user_defined;
	  }
	else
	  {
	    pat_type = hpgl_map_fill_type(pgls, render_mode);
	    hpgl_map_id_type(pgls, &pcl_id, render_mode);
	    pattern_dict = pgls->patterns;
	  }
	hpgl_call(pcl_set_drawing_color_rotation(pgls,
						 pat_type,
						 &pcl_id,
						 &pattern_dict,
						 (pgls->g.rotation / 90) & 3));
	gs_setsourcetransparent(pgls->pgs, pgls->g.source_transparent);
	if ( (render_mode == hpgl_rm_clip_and_fill_polygon) ||
	     ((render_mode == hpgl_rm_character) &&
	      ((pgls->g.character.fill_mode == hpgl_char_fill) ||
	       (pgls->g.character.fill_mode == hpgl_char_fill_edge))) )
	  hpgl_call(hpgl_polyfill_using_current_line_type(pgls));
	return 0;
}

 private int
hpgl_set_drawing_state(hpgl_state_t *pgls, hpgl_rendering_mode_t render_mode)
{
	/* do dash stuff. */
	hpgl_call(hpgl_set_graphics_dash_state(pgls));

	/* joins, caps, and line width. */
	hpgl_call(hpgl_set_graphics_line_attribute_state(pgls, render_mode));
	
	/* set up a clipping region */
	hpgl_call(hpgl_set_clipping_region(pgls, render_mode));

	/* set up the hpgl fills. */
	hpgl_call(hpgl_set_drawing_color(pgls, render_mode));

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
	pgls->g.pos = *pt;
	return 0;
}

 int
hpgl_add_point_to_path(hpgl_state_t *pgls, floatp x, floatp y,
		       hpgl_plot_function_t func, bool set_ctm)
{	
	static const int (*gs_procs[])(P3(gs_state *, floatp, floatp)) = {
	  hpgl_plot_function_procedures
	};

	if ( gx_path_is_null(gx_current_path(pgls->pgs)) )
	  {
	    /* Start a new GL/2 path. */
	    gs_point current_pt;
	    gs_point new_pt;
	    new_pt.x = x; new_pt.y = y;
	    if ( set_ctm )
	      hpgl_call(hpgl_set_ctm(pgls));
	    hpgl_call(gs_newpath(pgls->pgs));

	    /* moveto the current position */
	    hpgl_call(hpgl_get_current_position(pgls, &current_pt));
	    hpgl_call_check_lost(gs_moveto(pgls->pgs, 
					   current_pt.x, 
					   current_pt.y));
	  }
	{ int code = (*gs_procs[func])(pgls->pgs, x, y);

	  if ( code < 0 )
	    { hpgl_call_note_error(code);
	      if ( code == gs_error_limitcheck )
		hpgl_set_lost_mode(pgls, hpgl_lost_mode_entered);
	    }
	  else
	    { gs_point point;

	      if ( hpgl_plot_is_absolute(func) )
		hpgl_set_lost_mode(pgls, hpgl_lost_mode_cleared);

	      /* update hpgl's state position */
	      gs_currentpoint(pgls->pgs, &point);
	      hpgl_call(hpgl_set_current_position(pgls, &point));
	    }
	}

	return 0;
}

/* destroy the current path. */
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

/* converts pcl coordinate to device space and back to hpgl space */
 int
hpgl_add_pcl_point_to_path(hpgl_state_t *pgls, const gs_point *pcl_pt)
{
	gs_point dev_pt, hpgl_pt;

	hpgl_call(hpgl_clear_current_path(pgls));
	pcl_set_ctm(pgls, false);
	hpgl_call(gs_transform(pgls->pgs, pcl_pt->x, pcl_pt->y, &dev_pt));
	hpgl_call(hpgl_set_ctm(pgls));
	hpgl_call(gs_itransform(pgls->pgs, dev_pt.x, dev_pt.y, &hpgl_pt));
	hpgl_call(hpgl_add_point_to_path(pgls, hpgl_pt.x, hpgl_pt.y,
					 hpgl_plot_move_absolute, true));
	return 0;
}

 int
hpgl_add_arc_to_path(hpgl_state_t *pgls, floatp center_x, floatp center_y, 
		     floatp radius, floatp start_angle, floatp sweep_angle,
		     floatp chord_angle, bool start_moveto, bool draw)
{
	floatp start_angle_radians = start_angle * degrees_to_radians;
	/*
	 * Ensure that the sweep angle is an integral multiple of the
	 * chord angle, by decreasing the chord angle if necessary.
	 */
	int num_chords = ceil(sweep_angle / chord_angle);
	floatp chord_angle_radians =
	  sweep_angle / num_chords * degrees_to_radians;
	int i;
	floatp arccoord_x, arccoord_y; 

	hpgl_compute_arc_coords(radius, center_x, center_y, 
				start_angle_radians, 
				&arccoord_x, &arccoord_y);
	hpgl_call(hpgl_add_point_to_path(pgls, arccoord_x, arccoord_y, 
					 (draw && !start_moveto ? 
					 hpgl_plot_draw_absolute :
					 hpgl_plot_move_absolute), true));

	/* HAS - pen up/down is invariant in the loop */
	for ( i = 0; i < num_chords; i++ ) 
	  {
	    start_angle_radians += chord_angle_radians;
	    hpgl_compute_arc_coords(radius, center_x, center_y, 
				    start_angle_radians, 
				    &arccoord_x, &arccoord_y);
	    hpgl_call(hpgl_add_point_to_path(pgls, arccoord_x, arccoord_y, 
					     (draw ? hpgl_plot_draw_absolute :
					     hpgl_plot_move_absolute), true));
	  }
	return 0;
}

/* add a 3 point arc to the path */ 
 int
hpgl_add_arc_3point_to_path(hpgl_state_t *pgls, floatp start_x, floatp
			    start_y, floatp inter_x, floatp inter_y, 
			    floatp end_x, floatp end_y, floatp chord_angle,
			    bool draw)
{
	/* handle unusual cases as per pcltrm */
	if ( hpgl_3_same_points(start_x, start_y, 
				inter_x, inter_y, 
				end_x, end_y) )
	  {
	    hpgl_call(hpgl_add_point_to_path(pgls, start_x, start_y, draw, true));
	    return 0;
	  }
	if ( hpgl_3_no_intermediate(start_x, start_y, 
				    inter_x, inter_y,
				    end_x, end_y) )
	  {
	    hpgl_call(hpgl_add_point_to_path(pgls, start_x, start_y, draw, true));
	    hpgl_call(hpgl_add_point_to_path(pgls, end_x, end_y, draw, true));
	    return 0;
	  }
	if ( hpgl_3_same_endpoints(start_x, start_y, 
				   inter_x, y_inter, 
				   end_x, end_y) ) 
	  {
	    hpgl_call(hpgl_add_arc_to_path(pgls, (start_x + inter_x) / 2.0,
					   (start_y + inter_y) / 2.0,
					   (hypot((inter_x - start_x),
						  (inter_y - start_y)) / 2.0),
					   0.0, 360.0, chord_angle, false,
					   draw));
	    return 0;
	  }

	if ( hpgl_3_colinear_points(start_x, start_y, inter_x, inter_y, end_x, end_y) ) 
	  {
	    if ( hpgl_3_intermediate_between(start_x, start_y, 
					     inter_x, inter_y, 
					     end_x, end_y) ) 
	      {
		hpgl_call(hpgl_add_point_to_path(pgls, start_x, start_y, draw, true));
		hpgl_call(hpgl_add_point_to_path(pgls, end_x, end_x, draw, true));
	      }
	    else 
	      {
		hpgl_call(hpgl_add_point_to_path(pgls, start_x, start_y, draw, true));
		hpgl_call(hpgl_add_point_to_path(pgls, inter_x, inter_y, draw, true));
		hpgl_call(hpgl_add_point_to_path(pgls, end_x, end_y, draw, true));
	      }
	    return 0;
	  }

	/* normal 3 point arc case */
	{
	  hpgl_real_t center_x, center_y, radius;
	  hpgl_real_t start_angle, inter_angle, end_angle;
	  hpgl_real_t sweep_angle;

	  hpgl_call(hpgl_compute_arc_center(start_x, start_y, 
					    inter_x, inter_y, 
					    end_x, end_y, 
					    &center_x, &center_y));

	  radius = hypot(start_x - center_x, start_y - center_y);
	    
	  start_angle = radians_to_degrees *
	    hpgl_compute_angle(start_x - center_x, start_y - center_y);

	  inter_angle = radians_to_degrees *
	    hpgl_compute_angle(inter_x - center_x, inter_y - center_y);

	  end_angle = radians_to_degrees *
	    hpgl_compute_angle(end_x - center_x, end_y - center_y);
	    
	  sweep_angle = end_angle - start_angle;

	    /*
	     * Figure out which direction to draw the arc, depending on the
	     * relative position of start, inter, and end.  Case analysis
	     * shows that we should draw the arc counter-clockwise from S to
	     * E iff exactly 2 of S<I, I<E, and E<S are true, and clockwise
	     * if exactly 1 of these relations is true.  (These are the only
	     * possible cases if no 2 of the points coincide.)
	     */

	  if ( (start_angle < inter_angle) + (inter_angle < end_angle) +
	       (end_angle < start_angle) == 1
	       )
	    {
	      if ( sweep_angle > 0 )
		sweep_angle -= 360;
	    }
	  else
	    {
	      if ( sweep_angle < 0 )
		sweep_angle += 360;
	    }

	  hpgl_call(hpgl_add_arc_to_path(pgls, center_x, center_y, 
					 radius, start_angle, sweep_angle, 
					 (sweep_angle < 0.0 ) ?
					 -chord_angle : chord_angle, false,
					 draw));
	  return 0;
	}
}
	     
/* Bezier's are handled a bit differently than arcs as we use the
   gs_curveto() operator directly in lieue of flattening and using
   gs_moveto() and gs_lineto(). */

 int
hpgl_add_bezier_to_path(hpgl_state_t *pgls, floatp x1, floatp y1, 
			floatp x2, floatp y2, floatp x3, floatp y3, 
			floatp x4, floatp y4, bool draw)
{
	hpgl_call(hpgl_add_point_to_path(pgls, x1, y1, 
					 hpgl_plot_move_absolute, true));
	if ( draw )
	  hpgl_call(gs_curveto(pgls->pgs, x2, y2, x3, y3, x4, y4));

	/* this is done simply to set gl/2's current position */
	hpgl_call(hpgl_add_point_to_path(pgls, x4, y4, 
					 draw ? hpgl_plot_draw_absolute :
					 hpgl_plot_move_absolute, true));
	return 0;
}

/* an implicit gl/2 style closepath.  If the first and last point are
   the same the path gets closed. HAS - eliminate CSE gx_current_path */
 private int
hpgl_close_path(hpgl_state_t *pgls)
{
	gs_point first, first_device, last;
	/* if we do not have a subpath there is nothing to do.  HAS
           perhaps hpgl_draw_current_path() should make this
           observation instead of checking for a null path??? */
	if ( !gx_current_path(pgls->pgs)->first_subpath ) return 0;
	
	/* get the first points of the path in device space */
	first_device.x = (gx_current_path(pgls->pgs))->first_subpath->pt.x;
	first_device.y = (gx_current_path(pgls->pgs))->first_subpath->pt.y;
	/* convert to user/plu space */
	hpgl_call(gs_itransform(pgls->pgs,
				fixed2float(first_device.x), 
				fixed2float(first_device.y), 
				&first));

	/* get gl/2 current position -- always current units */
	hpgl_call(hpgl_get_current_position(pgls, &last));
	/* if the first and last are the same close the path (i.e
           force gs to apply join and miter) */
	if ( (first.x == last.x) && (first.y == last.y) )
	    hpgl_call(gs_closepath(pgls->pgs));
	return 0;
}
	
/* HAS -- There will need to be compression phase here note that
   extraneous PU's do not result in separate subpaths.  */

/* Draw (stroke or fill) the current path. */
 int
hpgl_draw_current_path(hpgl_state_t *pgls, hpgl_rendering_mode_t render_mode)
{
	gs_state *pgs = pgls->pgs;

	if ( gx_path_is_null(gx_current_path(pgs)) )
	  return 0;

	hpgl_call(hpgl_close_path(pgls));
	hpgl_call(hpgl_set_drawing_state(pgls, render_mode));
	
	switch ( render_mode )
	  {
	  case hpgl_rm_character:
	    {
	      /* HAS need to set attributes in set_drawing color (next 2) */

	      /* Intellifonts require eofill, but TrueType require fill. */
	      /****** HACK: look at the scaling technology of ******/
	      /****** the current font to decide. ******/
	      int (*fill)(P1(gs_state *));

	      if ( pgls->g.font->scaling_technology == plfst_Intellifont )
		fill = gs_eofill;
	      else
		fill = gs_fill;
	      switch ( pgls->g.character.fill_mode )
		{
		case hpgl_char_solid_edge:
		  gs_setgray(pgs, 0.0);	/* solid fill */
		  hpgl_call((*fill)(pgs));
		  /* falls through */
		case hpgl_char_edge:
char_edge:	  if ( pgls->g.bitmap_fonts_allowed )
		    break;	/* no edging */
		  if ( pgls->g.character.edge_pen != 0 )
		    { gs_setgray(pgs, 0.0);	/* solid edge */
		      gs_setlinewidth(pgs, 0.1);
		      hpgl_call(gs_stroke(pgs));
		    }
		  break;
		case hpgl_char_fill:
		  /****** SHOULD USE VECTOR FILL ******/
		  hpgl_call((*fill)(pgs));
		  break;
		case hpgl_char_fill_edge:
		  /****** SHOULD USE VECTOR FILL ******/
		  hpgl_call((*fill)(pgs));
		  goto char_edge;
		}
	    }
	    break;
	  case hpgl_rm_polygon:
	    if ( pgls->g.fill_type == hpgl_even_odd_rule )
	      hpgl_call(gs_eofill(pgs));
	    else /* hpgl_winding_number_rule */
	      hpgl_call(gs_fill(pgs));
	    break;
	  case hpgl_rm_clip_and_fill_polygon:
	    /* HACK handled by hpgl_set_drawing_color */
	    break;
	  case hpgl_rm_vector:
	  case hpgl_rm_vector_fill:
	    /* we reset the ctm before stroking to preserve the line width
	       information */
	    hpgl_call(hpgl_set_plu_ctm(pgls));
	    hpgl_call(gs_stroke(pgls->pgs));
	    break;
	  default :
	    dprintf("unknown render mode\n");
	  }

	/* the page has been marked */
	pgls->have_page = true;
	return 0;
}

/* HAS needs error checking for empty paths and such */
 int 
hpgl_copy_current_path_to_polygon_buffer(hpgl_state_t *pgls)
{
	gx_path *ppath = gx_current_path(pgls->pgs);

	gx_path_copy(ppath, &pgls->g.polygon.buffer.path, true );
	return 0;
}

 int
hpgl_gsave(hpgl_state_t *pgls)
{
	hpgl_call(gs_gsave(pgls->pgs));
	return 0;
}

 int
hpgl_grestore(hpgl_state_t *pgls)
{
	hpgl_call(gs_grestore(pgls->pgs));
	return 0;
}


 int 
hpgl_copy_polygon_buffer_to_current_path(hpgl_state_t *pgls)
{
	gx_path *ppath = gx_current_path(pgls->pgs);
	gx_path_copy(&pgls->g.polygon.buffer.path, ppath, true );
	return 0;
}
