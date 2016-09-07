/* Copyright (C) 2001-2012 Artifex Software, Inc.
   All Rights Reserved.

   This software is provided AS-IS with no warranty, either express or
   implied.

   This software is distributed under license and may not be copied,
   modified or distributed except as expressly authorized under the terms
   of the license contained in the file LICENSE in this distribution.

   Refer to licensing information at http://www.artifex.com or contact
   Artifex Software, Inc.,  7 Mt. Lassen Drive - Suite A-134, San Rafael,
   CA  94903, U.S.A., +1(415)492-9861, for further information.
*/
/* Allocator for languages (pcl, xps, etc.), simply uses the chunk
   memory manager see gsmchunk.c */


#include "std.h"
#include "gsmalloc.h"
#include "gsalloc.h"
#include "plalloc.h"
#include "gsmchunk.h"

gs_memory_t *
pl_alloc_init()
{
    gs_memory_t *mem = gs_malloc_init();
    gs_memory_t *pl_mem;
    int code;

    if (mem == NULL)
        return NULL;

#ifdef HEAP_ALLOCATOR_ONLY
    return mem;
#endif

    code = gs_memory_chunk_wrap(&pl_mem, mem);
    if (code < 0)
        return NULL;

    return pl_mem;
}

void
pl_alloc_finit(gs_memory_t *mem)
{
    gs_memory_t *tmem = gs_memory_chunk_unwrap(mem);
    gs_malloc_release(tmem);
}
