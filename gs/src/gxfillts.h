/* Copyright (C) 2002 artofcode LLC. All rights reserved.
  
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
/* Configurable algorithm for filling a slanted trapezoid. */

/*
 * Since we need several statically defined variants of this agorithm,
 * we store it in .h file and include it several times into gxfill.c .
 * Configuration macros (template arguments) are :
 * 
 *  FILL_DIRECT - See LOOP_FILL_RECTANGLE_DIRECT.
 *  FILL_TRAP_SLANTED_NAME - the name of the procedure to generate.
*/


private int
FILL_TRAP_SLANTED_NAME (const line_list *ll, gs_fixed_edge *le, gs_fixed_edge *re)
{
    /*
     * We want to get the effect of filling an area whose
     * outline is formed by dragging a square of side adj2
     * along the border of the trapezoid.  This is *not*
     * equivalent to simply expanding the corners by
     * adjust: There are 3 cases needing different
     * algorithms, plus rectangles as a fast special case.
     */
    int xli = fixed2int_var_pixround(le->end.x);
    int code = 0;
    const fill_options * const fo = ll->fo;
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
    fixed y_span_delta = _fixed_pixround_v + fo->adjust_above;
    fixed y_span_limit = fo->adjust_below + fo->adjust_above;

#define ADJUSTED_Y_SPANS_PIXEL(y)\
  (fixed_fraction((y) + y_span_delta) < y_span_limit)

    if (le->end.x <= le->start.x) {
	if (re->end.x >= re->start.x) {	/* Top wider than bottom. */
	    le->start.y = re->start.y -= fo->adjust_below;
	    le->end.y = re->end.y -= fo->adjust_below;
	    code = loop_fill_trap_np(ll, le, re);
	    le->start.y = re->start.y += fo->adjust_below;
	    le->end.y = re->end.y += fo->adjust_below;
	    if (ADJUSTED_Y_SPANS_PIXEL(le->end.y)) {
		if (code < 0)
		    return code;
		INCR(afill);
		code = LOOP_FILL_RECTANGLE_DIRECT(fo, 
		 xli, fixed2int_pixround(le->end.y - fo->adjust_below),
		     fixed2int_var_pixround(re->end.x) - xli, 1);
		vd_rect(le->end.x, le->end.y - fo->adjust_below, re->end.x, le->end.y, 1, VD_TRAP_COLOR);
	    }
	} else {	/* Slanted trapezoid. */
	    code = fill_slant_adjust(ll, le, re);
	}
    } else {
	if (re->end.x <= re->start.x) {	/* Bottom wider than top. */
	    if (ADJUSTED_Y_SPANS_PIXEL(le->start.y)) {
		INCR(afill);
		xli = fixed2int_var_pixround(le->start.x);
		code = LOOP_FILL_RECTANGLE_DIRECT(fo, 
		  xli, fixed2int_pixround(le->start.y - fo->adjust_below),
		     fixed2int_var_pixround(re->start.x) - xli, 1);
		vd_rect(le->end.x, le->start.y - fo->adjust_below, re->start.x, le->start.y, 1, VD_TRAP_COLOR);
		if (code < 0)
		    return code;
	    }
	    le->start.y = re->start.y += fo->adjust_above;
	    le->end.y = re->end.y += fo->adjust_above;
	    code = loop_fill_trap_np(ll, le, re);
	    le->start.y = re->start.y -= fo->adjust_above;
	    le->end.y = re->end.y -= fo->adjust_above;
	} else {	/* Slanted trapezoid. */
	    code = fill_slant_adjust(ll, le, re);
	}
    }
    return code;
}
