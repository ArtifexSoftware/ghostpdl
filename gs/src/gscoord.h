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
/* Interface to graphics state CTM procedures */
/* Requires gsmatrix.h and gsstate.h */

#ifndef gscoord_INCLUDED
#  define gscoord_INCLUDED

/* Coordinate system modification */
int gs_initmatrix(P1(gs_state *)), gs_defaultmatrix(P2(const gs_state *, gs_matrix *)),
      gs_currentmatrix(P2(const gs_state *, gs_matrix *)), gs_setmatrix(P2(gs_state *, const gs_matrix *)),
      gs_translate(P3(gs_state *, floatp, floatp)), gs_scale(P3(gs_state *, floatp, floatp)),
      gs_rotate(P2(gs_state *, floatp)), gs_concat(P2(gs_state *, const gs_matrix *));

/* Extensions */
int gs_setdefaultmatrix(P2(gs_state *, const gs_matrix *)), gs_currentcharmatrix(P3(gs_state *, gs_matrix *, bool)),
      gs_setcharmatrix(P2(gs_state *, const gs_matrix *)), gs_settocharmatrix(P1(gs_state *));

/* Coordinate transformation */
int gs_transform(P4(gs_state *, floatp, floatp, gs_point *)), gs_dtransform(P4(gs_state *, floatp, floatp, gs_point *)),
    gs_itransform(P4(gs_state *, floatp, floatp, gs_point *)), gs_idtransform(P4(gs_state *, floatp, floatp, gs_point *));

#ifndef gs_imager_state_DEFINED
#  define gs_imager_state_DEFINED
typedef struct gs_imager_state_s gs_imager_state;
#endif

int gs_imager_setmatrix(P2(gs_imager_state *, const gs_matrix *));
int gs_imager_idtransform(P4(const gs_imager_state *, floatp, floatp,
			     gs_point *));

#endif /* gscoord_INCLUDED */
