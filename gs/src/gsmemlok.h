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
/* Interface to monitor-locked heap memory allocator */

/* Initial version 2/1/98 by John Desrosiers (soho@crl.com) */

#if !defined(gsmemlok_INCLUDED)
#  define gsmemlok_INCLUDED

#include "gsmemory.h"
#include "gxsync.h"

/*
 * This allocator encapsulates another allocator with a mutex.
 * Note that it does not keep track of memory that it acquires:
 * thus free_all with FREE_ALL_DATA is a no-op.
 */

typedef struct gs_memory_locked_s {
    gs_memory_common;		/* interface outside world sees */
    gs_memory_t *target;	/* allocator to front */
    gx_monitor_t *monitor;	/* monitor to serialize access to functions */
} gs_memory_locked_t;

/* ---------- Public constructors/destructors ---------- */

/* Initialize a locked memory manager. */
int gs_memory_locked_init(P2(
			     gs_memory_locked_t * lmem,	/* allocator to init */
			     gs_memory_t * target	/* allocator to monitor lock */
			     ));

/* Release a locked memory manager. */
/* Note that this has no effect on the target. */
void gs_memory_locked_release(P1(gs_memory_locked_t *lmem));

/* Get the target of a locked memory manager. */
gs_memory_t * gs_memory_locked_target(P1(const gs_memory_locked_t *lmem));

#endif /*!defined(gsmemlok_INCLUDED) */
