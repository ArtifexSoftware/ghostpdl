/* Copyright (C) 1998 Aladdin Enterprises.  All rights reserved.
  
  This software is licensed to a single customer by Artifex Software Inc.
  under the terms of a specific OEM agreement.
*/

/*$RCSfile$ $Revision$ */
/* RasterOp-compositing interface */

#ifndef gsropc_INCLUDED
#  define gsropc_INCLUDED

#include "gscompt.h"
#include "gsropt.h"

/*
 * Define parameters for RasterOp-compositing.
 * There are two kinds of RasterOp compositing operations.
 * If texture == 0, the input data are the texture, and the source is
 * implicitly all 0 (black).  If texture != 0, it defines the texture,
 * and the input data are the source.  Note that in the latter case,
 * the client (the caller of gs_create_composite_rop) promises that
 * *texture will not change.
 */

#ifndef gx_device_color_DEFINED
#  define gx_device_color_DEFINED
typedef struct gx_device_color_s gx_device_color;

#endif

typedef struct gs_composite_rop_params_s {
    gs_logical_operation_t log_op;
    const gx_device_color *texture;
} gs_composite_rop_params_t;

/* Create a RasterOp-compositing object. */
int gs_create_composite_rop(P3(gs_composite_t ** ppcte,
			       const gs_composite_rop_params_t * params,
			       gs_memory_t * mem));

#endif /* gsropc_INCLUDED */
