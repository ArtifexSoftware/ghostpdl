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
/* Client interface to Level 2 color facilities */
/* (requires gscspace.h, gsmatrix.h) */

#ifndef gscolor2_INCLUDED
#  define gscolor2_INCLUDED

#include "gscindex.h"
#include "gsptype1.h"

/* ---------------- Graphics state ---------------- */

/*
 * Note that setcolorspace and setcolor copy the (top level of) their
 * structure argument, so if the client allocated it on the heap, the
 * client should free it after setting it in the graphics state.
 */

/* General color routines */
const gs_color_space *gs_currentcolorspace(const gs_state *);
int gs_setcolorspace(gs_state *, const gs_color_space *);
const gs_client_color *gs_currentcolor(const gs_state *);
int gs_setcolor(gs_state *, const gs_client_color *);


/* CIE-specific routines */
#ifndef gs_cie_render_DEFINED
#  define gs_cie_render_DEFINED
typedef struct gs_cie_render_s gs_cie_render;
#endif
const gs_cie_render *gs_currentcolorrendering(const gs_state *);
int gs_setcolorrendering(gs_state *, gs_cie_render *);

#endif /* gscolor2_INCLUDED */
