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
/* Interpreter's name table interface */

#ifndef iname_INCLUDED
#  define iname_INCLUDED

#include "inames.h"

/*
 * This file defines those parts of the name table API that refer to the
 * interpreter's distinguished instance.  Procedures in this file begin
 * with name_.
 */

/* ---------------- Procedural interface ---------------- */


/* Get the allocator for the name table. */
#define name_memory(mem)\
  names_memory(mem->gs_lib_ctx->gs_name_table)

/*
 * Look up and/or enter a name in the name table.
 * See inames.h for the values of enterflag, and the possible return values.
 */
#define name_ref(mem, ptr, size, pnref, enterflag)\
  names_ref(mem->gs_lib_ctx->gs_name_table, ptr, size, pnref, enterflag)
#define name_string_ref(mem, pnref, psref)\
  names_string_ref(mem->gs_lib_ctx->gs_name_table, pnref, psref)
/*
 * name_enter_string calls name_ref with a (permanent) C string.
 */
#define name_enter_string(mem, str, pnref)\
  names_enter_string(mem->gs_lib_ctx->gs_name_table, str, pnref)
/*
 * name_from_string essentially implements cvn.
 * It always enters the name, and copies the executable attribute.
 */
#define name_from_string(mem, psref, pnref)\
  names_from_string(mem->gs_lib_ctx->gs_name_table, psref, pnref)

/* Compare two names for equality. */
#define name_eq(pnref1, pnref2)\
  names_eq(pnref1, pnref2)

/* Invalidate the value cache for a name. */
#define name_invalidate_value_cache(mem, pnref)\
  names_invalidate_value_cache(mem->gs_lib_ctx->gs_name_table, pnref)

/* Convert between names and indices. */
#define name_index(mem, pnref)		/* ref => index */\
  names_index(mem->gs_lib_ctx->gs_name_table, pnref)
#define name_index_ptr(mem, nidx)		/* index => name */\
  names_index_ptr(mem->gs_lib_ctx->gs_name_table, nidx)
#define name_index_ref(mem, nidx, pnref)	/* index => ref */\
  names_index_ref(mem->gs_lib_ctx->gs_name_table, nidx, pnref)

/* Get the index of the next valid name. */
/* The argument is 0 or a valid index. */
/* Return 0 if there are no more. */
#define name_next_valid_index(mem, nidx)\
  names_next_valid_index(mem->gs_lib_ctx->gs_name_table, nidx)

/* Mark a name for the garbage collector. */
/* Return true if this is a new mark. */
#define name_mark_index(mem, nidx)\
  names_mark_index(mem->gs_lib_ctx->gs_name_table, nidx)

/* Get the object (sub-table) containing a name. */
/* The garbage collector needs this so it can relocate pointers to names. */
#define name_ref_sub_table(mem, pnref)\
  names_ref_sub_table(mem->gs_lib_ctx->gs_name_table, pnref)

#endif /* iname_INCLUDED */
