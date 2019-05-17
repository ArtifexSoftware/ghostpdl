/* Copyright (C) 2001-2019 Artifex Software, Inc.
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
    ctx->core->stdin_fn  = stdin_fn;
    ctx->core->stdout_fn = stdout_fn;
    ctx->core->stderr_fn = stderr_fn;

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
gsapi_run_file(void *lib, const char *file_name)
{
    gs_lib_ctx_t *ctx = (gs_lib_ctx_t *)lib;
    if (lib == NULL)
        return gs_error_Fatal;

    return pl_main_run_file(pl_main_get_instance(ctx->memory), file_name);
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
    ctx->core->poll_fn = poll_fn;
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
gsapi_run_string_begin(void *lib)
{
    gs_lib_ctx_t *ctx = (gs_lib_ctx_t *)lib;
    if (lib == NULL)
        return gs_error_Fatal;
    return pl_main_run_string_begin(pl_main_get_instance(ctx->memory));
}

GSDLLEXPORT int GSDLLAPI
gsapi_run_string_continue(void *lib, const char *str, unsigned int length)
{
    gs_lib_ctx_t *ctx = (gs_lib_ctx_t *)lib;
    if (lib == NULL)
        return gs_error_Fatal;
    return pl_main_run_string_continue(pl_main_get_instance(ctx->memory), str, length);
}

GSDLLEXPORT int GSDLLAPI
gsapi_run_string_end(void *lib)
{
    gs_lib_ctx_t *ctx = (gs_lib_ctx_t *)lib;
    if (lib == NULL)
        return gs_error_Fatal;
    return pl_main_run_string_end(pl_main_get_instance(ctx->memory));
}
    
GSDLLEXPORT int GSDLLAPI
gsapi_set_param(void *lib, gs_set_param_type type, const char *param, const void *value)
{
    gs_lib_ctx_t *ctx = (gs_lib_ctx_t *)lib;
    if (lib == NULL)
        return gs_error_Fatal;
    return pl_main_set_typed_param(pl_main_get_instance(ctx->memory), (pl_set_param_type)type, param, value);
}
