/* Copyright (C) 2001-2023 Artifex Software, Inc.
   All Rights Reserved.

   This software is provided AS-IS with no warranty, either express or
   implied.

   This software is distributed under license and may not be copied,
   modified or distributed except as expressly authorized under the terms
   of the license contained in the file LICENSE in this distribution.

   Refer to licensing information at http://www.artifex.com or contact
   Artifex Software, Inc.,  39 Mesa Street, Suite 108A, San Francisco,
   CA 94129, USA, for further information.
*/


/* Graphics state path procedures */
/* Requires gsstate.h */

#ifndef gspath_INCLUDED
#  define gspath_INCLUDED

#include "std.h"
#include "gstypes.h"
#include "gspenum.h"
#include "gsgstate.h"
#include "gxmatrix.h"

/*
 * Define clamped values for out-of-range coordinates.
 * Currently the path drawing routines can't handle values
 * close to the edge of the representable space.
 */
#define max_coord_fixed (max_fixed - int2fixed(1000))	/* arbitrary */
#define min_coord_fixed (-max_coord_fixed)
#define clamp_coord(xy)\
    (xy > fixed2float(max_coord_fixed) ? max_coord_fixed :\
     xy < fixed2float(min_coord_fixed) ? min_coord_fixed :\
     float2fixed(xy))

/* Path constructors */
int gs_newpath(gs_gstate *),
    gs_moveto(gs_gstate *, double, double),
    gs_rmoveto(gs_gstate *, double, double),
    gs_lineto(gs_gstate *, double, double),
    gs_rlineto(gs_gstate *, double, double),
    gs_arc(gs_gstate *, double, double, double, double, double),
    gs_arcn(gs_gstate *, double, double, double, double, double),
    /*
     * Because of an obscure bug in the IBM RS/6000 compiler, one (but not
     * both) bool argument(s) for gs_arc_add must come before the double
     * arguments.
     */
    gs_arc_add(gs_gstate *, bool, double, double, double, double, double, bool),
    gs_arcto(gs_gstate *, double, double, double, double, double, float[4]),
    gs_curveto(gs_gstate *, double, double, double, double, double, double),
    gs_rcurveto(gs_gstate *, double, double, double, double, double, double),
    gs_closepath(gs_gstate *);

/* gs_gstate-level procedures */
void make_quadrant_arc(gs_point *p, const gs_point *c,
        const gs_point *p0, const gs_point *p1, double r);

/* Add the current path to the path in the previous graphics state. */
int gs_upmergepath(gs_gstate *);

/* Path accessors and transformers */
int gs_currentpoint(gs_gstate *, gs_point *),
      gs_upathbbox(gs_gstate *, gs_rect *, bool),
      gs_dashpath(gs_gstate *),
      gs_flattenpath(gs_gstate *),
      gs_reversepath(gs_gstate *),
      gs_strokepath(gs_gstate *),
      gs_strokepath2(gs_gstate *);

/* The extra argument for gs_upathbbox controls whether to include */
/* a trailing moveto in the bounding box. */
#define gs_pathbbox(pgs, prect)\
  gs_upathbbox(pgs, prect, false)

/* Path enumeration */

/* This interface conditionally makes a copy of the path. */
gs_path_enum *gs_path_enum_alloc(gs_memory_t *, client_name_t);
int gs_path_enum_copy_init(gs_memory_t *mem, gs_path_enum *, const gs_gstate *, bool);

#define gs_path_enum_init(mem, penum, pgs)\
  gs_path_enum_copy_init(mem, penum, pgs, true)
int gs_path_enum_next(gs_path_enum *, gs_point[3]);  /* 0 when done */
void gs_path_enum_cleanup(gs_path_enum *);

/* Clipping */
int gs_clippath(gs_gstate *),
    gs_initclip(gs_gstate *),
    gs_clip(gs_gstate *),
    gs_eoclip(gs_gstate *);

#endif /* gspath_INCLUDED */
