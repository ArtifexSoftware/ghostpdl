/* Copyright (C) 1989-2003 artofcode LLC.  All rights reserved.
  
  This software is provided AS-IS with no warranty, either express or
  implied.
  
  This software is distributed under license and may not be copied,
  modified or distributed except as expressly authorized under the terms
  of the license contained in the file LICENSE in this distribution.
  
  For more information about licensing, please refer to
  http://www.ghostscript.com/licensing/. For information on
  commercial licensing, go to http://www.artifex.com/licensing/ or
  contact Artifex Software, Inc., 101 Lucas Valley Road #110,
  San Rafael, CA  94903, U.S.A., +1(415)492-9861.
*/

/* $Id$ */
/* Lower-level path filling procedures */

#include "gx.h"
#include "gserrors.h"
#include "gsstruct.h"
#include "gxfixed.h"
#include "gxdevice.h"
#include "gzpath.h"
#include "gzcpath.h"
#include "gxdcolor.h"
#include "gxhttile.h"
#include "gxistate.h"
#include "gxpaint.h"		/* for prototypes */
#include "gxfdrop.h"
#include "gxfill.h"
#include "gsptype2.h"
#include "gdevddrw.h"
#include "vdtrace.h"

/*
 * Define which fill algorithm(s) to use.  At least one of the following
 * two #defines must be included (not commented out).
 * The dropout prevention code requires FILL_TRAPEZOIDS defined.
 */

#define FILL_SCAN_LINES
#define FILL_TRAPEZOIDS
/*
 * Define whether to sample curves when using the scan line algorithm
 * rather than flattening them.  This produces more accurate output, at
 * some cost in time.
 */
#define FILL_CURVES

#ifdef DEBUG
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
private int
x_order(const active_line *lp1, const active_line *lp2)
{
    bool s1;

    INCR(order);
    if (lp1->x_current < lp2->x_current)
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
private void
print_active_line(const char *label, const active_line * alp)
{
    dlprintf5("[f]%s 0x%lx(%d): x_current=%f x_next=%f\n",
	      label, (ulong) alp, alp->direction,
	      fixed2float(alp->x_current), fixed2float(alp->x_next));
    dlprintf5("    start=(%f,%f) pt_end=0x%lx(%f,%f)\n",
	      fixed2float(alp->start.x), fixed2float(alp->start.y),
	      (ulong) alp->pseg,
	      fixed2float(alp->end.x), fixed2float(alp->end.y));
    dlprintf2("    prev=0x%lx next=0x%lx\n",
	      (ulong) alp->prev, (ulong) alp->next);
}
private void
print_line_list(const active_line * flp)
{
    const active_line *lp;

    for (lp = flp; lp != 0; lp = lp->next) {
	fixed xc = lp->x_current, xn = lp->x_next;

	dlprintf3("[f]0x%lx(%d): x_current/next=%g",
		  (ulong) lp, lp->direction,
		  fixed2float(xc));
	if (xn != xc)
	    dprintf1("/%g", fixed2float(xn));
	dputc('\n');
    }
}
inline private void
print_al(const char *label, const active_line * alp)
{
    if (gs_debug_c('F'))
	print_active_line(label, alp);
}
#else
#define print_al(label,alp) DO_NOTHING
#endif


/* Forward declarations */
private void init_line_list(line_list *, gs_memory_t *);
private void unclose_path(gx_path *, int);
private void free_line_list(line_list *);
private int add_y_list(gx_path *, line_list *, fixed, fixed,
		       const gs_fixed_rect *);
private int add_y_line(const segment *, const segment *, int, line_list *);
private void insert_x_new(active_line *, line_list *);
private bool end_x_line(active_line *, bool);

#define FILL_LOOP_PROC(proc)\
int proc(line_list *, gx_device *,\
  const gx_fill_params *, const gx_device_color *, gs_logical_operation_t,\
  const gs_fixed_rect *, fixed, fixed, fixed, fixed, fixed)
private FILL_LOOP_PROC(fill_loop_by_scan_lines);
private FILL_LOOP_PROC(fill_loop_by_trapezoids);

/*
 * This is the general path filling algorithm.
 * It uses the center-of-pixel rule for filling.
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

/*
 * Tweak the fill adjustment if necessary so that (nearly) empty
 * rectangles are guaranteed to produce some output.  This is a hack
 * to work around a bug in the Microsoft Windows PostScript driver,
 * which draws thin lines by filling zero-width rectangles, and in
 * some other drivers that try to fill epsilon-width rectangles.
 */
void
gx_adjust_if_empty(const gs_fixed_rect * pbox, gs_fixed_point * adjust)
{
    /*
     * For extremely large coordinates, the obvious subtractions could
     * overflow.  We can work around this easily by noting that since
     * we know q.{x,y} >= p.{x,y}, the subtraction overflows iff the
     * result is negative.
     */
    const fixed
	  dx = pbox->q.x - pbox->p.x, dy = pbox->q.y - pbox->p.y;

    if (dx < fixed_half && dx > 0 && (dy >= int2fixed(2) || dy < 0)) {
	adjust->x = arith_rshift_1(fixed_1 + fixed_epsilon - dx);
	if_debug1('f', "[f]thin adjust_x=%g\n",
		  fixed2float(adjust->x));
    } else if (dy < fixed_half && dy > 0 && (dx >= int2fixed(2) || dx < 0)) {
	adjust->y = arith_rshift_1(fixed_1 + fixed_epsilon - dy);
	if_debug1('f', "[f]thin adjust_y=%g\n",
		  fixed2float(adjust->y));
    }
}

/*
 * The general fill  path algorithm.
 */
private int
gx_general_fill_path(gx_device * pdev, const gs_imager_state * pis,
		     gx_path * ppath, const gx_fill_params * params,
		 const gx_device_color * pdevc, const gx_clip_path * pcpath)
{
    gs_fixed_point adjust;
    gs_logical_operation_t lop = pis->log_op;
    gs_fixed_rect ibox, bbox;
    gx_device_clip cdev;
    gx_device *dev = pdev;
    gx_device *save_dev = dev;
    gx_path ffpath;
    gx_path *pfpath;
    int code;
    fixed adjust_left, adjust_right, adjust_below, adjust_above;
    int max_fill_band = dev->max_fill_band;
#define NO_BAND_MASK ((fixed)(-1) << (sizeof(fixed) * 8 - 1))
    bool fill_by_trapezoids;
    bool pseudo_rasterization;
    line_list lst;

    adjust = params->adjust;
    /*
     * Compute the bounding box before we flatten the path.
     * This can save a lot of time if the path has curves.
     * If the path is neither fully within nor fully outside
     * the quick-check boxes, we could recompute the bounding box
     * and make the checks again after flattening the path,
     * but right now we don't bother.
     */
    gx_path_bbox(ppath, &ibox);
#   define SMALL_CHARACTER 500
    lst.bbox_left = fixed2int(ibox.p.x - adjust.x - fixed_epsilon);
    lst.bbox_width = fixed2int(fixed_ceiling(ibox.q.x + adjust.x)) - lst.bbox_left;
    pseudo_rasterization = ((adjust.x | adjust.y) == 0 && 
			    ibox.q.y - ibox.p.y < SMALL_CHARACTER * fixed_scale &&
			    ibox.q.x - ibox.p.x < SMALL_CHARACTER * fixed_scale);
    if (params->fill_zero_width && !pseudo_rasterization)
	gx_adjust_if_empty(&ibox, &adjust);
    if (vd_enabled) {
	fixed x0 = int2fixed(fixed2int(ibox.p.x - adjust.x - fixed_epsilon));
	fixed x1 = int2fixed(fixed2int(ibox.q.x + adjust.x + fixed_scale - fixed_epsilon));
	fixed y0 = int2fixed(fixed2int(ibox.p.y - adjust.y - fixed_epsilon));
	fixed y1 = int2fixed(fixed2int(ibox.q.y + adjust.y + fixed_scale - fixed_epsilon)), k;

	for (k = x0; k <= x1; k += fixed_scale)
	    vd_bar(k, y0, k, y1, 1, RGB(128, 128, 128));
	for (k = y0; k <= y1; k += fixed_scale)
	    vd_bar(x0, k, x1, k, 1, RGB(128, 128, 128));
    }
    /* Check the bounding boxes. */
    if_debug6('f', "[f]adjust=%g,%g bbox=(%g,%g),(%g,%g)\n",
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
	if_debug4('f', "   outer_box=(%g,%g),(%g,%g)\n",
		  fixed2float(bbox.p.x), fixed2float(bbox.p.y),
		  fixed2float(bbox.q.x), fixed2float(bbox.q.y));
	rect_intersect(ibox, bbox);
	if (ibox.p.x - adjust.x >= ibox.q.x + adjust.x ||
	    ibox.p.y - adjust.y >= ibox.q.y + adjust.y
	    ) { 		/* Intersection of boxes is empty! */
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
	    gx_make_clip_device(&cdev, gx_cpath_list(pcpath));
	    cdev.target = save_dev;
	    cdev.max_fill_band = save_dev->max_fill_band;
	    (*dev_proc(dev, open_device)) (dev);
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
	adjust_left = fixed_half - fixed_epsilon,
	    adjust_right = fixed_half /* + fixed_epsilon */ ;	/* see above */
    else
	adjust_left = adjust_right = adjust.x;
    if (adjust.y == fixed_half)
	adjust_below = fixed_half - fixed_epsilon,
	    adjust_above = fixed_half /* + fixed_epsilon */ ;	/* see above */
    else
	adjust_below = adjust_above = adjust.y;
    /* Initialize the active line list. */
    init_line_list(&lst, ppath->memory);
    lst.pseudo_rasterization = pseudo_rasterization;
    lst.pdevc = pdevc;
    lst.lop = lop;
    /*
     * We have a choice of two different filling algorithms:
     * scan-line-based and trapezoid-based.  They compare as follows:
     *
     *	    Scan    Trap
     *	    ----    ----
     *	     skip   +draw   0-height horizontal lines
     *	     slow   +fast   rectangles
     *	    +fast    slow   curves
     *	    +yes     no     write pixels at most once when adjust != 0
     *
     * Normally we use the scan line algorithm for characters, where curve
     * speed is important, and for non-idempotent RasterOps, where double
     * pixel writing must be avoided, and the trapezoid algorithm otherwise.
     * However, we always use the trapezoid algorithm for rectangles.
     */
#define DOUBLE_WRITE_OK() lop_is_idempotent(lop)
#ifdef FILL_SCAN_LINES
#  ifdef FILL_TRAPEZOIDS
    fill_by_trapezoids =
	(pseudo_rasterization || !gx_path_has_curves(ppath) || params->flatness >= 1.0);
#  else
    fill_by_trapezoids = false;
#  endif
#else
    fill_by_trapezoids = true;
#endif
#ifdef FILL_TRAPEZOIDS
    if (fill_by_trapezoids && !DOUBLE_WRITE_OK()) {
	/* Avoid filling rectangles by scan line. */
	gs_fixed_rect rbox;

	if (gx_path_is_rectangular(ppath, &rbox)) {
	    int x0 = fixed2int_pixround(rbox.p.x - adjust_left);
	    int y0 = fixed2int_pixround(rbox.p.y - adjust_below);
	    int x1 = fixed2int_pixround(rbox.q.x + adjust_right);
	    int y1 = fixed2int_pixround(rbox.q.y + adjust_above);

	    return gx_fill_rectangle_device_rop(x0, y0, x1 - x0, y1 - y0,
						pdevc, dev, lop);
	}
	fill_by_trapezoids = false; /* avoid double write */
    }
#endif
#undef DOUBLE_WRITE_OK
    /*
     * Pre-process curves.  When filling by trapezoids, we need to
     * flatten the path completely; when filling by scan lines, we only
     * need to monotonize it, unless FILL_CURVES is undefined.
     */
    gx_path_init_local(&ffpath, ppath->memory);
    if (!gx_path_has_curves(ppath))	/* don't need to flatten */
	pfpath = ppath;
    else
#if defined(FILL_CURVES) && defined(FILL_SCAN_LINES)
	/* Filling curves is possible. */
#  ifdef FILL_TRAPEZOIDS
	/* Not filling curves is also possible. */
    if (fill_by_trapezoids)
#  endif
#endif
#if !defined(FILL_CURVES) || defined(FILL_TRAPEZOIDS)
	/* Not filling curves is possible. */
	{   
	    gx_path_init_local(&ffpath, ppath->memory);
	    code = gx_path_add_flattened_accurate(ppath, &ffpath,
						  params->flatness,
						  pis->accurate_curves);
	    if (code < 0)
		return code;
	    pfpath = &ffpath;
	}
#endif
#if defined(FILL_CURVES) && defined(FILL_SCAN_LINES)
    /* Filling curves is possible. */
#  ifdef FILL_TRAPEZOIDS
    /* Not filling curves is also possible. */
    else
#  endif
    if (gx_path_is_monotonic(ppath))
	pfpath = ppath;
    else {
	gx_path_init_local(&ffpath, ppath->memory);
	code = gx_path_add_monotonized(ppath, &ffpath);
	if (code < 0)
	    return code;
	pfpath = &ffpath;
    }
#endif
    if ((code = add_y_list(pfpath, &lst, adjust_below, adjust_above, &ibox)) < 0)
	goto nope;
    {
	FILL_LOOP_PROC((*fill_loop));

	/* Some short-sighted compilers won't allow a conditional here.... */
	if (fill_by_trapezoids)
	    fill_loop = fill_loop_by_trapezoids;
	else
	    fill_loop = fill_loop_by_scan_lines;
	/* We assume (adjust.x | adjust.y) == 0 iff it's a character. */
	if (lst.bbox_width > MAX_LOCAL_SECTION && lst.pseudo_rasterization) {
	    /*
	     * Note that execution pass here only for character size
	     * grater that MAX_LOCAL_SECTION and lesser than 
	     * SMALL_CHARACTER. Therefore with !arch_small_memory
	     * the dynamic allocation only happens for characters 
	     * wider than 100 pixels.
	     */
	    lst.margin_set0.sect = (section *)gs_alloc_struct_array(pdev->memory, lst.bbox_width * 2, 
						   section, &st_section, "section");
	    if (lst.margin_set0.sect == 0)
		return_error(gs_error_VMerror);
	    lst.margin_set1.sect = lst.margin_set0.sect + lst.bbox_width;
	}
	if (lst.pseudo_rasterization) {
	    init_section(lst.margin_set0.sect, 0, lst.bbox_width);
	    init_section(lst.margin_set1.sect, 0, lst.bbox_width);
	}
	code = (*fill_loop)
	    (&lst, dev, params, pdevc, lop, &ibox,
	     adjust_left, adjust_right, adjust_below, adjust_above,
	   (max_fill_band == 0 ? NO_BAND_MASK : int2fixed(-max_fill_band)));
	if (lst.margin_set0.sect != lst.local_section0 && 
	    lst.margin_set0.sect != lst.local_section1)
	    gs_free_object(pdev->memory, min(lst.margin_set0.sect, lst.margin_set1.sect), "section");
    }
  nope:if (lst.close_count != 0)
	unclose_path(pfpath, lst.close_count);
    free_line_list(&lst);
    if (pfpath != ppath)	/* had to flatten */
	gx_path_free(pfpath, "gx_default_fill_path(flattened path)");
#ifdef DEBUG
    if (gs_debug_c('f')) {
	dlputs("[f]  # alloc    up  down horiz step slowx  iter  find  band bstep bfill\n");
	dlprintf5(" %5ld %5ld %5ld %5ld %5ld",
		  stats_fill.fill, stats_fill.fill_alloc,
		  stats_fill.y_up, stats_fill.y_down,
		  stats_fill.horiz);
	dlprintf4(" %5ld %5ld %5ld %5ld",
		  stats_fill.x_step, stats_fill.slow_x,
		  stats_fill.iter, stats_fill.find_y);
	dlprintf3(" %5ld %5ld %5ld\n",
		  stats_fill.band, stats_fill.band_step,
		  stats_fill.band_fill);
	dlputs("[f]    afill slant shall sfill mqcrs order slowo\n");
	dlprintf7("       %5ld %5ld %5ld %5ld %5ld %5ld %5ld\n",
		  stats_fill.afill, stats_fill.slant,
		  stats_fill.slant_shallow, stats_fill.sfill,
		  stats_fill.mq_cross, stats_fill.order,
		  stats_fill.slow_order);
    }
#endif
    return code;
}

/*
 * Fill a path.  This is the default implementation of the driver
 * fill_path procedure.
 */
int
gx_default_fill_path(gx_device * pdev, const gs_imager_state * pis,
		     gx_path * ppath, const gx_fill_params * params,
		 const gx_device_color * pdevc, const gx_clip_path * pcpath)
{
    int code;

    if (gx_dc_is_pattern2_color(pdevc)) {
	/*  Optimization for shading fill :
	    The general path filling algorithm subdivides fill region with 
	    trapezoid or rectangle subregions and then paints each subregion 
	    with given color. If the color is shading, each subregion to be 
	    subdivided into areas of constant color. But with radial 
	    shading each area is a high order polygon, being 
	    subdivided into smaller subregions, so as total number of
	    subregions grows huge. Faster processing is done here by changing 
	    the order of subdivision cycles : we first subdivide the shading into 
	    areas of constant color, then apply the general path filling algorithm 
	    (i.e. subdivide each area into trapezoids or rectangles), using the 
	    filling path as clip mask.
	*/

	gx_clip_path cpath_intersection;
	gx_path path_intersection;

	/*  Shading fill algorithm uses "current path" parameter of the general
	    path filling algorithm as boundary of constant color area,
	    so we need to intersect the filling path with the clip path now,
	    reducing the number of pathes passed to it :
	*/
	gx_path_init_local(&path_intersection, pdev->memory);
	gx_cpath_init_local_shared(&cpath_intersection, pcpath, pdev->memory);
	if ((code = gx_cpath_intersect(&cpath_intersection, ppath, params->rule, (gs_imager_state *)pis)) >= 0)
	    code = gx_cpath_to_path(&cpath_intersection, &path_intersection);

	/* Do fill : */
	if (code >= 0)
	    code = gx_dc_pattern2_fill_path_adjusted(pdevc, &path_intersection, NULL,  pdev);

	/* Destruct local data and return :*/
	gx_path_free(&path_intersection, "shading_fill_path_intersection");
	gx_cpath_free(&cpath_intersection, "shading_fill_cpath_intersection");
    } else {
	if ((params->adjust.x | params->adjust.y) == 0)
	    vd_get_dc('f');
	if (vd_enabled) {
	    vd_set_shift(0, 0);
	    vd_set_scale(VD_SCALE);
	    vd_set_origin(0, 0);
	    vd_erase(RGB(192, 192, 192));
	}
	code = gx_general_fill_path(pdev, pis, ppath, params, pdevc, pcpath);
	vd_release_dc;
    }
    return code;
}

/* Initialize the line list for a path. */
private void
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
    ll->margin_set0.margin_list = ll->margin_set1.margin_list = 0;
    ll->margin_set0.margin_touched = ll->margin_set1.margin_touched = 0;
    ll->margin_set0.y = ll->margin_set1.y = 0; /* A stub against indeterminism. Don't use it. */
    ll->free_margin_list = 0;
    ll->local_margin_alloc_count = 0;
    ll->margin_set0.sect = ll->local_section0;
    ll->margin_set1.sect = ll->local_section1;
    ll->pseudo_rasterization = false;
    /* Do not initialize ll->bbox_left, ll->bbox_width - they were set in advance. */
    INCR(fill);
}

/* Unlink any line_close segments added temporarily. */
private void
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

/* Free the line list. */
private void
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
    free_all_margins(ll);
}

/*
 * Construct a Y-sorted list of segments for rasterizing a path.  We assume
 * the path is non-empty.  Only include non-horizontal lines or (monotonic)
 * curve segments where one endpoint is locally Y-minimal, and horizontal
 * lines that might color some additional pixels.
 */
private int
add_y_list(gx_path * ppath, line_list *ll, fixed adjust_below, fixed adjust_above,
	   const gs_fixed_rect * pbox)
{
    segment *pseg = (segment *) ppath->first_subpath;
    int close_count = 0;
    /* fixed xmin = pbox->p.x; *//* not currently used */
    fixed ymin = pbox->p.y;
    /* fixed xmax = pbox->q.x; *//* not currently used */
    fixed ymax = pbox->q.y;
    bool pseudo_rasterization = ll->pseudo_rasterization;
    int code;

    while (pseg) {
	/* We know that pseg points to a subpath head (s_start). */
	subpath *psub = (subpath *) pseg;
	segment *plast = psub->last;
	int dir = 2;		/* hack to skip first segment */
	int first_dir, prev_dir;
	segment *prev;

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
	while ((prev_dir = dir, prev = pseg,
		(pseg = pseg->next) != 0 && pseg->type != s_start)
	    ) {
	    /* This element is either a line or a monotonic curve segment. */
	    fixed iy = pseg->pt.y;
	    fixed py = prev->pt.y;

	    /*
	     * Segments falling entirely outside the ibox in Y
	     * are treated as though they were horizontal, *
	     * i.e., they are never put on the list.
	     */
#define COMPUTE_DIR(xo, xe, yo, ye)\
  (ye > yo ? (ye <= ymin || yo >= ymax ? 0 : DIR_UP) :\
   ye < yo ? (yo <= ymin || ye >= ymax ? 0 : DIR_DOWN) :\
   2)
#define ADD_DIR_LINES(prev2, prev, this, pdir, dir)\
  BEGIN\
    if (pdir)\
      { if ((code = add_y_line(prev2, prev, pdir, ll)) < 0) return code; }\
    if (dir)\
      { if ((code = add_y_line(prev, this, dir, ll)) < 0) return code; }\
  END
	    dir = COMPUTE_DIR(prev->pt.x, pseg->pt.x, py, iy);
	    if (dir == 2) {	/* Put horizontal lines on the list */
		/* if they would color any pixels. */
		if (
		    (pseudo_rasterization && (prev->pt.x != pseg->pt.x)) ||  /* Since TT characters are not hinted. */
		    fixed2int_pixround(iy - adjust_below) <
		    fixed2int_pixround(iy + adjust_above)
		    ) {
		    INCR(horiz);
		    if ((code = add_y_line(prev, pseg,
					   DIR_HORIZONTAL, ll)) < 0
			)
			return code;
		}
		dir = 0;
	    }
	    if (dir > prev_dir) {
		ADD_DIR_LINES(prev->prev, prev, pseg, prev_dir, dir);
	    } else if (prev_dir == 2)	/* first segment */
		first_dir = dir;
	    if (pseg == plast) {
		/*
		 * We skipped the first segment of the subpath, so the last
		 * segment must receive special consideration.	Note that we
		 * have `closed' all subpaths.
		 */
		if (first_dir > dir) {
		    ADD_DIR_LINES(prev, pseg, psub->next, dir, first_dir);
		}
	    }
	}
#undef COMPUTE_DIR
#undef ADD_DIR_LINES
    }
    return close_count;
}
/*
 * Internal routine to test a segment and add it to the pending list if
 * appropriate.
 */
private int
add_y_line(const segment * prev_lp, const segment * lp, int dir, line_list *ll)
{
    gs_fixed_point this, prev;
    active_line *alp = ll->next_active;
    fixed y_start;

    if (alp == ll->limit) {	/* Allocate separately */
	alp = gs_alloc_struct(ll->memory, active_line,
			      &st_active_line, "active line");
	if (alp == 0)
	    return_error(gs_error_VMerror);
	alp->alloc_next = ll->active_area;
	ll->active_area = alp;
	INCR(fill_alloc);
    } else
	ll->next_active++;
    this.x = lp->pt.x;
    this.y = lp->pt.y;
    prev.x = prev_lp->pt.x;
    prev.y = prev_lp->pt.y;
    switch ((alp->direction = dir)) {
	case DIR_UP:
	    y_start = prev.y;
	    SET_AL_POINTS(alp, prev, this);
	    alp->pseg = lp;
	    break;
	case DIR_DOWN:
	    y_start = this.y;
	    SET_AL_POINTS(alp, this, prev);
	    alp->pseg = prev_lp;
	    break;
	case DIR_HORIZONTAL:
	    y_start = this.y;	/* = prev.y */
	    alp->start = prev;
	    alp->end = this;
	    /* Don't need to set dx or y_fast_max */
	    alp->pseg = prev_lp;	/* may not need this either */
	    break;
	default:		/* can't happen */
	    return_error(gs_error_unregistered);
    }
    /* Insert the new line in the Y ordering */
    {
	active_line *yp = ll->y_line;
	active_line *nyp;

	if (yp == 0) {
	    alp->next = alp->prev = 0;
	    ll->y_list = alp;
	} else if (y_start >= yp->start.y) {	/* Insert the new line after y_line */
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
	} else {		/* Insert the new line before y_line */
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
    }
    ll->y_line = alp;
    vd_bar(alp->start.x, alp->start.y, alp->end.x, alp->end.y, 1, RGB(0, 255, 0));
    print_al("add ", alp);
    return 0;
}

/* ---------------- Filling loop utilities ---------------- */

/* Insert a newly active line in the X ordering. */
private void
insert_x_new(active_line * alp, line_list *ll)
{
    active_line *next;
    active_line *prev = &ll->x_head;

    vd_bar(alp->start.x, alp->start.y, alp->end.x, alp->end.y, 1, RGB(128, 128, 0));
    alp->x_current = alp->start.x;
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
private inline void
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
     * of the Y-interval, which fill_loop_by_trapezoids currently proceeds.
     * Parts of horizontal lines, which do not contact a filled trapezoid,
     * are parts of the spot bondairy. They to be marked in
     * the 'sect' array.  - see process_h_lists.
     */
}

/*
 * Handle a line segment that just ended.  Return true iff this was
 * the end of a line sequence.
 */
private bool
end_x_line(active_line *alp, bool update)
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
	 (			/* Upward line, go forward along path. */
	  pseg->type == s_line_close ?	/* end of subpath */
	  ((const line_close_segment *)pseg)->sub->next :
	  pseg->next) :
	 (			/* Downward line, go backward along path. */
	  pseg->type == s_start ?	/* start of subpath */
	  ((const subpath *)pseg)->last->prev :
	  pseg->prev)
	);
    gs_fixed_point npt;

    npt.y = next->pt.y;
    if (!update)
	return npt.y <= pseg->pt.y;
    if_debug5('F', "[F]ended 0x%lx: pseg=0x%lx y=%f next=0x%lx npt.y=%f\n",
	      (ulong) alp, (ulong) pseg, fixed2float(pseg->pt.y),
	      (ulong) next, fixed2float(npt.y));
    if (npt.y <= pseg->pt.y) {	/* End of a line sequence */
	active_line *nlp = alp->next;

	alp->prev->next = nlp;
	if (nlp)
	    nlp->prev = alp->prev;
	if_debug1('F', "[F]drop 0x%lx\n", (ulong) alp);
	return true;
    }
    alp->pseg = next;
    npt.x = next->pt.x;
    SET_AL_POINTS(alp, alp->end, npt);
    vd_bar(alp->start.x, alp->start.y, alp->end.x, alp->end.y, 1, RGB(128, 0, 128));
    print_al("repl", alp);
    return false;
}

private inline int 
add_margin(line_list * ll, active_line * flp, active_line * alp, fixed y0, fixed y1)
{   vd_bar(alp->start.x, alp->start.y, alp->end.x, alp->end.y, 1, RGB(255, 255, 255));
    vd_bar(flp->start.x, flp->start.y, flp->end.x, flp->end.y, 1, RGB(255, 255, 255));
    return continue_margin_common(ll, &ll->margin_set0, flp, alp, y0, y1);
}

private inline int 
continue_margin(line_list * ll, active_line * flp, active_line * alp, fixed y0, fixed y1)
{   
    return continue_margin_common(ll, &ll->margin_set0, flp, alp, y0, y1);
}

private inline int 
complete_margin(line_list * ll, active_line * flp, active_line * alp, fixed y0, fixed y1)
{   
    return continue_margin_common(ll, &ll->margin_set1, flp, alp, y0, y1);
}

/*
 * Handle the case of a slanted trapezoid with adjustment.
 * To do this exactly right requires filling a central trapezoid
 * (or rectangle) plus two horizontal almost-rectangles.
 */
private int
fill_slant_adjust(fixed xlbot, fixed xbot, fixed y,
		  fixed xltop, fixed xtop, fixed height, fixed adjust_below,
		  fixed adjust_above, const gs_fixed_rect * pbox,
		  const gx_device_color * pdevc, gx_device * dev,
		  gs_logical_operation_t lop)
{
    fixed y1 = y + height;

    dev_proc_fill_trapezoid((*fill_trap)) =
	dev_proc(dev, fill_trapezoid);
    const fixed yb = y - adjust_below;
    const fixed ya = y + adjust_above;
    const fixed y1b = y1 - adjust_below;
    const fixed y1a = y1 + adjust_above;
    const gs_fixed_edge *plbot;
    const gs_fixed_edge *prbot;
    const gs_fixed_edge *pltop;
    const gs_fixed_edge *prtop;
    gs_fixed_edge vert_left, slant_left, vert_right, slant_right;
    int code;

    INCR(slant);
    vd_quad(xlbot, y, xbot, y, xtop, y + height, xltop, y + height, 1, VD_TRAP_COLOR);

    /* Set up all the edges, even though we may not need them all. */

    if (xlbot < xltop) {	/* && xbot < xtop */
	vert_left.start.x = vert_left.end.x = xlbot;
	vert_left.start.y = yb, vert_left.end.y = ya;
	vert_right.start.x = vert_right.end.x = xtop;
	vert_right.start.y = y1b, vert_right.end.y = y1a;
	slant_left.start.y = ya, slant_left.end.y = y1a;
	slant_right.start.y = yb, slant_right.end.y = y1b;
	plbot = &vert_left, prbot = &slant_right,
	    pltop = &slant_left, prtop = &vert_right;
    } else {
	vert_left.start.x = vert_left.end.x = xltop;
	vert_left.start.y = y1b, vert_left.end.y = y1a;
	vert_right.start.x = vert_right.end.x = xbot;
	vert_right.start.y = yb, vert_right.end.y = ya;
	slant_left.start.y = yb, slant_left.end.y = y1b;
	slant_right.start.y = ya, slant_right.end.y = y1a;
	plbot = &slant_left, prbot = &vert_right,
	    pltop = &vert_left, prtop = &slant_right;
    }
    slant_left.start.x = xlbot, slant_left.end.x = xltop;
    slant_right.start.x = xbot, slant_right.end.x = xtop;

    if (ya >= y1b) {
	/*
	 * The upper and lower adjustment bands overlap.
	 * Since the entire entity is less than 2 pixels high
	 * in this case, we could handle it very efficiently
	 * with no more than 2 rectangle fills, but for right now
	 * we don't attempt to do this.
	 */
	int iyb = fixed2int_var_pixround(yb);
	int iya = fixed2int_var_pixround(ya);
	int iy1b = fixed2int_var_pixround(y1b);
	int iy1a = fixed2int_var_pixround(y1a);

	INCR(slant_shallow);
	if (iy1b > iyb) {
	    code = (*fill_trap) (dev, plbot, prbot,
				 yb, y1b, false, pdevc, lop);
	    if (code < 0)
		return code;
	}
	if (iya > iy1b) {
	    int ix = fixed2int_var_pixround(vert_left.start.x);
	    int iw = fixed2int_var_pixround(vert_right.start.x) - ix;

	    code = LOOP_FILL_RECTANGLE(ix, iy1b, iw, iya - iy1b);
	    if (code < 0)
		return code;
	}
	if (iy1a > iya)
	    code = (*fill_trap) (dev, pltop, prtop,
				 ya, y1a, false, pdevc, lop);
	else
	    code = 0;
    } else {
	/*
	 * Clip the trapezoid if possible.  This can save a lot
	 * of work when filling paths that cross band boundaries.
	 */
	fixed yac;

	if (pbox->p.y < ya) {
	    code = (*fill_trap) (dev, plbot, prbot,
				 yb, ya, false, pdevc, lop);
	    if (code < 0)
		return code;
	    yac = ya;
	} else
	    yac = pbox->p.y;
	if (pbox->q.y > y1b) {
	    code = (*fill_trap) (dev, &slant_left, &slant_right,
				 yac, y1b, false, pdevc, lop);
	    if (code < 0)
		return code;
	    code = (*fill_trap) (dev, pltop, prtop,
				 y1b, y1a, false, pdevc, lop);
	} else
	    code = (*fill_trap) (dev, &slant_left, &slant_right,
				 yac, pbox->q.y, false, pdevc, lop);
    }
    return code;
}

/* Re-sort the x list by moving alp backward to its proper spot. */
private void
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
    prev->next = alp;
}

/* Move active lines by Y. */
private void
move_al_by_y(line_list *ll, fixed y1)
{
    fixed x;
    active_line *alp, *nlp;

    for (x = min_fixed, alp = ll->x_list; alp != 0; alp = nlp) {
	fixed xtop = alp->x_current = alp->x_next;

	nlp = alp->next;
	if (alp->end.y != y1 || !end_x_line(alp, true)) {
	    if (xtop <= x)
		resort_x_line(alp);
	    else
		x = xtop;
	}
    }
}

private int
loop_fill_trap(gx_device * dev, fixed fx0, fixed fw0, fixed fy0,
	       fixed fx1, fixed fw1, fixed fh, const gs_fixed_rect * pbox,
	       const gx_device_color * pdevc, gs_logical_operation_t lop,
	       int flags)
{
    fixed fy1 = fy0 + fh;
    fixed ybot = max(fy0, pbox->p.y);
    fixed ytop = min(fy1, pbox->q.y);
    gs_fixed_edge left, right;
    dev_proc_fill_trapezoid((*fill_trap)) = dev_proc(dev, fill_trapezoid);

    if (ybot >= ytop)
	return 0;
    vd_quad(fx0, ybot, fx0 + fw0, ybot, fx1 + fw1, ytop, fx1, ytop, 1, VD_TRAP_COLOR);
    left.start.y = right.start.y = fy0;
    left.end.y = right.end.y = fy1;
    right.start.x = (left.start.x = fx0) + fw0;
    right.end.x = (left.end.x = fx1) + fw1;
    if (flags & ftf_pseudo_rasterization)
	return gx_fill_trapezoid_narrow
	    (dev, &left, &right, ybot, ytop, flags, pdevc, lop);
    return (*fill_trap)
	(dev, &left, &right, ybot, ytop, false, pdevc, lop);
}

private int
fill_trap_slanted(gx_device * dev, const gs_fixed_rect * pbox,
	    const gx_device_color * pdevc, gs_logical_operation_t lop,
	    bool fill_direct, 
	    fixed xlbot, fixed xbot, fixed xltop, fixed xtop, fixed y, fixed y1,
	    fixed adjust_below, fixed adjust_above)
{
    /*
     * We want to get the effect of filling an area whose
     * outline is formed by dragging a square of side adj2
     * along the border of the trapezoid.  This is *not*
     * equivalent to simply expanding the corners by
     * adjust: There are 3 cases needing different
     * algorithms, plus rectangles as a fast special case.
     */
    fixed wbot = xbot - xlbot;
    fixed wtop = xtop - xltop;
    fixed height = y1 - y;
    dev_proc_fill_rectangle((*fill_rect)) = dev_proc(dev, fill_rectangle);
    int xli = fixed2int_var_pixround(xltop);
    int code = 0;
    /*
     * Define a faster test for
     *	fixed2int_pixround(y - below) != fixed2int_pixround(y + above)
     * where we know
     *	0 <= below <= _fixed_pixround_v,
     *	0 <= above <= min(fixed_half, fixed_1 - below).
     * Subtracting out the integer parts, this is equivalent to
     *	fixed2int_pixround(fixed_fraction(y) - below) !=
     *	  fixed2int_pixround(fixed_fraction(y) + above)
     * or to
     *	fixed2int(fixed_fraction(y) + _fixed_pixround_v - below) !=
     *	  fixed2int(fixed_fraction(y) + _fixed_pixround_v + above)
     * Letting A = _fixed_pixround_v - below and B = _fixed_pixround_v + above,
     * we can rewrite this as
     *	fixed2int(fixed_fraction(y) + A) != fixed2int(fixed_fraction(y) + B)
     * Because of the range constraints given above, this is true precisely when
     *	fixed_fraction(y) + A < fixed_1 && fixed_fraction(y) + B >= fixed_1
     * or equivalently
     *	fixed_fraction(y + B) < B - A.
     * i.e.
     *	fixed_fraction(y + _fixed_pixround_v + above) < below + above
     */
    fixed y_span_delta = _fixed_pixround_v + adjust_above;
    fixed y_span_limit = adjust_below + adjust_above;

#define ADJUSTED_Y_SPANS_PIXEL(y)\
  (fixed_fraction((y) + y_span_delta) < y_span_limit)

    if (xltop <= xlbot) {
	if (xtop >= xbot) {	/* Top wider than bottom. */
	    code = loop_fill_trap(dev, xlbot, wbot, y - adjust_below,
			xltop, wtop, height, pbox, pdevc, lop, 0);
	    if (ADJUSTED_Y_SPANS_PIXEL(y1)) {
		if (code < 0)
		    return code;
		INCR(afill);
		code = LOOP_FILL_RECTANGLE_DIRECT(
		 xli, fixed2int_pixround(y1 - adjust_below),
		     fixed2int_var_pixround(xtop) - xli, 1);
		vd_rect(xltop, y1 - adjust_below, xtop, y1, 1, VD_TRAP_COLOR);
	    }
	} else {	/* Slanted trapezoid. */
	    code = fill_slant_adjust(xlbot, xbot, y,
			  xltop, xtop, height, adjust_below,
				     adjust_above, pbox,
				     pdevc, dev, lop);
	}
    } else {
	if (xtop <= xbot) {	/* Bottom wider than top. */
	    if (ADJUSTED_Y_SPANS_PIXEL(y)) {
		INCR(afill);
		xli = fixed2int_var_pixround(xlbot);
		code = LOOP_FILL_RECTANGLE_DIRECT(
		  xli, fixed2int_pixround(y - adjust_below),
		     fixed2int_var_pixround(xbot) - xli, 1);
		vd_rect(xltop, y - adjust_below, xbot, y, 1, VD_TRAP_COLOR);
		if (code < 0)
		    return code;
	    }
	    code = loop_fill_trap(dev, xlbot, wbot, y + adjust_above,
			xltop, wtop, height, pbox, pdevc, lop, 0);
	} else {	/* Slanted trapezoid. */
	    code = fill_slant_adjust(xlbot, xbot, y,
			  xltop, xtop, height, adjust_below,
				     adjust_above, pbox,
				     pdevc, dev, lop);
	}
    }
    return code;
}

private int
fill_trap_or_rect(gx_device * dev, const gs_fixed_rect * pbox,
	    const gx_device_color * pdevc, gs_logical_operation_t lop,
	    bool fill_direct, 
	    fixed xlbot, fixed xbot, fixed xltop, fixed xtop, fixed y, fixed y1,
	    fixed adjust_below, fixed adjust_above,
	    active_line *flp, active_line *alp, 
	    line_list *ll, const bool pseudo_rasterization)
{
    /* xlbot, xbot, xltop, xtop, y, y1 must be consistent with flp, alp,
     * except for adjusted rectangles.
     * Rather pseudo_rasterization == ll->pseudo_rasterization
     * we pass it as a separate argument to allow inline optimization.
     */
    int code;

    if (xltop == xlbot && xtop == xbot) {
	int yi = fixed2int_pixround(y - adjust_below);
	int wi = fixed2int_pixround(y1 + adjust_above) - yi;
	int xli = fixed2int_var_pixround(xltop);
	int xi = fixed2int_var_pixround(xtop);
	dev_proc_fill_rectangle((*fill_rect)) = dev_proc(dev, fill_rectangle);

	if (pseudo_rasterization && xli == xi) {
	    /*
	     * The scan is empty but we should paint something 
	     * against a dropout. Choose one of two pixels which 
	     * is closer to the "axis".
	     */
	    fixed xx = int2fixed(xli);

	    if (xx - xltop < xtop - xx)
		++xi;
	    else
		--xli;
	}
	code = LOOP_FILL_RECTANGLE_DIRECT(xli, yi, xi - xli, wi);
	vd_rect(xltop, y, xtop, y1, 1, VD_TRAP_COLOR);
    } else {
	int flags = 0;

	if (pseudo_rasterization) {
	    flags |= ftf_pseudo_rasterization;
	    if (flp->start.x == alp->start.x && flp->start.y == y)
		flags |= ftf_peak0;
	    if (flp->end.x == alp->end.x && flp->end.y == y1)
		flags |= ftf_peak0;
	}
	code = loop_fill_trap(dev, xlbot, xbot - xlbot, y, xltop, 
			xtop - xltop, y1 - y, pbox, pdevc, lop, flags);
    }
    return code;
}

#define COVERING_PIXEL_CENTERS(y, y1, adjust_below, adjust_above)\
    (fixed_pixround(y - adjust_below) < fixed_pixround(y1 + adjust_above))

/* Find intersections of active lines within the band. 
   Intersect and reorder them, and correct the bund top. */
private void
intersect_al(line_list *ll, fixed y, fixed *y_top, int draw)
{
    fixed x = min_fixed, y1 = *y_top;
    active_line *alp, *stopx, *endp;
    const bool pseudo_rasterization = ll->pseudo_rasterization;

    /*
     * Loop invariants:
     *	alp = endp->next;
     *	for all lines lp from stopx up to alp,
     *	  lp->x_next = AL_X_AT_Y(lp, y1).
     */
    for (alp = stopx = ll->x_list;
	 INCR_EXPR(find_y), alp != 0;
	 endp = alp, alp = alp->next
	) {
	fixed nx = AL_X_AT_Y(alp, y1);
	fixed dx_old, dx_den;

	/* Check for intersecting lines. */
	if (nx >= x)
	    x = nx;
	else if ((pseudo_rasterization || draw >= 0) &&	/* don't bother if no pixels with no pseudo_rasterization */
		 (dx_old = alp->x_current - endp->x_current) >= 0 &&
		 (dx_den = dx_old + endp->x_next - nx) > dx_old
	    ) {		/* Make a good guess at the intersection */
	    /* Y value using only local information. */
	    fixed dy = y1 - y, y_new;

	    if_debug3('F', "[F]cross: dy=%g, dx_old=%g, dx_new=%g\n",
		      fixed2float(dy), fixed2float(dx_old),
		      fixed2float(dx_den - dx_old));
	    /* Do the computation in single precision */
	    /* if the values are small enough. */
	    y_new =
		((dy | dx_old) < 1L << (size_of(fixed) * 4 - 1) ?
		 dy * dx_old / dx_den :
		 (INCR_EXPR(mq_cross), fixed_mult_quo(dy, dx_old, dx_den)))
		+ y;
	    /* The crossing value doesn't have to be */
	    /* very accurate, but it does have to be */
	    /* greater than y and less than y1. */
	    if_debug3('F', "[F]cross y=%g, y_new=%g, y1=%g\n",
		      fixed2float(y), fixed2float(y_new),
		      fixed2float(y1));
	    stopx = alp;
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
	    if (y_new < y1) {
		y1 = y_new;
		nx = AL_X_AT_Y(alp, y1);
		draw = 0;
	    }
	    if (nx > x)
		x = nx;
	}
	alp->x_next = nx;
    }
    /* Recompute next_x for lines before the intersection. */
    for (alp = ll->x_list; alp != stopx; alp = alp->next)
	alp->x_next = AL_X_AT_Y(alp, y1);
#ifdef DEBUG
    if (gs_debug_c('F')) {
	dlprintf1("[F]after loop: y1=%f\n", fixed2float(y1));
	print_line_list(ll->x_list);
    }
#endif
    *y_top = y1;
}

/* ---------------- Trapezoid filling loop ---------------- */

/* Main filling loop.  Takes lines off of y_list and adds them to */
/* x_list as needed.  band_mask limits the size of each band, */
/* by requiring that ((y1 - 1) & band_mask) == (y0 & band_mask). */
private int
fill_loop_by_trapezoids(line_list *ll, gx_device * dev,
	       const gx_fill_params * params, const gx_device_color * pdevc,
		     gs_logical_operation_t lop, const gs_fixed_rect * pbox,
			fixed adjust_left, fixed adjust_right,
		    fixed adjust_below, fixed adjust_above, fixed band_mask)
{
    int rule = params->rule;
    const fixed y_limit = pbox->q.y;
    active_line *yll = ll->y_list;
    fixed y;
    int code;
    bool fill_direct = color_writes_pure(pdevc, lop);
    const bool pseudo_rasterization = ll->pseudo_rasterization;

    dev_proc_fill_rectangle((*fill_rect));

    if (yll == 0)
	return 0;		/* empty list */
    if (fill_direct)
	fill_rect = dev_proc(dev, fill_rectangle);
    else
	fill_rect = 0; /* unused. */
    y = yll->start.y;		/* first Y value */
    ll->fill_direct = fill_direct;
    ll->x_list = 0;
    ll->x_head.x_current = min_fixed;	/* stop backward scan */
    ll->margin_set0.y = fixed_pixround(y) - fixed_half;
    ll->margin_set1.y = fixed_pixround(y) - fixed_1 - fixed_half;
    while (1) {
	fixed y1;
	active_line *alp, *plp = NULL;
	bool covering_pixel_centers;

	INCR(iter);
	/* Move newly active lines from y to x list. */
	while (yll != 0 && yll->start.y == y) {
	    active_line *ynext = yll->next;	/* insert smashes next/prev links */

	    if (yll->direction == DIR_HORIZONTAL) {
		if (!pseudo_rasterization) {
		    /*
		     * This is a hack to make sure that isolated horizontal
		     * lines get stroked.
		     */
		    int yi = fixed2int_pixround(y - adjust_below);
		    int xi, wi;

		    if (yll->start.x <= yll->end.x) {
			xi = fixed2int_pixround(yll->start.x - adjust_left);
			wi = fixed2int_pixround(yll->end.x + adjust_right) - xi;
		    } else {
			xi = fixed2int_pixround(yll->end.x - adjust_left);
			wi = fixed2int_pixround(yll->start.x + adjust_right) - xi;
		    }
		    VD_RECT(xi, yi, wi, 1, VD_TRAP_COLOR);
		    code = LOOP_FILL_RECTANGLE_DIRECT(xi, yi, wi, 1);
		    if (code < 0)
			return code;
		} else if (pseudo_rasterization)
		    insert_h_new(yll, ll);
	    } else
		insert_x_new(yll, ll);
	    yll = ynext;
	}
	/* Check whether we've reached the maximum y. */
	if (y >= y_limit)
	    break;
	if (ll->x_list == 0) {	/* No active lines, skip to next start */
	    if (yll == 0)
		break;		/* no lines left */
	    /* We don't close margin set here because the next set
	     * may fall into same window. */
	    y = yll->start.y;
	    ll->h_list1 = ll->h_list0;
	    ll->h_list0 = 0;
	    continue;
	}
	if (vd_enabled) {
	    vd_circle(0, y, 3, RGB(255, 0, 0));
	    y += 0; /* Just a good place for a debugger breakpoint */
	}
	/* Find the next evaluation point. */
	/* Start by finding the smallest y value */
	/* at which any currently active line ends */
	/* (or the next to-be-active line begins). */
	y1 = (yll != 0 ? yll->start.y : y_limit);
	/* Make sure we don't exceed the maximum band height. */
	{
	    fixed y_band = y | ~band_mask;

	    if (y1 > y_band)
		y1 = y_band + 1;
	}
	for (alp = ll->x_list; alp != 0; alp = alp->next)
	    if (alp->end.y < y1)
		y1 = alp->end.y;
#ifdef DEBUG
	if (gs_debug_c('F')) {
	    dlprintf2("[F]before loop: y=%f y1=%f:\n",
		      fixed2float(y), fixed2float(y1));
	    print_line_list(ll->x_list);
	}
#endif
	/* Now look for line intersections before y1. */
	covering_pixel_centers = COVERING_PIXEL_CENTERS(y, y1, adjust_below, adjust_above);
	intersect_al(ll, y, &y1, (covering_pixel_centers ? 1 : -1)); /* May change y1. */

	/* Prepare dropout prevention. */
	if (pseudo_rasterization) {
	    code = start_margin_set(dev, ll, y1);
	    if (code < 0)
		return code;
	}
	/* Fill a multi-trapezoid band for the active lines. */
	covering_pixel_centers = COVERING_PIXEL_CENTERS(y, y1, adjust_below, adjust_above);
	if (covering_pixel_centers) {
	    fixed xlbot, xltop; /* as of last "outside" line */
	    int inside = 0;
	    active_line *flp;

	    INCR(band);

	    /* Generate trapezoids */

	    for (alp = ll->x_list; alp != 0; alp = alp->next) {
		fixed xbot = alp->x_current;
		fixed xtop = alp->x_current = alp->x_next;
		int code;

		print_al("step", alp);
		INCR(band_step);
		if (!INSIDE_PATH_P(inside, rule)) { 	/* i.e., outside */
		    inside += alp->direction;
		    if (INSIDE_PATH_P(inside, rule))	/* about to go in */
			xlbot = xbot, xltop = xtop, flp = alp;
		    continue;
		}
		/* We're inside a region being filled. */
		inside += alp->direction;
		if (INSIDE_PATH_P(inside, rule))	/* not about to go out */
		    continue;
		/* We just went from inside to outside, so fill the region. */
		INCR(band_fill);
		if ((adjust_left | adjust_right) != 0) {
		    xlbot -= adjust_left;
		    xbot += adjust_right;
		    xltop -= adjust_left;
		    xtop += adjust_right;
		}
		if (!(xltop == xlbot && xtop == xbot) && (adjust_below | adjust_above) != 0) {
		    /* Assuming pseudo_rasterization = false. */
		    code = fill_trap_slanted(dev, pbox, pdevc, lop, fill_direct, 
				xlbot, xbot, xltop, xtop, y, y1, adjust_below, adjust_above);
		} else {
		    code = fill_trap_or_rect(dev, pbox, pdevc, lop, fill_direct, 
				xlbot, xbot, xltop, xtop, y, y1, adjust_below, adjust_above,
				flp, alp, ll, pseudo_rasterization);
		    if (pseudo_rasterization) {
			if (code < 0)
			    return code;
			code = complete_margin(ll, flp, alp, y, y1);
			if (code < 0)
			    return code;
			code = margin_interior(ll, flp, alp, y, y1);
			if (code < 0)
			    return code;
			code = add_margin(ll, flp, alp, y, y1);
			if (code < 0)
			    return code;
			code = process_h_lists(ll, plp, flp, alp);
			plp = alp;
		    }
		}
		if (code < 0)
		    return code;
	    }
	} else {
	    /* No trapezoids generation needed. */
	    if (pseudo_rasterization) {
		/* Process dropouts near trapezoids. */
		active_line *flp;
		int inside = 0;

		for (alp = ll->x_list; alp != 0; alp = alp->next) {
		    alp->x_current = alp->x_next;

		    if (!INSIDE_PATH_P(inside, rule)) {		/* i.e., outside */
			inside += alp->direction;
			if (INSIDE_PATH_P(inside, rule))	/* about to go in */
			    flp = alp;
			continue;
		    }
		    /* We're inside a region being filled. */
		    inside += alp->direction;
		    if (INSIDE_PATH_P(inside, rule))	/* not about to go out */
			continue;
		    code = continue_margin(ll, flp, alp, y, y1);
		    if (code < 0)
			return code;
		    code = process_h_lists(ll, plp, flp, alp);
		    plp = alp;
		    if (code < 0)
			return code;
		}
	    }
	}
	if (plp != 0) {
	    code = process_h_lists(ll, plp, 0, 0);
	    if (code < 0)
		return code;
	}
	move_al_by_y(ll, y1);
	y = y1;
	ll->h_list1 = ll->h_list0;
	ll->h_list0 = 0;
    }
    if (pseudo_rasterization) {
	code = process_h_lists(ll, 0, 0, 0);
	if (code < 0)
	    return code;
	code = close_margins(dev, ll, &ll->margin_set1);
	if (code < 0)
	    return code;
	return close_margins(dev, ll, &ll->margin_set0);
    } 
    return 0;
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
 *	if prev == 0:
 *		next != 0
 *		rmin == rmax == MIN_COORD_VALUE
 *	else if next == 0:
 *		prev != 0
 *		rmin == rmax == MAX_COORD_VALUE
 *	else
 *		rmin < rmax
 *	if prev != 0:
 *		prev->next == this
 *		prev->rmax < rmin
 *	if next != 0:
 *		next->prev = this
 *		next->rmin > rmax
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

private coord_range_t *
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

private void
range_delete(coord_range_list_t *pcrl, coord_range_t *pcr)
{
    if_debug3('Q', "[Qr]delete 0x%lx: [%d,%d)\n", (ulong)pcr, pcr->rmin,
	      pcr->rmax);
    pcr->prev->next = pcr->next;
    pcr->next->prev = pcr->prev;
    pcr->next = pcrl->freed;
    pcrl->freed = pcr;
}

private void
range_list_clear(coord_range_list_t *pcrl)
{
    if_debug0('Q', "[Qr]clearing\n");
    pcrl->first.next = &pcrl->last;
    pcrl->last.prev = &pcrl->first;
    pcrl->current = &pcrl->last;
}

/* ------ "Public" procedures ------ */

/* Initialize a range list.  We require num_local >= 2. */
private void range_list_clear(coord_range_list_t *);
private void
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
private void
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
inline private void
range_list_rescan(coord_range_list_t *pcrl)
{
    pcrl->current = pcrl->first.next;
}

/* Free a range list. */
private void
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
private int
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
	    break;		/* don't merge into the min range */
	pcr->rmin = pcr->prev->rmin;
	range_delete(pcrl, pcr->prev);
    }
    while (rmax >= pcr->next->rmin) {
	/* Merge the range above pcr into this one. */
	if (!pcr->next->next)
	    break;		/* don't merge into the max range */
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

/* ---------------- Scan line filling loop ---------------- */

/* Forward references */
private int merge_ranges(coord_range_list_t *pcrl, line_list *ll,
			 fixed y_min, fixed y_top,
			 fixed adjust_left, fixed adjust_right);
private void set_scan_line_points(active_line *, fixed);

/* Main filling loop. */
private int
fill_loop_by_scan_lines(line_list *ll, gx_device * dev,
			const gx_fill_params * params,
			const gx_device_color * pdevc,
			gs_logical_operation_t lop, const gs_fixed_rect * pbox,
			fixed adjust_left, fixed adjust_right,
			fixed adjust_below, fixed adjust_above,
			fixed band_mask)
{
    int rule = params->rule;
    fixed fixed_flat = float2fixed(params->flatness);
    bool fill_direct = color_writes_pure(pdevc, lop);
    dev_proc_fill_rectangle((*fill_rect));
    active_line *yll = ll->y_list;
    fixed y_limit = pbox->q.y;
    /*
     * The meaning of adjust_below (B) and adjust_above (A) is that the
     * pixels that would normally be painted at coordinate Y get "smeared"
     * to coordinates Y-B through Y+A-epsilon, inclusive.  This is
     * equivalent to saying that the pixels actually painted at coordinate Y
     * are those contributed by scan lines Y-A+epsilon through Y+B,
     * inclusive.  (A = B = 0 is a special case, equivalent to B = 0, A =
     * epsilon.)
     */
    fixed y_frac_min =
	(adjust_above == fixed_0 ? fixed_half :
	 fixed_half + fixed_epsilon - adjust_above);
    fixed y_frac_max =
	fixed_half + adjust_below;
    int y0 = fixed2int(min_fixed);
    fixed y_bot = min_fixed;	/* normally int2fixed(y0) + y_frac_min */
    fixed y_top = min_fixed;	/* normally int2fixed(y0) + y_frac_max */
    coord_range_list_t rlist;
    coord_range_t rlocal[MAX_LOCAL_ACTIVE];
    int code = 0;

    if (yll == 0)		/* empty list */
	return 0;
    range_list_init(&rlist, rlocal, countof(rlocal), ll->memory);
    if (fill_direct)
	fill_rect = dev_proc(dev, fill_rectangle);
    ll->x_list = 0;
    ll->x_head.x_current = min_fixed;	/* stop backward scan */
    while (code >= 0) {
	active_line *alp, *nlp;
	fixed y, x;
	bool new_band;

	INCR(iter);

	/*
	 * Find the next sampling point, either the bottom of a sampling
	 * band or a line start.
	 */

	if (ll->x_list == 0)
	    y = (yll == 0 ? y_limit : yll->start.y);
	else {
	    y = y_bot + fixed_1;
	    if (yll != 0)
		y = min(y, yll->start.y);
	    for (alp = ll->x_list; alp != 0; alp = alp->next)
		if (!end_x_line(alp, false))
		    y = min(y, alp->end.y);
	}

	/* Move newly active lines from y to x list. */

	while (yll != 0 && yll->start.y == y) {
	    active_line *ynext = yll->next;	/* insert smashes next/prev links */

	    if (yll->direction == DIR_HORIZONTAL) {
		/* Ignore for now. */
	    } else {
		insert_x_new(yll, ll);
		set_scan_line_points(yll, fixed_flat);
	    }
	    yll = ynext;
	}

	/* Update active lines to y. */

	x = min_fixed;
	for (alp = ll->x_list; alp != 0; alp = nlp) {
	    fixed nx;

	    nlp = alp->next;
	  e:if (alp->end.y <= y) {
		if (end_x_line(alp, true))
		    continue;
		set_scan_line_points(alp, fixed_flat);
		goto e;
	    }
	    nx = alp->x_current =
		(alp->start.y >= y ? alp->start.x :
		 alp->curve_k < 0 ?
		 AL_X_AT_Y(alp, y) :
		 gx_curve_x_at_y(&alp->cursor, y));
	    if (nx < x) {
		/* Move this line backward in the list. */
		active_line *ilp = alp;

		while (nx < (ilp = ilp->prev)->x_current)
		    DO_NOTHING;
		/* Now ilp->x_current <= nx < ilp->next->x_cur. */
		alp->prev->next = alp->next;
		if (alp->next)
		    alp->next->prev = alp->prev;
		if (ilp->next)
		    ilp->next->prev = alp;
		alp->next = ilp->next;
		ilp->next = alp;
		alp->prev = ilp;
		continue;
	    }
	    x = nx;
	}

	if (y > y_top || y >= y_limit) {
	    /* We're beyond the end of the previous sampling band. */
	    const coord_range_t *pcr;

	    /* Fill the ranges for y0. */

	    for (pcr = rlist.first.next; pcr != &rlist.last;
		 pcr = pcr->next
		 ) {
		int x0 = pcr->rmin, x1 = pcr->rmax;

		if_debug4('Q', "[Qr]draw 0x%lx: [%d,%d),%d\n", (ulong)pcr,
			  x0, x1, y0);
		VD_RECT(x0, y0, x1 - x0, 1, VD_TRAP_COLOR);
		code = LOOP_FILL_RECTANGLE_DIRECT(x0, y0, x1 - x0, 1);
		if_debug3('F', "[F]drawing [%d:%d),%d\n", x0, x1, y0);
		if (code < 0)
		    goto done;
	    }
	    range_list_reset(&rlist);

	    /* Check whether we've reached the maximum y. */

	    if (y >= y_limit)
		break;

	    /* Reset the sampling band. */

	    y0 = fixed2int(y);
	    if (fixed_fraction(y) < y_frac_min)
		--y0;
	    y_bot = int2fixed(y0) + y_frac_min;
	    y_top = int2fixed(y0) + y_frac_max;
	    new_band = true;
	} else
	    new_band = false;

	if (y <= y_top) {
	    /*
	     * We're within the same Y pixel.  Merge regions for segments
	     * starting here (at y), up to y_top or the end of the segment.
	     * If this is the first sampling within the band, run the
	     * fill/eofill algorithm.
	     */
	    fixed y_min;

	    if (new_band) {
		int inside = 0;

		INCR(band);
		for (alp = ll->x_list; alp != 0; alp = alp->next) {
		    int x0 = fixed2int_pixround(alp->x_current - adjust_left);

		    for (;;) {
			/* We're inside a filled region. */
			print_al("step", alp);
			INCR(band_step);
			inside += alp->direction;
			if (!INSIDE_PATH_P(inside, rule))
			    break;
			/*
			 * Since we're dealing with closed paths, the test
			 * for alp == 0 shouldn't be needed, but we may have
			 * omitted lines that are to the right of the
			 * clipping region.
			 */
			if ((alp = alp->next) == 0)
			    goto out;
		    }
		    /* We just went from inside to outside, so fill the region. */
		    code = range_list_add(&rlist, x0,
					  fixed2int_rounded(alp->x_current +
							    adjust_right));
		    if (code < 0)
			goto done;
		}
	    out:
		y_min = min_fixed;
	    } else
		y_min = y;
	    code = merge_ranges(&rlist, ll, y_min, y_top, adjust_left,
				adjust_right);
	} /* else y < y_bot + 1, do nothing */
    }
 done:
    range_list_free(&rlist);
    return code;
}

/*
 * Merge regions for active segments starting at a given Y, or all active
 * segments, up to the end of the sampling band or the end of the segment,
 * into the range list.
 */
private int
merge_ranges(coord_range_list_t *pcrl, line_list *ll, fixed y_min, fixed y_top,
	     fixed adjust_left, fixed adjust_right)
{
    active_line *alp, *nlp;
    int code = 0;

    range_list_rescan(pcrl);
    for (alp = ll->x_list; alp != 0 && code >= 0; alp = nlp) {
	fixed x0 = alp->x_current, x1, xt;

	nlp = alp->next;
	if (alp->start.y < y_min)
	    continue;
	if (alp->end.y < y_top)
	    x1 = alp->end.x;
	else if (alp->curve_k < 0)
	    x1 = AL_X_AT_Y(alp, y_top);
	else
	    x1 = gx_curve_x_at_y(&alp->cursor, y_top);
	if (x0 > x1)
	    xt = x0, x0 = x1, x1 = xt;
	code = range_list_add(pcrl,
			      fixed2int_pixround(x0 - adjust_left),
			      fixed2int_rounded(x1 + adjust_right));
    }
    return code;
}

/*
 * Set curve_k and, if necessary, the curve rendering parameters for
 * the current segment of an active line.
 */
private void
set_scan_line_points(active_line * alp, fixed fixed_flat)
{
    const segment *pseg = alp->pseg;
    const gs_fixed_point *pp0;

    if (alp->direction < 0) {
	pseg =
	    (pseg->type == s_line_close ?
	     ((const line_close_segment *)pseg)->sub->next :
	     pseg->next);
	if (pseg->type != s_curve) {
	    alp->curve_k = -1;
	    return;
	}
	pp0 = &alp->end;
    } else {
	if (pseg->type != s_curve) {
	    alp->curve_k = -1;
	    return;
	}
	pp0 = &alp->start;
    }
    {
	const curve_segment *const pcseg = (const curve_segment *)pseg;

	alp->curve_k =
	    gx_curve_log2_samples(pp0->x, pp0->y, pcseg, fixed_flat);
	gx_curve_cursor_init(&alp->cursor, pp0->x, pp0->y, pcseg,
			     alp->curve_k);
    }
}
