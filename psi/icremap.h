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


/* Interpreter color remapping structure */

#ifndef icremap_INCLUDED
#  define icremap_INCLUDED

#include "gsccolor.h"
#include "iref.h"
#include "igstate.h"

/*
 * Define the structure used to communicate information back to the
 * interpreter for color remapping.  Pattern remapping doesn't use the
 * tint values, DeviceN remapping does.
 */
struct int_remap_color_info_s {
    op_proc_t proc;		/* remapping procedure */
    float tint[MAX_COMPONENTS_IN_DEVN];    /* must match limitcheck in psi/zcolor.c: validatedevicenspace */
};

#define private_st_int_remap_color_info() /* in zgstate.c */\
  gs_private_st_simple(st_int_remap_color_info, int_remap_color_info_t,\
    "int_remap_color_info_t")

#endif /* icremap_INCLUDED */
