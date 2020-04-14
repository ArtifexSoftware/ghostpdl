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


/* pggeom.h */
/* Definitions for HP-GL/2 geometry. */

#ifndef pggeom_INCLUDED
#  define pggeom_INCLUDED

#include "math_.h"
#include "gstypes.h"            /* for gs_point */

/* ------ Useful conversions ------ */

/*
 * Convert various other kinds of units to plotter units
 * (1 plu = 1/40 mm = 1/1016 inch).
 */
#define coord_2_plu(a) ((a) * (1016.0 / 7200.0))
#define plu_2_coord(a) ((a) * (7200.0 / 1016.0))
#define mm_2_plu(a) ((a) * 40.0)
#define inches_2_plu(a) ((a) * 1016.0)
#define plu_2_inches(a) ((a) / 1016.0)

/* ------ Lines, angles, arcs, and chords ------ */

/* calculate the distance between 2 points */
#define hpgl_compute_distance(x1, y1, x2, y2) \
  hypot((x1) - (x2), (y1) - (y2))

/* compute the angle between 0 and 2*PI given the slope */
double hpgl_compute_angle(double dx, double dy);

/* compute the center of an arc given 3 points on the arc */
int hpgl_compute_arc_center(double x1, double y1, double x2,
                            double y2, double x3, double y3,
                            double * pcx, double * pcy);

/* compute the coordinates of a point on an arc */
int hpgl_compute_arc_coords(double radius, double center_x,
                            double center_y, double angle,
                            double * px, double * py);

/* given a start point, angle (degrees) and magnitude of a vector compute its
   endpoints */
int hpgl_compute_vector_endpoints(double magnitude, double x, double y,
                                  double angle_degrees, double * endx,
                                  double * endy);

/* ------ 3-point arcs ------ */

#define epsilon (1.0/2048.0)
/* defined with epsilon */
#define equal(a, b) ((fabs((a)-(b)) < epsilon))

/* this definition simplifies subsequent definitions */
#define equal2(a, b, c, d) ((equal((a), (b))) && (equal((c), (d))))

/* points are equal.  HAS -- TEST for epsilon */
#define hpgl_3_same_points(x1, y1, x2, y2, x3, y3) \
        ((equal2((x1), (x2), (x2), (x3))) && (equal2((y1), (y2), (y2), (y3))))

/* points are on the same line */
#define hpgl_3_colinear_points(x1, y1, x2, y2, x3, y3) \
        (equal(((y1) - (y3)) * ((x1) - (x2)), ((y1) - (y2)) * ((x1) - (x3))))

/* intermediate is the same as first point or last */
#define hpgl_3_no_intermediate(x1, y1, x2, y2, x3, y3) \
        ((equal2((x1), (x2), (y1), (y2))) || (equal2((x2), (x3), (y2), (y3))))

/* intermediate lies between endpoints */
#define hpgl_3_intermediate_between(x1, y1, x2, y2, x3, y3) \
     ((((x1) >= (x2)) && ((x2) <= (x3))) && \
      (((y1) >= (y2)) && ((y2) <= (y3))))

/* equal endpoints */
#define hpgl_3_same_endpoints(x1, y1, x2, y2, x3, y3) \
        (equal2((x1), (x3), (y1), (y3)))
#endif /* pggeom_INCLUDED */
