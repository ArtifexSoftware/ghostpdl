/* Copyright (C) 2001-2023 Artifex Software, Inc.
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


#ifndef GSLIBCTX_H
#define GSLIBCTX_H

#include "std.h"
#include "stdio_.h"
#include "gs_dll_call.h"

typedef struct name_table_s *name_table_ptr;

/* Opaque type for root pointer here, because including gsstruct.h for the
   original gs_gc_root_t definition resulted in a circular header dependency. */
typedef struct gs_gc_root_s *gs_gc_root_ptr;

typedef struct gs_fapi_server_s gs_fapi_server;

/* A 'font directory' object (to avoid making fonts global). */
/* 'directory' is something of a misnomer: this structure */
/* just keeps track of the defined fonts, and the scaled font and */
/* rendered character caches. */
typedef struct gs_font_dir_s gs_font_dir;

typedef int (*client_check_file_permission_t) (gs_memory_t *mem, const char *fname, const int len, const char *permission);

typedef struct {
    char     *path;
    int       flags;
} gs_path_control_entry_t;

typedef struct {
    unsigned int             max;
    unsigned int             num;
    gs_path_control_entry_t *entry;
} gs_path_control_set_t;

typedef struct {
    int (*open_file)(const gs_memory_t *mem,
                           void        *secret,
                     const char        *fname,
                     const char        *mode,
                           gp_file    **file);
    int (*open_pipe)(const gs_memory_t *mem,
                           void        *secret,
                     const char        *fname,
                           char        *rfname, /* 4096 bytes */
                     const char        *mode,
                           gp_file    **file);
    int (*open_scratch)(const gs_memory_t *mem,
                              void        *secret,
                        const char        *prefix,
                              char        *rfname, /* 4096 bytes */
                        const char        *mode,
                              int          rm,
                              gp_file    **file);
    int (*open_printer)(const gs_memory_t *mem,
                              void        *secret,
                        const char        *fname, /* 4096 bytes */
                              int          binary,
                              gp_file    **file);
    int (*open_handle)(const gs_memory_t *mem,
                             void        *secret,
                       const char        *fname, /* 4096 bytes */
                       const char        *access,
                             gp_file    **file);
} gs_fs_t;

typedef struct gs_fs_list_s {
    gs_fs_t         fs;
    void           *secret;
    gs_memory_t    *memory;
    struct gs_fs_list_s *next;
} gs_fs_list_t;

typedef int (*gs_callout_fn)(void *, void *, const char *, int, int, void *);

typedef struct gs_callout_list_s {
    struct gs_callout_list_s *next;
    gs_callout_fn callout;
    void *handle;
} gs_callout_list_t;

typedef struct gs_globals gs_globals;

typedef struct {
    void *monitor;
    int refs;
    gs_memory_t *memory;
    FILE *fstdin;
    FILE *fstdout;
    FILE *fstderr;
    gp_file *fstdout2;		/* for redirecting %stdout and diagnostics */
    int stdout_is_redirected;	/* to stderr or fstdout2 */
    int stdout_to_stderr;
    int stdin_is_interactive;
    void *default_caller_handle;	/* identifies caller of GS DLL/shared object */
    void *std_caller_handle;
    void *poll_caller_handle;
    void *custom_color_callback;  /* pointer to color callback structure */
    int (GSDLLCALL *stdin_fn)(void *caller_handle, char *buf, int len);
    int (GSDLLCALL *stdout_fn)(void *caller_handle, const char *str, int len);
    int (GSDLLCALL *stderr_fn)(void *caller_handle, const char *str, int len);
    int (GSDLLCALL *poll_fn)(void *caller_handle);
    ulong gs_next_id; /* gs_id initialized here, private variable of gs_next_ids() */
    /* True if we are emulating CPSI. Ideally this would be in the imager
     * state, but this can't be done due to problems detecting changes in it
     * for the clist based devices. */
    int CPSI_mode;
    int scanconverter;
    int act_on_uel;

    int path_control_active;
    gs_path_control_set_t permit_reading;
    gs_path_control_set_t permit_writing;
    gs_path_control_set_t permit_control;
    gs_fs_list_t *fs;
    /* Ideally this pointer would only be present in CAL builds,
     * but that's too hard to arrange, so we live with it in
     * all builds. */
    void *cal_ctx;

    void *cms_context;  /* Opaque context pointer from underlying CMS in use */

    gs_callout_list_t *callouts;

    /* Stashed args */
    int arg_max;
    int argc;
    char **argv;

    /* clist io procs pointers. Indirected through here to allow
     * easy build time selection. */
    const void *clist_io_procs_memory;
    const void *clist_io_procs_file;

    gs_globals *globals;
} gs_lib_ctx_core_t;

typedef struct gs_lib_ctx_s
{
    gs_memory_t *memory;  /* mem->gs_lib_ctx->memory == mem */
    gs_lib_ctx_core_t *core;
    void *top_of_system;  /* use accessor functions to walk down the system
                           * to the desired structure gs_lib_ctx_get_*()
                           */
    name_table_ptr gs_name_table;  /* hack this is the ps interpreters name table
                                    * doesn't belong here
                                    */
    gs_gc_root_ptr name_table_root;
    /* Define whether dictionaries expand automatically when full. */
    int dict_auto_expand;  /* ps dictionary: false level 1 true level 2 or 3 */
    /* A table of local copies of the IODevices */
    struct gx_io_device_s **io_device_table;
    int io_device_table_count;
    int io_device_table_size;
    gs_gc_root_ptr io_device_table_root;
    client_check_file_permission_t client_check_file_permission;
    /* Define the default value of AccurateScreens that affects setscreen
       and setcolorscreen. */
    int screen_accurate_screens;
    uint screen_min_screen_levels;
    /* Accuracy vs. performance for ICC color */
    uint icc_color_accuracy;
    /* real time clock 'bias' value. Not strictly required, but some FTS
     * tests work better if realtime starts from 0 at boot time. */
    long real_time_0[2];

    /* font directory - see gsfont.h */
    gs_font_dir *font_dir;
    gs_gc_root_ptr font_dir_root;
    /* Keep the path for the ICCProfiles here so devices and the icc_manager
     * can get to it. Prevents needing two copies, one in the icc_manager
     * and one in the device */
    char *profiledir;               /* Directory used in searching for ICC profiles */
    int profiledir_len;             /* length of directory name (allows for Unicode) */
    gs_fapi_server **fapi_servers;
    char *default_device_list;
    int gcsignal;
    void *sjpxd_private; /* optional for use of jpx codec */
} gs_lib_ctx_t;

enum {
    GS_SCANCONVERTER_OLD = 0,
    GS_SCANCONVERTER_DEFAULT = 1,
    GS_SCANCONVERTER_EDGEBUFFER = 2,

    /* And finally a flag to let us know which is the default */
    GS_SCANCONVERTER_DEFAULT_IS_EDGEBUFFER = 1
};

/** initializes and stores itself in the given gs_memory_t pointer.
 * it is the responsibility of the gs_memory_t objects to copy
 * the pointer to subsequent memory objects.
 */
int gs_lib_ctx_init( gs_lib_ctx_t *ctx, gs_memory_t *mem );

/** Called when the lowest level allocator (the one which the lib_ctx was
 * initialised under) is about to be destroyed. The lib_ctx should tidy up
 * after itself. */
void gs_lib_ctx_fin( gs_memory_t *mem );

gs_lib_ctx_t *gs_lib_ctx_get_interp_instance( const gs_memory_t *mem );

void *gs_lib_ctx_get_cms_context( const gs_memory_t *mem );
int gs_lib_ctx_get_act_on_uel( const gs_memory_t *mem );

int gs_lib_ctx_register_callout(gs_memory_t *mem, gs_callout_fn, void *arg);
void gs_lib_ctx_deregister_callout(gs_memory_t *mem, gs_callout_fn, void *arg);
int gs_lib_ctx_callout(gs_memory_t *mem, const char *dev_name,
                       int id, int size, void *data);
int gs_lib_ctx_nts_adjust(gs_memory_t *mem, int adjust);

int gs_lib_ctx_set_icc_directory(const gs_memory_t *mem_gc, const char* pname,
                                 int dir_namelen);


/* Sets/Gets the string containing the list of device names we should search
 * to find a suitable default
 */
int
gs_lib_ctx_set_default_device_list(const gs_memory_t *mem, const char* dev_list_str,
                        int list_str_len);

/* Returns a pointer to the string not a new string */
int
gs_lib_ctx_get_default_device_list(const gs_memory_t *mem, char** dev_list_str,
                        int *list_str_len);

int
gs_check_file_permission (gs_memory_t *mem, const char *fname, const int len, const char *permission);

#define IS_LIBCTX_STDOUT(mem, f) (f == mem->gs_lib_ctx->core->fstdout)
#define IS_LIBCTX_STDERR(mem, f) (f == mem->gs_lib_ctx->core->fstderr)

/* Functions to init/fin JPX decoder libctx entry */
int sjpxd_create(gs_memory_t *mem);

void sjpxd_destroy(gs_memory_t *mem);

/* Path control list functions. */
typedef enum {
    gs_permit_file_reading = 0,
    gs_permit_file_writing = 1,
    gs_permit_file_control = 2
} gs_path_control_t;

enum {
    gs_path_control_flag_is_scratch_file = 1
};

int
gs_add_control_path(const gs_memory_t *mem, gs_path_control_t type, const char *path);

int
gs_add_control_path_len(const gs_memory_t *mem, gs_path_control_t type, const char *path, size_t path_len);

int
gs_add_control_path_flags(const gs_memory_t *mem, gs_path_control_t type, const char *path, int flags);

int
gs_add_control_path_len_flags(const gs_memory_t *mem, gs_path_control_t type, const char *path, size_t path_len, int flags);

int
gs_add_outputfile_control_path(gs_memory_t *mem, const char *fname);

int
gs_remove_outputfile_control_path(gs_memory_t *mem, const char *fname);

int
gs_add_explicit_control_path(gs_memory_t *mem, const char *arg, gs_path_control_t control);

int
gs_remove_control_path(const gs_memory_t *mem, gs_path_control_t type, const char *path);

int
gs_remove_control_path_len(const gs_memory_t *mem, gs_path_control_t type, const char *path, size_t path_len);

int
gs_remove_control_path_flags(const gs_memory_t *mem, gs_path_control_t type, const char *path, int flags);

int
gs_remove_control_path_len_flags(const gs_memory_t *mem, gs_path_control_t type, const char *path, size_t path_len, int flags);

void
gs_purge_control_paths(const gs_memory_t *mem, gs_path_control_t type);

void
gs_purge_scratch_files(const gs_memory_t *mem);

void
gs_activate_path_control(gs_memory_t *mem, int enable);

int
gs_is_path_control_active(const gs_memory_t *mem);

int
gs_add_fs(const gs_memory_t *mem, gs_fs_t *fn, void *secret);

void
gs_remove_fs(const gs_memory_t *mem, gs_fs_t *fn, void *secret);

int
gs_lib_ctx_stash_sanitized_arg(gs_lib_ctx_t *ctx, const char *argv);

int
gs_lib_ctx_stash_exe(gs_lib_ctx_t *ctx, const char *argv);

int gs_lib_ctx_get_args(gs_lib_ctx_t *ctx, const char * const **argv);

#endif /* GSLIBCTX_H */
