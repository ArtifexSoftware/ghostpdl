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


/* Internal device color conversion interfaces */

#ifndef gxdcconv_INCLUDED
#  define gxdcconv_INCLUDED

#include "std.h"
#include "gxfrac.h"
#include "gsgstate.h"

/* Color space conversion routines */
frac color_rgb_to_gray(frac r, frac g, frac b,
                       const gs_gstate * pgs);
void color_rgb_to_cmyk(frac r, frac g, frac b,
                       const gs_gstate * pgs, frac cmyk[4],
                       gs_memory_t * mem);
frac color_cmyk_to_gray(frac c, frac m, frac y, frac k,
                        const gs_gstate * pgs);
void color_cmyk_to_rgb(frac c, frac m, frac y, frac k,
                       const gs_gstate * pgs, frac rgb[3],
                       gs_memory_t * mem);

#endif /* gxdcconv_INCLUDED */
