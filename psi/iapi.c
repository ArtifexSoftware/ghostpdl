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



/* Public Application Programming Interface to Ghostscript interpreter */

#include "string_.h"
#include "ierrors.h"
#include "gscdefs.h"
#include "gstypes.h"
#include "gdebug.h"
#include "iapi.h"	/* Public API */
#include "iref.h"
#include "iminst.h"
#include "imain.h"
#include "imainarg.h"
#include "gsmemory.h"
#include "gsmalloc.h"
#include "gslibctx.h"
#include "gp.h"
#include "gsargs.h"

#ifndef GS_THREADSAFE
/* Number of threads to allow per process. Unless GS_THREADSAFE is defined
 * more than 1 is guaranteed to fail.
 */
static int gsapi_instance_counter = 0;
static const int gsapi_instance_max = 1;
#endif


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
GSDLLEXPORT int GSDLLAPI
gsapi_new_instance(void **pinstance, void *caller_handle)
{
    gs_memory_t *mem = NULL;
    gs_main_instance *minst = NULL;

    if (pinstance == NULL)
        return gs_error_Fatal;

#ifndef GS_THREADSAFE
    /* limited to 1 instance, till it works :) */
    if ( gsapi_instance_counter >= gsapi_instance_max )
        return gs_error_Fatal;
    ++gsapi_instance_counter;
#endif

    if (*pinstance == NULL)
        /* first instance in this process */
        mem = gs_malloc_init();
    else {
        /* nothing different for second thread initialization
         * seperate memory, ids, only stdio is process shared.
         */
        mem = gs_malloc_init();

    }
    if (mem == NULL)
        return gs_error_Fatal;
    minst = gs_main_alloc_instance(mem);
    if (minst == NULL) {
        gs_malloc_release(mem);
        return gs_error_Fatal;
    }
    mem->gs_lib_ctx->top_of_system = (void*) minst;
    mem->gs_lib_ctx->caller_handle = caller_handle;
    mem->gs_lib_ctx->custom_color_callback = NULL;
#ifdef METRO
    mem->gs_lib_ctx->stdin_fn = metro_stdin;
    mem->gs_lib_ctx->stdout_fn = metro_stdout;
    mem->gs_lib_ctx->stderr_fn = metro_stderr;
#else
    mem->gs_lib_ctx->stdin_fn = NULL;
    mem->gs_lib_ctx->stdout_fn = NULL;
    mem->gs_lib_ctx->stderr_fn = NULL;
#endif
    mem->gs_lib_ctx->poll_fn = NULL;

    *pinstance = (void*)(mem->gs_lib_ctx);
    return gsapi_set_arg_encoding(*pinstance, GS_ARG_ENCODING_LOCAL);
}

/* Set an instance of Ghostscript to respond to UEL (universal
 * exit language) strings in the input. */
GSDLLEXPORT void GSDLLAPI
gsapi_act_on_uel(void *instance)
{
    gs_lib_ctx_t *ctx = (gs_lib_ctx_t *)instance;

    ctx->act_on_uel = 1;
}

/* Destroy an instance of Ghostscript */
/* We do not support multiple instances, so make sure
 * we use the default instance only once.
 */
GSDLLEXPORT void GSDLLAPI
gsapi_delete_instance(void *instance)
{
    gs_lib_ctx_t *ctx = (gs_lib_ctx_t *)instance;
    if ((ctx != NULL)) {
        gs_memory_t *mem = (gs_memory_t *)(ctx->memory);
        gs_main_instance *minst = get_minst_from_memory(ctx->memory);

        ctx->caller_handle = NULL;
        ctx->stdin_fn = NULL;
        ctx->stdout_fn = NULL;
        ctx->stderr_fn = NULL;
        ctx->poll_fn = NULL;
        minst->display = NULL;

        gs_free_object(mem, minst, "init_main_instance");

        /* Release the memory (frees up everything) */
        gs_malloc_release(mem);

#ifndef GS_THREADSAFE
        --gsapi_instance_counter;
#endif
    }
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
    ctx->stdin_fn = stdin_fn;
    ctx->stdout_fn = stdout_fn;
    ctx->stderr_fn = stderr_fn;
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
    ctx->poll_fn = poll_fn;
    return 0;
}

/* Set the display callback structure */
GSDLLEXPORT int GSDLLAPI
gsapi_set_display_callback(void *instance, display_callback *callback)
{
    gs_lib_ctx_t *ctx = (gs_lib_ctx_t *)instance;
    if (instance == NULL)
        return gs_error_Fatal;
    get_minst_from_memory(ctx->memory)->display = callback;
    /* not in a language switched build */
    return 0;
}

/* Set/Get the default device list string */
GSDLLEXPORT int GSDLLAPI
gsapi_set_default_device_list(void *instance, char *list, int listlen)
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

static int utf16le_get_codepoint(FILE *file, const char **astr)
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
            rune = fgetc(file);
            if (rune == EOF)
                return EOF;
            c = fgetc(file);
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
            trail = fgetc(file);
            if (trail == EOF)
                return EOF;
            c = fgetc(file);
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

    if (encoding == GS_ARG_ENCODING_LOCAL) {
#if defined(__WIN32__) && !defined(METRO)
        /* For windows, we need to set it up so that we convert from 'local'
         * format (in this case whatever codepage is set) to utf8 format. At
         * the moment, all the other OS we care about provide utf8 anyway.
         */
        gs_main_inst_arg_decode(get_minst_from_memory(ctx->memory), gp_local_arg_encoding_get_codepoint);
#else
        gs_main_inst_arg_decode(get_minst_from_memory(ctx->memory), NULL);
#endif /* WIN32 */
        return 0;
    }
    if (encoding == GS_ARG_ENCODING_UTF8) {
        gs_main_inst_arg_decode(get_minst_from_memory(ctx->memory), NULL);
        return 0;
    }
    if (encoding == GS_ARG_ENCODING_UTF16LE) {
        gs_main_inst_arg_decode(get_minst_from_memory(ctx->memory), utf16le_get_codepoint);
        return 0;
    }
    return gs_error_Fatal;
}

GSDLLEXPORT int GSDLLAPI
gsapi_init_with_args(void *instance, int argc, char **argv)
{
    gs_lib_ctx_t *ctx = (gs_lib_ctx_t *)instance;
    if (instance == NULL)
        return gs_error_Fatal;

    return gs_main_init_with_args(get_minst_from_memory(ctx->memory), argc, argv);
}

/* The gsapi_run_* functions are like gs_main_run_* except
 * that the error_object is omitted.
 * An error object in minst is used instead.
 */

/* Setup up a suspendable run_string */
GSDLLEXPORT int GSDLLAPI
gsapi_run_string_begin(void *instance, int user_errors,
        int *pexit_code)
{
    gs_lib_ctx_t *ctx = (gs_lib_ctx_t *)instance;
    if (instance == NULL)
        return gs_error_Fatal;

    return gs_main_run_string_begin(get_minst_from_memory(ctx->memory),
                                    user_errors, pexit_code,
                                    &(get_minst_from_memory(ctx->memory)->error_object));
}

GSDLLEXPORT int GSDLLAPI
gsapi_run_string_continue(void *instance,
        const char *str, uint length, int user_errors, int *pexit_code)
{
    gs_lib_ctx_t *ctx = (gs_lib_ctx_t *)instance;
    if (instance == NULL)
        return gs_error_Fatal;

    return gs_main_run_string_continue(get_minst_from_memory(ctx->memory),
                                       str, length, user_errors, pexit_code,
                                       &(get_minst_from_memory(ctx->memory)->error_object));
}

GSDLLEXPORT uint GSDLLAPI
gsapi_get_uel_offset(void *instance)
{
    gs_lib_ctx_t *ctx = (gs_lib_ctx_t *)instance;
    if (instance == NULL)
        return 0;

    return gs_main_get_uel_offset(get_minst_from_memory(ctx->memory));
}

GSDLLEXPORT int GSDLLAPI
gsapi_run_string_end(void *instance,
        int user_errors, int *pexit_code)
{
    gs_lib_ctx_t *ctx = (gs_lib_ctx_t *)instance;
    if (instance == NULL)
        return gs_error_Fatal;

    return gs_main_run_string_end(get_minst_from_memory(ctx->memory),
                                  user_errors, pexit_code,
                                  &(get_minst_from_memory(ctx->memory)->error_object));
}

GSDLLEXPORT int GSDLLAPI
gsapi_run_string_with_length(void *instance,
        const char *str, uint length, int user_errors, int *pexit_code)
{
    gs_lib_ctx_t *ctx = (gs_lib_ctx_t *)instance;
    if (instance == NULL)
        return gs_error_Fatal;

    return gs_main_run_string_with_length(get_minst_from_memory(ctx->memory),
                                          str, length, user_errors, pexit_code,
                                          &(get_minst_from_memory(ctx->memory)->error_object));
}

GSDLLEXPORT int GSDLLAPI
gsapi_run_string(void *instance,
        const char *str, int user_errors, int *pexit_code)
{
    return gsapi_run_string_with_length(instance,
        str, (uint)strlen(str), user_errors, pexit_code);
}

GSDLLEXPORT int GSDLLAPI
gsapi_run_file(void *instance, const char *file_name,
        int user_errors, int *pexit_code)
{
    char *d, *temp;
    const char *c = file_name;
    char dummy[6];
    int rune, code, len;
    gs_lib_ctx_t *ctx = (gs_lib_ctx_t *)instance;
    gs_main_instance *minst;
    if (instance == NULL)
        return gs_error_Fatal;
    minst = get_minst_from_memory(ctx->memory);

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

#ifdef __WIN32__
GSDLLEXPORT int GSDLLAPI
gsapi_init_with_argsW(void *instance, int argc, wchar_t **argv)
{
    gs_lib_ctx_t *ctx = (gs_lib_ctx_t *)instance;
    int code;
    gs_arg_get_codepoint *old;
    if (instance == NULL)
        return gs_error_Fatal;

    old = gs_main_inst_get_arg_decode(get_minst_from_memory(ctx->memory));
    code = gsapi_set_arg_encoding(instance, GS_ARG_ENCODING_UTF16LE);
    if (code != 0)
        return code;
    code = gsapi_init_with_args(instance, 2*argc, (char **)argv);
    gs_main_inst_arg_decode(get_minst_from_memory(ctx->memory), old);
    return code;
}

GSDLLEXPORT int GSDLLAPI
gsapi_init_with_argsA(void *instance, int argc, char **argv)
{
    gs_lib_ctx_t *ctx = (gs_lib_ctx_t *)instance;
    int code;
    gs_arg_get_codepoint *old;
    if (instance == NULL)
        return gs_error_Fatal;

    old = gs_main_inst_get_arg_decode(get_minst_from_memory(ctx->memory));
    code = gsapi_set_arg_encoding(instance, GS_ARG_ENCODING_LOCAL);
    if (code != 0)
        return code;
    code = gsapi_init_with_args(instance, 2*argc, (char **)argv);
    gs_main_inst_arg_decode(get_minst_from_memory(ctx->memory), old);
    return code;
}

GSDLLEXPORT int GSDLLAPI
gsapi_run_fileW(void *instance, const wchar_t *file_name,
        int user_errors, int *pexit_code)
{
    gs_lib_ctx_t *ctx = (gs_lib_ctx_t *)instance;
    int code;
    gs_arg_get_codepoint *old;
    if (instance == NULL)
        return gs_error_Fatal;

    old = gs_main_inst_get_arg_decode(get_minst_from_memory(ctx->memory));
    code = gsapi_set_arg_encoding(instance, GS_ARG_ENCODING_UTF16LE);
    if (code != 0)
        return code;
    code = gsapi_run_file(instance, (const char *)file_name, user_errors, pexit_code);
    gs_main_inst_arg_decode(get_minst_from_memory(ctx->memory), old);
    return code;
}

GSDLLEXPORT int GSDLLAPI
gsapi_run_fileA(void *instance, const char *file_name,
        int user_errors, int *pexit_code)
{
    gs_lib_ctx_t *ctx = (gs_lib_ctx_t *)instance;
    int code;
    gs_arg_get_codepoint *old;
    if (instance == NULL)
        return gs_error_Fatal;

    old = gs_main_inst_get_arg_decode(get_minst_from_memory(ctx->memory));
    code = gsapi_set_arg_encoding(instance, GS_ARG_ENCODING_LOCAL);
    if (code != 0)
        return code;
    code = gsapi_run_file(instance, (const char *)file_name, user_errors, pexit_code);
    gs_main_inst_arg_decode(get_minst_from_memory(ctx->memory), old);
    return code;
}
#endif

/* Retrieve the memory allocator for the interpreter instance */
GSDLLEXPORT gs_memory_t * GSDLLAPI
gsapi_get_device_memory(void *instance)
{
    gs_lib_ctx_t *ctx = (gs_lib_ctx_t *)instance;
    return gs_main_get_device_memory(get_minst_from_memory(ctx->memory));
}

/* Retrieve the memory allocator for the interpreter instance */
GSDLLEXPORT int GSDLLAPI
gsapi_set_device(void *instance, gx_device *pdev)
{
    gs_lib_ctx_t *ctx = (gs_lib_ctx_t *)instance;
    return gs_main_set_device(get_minst_from_memory(ctx->memory), pdev);
}

/* Exit the interpreter */
GSDLLEXPORT int GSDLLAPI
gsapi_exit(void *instance)
{
    gs_lib_ctx_t *ctx = (gs_lib_ctx_t *)instance;
    if (instance == NULL)
        return gs_error_Fatal;

    gs_to_exit(ctx->memory, 0);
    return 0;
}

/* end of iapi.c */
