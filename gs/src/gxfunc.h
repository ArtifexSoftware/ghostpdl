/* Copyright (C) 1997, 1998, 1999 Aladdin Enterprises.  All rights reserved.

   This software is licensed to a single customer by Artifex Software Inc.
   under the terms of a specific OEM agreement.
 */

/*$RCSfile$ $Revision$ */
/* Internal definitions for Functions */

#ifndef gxfunc_INCLUDED
#  define gxfunc_INCLUDED

#include "gsfunc.h"
#include "gsstruct.h"

/* ---------------- Types and structures ---------------- */

/* Define the generic Function structure type.  This is never instantiated. */
extern_st(st_function);
#define public_st_function()	/* in gsfunc.c */\
  gs_public_st_ptrs2(st_function, gs_function_t, "gs_function_t",\
    function_enum_ptrs, function_reloc_ptrs, params.Domain, params.Range)

/* ---------------- Internal procedures ---------------- */

/* Generic free_params implementation. */
void fn_common_free_params(P2(gs_function_params_t * params, gs_memory_t * mem));

/* Generic free implementation. */
void fn_common_free(P3(gs_function_t * pfn, bool free_params, gs_memory_t * mem));

/* Check the values of m, n, Domain, and (if supplied) Range. */
int fn_check_mnDR(P3(const gs_function_params_t * params, int m, int n));

/* Get the monotonicity of a function over its Domain. */
int fn_domain_is_monotonic(P2(const gs_function_t *pfn,
			      gs_function_effort_t effort));

#endif /* gxfunc_INCLUDED */
