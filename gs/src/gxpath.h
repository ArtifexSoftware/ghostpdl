/* Copyright (C) 1989, 1995, 1996, 1997 Aladdin Enterprises.  All rights reserved.
  
  This file is part of Aladdin Ghostscript.
  
  Aladdin Ghostscript is distributed with NO WARRANTY OF ANY KIND.  No author
  or distributor accepts any responsibility for the consequences of using it,
  or for whether it serves any particular purpose or works at all, unless he
  or she says so in writing.  Refer to the Aladdin Ghostscript Free Public
  License (the "License") for full details.
  
  Every copy of Aladdin Ghostscript must include a copy of the License,
  normally in a plain ASCII text file named PUBLIC.  The License grants you
  the right to copy, modify and redistribute Aladdin Ghostscript, but only
  under certain conditions described in the License.  Among other things, the
  License requires that the copyright notice and this notice be preserved on
  all copies.
*/

/* gxpath.h */
/* Lower-level path routines for Ghostscript library */
/* Requires gxfixed.h */
#include "gscpm.h"
#include "gslparam.h"
#include "gspenum.h"

/* The routines and types in this interface use */
/* device, rather than user, coordinates, and fixed-point, */
/* rather than floating, representation. */

/* Opaque type for a path */
#ifndef gx_path_DEFINED
#  define gx_path_DEFINED
typedef struct gx_path_s gx_path;
#endif

/* Define the two insideness rules */
#define gx_rule_winding_number (-1)
#define gx_rule_even_odd 1

/* Define 'notes' that describe the role of a path segment. */
/* These are only for internal use; a normal segment's notes are 0. */
typedef enum {
  sn_none = 0,
  sn_not_first = 1,		/* segment is in curve/arc and not first */
  sn_from_arc = 2		/* segment is part of an arc */
} segment_notes;

/* Debugging routines */
#ifdef DEBUG
void gx_dump_path(P2(const gx_path *, const char *));
void gx_path_print(P1(const gx_path *));
#endif

/* Path constructors */

gx_path *gx_path_alloc(P2(gs_memory_t *, client_name_t));
void	gx_path_init(P2(gx_path *, gs_memory_t *)),
	gx_path_reset(P1(gx_path *)),
	gx_path_release(P1(gx_path *)),
	gx_path_share(P1(gx_path *));
int	gx_path_add_point(P3(gx_path *, fixed, fixed)),
	gx_path_add_relative_point(P3(gx_path *, fixed, fixed)),
	gx_path_add_line_notes(P4(gx_path *, fixed, fixed, segment_notes)),
	gx_path_add_lines_notes(P4(gx_path *, const gs_fixed_point *, int, segment_notes)),
	gx_path_add_rectangle(P5(gx_path *, fixed, fixed, fixed, fixed)),
	gx_path_add_char_path(P3(gx_path *, gx_path *, gs_char_path_mode)),
	gx_path_add_curve_notes(P8(gx_path *, fixed, fixed, fixed, fixed, fixed, fixed, segment_notes)),
	gx_path_add_partial_arc_notes(P7(gx_path *, fixed, fixed, fixed, fixed, floatp, segment_notes)),
	gx_path_add_path(P2(gx_path *, gx_path *)),
	gx_path_close_subpath_notes(P2(gx_path *, segment_notes)),
	  /* We have to remove the 'subpath' from the following name */
	  /* to keep it unique in the first 23 characters. */
	gx_path_pop_close_notes(P2(gx_path *, segment_notes));
/* The last argument to gx_path_add_partial_arc is a fraction for computing */
/* the curve parameters.  Here is the correct value for quarter-circles. */
/* (stroke uses this to draw round caps and joins.) */
#define quarter_arc_fraction 0.552285
/*
 * Backward-compatible constructors that don't take a notes argument.
 */
#define gx_path_add_line(ppath, x, y)\
  gx_path_add_line_notes(ppath, x, y, sn_none)
#define gx_path_add_lines(ppath, pts, count)\
  gx_path_add_lines_notes(ppath, pts, count, sn_none)
#define gx_path_add_curve(ppath, x1, y1, x2, y2, x3, y3)\
  gx_path_add_curve_notes(ppath, x1, y1, x2, y2, x3, y3, sn_none)
#define gx_path_add_partial_arc(ppath, x3, y3, xt, yt, fraction)\
  gx_path_add_partial_arc_notes(ppath, x3, y3, xt, yt, fraction, sn_none)
#define gx_path_close_subpath(ppath)\
  gx_path_close_subpath_notes(ppath, sn_none)
#define gx_path_pop_close_subpath(ppath)\
  gx_path_pop_close_notes(ppath, sn_none)

/* Path accessors */

gx_path *gx_current_path(P1(const gs_state *));
void	gx_path_assign(P2(gx_path *, const gx_path *));
int	gx_path_current_point(P2(const gx_path *, gs_fixed_point *)),
	gx_path_bbox(P2(gx_path *, gs_fixed_rect *));
bool	gx_path_has_curves(P1(const gx_path *)),
	gx_path_is_void(P1(const gx_path *)),	/* no segments */
	gx_path_is_null(P1(const gx_path *)),	/* nothing at all */
	gx_path_is_rectangle(P2(const gx_path *, gs_fixed_rect *)),
	gx_path_is_monotonic(P1(const gx_path *));
/* Inline versions of the above */
#define gx_path_has_curves_inline(ppath)\
  ((ppath)->curve_count != 0)
#define gx_path_is_void_inline(ppath)\
  ((ppath)->first_subpath == 0)
#define gx_path_is_null_inline(ppath)\
  (gx_path_is_void_inline(ppath) && !path_position_valid(ppath))

/* Path transformers */

/* gx_path_copy_reducing is internal. */
typedef enum {
  pco_none = 0,
  pco_init = 1,			/* initialize the destination path */
  pco_monotonize = 2,		/* make curves monotonic */
  pco_accurate = 4		/* flatten with accurate tangents at ends */
} gx_path_copy_options;
int	gx_path_copy_reducing(P4(const gx_path *ppath_old, gx_path *ppath_new,
				 fixed fixed_flatness,
				 gx_path_copy_options options));
#define gx_path_copy(old, new, init)\
  gx_path_copy_reducing(old, new, max_fixed, (init ? pco_init : pco_none))
#define gx_path_flatten(old, new, flatness)\
  gx_path_copy_reducing(old, new, float2fixed(flatness), pco_init)
#define gx_path_flatten_accurate(old, new, flatness, accurate)\
  gx_path_copy_reducing(old, new, float2fixed(flatness),\
			(accurate ? pco_init | pco_accurate : pco_init))
#define gx_path_monotonize(old, new)\
  gx_path_copy_reducing(old, new, max_fixed, pco_init | pco_monotonize)
int	gx_path_expand_dashes(P3(const gx_path * /*old*/, gx_path * /*new*/, const gs_imager_state *)),
	gx_path_copy_reversed(P3(const gx_path * /*old*/, gx_path * /*new*/, bool /*init*/)),
	gx_path_translate(P3(gx_path *, fixed, fixed)),
	gx_path_scale_exp2(P3(gx_path *, int, int));
void	gx_point_scale_exp2(P3(gs_fixed_point *, int, int)),
	gx_rect_scale_exp2(P3(gs_fixed_rect *, int, int));

/* Path enumerator */

/* This interface does not make a copy of the path. */
/* Do not use gs_path_enum_cleanup with this interface! */
int	gx_path_enum_init(P2(gs_path_enum *, const gx_path *));
int	gx_path_enum_next(P2(gs_path_enum *, gs_fixed_point [3])); /* 0 when done */
segment_notes
	gx_path_enum_notes(P1(const gs_path_enum *));
bool	gx_path_enum_backup(P1(gs_path_enum *));

/* ------ Clipping paths ------ */

/* Opaque type for a clipping path */
#ifndef gx_clip_path_DEFINED
#  define gx_clip_path_DEFINED
typedef struct gx_clip_path_s gx_clip_path;
#endif

/* Opaque type for a clip list. */
#ifndef gx_clip_list_DEFINED
#  define gx_clip_list_DEFINED
typedef struct gx_clip_list_s gx_clip_list;
#endif

int	gx_clip_to_rectangle(P2(gs_state *, gs_fixed_rect *)),
	gx_clip_to_path(P1(gs_state *)),
	gx_cpath_init(P2(gx_clip_path *, gs_memory_t *)),
	gx_cpath_from_rectangle(P3(gx_clip_path *, gs_fixed_rect *, gs_memory_t *)),
	gx_cpath_intersect(P4(gs_state *, gx_clip_path *, gx_path *, int)),
	gx_cpath_scale_exp2(P3(gx_clip_path *, int, int));
void	gx_cpath_release(P1(gx_clip_path *)),
	gx_cpath_share(P1(gx_clip_path *));
int	gx_cpath_path(P2(gx_clip_path *, gx_path *));
bool	gx_cpath_inner_box(P2(const gx_clip_path *, gs_fixed_rect *)),
	gx_cpath_outer_box(P2(const gx_clip_path *, gs_fixed_rect *)),
	gx_cpath_includes_rectangle(P5(const gx_clip_path *, fixed, fixed, fixed, fixed));
int	gx_cpath_set_outside(P2(gx_clip_path *, bool));
bool	gx_cpath_is_outside(P1(const gx_clip_path *));

/* ------ Rectangle utilities ------ */

/* Check whether a path bounding box is within a clipping box. */
#define rect_within(ibox, cbox)\
  (ibox.q.y <= cbox.q.y && ibox.q.x <= cbox.q.x &&\
   ibox.p.y >= cbox.p.y && ibox.p.x >= cbox.p.x)

/* Intersect a bounding box with a clipping box. */
#define rect_intersect(ibox, cbox)\
  { if ( cbox.p.x > ibox.p.x ) ibox.p.x = cbox.p.x;\
    if ( cbox.q.x < ibox.q.x ) ibox.q.x = cbox.q.x;\
    if ( cbox.p.y > ibox.p.y ) ibox.p.y = cbox.p.y;\
    if ( cbox.q.y < ibox.q.y ) ibox.q.y = cbox.q.y;\
  }
