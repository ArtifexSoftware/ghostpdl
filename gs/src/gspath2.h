/* Copyright (C) 1994 Aladdin Enterprises.  All rights reserved.
 * This software is licensed to a single customer by Artifex Software Inc.
 * under the terms of a specific OEM agreement.
 */

/*$RCSfile$ $Revision$ */
/* Level 2 graphics state path procedures */
/* Requires gsmatrix.h */

#ifndef gspath2_INCLUDED
#  define gspath2_INCLUDED

/* Miscellaneous */
int gs_setbbox(P5(gs_state *, floatp, floatp, floatp, floatp));

/* Rectangles */
int gs_rectappend(P3(gs_state *, const gs_rect *, uint));
int gs_rectclip(P3(gs_state *, const gs_rect *, uint));
int gs_rectfill(P3(gs_state *, const gs_rect *, uint));
int gs_rectstroke(P4(gs_state *, const gs_rect *, uint, const gs_matrix *));

#endif /* gspath2_INCLUDED */
