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



/* library context functionality for ghostscript
 * api callers get a gs_main_instance
 */

/* Capture stdin/out/err before gs.h redefines them. */
#include "stdio_.h"
#include "string_.h" /* memset */
#include "gp.h"
#include "gsicc_manage.h"
#include "gserrors.h"
#include "gscdefs.h"		/* for gs_lib_device_list */

/* Include the extern for the device list. */
extern_gs_lib_device_list();

static void
gs_lib_ctx_get_real_stdio(FILE **in, FILE **out, FILE **err)
{
    *in = stdin;
    *out = stdout;
    *err = stderr;
}

#include "gslibctx.h"
#include "gsmemory.h"

#ifndef GS_THREADSAFE
static gs_memory_t *mem_err_print = NULL;

gs_memory_t *
gs_lib_ctx_get_non_gc_memory_t()
{
    return mem_err_print ? mem_err_print->non_gc_memory : NULL;
}
#endif

/*  This sets the directory to prepend to the ICC profile names specified for
    defaultgray, defaultrgb, defaultcmyk, proofing, linking, named color and device */
void
gs_lib_ctx_set_icc_directory(const gs_memory_t *mem_gc, const char* pname,
                        int dir_namelen)
{
    char *result;
    gs_lib_ctx_t *p_ctx = mem_gc->gs_lib_ctx;
    gs_memory_t *p_ctx_mem = p_ctx->memory;

    /* If it is already set and the incoming is the default then don't set
       as we are coming from a VMreclaim which is trying to reset the user
       parameter */
    if (p_ctx->profiledir != NULL && strcmp(pname,DEFAULT_DIR_ICC) == 0) {
        return;
    }
    if (p_ctx->profiledir_len > 0) {
        if (strncmp(pname, p_ctx->profiledir, p_ctx->profiledir_len) == 0) {
            return;
        }
        gs_free_object(p_ctx_mem->non_gc_memory, p_ctx->profiledir,
                       "gsicc_set_icc_directory");
    }
    /* User param string.  Must allocate in non-gc memory */
    result = (char*) gs_alloc_bytes(p_ctx_mem->non_gc_memory, dir_namelen+1,
                                     "gsicc_set_icc_directory");
    if (result != NULL) {
        strcpy(result, pname);
        p_ctx->profiledir = result;
        p_ctx->profiledir_len = dir_namelen;
    }
}

/* Sets/Gets the string containing the list of default devices we should try */
int
gs_lib_ctx_set_default_device_list(const gs_memory_t *mem, const char* dev_list_str,
                        int list_str_len)
{
    char *result;
    gs_lib_ctx_t *p_ctx = mem->gs_lib_ctx;
    int code = 0;
    
    result = (char *)gs_alloc_bytes(mem->thread_safe_memory, list_str_len + 1,
             "gs_lib_ctx_set_default_device_list");

    if (result) {
      gs_free_object(mem->thread_safe_memory, p_ctx->default_device_list,
                "gs_lib_ctx_set_default_device_list");

      memcpy(result, dev_list_str, list_str_len);
      result[list_str_len] = '\0';
      p_ctx->default_device_list = result;
    }
    else {
        code = gs_note_error(gs_error_VMerror);
    }
    return code;
}

int
gs_lib_ctx_get_default_device_list(const gs_memory_t *mem, char** dev_list_str,
                        int *list_str_len)
{
    /* In the case the lib ctx hasn't been initialised */
    if (mem && mem->gs_lib_ctx && mem->gs_lib_ctx->default_device_list) {
        *dev_list_str = mem->gs_lib_ctx->default_device_list;
    }
    else {
        *dev_list_str = (char *)gs_dev_defaults;
    }

    *list_str_len = strlen(*dev_list_str);

    return 0;
}

int gs_lib_ctx_init( gs_memory_t *mem )
{
    gs_lib_ctx_t *pio = 0;

    if ( mem == 0 )
        return -1;  /* assert mem != 0 */

#ifndef GS_THREADSAFE
    mem_err_print = mem;
#endif

    if (mem->gs_lib_ctx) /* one time initialization */
        return 0;

    pio = (gs_lib_ctx_t*)gs_alloc_bytes_immovable(mem,
                                                  sizeof(gs_lib_ctx_t),
                                                  "gs_lib_ctx_init");
    if( pio == 0 )
        return -1;

    /* Wholesale blanking is cheaper than retail, and scales better when new
     * fields are added. */
    memset(pio, 0, sizeof(*pio));
    /* Now set the non zero/false/NULL things */
    pio->memory               = mem;
    gs_lib_ctx_get_real_stdio(&pio->fstdin, &pio->fstdout, &pio->fstderr );
    pio->stdin_is_interactive = true;
    /* id's 1 through 4 are reserved for Device color spaces; see gscspace.h */
    pio->gs_next_id           = 5;  /* this implies that each thread has its own complete state */

    /* Need to set this before calling gs_lib_ctx_set_icc_directory. */
    mem->gs_lib_ctx = pio;
    /* Initialize our default ICCProfilesDir */
    pio->profiledir = NULL;
    pio->profiledir_len = 0;
    gs_lib_ctx_set_icc_directory(mem, DEFAULT_DIR_ICC, strlen(DEFAULT_DIR_ICC));

    if (gs_lib_ctx_set_default_device_list(mem, gs_dev_defaults,
                        strlen(gs_dev_defaults)) < 0) {
        
        gs_free_object(mem, pio, "gsicc_set_icc_directory");
        mem->gs_lib_ctx = NULL;
    }

    /* Initialise the underlying CMS. */
    if (gscms_create(mem)) {

        gs_free_object(mem->non_gc_memory, mem->gs_lib_ctx->default_device_list,
                "gs_lib_ctx_fin");

        gs_free_object(mem, pio, "gsicc_set_icc_directory");
        mem->gs_lib_ctx = NULL;
        return -1;
    }
    
    
    gp_get_realtime(pio->real_time_0);

    return 0;
}

static void remove_ctx_pointers(gs_memory_t *mem)
{
    mem->gs_lib_ctx = NULL;
    if (mem->stable_memory && mem->stable_memory != mem)
        remove_ctx_pointers(mem->stable_memory);
    if (mem->non_gc_memory && mem->non_gc_memory != mem)
        remove_ctx_pointers(mem->non_gc_memory);
    if (mem->thread_safe_memory && mem->thread_safe_memory != mem)
        remove_ctx_pointers(mem->thread_safe_memory);
}

void gs_lib_ctx_fin( gs_memory_t *mem )
{
    gs_lib_ctx_t *ctx;
    if (!mem || !mem->gs_lib_ctx)
        return;
    gscms_destroy(mem);
    gs_free_object(mem->thread_safe_memory, mem->gs_lib_ctx->profiledir,
        "gsicc_set_icc_directory");
        
    gs_free_object(mem->thread_safe_memory, mem->gs_lib_ctx->default_device_list,
                "gs_lib_ctx_fin");

#ifndef GS_THREADSAFE
    mem_err_print = NULL;
#endif
    ctx = mem->gs_lib_ctx;
    remove_ctx_pointers(mem);
    gs_free_object(mem->thread_safe_memory, ctx, "gs_lib_ctx_init");
}

gs_lib_ctx_t *gs_lib_ctx_get_interp_instance(const gs_memory_t *mem)
{
    if (mem == NULL)
        return NULL;
    return mem->gs_lib_ctx;
}

void *gs_lib_ctx_get_cms_context( const gs_memory_t *mem )
{
    if (mem == NULL)
        return NULL;
    return mem->gs_lib_ctx->cms_context;
}

void gs_lib_ctx_set_cms_context( const gs_memory_t *mem, void *cms_context )
{
    if (mem == NULL)
        return;
    mem->gs_lib_ctx->cms_context = cms_context;
}

/* Provide a single point for all "C" stdout and stderr.
 */

int outwrite(const gs_memory_t *mem, const char *str, int len)
{
    int code;
    FILE *fout;
    gs_lib_ctx_t *pio = mem->gs_lib_ctx;

    if (len == 0)
        return 0;
    if (pio->stdout_is_redirected) {
        if (pio->stdout_to_stderr)
            return errwrite(mem, str, len);
        fout = pio->fstdout2;
    }
    else if (pio->stdout_fn) {
        return (*pio->stdout_fn)(pio->caller_handle, str, len);
    }
    else {
        fout = pio->fstdout;
    }
    code = fwrite(str, 1, len, fout);
    fflush(fout);
    return code;
}

#ifndef GS_THREADSAFE
int errwrite_nomem(const char *str, int len)
{
    return errwrite(mem_err_print, str, len);
}
#endif

int errwrite(const gs_memory_t *mem, const char *str, int len)
{
    int code;
    gs_lib_ctx_t *ctx;
    if (len == 0)
        return 0;
    if (mem == NULL) {
#ifdef GS_THREADSAFE
        return 0;
#else
        mem = mem_err_print;
        if (mem == NULL)
            return 0;
#endif
    }
    ctx = mem->gs_lib_ctx;
    if (ctx == NULL)
      return 0;
    if (ctx->stderr_fn)
        return (*ctx->stderr_fn)(ctx->caller_handle, str, len);

    code = fwrite(str, 1, len, ctx->fstderr);
    fflush(ctx->fstderr);
    return code;
}

void outflush(const gs_memory_t *mem)
{
    if (mem->gs_lib_ctx->stdout_is_redirected) {
        if (mem->gs_lib_ctx->stdout_to_stderr) {
            if (!mem->gs_lib_ctx->stderr_fn)
                fflush(mem->gs_lib_ctx->fstderr);
        }
        else
            fflush(mem->gs_lib_ctx->fstdout2);
    }
    else if (!mem->gs_lib_ctx->stdout_fn)
        fflush(mem->gs_lib_ctx->fstdout);
}

#ifndef GS_THREADSAFE
void errflush_nomem(void)
{
    errflush(mem_err_print);
}
#endif

void errflush(const gs_memory_t *mem)
{
    if (!mem->gs_lib_ctx->stderr_fn)
        fflush(mem->gs_lib_ctx->fstderr);
    /* else nothing to flush */
}
