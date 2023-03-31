/* Copyright (C) 2001-2023 Artifex Software, Inc.
   All Rights Reserved.

   This software is provided AS-IS with no warranty, either express or
   implied.

   This software is distributed under license and may not be copied,
   modified or distributed except as expressly authorized under the terms
   of the license contained in the file LICENSE in this distribution.

   Refer to licensing information at http://www.artifex.com or contact
   Artifex Software, Inc.,  39 Mesa Street, Suite 108A, San Francisco,
   CA 94129, USA, for further information.
*/


/* Structure definitions for standard allocator */
/* Requires gsmemory.h, gsstruct.h */

#ifndef gxalloc_INCLUDED
#  define gxalloc_INCLUDED

#include "gsalloc.h"
#include "gxobj.h"
#include "scommon.h"

/* ================ Clumps ================ */

/*
 * We obtain memory from the operating system in `clumps'.  A clump
 * may hold only a single large object (or string), or it may hold
 * many objects (allocated from the bottom up, always aligned)
 * and strings (allocated from the top down, not aligned).
 */

/*
 * Refs are allocated in the bottom-up section, along with struct objects.
 * In order to keep the overhead for refs small, we make consecutive
 * blocks of refs into a single allocator object of type st_refs.
 * To do this, we remember the start of the current ref object (if any),
 * and the end of the last block of allocated refs.  As long as
 * the latter is equal to the top of the allocated area, we can add
 * more refs to the current object; otherwise, we have to start a new one.
 * We assume that sizeof(ref) % obj_align_mod == 0; this means that if we
 * ever have to pad a block of refs, we never add as much as one entire ref.
 */

/*
 * When we do a save, we create a new 'inner' clump out of the remaining
 * space in the currently active clump.  Inner clumps must not be freed
 * by a restore.
 *
 * The garbage collector implements relocation for refs by scanning
 * forward to a free object.  Because of this, every ref object must end
 * with a dummy ref that can hold the relocation for the last block.
 * In order to put a reasonable upper bound on the scanning time, we
 * limit the length of the objects that contain runs of refs.
 */
#define max_size_st_refs (50 * sizeof(ref))

/*
 * Strings carry some additional overhead for use by the GC.
 * At the top of the clump is a table of relocation values for
 * 16N-character blocks of strings, where N is sizeof(uint).
 * This table is aligned, by adding padding above it if necessary.
 * Just below it is a mark table for the strings.  This table is also aligned,
 * to improve GC performance. The actual string data start below
 * the mark table.  These tables are not needed for a clump that holds
 * a single large (non-string) object, but they are needed for all other
 * clumps, including clumps created to hold a single large string.
 */

/*
 * Define the unit of data manipulation for marking strings.
 */
typedef uint string_mark_unit;

#define log2_sizeof_string_mark_unit ARCH_LOG2_SIZEOF_INT
/*
 * Define the quantum of relocation for strings, which determines
 * the quantum for reserving space.  This value must be a power of 2,
 * must be at least sizeof(string_mark_unit) * 8, and (because of the
 * unrolled loops in igcstr.c) currently must be equal to either 32 or 64.
 */
typedef uint string_reloc_offset;

#define log2_string_data_quantum (ARCH_LOG2_SIZEOF_INT + 4)
#define string_data_quantum (1 << log2_string_data_quantum)
/*
 * Define the quantum for reserving string space, including data,
 * marks, and relocation.
 */
#define string_space_quantum\
  (string_data_quantum + (string_data_quantum / 8) +\
   sizeof(string_reloc_offset))
/*
 * Compute the amount of space needed for a clump that holds only
 * a string of a given size.
 */
#define string_clump_space(nbytes)\
  (((nbytes) + (string_data_quantum - 1)) / string_data_quantum *\
   string_space_quantum)
/*
 * Compute the number of string space quanta in a given amount of storage.
 */
#define string_space_quanta(spacebytes)\
  ((spacebytes) / string_space_quantum)
/*
 * Compute the size of string marks for a given number of quanta.
 */
#define string_quanta_mark_size(nquanta)\
  ((nquanta) * (string_data_quantum / 8))
/*
 * Compute the size of the string freelists for a clump.
 */
#define STRING_FREELIST_SPACE(cp)\
  (((cp->climit - csbase(cp) + 255) >> 8) * sizeof(*cp->sfree1))

/*
 * To allow the garbage collector to combine clumps, we store in the
 * head of each clump the address to which its contents will be moved.
 */
/*typedef struct clump_head_s clump_head_t; *//* in gxobj.h */

/* Structure for a clump. */
typedef struct clump_s clump_t;
struct clump_s {
    clump_head_t *chead;	/* clump head, bottom of clump; */
    /* csbase is an alias for chead */
#define csbase(cp) ((byte *)(cp)->chead)
    /* Note that allocation takes place both from the bottom up */
    /* (aligned objects) and from the top down (strings). */
    byte *cbase;		/* bottom of clump data area */
    byte *int_freed_top;	/* top of most recent internal free area */
                                /* in clump (which may no longer be free), */
                                /* used to decide when to consolidate */
                                /* trailing free space in allocated area */
    byte *cbot;			/* bottom of free area */
                                /* (top of aligned objects) */
    obj_header_t *rcur;		/* current refs object, 0 if none */
    byte *rtop;			/* top of rcur */
    byte *ctop;			/* top of free area */
                                /* (bottom of strings) */
    byte *climit;		/* top of strings */
    byte *cend;			/* top of clump */
    clump_t *parent;            /* splay tree parent clump */
    clump_t *left;		/* splay tree left clump */
    clump_t *right;		/* splay tree right clump */
    clump_t *outer;		/* the clump of which this is */
                                /*   an inner clump, if any */
    uint inner_count;		/* number of clumps of which this is */
                                /*   the outer clump, if any */
    bool has_refs;		/* true if any refs in clump */
    bool c_alone;               /* this clump is for a single allocation */
    /*
     * Free lists for single bytes in blocks of 1 to 2*N-1 bytes, one per
     * 256 bytes in [csbase..climit), where N is sizeof(uint). The chain
     * pointer is a (1-byte) self-relative offset, terminated by a 0;
     * obviously, the chain is sorted by increasing address.  The free list
     * pointers themselves are offsets relative to csbase.
     *
     * Note that these lists overlay the GC relocation table, and that
     * sizeof(*sfree1) / 256 must be less than sizeof(string_reloc_offset) /
     * string_data_quantum (as real numbers).
     */
#define SFREE_NB 4		/* # of bytes for values on sfree list */
    uint *sfree1;
    /*
     * Free list for blocks of >= 2*N bytes.  Each block begins
     * with a N-byte size and a N-byte next block pointer,
     * both big-endian.  This too is sorted in increasing address order.
     */
    uint sfree;
    /* The remaining members are for the GC. */
    byte *odest;		/* destination for objects */
    byte *smark;		/* mark bits for strings */
    uint smark_size;
    byte *sbase;		/* base for computing smark offsets */
    string_reloc_offset *sreloc;	/* relocation for string blocks */
    byte *sdest;		/* destination for (top of) strings */
    byte *rescan_bot;		/* bottom of rescanning range if */
                                /* the GC mark stack overflows */
    byte *rescan_top;		/* top of range ditto */
};

/* The clump descriptor is exported only for isave.c. */
extern_st(st_clump);
#define public_st_clump()	/* in ialloc.c */\
  gs_public_st_ptrs3(st_clump, clump_t, "clump_t",\
    clump_enum_ptrs, clump_reloc_ptrs, left, right, parent)

/*
 * Macros for scanning a clump linearly, with the following schema:
 *      SCAN_CLUMP_OBJECTS(cp)                  << declares pre, size >>
 *              << code for all objects -- size not set yet >>
 *      DO_ALL
 *              << code for all objects -- size is set >>
 *      END_OBJECTS_SCAN
 *
 * NB on error END_OBJECTS_SCAN calls gs_abort in debug systems.
 */
#define SCAN_CLUMP_OBJECTS(cp)\
        {	obj_header_t *pre = (obj_header_t *)((cp)->cbase);\
                obj_header_t *end = (obj_header_t *)((cp)->cbot);\
                uint size;\
\
                for ( ; pre < end;\
                        pre = (obj_header_t *)((char *)pre + obj_size_round(size))\
                    )\
                {
#define DO_ALL\
                        size = pre_obj_contents_size(pre);\
                        {
#define END_OBJECTS_SCAN_NO_ABORT\
                        }\
                }\
        }
#ifdef DEBUG
#  define END_OBJECTS_SCAN\
                        }\
                }\
                if ( pre != end )\
                {	lprintf2("Clump parsing error, "PRI_INTPTR" != "PRI_INTPTR"\n",\
                                 (intptr_t)pre, (intptr_t)end);\
                    /*gs_abort((const gs_memory_t *)NULL);*/	\
                }\
        }
#else
#  define END_OBJECTS_SCAN END_OBJECTS_SCAN_NO_ABORT
#endif

/* Initialize a clump. */
/* This is exported for save/restore. */
void alloc_init_clump(clump_t *, byte *, byte *, bool, clump_t *);

/* Initialize the string freelists in a clump. */
void alloc_init_free_strings(clump_t *);

/* Find the clump for a pointer. */
/* Note that ptr_is_within_clump returns true even if the pointer */
/* is in an inner clump of the clump being tested. */
#define ptr_is_within_clump(ptr, cp)\
  PTR_BETWEEN((const byte *)(ptr), (cp)->cbase, (cp)->cend)
#define ptr_is_in_inner_clump(ptr, cp)\
  ((cp)->inner_count != 0 &&\
   PTR_BETWEEN((const byte *)(ptr), (cp)->cbot, (cp)->ctop))
#define ptr_is_in_clump(ptr, cp)\
  (ptr_is_within_clump(ptr, cp) && !ptr_is_in_inner_clump(ptr, cp))
typedef struct clump_locator_s {
    gs_ref_memory_t *memory;	/* for head & tail of chain */
    clump_t *cp;		/* one-element cache */
} clump_locator_t;
bool clump_locate_ptr(const void *, clump_locator_t *);
bool ptr_is_within_mem_clumps(const void *ptr, gs_ref_memory_t *mem);

#define clump_locate(ptr, clp)\
  (((clp)->cp != 0 && ptr_is_in_clump(ptr, (clp)->cp)) ||\
   clump_locate_ptr(ptr, clp))

/* Close up the current clump. */
/* This is exported for save/restore and for the GC. */
void alloc_close_clump(gs_ref_memory_t * mem);

/* Reopen the current clump after a GC. */
void alloc_open_clump(gs_ref_memory_t * mem);

/* Insert or remove a clump in the address-ordered chain. */
/* These are exported for the GC. */
void alloc_link_clump(clump_t *, gs_ref_memory_t *);
void alloc_unlink_clump(clump_t *, gs_ref_memory_t *);

/* Free a clump.  This is exported for save/restore and for the GC. */
void alloc_free_clump(clump_t *, gs_ref_memory_t *);

/* Print a clump debugging message. */
/* Unfortunately, the ANSI C preprocessor doesn't allow us to */
/* define the list of variables being printed as a macro. */
#define dprintf_clump_format\
  "%s "PRI_INTPTR" ("PRI_INTPTR".."PRI_INTPTR", "PRI_INTPTR".."PRI_INTPTR".."PRI_INTPTR")\n"
#define dmprintf_clump(mem, msg, cp)\
  dmprintf7(mem, dprintf_clump_format,\
            msg, (intptr_t)(cp), (intptr_t)(cp)->cbase, (intptr_t)(cp)->cbot,\
            (intptr_t)(cp)->ctop, (intptr_t)(cp)->climit, (intptr_t)(cp)->cend)
#define if_debug_clump(c, mem, msg, cp)\
  if_debug7m(c, mem,dprintf_clump_format,\
             msg, (intptr_t)(cp), (intptr_t)(cp)->cbase, (intptr_t)(cp)->cbot,\
             (intptr_t)(cp)->ctop, (intptr_t)(cp)->climit, (intptr_t)(cp)->cend)

/* ================ Allocator state ================ */

/* Structures for save/restore (not defined here). */
struct alloc_save_s;
struct alloc_change_s;

/*
 * Ref (PostScript object) type, only needed for the binary_token_names
 * member of the state.
 */
typedef struct ref_s ref;

/*
 * Define the number of freelists.  The index in the freelist array
 * is the ceiling of the size of the object contents (i.e., not including
 * the header) divided by obj_align_mod. There is an extra entry used to
 * keep a list of all free blocks > max_freelist_size.
 */
#define max_freelist_size 800	/* big enough for gstate & contents */
#define num_small_freelists\
  ((max_freelist_size + obj_align_mod - 1) / obj_align_mod + 1)
#define num_freelists (num_small_freelists + 1)

/*
 * Define the index of the freelist containing all free blocks >
 * max_freelist_size.
 */
#define LARGE_FREELIST_INDEX	num_small_freelists

/* Define the memory manager subclass for this allocator. */
struct gs_ref_memory_s {
    /* The following are set at initialization time. */
    gs_memory_common;
    size_t clump_size;
    size_t large_size;		/* min size to give large object */
                                /* its own clump: must be */
                                /* 1 mod obj_align_mod */
    uint space;			/* a_local, a_global, a_system */
#   if IGC_PTR_STABILITY_CHECK
    unsigned space_id:3; /* r_space_bits + 1 bit for "instability". */
#   endif
    /* Callers can change the following dynamically */
    /* (through a procedural interface). */
    gs_memory_gc_status_t gc_status;
    /* The following are updated dynamically. */
    bool is_controlled;		/* if true, this allocator doesn't manage */
                                /* its own clumps */
    size_t limit;		/* signal a VMerror when total */
                                /* allocated exceeds this */
    clump_t *root;		/* root of clump splay tree */
    clump_t *cc;		/* current clump */
    clump_locator_t cfreed;	/* clump where last object freed */
    size_t allocated;		/* total size of all clumps */
                                /* allocated at this save level */
    size_t gc_allocated;	/* value of (allocated + */
                                /* previous_status.allocated) after last GC */
    struct lost_ {		/* space freed and 'lost' (not put on a */
                                /* freelist) */
        size_t objects;
        size_t refs;
        size_t strings;
    } lost;
    /*
     * The following are for the interpreter's convenience: the
     * library initializes them as indicated and then never touches them.
     */
    int save_level;		/* # of saves with non-zero id */
    uint new_mask;		/* l_new or 0 (default) */
    uint test_mask;		/* l_new or ~0 (default) */
    stream *streams;		/* streams allocated at current level */
    ref *names_array;		/* system_names or user_names, if needed */
    /* Garbage collector information */
    gs_gc_root_t *roots;	/* roots for GC */
    /* Sharing / saved state information */
    int num_contexts;		/* # of contexts sharing this VM */
    struct alloc_change_s *changes;
    struct alloc_change_s *scan_limit;
    struct alloc_save_s *saved;
    long total_scanned;
    long total_scanned_after_compacting;
    struct alloc_save_s *reloc_saved;	/* for GC */
    gs_memory_status_t previous_status;		/* total allocated & used */
                                /* in outer save levels */
    size_t largest_free_size;	/* largest (aligned) size on large block list */
    /* We put the freelists last to keep the scalar offsets small. */
    obj_header_t *freelists[num_freelists];
};

/* The descriptor for gs_ref_memory_t is exported only for */
/* the alloc_save_t subclass; otherwise, it should be private. */
extern_st(st_ref_memory);
#define public_st_ref_memory()	/* in gsalloc.c */\
  gs_public_st_composite(st_ref_memory, gs_ref_memory_t,\
    "gs_ref_memory", ref_memory_enum_ptrs, ref_memory_reloc_ptrs)
#define st_ref_memory_max_ptrs 5  /* streams, names_array, changes, scan_limit, saved */

/* Define the procedures for the standard allocator. */
/* We export this for subclasses. */
extern const gs_memory_procs_t gs_ref_memory_procs;

/*
 * Scan the clumps of an allocator:
 *      SCAN_MEM_CLUMPS(mem, cp)
 *              << code to process clump cp >>
 *      END_CLUMPS_SCAN
 */
#define SCAN_MEM_CLUMPS(mem, cp)\
        {	clump_splay_walker sw;\
                clump_t *cp;\
                for (cp = clump_splay_walk_init(&sw, mem); cp != 0; cp = clump_splay_walk_fwd(&sw))\
                {
#define END_CLUMPS_SCAN\
                }\
        }

/* ================ Debugging ================ */

#ifdef DEBUG

/*
 * Define the options for a memory dump.  These may be or'ed together.
 */
typedef enum {
    dump_do_default = 0,	/* pro forma */
    dump_do_strings = 1,
    dump_do_type_addresses = 2,
    dump_do_no_types = 4,
    dump_do_pointers = 8,
    dump_do_pointed_strings = 16,	/* only if do_pointers also set */
    dump_do_contents = 32,
    dump_do_marks = 64
} dump_options_t;

/*
 * Define all the parameters controlling what gets dumped.
 */
typedef struct dump_control_s {
    dump_options_t options;
    const byte *bottom;
    const byte *top;
} dump_control_t;

/* Define the two most useful dump control structures. */
extern const dump_control_t dump_control_default;
extern const dump_control_t dump_control_all;

/* ------ Procedures ------ */

/* Print one object with the given options. */
/* Relevant options: type_addresses, no_types, pointers, pointed_strings, */
/* contents. */
void debug_print_object(const gs_memory_t *mem, const void *obj, const dump_control_t * control);

/* Print the contents of a clump with the given options. */
/* Relevant options: all. */
void debug_dump_clump(const gs_memory_t *mem, const clump_t * cp, const dump_control_t * control);
void debug_print_clump(const gs_memory_t *mem, const clump_t * cp);	/* default options */

/* Print the contents of all clumps managed by an allocator. */
/* Relevant options: all. */
void debug_dump_memory(const gs_ref_memory_t *mem,
                       const dump_control_t *control);

/* convenience routine to call debug_dump_memory(), defaults
   dump_control_t to dump_control_all.  */
void debug_dump_allocator(const gs_ref_memory_t *mem);

/* Find all the objects that contain a given pointer. */
void debug_find_pointers(const gs_ref_memory_t *mem, const void *target);

/* Dump the splay tree of clumps */
void debug_dump_clump_tree(const gs_ref_memory_t *mem);

#endif /* DEBUG */

/* Routines for walking/manipulating the splay tree of clumps */
typedef enum {
    SPLAY_APP_CONTINUE = 0,
    SPLAY_APP_STOP = 1
} splay_app_result_t;

/* Apply function 'fn' to every node in the clump splay tree.
 * These are applied in depth first order, which means that
 * 'fn' is free to perform any operation it likes on the subtree
 * rooted at the current node, as long as it doesn't affect
 * any nodes higher in the tree than the current node. The
 * classic example of this is deletion, which can cause the
 * subtree to be rrarranged. */
clump_t *clump_splay_app(clump_t *root, gs_ref_memory_t *imem, splay_app_result_t (*fn)(clump_t *, void *), void *arg);

typedef enum
{
    /* As we step, keep track of where we just stepped from. */
    SPLAY_FROM_ABOVE = 0,
    SPLAY_FROM_LEFT = 1,
    SPLAY_FROM_RIGHT = 2
} splay_dir_t;

/* Structure to contain details of a 'walker' for the clump
 * splay tree. Treat this as opaque. Only defined at this
 * public level to avoid the need for mallocs.
 * No user servicable parts inside. */
typedef struct
{
    /* The direction we most recently moved in the
     * splay tree traversal. */
    splay_dir_t from;
    /* The current node in the splay tree traversal. */
    clump_t *cp;
    /* The node that marks the end of the splay tree traversal.
     * (NULL for top of tree). */
    clump_t *end;
} clump_splay_walker;

/* Prepare a splay walker for walking forwards.
 * It will perform an in-order traversal of the entire tree,
 * starting at min and stopping after max with NULL. */
clump_t *clump_splay_walk_init(clump_splay_walker *sw, const gs_ref_memory_t *imem);

/* Prepare a splay walker for walking forwards,
 * starting from a point somewhere in the middle of the tree.
 * The traveral starts at cp, proceeds in-order to max, then
 * restarts at min, stopping with NULL after reaching mid again.
 */
clump_t *clump_splay_walk_init_mid(clump_splay_walker *sw, clump_t *cp);

/* Return the next node in the traversal of the clump
 * splay tree as set up by clump_splay_walk_init{,_mid}.
 * Will return NULL at the end point. */
clump_t *clump_splay_walk_fwd(clump_splay_walker *sw);

/* Prepare a splay walker for walking backwards.
 * It will perform a reverse-in-order traversal of the
 * entire tree, starting at max, and stopping after min
 * with NULL. */
clump_t *clump_splay_walk_bwd_init(clump_splay_walker *sw, const gs_ref_memory_t *imem);

/* Return the next node in the traversal of the clump
 * splay tree as set up by clump_splay_walk_bwd_init.
 * Will return NULL at the end point. */
clump_t *clump_splay_walk_bwd(clump_splay_walker *sw);

#endif /* gxalloc_INCLUDED */
