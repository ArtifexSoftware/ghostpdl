/* Copyright (C) 1997, 1998 Aladdin Enterprises.  All rights reserved.

   This file is part of Aladdin Ghostscript.

   Aladdin Ghostscript is distributed with NO WARRANTY OF ANY KIND.  No author
   or distributor accepts any responsibility for the consequences of using it,
   or for whether it serves any particular purpose or works at all, unless he
   or she says so in writing.  Refer to the Aladdin Ghostscript Free Public
   License (the "License") for full details.

   Every copy of Aladdin Ghostscript must include a copy of the License,
   normally in a plain ASCII text file named PUBLIC.  The License grants you
   the right to copy, modify and redistribute Aladdin Ghostscript, but only
   under certain conditions described in the License.  Among other things, the
   License requires that the copyright notice and this notice be preserved on
   all copies.
 */

/*$Id$ */
/* Generic Function support */
#include "gx.h"
#include "gserrors.h"
#include "gxfunc.h"

/* GC descriptor */
public_st_function();

/* Generic free_params implementation. */
void
fn_common_free_params(gs_function_params_t * params, gs_memory_t * mem)
{
    gs_free_object(mem, (void *)params->Range, "Range");	/* break const */
    gs_free_object(mem, (void *)params->Domain, "Domain");	/* break const */
}

/* Generic free implementation. */
void
fn_common_free(gs_function_t * pfn, bool free_params, gs_memory_t * mem)
{
    if (free_params)
	gs_function_free_params(pfn, mem);
    gs_free_object(mem, pfn, "fn_xxx_free");
}

/* Free an array of subsidiary Functions. */
void
fn_free_functions(gs_function_t ** Functions, int count, gs_memory_t * mem)
{
    int i;

    for (i = count; --i >= 0;)
	gs_function_free(Functions[i], true, mem);
    gs_free_object(mem, Functions, "Functions");
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
