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
/* Initialization for the imager */
#include "stdio_.h"
#include "memory_.h"
#include "gdebug.h"
#include "gscdefs.h"
#include "gsmemory.h"
#include "gsmalloc.h"
#include "gp.h"
#include "gslib.h"		/* interface definition */

/* Imported from gsmisc.c */
extern FILE *gs_debug_out;

/* Configuration information from gconfig.c. */
extern_gx_init_table();

/* Initialization to be done before anything else. */
int
gs_lib_init(FILE * debug_out)
{
    return gs_lib_init1(gs_lib_init0(debug_out));
}
gs_memory_t *
gs_lib_init0(FILE * debug_out)
{
    gs_memory_t *mem;

    gs_debug_out = debug_out;
    mem = (gs_memory_t *) gs_malloc_init();
    /* Reset debugging flags */
    memset(gs_debug, 0, 128);
    gs_log_errors = 0;
    return mem;
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
gs_lib_finit(int exit_status, int code)
{
    fflush(stderr);		/* in case of error exit */
    /* Do platform-specific cleanup. */
    gp_exit(exit_status, code);
    gs_malloc_release();
}
