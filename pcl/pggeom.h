/* Copyright (C) 1996 Aladdin Enterprises.  All rights reserved.
   Unauthorized use, copying, and/or distribution prohibited.
 */

/* pggeom.h */
/* Definitions for HP-GL/2 geometry. */

#ifndef pggeom_INCLUDED
#  define pggeom_INCLUDED

#include "math_.h"

/* normalize the chord angle to 0 - PI/2 */
floatp hpgl_normalize_chord_angle(P1(floatp angle));

/* gives the center of an arc given 3 points on the arc */
int hpgl_compute_arc_center(P8(floatp x1, floatp y1, floatp x2, 
			       floatp y2, floatp x3, floatp y3, 
			       floatp *cx, floatp *cy));

/* calculates rotation as clockwise or counterclockwise */
int hpgl_compute_arc_direction(P4(floatp start_x, floatp start_y,
				  floatp end_x, floatp end_y));

/* angle between 2 slopes */
floatp hpgl_compute_angle(P2(floatp dx, floatp dy));

/* number of chord angles within the arc */
int hpgl_compute_number_of_chords(P2(floatp arc_angle, 
				     floatp chord_angle));

/* the arc angle as an integral multiple of the chord angle */
floatp hpgl_adjust_arc_angle(P2(int num_chords, floatp chord_angle));

/* compute the coordinates on the arc */
void hpgl_compute_arc_coords(P6(floatp radius, floatp center_x, 
				floatp center_y, floatp angle, 
				floatp *x, floatp *y));

/* given a start point, angle (degrees) and magnitude of a vector compute its
   endpoints */

void hpgl_compute_vector_endpoints(P6(floatp magnitude, floatp x, floatp y, 
				     floatp angle, floatp *endx, floatp *endy));

/* calculate the distance between 2 points */
#define hpgl_compute_distance(x1, y1, x2, y2) \
      sqrt((((x1)-(x2))*((x1)-(x2)))+(((y1)-(y2))*((y1)-(y2))))

/* defines for arc 3 point */

/* points are equal.  HAS -- TEST for epsilon */
#define hpgl_3_same_points(x1, y1, x2, y2, x3, y3) \
      (((x1) == (x2)) && ((x2) == (x3)) && \
       ((y1) == (y2)) && ((y2) == (y3)))

/* points are on the same line */
#define hpgl_3_colinear_points(x1, y1, x2, y2, x3, y3) \
      ((((y1) - (y3)) * ((x1) - (x2))) == \
       (((y1) - (y2)) * ((x1) - (x3))))

/* intermediate is the same as first point or last */
#define hpgl_3_no_intermediate(x1, y1, x2, y2, x3, y3) \
     ((((x1) == (x2)) && ((y1) == (y2))) || \
      (((x2) == (x3)) && ((y2) == (y3))))

/* intermediate lies between endpoints */
#define hpgl_3_intermediate_between(x1, y1, x2, y2, x3, y3) \
     ((((x1) >= (x2)) && ((x2) <= (x3))) && \
      (((y1) >= (y2)) && ((y2) <= (y3))))

/* equal endpoints */
#define hpgl_3_same_endpoints(x1, y1, x2, y2, x3, y3) \
     (((x1) == (x3)) && ((y1) == (y3)))


/* useful conversions */

/* centipoints to plotter units (1/1016 inch) */
#define centipoints_2_plu(a) (((a) / 7200.0) * 1016.0)

/* millimeters to plotter units (1/1016 inch) */
#define mm_2_plu(a) ((a) * 40.0)

#endif                                /* <filename>_INCLUDED */
