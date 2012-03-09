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
/*$Id$ */

/* plplatf.c   Platform-related utils */

#include "string_.h"
#include "gdebug.h"
#include "gsio.h"
#include "gstypes.h"
#include "gsmemory.h"
#include "gsstruct.h"
#include "gp.h"
#include "gslib.h"
#include "plplatf.h"

/* ------------- Platform de/init --------- */
void
pl_platform_init(FILE *debug_out)
{
    gp_init();
    /* debug flags we reset this out of gs_lib_init0 which sets these
         and the allocator we want the debug setting but we do our own
         allocator */
    memset(gs_debug, 0, 128);
    gs_log_errors = 0;
}

void
pl_platform_dnit(int exit_status)
{
    /* Do platform-specific cleanup. */
    gp_exit(exit_status, 0);
}

/* ---------------- Stubs ---------------- */

/* Stubs for GC */
const gs_ptr_procs_t ptr_struct_procs = { NULL, NULL, NULL };
const gs_ptr_procs_t ptr_string_procs = { NULL, NULL, NULL };
const gs_ptr_procs_t ptr_const_string_procs = { NULL, NULL, NULL };
void * /* obj_header_t * */
gs_reloc_struct_ptr(const void * /* obj_header_t * */ obj, gc_state_t *gcst)
{	return (void *)obj;
}
void
gs_reloc_string(gs_string *sptr, gc_state_t *gcst)
{
}
void
gs_reloc_const_string(gs_const_string *sptr, gc_state_t *gcst)
{
}
const gs_ptr_procs_t ptr_name_index_procs = { NULL, NULL, NULL };

/* Stub for abnormal termination */
void
gs_exit(int exit_status)
{	pl_platform_dnit(exit_status);
        pl_exit(exit_status);	/* must be implemeted by caller */
}
