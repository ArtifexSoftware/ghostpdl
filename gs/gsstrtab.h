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

/*Id: gsstrtab.h  */
/* Tabular definition of GC descriptors */

#ifndef gsstrtab_INCLUDED
#  define gsstrtab_INCLUDED

#include "gsstruct.h"

/*
 * As an alternative to defining a procedure for the simpler structure types,
 * we can define the type's GC information using a stock procedure and a
 * table.  Each entry in the table defines one element of the structure.
 */
typedef struct gs_memory_tabular_struct_type_s
                                gs_memory_tabular_struct_type_t;

/* Define elements consisting of a single pointer. */

typedef enum {
    gc_ptr_struct,		/* obj * or const obj * */
    gc_ptr_string,		/* gs_string or gs_const_string */
    gc_ptr_ref
} gc_ptr_type_index_t;

typedef struct gc_ptr_element_s {
    ushort offset;
           ushort /*gc_ptr_type_index_t */ type;
} gc_ptr_element_t;

/* Define elements specifying superclasses. */

typedef struct gc_super_element_s {
    ushort offset;
    const gs_memory_tabular_struct_type_t *stype;
} gc_super_element_t;

/* Define elements specifying arrays. */

typedef struct gc_array_element_s {
    ushort offset;		/* of first element */
    ushort count;		/* 0 means compute from size of object */
    const gs_memory_tabular_struct_type_t *stype;	/* element type */
} gc_array_element_t;

/* Define the complete table of descriptor data. */

typedef struct gc_struct_data_s {
    ushort num_ptrs;
    byte num_supers;
    byte num_arrays;
    const gc_ptr_element_t *ptrs;
    const gc_super_element_t *supers;
    const gc_array_element_t *arrays;
} gc_struct_data_t;

struct gs_memory_tabular_struct_type_s {
    gs_memory_struct_type_common;
    gc_struct_data_t data;
};

/*
 * Define the enum_ptrs and reloc_ptrs procedures, and the declaration
 * macros, for table-specified structures.
 */
struct_proc_enum_ptrs(tabular_enum_ptrs);
struct_proc_reloc_ptrs(tabular_reloc_ptrs);
#define gs__st_table(tscope_st, stname, stype, sname, nptrs, ptrs, nsupers, supers, narrays, arrays)\
  tscope_st stname = {\
    sizeof(stype), sname, 0, 0, tabular_enum_ptrs, tabular_reloc_ptrs,\
    { nptrs, nsupers, narrays, ptrs, supers, arrays }\
  }
#define gs_public_st_table(stname, stype, sname, nptrs, ptrs, nsupers, supers, narrays, arrays)\
  gs__st_table(public gs_memory_tabular_struct_type_t,\
    stname, stype, sname, nptrs, ptrs, nsupers, supers, narrays, arrays)
#define gs_private_st_table(stname, stype, sname, nptrs, ptrs, nsupers, supers, narrays, arrays)\
  gs__st_table(private gs_memory_tabular_struct_type_t,\
    stname, stype, sname, nptrs, ptrs, nsupers, supers, narrays, arrays)

#endif /* gsstrtab_INCLUDED */
