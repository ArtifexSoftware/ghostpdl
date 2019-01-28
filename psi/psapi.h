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



/*
 * Internal API for Ghostscript interpreter.
 */

#ifndef psapi_INCLUDED
#  define psapi_INCLUDED

#include "gsmemory.h"
#include "gsdevice.h"

int
psapi_set_arg_encoding(gs_lib_ctx_t *instance,
                       int           encoding);

/* Must match iapi.h and plapi.h */
enum {
    PS_ARG_ENCODING_LOCAL = 0,
    PS_ARG_ENCODING_UTF8 = 1,
    PS_ARG_ENCODING_UTF16LE = 2
};

int
psapi_new_instance(gs_lib_ctx_t **pinstance,
                   void          *caller_handle);

void
psapi_delete_instance(gs_lib_ctx_t *instance);

void
psapi_act_on_uel(gs_lib_ctx_t *instance);

int
psapi_init_with_args(gs_lib_ctx_t  *instance,
                     int            argc,
                     char         **argv);
int
psapi_init_with_args01(gs_lib_ctx_t  *instance,
                      int            argc,
                      char         **argv);
int
psapi_init_with_args2(gs_lib_ctx_t  *instance);

typedef enum {
    psapi_spt_invalid = -1,
    psapi_spt_null    = 0,   /* void * is NULL */
    psapi_spt_bool    = 1,   /* void * is NULL (false) or non-NULL (true) */
    psapi_spt_int     = 2,   /* void * is a pointer to an int */
    psapi_spt_float   = 3,   /* void * is a float * */
    psapi_spt_name    = 4,   /* void * is a char * */
    psapi_spt_string  = 5    /* void * is a char * */
} psapi_sptype;
int
psapi_set_param(gs_lib_ctx_t *ctx,
                psapi_sptype  type,
                const char   *param,
                const void   *val);

int
psapi_add_path(gs_lib_ctx_t *ctx,
               const char   *path);

int
psapi_run_string_begin(gs_lib_ctx_t *instance,
                       int           user_errors,
                       int          *pexit_code);

int
psapi_run_string_continue(gs_lib_ctx_t   *instance,
                          const char     *str,
                          unsigned int    length,
                          int           user_errors,
                          int          *pexit_code);

unsigned int
psapi_get_uel_offset(gs_lib_ctx_t *instance);

int
psapi_run_string_end(gs_lib_ctx_t *instance,
                     int           user_errors,
                     int          *pexit_code);

int
psapi_run_string_with_length(gs_lib_ctx_t *instance,
                             const char   *str,
                             unsigned int  length,
                             int           user_errors,
                             int          *pexit_code);

int
psapi_run_string(gs_lib_ctx_t *ctx,
                 const char   *str,
                 int           user_errors,
                 int          *pexit_code);

int
psapi_run_file(gs_lib_ctx_t *instance,
               const char   *file_name,
               int           user_errors,
               int          *pexit_code);

gs_memory_t *
psapi_get_device_memory(gs_lib_ctx_t *instance);

int
psapi_set_device(gs_lib_ctx_t *instance,
                 gx_device  *pdev);

int
psapi_exit(gs_lib_ctx_t *instance);

int
psapi_force_geometry(gs_lib_ctx_t *ctx, const float *resolutions, const long *dimensions);

#endif /* psapi_INCLUDED */
