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
/* Configurable algorithm for filling a trapezoid */

/*
 * Since we need 3 statically defined variants of this agorithm,
 * we store it in .h file and include 3 times into gdevddrw.c and 
 * into gxfill.h . Configuration flags (macros) are :
 * 
 *   GX_FILL_TRAPEZOID - a name of method
 *   CONTIGUOUS_FILL   - prevent dropouts in narrow trapezoids
 *   SWAP_AXES         - assume swapped axes
 */

/*
 * Fill a trapezoid.  left.start => left.end and right.start => right.end
 * define the sides; ybot and ytop define the top and bottom.  Requires:
 *      {left,right}->start.y <= ybot <= ytop <= {left,right}->end.y.
 * Lines where left.x >= right.x will not be drawn.  Thanks to Paul Haeberli
 * for an early floating point version of this algorithm.
 */

GX_FILL_TRAPEZOID(gx_device * dev, const gs_fixed_edge * left,
    const gs_fixed_edge * right, fixed ybot, fixed ytop, int flags,
    const gx_device_color * pdevc, gs_logical_operation_t lop)
{
    const fixed ymin = fixed_pixround(ybot) + fixed_half;
    const fixed ymax = fixed_pixround(ytop);

    if (ymin >= ymax)
	return 0;		/* no scan lines to sample */
    {
	int iy = fixed2int_var(ymin);
	const int iy1 = fixed2int_var(ymax);
	trap_line l, r;
	register int rxl, rxr;
	int ry;
	const fixed
	    x0l = left->start.x, x1l = left->end.x, x0r = right->start.x,
	    x1r = right->end.x, dxl = x1l - x0l, dxr = x1r - x0r;
	const fixed	/* partial pixel offset to first line to sample */
	    ysl = ymin - left->start.y, ysr = ymin - right->start.y;
	fixed fxl;
	bool fill_direct = color_writes_pure(pdevc, lop);
	int max_rect_height = 1;  /* max height to do fill as rectangle */
	int code;
#if CONTIGUOUS_FILL
	const bool peak0 = ((flags & 1) != 0);
	const bool peak1 = ((flags & 2) != 0);
	int peak_y0 = ybot + fixed_half;
	int peak_y1 = ytop - fixed_half;
#endif

	if_debug2('z', "[z]y=[%d,%d]\n", iy, iy1);

	l.h = left->end.y - left->start.y;
	r.h = right->end.y - right->start.y;
	l.x = x0l + (fixed_half - fixed_epsilon);
	r.x = x0r + (fixed_half - fixed_epsilon);
	ry = iy;

/*
 * Free variables of FILL_TRAP_RECT:
 *	SWAP_AXES, pdevc, dev, lop
 * Free variables of FILL_TRAP_RECT_DIRECT:
 *	SWAP_AXES, fill_rect, dev, cindex
 */
#if SWAP_AXES
#define FILL_TRAP_RECT(x,y,w,h)\
  (SWAP_AXES ? gx_fill_rectangle_device_rop(y, x, h, w, pdevc, dev, lop) :\
   gx_fill_rectangle_device_rop(x, y, w, h, pdevc, dev, lop))
#define FILL_TRAP_RECT_DIRECT(x,y,w,h)\
  (SWAP_AXES ? (*fill_rect)(dev, y, x, h, w, cindex) :\
   (*fill_rect)(dev, x, y, w, h, cindex))
#else
#define FILL_TRAP_RECT(x,y,w,h)\
   gx_fill_rectangle_device_rop(x, y, w, h, pdevc, dev, lop)
#define FILL_TRAP_RECT_DIRECT(x,y,w,h)\
   (*fill_rect)(dev, x, y, w, h, cindex)
#endif

#define VD_RECT_SWAPPED(rxl, ry, rxr, iy)\
    vd_rect(int2fixed(SWAP_AXES ? ry : rxl), int2fixed(SWAP_AXES ? rxl : ry),\
            int2fixed(SWAP_AXES ? iy : rxr), int2fixed(SWAP_AXES ? rxr : iy),\
	    1, VD_RECT_COLOR);

	/* Compute the dx/dy ratios. */

	/*
	 * Compute the x offsets at the first scan line to sample.  We need
	 * to be careful in computing ys# * dx#f {/,%} h# because the
	 * multiplication may overflow.  We know that all the quantities
	 * involved are non-negative, and that ys# is usually less than 1 (as
	 * a fixed, of course); this gives us a cheap conservative check for
	 * overflow in the multiplication.
	 */
#define YMULT_QUO(ys, tl)\
  (ys < fixed_1 && tl.df < YMULT_LIMIT ? ys * tl.df / tl.h :\
   fixed_mult_quo(ys, tl.df, tl.h))

	/*
	 * It's worth checking for dxl == dxr, since this is the case
	 * for parallelograms (including stroked lines).
	 * Also check for left or right vertical edges.
	 */
	if (fixed_floor(l.x) == fixed_pixround(x1l)) {
	    /* Left edge is vertical, we don't need to increment. */
	    l.di = 0, l.df = 0;
	    fxl = 0;
	} else {
	    compute_dx(&l, dxl, ysl);
	    fxl = YMULT_QUO(ysl, l);
	    l.x += fxl;
	}
	if (fixed_floor(r.x) == fixed_pixround(x1r)) {
	    /* Right edge is vertical.  If both are vertical, */
	    /* we have a rectangle. */
	    if (l.di == 0 && l.df == 0)
		max_rect_height = max_int;
	    else
		r.di = 0, r.df = 0;
	}
	/*
	 * The test for fxl != 0 is required because the right edge might
	 * cross some pixel centers even if the left edge doesn't.
	 */
	else if (dxr == dxl && fxl != 0) {
	    if (l.di == 0)
		r.di = 0, r.df = l.df;
	    else		/* too hard to do adjustments right */
		compute_dx(&r, dxr, ysr);
	    if (ysr == ysl && r.h == l.h)
		r.x += fxl;
	    else
		r.x += YMULT_QUO(ysr, r);
	} else {
	    compute_dx(&r, dxr, ysr);
	    r.x += YMULT_QUO(ysr, r);
	}
	rxl = fixed2int_var(l.x);
	rxr = fixed2int_var(r.x);

	/*
	 * Take a shortcut if we're only sampling a single scan line,
	 * or if we have a rectangle.
	 */
	if (iy1 - iy <= max_rect_height) {
	    iy = iy1 - 1;
	    if_debug2('z', "[z]rectangle, x=[%d,%d]\n", rxl, rxr);
	} else {
	    /* Compute one line's worth of dx/dy. */
	    compute_ldx(&l, ysl);
	    if (dxr == dxl && ysr == ysl && r.h == l.h)
		r.ldi = l.ldi, r.ldf = l.ldf, r.xf = l.xf;
	    else
		compute_ldx(&r, ysr);
	}

#define STEP_LINE(ix, tl)\
  tl.x += tl.ldi;\
  if ( (tl.xf += tl.ldf) >= 0 ) tl.xf -= tl.h, tl.x++;\
  ix = fixed2int_var(tl.x)

#if CONTIGUOUS_FILL
/*
 * If left and right boundary round to same pixel index,
 * we would not paing the scan and would get a dropout.
 * Check for this case and choose one of two pixels
 * which is closer to the "axis". We need to exclude
 * 'peak' because it would paint an excessive pixel.
 */
#define SET_MINIMAL_WIDTH(ixl, ixr, l, r) \
    if (ixl == ixr) \
	if ((!peak0 || iy >= peak_y0) && (!peak1 || iy <= peak_y1)) {\
	    fixed x = int2fixed(ixl) + fixed_half;\
	    if (x - l.x < r.x - x)\
		++ixr;\
	    else\
		--ixl;\
	}

#define CONNECT_RECTANGLES(ixl, ixr, rxl, rxr, iy, ry, adj1, adj2, fill)\
    if (adj1 < adj2) {\
	if (iy - ry > 1) {\
	    VD_RECT_SWAPPED(rxl, ry, rxr, iy - 1);\
	    code = fill(rxl, ry, rxr - rxl, iy - ry - 1);\
	    if (code < 0)\
		goto xit;\
	    ry = iy - 1;\
	}\
	adj1 = adj2 = (adj2 + adj2) / 2;\
    }

#else
#define SET_MINIMAL_WIDTH(ixl, ixr, l, r) DO_NOTHING
#define CONNECT_RECTANGLES(ixl, ixr, rxl, rxr, iy, ry, adj1, adj2, fill) DO_NOTHING
#endif
	if (fill_direct) {
	    gx_color_index cindex = pdevc->colors.pure;
	    dev_proc_fill_rectangle((*fill_rect)) =
		dev_proc(dev, fill_rectangle);

	    SET_MINIMAL_WIDTH(rxl, rxr, l, r);
	    while (++iy != iy1) {
		register int ixl, ixr;

		STEP_LINE(ixl, l);
		STEP_LINE(ixr, r);
		SET_MINIMAL_WIDTH(ixl, ixr, l, r);
		if (ixl != rxl || ixr != rxr) {
		    CONNECT_RECTANGLES(ixl, ixr, rxl, rxr, iy, ry, rxr, ixl, FILL_TRAP_RECT_DIRECT);
		    CONNECT_RECTANGLES(ixl, ixr, rxl, rxr, iy, ry, ixr, rxl, FILL_TRAP_RECT_DIRECT);
		    VD_RECT_SWAPPED(rxl, ry, rxr, iy);
		    code = FILL_TRAP_RECT_DIRECT(rxl, ry, rxr - rxl, iy - ry);
		    if (code < 0)
			goto xit;
		    rxl = ixl, rxr = ixr, ry = iy;
		}
	    }
	    VD_RECT_SWAPPED(rxl, ry, rxr, iy);
	    code = FILL_TRAP_RECT_DIRECT(rxl, ry, rxr - rxl, iy - ry);
	} else {
	    SET_MINIMAL_WIDTH(rxl, rxr, l, r);
	    while (++iy != iy1) {
		register int ixl, ixr;

		STEP_LINE(ixl, l);
		STEP_LINE(ixr, r);
		SET_MINIMAL_WIDTH(ixl, ixr, l, r);
		if (ixl != rxl || ixr != rxr) {
		    CONNECT_RECTANGLES(ixl, ixr, rxl, rxr, iy, ry, rxr, ixl, FILL_TRAP_RECT);
		    CONNECT_RECTANGLES(ixl, ixr, rxl, rxr, iy, ry, ixr, rxl, FILL_TRAP_RECT);
		    VD_RECT_SWAPPED(rxl, ry, rxr, iy);
		    code = FILL_TRAP_RECT(rxl, ry, rxr - rxl, iy - ry);
		    if (code < 0)
			goto xit;
		    rxl = ixl, rxr = ixr, ry = iy;
		}
	    }
	    VD_RECT_SWAPPED(rxl, ry, rxr, iy);
	    code = FILL_TRAP_RECT(rxl, ry, rxr - rxl, iy - ry);
	}
#undef STEP_LINE
#undef SET_MINIMAL_WIDTH
#undef CONNECT_RECTANGLES
#undef FILL_TRAP_RECT
#undef FILL_TRAP_RECT_DIRECT
#undef YMULT_QUO
#undef VD_RECT_SWAPPED
xit:	if (code < 0 && fill_direct)
	    return_error(code);
	return_if_interrupt();
	return code;
    }
}

#undef GX_FILL_TRAPEZOID
#undef CONTIGUOUS_FILL
#undef SWAP_AXES
#undef FLAGS_TYPE
