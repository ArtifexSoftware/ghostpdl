/* Copyright (C) 2001-2021 Artifex Software, Inc.
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


/* Interface to graphics state CTM procedures */
/* Requires gsmatrix.h and gsstate.h */

#ifndef gscoord_INCLUDED
#  define gscoord_INCLUDED

#include "gsgstate.h"
#include "gsmatrix.h"

/* Coordinate system modification */
int gs_initmatrix(gs_gstate *),
    gs_defaultmatrix(const gs_gstate *, gs_matrix *),
    gs_currentmatrix(const gs_gstate *, gs_matrix *),
    gs_setmatrix(gs_gstate *, const gs_matrix *),
    gs_translate(gs_gstate *, double, double),
    gs_translate_untransformed(gs_gstate *, double, double),
    gs_scale(gs_gstate *, double, double),
    gs_rotate(gs_gstate *, double),
    gs_concat(gs_gstate *, const gs_matrix *);

/* Extensions */
int gs_setdefaultmatrix(gs_gstate *, const gs_matrix *),
    gs_currentcharmatrix(gs_gstate *, gs_matrix *, bool),
    gs_setcharmatrix(gs_gstate *, const gs_matrix *),
    gs_settocharmatrix(gs_gstate *),
    gs_updatematrices(gs_gstate *);

/* Coordinate transformation */
int gs_transform(gs_gstate *, double, double, gs_point *),
    gs_dtransform(gs_gstate *, double, double, gs_point *),
    gs_itransform(gs_gstate *, double, double, gs_point *),
    gs_idtransform(gs_gstate *, double, double, gs_point *);

int gs_gstate_setmatrix(gs_gstate *, const gs_matrix *);
int gs_gstate_idtransform(const gs_gstate *, double, double, gs_point *);

#endif /* gscoord_INCLUDED */
