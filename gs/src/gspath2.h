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
