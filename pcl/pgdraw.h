/* Copyright (C) 1996 Aladdin Enterprises.  All rights reserved.
   Unauthorized use, copying, and/or distribution prohibited.
 */

/* pgdraw.h */
/* Definitions for HP-GL/2 line drawing/path building routines */

#ifndef pgdraw_INCLUDED
#  define pgdraw_INCLUDED

/* puts a point into the path using the operation specified by
   gs_func() - moveto, rmoveto, lineto, rlineto */
int hpgl_add_point_to_path(P4(hpgl_state_t *pgls, floatp x, floatp y, 
			       int (*gs_func)(gs_state *pgs, 
					      floatp x, floatp y)));

/* puts an arc into the current path */
int hpgl_add_arc_to_path(P7(hpgl_state_t *pgls, floatp center_x, 
			  floatp center_y, floatp radius, 
			  floatp start_angle, floatp sweep_angle, 
			  floatp chord_angle));

/* put bezier into the current path uses */
int hpgl_add_bezier_to_path(P9(hpgl_state_t *pgls, floatp x1, 
				floatp y1, floatp x2, floatp y2, 
				floatp x3, floatp y3, floatp x4, 
				floatp y4));

/* clears the current path with stroke or fill */
int hpgl_draw_current_path(P1(hpgl_state_t *pgls));

/* utility routine to add a 2 points and stroke/fill */
int hpgl_draw_line(P5(hpgl_state_t *pgls, floatp x1, floatp y1, 
			 floatp x2, floatp y2));

/* Draw a dot.  A vector of zero length */
int hpgl_draw_dot(P3(hpgl_state_t *pgls, floatp x1, floatp y1));


/* destroys the current path */
int hpgl_clear_current_path(P1(hpgl_state_t *pgls));

/* closes the current path, making the first point and last point coincident */
int hpgl_close_current_path(P1(hpgl_state_t *pgls));

/* initialize the path machinery */
#define hpgl_init_path(state) ((state)->g.have_path = false)

#endif                          /* pgdraw_INCLUDED */
