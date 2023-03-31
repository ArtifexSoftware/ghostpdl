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


/* Path flattening algorithms */
#include "string_.h"
#include "gx.h"
#include "gserrors.h"
#include "gxarith.h"
#include "gxfixed.h"
#include "gzpath.h"
#include "memory_.h"

/* ---------------- Curve flattening ---------------- */

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
        x03 = pc->pt.x - x0,
        y03 = pc->pt.y - y0;
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
              x12 = pc->p1.x - pc->p2.x, y12 = pc->p1.y - pc->p2.y,
              dx0 = x0 - pc->p1.x - x12, dy0 = y0 - pc->p1.y - y12,
              dx1 = x12 - pc->p2.x + pc->pt.x, dy1 = y12 - pc->p2.y + pc->pt.y,
              adx0 = any_abs(dx0), ady0 = any_abs(dy0),
              adx1 = any_abs(dx1), ady1 = any_abs(dy1);
        fixed
            d = max(adx0, adx1) + max(ady0, ady1);
        /*
         * The following statement is split up to work around a
         * bug in the gcc 2.7.2 optimizer on H-P RISC systems.
         */
        uint qtmp = d - (d >> 2) /* 3/4 * D */ +fixed_flat - 1;
        uint q = qtmp / fixed_flat;

        if_debug6('2', "[2]d01=%g,%g d12=%g,%g d23=%g,%g\n",
                  fixed2float(pc->p1.x - x0), fixed2float(pc->p1.y - y0),
                  fixed2float(-x12), fixed2float(-y12),
                  fixed2float(pc->pt.x - pc->p2.x), fixed2float(pc->pt.y - pc->p2.y));
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
static void
split_curve_midpoint(fixed x0, fixed y0, const curve_segment * pc,
                     curve_segment * pc1, curve_segment * pc2)
{				/*
                                 * We have to define midpoint carefully to avoid overflow.
                                 * (If it overflows, something really pathological is going
                                 * on, but we could get infinite recursion that way....)
                                 */
#define midpoint(a,b)\
  (arith_rshift_1(a) + arith_rshift_1(b) + (((a) | (b)) & 1))
    fixed x12 = midpoint(pc->p1.x, pc->p2.x);
    fixed y12 = midpoint(pc->p1.y, pc->p2.y);

    /*
     * pc1 or pc2 may be the same as pc, so we must be a little careful
     * about the order in which we store the results.
     */
    pc1->p1.x = midpoint(x0, pc->p1.x);
    pc1->p1.y = midpoint(y0, pc->p1.y);
    pc2->p2.x = midpoint(pc->p2.x, pc->pt.x);
    pc2->p2.y = midpoint(pc->p2.y, pc->pt.y);
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

static inline void
print_points(const gs_fixed_point *points, int count)
{
#ifdef DEBUG
    int i;

    if (!gs_debug_c('3'))
        return;
    for (i = 0; i < count; i++)
      if_debug2('3', "[3]out x=%ld y=%ld\n",
                (long)points[i].x, (long)points[i].y);
#endif
}

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

/*  Initialize the iterator.
    Momotonic curves with non-zero length are only allowed.
 */
bool
gx_flattened_iterator__init(gx_flattened_iterator *self,
            fixed x0, fixed y0, const curve_segment *pc, int k)
{
    /* Note : Immediately after the ininialization it keeps an invalid (zero length) segment. */
    fixed x1, y1, x2, y2;
    const int k2 = k << 1, k3 = k2 + k;
    fixed bx2, by2, ax6, ay6;

    x1 = pc->p1.x;
    y1 = pc->p1.y;
    x2 = pc->p2.x;
    y2 = pc->p2.y;
    self->x0 = self->lx0 = self->lx1 = x0;
    self->y0 = self->ly0 = self->ly1 = y0;
    self->x3 = pc->pt.x;
    self->y3 = pc->pt.y;
    if (!curve_coeffs_ranged(self->x0, x1, x2, self->x3,
                             self->y0, y1, y2, self->y3,
                             &self->ax, &self->bx, &self->cx,
                             &self->ay, &self->by, &self->cy, k))
        return false;
    self->curve = true;
    self->k = k;
#ifdef DEBUG
        if (gs_debug_c('3')) {
            dlprintf4("[3]x0=%f y0=%f x1=%f y1=%f\n",
                      fixed2float(self->x0), fixed2float(self->y0),
                      fixed2float(x1), fixed2float(y1));
            dlprintf5("   x2=%f y2=%f x3=%f y3=%f  k=%d\n",
                      fixed2float(x2), fixed2float(y2),
                      fixed2float(self->x3), fixed2float(self->y3), self->k);
        }
#endif
    if (k == -1) {
        /* A special hook for gx_subdivide_curve_rec.
           Only checked the range.
           Returning with no initialization. */
        return true;
    }
    self->rmask = (1 << k3) - 1;
    self->i = (1 << k);
    self->rx = self->ry = 0;
    if_debug6('3', "[3]ax=%f bx=%f cx=%f\n   ay=%f by=%f cy=%f\n",
              fixed2float(self->ax), fixed2float(self->bx), fixed2float(self->cx),
              fixed2float(self->ay), fixed2float(self->by), fixed2float(self->cy));
    bx2 = self->bx << 1;
    by2 = self->by << 1;
    ax6 = ((self->ax << 1) + self->ax) << 1;
    ay6 = ((self->ay << 1) + self->ay) << 1;
    self->idx = arith_rshift(self->cx, self->k);
    self->idy = arith_rshift(self->cy, self->k);
    self->rdx = ((uint)self->cx << k2) & self->rmask;
    self->rdy = ((uint)self->cy << k2) & self->rmask;
    /* bx/y terms */
    self->id2x = arith_rshift(bx2, k2);
    self->id2y = arith_rshift(by2, k2);
    self->rd2x = ((uint)bx2 << self->k) & self->rmask;
    self->rd2y = ((uint)by2 << self->k) & self->rmask;
#   define adjust_rem(r, q, rmask) if ( r > rmask ) q ++, r &= rmask
    /* We can compute all the remainders as ints, */
    /* because we know they don't exceed M. */
    /* cx/y terms */
    self->idx += arith_rshift_1(self->id2x);
    self->idy += arith_rshift_1(self->id2y);
    self->rdx += ((uint)self->bx << self->k) & self->rmask,
    self->rdy += ((uint)self->by << self->k) & self->rmask;
    adjust_rem(self->rdx, self->idx, self->rmask);
    adjust_rem(self->rdy, self->idy, self->rmask);
    /* ax/y terms */
    self->idx += arith_rshift(self->ax, k3);
    self->idy += arith_rshift(self->ay, k3);
    self->rdx += (uint)self->ax & self->rmask;
    self->rdy += (uint)self->ay & self->rmask;
    adjust_rem(self->rdx, self->idx, self->rmask);
    adjust_rem(self->rdy, self->idy, self->rmask);
    self->id2x += self->id3x = arith_rshift(ax6, k3);
    self->id2y += self->id3y = arith_rshift(ay6, k3);
    self->rd2x += self->rd3x = (uint)ax6 & self->rmask,
    self->rd2y += self->rd3y = (uint)ay6 & self->rmask;
    adjust_rem(self->rd2x, self->id2x, self->rmask);
    adjust_rem(self->rd2y, self->id2y, self->rmask);
#   undef adjust_rem
    return true;
}

static inline bool
check_diff_overflow(fixed v0, fixed v1)
{
    if (v1 > 0)
        return (v0 < min_fixed + v1);
    else if (v1 < 0)
        return (v0 > max_fixed + v1);
    return false;
}

bool
gx_check_fixed_diff_overflow(fixed v0, fixed v1)
{
    return check_diff_overflow(v0, v1);
}
bool
gx_check_fixed_sum_overflow(fixed v0, fixed v1)
{
    /* We assume that clamp_point_aux have been applied to v1,
       thus -v alweays exists.
     */
    return check_diff_overflow(v0, -v1);
}

/*  Initialize the iterator with a line. */
bool
gx_flattened_iterator__init_line(gx_flattened_iterator *self,
            fixed x0, fixed y0, fixed x1, fixed y1)
{
    bool ox = check_diff_overflow(x0, x1);
    bool oy = check_diff_overflow(y0, y1);

    self->x0 = self->lx0 = self->lx1 = x0;
    self->y0 = self->ly0 = self->ly1 = y0;
    self->x3 = x1;
    self->y3 = y1;
    if (ox || oy) {
        /* Subdivide a long line into 4 segments, because the filling algorithm
           and the stroking algorithm need to compute differences
           of coordinates of end points.
           We can't use 2 segments, because gx_flattened_iterator__next
           implements a special code for that case,
           which requires differences of coordinates as well.
         */
        /* Note : the result of subdivision may be not strongly colinear. */
        self->ax = self->bx = 0;
        self->ay = self->by = 0;
        self->cx = ((ox ? (x1 >> 1) - (x0 >> 1) : (x1 - x0) >> 1) + 1) >> 1;
        self->cy = ((oy ? (y1 >> 1) - (y0 >> 1) : (y1 - y0) >> 1) + 1) >> 1;
        self->rd3x = self->rd3y = self->id3x = self->id3y = 0;
        self->rd2x = self->rd2y = self->id2x = self->id2y = 0;
        self->idx = self->cx;
        self->idy = self->cy;
        self->rdx = self->rdy = 0;
        self->rx = self->ry = 0;
        self->rmask = 0;
        self->k = 2;
        self->i = 4;
    } else {
        self->k = 0;
        self->i = 1;
    }
    self->curve = false;
    return true;
}

#ifdef DEBUG
static inline void
gx_flattened_iterator__print_state(gx_flattened_iterator *self)
{
    if (!gs_debug_c('3'))
        return;
    dlprintf4("[3]dx=%f+%d, dy=%f+%d\n",
              fixed2float(self->idx), self->rdx,
              fixed2float(self->idy), self->rdy);
    dlprintf4("   d2x=%f+%d, d2y=%f+%d\n",
              fixed2float(self->id2x), self->rd2x,
              fixed2float(self->id2y), self->rd2y);
    dlprintf4("   d3x=%f+%d, d3y=%f+%d\n",
              fixed2float(self->id3x), self->rd3x,
              fixed2float(self->id3y), self->rd3y);
}
#endif

/* Move to the next segment and store it to self->lx0, self->ly0, self->lx1, self->ly1 .
 * Return true iff there exist more segments.
 */
int
gx_flattened_iterator__next(gx_flattened_iterator *self)
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
    fixed x = self->lx1, y = self->ly1;

    if (self->i <= 0)
        return_error(gs_error_unregistered); /* Must not happen. */
    self->lx0 = self->lx1;
    self->ly0 = self->ly1;
    /* Fast check for N == 3, a common special case for small characters. */
    if (self->k <= 1) {
        --self->i;
        if (self->i == 0)
            goto last;
#	define poly2(a,b,c) arith_rshift_1(arith_rshift_1(arith_rshift_1(a) + b) + c)
        x += poly2(self->ax, self->bx, self->cx);
        y += poly2(self->ay, self->by, self->cy);
#	undef poly2
        if_debug2('3', "[3]dx=%f, dy=%f\n",
                  fixed2float(x - self->x0), fixed2float(y - self->y0));
        if_debug5('3', "[3]%s x=%g, y=%g x=%ld y=%ld\n",
                  (((x ^ self->x0) | (y ^ self->y0)) & float2fixed(-0.5) ?
                   "add" : "skip"),
                  fixed2float(x), fixed2float(y), (long)x, (long)y);
        self->lx1 = x, self->ly1 = y;
        return true;
    } else {
        --self->i;
        if (self->i == 0)
            goto last; /* don't bother with last accum */
#	ifdef DEBUG
            gx_flattened_iterator__print_state(self);
#	endif
#	define accum(i, r, di, dr, rmask)\
                        if ( (r += dr) > rmask ) r &= rmask, i += di + 1;\
                        else i += di
        accum(x, self->rx, self->idx, self->rdx, self->rmask);
        accum(y, self->ry, self->idy, self->rdy, self->rmask);
        accum(self->idx, self->rdx, self->id2x, self->rd2x, self->rmask);
        accum(self->idy, self->rdy, self->id2y, self->rd2y, self->rmask);
        accum(self->id2x, self->rd2x, self->id3x, self->rd3x, self->rmask);
        accum(self->id2y, self->rd2y, self->id3y, self->rd3y, self->rmask);
        if_debug5('3', "[3]%s x=%g, y=%g x=%ld y=%ld\n",
                  (((x ^ self->lx0) | (y ^ self->ly0)) & float2fixed(-0.5) ?
                   "add" : "skip"),
                  fixed2float(x), fixed2float(y), (long)x, (long)y);
#	undef accum
        self->lx1 = self->x = x;
        self->ly1 = self->y = y;
        return true;
    }
last:
    self->lx1 = self->x3;
    self->ly1 = self->y3;
    if_debug4('3', "[3]last x=%g, y=%g x=%ld y=%ld\n",
              fixed2float(self->lx1), fixed2float(self->ly1),
              (long)self->lx1, (long)self->ly1);
    return false;
}

static inline void
gx_flattened_iterator__unaccum(gx_flattened_iterator *self)
{
#   define unaccum(i, r, di, dr, rmask)\
                    if ( r < dr ) r += rmask + 1 - dr, i -= di + 1;\
                    else r -= dr, i -= di
    unaccum(self->id2x, self->rd2x, self->id3x, self->rd3x, self->rmask);
    unaccum(self->id2y, self->rd2y, self->id3y, self->rd3y, self->rmask);
    unaccum(self->idx, self->rdx, self->id2x, self->rd2x, self->rmask);
    unaccum(self->idy, self->rdy, self->id2y, self->rd2y, self->rmask);
    unaccum(self->x, self->rx, self->idx, self->rdx, self->rmask);
    unaccum(self->y, self->ry, self->idy, self->rdy, self->rmask);
#   undef unaccum
}

/* Move back to the previous segment and store it to self->lx0, self->ly0, self->lx1, self->ly1 .
 * This only works for states reached with gx_flattened_iterator__next.
 * Return true iff there exist more segments.
 */
int
gx_flattened_iterator__prev(gx_flattened_iterator *self)
{
    bool last; /* i.e. the first one in the forth order. */

    if (self->i >= 1 << self->k)
        return_error(gs_error_unregistered); /* Must not happen. */
    self->lx1 = self->lx0;
    self->ly1 = self->ly0;
    if (self->k <= 1) {
        /* If k==0, we have a single segment, return it.
           If k==1 && i < 2, return the last segment.
           Otherwise must not pass here.
           We caould allow to pass here with self->i == 1 << self->k,
           but we want to check the assertion about the last segment below.
         */
        self->i++;
        self->lx0 = self->x0;
        self->ly0 = self->y0;
        return false;
    }
    gx_flattened_iterator__unaccum(self);
    self->i++;
#   ifdef DEBUG
    if_debug5('3', "[3]%s x=%g, y=%g x=%ld y=%ld\n",
              (((self->x ^ self->lx1) | (self->y ^ self->ly1)) & float2fixed(-0.5) ?
               "add" : "skip"),
              fixed2float(self->x), fixed2float(self->y),
              (long)self->x, (long)self->y);
    gx_flattened_iterator__print_state(self);
#   endif
    last = (self->i == (1 << self->k) - 1);
    self->lx0 = self->x;
    self->ly0 = self->y;
    if (last)
        if (self->lx0 != self->x0 || self->ly0 != self->y0)
            return_error(gs_error_unregistered); /* Must not happen. */
    return !last;
}

/* Switching from the forward scanning to the backward scanning for the filtered1. */
void
gx_flattened_iterator__switch_to_backscan(gx_flattened_iterator *self, bool not_first)
{
    /*	When scanning forth, the accumulator stands on the end of a segment,
        except for the last segment.
        When scanning back, the accumulator should stand on the beginning of a segment.
        Assuming at least one forward step is done.
    */
    if (not_first)
        if (self->i > 0 && self->k != 1 /* This case doesn't use the accumulator. */)
            gx_flattened_iterator__unaccum(self);
}

#define max_points 50		/* arbitrary */

static int
generate_segments(gx_path * ppath, const gs_fixed_point *points,
                    int count, segment_notes notes)
{
    if (notes & sn_not_first) {
        print_points(points, count);
        return gx_path_add_lines_notes(ppath, points, count, notes);
    } else {
        int code;

        print_points(points, 1);
        code = gx_path_add_line_notes(ppath, points[0].x, points[0].y, notes);
        if (code < 0)
            return code;
        print_points(points + 1, count - 1);
        return gx_path_add_lines_notes(ppath, points + 1, count - 1, notes | sn_not_first);
    }
}

static int
gx_subdivide_curve_rec(gx_flattened_iterator *self,
                  gx_path * ppath, int k, curve_segment * pc,
                  segment_notes notes, gs_fixed_point *points)
{
    int code;

top :
    if (!gx_flattened_iterator__init(self,
                ppath->position.x, ppath->position.y, pc, k)) {
        /* Curve is too long.  Break into two pieces and recur. */
        curve_segment cseg;

        k--;
        split_curve_midpoint(ppath->position.x, ppath->position.y, pc, &cseg, pc);
        code = gx_subdivide_curve_rec(self, ppath, k, &cseg, notes, points);
        if (code < 0)
            return code;
        notes |= sn_not_first;
        goto top;
    } else if (k < 0) {
        /* This used to be k == -1, but... If we have a very long curve, we will first
         * go through the code above to split the long curve into 2. In fact for very
         * long curves we can go through that multiple times. This can lead to k being
         * < -1 by the time we finish subdividing the curve, and that meant we did not
         * satisfy the exit condition here, leading to a loop until VM error.
         */
        /* fixme : Don't need to init the iterator. Just wanted to check in_range. */
        return gx_path_add_curve_notes(ppath, pc->p1.x, pc->p1.y, pc->p2.x, pc->p2.y,
                        pc->pt.x, pc->pt.y, notes);
    } else {
        gs_fixed_point *ppt = points;
        bool more;

        for(;;) {
            code = gx_flattened_iterator__next(self);

            if (code < 0)
                return code;
            more = code;
            ppt->x = self->lx1;
            ppt->y = self->ly1;
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
