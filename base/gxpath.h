/* Copyright (C) 2001-2025 Artifex Software, Inc.
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


/* Fixed-point path procedures */
/* Requires gxfixed.h */

#ifndef gxpath_INCLUDED
#  define gxpath_INCLUDED

#include "gscpm.h"
#include "gslparam.h"
#include "gspenum.h"
#include "gsrect.h"
#include "gsgstate.h" /* For stroke flattening */

/* The routines and types in this interface use */
/* device, rather than user, coordinates, and fixed-point, */
/* rather than floating, representation. */

/* Define the two insideness rules */
#define gx_rule_winding_number (-1)
#define gx_rule_even_odd 1

/* Define 'notes' that describe the role of a path segment. */
/* These are only for internal use; a normal segment's notes are 0. */
typedef enum {
    sn_none      = 0,
    sn_not_first = 1,           /* segment is in curve/arc and not first */
    sn_from_arc  = 2,           /* segment is part of an arc             */
    sn_dash_head = 4,           /* segment follows a dash break          */
    sn_dash_tail = 8,           /* segment is followed by dash break     */
} segment_notes;

/*
 * Used by interpreters for optimizing bbox size for transparency groups.
 * Depending upon the option we will return a bbox that may just included the
 * clip path or may include the intersection of the current path and the clip
 * path and may include an adjustement for the stroke width.
*/
typedef enum {
    PATH_FILL,
    PATH_STROKE,
    NO_PATH
} gs_bbox_comp_t;

/* Debugging routines */
#ifdef DEBUG
void gx_dump_path(const gx_path *, const char *);
void gx_path_print(const gx_path *);
#endif

/* Path memory management */

/*
 * Path memory management is unfortunately a little tricky.  The
 * implementation details are in gzpath.h: we only present the API here.
 *
 * Path objects per se may be allocated in 3 different ways: on the
 * C stack, as separate objects in the heap, or (for the graphics state
 * only) contained in a larger heap-allocated object.
 *
 * Any number of paths may share segments.  The segments are stored in
 * their own, reference-counted object, and are freed when there are no
 * more references to that object.
 */

/*
 * Allocate a path on the heap, and initialize it.  If shared is NULL,
 * allocate a segments object; if shared is an existing path, share its
 * segments.
 */
gx_path *gx_path_alloc_shared(const gx_path * shared, gs_memory_t * mem,
                              client_name_t cname);

#define gx_path_alloc(mem, cname)\
  gx_path_alloc_shared(NULL, mem, cname)
/*
 * Initialize a path contained in an already-heap-allocated object,
 * optionally allocating its segments.
 */
int gx_path_init_contained_shared(gx_path * ppath, const gx_path * shared,
                                  gs_memory_t * mem, client_name_t cname);

#define gx_path_alloc_contained(ppath, mem, cname)\
  gx_path_init_contained_shared(ppath, NULL, mem, cname)
/*
 * Initialize a stack-allocated path.  This doesn't allocate anything,
 * but may still share the segments.  Note that it returns an error if
 * asked to share the segments of another local path.
 */
int gx_path_init_local_shared(gx_path * ppath, const gx_path * shared,
                              gs_memory_t * mem);

#define gx_path_init_local(ppath, mem)\
  (void)gx_path_init_local_shared(ppath, NULL, mem)	/* can't fail */

/* Special case constructors for paths that will only ever consist of a
 * single rectangle. Used for speed. No destruction required. The
 * memory pointer is only for callers further down the chain. */
void gx_path_preinit_local_rectangle(gx_path *ppath, gs_memory_t *mem);
void gx_path_init_local_rectangle(gx_path *ppath, gs_fixed_rect *rect);

/*
 * Initialize a stack-allocated pseudo-path for computing a bbox
 * for a dynamic path.
 *
 * Note: This bbox will always contain the origin.
 */
void gx_path_init_bbox_accumulator(gx_path * ppath);

/*
 * Ensure that a path owns its segments, by copying the segments if
 * they currently have multiple references.
 */
int gx_path_unshare(gx_path * ppath);

/*
 * Free a path by releasing its segments if they have no more references.
 * This also frees the path object iff it was allocated by gx_path_alloc.
 */
void gx_path_free(gx_path * ppath, client_name_t cname);

/*
 * Assign one path to another, adjusting reference counts appropriately.
 * Note that this requires that segments of the two paths (but not the path
 * objects themselves) were allocated with the same allocator.  Note also
 * that if ppfrom is stack-allocated, ppto is not, and ppto's segments are
 * currently shared, gx_path_assign must do the equivalent of a
 * gx_path_new(ppto), which allocates a new segments object for ppto.
 */
int gx_path_assign_preserve(gx_path * ppto, gx_path * ppfrom);

/*
 * Assign one path to another and free the first path at the same time.
 * (This may do less work than assign_preserve + free.)
 */
int gx_path_assign_free(gx_path * ppto, gx_path * ppfrom);

/* Path constructors */
/* Note that all path constructors have an implicit initial gx_path_unshare. */

int gx_path_new(gx_path *),
    gx_path_add_point(gx_path *, fixed, fixed),
    gx_path_add_relative_point(gx_path *, fixed, fixed),
    gx_path_add_line_notes(gx_path *, fixed, fixed, segment_notes),
    gx_path_add_gap_notes(gx_path *, fixed, fixed, segment_notes),
    gx_path_add_dash_notes(gx_path * ppath, fixed x, fixed y, fixed dx, fixed dy, segment_notes notes),
    gx_path_add_lines_notes(gx_path *, const gs_fixed_point *, int, segment_notes),
    gx_path_add_rectangle(gx_path *, fixed, fixed, fixed, fixed),
    gx_path_add_char_path(gx_path *, gx_path *, gs_char_path_mode),
    gx_path_add_curve_notes(gx_path *, fixed, fixed, fixed, fixed, fixed, fixed, segment_notes),
    gx_path_add_partial_arc_notes(gx_path *, fixed, fixed, fixed, fixed, double, segment_notes),
    gx_path_add_path(gx_path *, gx_path *),
    gx_path_close_subpath_notes(gx_path *, segment_notes),
          /* We have to remove the 'subpath' from the following name */
          /* to keep it unique in the first 23 characters. */
    gx_path_pop_close_notes(gx_path *, segment_notes);

/* Access path state flags */
byte gx_path_get_state_flags(gx_path *ppath);
void gx_path_set_state_flags(gx_path *ppath, byte flags);
bool gx_path_is_drawing(gx_path *ppath);

/*
 * The last argument to gx_path_add_partial_arc is a fraction for computing
 * the curve parameters.  Here is the correct value for quarter-circles.
 * (stroke uses this to draw round caps and joins.)
 */
#define quarter_arc_fraction 0.55228474983079334
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

typedef enum {
    pco_none = 0,
    pco_monotonize = 1,		/* make curves monotonic */
    pco_accurate = 2,		/* flatten with accurate tangents at ends */
    pco_for_stroke = 4,		/* flatten taking line width into account */
    pco_small_curves = 8	/* make curves small */
} gx_path_copy_options;

/* Path accessors */

gx_path *gx_current_path(const gs_gstate *);
bool gx_path_position_valid(const gx_path *ppath);
int gx_path_current_point(const gx_path *, gs_fixed_point *),
    gx_path_bbox(gx_path *, gs_fixed_rect *),
    gx_path_bbox_set(gx_path *, gs_fixed_rect *);
int gx_path_subpath_start_point(const gx_path *, gs_fixed_point *);
bool gx_path_has_curves(const gx_path *),
    gx_path_is_void(const gx_path *),	/* no segments */
    gx_path_is_null(const gx_path *),	/* nothing at all */
    gx_path__check_curves(const gx_path * ppath, gx_path_copy_options options, fixed fixed_flat),
    gx_path_has_long_segments(const gx_path * ppath);
typedef enum {
    prt_none = 0,
    prt_open = 1,		/* only 3 sides */
    prt_fake_closed = 2,	/* 4 lines, no closepath */
    prt_closed = 3		/* 3 or 4 lines + closepath */
} gx_path_rectangular_type;

gx_path_rectangular_type
gx_path_is_rectangular(const gx_path *, gs_fixed_rect *);

#define gx_path_is_rectangle(ppath, pbox)\
  (gx_path_is_rectangular(ppath, pbox) != prt_none)
/* Inline versions of the above */
#define gx_path_is_null_inline(ppath)\
  (gx_path_is_void(ppath) && !path_position_valid(ppath))

/* Macro to test if a curve is actually a line (i.e. that
 * both control points are at one end or the other).
 * (Yes, there are cases where the curve can be a line
 * where this does not apply, but they are not trivial to
 * to test for)
 */
#define gx_curve_is_really_line(x0, y0, seg)            \
    (((((curve_segment *)seg)->p1.x == x0 &&            \
       ((curve_segment *)seg)->p1.y == y0) ||           \
      (((curve_segment *)seg)->p1.x == (seg)->pt.x &&   \
       ((curve_segment *)seg)->p1.y == (seg)->pt.y)) && \
     ((((curve_segment *)seg)->p2.x == x0 &&            \
       ((curve_segment *)seg)->p2.y == y0) ||           \
      (((curve_segment *)seg)->p2.x == (seg)->pt.x &&   \
       ((curve_segment *)seg)->p2.y == (seg)->pt.y)))

/* Macro to test if a curve is actually a point (i.e. that
 * start/end and both control points are coincident).
 */
#define gx_curve_is_really_point(x0, y0, seg)           \
    (x0 == (seg)->pt.y && y0 == (seg)->pt.y &&          \
     ((curve_segment *)seg)->p1.x == x0 &&              \
     ((curve_segment *)seg)->p1.y == y0 &&              \
     ((curve_segment *)seg)->p2.x == x0 &&              \
     ((curve_segment *)seg)->p2.y == y0)

/* Path transformers */

/* The gs_gstate is only needed when flattening for stroke. */
int gx_path_copy_reducing(const gx_path * ppath_old, gx_path * ppath_new,
                          fixed fixed_flatness, const gs_gstate *pgs,
                          gx_path_copy_options options);

#define gx_path_copy(old, new)\
  gx_path_copy_reducing(old, new, max_fixed, NULL, pco_none)
#define gx_path_add_flattened_accurate(old, new, flatness, accurate)\
  gx_path_copy_reducing(old, new, float2fixed(flatness), NULL,\
                        (accurate ? pco_accurate : pco_none))
#define gx_path_add_flattened_for_stroke(old, new, flatness, pgs)\
  gx_path_copy_reducing(old, new, float2fixed(flatness), pgs,\
                        (pgs->accurate_curves ?\
                         pco_accurate | pco_for_stroke : pco_for_stroke))
int gx_path_add_dash_expansion(const gx_path * /*old*/, gx_path * /*new*/,
                                  const gs_gstate *),
      gx_path_copy_reversed(const gx_path * /*old*/, gx_path * /*new*/),
      gx_path_append_reversed(const gx_path * /*orig*/, gx_path * /*rev*/),
      gx_path_translate(gx_path *, fixed, fixed),
      gx_path_scale_exp2_shared(gx_path *ppath, int log2_scale_x,
                                   int log2_scale_y, bool segments_shared);
void gx_point_scale_exp2(gs_fixed_point *, int, int),
      gx_rect_scale_exp2(gs_fixed_rect *, int, int);

int gx_path_elide_1d(gx_path *ppath);

/* Path enumerator */

/* This interface does not make a copy of the path. */
/* Do not use gs_path_enum_cleanup with this interface! */
int gx_path_enum_init(gs_path_enum *, const gx_path *);
int gx_path_enum_next(gs_path_enum *, gs_fixed_point[3]);	/* 0 when done */

segment_notes
gx_path_enum_notes(const gs_path_enum *);
bool gx_path_enum_backup(gs_path_enum *);

/* An auxiliary function to add a path point with a specified transformation. */
int gs_moveto_aux(gs_gstate *pgs, gx_path *ppath, double x, double y);
int gx_setcurrentpoint_from_path(gs_gstate *pgs, gx_path *path);

/* Path optimization for the filling algorithm. */

int gx_path_merge_contacting_contours(gx_path *ppath);

/* ------ Clipping paths ------ */

/* Opaque type for a clipping path */
typedef struct gx_clip_path_s gx_clip_path;

/* Graphics state clipping */
int gx_clip_to_rectangle(gs_gstate *, gs_fixed_rect *);
int gx_clip_to_path(gs_gstate *);
int gx_default_clip_box(const gs_gstate *, gs_fixed_rect *);
int gx_effective_clip_path(gs_gstate *, gx_clip_path **);
int gx_curr_fixed_bbox(gs_gstate * pgs, gs_fixed_rect *bbox, gs_bbox_comp_t comp_type);
int gx_curr_bbox(gs_gstate * pgs, gs_rect *bbox, gs_bbox_comp_t comp_type);


/* Opaque type for a clip list. */
typedef struct gx_clip_list_s gx_clip_list;


/* We need abstract types for paths and fill/stroke parameters, */
/* for the path-oriented device procedures. */
typedef struct gx_fill_params_s gx_fill_params;
typedef struct gx_stroke_params_s gx_stroke_params;
typedef struct patch_fill_state_s  patch_fill_state_t;

/* Opaque type for a clipping path enumerator. */
typedef struct gs_cpath_enum_s gs_cpath_enum;

/*
 * Provide similar memory management for clip paths to what we have for
 * paths (see above for details).
 */
gx_clip_path *gx_cpath_alloc_shared(const gx_clip_path * shared,
                                    gs_memory_t * mem, client_name_t cname);

#define gx_cpath_alloc(mem, cname)\
  gx_cpath_alloc_shared(NULL, mem, cname)
int gx_cpath_init_contained_shared(gx_clip_path * pcpath,
                                   const gx_clip_path * shared,
                                   gs_memory_t * mem,
                                   client_name_t cname);

#define gx_cpath_alloc_contained(pcpath, mem, cname)\
  gx_cpath_init_contained_shared(pcpath, NULL, mem, cname)
int gx_cpath_init_local_shared(gx_clip_path * pcpath,
                               const gx_clip_path * shared,
                               gs_memory_t * mem);
/* Function that informs us that the usage of this cpath will be
 * safely nested within the existence of the 'shared' one. i.e.
 * we don't need to worry that the shared one may go away while
 * we contain pointers to it.
 */
int gx_cpath_init_local_shared_nested(gx_clip_path * pcpath,
                                      const gx_clip_path * shared,
                                      gs_memory_t * mem,
                                      bool safely_nested);

#define gx_cpath_init_local(pcpath, mem)\
  (void)gx_cpath_init_local_shared(pcpath, NULL, mem)	/* can't fail */

/* Special case constructors for clip paths that will only ever consist of a
 * single rectangle. Used for speed. No destruction required. The mem pointer
 * is only used by callers further down the chain needing such. */
void gx_cpath_preinit_local_rectangle(gx_clip_path *pcpath, gs_memory_t *mem);
void gx_cpath_init_local_rectangle(gx_clip_path *pcpath, gs_fixed_rect *r, gs_id id);

int gx_cpath_unshare(gx_clip_path * pcpath);
void gx_cpath_free(gx_clip_path * pcpath, client_name_t cname);
int gx_cpath_assign_preserve(gx_clip_path * pcpto, gx_clip_path * pcpfrom);
int gx_cpath_assign_free(gx_clip_path * pcpto, gx_clip_path * pcpfrom);

/* Clip path constructors and accessors */

int
    gx_cpath_reset(gx_clip_path *),		/* from_rectangle ((0,0),(0,0)) */
    gx_cpath_from_rectangle(gx_clip_path *, gs_fixed_rect *),
    gx_cpath_clip(gs_gstate *, gx_clip_path *, /*const*/ gx_path *, int),
    gx_cpath_intersect(gx_clip_path *, /*const*/ gx_path *, int,
                       gs_gstate *),
    gx_cpath_intersect_with_params(gx_clip_path *pcpath, /*const*/ gx_path *ppath_orig,
                   int rule, gs_gstate *pgs, const gx_fill_params * params),
    gx_cpath_scale_exp2_shared(gx_clip_path *pcpath, int log2_scale_x,
                               int log2_scale_y, bool list_shared,
                               bool segments_shared),
    gx_cpath_to_path(gx_clip_path *, gx_path *),
    gx_cpath_to_path_synthesize(const gx_clip_path * pcpath, gx_path * ppath);
int gx_cpath_ensure_path_list(gx_clip_path *pcpath);
bool
    gx_cpath_inner_box(const gx_clip_path *, gs_fixed_rect *),
    gx_cpath_outer_box(const gx_clip_path *, gs_fixed_rect *),
    gx_cpath_includes_rectangle(const gx_clip_path *, fixed, fixed,
                                fixed, fixed);
gx_path_rectangular_type cpath_is_rectangle(const gx_clip_path * pcpath, gs_fixed_rect *rect);
bool gx_cpath_rect_visible(gx_clip_path * pcpath, gs_int_rect *prect);
int gx_cpath_copy(const gx_clip_path * from, gx_clip_path * pcpath);

/* Enumerate a clipping path.  This interface does not copy the path. */
/* However, it does write into the path's "visited" flags. */
int gx_cpath_enum_init(gs_cpath_enum *, const gx_clip_path *);
int gx_cpath_enum_next(gs_cpath_enum *, gs_fixed_point[3]);		/* 0 when done */

segment_notes
gx_cpath_enum_notes(const gs_cpath_enum *);

#ifdef DEBUG
void gx_cpath_print(const gs_memory_t *mem, const gx_clip_path *);
#endif

#endif /* gxpath_INCLUDED */
