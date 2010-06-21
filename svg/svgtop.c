/* Copyright (C) 2008 Artifex Software, Inc.
   All Rights Reserved.

   This software is provided AS-IS with no warranty, either express or
   implied.

   This software is distributed under license and may not be copied, modified
   or distributed except as expressly authorized under the terms of that
   license.  Refer to licensing information at http://www.artifex.com/
   or contact Artifex Software, Inc.,  7 Mt. Lassen  Drive - Suite A-134,
   San Rafael, CA  94903, U.S.A., +1(415)492-9861, for further information.
*/

/* Top-level API implementation of Scalable Vector Graphics (SVG) */

/* Language wrapper implementation (see pltop.h) */

#include "ghostsvg.h"

#include "pltop.h"

#include "gxdevice.h" /* so we can include gxht.h below */
#include "gxht.h" /* gsht1.h is incomplete, we need storage size of gs_halftone */
#include "gsht1.h"

static int svg_install_halftone(svg_context_t *ctx, gx_device *pdevice);

#define SVG_PARSER_MIN_INPUT_SIZE 8192

/*
 * The SVG interpeter is identical to pl_interp_t.
 * The SVG interpreter instance is derived from pl_interp_instance_t.
 */

typedef struct svg_interp_instance_s svg_interp_instance_t;

struct svg_interp_instance_s
{
    pl_interp_instance_t pl;		/* common part: must be first */

    pl_page_action_t pre_page_action;   /* action before page out */
    void *pre_page_closure;		/* closure to call pre_page_action with */
    pl_page_action_t post_page_action;  /* action before page out */
    void *post_page_closure;		/* closure to call post_page_action with */

    svg_context_t *ctx;
};

/* version and build date are not currently used */
#define SVG_VERSION NULL
#define SVG_BUILD_DATE NULL

static const pl_interp_characteristics_t *
svg_imp_characteristics(const pl_interp_implementation_t *pimpl)
{
    static pl_interp_characteristics_t svg_characteristics =
    {
	"SVG",
	"<?xml", /* string to recognize SVG files */
	"Artifex",
	SVG_VERSION,
	SVG_BUILD_DATE,
	SVG_PARSER_MIN_INPUT_SIZE, /* Minimum input size */
    };
    return &svg_characteristics;
}

static int
svg_imp_allocate_interp(pl_interp_t **ppinterp,
	const pl_interp_implementation_t *pimpl,
	gs_memory_t *pmem)
{
    static pl_interp_t interp; /* there's only one interpreter */
    *ppinterp = &interp;
    return 0;
}

/* Do per-instance interpreter allocation/init. No device is set yet */
static int
svg_imp_allocate_interp_instance(pl_interp_instance_t **ppinstance,
	pl_interp_t *pinterp,
	gs_memory_t *pmem)
{
    svg_interp_instance_t *instance;
    svg_context_t *ctx;
    gs_state *pgs;

    dputs("-- svg_imp_allocate_interp_instance --\n");

    instance = (svg_interp_instance_t *) gs_alloc_bytes(pmem,
	    sizeof(svg_interp_instance_t), "svg_imp_allocate_interp_instance");

    ctx = (svg_context_t *) gs_alloc_bytes(pmem,
	    sizeof(svg_context_t), "svg_imp_allocate_interp_instance");

    pgs = gs_state_alloc(pmem);
    gsicc_init_iccmanager(pgs);

    memset(ctx, 0, sizeof(svg_context_t));

    if (!instance || !ctx || !pgs)
    {
	if (instance)
            gs_free_object(pmem, instance, "svg_imp_allocate_interp_instance");
        if (ctx)
            gs_free_object(pmem, ctx, "svg_imp_allocate_interp_instance");
        if (pgs)
            gs_state_free(pgs);
        return gs_error_VMerror;
    }

    ctx->instance = instance;
    ctx->memory = pmem;
    ctx->pgs = pgs;
    ctx->fontdir = NULL;

    /* TODO: load some builtin ICC profiles here */
    ctx->srgb = gs_cspace_new_DeviceRGB(ctx->memory);

    ctx->fill_rule = 1; /* 0=evenodd, 1=nonzero */

    ctx->fill_is_set = 0;
    ctx->fill_color[0] = 0.0;
    ctx->fill_color[1] = 0.0;
    ctx->fill_color[2] = 0.0;

    ctx->stroke_is_set = 0;
    ctx->stroke_color[0] = 0.0;
    ctx->stroke_color[1] = 0.0;
    ctx->stroke_color[2] = 0.0;

    instance->pre_page_action = 0;
    instance->pre_page_closure = 0;
    instance->post_page_action = 0;
    instance->post_page_closure = 0;

    instance->ctx = ctx;

    // XXX - svg_init_font_cache(ctx);

    *ppinstance = (pl_interp_instance_t *)instance;

    return 0;
}

/* Set a client language into an interperter instance */
static int
svg_imp_set_client_instance(pl_interp_instance_t *pinstance,
	pl_interp_instance_t *pclient,
	pl_interp_instance_clients_t which_client)
{
    /* ignore */
    return 0;
}

static int
svg_imp_set_pre_page_action(pl_interp_instance_t *pinstance,
	pl_page_action_t action, void *closure)
{
    svg_interp_instance_t *instance = (svg_interp_instance_t *)pinstance;
    instance->pre_page_action = action;
    instance->pre_page_closure = closure;
    return 0;
}

static int
svg_imp_set_post_page_action(pl_interp_instance_t *pinstance,
	pl_page_action_t action, void *closure)
{
    svg_interp_instance_t *instance = (svg_interp_instance_t *)pinstance;
    instance->post_page_action = action;
    instance->post_page_closure = closure;
    return 0;
}

static int
svg_imp_set_device(pl_interp_instance_t *pinstance, gx_device *pdevice)
{
    svg_interp_instance_t *instance = (svg_interp_instance_t *)pinstance;
    svg_context_t *ctx = instance->ctx;
    int code;

    dputs("-- svg_imp_set_device --\n");

    gs_opendevice(pdevice);

    code = gsicc_init_device_profile(ctx->pgs, pdevice);
    if (code < 0)
        return code;

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

    code = svg_install_halftone(ctx, pdevice);
    if (code < 0)
	goto cleanup_halftone;

    return 0;

cleanup_halftone:
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

static int
svg_imp_get_device_memory(pl_interp_instance_t *pinstance, gs_memory_t **ppmem)
{
    /* huh? we do nothing here */
    return 0;
}

static int
svg_imp_process_file(pl_interp_instance_t *pinstance, char *filename)
{
    svg_interp_instance_t *instance = (svg_interp_instance_t *)pinstance;
    svg_context_t *ctx = instance->ctx;
    svg_item_t *root;
    FILE *file;
    int code;
    char buf[4096];
    int n;

    dprintf1("svg_imp_process_file %s\n", filename);

    file = fopen(filename, "rb");
    if (!file)
	return gs_error_ioerror;

    while (!feof(file))
    {
	n = fread(buf, 1, sizeof buf, file);
	if (n == 0)
	    if (ferror(file))
		return gs_error_ioerror;
	code = svg_feed_xml_parser(ctx, buf, n);
	if (code < 0)
	{
	    gs_catch(code, "cannot parse SVG document");
	    fclose(file);
	    return code;
	}
    }

    root = svg_close_xml_parser(ctx);
    if (!root)
    {
	gs_catch(-1, "cannot parse xml document");
	fclose(file);
	return -1;
    }

    code = svg_parse_document(ctx, root);
    if (code < 0)
	gs_catch(code, "cannot parse SVG document");

    svg_free_item(ctx, root);
    fclose(file);
    return code;
}

/* Parse a cursor-full of data */
static int
svg_imp_process(pl_interp_instance_t *pinstance, stream_cursor_read *buf)
{
    svg_interp_instance_t *instance = (svg_interp_instance_t *)pinstance;
    svg_context_t *ctx = instance->ctx;
    int code;

    code = svg_feed_xml_parser(ctx, (const char*)buf->ptr + 1, buf->limit - buf->ptr);
    if (code < 0)
	gs_catch(code, "cannot parse SVG document");

    buf->ptr = buf->limit;

    return code;
}

/* Skip to end of job.
 * Return 1 if done, 0 ok but EOJ not found, else negative error code.
 */
static int
svg_imp_flush_to_eoj(pl_interp_instance_t *pinstance, stream_cursor_read *pcursor)
{
    /* assume SVG cannot be pjl embedded */
    pcursor->ptr = pcursor->limit;
    return 0;
}

/* Parser action for end-of-file */
static int
svg_imp_process_eof(pl_interp_instance_t *pinstance)
{
    svg_interp_instance_t *instance = (svg_interp_instance_t *)pinstance;
    svg_context_t *ctx = instance->ctx;
    svg_item_t *root;
    int code;

    dputs("-- svg_imp_process_eof --\n");

    root = svg_close_xml_parser(ctx);
    if (!root)
	return gs_rethrow(-1, "cannot parse xml document");

    code = svg_parse_document(ctx, root);
    if (code < 0)
	gs_catch(code, "cannot parse SVG document");

    svg_free_item(ctx, root);

    return code;
}

/* Report any errors after running a job */
static int
svg_imp_report_errors(pl_interp_instance_t *pinstance,
	int code,           /* prev termination status */
	long file_position, /* file position of error, -1 if unknown */
	bool force_to_cout  /* force errors to cout */
	)
{
    return 0;
}

/* Prepare interp instance for the next "job" */
static int
svg_imp_init_job(pl_interp_instance_t *pinstance)
{
    svg_interp_instance_t *instance = (svg_interp_instance_t *)pinstance;
    svg_context_t *ctx = instance->ctx;

    dputs("-- svg_imp_init_job --\n");

    return svg_open_xml_parser(ctx);
}

/* Wrap up interp instance after a "job" */
static int
svg_imp_dnit_job(pl_interp_instance_t *pinstance)
{
    dputs("-- svg_imp_dnit_job --\n");

    return 0;
}

/* Remove a device from an interperter instance */
static int
svg_imp_remove_device(pl_interp_instance_t *pinstance)
{
    svg_interp_instance_t *instance = (svg_interp_instance_t *)pinstance;
    svg_context_t *ctx = instance->ctx;

    int code = 0;       /* first error status encountered */
    int error;

    dputs("-- svg_imp_remove_device --\n");

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
static int
svg_imp_deallocate_interp_instance(pl_interp_instance_t *pinstance)
{
    svg_interp_instance_t *instance = (svg_interp_instance_t *)pinstance;
    svg_context_t *ctx = instance->ctx;
    gs_memory_t *mem = ctx->memory;

    dputs("-- svg_imp_deallocate_interp_instance --\n");

    /* language clients don't free the font cache machinery */

    // free gstate?
    gs_free_object(mem, ctx, "svg_imp_deallocate_interp_instance");
    gs_free_object(mem, instance, "svg_imp_deallocate_interp_instance");

    return 0;
}

/* Do static deinit of SVG interpreter */
static int
svg_imp_deallocate_interp(pl_interp_t *pinterp)
{
    /* nothing to do */
    return 0;
}

/* Parser implementation descriptor */
const pl_interp_implementation_t svg_implementation =
{
    svg_imp_characteristics,
    svg_imp_allocate_interp,
    svg_imp_allocate_interp_instance,
    svg_imp_set_client_instance,
    svg_imp_set_pre_page_action,
    svg_imp_set_post_page_action,
    svg_imp_set_device,
    svg_imp_init_job,
    svg_imp_process_file,
    svg_imp_process,
    svg_imp_flush_to_eoj,
    svg_imp_process_eof,
    svg_imp_report_errors,
    svg_imp_dnit_job,
    svg_imp_remove_device,
    svg_imp_deallocate_interp_instance,
    svg_imp_deallocate_interp,
    svg_imp_get_device_memory,
};

/*
 * End-of-page function called by SVG parser.
 */
int
svg_show_page(svg_context_t *ctx, int num_copies, int flush)
{
    pl_interp_instance_t *pinstance = ctx->instance;
    svg_interp_instance_t *instance = ctx->instance;

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

/*
 * We need to install a halftone ourselves, this is not
 * done automatically.
 */

static float
identity_transfer(floatp tint, const gx_transfer_map *ignore_map)
{
    return tint;
}

/* The following is a 45 degree spot screen with the spots enumerated
 * in a defined order. */
static byte order16x16[256] = {
    38, 11, 14, 32, 165, 105, 90, 171, 38, 12, 14, 33, 161, 101, 88, 167,
    30, 6, 0, 16, 61, 225, 231, 125, 30, 6, 1, 17, 63, 222, 227, 122,
    27, 3, 8, 19, 71, 242, 205, 110, 28, 4, 9, 20, 74, 246, 208, 106,
    35, 24, 22, 40, 182, 46, 56, 144, 36, 25, 22, 41, 186, 48, 58, 148,
    152, 91, 81, 174, 39, 12, 15, 34, 156, 95, 84, 178, 40, 13, 16, 34,
    69, 212, 235, 129, 31, 7, 2, 18, 66, 216, 239, 133, 32, 8, 2, 18,
    79, 254, 203, 114, 28, 4, 10, 20, 76, 250, 199, 118, 29, 5, 10, 21,
    193, 44, 54, 142, 36, 26, 23, 42, 189, 43, 52, 139, 37, 26, 24, 42,
    39, 12, 15, 33, 159, 99, 87, 169, 38, 11, 14, 33, 163, 103, 89, 172,
    31, 7, 1, 17, 65, 220, 229, 123, 30, 6, 1, 17, 62, 223, 233, 127,
    28, 4, 9, 20, 75, 248, 210, 108, 27, 3, 9, 19, 72, 244, 206, 112,
    36, 25, 23, 41, 188, 49, 60, 150, 35, 25, 22, 41, 184, 47, 57, 146,
    157, 97, 85, 180, 40, 13, 16, 35, 154, 93, 83, 176, 39, 13, 15, 34,
    67, 218, 240, 135, 32, 8, 3, 19, 70, 214, 237, 131, 31, 7, 2, 18,
    78, 252, 197, 120, 29, 5, 11, 21, 80, 255, 201, 116, 29, 5, 10, 21,
    191, 43, 51, 137, 37, 27, 24, 43, 195, 44, 53, 140, 37, 26, 23, 42
};

#define source_phase_x 4
#define source_phase_y 0

static int
svg_install_halftone(svg_context_t *ctx, gx_device *pdevice)
{
    gs_halftone ht;
    gs_string thresh;
    int code;

    int width = 16;
    int height = 16;
    thresh.data = order16x16;
    thresh.size = width * height;

    if (gx_device_must_halftone(pdevice))
    {
	ht.type = ht_type_threshold;
	ht.params.threshold.width = width;
	ht.params.threshold.height = height;
	ht.params.threshold.thresholds.data = thresh.data;
	ht.params.threshold.thresholds.size = thresh.size;
	ht.params.threshold.transfer = 0;
	ht.params.threshold.transfer_closure.proc = 0;

	gs_settransfer(ctx->pgs, identity_transfer);

	code = gs_sethalftone(ctx->pgs, &ht);
	if (code < 0)
	    return gs_throw(code, "could not install halftone");

	code = gs_sethalftonephase(ctx->pgs, 0, 0);
	if (code < 0)
	    return gs_throw(code, "could not set halftone phase");
    }

    return 0;
}

