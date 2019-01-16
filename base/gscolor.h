/* Copyright (C) 2001-2019 Artifex Software, Inc.
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


/* Client interface to color routines */

#ifndef gscolor_INCLUDED
#  define gscolor_INCLUDED

#include "stdpre.h"
#include "gxtmap.h"
#include "gsgstate.h"

/* Color and gray interface */
int gs_setgray(gs_gstate *, double);
int gs_currentgray(const gs_gstate *, float *);
int gs_setrgbcolor(gs_gstate *, double, double, double);
int gs_currentrgbcolor(const gs_gstate *, float[3]);
int gs_setnullcolor(gs_gstate *);

/* Transfer function */
int gs_settransfer(gs_gstate *, gs_mapping_proc);
int gs_settransfer_remap(gs_gstate *, gs_mapping_proc, bool);
gs_mapping_proc gs_currenttransfer(const gs_gstate *);

#endif /* gscolor_INCLUDED */
