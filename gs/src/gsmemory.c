/* Copyright (C) 1993, 1996, 1997 Aladdin Enterprises.  All rights reserved.

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

/* gsmemory.c */
/* Generic allocator support for Ghostscript library */
#include "gx.h"
#include "malloc_.h"
#include "memory_.h"
#include "gsmdebug.h"
#include "gsmemraw.h"
#include "gsrefct.h"		/* to check prototype */
#include "gsstruct.h"		/* ditto */

/* Define the fill patterns for unallocated memory. */
byte gs_alloc_fill_alloc = 0xa1;
byte gs_alloc_fill_block = 0xb1;
byte gs_alloc_fill_collected = 0xc1;
byte gs_alloc_fill_deleted = 0xd1;
byte gs_alloc_fill_free = 0xf1;

/* A 'structure' type descriptor for free blocks. */
gs_public_st_simple(st_free, byte, "(free)");

/* The 'structure' type descriptor for bytes. */
gs_public_st_simple(st_bytes, byte, "bytes");

/* The descriptors for elements and arrays of const strings. */
private_st_const_string();
public_st_const_string_element();
#define sptr ((gs_const_string *)vptr)
private 
ENUM_PTRS_BEGIN(const_string_enum_ptrs) return 0;

case 0:
*pep = (void *)sptr;
return ptr_const_string_type;
ENUM_PTRS_END
private RELOC_PTRS_BEGIN(const_string_reloc_ptrs)
{
    gs_reloc_const_string(sptr, gcst);
}
RELOC_PTRS_END
#undef sptr

/* Fill an unoccupied block with a pattern. */
/* Note that the block size may be too large for a single memset. */
void
gs_alloc_memset(void *ptr, int /*byte */ fill, ulong lsize)
{
    ulong msize = lsize;
    char *p = ptr;
    int isize;

    for (; msize; msize -= isize, p += isize) {
	isize = min(msize, max_int);
	memset(p, fill, isize);
    }
}

/* ------ Heap allocator ------ */

/*
 * An implementation of Ghostscript's memory manager interface
 * that works directly with the C heap.  We keep track of all allocated
 * blocks so we can free them at cleanup time.
 */
/* We must make sure that malloc_blocks leave the block aligned. */
#define malloc_block_data\
	malloc_block *next;\
	malloc_block *prev;\
	uint size;\
	gs_memory_type_ptr_t type;\
	client_name_t cname
struct malloc_block_data_s {
    malloc_block_data;
};
struct malloc_block_s {
    malloc_block_data;
/* ANSI C does not allow zero-size arrays, so we need the following */
/* unnecessary and wasteful workaround: */
#define _npad (-size_of(struct malloc_block_data_s) & 7)
    byte _pad[(_npad == 0 ? 8 : _npad)];	/* pad to double */
#undef _npad
};
private gs_memory_proc_alloc_bytes(gs_heap_alloc_bytes);
private gs_memory_proc_alloc_struct(gs_heap_alloc_struct);
private gs_memory_proc_alloc_byte_array(gs_heap_alloc_byte_array);
private gs_memory_proc_alloc_struct_array(gs_heap_alloc_struct_array);
private gs_memory_proc_resize_object(gs_heap_resize_object);
private gs_memory_proc_object_size(gs_heap_object_size);
private gs_memory_proc_object_type(gs_heap_object_type);
private gs_memory_proc_free_object(gs_heap_free_object);
private gs_memory_proc_alloc_string(gs_heap_alloc_string);
private gs_memory_proc_resize_string(gs_heap_resize_string);
private gs_memory_proc_free_string(gs_heap_free_string);
private gs_memory_proc_register_root(gs_heap_register_root);
private gs_memory_proc_unregister_root(gs_heap_unregister_root);
private gs_memory_proc_status(gs_heap_status);
private gs_memory_proc_enable_free(gs_heap_enable_free);
gs_memory_heap_t gs_memory_heap_default =
{
    {gs_heap_alloc_bytes,
     gs_heap_alloc_bytes,
     gs_heap_alloc_struct,
     gs_heap_alloc_struct,
     gs_heap_alloc_byte_array,
     gs_heap_alloc_byte_array,
     gs_heap_alloc_struct_array,
     gs_heap_alloc_struct_array,
     gs_heap_resize_object,
     gs_heap_object_size,
     gs_heap_object_type,
     gs_heap_free_object,
     gs_heap_alloc_string,
     gs_heap_alloc_string,
     gs_heap_resize_string,
     gs_heap_free_string,
     gs_heap_register_root,
     gs_heap_unregister_root,
     gs_heap_status,
     gs_heap_enable_free
    },
    gs_memory_heap_initial_values
};


/* The limit on allocable space is public. */
long gs_malloc_limit = max_long;

/* The record of maximum allocation is public. */
long gs_malloc_max = 0;		/* ref'd by XL */

/* Initialize the malloc heap. */
private long heap_available(P0());
void
gs_malloc_init(void)
{
    gs_memory_heap_construct(&gs_memory_heap_default, 0);
}
/* Estimate the amount of available memory by probing with mallocs. */
/* We may under-estimate by a lot, but that's better than winding up with */
/* a seriously inflated address space. */
/* This is quite a hack! */
#define max_malloc_probes 20
#define malloc_probe_size 64000
private long
heap_available(void)
{
    long avail = 0;
    void *probes[max_malloc_probes];
    uint n;

    for (n = 0; n < max_malloc_probes; n++) {
	if ((probes[n] = malloc(malloc_probe_size)) == 0)
	    break;
	if_debug2('a', "[a]heap_available probe[%d]=0x%lx\n",
		  n, (ulong) probes[n]);
	avail += malloc_probe_size;
    }
    while (n)
	free(probes[--n]);
    return avail;
}

/* Allocate various kinds of blocks. */
private byte *
gs_heap_alloc_bytes(gs_memory_t * mem, uint size, client_name_t cname)
{
    byte *ptr = 0;
    gs_memory_heap_t *const hmem = (gs_memory_heap_t *) mem;

#ifdef DEBUG
    const char *msg;
    static const char *ok_msg = "OK";

#  define set_msg(str) (msg = (str))
#else
#  define set_msg(str) DO_NOTHING
#endif
    if (size > gs_malloc_limit - sizeof(malloc_block)
	) {			/* Definitely too large to allocate; also avoids overflow. */
	set_msg("exceeded limit");
    } else {
	uint added = size + sizeof(malloc_block);

	if (gs_malloc_limit - added < hmem->malloc_used)
	    set_msg("exceeded limit");
	else if ((ptr = (byte *) (hmem->raw_memory
	       ? (*hmem->raw_memory->procs.malloc) (hmem->raw_memory, added)
				  : malloc(added))) == 0)
	    set_msg("failed");
	else {
	    malloc_block *bp = (malloc_block *) ptr;

	    if (hmem->malloc_list)
		hmem->malloc_list->prev = bp;
	    bp->next = hmem->malloc_list;
	    bp->prev = 0;
	    bp->size = size;
	    bp->type = &st_bytes;
	    bp->cname = cname;
	    hmem->malloc_list = bp;
	    set_msg(ok_msg);
	    ptr = (byte *) (bp + 1);
	    gs_alloc_fill(ptr, gs_alloc_fill_alloc, size);
	    hmem->malloc_used += size + sizeof(malloc_block);
	    if (hmem->malloc_used > hmem->malloc_max)
		hmem->malloc_max = hmem->malloc_used;
	}
    }
#ifdef DEBUG
    {
	malloc_block *bp = hmem->malloc_list;

	for (; bp; bp = bp->next);
    }
#endif
    return ptr;
#undef set_msg
}
private void *
gs_heap_alloc_struct(gs_memory_t * mem, gs_memory_type_ptr_t pstype,
		     client_name_t cname)
{
    void *ptr = gs_heap_alloc_bytes(mem, gs_struct_type_size(pstype), cname);

    if (ptr == 0)
	return 0;
    ((malloc_block *) ptr)[-1].type = pstype;
    return ptr;
}
private byte *
gs_heap_alloc_byte_array(gs_memory_t * mem, uint num_elements, uint elt_size,
			 client_name_t cname)
{
    ulong lsize = (ulong) num_elements * elt_size;

    if (lsize != (uint) lsize)
	return 0;
    return gs_heap_alloc_bytes(mem, (uint) lsize, cname);
}
private void *
gs_heap_alloc_struct_array(gs_memory_t * mem, uint num_elements,
			   gs_memory_type_ptr_t pstype, client_name_t cname)
{
    void *ptr = gs_heap_alloc_byte_array(mem, num_elements, gs_struct_type_size(pstype), cname);

    if (ptr == 0)
	return 0;
    ((malloc_block *) ptr)[-1].type = pstype;
    return ptr;
}
private void *
gs_heap_resize_object(gs_memory_t * mem, void *obj, uint new_num_elements,
		      client_name_t cname)
{
    gs_memory_heap_t *const hmem = (gs_memory_heap_t *) mem;

    malloc_block *ptr = (malloc_block *) obj - 1;
    gs_memory_type_ptr_t pstype = ptr->type;
    uint old_size = gs_object_size(mem, obj) + sizeof(malloc_block);
    uint new_size =
    gs_struct_type_size(pstype) * new_num_elements +
    sizeof(malloc_block);
    malloc_block *new_ptr = (malloc_block *) (hmem->raw_memory
      ? (*hmem->raw_memory->procs.realloc) (hmem->raw_memory, ptr, new_size)
				     : gs_realloc(ptr, old_size, new_size));

    if (new_ptr == 0)
	return 0;
    if (new_ptr->prev)
	new_ptr->prev->next = new_ptr;
    else
	hmem->malloc_list = new_ptr;
    if (new_ptr->next)
	new_ptr->next->prev = new_ptr;
    new_ptr->size = new_size - sizeof(malloc_block);
    hmem->malloc_used -= old_size;
    hmem->malloc_used += new_size;
    if (new_size > old_size)
	gs_alloc_fill((byte *) new_ptr + old_size,
		      gs_alloc_fill_alloc, new_size - old_size);
    return new_ptr + 1;
}
private uint
gs_heap_object_size(gs_memory_t * mem, const void *ptr)
{
    return ((const malloc_block *)ptr)[-1].size;
}
private gs_memory_type_ptr_t
gs_heap_object_type(gs_memory_t * mem, const void *ptr)
{
    return ((const malloc_block *)ptr)[-1].type;
}
private void
gs_heap_free_object(gs_memory_t * mem, void *ptr, client_name_t cname)
{
    gs_memory_heap_t *const hmem = (gs_memory_heap_t *) mem;
    malloc_block *bp;

    if (hmem->disable_free_object)
	return;
    bp = hmem->malloc_list;
    if (gs_debug_c('a'))
	dprintf3("[a-]gs_free(%s) 0x%lx(%u)\n",
		 client_name_string(cname), (ulong) ptr,
		 (ptr == 0 ? 0 : ((malloc_block *) ptr)[-1].size));
    if (ptr == 0)
	return;
    if (ptr == bp + 1) {
	hmem->malloc_list = bp->next;
	hmem->malloc_used -= bp->size + sizeof(malloc_block);
	if (hmem->malloc_list)
	    hmem->malloc_list->prev = 0;
	gs_alloc_fill(bp, gs_alloc_fill_free,
		      bp->size + sizeof(malloc_block));
	if (hmem->raw_memory)
	    (*hmem->raw_memory->procs.free) (hmem->raw_memory, bp);
	else
	    free(bp);
    } else {
	malloc_block *np;
	char *free_this;

	for (; (np = bp->next) != 0; bp = np) {
	    if (ptr == np + 1) {
		bp->next = np->next;
		if (np->next)
		    np->next->prev = bp;
		hmem->malloc_used -= np->size + sizeof(malloc_block);
		gs_alloc_fill(np, gs_alloc_fill_free,
			      np->size + sizeof(malloc_block));
		if (hmem->raw_memory)
		    (*hmem->raw_memory->procs.free) (hmem->raw_memory, np);
		else
		    free(np);
		return;
	    }
	}
	lprintf2("%s: free 0x%lx not found!\n",
		 client_name_string(cname), (ulong) ptr);
	free_this = (char *)((malloc_block *) ptr - 1);
	if (hmem->raw_memory)
	    (*hmem->raw_memory->procs.free) (hmem->raw_memory, free_this);
	else
	    free(free_this);
    }
}
private byte *
gs_heap_alloc_string(gs_memory_t * mem, uint nbytes, client_name_t cname)
{
    return gs_heap_alloc_bytes(mem, nbytes, cname);
}
private byte *
gs_heap_resize_string(gs_memory_t * mem, byte * data, uint old_num, uint new_num,
		      client_name_t cname)
{
    if (gs_heap_object_type(mem, data) != &st_bytes) {
	lprintf2("%s: resizing non-string 0x%lx!\n",
		 client_name_string(cname), (ulong) data);
    }
    return gs_heap_resize_object(mem, data, new_num, cname);
}
private void
gs_heap_free_string(gs_memory_t * mem, byte * data, uint nbytes,
		    client_name_t cname)
{
/****** SHOULD CHECK SIZE IF DEBUGGING ******/
    gs_heap_free_object(mem, data, cname);
}
private void
gs_heap_register_root(gs_memory_t * mem, gs_gc_root_t * rp, gs_ptr_type_t ptype,
		      void **up, client_name_t cname)
{
}
private void
gs_heap_unregister_root(gs_memory_t * mem, gs_gc_root_t * rp,
			client_name_t cname)
{
}
private void
gs_heap_status(gs_memory_t * mem, gs_memory_status_t * pstat)
{
    gs_memory_heap_t *const hmem = (gs_memory_heap_t *) mem;

    pstat->allocated = hmem->malloc_used + (hmem->raw_memory
	     ? (*hmem->raw_memory->procs.query_freespace) (hmem->raw_memory)
					    : heap_available());
    pstat->used = hmem->malloc_used;
}
private void
gs_heap_enable_free(gs_memory_t * mem, bool enable)
{
    gs_memory_heap_t *const hmem = (gs_memory_heap_t *) mem;

    hmem->disable_free_object = !enable;
}

/* Release all malloc'ed blocks. */
void
gs_malloc_release(void)
{
    gs_memory_heap_t *const hmem = &gs_memory_heap_default;

    malloc_block *bp = hmem->malloc_list;
    malloc_block *np;

    for (; bp != 0; bp = np) {
	np = bp->next;
	if (gs_debug_c('a'))
	    dprintf3("[a]gs_malloc_release(%s) 0x%lx(%u)\n",
		     client_name_string(bp->cname), (ulong) (bp + 1),
		     bp->size);
	gs_alloc_fill(bp + 1, gs_alloc_fill_free, bp->size);
	if (hmem->raw_memory)
	    (*hmem->raw_memory->procs.free) (hmem->raw_memory, bp);
	else
	    free(bp);
    }
    hmem->malloc_list = 0;
    hmem->malloc_used = 0;
}

/* ------ Other memory management ------ */

/* No-op pointer enumeration procedure */
ENUM_PTRS_BEGIN_PROC(gs_no_struct_enum_ptrs)
{
    return 0;
    ENUM_PTRS_END_PROC
}

/* No-op pointer relocation procedure */
RELOC_PTRS_BEGIN(gs_no_struct_reloc_ptrs)
{
}
RELOC_PTRS_END

/* No-op freeing procedures */
void
gs_ignore_free_object(gs_memory_t * mem, void *data, client_name_t cname)
{
}
void
gs_ignore_free_string(gs_memory_t * mem, byte * data, uint nbytes,
		      client_name_t cname)
{
}

/* Get the size of a structure from the descriptor. */
uint
gs_struct_type_size(gs_memory_type_ptr_t pstype)
{
    return pstype->ssize;
}

/* Get the name of a structure from the descriptor. */
struct_name_t
gs_struct_type_name(gs_memory_type_ptr_t pstype)
{
    return pstype->sname;
}

/* Normal freeing routine for reference-counted structures. */
void
rc_free_struct_only(gs_memory_t * mem, void *data, client_name_t cname)
{
    gs_free_object(mem, data, cname);
}

/*  Public constructors/destructors for gs_memory_heap_t */
int				/* -ve error code or 0 */
gs_memory_heap_construct(
			    gs_memory_heap_t * hmem,	/* allocator to init */
			    gs_memraw * raw_memory	/* underling raw memory allocator to use */
)
{
    hmem->procs = gs_memory_heap_default.procs;
    hmem->raw_memory = raw_memory;
    hmem->malloc_list = 0;
    hmem->malloc_used = 0;
    hmem->malloc_max = 0;
    hmem->disable_free_object = false;
    return 0;
}
void
gs_memory_heap_destruct(
			   gs_memory_heap_t * hmem	/* allocator to dnit */
)
{
}

gs_memraw *			/* rets raw memory allocator in this object */
gs_memory_heap_get_raw(
			  gs_memory_heap_t * hmem	/* allocator to operate on */
)
{
    return hmem->raw_memory;
}
