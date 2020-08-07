/* Copyright (C) 2001-2020 Artifex Software, Inc.
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


/* plmain.h */
/* Interface to main program utilities for PCL interpreters */

#ifndef plmain_INCLUDED
#  define plmain_INCLUDED

#include "stdpre.h"
#include "gsmemory.h"
#include "pltop.h"
#include "gsgstate.h"

typedef struct pl_main_instance_s pl_main_instance_t;

/* initialize gs_stdin, gs_stdout, and gs_stderr.  Eventually the gs
   library should provide an interface for doing this */
void pl_main_init_standard_io(void);

/* Initialize the instance parameters. */
void pl_main_init(pl_main_instance_t * pmi, gs_memory_t * memory);

/* Allocate and initialize the first graphics state. */
int pl_main_make_gstate(pl_main_instance_t * pmi, gs_gstate ** ppgs);

/* Print memory and time usage. */
void pl_print_usage(const pl_main_instance_t * pmi, const char *msg);

/* Finish a page, possibly printing usage statistics and/or pausing. */
int pl_finish_page(pl_main_instance_t * pmi, gs_gstate * pgs,
                   int num_copies, int flush);

/* common routine to set icc parameters usually passed from the command line. */
int pl_set_icc_params(const gs_memory_t *mem, gs_gstate *pgs);

pl_main_instance_t *pl_main_alloc_instance(gs_memory_t * memory);
int pl_main_set_display_callback(pl_main_instance_t *inst, void *callback);
int pl_main_run_file(pl_main_instance_t *minst, const char *filename);
int pl_main_init_with_args(pl_main_instance_t *inst, int argc, char *argv[]);
int pl_main_delete_instance(pl_main_instance_t *minst);
int pl_main_run_string_begin(pl_main_instance_t *minst);
int pl_main_run_string_continue(pl_main_instance_t *minst, const char *str, unsigned int length);
int pl_main_run_string_end(pl_main_instance_t *minst);
int pl_to_exit(gs_memory_t *mem);

int pl_main_set_param(pl_main_instance_t *minst, const char *arg);
int pl_main_set_string_param(pl_main_instance_t *minst, const char *arg);
int pl_main_set_parsed_param(pl_main_instance_t *minst, const char *arg);
typedef enum {
    pl_spt_invalid = -1,
    pl_spt_null    = 0,   /* void * is NULL */
    pl_spt_bool    = 1,   /* For set, void * is NULL (false) or non-NULL
                           * (true). For get, void * points to an int,
                           * 0 false, 1 true. */
    pl_spt_int     = 2,   /* void * is a pointer to an int */
    pl_spt_float   = 3,   /* void * is a float * */
    pl_spt_name    = 4,   /* void * is a char * */
    pl_spt_string  = 5,   /* void * is a char * */
    pl_spt_long    = 6,   /* void * is a long * */
    pl_spt_i64     = 7,   /* void * is an int64_t * */
    pl_spt_size_t  = 8,   /* void * is a size_t * */
    pl_spt_parsed  = 9,   /* void * is a pointer to a char * to be parsed */

    /* Setting a typed param causes it to be instantly fed to to the
     * device. This can cause the device to reinitialise itself. Hence,
     * setting a sequence of typed params can cause the device to reset
     * itself several times. Accordingly, if you OR the type with
     * pl_spt_more_to_come, the param will held ready to be passed into
     * the device, and will only actually be sent when the next typed
     * param is set without this flag (or on device init). Not valid
     * for get_typed_param. */
    pl_spt_more_to_come = 1<<31
} pl_set_param_type;
/* gs_spt_parsed allows for a string such as "<< /Foo 0 /Bar true >>" or
 * "[ 1 2 3 ]" etc to be used so more complex parameters can be set. */
int pl_main_set_typed_param(pl_main_instance_t *minst, pl_set_param_type type, const char *param, const void *value);

/* Called to get a value. value points to storage of the appropriate
 * type. If value is passed as NULL on entry, then the return code is
 * the number of bytes storage required for the type. Thus to read a
 * name/string/parsed value, call once with value=NULL, then obtain
 * the storage, and call again with value=the storage to get a nul
 * terminated string. (nul terminator is included in the count - hence
 * an empty string requires 1 byte storage) */
int pl_main_get_typed_param(pl_main_instance_t *minst, pl_set_param_type type, const char *param, void *value);

int pl_main_enumerate_params(pl_main_instance_t *minst, void **iterator, const char **key, pl_set_param_type *type);

/* instance accessors */
bool pl_main_get_interpolate(const gs_memory_t *mem);
bool pl_main_get_nocache(const gs_memory_t *mem);
bool pl_main_get_page_set_on_command_line(const gs_memory_t *mem);
bool pl_main_get_res_set_on_command_line(const gs_memory_t *mem);
bool pl_main_get_high_level_device(const gs_memory_t *mem);
void pl_main_get_forced_geometry(const gs_memory_t *mem, const float **resolutions, const long **dimensions);
int pl_main_get_scanconverter(const gs_memory_t *mem);
pl_main_instance_t *pl_main_get_instance(const gs_memory_t *mem);

typedef int pl_main_get_codepoint_t(gp_file *, const char **);
void pl_main_set_arg_decode(pl_main_instance_t *minst,
                            pl_main_get_codepoint_t *get_codepoint);

/* retrieve the PJL instance so languages can query PJL. */
bool pl_main_get_pjl_from_args(const gs_memory_t *mem); /* pjl was passed on the command line */

/* retrieve the PCL instance, used by PXL for pass through mode */
char *pl_main_get_pcl_personality(const gs_memory_t *mem);

pl_interp_implementation_t *pl_main_get_pcl_instance(const gs_memory_t *mem);
pl_interp_implementation_t *pl_main_get_pjl_instance(const gs_memory_t *mem);
#endif /* plmain_INCLUDED */
