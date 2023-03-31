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


/* This routine abstracts the platform specific ways to get some
 * global state shared between different instances of the gs libary.
 * There is special effort required to ensure that multiple instances
 * do not have race conditions in the setup of such global state. Not
 * all platforms support this, so on such platforms, no global state
 * is supported. */
#include "std.h"
#include "gslibctx.h"

/* Interface to platform-specific global variable routines. */

#ifndef gs_globals_INCLUDED
#  define gs_globals_INCLUDED

struct gs_globals
{
	int non_threadsafe_count;
};

void gs_globals_init(gs_globals *globals);

void gp_global_lock(gs_globals *globals);
void gp_global_unlock(gs_globals *globals);

#endif /* gs_globals_INCLUDED */
