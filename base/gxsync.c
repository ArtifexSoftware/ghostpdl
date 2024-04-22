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


/* Interface to platform-based synchronization primitives */

/* Initial version 2/1/98 by John Desrosiers (soho@crl.com) */

#include "memory_.h"
#include "gx.h"
#include "gserrors.h"
#include "gsmemory.h"
#include "gxsync.h"

/* This module abstracts the platform-specific synchronization primitives. */
/* Since these routines will see heavy use, performance is important. */

/* ----- Semaphore interface ----- */
/* These have the usual queued, counting semaphore semantics: at init time, */
/* the event count is set to 0 ('wait' will wait until 1st signal). */

/* Allocate & initialize a semaphore */
gx_semaphore_t *		/* returns a new semaphore, 0 if error */
gx_semaphore_alloc(
                      gs_memory_t * memory	/* memory allocator to use */
)
{
    gx_semaphore_t *sema;

    /* sizeof decl'd sema struct, minus semaphore placeholder's size, + actual semaphore size */
    unsigned semaSizeof
    = sizeof(*sema) - sizeof(sema->native) + gp_semaphore_sizeof();

    if (gp_semaphore_open(0) == 0)	/* see if gp_semaphores are movable */
        /* movable */
        sema = (gx_semaphore_t *) gs_alloc_bytes(memory, semaSizeof,
                                                 "gx_semaphore (create)");
    else
        /* unmovable */
        sema = (gx_semaphore_t *) gs_alloc_bytes_immovable(memory, semaSizeof,
                                                   "gx_semaphore (create)");
    if (sema == 0)
        return 0;

    /* Make sema remember which allocator was used to allocate it */
    sema->memory = memory;

    if (gp_semaphore_open(&sema->native) < 0) {
        gs_free_object(memory, sema, "gx_semaphore (alloc)");
        return 0;
    }
    return sema;
}

/* Deinit & free a semaphore */
void
gx_semaphore_free(
                     gx_semaphore_t * sema	/* semaphore to delete */
)
{
    if (sema) {
        gp_semaphore_close(&sema->native);
        gs_free_object(sema->memory, sema, "gx_semaphore (free)");
    }
}

gx_semaphore_t *(gx_semaphore_label)(gx_semaphore_t *sema, const char *name)
{
    (void)name;
    if (sema)
        gp_semaphore_label(&sema->native, name);
    return sema;
}

/* Macros defined in gxsync.h, but redefined here so compiler chex consistency */
#define gx_semaphore_wait(sema)  gp_semaphore_wait(&(sema)->native)
#define gx_semaphore_signal(sema)  gp_semaphore_signal(&(sema)->native)

/* ----- Monitor interface ----- */
/* These have the usual monitor semantics: at init time, */
/* the event count is set to 1 (1st 'enter' succeeds immediately). */

/* Allocate & Init a monitor */
gx_monitor_t *			/* returns a new monitor, 0 if error */
gx_monitor_alloc(
                    gs_memory_t * memory	/* memory allocator to use */
)
{
    gx_monitor_t *mon;

    /* sizeof decl'd mon struct, minus monitor placeholder's size, + actual monitor size */
    unsigned monSizeof
    = sizeof(*mon) - sizeof(mon->native) + gp_monitor_sizeof();

    if (gp_monitor_open(0) == 0)	/* see if gp_monitors are movable */
        /* movable */
        mon = (gx_monitor_t *) gs_alloc_bytes(memory, monSizeof,
                                              "gx_monitor (create)");
    else
        /* unmovable */
        mon = (gx_monitor_t *) gs_alloc_bytes_immovable(memory, monSizeof,
                                                     "gx_monitor (create)");
    if (mon == 0)
        return 0;

    /* Make monitor remember which allocator was used to allocate it */
    mon->memory = memory;

    if (gp_monitor_open(&mon->native) < 0) {
        gs_free_object(memory, mon, "gx_monitor (alloc)");
        return 0;
    }
    return mon;
}

/* Dnit & free a monitor */
void
gx_monitor_free(
                   gx_monitor_t * mon	/* monitor to delete */
)
{
    if (mon) {
        gp_monitor_close(&mon->native);
        gs_free_object(mon->memory, mon, "gx_monitor (free)");
    }
}

gx_monitor_t *
(gx_monitor_label)(gx_monitor_t *mon, const char *name)
{
    (void)name;
    if (mon)
        gp_monitor_label(&mon->native, name);
    return mon;
}

/* Macros defined in gxsync.h, but redefined here so compiler chex consistency */
#define gx_monitor_enter(mon)  gp_monitor_enter(&(mon)->native)
#define gx_monitor_leave(mon)  gp_monitor_leave(&(mon)->native)
