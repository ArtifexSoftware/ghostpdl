/* Copyright (C) 2001-2023 Artifex Software, Inc.
   All Rights Reserved.

   This software is provided AS-IS with no warranty, either express or
   implied.

   This software is distributed under license and may not be copied,
   modified or distributed except as expressly authorized under the terms
   of the license contained in the file LICENSE in this distribution.

   Refer to licensing information at http://www.artifex.com or contact
   Artifex Software, Inc.,  39 Mesa Street, Suite 108A, San Francisco,
   CA 94129, USA, for further information.
*/


/* Internal definitions for DeviceN color spaces */

#ifndef gxcdevn_INCLUDED
#  define gxcdevn_INCLUDED

#include "gxcindex.h"
#include "gsccolor.h"
#include "gxfrac.h"
#include "gscspace.h"

/* Cache for DeviceN color.  Note that currently this is a 1-entry cache. */
struct gs_device_n_map_s {
    rc_header rc;
    int (*tint_transform)(const float *in, float *out,
                          const gs_gstate *pgs, void *data);
    void *tint_transform_data;
    bool cache_valid;
    float tint[GS_CLIENT_COLOR_MAX_COMPONENTS];
    frac conc[GX_DEVICE_COLOR_MAX_COMPONENTS];
};
#define private_st_device_n_map() /* in gscdevn.c */\
  gs_private_st_ptrs1(st_device_n_map, gs_device_n_map, "gs_device_n_map",\
    device_n_map_enum_ptrs, device_n_map_reloc_ptrs, tint_transform_data)

/* Allocate and initialize a DeviceN map. */
int alloc_device_n_map(gs_device_n_map ** ppmap, gs_memory_t * mem,
                       client_name_t cname);

struct gs_device_n_colorant_s {
    rc_header rc;
    char *colorant_name;
    gs_color_space *cspace;
    struct gs_device_n_colorant_s * next;	/* Linked list */
};
#define private_st_device_n_colorant() /* in gscdevn.c */\
  gs_private_st_ptrs2(st_device_n_colorant, gs_device_n_colorant, "gs_device_n_colorant",\
    device_n_colorant_enum_ptrs, device_n_colorant_reloc_ptrs, cspace, next)

/* Check if we are using the alternate color space */
bool using_alt_color_space(const gs_gstate * pgs);

#endif /* gxcdevn_INCLUDED */
