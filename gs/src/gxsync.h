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
/* Interface to synchronization primitives */

/* Initial version 2/1/98 by John Desrosiers (soho@crl.com) */

#if !defined(gxsync_INCLUDED)
#  define gxsync_INCLUDED

#include "gpsync.h"
#include "gsmemory.h"

/* This module abstracts the platform-specific synchronization primitives. */
/* Since these routines will see heavy use, performance is important. */

/* ----- Semaphore interface ----- */
/* These have the usual queued, counting semaphore semantics: at init time, */
/* the event count is set to 0 ('wait' will wait until 1st signal). */
typedef struct gx_semaphore_s {
    gs_memory_t *memory;	/* allocator to free memory */
    gp_semaphore native;	/* MUST BE LAST last since length is undef'd */
    /*  platform-dep impl, len is gp_semaphore_sizeof() */
} gx_semaphore_t;

gx_semaphore_t *		/* returns a new semaphore, 0 if error */
    gx_semaphore_alloc(P1(
			  gs_memory_t * memory	/* memory allocator to use */
			  ));
void
    gx_semaphore_free(P1(
			 gx_semaphore_t * sema	/* semaphore to delete */
			 ));

#define gx_semaphore_wait(sema)  gp_semaphore_wait(&(sema)->native)
#define gx_semaphore_signal(sema)  gp_semaphore_signal(&(sema)->native)


/* ----- Monitor interface ----- */
/* These have the usual monitor semantics: at init time, */
/* the event count is set to 1 (1st 'enter' succeeds immediately). */
typedef struct gx_monitor_s {
    gs_memory_t *memory;	/* allocator to free memory */
    gp_monitor native;		/* platform-dep impl, len is gp_monitor_sizeof() */
} gx_monitor_t;

gx_monitor_t *			/* returns a new monitor, 0 if error */
    gx_monitor_alloc(P1(
			gs_memory_t * memory	/* memory allocator to use */
			));
void
    gx_monitor_free(P1(
		       gx_monitor_t * mon	/* monitor to delete */
		       ));

#define gx_monitor_enter(sema)  gp_monitor_enter(&(sema)->native)
#define gx_monitor_leave(sema)  gp_monitor_leave(&(sema)->native)

#endif /* !defined(gxsync_INCLUDED) */
