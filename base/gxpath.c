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


/* Internal path management routines for Ghostscript library */
#include "gx.h"
#include "gserrors.h"
#include "gsstruct.h"
#include "gxfixed.h"
#include "gzpath.h"

/* These routines all assume that all points are */
/* already in device coordinates, and in fixed representation. */
/* As usual, they return either 0 or a (negative) error code. */

/* Forward references */
static int path_alloc_copy(gx_path *);
static int gx_path_new_subpath(gx_path *);

#ifdef DEBUG
static void gx_print_segment(const gs_memory_t *,const segment *);

#  define trace_segment(msg, mem, pseg)\
     if ( gs_debug_c('P') ) dmlprintf(mem, msg), gx_print_segment(mem,pseg);
#else
#  define trace_segment(msg, mem, pseg) DO_NOTHING
#endif

/* Check a point against a preset bounding box. */
#define outside_bbox(ppath, px, py)\
 (px < ppath->bbox.p.x || px > ppath->bbox.q.x ||\
  py < ppath->bbox.p.y || py > ppath->bbox.q.y)
#define check_in_bbox(ppath, px, py)\
  if ( outside_bbox(ppath, px, py) )\
        return_error(gs_error_rangecheck)

/* Structure descriptors for paths and path segment types. */
public_st_path();
private_st_path_segments();
private_st_segment();
private_st_line();
private_st_dash();
private_st_line_close();
private_st_curve();
private_st_subpath();

/* ------ Initialize/free paths ------ */

static rc_free_proc(rc_free_path_segments);
static rc_free_proc(rc_free_path_segments_local);

/*
 * Define the default virtual path interface implementation.
 */
static int
    gz_path_add_point(gx_path *, fixed, fixed),
    gz_path_add_line_notes(gx_path *, fixed, fixed, segment_notes),
    gz_path_add_gap_notes(gx_path *, fixed, fixed, segment_notes),
    gz_path_add_curve_notes(gx_path *, fixed, fixed, fixed, fixed, fixed, fixed, segment_notes),
    gz_path_close_subpath_notes(gx_path *, segment_notes);
static byte gz_path_state_flags(gx_path *ppath, byte flags);

static gx_path_procs default_path_procs = {
    gz_path_add_point,
    gz_path_add_line_notes,
    gz_path_add_gap_notes,
    gz_path_add_curve_notes,
    gz_path_close_subpath_notes,
    gz_path_state_flags
};

/*
 * Define virtual path interface implementation for computing a path bbox.
 */
static int
    gz_path_bbox_add_point(gx_path *, fixed, fixed),
    gz_path_bbox_add_line_notes(gx_path *, fixed, fixed, segment_notes),
    gz_path_bbox_add_gap_notes(gx_path *, fixed, fixed, segment_notes),
    gz_path_bbox_add_curve_notes(gx_path *, fixed, fixed, fixed, fixed, fixed, fixed, segment_notes),
    gz_path_bbox_close_subpath_notes(gx_path *, segment_notes);

static gx_path_procs path_bbox_procs = {
    gz_path_bbox_add_point,
    gz_path_bbox_add_line_notes,
    gz_path_bbox_add_gap_notes,
    gz_path_bbox_add_curve_notes,
    gz_path_bbox_close_subpath_notes,
    gz_path_state_flags
};

static void
gx_path_init_contents(gx_path * ppath)
{
    ppath->box_last = 0;
    ppath->first_subpath = ppath->current_subpath = 0;
    ppath->subpath_count = 0;
    ppath->curve_count = 0;
    path_update_newpath(ppath);
    ppath->bbox_set = 0;
    ppath->bbox_accurate = 0;
    ppath->last_charpath_segment = 0;
    ppath->bbox.p.x = max_fixed;
    ppath->bbox.p.y = max_fixed;
    ppath->bbox.q.x = min_fixed;
    ppath->bbox.q.y = min_fixed;
}

/*
 * Initialize a path contained in an already-heap-allocated object,
 * optionally allocating its segments.
 */
static int
path_alloc_segments(gx_path_segments ** ppsegs, gs_memory_t * mem,
                    client_name_t cname)
{
    mem = gs_memory_stable(mem);
    rc_alloc_struct_1(*ppsegs, gx_path_segments, &st_path_segments,
                      mem, return_error(gs_error_VMerror), cname);
    (*ppsegs)->rc.free = rc_free_path_segments;
    return 0;
}
int
gx_path_init_contained_shared(gx_path * ppath, const gx_path * shared,
                              gs_memory_t * mem, client_name_t cname)
{
    if (shared) {
        if (shared->segments == &shared->local_segments) {
#ifdef DEBUG
            lprintf1("Attempt to share (local) segments of path "PRI_INTPTR"!\n",
                     (intptr_t)shared);
#endif
            return_error(gs_error_Fatal);
        }
        *ppath = *shared;
        rc_increment(ppath->segments);
    } else {
        int code = path_alloc_segments(&ppath->segments, mem, cname);

        if (code < 0)
            return code;
        gx_path_init_contents(ppath);
    }
    ppath->memory = mem;
    ppath->allocation = path_allocated_contained;
    ppath->procs = &default_path_procs;
    return 0;
}

/*
 * Allocate a path on the heap, and initialize it.  If shared is NULL,
 * allocate a segments object; if shared is an existing path, share its
 * segments.
 */
gx_path *
gx_path_alloc_shared(const gx_path * shared, gs_memory_t * mem,
                     client_name_t cname)
{
    gx_path *ppath = gs_alloc_struct(mem, gx_path, &st_path, cname);

    if (ppath == 0)
        return 0;
    ppath->procs = &default_path_procs;
    if (shared) {
        if (shared->segments == &shared->local_segments) {
#ifdef DEBUG
            lprintf1("Attempt to share (local) segments of path "PRI_INTPTR"!\n",
                     (intptr_t)shared);
#endif
            gs_free_object(mem, ppath, cname);
            return 0;
        }
        *ppath = *shared;
        rc_increment(ppath->segments);
    } else {
        int code = path_alloc_segments(&ppath->segments, mem, cname);

        if (code < 0) {
            gs_free_object(mem, ppath, cname);
            return 0;
        }
        gx_path_init_contents(ppath);
    }
    ppath->memory = mem;
    ppath->allocation = path_allocated_on_heap;
    return ppath;
}

/*
 * Initialize a stack-allocated path.  This doesn't allocate anything,
 * but may still share the segments.
 */
int
gx_path_init_local_shared(gx_path * ppath, const gx_path * shared,
                          gs_memory_t * mem)
{
    if (shared) {
        if (shared->segments == &shared->local_segments) {
#ifdef DEBUG
            lprintf1("Attempt to share (local) segments of path "PRI_INTPTR"!\n",
                     (intptr_t)shared);
#endif
            return_error(gs_error_Fatal);
        }
        *ppath = *shared;
        rc_increment(ppath->segments);
    } else {
        rc_init_free(&ppath->local_segments, mem, 1,
                     rc_free_path_segments_local);
        ppath->segments = &ppath->local_segments;
        gx_path_init_contents(ppath);
    }
    ppath->memory = mem;
    ppath->allocation = path_allocated_on_stack;
    ppath->procs = &default_path_procs;
    return 0;
}

void gx_path_preinit_local_rectangle(gx_path *ppath, gs_memory_t *mem)
{
    rc_init_free(&ppath->local_segments, mem, 1, NULL);
    ppath->segments = &ppath->local_segments;
    ppath->box_last = 0;
    ppath->first_subpath = ppath->current_subpath = 0;
    ppath->subpath_count = 0;
    ppath->curve_count = 0;
    path_update_newpath(ppath);
    ppath->bbox_set = 1;
    ppath->bbox_accurate = 1;
    ppath->last_charpath_segment = 0;
    ppath->memory = mem;
    ppath->allocation = path_allocated_on_stack;
    ppath->procs = &default_path_procs;
}

void gx_path_init_local_rectangle(gx_path *ppath, gs_fixed_rect *rect)
{
    ppath->bbox = *rect;
}

/*
 * Initialize a stack-allocated pseudo-path for computing a bbox
 * for a dynamic path.
 */
void
gx_path_init_bbox_accumulator(gx_path * ppath)
{
    ppath->box_last = 0;
    ppath->subpath_count = 0;
    ppath->curve_count = 0;
    ppath->local_segments.contents.subpath_first = 0;
    ppath->local_segments.contents.subpath_current = 0;
    ppath->segments = 0;
    path_update_newpath(ppath);
    ppath->bbox_set = 0;
    ppath->bbox.p.x = ppath->bbox.p.y = ppath->bbox.q.x = ppath->bbox.q.y = 0;
    ppath->bbox_accurate = 1;
    ppath->memory = NULL; /* Won't allocate anything. */
    ppath->allocation = path_allocated_on_stack;
    ppath->procs = &path_bbox_procs;
}

/* Guarantee that a path's segments are not shared with any other path. */
static inline int path_unshare(gx_path *ppath)
{
    int code = 0;

    if (gx_path_is_shared(ppath))
        code = path_alloc_copy(ppath);
    return code;
}

/*
 * Ensure that a path owns its segments, by copying the segments if
 * they currently have multiple references.
 */
int
gx_path_unshare(gx_path * ppath)
{
    return path_unshare(ppath);
}

/*
 * Free a path by releasing its segments if they have no more references.
 * This also frees the path object iff it was allocated by gx_path_alloc.
 */
void
gx_path_free(gx_path * ppath, client_name_t cname)
{
    rc_decrement(ppath->segments, cname);
    /* Clean up pointers for GC. */
    ppath->box_last = 0;
    ppath->segments = 0;	/* Nota bene */
    if (ppath->allocation == path_allocated_on_heap)
        gs_free_object(ppath->memory, ppath, cname);
}

/*
 * Assign one path to another, adjusting reference counts appropriately.
 * Note that this requires that segments of the two paths (but not the path
 * objects themselves) were allocated with the same allocator.  Note also
 * that since it does the equivalent of a gx_path_new(ppto), it may allocate
 * a new segments object for ppto.
 */
int
gx_path_assign_preserve(gx_path * ppto, gx_path * ppfrom)
{
    gx_path_segments *fromsegs = ppfrom->segments;
    gx_path_segments *tosegs = ppto->segments;
    gs_memory_t *mem = ppto->memory;
    gx_path_allocation_t allocation = ppto->allocation;

    if (fromsegs == &ppfrom->local_segments) {
        /* We can't use ppfrom's segments object. */
        if (tosegs == &ppto->local_segments || gx_path_is_shared(ppto)) {
            /* We can't use ppto's segments either.  Allocate a new one. */
            int code = path_alloc_segments(&tosegs, ppto->memory,
                                           "gx_path_assign");

            if (code < 0)
                return code;
            rc_decrement(ppto->segments, "gx_path_assign");
        } else {
            /* Use ppto's segments object. */
            rc_free_path_segments_local(tosegs->rc.memory, tosegs,
                                        "gx_path_assign");
        }
        tosegs->contents = fromsegs->contents;
        ppfrom->segments = tosegs;
        rc_increment(tosegs);	/* for reference from ppfrom */
    } else {
        /* We can use ppfrom's segments object. */
        rc_increment(fromsegs);
        rc_decrement(tosegs, "gx_path_assign");
    }
    *ppto = *ppfrom;
    ppto->memory = mem;
    ppto->allocation = allocation;
    return 0;
}

/*
 * Assign one path to another and free the first path at the same time.
 * (This may do less work than assign_preserve + free.)
 */
int
gx_path_assign_free(gx_path * ppto, gx_path * ppfrom)
{
    int code = 0;
    /*
     * Detect the special case where both paths have non-shared local
     * segments, since we can avoid allocating new segments in this case.
     */
    if (ppto->segments == &ppto->local_segments &&
        ppfrom->segments == &ppfrom->local_segments &&
        !gx_path_is_shared(ppto)
        ) {
#define fromsegs (&ppfrom->local_segments)
#define tosegs (&ppto->local_segments)
        gs_memory_t *mem = ppto->memory;
        gx_path_allocation_t allocation = ppto->allocation;

        rc_free_path_segments_local(tosegs->rc.memory, tosegs,
                                    "gx_path_assign_free");
        /* We record a bogus reference to fromsegs, which */
        /* gx_path_free will undo. */
        *ppto = *ppfrom;
        rc_increment(fromsegs);
        ppto->segments = tosegs;
        ppto->memory = mem;
        ppto->allocation = allocation;
#undef fromsegs
#undef tosegs
    } else {
        /* In all other cases, just do assign + free. */
        code = gx_path_assign_preserve(ppto, ppfrom);
    }
    gx_path_free(ppfrom, "gx_path_assign_free");
    return code;
}

/*
 * Free the segments of a path when their reference count goes to zero.
 * We do this in reverse order so as to maximize LIFO allocator behavior.
 * We don't have to worry about cleaning up pointers, because we're about
 * to free the segments object.
 */
static void
rc_free_path_segments_local(gs_memory_t * mem, void *vpsegs,
                            client_name_t cname)
{
    gx_path_segments *psegs = (gx_path_segments *) vpsegs;
    segment *pseg;

    mem = gs_memory_stable(mem);
    if (psegs->contents.subpath_first == 0)
        return;			/* empty path */
    pseg = (segment *) psegs->contents.subpath_current->last;
    while (pseg) {
        segment *prev = pseg->prev;

        trace_segment("[P]release", mem, pseg);
        gs_free_object(mem, pseg, cname);
        pseg = prev;
    }
}
static void
rc_free_path_segments(gs_memory_t * mem, void *vpsegs, client_name_t cname)
{
    rc_free_path_segments_local(mem, vpsegs, cname);
    gs_free_object(mem, vpsegs, cname);
}

/* ------ Incremental path building ------ */

/* Macro for opening the current subpath. */
/* ppath points to the path; sets psub to ppath->current_subpath. */
static inline int path_open(gx_path *ppath)
{
    int code = 0;
    if ( !path_is_drawing(ppath) ) {
      if ( path_position_valid(ppath) ) {
          code = gx_path_new_subpath(ppath);
      }
      else {
          code = gs_note_error(gs_error_nocurrentpoint);
      }
    }
    return code;
}

/* Macros for allocating path segments. */
/* Note that they assume that ppath points to the path. */
/* We have to split the macro into two because of limitations */
/* on the size of a single statement (sigh). */
static inline int path_alloc_segment(gx_path *ppath, segment **ppseg, subpath **ppsub, gs_memory_type_ptr_t pstype, segment_type seg_type, ushort notes, client_name_t cname)
{
    int code;

    code = path_unshare(ppath);
    if (code < 0) return code;
    if (ppsub)
        *ppsub = ppath->current_subpath;

    if( !(*ppseg = gs_alloc_struct(gs_memory_stable(ppath->memory), segment, pstype, cname)) )
      return_error(gs_error_VMerror);
    (*ppseg)->type = seg_type;
    (*ppseg)->notes = notes;
    (*ppseg)->next = 0;

    return 0;
}

static inline void path_alloc_link(segment *pseg, subpath *psub)
{
    segment *prev = psub->last;
    prev->next = (segment *)pseg;
    pseg->prev = prev;
    psub->last = (segment *)pseg;
}

/* Make a new path (newpath). */
int
gx_path_new(gx_path * ppath)
{
    gx_path_segments *psegs = ppath->segments;

    if (gx_path_is_shared(ppath)) {
        int code = path_alloc_segments(&ppath->segments, ppath->memory,
                                       "gx_path_new");

        if (code < 0) {
            /* Leave the path in a valid state, despite the error */
            ppath->segments = psegs;
            return code;
        }
        rc_decrement(psegs, "gx_path_new");
    } else {
        rc_free_path_segments_local(psegs->rc.memory, psegs, "gx_path_new");
    }
    gx_path_init_contents(ppath);
    return 0;
}

/* Open a new subpath. */
/* The client must invoke path_update_xxx. */
static int
gx_path_new_subpath(gx_path * ppath)
{
    subpath *psub;
    subpath *spp;
    int code;

    code = path_alloc_segment(ppath, (segment **)&spp, &psub, &st_subpath, s_start, sn_none, "gx_path_new_subpath");
    if (code < 0)
        return code;

    spp->last = (segment *) spp;
    spp->curve_count = 0;
    spp->is_closed = 0;
    spp->pt = ppath->position;
    if (!psub) {		/* first subpath */
        ppath->first_subpath = spp;
        spp->prev = 0;
    } else {
        segment *prev = psub->last;

        prev->next = (segment *) spp;
        spp->prev = prev;
    }
    ppath->current_subpath = spp;
    ppath->subpath_count++;
    trace_segment("[P]", ppath->memory, (const segment *)spp);
    return 0;
}

static inline void
gz_path_bbox_add(gx_path * ppath, fixed x, fixed y)
{
    if (!ppath->bbox_set) {
        ppath->bbox.p.x = ppath->bbox.q.x = x;
        ppath->bbox.p.y = ppath->bbox.q.y = y;
        ppath->bbox_set = 1;
    } else {
        if (ppath->bbox.p.x > x)
            ppath->bbox.p.x = x;
        if (ppath->bbox.p.y > y)
            ppath->bbox.p.y = y;
        if (ppath->bbox.q.x < x)
            ppath->bbox.q.x = x;
        if (ppath->bbox.q.y < y)
            ppath->bbox.q.y = y;
    }
}

static inline void
gz_path_bbox_move(gx_path * ppath, fixed x, fixed y)
{
    /* a trick : we store 'fixed' into 'double'. */
    ppath->position.x = x;
    ppath->position.y = y;
    ppath->state_flags |= psf_position_valid;
}

/* Add a point to the current path (moveto). */
int
gx_path_add_point(gx_path * ppath, fixed x, fixed y)
{
    return ppath->procs->add_point(ppath, x, y);
}
static int
gz_path_add_point(gx_path * ppath, fixed x, fixed y)
{
    if (ppath->bbox_set)
        check_in_bbox(ppath, x, y);
    ppath->position.x = x;
    ppath->position.y = y;
    path_update_moveto(ppath);
    return 0;
}
static int
gz_path_bbox_add_point(gx_path * ppath, fixed x, fixed y)
{
    gz_path_bbox_move(ppath, x, y);
    return 0;
}

/* Add a relative point to the current path (rmoveto). */
int
gx_path_add_relative_point(gx_path * ppath, fixed dx, fixed dy)
{
    if (!path_position_in_range(ppath))
        return_error((path_position_valid(ppath) ? gs_error_limitcheck :
                      gs_error_nocurrentpoint));
    {
        fixed nx = ppath->position.x + dx, ny = ppath->position.y + dy;

        /* Check for overflow in addition. */
        if (((nx ^ dx) < 0 && (ppath->position.x ^ dx) >= 0) ||
            ((ny ^ dy) < 0 && (ppath->position.y ^ dy) >= 0)
            )
            return_error(gs_error_limitcheck);
        if (ppath->bbox_set)
            check_in_bbox(ppath, nx, ny);
        ppath->position.x = nx;
        ppath->position.y = ny;
    }
    path_update_moveto(ppath);
    return 0;
}

/* Set the segment point and the current point in the path. */
/* Assumes ppath points to the path. */
#define path_set_point(pseg, fx, fy)\
        (pseg)->pt.x = ppath->position.x = (fx),\
        (pseg)->pt.y = ppath->position.y = (fy)

/* Add a line to the current path (lineto). */
int
gx_path_add_line_notes(gx_path * ppath, fixed x, fixed y, segment_notes notes)
{
    return ppath->procs->add_line(ppath, x, y, notes);
}
static int
gz_path_add_line_notes(gx_path * ppath, fixed x, fixed y, segment_notes notes)
{
    subpath *psub;
    line_segment *lp;
    int code;

    if (ppath->bbox_set)
        check_in_bbox(ppath, x, y);
    code = path_open(ppath);
    if (code < 0) return code;
    code = path_alloc_segment(ppath, (segment **)&lp, &psub, &st_line, s_line, notes, "gx_path_add_line");
    if (code < 0) return code;
    path_alloc_link((segment *)lp, psub);
    path_set_point(lp, x, y);
    path_update_draw(ppath);
    trace_segment("[P]", ppath->memory, (segment *) lp);
    return 0;
}
static int
gz_path_bbox_add_line_notes(gx_path * ppath, fixed x, fixed y, segment_notes notes)
{
    gz_path_bbox_add(ppath, x, y);
    gz_path_bbox_move(ppath, x, y);
    return 0;
}
/* Add a gap to the current path (lineto). */
int
gx_path_add_gap_notes(gx_path * ppath, fixed x, fixed y, segment_notes notes)
{
    return ppath->procs->add_gap(ppath, x, y, notes);
}
static int
gz_path_add_gap_notes(gx_path * ppath, fixed x, fixed y, segment_notes notes)
{
    subpath *psub;
    line_segment *lp;
    int code;

    if (ppath->bbox_set)
        check_in_bbox(ppath, x, y);
    code = path_open(ppath);
    if (code < 0) return code;
    code = path_alloc_segment(ppath, (segment **)&lp, &psub, &st_line, s_gap, notes, "gx_path_add_gap");
    if (code < 0) return code;
    path_alloc_link((segment *)lp, psub);
    path_set_point(lp, x, y);
    path_update_draw(ppath);
    trace_segment("[P]", ppath->memory, (segment *) lp);
    return 0;
}
static int
gz_path_bbox_add_gap_notes(gx_path * ppath, fixed x, fixed y, segment_notes notes)
{
    gz_path_bbox_add(ppath, x, y);
    gz_path_bbox_move(ppath, x, y);
    return 0;
}

/* Add multiple lines to the current path. */
/* Note that all lines have the same notes. */
int
gx_path_add_lines_notes(gx_path *ppath, const gs_fixed_point *ppts, int count,
                        segment_notes notes)
{
    subpath *psub;
    segment *prev;
    line_segment *lp = 0;
    int i;
    int code = 0;

    if (count <= 0)
        return 0;
    code = path_unshare(ppath);
    if (code < 0)
        return code;

    code = path_open(ppath);
    if (code < 0) return code;
    psub = ppath->current_subpath;
    prev = psub->last;
    /*
     * We could do better than the following, but this is a start.
     * Note that we don't make any attempt to undo partial additions
     * if we fail partway through; this is equivalent to what would
     * happen with multiple calls on gx_path_add_line.
     */
    for (i = 0; i < count; i++) {
        fixed x = ppts[i].x;
        fixed y = ppts[i].y;
        line_segment *next;

        if (ppath->bbox_set && outside_bbox(ppath, x, y)) {
            code = gs_note_error(gs_error_rangecheck);
            break;
        }
        if (!(next = gs_alloc_struct(gs_memory_stable(ppath->memory),
                                     line_segment, &st_line,
                                     "gx_path_add_lines"))
            ) {
            code = gs_note_error(gs_error_VMerror);
            break;
        }
        lp = next;
        lp->type = s_line;
        lp->notes = notes;
        prev->next = (segment *) lp;
        lp->prev = prev;
        lp->pt.x = x;
        lp->pt.y = y;
        prev = (segment *) lp;
        trace_segment("[P]", ppath->memory, (segment *) lp);
    }
    if (lp != 0)
        ppath->position.x = lp->pt.x,
            ppath->position.y = lp->pt.y,
            psub->last = (segment *) lp,
            lp->next = 0,
            path_update_draw(ppath);
    return code;
}

/* Add a dash to the current path (lineto with a small length). */
/* Only for internal use of the stroking algorithm. */
int
gx_path_add_dash_notes(gx_path * ppath, fixed x, fixed y, fixed dx, fixed dy, segment_notes notes)
{
    subpath *psub;
    dash_segment *lp;
    int code;

    if (ppath->bbox_set)
        check_in_bbox(ppath, x, y);
    code = path_open(ppath);
    if (code < 0) return code;
    code = path_alloc_segment(ppath, (segment **)&lp, &psub, &st_dash, s_dash, notes, "gx_dash_add_dash");
    if (code < 0) return code;
    path_alloc_link((segment *)lp, psub);
    path_set_point(lp, x, y);
    lp->tangent.x = dx;
    lp->tangent.y = dy;
    path_update_draw(ppath);
    trace_segment("[P]", ppath->memory, (segment *) lp);
    return 0;
}

/* Add a rectangle to the current path. */
/* This is a special case of adding a closed polygon. */
int
gx_path_add_rectangle(gx_path * ppath, fixed x0, fixed y0, fixed x1, fixed y1)
{
    gs_fixed_point pts[3];
    int code;

    pts[0].x = x0;
    pts[1].x = pts[2].x = x1;
    pts[2].y = y0;
    pts[0].y = pts[1].y = y1;
    if ((code = gx_path_add_point(ppath, x0, y0)) < 0 ||
        (code = gx_path_add_lines(ppath, pts, 3)) < 0 ||
        (code = gx_path_close_subpath(ppath)) < 0
        )
        return code;
    return 0;
}

/* Add a curve to the current path (curveto). */
int
gx_path_add_curve_notes(gx_path * ppath,
                 fixed x1, fixed y1, fixed x2, fixed y2, fixed x3, fixed y3,
                        segment_notes notes)
{
    return ppath->procs->add_curve(ppath, x1, y1, x2, y2, x3, y3, notes);
}
static int
gz_path_add_curve_notes(gx_path * ppath,
                 fixed x1, fixed y1, fixed x2, fixed y2, fixed x3, fixed y3,
                        segment_notes notes)
{
    subpath *psub;
    curve_segment *lp;
    int code;

    if (ppath->bbox_set) {
        check_in_bbox(ppath, x1, y1);
        check_in_bbox(ppath, x2, y2);
        check_in_bbox(ppath, x3, y3);
    }
    code = path_open(ppath);
    if (code < 0) return code;
    code = path_alloc_segment(ppath, (segment **)&lp, &psub, &st_curve, s_curve, notes, "gx_path_add_curve");
    if (code < 0) return code;
    path_alloc_link((segment *)lp, psub);
    lp->p1.x = x1;
    lp->p1.y = y1;
    lp->p2.x = x2;
    lp->p2.y = y2;
    path_set_point(lp, x3, y3);
    psub->curve_count++;
    ppath->curve_count++;
    path_update_draw(ppath);
    trace_segment("[P]", ppath->memory, (segment *) lp);
    return 0;
}
static int
gz_path_bbox_add_curve_notes(gx_path * ppath,
                 fixed x1, fixed y1, fixed x2, fixed y2, fixed x3, fixed y3,
                        segment_notes notes)
{
    gz_path_bbox_add(ppath, x1, y1);
    gz_path_bbox_add(ppath, x2, y2);
    gz_path_bbox_add(ppath, x3, y3);
    gz_path_bbox_move(ppath, x3, y3);
    return 0;
}

/*
 * Add an approximation of an arc to the current path.
 * The current point of the path is the initial point of the arc;
 * parameters are the final point of the arc
 * and the point at which the extended tangents meet.
 * We require that the arc be less than a semicircle.
 * The arc may go either clockwise or counterclockwise.
 * The approximation is a very simple one: a single curve
 * whose other two control points are a fraction F of the way
 * to the intersection of the tangents, where
 *      F = (4/3)(1 / (1 + sqrt(1+(d/r)^2)))
 * where r is the radius and d is the distance from either tangent
 * point to the intersection of the tangents.  This produces
 * a curve whose center point, as well as its ends, lies on
 * the desired arc.
 *
 * Because F has to be computed in user space, we let the client
 * compute it and pass it in as an argument.
 */
int
gx_path_add_partial_arc_notes(gx_path * ppath,
fixed x3, fixed y3, fixed xt, fixed yt, double fraction, segment_notes notes)
{
    fixed x0 = ppath->position.x, y0 = ppath->position.y;

    return gx_path_add_curve_notes(ppath,
                                   x0 + (fixed) ((xt - x0) * fraction),
                                   y0 + (fixed) ((yt - y0) * fraction),
                                   x3 + (fixed) ((xt - x3) * fraction),
                                   y3 + (fixed) ((yt - y3) * fraction),
                                   x3, y3, notes | sn_from_arc);
}

/* Append a path to another path, and reset the first path. */
/* Currently this is only used to append a path to its parent */
/* (the path in the previous graphics context). */
int
gx_path_add_path(gx_path * ppath, gx_path * ppfrom)
{
    int code = path_unshare(ppfrom);
    if (code < 0)
        return code;
    code = path_unshare(ppath);
    if (code < 0)
        return code;
    if (ppfrom->first_subpath) {	/* i.e. ppfrom not empty */
        if (ppath->first_subpath) {	/* i.e. ppath not empty */
            subpath *psub = ppath->current_subpath;
            segment *pseg = psub->last;
            subpath *pfsub = ppfrom->first_subpath;

            pseg->next = (segment *) pfsub;
            pfsub->prev = pseg;
        } else
            ppath->first_subpath = ppfrom->first_subpath;
        ppath->current_subpath = ppfrom->current_subpath;
        ppath->subpath_count += ppfrom->subpath_count;
        ppath->curve_count += ppfrom->curve_count;
    }
    /* Transfer the remaining state. */
    ppath->position = ppfrom->position;
    ppath->state_flags = ppfrom->state_flags;
    /* Reset the source path. */
    gx_path_init_contents(ppfrom);
    return 0;
}

/* Add a path or its bounding box to the enclosing path, */
/* and reset the first path.  Only used for implementing charpath and its */
/* relatives. */
int
gx_path_add_char_path(gx_path * to_path, gx_path * from_path,
                      gs_char_path_mode mode)
{
    int code;
    gs_fixed_rect bbox;

    switch (mode) {
        default:		/* shouldn't happen! */
            gx_path_new(from_path);
            return 0;
        case cpm_charwidth: {
            gs_fixed_point cpt;

            code = gx_path_current_point(from_path, &cpt);
            if (code < 0)
                break;
            return gx_path_add_point(to_path, cpt.x, cpt.y);
        }
        case cpm_true_charpath:
        case cpm_false_charpath:
            return gx_path_add_path(to_path, from_path);
        case cpm_true_charboxpath:
            gx_path_bbox(from_path, &bbox);
            code = gx_path_add_rectangle(to_path, bbox.p.x, bbox.p.y,
                                         bbox.q.x, bbox.q.y);
            break;
        case cpm_false_charboxpath:
            gx_path_bbox(from_path, &bbox);
            code = gx_path_add_point(to_path, bbox.p.x, bbox.p.y);
            if (code >= 0)
                code = gx_path_add_line(to_path, bbox.q.x, bbox.q.y);
            break;
    }
    if (code < 0)
        return code;
    gx_path_new(from_path);
    return 0;
}

/* Close the current subpath. */
int
gx_path_close_subpath_notes(gx_path * ppath, segment_notes notes)
{
    return ppath->procs->close_subpath(ppath, notes);
}
static int
gz_path_close_subpath_notes(gx_path * ppath, segment_notes notes)
{
    subpath *psub;
    line_close_segment *lp;
    int code;

    if (!path_subpath_open(ppath))
        return 0;
    if (path_last_is_moveto(ppath)) {
        /* The last operation was a moveto: create a subpath. */
        code = gx_path_new_subpath(ppath);
        if (code < 0)
            return code;
    }
    code = path_alloc_segment(ppath, (segment **)&lp, &psub, &st_line_close, s_line_close, notes, "gx_path_close_subpath");
    if (code < 0) return code;
    path_alloc_link((segment *)lp, psub);
    path_set_point(lp, psub->pt.x, psub->pt.y);
    lp->sub = psub;
    psub->is_closed = 1;
    path_update_closepath(ppath);
    trace_segment("[P]", ppath->memory, (segment *) lp);
    return 0;
}
static int
gz_path_bbox_close_subpath_notes(gx_path * ppath, segment_notes notes)
{
    return 0;
}

/* Access path state flags */
byte
gz_path_state_flags(gx_path *ppath, byte flags)
{
    byte flags_old = ppath->state_flags;
    ppath->state_flags = flags;
    return flags_old;
}
byte
gx_path_get_state_flags(gx_path *ppath)
{
    byte flags = ppath->procs->state_flags(ppath, 0);
    ppath->procs->state_flags(ppath, flags);
    return flags;
}
void
gx_path_set_state_flags(gx_path *ppath, byte flags)
{
    ppath->procs->state_flags(ppath, flags);
}
bool
gx_path_is_drawing(gx_path *ppath)
{
  return path_is_drawing(ppath);
}

/* Remove the last line from the current subpath, and then close it. */
/* The Type 1 font hinting routines use this if a path ends with */
/* a line to the start followed by a closepath. */
int
gx_path_pop_close_notes(gx_path * ppath, segment_notes notes)
{
    subpath *psub = ppath->current_subpath;
    segment *pseg;
    segment *prev;

    if (psub == 0 || (pseg = psub->last) == 0 ||
        pseg->type != s_line
        )
        return_error(gs_error_unknownerror);
    prev = pseg->prev;
    prev->next = 0;
    psub->last = prev;
    gs_free_object(ppath->memory, pseg, "gx_path_pop_close_subpath");
    return gx_path_close_subpath_notes(ppath, notes);
}

/* ------ Internal routines ------ */

/*
 * Copy the current path, because it was shared.
 */
static int
path_alloc_copy(gx_path * ppath)
{
    gx_path path_new;
    int code;

    gx_path_init_local(&path_new, ppath->memory);
    code = gx_path_copy(ppath, &path_new);
    if (code < 0) {
        gx_path_free(&path_new, "path_alloc_copy error");
        return code;
    }
    ppath->last_charpath_segment = 0;
    return gx_path_assign_free(ppath, &path_new);
}

/* ------ Debugging printout ------ */

#ifdef DEBUG

/* Print out a path with a label */
void
gx_dump_path(const gx_path * ppath, const char *tag)
{
    dmlprintf2(ppath->memory, "[P]Path "PRI_INTPTR" %s:\n", (intptr_t)ppath, tag);
    gx_path_print(ppath);
}

/* Print a path */
void
gx_path_print(const gx_path * ppath)
{
    const segment *pseg = (const segment *)ppath->first_subpath;

    dmlprintf5(ppath->memory,
               " %% state_flags=%d subpaths=%d, curves=%d, point=(%f,%f)\n",
               ppath->state_flags, ppath->subpath_count, ppath->curve_count,
               fixed2float(ppath->position.x),
               fixed2float(ppath->position.y));
    dmlprintf5(ppath->memory," %% box=(%f,%f),(%f,%f) last="PRI_INTPTR"\n",
               fixed2float(ppath->bbox.p.x), fixed2float(ppath->bbox.p.y),
               fixed2float(ppath->bbox.q.x), fixed2float(ppath->bbox.q.y),
               (intptr_t)ppath->box_last);
    dmlprintf4(ppath->memory,
               " %% segments="PRI_INTPTR" (refct=%ld, first="PRI_INTPTR", current="PRI_INTPTR")\n",
               (intptr_t)ppath->segments, (long)ppath->segments->rc.ref_count,
               (intptr_t)ppath->segments->contents.subpath_first,
               (intptr_t)ppath->segments->contents.subpath_current);
    while (pseg) {
        dmlputs(ppath->memory,"");
        gx_print_segment(ppath->memory, pseg);
        pseg = pseg->next;
    }
}
static void
gx_print_segment(const gs_memory_t *mem, const segment * pseg)
{
    double px = fixed2float(pseg->pt.x);
    double py = fixed2float(pseg->pt.y);
    char out[80];

    gs_snprintf(out, sizeof(out), PRI_INTPTR "<"PRI_INTPTR","PRI_INTPTR">:%u",
               (intptr_t)pseg, (intptr_t)pseg->prev, (intptr_t)pseg->next, pseg->notes);
    switch (pseg->type) {
        case s_start:{
                const subpath *const psub = (const subpath *)pseg;

                dmprintf5(mem, "   %1.4f %1.4f moveto\t%% %s #curves=%d last="PRI_INTPTR"\n",
                          px, py, out, psub->curve_count, (intptr_t)psub->last);
                break;
            }
        case s_curve:{
                const curve_segment *const pcur = (const curve_segment *)pseg;

                dmprintf7(mem, "   %1.4f %1.4f %1.4f %1.4f %1.4f %1.4f curveto\t%% %s\n",
                          fixed2float(pcur->p1.x), fixed2float(pcur->p1.y),
                          fixed2float(pcur->p2.x), fixed2float(pcur->p2.y),
                          px, py, out);
                break;
            }
        case s_line:
            dmprintf3(mem, "   %1.4f %1.4f lineto\t%% %s\n", px, py, out);
            break;
        case s_gap:
            dmprintf3(mem, "   %1.4f %1.4f gapto\t%% %s\n", px, py, out);
            break;
        case s_dash:{
                const dash_segment *const pd = (const dash_segment *)pseg;

                dmprintf5(mem, "   %1.4f %1.4f %1.4f  %1.4f dash\t%% %s\n",
                          fixed2float(pd->pt.x), fixed2float(pd->pt.y),
                          fixed2float(pd->tangent.x),fixed2float(pd->tangent.y),
                          out);
                break;
            }
        case s_line_close:{
                const line_close_segment *const plc =
                (const line_close_segment *)pseg;

                dmprintf4(mem, "   closepath\t%% %s %1.4f %1.4f "PRI_INTPTR"\n",
                          out, px, py, (intptr_t)(plc->sub));
                break;
            }
        default:
            dmprintf4(mem, "   %1.4f %1.4f <type "PRI_INTPTR">\t%% %s\n",
                      px, py, (intptr_t)pseg->type, out);
    }
}

#endif /* DEBUG */
