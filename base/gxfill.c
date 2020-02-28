/* Copyright (C) 2001-2019 Artifex Software, Inc.
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


/* A topological spot decomposition algorithm with dropout prevention. */
/*
   This is a dramaticly reorganized and improved revision of the
   filling algorithm, which was initially coded by Henry Stiles.
   The main improvements are:
        1. A dropout prevention for character fill.
        2. The spot topology analysis support
           for True Type grid fitting.
        3. Fixed the contiguity of a spot covering
           for shading fills with no dropouts.
*/
/* See is_spotan about the spot topology analysis support. */
/* Also defining lower-level path filling procedures */

#include "gx.h"
#include "gserrors.h"
#include "gsstruct.h"
#include "gxfixed.h"
#include "gxdevice.h"
#include "gzpath.h"
#include "gzcpath.h"
#include "gxdcolor.h"
#include "gxhttile.h"
#include "gxgstate.h"
#include "gxpaint.h"            /* for prototypes */
#include "gxfill.h"
#include "gxpath.h"
#include "gsptype1.h"
#include "gsptype2.h"
#include "gxpcolor.h"
#include "gdevddrw.h"
#include "gzspotan.h" /* Only for gx_san_trap_store. */
#include "memory_.h"
#include "stdint_.h"
#include "gsstate.h"            /* for gs_currentcpsimode */
#include "gxdevsop.h"
#include "gxscanc.h"
/*
#include "gxfilltr.h" - Do not remove this comment. "gxfilltr.h" is included below.
#include "gxfillsl.h" - Do not remove this comment. "gxfillsl.h" is included below.
#include "gxfillts.h" - Do not remove this comment. "gxfillts.h" is included below.
*/
#include "assert_.h"

/* #define ENABLE_TRAP_AMALGAMATION */

/*
 * ENABLE_TRAP_AMALGAMATION - Experimental trap amalgamation code, disabled
 * by default.
 *
 * Set this if you think that the potential gains from only having to send
 * one trapezoid rather than 3 trapezoids justifies the costs in
 * calculating whether this is possible.
 *
 * Note: Due to rounding inaccuracies while scan converting, there are
 * cases where the rectangles can round to different pixel boundaries than
 * the trapezoids do. This means that enabling the TRY_TO_EXTEND_TRAP
 * define will cause rendering differences, but tests seem to indicate that
 * this only happens in very borderline cases.
 */
#ifdef ENABLE_TRAP_AMALGAMATION
#define TRY_TO_EXTEND_TRAP 1
#else
#define TRY_TO_EXTEND_TRAP 0
#endif

#if defined(DEBUG) && !defined(GS_THREADSAFE)
/* Define the statistics structure instance. */
stats_fill_t stats_fill;
#endif

/* A predicate for spot insideness. */
/* rule = -1 for winding number rule, i.e. */
/* we are inside if the winding number is non-zero; */
/* rule = 1 for even-odd rule, i.e. */
/* we are inside if the winding number is odd. */
#define INSIDE_PATH_P(inside, rule) ((inside & rule) != 0)

/* ---------------- Active line management ---------------- */

/*
 * Define the ordering criterion for active lines that overlap in Y.
 * Return -1, 0, or 1 if lp1 precedes, coincides with, or follows lp2.
 *
 * The lines' x_current values are correct for some Y value that crosses
 * both of them and that is not both the start of one and the end of the
 * other.  (Neither line is horizontal.)  We want the ordering at this
 * Y value, or, of the x_current values are equal, greater Y values
 * (if any: this Y value might be the end of both lines).
 */
static int
x_order(const active_line *lp1, const active_line *lp2)
{
    bool s1;

    INCR(order);
    if (!lp1 || !lp2 || lp1->x_current < lp2->x_current)
        return -1;
    else if (lp1->x_current > lp2->x_current)
        return 1;
    /*
     * We need to compare the slopes of the lines.  Start by
     * checking one fast case, where the slopes have opposite signs.
     */
    if ((s1 = lp1->start.x < lp1->end.x) != (lp2->start.x < lp2->end.x))
        return (s1 ? 1 : -1);
    /*
     * We really do have to compare the slopes.  Fortunately, this isn't
     * needed very often.  We want the sign of the comparison
     * dx1/dy1 - dx2/dy2, or (since we know dy1 and dy2 are positive)
     * dx1 * dy2 - dx2 * dy1.  In principle, we can't simply do this using
     * doubles, since we need complete accuracy and doubles don't have
     * enough fraction bits.  However, with the usual 20+12-bit fixeds and
     * 64-bit doubles, both of the factors would have to exceed 2^15
     * device space pixels for the result to be inaccurate, so we
     * simply disregard this possibility.  ****** FIX SOMEDAY ******
     */
    INCR(slow_order);
    {
        fixed dx1 = lp1->end.x - lp1->start.x,
            dy1 = lp1->end.y - lp1->start.y;
        fixed dx2 = lp2->end.x - lp2->start.x,
            dy2 = lp2->end.y - lp2->start.y;
        double diff = (double)dx1 * dy2 - (double)dx2 * dy1;

        return (diff < 0 ? -1 : diff > 0 ? 1 : 0);
    }
}

/*
 * The active_line structure isn't really simple, but since its instances
 * only exist temporarily during a fill operation, we don't have to
 * worry about a garbage collection occurring.
 */
gs_private_st_simple(st_active_line, active_line, "active_line");

#ifdef DEBUG
/* Internal procedures for printing and checking active lines. */
static void
print_active_line(const gs_memory_t *mem, const char *label, const active_line * alp)
{
    dmlprintf5(mem, "[f]%s 0x%lx(%d): x_current=%f x_next=%f\n",
               label, (ulong) alp, alp->direction,
               fixed2float(alp->x_current), fixed2float(alp->x_next));
    dmlprintf5(mem, "    start=(%f,%f) pt_end=0x%lx(%f,%f)\n",
               fixed2float(alp->start.x), fixed2float(alp->start.y),
               (ulong) alp->pseg,
               fixed2float(alp->end.x), fixed2float(alp->end.y));
    dmlprintf2(mem, "    prev=0x%lx next=0x%lx\n",
               (ulong) alp->prev, (ulong) alp->next);
}
static void
print_line_list(const gs_memory_t *mem, const active_line * flp)
{
    const active_line *lp;

    for (lp = flp; lp != 0; lp = lp->next) {
        fixed xc = lp->x_current, xn = lp->x_next;

        dmlprintf3(mem, "[f]0x%lx(%d): x_current/next=%g",
                  (ulong) lp, lp->direction,
                  fixed2float(xc));
        if (xn != xc)
            dmprintf1(mem, "/%g", fixed2float(xn));
        dmputc(mem, '\n');
    }
}
static inline void
print_al(const gs_memory_t *mem, const char *label, const active_line * alp)
{
    if (gs_debug_c('F'))
        print_active_line(mem, label, alp);
}
#else
#define print_al(mem,label,alp) DO_NOTHING
#endif

static inline bool
is_spotan_device(gx_device * dev)
{
    /* Use open_device procedure to identify the type of the device
     * instead of the standard gs_object_type() because gs_cpath_accum_device
     * is allocaded on the stack i.e. has no block header with a descriptor
     * but has dev->memory set like a heap-allocated device.
     */
    return dev_proc(dev, open_device) == san_open;
}

/* Forward declarations */
static void free_line_list(line_list *);
static int add_y_list(gx_path *, line_list *);
static int add_y_line_aux(const segment * prev_lp, const segment * lp,
            const gs_fixed_point *curr, const gs_fixed_point *prev, int dir, line_list *ll);
static void insert_x_new(active_line *, line_list *);
static int  end_x_line(active_line *, const line_list *, bool);
static int step_al(active_line *alp, bool move_iterator);

#define FILL_LOOP_PROC(proc) int proc(line_list *, fixed band_mask)
static FILL_LOOP_PROC(spot_into_scan_lines);
static FILL_LOOP_PROC(spot_into_trapezoids);

/*
 * This is the general path filling algorithm.
 * It uses the center-of-pixel rule for filling
 * We can implement Microsoft's upper-left-corner-of-pixel rule
 * by subtracting (0.5, 0.5) from all the coordinates in the path.
 *
 * The adjust parameters are a hack for keeping regions
 * from coming out too faint: they specify an amount by which to expand
 * the sides of every filled region.
 * Setting adjust = fixed_half is supposed to produce the effect of Adobe's
 * any-part-of-pixel rule, but it doesn't quite, because of the
 * closed/open interval rule for regions.  We detect this as a special case
 * and do the slightly ugly things necessary to make it work.
 */

/* Initialize the line list for a path. */
static inline void
init_line_list(line_list *ll, gs_memory_t * mem)
{
    ll->memory = mem;
    ll->active_area = 0;
    ll->next_active = ll->local_active;
    ll->limit = ll->next_active + MAX_LOCAL_ACTIVE;
    ll->close_count = 0;
    ll->y_list = 0;
    ll->y_line = 0;
    ll->h_list0 = ll->h_list1 = 0;

    ll->x_head.prev = NULL;
    /* Bug 695234: Initialise the following to pacify valgrind */
    ll->x_head.start.x = 0;
    ll->x_head.start.y = 0;
    ll->x_head.end.x = 0;
    ll->x_head.end.y = 0;

    /* Do not initialize ll->bbox_left, ll->bbox_width - they were set in advance. */
    INCR(fill);
}

/* Unlink any line_close segments added temporarily. */
static inline void
unclose_path(gx_path * ppath, int count)
{
    subpath *psub;

    for (psub = ppath->first_subpath; count != 0;
         psub = (subpath *) psub->last->next
        )
        if (psub->last == (segment *) & psub->closer) {
            segment *prev = psub->closer.prev, *next = psub->closer.next;

            prev->next = next;
            if (next)
                next->prev = prev;
            psub->last = prev;
            count--;
        }
}

/*
 * The general fill  path algorithm.
 */
static int
gx_general_fill_path(gx_device * pdev, const gs_gstate * pgs,
                     gx_path * ppath, const gx_fill_params * params,
                 const gx_device_color * pdevc, const gx_clip_path * pcpath)
{
    gs_fixed_point adjust;
    gs_logical_operation_t lop = pgs->log_op;
    gs_fixed_rect ibox, bbox, sbox;
    gx_device_clip cdev;
    gx_device *dev = pdev;
    gx_device *save_dev = dev;
    gx_path ffpath;
    gx_path *pfpath;
    int code;
    int max_fill_band = dev->max_fill_band;
#define NO_BAND_MASK ((fixed)(-1) << (sizeof(fixed) * 8 - 1))
    const bool is_character = params->adjust.x == -1; /* See gxgstate.h */
    bool fill_by_trapezoids;
    bool big_path = ppath->subpath_count > 50;
    fill_options fo;
    line_list lst;
    int clipping = 0;
    int scanconverter;

    *(const fill_options **)&lst.fo = &fo; /* break 'const'. */
    /*
     * Compute the bounding box before we flatten the path.
     * This can save a lot of time if the path has curves.
     * If the path is neither fully within nor fully outside
     * the quick-check boxes, we could recompute the bounding box
     * and make the checks again after flattening the path,
     * but right now we don't bother.
     */
    gx_path_bbox(ppath, &ibox);
    if (is_character)
        adjust.x = adjust.y = 0;
    else
        adjust = params->adjust;
    lst.contour_count = 0;
    lst.windings = NULL;
    lst.bbox_left = fixed2int(ibox.p.x - adjust.x - fixed_epsilon);
    lst.bbox_width = fixed2int(fixed_ceiling(ibox.q.x + adjust.x)) - lst.bbox_left;
    /* Check the bounding boxes. */
    if_debug6m('f', pdev->memory, "[f]adjust=%g,%g bbox=(%g,%g),(%g,%g)\n",
               fixed2float(adjust.x), fixed2float(adjust.y),
               fixed2float(ibox.p.x), fixed2float(ibox.p.y),
               fixed2float(ibox.q.x), fixed2float(ibox.q.y));
    if (pcpath)
        gx_cpath_inner_box(pcpath, &bbox);
    else
        (*dev_proc(dev, get_clipping_box)) (dev, &bbox);
    if (!rect_within(ibox, bbox)) {
        /*
         * Intersect the path box and the clip bounding box.
         * If the intersection is empty, this fill is a no-op.
         */
        if (pcpath)
            gx_cpath_outer_box(pcpath, &bbox);
        if_debug4m('f', pdev->memory, "   outer_box=(%g,%g),(%g,%g)\n",
                   fixed2float(bbox.p.x), fixed2float(bbox.p.y),
                   fixed2float(bbox.q.x), fixed2float(bbox.q.y));
        rect_intersect(ibox, bbox);
        if (ibox.p.x - adjust.x >= ibox.q.x + adjust.x ||
            ibox.p.y - adjust.y >= ibox.q.y + adjust.y
            ) {                 /* Intersection of boxes is empty! */
            return 0;
        }
        /*
         * The path is neither entirely inside the inner clip box
         * nor entirely outside the outer clip box.
         * If we had to flatten the path, this is where we would
         * recompute its bbox and make the tests again,
         * but we don't bother right now.
         *
         * If there is a clipping path, set up a clipping device.
         */
        if (pcpath) {
            dev = (gx_device *) & cdev;
            gx_make_clip_device_on_stack(&cdev, pcpath, save_dev);
            cdev.max_fill_band = save_dev->max_fill_band;
            clipping = 1;
        }
    }
    /*
     * Compute the proper adjustment values.
     * To get the effect of the any-part-of-pixel rule,
     * we may have to tweak them slightly.
     * NOTE: We changed the adjust_right/above value from 0.5+epsilon
     * to 0.5 in release 5.01; even though this does the right thing
     * in every case we could imagine, we aren't confident that it's
     * correct.  (The old values were definitely incorrect, since they
     * caused 1-pixel-wide/high objects to color 2 pixels even if
     * they fell exactly on pixel boundaries.)
     */
    if (adjust.x == fixed_half)
        fo.adjust_left = fixed_half - fixed_epsilon,
            fo.adjust_right = fixed_half /* + fixed_epsilon */ ;        /* see above */
    else
        fo.adjust_left = fo.adjust_right = adjust.x;
    if (adjust.y == fixed_half)
        fo.adjust_below = fixed_half - fixed_epsilon,
            fo.adjust_above = fixed_half /* + fixed_epsilon */ ;        /* see above */
    else
        fo.adjust_below = fo.adjust_above = adjust.y;
    sbox.p.x = ibox.p.x - adjust.x;
    sbox.p.y = ibox.p.y - adjust.y;
    sbox.q.x = ibox.q.x + adjust.x;
    sbox.q.y = ibox.q.y + adjust.y;
    fo.pdevc = pdevc;
    fo.lop = lop;
    fo.fixed_flat = float2fixed(params->flatness);
    fo.ymin = ibox.p.y;
    fo.ymax = ibox.q.y;
    fo.dev = dev;
    fo.pbox = &sbox;
    fo.rule = params->rule;
    fo.is_spotan = is_spotan_device(dev);
    fo.fill_direct = color_writes_pure(pdevc, lop);
    fo.fill_rect = (fo.fill_direct ? dev_proc(dev, fill_rectangle) : NULL);
    fo.fill_trap = dev_proc(dev, fill_trapezoid);

    /*
     * We have a choice of two different filling algorithms:
     * scan-line-based and trapezoid-based.  They compare as follows:
     *
     *      Scan    Trap
     *      ----    ----
     *       skip   +draw   0-height horizontal lines
     *       slow   +fast   rectangles
     *      +fast    slow   curves
     *      +yes     no     write pixels at most once when adjust != 0
     *
     * Normally we use the scan line algorithm for characters, where curve
     * speed is important, and for non-idempotent RasterOps, where double
     * pixel writing must be avoided, and the trapezoid algorithm otherwise.
     * However, we always use the trapezoid algorithm for rectangles.
     */
    fill_by_trapezoids = (!gx_path_has_curves(ppath) ||
                          params->flatness >= 1.0 || fo.is_spotan);

    if (fill_by_trapezoids && !fo.is_spotan && !lop_is_idempotent(lop)) {
        gs_fixed_rect rbox;

        if (gx_path_is_rectangular(ppath, &rbox)) {
            int x0 = fixed2int_pixround(rbox.p.x - fo.adjust_left);
            int y0 = fixed2int_pixround(rbox.p.y - fo.adjust_below);
            int x1 = fixed2int_pixround(rbox.q.x + fo.adjust_right);
            int y1 = fixed2int_pixround(rbox.q.y + fo.adjust_above);

            return gx_fill_rectangle_device_rop(x0, y0, x1 - x0, y1 - y0,
                                                pdevc, dev, lop);
        }
        if (fo.adjust_left | fo.adjust_right | fo.adjust_below | fo.adjust_above)
            fill_by_trapezoids = false; /* avoid double writing pixels */
    }

    if (!fo.is_spotan && ((scanconverter = gs_getscanconverter(pdev->memory)) >= GS_SCANCONVERTER_EDGEBUFFER ||
                          (scanconverter == GS_SCANCONVERTER_DEFAULT && GS_SCANCONVERTER_DEFAULT_IS_EDGEBUFFER))) {
        gx_scan_converter_t *sc;
        /* If we have a request for accurate curves, make sure we exactly
         * match what we'd get for stroking. */
        if (!big_path && pgs->accurate_curves && gx_path_has_curves(ppath))
        {
            gx_path_init_local(&ffpath, ppath->memory);
            code = gx_path_copy_reducing(ppath, &ffpath, fo.fixed_flat, NULL,
                                         pco_small_curves | pco_accurate);
            if (code < 0)
                return code;
            ppath = &ffpath;
        }

        if (fill_by_trapezoids && !lop_is_idempotent(lop))
            fill_by_trapezoids = 0;
        if (!fill_by_trapezoids)
        {
            if (adjust.x == 0 && adjust.y == 0)
                sc = &gx_scan_converter;
            else
                sc = &gx_scan_converter_app;
        } else {
            if (adjust.x == 0 && adjust.y == 0)
                sc = &gx_scan_converter_tr;
            else
                sc = &gx_scan_converter_tr_app;
        }
        code = gx_scan_convert_and_fill(sc,
                                        dev,
                                        ppath,
                                        &ibox,
                                        fo.fixed_flat,
                                        params->rule,
                                        pdevc,
                                       (!fill_by_trapezoids && fo.fill_direct) ? -1 : (int)pgs->log_op);
        if (ppath == &ffpath)
            gx_path_free(ppath, "gx_general_fill_path");
        return code;
    }

    gx_path_init_local(&ffpath, ppath->memory);
    if (!big_path && !gx_path_has_curves(ppath))        /* don't need to flatten */
        pfpath = ppath;
    else if (is_spotan_device(dev))
        pfpath = ppath;
    else if (!big_path && !pgs->accurate_curves && gx_path__check_curves(ppath, pco_small_curves, fo.fixed_flat))
        pfpath = ppath;
    else {
        code = gx_path_copy_reducing(ppath, &ffpath, fo.fixed_flat, NULL,
                                     pco_small_curves | (pgs->accurate_curves ? pco_accurate : 0));
        if (code < 0)
            return code;
        pfpath = &ffpath;
        if (big_path) {
            code = gx_path_merge_contacting_contours(pfpath);
            if (code < 0)
                return code;
        }
    }
    fo.fill_by_trapezoids = fill_by_trapezoids;
    /* Initialize the active line list. */
    init_line_list(&lst, ppath->memory);
    if ((code = add_y_list(pfpath, &lst)) < 0)
        goto nope;
    {
        FILL_LOOP_PROC((*fill_loop));

        /* Some short-sighted compilers won't allow a conditional here.... */
        if (fill_by_trapezoids)
            fill_loop = spot_into_trapezoids;
        else
            fill_loop = spot_into_scan_lines;
        if (gs_currentcpsimode(pgs->memory) && is_character) {
            if (lst.contour_count > countof(lst.local_windings)) {
                lst.windings = (int *)gs_alloc_byte_array(pdev->memory, lst.contour_count,
                                sizeof(int), "gx_general_fill_path");
            } else
                lst.windings = lst.local_windings;
            memset(lst.windings, 0, sizeof(lst.windings[0]) * lst.contour_count);
        }
        code = (*fill_loop)
            (&lst, (max_fill_band == 0 ? NO_BAND_MASK : int2fixed(-max_fill_band)));
        if (lst.windings != NULL && lst.windings != lst.local_windings)
            gs_free_object(pdev->memory, lst.windings, "gx_general_fill_path");
    }
  nope:if (lst.close_count != 0)
        unclose_path(pfpath, lst.close_count);
    free_line_list(&lst);
    if (pfpath != ppath)        /* had to flatten */
        gx_path_free(pfpath, "gx_general_fill_path");
#if defined(DEBUG) && !defined(GS_THREADSAFE)
    if (gs_debug_c('f')) {
        dmlputs(ppath->memory,
                "[f]  # alloc    up  down horiz step slowx  iter  find  band bstep bfill\n");
        dmlprintf5(ppath->memory, " %5ld %5ld %5ld %5ld %5ld",
                   stats_fill.fill, stats_fill.fill_alloc,
                   stats_fill.y_up, stats_fill.y_down,
                   stats_fill.horiz);
        dmlprintf4(ppath->memory, " %5ld %5ld %5ld %5ld",
                   stats_fill.x_step, stats_fill.slow_x,
                   stats_fill.iter, stats_fill.find_y);
        dmlprintf3(ppath->memory, " %5ld %5ld %5ld\n",
                   stats_fill.band, stats_fill.band_step,
                   stats_fill.band_fill);
        dmlputs(ppath->memory,
                "[f]    afill slant shall sfill mqcrs order slowo\n");
        dmlprintf7(ppath->memory, "       %5ld %5ld %5ld %5ld %5ld %5ld %5ld\n",
                   stats_fill.afill, stats_fill.slant,
                   stats_fill.slant_shallow, stats_fill.sfill,
                   stats_fill.mq_cross, stats_fill.order,
                   stats_fill.slow_order);
    }
#endif
    if (clipping)
        gx_destroy_clip_device_on_stack(&cdev);
    return code;
}

static int
pass_shading_area_through_clip_path_device(gx_device * pdev, const gs_gstate * pgs,
                     gx_path * ppath, const gx_fill_params * params,
                 const gx_device_color * pdevc, const gx_clip_path * pcpath)
{
    if (pdevc == NULL) {
        gx_device_clip *cdev = (gx_device_clip *)pdev;

        return dev_proc(cdev->target, fill_path)(cdev->target, pgs, ppath, params, pdevc, pcpath);
    }
    /* We know that tha clip path device implements fill_path with default proc. */
    return gx_default_fill_path(pdev, pgs, ppath, params, pdevc, pcpath);
}

/*
 * Fill a path.  This is the default implementation of the driver
 * fill_path procedure.
 */
int
gx_default_fill_path(gx_device * pdev, const gs_gstate * pgs,
                     gx_path * ppath, const gx_fill_params * params,
                 const gx_device_color * pdevc, const gx_clip_path * pcpath)
{
    int code = 0;

    if (gx_dc_is_pattern2_color(pdevc)
        || pdevc->type == &gx_dc_type_data_ht_colored
        || (gx_dc_is_pattern1_color(pdevc) && gx_pattern_tile_is_clist(pdevc->colors.pattern.p_tile))
        ) {
        /*  Optimization for shading and halftone fill :
            The general filling algorithm subdivides the fill region into
            trapezoid or rectangle subregions and then paints each subregion
            with given color. If the color is complex, it also needs to be subdivided
            into constant color rectangles. In the worst case it gives
            a multiple of numbers of rectangles, which may be too slow.
            A faster processing may be obtained with installing a clipper
            device with the filling path, and then render the complex color
            through it. The speeding up happens due to the clipper device
            is optimised for fast scans through neighbour clipping rectangles.
        */
        /*  We need a single clipping path here, because shadings and
            halftones don't take 2 paths. Compute the clipping path intersection.
        */
        gx_clip_path cpath_intersection, cpath_with_shading_bbox;
        const gx_clip_path *pcpath1, *pcpath2;
        gs_gstate *pgs_noconst = (gs_gstate *)pgs; /* Break const. */

        if (ppath != NULL) {
            code = gx_cpath_init_local_shared_nested(&cpath_intersection, pcpath, pdev->memory, 1);
            if (code < 0)
                return code;
            if (pcpath == NULL) {
                gs_fixed_rect clip_box1;

                (*dev_proc(pdev, get_clipping_box)) (pdev, &clip_box1);
                code = gx_cpath_from_rectangle(&cpath_intersection, &clip_box1);
            }
            if (code >= 0)
                code = gx_cpath_intersect_with_params(&cpath_intersection, ppath, params->rule,
                        pgs_noconst, params);
            pcpath1 = &cpath_intersection;
        } else
            pcpath1 = pcpath;
        pcpath2 = pcpath1;
        if (code >= 0)
            code = gx_dc_pattern2_clip_with_bbox(pdevc, pdev, &cpath_with_shading_bbox, &pcpath1);
        /* Do fill : */
        if (code >= 0) {
            gs_fixed_rect clip_box;
            gs_int_rect cb;
            const gx_rop_source_t *rs = NULL;
            gx_device *dev;
            gx_device_clip cdev;

            gx_cpath_outer_box(pcpath1, &clip_box);
            cb.p.x = fixed2int_pixround(clip_box.p.x);
            cb.p.y = fixed2int_pixround(clip_box.p.y);
            cb.q.x = fixed2int_pixround(clip_box.q.x);
            cb.q.y = fixed2int_pixround(clip_box.q.y);
            if (gx_dc_is_pattern2_color(pdevc) &&
                    (*dev_proc(pdev, dev_spec_op))(pdev,
                         gxdso_pattern_handles_clip_path, NULL, 0) > 0) {
                /* A special interaction with clist writer device :
                   pass the intersected clipping path. It uses an unusual call to
                   fill_path with NULL device color. */
                code = (*dev_proc(pdev, fill_path))(pdev, pgs, ppath, params, NULL, pcpath1);
                dev = pdev;
            } else {
                gx_make_clip_device_on_stack(&cdev, pcpath1, pdev);
                dev = (gx_device *)&cdev;
                if ((*dev_proc(pdev, dev_spec_op))(pdev,
                        gxdso_pattern_shading_area, NULL, 0) > 0)
                    set_dev_proc(&cdev, fill_path, pass_shading_area_through_clip_path_device);
                code = 0;
            }
            if (code >= 0)
                code = pdevc->type->fill_rectangle(pdevc,
                        cb.p.x, cb.p.y, cb.q.x - cb.p.x, cb.q.y - cb.p.y,
                        dev, pgs->log_op, rs);
        }
        if (ppath != NULL)
            gx_cpath_free(&cpath_intersection, "shading_fill_cpath_intersection");
        if (pcpath1 != pcpath2)
            gx_cpath_free(&cpath_with_shading_bbox, "shading_fill_cpath_intersection");
    } else {
        code = gx_general_fill_path(pdev, pgs, ppath, params, pdevc, pcpath);
    }
    return code;
}

/*
 * Fill/Stroke a path.  This is the default implementation of the driver
 * fill_path procedure.
 */
int
gx_default_fill_stroke_path(gx_device * pdev, const gs_gstate * pgs,
                            gx_path * ppath,
                            const gx_fill_params * params_fill,
                            const gx_device_color * pdevc_fill,
                            const gx_stroke_params * params_stroke,
                            const gx_device_color * pdevc_stroke,
                            const gx_clip_path * pcpath)
{
    int code = dev_proc(pdev, fill_path)(pdev, pgs, ppath, params_fill, pdevc_fill, pcpath);

    if (code < 0)
        return code;
    /* Swap colors to make sure the pgs colorspace is correct for stroke */
    gs_swapcolors_quick(pgs);
    code = dev_proc(pdev, stroke_path)(pdev, pgs, ppath, params_stroke, pdevc_stroke, pcpath);
    gs_swapcolors_quick(pgs);
    return code;
}

/* Free the line list. */
static void
free_line_list(line_list *ll)
{
    /* Free any individually allocated active_lines. */
    gs_memory_t *mem = ll->memory;
    active_line *alp;

    while ((alp = ll->active_area) != 0) {
        active_line *next = alp->alloc_next;

        gs_free_object(mem, alp, "active line");
        ll->active_area = next;
    }
}

static inline active_line *
make_al(line_list *ll)
{
    active_line *alp = ll->next_active;

    if (alp == ll->limit) {     /* Allocate separately */
        alp = gs_alloc_struct(ll->memory, active_line,
                              &st_active_line, "active line");
        if (alp == 0)
            return NULL;
        alp->alloc_next = ll->active_area;
        ll->active_area = alp;
        INCR(fill_alloc);
    } else
        ll->next_active++;
    alp->contour_count = ll->contour_count;
    return alp;
}

/* Insert the new line in the Y ordering */
static void
insert_y_line(line_list *ll, active_line *alp)
{
    active_line *yp = ll->y_line;
    active_line *nyp;
    fixed y_start = alp->start.y;

    if (yp == 0) {
        alp->next = alp->prev = 0;
        ll->y_list = alp;
    } else if (y_start >= yp->start.y) {        /* Insert the new line after y_line */
        while (INCR_EXPR(y_up),
               ((nyp = yp->next) != NULL &&
                y_start > nyp->start.y)
            )
            yp = nyp;
        alp->next = nyp;
        alp->prev = yp;
        yp->next = alp;
        if (nyp)
            nyp->prev = alp;
    } else {            /* Insert the new line before y_line */
        while (INCR_EXPR(y_down),
               ((nyp = yp->prev) != NULL &&
                y_start < nyp->start.y)
            )
            yp = nyp;
        alp->prev = nyp;
        alp->next = yp;
        yp->prev = alp;
        if (nyp)
            nyp->next = alp;
        else
            ll->y_list = alp;
    }
    ll->y_line = alp;
    print_al(ll->memory, "add ", alp);
}

typedef struct contour_cursor_s {
    segment *prev, *pseg, *pfirst, *plast;
    gx_flattened_iterator fi;
    bool more_flattened;
    bool first_flattened;
    int dir;
    bool monotonic_y;
    bool monotonic_x;
    bool crossing;
} contour_cursor;

static inline int
compute_dir(const fill_options *fo, fixed y0, fixed y1)
{
    if (max(y0, y1) < fo->ymin)
        return DIR_OUT_OF_Y_RANGE;
    if (min(y0, y1) > fo->ymax)
        return DIR_OUT_OF_Y_RANGE;
    return (y0 < y1 ? DIR_UP :
            y0 > y1 ? DIR_DOWN : DIR_HORIZONTAL);
}

static inline int
add_y_curve_part(line_list *ll, segment *s0, segment *s1, int dir,
    gx_flattened_iterator *fi, bool more1, bool step_back, bool monotonic_x)
{
    active_line *alp = make_al(ll);
    int code;

    if (alp == NULL)
        return_error(gs_error_VMerror);
    alp->pseg = (dir == DIR_UP ? s1 : s0);
    alp->direction = dir;
    alp->fi = *fi;
    alp->more_flattened = more1;
    if (dir != DIR_UP && more1)
        gx_flattened_iterator__switch_to_backscan(&alp->fi, more1);
    if (step_back) {
        do {
            code = gx_flattened_iterator__prev(&alp->fi);
            if (code < 0)
                return code;
            alp->more_flattened = code;
            if (compute_dir(ll->fo, alp->fi.ly0, alp->fi.ly1) != 2)
                break;
        } while (alp->more_flattened);
    }
    code = step_al(alp, false);
    if (code < 0)
        return code;
    alp->monotonic_y = false;
    alp->monotonic_x = monotonic_x;
    insert_y_line(ll, alp);
    return 0;
}

static inline int
add_y_line(const segment * prev_lp, const segment * lp, int dir, line_list *ll)
{
    return add_y_line_aux(prev_lp, lp, &lp->pt, &prev_lp->pt, dir, ll);
}

static inline int
start_al_pair(line_list *ll, contour_cursor *q, contour_cursor *p)
{
    int code;

    if (q->monotonic_y)
        code = add_y_line(q->prev, q->pseg, DIR_DOWN, ll);
    else
        code = add_y_curve_part(ll, q->prev, q->pseg, DIR_DOWN, &q->fi,
                            !q->first_flattened, false, q->monotonic_x);
    if (code < 0)
        return code;
    if (p->monotonic_y)
        code = add_y_line(p->prev, p->pseg, DIR_UP, ll);
    else
        code = add_y_curve_part(ll, p->prev, p->pseg, DIR_UP, &p->fi,
                            p->more_flattened, false, p->monotonic_x);
    return code;
}

/* Start active lines from a minimum of a possibly non-monotonic curve. */
static int
start_al_pair_from_min(line_list *ll, contour_cursor *q)
{
    int dir, code;
    const fill_options * const fo = ll->fo;

    /* q stands at the first segment, which isn't last. */
    do {
        code = gx_flattened_iterator__next(&q->fi);
        if (code < 0)
            return code;
        q->more_flattened = code;
        dir = compute_dir(fo, q->fi.ly0, q->fi.ly1);
        if (q->fi.ly0 > fo->ymax && ll->y_break > q->fi.y0)
            ll->y_break = q->fi.ly0;
        if (q->fi.ly1 > fo->ymax && ll->y_break > q->fi.ly1)
            ll->y_break = q->fi.ly1;
        if (q->fi.ly0 >= fo->ymin) {
            if (dir == DIR_UP && ll->main_dir == DIR_DOWN) {
                code = add_y_curve_part(ll, q->prev, q->pseg, DIR_DOWN, &q->fi,
                                        true, true, q->monotonic_x);
                if (code < 0)
                    return code;
                code = add_y_curve_part(ll, q->prev, q->pseg, DIR_UP, &q->fi,
                                        q->more_flattened, false, q->monotonic_x);
                if (code < 0)
                    return code;
            } else if (q->fi.ly1 < fo->ymin) {
                code = add_y_curve_part(ll, q->prev, q->pseg, DIR_DOWN, &q->fi,
                                        true, false, q->monotonic_x);
                if (code < 0)
                    return code;
            }
        } else if (q->fi.ly1 >= fo->ymin) {
            code = add_y_curve_part(ll, q->prev, q->pseg, DIR_UP, &q->fi,
                                    q->more_flattened, false, q->monotonic_x);
            if (code < 0)
                return code;
        }
        q->first_flattened = false;
        q->dir = dir;
        if (dir == DIR_DOWN || dir == DIR_UP)
            ll->main_dir = dir;
    } while(q->more_flattened);
    /* q stands at the last segment. */
    return 0;
    /* note : it doesn't depend on the number of curve minimums,
       which may vary due to arithmetic errors. */
}

static inline int
init_contour_cursor(const fill_options * const fo, contour_cursor *q)
{
    if (q->pseg->type == s_curve) {
        curve_segment *s = (curve_segment *)q->pseg;
        fixed ymin = min(min(q->prev->pt.y, s->p1.y), min(s->p2.y, s->pt.y));
        fixed ymax = max(max(q->prev->pt.y, s->p1.y), max(s->p2.y, s->pt.y));
        bool in_band = ymin <= fo->ymax && ymax >= fo->ymin;
        q->crossing = ymin < fo->ymin && ymax >= fo->ymin;
        q->monotonic_y = !in_band ||
            (!q->crossing &&
            ((q->prev->pt.y <= s->p1.y && s->p1.y <= s->p2.y && s->p2.y <= s->pt.y) ||
             (q->prev->pt.y >= s->p1.y && s->p1.y >= s->p2.y && s->p2.y >= s->pt.y)));
        q->monotonic_x =
            ((q->prev->pt.x <= s->p1.x && s->p1.x <= s->p2.x && s->p2.x <= s->pt.x) ||
             (q->prev->pt.x >= s->p1.x && s->p1.x >= s->p2.x && s->p2.x >= s->pt.x));
    } else
        q->monotonic_y = true;
    if (!q->monotonic_y) {
        curve_segment *s = (curve_segment *)q->pseg;
        int k = gx_curve_log2_samples(q->prev->pt.x, q->prev->pt.y, s, fo->fixed_flat);

        if (!gx_flattened_iterator__init(&q->fi, q->prev->pt.x, q->prev->pt.y, s, k))
            return_error(gs_error_rangecheck);
    } else {
        q->dir = compute_dir(fo, q->prev->pt.y, q->pseg->pt.y);
        gx_flattened_iterator__init_line(&q->fi,
            q->prev->pt.x, q->prev->pt.y, q->pseg->pt.x, q->pseg->pt.y); /* fake for curves. */
    }
    q->first_flattened = true;
    return 0;
}

static int
scan_contour(line_list *ll, segment *pfirst, segment *plast, segment *prev)
{
    contour_cursor p, q;
    int code;
    bool only_horizontal = true, saved = false;
    const fill_options * const fo = ll->fo;
    contour_cursor save_q;

    q.prev = prev;
    q.pseg = plast;
    q.pfirst = pfirst;
    q.plast = plast;

    memset(&save_q, 0, sizeof(save_q)); /* Quiet gcc warning. */
    p.monotonic_x = false; /* Quiet gcc warning. */
    q.monotonic_x = false; /* Quiet gcc warning. */

    /* The first thing we do is to walk BACKWARDS along the contour (aka the subpath or
     * set of curve segments). We stop either when we reach the beginning again,
     * or when we hit a non-horizontal segment. If we hit the beginning, then we
     * remember that the segment is 'only_horizontal'. Otherwise, we remember the
     * direction of this contour (UP or DOWN). */
    save_q.dir = DIR_OUT_OF_Y_RANGE;
    ll->main_dir = DIR_HORIZONTAL;
    for (; ; q.pseg = q.prev, q.prev = q.prev->prev) {
        /* Prepare to walk FORWARDS along the single curve segment in 'q'. */
        code = init_contour_cursor(fo, &q);
        if (code < 0)
            return code;
        for (;;) {
            code = gx_flattened_iterator__next(&q.fi);
            assert(code >= 0); /* Indicates an internal error */
            if (code < 0)
                return code;
            q.dir = compute_dir(fo, q.fi.ly0, q.fi.ly1);
            if (q.dir == DIR_DOWN || q.dir == DIR_UP)
                ll->main_dir = q.dir;
            /* Exit the loop when we hit the end of the single curve segment */
            if (!code)
                break;
            q.first_flattened = false;
        }
        /* Thus first_flattened == true, iff the curve needed no subdivisions */
        q.more_flattened = false;
        /* ll->maindir == DIR_HORIZONTAL, iff the curve is entirely horizontal
         * (or entirely out of y range). It will contain whatever the last UP or DOWN
         * section was otherwise. */
        if (ll->main_dir != DIR_HORIZONTAL) {
            only_horizontal = false;
            /* Now we know we're not entirely horizontal, we can abort this run. */
            break;
        }
        /* If we haven't saved one yet, and we are not out of range, save this one.
         * i.e. save_q and save_fi are the first 'in range' section. We can save
         * time in subsequent passes through by starting here. */
        if (!saved && q.dir != DIR_OUT_OF_Y_RANGE) {
            save_q = q;
            saved = true;
        }
        if (q.prev == q.pfirst)
            break; /* Exit if we're back at the start of the contour */
    }

    /* Second run through; this is where we actually do the hard work. */
    /* Restore q if we saved it above */
    if (saved) {
        q = save_q;
    }
    /* q is now at the start of a run that we know will head in an
     * ll->main_dir direction. Start p at the next place and walk
     * forwards. */
    for (p.prev = q.pfirst; p.prev != q.plast; p.prev = p.pseg) {
        bool added;
        p.pseg = p.prev->next;
        /* Can we ignore this segment entirely? */
        if (!only_horizontal && p.pseg->type != s_curve &&
            p.prev->pt.x == p.pseg->pt.x && p.prev->pt.y == p.pseg->pt.y)
            continue;

        /* Prepare to walk down 'p' */
        code = init_contour_cursor(fo, &p);
        if (code < 0)
            return code;

        do {
            /* Find the next flattened section that is within range */
            do {
                code = gx_flattened_iterator__next(&p.fi);
                assert(code >= 0); /* Indicates an internal error */
                if (code < 0)
                    return code;
                p.more_flattened = code;
                p.dir = compute_dir(fo, p.fi.ly0, p.fi.ly1);
            } while (p.more_flattened && p.dir == DIR_OUT_OF_Y_RANGE);

            /* So either we're in range, or we've reached the end of the segment (or both) */
            /* FIXME: Can we break here if p.dir == DIR_OUT_OF_Y_RANGE? */

            /* Set ll->y_break to be the smallest line endpoint we find > ymax */
            if (p.fi.ly0 > fo->ymax && ll->y_break > p.fi.ly0)
                ll->y_break = p.fi.ly0;
            if (p.fi.ly1 > fo->ymax && ll->y_break > p.fi.ly1)
                ll->y_break = p.fi.ly1;

            added = 0;
            if (p.dir == DIR_HORIZONTAL) {
                if (p.monotonic_y) {
                    if (
#ifdef FILL_ZERO_WIDTH
                        (fo->adjust_below | fo->adjust_above) != 0) {
#else
                        (fo->adjust_below + fo->adjust_above >= (fixed_1 - fixed_epsilon) ||
                         fixed2int_pixround(p.pseg->pt.y - fo->adjust_below) <
                         fixed2int_pixround(p.pseg->pt.y + fo->adjust_above))) {
#endif
                        /* Add it here to avoid double processing in process_h_segments. */
                        code = add_y_line(p.prev, p.pseg, DIR_HORIZONTAL, ll);
                        if (code < 0)
                            return code;
                        added = 1;
                    }
                }
            } else {
                if (p.fi.ly0 >= fo->ymin)
                {
                    if (p.dir == DIR_UP && ll->main_dir == DIR_DOWN) {
                        /* p starts within range, and heads up. q was heading down. Therefore
                         * they meet at a local minima. Start a pair of active lines from that. */
                        code = start_al_pair(ll, &q, &p);
                        if (code < 0)
                            return code;
                        added = 1;
                    } else if (p.fi.ly1 < fo->ymin) {
                        /* p heading downwards */
                        if (p.monotonic_y)
                            code = add_y_line(p.prev, p.pseg, DIR_DOWN, ll);
                        else
                            code = add_y_curve_part(ll, p.prev, p.pseg, DIR_DOWN, &p.fi,
                                                    !p.first_flattened, false, p.monotonic_x);
                        if (code < 0)
                            return code;
                        added = 1;
                    }
                } else if (p.fi.ly1 >= fo->ymin) {
                    /* p heading upwards */
                    if (p.monotonic_y)
                        code = add_y_line(p.prev, p.pseg, DIR_UP, ll);
                    else
                        code = add_y_curve_part(ll, p.prev, p.pseg, DIR_UP, &p.fi,
                                                p.more_flattened, false, p.monotonic_x);
                    if (code < 0)
                        return code;
                    added = 1;
                }
                if (p.dir == DIR_DOWN || p.dir == DIR_UP)
                    ll->main_dir = p.dir;
            }
            if (!p.monotonic_y && p.more_flattened) {
                code = start_al_pair_from_min(ll, &p);
                if (code < 0)
                    return code;
                added = 1;
            }
            if (p.dir == DIR_DOWN || p.dir == DIR_HORIZONTAL) {
                q.prev = p.prev;
                q.pseg = p.pseg;
                q.monotonic_y = p.monotonic_y;
                q.more_flattened = p.more_flattened;
                q.first_flattened = p.first_flattened;
                q.fi = p.fi;
                q.dir = p.dir;
            }
            /* If we haven't added lines based on this segment, and there
             * are more flattened lines to come from it, ensure we loop
             * to get the rest of them. */
            /* FIXME: Do we need '!added' here? Try removing it. */
        } while (!added && p.more_flattened);
    }
    return 0;
}

/*
 * Construct a Y-sorted list of segments for rasterizing a path.  We assume
 * the path is non-empty.  Only include non-horizontal lines or (monotonic)
 * curve segments where one endpoint is locally Y-minimal, and horizontal
 * lines that might color some additional pixels.
 */
static int
add_y_list(gx_path * ppath, line_list *ll)
{
    subpath *psub = ppath->first_subpath;
    int close_count = 0;
    int code;

    ll->y_break = max_fixed;

    for (;psub; psub = (subpath *)psub->last->next) {
        /* We know that pseg points to a subpath head (s_start). */
        segment *pfirst = (segment *)psub;
        segment *plast = psub->last, *prev;

        if (plast->type != s_line_close) {
            /* Create a fake s_line_close */
            line_close_segment *lp = &psub->closer;
            segment *next = plast->next;

            lp->next = next;
            lp->prev = plast;
            plast->next = (segment *) lp;
            if (next)
                next->prev = (segment *) lp;
            lp->type = s_line_close;
            lp->pt = psub->pt;
            lp->sub = psub;
            psub->last = plast = (segment *) lp;
            ll->close_count++;
        }
        prev = plast->prev;
        code = scan_contour(ll, pfirst, plast, prev);
        if (code < 0)
            return code;
        ll->contour_count++;
    }
    return close_count;
}

static int
step_al(active_line *alp, bool move_iterator)
{
    bool forth = (alp->direction == DIR_UP || !alp->fi.curve);

    if (move_iterator) {
        int code;

        if (forth)
            code = gx_flattened_iterator__next(&alp->fi);
        else
            code = gx_flattened_iterator__prev(&alp->fi);
        if (code < 0)
            return code;
        alp->more_flattened = code;
    }
    /* Note that we can get alp->fi.ly0 == alp->fi.ly1
       if the curve tangent is horizontal. */
    alp->start.x = (forth ? alp->fi.lx0 : alp->fi.lx1);
    alp->start.y = (forth ? alp->fi.ly0 : alp->fi.ly1);
    alp->end.x = (forth ? alp->fi.lx1 : alp->fi.lx0);
    alp->end.y = (forth ? alp->fi.ly1 : alp->fi.ly0);
    alp->diff.x = alp->end.x - alp->start.x;
    alp->diff.y = alp->end.y - alp->start.y;
    SET_NUM_ADJUST(alp);
    (alp)->y_fast_max = MAX_MINUS_NUM_ADJUST(alp) /
      ((alp->diff.x >= 0 ? alp->diff.x : -alp->diff.x) | 1) + alp->start.y;
    return 0;
}

static int
init_al(active_line *alp, const segment *s0, const segment *s1, const line_list *ll)
{
    const segment *ss = (alp->direction == DIR_UP ? s1 : s0);
    /* Warning : p0 may be equal to &alp->end. */
    bool curve = (ss != NULL && ss->type == s_curve);
    int code;

    if (curve) {
        if (alp->direction == DIR_UP) {
            const curve_segment *cs = (const curve_segment *)s1;
            int k = gx_curve_log2_samples(s0->pt.x, s0->pt.y, cs, ll->fo->fixed_flat);

            gx_flattened_iterator__init(&alp->fi,
                s0->pt.x, s0->pt.y, (const curve_segment *)s1, k);
            code = step_al(alp, true);
            if (code < 0)
                return code;
            if (!ll->fo->fill_by_trapezoids) {
                alp->monotonic_y = (s0->pt.y <= cs->p1.y && cs->p1.y <= cs->p2.y && cs->p2.y <= cs->pt.y);
                alp->monotonic_x = (s0->pt.x <= cs->p1.x && cs->p1.x <= cs->p2.x && cs->p2.x <= cs->pt.x) ||
                                   (s0->pt.x >= cs->p1.x && cs->p1.x >= cs->p2.x && cs->p2.x >= cs->pt.x);
            }
        } else {
            const curve_segment *cs = (const curve_segment *)s0;
            int k = gx_curve_log2_samples(s1->pt.x, s1->pt.y, cs, ll->fo->fixed_flat);
            bool more;

            gx_flattened_iterator__init(&alp->fi,
                s1->pt.x, s1->pt.y, (const curve_segment *)s0, k);
            alp->more_flattened = false;
            do {
                code = gx_flattened_iterator__next(&alp->fi);
                if (code < 0)
                    return code;
                more = code;
                alp->more_flattened |= more;
            } while(more);
            gx_flattened_iterator__switch_to_backscan(&alp->fi, alp->more_flattened);
            code = step_al(alp, false);
            if (code < 0)
                return code;
            if (!ll->fo->fill_by_trapezoids) {
                alp->monotonic_y = (s0->pt.y >= cs->p1.y && cs->p1.y >= cs->p2.y && cs->p2.y >= cs->pt.y);
                alp->monotonic_x = (s0->pt.x <= cs->p1.x && cs->p1.x <= cs->p2.x && cs->p2.x <= cs->pt.x) ||
                                   (s0->pt.x >= cs->p1.x && cs->p1.x >= cs->p2.x && cs->p2.x >= cs->pt.x);
            }
        }
    } else {
        gx_flattened_iterator__init_line(&alp->fi,
                s0->pt.x, s0->pt.y, s1->pt.x, s1->pt.y);
        code = step_al(alp, true);
        if (code < 0)
            return code;
        alp->monotonic_x = alp->monotonic_y = true;
    }
    alp->pseg = s1;
    return 0;
}
/*
 * Internal routine to test a segment and add it to the pending list if
 * appropriate.
 */
static int
add_y_line_aux(const segment * prev_lp, const segment * lp,
            const gs_fixed_point *curr, const gs_fixed_point *prev, int dir, line_list *ll)
{
    int code;

    active_line *alp = make_al(ll);
    if (alp == NULL)
        return_error(gs_error_VMerror);
    alp->more_flattened = false;
    switch ((alp->direction = dir)) {
        case DIR_UP:
            code = init_al(alp, prev_lp, lp, ll);
            if (code < 0)
                return code;
            break;
        case DIR_DOWN:
            code = init_al(alp, lp, prev_lp, ll);
            if (code < 0)
                return code;
            break;
        case DIR_HORIZONTAL:
            alp->start = *prev;
            alp->end = *curr;
            /* Don't need to set dx or y_fast_max */
            alp->pseg = prev_lp;        /* may not need this either */
            break;
        default:                /* can't happen */
            return_error(gs_error_unregistered);
    }
    insert_y_line(ll, alp);
    return 0;
}

/* ---------------- Filling loop utilities ---------------- */

/* Insert a newly active line in the X ordering. */
static void
insert_x_new(active_line * alp, line_list *ll)
{
    active_line *next;
    active_line *prev = &ll->x_head;

    alp->x_current = alp->start.x;
    alp->x_next = alp->start.x; /*      If the spot starts with a horizontal segment,
                                    we need resort_x_line to work properly
                                    in the very beginning. */
    while (INCR_EXPR(x_step),
           (next = prev->next) != 0 && x_order(next, alp) < 0
        )
        prev = next;
    alp->next = next;
    alp->prev = prev;
    if (next != 0)
        next->prev = alp;
    prev->next = alp;
}

/* Insert a newly active line in the h list. */
/* h list isn't ordered because x intervals may overlap. */
/* todo : optimize with maintaining ordered interval list;
   Unite contacting inrevals, like we did in add_margin.
 */
static inline void
insert_h_new(active_line * alp, line_list *ll)
{
    alp->next = ll->h_list0;
    alp->prev = 0;
    if (ll->h_list0 != 0)
        ll->h_list0->prev = alp;
    ll->h_list0 = alp;
    /*
     * h list keeps horizontal lines for a given y.
     * There are 2 lists, corresponding to upper and lower ends
     * of the Y-interval, which spot_into_trapezoids currently proceeds.
     * Parts of horizontal lines, which do not contact a filled trapezoid,
     * are parts of the spot bondairy. They to be marked in
     * the 'sect' array.  - see process_h_lists.
     */
}

static inline void
remove_al(const line_list *ll, active_line *alp)
{
    active_line *nlp = alp->next;

    alp->prev->next = nlp;
    if (nlp)
        nlp->prev = alp->prev;
    if_debug1m('F', ll->memory, "[F]drop 0x%lx\n", (ulong) alp);
}

/*
 * Handle a line segment that just ended.  Return true iff this was
 * the end of a line sequence.
 */
static int
end_x_line(active_line *alp, const line_list *ll, bool update)
{
    const segment *pseg = alp->pseg;
    /*
     * The computation of next relies on the fact that
     * all subpaths have been closed.  When we cycle
     * around to the other end of a subpath, we must be
     * sure not to process the start/end point twice.
     */
    const segment *next =
        (alp->direction == DIR_UP ?
         (                      /* Upward line, go forward along path. */
          pseg->type == s_line_close ?  /* end of subpath */
          ((const line_close_segment *)pseg)->sub->next :
          pseg->next) :
         (                      /* Downward line, go backward along path. */
          pseg->type == s_start ?       /* start of subpath */
          ((const subpath *)pseg)->last->prev :
          pseg->prev)
        );
    int code;

    if (alp->end.y < alp->start.y) {
        /* fixme: The condition above causes a horizontal
           part of a curve near an Y maximum to process twice :
           once scanning the left spot boundary and once scanning the right one.
           In both cases it will go to the H list.
           However the dropout prevention logic isn't
           sensitive to that, and such segments does not affect
           trapezoids. Thus the resulting raster doesn't depend on that.
           However it would be nice to improve someday.
         */
        remove_al(ll, alp);
        return true;
    } else if (alp->more_flattened)
        return false;
    code = init_al(alp, pseg, next, ll);
    if (code < 0)
        return code;
    if (alp->start.y > alp->end.y) {
        /* See comment above. */
        remove_al(ll, alp);
        return true;
    }
    alp->x_current = alp->x_next = alp->start.x;
    print_al(ll->memory, "repl", alp);
    return false;
}

/*
 * Handle the case of a slanted trapezoid with adjustment.
 * To do this exactly right requires filling a central trapezoid
 * (or rectangle) plus two horizontal almost-rectangles.
 */
static int
fill_slant_adjust(const line_list *ll,
        const active_line *flp, const active_line *alp, fixed y, fixed y1)
{
    const fill_options * const fo = ll->fo;
    const fixed Yb = y - fo->adjust_below;
    fixed Ya = y + fo->adjust_above;
    fixed Y1b = y1 - fo->adjust_below;
    const fixed Y1a = y1 + fo->adjust_above;
    const gs_fixed_edge *plbot, *prbot, *pltop, *prtop;
    gs_fixed_edge vert_left, slant_left, vert_right, slant_right;
    int code;

    INCR(slant);

    /* Set up all the edges, even though we may not need them all. */

    slant_left.start.x = flp->start.x - fo->adjust_left;
    slant_left.end.x = flp->end.x - fo->adjust_left;
    slant_right.start.x = alp->start.x + fo->adjust_right;
    slant_right.end.x = alp->end.x + fo->adjust_right;
    if (flp->start.x < flp->end.x) {
        vert_left.start.x = vert_left.end.x = flp->x_current - fo->adjust_left;
        vert_left.start.y = Yb, vert_left.end.y = Ya;
        vert_right.start.x = vert_right.end.x = alp->x_next + fo->adjust_right;
        vert_right.start.y = Y1b, vert_right.end.y = Y1a;
        slant_left.start.y = flp->start.y + fo->adjust_above;
        slant_left.end.y = flp->end.y + fo->adjust_above;
        slant_right.start.y = alp->start.y - fo->adjust_below;
        slant_right.end.y = alp->end.y - fo->adjust_below;
        plbot = &vert_left, prbot = &slant_right;
        pltop = &slant_left, prtop = &vert_right;
        if (TRY_TO_EXTEND_TRAP)
        {
            int x, cx, cy, dy;
            /* Extend the slanted edges so they extend above/below the
             * adjusted Y range. This means we could maybe use them
             * directly. */
            while (slant_right.end.y < Y1a)
            {
                slant_right.end.x += slant_right.end.x - slant_right.start.x;
                slant_right.end.y += slant_right.end.y - slant_right.start.y;
            }
            while (slant_left.start.y > Yb)
            {
                slant_left.start.x -= slant_left.end.x - slant_left.start.x;
                slant_left.start.y -= slant_left.end.y - slant_left.start.y;
            }
            /* Consider the centre of the pixel above and to the right of
             * vert_right.start. If that's inside our newly extended trap
             * then we can't use the extended one without changing the
             * rendered result. */
            cy = fixed_pixround(vert_right.start.y)+fixed_half;
            cx = fixed_pixround(vert_right.start.x)+fixed_half;
            dy = slant_right.end.y-slant_right.start.y;
            x = vert_right.start.x +
                (fixed)((((int64_t)(slant_right.end.x-slant_right.start.x)) *
                         ((int64_t)(cy-vert_right.start.y)) + dy - 1) /
                        ((int64_t)dy));
            if ((cy >= Y1a) || (x < cx))
            {
                /* We are safe to extend the trap upwards */
                Y1b = Y1a;
            }
            /* Consider the centre of the pixel below and to the left of
             * vert_left.end. If that's inside our newly extended trap
             * then we can't use the extended one without changing the
             * rendered result. */
            cy = fixed_pixround(vert_left.end.y)-fixed_half;
            cx = fixed_pixround(vert_left.end.x)-fixed_half;
            dy = slant_left.end.y-slant_left.start.y;
            x = vert_left.end.x -
                (fixed)((((int64_t)(slant_left.end.x-slant_left.start.x)) *
                         ((int64_t)(vert_left.end.y-cy)) + dy - 1) /
                        ((int64_t)dy));
            if ((cy < Yb) || (x > cx))
            {
                /* We are safe to extend the trap downwards */
                Ya = Yb;
            }
        }
    } else {
        vert_left.start.x = vert_left.end.x = flp->x_next - fo->adjust_left;
        vert_left.start.y = Y1b, vert_left.end.y = Y1a;
        vert_right.start.x = vert_right.end.x = alp->x_current + fo->adjust_right;
        vert_right.start.y = Yb, vert_right.end.y = Ya;
        slant_left.start.y = flp->start.y - fo->adjust_below;
        slant_left.end.y = flp->end.y - fo->adjust_below;
        slant_right.start.y = alp->start.y + fo->adjust_above;
        slant_right.end.y = alp->end.y + fo->adjust_above;
        plbot = &slant_left, prbot = &vert_right;
        pltop = &vert_left, prtop = &slant_right;
        if (TRY_TO_EXTEND_TRAP)
        {
            int x, cx, cy, dy;
            /* Extend the slanted edges so they extend above/below the
             * adjusted Y range. This means we could maybe use them
             * directly. */
            while (slant_left.end.y < Y1a)
            {
                slant_left.end.x += slant_left.end.x - slant_left.start.x;
                slant_left.end.y += slant_left.end.y - slant_left.start.y;
            }
            while (slant_right.start.y > Yb)
            {
                slant_right.start.x -= slant_right.end.x-slant_right.start.x;
                slant_right.start.y -= slant_right.end.y-slant_right.start.y;
            }
            /* Consider the centre of the pixel above and to the left of
             * vert_left.start. If that's inside our newly extended trap
             * then we can't use the extended one without changing the
             * rendered result. */
            cy = fixed_pixround(vert_left.start.y)+fixed_half;
            cx = fixed_pixround(vert_left.start.x)-fixed_half;
            dy = slant_left.end.y-slant_left.start.y;
            x = vert_left.start.x -
                (fixed)((((int64_t)(slant_left.start.x-slant_left.end.x)) *
                         ((int64_t)(cy-vert_left.start.y)) + dy - 1) /
                        ((int64_t)dy));
            if ((cy >= Y1a) || (x > cx))
            {
                /* We are safe to extend the trap upwards */
                Y1b = Y1a;
            }
            /* Consider the centre of the pixel below and to the left of
             * vert_left.end. If that's inside our newly extended trap
             * then we can't use the extended one without changing the
             * rendered result. */
            cy = fixed_pixround(vert_right.end.y)-fixed_half;
            cx = fixed_pixround(vert_right.end.x)-fixed_half;
            dy = slant_right.end.y-slant_right.start.y;
            x = vert_right.end.x +
                (fixed)((((int64_t)(slant_right.start.x-slant_right.end.x)) *
                         ((int64_t)(vert_right.end.y-cy)) + dy - 1) /
                        ((int64_t)dy));
            if ((cy < Yb) || (x < cx))
            {
                /* We are safe to extend the trap downwards */
                Ya = Yb;
            }
        }
    }

    if (Ya >= Y1b) {
        /*
         * The upper and lower adjustment bands overlap.
         * Since the entire entity is less than 2 pixels high
         * in this case, we could handle it very efficiently
         * with no more than 2 rectangle fills, but for right now
         * we don't attempt to do this.
         */
        int iYb = fixed2int_var_pixround(Yb);
        int iYa = fixed2int_var_pixround(Ya);
        int iY1b = fixed2int_var_pixround(Y1b);
        int iY1a = fixed2int_var_pixround(Y1a);

        INCR(slant_shallow);
        if (iY1b > iYb) {
            code = fo->fill_trap(fo->dev, plbot, prbot,
                                 Yb, Y1b, false, fo->pdevc, fo->lop);
            if (code < 0)
                return code;
        }
        if (iYa > iY1b) {
            int ix = fixed2int_var_pixround(vert_left.start.x);
            int iw = fixed2int_var_pixround(vert_right.start.x) - ix;

            code = gx_fill_rectangle_device_rop(ix, iY1b, iw, iYa - iY1b,
                        fo->pdevc, fo->dev, fo->lop);
            if (code < 0)
                return code;
        }
        if (iY1a > iYa)
            code = fo->fill_trap(fo->dev, pltop, prtop,
                                 Ya, Y1a, false, fo->pdevc, fo->lop);
        else
            code = 0;
    } else {
        /*
         * Clip the trapezoid if possible.  This can save a lot
         * of work when filling paths that cross band boundaries.
         */
        fixed Yac;

        if (fo->pbox->p.y < Ya) {
            code = fo->fill_trap(fo->dev, plbot, prbot,
                                 Yb, Ya, false, fo->pdevc, fo->lop);
            if (code < 0)
                return code;
            Yac = Ya;
        } else
            Yac = fo->pbox->p.y;
        if (fo->pbox->q.y > Y1b) {
            code = fo->fill_trap(fo->dev, &slant_left, &slant_right,
                                 Yac, Y1b, false, fo->pdevc, fo->lop);
            if (code < 0)
                return code;
            code = fo->fill_trap(fo->dev, pltop, prtop,
                                 Y1b, Y1a, false, fo->pdevc, fo->lop);
        } else
            code = fo->fill_trap(fo->dev, &slant_left, &slant_right,
                                 Yac, fo->pbox->q.y, false, fo->pdevc, fo->lop);
    }
    return code;
}

/* Re-sort the x list by moving alp backward to its proper spot. */
static inline void
resort_x_line(active_line * alp)
{
    active_line *prev = alp->prev;
    active_line *next = alp->next;

    prev->next = next;
    if (next)
        next->prev = prev;
    while (x_order(prev, alp) > 0) {
        if_debug2('F', "[F]swap 0x%lx,0x%lx\n",
                   (ulong) alp, (ulong) prev);
        next = prev, prev = prev->prev;
    }
    alp->next = next;
    alp->prev = prev;
    /* next might be null, if alp was in the correct spot already. */
    if (next)
        next->prev = alp;
    /* prev can be null if the path reaches (beyond) the extent of our coordinate space */
    if (prev)
        prev->next = alp;
}

/* Move active lines by Y. */
static inline int
move_al_by_y(line_list *ll, fixed y1)
{
    fixed x;
    active_line *alp, *nlp;
    int code;

    for (x = min_fixed, alp = ll->x_list; alp != 0; alp = nlp) {
        bool notend = false;
        alp->x_current = alp->x_next;

        nlp = alp->next;
        if (alp->end.y == y1 && alp->more_flattened) {
            code = step_al(alp, true);
            if (code < 0)
                return code;
            alp->x_current = alp->x_next = alp->start.x;
            notend = (alp->end.y >= alp->start.y);
        }
        if (alp->end.y > y1 || notend) {
            if (alp->x_next <= x)
                resort_x_line(alp);
            else
                x = alp->x_next;
        } else {
            code = end_x_line(alp, ll, true);
            if (code < 0)
                return code;
            if (!code) {
                if (alp->x_next <= x)
                    resort_x_line(alp);
                else
                    x = alp->x_next;
            }
        }
    }
    return 0;
}

/* Process horizontal segment of curves. */
static inline int
process_h_segments(line_list *ll, fixed y)
{
    active_line *alp, *nlp;
    int inserted = 0;

    for (alp = ll->x_list; alp != 0; alp = nlp) {
        nlp = alp->next;
        if (alp->start.y == y && alp->end.y == y) {
            inserted = 1;
        }
    }
    return inserted;
    /* After this should call move_al_by_y and step to the next band. */
}

static inline int
loop_fill_trap_np(const line_list *ll, const gs_fixed_edge *le, const gs_fixed_edge *re, fixed y, fixed y1)
{
    const fill_options * const fo = ll->fo;
    fixed ybot = max(y, fo->pbox->p.y);
    fixed ytop = min(y1, fo->pbox->q.y);

    if (ybot >= ytop)
        return 0;
    return (*fo->fill_trap)
        (fo->dev, le, re, ybot, ytop, false, fo->pdevc, fo->lop);
}

/* Define variants of the algorithm for filling a slanted trapezoid. */
#define TEMPLATE_slant_into_trapezoids slant_into_trapezoids__fd
#define FILL_DIRECT 1
#include "gxfillts.h"
#undef TEMPLATE_slant_into_trapezoids
#undef FILL_DIRECT

#define TEMPLATE_slant_into_trapezoids slant_into_trapezoids__nd
#define FILL_DIRECT 0
#include "gxfillts.h"
#undef TEMPLATE_slant_into_trapezoids
#undef FILL_DIRECT

#define COVERING_PIXEL_CENTERS(y, y1, adjust_below, adjust_above)\
    (fixed_pixround(y - adjust_below) < fixed_pixround(y1 + adjust_above))

/* Find an intersection within y, y1. */
/* lp->x_current, lp->x_next must be set for y, y1. */
static bool
intersect(active_line *endp, active_line *alp, fixed y, fixed y1, fixed *p_y_new)
{
    fixed y_new, dy;
    fixed dx_old = alp->x_current - endp->x_current;
    fixed dx_den = dx_old + endp->x_next - alp->x_next;

    if (dx_den <= dx_old || dx_den == 0)
        return false; /* Intersection isn't possible. */
    dy = y1 - y;
    if_debug3('F', "[F]cross: dy=%g, dx_old=%g, dx_new=%g\n",
              fixed2float(dy), fixed2float(dx_old),
              fixed2float(dx_den - dx_old));
    /* Do the computation in single precision */
    /* if the values are small enough. */
    y_new =
        (((ufixed)(dy | dx_old)) < (1L << (size_of(fixed) * 4 - 1)) ?
         dy * dx_old / dx_den :
         (INCR_EXPR(mq_cross), fixed_mult_quo(dy, dx_old, dx_den)))
        + y;
    /* The crossing value doesn't have to be */
    /* very accurate, but it does have to be */
    /* greater than y and less than y1. */
    if_debug3('F', "[F]cross y=%g, y_new=%g, y1=%g\n",
              fixed2float(y), fixed2float(y_new),
              fixed2float(y1));
    if (y_new <= y) {
        /*
         * This isn't possible.  Recompute the intersection
         * accurately.
         */
        fixed ys, xs0, xs1, ye, xe0, xe1, dy, dx0, dx1;

        INCR(cross_slow);
        if (endp->start.y < alp->start.y)
            ys = alp->start.y,
                xs0 = AL_X_AT_Y(endp, ys), xs1 = alp->start.x;
        else
            ys = endp->start.y,
                xs0 = endp->start.x, xs1 = AL_X_AT_Y(alp, ys);
        if (endp->end.y > alp->end.y)
            ye = alp->end.y,
                xe0 = AL_X_AT_Y(endp, ye), xe1 = alp->end.x;
        else
            ye = endp->end.y,
                xe0 = endp->end.x, xe1 = AL_X_AT_Y(alp, ye);
        dy = ye - ys;
        dx0 = xe0 - xs0;
        dx1 = xe1 - xs1;
        /* We need xs0 + cross * dx0 == xs1 + cross * dx1. */
        if (dx0 == dx1) {
            /* The two lines are coincident.  Do nothing. */
            y_new = y1;
        } else {
            double cross = (double)(xs0 - xs1) / (dx1 - dx0);

            y_new = (fixed)(ys + cross * dy);
            if (y_new <= y) {
                /*
                 * This can only happen through some kind of
                 * numeric disaster, but we have to check.
                 */
                INCR(cross_low);
                y_new = y + fixed_epsilon;
            }
        }
    }
    *p_y_new = y_new;
    return true;
}

static inline void
set_x_next(active_line *endp, active_line *alp, fixed x)
{
    while(endp != alp) {
        endp->x_next = x;
        endp = endp->next;
    }
}

static inline int
coord_weight(const active_line *alp)
{
    return 1 + min(any_abs((int)((int64_t)alp->diff.y * 8 / alp->diff.x)), 256);
}

/* Find intersections of active lines within the band.
   Intersect and reorder them, and correct the bund top. */
static void
intersect_al(line_list *ll, fixed y, fixed *y_top, int draw, bool all_bands)
{
    fixed x = min_fixed, y1 = *y_top;
    active_line *alp, *stopx = NULL;
    active_line *endp = NULL;

    if (y == y1) {
        /* Rather the intersection algorithm can handle this case with
           retrieving x_next equal to x_current,
           we bypass it for safety reason.
         */
    } else if (draw >= 0 || all_bands) {
        /*
         * Loop invariants:
         *      alp = endp->next;
         *      for all lines lp from stopx up to alp,
         *        lp->x_next = AL_X_AT_Y(lp, y1).
         */
        for (alp = stopx = ll->x_list;
             INCR_EXPR(find_y), alp != 0;
             endp = alp, alp = alp->next
            ) {
            fixed nx = AL_X_AT_Y(alp, y1);
            fixed y_new;

            alp->x_next = nx;
            /* Check for intersecting lines. */
            if (nx >= x)
                x = nx;
            else if (alp->x_current >= endp->x_current &&
                     intersect(endp, alp, y, y1, &y_new)) {
                if (y_new <= y1) {
                    stopx = endp;
                    y1 = y_new;
                    if (endp->diff.x == 0)
                        nx = endp->start.x;
                    else if (alp->diff.x == 0)
                        nx = alp->start.x;
                    else {
                        fixed nx0 = AL_X_AT_Y(endp, y1);
                        fixed nx1 = AL_X_AT_Y(alp, y1);
                        if (nx0 != nx1) {
                            /* Different results due to arithmetic errors.
                               Choose an imtermediate point.
                               We don't like to use floating numbners here,
                               but the code with int64_t isn't much better. */
                            int64_t w0 = coord_weight(endp);
                            int64_t w1 = coord_weight(alp);

                            nx = (fixed)((w0 * nx0 + w1 * nx1) / (w0 + w1));
                        } else
                            nx = nx0;
                    }
                    endp->x_next = alp->x_next = nx;  /* Ensure same X. */
                    /* Can't guarantee same x for triple intersections here.
                       Will take care below */
                }
                if (nx > x)
                    x = nx;
            }
        }
        /* Recompute next_x for lines before the intersection. */
        for (alp = ll->x_list; alp != stopx; alp = alp->next)
            alp->x_next = AL_X_AT_Y(alp, y1);
        /* Ensure X monotonity (particularly imporoves triple intersections). */
        if (ll->x_list != NULL) {
            for (;;) {
                fixed x1;
                double sx = 0xbaadf00d; /* 'fixed' can overflow. We operate 7-bytes integers. */
                int k = 0xbaadf00d, n;

                endp = ll->x_list;
                x1 = endp->x_next;
                for (alp = endp->next; alp != NULL; x1 = alp->x_next, alp = alp->next)
                    if (alp->x_next < x1)
                        break;
                if (alp == NULL)
                    break;
                x1 = endp->x_next;
                n = 0;
                for (alp = endp->next; alp != NULL; alp = alp->next) {
                     x = alp->x_next;
                     if (x < x1) {
                        if (n == 0) {
                            if (endp->diff.x == 0) {
                                k = -1;
                                sx = x1;
                            } else {
                                k = coord_weight(endp);
                                sx = (double)x1 * k;
                            }
                            n = 1;
                        }
                        n++;
                        if (alp->diff.x == 0) {
                            /* Vertical lines have a higher priority. */
                            if (k <= -1) {
                                sx += x;
                                k--;
                            } else {
                                k = -1;
                                sx = x;
                            }
                        } else if (k > 0) {
                            int w = coord_weight(alp);

                            sx += (double)x * w;
                            k += w;
                        }
                     } else {
                        if (n > 1) {
                            k = any_abs(k);
                            set_x_next(endp, alp, (fixed)((sx + k / 2) / k));
                        }
                        x1 = alp->x_next;
                        n = 0;
                        endp = alp;
                     }
                }
                if (n > 1) {
                    k = any_abs(k);
                    set_x_next(endp, alp, (fixed)((sx + k / 2) / k));
                }
            }
        }
    } else {
        /* Recompute next_x for lines before the intersection. */
        for (alp = ll->x_list; alp != stopx; alp = alp->next)
            alp->x_next = AL_X_AT_Y(alp, y1);
    }
#ifdef DEBUG
    if (gs_debug_c('F')) {
        dmlprintf1(ll->memory, "[F]after loop: y1=%f\n", fixed2float(y1));
        print_line_list(ll->memory, ll->x_list);
    }
#endif
    *y_top = y1;
}

/* ---------------- Trapezoid filling loop ---------------- */

/* Generate specialized algorythms for the most important cases : */

/* The smart winding counter advance operator : */
#define signed_eo(a) ((a) < 0 ? -((a) & 1) : (a) > 0 ? ((a) & 1) : 0)
#define ADVANCE_WINDING(inside, alp, ll) \
                {   int k = alp->contour_count; \
                    int v = ll->windings[k]; \
                    inside -= signed_eo(v); \
                    v = ll->windings[k] += alp->direction; \
                    inside += signed_eo(v); \
                }

#define IS_SPOTAN 0
#define SMART_WINDING 1
#define FILL_ADJUST 0
#define FILL_DIRECT 1
#define TEMPLATE_spot_into_trapezoids spot_into_trapezoids__nj_fd_sw
#include "gxfilltr.h"
#undef IS_SPOTAN
#undef SMART_WINDING
#undef FILL_ADJUST
#undef FILL_DIRECT
#undef TEMPLATE_spot_into_trapezoids

#define IS_SPOTAN 0
#define SMART_WINDING 1
#define FILL_ADJUST 0
#define FILL_DIRECT 0
#define TEMPLATE_spot_into_trapezoids spot_into_trapezoids__nj_nd_sw
#include "gxfilltr.h"
#undef IS_SPOTAN
#undef SMART_WINDING
#undef FILL_ADJUST
#undef FILL_DIRECT
#undef TEMPLATE_spot_into_trapezoids

#undef signed_eo
#undef ADVANCE_WINDING
/* The simple winding counter advance operator : */
#define ADVANCE_WINDING(inside, alp, ll) inside += alp->direction

#define IS_SPOTAN 1
#define SMART_WINDING 0
#define FILL_ADJUST 0
#define FILL_DIRECT 1
#define TEMPLATE_spot_into_trapezoids spot_into_trapezoids__spotan
#include "gxfilltr.h"
#undef IS_SPOTAN
#undef SMART_WINDING
#undef FILL_ADJUST
#undef FILL_DIRECT
#undef TEMPLATE_spot_into_trapezoids

#define IS_SPOTAN 0
#define SMART_WINDING 0
#define FILL_ADJUST 1
#define FILL_DIRECT 1
#define TEMPLATE_spot_into_trapezoids spot_into_trapezoids__aj_fd
#include "gxfilltr.h"
#undef IS_SPOTAN
#undef SMART_WINDING
#undef FILL_ADJUST
#undef FILL_DIRECT
#undef TEMPLATE_spot_into_trapezoids

#define IS_SPOTAN 0
#define SMART_WINDING 0
#define FILL_ADJUST 1
#define FILL_DIRECT 0
#define TEMPLATE_spot_into_trapezoids spot_into_trapezoids__aj_nd
#include "gxfilltr.h"
#undef IS_SPOTAN
#undef SMART_WINDING
#undef FILL_ADJUST
#undef FILL_DIRECT
#undef TEMPLATE_spot_into_trapezoids

#define IS_SPOTAN 0
#define SMART_WINDING 0
#define FILL_ADJUST 0
#define FILL_DIRECT 1
#define TEMPLATE_spot_into_trapezoids spot_into_trapezoids__nj_fd
#include "gxfilltr.h"
#undef IS_SPOTAN
#undef SMART_WINDING
#undef FILL_ADJUST
#undef FILL_DIRECT
#undef TEMPLATE_spot_into_trapezoids

#define IS_SPOTAN 0
#define SMART_WINDING 0
#define FILL_ADJUST 0
#define FILL_DIRECT 0
#define TEMPLATE_spot_into_trapezoids spot_into_trapezoids__nj_nd
#include "gxfilltr.h"
#undef IS_SPOTAN
#undef SMART_WINDING
#undef FILL_ADJUST
#undef FILL_DIRECT
#undef TEMPLATE_spot_into_trapezoids

#undef ADVANCE_WINDING

/* Main filling loop.  Takes lines off of y_list and adds them to */
/* x_list as needed.  band_mask limits the size of each band, */
/* by requiring that ((y1 - 1) & band_mask) == (y0 & band_mask). */
static int
spot_into_trapezoids(line_list *ll, fixed band_mask)
{
    /* For better performance, choose a specialized algorithm : */
    const fill_options * const fo = ll->fo;

    if (fo->is_spotan)
        return spot_into_trapezoids__spotan(ll, band_mask);
    if (fo->adjust_below | fo->adjust_above | fo->adjust_left | fo->adjust_right) {
        if (fo->fill_direct)
            return spot_into_trapezoids__aj_fd(ll, band_mask);
        else
            return spot_into_trapezoids__aj_nd(ll, band_mask);
    } else if (ll->windings != NULL) {
        if (fo->fill_direct)
            return spot_into_trapezoids__nj_fd_sw(ll, band_mask);
        else
            return spot_into_trapezoids__nj_nd_sw(ll, band_mask);
    } else {
        if (fo->fill_direct)
            return spot_into_trapezoids__nj_fd(ll, band_mask);
        else
            return spot_into_trapezoids__nj_nd(ll, band_mask);
    }
}

/* ---------------- Range list management ---------------- */

/*
 * We originally thought we would store fixed values in coordinate ranges,
 * but it turned out we want to store integers.
 */
typedef int coord_value_t;
#define MIN_COORD_VALUE min_int
#define MAX_COORD_VALUE max_int
/*
 * The invariant for coord_range_ts in a coord_range_list_t:
 *      if prev == 0:
 *              next != 0
 *              rmin == rmax == MIN_COORD_VALUE
 *      else if next == 0:
 *              prev != 0
 *              rmin == rmax == MAX_COORD_VALUE
 *      else
 *              rmin < rmax
 *      if prev != 0:
 *              prev->next == this
 *              prev->rmax < rmin
 *      if next != 0:
 *              next->prev = this
 *              next->rmin > rmax
 * A coord_range_list_t has a boundary element at each end to avoid having
 * to test for null pointers in the insertion loop.  The boundary elements
 * are the only empty ranges.
 */
typedef struct coord_range_s coord_range_t;
struct coord_range_s {
    coord_value_t rmin, rmax;
    coord_range_t *prev, *next;
    coord_range_t *alloc_next;
};
/* We declare coord_range_ts as simple because they only exist transiently. */
gs_private_st_simple(st_coord_range, coord_range_t, "coord_range_t");

typedef struct coord_range_list_s {
    gs_memory_t *memory;
    struct rl_ {
        coord_range_t *first, *next, *limit;
    } local;
    coord_range_t *allocated;
    coord_range_t *freed;
    coord_range_t *current;
    coord_range_t first, last;
} coord_range_list_t;

static coord_range_t *
range_alloc(coord_range_list_t *pcrl)
{
    coord_range_t *pcr;

    if (pcrl->freed) {
        pcr = pcrl->freed;
        pcrl->freed = pcr->next;
    } else if (pcrl->local.next < pcrl->local.limit)
        pcr = pcrl->local.next++;
    else {
        pcr = gs_alloc_struct(pcrl->memory, coord_range_t, &st_coord_range,
                              "range_alloc");
        if (pcr == 0)
            return 0;
        pcr->alloc_next = pcrl->allocated;
        pcrl->allocated = pcr;
    }
    return pcr;
}

static void
range_delete(coord_range_list_t *pcrl, coord_range_t *pcr)
{
    if_debug3('Q', "[Qr]delete 0x%lx: [%d,%d)\n", (ulong)pcr, pcr->rmin,
              pcr->rmax);
    pcr->prev->next = pcr->next;
    pcr->next->prev = pcr->prev;
    pcr->next = pcrl->freed;
    pcrl->freed = pcr;
}

static void
range_list_clear(coord_range_list_t *pcrl)
{
    if_debug0('Q', "[Qr]clearing\n");
    pcrl->first.next = &pcrl->last;
    pcrl->last.prev = &pcrl->first;
    pcrl->current = &pcrl->last;
}

/* ------ "Public" procedures ------ */

/* Initialize a range list.  We require num_local >= 2. */
static void range_list_clear(coord_range_list_t *);
static void
range_list_init(coord_range_list_t *pcrl, coord_range_t *pcr_local,
                int num_local, gs_memory_t *mem)
{
    pcrl->memory = mem;
    pcrl->local.first = pcrl->local.next = pcr_local;
    pcrl->local.limit = pcr_local + num_local;
    pcrl->allocated = pcrl->freed = 0;
    pcrl->first.rmin = pcrl->first.rmax = MIN_COORD_VALUE;
    pcrl->first.prev = 0;
    pcrl->last.rmin = pcrl->last.rmax = MAX_COORD_VALUE;
    pcrl->last.next = 0;
    range_list_clear(pcrl);
}

/* Reset an initialized range list to the empty state. */
static void
range_list_reset(coord_range_list_t *pcrl)
{
    if (pcrl->first.next != &pcrl->last) {
        pcrl->last.prev->next = pcrl->freed;
        pcrl->freed = pcrl->first.next;
        range_list_clear(pcrl);
    }
}

/*
 * Move the cursor to the beginning of a range list, in the belief that
 * the next added ranges will roughly parallel the existing ones.
 * (Only affects performance, not function.)
 */
static inline void
range_list_rescan(coord_range_list_t *pcrl)
{
    pcrl->current = pcrl->first.next;
}

/* Free a range list. */
static void
range_list_free(coord_range_list_t *pcrl)
{
    coord_range_t *pcr;

    while ((pcr = pcrl->allocated) != 0) {
        coord_range_t *next = pcr->alloc_next;

        gs_free_object(pcrl->memory, pcr, "range_list_free");
        pcrl->allocated = next;
    }
}

/* Add a range. */
static int
range_list_add(coord_range_list_t *pcrl, coord_value_t rmin, coord_value_t rmax)
{
    coord_range_t *pcr = pcrl->current;

    if (rmin >= rmax)
        return 0;
    /*
     * Usually, ranges are added in increasing order within each scan line,
     * and overlapping ranges don't differ much.  Optimize accordingly.
     */
 top:
    if (rmax < pcr->rmin) {
        if (rmin > pcr->prev->rmax)
            goto ins;
        pcr = pcr->prev;
        goto top;
    }
    if (rmin > pcr->rmax) {
        pcr = pcr->next;
        if (rmax < pcr->rmin)
            goto ins;
        goto top;
    }
    /*
     * Now we know that (rmin,rmax) overlaps (pcr->rmin,pcr->rmax).
     * Don't delete or merge into the special min and max ranges.
     */
    while (rmin <= pcr->prev->rmax) {
        /* Merge the range below pcr into this one. */
        if (!pcr->prev->prev)
            break;              /* don't merge into the min range */
        pcr->rmin = pcr->prev->rmin;
        range_delete(pcrl, pcr->prev);
    }
    while (rmax >= pcr->next->rmin) {
        /* Merge the range above pcr into this one. */
        if (!pcr->next->next)
            break;              /* don't merge into the max range */
        pcr->rmax = pcr->next->rmax;
        range_delete(pcrl, pcr->next);
    }
    /*
     * Now we know that the union of (rmin,rmax) and (pcr->rmin,pcr->rmax)
     * doesn't overlap or abut either adjacent range, except that it may
     * abut if the adjacent range is the special min or max range.
     */
    if (rmin < pcr->rmin) {
        if_debug3('Q', "[Qr]update 0x%lx => [%d,%d)\n", (ulong)pcr, rmin,
                  pcr->rmax);
        pcr->rmin = rmin;
    }
    if (rmax > pcr->rmax) {
        if_debug3('Q', "[Qr]update 0x%lx => [%d,%d)\n", (ulong)pcr, pcr->rmin,
                  rmax);
        pcr->rmax = rmax;
    }
    pcrl->current = pcr->next;
    return 0;
 ins:
    /* Insert a new range below pcr. */
    {
        coord_range_t *prev = range_alloc(pcrl);

        if (prev == 0)
            return_error(gs_error_VMerror);
        if_debug3('Q', "[Qr]insert 0x%lx: [%d,%d)\n", (ulong)prev, rmin, rmax);
        prev->rmin = rmin, prev->rmax = rmax;
        (prev->prev = pcr->prev)->next = prev;
        prev->next = pcr;
        pcr->prev = prev;
    }
    pcrl->current = pcr;
    return 0;
}

/*
 * Merge regions for active segments starting at a given Y, or all active
 * segments, up to the end of the sampling band or the end of the segment,
 * into the range list.
 */
static int
merge_ranges(coord_range_list_t *pcrl, const line_list *ll, fixed y_min, fixed y_top)
{
    active_line *alp, *nlp;
    int code = 0;

    range_list_rescan(pcrl);
    for (alp = ll->x_list; alp != 0 && code >= 0; alp = nlp) {
        fixed x0 = alp->x_current, x1, xt;
        bool forth = (alp->direction == DIR_UP || !alp->fi.curve);
        fixed xe = (forth ? alp->fi.x3 : alp->fi.x0);
        fixed ye = (forth ? alp->fi.y3 : alp->fi.y0);

        nlp = alp->next;
        if (alp->start.y < y_min)
            continue;
        if (alp->monotonic_x && alp->monotonic_y && ye <= y_top) {
            x1 = xe;
            if (x0 > x1)
                xt = x0, x0 = x1, x1 = xt;
            code = range_list_add(pcrl,
                                  fixed2int_pixround(x0 - ll->fo->adjust_left),
                                  fixed2int_rounded(x1 + ll->fo->adjust_right));
            alp->more_flattened = false; /* Skip all the segments left. */
        } else {
            x1 = x0;
            for (;;) {
                if (alp->end.y <= y_top)
                    xt = alp->end.x;
                else
                    xt = AL_X_AT_Y(alp, y_top);
                x0 = min(x0, xt);
                x1 = max(x1, xt);
                if (!alp->more_flattened || alp->end.y > y_top)
                    break;
                code = step_al(alp, true);
                if (code < 0)
                    return code;
                if (alp->end.y < alp->start.y) {
                    remove_al(ll, alp); /* End of a monotonic part of a curve. */
                    break;
                }
            }
            code = range_list_add(pcrl,
                                  fixed2int_pixround(x0 - ll->fo->adjust_left),
                                  fixed2int_rounded(x1 + ll->fo->adjust_right));
        }

    }
    return code;
}

/* ---------------- Scan line filling loop ---------------- */

/* defina specializations of the scanline algorithm. */

#define FILL_DIRECT 1
#define TEMPLATE_spot_into_scanlines spot_into_scan_lines_fd
#include "gxfillsl.h"
#undef FILL_DIRECT
#undef TEMPLATE_spot_into_scanlines

#define FILL_DIRECT 0
#define TEMPLATE_spot_into_scanlines spot_into_scan_lines_nd
#include "gxfillsl.h"
#undef FILL_DIRECT
#undef TEMPLATE_spot_into_scanlines

static int
spot_into_scan_lines(line_list *ll, fixed band_mask)
{
    if (ll->fo->fill_direct)
        return spot_into_scan_lines_fd(ll, band_mask);
    else
        return spot_into_scan_lines_nd(ll, band_mask);
}
