/* Portions Copyright (C) 2001 artofcode LLC.
   Portions Copyright (C) 1996, 2001 Artifex Software Inc.
   Portions Copyright (C) 1988, 2000 Aladdin Enterprises.
   This software is based in part on the work of the Independent JPEG Group.
   All Rights Reserved.

   This software is distributed under license and may not be copied, modified
   or distributed except as expressly authorized under the terms of that
   license.  Refer to licensing information at http://www.artifex.com/ or
   contact Artifex Software, Inc., 101 Lucas Valley Road #110,
   San Rafael, CA  94903, (415)492-9861, for further information. */

/*$RCSfile$ $Revision$ */
/* Internal device color conversion interfaces */

#ifndef gxdcconv_INCLUDED
#  define gxdcconv_INCLUDED

#include "gxfrac.h"

/* Color space conversion routines */
frac color_rgb_to_gray(frac r, frac g, frac b,
		       const gs_imager_state * pis);
void color_rgb_to_cmyk(frac r, frac g, frac b,
		       const gs_imager_state * pis, frac cmyk[4]);
frac color_cmyk_to_gray(frac c, frac m, frac y, frac k,
			const gs_imager_state * pis);
void color_cmyk_to_rgb(frac c, frac m, frac y, frac k,
		       const gs_imager_state * pis, frac rgb[3]);

#endif /* gxdcconv_INCLUDED */
