/* Copyright (C) 1993, 1995, 1999 Aladdin Enterprises.  All rights reserved.

   This software is licensed to a single customer by Artifex Software Inc.
   under the terms of a specific OEM agreement.
 */

/*$RCSfile$ $Revision$ */
/* Definition of 'save' structure */
/* Requires isave.h */

#ifndef isstate_INCLUDED
#  define isstate_INCLUDED

/* Saved state of allocator and other things as needed. */
/*typedef struct alloc_save_s alloc_save_t; *//* in isave.h */
struct alloc_save_s {
    gs_ref_memory_t state;	/* must be first for subclassing */
    vm_spaces spaces;
    bool restore_names;
    bool is_current;
    ulong id;
    void *client_data;
};

#define private_st_alloc_save()	/* in isave.c */\
  gs_private_st_suffix_add1(st_alloc_save, alloc_save_t, "alloc_save",\
    save_enum_ptrs, save_reloc_ptrs, st_ref_memory, client_data)

#endif /* isstate_INCLUDED */
