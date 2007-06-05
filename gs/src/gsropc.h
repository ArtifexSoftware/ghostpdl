/* Copyright (C) 2001-2006 Artifex Software, Inc.
   All Rights Reserved.
  
   This software is provided AS-IS with no warranty, either express or
   implied.

   This software is distributed under license and may not be copied, modified
   or distributed except as expressly authorized under the terms of that
   license.  Refer to licensing information at http://www.artifex.com/
   or contact Artifex Software, Inc.,  7 Mt. Lassen Drive - Suite A-134,
   San Rafael, CA  94903, U.S.A., +1(415)492-9861, for further information.
*/

/* $Id$ */
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
int gs_create_composite_rop(gs_composite_t ** ppcte,
			    const gs_composite_rop_params_t * params,
			    gs_memory_t * mem);

#endif /* gsropc_INCLUDED */
