/* Copyright (C) 1992, 1993, 1997 Aladdin Enterprises.  All rights reserved.
  
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
