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

#ifdef PACIFY_VALGRIND
#include <valgrind.h>
#endif

/* ------------- Platform de/init --------- */
void
pl_platform_init(FILE * debug_out)
{
    gp_init();
    /* debug flags we reset this out of gs_lib_init0 which sets these
       and the allocator we want the debug setting but we do our own
       allocator */
#ifdef PACIFY_VALGRIND
    VALGRIND_HG_DISABLE_CHECKING(gs_debug, 128);
#endif
    memset(gs_debug, 0, 128);
    gs_log_errors = 0;
}

void
pl_platform_dnit(int exit_status)
{
    /* Do platform-specific cleanup. */
    gp_exit(exit_status, 0);
}
