/* Copyright (C) 1996, 1997, 1998 Aladdin Enterprises.  All rights
   reserved.  Unauthorized use, copying, and/or distribution
   prohibited.  */

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
