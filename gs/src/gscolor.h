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
/* Client interface to color routines */

#ifndef gscolor_INCLUDED
#  define gscolor_INCLUDED

#include "gxtmap.h"

/* Color and gray interface */
int gs_setgray(P2(gs_state *, floatp));
int gs_currentgray(P2(const gs_state *, float *));
int gs_setrgbcolor(P4(gs_state *, floatp, floatp, floatp));
int gs_currentrgbcolor(P2(const gs_state *, float[3]));
int gs_setnullcolor(P1(gs_state *));

/* Transfer function */
int gs_settransfer(P2(gs_state *, gs_mapping_proc));
int gs_settransfer_remap(P3(gs_state *, gs_mapping_proc, bool));
gs_mapping_proc gs_currenttransfer(P1(const gs_state *));

#endif /* gscolor_INCLUDED */
