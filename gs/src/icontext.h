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
/* Externally visible context state */
/* Requires iref.h, stdio_.h */

#ifndef icontext_INCLUDED
#  define icontext_INCLUDED

#include "gsstype.h"		/* for extern_st */
#include "icstate.h"

/* Declare the GC descriptor for context states. */
extern_st(st_context_state);

/*
 * Define the procedure for resetting user parameters when switching
 * contexts. This is defined in either zusparam.c or inouparm.c.
 */
extern int set_user_params(P2(i_ctx_t *i_ctx_p, const ref * paramdict));

/* Allocate the state of a context, always in local VM. */
/* If *ppcst == 0, allocate the state object as well. */
int context_state_alloc(P4(gs_context_state_t ** ppcst,
			   const ref *psystem_dict,
			   const dict_defaults_t *dict_defaults,
			   const gs_dual_memory_t * dmem));

/* Load the state of the interpreter from a context. */
/* The argument is not const because caches may be updated. */
int context_state_load(P1(gs_context_state_t *));

/* Store the state of the interpreter into a context. */
int context_state_store(P1(gs_context_state_t *));

/* Free the contents of the state of a context, always to its local VM. */
/* Return a mask of which of its VMs, if any, we freed. */
int context_state_free(P1(gs_context_state_t *));

#endif /* icontext_INCLUDED */
