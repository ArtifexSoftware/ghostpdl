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

/* pccoord.h - centipoint coordinate structures for PCL */

#ifndef pccoord_INCLUDED
#define pccoord_INCLUDED

/*
 * Following the PCL documentation, we represent coordinates internally in
 * centipoints (1/7200").
 */
#if arch_sizeof_int == 2
typedef long    coord;
#else
typedef int     coord;
#endif

#define pcl_coord_scale 7200
#define inch2coord(v)   ((coord)((v) * (coord)pcl_coord_scale))
#define coord2inch(c)   ((c) / (float)pcl_coord_scale)

typedef struct coord_point_s {
    coord   x, y;
} coord_point_t;

#endif			/* pccoord_INCLUDED */
