/* Copyright (C) 2001-2018 Artifex Software, Inc.
   All Rights Reserved.

   This software is provided AS-IS with no warranty, either express or
   implied.

   This software is distributed under license and may not be copied,
   modified or distributed except as expressly authorized under the terms
   of the license contained in the file LICENSE in this distribution.

   Refer to licensing information at http://www.artifex.com or contact
   Artifex Software, Inc.,  1305 Grant Avenue - Suite 200, Novato,
   CA 94945, U.S.A., +1(415)492-9861, for further information.
*/

/* Top-level API implementation of XML Paper Specification */

/* Language wrapper implementation (see pltop.h) */

#include "ghostxps.h"

#include "pltop.h"
#include "plmain.h"

#include "plparse.h" /* for e_ExitLanguage */
#include "plmain.h"
#include "gxdevice.h" /* so we can include gxht.h below */
#include "gxht.h" /* gsht1.h is incomplete, we need storage size of gs_halftone */
#include "gsht1.h"

int xps_zip_trace = 0;
int xps_doc_trace = 0;

static int xps_install_halftone(xps_context_t *ctx, gx_device *pdevice);

#define XPS_PARSER_MIN_INPUT_SIZE (8192 * 4)

/*
 * The XPS interpeter is identical to pl_interp_t.
 * The XPS interpreter instance is derived from pl_interp_implementation_t.
 */

typedef struct xps_interp_instance_s xps_interp_instance_t;

struct xps_interp_instance_s
{
    gs_memory_t *memory;                /* memory allocator to use */

    xps_context_t *ctx;
    FILE *scratch_file;
    char scratch_name[gp_file_name_sizeof];
};

/* version and build date are not currently used */
#define XPS_VERSION NULL
#define XPS_BUILD_DATE NULL

static int
xps_detect_language(const char *s, int len)
{
    if (len < 2)
        return 0;
    if (memcmp(s, "PK", 2) == 0)
        return 100;
    return 0;
}

static const pl_interp_characteristics_t *
xps_imp_characteristics(const pl_interp_implementation_t *pimpl)
{
    static pl_interp_characteristics_t xps_characteristics =
    {
        "XPS",
        xps_detect_language,
        "Artifex",
        XPS_VERSION,
        XPS_BUILD_DATE,
        XPS_PARSER_MIN_INPUT_SIZE, /* Minimum input size */
    };
    return &xps_characteristics;
}

static void
xps_set_nocache(pl_interp_implementation_t *impl, gs_font_dir *font_dir)
{
    bool nocache;
    xps_interp_instance_t *xpsi  = impl->interp_client_data;
    nocache = pl_main_get_nocache(xpsi->memory);
    if (nocache)
        gs_setcachelimit(font_dir, 0);
    return;
}


static int
xps_set_icc_user_params(pl_interp_implementation_t *impl, gs_gstate *pgs)
{
    xps_interp_instance_t *xpsi  = impl->interp_client_data;
    return pl_set_icc_params(xpsi->memory, pgs);
}

/* Do per-instance interpreter allocation/init. No device is set yet */
static int
xps_imp_allocate_interp_instance(pl_interp_implementation_t *impl,
                                 gs_memory_t *pmem)
{
    xps_interp_instance_t *instance;
    xps_context_t *ctx;
    gs_gstate *pgs;

    instance = (xps_interp_instance_t *) gs_alloc_bytes(pmem,
            sizeof(xps_interp_instance_t), "xps_imp_allocate_interp_instance");

    ctx = (xps_context_t *) gs_alloc_bytes(pmem,
            sizeof(xps_context_t), "xps_imp_allocate_interp_instance");

    pgs = gs_gstate_alloc(pmem);

    if (!instance || !ctx || !pgs)
    {
        if (instance)
            gs_free_object(pmem, instance, "xps_imp_allocate_interp_instance");
        if (ctx)
            gs_free_object(pmem, ctx, "xps_imp_allocate_interp_instance");
        if (pgs)
            gs_gstate_free(pgs);
        return gs_error_VMerror;
    }

    gsicc_init_iccmanager(pgs);
    memset(ctx, 0, sizeof(xps_context_t));

    ctx->instance = instance;
    ctx->memory = pmem;
    ctx->pgs = pgs;
    /* Declare PDL client support for high level patterns, for the benefit
     * of pdfwrite and other high-level devices
     */
    ctx->pgs->have_pattern_streams = true;
    ctx->fontdir = NULL;
    ctx->preserve_tr_mode = 0;

    ctx->file = NULL;
    ctx->zip_count = 0;
    ctx->zip_table = NULL;

    /* Gray, RGB and CMYK profiles set when color spaces installed in graphics lib */
    ctx->gray_lin = gs_cspace_new_ICC(ctx->memory, ctx->pgs, -1);
    ctx->gray = gs_cspace_new_ICC(ctx->memory, ctx->pgs, 1);
    ctx->cmyk = gs_cspace_new_ICC(ctx->memory, ctx->pgs, 4);
    ctx->srgb = gs_cspace_new_ICC(ctx->memory, ctx->pgs, 3);
    ctx->scrgb = gs_cspace_new_ICC(ctx->memory, ctx->pgs, 3);

    instance->ctx = ctx;
    instance->scratch_file = NULL;
    instance->scratch_name[0] = 0;
    instance->memory = pmem;
    
    /* NB needs error handling */
    ctx->fontdir = gs_font_dir_alloc(ctx->memory);

    gs_setaligntopixels(ctx->fontdir, 1); /* no subpixels */
    gs_setgridfittt(ctx->fontdir, 1); /* see gx_ttf_outline in gxttfn.c for values */

    impl->interp_client_data = instance;

    return 0;
}

/* Prepare interp instance for the next "job" */
static int
xps_imp_init_job(pl_interp_implementation_t *impl,
                 gx_device                  *pdevice)
{
    xps_interp_instance_t *instance = impl->interp_client_data;
    xps_context_t *ctx = instance->ctx;
    gs_c_param_list list;
    int code;

    if (gs_debug_c('|'))
        xps_zip_trace = 1;
    if (gs_debug_c('|'))
        xps_doc_trace = 1;

    ctx->font_table = xps_hash_new(ctx);
    ctx->colorspace_table = xps_hash_new(ctx);

    ctx->start_part = NULL;

    ctx->use_transparency = 1;
    if (getenv("XPS_DISABLE_TRANSPARENCY"))
        ctx->use_transparency = 0;

    ctx->opacity_only = 0;
    ctx->fill_rule = 0;

    code = gs_setdevice_no_erase(ctx->pgs, pdevice);
    if (code < 0)
        goto cleanup_setdevice;

    /* Check if the device wants PreserveTrMode (pdfwrite) */
    gs_c_param_list_write(&list, pdevice->memory);
    code = gs_getdeviceparams(pdevice, (gs_param_list *)&list);
    if (code >= 0) {
        gs_c_param_list_read(&list);
        code = param_read_bool((gs_param_list *)&list, "PreserveTrMode", &ctx->preserve_tr_mode);
    }
    gs_c_param_list_release(&list);
    if (code < 0)
        return code;

    gs_setaccuratecurves(ctx->pgs, true); /* NB not sure */
    gs_setfilladjust(ctx->pgs, 0, 0);
    (void)xps_set_icc_user_params(impl, ctx->pgs);
    xps_set_nocache(impl, ctx->fontdir);

    gs_setscanconverter(ctx->pgs, pl_main_get_scanconverter(ctx->memory));    

    /* gsave and grestore (among other places) assume that */
    /* there are at least 2 gstates on the graphics stack. */
    /* Ensure that now. */
    code = gs_gsave(ctx->pgs);
    if (code < 0)
        goto cleanup_gsave;

    code = gs_erasepage(ctx->pgs);
    if (code < 0)
        goto cleanup_erase;

    code = xps_install_halftone(ctx, pdevice);
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

/* Parse an entire random access file */
static int
xps_imp_process_file(pl_interp_implementation_t *impl, char *filename)
{
    xps_interp_instance_t *instance = impl->interp_client_data;
    xps_context_t *ctx = instance->ctx;
    int code;

    code = xps_process_file(ctx, filename);
    if (code)
        gs_catch1(code, "cannot process xps file '%s'", filename);

    return code;
}

/* Do any setup for parser per-cursor */
static int                      /* ret 0 or +ve if ok, else -ve error code */
xps_impl_process_begin(pl_interp_implementation_t * impl)
{
    return 0;
}

/* Parse a cursor-full of data */
static int
xps_imp_process(pl_interp_implementation_t *impl, stream_cursor_read *cursor)
{
    xps_interp_instance_t *instance = impl->interp_client_data;
    xps_context_t *ctx = instance->ctx;
    int avail, n;

    if (!instance->scratch_file)
    {
        instance->scratch_file = gp_open_scratch_file(ctx->memory,
            "ghostxps-scratch-", instance->scratch_name, "wb");
        if (!instance->scratch_file)
        {
            gs_catch(gs_error_invalidfileaccess, "cannot open scratch file");
            return e_ExitLanguage;
        }
        if_debug1m('|', ctx->memory, "xps: open scratch file '%s'\n", instance->scratch_name);
    }

    avail = cursor->limit - cursor->ptr;
    n = fwrite(cursor->ptr + 1, 1, avail, instance->scratch_file);
    if (n != avail)
    {
        gs_catch(gs_error_invalidfileaccess, "cannot write to scratch file");
        return e_ExitLanguage;
    }
    cursor->ptr = cursor->limit;

    return 0;
}

static int                      /* ret 0 or +ve if ok, else -ve error code */
xps_impl_process_end(pl_interp_implementation_t * impl)
{
    return 0;
}

/* Skip to end of job.
 * Return 1 if done, 0 ok but EOJ not found, else negative error code.
 */
static int
xps_imp_flush_to_eoj(pl_interp_implementation_t *impl, stream_cursor_read *pcursor)
{
    /* assume XPS cannot be pjl embedded */
    pcursor->ptr = pcursor->limit;
    return 0;
}

/* Parser action for end-of-file */
static int
xps_imp_process_eof(pl_interp_implementation_t *impl)
{
    xps_interp_instance_t *instance = impl->interp_client_data;
    xps_context_t *ctx = instance->ctx;
    int code;

    if (instance->scratch_file)
    {
        if_debug0m('|', ctx->memory, "xps: executing scratch file\n");
        fclose(instance->scratch_file);
        instance->scratch_file = NULL;
        code = xps_process_file(ctx, instance->scratch_name);
        unlink(instance->scratch_name);
        if (code < 0)
        {
            gs_catch(code, "cannot process XPS file");
            return e_ExitLanguage;
        }
    }

    return 0;
}

/* Report any errors after running a job */
static int
xps_imp_report_errors(pl_interp_implementation_t *impl,
        int code,           /* prev termination status */
        long file_position, /* file position of error, -1 if unknown */
        bool force_to_cout  /* force errors to cout */
        )
{
    return 0;
}

static void xps_free_key_func(xps_context_t *ctx, void *ptr)
{
    xps_free(ctx, ptr);
}

static void xps_free_font_func(xps_context_t *ctx, void *ptr)
{
    xps_free_font(ctx, ptr);
}

/* Wrap up interp instance after a "job" */
static int
xps_imp_dnit_job(pl_interp_implementation_t *impl)
{
    xps_interp_instance_t *instance = impl->interp_client_data;
    xps_context_t *ctx = instance->ctx;
    int i, code;

    /* return to original gstate */
    code = gs_grestore_only(ctx->pgs); /* destroys gs_save stack */

    if (gs_debug_c('|'))
        xps_debug_fixdocseq(ctx);

    for (i = 0; i < ctx->zip_count; i++)
        xps_free(ctx, ctx->zip_table[i].name);
    xps_free(ctx, ctx->zip_table);

    /* TODO: free resources too */
    xps_hash_free(ctx, ctx->font_table, xps_free_key_func, xps_free_font_func);
    xps_hash_free(ctx, ctx->colorspace_table, xps_free_key_func, NULL);

    xps_free_fixed_pages(ctx);
    xps_free_fixed_documents(ctx);

    return code;
}

/* Deallocate a interpreter instance */
static int
xps_imp_deallocate_interp_instance(pl_interp_implementation_t *impl)
{
    xps_interp_instance_t *instance = impl->interp_client_data;
    xps_context_t *ctx = instance->ctx;
    gs_memory_t *mem = ctx->memory;

    /* language clients don't free the font cache machinery */
    rc_decrement_cs(ctx->gray_lin, "xps_imp_deallocate_interp_instance");
    rc_decrement_cs(ctx->gray, "xps_imp_deallocate_interp_instance");
    rc_decrement_cs(ctx->cmyk, "xps_imp_deallocate_interp_instance");
    rc_decrement_cs(ctx->srgb, "xps_imp_deallocate_interp_instance");
    rc_decrement_cs(ctx->scrgb, "xps_imp_deallocate_interp_instance");

    gs_gstate_free(ctx->pgs);

    gs_free_object(mem, ctx->fontdir, "xps_imp_deallocate_interp_instance");
    gs_free_object(mem, ctx, "xps_imp_deallocate_interp_instance");
    gs_free_object(mem, instance, "xps_imp_deallocate_interp_instance");

    return 0;
}

/* Parser implementation descriptor */
pl_interp_implementation_t xps_implementation =
{
    xps_imp_characteristics,
    xps_imp_allocate_interp_instance,
    NULL,
    NULL,
    NULL,
    xps_imp_init_job,
    xps_imp_process_file,
    xps_impl_process_begin,
    xps_imp_process,
    xps_impl_process_end,
    xps_imp_flush_to_eoj,
    xps_imp_process_eof,
    xps_imp_report_errors,
    xps_imp_dnit_job,
    xps_imp_deallocate_interp_instance,
    NULL,
};

/*
 * End-of-page function called by XPS parser.
 */
int
xps_show_page(xps_context_t *ctx, int num_copies, int flush)
{
    return pl_finish_page(ctx->memory->gs_lib_ctx->top_of_system,
                          ctx->pgs, num_copies, flush);
}

/*
 * We need to install a halftone ourselves, this is not
 * done automatically.
 */

static float
identity_transfer(double tint, const gx_transfer_map *ignore_map)
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
xps_install_halftone(xps_context_t *ctx, gx_device *pdevice)
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
