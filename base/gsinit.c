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


/* Initialization for the imager */
#include "stdio_.h"
#include "memory_.h"
#include "gdebug.h"
#include "gscdefs.h"
#include "gsmemory.h"
#include "gsmalloc.h"
#include "gp.h"
#include "gslib.h"		/* interface definition */
#include "gxfapi.h"

#include "valgrind.h"

/* Configuration information from gconfig.c. */
extern_gx_init_table();

/* Initialization to be done before anything else. */
int
gs_lib_init(gp_file * debug_out)
{
    return gs_lib_init1(gs_lib_init0(debug_out));
}
gs_memory_t *
gs_lib_init0(gp_file * debug_out)
{
    /* Reset debugging flags */
#ifdef PACIFY_VALGRIND
    VALGRIND_HG_DISABLE_CHECKING(gs_debug, 128);
#endif
    memset(gs_debug, 0, 128);
    gs_log_errors = 0;

    return (gs_memory_t *) gs_malloc_init();
}
int
gs_lib_init1(gs_memory_t * mem)
{
    /* Run configuration-specific initialization procedures. */
    init_proc((*const *ipp));
    int code;

    for (ipp = gx_init_table; *ipp != 0; ++ipp)
        if ((code = (**ipp)(mem)) < 0)
            return code;
    return 0;
}

/* Clean up after execution. */
void
gs_lib_finit(int exit_status, int code, gs_memory_t *mem)
{
    gs_fapi_finit (mem);

    /* Do platform-specific cleanup. */
    gp_exit(exit_status, code);

    /* NB: interface problem.
     * if gs_lib_init0 was called the we should
     *    gs_malloc_release(mem);
     * else
     *    someone else has control of mem so we can't free it.
     *    gs_view and iapi.h interface
     */
}
