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
/* Generic dictionary stack API */

#ifndef idstack_INCLUDED
#  define idstack_INCLUDED

#include "iddstack.h"
#include "idsdata.h"
#include "istack.h"

/* Define the type of pointers into the dictionary stack. */
typedef s_ptr ds_ptr;
typedef const_s_ptr const_ds_ptr;

/* Clean up a dictionary stack after a garbage collection. */
void dstack_gc_cleanup(P1(dict_stack_t *));

/*
 * Define a special fast entry for name lookup on a dictionary stack.
 * The key is known to be a name; search the entire dict stack.
 * Return the pointer to the value slot.
 * If the name isn't found, just return 0.
 */
ref *dstack_find_name_by_index(P2(dict_stack_t *, uint));

/*
 * Define an extra-fast macro for name lookup, optimized for
 * a single-probe lookup in the top dictionary on the stack.
 * Amazingly enough, this seems to hit over 90% of the time
 * (aside from operators, of course, which are handled either with
 * the special cache pointer or with 'bind').
 */
#define dstack_find_name_by_index_inline(pds,nidx,htemp)\
  ((pds)->top_keys[htemp = dict_hash_mod_inline(dict_name_index_hash(nidx),\
     (pds)->top_npairs) + 1] == pt_tag(pt_literal_name) + (nidx) ?\
   (pds)->top_values + htemp : dstack_find_name_by_index(pds, nidx))
/*
 * Define a similar macro that only checks the top dictionary on the stack.
 */
#define if_dstack_find_name_by_index_top(pds,nidx,htemp,pvslot)\
  if ( (((pds)->top_keys[htemp = dict_hash_mod_inline(dict_name_index_hash(nidx),\
	 (pds)->top_npairs) + 1] == pt_tag(pt_literal_name) + (nidx)) ?\
	((pvslot) = (pds)->top_values + (htemp), 1) :\
	0)\
     )

#endif /* idstack_INCLUDED */
