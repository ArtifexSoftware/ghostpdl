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
/* Interpreter color remapping structure */

#ifndef icremap_INCLUDED
#  define icremap_INCLUDED

#include "gsccolor.h"

/*
 * Define the structure used to communicate information back to the
 * interpreter for color remapping.  Pattern remapping doesn't use the
 * tint values, DeviceN remapping does.
 */
#ifndef int_remap_color_info_DEFINED
#  define int_remap_color_info_DEFINED
typedef struct int_remap_color_info_s int_remap_color_info_t;
#endif
struct int_remap_color_info_s {
    op_proc_t proc;		/* remapping procedure */
    float tint[GS_CLIENT_COLOR_MAX_COMPONENTS];
};

#define private_st_int_remap_color_info() /* in zgstate.c */\
  gs_private_st_simple(st_int_remap_color_info, int_remap_color_info_t,\
    "int_remap_color_info_t")

#endif /* icremap_INCLUDED */
