/* Copyright (C) 1996, 1997 Aladdin Enterprises.  All rights reserved.
   Unauthorized use, copying, and/or distribution prohibited.
 */

/* pggeom.c */
/* HP-GL/2 geometry routines */

#include "stdio_.h"
#include "math_.h"
#include "pggeom.h"

/* HAS most of these computations require more error checking */

/* ------ Lines, angles, arcs, and chords ------ */

/* compute the angle between 0 and 2*PI given the slope */
floatp
hpgl_compute_angle(floatp dx, floatp dy) 
{
	floatp alpha = atan2(dy, dx);

	return (alpha < 0 ? alpha + M_PI * 2.0 : alpha);
}

/* normalize the chord angle to 0 - PI/2 */
floatp
hpgl_normalize_chord_angle(floatp angle)
{
	if ( angle < 0 )
	  angle = -angle;
	if ( angle > 360 )
	  angle = angle - (floor(angle / 360.0) * 360.0);
	if ( angle > 180 )
	  angle = 360 - angle;
	if ( angle < 0.1 )
	  angle = 0.36;
	return angle;
}

/* number of chord angles within the arc */
int
hpgl_compute_number_of_chords(floatp arc_angle, floatp chord_angle)
{
	return (int)ceil(fabs(arc_angle / chord_angle));
}

/* the arc angle as an integral multiple of the chord angle */
floatp
hpgl_adjust_arc_angle(int num_chords, floatp arc_angle)
{
	return(num_chords * arc_angle);
}

/* compute the center of an arc given 3 points on the arc */
int
hpgl_compute_arc_center(floatp x1, floatp y1, floatp x2, floatp y2, 
			floatp x3, floatp y3, floatp *pcx, floatp *pcy)

{
	floatp px2, py2, dx2, dy2, px3, py3, dx3, dy3;
	double denom, t2;

	/*
	 * The center is the intersection of the perpendicular bisectors
	 * of the 3 chords.  Any two will do for the computation.
	 * (For greatest numerical stability, we should probably choose
	 * the two outside chords, but this is a refinement that we will
	 * leave for the future.)
	 * We define each bisector by a line with the equations
	 *	xi = pxi + ti * dxi
	 *	yi = pyi + ti * dyi
	 * where i is 2 or 3.
	 */

#define compute_bisector(px, py, dx, dy, xa, ya, xb, yb)\
  (px = (xa + xb) / 2, py = (ya + yb) / 2,\
   dx = (ya - yb), dy = (xb - xa) /* 90 degree rotation (either way is OK) */)

	compute_bisector(px2, py2, dx2, dy2, x1, y1, x2, y2);
	compute_bisector(px3, py3, dx3, dy3, x1, y1, x3, y3);

#undef compute_bisector

	/*
	 * Now find the intersections by solving for t2 or t3:
	 *	px2 + t2 * dx2 = px3 + t3 * dx3
	 *	py2 + t2 * dy2 = py3 + t3 * dy3
	 * i.e., in standard form,
	 *	t2 * dx2 - t3 * dx3 = px3 - px2
	 *	t2 * dy2 - t3 * dy3 = py3 - py2
	 * The solution of
	 *	a*x + b*y = c
	 *	d*x + e*y = f
	 * is
	 *	denom = a*e - b*d
	 *	x = (c*e - b*f) / denom
	 *	y = (a*f - c*d) / denom
	 */
	denom = dx3 * dy2 - dx2 * dy3;
	if ( fabs(denom) < 1.0e-6 )
	  return -1;		/* degenerate */

	t2 = ((px3 - px2) * (-dy3) - (-dx3) * (py3 - py2)) / denom;
	*pcx = px2 + t2 * dx2;
	*pcy = py2 + t2 * dy2;
	return 0;
}

/* calculate rotation as clockwise (false) or counterclockwise (true). */
/* Algorithm is the same as counterclockwise calculation in gxstroke. */
bool
hpgl_compute_arc_direction(floatp start_x, floatp start_y, 
			   floatp end_x, floatp end_y) 
{
	return((start_x * end_y) > (end_x * start_y)); 
}

/* compute the coordinates of a point on an arc */
int
hpgl_compute_arc_coords(floatp radius, floatp center_x, floatp center_y, 
			floatp angle, floatp *px, floatp *py)
{
	*px = radius * cos(angle) + center_x;
	*py = radius * sin(angle) + center_y;
	return 0;
}

/* given a start point, angle (degrees) and magnitude of a vector compute its
   endpoints */
int
hpgl_compute_vector_endpoints(floatp magnitude, floatp x, floatp y, 
			      floatp angle_degrees, floatp *endx, floatp *endy)

{
	return hpgl_compute_arc_coords(magnitude, x, y,
				       angle_degrees * (M_PI/180.0),
				       endx, endy);
}

 int 
hpgl_compute_dot_product(gs_point *pt1, gs_point *pt2, floatp *result)
{
	*result = ((pt1->x * pt2->x) + (pt1->y * pt2->y));
	return 0;
}

/* scales a 2d vector */
 int 
hpgl_scale_vector(gs_point *pt1, floatp scale, gs_point *result)
{
	result->x = scale * pt1->x;
	result->y = scale * pt1->y;
	return 0;
}
