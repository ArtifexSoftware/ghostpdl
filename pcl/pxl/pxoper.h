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


/* pxoper.h */
/* Definitions for PCL XL operators */

#ifndef pxoper_INCLUDED
#  define pxoper_INCLUDED

#include "gserrors.h"
#include "pxattr.h"
#include "pxerrors.h"
#include "pxvalue.h"

#ifndef px_state_DEFINED
#  define px_state_DEFINED
typedef struct px_state_s px_state_t;
#endif

#ifndef px_parser_state_t_DEFINED
#  define px_parser_state_t_DEFINED
typedef struct px_parser_state_s px_parser_state_t;
#endif

/*
 * The first argument of an operator procedure is a px_args_t.  Two kinds
 * of arguments require (potentially) special treatment: arrays, and data
 * read from the data source.
 *
 * By default, the storage for an array argument is released after the
 * operator is called; the operator does not know or care whether the
 * storage was allocated on the heap or somewhere else.  However, if the
 * operator wants the storage to persist, it should examine the pxd_on_heap
 * flag in the array value.  If this flag is set, the storage is already
 * heap-allocated; the operator should simply clear the flag to prevent the
 * storage from being released.  If pxd_on_heap is not set, the operator
 * should allocate storage for the array on the heap and then copy the
 * contents to the heap storage.
 *
 * If an operator needs data from the data source, it should check the
 * source.available member of the argument record.  If at least as much data
 * is available as the operator needs, the operator should read the data it
 * needs, update source.data and source.available (and, if it wishes,
 * source.position) accordingly, and return 0 as usual.  If not enough data
 * is available, the operator should read as much as it wants (which may not
 * be all of what is available) and return the special value pxNeedData.
 *
 * The variables source.position and source.count are provided solely so
 * that simple data-reading operators don't need to allocate a separate
 * state record.  The scanner itself doesn't touch them, except for
 * initializing source.position to 0 before invoking the operator the first
 * time.
 *
 * We provide parser_macro_state and parser_operator_count so that we can
 * implement ExecStream by simple recursion.
 */

/* ---------------- Definitions ---------------- */

/*
 * Define the structure for operator arguments.  This structure is never
 * allocated separately (only embedded in a px_parser_state_t) or referenced
 * persistently, and all its non-transient pointers point into the parser
 * state.  Consequently, we don't need a marking procedure for it, but we do
 * need to relocate those pointers.  We handle this within
 * px_parser_state_reloc_ptrs.
 */
#define max_px_args 20
typedef struct px_args_s
{
    struct ds_
    {
        ulong position;         /* position in data block, initially 0, */
        /* provided for the operator's convenience */
        uint count;             /* another variable for the operators */
        uint available;         /* amount of data available in block */
        bool phase;             /* upon first call of the operator this will be 0. */
        const byte *data;
    } source;
    /*
     * ExecStream needs a pointer to the parser state so it can set the
     * parser's macro-state after returning, and to report the stream's
     * operator count in case of an error.
     */
    px_parser_state_t *parser;
    px_value_t *pv[max_px_args];
} px_args_t;

/*
 * Define the value that an operator returns if it needs more data from the
 * data source.
 */
#define pxNeedData 42           /* not 0 or 1, and >0 */

/*
 * contrary to the specification and common sense the pxPassThrough
 * operator does not know how much data it requires, the parser
 * requires a special flag to know it is dealing with PassThrough */
#define pxPassThrough 43

/* Define the argument list for operators. */
#define px_operator_proc(proc)\
  int proc(px_args_t *, px_state_t *)

#endif /* pxoper_INCLUDED */
