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

/* Top-level API implementation of PDF */

/* Language wrapper implementation (see pltop.h) */

#include "ghostpdf.h"

#include "pltop.h"
#include "plmain.h"

#include "plparse.h" /* for e_ExitLanguage */
#include "plmain.h"
#include "gxdevice.h" /* so we can include gxht.h below */
#include "gxht.h" /* gsht1.h is incomplete, we need storage size of gs_halftone */
#include "gsht1.h"

#define PDF_PARSER_MIN_INPUT_SIZE (8192 * 4)

/*
 * The PDF interpreter instance is derived from pl_interp_implementation_t.
 */

typedef struct pdf_interp_instance_s
{
    gs_memory_t *memory;                /* memory allocator to use */

    pdf_context *ctx;
    FILE *scratch_file;
    char scratch_name[gp_file_name_sizeof];
}pdf_interp_instance_t;

/* version and build date are not currently used */
#define PDF_VERSION NULL
#define PDF_BUILD_DATE NULL

static const pl_interp_characteristics_t *
pdf_imp_characteristics(const pl_interp_implementation_t *pimpl)
{
    static pl_interp_characteristics_t pdf_characteristics =
    {
        "PDF",
        "%!PDF", /* string to recognize PDF files */
        "Artifex",
        PDF_VERSION,
        PDF_BUILD_DATE,
        PDF_PARSER_MIN_INPUT_SIZE, /* Minimum input size */
    };
    return &pdf_characteristics;
}

#if 0 /* the following are not currently used */
static void
pdf_set_nocache(pl_interp_implementation_t *impl, gs_font_dir *font_dir)
{
    bool nocache;
    pdf_interp_instance_t *pdfi  = impl->interp_client_data;
    nocache = pl_main_get_nocache(pdfi->memory);
    if (nocache)
        gs_setcachelimit(font_dir, 0);
    return;
}


static int
pdf_set_icc_user_params(pl_interp_implementation_t *impl, gs_gstate *pgs)
{
    pdf_interp_instance_t *pdfi  = impl->interp_client_data;
    return pl_set_icc_params(pdfi->memory, pgs);
}

#endif

/* Do per-instance interpreter allocation/init. No device is set yet */
static int
pdf_imp_allocate_interp_instance(pl_interp_implementation_t *impl,
                                 gs_memory_t *pmem)
{
    pdf_interp_instance_t *instance;
    pdf_context *ctx;

    instance = (pdf_interp_instance_t *) gs_alloc_bytes(pmem,
            sizeof(pdf_interp_instance_t), "pdf_imp_allocate_interp_instance");

    if (!instance)
        return gs_error_VMerror;

    ctx = pdfi_create_context(pmem);

    if (ctx == NULL) {
        gs_free_object(pmem, instance, "pdf_imp_allocate_interp_instance");
        return gs_error_VMerror;
    }

    ctx->instance = instance;
    instance->ctx = ctx;
    instance->scratch_file = NULL;
    instance->scratch_name[0] = 0;
    instance->memory = pmem;
    
    impl->interp_client_data = instance;

    return 0;
}

static int
pdf_imp_set_device(pl_interp_implementation_t *impl, gx_device *pdevice)
{
    pdf_interp_instance_t *instance = impl->interp_client_data;
    pdf_context *ctx = instance->ctx;
    gs_c_param_list list;
    int code;

    code = gs_opendevice(pdevice);
    if (code < 0)
        goto cleanup_setdevice;

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

/* Parse an entire random access file */
static int
pdf_imp_process_file(pl_interp_implementation_t *impl, char *filename)
{
    pdf_interp_instance_t *instance = impl->interp_client_data;
    pdf_context *ctx = instance->ctx;
    int code;
    pl_main_instance_t *imain;

    imain = pl_main_get_instance(ctx->memory);

    ctx->first_page = imain->first_page;             /* -dFirstPage= */
    ctx->last_page = imain->last_page;              /* -dLastPage= */
    ctx->pdfdebug = imain->pdfdebug;
    ctx->pdfstoponerror = imain->pdfstoponerror;
    ctx->pdfstoponwarning = imain->pdfstoponwarning;
    ctx->notransparency = imain->notransparency;
    ctx->nocidfallback = imain->nocidfallback;
    ctx->no_pdfmark_outlines = imain->no_pdfmark_outlines;
    ctx->no_pdfmark_dests = imain->no_pdfmark_dests;
    ctx->pdffitpage = imain->pdffitpage;
    ctx->usecropbox = imain->usecropbox;
    ctx->useartbox = imain->useartbox;
    ctx->usebleedbox = imain->usebleedbox;
    ctx->usetrimbox = imain->usetrimbox;
    ctx->printed = imain->printed;
    ctx->showacroform = imain->showacroform;
    ctx->showannots = imain->showannots;
    ctx->nouserunit = imain->nouserunit;
    ctx->renderttnotdef = imain->renderttnotdef;

    if (imain->PDFPassword) {
        ctx->PDFPassword = (char *)gs_alloc_bytes(ctx->memory, strlen(imain->PDFPassword) + 1, "allocate PDFpassword parameter");
        if (ctx->PDFPassword == NULL) {
            gs_free_object(ctx->memory, ctx, "pdf_imp_allocate_interp_instance");
            gs_free_object(ctx->memory, instance, "pdf_imp_allocate_interp_instance");
            return gs_error_VMerror;
        }
        memset(ctx->PDFPassword, 0x00, strlen(imain->PDFPassword) + 1);
        strcpy(ctx->PDFPassword, imain->PDFPassword);
    }
    if(imain->PageList) {
        ctx->PageList = (char *)gs_alloc_bytes(ctx->memory, strlen(imain->PageList) + 1, "allocate PDFpassword parameter");
        if (ctx->PageList == NULL) {
            if (ctx->PDFPassword != NULL) {
                gs_free_object(ctx->memory, ctx->PDFPassword, "pdf_imp_allocate_interp_instance");
                ctx->PDFPassword = NULL;
            }
            gs_free_object(ctx->memory, ctx, "pdf_imp_allocate_interp_instance");
            gs_free_object(ctx->memory, instance, "pdf_imp_allocate_interp_instance");
            return gs_error_VMerror;
        }
        memset(ctx->PageList, 0x00, strlen(imain->PageList) + 1);
        strcpy(ctx->PageList, imain->PageList);
    }

    code = pdfi_process_pdf_file(ctx, filename);
    if (code)
        return code;

    return 0;
}

/* Parse a cursor-full of data */
static int
pdf_imp_process(pl_interp_implementation_t *impl, stream_cursor_read *cursor)
{
    pdf_interp_instance_t *instance = impl->interp_client_data;
    pdf_context *ctx = instance->ctx;
    int avail, n;

    if (!instance->scratch_file)
    {
        instance->scratch_file = gp_open_scratch_file(ctx->memory,
            "ghostpdf-scratch-", instance->scratch_name, "wb");
        if (!instance->scratch_file)
        {
            gs_catch(gs_error_invalidfileaccess, "cannot open scratch file");
            return e_ExitLanguage;
        }
        if_debug1m('|', ctx->memory, "pdf: open scratch file '%s'\n", instance->scratch_name);
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

/* Skip to end of job.
 * Return 1 if done, 0 ok but EOJ not found, else negative error code.
 */
static int
pdf_imp_flush_to_eoj(pl_interp_implementation_t *impl, stream_cursor_read *pcursor)
{
    /* assume PDF cannot be pjl embedded */
    pcursor->ptr = pcursor->limit;
    return 0;
}

/* Parser action for end-of-file */
static int
pdf_imp_process_eof(pl_interp_implementation_t *impl)
{
    pdf_interp_instance_t *instance = impl->interp_client_data;
    pdf_context *ctx = instance->ctx;
    int code;

    if (instance->scratch_file)
    {
        if_debug0m('|', ctx->memory, "pdf: executing scratch file\n");
        fclose(instance->scratch_file);
        instance->scratch_file = NULL;
        code = pdfi_process_pdf_file(ctx, instance->scratch_name);
        unlink(instance->scratch_name);
        if (code < 0)
        {
            gs_catch(code, "cannot process PDF file");
            return e_ExitLanguage;
        }
    }

    return 0;
}

/* Report any errors after running a job */
static int
pdf_imp_report_errors(pl_interp_implementation_t *impl,
        int code,           /* prev termination status */
        long file_position, /* file position of error, -1 if unknown */
        bool force_to_cout  /* force errors to cout */
        )
{
    return 0;
}

/* Prepare interp instance for the next "job" */
static int
pdf_imp_init_job(pl_interp_implementation_t *impl)
{
    pdf_interp_instance_t *instance = impl->interp_client_data;
    pdf_context *ctx = instance->ctx;

    ctx->use_transparency = 1;
    if (getenv("PDF_DISABLE_TRANSPARENCY"))
        ctx->use_transparency = 0;

    return 0;
}

/* Wrap up interp instance after a "job" */
static int
pdf_imp_dnit_job(pl_interp_implementation_t *impl)
{
    return 0;
}

/* Remove a device from an interperter instance */
static int
pdf_imp_remove_device(pl_interp_implementation_t *impl)
{
    pdf_interp_instance_t *instance = impl->interp_client_data;
    pdf_context *ctx = instance->ctx;

    /* return to original gstate */
    return gs_grestore_only(ctx->pgs); /* destroys gs_save stack */
}

/* Deallocate a interpreter instance */
static int
pdf_imp_deallocate_interp_instance(pl_interp_implementation_t *impl)
{
    pdf_interp_instance_t *instance = impl->interp_client_data;
    pdf_context *ctx = instance->ctx;
    gs_memory_t *mem = ctx->memory;
    int code = 0;

    code = pdfi_free_context(mem, ctx);

    gs_free_object(mem, instance, "pdf_imp_deallocate_interp_instance");

    return code;
}

/* Parser implementation descriptor */
pl_interp_implementation_t pdf_implementation =
{
    pdf_imp_characteristics,
    pdf_imp_allocate_interp_instance,
    pdf_imp_set_device,
    pdf_imp_init_job,
    pdf_imp_process_file,
    pdf_imp_process,
    pdf_imp_flush_to_eoj,
    pdf_imp_process_eof,
    pdf_imp_report_errors,
    pdf_imp_dnit_job,
    pdf_imp_remove_device,
    pdf_imp_deallocate_interp_instance,
    NULL,
};

