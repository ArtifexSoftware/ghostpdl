/* Copyright (C) 1995 Aladdin Enterprises.  All rights reserved.
  
  This file is part of Aladdin Ghostscript.
  
  Aladdin Ghostscript is distributed with NO WARRANTY OF ANY KIND.  No author
  or distributor accepts any responsibility for the consequences of using it,
  or for whether it serves any particular purpose or works at all, unless he
  or she says so in writing.  Refer to the Aladdin Ghostscript Free Public
  License (the "License") for full details.
  
  Every copy of Aladdin Ghostscript must include a copy of the License,
  normally in a plain ASCII text file named PUBLIC.  The License grants you
  the right to copy, modify and redistribute Aladdin Ghostscript, but only
  under certain conditions described in the License.  Among other things, the
  License requires that the copyright notice and this notice be preserved on
  all copies.
*/

/* gsalloc.h */
/* Memory allocator extensions for standard allocator */

#ifndef gsalloc_INCLUDED
#  define gsalloc_INCLUDED

/*
 * Define a structure and interface for GC-related allocator state.
 */
typedef struct gs_memory_gc_status_s {
		/* Set by client */
	long vm_threshold;		/* GC interval */
	long max_vm;			/* maximum allowed allocation */
	int *psignal;			/* if not NULL, store signal_value */
				/* here if we go over the vm_threshold */
	int signal_value;		/* value to store in *psignal */
	bool enabled;			/* auto GC enabled if true */
		/* Set by allocator */
	long requested;			/* amount of last failing request */
} gs_memory_gc_status_t;
void gs_memory_gc_status(P2(const gs_ref_memory_t *, gs_memory_gc_status_t *));
void gs_memory_set_gc_status(P2(gs_ref_memory_t *, const gs_memory_gc_status_t *));

/* ------ Internal routines ------ */

/* Initialize after a save. */
void ialloc_reset(P1(gs_ref_memory_t *));

/* Initialize after a save or GC. */
void ialloc_reset_free(P1(gs_ref_memory_t *));

/* Set the cached allocation limit of an alloctor from its GC parameters. */
void ialloc_set_limit(P1(gs_ref_memory_t *));

#endif					/* gsalloc_INCLUDED */
