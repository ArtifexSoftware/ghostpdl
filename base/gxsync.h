/* Copyright (C) 2001-2024 Artifex Software, Inc.
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
    gx_semaphore_alloc(
                       gs_memory_t * memory	/* memory allocator to use */
                       );
void
    gx_semaphore_free(
                      gx_semaphore_t * sema	/* semaphore to delete */
                      );

gx_semaphore_t *gx_semaphore_label(gx_semaphore_t *mon, const char *name);
#ifndef BOBBIN
#define gx_semaphore_label(A,B) (A)
#endif

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
    gx_monitor_alloc(
                     gs_memory_t * memory	/* memory allocator to use */
                     );
void
    gx_monitor_free(
                    gx_monitor_t * mon	/* monitor to delete */
                    );

gx_monitor_t *gx_monitor_label(gx_monitor_t *mon, const char *name);
#ifndef BOBBIN
#define gx_monitor_label(A,B) (A)
#endif

#define gx_monitor_enter(mon)  gp_monitor_enter(&(mon)->native)
#define gx_monitor_leave(mon)  gp_monitor_leave(&(mon)->native)

#endif /* !defined(gxsync_INCLUDED) */
