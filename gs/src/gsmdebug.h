/* Copyright (C) 1995, 1996 Aladdin Enterprises.  All rights reserved.
  
  This file is part of Aladdin Ghostscript.
  
  Aladdin Ghostscript is distributed with NO WARRANTY OF ANY KIND.  No author
  or distributor accepts any responsibility for the consequences of using it,
  or for whether it serves any particular purpose or works at all, unless he
  or she says so in writing.  Refer to the Aladdin Ghostscript Free Public
  License (the "License") for full details.
  
  Every copy of Aladdin Ghostscript must include a copy of the License,
  normally in a plain ASCII text file named PUBLIC.  The License grants you
  the right to copy, modify and redistribute Aladdin Ghostscript, but only
  under certain conditions described in the License.  Among other things, the
  License requires that the copyright notice and this notice be preserved on
  all copies.
*/

/* gsmdebug.h */
/* Debugging definitions for memory manager */
/* Requires gdebug.h (for gs_debug) */

/* Define the fill patterns used for debugging the allocator. */
extern byte
  gs_alloc_fill_alloc,		/* allocated but not initialized */
  gs_alloc_fill_block,		/* locally allocated block */
  gs_alloc_fill_collected,	/* garbage collected */
  gs_alloc_fill_deleted,	/* locally deleted block */
  gs_alloc_fill_free;		/* freed */

/* Define an alias for a specialized debugging flag */
/* that used to be a separate variable. */
#define gs_alloc_debug gs_debug['@']

/* Conditionally fill unoccupied blocks with a pattern. */
extern void gs_alloc_memset(P3(void *, int/*byte*/, ulong));
#ifdef DEBUG
/* The following peculiar syntax avoids incorrect capture of an 'else'. */
#  define gs_alloc_fill(ptr, fill, len)\
     do { if ( gs_alloc_debug ) gs_alloc_memset(ptr, fill, (ulong)(len));\
     } while ( 0 )
#else
#  define gs_alloc_fill(ptr, fill, len)\
     DO_NOTHING
#endif
