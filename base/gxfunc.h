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
void fn_common_free_params(gs_function_params_t * params, gs_memory_t * mem);

/* Generic free implementation. */
void fn_common_free(gs_function_t * pfn, bool free_params, gs_memory_t * mem);

/* Check the values of m, n, Domain, and (if supplied) Range. */
int fn_check_mnDR(const gs_function_params_t * params, int m, int n);

/* Generic get_info implementation (no Functions or DataSource). */
FN_GET_INFO_PROC(gs_function_get_info_default);

/*
 * Write generic parameters (FunctionType, Domain, Range) on a parameter list.
 */
int fn_common_get_params(const gs_function_t *pfn, gs_param_list *plist);

/*
 * Copy an array of numeric values when scaling a function.
 */
void *fn_copy_values(const void *pvalues, int count, int size,
                     gs_memory_t *mem);

/*
 * If necessary, scale the Range or Decode array for fn_make_scaled.
 * Note that we must always allocate a new array.
 */
int fn_scale_pairs(const float **ppvalues, const float *pvalues, int npairs,
                   const gs_range_t *pranges, gs_memory_t *mem);

/*
 * Scale the generic part of a function (Domain and Range).
 * The client must have copied the parameters already.
 */
int fn_common_scale(gs_function_t *psfn, const gs_function_t *pfn,
                    const gs_range_t *pranges, gs_memory_t *mem);

/* Serialize. */
int fn_common_serialize(const gs_function_t * pfn, stream *s);

#endif /* gxfunc_INCLUDED */
