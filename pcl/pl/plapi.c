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

#include "plapi.h"
#include "plmain.h"
#include "gsmchunk.h"
#include "gsmalloc.h"
#include "gserrors.h"
#include "gsexit.h"

GSDLLEXPORT int GSDLLAPI
plapi_new_instance(void **lib, void *caller_handle)
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
plapi_init_with_args(void *lib, int argc, char **argv)
{
    gs_lib_ctx_t *ctx = (gs_lib_ctx_t *)lib;
    if (lib == NULL)
        return gs_error_Fatal;

    return pl_main_init_with_args(pl_main_get_instance(ctx->memory), argc, argv);
}

GSDLLEXPORT int GSDLLAPI
plapi_run_file(void *lib, const char *file_name)
{
    gs_lib_ctx_t *ctx = (gs_lib_ctx_t *)lib;
    if (lib == NULL)
        return gs_error_Fatal;
    
    return pl_main_run_file(pl_main_get_instance(ctx->memory), file_name);
}

GSDLLEXPORT int GSDLLAPI
plapi_exit(void *lib)
{
    gs_lib_ctx_t *ctx = (gs_lib_ctx_t *)lib;
    if (lib == NULL)
        return gs_error_Fatal;

    pl_to_exit(ctx->memory);
    return 0;
}

GSDLLEXPORT int GSDLLAPI
plapi_delete_instance(void *lib)
{
    gs_lib_ctx_t *ctx = (gs_lib_ctx_t *)lib;
    if (lib == NULL)
        return gs_error_Fatal;

    pl_main_delete_instance(pl_main_get_instance(ctx->memory));
    return 0;
}

/* Set the display callback structure */
GSDLLEXPORT int GSDLLAPI
plapi_set_display_callback(void *lib, void *callback)
{
    gs_lib_ctx_t *ctx = (gs_lib_ctx_t *)lib;
    if (lib == NULL)
        return gs_error_Fatal;
    pl_main_set_display_callback(pl_main_get_instance(ctx->memory), callback);
    return 0;
}

GSDLLEXPORT int GSDLLAPI
plapi_run_string_begin(void *lib)
{
    gs_lib_ctx_t *ctx = (gs_lib_ctx_t *)lib;
    if (lib == NULL)
        return gs_error_Fatal;
    return pl_main_run_string_begin(pl_main_get_instance(ctx->memory));
}

GSDLLEXPORT int GSDLLAPI
plapi_run_string_continue(void *lib, const char *str, unsigned int length)
{
    gs_lib_ctx_t *ctx = (gs_lib_ctx_t *)lib;
    if (lib == NULL)
        return gs_error_Fatal;
    return pl_main_run_string_continue(pl_main_get_instance(ctx->memory), str, length);
}

GSDLLEXPORT int GSDLLAPI
plapi_run_string_end(void *lib)
{
    gs_lib_ctx_t *ctx = (gs_lib_ctx_t *)lib;
    if (lib == NULL)
        return gs_error_Fatal;
    return pl_main_run_string_end(pl_main_get_instance(ctx->memory));
}
    
