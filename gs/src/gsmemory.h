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

/*Id: gsmemory.h  */
/* Client interface for memory allocation */

/*
 * The allocator knows about two basic kinds of memory: objects, which are
 * aligned and cannot have pointers to their interior, and strings, which are
 * not aligned and which can have interior references.
 *
 * The standard allocator includes a garbage collector.  The garbage
 * collector normally may move objects, relocating pointers to them;
 * however, objects may be declared immovable at the time of allocation.
 * Clients must not attempt to resize immovable objects, and must not create
 * references to substrings of immovable strings.
 */

#ifndef gsmemory_INCLUDED
#  define gsmemory_INCLUDED

#include "gsmemraw.h"

/*
 * Define the (opaque) type for a structure descriptor.
 */
typedef struct gs_memory_struct_type_s gs_memory_struct_type_t;
typedef const gs_memory_struct_type_t *gs_memory_type_ptr_t;

	/* Accessors for structure types. */

typedef client_name_t struct_name_t;

/* Get the size of a structure from the descriptor. */
uint gs_struct_type_size(P1(gs_memory_type_ptr_t));

/* Get the name of a structure from the descriptor. */
struct_name_t gs_struct_type_name(P1(gs_memory_type_ptr_t));

#define gs_struct_type_name_string(styp)\
  ((const char *)gs_struct_type_name(styp))

/* An opaque type for the garbage collector state. */
/* We need this because it is passed to pointer implementation procedures. */
typedef struct gc_state_s gc_state_t;

/*
 * A pointer type defines how to mark the referent of the pointer.
 * We define it here so that we can define ptr_struct_type,
 * which is needed so we can define gs_register_struct_root.
 */
typedef struct gs_ptr_procs_s {

    /* Unmark the referent of a pointer. */

#define ptr_proc_unmark(proc)\
  void proc(P2(void *, gc_state_t *))
    ptr_proc_unmark((*unmark));

    /* Mark the referent of a pointer. */
    /* Return true iff it was unmarked before. */

#define ptr_proc_mark(proc)\
  bool proc(P2(void *, gc_state_t *))
    ptr_proc_mark((*mark));

    /* Relocate a pointer. */
    /* Note that the argument is const, but the */
    /* return value is not: this shifts the compiler */
    /* 'discarding const' warning from the call sites */
    /* (the reloc_ptr routines) to the implementations. */

#define ptr_proc_reloc(proc, typ)\
  typ *proc(P2(const typ *, gc_state_t *))
    ptr_proc_reloc((*reloc), void);

} gs_ptr_procs_t;
typedef const gs_ptr_procs_t *gs_ptr_type_t;

/* Define the pointer type for ordinary structure pointers. */
extern const gs_ptr_procs_t ptr_struct_procs;

#define ptr_struct_type (&ptr_struct_procs)

/* Define the pointer types for a pointer to a gs_[const_]string. */
extern const gs_ptr_procs_t ptr_string_procs;

#define ptr_string_type (&ptr_string_procs)
extern const gs_ptr_procs_t ptr_const_string_procs;

#define ptr_const_string_type (&ptr_const_string_procs)

/* Register a structure root. */
#define gs_register_struct_root(mem, root, pp, cname)\
  gs_register_root(mem, root, ptr_struct_type, pp, cname)

/*
 * Define the type for a GC root.
 */
typedef struct gs_gc_root_s gs_gc_root_t;
struct gs_gc_root_s {
    gs_gc_root_t *next;
    gs_ptr_type_t ptype;
    void **p;
};

#define public_st_gc_root_t()	/* in gsmemory.c */\
  gs_public_st_ptrs1(st_gc_root_t, gs_gc_root_t, "gs_gc_root_t",\
    gc_root_enum_ptrs, gc_root_reloc_ptrs, next)

/* Print a root debugging message. */
#define if_debug_root(c, msg, rp)\
  if_debug4(c, "%s 0x%lx: 0x%lx -> 0x%lx\n",\
	    msg, (ulong)(rp), (ulong)(rp)->p, (ulong)*(rp)->p)

/*
 * Define the memory manager procedural interface.
 */
typedef struct gs_memory_s gs_memory_t;
typedef struct gs_memory_procs_s {

    gs_raw_memory_procs(gs_memory_t);	/* defined in gsmemraw.h */

    /* Redefine inherited procedures with the new allocator type. */

#define gs_memory_proc_alloc_bytes(proc)\
  gs_memory_t_proc_alloc_bytes(proc, gs_memory_t)
#define gs_memory_proc_resize_object(proc)\
  gs_memory_t_proc_resize_object(proc, gs_memory_t)
#define gs_memory_proc_free_object(proc)\
  gs_memory_t_proc_free_object(proc, gs_memory_t)
#define gs_memory_proc_status(proc)\
  gs_memory_t_proc_status(proc, gs_memory_t)
#define gs_memory_proc_free_all(proc)\
  gs_memory_t_proc_free_all(proc, gs_memory_t)

    /*
     * Allocate possibly movable bytes.  (We inherit allocating immovable
     * bytes from the raw memory allocator.)
     */

#define gs_alloc_bytes(mem, nbytes, cname)\
  (*(mem)->procs.alloc_bytes)(mem, nbytes, cname)
    gs_memory_proc_alloc_bytes((*alloc_bytes));

    /*
     * Allocate a structure.
     */

#define gs_memory_proc_alloc_struct(proc)\
  void *proc(P3(gs_memory_t *mem, gs_memory_type_ptr_t pstype,\
    client_name_t cname))
#define gs_alloc_struct(mem, typ, pstype, cname)\
  (typ *)(*(mem)->procs.alloc_struct)(mem, pstype, cname)
    gs_memory_proc_alloc_struct((*alloc_struct));
#define gs_alloc_struct_immovable(mem, typ, pstype, cname)\
  (typ *)(*(mem)->procs.alloc_struct_immovable)(mem, pstype, cname)
    gs_memory_proc_alloc_struct((*alloc_struct_immovable));

    /*
     * Allocate an array of bytes.
     */

#define gs_memory_proc_alloc_byte_array(proc)\
  byte *proc(P4(gs_memory_t *mem, uint num_elements, uint elt_size,\
    client_name_t cname))
#define gs_alloc_byte_array(mem, nelts, esize, cname)\
  (*(mem)->procs.alloc_byte_array)(mem, nelts, esize, cname)
    gs_memory_proc_alloc_byte_array((*alloc_byte_array));
#define gs_alloc_byte_array_immovable(mem, nelts, esize, cname)\
  (*(mem)->procs.alloc_byte_array_immovable)(mem, nelts, esize, cname)
    gs_memory_proc_alloc_byte_array((*alloc_byte_array_immovable));

    /*
     * Allocate an array of structures.
     */

#define gs_memory_proc_alloc_struct_array(proc)\
  void *proc(P4(gs_memory_t *mem, uint num_elements,\
    gs_memory_type_ptr_t pstype, client_name_t cname))
#define gs_alloc_struct_array(mem, nelts, typ, pstype, cname)\
  (typ *)(*(mem)->procs.alloc_struct_array)(mem, nelts, pstype, cname)
    gs_memory_proc_alloc_struct_array((*alloc_struct_array));
#define gs_alloc_struct_array_immovable(mem, nelts, typ, pstype, cname)\
 (typ *)(*(mem)->procs.alloc_struct_array_immovable)(mem, nelts, pstype, cname)
    gs_memory_proc_alloc_struct_array((*alloc_struct_array_immovable));

    /*
     * Get the size of an object (anything except a string).
     */

#define gs_memory_proc_object_size(proc)\
  uint proc(P2(gs_memory_t *mem, const void *obj))
#define gs_object_size(mem, obj)\
  (*(mem)->procs.object_size)(mem, obj)
    gs_memory_proc_object_size((*object_size));

    /*
     * Get the type of an object (anything except a string).
     * The value returned for byte objects is useful only for
     * printing.
     */

#define gs_memory_proc_object_type(proc)\
  gs_memory_type_ptr_t proc(P2(gs_memory_t *mem, const void *obj))
#define gs_object_type(mem, obj)\
  (*(mem)->procs.object_type)(mem, obj)
    gs_memory_proc_object_type((*object_type));

    /*
     * Allocate a string (unaligned bytes).
     */

#define gs_memory_proc_alloc_string(proc)\
  byte *proc(P3(gs_memory_t *mem, uint nbytes, client_name_t cname))
#define gs_alloc_string(mem, nbytes, cname)\
  (*(mem)->procs.alloc_string)(mem, nbytes, cname)
    gs_memory_proc_alloc_string((*alloc_string));
#define gs_alloc_string_immovable(mem, nbytes, cname)\
  (*(mem)->procs.alloc_string_immovable)(mem, nbytes, cname)
    gs_memory_proc_alloc_string((*alloc_string_immovable));

    /*
     * Resize a string.
     */

#define gs_memory_proc_resize_string(proc)\
  byte *proc(P5(gs_memory_t *mem, byte *data, uint old_num, uint new_num,\
    client_name_t cname))
#define gs_resize_string(mem, data, oldn, newn, cname)\
  (*(mem)->procs.resize_string)(mem, data, oldn, newn, cname)
    gs_memory_proc_resize_string((*resize_string));

    /*
     * Free a string.
     */

#define gs_memory_proc_free_string(proc)\
  void proc(P4(gs_memory_t *mem, byte *data, uint nbytes,\
    client_name_t cname))
#define gs_free_string(mem, data, nbytes, cname)\
  (*(mem)->procs.free_string)(mem, data, nbytes, cname)
    gs_memory_proc_free_string((*free_string));

    /*
     * Register a root for the garbage collector.  root = NULL
     * asks the memory manager to allocate the root object
     * itself (immovable, in the manager's parent).
     */

#define gs_memory_proc_register_root(proc)\
  void proc(P5(gs_memory_t *mem, gs_gc_root_t *root, gs_ptr_type_t ptype,\
    void **pp, client_name_t cname))
#define gs_register_root(mem, root, ptype, pp, cname)\
  (*(mem)->procs.register_root)(mem, root, ptype, pp, cname)
    gs_memory_proc_register_root((*register_root));

    /*
     * Unregister a root.  Note that this can only be used with
     * roots that were allocated by the client.
     */

#define gs_memory_proc_unregister_root(proc)\
  void proc(P3(gs_memory_t *mem, gs_gc_root_t *root, client_name_t cname))
#define gs_unregister_root(mem, root, cname)\
  (*(mem)->procs.unregister_root)(mem, root, cname)
    gs_memory_proc_unregister_root((*unregister_root));

    /*
     * Enable or disable the freeing operations: when disabled,
     * these operations return normally but do nothing.  The
     * garbage collector and the PostScript interpreter
     * 'restore' operator need to temporarily disable the
     * freeing functions of (an) allocator(s) while running
     * finalization procedures.
     */

#define gs_memory_proc_enable_free(proc)\
  void proc(P2(gs_memory_t *mem, bool enable))
#define gs_enable_free(mem, enable)\
  (*(mem)->procs.enable_free)(mem, enable)
    gs_memory_proc_enable_free((*enable_free));

} gs_memory_procs_t;

/* Define no-op freeing procedures for use by enable_free. */
gs_memory_proc_free_object(gs_ignore_free_object);
gs_memory_proc_free_string(gs_ignore_free_string);

/*
 * Allocate a structure using a "raw memory" allocator.  Note that this does
 * not retain the identity of the structure.  Note also that it returns a
 * void *, and does not take the type of the returned pointer as a
 * parameter.
 */
void *gs_raw_alloc_struct_immovable(P3(gs_raw_memory_t * rmem,
				       gs_memory_type_ptr_t pstype,
				       client_name_t cname));

/*
 * Define an abstract allocator instance.
 * Subclasses may have state as well.
 */
#define gs_memory_common\
	gs_memory_procs_t procs
struct gs_memory_s {
    gs_memory_common;
};

#endif /* gsmemory_INCLUDED */
