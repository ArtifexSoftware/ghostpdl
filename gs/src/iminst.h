/* Portions Copyright (C) 2001 artofcode LLC.
   Portions Copyright (C) 1996, 2001 Artifex Software Inc.
   Portions Copyright (C) 1988, 2000 Aladdin Enterprises.
   This software is based in part on the work of the Independent JPEG Group.
   All Rights Reserved.

   This software is distributed under license and may not be copied, modified
   or distributed except as expressly authorized under the terms of that
   license.  Refer to licensing information at http://www.artifex.com/ or
   contact Artifex Software, Inc., 101 Lucas Valley Road #110,
   San Rafael, CA  94903, (415)492-9861, for further information. */

/*$RCSfile$ $Revision$ */
/* Definition of interpreter instance */
/* Requires stdio_.h, gsmemory.h, iref.h, iapi.h */

#ifndef iminst_INCLUDED
#  define iminst_INCLUDED

#ifndef gs_main_instance_DEFINED
#  define gs_main_instance_DEFINED
typedef struct gs_main_instance_s gs_main_instance;
#endif

#ifndef NO_GS_MEMORY_GLOBALS
#include "gsmemory.h"
#include "gsmalloc.h"
#endif

/*
 * Define the structure of a search path.  Currently there is only one,
 * but there might be more someday.
 *
 *      container - an array large enough to hold the specified maximum
 * number of directories.  Both the array and all the strings in it are
 * in the 'foreign' VM space.
 *      list - the initial interval of container that defines the actual
 * search list.
 *      env - the contents of an environment variable, implicitly added
 * at the end of the list; may be 0.
 *      final - the final set of directories specified in the makefile;
 * may be 0.
 *      count - the number of elements in the list, excluding a possible
 * initial '.', env, and final.
 */
typedef struct gs_file_path_s {
    ref container;
    ref list;
    const char *env;
    const char *final;
    uint count;
} gs_file_path;

/* buffer sizes for stdio */
#define STDIN_BUF_SIZE 128
#define STDOUT_BUF_SIZE 128
#define STDERR_BUF_SIZE 128

/*
 * Here is where we actually define the structure of interpreter instances.
 * Clients should not reference any of the members.  Note that in order to
 * be able to initialize this structure statically, members including
 * unions must come last (and be initialized to 0 by default).
 */
struct gs_main_instance_s {
    /* The following are set during initialization. */
    FILE *fstdin;
    FILE *fstdout;
    FILE *fstderr;
    bool stdin_is_interactive;

    /** (C) heap allocator */
    gs_memory_t *heap;		

#ifndef NO_WRAPPED_MEMORY_BIND
    /** private: unwrapped heap 
     */
    gs_malloc_memory_t *malloc_memory_default;         
#endif 

    uint memory_chunk_size;	/* 'wholesale' allocation unit */

    ulong name_table_size;
    uint run_buffer_size;
    int init_done;		/* highest init done so far */
    int user_errors;		/* define what to do with errors */
    bool search_here_first;	/* if true, make '.' first lib dir */
    bool run_start;		/* if true, run 'start' after */
				/* processing command line */
    gs_file_path lib_path;	/* library search list (GS_LIB) */
    long base_time[2];		/* starting usertime */
    void *readline_data;	/* data for gp_readline */
    char stdin_buf[STDIN_BUF_SIZE];	/* for e_NeedStdin callout */
    char stdout_buf[STDOUT_BUF_SIZE];	/* for e_NeedStdout callout */
    char stderr_buf[STDERR_BUF_SIZE];	/* for e_NeedStderr callout */
    ref error_object;		/* Use by gsapi_*() */
    void *caller_handle;	/* identifies caller of GS DLL/shared object */
    int (GSDLLCALL *stdin_fn)(void *caller_handle, char *buf, int len);
    int (GSDLLCALL *stdout_fn)(void *caller_handle, const char *str, int len);
    int (GSDLLCALL *stderr_fn)(void *caller_handle, const char *str, int len);
    int (GSDLLCALL *poll_fn)(void *caller_handle);
    display_callback *display;	/* callback structure for display device */
    /* The following are updated dynamically. */
    i_ctx_t *i_ctx_p;		/* current interpreter context state */
};

/*
 * Note that any file that uses the following definition of default values
 * must include gconfig.h, because of SEARCH_HERE_FIRST.
 */
#ifndef NO_WRAPPED_MEMORY_BIND
# define gs_main_instance_default_init_values\
  0, 0, 0, 1 /*true*/, 0, 0, 20000, 0, 0, -1, 0, SEARCH_HERE_FIRST, 1
#else 
# define gs_main_instance_default_init_values\
  0, 0, 0, 1 /*true*/, 0, 20000, 0, 0, -1, 0, SEARCH_HERE_FIRST, 1
#endif 
extern const gs_main_instance gs_main_instance_init_values;

#endif /* iminst_INCLUDED */
