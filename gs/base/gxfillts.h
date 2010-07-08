/* Copyright (C) 2001-2006 Artifex Software, Inc.
   All Rights Reserved.

   This software is provided AS-IS with no warranty, either express or
   implied.

   This software is distributed under license and may not be copied, modified
   or distributed except as expressly authorized under the terms of that
   license.  Refer to licensing information at http://www.artifex.com/
   or contact Artifex Software, Inc.,  7 Mt. Lassen Drive - Suite A-134,
   San Rafael, CA  94903, U.S.A., +1(415)492-9861, for further information.
*/

/* $Id$ */
/* Configurable algorithm for filling a slanted trapezoid. */

/*
 * Since we need several statically defined variants of this agorithm,
 * we store it in .h file and include it several times into gxfill.c .
 * Configuration macros (template arguments) are :
 *
 *  FILL_DIRECT - See LOOP_FILL_RECTANGLE_DIRECT.
 *  TEMPLATE_slant_into_trapezoids - the name of the procedure to generate.
 *  TRY_TO_EXTEND_TRAP - whether we do some extra maths to see if we can
 *  extend a trapezoid to reduce the total number produced.
 */

static inline int
TEMPLATE_slant_into_trapezoids (const line_list *ll,
        const active_line *flp, const active_line *alp, fixed y, fixed y1)
{
    /*
     * We want to get the effect of filling an area whose
     * outline is formed by dragging a square of side adj2
     * along the border of the trapezoid.  This is *not*
     * equivalent to simply expanding the corners by
     * adjust: There are 3 cases needing different
     * algorithms, plus rectangles as a fast special case.
     */
    const fill_options * const fo = ll->fo;
    gs_fixed_edge le, re;
    int code = 0;
    /*
     * Define a faster test for
     *  fixed2int_pixround(y - below) != fixed2int_pixround(y + above)
     * where we know
     *  0 <= below <= _fixed_pixround_v,
     *  0 <= above <= min(fixed_half, fixed_1 - below).
     * Subtracting out the integer parts, this is equivalent to
     *  fixed2int_pixround(fixed_fraction(y) - below) !=
     *    fixed2int_pixround(fixed_fraction(y) + above)
     * or to
     *  fixed2int(fixed_fraction(y) + _fixed_pixround_v - below) !=
     *    fixed2int(fixed_fraction(y) + _fixed_pixround_v + above)
     * Letting A = _fixed_pixround_v - below and B = _fixed_pixround_v + above,
     * we can rewrite this as
     *  fixed2int(fixed_fraction(y) + A) != fixed2int(fixed_fraction(y) + B)
     * Because of the range constraints given above, this is true precisely when
     *  fixed_fraction(y) + A < fixed_1 && fixed_fraction(y) + B >= fixed_1
     * or equivalently
     *  fixed_fraction(y + B) < B - A.
     * i.e.
     *  fixed_fraction(y + _fixed_pixround_v + above) < below + above
     */
    fixed y_span_delta = _fixed_pixround_v + fo->adjust_above;
    fixed y_span_limit = fo->adjust_below + fo->adjust_above;
    le.start.x = flp->start.x - fo->adjust_left;
    le.end.x = flp->end.x - fo->adjust_left;
    re.start.x = alp->start.x + fo->adjust_right;
    re.end.x = alp->end.x + fo->adjust_right;

#define ADJUSTED_Y_SPANS_PIXEL(y)\
  (fixed_fraction((y) + y_span_delta) < y_span_limit)

    if ((le.end.x == le.start.x) && (re.end.x == re.start.x)) {
        /* Rectangle. */
        le.start.x = fixed2int_pixround(le.start.x);
        le.start.y = fixed2int_pixround(y - fo->adjust_below);
        re.start.x = fixed2int_pixround(re.start.x);
        re.start.y = fixed2int_pixround(y1 + fo->adjust_above);
        return LOOP_FILL_RECTANGLE_DIRECT(fo, le.start.x, le.start.y,
                               re.start.x-le.start.x, re.start.y-le.start.y);
    } else if ((le.end.x <= le.start.x) && (re.end.x >= re.start.x)) {
        /* Top wider than bottom. */
        le.start.y = flp->start.y - fo->adjust_below;
        le.end.y   = flp->end.y - fo->adjust_below;
        re.start.y = alp->start.y - fo->adjust_below;
        re.end.y   = alp->end.y - fo->adjust_below;

        if (ADJUSTED_Y_SPANS_PIXEL(y1)) {
            /* Strictly speaking the area we want to fill is a rectangle sat
             * atop a trapezoid. For some platforms, it would be cheaper to
             * just emit a single trap if we can get away with it. */
            int xli = fixed2int_var_pixround(flp->x_next - fo->adjust_left);
            int xri = fixed2int_var_pixround(alp->x_next + fo->adjust_right);

            if (TRY_TO_EXTEND_TRAP) {
                int newxl, newxr;

                /* Find the two X extents at the new top position */
                newxl = le.start.x - (fixed)
                             (((int64_t)(le.start.x-le.end.x))*
                              ((int64_t)(y1 + fo->adjust_above-le.start.y))+
                              (le.end.y-le.start.y-1))/(le.end.y-le.start.y);
                newxr = re.start.x + (fixed)
                             (((int64_t)(re.end.x-re.start.x))*
                              ((int64_t)(y1 + fo->adjust_above-re.start.y))+
                              (re.end.y-re.start.y-1))/(re.end.y-re.start.y);

                /* If extending the trap covers the same pixels as the
                 * rectangle would have done, then just plot it! */
                if ((fixed2int_var_pixround(newxl) == xli) &&
                    (fixed2int_var_pixround(newxr) == xri)) {
                    gs_fixed_edge newle, newre;

                    newle.start.x = le.start.x;
                    newle.start.y = le.start.y;
                    newle.end.x   = le.end.x;
                    newle.end.y   = le.end.y;
                    while (newle.end.y < y1 + fo->adjust_above) {
                        newle.end.x -= le.start.x-le.end.x;
                        newle.end.y += le.end.y-le.start.y;
                    }

                    newre.start.x = re.start.x;
                    newre.start.y = re.start.y;
                    newre.end.x   = re.end.x;
                    newre.end.y   = re.end.y;
                    while (newre.end.y < y1 + fo->adjust_above) {
                        newre.end.x += re.end.x-re.start.x;
                        newre.end.y += re.end.y-re.start.y;
                    }

                    return loop_fill_trap_np(ll, &newle, &newre,
                                y - fo->adjust_below, y1 + fo->adjust_above);
                }
            }
            /* Otherwise we'll emit the rectangle and then the trap */
            INCR(afill);
            code = LOOP_FILL_RECTANGLE_DIRECT(fo,
                     xli, fixed2int_pixround(y1 - fo->adjust_below),
                     xri - xli, 1);
            vd_rect(flp->x_next - fo->adjust_left, y1 - fo->adjust_below,
                    alp->x_next + fo->adjust_right, y1, 1, VD_TRAP_COLOR);
            if (code < 0)
                return code;
        }
        return loop_fill_trap_np(ll, &le, &re, y - fo->adjust_below, y1 - fo->adjust_below);
    } else if ((le.end.x >= le.start.x) && (re.end.x <= re.start.x)) {
        /* Bottom wider than top. */
        le.start.y = flp->start.y + fo->adjust_above;
        le.end.y   = flp->end.y + fo->adjust_above;
        re.start.y = alp->start.y + fo->adjust_above;
        re.end.y   = alp->end.y + fo->adjust_above;

        if (ADJUSTED_Y_SPANS_PIXEL(y)) {
            /* Strictly speaking the area we want to fill is a trapezoid sat
             * atop a rectangle. For some platforms, it would be cheaper to
             * just emit a single trap if we can get away with it. */
            int xli = fixed2int_var_pixround(flp->x_current - fo->adjust_left);
            int xri = fixed2int_var_pixround(alp->x_current + fo->adjust_right);

            if (TRY_TO_EXTEND_TRAP) {
                int newxl, newxr;

                /* Find the two X extents at the new bottom position */
                newxl = le.end.x - (fixed)
                        (((int64_t)(le.end.x-le.start.x))*
                         ((int64_t)(le.end.y-(y - fo->adjust_below)))+
                         (le.end.y-le.start.y+1))/(le.end.y-le.start.y);
                newxr = re.end.x + (fixed)
                        (((int64_t)(re.start.x-re.end.x))*
                         ((int64_t)(re.end.y-(y - fo->adjust_below)))+
                         (re.end.y-re.start.y+1))/(re.end.y-re.start.y);

                /* If extending the trap covers the same pixels as the
                 * rectangle would have done, then just plot it! */
                if ((fixed2int_var_pixround(newxl) == xli) &&
                    (fixed2int_var_pixround(newxr) == xri)) {
                    gs_fixed_edge newle, newre;

                    newle.start.x = le.start.x;
                    newle.start.y = le.start.y;
                    newle.end.x   = le.end.x;
                    newle.end.y   = le.end.y;
                    while (newle.start.y > y - fo->adjust_below) {
                        newle.start.x -= le.end.x-le.start.x;
                        newle.start.y -= le.end.y-le.start.y;
                    }

                    newre.start.x = re.start.x;
                    newre.start.y = re.start.y;
                    newre.end.x   = re.end.x;
                    newre.end.y   = re.end.y;
                    while (newre.start.y > y - fo->adjust_below) {
                        newre.start.x += re.start.x-re.end.x;
                        newre.start.y -= re.end.y-re.start.y;
                    }

                    return loop_fill_trap_np(ll, &newle, &newre,
                                y - fo->adjust_below, y1 + fo->adjust_above);
                }
            }
            /* Otherwise we'll emit the rectangle and then the trap */
            INCR(afill);
            code = LOOP_FILL_RECTANGLE_DIRECT(fo,
                     xli, fixed2int_pixround(y - fo->adjust_below),
                     xri - xli, 1);
            vd_rect(flp->x_current - fo->adjust_left, y - fo->adjust_below,
                    alp->x_current + fo->adjust_right, y, 1, VD_TRAP_COLOR);
            if (code < 0)
                return code;
        }
        return loop_fill_trap_np(ll, &le, &re, y + fo->adjust_above, y1 + fo->adjust_above);
    }
    /* Otherwise, handle it as a slanted trapezoid. */
    return fill_slant_adjust(ll, flp, alp, y, y1);
}

