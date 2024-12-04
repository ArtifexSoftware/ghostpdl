/* Copyright (C) 2001-2025 Artifex Software, Inc.
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



/* Private internal API to Ghostscript interpreter */

#include "string_.h"
#include "ierrors.h"
#include "gscdefs.h"
#include "gstypes.h"
#include "gdebug.h"
#include "psapi.h"	/* Public API */
#include "iref.h"
#include "iminst.h"
#include "imain.h"
#include "imainarg.h"
#include "gsmemory.h"
#include "gsmalloc.h"
#include "gslibctx.h"
#include "gp.h"
#include "gsargs.h"
#include "ialloc.h"
#include "icstate.h"
#include "store.h"
#include "iname.h"
#include "interp.h"
#include "gxgstate.h"

/* This is the fallback for the number of threads to allow per process; i.e. just one.
 * This is only ever used if the gp_get_globals function returns 0 (i.e. only for
 * platforms that don't support threading).
 */
static int gsapi_instance_counter = 0;
static const int gsapi_instance_max = 1;


#ifdef METRO
static int GSDLLCALL metro_stdin(void *v, char *buf, int len)
{
        return 0;
}

static int GSDLLCALL metro_stdout(void *v, const char *str, int len)
{
#ifdef DEBUG
        OutputDebugStringWRT(str, len);
#endif
        return len;
}

static int GSDLLCALL metro_stderr(void *v, const char *str, int len)
{
        return metro_stdout(v, str, len);
}
#endif

/* Create a new instance of Ghostscript.
 * First instance per process call with *pinstance == NULL
 * next instance in a proces call with *pinstance == copy of valid_instance pointer
 * *pinstance is set to a new instance pointer.
 */
int
psapi_new_instance(gs_lib_ctx_t **pinstance,
                   void          *caller_handle)
{
    gs_memory_t *mem = NULL;
    gs_main_instance *minst = NULL;

    if (pinstance == NULL)
        return gs_error_Fatal;

    if (gp_get_globals() == NULL) {
        /* This platform does not support the thread safe instance
         * handling. We'll drop back to the old mechanism we've used
         * to handle limiting ourselves to 1 instance in the past,
         * despite this being thread-unsafe itself. */
        if ( gsapi_instance_counter >= gsapi_instance_max )
            return gs_error_Fatal;
        ++gsapi_instance_counter;
    }

    mem = gs_malloc_init_with_context(*pinstance);
    if (mem == NULL)
        return gs_error_Fatal;
    minst = gs_main_alloc_instance(mem);
    if (minst == NULL) {
        gs_malloc_release(mem);
        return gs_error_Fatal;
    }
    mem->gs_lib_ctx->top_of_system = (void*) minst;
    mem->gs_lib_ctx->core->default_caller_handle = caller_handle;
    mem->gs_lib_ctx->core->custom_color_callback = NULL;
#ifdef METRO
    if (mem->gs_lib_ctx->core->stdin_fn == NULL)
        mem->gs_lib_ctx->core->stdin_fn = metro_stdin;
    if (mem->gs_lib_ctx->core->stdout_fn == NULL)
        mem->gs_lib_ctx->core->stdout_fn = metro_stdout;
    if (mem->gs_lib_ctx->core->stderr_fn == NULL)
        mem->gs_lib_ctx->core->stderr_fn = metro_stderr;
#endif
    mem->gs_lib_ctx->core->poll_fn = NULL;

    *pinstance = mem->gs_lib_ctx;
    return psapi_set_arg_encoding(*pinstance, PS_ARG_ENCODING_LOCAL);
}

/* Set an instance of Ghostscript to respond to UEL (universal
 * exit language) strings in the input. */
void
psapi_act_on_uel(gs_lib_ctx_t *ctx)
{
    ctx->core->act_on_uel = 1;
}

/* Destroy an instance of Ghostscript */
/* We do not support multiple instances, so make sure
 * we use the default instance only once.
 */
void
psapi_delete_instance(gs_lib_ctx_t *ctx)
{
    gs_memory_t *mem;
    gs_main_instance *minst;

    if (ctx == NULL)
        return;

    mem = (gs_memory_t *)(ctx->memory);
    minst = get_minst_from_memory(ctx->memory);

    ctx->core->default_caller_handle = NULL;
    ctx->core->stdin_fn = NULL;
    ctx->core->stdout_fn = NULL;
    ctx->core->stderr_fn = NULL;
    ctx->core->poll_fn = NULL;
    minst->display = NULL;

    if (minst->param_list) {
        gs_c_param_list_release(minst->param_list);
        gs_free_object(minst->heap, minst->param_list, "psapi_delete_instance");
    }

    gs_c_param_list_release(&minst->enum_params);
    gs_free_object(minst->heap, minst->enum_keybuf, "psapi_delete_instance");

    gs_free_object(mem, minst, "init_main_instance");

    /* Release the memory (frees up everything) */
    gs_malloc_release(mem);

    if (gp_get_globals() == NULL)
        --gsapi_instance_counter;
}

static int ascii_get_codepoint(stream *s, const char **astr)
{
    if (s)
        return spgetc(s);
    else {
        int rune = **astr;
        (*astr)++;
        if (rune != 0)
            return rune;
    }
    return EOF;
}

static int utf16le_get_codepoint(stream *s, const char **astr)
{
    int c;
    int rune;
    int trail;

    /* This code spots the BOM for 16bit LE and ignores it. Strictly speaking
     * this may be wrong, as we are only supposed to ignore it at the beginning
     * of the string, but if anyone is stupid enough to use ZWNBSP (zero width
     * non breaking space) in the middle of their strings, then they deserve
     * what they get. */
    /* We spot the BOM for 16bit BE and treat that as EOF. We'd rather give
     * up on dealing with a broken file than try to run something we know to
     * be wrong. */

    do {
        if (s) {
            rune = spgetc(s);
            if (rune == EOF)
                return EOF;
            c = spgetc(s);
            if (c == EOF)
                return EOF;
            rune += c<<8;
        } else {
            rune = (*astr)[0] | ((*astr)[1]<<8);
            if (rune != 0)
                (*astr) += 2;
            else
                return EOF;
        }
        if (rune == 0xFEFF) /* BOM - ignore it */
            continue;
        if (rune == 0xFFFE) /* BOM for BE - hopelessly broken */
            return EOF;
        if (rune < 0xD800 || rune >= 0xE000)
            return rune;
        if (rune >= 0xDC00) /* Found a trailing surrogate pair. Skip it */
            continue;
lead: /* We've just read a leading surrogate */
        rune -= 0xD800;
        rune <<= 10;
        if (s) {
            trail = spgetc(s);
            if (trail == EOF)
                return EOF;
            c = spgetc(s);
            if (c == EOF)
                return EOF;
            trail += c<<8;
        } else {
            trail = (*astr)[0] | ((*astr)[1]<<8);
            if (trail != 0)
                (*astr) += 2;
            else
                return EOF;
        }
        if (trail < 0xd800 || trail >= 0xE000) {
            if (rune == 0xFEFF) /* BOM - ignore it. */
                continue;
            if (rune == 0xFFFE) /* BOM for BE - hopelessly broken. */
                return EOF;
            /* No trail surrogate was found, so skip the lead surrogate and
             * return the rune we landed on. */
            return trail;
        }
        if (trail < 0xdc00) {
            /* We found another leading surrogate. */
            rune = trail;
            goto lead;
        }
        break;
    } while (1);

    return rune + (trail-0xDC00) + 0x10000;
}

/* Initialise the interpreter */
int
psapi_set_arg_encoding(gs_lib_ctx_t *ctx, int encoding)
{
    if (ctx == NULL)
        return gs_error_Fatal;

    if (encoding == PS_ARG_ENCODING_LOCAL) {
#if defined(__WIN32__) && !defined(METRO)
        /* For windows, we need to set it up so that we convert from 'local'
         * format (in this case whatever codepage is set) to utf8 format. At
         * the moment, all the other OS we care about provide utf8 anyway.
         */
        gs_main_inst_arg_decode(get_minst_from_memory(ctx->memory), gp_local_arg_encoding_get_codepoint);
#else
        gs_main_inst_arg_decode(get_minst_from_memory(ctx->memory), ascii_get_codepoint);
#endif /* WIN32 */
        return 0;
    }
    if (encoding == PS_ARG_ENCODING_UTF8) {
        gs_main_inst_arg_decode(get_minst_from_memory(ctx->memory), NULL);
        return 0;
    }
    if (encoding == PS_ARG_ENCODING_UTF16LE) {
        gs_main_inst_arg_decode(get_minst_from_memory(ctx->memory), utf16le_get_codepoint);
        return 0;
    }
    return gs_error_Fatal;
}

int
psapi_init_with_args(gs_lib_ctx_t *ctx, int argc, char **argv)
{
    if (ctx == NULL)
        return gs_error_Fatal;

    return gs_main_init_with_args(get_minst_from_memory(ctx->memory), argc, argv);
}

int
psapi_init_with_args01(gs_lib_ctx_t *ctx, int argc, char **argv)
{
    if (ctx == NULL)
        return gs_error_Fatal;

    return gs_main_init_with_args01(get_minst_from_memory(ctx->memory), argc, argv);
}

int
psapi_init_with_args2(gs_lib_ctx_t *ctx)
{
    if (ctx == NULL)
        return gs_error_Fatal;

    return gs_main_init_with_args2(get_minst_from_memory(ctx->memory));
}

int
psapi_set_device_param(gs_lib_ctx_t *ctx,
                       gs_param_list *plist)
{
    gs_main_instance *minst = get_minst_from_memory(ctx->memory);

    return gs_putdeviceparams(minst->i_ctx_p->pgs->device, plist);
}

int
psapi_get_device_params(gs_lib_ctx_t *ctx,
                        gs_param_list *plist)
{
    gs_main_instance *minst = get_minst_from_memory(ctx->memory);

    if (minst->i_ctx_p->pgs->device == NULL)
        return 0;
    return gs_getdeviceparams(minst->i_ctx_p->pgs->device, plist);
}

int
psapi_set_param(gs_lib_ctx_t *ctx,
                gs_param_list *plist)
{
    gs_main_instance *minst = get_minst_from_memory(ctx->memory);

    return gs_main_set_language_param(minst, plist);
}

int
psapi_add_path(gs_lib_ctx_t *ctx,
               const char   *path)
{
    gs_main_instance *minst = get_minst_from_memory(ctx->memory);

    return gs_main_add_lib_path(minst, path);
}

/* The gsapi_run_* functions are like gs_main_run_* except
 * that the error_object is omitted.
 * An error object in minst is used instead.
 */

/* Setup up a suspendable run_string */
int
psapi_run_string_begin(gs_lib_ctx_t *ctx,
                       int           user_errors,
                       int          *pexit_code)
{
    gs_main_instance *minst;
    int code;

    if (ctx == NULL)
        return gs_error_Fatal;
    minst = get_minst_from_memory(ctx->memory);

    if (minst->mid_run_string == 1)
        return -1;
    minst->mid_run_string = 1;

    code = gs_main_run_string_begin(minst, user_errors, pexit_code,
                                    &(minst->error_object));
    if (code < 0)
        minst->mid_run_string = 0;

    return code;
}

int
psapi_run_string_continue(gs_lib_ctx_t *ctx,
                          const char   *str,
                          unsigned int  length,
                          int         user_errors,
                          int        *pexit_code)
{
    gs_main_instance *minst;
    int code;

    if (ctx == NULL)
        return gs_error_Fatal;
    minst = get_minst_from_memory(ctx->memory);

    code = gs_main_run_string_continue(minst, str, length, user_errors,
                                       pexit_code,
                                       &(minst->error_object));
    if (code < 0)
        minst->mid_run_string = 0;

    return code;
}

uint
psapi_get_uel_offset(gs_lib_ctx_t *ctx)
{
    if (ctx == NULL)
        return 0;

    return gs_main_get_uel_offset(get_minst_from_memory(ctx->memory));
}

int
psapi_run_string_end(gs_lib_ctx_t *ctx,
                     int           user_errors,
                     int          *pexit_code)
{
    int code;
    gs_main_instance *minst;

    if (ctx == NULL)
        return gs_error_Fatal;
    minst = get_minst_from_memory(ctx->memory);

    code = gs_main_run_string_end(minst, user_errors, pexit_code,
                                  &(minst->error_object));

    minst->mid_run_string = 0;
    return code;
}

int
psapi_run_string_with_length(gs_lib_ctx_t *ctx,
                             const char   *str,
                             unsigned int  length,
                             int           user_errors,
                             int          *pexit_code)
{
    if (ctx == NULL)
        return gs_error_Fatal;

    return gs_main_run_string_with_length(get_minst_from_memory(ctx->memory),
                                          str, length, user_errors, pexit_code,
                                          &(get_minst_from_memory(ctx->memory)->error_object));
}

int
psapi_run_string(gs_lib_ctx_t *ctx,
                 const char   *str,
                 int           user_errors,
                 int          *pexit_code)
{
    if (ctx == NULL)
        return gs_error_Fatal;

    return gs_main_run_string(get_minst_from_memory(ctx->memory),
                              str, user_errors, pexit_code,
                              &(get_minst_from_memory(ctx->memory)->error_object));
}

int
psapi_run_file(gs_lib_ctx_t *ctx,
               const char   *file_name,
               int           user_errors,
               int          *pexit_code)
{
    char *d, *temp;
    const char *c = file_name;
    char dummy[6];
    int rune, code, len;
    gs_main_instance *minst;
    if (ctx == NULL)
        return gs_error_Fatal;
    minst = get_minst_from_memory(ctx->memory);

    if (minst->mid_run_string == 1)
        return -1;

    /* Convert the file_name to utf8 */
    if (minst->get_codepoint) {
        len = 1;
        while ((rune = minst->get_codepoint(NULL, &c)) >= 0)
            len += codepoint_to_utf8(dummy, rune);
        temp = (char *)gs_alloc_bytes_immovable(ctx->memory, len, "gsapi_run_file");
        if (temp == NULL)
            return 0;
        c = file_name;
        d = temp;
        while ((rune = minst->get_codepoint(NULL, &c)) >= 0)
           d += codepoint_to_utf8(d, rune);
        *d = 0;
    }
    else {
      temp = (char *)file_name;
    }
    code = gs_main_run_file2(minst, temp, user_errors, pexit_code,
                             &(minst->error_object));
    if (temp != file_name)
        gs_free_object(ctx->memory, temp, "gsapi_run_file");
    return code;
}

/* Retrieve the memory allocator for the interpreter instance */
gs_memory_t *
psapi_get_device_memory(gs_lib_ctx_t *ctx)
{
    if (ctx == NULL)
        return NULL;
    return gs_main_get_device_memory(get_minst_from_memory(ctx->memory));
}

/* Retrieve the memory allocator for the interpreter instance */
int
psapi_set_device(gs_lib_ctx_t *ctx, gx_device *pdev)
{
    if (ctx == NULL)
        return gs_error_Fatal;
    return gs_main_set_device(get_minst_from_memory(ctx->memory), pdev);
}

/* Exit the interpreter */
int
psapi_exit(gs_lib_ctx_t *ctx)
{
    if (ctx == NULL)
        return gs_error_Fatal;

    return gs_to_exit(ctx->memory, 0);
}

int
psapi_force_geometry(gs_lib_ctx_t *ctx, const float *resolutions, const long *dimensions)
{
    int code;

    if (ctx == NULL)
        return gs_error_Fatal;
    code = gs_main_force_resolutions(get_minst_from_memory(ctx->memory), resolutions);
    if (code < 0)
        return code;
    return gs_main_force_dimensions(get_minst_from_memory(ctx->memory), dimensions);
}
