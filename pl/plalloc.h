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
/*$Id$*/

/* A simple memory allocator for use by pcl and pxl */
gs_memory_t *pl_alloc_init(void);



/* If true PL_KEEP_GLOBAL_FREE_LIST will force all memory allocations to be stored
 * in a linked list, calling mem_node_free_all_remaining() will free any remaining
 * blocks.  This can be used to force a return to zero memory usage prior to 
 * program termination.  Since this isn't free not all system will need/want the overhead
 * of searching for the block to be freed on every deallocation. 
 * 
 * To disable the feature define PL_KEEP_GLOBAL_FREE_LIST to false 
 */

#define PL_KEEP_GLOBAL_FREE_LIST true

/* free all remaining memory blocks */
void pl_mem_node_free_all_remaining(gs_memory_t *mem);
