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
/* Definitions for FunctionType 0 (Sampled) Functions */

#ifndef gsfunc0_INCLUDED
#  define gsfunc0_INCLUDED

#include "gsfunc.h"
#include "gsdsrc.h"

/* ---------------- Types and structures ---------------- */

/* Define the Function type. */
#define function_type_Sampled 0

/* Define Sampled functions. */
typedef struct gs_function_Sd_params_s {
    gs_function_params_common;
    int Order;			/* 1 or 3, optional */
    gs_data_source_t DataSource;
    int BitsPerSample;		/* 1, 2, 4, 8, 12, 16, 24, 32 */
    const float *Encode;	/* 2 x m, optional */
    const float *Decode;	/* 2 x n, optional */
    const int *Size;		/* m */
} gs_function_Sd_params_t;

#define private_st_function_Sd()	/* in gsfunc.c */\
  gs_private_st_composite(st_function_Sd, gs_function_Sd_t,\
    "gs_function_Sd_t", function_Sd_enum_ptrs, function_Sd_reloc_ptrs)

/* ---------------- Procedures ---------------- */

/* Allocate and initialize a Sampled function. */
int gs_function_Sd_init(P3(gs_function_t ** ppfn,
			   const gs_function_Sd_params_t * params,
			   gs_memory_t * mem));

/* Free the parameters of a Sampled function. */
void gs_function_Sd_free_params(P2(gs_function_Sd_params_t * params,
				   gs_memory_t * mem));

#endif /* gsfunc0_INCLUDED */
