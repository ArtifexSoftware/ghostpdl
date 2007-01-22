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

/*$Id: mettop.c 2326 2006-02-21 18:57:42Z henrys $*/

/* Top-level API implementation of XML Paper Specification */

/* Language wrapper implementation (see pltop.h) */

#include "ghostxps.h"

#include "pltop.h"

#define XPS_PARSER_MIN_INPUT_SIZE 8192

/*
 * The XPS interpeter is identical to pl_interp_t.
 * The XPS interpreter instance is derived from pl_interp_instance_t.
 */

typedef struct xps_interp_instance_s xps_interp_instance_t;

struct xps_interp_instance_s
{
    pl_interp_instance_t pl;		/* common part: must be first */

    pl_page_action_t pre_page_action;   /* action before page out */
    void *pre_page_closure;		/* closure to call pre_page_action with */
    pl_page_action_t post_page_action;  /* action before page out */
    void *post_page_closure;		/* closure to call post_page_action with */

    xps_context_t *ctx;
};

/* version and build date are not currently used */
#define XPS_VERSION NULL
#define XPS_BUILD_DATE NULL

const pl_interp_characteristics_t *
xps_imp_characteristics(const pl_interp_implementation_t *pimpl)
{
    static pl_interp_characteristics_t met_characteristics =
    {
	"XPS",    
	"PK", /* string to recognize XPS files */
	"Artifex",
	XPS_VERSION,
	XPS_BUILD_DATE,
	XPS_PARSER_MIN_INPUT_SIZE, /* Minimum input size */
    };
    return &met_characteristics;
}

private int  
xps_imp_allocate_interp(pl_interp_t **ppinterp,
	const pl_interp_implementation_t *pimpl,
	gs_memory_t *pmem)
{
    static pl_interp_t interp; /* there's only one interpreter */
    *ppinterp = &interp;
    return 0;
}

/* Do per-instance interpreter allocation/init. No device is set yet */
private int
xps_imp_allocate_interp_instance(pl_interp_instance_t **ppinstance,
	pl_interp_t *pinterp,
	gs_memory_t *pmem)
{
    xps_interp_instance_t *instance;
    xps_context_t *ctx;
    gs_state *pgs;

dputs("xps_imp_allocate_interp_instance!\n");

    instance = (xps_interp_instance_t *) gs_alloc_bytes(pmem,
	    sizeof(xps_interp_instance_t), "met_allocate_interp_instance");

    ctx = (xps_context_t *) gs_alloc_bytes(pmem,
	    sizeof(xps_context_t), "met_allocate_interp_instance");

    pgs = gs_state_alloc(pmem);

    if (!instance || !ctx || !pgs)
    {
	if (instance)
            gs_free_object(pmem, instance, "xps_imp_allocate_interp_instance");
        if (ctx)
            gs_free_object(pmem, ctx, "xps_imp_allocate_interp_instance");
        if (pgs)
            gs_state_free(pgs);
        return gs_error_VMerror;
    }

    ctx->instance = instance;
    ctx->memory = pmem;
    ctx->pgs = pgs;

    instance->pre_page_action = 0;
    instance->pre_page_closure = 0;
    instance->post_page_action = 0;
    instance->post_page_closure = 0;

    instance->ctx = ctx;

    *ppinstance = (pl_interp_instance_t *)instance;

    return 0;
}

/* Set a client language into an interperter instance */
private int
xps_imp_set_client_instance(pl_interp_instance_t *pinstance,
	pl_interp_instance_t *pclient,
	pl_interp_instance_clients_t which_client)
{
    /* ignore */
    return 0;
}

private int
xps_imp_set_pre_page_action(pl_interp_instance_t *pinstance,
	pl_page_action_t action, void *closure)
{
    xps_interp_instance_t *instance = (xps_interp_instance_t *)pinstance;
    instance->pre_page_action = action;
    instance->pre_page_closure = closure;
    return 0;
}

private int
xps_imp_set_post_page_action(pl_interp_instance_t *pinstance,
	pl_page_action_t action, void *closure)
{
    xps_interp_instance_t *instance = (xps_interp_instance_t *)pinstance;
    instance->post_page_action = action;
    instance->post_page_closure = closure;
    return 0;
}

private int
xps_imp_set_device(pl_interp_instance_t *pinstance, gx_device *pdevice)
{
    xps_interp_instance_t *instance = (xps_interp_instance_t *)pinstance;
    xps_context_t *ctx = instance->ctx;
    int code;

    gs_opendevice(pdevice);

    code = gs_setdevice_no_erase(ctx->pgs, pdevice);
    if (code < 0)
	goto cleanup_setdevice;

    gs_setaccuratecurves(ctx->pgs, true);  /* NB not sure */

    /* gsave and grestore (among other places) assume that */
    /* there are at least 2 gstates on the graphics stack. */
    /* Ensure that now. */
    code = gs_gsave(ctx->pgs);
    if (code < 0)
	goto cleanup_gsave;

    code = gs_erasepage(ctx->pgs);
    if (code < 0)
	goto cleanup_erase;

    return 0;

cleanup_erase:
    /* undo gsave */
    gs_grestore_only(ctx->pgs);     /* destroys gs_save stack */

cleanup_gsave:
    /* undo setdevice */
    gs_nulldevice(ctx->pgs);

cleanup_setdevice:
    /* nothing to undo */
    return code;
}

private int   
xps_imp_get_device_memory(pl_interp_instance_t *pinstance, gs_memory_t **ppmem)
{
    /* huh? we do nothing here */
    return 0;
}

/* Prepare interp instance for the next "job" */
private int
xps_imp_init_job(pl_interp_instance_t *pinstance)
{
    return 0;
}

/* Parse a cursor-full of data */
private int
xps_imp_process(pl_interp_instance_t *pinstance, stream_cursor_read *pcursor)
{
    xps_interp_instance_t *instance = (xps_interp_instance_t *)pinstance;    
    xps_context_t *ctx = instance->ctx;
    return xps_process_data(ctx, pcursor);
}

/* Skip to end of job.
 * Return 1 if done, 0 ok but EOJ not found, else negative error code.
 */
private int
xps_imp_flush_to_eoj(pl_interp_instance_t *pinstance, stream_cursor_read *pcursor)
{
    /* assume met cannot be pjl embedded */
    return -1;
}

/* Parser action for end-of-file */
private int
xps_imp_process_eof(pl_interp_instance_t *pinstance)
{
    return 0;
}

/* Report any errors after running a job */
private int
xps_imp_report_errors(pl_interp_instance_t *pinstance,
	int code,           /* prev termination status */
	long file_position, /* file position of error, -1 if unknown */
	bool force_to_cout  /* force errors to cout */
	)
{
    return 0;
}

/* Wrap up interp instance after a "job" */
private int
xps_imp_dnit_job(pl_interp_instance_t *pinstance)
{
    xps_interp_instance_t *instance = (xps_interp_instance_t *)pinstance;

    (void)instance; // unused warning

    // NB lets free some stuff.
    // zip_end_job(pctx->pst, pctx->pctx, pctx->pzip);

    return 0;
}

/* Remove a device from an interperter instance */
private int
xps_imp_remove_device(pl_interp_instance_t *pinstance)
{
    xps_interp_instance_t *instance = (xps_interp_instance_t *)pinstance;
    xps_context_t *ctx = instance->ctx;

    int code = 0;       /* first error status encountered */
    int error;

    /* return to original gstate  */
    gs_grestore_only(ctx->pgs);        /* destroys gs_save stack */

    /* Deselect device */
    /* NB */
    error = gs_nulldevice(ctx->pgs);
    if (code >= 0)
        code = error;

    return code;
}

/* Deallocate a interpreter instance */
private int
xps_imp_deallocate_interp_instance(pl_interp_instance_t *pinstance)
{
    xps_interp_instance_t *instance = (xps_interp_instance_t *)pinstance;
    xps_context_t *ctx = instance->ctx;
    gs_memory_t *mem = ctx->memory;

dputs("xps_imp_deallocate_interp_instance!\n");

    xps_debug_parts(ctx);
    xps_debug_type_map(ctx, "Default", ctx->defaults);
    xps_debug_type_map(ctx, "Override", ctx->overrides);
    xps_debug_relations(ctx, ctx->relations);

    // free gstate?
    gs_free_object(mem, ctx, "xps_imp_deallocate_interp_instance");
    gs_free_object(mem, instance, "xps_imp_deallocate_interp_instance");

    return 0;
}

/* Do static deinit of XPS interpreter */
private int
xps_imp_deallocate_interp(pl_interp_t *pinterp)
{
    /* nothing to do */
    return 0;
}

/* Parser implementation descriptor */
const pl_interp_implementation_t xps_implementation =
{
    xps_imp_characteristics,
    xps_imp_allocate_interp,
    xps_imp_allocate_interp_instance,
    xps_imp_set_client_instance,
    xps_imp_set_pre_page_action,
    xps_imp_set_post_page_action,
    xps_imp_set_device,
    xps_imp_init_job,
    xps_imp_process,
    xps_imp_flush_to_eoj,
    xps_imp_process_eof,
    xps_imp_report_errors,
    xps_imp_dnit_job,
    xps_imp_remove_device,
    xps_imp_deallocate_interp_instance,
    xps_imp_deallocate_interp,
    xps_imp_get_device_memory,
};

/* 
 * End-of-page function called by XPS parser.
 */
int
xps_show_page(xps_context_t *ctx, int num_copies, int flush)
{
    pl_interp_instance_t *pinstance = ctx->instance;
    xps_interp_instance_t *instance = ctx->instance;

    int code = 0;

    /* do pre-page action */
    if (instance->pre_page_action)
    {
        code = instance->pre_page_action(pinstance, instance->pre_page_closure);
        if (code < 0)
            return code;
        if (code != 0)
            return 0;    /* code > 0 means abort w/no error */
    }

    /* output the page */
    code = gs_output_page(ctx->pgs, num_copies, flush);
    if (code < 0)
        return code;

    /* do post-page action */ 
    if (instance->post_page_action)
    {
	code = instance->post_page_action(pinstance, instance->post_page_closure);
        if (code < 0)
            return code;
    }

    return 0;
}

