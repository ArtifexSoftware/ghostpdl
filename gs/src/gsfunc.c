/* Copyright (C) 1997, 2000 Aladdin Enterprises.  All rights reserved.
  
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
/* Generic Function support */
#include "gx.h"
#include "gserrors.h"
#include "gsparam.h"
#include "gxfunc.h"

/* GC descriptors */
public_st_function();

/* Generic free_params implementation. */
void
fn_common_free_params(gs_function_params_t * params, gs_memory_t * mem)
{
    gs_free_const_object(mem, params->Range, "Range");
    gs_free_const_object(mem, params->Domain, "Domain");
}

/* Generic free implementation. */
void
fn_common_free(gs_function_t * pfn, bool free_params, gs_memory_t * mem)
{
    if (free_params)
	gs_function_free_params(pfn, mem);
    gs_free_object(mem, pfn, "fn_common_free");
}

/* Check the values of m, n, Domain, and (if supplied) Range. */
int
fn_check_mnDR(const gs_function_params_t * params, int m, int n)
{
    int i;

    if (m <= 0 || n <= 0)
	return_error(gs_error_rangecheck);
    for (i = 0; i < m; ++i)
	if (params->Domain[2 * i] > params->Domain[2 * i + 1])
	    return_error(gs_error_rangecheck);
    if (params->Range != 0)
	for (i = 0; i < n; ++i)
	    if (params->Range[2 * i] > params->Range[2 * i + 1])
		return_error(gs_error_rangecheck);
    return 0;
}

/* Get the monotonicity of a function over its Domain. */
int
fn_domain_is_monotonic(const gs_function_t *pfn, gs_function_effort_t effort)
{
#define MAX_M 16		/* arbitrary */
    float lower[MAX_M], upper[MAX_M];
    int i;

    if (pfn->params.m > MAX_M)
	return gs_error_undefined;
    for (i = 0; i < pfn->params.m; ++i) {
	lower[i] = pfn->params.Domain[2 * i];
	upper[i] = pfn->params.Domain[2 * i + 1];
    }
    return gs_function_is_monotonic(pfn, lower, upper, effort);
}

/* Return default function information. */
void
gs_function_get_info_default(const gs_function_t *pfn, gs_function_info_t *pfi)
{
    pfi->DataSource = 0;
    pfi->Functions = 0;
}

/* Write generic parameters (FunctionType, Domain, Range) on a parameter list. */
int
fn_common_get_params(const gs_function_t *pfn, gs_param_list *plist)
{
    int ecode = param_write_int(plist, "FunctionType", &FunctionType(pfn));
    int code;

    if (pfn->params.Domain) {
	code = param_write_float_values(plist, "Domain", pfn->params.Domain,
					2 * pfn->params.m, false);
	if (code < 0)
	    ecode = code;
    }
    if (pfn->params.Range) {
	code = param_write_float_values(plist, "Range", pfn->params.Range,
					2 * pfn->params.n, false);
	if (code < 0)
	    ecode = code;
    }
    return ecode;
}
