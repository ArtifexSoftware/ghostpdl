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


/* chunk consolidating wrapper on a base memory allocator */

#ifndef gsmchunk_INCLUDED
#  define gsmchunk_INCLUDED

#include "std.h"

#define CHUNK_SIZE		65536

/* ---------- Public constructors/destructors ---------- */

/* Initialize a gs_memory_chunk_t */
        /* -ve error code or 0 */
int gs_memory_chunk_wrap(gs_memory_t **wrapped,	/* chunk allocator init */
                      gs_memory_t * target );	/* base allocator */

/* Release a chunk memory manager and all of the memory it held */
void gs_memory_chunk_release(gs_memory_t *cmem);

/* Release chunk memory manager, and return the target */
/* if "mem" is not a chunk memory manager instance, "mem"
 * is return untouched
 */
gs_memory_t * /* Always succeeds */
gs_memory_chunk_unwrap(gs_memory_t *mem);

/* ---------- Accessors ------------- */

/* Retrieve this allocator's target */
gs_memory_t *gs_memory_chunk_target(const gs_memory_t *cmem);

#ifdef DEBUG
    void gs_memory_chunk_dump_memory(const gs_memory_t *mem);
#endif /* DEBUG */

#endif /* gsmchunk_INCLUDED */
