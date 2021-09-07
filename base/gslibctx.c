/* Copyright (C) 2001-2021 Artifex Software, Inc.
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
#include "gpmisc.h"
#include "gsicc_manage.h"
#include "gserrors.h"
#include "gscdefs.h"            /* for gs_lib_device_list */
#include "gsstruct.h"           /* for gs_gc_root_t */
#ifdef WITH_CAL
#include "cal.h"
#endif
#include "gsargs.h"

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
        p_ctx->profiledir = NULL;
        p_ctx->profiledir_len = 0;
    }
    /* User param string.  Must allocate in non-gc memory */
    result = (char*) gs_alloc_bytes(p_ctx_mem, dir_namelen+1,
                                     "gs_lib_ctx_set_icc_directory");
    if (result == NULL) {
        return gs_error_VMerror;
    }
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

static int
fs_file_open_file(const gs_memory_t *mem,
                        void        *secret,
                  const char        *fname,
                  const char        *mode,
                        gp_file    **file)
{
    FILE *f;

    *file = gp_file_FILE_alloc(mem);
    if (*file == NULL)
        return 0;
    f = gp_fopen_impl(mem->non_gc_memory, fname, mode);
    if (gp_file_FILE_set(*file, f, fclose)) {
        *file = NULL;
        return gs_error_VMerror;
    }
    return 0;
}

static int
fs_file_open_scratch(const gs_memory_t *mem, void *secret, const char *prefix, char *rfname, const char *mode, int rm, gp_file **file)
{
    *file = gp_file_FILE_alloc(mem);
    if (*file == NULL)
        return gs_error_VMerror;
    if (gp_file_FILE_set(*file,
                         gp_open_scratch_file_impl(mem,
                                                   prefix,
                                                   rfname,
                                                   mode,
                                                   rm),
                                                   NULL)) {
        *file = NULL;
        return gs_error_invalidfileaccess;
    }

    return 0;
}

static int
fs_file_open_printer(const gs_memory_t *mem, void *secret, const char *fname, int binary_mode, gp_file **file)
{
    FILE *f;
    int (*close)(FILE *) = NULL;

    *file = gp_file_FILE_alloc(mem);
    if (*file == NULL)
        return gs_error_VMerror;
    f = gp_open_printer_impl(mem->non_gc_memory, fname, &binary_mode, &close);
    if (gp_file_FILE_set(*file, f, close)) {
        *file = NULL;
        return gs_error_invalidfileaccess;
    }
    gp_setmode_binary_impl(f, binary_mode);

    return 0;
}

#ifdef WITH_CAL
static void *
cal_do_malloc(void *opaque, size_t size)
{
    gs_memory_t *mem = (gs_memory_t *)opaque;
    return gs_alloc_bytes(mem, size, "cal_do_malloc");
}

static void *
cal_do_realloc(void *opaque, void *ptr, size_t newsize)
{
    gs_memory_t *mem = (gs_memory_t *)opaque;
    return gs_resize_object(mem, ptr, newsize, "cal_do_malloc");
}

static void
cal_do_free(void *opaque, void *ptr)
{
    gs_memory_t *mem = (gs_memory_t *)opaque;
    gs_free_object(mem, ptr, "cal_do_malloc");
}

static cal_allocators cal_allocs =
{
    cal_do_malloc,
    cal_do_realloc,
    cal_do_free
};
#endif

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
        pio->core->fs = (gs_fs_list_t *)gs_alloc_bytes_immovable(mem,
                                                                 sizeof(gs_fs_list_t),
                                                                 "gs_lib_ctx_init(gs_fs_list_t)");
        if (pio->core->fs == NULL) {
            gs_free_object(mem, pio->core, "gs_lib_ctx_init");
            gs_free_object(mem, pio, "gs_lib_ctx_init");
            return -1;
        }
#ifdef WITH_CAL
        pio->core->cal_ctx = cal_init(&cal_allocs, mem);
        if (pio->core->cal_ctx == NULL) {
            gs_free_object(mem, pio->core->fs, "gs_lib_ctx_init");
            gs_free_object(mem, pio->core, "gs_lib_ctx_init");
            gs_free_object(mem, pio, "gs_lib_ctx_init");
            return -1;
        }
#endif
        pio->core->fs->fs.open_file = fs_file_open_file;
        /* The iodev will fill this in later, if pipes are enabled */
        pio->core->fs->fs.open_pipe = NULL;
        pio->core->fs->fs.open_scratch = fs_file_open_scratch;
        pio->core->fs->fs.open_printer = fs_file_open_printer;
        pio->core->fs->secret = NULL;
        pio->core->fs->memory = mem;
        pio->core->fs->next   = NULL;

        pio->core->monitor = gx_monitor_alloc(mem);
        if (pio->core->monitor == NULL) {
#ifdef WITH_CAL
            cal_fin(pio->core->cal_ctx, mem);
#endif
            gs_free_object(mem, pio->core->fs, "gs_lib_ctx_init");
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
    if (gs_add_control_path(mem, gs_permit_file_writing, gp_null_file_name) < 0)
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
    int refs, i;
    gs_fs_list_t *fs;
    gs_callout_list_t *entry;

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

    gx_monitor_enter((gx_monitor_t *)(ctx->core->monitor));
    refs = --ctx->core->refs;
    gx_monitor_leave((gx_monitor_t *)(ctx->core->monitor));
    if (refs == 0) {
        gx_monitor_free((gx_monitor_t *)(ctx->core->monitor));
#ifdef WITH_CAL
        cal_fin(ctx->core->cal_ctx, ctx->core->memory);
#endif
        gs_purge_scratch_files(ctx->core->memory);
        gs_purge_control_paths(ctx->core->memory, gs_permit_file_reading);
        gs_purge_control_paths(ctx->core->memory, gs_permit_file_writing);
        gs_purge_control_paths(ctx->core->memory, gs_permit_file_control);

        fs = ctx->core->fs;
        while (fs) {
            gs_fs_list_t *next = fs->next;
            gs_free_object(fs->memory, fs, "gs_lib_ctx_fin");
            fs = next;
        }

        entry = ctx->core->callouts;
        while (entry) {
            gs_callout_list_t *next = entry->next;
            gs_free_object(mem->non_gc_memory, entry, "gs_callout_list_t");
            entry = next;
        }

        for (i = 0; i < ctx->core->argc; i++)
            gs_free_object(ctx->core->memory, ctx->core->argv[i], "gs_lib_ctx_arg");
        gs_free_object(ctx->core->memory, ctx->core->argv, "gs_lib_ctx_args");

        gs_free_object(ctx->core->memory, ctx->core, "gs_lib_ctx_fin");
    }
    remove_ctx_pointers(ctx_mem);

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
    gs_lib_ctx_t *pio = mem->gs_lib_ctx;
    gs_lib_ctx_core_t *core = pio->core;

    if (len == 0)
        return 0;
    if (core->stdout_is_redirected) {
        if (core->stdout_to_stderr)
            return errwrite(mem, str, len);
        code = gp_fwrite(str, 1, len, core->fstdout2);
        gp_fflush(core->fstdout2);
    } else if (core->stdout_fn) {
        return (*core->stdout_fn)(core->std_caller_handle, str, len);
    } else {
        code = fwrite(str, 1, len, core->fstdout);
        fflush(core->fstdout);
    }
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
        return (*core->stderr_fn)(core->std_caller_handle, str, len);

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
            gp_fflush(core->fstdout2);
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

static void
rewrite_percent_specifiers(char *s)
{
    char *match_start;

    while (*s)
    {
        int flags;
        /* Find a % */
        while (*s && *s != '%')
            s++;
        if (*s == 0)
            return;
        match_start = s;
        s++;
        /* Skip over flags (just one instance of any given flag, in any order) */
        flags = 0;
        while (*s) {
            if (*s == '-' && (flags & 1) == 0)
                flags |= 1;
            else if (*s == '+' && (flags & 2) == 0)
                flags |= 2;
            else if (*s == ' ' && (flags & 4) == 0)
                flags |= 4;
            else if (*s == '0' && (flags & 8) == 0)
                flags |= 8;
            else if (*s == '#' && (flags & 16) == 0)
                flags |= 16;
            else
                break;
            s++;
        }
        /* Skip over width */
        while (*s >= '0' && *s <= '9')
            s++;
        /* Skip over .precision */
        if (*s == '.' && s[1] >= '0' && s[1] <= '9') {
            s++;
            while (*s >= '0' && *s <= '9')
                s++;
        }
        /* Skip over 'l' */
        if (*s == 'l')
            s++;
        if (*s == 'd' ||
            *s == 'i' ||
            *s == 'u' ||
            *s == 'o' ||
            *s == 'x' ||
            *s == 'X') {
            /* Success! */
            memset(match_start, '*', s - match_start + 1);
        }
        /* If we have escaped percents ("%%") so the percent
           will survive a call to sprintf and co, then we need
           to drop the extra one here, because the validation
           code will see the string *after* it's been sprintf'ed.
         */
        else if (*s == '%') {
            char *s0 = s;
            while (*s0) {
                *s0 = *(s0 + 1);
                s0++;
            }
        }
    }
}

/* For the OutputFile permission we have to deal with formattable strings
   i.e. ones that have "%d" or similar in them. For these we want to replace
   everything after the %d with a wildcard "*".
   Since we do this in two places (the -s and -o cases) split it into a
   function.
 */
#define IS_WHITESPACE(c) ((c == 0x20) || (c == 0x9) || (c == 0xD) || (c == 0xA))
int
gs_add_outputfile_control_path(gs_memory_t *mem, const char *fname)
{
    char f[gp_file_name_sizeof];
    int code;

    /* Be sure the string copy will fit */
    if (strlen(fname) >= gp_file_name_sizeof)
        return gs_error_rangecheck;
    strcpy(f, fname);
    /* Try to rewrite any %d (or similar) in the string */
    rewrite_percent_specifiers(f);

    code = gs_add_control_path(mem, gs_permit_file_control, f);
    if (code < 0)
        return code;
    return gs_add_control_path(mem, gs_permit_file_writing, f);
}

int
gs_remove_outputfile_control_path(gs_memory_t *mem, const char *fname)
{
    char f[gp_file_name_sizeof];
    int code;

    /* Be sure the string copy will fit */
    if (strlen(fname) >= gp_file_name_sizeof)
        return gs_error_rangecheck;
    strcpy(f, fname);
    /* Try to rewrite any %d (or similar) in the string */
    rewrite_percent_specifiers(f);

    code = gs_remove_control_path(mem, gs_permit_file_control, f);
    if (code < 0)
        return code;
    return gs_remove_control_path(mem, gs_permit_file_writing, f);
}

int
gs_add_explicit_control_path(gs_memory_t *mem, const char *arg, gs_path_control_t control)
{
    char *p2, *p1 = (char *)arg;
    const char *lim;
    int code = 0;

    if (arg == NULL)
        return 0;
    lim = arg + strlen(arg);
    while (code >= 0 && p1 < lim && (p2 = strchr(p1, (int)gp_file_name_list_separator)) != NULL) {
        code = gs_add_control_path_len(mem, control, p1, (int)(p2 - p1));
        p1 = p2 + 1;
    }
    if (p1 < lim)
        code = gs_add_control_path_len(mem, control, p1, (int)(lim - p1));
    return code;
}

int
gs_add_control_path_len(const gs_memory_t *mem, gs_path_control_t type, const char *path, size_t len)
{
    return gs_add_control_path_len_flags(mem, type, path, len, 0);
}

int
gs_add_control_path_len_flags(const gs_memory_t *mem, gs_path_control_t type, const char *path, size_t len, int flags)
{
    gs_path_control_set_t *control;
    unsigned int n, i;
    gs_lib_ctx_core_t *core;
    char *buffer;
    uint rlen;

    if (path == NULL || len == 0)
        return 0;

    if (mem == NULL || mem->gs_lib_ctx == NULL ||
        (core = mem->gs_lib_ctx->core) == NULL)
        return gs_error_unknownerror;

    switch(type) {
        case gs_permit_file_reading:
            control = &core->permit_reading;
            break;
        case gs_permit_file_writing:
            control = &core->permit_writing;
            break;
        case gs_permit_file_control:
            control = &core->permit_control;
            break;
        default:
            return gs_error_rangecheck;
    }

    rlen = len+1;
    buffer = (char *)gs_alloc_bytes(core->memory, rlen, "gp_validate_path");
    if (buffer == NULL)
        return gs_error_VMerror;

    if (gp_file_name_reduce(path, (uint)len, buffer, &rlen) != gp_combine_success)
        return gs_error_invalidfileaccess;
    buffer[rlen] = 0;

    n = control->num;
    for (i = 0; i < n; i++)
    {
        if (strncmp(control->entry[i].path, buffer, rlen) == 0 &&
            control->entry[i].path[rlen] == 0) {
            gs_free_object(core->memory, buffer, "gs_add_control_path_len");
            return 0; /* Already there! */
        }
    }

    if (control->num == control->max) {
        gs_path_control_entry_t *p;

        n = control->max * 2;
        if (n == 0) {
            n = 4;
            p = (gs_path_control_entry_t *)gs_alloc_bytes(core->memory, sizeof(*p)*n, "gs_lib_ctx(entries)");
        } else
            p = (gs_path_control_entry_t *)gs_resize_object(core->memory, control->entry, sizeof(*p)*n, "gs_lib_ctx(entries)");
        if (p == NULL) {
            gs_free_object(core->memory, buffer, "gs_add_control_path_len");
            return gs_error_VMerror;
        }
        control->entry = p;
        control->max = n;
    }

    n = control->num;
    control->entry[n].path = buffer;
    control->entry[n].path[len] = 0;
    control->entry[n].flags = flags;
    control->num++;

    return 0;
}

int
gs_add_control_path(const gs_memory_t *mem, gs_path_control_t type, const char *path)
{
    return gs_add_control_path_len_flags(mem, type, path, path ? strlen(path) : 0, 0);
}

int
gs_add_control_path_flags(const gs_memory_t *mem, gs_path_control_t type, const char *path, int flags)
{
    return gs_add_control_path_len_flags(mem, type, path, path ? strlen(path) : 0, flags);
}

int
gs_remove_control_path_len(const gs_memory_t *mem, gs_path_control_t type, const char *path, size_t len)
{
    return gs_remove_control_path_len_flags(mem, type, path, len, 0);
}

int
gs_remove_control_path_len_flags(const gs_memory_t *mem, gs_path_control_t type, const char *path, size_t len, int flags)
{
    gs_path_control_set_t *control;
    unsigned int n, i;
    gs_lib_ctx_core_t *core;
    char *buffer;
    uint rlen;

    if (path == NULL || len == 0)
        return 0;

    if (mem == NULL || mem->gs_lib_ctx == NULL ||
        (core = mem->gs_lib_ctx->core) == NULL)
        return gs_error_unknownerror;

    switch(type) {
        case gs_permit_file_reading:
            control = &core->permit_reading;
            break;
        case gs_permit_file_writing:
            control = &core->permit_writing;
            break;
        case gs_permit_file_control:
            control = &core->permit_control;
            break;
        default:
            return gs_error_rangecheck;
    }

    rlen = len+1;
    buffer = (char *)gs_alloc_bytes(core->memory, rlen, "gp_validate_path");
    if (buffer == NULL)
        return gs_error_VMerror;

    if (gp_file_name_reduce(path, (uint)len, buffer, &rlen) != gp_combine_success)
        return gs_error_invalidfileaccess;
    buffer[rlen] = 0;

    n = control->num;
    for (i = 0; i < n; i++) {
        if (control->entry[i].flags == flags &&
            strncmp(control->entry[i].path, buffer, len) == 0 &&
            control->entry[i].path[len] == 0)
            break;
    }
    gs_free_object(core->memory, buffer, "gs_remove_control_path_len");
    if (i == n)
        return 0;

    gs_free_object(core->memory, control->entry[i].path, "gs_lib_ctx(path)");
    for (;i < n-1; i++)
        control->entry[i] = control->entry[i+1];
    control->num = n-1;

    return 0;
}

int
gs_remove_control_path(const gs_memory_t *mem, gs_path_control_t type, const char *path)
{
    if (path == NULL)
        return 0;

    return gs_remove_control_path_len_flags(mem, type, path, strlen(path), 0);
}

int
gs_remove_control_path_flags(const gs_memory_t *mem, gs_path_control_t type, const char *path, int flags)
{
    if (path == NULL)
        return 0;

    return gs_remove_control_path_len_flags(mem, type, path, strlen(path), flags);
}

void
gs_purge_control_paths(const gs_memory_t *mem, gs_path_control_t type)
{
    gs_path_control_set_t *control;
    unsigned int n, in, out;
    gs_lib_ctx_core_t *core;

    if (mem == NULL || mem->gs_lib_ctx == NULL ||
        (core = mem->gs_lib_ctx->core) == NULL)
        return;

    switch(type) {
        case gs_permit_file_reading:
            control = &core->permit_reading;
            break;
        case gs_permit_file_writing:
            control = &core->permit_writing;
            break;
        case gs_permit_file_control:
            control = &core->permit_control;
            break;
        default:
            return;
    }

    n = control->num;
    for (in = out = 0; in < n; in++) {
        if (control->entry[in].flags & gs_path_control_flag_is_scratch_file) {
            /* Don't purge scratch files! */
            control->entry[out++] = control->entry[in];
        } else
            gs_free_object(core->memory, control->entry[in].path, "gs_lib_ctx(path)");
    }
    control->num = out;
    if (out == 0) {
        gs_free_object(core->memory, control->entry, "gs_lib_ctx(paths)");
        control->entry = NULL;
        control->max = 0;
    }
}

void
gs_purge_scratch_files(const gs_memory_t *mem)
{
    gs_path_control_set_t *control;
    gs_path_control_t type;
    int n, in, out;
    gs_lib_ctx_core_t *core;

    if (mem == NULL || mem->gs_lib_ctx == NULL ||
        (core = mem->gs_lib_ctx->core) == NULL)
        return;

    for (type = gs_permit_file_reading; type <= gs_permit_file_control; type++)
    {
        switch(type) {
            default:
            case gs_permit_file_reading:
                control = &core->permit_reading;
                break;
            case gs_permit_file_writing:
                control = &core->permit_writing;
                break;
            case gs_permit_file_control:
                control = &core->permit_control;
                break;
        }

        n = control->num;
        for (in = out = 0; in < n; in++) {
            if ((control->entry[in].flags & gs_path_control_flag_is_scratch_file) == 0) {
                /* Only purge scratch files! */
                control->entry[out++] = control->entry[in];
            } else {
                if (type == gs_permit_file_reading) {
                    /* Call gp_unlink_impl, as we don't want gp_unlink
                     * to go looking in the lists we are currently
                     * manipulating! */
                    gp_unlink_impl(core->memory, control->entry[in].path);
                }
                gs_free_object(core->memory, control->entry[in].path, "gs_lib_ctx(path)");
            }
        }
        control->num = out;
        if (out == 0) {
            gs_free_object(core->memory, control->entry, "gs_lib_ctx(paths)");
            control->entry = NULL;
            control->max = 0;
        }
    }
}

void
gs_activate_path_control(gs_memory_t *mem, int enable)
{
    gs_lib_ctx_core_t *core;

    if (mem == NULL || mem->gs_lib_ctx == NULL ||
        (core = mem->gs_lib_ctx->core) == NULL)
        return;

    core->path_control_active = enable;
}

int
gs_is_path_control_active(const gs_memory_t *mem)
{
    gs_lib_ctx_core_t *core;

    if (mem == NULL || mem->gs_lib_ctx == NULL ||
        (core = mem->gs_lib_ctx->core) == NULL)
        return 0;

    return core->path_control_active;
}

int
gs_add_fs(const gs_memory_t *mem,
                gs_fs_t     *fs,
                void        *secret)
{
    gs_fs_list_t *fsl;
    gs_lib_ctx_core_t *core;

    if (mem == NULL || mem->gs_lib_ctx == NULL ||
        (core = mem->gs_lib_ctx->core) == NULL)
        return gs_error_unknownerror;

    fsl = (gs_fs_list_t *)gs_alloc_bytes_immovable(mem->non_gc_memory,
                                                   sizeof(gs_fs_list_t),
                                                   "gs_fs_list_t");
    if (fsl == NULL)
        return gs_error_VMerror;
    fsl->fs      = *fs;
    fsl->secret  = secret;
    fsl->memory  = mem->non_gc_memory;
    fsl->next    = core->fs;
    core->fs     = fsl;

    return 0;
}

void
gs_remove_fs(const gs_memory_t *mem,
                   gs_fs_t     *rfs,
                   void        *secret)
{
    gs_fs_list_t **pfs;
    gs_lib_ctx_core_t *core;

    if (mem == NULL || mem->gs_lib_ctx == NULL ||
        (core = mem->gs_lib_ctx->core) == NULL)
        return;

    pfs = &core->fs;
    while (*pfs)
    {
        gs_fs_list_t *fs = *pfs;
        if (fs->fs.open_file == rfs->open_file &&
            fs->fs.open_pipe == rfs->open_pipe &&
            fs->fs.open_scratch == rfs->open_scratch &&
            fs->fs.open_printer == rfs->open_printer &&
            fs->secret == secret) {
            *pfs = fs->next;
            gs_free_object(fs->memory, fs, "gs_fs_t");
        } else
            pfs = &(*pfs)->next;
    }
}

int
gs_lib_ctx_stash_sanitized_arg(gs_lib_ctx_t *ctx, const char *arg)
{
    gs_lib_ctx_core_t *core;
    size_t len;
    const char *p;
    int elide = 0;

    if (ctx == NULL || ctx->core == NULL || arg == NULL)
        return 0;

    /* Sanitize arg */
    switch(*arg)
    {
    case '-':
        switch (arg[1])
        {
        case 0:   /* We can let - through unchanged */
        case '-': /* Need to check for permitted file lists */
            /* By default, we want to keep the key, but lose the value */
            p = arg+2;
            while (*p && *p != '=')
                p++;
            if (*p == '=')
                p++;
            if (*p == 0)
                break; /* No value to elide */
            /* Check for our blocked values here */
#define ARG_MATCHES(STR, ARG, LEN) \
    (strlen(STR) == LEN && !memcmp(STR, ARG, LEN))
            if (ARG_MATCHES("permit-file-read", arg+2, p-arg-3))
                elide=1;
            if (ARG_MATCHES("permit-file-write", arg+2, p-arg-3))
                elide=1;
            if (ARG_MATCHES("permit-file-control", arg+2, p-arg-3))
                elide=1;
            if (ARG_MATCHES("permit-file-all", arg+2, p-arg-3))
                elide=1;
#undef ARG_MATCHES
            /* Didn't match a blocked value, so allow it. */
            break;
        case 'd': /* We can let -dFoo=<whatever> through unchanged */
        case 'D': /* We can let -DFoo=<whatever> through unchanged */
        case 'r': /* We can let -r through unchanged */
        case 'Z': /* We can let -Z through unchanged */
        case 'g': /* We can let -g through unchanged */
        case 'P': /* We can let -P through unchanged */
        case '+': /* We can let -+ through unchanged */
        case '_': /* We can let -_ through unchanged */
        case 'u': /* We can let -u through unchanged */
        case 'q': /* We can let -q through unchanged */
            break;
        case 'I': /* Let through the I, but hide anything else */
        case 'f': /* Let through the I, but hide anything else */
            if (arg[2] == 0)
                break;
            p = arg+2;
            while (*p == 32)
                p++;
            elide = 1;
            break;
        case 's':
        case 'S':
            /* By default, we want to keep the key, but lose the value */
            p = arg+2;
            while (*p && *p != '=')
                p++;
            if (*p == '=')
                p++;
            if (*p == 0)
                break; /* No value to elide */
            /* Check for our whitelisted values here */
#define ARG_MATCHES(STR, ARG, LEN) \
    (strlen(STR) == LEN && !memcmp(STR, ARG, LEN))
            if (ARG_MATCHES("DEFAULTPAPERSIZE", arg+2, p-arg-3))
                break;
            if (ARG_MATCHES("DEVICE", arg+2, p-arg-3))
                break;
            if (ARG_MATCHES("PAPERSIZE", arg+2, p-arg-3))
                break;
            if (ARG_MATCHES("SUBSTFONT", arg+2, p-arg-3))
                break;
            if (ARG_MATCHES("ColorConversionStrategy", arg+2, p-arg-3))
                break;
            if (ARG_MATCHES("NupControl", arg+2, p-arg-3))
                break;
            if (ARG_MATCHES("PageList", arg+2, p-arg-3))
                break;
            if (ARG_MATCHES("ProcessColorModel", arg+2, p-arg-3))
                break;
#undef ARG_MATCHES
            /* Didn't match a whitelisted value, so elide it. */
            elide = 1;
            break;
        default:
            /* Shouldn't happen, but elide it just in case */
            arg = "?";
            break;
        }
        break;
    case '@':
        /* Shouldn't happen */
    default:
        /* Anything else should be elided */
        arg = "?";
        break;
    }

    core = ctx->core;
    if (elide)
        len = p-arg;
    else
        len = strlen(arg);

    if (core->arg_max == core->argc) {
        char **argv;
        int newlen = core->arg_max * 2;
        if (newlen == 0)
            newlen = 4;
        argv = (char **)gs_alloc_bytes(ctx->core->memory, sizeof(char *) * newlen,
                                       "gs_lib_ctx_args");
        if (argv == NULL)
            return gs_error_VMerror;
        if (core->argc > 0) {
            memcpy(argv, core->argv, sizeof(char *) * core->argc);
            gs_free_object(ctx->memory, core->argv, "gs_lib_ctx_args");
        }
        core->argv = argv;
        core->arg_max = newlen;
    }

    core->argv[core->argc] = (char *)gs_alloc_bytes(ctx->core->memory, len+1+elide,
                                                    "gs_lib_ctx_arg");
    if (core->argv[core->argc] == NULL)
        return gs_error_VMerror;
    memcpy(core->argv[core->argc], arg, len);
    if (elide) {
        core->argv[core->argc][len] = '?';
    }
    core->argv[core->argc][len+elide] = 0;
    core->argc++;

    return 0;
}

int
gs_lib_ctx_stash_exe(gs_lib_ctx_t *ctx, const char *arg)
{
    gs_lib_ctx_core_t *core;
    size_t len;
    const char *p, *word;
    const char *sep = gp_file_name_directory_separator();
    size_t seplen = strlen(sep);

    if (ctx == NULL || ctx->core == NULL || arg == NULL)
        return 0;

    /* Sanitize arg */
    p = arg;
    word = NULL;
    for (p = arg; *p; p++) {
        if (memcmp(sep, p, seplen) == 0) {
            word = p+seplen;
            p += seplen-1;
        }
#if defined(__WIN32__) || defined(__OS2__) || defined(METRO)
        if (*p == '\\')
            word = p+1;
#endif
    }
    len = p - (word ? word : arg) + 1;
    if (word)
        len += 5;

    core = ctx->core;
    if (core->arg_max == core->argc) {
        char **argv;
        int newlen = core->arg_max * 2;
        if (newlen == 0)
            newlen = 4;
        argv = (char **)gs_alloc_bytes(ctx->core->memory, sizeof(char *) * newlen,
                                       "gs_lib_ctx_args");
        if (argv == NULL)
            return gs_error_VMerror;
        if (core->argc > 0) {
            memcpy(argv, core->argv, sizeof(char *) * core->argc);
            gs_free_object(ctx->memory, core->argv, "gs_lib_ctx_args");
        }
        core->argv = argv;
        core->arg_max = newlen;
    }

    core->argv[core->argc] = (char *)gs_alloc_bytes(ctx->core->memory, len,
                                                    "gs_lib_ctx_arg");
    if (core->argv[core->argc] == NULL)
        return gs_error_VMerror;
    if (word)
        strcpy(core->argv[core->argc], "path/");
    else
        core->argv[core->argc][0] = 0;
    strcat(core->argv[core->argc], word ? word : arg);
    core->argc++;

    return 0;
}

int gs_lib_ctx_get_args(gs_lib_ctx_t *ctx, const char * const **argv)
{
    gs_lib_ctx_core_t *core;

    if (ctx == NULL || ctx->core == NULL || argv == NULL)
        return 0;

    core = ctx->core;
    *argv = (const char * const *)core->argv;
    return core->argc;
}

int gs_lib_ctx_register_callout(gs_memory_t *mem, gs_callout_fn fn, void *arg)
{
    gs_lib_ctx_core_t *core;
    gs_callout_list_t *entry;

    if (mem == NULL || mem->gs_lib_ctx == NULL ||
        mem->gs_lib_ctx->core == NULL || fn == NULL)
        return 0;

   core = mem->gs_lib_ctx->core;
   entry = (gs_callout_list_t *)gs_alloc_bytes(core->memory,
                                               sizeof(*entry),
                                               "gs_callout_list_t");
   if (entry == NULL)
       return_error(gs_error_VMerror);
   entry->next = core->callouts;
   entry->callout = fn;
   entry->handle = arg;
   core->callouts = entry;

   return 0;
}

void gs_lib_ctx_deregister_callout(gs_memory_t *mem, gs_callout_fn fn, void *arg)
{
    gs_lib_ctx_core_t *core;
    gs_callout_list_t **entry;

    if (mem == NULL || mem->gs_lib_ctx == NULL ||
        mem->gs_lib_ctx->core == NULL || fn == NULL)
        return;

   core = mem->gs_lib_ctx->core;
   entry = &core->callouts;
   while (*entry) {
        if ((*entry)->callout == fn && (*entry)->handle == arg) {
            gs_callout_list_t *next = (*entry)->next;
            gs_free_object(core->memory, *entry, "gs_callout_list_t");
            *entry = next;
        } else {
            entry = &(*entry)->next;
        }
   }
}

int gs_lib_ctx_callout(gs_memory_t *mem, const char *dev_name,
                       int id, int size, void *data)
{
    gs_lib_ctx_core_t *core;
    gs_callout_list_t *entry;

    if (mem == NULL || mem->gs_lib_ctx == NULL || mem->gs_lib_ctx->core == NULL)
        return -1;

    core = mem->gs_lib_ctx->core;
    entry = core->callouts;
    while (entry) {
        int code = entry->callout(mem->gs_lib_ctx->top_of_system,
                                  entry->handle, dev_name, id, size, data);
        if (code >= 0)
            return code;
        if (code != gs_error_unknownerror)
            return code;
        entry = entry->next;
    }
    return -1;
}
