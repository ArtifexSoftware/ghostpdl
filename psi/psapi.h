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

int
psapi_set_param(gs_lib_ctx_t *ctx,
                gs_param_list *plist);

int
psapi_set_device_param(gs_lib_ctx_t *ctx,
                       gs_param_list *plist);

int
psapi_get_device_params(gs_lib_ctx_t *ctx,
                        gs_param_list *plist);

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
