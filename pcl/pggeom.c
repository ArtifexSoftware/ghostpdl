/* Copyright (C) 1996 Aladdin Enterprises.  All rights reserved.
   Unauthorized use, copying, and/or distribution prohibited.
 */

/* pggeom.c */
/* HP-GL/2 geometry routines */

#include "stdio_.h"
#include "math_.h"
#include "pggeom.h"

/* HAS most of these computations require more error checking */

 int
hpgl_compute_arc_center(floatp x1, floatp y1, floatp x2, floatp y2, 
			floatp x3, floatp y3, floatp *cx, floatp *cy)

{
	floatp s12, s13, len1, len2, len3, dx12, dy12, dx13, dy13;
	floatp resx, resy;

	if ( x1 == x3 && y1 == y3 )
	  return 0;

	dx12 = x1 - x2;
	dy12 = y1 - y2;
	dx13 = x1 - x3;
	dy13 = y1 - y3;

	s12 = asin(dy12 / sqrt(dx12 * dx12 + dy12 * dy12));
	s13 = asin(dy13 / sqrt(dx13 * dx13 + dy13 * dy13));
	if ( fabs(s12 - s13) < .01 )
	  return 0;

	len1 = x1 * x1 + y1 * y1;
	len2 = x2 * x2 + y2 * y2;
	len3 = x3 * x3 + y3 * y3;

	resy = (dx12 * (len3 - len1) - dx13 * (len2 - len1)) /
		(2 * (dx13 * dy12 - dx12 * dy13));
	if ( x1 != x3 )
	  resx = (len3 + 2 * (resy) * dy13 - len1) / (2 * (-dx13));
	else
	  resx = (len2 + 2 * (resy) * dy12 - len1) / (2 * (-dx12));

	*cx = resx;
	*cy = resy;
	return 1;
}

/* compute the angle between 0 to 2PI between 2 slopes */
floatp
hpgl_compute_angle(floatp dx, floatp dy) 
{
	floatp alpha;

	if ( dx == 0 ) {
	  if ( dy > 0 )
	    alpha = (M_PI / 2.0);
	  else
	    alpha = 3 * (M_PI / 2.0);
	} else if ( dy == 0 ) {
	  if ( dx > 0 )
	    alpha = 0;
	  else
	    alpha = M_PI;
	} else {
	  alpha = atan(dy / dx);	/* range = -PI/2 to PI/2 */
	  if ( dx < 0 )
	    alpha += M_PI;
	  else if ( dy < 0 )
	    alpha += (M_PI * 2.0);
	}
	return (alpha);
}

/* returns true if counterclockwise.  Algorithm is the same as
   counterclockwise calculation is gxstroke. */
bool
hpgl_compute_arc_direction(floatp start_x, floatp start_y, 
			   floatp end_x, floatp end_y) 
{
	return((start_x * end_y) > (end_x * start_y)); 
}

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

 int
hpgl_compute_number_of_chords(floatp arc_angle, floatp chord_angle)
{
	floatp num_chords;

	num_chords = ceil(arc_angle/chord_angle);
	if ( num_chords < 0.0 ) num_chords = -num_chords;
	return( (int)num_chords);
}

 floatp
hpgl_adjust_arc_angle(int num_chords, floatp arc_angle)
{
	return(num_chords * arc_angle);
}

 void
hpgl_compute_arc_coords(floatp radius, floatp center_x, floatp center_y, 
			floatp angle, floatp *x, floatp *y)
{
	*x = radius * cos(angle) + center_x;
	*y = radius * sin(angle) + center_y;
}

 void
hpgl_compute_vector_endpoints(floatp magnitude, floatp x, floatp y, 
			      floatp angle, floatp *endx, floatp *endy)

{
	hpgl_compute_arc_coords(magnitude, x, y, (angle * (M_PI/180.0)),
				endx, endy);
	return;
}
