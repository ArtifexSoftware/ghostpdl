/* Allocator for languages (pcl, xps, etc.), simply uses the chunk
   memory manager see gsmchunk.c */ 

/*$Id$*/

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
