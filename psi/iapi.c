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



/* Public Application Programming Interface to Ghostscript interpreter */

#include "string_.h"
#include "ierrors.h"
#include "gscdefs.h"
#include "gstypes.h"
#include "gdebug.h"
#include "iapi.h"	/* Public API */
#include "psapi.h"
#include "iref.h"
#include "iminst.h"
#include "imain.h"
#include "imainarg.h"
#include "gsmemory.h"
#include "gsmalloc.h"
#include "gslibctx.h"
#include "gp.h"
#include "gsargs.h"
#include "gdevdsp.h"

typedef struct { int a[(int)GS_ARG_ENCODING_LOCAL   == (int)PS_ARG_ENCODING_LOCAL   ? 1 : -1]; } compile_time_assert_0;
typedef struct { int a[(int)GS_ARG_ENCODING_UTF8    == (int)PS_ARG_ENCODING_UTF8    ? 1 : -1]; } compile_time_assert_1;
typedef struct { int a[(int)GS_ARG_ENCODING_UTF16LE == (int)PS_ARG_ENCODING_UTF16LE ? 1 : -1]; } compile_time_assert_2;

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

/* Create a new instance of Ghostscript.
 * First instance per process call with *pinstance == NULL
 * next instance in a proces call with *pinstance == copy of valid_instance pointer
 * *pinstance is set to a new instance pointer.
 */
GSDLLEXPORT int GSDLLAPI
gsapi_new_instance(void **pinstance, void *caller_handle)
{
    return psapi_new_instance((gs_lib_ctx_t **)pinstance, caller_handle);
}

/* Destroy an instance of Ghostscript */
/* We do not support multiple instances, so make sure
 * we use the default instance only once.
 */
GSDLLEXPORT void GSDLLAPI
gsapi_delete_instance(void *instance)
{
    psapi_delete_instance(instance);
}

/* Set the callback functions for stdio */
GSDLLEXPORT int GSDLLAPI
gsapi_set_stdio(void *instance,
    int(GSDLLCALL *stdin_fn)(void *caller_handle, char *buf, int len),
    int(GSDLLCALL *stdout_fn)(void *caller_handle, const char *str, int len),
    int(GSDLLCALL *stderr_fn)(void *caller_handle, const char *str, int len))
{
    gs_lib_ctx_t *ctx = (gs_lib_ctx_t *)instance;
    if (instance == NULL)
        return gs_error_Fatal;
    return gsapi_set_stdio_with_handle(instance,
                                       stdin_fn, stdout_fn, stderr_fn,
                                       ctx->core->default_caller_handle);
}

GSDLLEXPORT int GSDLLAPI
gsapi_set_stdio_with_handle(void *instance,
    int(GSDLLCALL *stdin_fn)(void *caller_handle, char *buf, int len),
    int(GSDLLCALL *stdout_fn)(void *caller_handle, const char *str, int len),
    int(GSDLLCALL *stderr_fn)(void *caller_handle, const char *str, int len),
    void *caller_handle)
{
    gs_lib_ctx_t *ctx = (gs_lib_ctx_t *)instance;
    if (instance == NULL)
        return gs_error_Fatal;
    ctx->core->stdin_fn = stdin_fn;
    ctx->core->stdout_fn = stdout_fn;
    ctx->core->stderr_fn = stderr_fn;
    ctx->core->std_caller_handle = caller_handle;
    return 0;
}

/* Set the callback function for polling */
GSDLLEXPORT int GSDLLAPI
gsapi_set_poll(void *instance,
    int(GSDLLCALL *poll_fn)(void *caller_handle))
{
    gs_lib_ctx_t *ctx = (gs_lib_ctx_t *)instance;
    if (instance == NULL)
        return gs_error_Fatal;
    return gsapi_set_poll_with_handle(instance, poll_fn,
                                      ctx->core->default_caller_handle);
}

GSDLLEXPORT int GSDLLAPI
gsapi_set_poll_with_handle(void *instance,
    int(GSDLLCALL *poll_fn)(void *caller_handle),
    void *caller_handle)
{
    gs_lib_ctx_t *ctx = (gs_lib_ctx_t *)instance;
    if (instance == NULL)
        return gs_error_Fatal;
    ctx->core->poll_fn = poll_fn;
    ctx->core->poll_caller_handle = caller_handle;
    return 0;
}

static int
legacy_display_callout(void *instance,
                       void *handle,
                       const char *dev_name,
                       int id,
                       int size,
                       void *data)
{
    gs_main_instance *inst = (gs_main_instance *)instance;

    if (dev_name == NULL)
        return -1;
    if (strcmp(dev_name, "display") != 0)
        return -1;

    if (id == DISPLAY_CALLOUT_GET_CALLBACK_LEGACY) {
        /* get display callbacks */
        gs_display_get_callback_t *cb = (gs_display_get_callback_t *)data;
        cb->callback = inst->display;
        return 0;
    }
    return -1;
}

/* Set the display callback structure */
GSDLLEXPORT int GSDLLAPI
gsapi_set_display_callback(void *instance, display_callback *callback)
{
    gs_lib_ctx_t *ctx = (gs_lib_ctx_t *)instance;
    gs_main_instance *minst;
    int code;
    if (instance == NULL)
        return gs_error_Fatal;
    minst = get_minst_from_memory(ctx->memory);
    if (minst->display == NULL && callback != NULL) {
        /* First registration. */
        code = gs_lib_ctx_register_callout(minst->heap,
                                           legacy_display_callout,
                                           minst);
        if (code < 0)
            return code;
    }
    if (minst->display != NULL && callback == NULL) {
        /* Deregistered. */
        gs_lib_ctx_deregister_callout(minst->heap,
                                      legacy_display_callout,
                                      minst);
    }
    minst->display = callback;
    /* not in a language switched build */
    return 0;
}

GSDLLEXPORT int GSDLLAPI
gsapi_register_callout(void *instance, gs_callout fn, void *handle)
{
    gs_lib_ctx_t *ctx = (gs_lib_ctx_t *)instance;
    if (instance == NULL)
        return gs_error_Fatal;
    return gs_lib_ctx_register_callout(ctx->memory, fn, handle);
}

GSDLLEXPORT void GSDLLAPI
gsapi_deregister_callout(void *instance, gs_callout fn, void *handle)
{
    gs_lib_ctx_t *ctx = (gs_lib_ctx_t *)instance;
    if (instance == NULL)
        return;
    gs_lib_ctx_deregister_callout(ctx->memory, fn, handle);
}

/* Set/Get the default device list string */
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

/* Initialise the interpreter */
GSDLLEXPORT int GSDLLAPI
gsapi_set_arg_encoding(void *instance, int encoding)
{
    gs_lib_ctx_t *ctx = (gs_lib_ctx_t *)instance;
    return psapi_set_arg_encoding(ctx, encoding);
}

GSDLLEXPORT int GSDLLAPI
gsapi_init_with_args(void *instance, int argc, char **argv)
{
    gs_lib_ctx_t *ctx = (gs_lib_ctx_t *)instance;
    return psapi_init_with_args(ctx, argc, argv);
}

/* The gsapi_run_* functions are like gs_main_run_* except
 * that the error_object is omitted.
 * An error object in minst is used instead.
 */

/* Setup up a suspendable run_string */
GSDLLEXPORT int GSDLLAPI
gsapi_run_string_begin(void *instance,
                       int   user_errors,
                       int  *pexit_code)
{
    gs_lib_ctx_t *ctx = (gs_lib_ctx_t *)instance;
    return psapi_run_string_begin(ctx, user_errors, pexit_code);
}

GSDLLEXPORT int GSDLLAPI
gsapi_run_string_continue(void         *instance,
                          const char   *str,
                          unsigned int  length,
                          int           user_errors,
                          int          *pexit_code)
{
    gs_lib_ctx_t *ctx = (gs_lib_ctx_t *)instance;
    return psapi_run_string_continue(ctx, str, length, user_errors, pexit_code);
}

GSDLLEXPORT int GSDLLAPI
gsapi_run_string_end(void *instance,
                     int   user_errors,
                     int  *pexit_code)
{
    gs_lib_ctx_t *ctx = (gs_lib_ctx_t *)instance;
    return psapi_run_string_end(ctx, user_errors, pexit_code);
}

GSDLLEXPORT int GSDLLAPI
gsapi_run_string_with_length(void         *instance,
                             const char   *str,
                             unsigned int  length,
                             int           user_errors,
                             int          *pexit_code)
{
    gs_lib_ctx_t *ctx = (gs_lib_ctx_t *)instance;
    return psapi_run_string_with_length(ctx, str, length, user_errors, pexit_code);
}

GSDLLEXPORT int GSDLLAPI
gsapi_run_string(void       *instance,
                 const char *str,
                 int         user_errors,
                 int        *pexit_code)
{
    gs_lib_ctx_t *ctx = (gs_lib_ctx_t *)instance;
    return psapi_run_string_with_length(ctx,
                                        str,
                                        (unsigned int)strlen(str),
                                        user_errors,
                                        pexit_code);
}

GSDLLEXPORT int GSDLLAPI
gsapi_run_file(void       *instance,
               const char *file_name,
               int         user_errors,
               int        *pexit_code)
{
    gs_lib_ctx_t *ctx = (gs_lib_ctx_t *)instance;
    return psapi_run_file(ctx, file_name, user_errors, pexit_code);
}

#ifdef __WIN32__
GSDLLEXPORT int GSDLLAPI
gsapi_init_with_argsW(void     *instance,
                      int       argc,
                      wchar_t **argv)
{
    gs_lib_ctx_t *ctx = (gs_lib_ctx_t *)instance;
    int code;
    gs_arg_get_codepoint *old;
    if (instance == NULL)
        return gs_error_Fatal;

    old = gs_main_inst_get_arg_decode(get_minst_from_memory(ctx->memory));
    code = psapi_set_arg_encoding(ctx, PS_ARG_ENCODING_UTF16LE);
    if (code != 0)
        return code;
    code = psapi_init_with_args(ctx, 2*argc, (char **)argv);
    gs_main_inst_arg_decode(get_minst_from_memory(ctx->memory), old);
    return code;
}

GSDLLEXPORT int GSDLLAPI
gsapi_init_with_argsA(void  *instance,
                      int    argc,
                      char **argv)
{
    gs_lib_ctx_t *ctx = (gs_lib_ctx_t *)instance;
    int code;
    gs_arg_get_codepoint *old;
    if (instance == NULL)
        return gs_error_Fatal;

    old = gs_main_inst_get_arg_decode(get_minst_from_memory(ctx->memory));
    code = psapi_set_arg_encoding(ctx, PS_ARG_ENCODING_LOCAL);
    if (code != 0)
        return code;
    code = psapi_init_with_args(ctx, 2*argc, (char **)argv);
    gs_main_inst_arg_decode(get_minst_from_memory(ctx->memory), old);
    return code;
}

GSDLLEXPORT int GSDLLAPI
gsapi_run_fileW(void          *instance,
                const wchar_t *file_name,
                int            user_errors,
                int           *pexit_code)
{
    gs_lib_ctx_t *ctx = (gs_lib_ctx_t *)instance;
    int code;
    gs_arg_get_codepoint *old;
    if (instance == NULL)
        return gs_error_Fatal;

    old = gs_main_inst_get_arg_decode(get_minst_from_memory(ctx->memory));
    code = psapi_set_arg_encoding(ctx, PS_ARG_ENCODING_UTF16LE);
    if (code != 0)
        return code;
    code = psapi_run_file(ctx, (const char *)file_name, user_errors, pexit_code);
    gs_main_inst_arg_decode(get_minst_from_memory(ctx->memory), old);
    return code;
}

GSDLLEXPORT int GSDLLAPI
gsapi_run_fileA(void       *instance,
                const char *file_name,
                int         user_errors,
                int        *pexit_code)
{
    gs_lib_ctx_t *ctx = (gs_lib_ctx_t *)instance;
    int code;
    gs_arg_get_codepoint *old;
    if (instance == NULL)
        return gs_error_Fatal;

    old = gs_main_inst_get_arg_decode(get_minst_from_memory(ctx->memory));
    code = psapi_set_arg_encoding(ctx, PS_ARG_ENCODING_LOCAL);
    if (code != 0)
        return code;
    code = psapi_run_file(ctx, (const char *)file_name, user_errors, pexit_code);
    gs_main_inst_arg_decode(get_minst_from_memory(ctx->memory), old);
    return code;
}
#endif

/* Exit the interpreter */
GSDLLEXPORT int GSDLLAPI
gsapi_exit(void *instance)
{
    gs_lib_ctx_t *ctx = (gs_lib_ctx_t *)instance;
    return psapi_exit(ctx);
}

GSDLLEXPORT int GSDLLAPI
gsapi_set_param(void *lib, gs_set_param_type type, const char *param, const void *value)
{
    int code = 0;
    gs_param_string str_value;
    bool bval;
    int more_to_come = type & gs_spt_more_to_come;
    gs_main_instance *minst;
    gs_c_param_list *params;
    gs_lib_ctx_t *ctx = (gs_lib_ctx_t *)lib;

    if (lib == NULL)
        return gs_error_Fatal;
    minst = get_minst_from_memory(ctx->memory);

    /* First off, ensure we have a param list to work with. */
    params = minst->param_list;
    if (params == NULL) {
        params = minst->param_list =
                   gs_c_param_list_alloc(minst->heap, "gs_main_instance_param_list");
        if (params == NULL)
            return_error(gs_error_VMerror);
        gs_c_param_list_write(params, minst->heap);
        gs_param_list_set_persistent_keys((gs_param_list *)params, false);
    }

    type &= ~gs_spt_more_to_come;

    /* Set the passed param in the device params */
    gs_c_param_list_write_more(params);
    switch (type)
    {
    case gs_spt_null:
        code = param_write_null((gs_param_list *) params,
                                param);
        break;
    case gs_spt_bool:
        bval = (value != NULL);
        code = param_write_bool((gs_param_list *) params,
                                param, &bval);
        break;
    case gs_spt_int:
        code = param_write_int((gs_param_list *) params,
                               param, (int *)value);
        break;
    case gs_spt_float:
        code = param_write_float((gs_param_list *) params,
                                 param, (float *)value);
        break;
    case gs_spt_name:
        param_string_from_transient_string(str_value, value);
        code = param_write_name((gs_param_list *) params,
                                param, &str_value);
        break;
    case gs_spt_string:
        param_string_from_transient_string(str_value, value);
        code = param_write_string((gs_param_list *) params,
                                  param, &str_value);
        break;
    case gs_spt_long:
        code = param_write_long((gs_param_list *) params,
                                param, (long *)value);
        break;
    case gs_spt_i64:
        code = param_write_i64((gs_param_list *) params,
                               param, (int64_t *)value);
        break;
    case gs_spt_size_t:
        code = param_write_size_t((gs_param_list *) params,
                                  param, (size_t *)value);
        break;
    case gs_spt_parsed:
        code = gs_param_list_add_parsed_value((gs_param_list *)params,
                                              param, (void *)value);
        break;
    default:
        code = gs_note_error(gs_error_rangecheck);
    }
    if (code < 0) {
        gs_c_param_list_release(params);
        return code;
    }
    gs_c_param_list_read(params);

    if (more_to_come) {
        /* Leave it in the param list for later. */
        return 0;
    }

    /* Send it to the device. */
    code = psapi_set_device_param(ctx, (gs_param_list *)params);
    if (code < 0)
        return code;

    /* Send it to the language */
    code = psapi_set_param(ctx, (gs_param_list *)params);

    gs_c_param_list_release(params);

    return code;
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

/* end of iapi.c */
