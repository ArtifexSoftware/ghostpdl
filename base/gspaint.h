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


/* Erase/fill/stroke procedures */
/* Requires gsstate.h */

#ifndef gspaint_INCLUDED
#  define gspaint_INCLUDED

#include "stdpre.h"
#include "gsgstate.h"

/* Painting */
int gs_erasepage(gs_gstate *),
    gs_fillpage(gs_gstate *),
    gs_fill(gs_gstate *),
    gs_eofill(gs_gstate *),
    gs_stroke(gs_gstate *),
    gs_fillstroke(gs_gstate * pgs, int *restart),
    gs_eofillstroke(gs_gstate * pgs, int *restart);

/* Image tracing */
int gs_imagepath(gs_gstate *, int, int, const byte *);

#endif /* gspaint_INCLUDED */
