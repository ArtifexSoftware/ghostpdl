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
/* RasterOp / transparency procedure interface */

#ifndef gsrop_INCLUDED
#  define gsrop_INCLUDED

#include "gsropt.h"

/* Procedural interface */

int gs_setrasterop(P2(gs_state *, gs_rop3_t));
gs_rop3_t gs_currentrasterop(P1(const gs_state *));
int gs_setsourcetransparent(P2(gs_state *, bool));
bool gs_currentsourcetransparent(P1(const gs_state *));
int gs_settexturetransparent(P2(gs_state *, bool));
bool gs_currenttexturetransparent(P1(const gs_state *));

/* Save/restore the combined logical operation. */
gs_logical_operation_t gs_current_logical_op(P1(const gs_state *));
int gs_set_logical_op(P2(gs_state *, gs_logical_operation_t));

#endif /* gsrop_INCLUDED */
