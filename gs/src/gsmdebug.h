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
/* Allocator debugging definitions and interface */
/* Requires gdebug.h (for gs_debug) */

#ifndef gsmdebug_INCLUDED
#  define gsmdebug_INCLUDED

/* Define the fill patterns used for debugging the allocator. */
extern const byte
       gs_alloc_fill_alloc,	/* allocated but not initialized */
       gs_alloc_fill_block,	/* locally allocated block */
       gs_alloc_fill_collected,	/* garbage collected */
       gs_alloc_fill_deleted,	/* locally deleted block */
       gs_alloc_fill_free;	/* freed */

/* Define an alias for a specialized debugging flag */
/* that used to be a separate variable. */
#define gs_alloc_debug gs_debug['@']

/* Conditionally fill unoccupied blocks with a pattern. */
extern void gs_alloc_memset(void *, int /*byte */ , ulong);

#ifdef DEBUG
#  define gs_alloc_fill(ptr, fill, len)\
     BEGIN if ( gs_alloc_debug ) gs_alloc_memset(ptr, fill, (ulong)(len)); END
#else
#  define gs_alloc_fill(ptr, fill, len)\
     DO_NOTHING
#endif

#endif /* gsmdebug_INCLUDED */
