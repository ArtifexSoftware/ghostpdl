/* Copyright (C) 1991, 1992, 1993, 1996, 1997 Aladdin Enterprises.  All rights reserved.

   This software is licensed to a single customer by Artifex Software Inc.
   under the terms of a specific OEM agreement.
 */

/*$RCSfile$ $Revision$ */
/* Client interface to color routines */

#ifndef gscolor_INCLUDED
#  define gscolor_INCLUDED

#include "gxtmap.h"

/* Color and gray interface */
int gs_setgray(P2(gs_state *, floatp));
float gs_currentgray(P1(const gs_state *));
int gs_setrgbcolor(P4(gs_state *, floatp, floatp, floatp)), gs_currentrgbcolor(P2(const gs_state *, float[3]));
int gs_setnullcolor(P1(gs_state *));

/* Transfer function */
int gs_settransfer(P2(gs_state *, gs_mapping_proc)), gs_settransfer_remap(P3(gs_state *, gs_mapping_proc, bool));
gs_mapping_proc gs_currenttransfer(P1(const gs_state *));

#endif /* gscolor_INCLUDED */
