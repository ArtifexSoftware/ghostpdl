/*
 * Copyright (C) 1998 Aladdin Enterprises.
 * All rights reserved.
 *
 * This file is part of Aladdin Ghostscript.
 *
 * Aladdin Ghostscript is distributed with NO WARRANTY OF ANY KIND.  No author
 * or distributor accepts any responsibility for the consequences of using it,
 * or for whether it serves any particular purpose or works at all, unless he
 * or she says so in writing.  Refer to the Aladdin Ghostscript Free Public
 * License (the "License") for full details.
 *
 * Every copy of Aladdin Ghostscript must include a copy of the License,
 * normally in a plain ASCII text file named PUBLIC.  The License grants you
 * the right to copy, modify and redistribute Aladdin Ghostscript, but only
 * under certain conditions described in the License.  Among other things, the
 * License requires that the copyright notice and this notice be preserved on
 * all copies.
 */

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
} coord_point;

/*
 * The current PCL addressable position (in pcursor.c). This is NOT part of
 * the PCL state, though it previously was implmented as such. For this
 * reason, it is declared here.
 *
 * This point is in "pseudo print direction" space.
 */
extern  coord_point pcl_cap;

#endif			/* pccoord_INCLUDED */
