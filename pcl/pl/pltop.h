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


/* pltop.h */
/* API for language interpreters */


#ifndef pltop_INCLUDED
#  define pltop_INCLUDED

#include "gsgc.h"
#include "scommon.h"
#include "gsdevice.h"

/*
 * Generic interpreter data types which may be subclassed by specific interpreters
 */
typedef struct pl_interp_implementation_s pl_interp_implementation_t;   /* fwd decl */

/*
 * Implementation characteristics descriptor
 */
typedef struct pl_interp_characteristics_s
{
    const char *language;       /* generic language should correspond with
                                   HP documented PJL name */
    int (*auto_sense)(const char *string, int length);      /* routine used to detect language - returns a score: 0 is definitely not, 100 is definitely yes. */
} pl_interp_characteristics_t;

/*
 * Function to return the characteristics to the main loop.
 */
const pl_interp_characteristics_t *pl_characteristics(const
                                                      pl_interp_implementation_t *);
typedef const pl_interp_characteristics_t
    *(*pl_interp_proc_characteristics_t) (const pl_interp_implementation_t *);

/*
 * Allocate language client data.
 */
int pl_allocate_interp_instance(pl_interp_implementation_t *, gs_memory_t *);
typedef int (*pl_interp_proc_allocate_interp_instance_t) (pl_interp_implementation_t *,
                                                          gs_memory_t *);


/*
 * Get the allocator with which to allocate a device
 * NOTE: only one interpreter is permitted to return a
 * allocator.
 */
gs_memory_t *
pl_get_device_memory(pl_interp_implementation_t *);
typedef gs_memory_t * (*pl_interp_proc_get_device_memory_t) (pl_interp_implementation_t *);

/*
 * Pass some parameter/values to a language.
 */
int pl_set_param(pl_interp_implementation_t *, gs_param_list *plist);
typedef int (*pl_interp_proc_set_param_t) (pl_interp_implementation_t *,
                                           gs_param_list *plist);

/*
 * Add a path to a language's search path.
 */
int pl_add_path(pl_interp_implementation_t *impl, const char *path);
typedef int (*pl_interp_proc_add_path_t) (pl_interp_implementation_t *,
                                          const char *);

/*
 * Do any language specific init required after the args have been sent.
 */
int pl_post_args_init(pl_interp_implementation_t *);
typedef int (*pl_interp_proc_post_args_init_t) (pl_interp_implementation_t *);

/*
 * Work to be done when a job begins.
 */
int pl_init_job(pl_interp_implementation_t *, gx_device *);
typedef int (*pl_interp_proc_init_job_t) (pl_interp_implementation_t *, gx_device *);

/*
 * Run prefix commands before an encapsulated job.
 */
int pl_run_prefix_commands(pl_interp_implementation_t *, const char *prefix);
typedef int (*pl_interp_proc_run_prefix_commands_t) (pl_interp_implementation_t *, const char *prefix);

/*
 * Process a stream of PDL data.
 */
int pl_process_begin(pl_interp_implementation_t *);
typedef int (*pl_interp_proc_process_begin_t) (pl_interp_implementation_t *);

int pl_process(pl_interp_implementation_t *, stream_cursor_read *);
typedef int (*pl_interp_proc_process_t) (pl_interp_implementation_t *,
                                         stream_cursor_read *);

int pl_process_end(pl_interp_implementation_t *);
typedef int (*pl_interp_proc_process_end_t) (pl_interp_implementation_t *);

/*
 * The process_file function is an optional optimized path for
 * languages that want to use a random access file. If this function
 * is called for a job, pl_process, pl_flush_to_eoj and
 * pl_process_eof are not called.
 */
int pl_process_file(pl_interp_implementation_t *, const char *);
typedef int (*pl_interp_proc_process_file_t) (pl_interp_implementation_t *, const char *);

/*
 * Process and ignore all data until an end of job delimiter is
 * reached or end of data.
 */
int pl_flush_to_eoj(pl_interp_implementation_t *, stream_cursor_read *);
typedef int (*pl_interp_proc_flush_to_eoj_t) (pl_interp_implementation_t *,
                                              stream_cursor_read *);

/*
 * Actions to be be performed upon end of file.
 */
int pl_process_eof(pl_interp_implementation_t *);
typedef int (*pl_interp_proc_process_eof_t) (pl_interp_implementation_t *);

/*
 * Perform any error reporting.
 */
int pl_report_errors(pl_interp_implementation_t *, int, long, bool);
typedef int (*pl_interp_proc_report_errors_t) (pl_interp_implementation_t *, int,
                                               long, bool);

/*
 * Actions associated with ending a job
 */
int pl_dnit_job(pl_interp_implementation_t *);
typedef int (*pl_interp_proc_dnit_job_t) (pl_interp_implementation_t *);

/*
 * Free everything.
 */
int pl_deallocate_interp_instance(pl_interp_implementation_t *);
typedef int (*pl_interp_proc_deallocate_interp_instance_t) (pl_interp_implementation_t *);

typedef enum {
    PL_RESET_RESOURCES = 1
} pl_interp_reset_reason;

int pl_reset(pl_interp_implementation_t *, pl_interp_reset_reason reason);
typedef int (*pl_interp_proc_reset_t)(pl_interp_implementation_t *, pl_interp_reset_reason);

/*
 * Define a generic interpreter implementation
 */
struct pl_interp_implementation_s
{
    /* Procedure vector */
    pl_interp_proc_characteristics_t proc_characteristics;
    pl_interp_proc_allocate_interp_instance_t proc_allocate_interp_instance;
    pl_interp_proc_get_device_memory_t proc_get_device_memory;
    pl_interp_proc_set_param_t proc_set_param;
    pl_interp_proc_add_path_t proc_add_path;
    pl_interp_proc_post_args_init_t proc_post_args_init;
    pl_interp_proc_init_job_t proc_init_job;
    pl_interp_proc_run_prefix_commands_t proc_run_prefix_commands;
    pl_interp_proc_process_file_t proc_process_file;
    pl_interp_proc_process_begin_t proc_process_begin;
    pl_interp_proc_process_t proc_process;
    pl_interp_proc_process_end_t proc_process_end;
    pl_interp_proc_flush_to_eoj_t proc_flush_to_eoj;
    pl_interp_proc_process_eof_t proc_process_eof;
    pl_interp_proc_report_errors_t proc_report_errors;
    pl_interp_proc_dnit_job_t proc_dnit_job;
    pl_interp_proc_deallocate_interp_instance_t
        proc_deallocate_interp_instance;
    pl_interp_proc_reset_t reset;
    void *interp_client_data;
};

#endif /* pltop_INCLUDED */
