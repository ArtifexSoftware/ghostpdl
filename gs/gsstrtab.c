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

/*Id: gsstrtab.c  */
/* Implement tabular definition of GC descriptors */
#include "std.h"
#include "gstypes.h"
#include "gsmemory.h"
#include "gsstrtab.h"

/* Compute the number of pointers in an object. */
private uint
tabular_num_ptrs(const gs_memory_tabular_struct_type_t * ptst, uint size)
{
    uint total = ptst->data.num_ptrs;
    uint i;

    for (i = 0; i < ptst->data.num_supers; ++i) {
	const gs_memory_tabular_struct_type_t *pstype =
	ptst->data.supers[i].stype;

	total += tabular_num_ptrs(pstype, pstype->ssize);
    }

    for (i = 0; i < ptst->data.num_arrays; ++i) {
	const gc_array_element_t *pae = &ptst->data.arrays[i];
	const gs_memory_tabular_struct_type_t *pstype = pae->stype;
	uint count = pae->count;
	uint elt_size = pstype->ssize;

	if (count == 0)
	    count = (size - pae->offset) / elt_size;
	total += tabular_num_ptrs(pstype, pstype->ssize) * count;
    }

    return total;
}

/* Enumerate pointers */
ENUM_PTRS_BEGIN_PROC(tabular_enum_ptrs)
{

    const gc_struct_data_t *psd =
    &((const gs_memory_tabular_struct_type_t *)pstype)->data;
    uint idx = index;
    uint i;

    if (idx < psd->num_ptrs) {
	const gc_ptr_element_t *ppe = &psd->ptrs[idx];
	char *pptr = (char *)vptr + ppe->offset;

	switch ((gc_ptr_type_index_t) ppe->type) {
	    case gc_ptr_struct:
		return ENUM_OBJ(*(void **)pptr);
	    case gc_ptr_string:
		return ENUM_STRING((gs_string *) pptr);
/****** WHAT ABOUT REFS? ******/
	}
    }
    idx -= psd->num_ptrs;

    for (i = 0; i < psd->num_supers; ++i) {
	const gc_super_element_t *pse = &psd->supers[i];
	const gs_memory_tabular_struct_type_t *pstype = pse->stype;
	void *psuper = (void *)((char *)vptr + pse->offset);
	uint count = tabular_num_ptrs(pstype, pstype->ssize);

	if (idx < count)
	    return ENUM_USING(*(const gs_memory_struct_type_t *)pstype,
			      psuper, pstype->ssize, idx);
	idx -= count;
    }

    for (i = 0; i < psd->num_arrays; ++i) {
	const gc_array_element_t *pae = &psd->arrays[i];
	const gs_memory_tabular_struct_type_t *pstype = pae->stype;
	char *parray = (char *)vptr + pae->offset;
	uint count = pae->count;
	uint elt_size = pstype->ssize;

	if (count == 0)
	    count = (size - pae->offset) / elt_size;
	if (idx < count)
	    return ENUM_USING(*(const gs_memory_struct_type_t *)pstype,
			      (void *)(parray + idx * elt_size), elt_size,
			      idx);
	idx -= count;
    }

    return 0;

}
ENUM_PTRS_END_PROC

/* Relocate pointers */
RELOC_PTRS_BEGIN(tabular_reloc_ptrs)
{

    const gc_struct_data_t *psd =
    &((const gs_memory_tabular_struct_type_t *)pstype)->data;
    uint i;

    for (i = 0; i < psd->num_ptrs; ++i) {
	const gc_ptr_element_t *ppe = &psd->ptrs[i];
	char *pptr = (char *)vptr + ppe->offset;

	switch ((gc_ptr_type_index_t) ppe->type) {
	    case gc_ptr_struct:
		RELOC_OBJ_VAR(*(void **)pptr);
		break;
	    case gc_ptr_string:
		RELOC_STRING_VAR(*(gs_string *) pptr);
		break;
/****** WHAT ABOUT REFS? ******/
	}
    }

    for (i = 0; i < psd->num_supers; ++i) {
	const gc_super_element_t *pse = &psd->supers[i];
	const gs_memory_tabular_struct_type_t *pstype = pse->stype;
	void *psuper = (void *)((char *)vptr + pse->offset);

	RELOC_USING(*(const gs_memory_struct_type_t *)pstype,
		    psuper, pstype->ssize);
    }

    for (i = 0; i < psd->num_arrays; ++i) {
	const gc_array_element_t *pae = &psd->arrays[i];
	const gs_memory_tabular_struct_type_t *pstype = pae->stype;
	char *parray = (char *)vptr + pae->offset;
	uint count = pae->count;
	uint elt_size = pstype->ssize;
	uint index;

	if (count == 0)
	    count = (size - pae->offset) / elt_size;
	for (index = 0; index < count; ++index)
	    RELOC_USING(*(const gs_memory_struct_type_t *)pstype,
			(void *)(parray + index * elt_size), elt_size);
    }

} RELOC_PTRS_END
