/* Copyright (C) 1996, 1997 Aladdin Enterprises.  All rights reserved.
   Unauthorized use, copying, and/or distribution prohibited.
 */

/* pgdraw.h */
/* Definitions for HP-GL/2 line drawing/path building routines */

#ifndef pgdraw_INCLUDED
#  define pgdraw_INCLUDED

/* set ctm global for SC command only */
int hpgl_set_ctm(P1(hpgl_state_t *pgls));

/* function to get the current hpgl/2 state position */
int hpgl_get_current_position(P2(hpgl_state_t *pgls, gs_point *pt));

/* puts a point into the path using the operation specified by func */
int hpgl_add_point_to_path(P4(hpgl_state_t *pgls, floatp x, floatp y, 
			      hpgl_plot_function_t func));

/* puts an arc into the current path.  start moveto indicates that we
   use moveto to go from the arc center to arc circumference. */
int hpgl_add_arc_to_path(P9(hpgl_state_t *pgls, floatp center_x, 
			    floatp center_y, floatp radius, 
			    floatp start_angle, floatp sweep_angle, 
			    floatp chord_angle, bool start_moveto,
			    bool draw));

/* puts a 3 point arc into the current path.  Note that the
   decomposition is a bit different for 3 point arcs since the polygon
   wedge routines use this function as well */

int hpgl_add_arc_3point_to_path(P9(hpgl_state_t *pgls, floatp start_x, floatp
				   start_y, floatp inter_x, floatp inter_y, 
				   floatp end_x, floatp end_y, floatp chord_angle,
				   bool draw));

/* put bezier into the current path */
int hpgl_add_bezier_to_path(P10(hpgl_state_t *pgls, floatp x1, 
				floatp y1, floatp x2, floatp y2, 
				floatp x3, floatp y3, floatp x4, 
				floatp y4, bool draw));

/* clears the current path with stroke or fill */
int hpgl_draw_current_path(P2(hpgl_state_t *pgls, 
			      hpgl_rendering_mode_t render_mode));

/* save gs graphics state + HPGL/2's first moveto state */
int hpgl_gsave(P1(hpgl_state_t *pgls));

/* restore gs graphics state + HPGL/2's first moveto state */
int hpgl_grestore(P1(hpgl_state_t *pgls));

/* path copying for polygons rendering */
int hpgl_copy_polygon_buffer_to_current_path(P1(hpgl_state_t *pgls));

int hpgl_copy_current_path_to_polygon_buffer(P1(hpgl_state_t *pgls));

/* draw the current path with stroke or fill, but do not clear */
int hpgl_draw_and_preserve_path(P2(hpgl_state_t *pgls, 
				   hpgl_rendering_mode_t render_mode));

/* destroy the current path */
int hpgl_clear_current_path(P1(hpgl_state_t *pgls));

/* closes the current path, making the first point and last point coincident */
int hpgl_close_current_path(P1(hpgl_state_t *pgls));

/* adds a pcl point to the current path */
int hpgl_add_pcl_point_to_path(P2(hpgl_state_t *pgls, gs_point *pcl_point));

/* initialize the path machinery */
#define hpgl_init_path(state) ((state)->g.have_first_moveto = false)

#endif                          /* pgdraw_INCLUDED */
