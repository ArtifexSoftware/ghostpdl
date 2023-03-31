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


/* Internal graphics state API */

#ifndef gxstate_INCLUDED
#  define gxstate_INCLUDED

#include "gsgstate.h"
#include "gscspace.h"

/*
 * The interfaces in this file are for internal use only, primarily by the
 * interpreter.  They are not guaranteed to remain stable from one release
 * to another.
 */

/* Memory and save/restore management */
gs_memory_t *gs_gstate_memory(const gs_gstate *);
gs_gstate *gs_gstate_saved(const gs_gstate *);
gs_gstate *gs_gstate_swap_saved(gs_gstate *, gs_gstate *);
gs_memory_t *gs_gstate_swap_memory(gs_gstate *, gs_memory_t *);

/*
 * "Client data" interface for graphics states.
 *
 * As of release 4.36, the copy procedure is superseded by copy_for
 * (although it will still be called if there is no copy_for procedure).
 */
typedef void *(*gs_gstate_alloc_proc_t) (gs_memory_t * mem);
typedef int (*gs_gstate_copy_proc_t) (void *to, const void *from);
typedef void (*gs_gstate_free_proc_t) (void *old, gs_memory_t * mem, gs_gstate *pgs);

typedef enum {
    copy_for_gsave,		/* from = current, to = new(saved) */
    copy_for_grestore,		/* from = saved, to = current */
    copy_for_gstate,		/* from = current, to = new(copy) */
    copy_for_setgstate,		/* from = stored, to = current */
    copy_for_copygstate,	/* from & to are specified explicitly */
    copy_for_currentgstate	/* from = current, to = stored */
} gs_gstate_copy_reason_t;

/* Note that the 'from' argument of copy_for is not const. */
/* This is deliberate -- some clients need this. */
typedef int (*gs_gstate_copy_for_proc_t) (void *to, void *from,
                                         gs_gstate_copy_reason_t reason);
typedef struct gs_gstate_client_procs_s {
    gs_gstate_alloc_proc_t alloc;
    gs_gstate_copy_proc_t copy;
    gs_gstate_free_proc_t free;
    gs_gstate_copy_for_proc_t copy_for;
} gs_gstate_client_procs;
void gs_gstate_set_client(gs_gstate *, void *, const gs_gstate_client_procs *,
                            bool client_has_pattern_streams);

/* gzstate.h redefines the following: */
#ifndef gs_gstate_client_data
void *gs_gstate_client_data(const gs_gstate *);
#endif

/* Accessories. */
gs_id gx_get_clip_path_id(gs_gstate *);

#endif /* gxstate_INCLUDED */
