/* Copyright (C) 1992, 1993, 1997 Aladdin Enterprises.  All rights reserved.
  
  This software is licensed to a single customer by Artifex Software Inc.
  under the terms of a specific OEM agreement.
*/

/*$RCSfile$ $Revision$ */
/* Internal device color conversion interfaces */

#ifndef gxdcconv_INCLUDED
#  define gxdcconv_INCLUDED

#include "gxfrac.h"

/* Color space conversion routines */
frac color_rgb_to_gray(P4(frac r, frac g, frac b,
			  const gs_imager_state * pis));
void color_rgb_to_cmyk(P5(frac r, frac g, frac b,
			  const gs_imager_state * pis, frac cmyk[4]));
frac color_cmyk_to_gray(P5(frac c, frac m, frac y, frac k,
			   const gs_imager_state * pis));
void color_cmyk_to_rgb(P6(frac c, frac m, frac y, frac k,
			  const gs_imager_state * pis, frac rgb[3]));

#endif /* gxdcconv_INCLUDED */
