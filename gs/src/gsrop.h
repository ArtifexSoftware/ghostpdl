/* Copyright (C) 1995, 1996, 1998 Aladdin Enterprises.  All rights reserved.

   This software is licensed to a single customer by Artifex Software Inc.
   under the terms of a specific OEM agreement.
 */

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
