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


/* Definition of interpreter instance */
/* Requires stdio_.h, gsmemory.h, iref.h, iapi.h */

#ifndef iminst_INCLUDED
#  define iminst_INCLUDED

#include "iref.h"
#include "imain.h"

#ifndef display_callback_DEFINED
# define display_callback_DEFINED
typedef struct display_callback_s display_callback;
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
 *      first_is_current - 1 if we have inserted "gp_current_directory"
 * as the first entry.
 *      count - the number of elements in the list, excluding a possible
 * initial '.', env, and final.
 */
typedef struct gs_file_path_s {
    ref container;
    ref list;
    const char *env;
    const char *final;
    uint first_is_current;
    uint count;
} gs_file_path;

/*
 * Here is where we actually define the structure of interpreter instances.
 * Clients should not reference any of the members.  Note that in order to
 * be able to initialize this structure statically, members including
 * unions must come last (and be initialized to 0 by default).
 */
struct gs_main_instance_s {
    /* The following are set during initialization. */
    gs_memory_t *heap;		/* (C) heap allocator */
    uint memory_clump_size;	/* 'wholesale' allocation unit */
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
    ref error_object;		/* Use by gsapi_*() */
    int (*get_codepoint)(gp_file *file, const char **astr);
                                /* Get next 'unicode' codepoint */
    display_callback *display;	/* callback structure for display device */
    /* The following are updated dynamically. */
    i_ctx_t *i_ctx_p;		/* current interpreter context state */
    char *saved_pages_initial_arg;	/* used to defer processing of --saved-pages=begin... */
    bool saved_pages_test_mode;	/* for regression testing of saved-pages */
};

/*
 * Note that any file that uses the following definition of default values
 * must include gconfig.h, because of SEARCH_HERE_FIRST.
 */
#define gs_main_instance_default_init_values\
  0/* heap */, 20000/* clump_size */, 0/* name_table_size */, 0/* run_buffer_size */,\
  -1/* init_done */, 0/* user_errors */, SEARCH_HERE_FIRST/* duh */, 1/* run_start */,

extern const gs_main_instance gs_main_instance_init_values;

#endif /* iminst_INCLUDED */
