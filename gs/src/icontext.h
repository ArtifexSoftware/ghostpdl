/* Copyright (C) 1997 Aladdin Enterprises.  All rights reserved.
  
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

/* icontext.h */
/* Externally visible context state */
/* Requires iref.h */

#ifndef icontext_INCLUDED
#  define icontext_INCLUDED

#include "imemory.h"
#include "istack.h"

/*
 * Define the externally visible state of an interpreter context.
 * If we aren't supporting Display PostScript features, there is only
 * a single context.
 */
typedef struct gs_context_state_s {
	/* These refs are t_struct pointing to ref_stack structures. */
	ref dstack, estack, ostack;
	gs_state *pgs;
	gs_dual_memory_t memory;
	ref array_packing;		/* t_boolean */
	ref binary_object_format;	/* t_integer */
	long rand_state;	/* (not in Red Book) */
	long usertime_total;	/* total accumulated usertime, */
				/* not counting current time if running */
	bool keep_usertime;	/* true if context ever executed usertime */
	/* View clipping is handled in the graphics state. */
	/****** User parameters ******/
	/****** %stdin, %stdout ******/
} gs_context_state_t;
/*
 * Note that in single-context systems, we never allocate a context state
 * on the heap: we only allocate one in a local variable for long enough
 * to initialize it and load the interpreter state from it.  Therefore,
 * we declare the GC type for gs_context_state_t here, but we don't
 * actually create it unless multiple contexts are supported.
 */
#define private_st_context_state()	/* in zcontext.c, not icontext.c */\
  gs_private_st_composite(st_context_state, gs_context_state_t,\
    "gs_context_state_t", context_state_enum_ptrs, context_state_reloc_ptrs)

/* Allocate the state of a context. */
/* The client is responsible for the 'memory' member. */
int context_state_alloc(P2(gs_context_state_t *, gs_ref_memory_t *));

/* Load the state of the interpreter from a context. */
void context_state_load(P1(const gs_context_state_t *));

/* Store the state of the interpreter into a context. */
void context_state_store(P1(gs_context_state_t *));

/* Free the state of a context. */
void context_state_free(P2(gs_context_state_t *, gs_ref_memory_t *));

#endif					/* icontext_INCLUDED */
