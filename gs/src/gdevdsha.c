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
#include "gxdevice.h"
#include "gserrors.h"

int 
gx_default_fill_pixel(const gs_fill_attributes *fa, int i, int j, const frac32 *c)
{
    return_error(gs_error_unregistered); /* Unimplemented. */
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
