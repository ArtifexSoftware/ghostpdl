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
/* API for alpha value in graphics state */

#ifndef gsalpha_INCLUDED
#  define gsalpha_INCLUDED

/*
 * This tiny little file is separate so that it can be included by
 * gsstate.c for initializing the alpha value, even in configurations
 * that don't have full alpha support.
 */

/* Set/read alpha value. */
int gs_setalpha(P2(gs_state *, floatp));
float gs_currentalpha(P1(const gs_state *));

#endif /* gsalpha_INCLUDED */
