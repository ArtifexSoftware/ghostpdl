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


/* Client interface to Level 1 extended color facilities */
/* Requires gscolor.h */

#ifndef gscolor1_INCLUDED
#  define gscolor1_INCLUDED

#include "stdpre.h"
#include "gxtmap.h"
#include "gsgstate.h"

/* Color and gray interface */
int gs_setcmykcolor(gs_gstate *, double, double, double, double),
    gs_currentcmykcolor(const gs_gstate *, float[4]),
    gs_setblackgeneration(gs_gstate *, gs_mapping_proc),
    gs_setblackgeneration_remap(gs_gstate *, gs_mapping_proc, bool);
gs_mapping_proc gs_currentblackgeneration(const gs_gstate *);
int gs_setundercolorremoval(gs_gstate *, gs_mapping_proc),
    gs_setundercolorremoval_remap(gs_gstate *, gs_mapping_proc, bool);
gs_mapping_proc gs_currentundercolorremoval(const gs_gstate *);

/* Transfer function */
int gs_setcolortransfer(gs_gstate *, gs_mapping_proc /*red */ ,
                        gs_mapping_proc /*green */ ,
                        gs_mapping_proc /*blue */ ,
                        gs_mapping_proc /*gray */ ),
    gs_setcolortransfer_remap(gs_gstate *, gs_mapping_proc /*red */ ,
                              gs_mapping_proc /*green */ ,
                              gs_mapping_proc /*blue */ ,
                              gs_mapping_proc /*gray */ , bool);
void gs_currentcolortransfer(const gs_gstate *, gs_mapping_proc[4]);

#endif /* gscolor1_INCLUDED */
