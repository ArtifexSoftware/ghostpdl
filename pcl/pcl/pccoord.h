/* Copyright (C) 2001-2021 Artifex Software, Inc.
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


/* pccoord.h - centipoint coordinate structures for PCL */

#ifndef pccoord_INCLUDED
#define pccoord_INCLUDED

/*
 * Following the PCL documentation, we represent coordinates internally in
 * centipoints (1/7200").
 */
#if ARCH_SIZEOF_INT == 2
typedef long coord;
#else
typedef int coord;
#endif

#define pcl_coord_scale 7200
#define inch2coord(v)   ((coord)((v) * (coord)pcl_coord_scale))
#define coord2inch(c)   ((c) / (float)pcl_coord_scale)

typedef struct coord_point_s
{
    coord x, y;
} coord_point_t;

#endif /* pccoord_INCLUDED */
