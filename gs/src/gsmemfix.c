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

/* gsmemfix.c Very simple fixed-block raw memory allocator */

/* Initial version 02/03/1998 by John Desrosiers (soho@crl.com) */

#include "memory_.h"
#include "gsmemraw.h"
#include "gsmemfix.h"

/* --------------- Constants ---------------------- */

#define gs_memfix_align 8	/* align memory to double */
#define ChunkSignature 'CS'

/* ------------- Structures ----------- */

/* Allocated/free chunk */
#define memfix_chunk_data\
	long total_size;	/* sizeof chunk including this overhead */\
	short unsigned is_allocated
struct memfix_chunk_data_s {
    memfix_chunk_data;
};
typedef struct memfix_chunk_s {
    memfix_chunk_data;
    /* ANSI C does not allow zero-size arrays, so we need the following */
    /* unnecessary and wasteful workaround: */
#define _npad (-size_of(struct memfix_chunk_data_s) & (gs_memfix_align - 1))
    byte _pad[(_npad == 0 ? gs_memfix_align : _npad)];
#undef _npad
} gs_memfix_chunk;


/* Fixed memory allocation block */
typedef struct gs_memfix_alloc_block_s gs_memfix_alloc_block;

#define memfix_alloc_block_data\
	gs_memfix_alloc_block *next;	/* next block, 0 if last one */\
	long	total_size;		/* total # bytes in this block */\
	byte	*original_memory;	/* memory as provided to add_raw */\
	long	original_memory_sizeof	/* memory size as provided to add_raw */
struct memfix_block_data_s {
    memfix_alloc_block_data;
};
struct gs_memfix_alloc_block_s {
    memfix_alloc_block_data;
#define _npad (-size_of(struct memfix_chunk_data_s) & (gs_memfix_align - 1))
    byte _pad[(_npad == 0 ? gs_memfix_align : _npad)];
#undef _npad
    gs_memfix_chunk first_chunk;	/* first chunk, not a pointer */
};

/* --------------- Forward Decl's ---------------------- */

private void
     trim_and_allocate_chunk(P3(
				   gs_memfix * memfix,	/* memfix to update */
				   gs_memfix_chunk * chunk,	/* free chunk to allocate */
				   unsigned int bytes_to_alloc	/* bytes to allocate */
			     ));

private mem_proc_destructor(gs_memfix_destructor);
private mem_proc_query_freespace(gs_memfix_query_freespace);
private mem_proc_malloc(gs_memfix_malloc);
private mem_proc_realloc(gs_memfix_realloc);
private mem_proc_free(gs_memfix_free);


/* -------------- Public Procedures ------------------------------------ */

/* Construct an empty */
void
gs_memfix_constructor(
			 gs_memfix * memfix	/* memfix to construct */
)
{
    static gs_memraw_procs static_procs =
    {
	gs_memfix_destructor, gs_memfix_query_freespace,
	gs_memfix_malloc, gs_memfix_realloc, gs_memfix_free
    };

    memfix->procs = static_procs;
    memfix->total_universe = memfix->total_free = 0;
    memfix->first = 0;
}

/* Destruct a raw memory allocator */
private void
gs_memfix_destructor(
			gs_memraw * memraw	/* memory to destruct */
)
{
    /* It'd be nice to dealloc mem, but there's nobody to hand it back to */
    /* Should cascade down to superclasses, but there are none */
}

/* Add raw memory block to a */
int				/* ret 0 if ok, -1 if error raw memory allocator */
gs_memfix_add_raw(
		     gs_memfix * memfix,	/* memory to add to */
		     byte * raw_memory,		/* raw memory to add to */
		     long raw_sizeof	/* sizeof raw memory */
)
{
    byte *original_raw_memory = raw_memory;
    long original_raw_sizeof = raw_sizeof;
    gs_memfix_alloc_block *block;

    /* Align address & length of block */
    unsigned head_align = (-(long)raw_memory) & (gs_memfix_align - 1);

    raw_memory += head_align;
    raw_sizeof -= head_align;
    raw_sizeof -= raw_sizeof & (gs_memfix_align - 1);
    if (raw_sizeof < sizeof(gs_memfix_alloc_block))
	return -1;		/* if it's too small, spit it back */

    block = (gs_memfix_alloc_block *) raw_memory;
    block->original_memory = original_raw_memory;
    block->original_memory_sizeof = original_raw_sizeof;
    block->total_size = raw_sizeof;
    block->next = memfix->first;
    block->first_chunk.is_allocated = 0;
    block->first_chunk.total_size
	= raw_sizeof - sizeof(*block) + sizeof(block->first_chunk);

    memfix->first = block;
    memfix->total_universe += raw_sizeof - sizeof(*block);
    memfix->total_free += raw_sizeof - sizeof(*block);

    return 0;
}

/* Remove the most recently added raw memory block from raw memory allocator */
/* NB that, unless all blocks in block are unallocated, will be unusable */
int				/* returns size of returned memory, 0 if none */
gs_memfix_remove_raw(
			gs_memfix * memfix,	/* memory to remove from */
			byte ** removed		/* RETURNS pointer to removed memory */
)
{
    gs_memfix_alloc_block *block = memfix->first;

    if (block) {
	long original_sizeof = block->original_memory_sizeof;

	*removed = block->original_memory;
	memfix->total_universe
	    -= block->total_size - sizeof(gs_memfix_alloc_block);
	memfix->total_free
	    -= block->total_size - sizeof(gs_memfix_alloc_block);
	memfix->first = block->next;
	return original_sizeof;
    } else
	return 0;
}

/* Return amount of free memory in raw memory allocator */
private long			/* # bytes free */
gs_memfix_query_freespace(
			     gs_memraw * memraw		/* mem device to operate on */
)
{
    gs_memfix *const memfix = (gs_memfix *) memraw;

    return memfix->total_free;
}

/* Allocate requested # bytes from given raw memory allocator */
private void *			/* rets allocated bytes, 0 if none */
gs_memfix_malloc(
		    gs_memraw * memraw,		/* mem device to operate on */
		    unsigned int malloc_sizeof	/* # bytes to malloc */
)
{
    gs_memfix *const memfix = (gs_memfix *) memraw;
    gs_memfix_alloc_block *block;
    void *found_memory = 0;

    /* All mallocs must allocate an aligned # of bytes & include overhead */
    malloc_sizeof
	+= (-(int)malloc_sizeof & (gs_memfix_align - 1)) + sizeof(gs_memfix_chunk);

    /* Search each allocation block for a free block of sufficient size */
    for (block = memfix->first; !found_memory && block; block = block->next) {
	gs_memfix_chunk *limit_chunk
	= (gs_memfix_chunk *) ((byte *) block + block->total_size);
	gs_memfix_chunk *last_free = 0;
	gs_memfix_chunk *chunk;

	/* Search for first free chunk w/a sufficent free block */
	for (chunk = &block->first_chunk; chunk < limit_chunk;
	 chunk = (gs_memfix_chunk *) ((byte *) chunk + chunk->total_size)) {
	    if (chunk->is_allocated)
		last_free = 0;
	    else {
		/* consolidate free chunks as we go */
		if (last_free)
		    last_free->total_size += chunk->total_size;
		else
		    last_free = chunk;
		if (last_free->total_size >= malloc_sizeof) {
		    /* found enough memory */
		    trim_and_allocate_chunk(memfix, last_free, malloc_sizeof);
		    found_memory = (void *)(last_free + 1);
		    break;
		}
	    }
	}
    }
    return found_memory;
}

/* Deallocate memory taken from this raw memory allocator */
private void
gs_memfix_free(
		  gs_memraw * memraw,	/* mem device to operate on */
		  void *raw	/* memory to deallocate */
)
{
    gs_memfix *const memfix = (gs_memfix *) memraw;
    gs_memfix_chunk *const chunk = (gs_memfix_chunk *) raw - 1;

    /* This prevents multiple & incorrect de-allocations */
    if (raw && chunk->is_allocated == ChunkSignature) {
	memfix->total_free += chunk->total_size;
	chunk->is_allocated = 0;
    }
}

/* Resize memory taken from this raw memory allocator */
private void *			/* address of reallocated block, 0 if error */
gs_memfix_realloc(
		     gs_memraw * memraw,	/* mem device to operate on */
		     void *old_raw,	/* memory to reallocate */
		     unsigned int malloc_sizeof		/* # bytes to malloc */
)
{
    gs_memfix *const memfix = (gs_memfix *) memraw;

    if (!old_raw || !malloc_sizeof)
	/* Handle degenerate cases */
	return !old_raw ? gs_memfix_malloc(memraw, malloc_sizeof)
	    : (gs_memfix_free(memraw, old_raw), 0);
    else {
	/* Credible realloc, size can be GT, LT or approx EQ to original */
	gs_memfix_chunk *const old_chunk = (gs_memfix_chunk *) old_raw - 1;
	gs_memfix_chunk *new_chunk = 0;
	long old_malloc_sizeof
	= old_chunk->total_size - sizeof(gs_memfix_chunk);

	/* All mallocs must allocate an aligned # of bytes & include overhead */
	malloc_sizeof
	    += (-(int)malloc_sizeof & (gs_memfix_align - 1)) + sizeof(gs_memfix_chunk);

	/* This prevents multiple & incorrect de-allocations */
	if (old_chunk->is_allocated != ChunkSignature)
	    return 0;		/* garbage input */

	if (malloc_sizeof <= old_chunk->total_size) {
	    /* Easy case: just shrink or maintain allocation */
	    memfix->total_free += old_chunk->total_size;	/* pretend to deallocate */
	    trim_and_allocate_chunk(memfix, old_chunk, malloc_sizeof);
	    new_chunk = old_chunk;
	} else {
	    /*
	     * Increase alloc'n. Try to:
	     * i  ) Expand into any free space after curr block
	     * ii ) Expand into any free space before curr block & move memory
	     * iii) Find new mem space, copy old to new, delete old
	     */
	    /* First, have to find current allocation's block & position within */

	    /* Search allocation blocks for this block. Search for free space too */
	    long extra_bytes_needed = malloc_sizeof - old_chunk->total_size;
	    gs_memfix_alloc_block *found_block = 0;
	    gs_memfix_alloc_block *block;

	    for (block = memfix->first; !found_block && block;
		 block = block->next) {
		gs_memfix_chunk *limit_chunk
		= (gs_memfix_chunk *) ((byte *) block + block->total_size);
		gs_memfix_chunk *last_free;
		gs_memfix_chunk *chunk;
		gs_memfix_chunk *chunk_after;
		long chunk_after_length;

		/* Quickly eliminate this block if it doesn't encompass old_chunk */
		if (old_chunk < &block->first_chunk || old_chunk >= limit_chunk)
		    continue;
		found_block = block;

		/* Consolidate free space after this chunk, grow old chunk into it */
		chunk_after = (gs_memfix_chunk *)
		    ((byte *) old_chunk + old_chunk->total_size);
		chunk_after_length = 0;
		for (chunk = chunk_after; chunk < limit_chunk;
		     chunk = (gs_memfix_chunk *) ((byte *) chunk + chunk->total_size))
		    if (!chunk->is_allocated)
			chunk_after_length += chunk->total_size;
		    else
			break;	/* no more freespace in block */
		if (chunk_after_length) {
		    chunk_after->total_size = chunk_after_length;	/* consolidate tail pieces */

		    /* Grow chunk into freespace after it */
		    trim_and_allocate_chunk(memfix, chunk_after,
			       min(chunk_after_length, extra_bytes_needed));
		    old_chunk->total_size += chunk_after->total_size;
		    if ((extra_bytes_needed -= chunk_after->total_size) <= 0) {
			new_chunk = old_chunk;
			break;	/* done */
		    }
		}
		/* Try to consolidate with free chunk before old one */

		/* Search for last free chunk before old chunk */
		last_free = 0;
		for (chunk = &block->first_chunk; chunk <= old_chunk;
		     chunk = (gs_memfix_chunk *) ((byte *) chunk + chunk->total_size)) {
		    if (!chunk->is_allocated) {
			/* consolidate free chunks as we go */
			if (last_free)
			    last_free->total_size += chunk->total_size;
			else
			    last_free = chunk;
		    } else if (chunk != old_chunk)
			last_free = 0;
		    else {
			/* worked back up to old chunk */
			if (last_free && last_free->total_size >= extra_bytes_needed) {
			    /* There's an adequate free block before old chunk */
			    new_chunk = last_free;
			    memfix->total_free += old_chunk->total_size;
			    new_chunk->total_size += old_chunk->total_size;
			    /* use memmove instead of memcpy since it handles overlap */
			    memmove(new_chunk + 1, old_chunk + 1,
				old_chunk->total_size - sizeof(*old_chunk));
			    trim_and_allocate_chunk(memfix, new_chunk, malloc_sizeof);
			}
			break;	/* fail or success, no in-place reallocation possible */
		    }
		}
	    }
	}
	/* Last-ditch maneuver: if no luck, try to alloc new & copy to it */
	if (!new_chunk) {
	    char *new_raw = gs_memfix_malloc(memraw, malloc_sizeof);

	    if (new_raw) {
		new_chunk = (gs_memfix_chunk *) new_raw - 1;
		memcpy(new_raw, old_raw, old_chunk->total_size - sizeof(*old_chunk));
		gs_memfix_free(memraw, old_raw);
	    }
	}
	/* If ultimate failure, go back to original size; else return new chunk */
	if (new_chunk)
	    return (new_chunk + 1);	/* xvert chunk to its raw mem */
	else {
	    /* realloc fails, scale block back to original size, ret NULL */
	    gs_memfix_realloc(memraw, old_raw, old_malloc_sizeof);
	    return 0;
	}
    }
}

/* -------------- Private Procedures ------------------------------------ */

/* Trim & allocate a given free chunk */
private void
trim_and_allocate_chunk(
			   gs_memfix * memfix,	/* memory to alloc from */
			   gs_memfix_chunk * chunk,	/* free chunk to allocate */
			   unsigned int bytes_to_alloc	/* bytes to allocate */
)
{
    chunk->is_allocated = ChunkSignature;

    /* if there's only a few bytes extra, lump them in with allocation */
    if (chunk->total_size
	>= bytes_to_alloc + sizeof(gs_memfix_chunk)) {
	/* Make remaining part of block into new block */
	gs_memfix_chunk *created = (gs_memfix_chunk *)
	((byte *) chunk + bytes_to_alloc);

	created->total_size = chunk->total_size - bytes_to_alloc;
	created->is_allocated = 0;
	chunk->total_size = bytes_to_alloc;
    }
    /* update book keeping */
    memfix->total_free -= chunk->total_size;
}
