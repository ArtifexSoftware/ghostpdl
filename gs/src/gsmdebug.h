/* Copyright (C) 1995, 1996, 1998 Aladdin Enterprises.  All rights reserved.

   This software is licensed to a single customer by Artifex Software Inc.
   under the terms of a specific OEM agreement.
 */

/*$RCSfile$ $Revision$ */
/* Allocator debugging definitions and interface */
/* Requires gdebug.h (for gs_debug) */

#ifndef gsmdebug_INCLUDED
#  define gsmdebug_INCLUDED

/* Define the fill patterns used for debugging the allocator. */
extern byte
       gs_alloc_fill_alloc,	/* allocated but not initialized */
       gs_alloc_fill_block,	/* locally allocated block */
       gs_alloc_fill_collected,	/* garbage collected */
       gs_alloc_fill_deleted,	/* locally deleted block */
       gs_alloc_fill_free;	/* freed */

/* Define an alias for a specialized debugging flag */
/* that used to be a separate variable. */
#define gs_alloc_debug gs_debug['@']

/* Conditionally fill unoccupied blocks with a pattern. */
extern void gs_alloc_memset(P3(void *, int /*byte */ , ulong));

#ifdef DEBUG
#  define gs_alloc_fill(ptr, fill, len)\
     BEGIN if ( gs_alloc_debug ) gs_alloc_memset(ptr, fill, (ulong)(len)); END
#else
#  define gs_alloc_fill(ptr, fill, len)\
     DO_NOTHING
#endif

#endif /* gsmdebug_INCLUDED */
