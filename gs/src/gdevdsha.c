/* Copyright (C) 2004 Aladdin Enterprises.  All rights reserved.
  
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
/* Default shading drawing device procedures. */

#include "gx.h"
#include "gserrors.h"
#include "gxdevice.h"
#include "gxcindex.h"

int 
gx_default_fill_linear_color_scanline(const gs_fill_attributes *fa,
	int i0, int j, int w,
	const frac32 *c0, const long *addx, const long *mulx, const ulong *divx)
{
    /* This default implementation decomposes the area into constant color rectangles.
       Devices may supply optimized implementations with
       the inversed nesting of the i,k cicles,
       i.e. with enumerating planes first, with a direct writing to the raster,
       and with a fixed bits per component.
     */
    frac32 c[GX_DEVICE_COLOR_MAX_COMPONENTS];
    long f[GX_DEVICE_COLOR_MAX_COMPONENTS];
    int i, i1 = i0 + w, bi = i0, k;
    gx_color_index ci0 = 0, ci1;
    const gx_device_color_info *cinfo = &fa->pdev->color_info;
    int n = cinfo->num_components;
    int si, ei, code;

    if (j < fa->clip->p.y || j > fa->clip->q.y)
	return 0;
    for (k = 0; k < n; k++) {
	int shift = cinfo->comp_shift[k];
	int bits = cinfo->comp_bits[k];

	c[k] = c0[k];
	f[k] = addx[k];
	ci0 |= ((gx_color_index)(c[k] >> (sizeof(c[k]) * 8 - bits)) << shift);
    }
    for (i = i0 + 1; i < i1; i++) {
	ci1 = 0;
	for (k = 0; k < n; k++) {
	    int shift = cinfo->comp_shift[k];
	    int bits = cinfo->comp_bits[k];
	    long m = f[k] + mulx[k];

	    f[k] = m % divx[k];
	    c[k] += m / divx[k];
	    ci1 |= ((gx_color_index)(c[k] >> (sizeof(c[k]) * 8 - bits)) << shift);
	}
	if (ci1 != ci0) {
	    si = max(bi, fa->clip->p.x);
	    ei = min(i, fa->clip->q.x);
	    if (si < ei) {
		if (fa->swap_axes)
		    code = dev_proc(fa->pdev, fill_rectangle)(fa->pdev, j, bi, 1, i - bi, ci0);
		else
		    code = dev_proc(fa->pdev, fill_rectangle)(fa->pdev, bi, j, i - bi, 1, ci0);
		if (code < 0)
		    return code;
	    }
	    bi = i;
	    ci0 = ci1;
	}
    }
    si = max(bi, fa->clip->p.x);
    ei = min(i, fa->clip->q.x);
    if (si < ei) {
	if (fa->swap_axes)
	    return dev_proc(fa->pdev, fill_rectangle)(fa->pdev, j, bi, 1, i - bi, ci0);
	else
	    return dev_proc(fa->pdev, fill_rectangle)(fa->pdev, bi, j, i - bi, 1, ci0);
    }
    return 0;
}

int 
gx_default_fill_linear_color_trapezoid(const gs_fill_attributes *fa,
	const gs_fixed_point *p0, const gs_fixed_point *p1,
	const gs_fixed_point *p2, const gs_fixed_point *p3,
	const frac32 *c0, const frac32 *c1,
	const frac32 *c2, const frac32 *c3)
{
    return_error(gs_error_unregistered); /* Unimplemented. */
}

int 
gx_default_fill_linear_color_triangle(const gs_fill_attributes *fa,
	const gs_fixed_point *p0, const gs_fixed_point *p1,
	const gs_fixed_point *p2,
	const frac32 *c0, const frac32 *c1, const frac32 *c2)
{
    return_error(gs_error_unregistered); /* Unimplemented. */
}
