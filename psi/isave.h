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


/* Procedures for save/restore */
/* Requires imemory.h */

#ifndef isave_INCLUDED
#  define isave_INCLUDED

#include "gsgstate.h"
#include "idosave.h"

/*
 * In PLRM2, save objects are simple, not composite.  Consequently, we
 * cannot use their natural representation, namely a t_struct pointing to an
 * alloc_save_t, since we aren't willing to allocate them all in global VM
 * and rely on garbage collection to clean them up.  Instead, we assign each
 * one a unique "save ID", and store this in the alloc_save_t object.
 * Mapping the number to the object requires at most searching the local
 * save chain for the current gs_dual_memory_t, and this approach means we
 * don't have to do anything to invalidate save objects when we do a
 * restore.
 *
 * In PLRM3, Adobe did the reasonable thing and changed save objects to
 * composite.  However, this means that 'restore' must treat save objects on
 * the stack differently in LL2 vs. LL3 (yes, the Genoa LL2 and LL3 tests
 * require this!).  See zvmem.c:restore_check_stack.
 */
typedef struct alloc_save_s alloc_save_t;

/* 'Save' structure */
typedef struct vm_save_s vm_save_t;
struct vm_save_s {
    gs_gstate *gsave;          /* old graphics state */
};

/* Initialize the save machinery. */
extern void alloc_save_init(gs_dual_memory_t *);

/* Map a save ID to its save object.  Return 0 if the ID is invalid. */
alloc_save_t *alloc_find_save(const gs_dual_memory_t *, ulong);

/*
 * Save the state.  Return 0 if we can't allocate the save object,
 * otherwise return the save ID.  The second argument is a client data
 * pointer, assumed to point to an object.
 */
int alloc_save_state(gs_dual_memory_t * dmem, void *cdata, ulong *psid);

/* Get the client pointer passed to alloc_saved_state. */
void *alloc_save_client_data(const alloc_save_t *);

/* Return (the id of) the innermost externally visible save object. */
ulong alloc_save_current_id(const gs_dual_memory_t *);
alloc_save_t *alloc_save_current(const gs_dual_memory_t *);

/* Check whether a pointer refers to an object allocated since a given save. */
bool alloc_is_since_save(const void *, const alloc_save_t *);

/* Check whether a name was created since a given save. */
bool alloc_name_is_since_save(const gs_memory_t *mem, const ref *, const alloc_save_t *);
bool alloc_name_index_is_since_save(const gs_memory_t *mem, uint, const alloc_save_t *);

/*
 * Check whether any names have been created since a given save
 * that might be released by the restore.
 */
bool alloc_any_names_since_save(const alloc_save_t *);

/*
 * Do one step of restoring the state.  Return true if the argument
 * was the innermost save, in which case this is the last (or only) step.
 * Assume the caller obtained the argument by calling alloc_find_save;
 * if this is the case, the operation cannot fail.
 */
int alloc_restore_step_in(gs_dual_memory_t *, alloc_save_t *);

/*
 * Forget a save -- like committing a transaction (restore is like
 * aborting a transaction).  Assume the caller obtained the argument
 * by calling alloc_find_save.  Note that forgetting a save does not
 * require checking pointers for recency.
 */
int alloc_forget_save_in(gs_dual_memory_t *, alloc_save_t *);

/* Release all memory -- like doing a restore "past the bottom". */
int alloc_restore_all(i_ctx_t *i_ctx_p);
/* Filter save change lists. */
void alloc_save__filter_changes(gs_ref_memory_t *mem);

/* ------ Internals ------ */

/*
 * If we are in a save, we want to save the old contents if l_new is
 * not set; if we are not in a save, we never want to save old contents.
 * We can test this quickly with a single mask that is l_new if we are
 * in a save, and -1 if we are not, since type_attrs of a valid ref
 * cannot be 0; this is the test_mask in a gs_dual_memory_t.  Similarly,
 * we want to set the l_new bit in newly allocated objects iff we are in
 * a save; this is the new_mask in a gs_dual_memory_t.
 */

/* Record that we are in a save. */
void alloc_set_in_save(gs_dual_memory_t *);

/* Record that we are not in a save. */
void alloc_set_not_in_save(gs_dual_memory_t *);

/* Remove entries from font and character caches. */
int  font_restore(const alloc_save_t * save);

/* Accessor to get a memory pointer from the saved state for the
   express purpose of getting the library context. */
gs_memory_t *gs_save_any_memory(const alloc_save_t *save);

int
restore_check_save(i_ctx_t *i_ctx_p, alloc_save_t **asave);

int
dorestore(i_ctx_t *i_ctx_p, alloc_save_t *asave);

#endif /* isave_INCLUDED */
