/* Copyright (C) 2001-2012 Artifex Software, Inc.
   All Rights Reserved.

   This software is provided AS-IS with no warranty, either express or
   implied.

   This software is distributed under license and may not be copied,
   modified or distributed except as expressly authorized under the terms
   of the license contained in the file LICENSE in this distribution.

   Refer to licensing information at http://www.artifex.com or contact
   Artifex Software, Inc.,  7 Mt. Lassen Drive - Suite A-134, San Rafael,
   CA  94903, U.S.A., +1(415)492-9861, for further information.
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

#ifdef __ANDROID__
#include <android/log.h>

#ifdef DEBUG
#define ENABLE_ANDROID_LOGGING
#endif

#ifdef ENABLE_ANDROID_LOGGING
typedef struct
{
    int fill;
    char buffer[4096];
} android_out;

static android_out android_stdout_buffer = { 0 };
static android_out android_stderr_buffer = { 0 };

extern void usleep(int);

static int
android_output(android_out *log, const char *str, int len)
{
    const char *p, *q, *end;

    end = str + len;
    p = str;
    q = p;
    do
    {
        /* Find the end of the string, or the next \n */
        while (p != end && *p != '\n')
            p++;

        /* We need to output from q to p. Limit ourselves to what
         * will fit in the existing */
        if (p - q >= sizeof(log->buffer)-1 - log->fill)
                p = q + sizeof(log->buffer)-1 - log->fill;

        memcpy(&log->buffer[log->fill], q, p-q);
        log->fill += p-q;
        if (*p == '\n')
        {
            log->buffer[log->fill] = 0;
            __android_log_print(ANDROID_LOG_ERROR, "ghostscript", "%s", log->buffer);
            usleep(1);
            log->fill = 0;
            p++; /* Skip over the \n */
        }
        else if (log->fill >= sizeof(log->buffer)-1)
        {
            log->buffer[sizeof(log->buffer)-1] = 0;
            __android_log_print(ANDROID_LOG_ERROR, "ghostscript", "%s", log->buffer);
            usleep(1);
            log->fill = 0;
        }
        q = p;
    }
    while (p != end);

    return len;
}
#endif

static int GSDLLCALL android_stdin(void *v, char *buf, int len)
{
    return 0;
}

static int GSDLLCALL android_stdout(void *v, const char *str, int len)
{
#ifdef ENABLE_ANDROID_LOGGING
    return android_output(&android_stdout_buffer, str, len);
#else
    return len;
#endif
}

static int GSDLLCALL android_stderr(void *v, const char *str, int len)
{
#ifdef ENABLE_ANDROID_LOGGING
    return android_output(&android_stderr_buffer, str, len);
#else
    return len;
#endif
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
#elif defined(__ANDROID__)
    mem->gs_lib_ctx->stdin_fn = android_stdin;
    mem->gs_lib_ctx->stdout_fn = android_stdout;
    mem->gs_lib_ctx->stderr_fn = android_stderr;
#else
    mem->gs_lib_ctx->stdin_fn = NULL;
    mem->gs_lib_ctx->stdout_fn = NULL;
    mem->gs_lib_ctx->stderr_fn = NULL;
#endif
    mem->gs_lib_ctx->poll_fn = NULL;

    *pinstance = (void*)(mem->gs_lib_ctx);
    return gsapi_set_arg_encoding(*pinstance, GS_ARG_ENCODING_LOCAL);
}

/* Destroy an instance of Ghostscript */
/* We do not support multiple instances, so make sure
 * we use the default instance only once.
 */
GSDLLEXPORT void GSDLLAPI
gsapi_delete_instance(void *lib)
{
    gs_lib_ctx_t *ctx = (gs_lib_ctx_t *)lib;
    if ((ctx != NULL)) {
        gs_main_instance *minst = get_minst_from_memory(ctx->memory);

        ctx->caller_handle = NULL;
        ctx->stdin_fn = NULL;
        ctx->stdout_fn = NULL;
        ctx->stderr_fn = NULL;
        ctx->poll_fn = NULL;
        minst->display = NULL;

        /* Release the memory (frees up everything) */
        gs_malloc_release(minst->heap);

#ifndef GS_THREADSAFE
        --gsapi_instance_counter;
#endif
    }
}

/* Set the callback functions for stdio */
GSDLLEXPORT int GSDLLAPI
gsapi_set_stdio(void *lib,
    int(GSDLLCALL *stdin_fn)(void *caller_handle, char *buf, int len),
    int(GSDLLCALL *stdout_fn)(void *caller_handle, const char *str, int len),
    int(GSDLLCALL *stderr_fn)(void *caller_handle, const char *str, int len))
{
    gs_lib_ctx_t *ctx = (gs_lib_ctx_t *)lib;
    if (lib == NULL)
        return gs_error_Fatal;
    ctx->stdin_fn = stdin_fn;
    ctx->stdout_fn = stdout_fn;
    ctx->stderr_fn = stderr_fn;
    return 0;
}

/* Set the callback function for polling */
GSDLLEXPORT int GSDLLAPI
gsapi_set_poll(void *lib,
    int(GSDLLCALL *poll_fn)(void *caller_handle))
{
    gs_lib_ctx_t *ctx = (gs_lib_ctx_t *)lib;
    if (lib == NULL)
        return gs_error_Fatal;
    ctx->poll_fn = poll_fn;
    return 0;
}

/* Set the display callback structure */
GSDLLEXPORT int GSDLLAPI
gsapi_set_display_callback(void *lib, display_callback *callback)
{
    gs_lib_ctx_t *ctx = (gs_lib_ctx_t *)lib;
    if (lib == NULL)
        return gs_error_Fatal;
    get_minst_from_memory(ctx->memory)->display = callback;
    /* not in a language switched build */
    return 0;
}

/* Set/Get the default device list string */
GSDLLEXPORT int GSDLLAPI
gsapi_set_default_device_list(void *lib, char *list, int listlen)
{
    gs_lib_ctx_t *ctx = (gs_lib_ctx_t *)lib;
    if (lib == NULL)
        return gs_error_Fatal;
    return gs_lib_ctx_set_default_device_list(ctx->memory, list, listlen);
}

GSDLLEXPORT int GSDLLAPI
gsapi_get_default_device_list(void *lib, char **list, int *listlen)
{
    gs_lib_ctx_t *ctx = (gs_lib_ctx_t *)lib;
    if (lib == NULL)
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

#ifdef GS_NO_UTF8
static int clean8bit_get_codepoint(FILE *file, const char **astr)
{
    return (file ? fgetc(file) : (**astr ? (int)(unsigned char)*(*astr)++ : EOF));
}
#endif

/* Initialise the interpreter */
GSDLLEXPORT int GSDLLAPI
gsapi_set_arg_encoding(void *lib, int encoding)
{
    gs_lib_ctx_t *ctx = (gs_lib_ctx_t *)lib;
    if (lib == NULL)
        return gs_error_Fatal;

#if defined(GS_NO_UTF8)
    if (encoding == GS_ARG_ENCODING_LOCAL) {
        /* For GS_NO_UTF8 builds, we don't use utf8 internally, and we assume
         * that all inputs are 8 bit clean. */
        gs_main_inst_arg_decode(get_minst_from_memory(ctx->memory), clean8bit_get_codepoint);
        return 0;
    }
#else
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
#endif
    return gs_error_Fatal;
}

GSDLLEXPORT int GSDLLAPI
gsapi_init_with_args(void *lib, int argc, char **argv)
{
    gs_lib_ctx_t *ctx = (gs_lib_ctx_t *)lib;
    if (lib == NULL)
        return gs_error_Fatal;

    return gs_main_init_with_args(get_minst_from_memory(ctx->memory), argc, argv);
}

/* The gsapi_run_* functions are like gs_main_run_* except
 * that the error_object is omitted.
 * An error object in minst is used instead.
 */

/* Setup up a suspendable run_string */
GSDLLEXPORT int GSDLLAPI
gsapi_run_string_begin(void *lib, int user_errors,
        int *pexit_code)
{
    gs_lib_ctx_t *ctx = (gs_lib_ctx_t *)lib;
    if (lib == NULL)
        return gs_error_Fatal;

    return gs_main_run_string_begin(get_minst_from_memory(ctx->memory),
                                    user_errors, pexit_code,
                                    &(get_minst_from_memory(ctx->memory)->error_object));
}

GSDLLEXPORT int GSDLLAPI
gsapi_run_string_continue(void *lib,
        const char *str, uint length, int user_errors, int *pexit_code)
{
    gs_lib_ctx_t *ctx = (gs_lib_ctx_t *)lib;
    if (lib == NULL)
        return gs_error_Fatal;

    return gs_main_run_string_continue(get_minst_from_memory(ctx->memory),
                                       str, length, user_errors, pexit_code,
                                       &(get_minst_from_memory(ctx->memory)->error_object));
}

GSDLLEXPORT int GSDLLAPI
gsapi_run_string_end(void *lib,
        int user_errors, int *pexit_code)
{
    gs_lib_ctx_t *ctx = (gs_lib_ctx_t *)lib;
    if (lib == NULL)
        return gs_error_Fatal;

    return gs_main_run_string_end(get_minst_from_memory(ctx->memory),
                                  user_errors, pexit_code,
                                  &(get_minst_from_memory(ctx->memory)->error_object));
}

GSDLLEXPORT int GSDLLAPI
gsapi_run_string_with_length(void *lib,
        const char *str, uint length, int user_errors, int *pexit_code)
{
    gs_lib_ctx_t *ctx = (gs_lib_ctx_t *)lib;
    if (lib == NULL)
        return gs_error_Fatal;

    return gs_main_run_string_with_length(get_minst_from_memory(ctx->memory),
                                          str, length, user_errors, pexit_code,
                                          &(get_minst_from_memory(ctx->memory)->error_object));
}

GSDLLEXPORT int GSDLLAPI
gsapi_run_string(void *lib,
        const char *str, int user_errors, int *pexit_code)
{
    return gsapi_run_string_with_length(lib,
        str, (uint)strlen(str), user_errors, pexit_code);
}

GSDLLEXPORT int GSDLLAPI
gsapi_run_file(void *lib, const char *file_name,
        int user_errors, int *pexit_code)
{
#ifndef GS_NO_UTF8
    char *d, *temp;
    const char *c = file_name;
    char dummy[6];
    int rune, code, len;
#endif
    gs_lib_ctx_t *ctx = (gs_lib_ctx_t *)lib;
    gs_main_instance *minst;
    if (lib == NULL)
        return gs_error_Fatal;
    minst = get_minst_from_memory(ctx->memory);

#ifdef GS_NO_UTF8
    return gs_main_run_file(minst, file_name, user_errors, pexit_code,
                            &(minst->error_object));
#else
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
    code =  gs_main_run_file(minst, temp, user_errors, pexit_code,
                             &(minst->error_object));
    if (temp != file_name)
        gs_free_object(ctx->memory, temp, "gsapi_run_file");
    return code;
#endif
}

#ifdef __WIN32__
GSDLLEXPORT int GSDLLAPI
gsapi_init_with_argsW(void *lib, int argc, wchar_t **argv)
{
#ifdef GS_NO_UTF8
    /* Cannot call the W entrypoints in a GS_NO_UTF8 build */
    return gs_error_Fatal;
#else
    gs_lib_ctx_t *ctx = (gs_lib_ctx_t *)lib;
    int code;
    gs_arg_get_codepoint *old;
    if (lib == NULL)
        return gs_error_Fatal;

    old = gs_main_inst_get_arg_decode(get_minst_from_memory(ctx->memory));
    code = gsapi_set_arg_encoding(lib, GS_ARG_ENCODING_UTF16LE);
    if (code != 0)
        return code;
    code = gsapi_init_with_args(lib, 2*argc, (char **)argv);
    gs_main_inst_arg_decode(get_minst_from_memory(ctx->memory), old);
    return code;
#endif
}

GSDLLEXPORT int GSDLLAPI
gsapi_init_with_argsA(void *lib, int argc, char **argv)
{
    gs_lib_ctx_t *ctx = (gs_lib_ctx_t *)lib;
    int code;
    gs_arg_get_codepoint *old;
    if (lib == NULL)
        return gs_error_Fatal;

    old = gs_main_inst_get_arg_decode(get_minst_from_memory(ctx->memory));
    code = gsapi_set_arg_encoding(lib, GS_ARG_ENCODING_LOCAL);
    if (code != 0)
        return code;
    code = gsapi_init_with_args(lib, 2*argc, (char **)argv);
    gs_main_inst_arg_decode(get_minst_from_memory(ctx->memory), old);
    return code;
}

GSDLLEXPORT int GSDLLAPI
gsapi_run_fileW(void *lib, const wchar_t *file_name,
        int user_errors, int *pexit_code)
{
#ifdef GS_NO_UTF8
    /* Cannot call the W entrypoints in a GS_NO_UTF8 build */
    return gs_error_Fatal;
#else
    gs_lib_ctx_t *ctx = (gs_lib_ctx_t *)lib;
    int code;
    gs_arg_get_codepoint *old;
    if (lib == NULL)
        return gs_error_Fatal;

    old = gs_main_inst_get_arg_decode(get_minst_from_memory(ctx->memory));
    code = gsapi_set_arg_encoding(lib, GS_ARG_ENCODING_UTF16LE);
    if (code != 0)
        return code;
    code = gsapi_run_file(lib, (const char *)file_name, user_errors, pexit_code);
    gs_main_inst_arg_decode(get_minst_from_memory(ctx->memory), old);
    return code;
#endif
}

GSDLLEXPORT int GSDLLAPI
gsapi_run_fileA(void *lib, const char *file_name,
        int user_errors, int *pexit_code)
{
    gs_lib_ctx_t *ctx = (gs_lib_ctx_t *)lib;
    int code;
    gs_arg_get_codepoint *old;
    if (lib == NULL)
        return gs_error_Fatal;

    old = gs_main_inst_get_arg_decode(get_minst_from_memory(ctx->memory));
    code = gsapi_set_arg_encoding(lib, GS_ARG_ENCODING_LOCAL);
    if (code != 0)
        return code;
    code = gsapi_run_file(lib, (const char *)file_name, user_errors, pexit_code);
    gs_main_inst_arg_decode(get_minst_from_memory(ctx->memory), old);
    return code;
}
#endif

/* Exit the interpreter */
GSDLLEXPORT int GSDLLAPI
gsapi_exit(void *lib)
{
    gs_lib_ctx_t *ctx = (gs_lib_ctx_t *)lib;
    if (lib == NULL)
        return gs_error_Fatal;

    gs_to_exit(ctx->memory, 0);
    return 0;
}

/* end of iapi.c */
