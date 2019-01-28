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
 * Microsoft Windows platform polling support for Ghostscript.
 *
 */

#include "gx.h"
#include "gp.h"
#include "gpcheck.h"

/* ------ Process message loop ------ */
/*
 * Check messages and interrupts; return true if interrupted.
 * This is called frequently - it must be quick!
 */
#ifdef CHECK_INTERRUPTS
int
gp_check_interrupts(const gs_memory_t *mem)
{
#ifndef GS_THREADSAFE
    if(mem == NULL) {
        /* MAJOR HACK will NOT work in multithreaded environment */
        mem = gs_lib_ctx_get_non_gc_memory_t();
    }
#endif
    if (mem && mem->gs_lib_ctx && mem->gs_lib_ctx->core->poll_fn)
        return (*mem->gs_lib_ctx->core->poll_fn)(mem->gs_lib_ctx->core->caller_handle);
    return 0;
}
#endif
