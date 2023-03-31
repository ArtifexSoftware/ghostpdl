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


/* Prototypes for debugging procedures in idebug.c */

#ifndef idebug_INCLUDED
#  define idebug_INCLUDED

#include "std.h"
#include "iref.h"

/* Print individual values. */
void debug_print_name(const gs_memory_t *mem, const ref *);
void debug_print_name_index(const gs_memory_t *mem, uint /*name_index_t*/);
void debug_print_ref(const gs_memory_t *mem, const ref *);
void debug_print_ref_packed(const gs_memory_t *mem, const ref_packed *);

/* Dump regions of memory. */
void debug_dump_one_ref(const gs_memory_t *mem, const ref *);
void debug_dump_refs(const gs_memory_t *mem,
                     const ref * from, uint size, const char *msg);
void debug_dump_array(const gs_memory_t *mem, const ref * array);

/* Dump a stack.  Using this requires istack.h. */
void debug_dump_stack(const gs_memory_t *mem,
                      const ref_stack_t * pstack, const char *msg);

#endif /* idebug_INCLUDED */
