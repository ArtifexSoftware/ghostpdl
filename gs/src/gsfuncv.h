/* Copyright (C) 2000 Aladdin Enterprises.  All rights reserved.
  
  This software is provided AS-IS with no warranty, either express or
  implied.
  
  This software is distributed under license and may not be copied,
  modified or distributed except as expressly authorized under the terms
  of the license contained in the file LICENSE in this distribution.
  
  For more information about licensing, please refer to
  http://www.ghostscript.com/licensing/. For information on
  commercial licensing, go to http://www.artifex.com/licensing/ or
  contact Artifex Software, Inc., 101 Lucas Valley Road #110,
  San Rafael, CA  94903, U.S.A., +1(415)492-9861.
*/

/*$RCSfile$ $Revision$ */
/* Definitions for "Vanilla" Functions */

#ifndef gsfuncv_INCLUDED
#  define gsfuncv_INCLUDED

#include "gsfunc.h"

/*
 * The simplest type of Function, "Vanilla" Functions just store closure
 * data.  The client provides the evaluation procedure.
 */

/* ---------------- Types and structures ---------------- */

#define function_type_Vanilla (-1)

typedef struct gs_function_Va_params_s {
    gs_function_params_common;
    fn_evaluate_proc_t eval_proc;
    void *eval_data;
    int is_monotonic;
} gs_function_Va_params_t;

typedef struct gs_function_Va_s {
    gs_function_head_t head;
    gs_function_Va_params_t params;
} gs_function_Va_t;

#define private_st_function_Va()	/* in gsfunc.c */\
  gs_private_st_suffix_add1(st_function_Va, gs_function_Va_t,\
    "gs_function_Va_t", function_Va_enum_ptrs, function_Va_reloc_ptrs,\
    st_function, params.eval_data)

/* ---------------- Procedures ---------------- */

/* Allocate and initialize a Vanilla function. */
int gs_function_Va_init(P3(gs_function_t ** ppfn,
			   const gs_function_Va_params_t * params,
			   gs_memory_t * mem));

/* Free the parameters of a Vanilla function. */
void gs_function_Va_free_params(P2(gs_function_Va_params_t * params,
				   gs_memory_t * mem));

#endif /* gsfuncv_INCLUDED */
