/* Copyright (C) 1998 Aladdin Enterprises.  All rights reserved.
  
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

/* gsmemfix.h Interface files for fixed-block raw memory allocator */

/* Initial version 02/03/1998 by John Desrosiers (soho@crl.com) */

#if !defined(gsmemfix__INCLUDED)
 #define gsmemfix__INCLUDED

 #include "gsmemraw.h"


struct gs_memfix_alloc_block_s;	/* opaque */

/*
 * A raw-memory allocator.  Subclasses may have state as well.
 */
#define gs_memfix_common\
	gs_memraw_common;\
	struct gs_memfix_alloc_block_s	*first;	/* first allocation block in chain */\
	long		total_universe;	/* total # bytes in universe */\
	long		total_free	/* total # bytes free */
typedef struct gs_memfix_s {
	gs_memfix_common;
} gs_memfix;

/* Constructor for gs_memfix's */
void gs_memfix_constructor(gs_memfix *);

/* Remove the most recently added raw memory block from pool */
/* NB that, unless all blocks in block are unallocated, pool will be unusable */ 
int	/* returns size of returned memory, 0 if none */
gs_memfix_remove_raw(P2(
	gs_memfix		*memfix,			/* memory pool to remove from */
 	byte	      	**removed		/* RETURNS pointer to removed memory */
));

/* Add raw memory block to a pool */
int	/* ret 0 if ok, -1 if error */
gs_memfix_add_raw(P3(
	gs_memfix	*memfix,		/* memory pool to add to */
	byte		*raw_memory,	/* raw memory to add to pool */
	long		raw_sizeof	/* sizeof raw memory */
));


#endif /*!defined(gsmemfix__INCLUDED)*/

