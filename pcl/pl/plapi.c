/* Copyright (C) 2001-2020 Artifex Software, Inc.
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

#include "plapi.h"
#include "plmain.h"
#include "gsmchunk.h"
#include "gsmalloc.h"
#include "gserrors.h"
#include "gsexit.h"

#include "gp.h"
#include "gscdefs.h"
#include "gsmemory.h"

/* Return revision numbers and strings of Ghostscript. */
/* Used for determining if wrong GSDLL loaded. */
/* This may be called before any other function. */
GSDLLEXPORT int GSDLLAPI
gsapi_revision(gsapi_revision_t *pr, int rvsize)
{
    if (rvsize < sizeof(gsapi_revision_t))
        return sizeof(gsapi_revision_t);
    pr->product = gs_product;
    pr->copyright = gs_copyright;
    pr->revision = gs_revision;
    pr->revisiondate = gs_revisiondate;
    return 0;
}

GSDLLEXPORT int GSDLLAPI
gsapi_new_instance(void **lib, void *caller_handle)
{
    gs_memory_t *heap_mem = gs_malloc_init();
    gs_memory_t *chunk_mem;
    pl_main_instance_t *minst;
    int code;

    if (heap_mem == NULL)
        return gs_error_Fatal;

    code = gs_memory_chunk_wrap(&chunk_mem, heap_mem);
    if (code < 0) {
        gs_malloc_release(heap_mem);
        return gs_error_Fatal;
    }

    minst = pl_main_alloc_instance(chunk_mem);
    if (minst == NULL) {
        gs_malloc_release(gs_memory_chunk_unwrap(chunk_mem));
        return gs_error_Fatal;
    }

    *lib = (void *)(chunk_mem->gs_lib_ctx);
    chunk_mem->gs_lib_ctx->core->default_caller_handle = caller_handle;

    return 0;
}

GSDLLEXPORT int GSDLLAPI
gsapi_set_stdio(void *instance,
    int (GSDLLCALLPTR stdin_fn)(void *caller_handle, char *buf, int len),
    int (GSDLLCALLPTR stdout_fn)(void *caller_handle, const char *str, int len),
    int (GSDLLCALLPTR stderr_fn)(void *caller_handle, const char *str, int len))
{
    gs_lib_ctx_t *ctx = (gs_lib_ctx_t *)instance;

    if (ctx == NULL)
        return gs_error_Fatal;
    return gsapi_set_stdio_with_handle(instance,
                                       stdin_fn, stdout_fn, stderr_fn,
                                       ctx->core->default_caller_handle);
}

GSDLLEXPORT int GSDLLAPI
gsapi_set_stdio_with_handle(void *instance,
    int (GSDLLCALLPTR stdin_fn)(void *caller_handle, char *buf, int len),
    int (GSDLLCALLPTR stdout_fn)(void *caller_handle, const char *str, int len),
    int (GSDLLCALLPTR stderr_fn)(void *caller_handle, const char *str, int len),
    void *caller_handle)
{
    gs_lib_ctx_t *ctx = (gs_lib_ctx_t *)instance;

    if (ctx == NULL)
        return gs_error_Fatal;
    ctx->core->stdin_fn  = stdin_fn;
    ctx->core->stdout_fn = stdout_fn;
    ctx->core->stderr_fn = stderr_fn;
    ctx->core->std_caller_handle = caller_handle;

    return 0;
}

GSDLLEXPORT int GSDLLAPI
gsapi_init_with_args(void *lib, int argc, char **argv)
{
    gs_lib_ctx_t *ctx = (gs_lib_ctx_t *)lib;
    if (lib == NULL)
        return gs_error_Fatal;

    return pl_main_init_with_args(pl_main_get_instance(ctx->memory), argc, argv);
}

GSDLLEXPORT int GSDLLAPI
gsapi_run_file(void *lib, const char *file_name, int user_errors, int *pexit_code)
{
    gs_lib_ctx_t *ctx = (gs_lib_ctx_t *)lib;
    int code, code1;

    if (pexit_code != NULL)
        *pexit_code = 0;

    if (lib == NULL)
        return gs_error_Fatal;

    code = gs_add_control_path(ctx->memory, gs_permit_file_reading, file_name);
    if (code < 0) return code;

    code = pl_main_run_file(pl_main_get_instance(ctx->memory), file_name);

    code1 = gs_remove_control_path(ctx->memory, gs_permit_file_reading, file_name);
    if (code >= 0 && code1 < 0) code = code1;

    return code;
}

GSDLLEXPORT int GSDLLAPI
gsapi_exit(void *lib)
{
    gs_lib_ctx_t *ctx = (gs_lib_ctx_t *)lib;
    if (lib == NULL)
        return gs_error_Fatal;

    return pl_to_exit(ctx->memory);
}

GSDLLEXPORT int GSDLLAPI
gsapi_delete_instance(void *lib)
{
    gs_lib_ctx_t *ctx = (gs_lib_ctx_t *)lib;
    if (lib == NULL)
        return gs_error_Fatal;

    return pl_main_delete_instance(pl_main_get_instance(ctx->memory));
}

GSDLLEXPORT int GSDLLAPI gsapi_set_poll(void *instance,
    int (GSDLLCALLPTR poll_fn)(void *caller_handle))
{
    gs_lib_ctx_t *ctx = (gs_lib_ctx_t *)instance;
    if (instance == NULL)
        return gs_error_Fatal;
    return gsapi_set_poll_with_handle(instance, poll_fn,
                                      ctx->core->default_caller_handle);
}

GSDLLEXPORT int GSDLLAPI gsapi_set_poll_with_handle(void *instance,
    int (GSDLLCALLPTR poll_fn)(void *caller_handle),
    void *caller_handle)
{
    gs_lib_ctx_t *ctx = (gs_lib_ctx_t *)instance;
    if (instance == NULL)
        return gs_error_Fatal;
    ctx->core->poll_fn = poll_fn;
    ctx->core->poll_caller_handle = caller_handle;
    return 0;
}

GSDLLEXPORT int GSDLLAPI
gsapi_set_display_callback(void *lib, display_callback *callback)
{
    gs_lib_ctx_t *ctx = (gs_lib_ctx_t *)lib;
    if (lib == NULL)
        return gs_error_Fatal;
    pl_main_set_display_callback(pl_main_get_instance(ctx->memory), callback);
    return 0;
}

GSDLLEXPORT int GSDLLAPI
gsapi_set_default_device_list(void *instance, const char *list, int listlen)
{
    gs_lib_ctx_t *ctx = (gs_lib_ctx_t *)instance;
    if (instance == NULL)
        return gs_error_Fatal;
    return gs_lib_ctx_set_default_device_list(ctx->memory, list, listlen);
}

GSDLLEXPORT int GSDLLAPI
gsapi_get_default_device_list(void *instance, char **list, int *listlen)
{
    gs_lib_ctx_t *ctx = (gs_lib_ctx_t *)instance;
    if (instance == NULL)
        return gs_error_Fatal;
    return gs_lib_ctx_get_default_device_list(ctx->memory, list, listlen);
}

static int utf16le_get_codepoint(gp_file *file, const char **astr)
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
        if (file) {
            rune = gp_fgetc(file);
            if (rune == EOF)
                return EOF;
            c = gp_fgetc(file);
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
        if (file) {
            trail = gp_fgetc(file);
            if (trail == EOF)
                return EOF;
            c = gp_fgetc(file);
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
GSDLLEXPORT int GSDLLAPI
gsapi_set_arg_encoding(void *instance, int encoding)
{
    gs_lib_ctx_t *ctx = (gs_lib_ctx_t *)instance;
    if (instance == NULL)
        return gs_error_Fatal;

    if (encoding == PL_ARG_ENCODING_LOCAL) {
#if defined(__WIN32__) && !defined(METRO)
        /* For windows, we need to set it up so that we convert from 'local'
         * format (in this case whatever codepage is set) to utf8 format. At
         * the moment, all the other OS we care about provide utf8 anyway.
         */
        pl_main_set_arg_decode(pl_main_get_instance(ctx->memory), gp_local_arg_encoding_get_codepoint);
#else
        pl_main_set_arg_decode(pl_main_get_instance(ctx->memory), NULL);
#endif /* WIN32 */
        return 0;
    }
    if (encoding == PL_ARG_ENCODING_UTF8) {
        pl_main_set_arg_decode(pl_main_get_instance(ctx->memory), NULL);
        return 0;
    }
    if (encoding == PL_ARG_ENCODING_UTF16LE) {
        pl_main_set_arg_decode(pl_main_get_instance(ctx->memory), utf16le_get_codepoint);
        return 0;
    }
    return gs_error_Fatal;
}

GSDLLEXPORT int GSDLLAPI
gsapi_run_string_begin(void *lib, int user_errors, int *pexit_code)
{
    gs_lib_ctx_t *ctx = (gs_lib_ctx_t *)lib;

    if (pexit_code != NULL)
        *pexit_code = 0;

    if (lib == NULL)
        return gs_error_Fatal;
    return pl_main_run_string_begin(pl_main_get_instance(ctx->memory));
}

GSDLLEXPORT int GSDLLAPI
gsapi_run_string_continue(void *lib, const char *str, unsigned int length,
                          int user_errors, int *pexit_code)
{
    gs_lib_ctx_t *ctx = (gs_lib_ctx_t *)lib;

    if (pexit_code != NULL)
        *pexit_code = 0;

    if (lib == NULL)
        return gs_error_Fatal;
    return pl_main_run_string_continue(pl_main_get_instance(ctx->memory), str, length);
}

GSDLLEXPORT int GSDLLAPI
gsapi_run_string_end(void *lib, int user_errors, int *pexit_code)
{
    gs_lib_ctx_t *ctx = (gs_lib_ctx_t *)lib;

    if (pexit_code != NULL)
        *pexit_code = 0;

    if (lib == NULL)
        return gs_error_Fatal;
    return pl_main_run_string_end(pl_main_get_instance(ctx->memory));
}

GSDLLEXPORT int GSDLLAPI
gsapi_run_string_with_length(void *lib, const char *str, unsigned int length,
                             int user_errors, int *pexit_code)
{
    int code = gsapi_run_string_begin(lib, user_errors, pexit_code);
    if (code < 0)
        return code;
    code = gsapi_run_string_continue(lib, str, length, user_errors, pexit_code);
    if (code < 0 && code != gs_error_NeedInput)
        return code;
    code = gsapi_run_string_end(lib, user_errors, pexit_code);
    if (code == gs_error_NeedInput)
        return_error(gs_error_Fatal);
    return code;
}

GSDLLEXPORT int GSDLLAPI
gsapi_run_string(void *lib, const char *str,
                 int user_errors, int *pexit_code)
{
    return gsapi_run_string_with_length(lib, str, (unsigned int)strlen(str),
                                        user_errors, pexit_code);
}

GSDLLEXPORT int GSDLLAPI
gsapi_set_param(void *lib, const char *param, const void *value, gs_set_param_type type)
{
    gs_lib_ctx_t *ctx = (gs_lib_ctx_t *)lib;
    if (lib == NULL)
        return gs_error_Fatal;
    return pl_main_set_typed_param(pl_main_get_instance(ctx->memory), (pl_set_param_type)type, param, value);
}

GSDLLEXPORT int GSDLLAPI
gsapi_get_param(void *lib, const char *param, void *value, gs_set_param_type type)
{
    gs_lib_ctx_t *ctx = (gs_lib_ctx_t *)lib;
    if (lib == NULL)
        return gs_error_Fatal;
    return pl_main_get_typed_param(pl_main_get_instance(ctx->memory), (pl_set_param_type)type, param, value);
}

GSDLLEXPORT int GSDLLAPI
gsapi_enumerate_params(void *lib, void **iterator, const char **key, gs_set_param_type *type)
{
    gs_lib_ctx_t *ctx = (gs_lib_ctx_t *)lib;
    if (lib == NULL)
        return gs_error_Fatal;
    return pl_main_enumerate_params(pl_main_get_instance(ctx->memory),
                                    iterator, key, (pl_set_param_type*)type);
}

GSDLLEXPORT int GSDLLAPI
gsapi_add_control_path(void *instance, int type, const char *path)
{
    gs_lib_ctx_t *ctx = (gs_lib_ctx_t *)instance;
    if (ctx == NULL)
        return gs_error_Fatal;
    return gs_add_control_path(ctx->memory, type, path);
}

GSDLLEXPORT int GSDLLAPI
gsapi_remove_control_path(void *instance, int type, const char *path)
{
    gs_lib_ctx_t *ctx = (gs_lib_ctx_t *)instance;
    if (ctx == NULL)
        return gs_error_Fatal;
    return gs_remove_control_path(ctx->memory, type, path);
}

GSDLLEXPORT void GSDLLAPI
gsapi_purge_control_paths(void *instance, int type)
{
    gs_lib_ctx_t *ctx = (gs_lib_ctx_t *)instance;
    if (ctx == NULL)
        return;
    gs_purge_control_paths(ctx->memory, type);
}

GSDLLEXPORT void GSDLLAPI
gsapi_activate_path_control(void *instance, int enable)
{
    gs_lib_ctx_t *ctx = (gs_lib_ctx_t *)instance;
    if (ctx == NULL)
        return;
    gs_activate_path_control(ctx->memory, enable);
}

GSDLLEXPORT int GSDLLAPI
gsapi_is_path_control_active(void *instance)
{
    gs_lib_ctx_t *ctx = (gs_lib_ctx_t *)instance;
    if (ctx == NULL)
        return 0;
    return gs_is_path_control_active(ctx->memory);
}

GSDLLEXPORT int GSDLLAPI
gsapi_add_fs(void *instance, gsapi_fs_t *fs, void *secret)
{
    gs_lib_ctx_t *ctx = (gs_lib_ctx_t *)instance;
    if (ctx == NULL)
        return 0;
    return gs_add_fs(ctx->memory, (gs_fs_t *)fs, secret);
}

GSDLLEXPORT void GSDLLAPI
gsapi_remove_fs(void *instance, gsapi_fs_t *fs, void *secret)
{
    gs_lib_ctx_t *ctx = (gs_lib_ctx_t *)instance;
    if (ctx == NULL)
        return;
    gs_remove_fs(ctx->memory, (gs_fs_t *)fs, secret);
}

GSDLLEXPORT int GSDLLAPI gsapi_register_callout(
   void *instance, gs_callout fn, void *handle)
{
    gs_lib_ctx_t *ctx = (gs_lib_ctx_t *)instance;
    if (instance == NULL)
        return gs_error_Fatal;
    return gs_lib_ctx_register_callout(ctx->memory, fn, handle);
}

/* Deregister a handler for gs callouts. */
GSDLLEXPORT void GSDLLAPI gsapi_deregister_callout(
   void *instance, gs_callout fn, void *handle)
{
    gs_lib_ctx_t *ctx = (gs_lib_ctx_t *)instance;
    if (instance == NULL)
        return;
    gs_lib_ctx_deregister_callout(ctx->memory, fn, handle);
}
