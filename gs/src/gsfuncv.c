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
/* "Vanilla" Function support */
#include "gx.h"
#include "gserrors.h"
#include "gsfuncv.h"
#include "gsparam.h"
#include "gxfunc.h"

/* GC descriptor */
private_st_function_Va();

/*
 * Test whether a Vanilla function is monotonic.  (This information is
 * provided at function definition time.)
 */
private int
fn_Va_is_monotonic(const gs_function_t * pfn_common,
		   const float *lower, const float *upper,
		   gs_function_effort_t effort)
{
    const gs_function_Va_t *const pfn =
	(const gs_function_Va_t *)pfn_common;

    return pfn->params.is_monotonic;
}

/* Free the parameters of a Vanilla function. */
void
gs_function_Va_free_params(gs_function_Va_params_t * params,
			   gs_memory_t * mem)
{
    gs_free_object(mem, params->eval_data, "eval_data");
    fn_common_free_params((gs_function_params_t *) params, mem);
}

/* Allocate and initialize a Vanilla function. */
int
gs_function_Va_init(gs_function_t ** ppfn,
		    const gs_function_Va_params_t * params,
		    gs_memory_t * mem)
{
    static const gs_function_head_t function_Va_head = {
	function_type_Vanilla,
	{
	    NULL,			/* filled in from params */
	    (fn_is_monotonic_proc_t) fn_Va_is_monotonic,
	    gs_function_get_info_default,
	    fn_common_get_params,	/****** WHAT TO DO ABOUT THIS? ******/
	    (fn_free_params_proc_t) gs_function_Va_free_params,
	    fn_common_free
	}
    };
    int code;

    *ppfn = 0;			/* in case of error */
    code = fn_check_mnDR((const gs_function_params_t *)params, 1, params->n);
    if (code < 0)
	return code;
    {
	gs_function_Va_t *pfn =
	    gs_alloc_struct(mem, gs_function_Va_t, &st_function_Va,
			    "gs_function_Va_init");

	if (pfn == 0)
	    return_error(gs_error_VMerror);
	pfn->params = *params;
	pfn->head = function_Va_head;
	pfn->head.procs.evaluate = params->eval_proc;
	pfn->head.is_monotonic = params->is_monotonic;
	*ppfn = (gs_function_t *) pfn;
    }
    return 0;
}
