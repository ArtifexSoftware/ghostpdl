/* Copyright (C) 1989, 1996 Aladdin Enterprises.  All rights reserved.
 * This software is licensed to a single customer by Artifex Software Inc.
 * under the terms of a specific OEM agreement.
 */

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
