/* Copyright (C) 1997, 1998 Aladdin Enterprises.  All rights reserved.
  
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
/* Path flattening algorithms */
#include "gx.h"
#include "gxarith.h"
#include "gxfixed.h"
#include "gzpath.h"
#include "memory_.h"
#include "vdtrace.h"
#include <assert.h>

/* Define whether to merge nearly collinear line segments when flattening */
/* curves.  This is very good for performance, but we feel a little */
/* uneasy about its effects on character appearance. */
#define MERGE_COLLINEAR_SEGMENTS 1

/* ---------------- Curve flattening ---------------- */

#define x1 pc->p1.x
#define y1 pc->p1.y
#define x2 pc->p2.x
#define y2 pc->p2.y
#define x3 pc->pt.x
#define y3 pc->pt.y

/*
 * To calculate how many points to sample along a path in order to
 * approximate it to the desired degree of flatness, we define
 *      dist((x,y)) = abs(x) + abs(y);
 * then the number of points we need is
 *      N = 1 + sqrt(3/4 * D / flatness),
 * where
 *      D = max(dist(p0 - 2*p1 + p2), dist(p1 - 2*p2 + p3)).
 * Since we are going to use a power of 2 for the number of intervals,
 * we can avoid the square root by letting
 *      N = 1 + 2^(ceiling(log2(3/4 * D / flatness) / 2)).
 * (Reference: DEC Paris Research Laboratory report #1, May 1989.)
 *
 * We treat two cases specially.  First, if the curve is very
 * short, we halve the flatness, to avoid turning short shallow curves
 * into short straight lines.  Second, if the curve forms part of a
 * character (indicated by flatness = 0), we let
 *      N = 1 + 2 * max(abs(x3-x0), abs(y3-y0)).
 * This is probably too conservative, but it produces good results.
 */
int
gx_curve_log2_samples(fixed x0, fixed y0, const curve_segment * pc,
		      fixed fixed_flat)
{
    fixed
	x03 = x3 - x0,
	y03 = y3 - y0;
    int k;

    if (x03 < 0)
	x03 = -x03;
    if (y03 < 0)
	y03 = -y03;
    if ((x03 | y03) < int2fixed(16))
	fixed_flat >>= 1;
    if (fixed_flat == 0) {	/* Use the conservative method. */
	fixed m = max(x03, y03);

	for (k = 1; m > fixed_1;)
	    k++, m >>= 1;
    } else {
	const fixed
	      x12 = x1 - x2, y12 = y1 - y2, dx0 = x0 - x1 - x12, dy0 = y0 - y1 - y12,
	      dx1 = x12 - x2 + x3, dy1 = y12 - y2 + y3, adx0 = any_abs(dx0),
	      ady0 = any_abs(dy0), adx1 = any_abs(dx1), ady1 = any_abs(dy1);

	fixed
	    d = max(adx0, adx1) + max(ady0, ady1);
	/*
	 * The following statement is split up to work around a
	 * bug in the gcc 2.7.2 optimizer on H-P RISC systems.
	 */
	uint qtmp = d - (d >> 2) /* 3/4 * D */ +fixed_flat - 1;
	uint q = qtmp / fixed_flat;

	if_debug6('2', "[2]d01=%g,%g d12=%g,%g d23=%g,%g\n",
		  fixed2float(x1 - x0), fixed2float(y1 - y0),
		  fixed2float(-x12), fixed2float(-y12),
		  fixed2float(x3 - x2), fixed2float(y3 - y2));
	if_debug2('2', "     D=%f, flat=%f,",
		  fixed2float(d), fixed2float(fixed_flat));
	/* Now we want to set k = ceiling(log2(q) / 2). */
	for (k = 0; q > 1;)
	    k++, q = (q + 3) >> 2;
	if_debug1('2', " k=%d\n", k);
    }
    return k;
}

/*
 * Split a curve segment into two pieces at the (parametric) midpoint.
 * Algorithm is from "The Beta2-split: A special case of the Beta-spline
 * Curve and Surface Representation," B. A. Barsky and A. D. DeRose, IEEE,
 * 1985, courtesy of Crispin Goswell.
 */
private void
split_curve_midpoint(fixed x0, fixed y0, const curve_segment * pc,
		     curve_segment * pc1, curve_segment * pc2)
{				/*
				 * We have to define midpoint carefully to avoid overflow.
				 * (If it overflows, something really pathological is going
				 * on, but we could get infinite recursion that way....)
				 */
#define midpoint(a,b)\
  (arith_rshift_1(a) + arith_rshift_1(b) + ((a) & (b) & 1) + 1)
    fixed x12 = midpoint(x1, x2);
    fixed y12 = midpoint(y1, y2);

    /*
     * pc1 or pc2 may be the same as pc, so we must be a little careful
     * about the order in which we store the results.
     */
    pc1->p1.x = midpoint(x0, x1);
    pc1->p1.y = midpoint(y0, y1);
    pc2->p2.x = midpoint(x2, x3);
    pc2->p2.y = midpoint(y2, y3);
    pc1->p2.x = midpoint(pc1->p1.x, x12);
    pc1->p2.y = midpoint(pc1->p1.y, y12);
    pc2->p1.x = midpoint(x12, pc2->p2.x);
    pc2->p1.y = midpoint(y12, pc2->p2.y);
    if (pc2 != pc)
	pc2->pt.x = pc->pt.x,
	    pc2->pt.y = pc->pt.y;
    pc1->pt.x = midpoint(pc1->p2.x, pc2->p1.x);
    pc1->pt.y = midpoint(pc1->p2.y, pc2->p1.y);
#undef midpoint
}

private inline void
print_points(const gs_fixed_point *points, int count)
{
#ifdef DEBUG    
    int i;

    if (!gs_debug_c('3'))
	return;
    for (i = 0; i < count; i++)
	if_debug2('3', "[3]out x=%d y=%d\n", points[i].x, points[i].y);
#endif
}


#undef x1
#undef y1
#undef x2
#undef y2
#undef x3
#undef y3

bool
curve_coeffs_ranged(fixed x0, fixed x1, fixed x2, fixed x3, 
		    fixed y0, fixed y1, fixed y2, fixed y3, 
		    fixed *ax, fixed *bx, fixed *cx, 
		    fixed *ay, fixed *by, fixed *cy, 
		    int k)
{
    fixed x01, x12, y01, y12;

    curve_points_to_coefficients(x0, x1, x2, x3, 
				 *ax, *bx, *cx, x01, x12);
    curve_points_to_coefficients(y0, y1, y2, y3, 
				 *ay, *by, *cy, y01, y12);
#   define max_fast (max_fixed / 6)
#   define min_fast (-max_fast)
#   define in_range(v) (v < max_fast && v > min_fast)
    if (k > k_sample_max ||
	!in_range(*ax) || !in_range(*ay) ||
	!in_range(*bx) || !in_range(*by) ||
	!in_range(*cx) || !in_range(*cy)
	)
	return false;
#undef max_fast
#undef min_fast
#undef in_range
    return true;
}

#if FLATTENED_ITERATOR_SELFTEST
#if FLATTENED_ITERATOR_BACKSCAN
private void gx_flattened_iterator__test_filtered1(gx_flattened_iterator *this);
#endif
private void gx_flattened_iterator__test_filtered2(gx_flattened_iterator *this);
#endif

/*  Initialize the iterator. 
    Momotonic curves with non-zero length are only allowed.
 */
bool
gx_flattened_iterator__init(gx_flattened_iterator *this, 
	    fixed x0, fixed y0, const curve_segment *pc, int k, bool reverse)
{
    /* Note : Immediately after the ininialization it keeps an invalid (zero length) segment. */
    fixed x1, y1, x2, y2;
    const int k2 = k << 1, k3 = k2 + k;
    fixed bx2, by2, ax6, ay6;

    if (!reverse) {
	x1 = pc->p1.x;
	y1 = pc->p1.y;
	x2 = pc->p2.x;
	y2 = pc->p2.y;
	this->x0 = this->lx0 = this->lx1 = x0;
	this->y0 = this->ly0 = this->ly1 = y0;
	this->x3 = pc->pt.x;
	this->y3 = pc->pt.y;
    } else {
	x1 = pc->p2.x;
	y1 = pc->p2.y;
	x2 = pc->p1.x;
	y2 = pc->p1.y;
	this->x0 = this->lx0 = this->lx1 = pc->pt.x;
	this->y0 = this->ly0 = this->ly1 = pc->pt.y;
	this->x3 = x0;
	this->y3 = y0;
    }
    if (!curve_coeffs_ranged(this->x0, x1, x2, this->x3,
			     this->y0, y1, y2, this->y3, 
			     &this->ax, &this->bx, &this->cx, 
			     &this->ay, &this->by, &this->cy, k))
	return false;
#   if CURVED_TRAPEZOID_FILL
	this->curve = true;
#   endif
    this->ahead = false;
    this->prev_filtered1_i = 0; /* stub */
    this->prev_filtered2_i = 0; /* stub */
    vd_curve(this->x0, this->y0, x1, y1, x2, y2, this->x3, this->y3, 0, RGB(255, 255, 255));
    this->k = k;
#   ifdef DEBUG
	if (gs_debug_c('3')) {
	    dlprintf4("[3]x0=%f y0=%f x1=%f y1=%f\n",
		      fixed2float(this->x0), fixed2float(this->y0),
		      fixed2float(x1), fixed2float(y1));
	    dlprintf5("   x2=%f y2=%f x3=%f y3=%f  k=%d\n",
		      fixed2float(x2), fixed2float(y2),
		      fixed2float(this->x3), fixed2float(this->y3), this->k);
	}
#   endif
#   if !FLATTENED_ITERATOR_BACKSCAN
        memset(this->skip_points, 0, sizeof(this->skip_points));
#   endif
    if (k == -1) {
	/* A special hook for gx_subdivide_curve_rec.
	   Only checked the range. 
	   Returning with no initialization. */
	return true;
    }
    this->rmask = (1 << k3) - 1;
    this->i = (1 << k);
    this->rx = this->ry = 0;
    if_debug6('3', "[3]ax=%f bx=%f cx=%f\n   ay=%f by=%f cy=%f\n",
	      fixed2float(this->ax), fixed2float(this->bx), fixed2float(this->cx),
	      fixed2float(this->ay), fixed2float(this->by), fixed2float(this->cy));
    bx2 = this->bx << 1;
    by2 = this->by << 1;
    ax6 = ((this->ax << 1) + this->ax) << 1;
    ay6 = ((this->ay << 1) + this->ay) << 1;
    this->idx = arith_rshift(this->cx, this->k);
    this->idy = arith_rshift(this->cy, this->k);
    this->rdx = ((uint)this->cx << k2) & this->rmask;
    this->rdy = ((uint)this->cy << k2) & this->rmask;
    /* bx/y terms */
    this->id2x = arith_rshift(bx2, k2);
    this->id2y = arith_rshift(by2, k2);
    this->rd2x = ((uint)bx2 << this->k) & this->rmask;
    this->rd2y = ((uint)by2 << this->k) & this->rmask;
#   define adjust_rem(r, q, rmask) if ( r > rmask ) q ++, r &= rmask
    /* We can compute all the remainders as ints, */
    /* because we know they don't exceed M. */
    /* cx/y terms */
    this->idx += arith_rshift_1(this->id2x);
    this->idy += arith_rshift_1(this->id2y);
    this->rdx += ((uint)this->bx << this->k) & this->rmask,
    this->rdy += ((uint)this->by << this->k) & this->rmask;
    adjust_rem(this->rdx, this->idx, this->rmask);
    adjust_rem(this->rdy, this->idy, this->rmask);
    /* ax/y terms */
    this->idx += arith_rshift(this->ax, k3);
    this->idy += arith_rshift(this->ay, k3);
    this->rdx += (uint)this->ax & this->rmask;
    this->rdy += (uint)this->ay & this->rmask;
    adjust_rem(this->rdx, this->idx, this->rmask);
    adjust_rem(this->rdy, this->idy, this->rmask);
    this->id2x += this->id3x = arith_rshift(ax6, k3);
    this->id2y += this->id3y = arith_rshift(ay6, k3);
    this->rd2x += this->rd3x = (uint)ax6 & this->rmask,
    this->rd2y += this->rd3y = (uint)ay6 & this->rmask;
    adjust_rem(this->rd2x, this->id2x, this->rmask);
    adjust_rem(this->rd2y, this->id2y, this->rmask);
#   undef adjust_rem
#   if CURVED_TRAPEZOID_FILL
#	if FLATTENED_ITERATOR_SELFTEST
#	    if FLATTENED_ITERATOR_BACKSCAN
		gx_flattened_iterator__test_filtered1(this);
#	    endif
	    gx_flattened_iterator__test_filtered2(this);
#	endif
#   endif
    return true;
}

/*  Initialize the iterator with a line. */
bool
gx_flattened_iterator__init_line(gx_flattened_iterator *this, 
	    fixed x0, fixed y0, fixed x1, fixed y1)
{
    this->x0 = this->lx0 = this->lx1 = x0;
    this->y0 = this->ly0 = this->ly1 = y0;
    this->x3 = x1;
    this->y3 = y1;
    this->k = 0;
    this->i = 1;
#   if CURVED_TRAPEZOID_FILL
	this->curve = false;
#   endif
    this->ahead = false;
    return true;
}

#ifdef DEBUG
private inline void
gx_flattened_iterator__print_state(gx_flattened_iterator *this)
{
    if (!gs_debug_c('3'))
	return;
    dlprintf4("[3]dx=%f+%d, dy=%f+%d\n",
	      fixed2float(this->idx), this->rdx,
	      fixed2float(this->idy), this->rdy);
    dlprintf4("   d2x=%f+%d, d2y=%f+%d\n",
	      fixed2float(this->id2x), this->rd2x,
	      fixed2float(this->id2y), this->rd2y);
    dlprintf4("   d3x=%f+%d, d3y=%f+%d\n",
	      fixed2float(this->id3x), this->rd3x,
	      fixed2float(this->id3y), this->rd3y);
}
#endif


#define coord_near(v, ptv) (!( ((v) ^ (ptv)) & float2fixed(-0.5) ))

/* Move to the next segment and store it to this->lx0, this->ly0, this->lx1, this->ly1 .
 * Return true iff there exist more segments.
 * Note : It can generate small or collinear segments. 
 */
bool
gx_flattened_iterator__next(gx_flattened_iterator *this)
{
    /*
     * We can compute successive values by finite differences,
     * using the formulas:
     x(t) =
     a*t^3 + b*t^2 + c*t + d =>
     dx(t) = x(t+e)-x(t) =
     a*(3*t^2*e + 3*t*e^2 + e^3) + b*(2*t*e + e^2) + c*e =
     (3*a*e)*t^2 + (3*a*e^2 + 2*b*e)*t + (a*e^3 + b*e^2 + c*e) =>
     d2x(t) = dx(t+e)-dx(t) =
     (3*a*e)*(2*t*e + e^2) + (3*a*e^2 + 2*b*e)*e =
     (6*a*e^2)*t + (6*a*e^3 + 2*b*e^2) =>
     d3x(t) = d2x(t+e)-d2x(t) =
     6*a*e^3;
     x(0) = d, dx(0) = (a*e^3 + b*e^2 + c*e),
     d2x(0) = 6*a*e^3 + 2*b*e^2;
     * In these formulas, e = 1/2^k; of course, there are separate
     * computations for the x and y values.
     *
     * There is a tradeoff in doing the above computation in fixed
     * point.  If we separate out the constant term (d) and require that
     * all the other values fit in a long, then on a 32-bit machine with
     * 12 bits of fraction in a fixed, k = 4 implies a maximum curve
     * size of 128 pixels; anything larger requires subdividing the
     * curve.  On the other hand, doing the computations in explicit
     * double precision slows down the loop by a factor of 3 or so.  We

     * found to our surprise that the latter is actually faster, because
     * the additional subdivisions cost more than the slower loop.
     *
     * We represent each quantity as I+R/M, where I is an "integer" and
     * the "remainder" R lies in the range 0 <= R < M=2^(3*k).  Note
     * that R may temporarily exceed M; for this reason, we require that
     * M have at least one free high-order bit.  To reduce the number of
     * variables, we don't actually compute M, only M-1 (rmask).  */
    fixed x = this->lx1, y = this->ly1;

    this->lx0 = this->lx1;
    this->ly0 = this->ly1;
    /* Fast check for N == 3, a common special case for small characters. */
    if (this->k <= 1) {
	--this->i;
	if (this->i == 0)
	    goto last;
#	define poly2(a,b,c) arith_rshift_1(arith_rshift_1(arith_rshift_1(a) + b) + c)
	x += poly2(this->ax, this->bx, this->cx);
	y += poly2(this->ay, this->by, this->cy);
#	undef poly2
	if_debug2('3', "[3]dx=%f, dy=%f\n",
		  fixed2float(x - this->x0), fixed2float(y - this->y0));
	if_debug5('3', "[3]%s x=%g, y=%g x=%d y=%d\n",
		  (((x ^ this->x0) | (y ^ this->y0)) & float2fixed(-0.5) ?
		   "add" : "skip"),
		  fixed2float(x), fixed2float(y), x, y);
	if (
#	if CURVED_TRAPEZOID_FILL0_COMPATIBLE
	    ((x ^ this->x0) | (y ^ this->y0)) & float2fixed(-0.5)
#	else
	    (((x ^ this->x0) | (y ^ this->y0)) & float2fixed(-0.5)) &&
	    (((x ^ this->x3) | (y ^ this->y3)) & float2fixed(-0.5))
#	endif
	    ) {
	    this->lx1 = x, this->ly1 = y;
	    vd_bar(this->lx0, this->ly0, this->lx1, this->ly1, 1, RGB(0, 255, 0));
	    return true;
	}
	--this->i;
    } else {
	--this->i;
	if (this->i == 0)
	    goto last; /* don't bother with last accum */
#	ifdef DEBUG
	    gx_flattened_iterator__print_state(this);
#	endif
#	define accum(i, r, di, dr, rmask)\
			if ( (r += dr) > rmask ) r &= rmask, i += di + 1;\
			else i += di
	accum(x, this->rx, this->idx, this->rdx, this->rmask);
	accum(y, this->ry, this->idy, this->rdy, this->rmask);
	accum(this->idx, this->rdx, this->id2x, this->rd2x, this->rmask);
	accum(this->idy, this->rdy, this->id2y, this->rd2y, this->rmask);
	accum(this->id2x, this->rd2x, this->id3x, this->rd3x, this->rmask);
	accum(this->id2y, this->rd2y, this->id3y, this->rd3y, this->rmask);
	if_debug5('3', "[3]%s x=%g, y=%g x=%d y=%d\n",
		  (((x ^ this->lx0) | (y ^ this->ly0)) & float2fixed(-0.5) ?
		   "add" : "skip"),
		  fixed2float(x), fixed2float(y), x, y);
#	undef accum
	this->lx1 = this->x = x;
	this->ly1 = this->y = y;
	vd_bar(this->lx0, this->ly0, this->lx1, this->ly1, 1, RGB(0, 255, 0));
	return true;
    }
last:
    this->lx1 = this->x3;
    this->ly1 = this->y3;
    if_debug4('3', "[3]last x=%g, y=%g x=%d y=%d\n",
	      fixed2float(this->lx1), fixed2float(this->ly1), this->lx1, this->ly1);
    vd_bar(this->lx0, this->ly0, this->lx1, this->ly1, 1, RGB(0, 255, 0));
    return false;
}

/* Move to the next segment uniting small segments,
 * and store it to this->gx0, this->gy0, this->gx1, this->gy1 .
 * Return true iff there exist more segments.
 * Note : It can generate nearly collinear segments. 
 */
private bool
gx_flattened_iterator__next_filtered1(gx_flattened_iterator *this)
{
    fixed x0 = this->lx1, y0 = this->ly1;
    bool end = (this->i == 1 << this->k);

    this->gx0 = x0;
    this->gy0 = y0;
    this->prev_filtered1_i = this->filtered1_i;
    for (;;) {
	bool more = gx_flattened_iterator__next(this);

#	if CURVED_TRAPEZOID_FILL0_COMPATIBLE
	    if (!more) {
		this->filtered1_i = this->i;
		if (end)
		    this->last_filtered1_i = this->i;
		this->gx0 = x0;
		this->gy0 = y0;
		this->gx1 = this->lx1;
		this->gy1 = this->ly1;
		return false;
	    }
#	endif
	if (!coord_near(x0, this->lx1) || !coord_near(y0, this->ly1)) {
	    this->last_filtered1_i = this->i;
	    this->filtered1_i = this->i;
	    this->gx0 = x0;
	    this->gy0 = y0;
	    this->gx1 = this->lx1;
	    this->gy1 = this->ly1;
	    return more;
	}
    }
}

/*
 * Check for nearly collinear segments -- 
 * those where one coordinate of all three points
 * (the two endpoints and the midpoint) lie within the same
 * half-pixel and both coordinates are monotonic.
 */
private inline bool
gx_check_nearly_collinear(fixed x0, fixed y0, fixed x1, fixed y1, fixed x2, fixed y2)
{
#if MERGE_COLLINEAR_SEGMENTS
    /*	The macro name above is taken from the old code.
	It appears some confusing.
	Actually it merges segments, which are nearly collinear to
	coordinate axes.
     */
    /* fixme: optimise: don't check the coordinate order for monotonic curves. */
#   define coords_in_order(v0, v1, v2) ( (((v1) - (v0)) ^ ((v2) - (v1))) >= 0 )
    if ((coord_near(x2, x0) || coord_near(y2, y0))
	    /* X or Y coordinates are within a half-pixel. */ &&
	    coords_in_order(x0, x1, x2) &&
	    coords_in_order(y0, y1, y2))
	return true;
#   undef coords_in_order
#endif
    return false;
}

/* Move to the next segment uniting nearly collinear segments,
 * and store it to this->fx0, this->fy0, this->fx1, this->fy1 .
 * Return true iff there exist more segments.
 */
private inline bool
gx_flattened_iterator__next_filtered2_aux(gx_flattened_iterator *this)
{
    /* fixme: CURVED_TRAPEZOID_FILL0_COMPATIBLE 0 is not implemented yet. */
    if (this->i == 1 << this->k) {
	/* The first segment. */
	/* Don't check collinearity because the old code does not. */
	this->fx0 = this->lx0;
	this->fy0 = this->ly0;
	this->ahead = false;
	gx_flattened_iterator__next_filtered1(this);
	this->fx1 = this->gx1;
	this->fy1 = this->gy1;
	this->filtered2_i = this->filtered1_i;
	this->last_filtered2_i = this->filtered1_i;
	if (this->i != 0) {
	    /* The unfiltered iterator will go ahead. */
	    gx_flattened_iterator__next_filtered1(this);
	    this->ahead = true;
	}
    } else {
	this->fx0 = this->fx1;
	this->fy0 = this->fy1;
	this->prev_filtered2_i = this->filtered2_i;
	for (;;) {
	    if (!this->i) {
		/* The unfiltered iterator riched the last segment. */
		if (!this->ahead) {
		    this->fx1 = this->gx1;
		    this->fy1 = this->gy1;
		    break;
		}
		this->ahead = false;
		this->filtered2_i = this->filtered1_i;
		if (this->gx0 == this->fx0 && this->gx0 == this->fx0) {
		    /* The last segment appears immediately after the first one,
		       but the first one was yielded immediately. */
		    this->fx1 = this->gx1;
		    this->fy1 = this->gy1;
		    break;
		} else {
		    /* Don't merge the last segment because the old code does not. */
		    this->fx1 = this->gx0;
		    this->fy1 = this->gy0;
		    vd_bar(this->fx0, this->fy0, this->fx1, this->fy1, 1, RGB(0, 0, 255));
		    return true;
		} 
	    } else {
		this->filtered2_i = this->filtered1_i;
		if (!gx_flattened_iterator__next_filtered1(this)) {
		    /* The old code always yields the last segment. */
		    this->fx1 = this->gx0;
		    this->fy1 = this->gy0;
		    break;
		}
		if (!gx_check_nearly_collinear(this->fx0, this->fy0, 
			this->gx0, this->gy0, this->gx1, this->gy1)) {
		    this->fx1 = this->gx0;
		    this->fy1 = this->gy0;
		    this->last_filtered2_i = this->filtered2_i;
		    this->xn = this->gx1;
		    this->yn = this->gy1;
		    break;
		}
	    }
	}
    }
    vd_bar(this->fx0, this->fy0, this->fx1, this->fy1, 1, RGB(0, 0, 255));
    return this->ahead || this->i != 0;
}

bool
gx_flattened_iterator__next_filtered2(gx_flattened_iterator *this)
{
#   if FLATTENED_ITERATOR_BACKSCAN
	return gx_flattened_iterator__next_filtered2_aux(this);
#   else
	bool more = gx_flattened_iterator__next_filtered2_aux(this);

	assert(this->filtered2_i < sizeof(this->skip_points) * 8);
	assert(this->filtered2_i >= 0);
	this->skip_points[this->filtered2_i >> 3] |= 1 << (this->filtered2_i & 7);
	return more;
#   endif
}

private inline void
gx_flattened_iterator__unaccum(gx_flattened_iterator *this)
{
#   define unaccum(i, r, di, dr, rmask)\
		    if ( r < dr ) r += rmask + 1 - dr, i -= di + 1;\
		    else r -= dr, i -= di
    unaccum(this->id2x, this->rd2x, this->id3x, this->rd3x, this->rmask);
    unaccum(this->id2y, this->rd2y, this->id3y, this->rd3y, this->rmask);
    unaccum(this->idx, this->rdx, this->id2x, this->rd2x, this->rmask);
    unaccum(this->idy, this->rdy, this->id2y, this->rd2y, this->rmask);
    unaccum(this->x, this->rx, this->idx, this->rdx, this->rmask);
    unaccum(this->y, this->ry, this->idy, this->rdy, this->rmask);
#   undef unaccum
}

/* Move back to the previous segment and store it to this->lx0, this->ly0, this->lx1, this->ly1 .
 * This only works for states reached with gx_flattened_iterator__next.
 * Return true iff there exist more segments.
 * Note : It can generate collinear segments. 
 */
bool
gx_flattened_iterator__prev(gx_flattened_iterator *this)
{
    bool last; /* i.e. the first one in the forth order. */

    assert(this->i < 1 << this->k);
    this->lx1 = this->lx0;
    this->ly1 = this->ly0;
    if (this->k <= 1) {
	/* If k==0, we have a single segment, return it.
	   If k==1 && i < 2, return the last segment.
	   Otherwise must not pass here.
	   We caould allow to pass here with this->i == 1 << this->k,
	   but we want to check the assertion about the last segment below.
	 */
	this->i++;
	this->lx0 = this->x0;
	this->ly0 = this->y0;
	vd_bar(this->lx0, this->ly0, this->lx1, this->ly1, 1, RGB(0, 0, 255));
	return false;
    }
    gx_flattened_iterator__unaccum(this);
    this->i++;
#   ifdef DEBUG
    if_debug5('3', "[3]%s x=%g, y=%g x=%d y=%d\n",
	      (((this->x ^ this->lx1) | (this->y ^ this->ly1)) & float2fixed(-0.5) ?
	       "add" : "skip"),
	      fixed2float(this->x), fixed2float(this->y), this->x, this->y);
    gx_flattened_iterator__print_state(this);
#   endif
    last = (this->i == (1 << this->k) - 1);
    this->lx0 = this->x;
    this->ly0 = this->y;
    vd_bar(this->lx0, this->ly0, this->lx1, this->ly1, 1, RGB(0, 0, 255));
    if (last)
	assert(this->lx0 == this->x0 && this->ly0 == this->y0);
    return !last;
}

/* Switching from the forward scanning to the backward scanning for the filtered1. */
void
gx_flattened_iterator__switch_to_backscan1(gx_flattened_iterator *this)
{
    /*	When scanning forth, the accumulator stands on the end of a segment,
	except for the last segment.
	When scanning back, the accumulator should stand on the beginning of a segment.
	Asuuming that at least one forward step is done.
    */
    if (this->i == 0)
	return;
    if (this->k <= 1)
	return; /* This case doesn't use the accumulator. */
    gx_flattened_iterator__unaccum(this);
#   if FLATTENED_ITERATOR_BACKSCAN
	while (this->i < this->prev_filtered1_i) {
	    gx_flattened_iterator__prev(this);
	}
#   endif
}

#if FLATTENED_ITERATOR_BACKSCAN
/* Move to the previous segment uniting small segments.
 * Return true iff there exist more segments.
 * Note : It can generate nearly collinear segments. 
 */
private bool
gx_flattened_iterator__prev_filtered1(gx_flattened_iterator *this)
{
#   if CURVED_TRAPEZOID_FILL0_COMPATIBLE
    /*	Stores the result to this->gx0, this->gy0, this->gx1, this->gy1 .
	When returned, the base (unfiltered) iterator stands
	on the segment, which's end point is the starting point
	of the result, except for the curve end.
	With the curve end it stands at the first unfiltered segment.
     */
    if (this->last_filtered1_i == 0) {
	/* A single segment curve. */
	this->gx0 = this->x0;
	this->gy0 = this->y0;
	this->gx1 = this->x3;
	this->gy1 = this->y3;
	this->filtered1_i = 0;
	return false;
    } else if (this->i < this->last_filtered1_i) {
	/* next_filtered1 has an anomaly with the last but one
	   due to possibly insufficient points to reach 
	   !coord_near. To get a consistent state we scan 
	   until the last_filtered1_i with 'prev'. */
	this->filtered1_i = this->i;
	while (this->i < this->last_filtered1_i) {
	    gx_flattened_iterator__prev(this);
	}
	assert(this->i < 1 << this->k);
	this->gx0 = this->lx1;
	this->gy0 = this->ly1;
	return true;
    } else {
	fixed x1 = this->gx0, y1 = this->gy0;
	fixed x0 = this->lx0, y0 = this->ly0;

	this->filtered1_i = this->i;
	for (;;) {
	    if (this->i + 1 == 1 << this->k) {
		this->gx0 = this->lx0;
		this->gy0 = this->ly0;
		this->gx1 = x1;
		this->gy1 = y1;
		return false;
	    }
	    gx_flattened_iterator__prev(this);
	    if (!coord_near(x0, this->lx0) || !coord_near(y0, this->ly0)) {
		this->gx0 = this->lx1;
		this->gy0 = this->ly1;
		this->gx1 = x1;
		this->gy1 = y1;
		return true;
	    }
	}
    }
#   else
    /*	Stores the result to this->lx0, this->ly0, this->lx1, this->ly1 . */
    fixed x1 = this->lx0, y1 = this->ly0;

    for (;;) {
	if (!gx_flattened_iterator__prev(this)) {
	    this->lx1 = x1;
	    this->ly1 = y1;
	    return false;
	}
	if (!coord_near(x1, this->lx0) || !coord_near(y1, this->ly0)) {
	    this->lx1 = x1;
	    this->ly1 = y1;
	    return true;
	}
    }
#   endif
}
#endif /* FLATTENED_ITERATOR_BACKSCAN */

#if !FLATTENED_ITERATOR_BACKSCAN
private bool
gx_flattened_iterator__prev_filtered2_aux(gx_flattened_iterator *this)
{
    for (;;) { /* this->filtered2_i counts back. */
	bool more = gx_flattened_iterator__prev(this);

	if (!more)
	    return more;
	if (this->skip_points[this->i >> 3] & 1 << (this->i & 7))
	    return more;
    }
}
#endif /* FLATTENED_ITERATOR_BACKSCAN */


/* Switching from the forward scanning to the backward scanning for the filtered1. */
void
gx_flattened_iterator__switch_to_backscan2(gx_flattened_iterator *this, 
	    bool last_segment)
{
#   if FLATTENED_ITERATOR_BACKSCAN
	if (last_segment)
	    return;
	gx_flattened_iterator__switch_to_backscan1(this);
	while (this->filtered1_i < this->prev_filtered2_i) {
	    this->xn = this->gx1;
	    this->yn = this->gy1;
	    gx_flattened_iterator__prev_filtered1(this);
	}
#   else
	gx_flattened_iterator__switch_to_backscan1(this);
	if (!last_segment)
	    assert(gx_flattened_iterator__prev_filtered2_aux(this));
	gx_flattened_iterator__prev_filtered2_aux(this);
#   endif
}

#if FLATTENED_ITERATOR_BACKSCAN
#if FLATTENED_ITERATOR_SELFTEST

private void
gx_flattened_iterator__test_backscan1(gx_flattened_iterator *this, 
	byte *skip_points, int skip_points_len, bool single_segment)
{
    gx_flattened_iterator fi = *this;
    bool missed_forth1 = false, extra_forth1 = false;
    int i, j;
    bool more;

    if (single_segment) {
	/* We stand on the last segment, there is no more. */
	return;
    }
    gx_flattened_iterator__switch_to_backscan1(&fi);
    i = fi.filtered1_i + 1; /* fi.i counts back. */
    do {
	more = gx_flattened_iterator__prev_filtered1(&fi);
	j = fi.filtered1_i;
	/* The bit index corresponds to the end of a filtered segment. */
	for (; i < j; i++) {
	    if (i <= skip_points_len * 8 &&
		skip_points[i >> 3] & (1 << (i & 7)))
		assert(missed_forth1);
	}
	if (j <= skip_points_len * 8 &&
	    !(skip_points[j >> 3] & (1 << (j & 7))))
	    assert(extra_forth1);
	i = j + 1;
    } while(more);
}

private void
gx_flattened_iterator__test_filtered1(gx_flattened_iterator *this)
{
    /* 'this' must be just initialized. */
    byte skip_points[(1 << k_sample_max) / 8];
    gx_flattened_iterator fi = *this;
    bool more, single_segment = true;

    memset(skip_points, 0, sizeof(skip_points));
    do {
	more = gx_flattened_iterator__next_filtered1(&fi);
	if (fi.i <= sizeof(skip_points) * 8) {
	    skip_points[fi.i >> 3] |= 1 << (fi.i & 7);
	    /* The bit index corresponds to the end of an unfiltered segment. */
	}
	if (FLATTENED_ITERATOR_HEAVY_SELFTEST || !more)
	    gx_flattened_iterator__test_backscan1(&fi, skip_points, sizeof(skip_points), 
		    single_segment);
	single_segment &= !more;
    } while(more);
}
#endif /* FLATTENED_ITERATOR_SELFTEST */

/* Move to the previous segment uniting nearly collinear segments,
 * and store it to this->fx0, this->fy0, this->fx1, this->fy1 .
 * Return true iff there exist more segments.
 * It should return exactly same segments, which 'next_filtered2' does.
 */
bool
gx_flattened_iterator__prev_filtered2(gx_flattened_iterator *this)
{
    /*	This algotithm is based on 2 properties of gx_check_nearly_collinear :
	If 3 points are nearly collinear, same central point is nearly collinear
	with any 2 points taken from same interval.
	If 3 points are not nearly collinear, same central point is not nearly collinear
	with any 2 points taken from a wider interval.
     */
    /*	When returned, the base (unfiltered2, i.e. filtered1) iterator stands
	on the segment, which's end point is the starting point
	of the result, except for the curve end.
	With the curve end it stands at the first filtered1 segment.
	Note that this->i is one step ahead due to filtered1.
     */
    /* fixme: CURVED_TRAPEZOID_FILL0_COMPATIBLE 0 is not implemented yet. */
    fixed x1 = this->fx0, y1 = this->fy0;

    if (this->last_filtered2_i == 0) {
	/* A single segment curve. */
	this->fx0 = this->x0;
	this->fy0 = this->y0;
	this->fx1 = this->x3;
	this->fy1 = this->y3;
	this->filtered2_i = 0;
	return false;
    } else if (this->filtered1_i < this->last_filtered2_i) {
	/* next_filtered2 has an anomaly with the last but one
	   due to possibly insufficient points to reach 
	   !gx_check_nearly_collinear. We scan until the last_filtered2_i
	   with 'prev' and do one ahead step with 'prev_filtered1'. 
	   Note this is not executed with a single segment curve. */
	while (this->filtered1_i < this->last_filtered2_i) {
	    if (!gx_flattened_iterator__prev_filtered1(this))
		break;
	}
	if (x1 == this->gx1 && y1 == this->gy1) {
	    /* The last segment appears immediately after the first one,
	       but the last one was yielded immediately. */
	    this->filtered2_i = this->filtered1_i;
	    this->fx1 = x1;
	    this->fy1 = y1;
	    this->fx0 = this->gx0;
	    this->fy0 = this->gy0;
	    return false;
	}
        this->filtered2_i = this->filtered1_i;
	this->ahead = true;
	this->fx1 = x1;
	this->fy1 = y1;
	this->fx0 = this->gx1;
	this->fy0 = this->gy1;
	return true;
    } else if (!this->ahead) {
#	if CURVED_TRAPEZOID_FILL0_COMPATIBLE
	    this->fx1 = x1;
	    this->fy1 = y1;
	    this->fx0 = this->gx0;
	    this->fy0 = this->gy0;
	    this->filtered2_i = this->filtered1_i;
	    return false;
#	else
	    /* Must not happen. */
	    assert(0);
	    return false;
#	endif
    } else if (this->gx0 == this->x0 && this->gy0 == this->y0) {
	/* The base iterator is ahead and has no more segments. */
	this->fx1 = x1;
	this->fy1 = y1;
	this->fx0 = this->gx0;
	this->fy0 = this->gy0;
	this->filtered2_i = this->filtered1_i;
	this->ahead = false;
	return false;
    } else {
	fixed x0 = this->gx0, y0 = this->gy0;
	fixed xn = this->xn, yn = this->yn;
	bool more;

	assert(this->gx1 == this->fx0 && this->gy1 == this->fy0);
	/*  Invariant :
	    The last yielded filtered2 segment is (p1, p2) where 
		p1 = (x1, y1), p2 = (this->fx1, this->fy1);
	    The next filtered1 point after p1 is n = (nx, ny).
	    The base (the filtered1) iterator stands on (p0, p1) 
		where p0 = (x0, y0), p1 = (x1, y1).
	 */
	for (;;) {
	    fixed xp, yp;

	    this->filtered2_i = this->filtered1_i;
	    more = gx_flattened_iterator__prev_filtered1(this);
	    xp = this->gx0, yp = this->gy0;
	    if (gx_check_nearly_collinear(xp, yp, x0, y0, x1, y1)) {
		/* p0 was skipped by the forward iterator. */
		/* Find minimal p being collinear with x0, x1
		   and not collinear with x1 and n. 
		   This is what the forward filtered2 iterator does. */
		if (more && !gx_check_nearly_collinear(xp, yp, x1, y1, xn, yn)) {
		    /* The forward filtered2 iterator started with p. */
		    /* Find minimal p being collinear with x0, x1.
		       It is not collinear with x1 and n due to the check above,
		       and won't be after enhanced. */

		    for (;;) {
			this->xn = this->gx1, this->yn = this->gy1;
			more = gx_flattened_iterator__prev_filtered1(this);
			x0 = this->gx1, y0 = this->gy1;
			xp = this->gx0, yp = this->gy0;
			if (!gx_check_nearly_collinear(xp, yp, x0, y0, x1, y1)) {
			    this->fx0 = x0, this->fy0 = y0;
			    this->fx1 = x1, this->fy1 = y1;
			    return true;
			} else if (!more) {
#			    if CURVED_TRAPEZOID_FILL0_COMPATIBLE
				/* The old code always yields the first segment. */
				this->fx0 = x0, this->fy0 = y0;
				this->fx1 = x1, this->fy1 = y1;
				return true;
#			    else
				this->fx0 = xp, this->fy0 = yp;
				this->fx1 = x1, this->fy1 = y1;
				/* The invariant is broken, no more calls please. */
				return false;
#			    endif
			}
		    }
		} else if (!more) {
#		    if CURVED_TRAPEZOID_FILL0_COMPATIBLE
			/* The old code always yields the first segment. */
			this->fx0 = x0, this->fy0 = y0;
			this->fx1 = x1, this->fy1 = y1;
			return true;
#		    else
			this->fx0 = xp, this->fy0 = yp;
			this->fx1 = x1, this->fy1 = y1;
			/* The invariant is broken, no more calls please. */
			return false;
#		    endif
		}
	    } else {
		/* p0 was yielded by the forward iterator. */
		this->fx0 = x0, this->fy0 = y0;
		this->fx1 = x1, this->fy1 = y1;
		/* Restore the invariant: */
		this->xn = x1, this->yn = y1;
		return true;
	    }
	}

    }
}
#endif /* FLATTENED_ITERATOR_BACKSCAN */

#if FLATTENED_ITERATOR_SELFTEST

private void
gx_flattened_iterator__test_backscan2(gx_flattened_iterator *this, 
	gs_fixed_point *points, gs_fixed_point *ppt, byte *skip_points, 
	int skip_points_len, bool single_segment, bool last_segment)
{   int i, j;
    gx_flattened_iterator fi = *this;
    bool more;
    bool missed_back2 = false, extra_back2 = false;

    if (single_segment) {
	/* We stand on the last segment, there is no more. */
	return;
    }
    gx_flattened_iterator__switch_to_backscan2(&fi, last_segment);
    i = fi.last_filtered2_i + 1; /* fi.i counts back. */
    if (points != NULL)
	ppt--;
    do {
	more = gx_flattened_iterator__prev_filtered2(&fi);
	j = fi.filtered2_i;
	/* The bit index corresponds to the end of a filtered segment. */
	for (; i < j; i++) {
	    if (i <= skip_points_len * 8 &&
		skip_points[i >> 3] & (1 << (i & 7)))
		assert(missed_back2);
	}
	if (j <= skip_points_len * 8 &&
	    !(skip_points[j >> 3] & (1 << (j & 7))))
	    assert(extra_back2);
	i = j + 1;
	if (points != NULL) {
	    ppt--;
	    assert(ppt >= points);
	    assert(ppt->x == fi.fx1 && ppt->y == fi.fy1);
	}
    } while(more);
    if (points != NULL)
	assert(ppt == points);
}

private void
gx_flattened_iterator__test_filtered2(gx_flattened_iterator *this)
{
    /* 'this' must be just initialized. */
    byte skip_points[(1 << k_sample_max) / 8];
    gx_flattened_iterator fi = *this, fi1;
    bool more, first = true, single_segment = true;
    bool missed_forth2 = false, extra_forth2 = false;
    int i, j;
    gs_fixed_point points[100], *ppt = points, *ppe = ppt + count_of(points), *ppp = points;

    memset(skip_points, 0, sizeof(skip_points));
    do {
	more = gx_flattened_iterator__next_filtered1(&fi);
	if (ppt > points + 1 && more)
	    if (gx_check_nearly_collinear(ppt[-2].x, ppt[-2].y, 
			    ppt[-1].x, ppt[-1].y, fi.gx1, fi.gy1))
		--ppt;		/* remove middle point */
	ppt->x = fi.gx1;
	ppt->y = fi.gy1;
	ppt++;
    } while(more && ppt < ppe);
    fi = *this;
    do {
	fixed x = fi.lx1, y = fi.ly1;

	more = gx_flattened_iterator__next_filtered1(&fi);
	if (!first) {
	    fi1 = fi;
	    while (more) {
		more = gx_flattened_iterator__next_filtered1(&fi1);
		if (!more) {
		    more = true;
		    break;
		}
		if (!gx_check_nearly_collinear(x, y, fi1.gx0, fi1.gy0, fi1.gx1, fi1.gy1)) 
		    break;
		fi = fi1;
	    }
	}
	if (fi.i <= sizeof(skip_points) * 8)
	    skip_points[fi.i >> 3] |= 1 << (fi.i & 7);
	first = false;
    } while(more);
    fi = *this;
    i = (1 << fi.k) - 1; /* fi.i counts back. */
    do {
	more = gx_flattened_iterator__next_filtered2(&fi);
	j = fi.filtered2_i;
	/* The bit index corresponds to the end of a filtered segment. */
	for (; i > j; i--) {
	    if (i <= sizeof(skip_points) * 8 &&
		skip_points[i >> 3] & (1 << (i & 7)))
		assert(missed_forth2);
	}
	if (j <= sizeof(skip_points) * 8 &&
	    !(skip_points[j >> 3] & (1 << (j & 7))))
	    assert(extra_forth2);
	i = j - 1;
	if (ppt < ppe) {
	    assert(ppp < ppt);
	    assert(ppp->x == fi.fx1 && ppp->y == fi.fy1);
	    ppp++;
	}
	if (FLATTENED_ITERATOR_HEAVY_SELFTEST || !more)
	    gx_flattened_iterator__test_backscan2(&fi, 
		(ppt < ppe ? points : NULL), ppp, skip_points, 
		sizeof(skip_points), single_segment, !more);
	single_segment &= !more;
    } while(more);
    if (ppt < ppe)
	assert(ppp == ppt);
}
#endif /* FLATTENED_ITERATOR_SELFTEST */

#if !FLATTENED_ITERATOR_BACKSCAN
/* Move to the previous segment uniting nearly collinear segments,
 * and store it to this->fx0, this->fy0, this->fx1, this->fy1 .
 * Return true iff there exist more segments.
 * It should return exactly same segments, which 'next_filtered2' does.
 */
bool
gx_flattened_iterator__prev_filtered2(gx_flattened_iterator *this)
{
    this->fx1 = this->fx0;
    this->fy1 = this->fy0;
    this->filtered2_i = this->i;
    if (this->i + 1 == 1 << this->k) {
	this->fx0 = this->x0;
	this->fy0 = this->y0;
	return false;
    } else {
	gx_flattened_iterator__prev_filtered2_aux(this);
	if (this->skip_points[this->i >> 3] & 1 << (this->i & 7)) {
	    this->fx0 = this->lx1;
	    this->fy0 = this->ly1;
	    return true;
	} else {
	    this->fx0 = this->x0;
	    this->fy0 = this->y0;
	    return false;
	}
    }
}

#endif /* !FLATTENED_ITERATOR_BACKSCAN */

#define max_points 50		/* arbitrary */

private int
generate_segments(gx_path * ppath, const gs_fixed_point *points, 
		    int count, segment_notes notes)
{
    /* vd_moveto(ppath->position.x, ppath->position.y); */
    if (notes & sn_not_first) {
	/* vd_lineto_multi(points, count); */
	print_points(points, count);
	return gx_path_add_lines_notes(ppath, points, count, notes);
    } else {
	int code;

	/* vd_lineto(points[0].x, points[0].y); */
	print_points(points, 1);
	code = gx_path_add_line_notes(ppath, points[0].x, points[0].y, notes);
	if (code < 0)
	    return code;
	/* vd_lineto_multi(points + 1, count - 1); */
	print_points(points + 1, count - 1);
	return gx_path_add_lines_notes(ppath, points + 1, count - 1, notes | sn_not_first);
    }
}

private int
gx_subdivide_curve_rec(gx_flattened_iterator *this, 
		  gx_path * ppath, int k, curve_segment * pc,
		  segment_notes notes, gs_fixed_point *points)
{
    int code;

top :
    if (!gx_flattened_iterator__init(this, 
		ppath->position.x, ppath->position.y, pc, k, false)) {
	/* Curve is too long.  Break into two pieces and recur. */
	curve_segment cseg;

	k--;
	split_curve_midpoint(ppath->position.x, ppath->position.y, pc, &cseg, pc);
	code = gx_subdivide_curve_rec(this, ppath, k, &cseg, notes, points);
	if (code < 0)
	    return code;
	notes |= sn_not_first;
	goto top;
    } else if (k == -1) {
	/* fixme : Don't need to init the iterator. Just wanted to check in_range. */
	return gx_path_add_curve_notes(ppath, pc->p1.x, pc->p1.y, pc->p2.x, pc->p2.y, 
			pc->pt.x, pc->pt.y, notes);
    } else {
	gs_fixed_point *ppt = points;
	bool more;

	for(;;) {
#	    if 0
		/* The old code does so - keeping it for a while. */
		more = gx_flattened_iterator__next_filtered1(this);
		if (ppt > points + 1 && more)
		    if (gx_check_nearly_collinear(ppt[-2].x, ppt[-2].y, 
				    ppt[-1].x, ppt[-1].y, this->lx1, this->ly1))
			--ppt;		/* remove middle point */
		ppt->x = this->lx1;
		ppt->y = this->ly1;
#	    else
		more = gx_flattened_iterator__next_filtered2(this);
		ppt->x = this->fx1;
		ppt->y = this->fy1;
#	    endif
	    ppt++;
	    if (ppt == &points[max_points] || !more) {
		gs_fixed_point *pe = (more ?  ppt - 2 : ppt);

		code = generate_segments(ppath, points, pe - points, notes);
		if (code < 0)
		    return code;
		if (!more)
		    return 0;
		notes |= sn_not_first;
		memcpy(points, pe, (char *)ppt - (char *)pe);
		ppt = points + (ppt - pe);
	    }
	}
    }
}

#undef coord_near

/*
 * Flatten a segment of the path by repeated sampling.
 * 2^k is the number of lines to produce (i.e., the number of points - 1,
 * including the endpoints); we require k >= 1.
 * If k or any of the coefficient values are too large,
 * use recursive subdivision to whittle them down.
 */

int
gx_subdivide_curve(gx_path * ppath, int k, curve_segment * pc, segment_notes notes)
{
    gs_fixed_point points[max_points + 1];
    gx_flattened_iterator iter;

    return gx_subdivide_curve_rec(&iter, ppath, k, pc, notes, points);
}

#undef max_points


