/* Copyright (C) 2001-2019 Artifex Software, Inc.
   All Rights Reserved.

   This software is provided AS-IS with no warranty, either express or
   implied.

   This software is distributed under license and may not be copied,
   modified or distributed except as expressly authorized under the terms
   of the license contained in the file LICENSE in this distribution.

   Refer to licensing information at http://www.artifex.com or contact
   Artifex Software, Inc.,  1305 Grant Avenue - Suite 200, Novato,
   CA 94945, U.S.A., +1(415)492-9861, for further information.
*/


/* Client interface to Level 2 color facilities */
/* (requires gscspace.h, gsmatrix.h) */

#ifndef gscolor2_INCLUDED
#  define gscolor2_INCLUDED

#include "gscindex.h"
#include "gsptype1.h"
#include "gscie.h"

/* ---------------- Graphics state ---------------- */

/*
 * Note that setcolorspace and setcolor copy the (top level of) their
 * structure argument, so if the client allocated it on the heap, the
 * client should free it after setting it in the graphics state.
 */

/* General color routines */
gs_color_space *gs_currentcolorspace(const gs_gstate *);
int gs_setcolorspace(gs_gstate *, gs_color_space *);
int gs_setcolorspace_only(gs_gstate *, gs_color_space *);
const gs_client_color *gs_currentcolor(const gs_gstate *);
int gs_setcolor(gs_gstate *, const gs_client_color *);
const gx_device_color *gs_currentdevicecolor(const gs_gstate *);

/* Look up with restriction */
int
gs_indexed_limit_and_lookup(const gs_client_color * pc,const gs_color_space *pcs,
                         gs_client_color *pcc);

/* Declare the Indexed color space type. */
extern const gs_color_space_type gs_color_space_type_Indexed;
extern const gs_color_space_type gs_color_space_type_Indexed_Named;

/* CIE-specific routines */
const gs_cie_render *gs_currentcolorrendering(const gs_gstate *);
int gs_setcolorrendering(gs_gstate *, gs_cie_render *);

/* High level device support */
int gs_includecolorspace(gs_gstate * pgs, const byte *res_name, int name_length);

#endif /* gscolor2_INCLUDED */
