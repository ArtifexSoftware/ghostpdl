/* Copyright (C) 2001-2012 Artifex Software, Inc.
   All Rights Reserved.

   This software is provided AS-IS with no warranty, either express or
   implied.

   This software is distributed under license and may not be copied,
   modified or distributed except as expressly authorized under the terms
   of the license contained in the file LICENSE in this distribution.

   Refer to licensing information at http://www.artifex.com or contact
   Artifex Software, Inc.,  7 Mt. Lassen Drive - Suite A-134, San Rafael,
   CA  94903, U.S.A., +1(415)492-9861, for further information.
*/


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
