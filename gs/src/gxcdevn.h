/* Copyright (C) 1999 Aladdin Enterprises.  All rights reserved.
 * This software is licensed to a single customer by Artifex Software Inc.
 * under the terms of a specific OEM agreement.
 */

/*$RCSfile$ $Revision$ */
/* Internal definitions for DeviceN color spaces */

#ifndef gxcdevn_INCLUDED
#  define gxcdevn_INCLUDED

#include "gsrefct.h"
#include "gxcindex.h"

/* Cache for DeviceN color.  Note that currently this is a 1-entry cache. */
#ifndef gs_device_n_map_DEFINED
#  define gs_device_n_map_DEFINED
typedef struct gs_device_n_map_s gs_device_n_map;
#endif
struct gs_device_n_map_s {
    rc_header rc;
    int (*tint_transform)(P5(const gs_device_n_params * params,
			     const float *in, float *out,
			     const gs_imager_state *pis, void *data));
    void *tint_transform_data;
    bool cache_valid;
    float tint[GS_CLIENT_COLOR_MAX_COMPONENTS];
    frac conc[GX_DEVICE_COLOR_MAX_COMPONENTS];
};
#define private_st_device_n_map() /* in gscdevn.c */\
  gs_private_st_ptrs1(st_device_n_map, gs_device_n_map, "gs_device_n_map",\
    device_n_map_enum_ptrs, device_n_map_reloc_ptrs, tint_transform_data)

/* Allocate and initialize a DeviceN map. */
int alloc_device_n_map(P3(gs_device_n_map ** ppmap, gs_memory_t * mem,
			  client_name_t cname));

#endif /* gxcdevn_INCLUDED */
