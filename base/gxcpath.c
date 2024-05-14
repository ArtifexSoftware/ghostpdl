/* Copyright (C) 2001-2024 Artifex Software, Inc.
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


/* Implementation of clipping paths, other than actual clipping */
#include "gx.h"
#include "string_.h"
#include "gserrors.h"
#include "gsstruct.h"
#include "gsutil.h"
#include "gsline.h"
#include "gxdevice.h"
#include "gxfixed.h"
#include "gxpaint.h"
#include "gscoord.h"		/* needs gsmatrix.h */
#include "gxgstate.h"
#include "gzpath.h"
#include "gzcpath.h"
#include "gzacpath.h"

/* Forward references */
static void gx_clip_list_from_rectangle(gx_clip_list *, gs_fixed_rect *);

/* Other structure types */
public_st_clip_rect();
public_st_clip_list();
public_st_clip_path();
private_st_clip_rect_list();
public_st_device_clip();
private_st_cpath_path_list();

/* GC procedures for gx_clip_path */
static
ENUM_PTRS_WITH(clip_path_enum_ptrs, gx_clip_path *cptr) return ENUM_USING(st_path, &cptr->path, sizeof(cptr->path), index - 3);

case 0:
return ENUM_OBJ((cptr->rect_list == &cptr->local_list ? 0 :
             cptr->rect_list));
case 1:
return ENUM_OBJ(cptr->path_list);
case 2:
return ENUM_OBJ((cptr->cached == &cptr->rect_list->list.single ? 0 :
             cptr->cached));
ENUM_PTRS_END
static
RELOC_PTRS_WITH(clip_path_reloc_ptrs, gx_clip_path *cptr)
{
    if (cptr->rect_list != &cptr->local_list)
        RELOC_VAR(cptr->rect_list);
    RELOC_VAR(cptr->path_list);
    if (cptr->cached != &cptr->rect_list->list.single)
        RELOC_VAR(cptr->cached);
    RELOC_USING(st_path, &cptr->path, sizeof(gx_path));
}
RELOC_PTRS_END

/* GC procedures for gx_device_clip */
static
ENUM_PTRS_WITH(device_clip_enum_ptrs, gx_device_clip *cptr)
{
    if (index < st_clip_list_max_ptrs + 3)
        return ENUM_USING(st_clip_list, &cptr->list,
                          sizeof(gx_clip_list), index - 3);
    return ENUM_USING(st_device_forward, vptr,
                      sizeof(gx_device_forward),
                      index - (st_clip_list_max_ptrs + 3));
}
case 0:
ENUM_RETURN((cptr->current == &cptr->list.single ? NULL :
             (void *)cptr->current));
case 1:
ENUM_RETURN((cptr->cpath));
case 2:
ENUM_RETURN((cptr->rect_list));
ENUM_PTRS_END
static
RELOC_PTRS_WITH(device_clip_reloc_ptrs, gx_device_clip *cptr)
{
    if (cptr->current == &cptr->list.single)
        cptr->current = &((gx_device_clip *)RELOC_OBJ(vptr))->list.single;
    else
        RELOC_PTR(gx_device_clip, current);
    RELOC_PTR(gx_device_clip, cpath);
    RELOC_PTR(gx_device_clip, rect_list);
    RELOC_USING(st_clip_list, &cptr->list, sizeof(gx_clip_list));
    RELOC_USING(st_device_forward, vptr, sizeof(gx_device_forward));
}
RELOC_PTRS_END

/* Define an empty clip list. */
static const gx_clip_list clip_list_empty = {
    {
        0,       /* next */
        0,       /* prev */
        min_int, /* ymin */
        max_int, /* ymax */
        0,       /* xmin */
        0,       /* xmax */
        0        /* to_visit */
    }, /* single */
    0, /* head */
    0, /* tail */
    0, /* insert */
    0, /* xmin */
    0, /* xmax */
    0, /* count */
    0  /* transpose = false */
};

/* ------ Clipping path memory management ------ */

static rc_free_proc(rc_free_cpath_list);
static rc_free_proc(rc_free_cpath_list_local);
static rc_free_proc(rc_free_cpath_path_list);

/*
 * Initialize those parts of the contents of a clip path that aren't
 * part of the path.
 */
static void
cpath_init_rectangle(gx_clip_path * pcpath, gs_fixed_rect * pbox)
{
    gx_clip_list_from_rectangle(&pcpath->rect_list->list, pbox);
    pcpath->inner_box = *pbox;
    pcpath->path_valid = false;
    pcpath->path_fill_adjust.x = 0;
    pcpath->path_fill_adjust.y = 0;
    pcpath->path.bbox = *pbox;
    gx_cpath_set_outer_box(pcpath);
    pcpath->id = gs_next_ids(pcpath->path.memory, 1);	/* path changed => change id */
    pcpath->cached = NULL;
}
static void
cpath_init_own_contents(gx_clip_path * pcpath)
{    /* We could make null_rect static, but then it couldn't be const. */
    gs_fixed_rect null_rect;

    null_rect.p.x = null_rect.p.y = null_rect.q.x = null_rect.q.y = 0;
    cpath_init_rectangle(pcpath, &null_rect);
    pcpath->path_list = NULL;
}
static void
cpath_share_own_contents(gx_clip_path * pcpath, const gx_clip_path * shared)
{
    pcpath->inner_box = shared->inner_box;
    pcpath->path_valid = shared->path_valid;
    pcpath->path_fill_adjust = shared->path_fill_adjust;
    pcpath->outer_box = shared->outer_box;
    pcpath->id = shared->id;
    pcpath->cached = NULL;
}

/* Allocate only the segments of a clipping path on the heap. */
static int
cpath_alloc_list(gx_clip_rect_list ** prlist, gs_memory_t * mem,
                 client_name_t cname)
{
    rc_alloc_struct_1(*prlist, gx_clip_rect_list, &st_clip_rect_list, mem,
                      return_error(gs_error_VMerror), cname);
    (*prlist)->rc.free = rc_free_cpath_list;
    return 0;
}
int
gx_cpath_init_contained_shared(gx_clip_path * pcpath,
        const gx_clip_path * shared, gs_memory_t * mem, client_name_t cname)
{
    if (shared) {
        if (shared->path.segments == &shared->path.local_segments) {
#ifdef DEBUG
            lprintf1("Attempt to share (local) segments of clip path "PRI_INTPTR"!\n",
                     (intptr_t)shared);
#endif
            return_error(gs_error_Fatal);
        }
        *pcpath = *shared;
        pcpath->path.memory = mem;
        pcpath->path.allocation = path_allocated_contained;
        rc_increment(pcpath->path.segments);
        rc_increment(pcpath->rect_list);
        rc_increment(pcpath->path_list);
    } else {
        int code = cpath_alloc_list(&pcpath->rect_list, mem, cname);

        if (code < 0)
            return code;
        code = gx_path_alloc_contained(&pcpath->path, mem, cname);
        if (code < 0) {
            gs_free_object(mem, pcpath->rect_list, cname);
            pcpath->rect_list = 0;
            return code;
        }
        cpath_init_own_contents(pcpath);
    }
    return 0;
}
#define gx_cpath_alloc_contents(pcpath, shared, mem, cname)\
  gx_cpath_init_contained_shared(pcpath, shared, mem, cname)

/* Allocate all of a clipping path on the heap. */
gx_clip_path *
gx_cpath_alloc_shared(const gx_clip_path * shared, gs_memory_t * mem,
                      client_name_t cname)
{
    gx_clip_path *pcpath =
    gs_alloc_struct(mem, gx_clip_path, &st_clip_path, cname);
    int code;

    if (pcpath == 0)
        return 0;
    code = gx_cpath_alloc_contents(pcpath, shared, mem, cname);
    if (code < 0) {
        gs_free_object(mem, pcpath, cname);
        return 0;
    }
    pcpath->path.allocation = path_allocated_on_heap;
    return pcpath;
}

/* Initialize a stack-allocated clipping path. */
int
gx_cpath_init_local_shared_nested(gx_clip_path * pcpath,
                            const gx_clip_path * shared,
                                  gs_memory_t  * mem,
                                  bool           safely_nested)
{
    if (shared) {
        if ((shared->path.segments == &shared->path.local_segments) &&
            !safely_nested) {
#ifdef DEBUG
            lprintf1("Attempt to share (local) segments of clip path "PRI_INTPTR"!\n",
                     (intptr_t)shared);
#endif
            return_error(gs_error_Fatal);
        }
        pcpath->path = shared->path;
        pcpath->path.allocation = path_allocated_on_stack;
        rc_increment(pcpath->path.segments);
        pcpath->rect_list = shared->rect_list;
        rc_increment(pcpath->rect_list);
        pcpath->path_list = shared->path_list;
        rc_increment(pcpath->path_list);
        cpath_share_own_contents(pcpath, shared);
        pcpath->rule = shared->rule;
    } else {
        gx_path_init_local(&pcpath->path, mem);
        rc_init_free(&pcpath->local_list, mem, 1, rc_free_cpath_list_local);
        pcpath->rect_list = &pcpath->local_list;
        cpath_init_own_contents(pcpath);
    }
    return 0;
}

int
gx_cpath_init_local_shared(gx_clip_path * pcpath, const gx_clip_path * shared,
                           gs_memory_t * mem)
{
    return gx_cpath_init_local_shared_nested(pcpath, shared, mem, 0);
}

/* Unshare a clipping path. */
int
gx_cpath_unshare(gx_clip_path * pcpath)
{
    int code = gx_path_unshare(&pcpath->path);
    gx_clip_rect_list *rlist = pcpath->rect_list;

    if (code < 0)
        return code;
    if (rlist->rc.ref_count > 1) {
        int code = cpath_alloc_list(&pcpath->rect_list, pcpath->path.memory,
                                    "gx_cpath_unshare");

        if (code < 0)
            return code;
        /* Copy the rectangle list. */
/**************** NYI ****************/
        /* Until/Unless we implement this, NULL out the list */
        memset(&pcpath->rect_list->list, 0x00, sizeof(gx_clip_list));
        rc_decrement(rlist, "gx_cpath_unshare");
    }
    return code;
}

/* Free a clipping path. */
void
gx_cpath_free(gx_clip_path * pcpath, client_name_t cname)
{
    if (pcpath == 0L)
        return;

    rc_decrement(pcpath->rect_list, cname);
    rc_decrement(pcpath->path_list, cname);
    /* Clean up pointers for GC. */
    pcpath->rect_list = 0;
    pcpath->path_list = 0;
    {
        gx_path_allocation_t alloc = pcpath->path.allocation;

        if (alloc == path_allocated_on_heap) {
            pcpath->path.allocation = path_allocated_contained;
            gx_path_free(&pcpath->path, cname);
            gs_free_object(pcpath->path.memory, pcpath, cname);
        } else
            gx_path_free(&pcpath->path, cname);
    }
}

/* Assign a clipping path, preserving the source. */
int
gx_cpath_assign_preserve(gx_clip_path * pcpto, gx_clip_path * pcpfrom)
{
    int code = gx_path_assign_preserve(&pcpto->path, &pcpfrom->path);
    gx_clip_rect_list *fromlist = pcpfrom->rect_list;
    gx_clip_rect_list *tolist = pcpto->rect_list;
    gx_path path;

    if (code < 0)
        return 0;
    if (fromlist == &pcpfrom->local_list) {
        /* We can't use pcpfrom's list object. */
        if (tolist == &pcpto->local_list || tolist->rc.ref_count > 1) {
            /* We can't use pcpto's list either.  Allocate a new one. */
            int code = cpath_alloc_list(&tolist, tolist->rc.memory,
                                        "gx_cpath_assign");

            if (code < 0) {
                rc_decrement(pcpto->path.segments, "gx_path_assign");
                return code;
            }
            rc_decrement(pcpto->rect_list, "gx_cpath_assign");
        } else {
            /* Use pcpto's list object. */
            rc_free_cpath_list_local(tolist->rc.memory, tolist,
                                     "gx_cpath_assign");
        }
        tolist->list = fromlist->list;
        pcpfrom->rect_list = tolist;
        rc_increment(tolist);
    } else {
        /* We can use pcpfrom's list object. */
        rc_increment(fromlist);
        rc_decrement(pcpto->rect_list, "gx_cpath_assign");
    }
    rc_increment(pcpfrom->path_list);
    rc_decrement(pcpto->path_list, "gx_cpath_assign");
    path = pcpto->path, *pcpto = *pcpfrom, pcpto->path = path;
    return 0;
}

/* Assign a clipping path, releasing the source. */
int
gx_cpath_assign_free(gx_clip_path * pcpto, gx_clip_path * pcpfrom)
{				/* For right now, just do assign + free. */
    int code = gx_cpath_assign_preserve(pcpto, pcpfrom);

    if (code < 0)
        return code;
    gx_cpath_free(pcpfrom, "gx_cpath_assign_free");
    return 0;
}

/* Free the clipping list when its reference count goes to zero. */
static void
rc_free_cpath_list_local(gs_memory_t * mem, void *vrlist,
                         client_name_t cname)
{
    gx_clip_rect_list *rlist = (gx_clip_rect_list *) vrlist;

    gx_clip_list_free(&rlist->list, mem);
}
static void
rc_free_cpath_list(gs_memory_t * mem, void *vrlist, client_name_t cname)
{
    rc_free_cpath_list_local(mem, vrlist, cname);
    gs_free_object(mem, vrlist, cname);
}

static void
rc_free_cpath_path_list(gs_memory_t * mem, void *vplist, client_name_t cname)
{
    gx_cpath_path_list *plist = (gx_cpath_path_list *)vplist;
    rc_decrement(plist->next, cname);
    gx_path_free(&plist->path, cname);
    gs_free_object(plist->path.memory, plist, cname);
}

/* Allocate a new clip path list node. The created node has a ref count
   of 1, and "steals" the reference to next (i.e. does not increment
   its reference count). */
static int
gx_cpath_path_list_new(gs_memory_t *mem, gx_clip_path *pcpath, int rule,
                        gx_path *ppfrom, gx_cpath_path_list *next, gx_cpath_path_list **pnew)
{
    int code;
    client_name_t cname = "gx_cpath_path_list_new";
    gx_cpath_path_list *pcplist = gs_alloc_struct(mem, gx_cpath_path_list,
                                                  &st_cpath_path_list, cname);

    if (pcplist == 0)
        return_error(gs_error_VMerror);
    rc_init_free(pcplist, mem, 1, rc_free_cpath_path_list);
    if (pcpath!=NULL && !pcpath->path_valid) {
        code = gx_path_init_contained_shared(&pcplist->path, NULL, mem, cname);
        if (code < 0) {
            gs_free_object(mem, pcplist, "gx_cpath_path_list_new");
            return code;
        }
        code = gx_cpath_to_path(pcpath, &pcplist->path);
    } else {
        gx_path_init_local(&pcplist->path, mem);
        code = gx_path_assign_preserve(&pcplist->path, ppfrom);
    }
    if (code < 0)
        return code;
    pcplist->next = next;
    rc_increment(next);
    pcplist->rule = rule;
    *pnew = pcplist;
    return 0;
}

/* ------ Clipping path accessing ------ */

/* Synthesize a path from a clipping path. */
int
gx_cpath_to_path_synthesize(const gx_clip_path * pcpath, gx_path * ppath)
{
    /* Synthesize a path. */
    gs_cpath_enum cenum;
    gs_fixed_point pts[3];
    int code;

    gx_cpath_enum_init(&cenum, pcpath);
    while ((code = gx_cpath_enum_next(&cenum, pts)) != 0) {
        switch (code) {
            case gs_pe_moveto:
                code = gx_path_add_point(ppath, pts[0].x, pts[0].y);
                break;
            case gs_pe_lineto:
                code = gx_path_add_line_notes(ppath, pts[0].x, pts[0].y,
                                           gx_cpath_enum_notes(&cenum));
                break;
            case gs_pe_gapto:
                code = gx_path_add_gap_notes(ppath, pts[0].x, pts[0].y,
                                           gx_cpath_enum_notes(&cenum));
                break;
            case gs_pe_curveto:
                code = gx_path_add_curve_notes(ppath, pts[0].x, pts[0].y,
                                               pts[1].x, pts[1].y,
                                               pts[2].x, pts[2].y,
                                           gx_cpath_enum_notes(&cenum));
                break;
            case gs_pe_closepath:
                code = gx_path_close_subpath_notes(ppath,
                                           gx_cpath_enum_notes(&cenum));
                break;
            default:
                if (code >= 0)
                    code = gs_note_error(gs_error_unregistered);
        }
        if (code < 0)
            break;
    }
    return 0;
}

/* Return the path of a clipping path. */
int
gx_cpath_to_path(gx_clip_path * pcpath, gx_path * ppath)
{
    if (!pcpath->path_valid) {
        gx_path rpath;
        int code;

        gx_path_init_local(&rpath, pcpath->path.memory);
        code = gx_cpath_to_path_synthesize(pcpath, &rpath);
        if (code < 0) {
            gx_path_free(&rpath, "gx_cpath_to_path error");
            return code;
        }
        code = gx_path_assign_free(&pcpath->path, &rpath);
        if (code < 0)
            return code;
        pcpath->path_valid = true;
        pcpath->path_fill_adjust.x = 0;
        pcpath->path_fill_adjust.y = 0;
    }
    return gx_path_assign_preserve(ppath, &pcpath->path);
}
/* Return the inner and outer check rectangles for a clipping path. */
/* Return true iff the path is a rectangle. */
bool
gx_cpath_inner_box(const gx_clip_path * pcpath, gs_fixed_rect * pbox)
{
    *pbox = pcpath->inner_box;
    return clip_list_is_rectangle(gx_cpath_list(pcpath));
}
bool
gx_cpath_outer_box(const gx_clip_path * pcpath, gs_fixed_rect * pbox)
{
    *pbox = pcpath->outer_box;
    return clip_list_is_rectangle(gx_cpath_list(pcpath));
}

/* Test if a clipping path includes a rectangle. */
/* The rectangle need not be oriented correctly, i.e. x0 > x1 is OK. */
bool
gx_cpath_includes_rectangle(register const gx_clip_path * pcpath,
                            fixed x0, fixed y0, fixed x1, fixed y1)
{
    return
        (x0 <= x1 ?
         (pcpath->inner_box.p.x <= x0 && x1 <= pcpath->inner_box.q.x) :
         (pcpath->inner_box.p.x <= x1 && x0 <= pcpath->inner_box.q.x)) &&
        (y0 <= y1 ?
         (pcpath->inner_box.p.y <= y0 && y1 <= pcpath->inner_box.q.y) :
         (pcpath->inner_box.p.y <= y1 && y0 <= pcpath->inner_box.q.y));
}

/* Set the outer clipping box to the path bounding box, */
/* expanded to pixel boundaries. */
void
gx_cpath_set_outer_box(gx_clip_path * pcpath)
{
    pcpath->outer_box.p.x = fixed_floor(pcpath->path.bbox.p.x);
    pcpath->outer_box.p.y = fixed_floor(pcpath->path.bbox.p.y);
    pcpath->outer_box.q.x = fixed_ceiling(pcpath->path.bbox.q.x);
    pcpath->outer_box.q.y = fixed_ceiling(pcpath->path.bbox.q.y);
}

/* Return the rectangle list of a clipping path (for local use only). */
const gx_clip_list *
gx_cpath_list(const gx_clip_path *pcpath)
{
    return &pcpath->rect_list->list;
}
/* Internal non-const version of the same accessor. */
static inline gx_clip_list *
gx_cpath_list_private(const gx_clip_path *pcpath)
{
    return &pcpath->rect_list->list;
}

/* ------ Clipping path setting ------ */

/* Create a rectangular clipping path. */
/* The supplied rectangle may not be oriented correctly, */
/* but it will be oriented correctly upon return. */
static int
cpath_set_rectangle(gx_clip_path * pcpath, gs_fixed_rect * pbox)
{
    gx_clip_rect_list *rlist = pcpath->rect_list;

    if (rlist->rc.ref_count <= 1)
        gx_clip_list_free(&rlist->list, rlist->rc.memory);
    else {
        int code = cpath_alloc_list(&pcpath->rect_list, pcpath->path.memory,
                                    "gx_cpath_from_rectangle");

        if (code < 0) {
            pcpath->rect_list = rlist;
            return code;
        }
        rc_decrement(rlist, "gx_cpath_from_rectangle");
        rlist = pcpath->rect_list;
    }
    cpath_init_rectangle(pcpath, pbox);
    return 0;
}
int
gx_cpath_from_rectangle(gx_clip_path * pcpath, gs_fixed_rect * pbox)
{
    int code = gx_path_new(&pcpath->path);

    if (code < 0)
        return code;
    return cpath_set_rectangle(pcpath, pbox);
}
int
gx_cpath_reset(gx_clip_path * pcpath)
{
    gs_fixed_rect null_rect;

    null_rect.p.x = null_rect.p.y = null_rect.q.x = null_rect.q.y = 0;
    rc_decrement(pcpath->path_list, "gx_cpath_reset");
    return gx_cpath_from_rectangle(pcpath, &null_rect);
}

/* If a clipping path is a rectangle, return the rectangle.
 * If its a rectangular path, also return the rectangle.
 */
gx_path_rectangular_type cpath_is_rectangle(const gx_clip_path * pcpath, gs_fixed_rect *rect)
{
    if (pcpath->path_valid) {
        return gx_path_is_rectangle((const gx_path *)&pcpath->path, rect);
    }
    if (pcpath->inner_box.p.x != pcpath->path.bbox.p.x ||
        pcpath->inner_box.p.y != pcpath->path.bbox.p.y ||
        pcpath->inner_box.q.x != pcpath->path.bbox.q.x ||
        pcpath->inner_box.q.y != pcpath->path.bbox.q.y)
        return prt_none;
    rect->p.x = pcpath->inner_box.p.x;
    rect->p.y = pcpath->inner_box.p.y;
    rect->q.x = pcpath->inner_box.q.x;
    rect->q.y = pcpath->inner_box.q.y;
    return prt_closed;
}

/* Intersect a new clipping path with an old one. */
/* Flatten the new path first (in a copy) if necessary. */
int
gx_cpath_clip(gs_gstate *pgs, gx_clip_path *pcpath,
              /*const*/ gx_path *ppath_orig, int rule)
{
    return gx_cpath_intersect(pcpath, ppath_orig, rule, pgs);
}

int
gx_cpath_ensure_path_list(gx_clip_path *pcpath)
{
    if (pcpath == NULL || pcpath->path_list)
        return 0;
    return gx_cpath_path_list_new(pcpath->path.memory, pcpath, pcpath->rule,
                                  &pcpath->path, NULL, &pcpath->path_list);
}

int
gx_cpath_intersect_with_params(gx_clip_path *pcpath, /*const*/ gx_path *ppath_orig,
                   int rule, gs_gstate *pgs, const gx_fill_params * params)
{
    gx_path fpath;
    /*const*/ gx_path *ppath = ppath_orig;
    gs_fixed_rect old_box, new_box;
    int code;
    int pcpath_is_rect;

    pcpath->cached = NULL;
    /* Flatten the path if necessary. */
    if (gx_path_has_curves_inline(ppath)) {
        gx_path_init_local(&fpath, pgs->memory);
        code = gx_path_add_flattened_accurate(ppath, &fpath,
                                              gs_currentflat_inline(pgs),
                                              pgs->accurate_curves);
        if (code < 0)
            return code;
        ppath = &fpath;
    }

    pcpath_is_rect = gx_cpath_inner_box(pcpath, &old_box);
    if (pcpath_is_rect &&
        ((code = gx_path_is_rectangle(ppath, &new_box)) ||
         gx_path_is_void(ppath))
        ) {
        int changed = 0;

        if (!code) {
            /* The new path is void. */
            if (gx_path_current_point(ppath, &new_box.p) < 0) {
                /* Use the user space origin (arbitrarily). */
                new_box.p.x = float2fixed(pgs->ctm.tx);
                new_box.p.y = float2fixed(pgs->ctm.ty);
            }
            new_box.q = new_box.p;
            changed = 1;
        } else {
            {   /* Apply same adjustment as for filling the path. */
                gs_fixed_point adjust = params != NULL ? params->adjust : pgs->fill_adjust;
                fixed adjust_xl, adjust_xu, adjust_yl, adjust_yu;

                if (adjust.x == -1)
                    adjust_xl = adjust_xu = adjust_yl = adjust_yu = 0;
                else {
                    adjust_xl = (adjust.x == fixed_half ? fixed_half - fixed_epsilon : adjust.x);
                    adjust_yl = (adjust.y == fixed_half ? fixed_half - fixed_epsilon : adjust.y);
                    adjust_xu = adjust.x;
                    adjust_yu = adjust.y;
                }
                new_box.p.x = int2fixed(fixed2int_pixround(new_box.p.x - adjust_xl));
                new_box.p.y = int2fixed(fixed2int_pixround(new_box.p.y - adjust_yl));
                new_box.q.x = int2fixed(fixed2int_pixround(new_box.q.x + adjust_xu));
                new_box.q.y = int2fixed(fixed2int_pixround(new_box.q.y + adjust_yu));
            }
            /* Intersect the two rectangles if necessary. */
            if (old_box.p.x >= new_box.p.x)
                new_box.p.x = old_box.p.x, ++changed;
            if (old_box.p.y >= new_box.p.y)
                new_box.p.y = old_box.p.y, ++changed;
            if (old_box.q.x <= new_box.q.x)
                new_box.q.x = old_box.q.x, ++changed;
            if (old_box.q.y <= new_box.q.y)
                new_box.q.y = old_box.q.y, ++changed;
            /* Check for a degenerate rectangle. */
            if (new_box.q.x < new_box.p.x || new_box.q.y < new_box.p.y)
                new_box.p = new_box.q, changed = 1;
        }
        if (changed == 4) {
            /* The new box/path is the same as the old. */
            return 0;
        }
        /* Release the existing path. */
        rc_decrement(pcpath->path_list, "gx_cpath_intersect");
        pcpath->path_list = NULL;
        gx_path_new(&pcpath->path);
        ppath->bbox = new_box;
        cpath_set_rectangle(pcpath, &new_box);
        if (changed == 0) {
            /* The path is valid; otherwise, defer constructing it. */
            gx_path_assign_preserve(&pcpath->path, ppath);
            pcpath->path_valid = true;
            pcpath->path_fill_adjust = params != NULL ? params->adjust : pgs->fill_adjust;
        }
    } else {
        /* New clip path is nontrivial.  Intersect the slow way. */
        gx_cpath_path_list *next = NULL;
        bool path_valid =
            pcpath_is_rect &&
            gx_path_bbox(ppath, &new_box) >= 0 &&
            gx_cpath_includes_rectangle(pcpath,
                                        new_box.p.x, new_box.p.y,
                                        new_box.q.x, new_box.q.y);

        if (!path_valid && next == NULL) {
            /* gx_cpaths should generally have a path_list set within
             * them. In some cases (filled images), they may not. Ensure
             * that they do, and remember the path_list */
            code = gx_cpath_ensure_path_list(pcpath);
            if (code < 0)
                goto ex;
            /* gx_cpath_intersect_path_slow NULLs pcpath->path_list, so
             * remember it here. */
            next = pcpath->path_list;
            rc_increment(next);
        }
        code = gx_cpath_intersect_path_slow(pcpath, (params != NULL ? ppath_orig : ppath),
                            rule, pgs, params);
        if (code < 0) {
            rc_decrement(next, "gx_cpath_clip");
            goto ex;
        }
        if (path_valid) {
            gx_path_assign_preserve(&pcpath->path, ppath_orig);
            pcpath->path_valid = true;
            pcpath->path_fill_adjust = params != NULL ? params->adjust : pgs->fill_adjust;
            pcpath->rule = rule;
        } else {
            code = gx_cpath_path_list_new(pcpath->path.memory, NULL, rule,
                                          ppath_orig, next, &pcpath->path_list);
        }
        rc_decrement(next, "gx_cpath_clip");
    }
ex:
    if (ppath != ppath_orig)
        gx_path_free(ppath, "gx_cpath_clip");
    return code;
}
int
gx_cpath_intersect(gx_clip_path *pcpath, /*const*/ gx_path *ppath_orig,
                   int rule, gs_gstate *pgs)
{
    return gx_cpath_intersect_with_params(pcpath, ppath_orig,
                   rule, pgs, NULL);
}

/* Scale a clipping path by a power of 2. */
int
gx_cpath_scale_exp2_shared(gx_clip_path * pcpath, int log2_scale_x,
                           int log2_scale_y, bool list_shared,
                           bool segments_shared)
{
    int code =
        (pcpath->path_valid ?
         gx_path_scale_exp2_shared(&pcpath->path, log2_scale_x, log2_scale_y,
                                   segments_shared) :
         0);
    gx_clip_list *list = gx_cpath_list_private(pcpath);
    gx_clip_rect *pr;

    if (code < 0)
        return code;
    /* Scale the fixed entries. */
    gx_rect_scale_exp2(&pcpath->inner_box, log2_scale_x, log2_scale_y);
    gx_rect_scale_exp2(&pcpath->outer_box, log2_scale_x, log2_scale_y);
    if (!list_shared) {
        /* Scale the clipping list. */
        pr = list->head;
        if (pr == 0)
            pr = &list->single;
        for (; pr != 0; pr = pr->next)
            if (pr != list->head && pr != list->tail) {

#define SCALE_V(v, s)\
  if ( pr->v != min_int && pr->v != max_int )\
    pr->v = (s >= 0 ? pr->v << s : pr->v >> -s)

                SCALE_V(xmin, log2_scale_x);
                SCALE_V(xmax, log2_scale_x);
                SCALE_V(ymin, log2_scale_y);
                SCALE_V(ymax, log2_scale_y);
#undef SCALE_V
            }
        if (log2_scale_x > 0) {
            list->xmin <<= log2_scale_x;
            list->xmax <<= log2_scale_x;
        } else {
            list->xmin = arith_rshift(list->xmin, -log2_scale_x);
            list->xmax = arith_rshift(list->xmax, -log2_scale_x);
        }
    }
    pcpath->id = gs_next_ids(pcpath->path.memory, 1);	/* path changed => change id */
    return 0;
}

/* ------ Clipping list routines ------ */

/* Initialize a clip list. */
void
gx_clip_list_init(gx_clip_list * clp)
{
    *clp = clip_list_empty;
}

/* Initialize a clip list to a rectangle. */
/* The supplied rectangle may not be oriented correctly, */
/* but it will be oriented correctly upon return. */
static void
gx_clip_list_from_rectangle(register gx_clip_list * clp,
                            register gs_fixed_rect * rp)
{
    gx_clip_list_init(clp);
    if (rp->p.x > rp->q.x) {
        fixed t = rp->p.x;

        rp->p.x = rp->q.x;
        rp->q.x = t;
    }
    if (rp->p.y > rp->q.y) {
        fixed t = rp->p.y;

        rp->p.y = rp->q.y;
        rp->q.y = t;
    }
    clp->single.xmin = clp->xmin = fixed2int_var(rp->p.x);
    clp->single.ymin = fixed2int_var(rp->p.y);
    /* Handle degenerate rectangles specially. */
    clp->single.xmax = clp->xmax =
        (rp->q.x == rp->p.x ? clp->single.xmin :
         fixed2int_var_ceiling(rp->q.x));
    clp->single.ymax =
        (rp->q.y == rp->p.y ? clp->single.ymin :
         fixed2int_var_ceiling(rp->q.y));
    clp->count = 1;
}

/* Start enumerating a clipping path. */
int
gx_cpath_enum_init(gs_cpath_enum * penum, const gx_clip_path * pcpath)
{
    if ((penum->using_path = pcpath->path_valid)) {
        gx_path_enum_init(&penum->path_enum, &pcpath->path);
        penum->rp = penum->visit = 0;
        penum->first_visit = visit_left;
    } else {
        gx_path empty_path;
        gx_clip_list *clp = gx_cpath_list_private(pcpath);
        gx_clip_rect *head = (clp->count <= 1 ? &clp->single : clp->head);
        gx_clip_rect *rp;

        /* Initialize the pointers in the path_enum properly. */
        gx_path_init_local(&empty_path, pcpath->path.memory);
        gx_path_enum_init(&penum->path_enum, &empty_path);
        penum->first_visit = visit_left;
        penum->visit = head;
        for (rp = head; rp != 0; rp = rp->next)
            rp->to_visit =
                (rp->xmin < rp->xmax && rp->ymin < rp->ymax ?
                 visit_left | visit_right : 0);
        penum->rp = 0;		/* scan will initialize */
        penum->any_rectangles = false;
        penum->state = cpe_scan;
        penum->have_line = false;
    }
    return 0;
}

/* Enumerate the next segment of a clipping path. */
/* In general, this produces a path made up of zillions of tiny lines. */
int
gx_cpath_enum_next(gs_cpath_enum * penum, gs_fixed_point pts[3])
{
    if (penum->using_path)
        return gx_path_enum_next(&penum->path_enum, pts);
#define set_pt(xi, yi)\
  (pts[0].x = int2fixed(xi), pts[0].y = int2fixed(yi))
#define set_line(xi, yi)\
  (penum->line_end.x = (xi), penum->line_end.y = (yi), penum->have_line = true)
    if (penum->have_line) {
        set_pt(penum->line_end.x, penum->line_end.y);
        penum->have_line = false;
        return gs_pe_lineto;
    } {
        gx_clip_rect *visit = penum->visit;
        gx_clip_rect *rp = penum->rp;
        cpe_visit_t first_visit = penum->first_visit;
        cpe_state_t state = penum->state;
        gx_clip_rect *look;
        int code;

        switch (state) {

            case cpe_scan:
                /* Look for the start of an edge to trace. */
                for (; visit != 0; visit = visit->next) {
                    if (visit->to_visit & visit_left) {
                        set_pt(visit->xmin, visit->ymin);
                        first_visit = visit_left;
                        state = cpe_left;
                    } else if (visit->to_visit & visit_right) {
                        set_pt(visit->xmax, visit->ymax);
                        first_visit = visit_right;
                        state = cpe_right;
                    } else
                        continue;
                    rp = visit;
                    code = gs_pe_moveto;
                    penum->any_rectangles = true;
                    goto out;
                }
                /* We've enumerated all the edges. */
                state = cpe_done;
                if (!penum->any_rectangles) {
                    /* We didn't have any rectangles. */
                    set_pt(fixed_0, fixed_0);
                    code = gs_pe_moveto;
                    break;
                }
                /* falls through */

            case cpe_done:
                /* All done. */
                code = 0;
                break;

/* We can't use the BEGIN ... END hack here: we need to be able to break. */
#define return_line(px, py)\
  set_pt(px, py); code = gs_pe_lineto; break

            case cpe_left:

              left:		/* Trace upward along a left edge. */
                /* We're at the lower left corner of rp. */
                rp->to_visit &= ~visit_left;
                /* Look for an adjacent rectangle above rp. */
                for (look = rp;
                     (look = look->next) != 0 &&
                     (look->ymin == rp->ymin ||
                      (look->ymin == rp->ymax && look->xmax <= rp->xmin));
                    );
                /* Now we know look->ymin >= rp->ymax. */
                if (look == 0 || look->ymin > rp->ymax ||
                    look->xmin >= rp->xmax
                    ) {		/* No adjacent rectangle, switch directions. */
                    state =
                        (rp == visit && first_visit == visit_right ? cpe_close :
                         (set_line(rp->xmax, rp->ymax), cpe_right));
                    return_line(rp->xmin, rp->ymax);
                }
                /* We found an adjacent rectangle. */
                /* See if it also adjoins a rectangle to the left of rp. */
                {
                    gx_clip_rect *prev = rp->prev;
                    gx_clip_rect *cur = rp;

                    if (prev != 0 && prev->ymax == rp->ymax &&
                        look->xmin < prev->xmax
                        ) {	/* There's an adjoining rectangle as well. */
                        /* Switch directions. */
                        rp = prev;
                        state =
                            (rp == visit && first_visit == visit_right ? cpe_close :
                             (set_line(prev->xmax, prev->ymax), cpe_right));
                        return_line(cur->xmin, cur->ymax);
                    }
                    rp = look;
                    if (rp == visit && first_visit == visit_left)
                        state = cpe_close;
                    else if (rp->xmin == cur->xmin)
                        goto left;
                    else
                        set_line(rp->xmin, rp->ymin);
                    return_line(cur->xmin, cur->ymax);
                }

            case cpe_right:

              right:		/* Trace downward along a right edge. */
                /* We're at the upper right corner of rp. */
                rp->to_visit &= ~visit_right;
                /* Look for an adjacent rectangle below rp. */
                for (look = rp;
                     (look = look->prev) != 0 &&
                     (look->ymax == rp->ymax ||
                      (look->ymax == rp->ymin && look->xmin >= rp->xmax));
                    );
                /* Now we know look->ymax <= rp->ymin. */
                if (look == 0 || look->ymax < rp->ymin ||
                    look->xmax <= rp->xmin
                    ) {		/* No adjacent rectangle, switch directions. */
                    state =
                        (rp == visit && first_visit == visit_left ? cpe_close :
                         (set_line(rp->xmin, rp->ymin), cpe_left));
                    return_line(rp->xmax, rp->ymin);
                }
                /* We found an adjacent rectangle. */
                /* See if it also adjoins a rectangle to the right of rp. */
                {
                    gx_clip_rect *next = rp->next;
                    gx_clip_rect *cur = rp;

                    if (next != 0 && next->ymin == rp->ymin &&
                        look->xmax > next->xmin
                        ) {	/* There's an adjoining rectangle as well. */
                        /* Switch directions. */
                        rp = next;
                        state =
                            (rp == visit && first_visit == visit_left ? cpe_close :
                             (set_line(next->xmin, next->ymin), cpe_left));
                        return_line(cur->xmax, cur->ymin);
                    }
                    rp = look;
                    if (rp == visit && first_visit == visit_right)
                        state = cpe_close;
                    else if (rp->xmax == cur->xmax)
                        goto right;
                    else
                        set_line(rp->xmax, rp->ymax);
                    return_line(cur->xmax, cur->ymin);
                }

#undef return_line

            case cpe_close:
                /* We've gone all the way around an edge. */
                code = gs_pe_closepath;
                state = cpe_scan;
                break;

            default:
                return_error(gs_error_unknownerror);
        }

      out:			/* Store the state before exiting. */
        penum->visit = visit;
        penum->rp = rp;
        penum->first_visit = first_visit;
        penum->state = state;
        return code;
    }
#undef set_pt
#undef set_line
}
segment_notes
gx_cpath_enum_notes(const gs_cpath_enum * penum)
{
    return sn_none;
}

/* Free a clip list. */
void
gx_clip_list_free(gx_clip_list * clp, gs_memory_t * mem)
{
    gx_clip_rect *rp = clp->tail;

    while (rp != 0) {
        gx_clip_rect *prev = rp->prev;

        gs_free_object(mem, rp, "gx_clip_list_free");
        rp = prev;
    }
    gx_clip_list_init(clp);
}

/* Check whether a rectangle has a non-empty intersection with a clipping patch. */
bool
gx_cpath_rect_visible(gx_clip_path * pcpath, gs_int_rect *prect)
{
    const gx_clip_rect *pr;
    const gx_clip_list *list = &pcpath->rect_list->list;

    switch (list->count) {
        case 0:
            return false;
        case 1:
            pr = &list->single;
            break;
        default:
            pr = list->head;
    }
    for (; pr != 0; pr = pr->next) {
        if (pr->xmin > prect->q.x)
            continue;
        if (pr->xmax < prect->p.x)
            continue;
        if (pr->ymin > prect->q.y)
            continue;
        if (pr->ymax < prect->p.y)
            continue;
        return true;
    }
    return false;
}

int
gx_cpath_copy(const gx_clip_path * from, gx_clip_path * pcpath)
{   /* *pcpath must be initialized. */
    gx_clip_rect *r, *s;
    gx_clip_list *l = &pcpath->rect_list->list;

    pcpath->path_valid = false;
    /* NOTE: pcpath->path still contains the old path. */
    if (pcpath->path_list)
        rc_decrement(pcpath->path_list, "gx_cpath_copy");
    pcpath->path_list = NULL;
    pcpath->rule = from->rule;
    pcpath->outer_box = from->outer_box;
    pcpath->inner_box = from->inner_box;
    pcpath->cached = NULL;
    l->single = from->rect_list->list.single;
    for (r = from->rect_list->list.head; r != NULL; r = r->next) {
        if (pcpath->rect_list->rc.memory == NULL)
            s = gs_alloc_struct(from->rect_list->rc.memory, gx_clip_rect, &st_clip_rect, "gx_cpath_copy");
        else
            s = gs_alloc_struct(pcpath->rect_list->rc.memory, gx_clip_rect, &st_clip_rect, "gx_cpath_copy");

        if (s == NULL)
            return_error(gs_error_VMerror);
        *s = *r;
        s->next = NULL;
        if (l->tail) {
            s->prev = l->tail;
            l->tail->next = s;
        } else {
            l->head = s;
            s->prev = NULL;
        }
        l->tail = s;
    }
    l->count = from->rect_list->list.count;
    return 0;
}

/* ------ Debugging printout ------ */

#ifdef DEBUG

/* Print a clipping list. */
static void
gx_clip_list_print(const gs_memory_t *mem, const gx_clip_list *list)
{
    const gx_clip_rect *pr;

    dmlprintf3(mem, "   list count=%d xmin=%d xmax=%d\n",
               list->count, list->xmin, list->xmax);
    switch (list->count) {
        case 0:
            pr = 0;
            break;
        case 1:
            pr = &list->single;
            break;
        default:
            pr = list->head;
    }
    for (; pr != 0; pr = pr->next)
        dmlprintf4(mem, "   rect: (%d,%d),(%d,%d)\n",
                   pr->xmin, pr->ymin, pr->xmax, pr->ymax);
}

/* Print a clipping path */
void
gx_cpath_print(const gs_memory_t *mem, const gx_clip_path * pcpath)
{
    if (pcpath->path_valid)
        gx_path_print(&pcpath->path);
    else
        dmlputs(mem, "   (path not valid)\n");
    dmlprintf4(mem, "   inner_box=(%g,%g),(%g,%g)\n",
               fixed2float(pcpath->inner_box.p.x),
               fixed2float(pcpath->inner_box.p.y),
               fixed2float(pcpath->inner_box.q.x),
               fixed2float(pcpath->inner_box.q.y));
    dmlprintf4(mem, "     outer_box=(%g,%g),(%g,%g)",
               fixed2float(pcpath->outer_box.p.x),
               fixed2float(pcpath->outer_box.p.y),
               fixed2float(pcpath->outer_box.q.x),
               fixed2float(pcpath->outer_box.q.y));
    dmprintf2(mem, "     rule=%d list.refct=%ld\n",
              pcpath->rule, pcpath->rect_list->rc.ref_count);
    gx_clip_list_print(mem, gx_cpath_list(pcpath));
}

#endif /* DEBUG */
