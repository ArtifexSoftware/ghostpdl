/* Copyright (C) 1997, 1998, 1999 Aladdin Enterprises.  All rights reserved.
  
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

/* $Id$ */
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

/* Generic get_info implementation (no Functions or DataSource). */
FN_GET_INFO_PROC(gs_function_get_info_default);

/* Write generic parameters (FunctionType, Domain, Range) on a parameter list. */
int fn_common_get_params(P2(const gs_function_t *pfn, gs_param_list *plist));

#endif /* gxfunc_INCLUDED */
