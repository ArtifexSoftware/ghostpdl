/* Copyright (C) 1989, 1992, 1993, 1994, 1996, 1997, 1998 Aladdin Enterprises.  All rights reserved.

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

/*$Id$ */
/* Generic execution stack API */

#ifndef iestack_INCLUDED
#  define iestack_INCLUDED

#include "istack.h"

/* Define the execution stack structure. */
typedef struct exec_stack_s {

    ref_stack stack;		/* the actual execution stack */

/*
 * To improve performance, we cache the currentfile pointer
 * (i.e., `shallow-bind' it in Lisp terminology).  The invariant is as
 * follows: either esfile points to the currentfile slot on the estack
 * (i.e., the topmost slot with an executable file), or it is 0.
 * To maintain the invariant, it is sufficient that whenever a routine
 * pushes or pops anything on the estack, if the object *might* be
 * an executable file, invoke esfile_clear_cache(); alternatively,
 * immediately after pushing an object, invoke esfile_check_cache().
 */
    ref *current_file;

} exec_stack_t;

/* Define pointers into the execution stack. */
typedef s_ptr es_ptr;
typedef const_s_ptr const_es_ptr;

#endif /* iestack_INCLUDED */
