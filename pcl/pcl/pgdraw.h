/* Copyright (C) 2001-2020 Artifex Software, Inc.
   All Rights Reserved.

   This software is provided AS-IS with no warranty, either express or
   implied.

   This software is distributed under license and may not be copied,
   modified or distributed except as expressly authorized under the terms
   of the license contained in the file LICENSE in this distribution.

   Refer to licensing information at http://www.artifex.com or contact
   Artifex Software, Inc.,  1305 Grant Avenue - Suite 200, Novato,
   CA 94945, U.S.A., +1(415)492-9861, for further information.
*/


/* pgdraw.h */
/* Definitions for HP-GL/2 line drawing/path building routines */

#ifndef pgdraw_INCLUDED
#  define pgdraw_INCLUDED

/* set plu ctm, exported only so that labels can ignore scaling */
int hpgl_set_plu_ctm(hpgl_state_t * pgls);

/* compute the scaling transformation from plu to user units */
int hpgl_compute_user_units_to_plu_ctm(const hpgl_state_t * pgls,
                                       gs_matrix * pmat);

/* The following 2 functions can be used together to calculate a ctm
   without picture frame scaling.  */
int hpgl_set_pcl_to_plu_ctm(hpgl_state_t * pgls);

int hpgl_set_user_units_to_plu_ctm(const hpgl_state_t * pgls);

/* set (user units) ctm */
int hpgl_set_ctm(hpgl_state_t * pgls);

int hpgl_get_selected_pen(hpgl_state_t * pgls);

/* set the hpgl/2 clipping region accounting for pcl picture frame and
   gl/2 soft clip window */
int
hpgl_set_clipping_region(hpgl_state_t * pgls,
                         hpgl_rendering_mode_t render_mode);

/* function set up the current drawing attributes this is only used by
   the character code since it does most of it's own graphic's state
   bookkeeping */
int hpgl_set_drawing_color(hpgl_state_t * pgls,
                           hpgl_rendering_mode_t render_mode);

/* function to get the current hpgl/2 state position */
int hpgl_get_current_position(hpgl_state_t * pgls, gs_point * pt);

/* update the carriage return position to the current gl/2 positiion */
int hpgl_update_carriage_return_pos(hpgl_state_t * pgls);

/* function to set the current hpgl/2 state position */
int hpgl_set_current_position(hpgl_state_t * pgls, gs_point * pt);

/* puts a point into the path using the operation specified by func */
int hpgl_add_point_to_path(hpgl_state_t * pgls, double x, double y,
                           hpgl_plot_function_t func, bool set_ctm);

/* puts an arc into the current path.  start moveto indicates that we
   use moveto to go from the arc center to arc circumference. */
int hpgl_add_arc_to_path(hpgl_state_t * pgls, double center_x,
                         double center_y, double radius,
                         double start_angle, double sweep_angle,
                         double chord_angle, bool start_moveto,
                         hpgl_plot_function_t draw, bool set_ctm);

/* puts a 3 point arc into the current path.  Note that the
   decomposition is a bit different for 3 point arcs since the polygon
   wedge routines use this function as well */
int hpgl_add_arc_3point_to_path(hpgl_state_t * pgls, double start_x, double
                                start_y, double inter_x, double inter_y,
                                double end_x, double end_y,
                                double chord_angle,
                                hpgl_plot_function_t draw);

int hpgl_close_path(hpgl_state_t * pgls);

/* put bezier into the current path */
int hpgl_add_bezier_to_path(hpgl_state_t * pgls, double x1,
                            double y1, double x2, double y2,
                            double x3, double y3, double x4,
                            double y4, hpgl_plot_function_t draw);

/* clears the current path with stroke or fill */
int hpgl_draw_current_path(hpgl_state_t * pgls,
                           hpgl_rendering_mode_t render_mode);

/* save/restore gs graphics state + HPGL/2's first moveto state */
#define hpgl_gsave(pgls)    pcl_gsave(pgls)
#define hpgl_grestore(pgls) pcl_grestore(pgls)

/* path copying for polygons rendering */
int hpgl_copy_polygon_buffer_to_current_path(hpgl_state_t * pgls);

int hpgl_copy_current_path_to_polygon_buffer(hpgl_state_t * pgls);

/* draw the current path with stroke or fill, but do not clear */
int hpgl_draw_and_preserve_path(hpgl_state_t * pgls,
                                hpgl_rendering_mode_t render_mode);

/* destroy the current path */
int hpgl_clear_current_path(hpgl_state_t * pgls);

/* closes the current path, making the first point and last point coincident */
int hpgl_close_current_path(hpgl_state_t * pgls);

/* adds a pcl point to the current path */
int hpgl_add_pcl_point_to_path(hpgl_state_t * pgls,
                               const gs_point * pcl_point);

/* closes a subpolygon; PM1 or CI */
int hpgl_close_subpolygon(hpgl_state_t * pgls);

hpgl_real_t hpgl_width_scale(hpgl_state_t * pgls);

void hpgl_set_hpgl_path_mode(hpgl_state_t * pgls, bool enable);

#endif /* pgdraw_INCLUDED */
