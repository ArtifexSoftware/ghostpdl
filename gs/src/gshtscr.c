/* Copyright (C) 1993, 2000 Aladdin Enterprises.  All rights reserved.
  
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
/* Screen (Type 1) halftone processing for Ghostscript library */
#include "math_.h"
#include "gx.h"
#include "gserrors.h"
#include "gsstruct.h"
#include "gxarith.h"
#include "gzstate.h"
#include "gxdevice.h"           /* for gzht.h */
#include "gzht.h"
#include "gswts.h"

/* Define whether to force all halftones to be strip halftones, */
/* for debugging. */
static const bool FORCE_STRIP_HALFTONES = false;

/* Structure descriptors */
private_st_gs_screen_enum();

/* GC procedures */
private 
ENUM_PTRS_WITH(screen_enum_enum_ptrs, gs_screen_enum *eptr)
{
    if (index < 1 + st_ht_order_max_ptrs) {
        gs_ptr_type_t ret =
            ENUM_USING(st_ht_order, &eptr->order, sizeof(eptr->order),
                       index - 1);

        if (ret == 0)           /* don't stop early */
            ENUM_RETURN(0);
        return ret;
    }
    return ENUM_USING(st_halftone, &eptr->halftone, sizeof(eptr->halftone),
                      index - (1 + st_ht_order_max_ptrs));
}
ENUM_PTR(0, gs_screen_enum, pgs);
ENUM_PTRS_END
private RELOC_PTRS_WITH(screen_enum_reloc_ptrs, gs_screen_enum *eptr)
{
    RELOC_PTR(gs_screen_enum, pgs);
    RELOC_USING(st_halftone, &eptr->halftone, sizeof(gs_halftone));
    RELOC_USING(st_ht_order, &eptr->order, sizeof(gx_ht_order));
}
RELOC_PTRS_END

/* Define the default value of AccurateScreens that affects setscreen
   and setcolorscreen. Note that this is effectively a global, and
   thus gets in the way of reentrancy. We'll want to fix that. */
private bool screen_accurate_screens;

/* Default AccurateScreens control */
void
gs_setaccuratescreens(bool accurate)
{
    screen_accurate_screens = accurate;
}
bool
gs_currentaccuratescreens(void)
{
    return screen_accurate_screens;
}

/* As with AccurateScreens, this is also effectively a global. However,
   it is going away soon. */
private bool screen_use_wts;

void
gs_setusewts(bool use_wts)
{
    screen_use_wts = use_wts;
}
bool
gs_currentusewts(void)
{
    return screen_use_wts;
}

/* Define the MinScreenLevels user parameter similarly. */
private uint screen_min_screen_levels;

void
gs_setminscreenlevels(uint levels)
{
    screen_min_screen_levels = levels;
}
uint
gs_currentminscreenlevels(void)
{
    return screen_min_screen_levels;
}

/* Initialize the screen control statics at startup. */
init_proc(gs_gshtscr_init);     /* check prototype */
int
gs_gshtscr_init(gs_memory_t *mem)
{
    gs_setaccuratescreens(false);
    gs_setminscreenlevels(1);
    return 0;
}

/*
 * The following implementation notes complement the general discussion of
 * halftone tiles found in gxdht.h.
 *
 * Currently we allow R(') > 1 (i.e., multiple basic cells per multi-cell)
 * only if AccurateScreens is true or if B (the number of pixels in a basic
 * cell) < MinScreenLevels; if AccurateScreens is false and B >=
 * MinScreenLevels, multi-cells and basic cells are the same.
 *
 * To find the smallest super-cell for a given multi-cell size, i.e., the
 * smallest (absolute value) coordinates where the corners of multi-cells
 * lie on the coordinate axes, we compute the values of i and j that give
 * the minimum value of W by:
 *      D = gcd(abs(M'), abs(N)), i = M'/D, j = N/D, W = C / D,
 * and similarly
 *      D' = gcd(abs(M), abs(N')), i' = N'/D', j' = M/D', W' = C / D'.
 */

/* Compute the derived values of a halftone tile. */
void
gx_compute_cell_values(gx_ht_cell_params_t * phcp)
{
    const int M = phcp->M, N = phcp->N, M1 = phcp->M1, N1 = phcp->N1;
    const uint m = any_abs(M), n = any_abs(N);
    const uint m1 = any_abs(M1), n1 = any_abs(N1);
    const ulong C = phcp->C = (ulong)m * m1 + (ulong)n * n1;
    const int D = phcp->D = igcd(m1, n);
    const int D1 = phcp->D1 = igcd(m, n1);

    phcp->W = C / D, phcp->W1 = C / D1;
    /* Compute the shift value. */
    /* If M1 or N is zero, the shift is zero. */
    if (M1 && N) {
        int h = 0, k = 0, dy = 0;
        int shift;

        /*
         * There may be a faster way to do this: see Knuth vol. 2,
         * section 4.5.2, Algorithm X (p. 302) and exercise 15
         * (p. 315, solution p. 523).
         */
        while (dy != D)
            if (dy > D) {
                if (M1 > 0)
                    ++k;
                else
                    --k;
                dy -= m1;
            } else {
                if (N > 0)
                    ++h;
                else
                    --h;
                dy += n;
            }
        shift = h * M + k * N1;
        /* We just computed what amounts to a right shift; */
        /* what we want is a left shift. */
        phcp->S = imod(-shift, phcp->W);
    } else
        phcp->S = 0;
    if_debug12('h', "[h]MNR=(%d,%d)/%d, M'N'R'=(%d,%d)/%d => C=%lu, D=%d, D'=%d, W=%u, W'=%u, S=%d\n",
               M, N, phcp->R, M1, N1, phcp->R1,
               C, D, D1, phcp->W, phcp->W1, phcp->S);
}

/* Forward references */
private int pick_cell_size(gs_screen_halftone * ph,
     const gs_matrix * pmat, ulong max_size, uint min_levels, bool accurate,
			   gx_ht_cell_params_t * phcp);

/* Allocate a screen enumerator. */
gs_screen_enum *
gs_screen_enum_alloc(gs_memory_t * mem, client_name_t cname)
{
    return gs_alloc_struct(mem, gs_screen_enum, &st_gs_screen_enum, cname);
}

/* Set up for halftone sampling. */
int
gs_screen_init(gs_screen_enum * penum, gs_state * pgs,
               gs_screen_halftone * phsp)
{
    return gs_screen_init_accurate(penum, pgs, phsp,
                                   screen_accurate_screens);
}
int
gs_screen_init_memory(gs_screen_enum * penum, gs_state * pgs,
                gs_screen_halftone * phsp, bool accurate, gs_memory_t * mem)
{
    int code =
    gs_screen_order_init_memory(&penum->order, pgs, phsp, accurate, mem);

    if (code < 0)
        return code;
    return
        gs_screen_enum_init_memory(penum, &penum->order, pgs, phsp, mem);
}

/* Allocate and initialize a spot screen. */
/* This is the first half of gs_screen_init_accurate. */
int
gs_screen_order_alloc(gx_ht_order *porder, gs_memory_t *mem)
{
    uint num_levels = porder->params.W * porder->params.D;
    int code;

    if (!FORCE_STRIP_HALFTONES &&
        ((ulong)porder->params.W1 * bitmap_raster(porder->params.W) +
           num_levels * sizeof(*porder->levels) +
           porder->params.W * porder->params.W1 * sizeof(gx_ht_bit)) <=
        porder->screen_params.max_size) {
        /*
         * Allocate an order for the entire tile, but only sample one
         * strip.  Note that this causes the order parameters to be
         * self-inconsistent until gx_ht_construct_spot_order fixes them
         * up: see gxdht.h for more information.
         */
        code = gx_ht_alloc_order(porder, porder->params.W,
                                 porder->params.W1, 0,
                                 num_levels, mem);
        porder->height = porder->orig_height = porder->params.D;
        porder->shift = porder->orig_shift = porder->params.S;
    } else {
        /* Just allocate the order for a single strip. */
        code = gx_ht_alloc_order(porder, porder->params.W,
                                 porder->params.D, porder->params.S,
                                 num_levels, mem);
    }
    return code;
}
int
gs_screen_order_init_memory(gx_ht_order * porder, const gs_state * pgs,
                            gs_screen_halftone * phsp, bool accurate,
                            gs_memory_t * mem)
{
    gs_matrix imat;
    ulong max_size = max_tile_cache_bytes;
    int code;

    if (phsp->frequency < 0.1)
        return_error(gs_error_rangecheck);
    gs_deviceinitialmatrix(gs_currentdevice(pgs), &imat);
    code = pick_cell_size(phsp, &imat, max_size,
                          screen_min_screen_levels, accurate,
                          &porder->params);
    if (code < 0)
        return code;
    gx_compute_cell_values(&porder->params);
    porder->screen_params.matrix = imat;
    porder->screen_params.max_size = max_size;
    return gs_screen_order_alloc(porder, mem);
}


/*
 * Select a set of screen cell dimensions that "most closely" approximates
 * a requested frequency and angle, within the restriction of a minimum
 * number of levels and a maximum cell size.
 *
 * In all cases, begin with the "ideal" (infinite resolution) cell, and 
 * find the actual cell (integral pixel dimensions) which "most closely"
 * approximates this cell. If this cell provides the required number of
 * levels and the AccurateScreens flag is not set, use this cell.
 * Otherwise, select the next larger multiple of the ideal cell in both
 * dimensions and try again. Repeat this process until the maximum cell
 * size is reached.
 *
 * In prior versions of this code, the metric used to determine the 
 * "best" approximation was the product of the frequency variation (as a
 * fraction of the desired frequency) and angle variation (in degrees).
 * Aside from the arbitrary combination of units, this metric ignores
 * frequency variation entirely if the angle variation is 0, as is often
 * the case if a 45 degree angle is requested (angle variation is also
 * ignored if the frequency variation is 0, but this case does not often
 * arise). This metric also compares what can be achieved in a given
 * cell size with an ideal that can probably not be achieved in any cell
 * size.
 *
 * A more relevant metric is how close the approximation in a given cell
 * size approaches what can be achieved with the maximum possible cell.
 * If an m x n pixel cell produces the same result as would be achieved
 * by a cell that covers the entire page (as is achieved by well-tempered
 * screening, for example), then the m x n pixel cell is perfect even if
 * it does not exactly match the specified angle and frequency.
 *
 * This suggests that the "closeness of fit" of a screen cell be
 * determined by replicating the cell as often as necessary to fill the
 * page, then measuring the worst-case displacement of a given cell pixel
 * from the location it would have occupied if the screen cell had been
 * enlarged to enclose the entire page. It is not possible to make this
 * calculation without knowledge of the full page size, which we do not
 * have. However, a good proxy for the described metric is the distance
 * variation divided by the characteristic cell dimension (more on this
 * below).
 *
 * The following discussion describes the theory behind the calculation
 * of the screen cell parameters.
 *
 * "Screen space" is the coordinate system in which the user describes
 * the screen whitening order. This whitening order may described by a
 * threshold function v0:RxR ==> (0, 1], where v0(x, y) identifies the
 * lowest intensity for which the point (x, y) should be "white". The
 * interpretation of "white" is based on the polarity of the color model:
 * for additive color models it refers to the "on" value, for subtractive
 * it is the "off" value. For any discretization into pixels, the value
 * to be assigned to a pixel is the value v0 assigns to the centerpoint
 * of that pixel.
 *
 * v0 is a symmetric, two-dimensional, periodic function: for any (x, y)
 * in screen space, 
 *
 *      v0(x, y) = v0(x + 2, y) = v0(x, y + 2)
 *
 * The convention used by PostScript is that the region (-1, 1] x (-1, 1]
 * is the representative period that a screen order procedure must
 * provide (there is some debate as to which endpoints are considered
 * inside the interval).
 *
 * The user-provided spot function defines v0, but is not v0 itself.
 * Rather, the spot function defines an order among points by assigning
 * them values in [-1, 1]. Pixels are "whitened" in the order of their
 * values (lowest to highest), with those assigned -1 whitened first
 * and those assigned 1 whitened last (the order of whitening amoung
 * points with equal value is undefined and implementation specific).
 *
 * The screen angle and frequency parameters specify the mapping from
 * screen space to the "modified initial default user space" ("MIDUS").
 * The MIDUS is a bit of a strange beast, that has the following
 * properties:
 *
 *   a. The space is defined solely by the system hardware resolution
 *      and physical page orientation; it is independent of all
 *      interpreter-level manipulation. In particular, changing the
 *      default transformation matrix via the Install procedure of
 *      the PostScript page device dictionary does not alter this
 *      space.
 *
 *   b. The units of the space are points (1/72 of an inch). (This is a
 *      Ghostscript convention; since the space is not user accessible
 *      its units of concern only to this code.)
 *
 *   c. The x-axis of the space aligns with that of the initial default
 *      user space (that defined by the transformation returned by the
 *      current device's get_initial_transformation method).
 *
 *   d. The y-axis is oriented in the opposite direction of the the
 *      default user space y-axis (i.e.: MIDUS is a left-handed space).
 *      This rather curious convention follows that used by Adobe's
 *      implementations. It most likely derives from the earliest
 *      PostScript implementations, when the screen space was defined
 *      directly in terms of device space (all of the early PostScript,
 *      and essentially all current devices, have left-handed device
 *      coordinate systems). The convention is maintained for consistency
 *      with those earlier systems.
 *
 * It is no less correct, and more consistent with the documentation,
 * to let angle and frequency define screen space directly with respect
 * to the device coordinate space. Under such an arrangement, "landscape"
 * devices (those for which the fast scan direction is aligned with the
 * page height) will produce visually different output than portrait
 * devices. The arrangement described above will cause all devices to
 * match the output of a traditional "portrait" device. While both
 * arrangements are correct, the latter will likely result in fewer 
 * customer support calls.
 *
 * The transformation from MIDUS to screen space is:
 *
 *             -                                 -
 *    T01  =  | 36 * cos(a) / f   36 * sin(a) / f |
 *            | -36 * sin(a) / f  36 * cos(a) / f |
 *             -                                 -
 *
 * where f is the frequency, a the angle, and we ignore halftone phase.
 * The inverse of this transformation is:
 *
 *             -                                  -
 *    T10  =  | f * cos(a) / 36   -f * sin(a) / 36 |
 *            | f * sin(a) / 36    f * cos(a) / 36 |
 *             -                                  -
 * Hence the function v1 in default user space that corresponds to v0 may
 * be defined by:
 *
 *    v1(x, y) = v0((x, y) * T10)
 *             = v0( (x * cos(a) + y * sin(a)) * f / 36,
 *                   (-x * sin(a) + y * cos(a)) * f / 36 )
 *
 * T10 is a linear transformation, so:
 *
 *    v1(x + x1, y + y1) = v0((x + x1, y + y1) * T10)
 *                       = v0((x, y) * T10 + (x1, y1) * T10)
 *
 * Set (x1, y1) = (2, 0) * T01 and (x2, y2) = (0, 2) * T01. Then
 *
 *    v1(x + x1, y + y1) = v0((x, y) * T10 + (x1, y1) * T10)
 *                       = v0((x, y) * T10 + (2, 0) * T01 * T10)
 *                       = v0((x, y) * T10) = v1(x, y)
 *
 * and similarly
 *
 *    v1(x + x2, y + y2) = v0((x, y) * T10 + (0, 2) * T01 * T10)
 *                       = v1(x, y)
 *
 * In this way the periodicity constraints/periodicity vectors of v1 can
 * be derived from those of v0. This can also be expressed directly as:
 *
 *    v1(x, y) = v1( x + 72 * cos(a) / f, y + 72 * sin(a) / f )
 *             = v1( x - 72 * sin(a) / f, y + 72 * cos(a) / f )
 *
 * Let T12 be the transformation from the MIDUS to device space. This is
 * the non-translation part of *pmat, modified to flip the y-axis. Let
 * T21 be its inverse (we require that T12 not be singular). Then v2, the
 * function in device space corresponding to v1, can be defined by:
 *
 *    v2(x, y) = v1((x, y) * T21) = v0((x, y) * T21 * T10)
 *
 * As above, the periodicity vectors for this function are
 *
 *    (2, 0) * T01 * T12   and  (0, 2) * T01 * T12
 *
 * In practice, T12 is always diagonal, so
 *           -                  -              -                  -
 *    T12 = | pmat->xx      0    |  or  T12 = |    0      pmat->xy |
 *          |    0     -pmat->yy |            | -pmat->yx     0    |
 *           -                  -              -                  -
 *
 * where the first case is considered "portrait" and the second 
 * "landscape". So, for the two cases
 *
 *           -                    -              -                     -
 *    T21 = | 1 / pmat->xx    0    |  or  T21 = |    0    -1 / pmat->yx |
 *          |    0   -1 / pmat->yy |            | 1 / pmat->xy    0     |
 *           -                    -              -                     -
 *
 * So for the portrait case,
 *
 *  v2(x, y) = v1(x / pmat->xx, -y / pmat->yy)
 *           = v0( (x * cos(a) / pmat->xx - y * sin(a) / pmat->yy) * f / 36,
 *                 (-x * sin(a) / pmat->xx + y * cos(a) / pmat->yy) * f / 36 )
 *
 * and for the landscape case:
 *
 *  v2(x, y) = v1(y / pmat->xy, -x / pmat->yx)
 *           = v0( (y * cos(a) / pmat->xy + x * sin(a) / pmat->yx) * f / 36,
 *                 (-y * sin(a) / pmat->xy + x * cos(a) / pmat->yx) * f / 36 )
 *
 * For the portrait case, the periodicity constraints are:
 *
 *  v2(x, y) = v2( x + 72 * pmat->xx * cos(a) / f,
 *                 y - 72 * pmat->yy * sin(a) / f )
 *           = v2( x - 72 * pmat->xx * sin(a) / f,
 *                 y + 72 * pmat->yy * cos(a) / f )
 *
 * while for the landscape case they are:
 *
 *  v2(x, y) = v2( x - 72 * pmat->yx * sin(a) / f, 
 *                 y + 72 * pmat->xy * cos(a) / f )
 *           = v2( x + 72 * pmat->yx * cos(a) / f,
 *                 y + 72 * pmat->xy * sin(a) / f )
 *
 * The pair of periodicity vectors given above define a parallelogram that
 * is the "ideal" screen cell in device space. They are perpendicular only
 * if T12 is angle-preserving (mathematicians call such transformations
 * "orthogonal", but in computer graphics that term is often used to denote
 * diagonal transformations). To form a super cell it is necessary to convert
 * the vector endpoints to integers. This can be done individually for each
 * vector, though care must be taken to round values either uniformly
 * towards 0 or uniformly away from 0 (otherwise vectors that are
 * perpendicular in their ideal form may no longer be perpendicular when
 * rounded).
 *
 * The actual angle and actual frequency can be found by converting the
 * rounded vectors back into screen space, and comparing the result against
 * the original vectors. The angle of the resulting vector from horizontal
 * (resp. vertical) is the difference between the requested and actual
 * angle, while the ratio of the length of the vector to 2 is the ratio of
 * actual to requested frequency. The two vectors may give different values
 * for these parameters; by convention, those for the (2, 0) vector are
 * used (in practice, the difference between values given by the two
 * vectors are small).
 *
 * This procedure reads (but does not alter) ph->frequency and ph->angle,
 * and sets ph->actual_frequency and ph->actual_angle, as well as *phcp.
 */



private int
pick_cell_size(
    gs_screen_halftone *    ph,
    const gs_matrix *       pmat,
    ulong                   max_size,
    uint                    min_levels,
    bool                    accurate,
    gx_ht_cell_params_t *   phcp )
{
    gs_matrix               T01, T12, T02;
    double                  abs_det_T02;
    gs_point                uv0, uv1;   /* vectors, not points */
    int                     rt, best_rt = -1;
    double                  d, best_var = 4.0, d_a;

    /* form the screen space to MIDUS transformation */
    gs_make_rotation(ph->angle, &T01);
    gs_matrix_scale(&T01, 72.0 / ph->frequency, 72.0 / ph->frequency, &T01);

    /* and the screen space to device space transformation */
    gs_matrix_scale(pmat, 1, -1, &T12);
    gs_matrix_multiply(&T01, &T12, &T02);
    if ((abs_det_T02 = fabs(T02.xx * T02.yy - T02.xy * T02.yx)) == 0)
        return_error(gs_error_rangecheck);

    /* from which we can form the ideal periodicity vectors */
    gs_distance_transform(1.0, 0.0, &T02, &uv0);
    gs_distance_transform(0.0, 1.0, &T02, &uv1);
    d = 2 * hypot(uv0.x, uv0.y);

    if_debug12( 'h',
                "[h]Requested: f=%g a=%g mat=[%g %g %g %g]"
                " max_size=%lu min_levels=%u =>\n"
                "     u0=%g v0=%g, u1=%g v1=%g\n",
                ph->frequency, ph->angle,
                T12.xx, T12.xy, T12.yx, T12.yy,
                max_size, min_levels,
                uv0.x, uv0.y, uv1.x, uv1.y );

    /*
     * Set a lower bound for rt (det_T02 is the area of the ideal 
     * parallelogram).
     */
    if (min_levels < 4)
        min_levels = 4;
    rt = (int)ceil(min_levels / abs_det_T02);

    /*
     * Starting from the minimum rt value, attempt to form increasingly
     * large multiple of the screen size, until and exact fit is found
     * (unlikely unless the requested angle is 0) or the maximum cell
     * size is reached. Any accepted arrangment must support at least the
     * minimum number of levels.
     */
    for (;; ++rt) {
        gx_ht_cell_params_t p;
        double              x0, y0, x1, y1, ix0, iy0, ix1, iy1, var;
        long                wt, raster, wt_size;

        /*
         * discretize and calculate the (relative) variance. Note that
         * all round is away from 0.
         */
        x0 = uv0.x * rt;
        y0 = uv0.y * rt;
        x1 = uv1.x * rt;
        y1 = uv1.y * rt;
        ix0 = (x0 < 0 ? ceil(x0 - 0.5) : floor(x0 + 0.5));
        iy0 = (y0 < 0 ? ceil(y0 - 0.5) : floor(y0 + 0.5));
        ix1 = (x1 < 0 ? ceil(x1 - 0.5) : floor(x1 + 0.5));
        iy1 = (y1 < 0 ? ceil(y1 - 0.5) : floor(y1 + 0.5));
        if_debug3('h', "[h]trying m=%d, n=%d, r=%d\n", (int)ix0, (int)iy0, rt);
        var = (hypot(x0 - ix0, y0 - iy0) + hypot(x1 - ix1, y1 - iy1)) / rt;
        if (var >= best_var)
            continue;

        /*
         * Calculate the screen parameters.
         *
         * Note that p.M and p.M1 (respectively p.N and p.N1) refer to
         * different device coordinate axes, and M1 always has the same
         * sign as M and N1 has the same sign as N. These somewhat peculiar
         * arrangments are used for consistency with other code that
         * manipulates the gx_ht_clee_params_t structure.
         */
        p.R = rt;
        p.R1 = rt;  /* unused */
        p.M = ix0;
        p.N = iy0;
        p.M1 = (ix0 >= 0 ? any_abs(iy1) : -any_abs(iy1));
        p.N1 = (iy0 >= 0 ? any_abs(ix1) : -any_abs(ix1));
        gx_compute_cell_values(&p);
        if ((wt = p.W) >= max_short)
            continue;

        /* check levels against min_levels and strip size against max_size */
        if ((raster = bitmap_raster(wt)) * p.D > max_size)
            break;
        if (raster > max_long / wt || p.C < min_levels)
            continue;

        /* this combination looks OK; record it */
        *phcp = p;
        best_rt = rt;
        best_var = var;
        wt_size = raster * wt;
        if_debug2('h', "*** best wt_size=%ld, var=%g\n", wt_size, var / d);

        /* quit now if not using accurate screens, or within 1% of correct */
        if (!accurate || var / d <= .01)
            break;
    }

    /* see if an acceptable cell was found */
    if (best_rt == -1)
        return_error(gs_error_limitcheck);

    /* determine the actual angle and frequency */
    gs_distance_transform_inverse( (double)phcp->M / (double)rt,
                                   (double)phcp->N / (double)rt,
                                   &T02,
                                   &uv0 );
    ph->actual_frequency = ph->frequency / hypot(uv0.x, uv0.y);
    d_a = atan2(uv0.y, uv0.x);
    ph->actual_angle = ph->angle + d_a * radians_to_degrees;
    if_debug5( 'h', "[h]Chosen: f=%g a=%g M=%d N=%d R=%d\n",
               ph->actual_frequency, ph->actual_angle,
               phcp->M, phcp->N, phcp->R );

    return 0;
}

/* Prepare to sample a spot screen. */
/* This is the second half of gs_screen_init_accurate. */
int
gs_screen_enum_init_memory(gs_screen_enum * penum, const gx_ht_order * porder,
                           gs_state * pgs, const gs_screen_halftone * phsp,
                           gs_memory_t * mem)
{
    penum->pgs = pgs;           /* ensure clean for GC */
    penum->order = *porder;
    penum->halftone.rc.memory = mem;
    penum->halftone.type = ht_type_screen;
    penum->halftone.params.screen = *phsp;
    penum->x = penum->y = 0;

    if (porder->wse == NULL) {
	penum->strip = porder->num_levels / porder->width;
	penum->shift = porder->shift;
	/*
	 * We want a transformation matrix that maps the parallelogram
	 * (0,0), (U,V), (U-V',V+U'), (-V',U') to the square (+/-1, +/-1).
	 * If the coefficients are [a b c d e f] and we let
	 *      u = U = M/R, v = V = N/R,
	 *      r = -V' = -N'/R', s = U' = M'/R',
	 * then we just need to solve the equations:
	 *      a*0 + c*0 + e = -1      b*0 + d*0 + f = -1
	 *      a*u + c*v + e = 1       b*u + d*v + f = 1
	 *      a*r + c*s + e = -1      b*r + d*s + f = 1
	 * This has the following solution:
	 *      Q = 2 / (M*M' + N*N')
	 *      a = Q * R * M'
	 *      b = -Q * R' * N
	 *      c = Q * R * N'
	 *      d = Q * R' * M
	 *      e = -1
	 *      f = -1
	 */
	{
	    const int M = porder->params.M, N = porder->params.N, R = porder->params.R;
	    const int M1 = porder->params.M1, N1 = porder->params.N1, R1 = porder->params.R1;
	    double Q = 2.0 / ((long)M * M1 + (long)N * N1);

	    penum->mat.xx = Q * (R * M1);
	    penum->mat.xy = Q * (-R1 * N);
	    penum->mat.yx = Q * (R * N1);
	    penum->mat.yy = Q * (R1 * M);
	    penum->mat.tx = -1.0;
	    penum->mat.ty = -1.0;
	    gs_matrix_invert(&penum->mat, &penum->mat_inv);
	}
	if_debug7('h', "[h]Screen: (%dx%d)/%d [%f %f %f %f]\n",
		  porder->width, porder->height, porder->params.R,
		  penum->mat.xx, penum->mat.xy,
		  penum->mat.yx, penum->mat.yy);
    }
    return 0;
}

/* Report current point for sampling */
int
gs_screen_currentpoint(gs_screen_enum * penum, gs_point * ppt)
{
    gs_point pt;
    int code;
    double sx, sy; /* spot center in spot coords (integers) */
    gs_point spot_center; /* device coords */

    if (penum->order.wse) {
	int code;
	code = gs_wts_screen_enum_currentpoint(penum->order.wse, ppt);
	if (code > 0) {
	    wts_sort_blue(penum->order.wse);
	}
	return code;
    }

    if (penum->y >= penum->strip) {     /* all done */
        gx_ht_construct_spot_order(&penum->order);
        return 1;
    }
    /* We displace the sampled coordinates very slightly */
    /* in order to reduce the likely number of points */
    /* for which the spot function returns the same value. */
    if ((code = gs_point_transform(penum->x + 0.501, penum->y + 0.498, &penum->mat, &pt)) < 0)
        return code;

    /* find the spot center in device coords : */
    sx = ceil( pt.x / 2 ) * 2;
    sy = ceil( pt.y / 2 ) * 2;
    if ((code = gs_point_transform(sx, sy, &penum->mat_inv, &spot_center)) < 0)
        return code;

    /* shift the spot center to nearest pixel center : */
    spot_center.x = floor(spot_center.x) + 0.5;
    spot_center.y = floor(spot_center.y) + 0.5;

    /* compute the spot function arguments for the shifted spot : */
    if ((code = gs_distance_transform(penum->x - spot_center.x + 0.501, 
                                      penum->y - spot_center.y + 0.498,
                                      &penum->mat, &pt)) < 0)
        return code;
    pt.x += 1;
    pt.y += 1;

    if (pt.x < -1.0)
        pt.x += ((int)(-ceil(pt.x)) + 1) & ~1;
    else if (pt.x >= 1.0)
        pt.x -= ((int)pt.x + 1) & ~1;
    if (pt.y < -1.0)
        pt.y += ((int)(-ceil(pt.y)) + 1) & ~1;
    else if (pt.y >= 1.0)
        pt.y -= ((int)pt.y + 1) & ~1;
    *ppt = pt;
    return 0;
}

/* Record next halftone sample */
int
gs_screen_next(gs_screen_enum * penum, floatp value)
{
    if (penum->order.wse) {
	return gs_wts_screen_enum_next (penum->order.wse, value);
    } else {
	ht_sample_t sample;
	int width = penum->order.width;
	gx_ht_bit *bits = (gx_ht_bit *)penum->order.bit_data;

	if (value < -1.0 || value > 1.0)
	    return_error(gs_error_rangecheck);
	sample = (ht_sample_t) ((value + 1) * max_ht_sample);
#ifdef DEBUG
	if (gs_debug_c('H')) {
	    gs_point pt;

	    gs_screen_currentpoint(penum, &pt);
	    dlprintf6("[H]sample x=%d y=%d (%f,%f): %f -> %u\n",
		      penum->x, penum->y, pt.x, pt.y, value, sample);
	}
#endif
	bits[penum->y * width + penum->x].mask = sample;
	if (++(penum->x) >= width)
	    penum->x = 0, ++(penum->y);
	return 0;
    }
}

/* Install a fully constructed screen in the gstate. */
int
gs_screen_install(gs_screen_enum * penum)
{
    gx_device_halftone dev_ht;
    int code;

    dev_ht.rc.memory = penum->halftone.rc.memory;
    dev_ht.order = penum->order;
    dev_ht.components = 0;
    if ((code = gx_ht_install(penum->pgs, &penum->halftone, &dev_ht)) < 0)
        gx_device_halftone_release(&dev_ht, dev_ht.rc.memory);
    return code;
}
