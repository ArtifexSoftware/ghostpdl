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


/* Level 2 graphics state path procedures */
/* Requires gsmatrix.h */

#ifndef gspath2_INCLUDED
#  define gspath2_INCLUDED

#include "gsgstate.h"
#include "gsmatrix.h"

/* Miscellaneous */
int gs_setbbox(gs_gstate *, double, double, double, double);

/* Rectangles */
int gs_rectappend(gs_gstate *, const gs_rect *, uint);
int gs_rectclip(gs_gstate *, const gs_rect *, uint);
int gs_rectfill(gs_gstate *, const gs_rect *, uint);
int gs_rectstroke(gs_gstate *, const gs_rect *, uint, const gs_matrix *);

#endif /* gspath2_INCLUDED */
