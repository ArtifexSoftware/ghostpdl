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



/* library context functionality for ghostscript
 * api callers get a gs_main_instance
 */

/* Capture stdin/out/err before gs.h redefines them. */
#include "stdio_.h"
#include "string_.h" /* memset */
#include "gp.h"
#include "gsicc_manage.h"
#include "gserrors.h"
#include "gscdefs.h"            /* for gs_lib_device_list */
#include "gsstruct.h"           /* for gs_gc_root_t */

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
    return mem_err_print ? mem_err_print : NULL;
}
#endif

/*  This sets the directory to prepend to the ICC profile names specified for
    defaultgray, defaultrgb, defaultcmyk, proofing, linking, named color and device */
int
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
        return 0;
    }
    if (p_ctx->profiledir != NULL && p_ctx->profiledir_len > 0) {
        if (strncmp(pname, p_ctx->profiledir, p_ctx->profiledir_len) == 0) {
            return 0;
        }
        gs_free_object(p_ctx_mem, p_ctx->profiledir,
                       "gs_lib_ctx_set_icc_directory");
    }
    /* User param string.  Must allocate in non-gc memory */
    result = (char*) gs_alloc_bytes(p_ctx_mem, dir_namelen+1,
                                     "gs_lib_ctx_set_icc_directory");
    if (result == NULL)
        return -1;
    strcpy(result, pname);
    p_ctx->profiledir = result;
    p_ctx->profiledir_len = dir_namelen;
    return 0;
}

/* Sets/Gets the string containing the list of default devices we should try */
int
gs_lib_ctx_set_default_device_list(const gs_memory_t *mem, const char* dev_list_str,
                        int list_str_len)
{
    char *result;
    gs_lib_ctx_t *p_ctx = mem->gs_lib_ctx;
    gs_memory_t *ctx_mem = p_ctx->memory;
    int code = 0;

    result = (char *)gs_alloc_bytes(ctx_mem, list_str_len + 1,
             "gs_lib_ctx_set_default_device_list");

    if (result) {
      gs_free_object(ctx_mem, p_ctx->default_device_list,
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

static int
gs_lib_ctx_alloc_root_structure(gs_memory_t *mem, gs_gc_root_ptr *rp)
{
	int code = 0;

	*rp = gs_raw_alloc_struct_immovable(mem, &st_gc_root_t, "gs_lib_ctx_alloc_root_structure");
	if (*rp == 0)
		code = gs_note_error(gs_error_VMerror);

	return code;
}

int gs_lib_ctx_init(gs_lib_ctx_t *ctx, gs_memory_t *mem)
{
    gs_lib_ctx_t *pio = NULL;

    /* Check the non gc allocator is being passed in */
    if (mem == 0 || mem != mem->non_gc_memory)
        return_error(gs_error_Fatal);

#ifndef GS_THREADSAFE
    mem_err_print = mem;
#endif

    if (mem->gs_lib_ctx) /* one time initialization */
        return 0;

    pio = (gs_lib_ctx_t*)gs_alloc_bytes_immovable(mem,
                                                  sizeof(gs_lib_ctx_t),
                                                  "gs_lib_ctx_init");
    if(pio == NULL)
        return -1;

    /* Wholesale blanking is cheaper than retail, and scales better when new
     * fields are added. */
    memset(pio, 0, sizeof(*pio));

    if (ctx != NULL) {
        pio->core = ctx->core;
        gx_monitor_enter((gx_monitor_t *)(pio->core->monitor));
        pio->core->refs++;
        gx_monitor_leave((gx_monitor_t *)(pio->core->monitor));
    } else {
        pio->core = (gs_lib_ctx_core_t *)gs_alloc_bytes_immovable(mem,
                                                                  sizeof(gs_lib_ctx_core_t),
                                                                  "gs_lib_ctx_init(core)");
        if (pio->core == NULL) {
            gs_free_object(mem, pio, "gs_lib_ctx_init");
            return -1;
        }
        memset(pio->core, 0, sizeof(*pio->core));

        pio->core->monitor = gx_monitor_alloc(mem);
        if (pio->core->monitor == NULL) {
            gs_free_object(mem, pio->core, "gs_lib_ctx_init");
            gs_free_object(mem, pio, "gs_lib_ctx_init");
            return -1;
        }
        pio->core->refs = 1;
        pio->core->memory = mem;

        /* Set the non-zero "shared" things */
        gs_lib_ctx_get_real_stdio(&pio->core->fstdin, &pio->core->fstdout, &pio->core->fstderr );
        pio->core->stdin_is_interactive = true;
        /* id's 1 through 4 are reserved for Device color spaces; see gscspace.h */
        pio->core->gs_next_id = 5; /* Cloned contexts share the state */
        /* Set scanconverter to 1 (default) */
        pio->core->scanconverter = GS_SCANCONVERTER_DEFAULT;
    }

    /* Now set the non zero/false/NULL things */
    pio->memory               = mem;

    /* Need to set this before calling gs_lib_ctx_set_icc_directory. */
    mem->gs_lib_ctx = pio;
    /* Initialize our default ICCProfilesDir */
    pio->profiledir = NULL;
    pio->profiledir_len = 0;
    pio->icc_color_accuracy = MAX_COLOR_ACCURACY;
    if (gs_lib_ctx_set_icc_directory(mem, DEFAULT_DIR_ICC, strlen(DEFAULT_DIR_ICC)) < 0)
      goto Failure;

    if (gs_lib_ctx_set_default_device_list(mem, gs_dev_defaults,
                                           strlen(gs_dev_defaults)) < 0)
        goto Failure;

    /* Initialise the underlying CMS. */
    if (gscms_create(mem))
        goto Failure;

    /* Initialise any lock required for the jpx codec */
    if (sjpxd_create(mem))
        goto Failure;

    pio->client_check_file_permission = NULL;
    gp_get_realtime(pio->real_time_0);

    if (gs_lib_ctx_alloc_root_structure(mem, &pio->name_table_root))
        goto Failure;

    if (gs_lib_ctx_alloc_root_structure(mem, &pio->io_device_table_root))
        goto Failure;

    if (gs_lib_ctx_alloc_root_structure(mem, &pio->font_dir_root))
        goto Failure;

    return 0;

Failure:
    gs_lib_ctx_fin(mem);
    return -1;
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

void gs_lib_ctx_fin(gs_memory_t *mem)
{
    gs_lib_ctx_t *ctx;
    gs_memory_t *ctx_mem;
    int refs;

    if (!mem || !mem->gs_lib_ctx)
        return;

    ctx = mem->gs_lib_ctx;
    ctx_mem = ctx->memory;

    sjpxd_destroy(mem);
    gscms_destroy(ctx_mem);
    gs_free_object(ctx_mem, ctx->profiledir,
        "gs_lib_ctx_fin");

    gs_free_object(ctx_mem, ctx->default_device_list,
                "gs_lib_ctx_fin");

    gs_free_object(ctx_mem, ctx->name_table_root, "gs_lib_ctx_fin");
    gs_free_object(ctx_mem, ctx->io_device_table_root, "gs_lib_ctx_fin");
    gs_free_object(ctx_mem, ctx->font_dir_root, "gs_lib_ctx_fin");

#ifndef GS_THREADSAFE
    mem_err_print = NULL;
#endif
    remove_ctx_pointers(ctx_mem);

    gx_monitor_enter((gx_monitor_t *)(ctx->core->monitor));
    refs = --ctx->core->refs;
    gx_monitor_leave((gx_monitor_t *)(ctx->core->monitor));
    if (refs == 0) {
        gx_monitor_free((gx_monitor_t *)(ctx->core->monitor));
        gs_free_object(ctx->core->memory, ctx->core, "gs_lib_ctx_fin");
    }

    gs_free_object(ctx_mem, ctx, "gs_lib_ctx_init");
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

int gs_lib_ctx_get_act_on_uel( const gs_memory_t *mem )
{
    if (mem == NULL)
        return 0;
    return mem->gs_lib_ctx->core->act_on_uel;
}

/* Provide a single point for all "C" stdout and stderr.
 */

int outwrite(const gs_memory_t *mem, const char *str, int len)
{
    int code;
    FILE *fout;
    gs_lib_ctx_t *pio = mem->gs_lib_ctx;
    gs_lib_ctx_core_t *core = pio->core;

    if (len == 0)
        return 0;
    if (core->stdout_is_redirected) {
        if (core->stdout_to_stderr)
            return errwrite(mem, str, len);
        fout = core->fstdout2;
    }
    else if (core->stdout_fn) {
        return (*core->stdout_fn)(core->caller_handle, str, len);
    }
    else {
        fout = core->fstdout;
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
    gs_lib_ctx_core_t *core;
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
    core = ctx->core;
    if (core->stderr_fn)
        return (*core->stderr_fn)(core->caller_handle, str, len);

    code = fwrite(str, 1, len, core->fstderr);
    fflush(core->fstderr);
    return code;
}

void outflush(const gs_memory_t *mem)
{
    gs_lib_ctx_core_t *core = mem->gs_lib_ctx->core;
    if (core->stdout_is_redirected) {
        if (core->stdout_to_stderr) {
            if (!core->stderr_fn)
                fflush(core->fstderr);
        }
        else
            fflush(core->fstdout2);
    }
    else if (!core->stdout_fn)
        fflush(core->fstdout);
}

#ifndef GS_THREADSAFE
void errflush_nomem(void)
{
    errflush(mem_err_print);
}
#endif

void errflush(const gs_memory_t *mem)
{
    if (!mem->gs_lib_ctx->core->stderr_fn)
        fflush(mem->gs_lib_ctx->core->fstderr);
    /* else nothing to flush */
}

int
gs_check_file_permission (gs_memory_t *mem, const char *fname, const int len, const char *permission)
{
    int code = 0;
    if (mem->gs_lib_ctx->client_check_file_permission != NULL) {
        code = mem->gs_lib_ctx->client_check_file_permission(mem, fname, len, permission);
    }
    return code;
}
