/* Copyright (C) 1997 Aladdin Enterprises.  All rights reserved.

   This software is licensed to a single customer by Artifex Software Inc.
   under the terms of a specific OEM agreement.
 */

/*$RCSfile$ $Revision$ */
/* Client interface to Display PostScript facilities. */

#ifndef gsdps_INCLUDED
#  define gsdps_INCLUDED

/* Device-source images */
#include "gsiparm2.h"

/* View clipping */
int gs_initviewclip(P1(gs_state *));
int gs_eoviewclip(P1(gs_state *));
int gs_viewclip(P1(gs_state *));
int gs_viewclippath(P1(gs_state *));

#endif /* gsdps_INCLUDED */
