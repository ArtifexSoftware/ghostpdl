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
/* Dictionary stack API subset needed by idict.h */

#ifndef iddstack_INCLUDED
#  define iddstack_INCLUDED

#ifndef dict_stack_DEFINED
#  define dict_stack_DEFINED
typedef struct dict_stack_s dict_stack_t;
#endif

/*
 * Reset the cached top values.  Every routine that alters the
 * dictionary stack (including changing the protection or size of the
 * top dictionary on the stack) must call this.
 */
void dstack_set_top(dict_stack_t *);

/* Check whether a dictionary is one of the permanent ones on the d-stack. */
bool dstack_dict_is_permanent(const dict_stack_t *, const ref *);

#endif /* iddstack_INCLUDED */
