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
/* Internals for RasterOp compositing */

#ifndef gxropc_INCLUDED
#  define gxropc_INCLUDED

#include "gsropc.h"
#include "gxcomp.h"

/* Define RasterOp-compositing objects. */
typedef struct gs_composite_rop_s {
    gs_composite_common;
    gs_composite_rop_params_t params;
} gs_composite_rop_t;

#define private_st_composite_rop() /* in gsropc.c */\
  gs_private_st_ptrs1(st_composite_rop, gs_composite_rop_t,\
    "gs_composite_rop_t", composite_rop_enum_ptrs, composite_rop_reloc_ptrs,\
    params.texture)

/*
 * Initialize a RasterOp compositing function from parameters.
 * We make this visible so that clients can allocate gs_composite_rop_t
 * objects on the stack, to reduce memory manager overhead.
 */
void gx_init_composite_rop(gs_composite_rop_t * pcte,
			   const gs_composite_rop_params_t * params);

#endif /* gxropc_INCLUDED */
