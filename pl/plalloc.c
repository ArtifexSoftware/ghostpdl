/* subclass the allocater for pcl and xl.  Redefine methods to use the
   standard C allocator.  For a number of reasons the default
   allocator is not necessary for pcl or pxl.  Neither language
   supports garbage collection nor do they use relocation.  There is
   no necessary distinction between system, global or local vm.
   Hence a simple allocator based on malloc, realloc and free. */
/*$Id$*/

#include "std.h"
#include "gsmalloc.h"
#include "gsalloc.h"
#include "plalloc.h"

gs_memory_t *
pl_alloc_init()
{
    gs_memory_t *mem = gs_malloc_init();
    gs_memory_t *pl_mem;

    if (mem == NULL) return NULL;

    /* fix me... the second parameter (chunk size) should be a member of
       pl_main_instance_t */
    pl_mem = (gs_memory_t *)ialloc_alloc_state(mem, 20000);
    /* if ialloc fails we return NULL here */
    return pl_mem;
}
