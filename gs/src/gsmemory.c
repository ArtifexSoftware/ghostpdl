/* Copyright (C) 1993, 1996, 1997, 1998 Aladdin Enterprises.  All rights reserved.

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

/*Id: gsmemory.c  */
/* Generic allocator support */
#include "memory_.h"
#include "gstypes.h"
#include "gsmemory.h"
#include "gsmdebug.h"
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

/* The structure type descriptor for GC roots. */
public_st_gc_root_t();

/* The descriptors for elements and arrays of const strings. */
private_st_const_string();
public_st_const_string_element();

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

/* Allocate a structure using a "raw memory" allocator. */
void *
gs_raw_alloc_struct_immovable(gs_raw_memory_t * rmem,
			      gs_memory_type_ptr_t pstype,
			      client_name_t cname)
{
    return gs_alloc_bytes_immovable(rmem, gs_struct_type_size(pstype), cname);
}

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
    if (mem != 0)
	gs_free_object(mem, data, cname);
}

/* ---------------- Basic-structure GC procedures ---------------- */

/* Enumerate pointers */
ENUM_PTRS_BEGIN_PROC(basic_enum_ptrs)
{
    const gc_struct_data_t *psd = pstype->proc_data;

    if (index < psd->num_ptrs) {
	const gc_ptr_element_t *ppe = &psd->ptrs[index];
	char *pptr = (char *)vptr + ppe->offset;

	switch ((gc_ptr_type_index_t)ppe->type) {
	    case GC_ELT_OBJ:
		return ENUM_OBJ(*(void **)pptr);
	    case GC_ELT_STRING:
		return ENUM_STRING((gs_string *) pptr);
	    case GC_ELT_CONST_STRING:
		return ENUM_CONST_STRING((gs_string *) pptr);
	    /****** WHAT ABOUT REFS? ******/
	}
    }
    if (!psd->super_type)
	return 0;
    return ENUM_USING(*(psd->super_type),
		      (void *)((char *)vptr + psd->super_offset),
		      pstype->ssize, index - psd->num_ptrs);
}
ENUM_PTRS_END_PROC

/* Relocate pointers */
RELOC_PTRS_BEGIN(basic_reloc_ptrs)
{
    const gc_struct_data_t *psd = pstype->proc_data;
    uint i;

    for (i = 0; i < psd->num_ptrs; ++i) {
	const gc_ptr_element_t *ppe = &psd->ptrs[i];
	char *pptr = (char *)vptr + ppe->offset;

	switch ((gc_ptr_type_index_t) ppe->type) {
	    case GC_ELT_OBJ:
		RELOC_OBJ_VAR(*(void **)pptr);
		break;
	    case GC_ELT_STRING:
		RELOC_STRING_VAR(*(gs_string *)pptr);
		break;
	    case GC_ELT_CONST_STRING:
		RELOC_CONST_STRING_VAR(*(gs_const_string *)pptr);
		break;
	    /****** WHAT ABOUT REFS? ******/
	}
    }
    if (psd->super_type)
	RELOC_USING(*(psd->super_type),
		      (void *)((char *)vptr + psd->super_offset),
		      pstype->ssize);
} RELOC_PTRS_END

/* ---------------- Implement memory using raw memory ---------------- */

/*
 * This allocator implements the full gs_memory_t API using an underlying
 * raw memory allocator in a minimal way, by ignoring garbage collection
 * issues and passing through all other requests.  Note that because raw
 * memory is always immovable, the movable and immovable allocation
 * procedures are the same here.
 */

#if 0				/* ****** NYI ****** */

/* Raw memory procedures */
private gs_memory_proc_alloc_bytes(gs_on_raw_alloc_bytes);
private gs_memory_proc_resize_object(gs_on_raw_resize_object);
private gs_memory_proc_free_object(gs_on_raw_free_object);
private gs_memory_proc_status(gs_on_raw_status);
private gs_memory_proc_free_all(gs_on_raw_free_all);

/* Object memory procedures */
private gs_memory_proc_alloc_struct(gs_on_raw_alloc_struct);
private gs_memory_proc_alloc_byte_array(gs_on_raw_alloc_byte_array);
private gs_memory_proc_alloc_struct_array(gs_on_raw_alloc_struct_array);
private gs_memory_proc_object_size(gs_on_raw_object_size);
private gs_memory_proc_object_type(gs_on_raw_object_type);
private gs_memory_proc_alloc_string(gs_on_raw_alloc_string);
private gs_memory_proc_resize_string(gs_on_raw_resize_string);
private gs_memory_proc_free_string(gs_on_raw_free_string);
private gs_memory_proc_register_root(gs_on_raw_register_root);
private gs_memory_proc_unregister_root(gs_on_raw_unregister_root);
private gs_memory_proc_enable_free(gs_on_raw_enable_free);
private const gs_memory_procs_t gs_on_raw_memory_procs =
{
    /* Raw memory procedures */
    gs_on_raw_alloc_bytes,
    gs_on_raw_resize_object,
    gs_on_raw_free_object,
    gs_on_raw_status,
    gs_on_raw_free_all,
    /* Object memory procedures */
    gs_on_raw_alloc_bytes,
    gs_on_raw_alloc_struct,
    gs_on_raw_alloc_struct,
    gs_on_raw_alloc_byte_array,
    gs_on_raw_alloc_byte_array,
    gs_on_raw_alloc_struct_array,
    gs_on_raw_alloc_struct_array,
    gs_on_raw_object_size,
    gs_on_raw_object_type,
    gs_on_raw_alloc_string,
    gs_on_raw_alloc_string,
    gs_on_raw_resize_string,
    gs_on_raw_free_string,
    gs_on_raw_register_root,
    gs_on_raw_unregister_root,
    gs_on_raw_enable_free
};

#endif /* ****** NYI ****** */
