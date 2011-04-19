/* Portions Copyright (C) 2001 artofcode LLC.
   Portions Copyright (C) 1996, 2001 Artifex Software Inc.
   Portions Copyright (C) 1988, 2000 Aladdin Enterprises.
   This software is based in part on the work of the Independent JPEG Group.
   All Rights Reserved.

   This software is distributed under license and may not be copied, modified
   or distributed except as expressly authorized under the terms of that
   license.  Refer to licensing information at http://www.artifex.com/ or
   contact Artifex Software, Inc., 101 Lucas Valley Road #110,
   San Rafael, CA  94903, (415)492-9861, for further information. */
/*$Id$ */

/* pggeom.c */
/* HP-GL/2 geometry routines */

#include "stdio_.h"
#include "pggeom.h"
#include "gxfarith.h" /* for gs_sincos */

/* HAS most of these computations require more error checking */

/* ------ Lines, angles, arcs, and chords ------ */

/* compute the angle between 0 and 2*PI given the slope */
floatp
hpgl_compute_angle(floatp dx, floatp dy)
{
        floatp alpha = atan2(dy, dx);

        return (alpha < 0 ? alpha + M_PI * 2.0 : alpha);
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

/* compute the coordinates of a point on an arc */
int
hpgl_compute_arc_coords(floatp radius, floatp center_x, floatp center_y,
                        floatp angle, floatp *px, floatp *py)
{
        gs_sincos_t sincos;
        gs_sincos_degrees(angle, &sincos);
        *px = radius * sincos.cos + center_x;
        *py = radius * sincos.sin + center_y;
        return 0;
}

/* given a start point, angle (degrees) and magnitude of a vector compute its
   endpoints */
int
hpgl_compute_vector_endpoints(floatp magnitude, floatp x, floatp y,
                              floatp angle_degrees, floatp *endx, floatp *endy)

{
        return hpgl_compute_arc_coords(magnitude, x, y,
                                       angle_degrees, endx, endy);
}
