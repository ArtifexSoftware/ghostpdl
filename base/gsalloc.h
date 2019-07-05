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


/* Memory allocator extensions for standard allocator */

#ifndef gsalloc_INCLUDED
#  define gsalloc_INCLUDED

#include "std.h"

typedef struct gs_ref_memory_s gs_ref_memory_t;

/*
 * Define a structure and interface for GC-related allocator state.
 */
typedef struct gs_memory_gc_status_s {
        /* Set by client */
    size_t vm_threshold;		/* GC interval */
    size_t max_vm;		/* maximum allowed allocation */

    int signal_value;		/* value to store in gs_lib_ctx->gcsignal */
    bool enabled;		/* auto GC enabled if true */
        /* Set by allocator */
    size_t requested;		/* amount of last failing request */
} gs_memory_gc_status_t;
void gs_memory_gc_status(const gs_ref_memory_t *, gs_memory_gc_status_t *);
void gs_memory_set_gc_status(gs_ref_memory_t *, const gs_memory_gc_status_t *);
void gs_memory_set_vm_threshold(gs_ref_memory_t * mem, size_t val);
void gs_memory_set_vm_reclaim(gs_ref_memory_t * mem, bool enabled);

/* ------ Initialization ------ */

/*
 * Allocate and mostly initialize the state of an allocator (system, global,
 * or local).  Does not initialize global or space.
 */
gs_ref_memory_t *ialloc_alloc_state(gs_memory_t *, uint);

/*
 * Function to free a gs_ref_memory_t allocated by ialloc_alloc_state. ONLY
 * USEFUL FOR ERROR CLEANUP IMMEDIATELY AFTER ALLOCATION!
 */
void ialloc_free_state(gs_ref_memory_t *);

/*
 * Add a clump to an externally controlled allocator.  Such allocators
 * allocate all objects as immovable, are not garbage-collected, and
 * don't attempt to acquire additional memory (or free clumps) on their own.
 */
int ialloc_add_clump(gs_ref_memory_t *, ulong, client_name_t);

/* ------ Internal routines ------ */

/* Prepare for a GC. */
void ialloc_gc_prepare(gs_ref_memory_t *);

/* Initialize after a save. */
void ialloc_reset(gs_ref_memory_t *);

/* Initialize after a save or GC. */
void ialloc_reset_free(gs_ref_memory_t *);

/* Set the cached allocation limit of an alloctor from its GC parameters. */
void ialloc_set_limit(gs_ref_memory_t *);

/* Consolidate free objects. */
void ialloc_consolidate_free(gs_ref_memory_t *);

#endif /* gsalloc_INCLUDED */
