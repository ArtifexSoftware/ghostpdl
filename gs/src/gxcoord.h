/* Copyright (C) 1994 Aladdin Enterprises.  All rights reserved.

   This software is licensed to a single customer by Artifex Software Inc.
   under the terms of a specific OEM agreement.
 */

/*$RCSfile$ $Revision$ */
/* Internal graphics state CTM procedures */
/* Requires gxmatrix.h and gzstate.h */

#ifndef gxcoord_INCLUDED
#  define gxcoord_INCLUDED

#include "gscoord.h"

/* Set the translation to a fixed value, and translate any existing path. */
/* Used by gschar.c to prepare for a BuildChar or BuildGlyph procedure. */
int gx_translate_to_fixed(P3(gs_state *, fixed, fixed));

/* Scale the CTM and character matrix for oversampling. */
int gx_scale_char_matrix(P3(gs_state *, int, int));

/* Compute the coefficients for fast fixed-point distance transformations */
/* from a transformation matrix. */
int gx_matrix_to_fixed_coeff(P3(const gs_matrix *, fixed_coeff *, int));

#endif /* gxcoord_INCLUDED */
