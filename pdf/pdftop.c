/* Copyright (C) 2018-2024 Artifex Software, Inc.
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

/* Top-level API implementation of PDF */

/* Language wrapper implementation (see pltop.h) */

#include "ghostpdf.h"

#include "pltop.h"
#include "plmain.h"

#include "plparse.h"        /* for e_ExitLanguage */
#include "gxdevice.h"       /* so we can include gxht.h below */
#include "gxht.h"           /* gsht1.h is incomplete, we need storage size of gs_halftone */
#include "gsht1.h"
#include "pdf_device.h"
#include "pdf_misc.h"

#include "gsstate.h"        /* For gs_sethalftonephase() */
#include "gspaint.h"        /* For gs_erasepage() */
#include "gscolor3.h"       /* For gs_setsmoothness() */

extern const char gp_file_name_list_separator;

static int pdfi_install_halftone(pdf_context *ctx, gx_device *pdevice);
static int pdf_impl_add_path(pl_interp_implementation_t *impl, const char *path);

/*
 * The PDF interpreter instance is derived from pl_interp_implementation_t.
 */

typedef struct pdf_interp_instance_s
{
    gs_memory_t *memory;                /* memory allocator to use */

    pdf_context *ctx;
    gp_file *scratch_file;
    char scratch_name[gp_file_name_sizeof];
}pdf_interp_instance_t;

extern const char gp_file_name_list_separator;

#define GS_LIB_DEFAULT_STRING GS_STRINGIZE(GS_LIB_DEFAULT)

static int
pdf_detect_language(const char *s, int len)
{
    if (len >= 5 && memcmp(s, "%PDF-", 5) == 0)
        return 100;
    return 0;
}

static const pl_interp_characteristics_t *
pdf_impl_characteristics(const pl_interp_implementation_t *pimpl)
{
    static pl_interp_characteristics_t pdf_characteristics =
    {
        "PDF",
        pdf_detect_language,
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

static int
pdf_impl_finish_page(pdf_context *ctx)
{
    return pl_finish_page(ctx->memory->gs_lib_ctx->top_of_system, ctx->pgs, 1, true);
}

/* Do per-instance interpreter allocation/init. No device is set yet */
static int
pdf_impl_allocate_interp_instance(pl_interp_implementation_t *impl,
                                 gs_memory_t *pmem)
{
    pdf_interp_instance_t *instance;
    pdf_context *ctx;
    int code;
    const char *rompathstr = "%rom%Resource/Init";
    const char *deflibstr = GS_LIB_DEFAULT_STRING;
    char *newpathsstr = (char *)deflibstr;

    instance = (pdf_interp_instance_t *) gs_alloc_bytes(pmem,
            sizeof(pdf_interp_instance_t), "pdf_impl_allocate_interp_instance");

    if (!instance)
        return gs_error_VMerror;

    ctx = pdfi_create_context(pmem);

    if (ctx == NULL) {
        gs_free_object(pmem, instance, "pdf_impl_allocate_interp_instance");
        return gs_error_VMerror;
    }

    ctx->finish_page = pdf_impl_finish_page;

    ctx->instance = instance;
    instance->ctx = ctx;
    instance->scratch_file = NULL;
    instance->scratch_name[0] = 0;
    instance->memory = pmem;

    impl->interp_client_data = instance;
    if (COMPILE_INITS == 1) {
        newpathsstr = (char *)gs_alloc_bytes(ctx->memory, strlen(rompathstr) + strlen(GS_LIB_DEFAULT_STRING) + 2, "temp paths string");
        if (newpathsstr == NULL) {
            newpathsstr = (char *)rompathstr;
        }
        else {
            char sepstr[2];

            sepstr[0] = gp_file_name_list_separator;
            sepstr[1] = '\0';

            memcpy(newpathsstr, rompathstr, strlen(rompathstr) + 1);
            strncat(newpathsstr, sepstr, strlen(sepstr));
            strncat(newpathsstr, GS_LIB_DEFAULT_STRING, strlen(GS_LIB_DEFAULT_STRING));
        }
    }

    code = pdfi_add_initial_paths_to_search_paths(ctx, newpathsstr, strlen(newpathsstr));
    if (newpathsstr != deflibstr && newpathsstr != rompathstr) {
        gs_free_object(ctx->memory, newpathsstr, "temp paths string");
    }
    return code;
}

static int
pdf_impl_set_device(pl_interp_implementation_t *impl, gx_device *pdevice)
{
    pdf_interp_instance_t *instance = impl->interp_client_data;
    pdf_context *ctx = instance->ctx;
    int code;

#if 0
    gx_device_fill_in_procs(pmi->device);
    code = gs_opendevice(pdevice);
    if (code < 0)
        goto cleanup_setdevice;
#endif

    /* gsave and grestore (among other places) assume that */
    /* there are at least 2 gstates on the graphics stack. */
    /* Ensure that now. */
    code = gs_gsave(ctx->pgs);
    if (code < 0)
        goto cleanup_gsave;

    code = gs_setdevice_no_erase(ctx->pgs, pdevice);
    if (code < 0)
        goto cleanup_setdevice;

    /* TODO: See stuff with init_graphics in pdfi_page_render -- I think this
     * should be collected in one place?
     * This is essentially doing it one time, and the other is doing it per page.
     * The main thing seems to be doing it here is before the pdfwrite device
     * gets setup, and doing it in pdf_page.c is after the pdfwrite device gets
     * setup.  It can cause spurious 'gs' state entries if there are discrepencies.
     */
    /* Doing this here to avoid having a ".02 /SM" entry showing up in ExtGState
     * of pdfwrite output.
     */
    code = gs_setsmoothness(ctx->pgs, 0.02); /* Match gs code */

    /* NOTE: Not sure what this is or when it matters, but it should match gs, which sets it to true */
    /* TODO: Commented out because it caused a seemingly random incorrect pdfwrite output.
     * Will try putting it back in when pdfwrite is more stable.
     */
    //gs_setlimitclamp(ctx->pgs, true); /* Match gs code */

    /* We need to setup strokeadjust the same way gs does, which is currently based on
     * the device resolution and the DITHERPPI arg.
     *
     * TODO: Ken thinks 150 dpi is too low for the cutoff, but if so it should
     * change in both gs and here, so they match.  See gs_init.ps/.useloresscreen
     *
     * TODO: Currently pdfi-in-gs is not inheriting the graphics state from gs, so it
     * will have the wrong setting at 300dpi.  That needs to be fixed.
     * (but pdfi-in-gs never actually calls this function, it is supposed to inherit
     * the device and graphics state from gs.  Remove this comment when that is fixed :)
     */
    if (pdevice->HWResolution[0] >= 150 || pdevice->HWResolution[1] >= 150 || ctx->args.ditherppi) {
        /* false for "high resolution" devices */
        gs_setstrokeadjust(ctx->pgs, false);
    } else {
        /* true for "low resolution" devices */
        gs_setstrokeadjust(ctx->pgs, true);
    }

    gs_setscanconverter(ctx->pgs, pl_main_get_scanconverter(ctx->memory));

    code = gs_erasepage(ctx->pgs);
    if (code < 0)
        goto cleanup_erase;

    code = pdfi_install_halftone(ctx, pdevice);
    if (code < 0)
        goto cleanup_halftone;

    /* TODO: Hack to do what is in the bottom of gs_pdfwr.ps
     * This basically causes the pdfwrite device to be initialized.
     * Not sure if this is the correct place to do this.
     * Note that if running gs/pdfi, it will happen in the gs interpreter.
     * Putting it here means it only runs in gpdf, which seems correct.
     */
    if (pdfi_device_check_param_exists(pdevice, "ForOPDFRead")) {
        gs_newpath(ctx->pgs);
        gs_fill(ctx->pgs);
    }

    ctx->job_gstate_level = ctx->pgs->level;
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
pdf_impl_process_file(pl_interp_implementation_t *impl, const char *filename)
{
    pdf_interp_instance_t *instance = impl->interp_client_data;
    pdf_context *ctx = instance->ctx;
    int code;

    code = pdfi_process_pdf_file(ctx, (char *)filename);
    if (code)
        return code;

    return 0;
}

static int
pdf_impl_process_begin(pl_interp_implementation_t * impl)
{
    return 0;
}

/* Parse a cursor-full of data */
static int
pdf_impl_process(pl_interp_implementation_t *impl, stream_cursor_read *cursor)
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
    n = gp_fwrite(cursor->ptr + 1, 1, avail, instance->scratch_file);
    if (n != avail)
    {
        gs_catch(gs_error_invalidfileaccess, "cannot write to scratch file");
        return e_ExitLanguage;
    }
    cursor->ptr = cursor->limit;

    return 0;
}

static int                      /* ret 0 or +ve if ok, else -ve error code */
pdf_impl_process_end(pl_interp_implementation_t * impl)
{
    return 0;
}

/* Skip to end of job.
 * Return 1 if done, 0 ok but EOJ not found, else negative error code.
 */
static int
pdf_impl_flush_to_eoj(pl_interp_implementation_t *impl, stream_cursor_read *pcursor)
{
    /* assume PDF cannot be pjl embedded */
    pcursor->ptr = pcursor->limit;
    return 0;
}

/* Parser action for end-of-file */
static int
pdf_impl_process_eof(pl_interp_implementation_t *impl)
{
    pdf_interp_instance_t *instance = impl->interp_client_data;
    pdf_context *ctx = instance->ctx;
    int code;

    if (instance->scratch_file)
    {
        if_debug0m('|', ctx->memory, "pdf: executing scratch file\n");
        gp_fclose(instance->scratch_file);
        instance->scratch_file = NULL;
        code = pdfi_process_pdf_file(ctx, instance->scratch_name);
        gp_unlink(ctx->memory, instance->scratch_name);
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
pdf_impl_report_errors(pl_interp_implementation_t *impl,
        int code,           /* prev termination status */
        long file_position, /* file position of error, -1 if unknown */
        bool force_to_cout  /* force errors to cout */
        )
{
    return 0;
}

#if 0
/*
 * Get the allocator with which to allocate a device
 */
static gs_memory_t *
pdf_impl_get_device_memory(pl_interp_implementation_t *impl)
{
    pdf_interp_instance_t *instance = impl->interp_client_data;
    pdf_context *ctx = instance->ctx;

    return ctx->memory;
}
#endif

static int plist_value_get_int64(gs_param_typed_value *pvalue, int64_t *pint)
{
    if (pvalue->type == gs_param_type_i64) {
        *pint = pvalue->value.i64;
        return 0;
    }
    return_error(gs_error_typecheck);
}

/* Get the value for a string or a name (null terminated) */
static int plist_value_get_string_or_name(pdf_context *ctx, gs_param_typed_value *pvalue,
                                          char **pstr, int *plen, bool *is_name)
{
    const byte *data;
    uint size;

    if (pvalue->type == gs_param_type_string) {
        data = pvalue->value.s.data;
        size = pvalue->value.s.size;
        *is_name = false;
    } else if (pvalue->type == gs_param_type_name) {
        data = pvalue->value.n.data;
        size = pvalue->value.n.size;
        *is_name = true;
    } else {
        return_error(gs_error_typecheck);
    }

    /* Free any previous value, for example if param appears more than once */
    if (*pstr)
        gs_free_object(ctx->memory, *pstr, "plist_value_get_string_or_name()");

    *pstr = (char *)gs_alloc_bytes(ctx->memory, size + 1, "plist_value_get_string_or_name()");
    if (*pstr == NULL)
        return_error(gs_error_VMerror);

    memcpy(*pstr, data, size);
    *((*pstr)+size) = 0; /* Null terminate */
    *plen = size;
    return 0;
}

/* Get the value for a list of names as c-strings, (null terminated) */
/* Format: -s/Item1,/Item2,/Item3 (no white space) */
static int plist_value_get_namelist(pdf_context *ctx, gs_param_typed_value *pvalue,
                                    char ***pstrlist)
{
    char *data;
    uint size;

    if (pvalue->type != gs_param_type_string)
        return_error(gs_error_typecheck);

    data = (char *)pvalue->value.s.data;
    size = pvalue->value.s.size;

    return pdfi_parse_name_cstring_array(ctx, data, size, pstrlist);
}

static int plist_value_get_int(gs_param_typed_value *pvalue, int *pint)
{
    if (pvalue->type == gs_param_type_int) {
        *pint = (int64_t)pvalue->value.i;
        return 0;
    }
    if (pvalue->type == gs_param_type_i64) {
        int64_t i64;
        int code;

        code = plist_value_get_int64(pvalue, &i64);
        if (code < 0)
            return code;

        if (i64 > ((int64_t)1 << 32))
            return_error(gs_error_rangecheck);

        *pint = (int)i64;
        return 0;
    }
    return_error(gs_error_typecheck);
}

static int plist_value_get_bool(gs_param_typed_value *pvalue, bool *pbool)
{
    if (pvalue->type == gs_param_type_bool) {
        *pbool = pvalue->value.b;
        return 0;
    }
    return_error(gs_error_typecheck);
}

#define argis(P, S) \
(!strncmp((P), (S), sizeof(S)-1) && ((P)[sizeof(S)-1] == 0 || (P)[sizeof(S)-1] == '=' || (P)[sizeof(S)-1] == '#'))

static int
pdf_impl_set_param(pl_interp_implementation_t *impl,
                   gs_param_list    *plist)
{
    pdf_interp_instance_t *instance = impl->interp_client_data;
    pdf_context *ctx = instance->ctx;
    gs_param_enumerator_t enumerator;
    gs_param_key_t key;
    int code;
    int len;
    bool discard_isname;
    bool Printed_set = false;

    param_init_enumerator(&enumerator);
    if ((code = param_get_next_key(plist, &enumerator, &key)) == 0) {
        char param[256];	/* big enough for any reasonable key */
        gs_param_typed_value pvalue;

        if (key.size > sizeof(param) - 1) {
            code = gs_note_error(gs_error_rangecheck);
            goto exit;
        }
        memcpy(param, key.data, key.size);
        param[key.size] = 0;
        if ((code = param_read_typed(plist, param, &pvalue)) != 0) {
            code = (code > 0 ? gs_note_error(gs_error_unknownerror) : code);
            goto exit;
        }

        if (argis(param, "QUIET")) {
            code = plist_value_get_bool(&pvalue, &ctx->args.QUIET);
            if (code < 0)
                return code;
        }
        if (argis(param, "FirstPage")) {
            code = plist_value_get_int(&pvalue, &ctx->args.first_page);
            if (code < 0)
                return code;
        }
        if (argis(param, "LastPage")) {
            code = plist_value_get_int(&pvalue, &ctx->args.last_page);
            if (code < 0)
                return code;
        }
        /* PDF interpreter flags */
        if (argis(param, "VerboseErrors")) {
            code = plist_value_get_bool(&pvalue, &ctx->args.verbose_errors);
            if (code < 0)
                return code;
        }
        if (argis(param, "VerboseWarnings")) {
            code = plist_value_get_bool(&pvalue, &ctx->args.verbose_warnings);
            if (code < 0)
                return code;
        }
        if (argis(param, "PDFDEBUG")) {
            code = plist_value_get_bool(&pvalue, &ctx->args.pdfdebug);
            if (code < 0)
                return code;
        }
        if (argis(param, "PDFSTOPONERROR")) {
            code = plist_value_get_bool(&pvalue, &ctx->args.pdfstoponerror);
            if (code < 0)
                return code;
        }
        if (argis(param, "PDFSTOPONWARNING")) {
            code = plist_value_get_bool(&pvalue, &ctx->args.pdfstoponwarning);
            if (code < 0)
                return code;
            if (ctx->args.pdfstoponwarning != 0)
                ctx->args.pdfstoponerror = 1;
        }
        if (argis(param, "NOTRANSPARENCY")) {
            code = plist_value_get_bool(&pvalue, &ctx->args.notransparency);
            if (code < 0)
                return code;
        }
        if (argis(param, "PDFNOCIDFALLBACK")) {
            code = plist_value_get_bool(&pvalue, &ctx->args.nocidfallback);
            if (code < 0)
                return code;
        }
        if (argis(param, "NO_PDFMARK_OUTLINES")) {
            code = plist_value_get_bool(&pvalue, &ctx->args.no_pdfmark_outlines);
            if (code < 0)
                return code;
        }
        if (argis(param, "NO_PDFMARK_DESTS")) {
            code = plist_value_get_bool(&pvalue, &ctx->args.no_pdfmark_dests);
            if (code < 0)
                return code;
        }
        if (argis(param, "PDFFitPage")) {
            code = plist_value_get_bool(&pvalue, &ctx->args.pdffitpage);
            if (code < 0)
                return code;
        }
        if (argis(param, "UseCropBox")) {
            code = plist_value_get_bool(&pvalue, &ctx->args.usecropbox);
            if (code < 0)
                return code;
        }
        if (argis(param, "UseArtBox")) {
            code = plist_value_get_bool(&pvalue, &ctx->args.useartbox);
            if (code < 0)
                return code;
        }
        if (argis(param, "UseBleedBox")) {
            code = plist_value_get_bool(&pvalue, &ctx->args.usebleedbox);
            if (code < 0)
                return code;
        }
        if (argis(param, "UseTrimBox")) {
            code = plist_value_get_bool(&pvalue, &ctx->args.usetrimbox);
            if (code < 0)
                return code;
        }
        if (argis(param, "Printed")) {
            code = plist_value_get_bool(&pvalue, &ctx->args.printed);
            if (code < 0)
                return code;
            Printed_set = true;
        }
        if (argis(param, "DITHERPPI")) {
            code = plist_value_get_bool(&pvalue, &ctx->args.ditherppi);
            if (code < 0)
                return code;
        }
        if (argis(param, "ShowAcroForm")) {
            code = plist_value_get_bool(&pvalue, &ctx->args.showacroform);
            if (code < 0)
                return code;
        }
        if (argis(param, "ShowAnnots")) {
            code = plist_value_get_bool(&pvalue, &ctx->args.showannots);
            if (code < 0)
                return code;
        }
        if (argis(param, "SHOWANNOTTYPES")) {
            code = plist_value_get_namelist(ctx, &pvalue, &ctx->args.showannottypes);
            if (code < 0)
                return code;
        }
        if (argis(param, "PreserveAnnots")) {
            code = plist_value_get_bool(&pvalue, &ctx->args.preserveannots);
            if (code < 0)
                return code;
        }
        if (argis(param, "PRESERVEANNOTTYPES")) {
            code = plist_value_get_namelist(ctx, &pvalue, &ctx->args.preserveannottypes);
            if (code < 0)
                return code;
        }
        if (argis(param, "PreserveMarkedContent")) {
            code = plist_value_get_bool(&pvalue, &ctx->args.preservemarkedcontent);
            if (code < 0)
                return code;
        }
        if (argis(param, "PreserveEmbeddedFiles")) {
            code = plist_value_get_bool(&pvalue, &ctx->args.preserveembeddedfiles);
            if (code < 0)
                return code;
        }
        if (argis(param, "PreserveDocView")) {
            code = plist_value_get_bool(&pvalue, &ctx->args.preservedocview);
            if (code < 0)
                return code;
        }
        if (argis(param, "NoUserUnit")) {
            code = plist_value_get_bool(&pvalue, &ctx->args.nouserunit);
            if (code < 0)
                return code;
        }
        if (argis(param, "RENDERTTNOTDEF")) {
            code = plist_value_get_bool(&pvalue, &ctx->args.renderttnotdef);
            if (code < 0)
                return code;
        }
        if (argis(param, "PDFINFO")) {
            code = plist_value_get_bool(&pvalue, &ctx->args.pdfinfo);
            if (code < 0)
                return code;
        }
        if (argis(param, "DOPDFMARKS")) {
            code = plist_value_get_bool(&pvalue, &ctx->args.dopdfmarks);
            if (code < 0)
                return code;
        }
        if (argis(param, "PDFPassword")) {
            code = plist_value_get_string_or_name(ctx, &pvalue, &ctx->encryption.Password , &ctx->encryption.PasswordLen, &discard_isname);
            if (code < 0)
                return code;
        }
        if (argis(param, "UsePDFX3Profile")) {
            /* This is a weird one because it can be either a bool or an int.
             * If it's a bool=true, then it defaults to PDFX3Profile_num = 0
             * If it's an int, then we set the flag to true and use the
             * specified number.
             * PS: Yuck!
             */
            code = plist_value_get_bool(&pvalue, &ctx->args.UsePDFX3Profile);
            if (code == gs_error_typecheck) {
                code = plist_value_get_int(&pvalue, &ctx->args.PDFX3Profile_num);
                if (code == 0)
                    ctx->args.UsePDFX3Profile = true;
            }
            if (code < 0)
                return code;
        }
        if (argis(param, "NOSUBSTDEVICECOLORS")) {
            code = plist_value_get_bool(&pvalue, &ctx->args.NOSUBSTDEVICECOLORS);
            if (code < 0)
                return code;
        }
        if (argis(param, "UseOutputIntent")) {
            code = plist_value_get_string_or_name(ctx, &pvalue, &ctx->args.UseOutputIntent, &len, &discard_isname);
            if (code < 0)
                return code;
        }
        if (argis(param, "FONTPATH")) {
            char *s = NULL;
            int slen;
            code = plist_value_get_string_or_name(ctx, &pvalue, &s , &slen, &discard_isname);
            if (code < 0)
                return code;
            code = pdfi_add_paths_to_search_paths(ctx, (const char *)s, slen, true);
            gs_free_object(ctx->memory, s, "FONTPATH param string");
        }
        if (argis(param, "FONTMAP")) {
            char *s = NULL;
            int slen;
            code = plist_value_get_string_or_name(ctx, &pvalue, &s, &slen, &discard_isname);
            if (code < 0)
                return code;
            code = pdfi_add_fontmapfiles(ctx, (const char *)s, slen);
            gs_free_object(ctx->memory, s, "FONTMAP param string");
        }
        if (argis(param, "CIDSubstPath")) {
            code = plist_value_get_string_or_name(ctx, &pvalue, (char **)&ctx->args.cidfsubstpath.data, (int *)&ctx->args.cidfsubstpath.size, &discard_isname);
            if (code < 0)
                return code;
        }
        if (argis(param, "CIDFSubstFont")) {
            code = plist_value_get_string_or_name(ctx, &pvalue, (char **)&ctx->args.cidfsubstfont.data, (int *)&ctx->args.cidfsubstfont.size, &discard_isname);
            if (code < 0)
                return code;
        }
        if (argis(param, "SUBSTFONT")) {
            code = plist_value_get_string_or_name(ctx, &pvalue, (char **)&ctx->args.defaultfont.data, (int *)&ctx->args.defaultfont.size, &ctx->args.defaultfont_is_name);
            if (code < 0)
                return code;
        }
        if (argis(param, "IgnoreToUnicode")) {
            code = plist_value_get_bool(&pvalue, &ctx->args.ignoretounicode);
            if (code < 0)
                return code;
        }
        if (argis(param, "OutputFile")) {
            if (!Printed_set)
                ctx->args.printed = true;
        }
    }

 exit:
    return code;
}

static int
pdf_impl_add_path(pl_interp_implementation_t *impl, const char *path)
{
    pdf_interp_instance_t *instance = impl->interp_client_data;
    pdf_context *ctx = instance->ctx;

    return pdfi_add_paths_to_search_paths(ctx, path, strlen(path), false);
}

static int
pdf_impl_post_args_init(pl_interp_implementation_t *impl)
{
    return 0;
}

/* Prepare interp instance for the next "job" */
static int
pdf_impl_init_job(pl_interp_implementation_t *impl,
                 gx_device                  *device)
{
    pdf_interp_instance_t *instance = impl->interp_client_data;
    pdf_context *ctx = instance->ctx;

    if (getenv("PDF_DISABLE_TRANSPARENCY"))
        ctx->args.notransparency = true;

    return pdf_impl_set_device(impl, device);
}

/* Wrap up interp instance after a "job" */
static int
pdf_impl_dnit_job(pl_interp_implementation_t *impl)
{
    pdf_interp_instance_t *instance = impl->interp_client_data;
    pdf_context *ctx = instance->ctx;

    return gs_grestoreall(ctx->pgs); /* pdf_impl_set_device does a gsave which we need to restore now.
                                      * Using grestoreall will restore any dangling gstates back to the last restore.
                                      * This should ensure we don't have dangling references to the device created by the
                                      * top level code.
                                      */
}

/* Deallocate a interpreter instance */
static int
pdf_impl_deallocate_interp_instance(pl_interp_implementation_t *impl)
{
    pdf_interp_instance_t *instance = impl->interp_client_data;
    pdf_context *ctx = instance->ctx;
    gs_memory_t *mem = instance->memory;
    int code = 0;

    code = pdfi_free_context(ctx);

    gs_free_object(mem, instance, "pdf_impl_deallocate_interp_instance");

    return code;
}

/* Parser implementation descriptor */
pl_interp_implementation_t pdf_implementation =
{
    pdf_impl_characteristics,
    pdf_impl_allocate_interp_instance,
    NULL,  /* pdf_impl_get_device_memory, */
    pdf_impl_set_param,
    pdf_impl_add_path,
    pdf_impl_post_args_init,
    pdf_impl_init_job,
    NULL,                               /* run_prefix_commands */
    pdf_impl_process_file,
    pdf_impl_process_begin,
    pdf_impl_process,
    pdf_impl_process_end,
    pdf_impl_flush_to_eoj,
    pdf_impl_process_eof,
    pdf_impl_report_errors,
    pdf_impl_dnit_job,
    pdf_impl_deallocate_interp_instance,
    NULL,                               /* interp_client_data */
};

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
pdfi_install_halftone(pdf_context *ctx, gx_device *pdevice)
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
        memset(&ht.rc, 0x00, sizeof(ht.rc));
        ht.type = ht_type_threshold;
        ht.objtype = HT_OBJTYPE_DEFAULT;
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
