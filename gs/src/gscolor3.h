/* Copyright (C) 1997 Aladdin Enterprises.  All rights reserved.
  
  This software is licensed to a single customer by Artifex Software Inc.
  under the terms of a specific OEM agreement.
*/

/*$RCSfile$ $Revision$ */
/* Client interface to LanguageLevel 3 color facilities */

#ifndef gscolor3_INCLUDED
#  define gscolor3_INCLUDED

/* Smooth shading */
#ifndef gs_shading_t_DEFINED
#  define gs_shading_t_DEFINED
typedef struct gs_shading_s gs_shading_t;
#endif

int gs_setsmoothness(P2(gs_state *, floatp));
float gs_currentsmoothness(P1(const gs_state *));
int gs_shfill(P2(gs_state *, const gs_shading_t *));

#endif /* gscolor3_INCLUDED */
