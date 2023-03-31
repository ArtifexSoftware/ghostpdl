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


/* chunk consolidating wrapper on a base memory allocator */

/* This uses dual binary trees to handle the free list. One tree
 * holds the blocks in size order, one in location order. We use
 * a top-down semi-splaying access scheme on lookups and
 * insertions. */

#include "memory_.h"
#include "gx.h"
#include "gsstruct.h"
#include "gxobj.h"
#include "gsstype.h"
#include "gserrors.h"
#include "gsmchunk.h"
#include "gxsync.h"
#include "malloc_.h" /* For MEMENTO */
#include "assert_.h"
#include "gsmdebug.h"

/* Enable DEBUG_CHUNK to check the validity of the heap at every turn */
#undef DEBUG_CHUNK

/* Enable DEBUG_SEQ to keep sequence numbers in every block */
#undef DEBUG_SEQ

/* Enable DEBUG_CHUNK_PRINT to print the heap at every turn */
#undef DEBUG_CHUNK_PRINT

/* Enable DEBUG_CHUNK_PRINT_SLABS to list the slabs in the heap */
#undef DEBUG_CHUNK_PRINT_SLABS

#if defined(DEBUG_CHUNK_PRINT_SLABS) && !defined(DEBUG_CHUNK_PRINT)
#define DEBUG_CHUNK_PRINT
#endif

#if defined(DEBUG_CHUNK_PRINT) && !defined(DEBUG_CHUNK)
#define DEBUG_CHUNK
#endif

#if defined(DEBUG_CHUNK) && !defined(DEBUG)
#define DEBUG
#define CHUNK_FAKE_ASSERT
#endif

#ifdef DEBUG
#ifdef CHUNK_FAKE_ASSERT
#define CHUNK_ASSERT(M,A) gs_chunk_assert(M, A, #A)

static void gs_chunk_assert(gs_memory_t *m, int v, const char *s)
{
    void (*crash)(void);
    if (v)
        return;
    dmlprintf1(m, "Assert failed: %s\n", s);
    crash = NULL;
    crash();
}
#else
#define CHUNK_ASSERT(M,A) assert(A)
#endif
#endif

/* Raw memory procedures */
static gs_memory_proc_alloc_bytes(chunk_alloc_bytes_immovable);
static gs_memory_proc_resize_object(chunk_resize_object);
static gs_memory_proc_free_object(chunk_free_object);
static gs_memory_proc_stable(chunk_stable);
static gs_memory_proc_status(chunk_status);
static gs_memory_proc_free_all(chunk_free_all);
static gs_memory_proc_consolidate_free(chunk_consolidate_free);

/* Object memory procedures */
static gs_memory_proc_alloc_bytes(chunk_alloc_bytes);
static gs_memory_proc_alloc_struct(chunk_alloc_struct);
static gs_memory_proc_alloc_struct(chunk_alloc_struct_immovable);
static gs_memory_proc_alloc_byte_array(chunk_alloc_byte_array);
static gs_memory_proc_alloc_byte_array(chunk_alloc_byte_array_immovable);
static gs_memory_proc_alloc_struct_array(chunk_alloc_struct_array);
static gs_memory_proc_alloc_struct_array(chunk_alloc_struct_array_immovable);
static gs_memory_proc_object_size(chunk_object_size);
static gs_memory_proc_object_type(chunk_object_type);
static gs_memory_proc_alloc_string(chunk_alloc_string);
static gs_memory_proc_alloc_string(chunk_alloc_string_immovable);
static gs_memory_proc_resize_string(chunk_resize_string);
static gs_memory_proc_free_string(chunk_free_string);
static gs_memory_proc_register_root(chunk_register_root);
static gs_memory_proc_unregister_root(chunk_unregister_root);
static gs_memory_proc_enable_free(chunk_enable_free);
static gs_memory_proc_set_object_type(chunk_set_object_type);
static gs_memory_proc_defer_frees(chunk_defer_frees);
static const gs_memory_procs_t chunk_procs =
{
    /* Raw memory procedures */
    chunk_alloc_bytes_immovable,
    chunk_resize_object,
    chunk_free_object,
    chunk_stable,
    chunk_status,
    chunk_free_all,
    chunk_consolidate_free,
    /* Object memory procedures */
    chunk_alloc_bytes,
    chunk_alloc_struct,
    chunk_alloc_struct_immovable,
    chunk_alloc_byte_array,
    chunk_alloc_byte_array_immovable,
    chunk_alloc_struct_array,
    chunk_alloc_struct_array_immovable,
    chunk_object_size,
    chunk_object_type,
    chunk_alloc_string,
    chunk_alloc_string_immovable,
    chunk_resize_string,
    chunk_free_string,
    chunk_register_root,
    chunk_unregister_root,
    chunk_enable_free,
    chunk_set_object_type,
    chunk_defer_frees
};

typedef struct chunk_obj_node_s {
    gs_memory_type_ptr_t type;
#ifdef DEBUG_SEQ
    unsigned int sequence;
#endif
    struct chunk_obj_node_s *defer_next;
    size_t size; /* Actual size of block */
    size_t padding; /* Actual size - requested size */
} chunk_obj_node_t;

typedef struct chunk_free_node_s {
    struct chunk_free_node_s *left_loc;
    struct chunk_free_node_s *right_loc;
    struct chunk_free_node_s *left_size;
    struct chunk_free_node_s *right_size;
    size_t size;                  /* size of entire freelist block */
} chunk_free_node_t;

/*
 * Note: All objects within a chunk are 'aligned' since we round_up_to_align
 * the free list pointer when removing part of a free area.
 */
typedef struct chunk_slab_s {
    struct chunk_slab_s *next;
} chunk_slab_t;

typedef struct gs_memory_chunk_s {
    gs_memory_common;           /* interface outside world sees */
    gs_memory_t *target;        /* base allocator */
    chunk_slab_t *slabs;         /* list of slabs for freeing */
    chunk_free_node_t *free_size;/* free tree */
    chunk_free_node_t *free_loc; /* free tree */
    chunk_obj_node_t *defer_finalize_list;
    chunk_obj_node_t *defer_free_list;
    size_t used;
    size_t max_used;
    size_t total_free;
#ifdef DEBUG_SEQ
    unsigned int sequence;
#endif
    int deferring;
} gs_memory_chunk_t;

#define SIZEOF_ROUND_ALIGN(a) ROUND_UP(sizeof(a), obj_align_mod)

/* ---------- Public constructors/destructors ---------- */

/* Initialize a gs_memory_chunk_t */
int
gs_memory_chunk_wrap(gs_memory_t **wrapped,      /* chunk allocator init */
                    gs_memory_t  *target)       /* base allocator */
{
    /* Use the non-GC allocator of the target. */
    gs_memory_t       *non_gc_target = target->non_gc_memory;
    gs_memory_chunk_t *cmem = NULL;

    if (non_gc_target)
        cmem = (gs_memory_chunk_t *)gs_alloc_bytes_immovable(non_gc_target,
                                                            sizeof(gs_memory_chunk_t),
                                                            "gs_memory_chunk_wrap");
    if (cmem == NULL) {
        *wrapped = NULL;
        return_error(gs_error_VMerror);
    }
    cmem->stable_memory = (gs_memory_t *)cmem;	/* we are stable */
    cmem->procs = chunk_procs;
    cmem->gs_lib_ctx = non_gc_target->gs_lib_ctx;
    cmem->non_gc_memory = (gs_memory_t *)cmem;	/* and are not subject to GC */
    cmem->thread_safe_memory = non_gc_target->thread_safe_memory;
    cmem->target = non_gc_target;
    cmem->slabs = NULL;
    cmem->free_size = NULL;
    cmem->free_loc = NULL;
    cmem->used = 0;
    cmem->max_used = 0;
    cmem->total_free = 0;
#ifdef DEBUG_SEQ
    cmem->sequence = 0;
#endif
    cmem->deferring = 0;
    cmem->defer_finalize_list = NULL;
    cmem->defer_free_list = NULL;

#ifdef DEBUG_CHUNK_PRINT
    dmlprintf1(non_gc_target, "New chunk "PRI_INTPTR"\n", (intptr_t)cmem);
#endif

    /* Init the chunk management values */
    *wrapped = (gs_memory_t *)cmem;
    return 0;
}

/* Release a chunk memory manager. */
/* Note that this has no effect on the target. */
void
gs_memory_chunk_release(gs_memory_t *mem)
{
    gs_memory_free_all((gs_memory_t *)mem, FREE_ALL_EVERYTHING,
                       "gs_memory_chunk_release");
}

/* Release chunk memory manager, and return the target */
gs_memory_t * /* Always succeeds */
gs_memory_chunk_unwrap(gs_memory_t *mem)
{
    gs_memory_t *tmem;

    /* If this isn't a chunk, nothing to unwrap */
    if (mem->procs.status != chunk_status)
        return mem;

    tmem = ((gs_memory_chunk_t *)mem)->target;
    gs_memory_chunk_release(mem);

    return tmem;
}

/* ---------- Accessors ------------- */

/* Retrieve this allocator's target */
gs_memory_t *
gs_memory_chunk_target(const gs_memory_t *mem)
{
    gs_memory_chunk_t *cmem = (gs_memory_chunk_t *)mem;
    return cmem->target;
}

/* -------- Private members --------- */

/* Note that all of the data is 'immovable' and is opaque to the base allocator */
/* thus even if it is a GC type of allocator, no GC functions will be applied   */
/* All allocations are done in the target */

/* Procedures */

static void
chunk_mem_node_free_all_slabs(gs_memory_chunk_t *cmem)
{
    chunk_slab_t *slab, *next;
    gs_memory_t *const target = cmem->target;

    for (slab = cmem->slabs; slab != NULL; slab = next) {
        next = slab->next;
        gs_free_object(target, slab, "chunk_mem_node_free_all_slabs");
    }

    cmem->slabs = NULL;
    cmem->free_size = NULL;
    cmem->free_loc = NULL;
    cmem->total_free = 0;
    cmem->used = 0;
}

static void
chunk_free_all(gs_memory_t * mem, uint free_mask, client_name_t cname)
{
    gs_memory_chunk_t * const cmem = (gs_memory_chunk_t *)mem;
    gs_memory_t * const target = cmem->target;

    if (free_mask & FREE_ALL_DATA)
        chunk_mem_node_free_all_slabs(cmem);
    /* Only free the structures and the allocator itself. */
    if (mem->stable_memory) {
        if (mem->stable_memory != mem)
            gs_memory_free_all(mem->stable_memory, free_mask, cname);
        if (free_mask & FREE_ALL_ALLOCATOR)
            mem->stable_memory = 0;
    }
    if (free_mask & FREE_ALL_STRUCTURES) {
        cmem->target = 0;
    }
    if (free_mask & FREE_ALL_ALLOCATOR)
        gs_free_object(target, cmem, cname);
}

extern const gs_memory_struct_type_t st_bytes;

#ifdef DEBUG
static int dump_free_loc(gs_memory_t *mem, chunk_free_node_t *node, int depth, void **limit, uint *total)
{
#ifdef DEBUG_CHUNK_PRINT
    int i;
#endif
    int count;

    if (node == NULL)
        return 0;

    count = dump_free_loc(mem, node->left_loc, depth + 1 + (depth&1), limit, total);
    *total += node->size;
#ifdef DEBUG_CHUNK_PRINT
    if (depth != 0) {
        for (i = (depth-1)>>1; i != 0; i--)
            dmlprintf(mem, " ");
        if (depth & 1)
            dmlprintf(mem, "/");
        else
            dmlprintf(mem, "\\");
    }
    dmlprintf3(mem, PRI_INTPTR"+%x->"PRI_INTPTR"\n", (intptr_t)node, node->size, (intptr_t)((byte *)node)+node->size);
#endif
    CHUNK_ASSERT(mem, *limit < (void *)node);
    *limit = ((byte *)node)+node->size;
    return 1 + count + dump_free_loc(mem, node->right_loc, depth + 2 + (depth&1), limit, total);
}

static int dump_free_size(gs_memory_t *mem, chunk_free_node_t *node, int depth, uint *size, void **addr)
{
#ifdef DEBUG_CHUNK_PRINT
    int i;
#endif
    int count;

    if (node == NULL)
        return 0;

    count = dump_free_size(mem, node->left_size, depth + 1 + (depth&1), size, addr);
#ifdef DEBUG_CHUNK_PRINT
    if (depth != 0) {
        for (i = (depth-1)>>1; i != 0; i--)
            dmlprintf(mem, " ");
        if (depth & 1)
            dmlprintf(mem, "/");
        else
            dmlprintf(mem, "\\");
    }
    dmlprintf3(mem, PRI_INTPTR"+%x->"PRI_INTPTR"\n", (intptr_t)node, node->size, (intptr_t)((byte *)node)+node->size);
#endif
    CHUNK_ASSERT(mem, *size < node->size || (*size == node->size && *addr < (void *)node));
    *size = node->size;
    *addr = node;
    return 1 + count + dump_free_size(mem, node->right_size, depth + 2 + (depth&1), size, addr);
}

#ifdef DEBUG_CHUNK_PRINT
static size_t
largest_free_block(chunk_free_node_t *size)
{
    if (size == NULL)
        return 0;
    while (1) {
        if (size->right_size == NULL)
            return size->size;
        size = size->right_size;
    }
}
#endif

void
gs_memory_chunk_dump_memory(const gs_memory_t *mem)
{
    const gs_memory_chunk_t *cmem = (const gs_memory_chunk_t *)mem;
    int count1, count2;
    void *limit = NULL;
    void *addr = NULL;
    uint size = 1;
    uint total = 0;

#ifdef DEBUG_CHUNK_PRINT
    dmlprintf1(cmem->target, "Chunk "PRI_INTPTR":\n", (intptr_t)cmem);
    dmlprintf3(cmem->target, "Used=%"PRIxSIZE", Max Used=%"PRIxSIZE", Total Free=%"PRIxSIZE"\n", cmem->used, cmem->max_used, cmem->total_free);
    dmlprintf1(cmem->target, "Largest free block=%d bytes\n", largest_free_block(cmem->free_size));
#ifdef DEBUG_CHUNK_PRINT_SLABS
    {
        chunk_slab_t *slab;
        dmlprintf(cmem->target, "Slabs:\n");
        for (slab = cmem->slabs; slab != NULL; slab = slab->next)
            dmlprintf1(cmem->target, " "PRI_INTPTR"\n", (intptr_t)slab);
    }
#endif
    dmlprintf(cmem->target, "Locs:\n");
#endif
    count1 = dump_free_loc(cmem->target, cmem->free_loc, 0, &limit, &total);
#ifdef DEBUG_CHUNK_PRINT
    dmlprintf(cmem->target, "Sizes:\n");
#endif
    count2 = dump_free_size(cmem->target, cmem->free_size, 0, &size, &addr);
    if (count1 != count2) {
        void (*crash)(void) = NULL;
        dmlprintf2(cmem->target, "Tree mismatch! %d vs %d\n", count1, count2);
        crash();
    }
    if (total != cmem->total_free) {
        void (*crash)(void) = NULL;
        dmlprintf2(cmem->target, "Free size mismatch! %u vs %lu\n", total, cmem->total_free);
        crash();
    }
}
#endif

/* round up objects to make sure we have room for a header left */
inline static uint
round_up_to_align(uint size)
{
    uint num_node_headers = (size + SIZEOF_ROUND_ALIGN(chunk_obj_node_t) - 1) / SIZEOF_ROUND_ALIGN(chunk_obj_node_t);

    return num_node_headers * SIZEOF_ROUND_ALIGN(chunk_obj_node_t);
}

static inline int CMP_SIZE(const chunk_free_node_t * a, const chunk_free_node_t * b)
{
    if (a->size > b->size)
        return 1;
    if (a->size < b->size)
        return 0;
    return (a > b);
}

static void insert_free_size(gs_memory_chunk_t *cmem, chunk_free_node_t *node)
{
    chunk_free_node_t **ap;
    chunk_free_node_t *a, *b, *c;

    node->left_size  = NULL;
    node->right_size = NULL;

    /* Insert into size */
    ap = &cmem->free_size;
    while ((a = *ap) != NULL) {
        if (CMP_SIZE(a, node)) {
            b = a->left_size;
            if (b == NULL) {
                ap = &a->left_size;
                break; /* Stop searching */
            }
            if (CMP_SIZE(b, node)) {
                c = b->left_size;
                if (c == NULL) {
                    ap = &b->left_size;
                    break;
                }
                /* Splay:        a             c
                 *            b     Z   =>  W     b
                 *          c   Y               X   a
                 *         W X                     Y Z
                 */
                *ap = c;
                a->left_size  = b->right_size;
                b->left_size  = c->right_size;
                b->right_size = a;
                c->right_size = b;
                if (CMP_SIZE(c, node))
                    ap = &c->left_size;
                else
                    ap = &b->left_size;
            } else {
                c = b->right_size;
                if (c == NULL) {
                    ap = &b->right_size;
                    break;
                }
                /* Splay:         a             c
                 *            b       Z  =>   b   a
                 *          W   c            W X Y Z
                 *             X Y
                 */
                *ap = c;
                a->left_size  = c->right_size;
                b->right_size = c->left_size;
                c->left_size  = b;
                c->right_size = a;
                if (CMP_SIZE(c, node))
                    ap = &b->right_size;
                else
                    ap = &a->left_size;
            }
        } else {
            b = a->right_size;
            if (b == NULL)
            {
                ap = &a->right_size;
                break;
            }
            if (CMP_SIZE(b, node)) {
                c = b->left_size;
                if (c == NULL) {
                    ap = &b->left_size;
                    break;
                }
                /* Splay:      a                c
                 *         W       b    =>    a   b
                 *               c   Z       W X Y Z
                 *              X Y
                 */
                *ap = c;
                a->right_size = c->left_size;
                b->left_size  = c->right_size;
                c->left_size  = a;
                c->right_size = b;
                if (CMP_SIZE(c, node))
                    ap = &a->right_size;
                else
                    ap = &b->left_size;
            } else {
                c = b->right_size;
                if (c == NULL) {
                    ap = &b->right_size;
                    break;
                }
                /* Splay:    a                   c
                 *        W     b      =>     b     Z
                 *            X   c         a   Y
                 *               Y Z       W X
                 */
                *ap = c;
                a->right_size = b->left_size;
                b->right_size = c->left_size;
                b->left_size  = a;
                c->left_size  = b;
                if (CMP_SIZE(c, node))
                    ap = &b->right_size;
                else
                    ap = &c->right_size;
            }
        }
    }
    *ap = node;
}

static void insert_free_loc(gs_memory_chunk_t *cmem, chunk_free_node_t *node)
{
    chunk_free_node_t **ap;
    chunk_free_node_t *a, *b, *c;

    node->left_loc   = NULL;
    node->right_loc  = NULL;

    /* Insert into loc */
    ap = &cmem->free_loc;
    while ((a = *ap) != NULL) {
        if (a > node) {
            b = a->left_loc;
            if (b == NULL) {
                ap = &a->left_loc;
                break; /* Stop searching */
            }
            if (b > node) {
                c = b->left_loc;
                if (c == NULL) {
                    ap = &b->left_loc;
                    break;
                }
                /* Splay:        a             c
                 *            b     Z   =>  W     b
                 *          c   Y               X   a
                 *         W X                     Y Z
                 */
                *ap = c;
                a->left_loc  = b->right_loc;
                b->left_loc  = c->right_loc;
                b->right_loc = a;
                c->right_loc = b;
                if (c > node)
                    ap = &c->left_loc;
                else
                    ap = &b->left_loc;
            } else {
                c = b->right_loc;
                if (c == NULL) {
                    ap = &b->right_loc;
                    break;
                }
                /* Splay:         a             c
                 *            b       Z  =>   b   a
                 *          W   c            W X Y Z
                 *             X Y
                 */
                *ap = c;
                a->left_loc  = c->right_loc;
                b->right_loc = c->left_loc;
                c->left_loc  = b;
                c->right_loc = a;
                if (c > node)
                    ap = &b->right_loc;
                else
                    ap = &a->left_loc;
            }
        } else {
            b = a->right_loc;
            if (b == NULL)
            {
                ap = &a->right_loc;
                break;
            }
            if (b > node) {
                c = b->left_loc;
                if (c == NULL) {
                    ap = &b->left_loc;
                    break;
                }
                /* Splay:      a                c
                 *         W       b    =>    a   b
                 *               c   Z       W X Y Z
                 *              X Y
                 */
                *ap = c;
                a->right_loc = c->left_loc;
                b->left_loc  = c->right_loc;
                c->left_loc  = a;
                c->right_loc = b;
                if (c > node)
                    ap = &a->right_loc;
                else
                    ap = &b->left_loc;
            } else {
                c = b->right_loc;
                if (c == NULL) {
                    ap = &b->right_loc;
                    break;
                }
                /* Splay:    a                   c
                 *        W     b      =>     b     Z
                 *            X   c         a   Y
                 *               Y Z       W X
                 */
                *ap = c;
                a->right_loc = b->left_loc;
                b->right_loc = c->left_loc;
                b->left_loc  = a;
                c->left_loc  = b;
                if (c > node)
                    ap = &b->right_loc;
                else
                    ap = &c->right_loc;
            }
        }
    }
    *ap = node;
}

static void insert_free(gs_memory_chunk_t *cmem, chunk_free_node_t *node, uint size)
{
    node->size = size;
    insert_free_size(cmem, node);
    insert_free_loc(cmem, node);
}

static void remove_free_loc(gs_memory_chunk_t *cmem, chunk_free_node_t *node)
{
    chunk_free_node_t **ap = &cmem->free_loc;

    while (*ap != node)
    {
        if (*ap > node)
            ap = &(*ap)->left_loc;
        else
            ap = &(*ap)->right_loc;
    }

    if (node->left_loc == NULL)
        *ap = node->right_loc;
    else if (node->right_loc == NULL)
        *ap = node->left_loc;
    else {
        /* Find the in-order predecessor to node and use that to replace node */
        chunk_free_node_t **bp;
        chunk_free_node_t  *b;
        bp = &node->left_loc;
        while ((*bp)->right_loc)
            bp = &(*bp)->right_loc;
        b = (*bp);
        *bp = b->left_loc;
        b->left_loc = node->left_loc;
        b->right_loc = node->right_loc;
        *ap = b;
    }
}

static void remove_free_size(gs_memory_chunk_t *cmem, chunk_free_node_t *node)
{
    chunk_free_node_t **ap = &cmem->free_size;

    while (*ap != node)
    {
        if (CMP_SIZE(*ap, node))
            ap = &(*ap)->left_size;
        else
            ap = &(*ap)->right_size;
    }

    if (node->left_size == NULL)
        *ap = node->right_size;
    else if (node->right_size == NULL)
        *ap = node->left_size;
    else {
        /* Find the in-order predecessor to node and use that to replace node */
        chunk_free_node_t **bp;
        chunk_free_node_t  *b;
        bp = &node->left_size;
        while ((*bp)->right_size)
            bp = &(*bp)->right_size;
        b = (*bp);
        *bp = b->left_size;
        b->left_size = node->left_size;
        b->right_size = node->right_size;
        *ap = b;
    }
}

static void remove_free_size_fast(gs_memory_chunk_t *cmem, chunk_free_node_t **ap)
{
    chunk_free_node_t *node = *ap;

    if (node->left_size == NULL)
        *ap = node->right_size;
    else if (node->right_size == NULL)
        *ap = node->left_size;
    else {
        /* Find the in-order predecessor to node and use that to replace node */
        chunk_free_node_t **bp;
        chunk_free_node_t  *b;
        bp = &node->left_size;
        while ((*bp)->right_size)
            bp = &(*bp)->right_size;
        b = (*bp);
        *bp = b->left_size;
        b->left_size = node->left_size;
        b->right_size = node->right_size;
        *ap = b;
    }
}

static void remove_free(gs_memory_chunk_t *cmem, chunk_free_node_t *node)
{
    remove_free_loc(cmem, node);
    remove_free_size(cmem, node);
}

#if defined(MEMENTO) || defined(SINGLE_OBJECT_MEMORY_BLOCKS_ONLY)
#define SINGLE_OBJECT_CHUNK(size) (1)
#else
#define SINGLE_OBJECT_CHUNK(size) ((size) > (CHUNK_SIZE>>1))
#endif

/* All of the allocation routines reduce to this function */
static byte *
chunk_obj_alloc(gs_memory_t *mem, size_t size, gs_memory_type_ptr_t type, client_name_t cname)
{
    gs_memory_chunk_t  *cmem = (gs_memory_chunk_t *)mem;
    chunk_free_node_t **ap, **okp;
    chunk_free_node_t  *a, *b, *c;
    size_t newsize;
    chunk_obj_node_t *obj = NULL;

    newsize = round_up_to_align(size + SIZEOF_ROUND_ALIGN(chunk_obj_node_t));	/* space we will need */
    /* When we free this block it might have to go in free - so it had
     * better be large enough to accommodate a complete free node! */
    if (newsize < SIZEOF_ROUND_ALIGN(chunk_free_node_t))
        newsize = SIZEOF_ROUND_ALIGN(chunk_free_node_t);
    /* Protect against overflow */
    if (newsize < size)
        return NULL;

#ifdef DEBUG_SEQ
    cmem->sequence++;
#endif

#ifdef DEBUG_CHUNK_PRINT
#ifdef DEBUG_SEQ
    dmlprintf4(mem, "Event %x: malloc(chunk="PRI_INTPTR", size=%"PRIxSIZE", cname=%s)\n",
               cmem->sequence, (intptr_t)cmem, newsize, cname);
#else
    dmlprintf3(mem, "malloc(chunk="PRI_INTPTR", size=%"PRIxSIZE", cname=%s)\n",
               (intptr_t)cmem, newsize, cname);
#endif
#endif

    /* Large blocks are allocated directly */
    if (SINGLE_OBJECT_CHUNK(size)) {
        obj = (chunk_obj_node_t *)gs_alloc_bytes_immovable(cmem->target, newsize, cname);
        if (obj == NULL)
            return NULL;
    } else {
        /* Find the smallest free block that's large enough */
        /* okp points to the parent pointer to the block we pick */
        ap = &cmem->free_size;
        okp = NULL;
        while ((a = *ap) != NULL) {
            if (a->size >= newsize) {
                b = a->left_size;
                if (b == NULL) {
                    okp = ap; /* a will do */
                    break; /* Stop searching */
                }
                if (b->size >= newsize) {
                    c = b->left_size;
                    if (c == NULL) {
                        okp = &a->left_size; /* b is as good as we're going to get */
                        break;
                    }
                    /* Splay:        a             c
                     *            b     Z   =>  W     b
                     *          c   Y               X   a
                     *         W X                     Y Z
                     */
                    *ap = c;
                    a->left_size  = b->right_size;
                    b->left_size  = c->right_size;
                    b->right_size = a;
                    c->right_size = b;
                    if (c->size >= newsize) {
                        okp = ap; /* c is the best so far */
                        ap = &c->left_size;
                    } else {
                        okp = &c->right_size; /* b is the best so far */
                        ap = &b->left_size;
                    }
                } else {
                    c = b->right_size;
                    if (c == NULL) {
                        okp = ap; /* a is as good as we are going to get */
                        break;
                    }
                    /* Splay:         a             c
                     *            b       Z  =>   b   a
                     *          W   c            W X Y Z
                     *             X Y
                     */
                    *ap = c;
                    a->left_size  = c->right_size;
                    b->right_size = c->left_size;
                    c->left_size  = b;
                    c->right_size = a;
                    if (c->size >= newsize) {
                        okp = ap; /* c is the best so far */
                        ap = &b->right_size;
                    } else {
                        okp = &c->right_size; /* a is the best so far */
                        ap = &a->left_size;
                    }
                }
            } else {
                b = a->right_size;
                if (b == NULL)
                    break; /* No better match to be found */
                if (b->size >= newsize) {
                    c = b->left_size;
                    if (c == NULL) {
                        okp = &a->right_size; /* b is as good as we're going to get */
                        break;
                    }
                    /* Splay:      a                c
                     *         W       b    =>    a   b
                     *               c   Z       W X Y Z
                     *              X Y
                     */
                    *ap = c;
                    a->right_size = c->left_size;
                    b->left_size  = c->right_size;
                    c->left_size  = a;
                    c->right_size = b;
                    if (c->size >= newsize) {
                        okp = ap; /* c is the best so far */
                        ap = &a->right_size;
                    } else {
                        okp = &c->right_size; /* b is the best so far */
                        ap = &b->left_size;
                    }
                } else {
                    c = b->right_size;
                    if (c == NULL)
                        break; /* No better match to be found */
                    /* Splay:    a                   c
                     *        W     b      =>     b     Z
                     *            X   c         a   Y
                     *               Y Z       W X
                     */
                    *ap = c;
                    a->right_size = b->left_size;
                    b->right_size = c->left_size;
                    b->left_size  = a;
                    c->left_size  = b;
                    if (c->size >= newsize) {
                        okp = ap; /* c is the best so far */
                        ap = &b->right_size;
                    } else
                        ap = &c->right_size;
                }
            }
        }

        /* So *okp points to the most appropriate free tree entry. */

        if (okp == NULL) {
            /* No appropriate free space slot. We need to allocate a new slab. */
            chunk_slab_t *slab;
            uint slab_size = newsize + SIZEOF_ROUND_ALIGN(chunk_slab_t);

            if (slab_size <= (CHUNK_SIZE>>1))
                slab_size = CHUNK_SIZE;
            slab = (chunk_slab_t *)gs_alloc_bytes_immovable(cmem->target, slab_size, cname);
            if (slab == NULL)
                return NULL;
            slab->next = cmem->slabs;
            cmem->slabs = slab;

            obj = (chunk_obj_node_t *)(((byte *)slab) + SIZEOF_ROUND_ALIGN(chunk_slab_t));
            if (slab_size != newsize + SIZEOF_ROUND_ALIGN(chunk_slab_t)) {
                insert_free(cmem, (chunk_free_node_t *)(((byte *)obj)+newsize), slab_size - newsize - SIZEOF_ROUND_ALIGN(chunk_slab_t));
                cmem->total_free += slab_size - newsize - SIZEOF_ROUND_ALIGN(chunk_slab_t);
            }
        } else {
            chunk_free_node_t *ok = *okp;
            obj = (chunk_obj_node_t *)(void *)ok;
            if (ok->size >= newsize + SIZEOF_ROUND_ALIGN(chunk_free_node_t)) {
                chunk_free_node_t *tail = (chunk_free_node_t *)(((byte *)ok) + newsize);
                uint tail_size = ok->size - newsize;
                remove_free_size_fast(cmem, okp);
                remove_free_loc(cmem, ok);
                insert_free(cmem, tail, tail_size);
            } else {
                newsize = ok->size;
                remove_free_size_fast(cmem, okp);
                remove_free_loc(cmem, ok);
            }
            cmem->total_free -= newsize;
        }
    }

    if (gs_alloc_debug) {
        memset((byte *)(obj) + SIZEOF_ROUND_ALIGN(chunk_obj_node_t), 0xa1, newsize - SIZEOF_ROUND_ALIGN(chunk_obj_node_t));
        memset((byte *)(obj) + SIZEOF_ROUND_ALIGN(chunk_obj_node_t), 0xac, size);
    }

    cmem->used += newsize;
    obj->size = newsize; /* actual size */
    obj->padding = newsize - size; /* actual size - client requested size */
    obj->type = type;    /* and client desired type */
    obj->defer_next = NULL;

#ifdef DEBUG_SEQ
    obj->sequence = cmem->sequence;
#endif
    if (gs_debug_c('A'))
        dmlprintf3(mem, "[a+]chunk_obj_alloc (%s)(%"PRIuSIZE") = "PRI_INTPTR": OK.\n",
                   client_name_string(cname), size, (intptr_t) obj);
#ifdef DEBUG_CHUNK_PRINT
#ifdef DEBUG_SEQ
    dmlprintf5(mem, "Event %x: malloced(chunk="PRI_INTPTR", addr="PRI_INTPTR", size=%"PRIxSIZE", cname=%s)\n",
               obj->sequence, (intptr_t)cmem, (intptr_t)obj, obj->size, cname);
#else
    dmlprintf4(mem, "malloced(chunk="PRI_INTPTR", addr="PRI_INTPTR", size=%"PRIxSIZE", cname=%s)\n",
               (intptr_t)cmem, (intptr_t)obj, obj->size, cname);
#endif
#endif
#ifdef DEBUG_CHUNK
    gs_memory_chunk_dump_memory(cmem);
#endif

    return (byte *)Memento_label((byte *)(obj) + SIZEOF_ROUND_ALIGN(chunk_obj_node_t), cname);
}

static byte *
chunk_alloc_bytes_immovable(gs_memory_t * mem, size_t size, client_name_t cname)
{
    return chunk_obj_alloc(mem, size, &st_bytes, cname);
}

static byte *
chunk_alloc_bytes(gs_memory_t * mem, size_t size, client_name_t cname)
{
    return chunk_obj_alloc(mem, size, &st_bytes, cname);
}

static void *
chunk_alloc_struct_immovable(gs_memory_t * mem, gs_memory_type_ptr_t pstype,
                             client_name_t cname)
{
    return chunk_obj_alloc(mem, pstype->ssize, pstype, cname);
}

static void *
chunk_alloc_struct(gs_memory_t * mem, gs_memory_type_ptr_t pstype,
                   client_name_t cname)
{
    return chunk_obj_alloc(mem, pstype->ssize, pstype, cname);
}

static byte *
chunk_alloc_byte_array_immovable(gs_memory_t * mem, size_t num_elements,
                                 size_t elt_size, client_name_t cname)
{
    return chunk_alloc_bytes(mem, num_elements * elt_size, cname);
}

static byte *
chunk_alloc_byte_array(gs_memory_t * mem, size_t num_elements, size_t elt_size,
                   client_name_t cname)
{
    return chunk_alloc_bytes(mem, num_elements * elt_size, cname);
}

static void *
chunk_alloc_struct_array_immovable(gs_memory_t * mem, size_t num_elements,
                                   gs_memory_type_ptr_t pstype, client_name_t cname)
{
    return chunk_obj_alloc(mem, num_elements * pstype->ssize, pstype, cname);
}

static void *
chunk_alloc_struct_array(gs_memory_t * mem, size_t num_elements,
                         gs_memory_type_ptr_t pstype, client_name_t cname)
{
    return chunk_obj_alloc(mem, num_elements * pstype->ssize, pstype, cname);
}

static void *
chunk_resize_object(gs_memory_t * mem, void *ptr, size_t new_num_elements, client_name_t cname)
{
    void *new_ptr = NULL;

    if (ptr != NULL) {
        /* This isn't particularly efficient, but it is rarely used */
        chunk_obj_node_t *obj = (chunk_obj_node_t *)(((byte *)ptr) - SIZEOF_ROUND_ALIGN(chunk_obj_node_t));
        size_t new_size = (obj->type->ssize * new_num_elements);
        size_t old_size = obj->size - obj->padding;
        /* get the type from the old object */
        gs_memory_type_ptr_t type = obj->type;
        gs_memory_chunk_t *cmem = (gs_memory_chunk_t *)mem;
        size_t save_max_used = cmem->max_used;

        if (new_size == old_size)
            return ptr;
        if ((new_ptr = chunk_obj_alloc(mem, new_size, type, cname)) == 0)
            return NULL;
        memcpy(new_ptr, ptr, min(old_size, new_size));
        chunk_free_object(mem, ptr, cname);
        cmem->max_used = save_max_used;
        if (cmem->used > cmem->max_used)
            cmem->max_used = cmem->used;
    }

    return new_ptr;
}

static void
chunk_free_object(gs_memory_t *mem, void *ptr, client_name_t cname)
{
    gs_memory_chunk_t * const cmem = (gs_memory_chunk_t *)mem;
    size_t obj_node_size;
    chunk_obj_node_t *obj;
    struct_proc_finalize((*finalize));
    chunk_free_node_t **ap, **gtp, **ltp;
    chunk_free_node_t *a, *b, *c;

    if (ptr == NULL)
        return;

    /* back up to obj header */
    obj_node_size = SIZEOF_ROUND_ALIGN(chunk_obj_node_t);
    obj = (chunk_obj_node_t *)(((byte *)ptr) - obj_node_size);

    if (cmem->deferring) {
        if (obj->defer_next == NULL) {
            obj->defer_next = cmem->defer_finalize_list;
            cmem->defer_finalize_list = obj;
        }
        return;
    }

#ifdef DEBUG_CHUNK_PRINT
#ifdef DEBUG_SEQ
    cmem->sequence++;
    dmlprintf6(cmem->target, "Event %x: free(chunk="PRI_INTPTR", addr="PRI_INTPTR", size=%x, num=%x, cname=%s)\n",
               cmem->sequence, (intptr_t)cmem, (intptr_t)obj, obj->size, obj->sequence, cname);
#else
    dmlprintf4(cmem->target, "free(chunk="PRI_INTPTR", addr="PRI_INTPTR", size=%x, cname=%s)\n",
               (intptr_t)cmem, (intptr_t)obj, obj->size, cname);
#endif
#endif

    if (obj->type) {
        finalize = obj->type->finalize;
        if (finalize != NULL)
            finalize(mem, ptr);
    }
    /* finalize may change the head_**_chunk doing free of stuff */

    if_debug3m('A', cmem->target, "[a-]chunk_free_object(%s) "PRI_INTPTR"(%"PRIuSIZE")\n",
               client_name_string(cname), (intptr_t)ptr, obj->size);

    cmem->used -= obj->size;

    if (SINGLE_OBJECT_CHUNK(obj->size - obj->padding)) {
        gs_free_object(cmem->target, obj, "chunk_free_object(single object)");
#ifdef DEBUG_CHUNK
        gs_memory_chunk_dump_memory(cmem);
#endif
        return;
    }

    /* We want to find where to insert this free entry into our free tree. We need to know
     * both the point to the left of it, and the point to the right of it, in order to see
     * if we can merge the free entries. Accordingly, we search from the top of the tree
     * and keep pointers to the nodes that we pass that are greater than it, and less than
     * it. */
    gtp = NULL; /* gtp is set to the address of the pointer to the node where we last stepped left */
    ltp = NULL; /* ltp is set to the address of the pointer to the node where we last stepped right */
    ap = &cmem->free_loc;
    while ((a = *ap) != NULL) {
        if ((void *)a > (void *)obj) {
            b = a->left_loc; /* Try to step left from a */
            if (b == NULL) {
                gtp = ap; /* a is greater than us */
                break;
            }
            if ((void *)b > (void *)obj) {
                c = b->left_loc; /* Try to step left from b */
                if (c == NULL) {
                    gtp = &a->left_loc; /* b is greater than us */
                    break;
                }
                /* Splay:        a             c
                 *            b     Z   =>  W     b
                 *          c   Y               X   a
                 *         W X                     Y Z
                 */
                *ap = c;
                a->left_loc  = b->right_loc;
                b->left_loc  = c->right_loc;
                b->right_loc = a;
                c->right_loc = b;
                if ((void *)c > (void *)obj) { /* W */
                    gtp = ap; /* c is greater than us */
                    ap = &c->left_loc;
                } else { /* X */
                    gtp = &c->right_loc; /* b is greater than us */
                    ltp = ap; /* c is less than us */
                    ap = &b->left_loc;
                }
            } else {
                c = b->right_loc; /* Try to step right from b */
                if (c == NULL) {
                    gtp = ap; /* a is greater than us */
                    ltp = &a->left_loc; /* b is less than us */
                    break;
                }
                /* Splay:         a             c
                 *            b       Z  =>   b   a
                 *          W   c            W X Y Z
                 *             X Y
                 */
                *ap = c;
                a->left_loc  = c->right_loc;
                b->right_loc = c->left_loc;
                c->left_loc  = b;
                c->right_loc = a;
                if ((void *)c > (void *)obj) { /* X */
                    gtp = ap; /* c is greater than us */
                    ltp = &c->left_loc; /* b is less than us */
                    ap = &b->right_loc;
                } else { /* Y */
                    gtp = &c->right_loc; /* a is greater than us */
                    ltp = ap; /* c is less than us */
                    ap = &a->left_loc;
                }
            }
        } else {
            b = a->right_loc; /* Try to step right from a */
            if (b == NULL) {
                ltp = ap; /* a is less than us */
                break;
            }
            if ((void *)b > (void *)obj) {
                c = b->left_loc;
                if (c == NULL) {
                    gtp = &a->right_loc; /* b is greater than us */
                    ltp = ap; /* a is less than us */
                    break;
                }
                /* Splay:      a                c
                 *         W       b    =>    a   b
                 *               c   Z       W X Y Z
                 *              X Y
                 */
                *ap = c;
                a->right_loc = c->left_loc;
                b->left_loc  = c->right_loc;
                c->left_loc  = a;
                c->right_loc = b;
                if ((void *)c > (void *)obj) { /* X */
                    gtp = ap; /* c is greater than us */
                    ltp = &c->left_loc; /* a is less than us */
                    ap = &a->right_loc;
                } else { /* Y */
                    gtp = &c->right_loc; /* b is greater than us */
                    ltp = ap; /* c is less than than us */
                    ap = &b->left_loc;
                }
            } else {
                c = b->right_loc;
                if (c == NULL) {
                    ltp = &a->right_loc; /* b is greater than us */
                    break;
                }
                /* Splay:    a                   c
                 *        W     b      =>     b     Z
                 *            X   c         a   Y
                 *               Y Z       W X
                 */
                *ap = c;
                a->right_loc = b->left_loc;
                b->right_loc = c->left_loc;
                b->left_loc  = a;
                c->left_loc  = b;
                if ((void *)c > (void *)obj) { /* Y */
                    gtp = ap; /* c is greater than us */
                    ltp = &c->left_loc; /* b is less than us */
                    ap = &b->right_loc;
                } else { /* Z */
                    ltp = ap; /* c is less than than us */
                    ap = &c->right_loc;
                }
            }
        }
    }

    if (ltp) {
        /* There is at least 1 node smaller than us - check for merging */
        chunk_free_node_t *ltfree = (chunk_free_node_t *)(*ltp);
        if ((((byte *)ltfree) + ltfree->size) == (byte *)(void *)obj) {
            /* Merge! */
            cmem->total_free += obj->size;
            remove_free_size(cmem, ltfree);
            ltfree->size += obj->size;
            if (gtp) {
                /* There is at least 1 node greater than us - check for merging */
                chunk_free_node_t *gtfree = (chunk_free_node_t *)(*gtp);
                if ((((byte *)obj) + obj->size) == (byte *)(void *)gtfree) {
                    /* Double merge! */
                    ltfree->size += gtfree->size;
                    remove_free(cmem, gtfree);
                }
                gtp = NULL;
            }
            insert_free_size(cmem, ltfree);
            if (gs_alloc_debug)
                memset(((byte *)ltfree) + SIZEOF_ROUND_ALIGN(chunk_free_node_t), 0x69, ltfree->size - SIZEOF_ROUND_ALIGN(chunk_free_node_t));
            obj = NULL;
        }
    }
    if (gtp && obj) {
        /* There is at least 1 node greater than us - check for merging */
        chunk_free_node_t *gtfree = (chunk_free_node_t *)(*gtp);
        if ((((byte *)obj) + obj->size) == (byte *)(void *)gtfree) {
            /* Merge! */
            chunk_free_node_t *objfree = (chunk_free_node_t *)(void *)obj;
            uint obj_size = obj->size;
            cmem->total_free += obj_size;
            remove_free_size(cmem, gtfree);
            *objfree = *gtfree;
            objfree->size += obj_size;
            *gtp = objfree;
            insert_free_size(cmem, objfree);
            if (gs_alloc_debug)
                memset(((byte *)objfree) + SIZEOF_ROUND_ALIGN(chunk_free_node_t), 0x96, objfree->size - SIZEOF_ROUND_ALIGN(chunk_free_node_t));
            obj = NULL;
        }
    }

    if (obj) {
        /* Insert new one */
        chunk_free_node_t *objfree = (chunk_free_node_t *)(void *)obj;
        cmem->total_free += obj->size;
        objfree->size = obj->size;
        objfree->left_loc = NULL;
        objfree->right_loc = NULL;
        if (gtp) {
            ap = &(*gtp)->left_loc;
            while (*ap) {
                ap = &(*ap)->right_loc;
            }
        } else if (ltp) {
            ap = &(*ltp)->right_loc;
            while (*ap) {
                ap = &(*ap)->left_loc;
            }
        } else
            ap = &cmem->free_loc;
        *ap = objfree;
        insert_free_size(cmem, objfree);
        if (gs_alloc_debug)
            memset(((byte *)objfree) + SIZEOF_ROUND_ALIGN(chunk_free_node_t), 0x9b, objfree->size - SIZEOF_ROUND_ALIGN(chunk_free_node_t));
    }

#ifdef DEBUG_CHUNK
    gs_memory_chunk_dump_memory(cmem);
#endif
}

static byte *
chunk_alloc_string_immovable(gs_memory_t * mem, size_t nbytes, client_name_t cname)
{
    /* we just alloc bytes here */
    return chunk_alloc_bytes(mem, nbytes, cname);
}

static byte *
chunk_alloc_string(gs_memory_t * mem, size_t nbytes, client_name_t cname)
{
    /* we just alloc bytes here */
    return chunk_alloc_bytes(mem, nbytes, cname);
}

static byte *
chunk_resize_string(gs_memory_t * mem, byte * data, size_t old_num, size_t new_num,
                    client_name_t cname)
{
    /* just resize object - ignores old_num */
    return chunk_resize_object(mem, data, new_num, cname);
}

static void
chunk_free_string(gs_memory_t * mem, byte * data, size_t nbytes,
                  client_name_t cname)
{
    chunk_free_object(mem, data, cname);
}

static void
chunk_status(gs_memory_t * mem, gs_memory_status_t * pstat)
{
    gs_memory_chunk_t *cmem = (gs_memory_chunk_t *)mem;

    pstat->allocated = cmem->used;
    pstat->used = cmem->used - cmem->total_free;
    pstat->max_used = cmem->max_used;
    pstat->limit = (size_t)~1;      /* No limit on allocations */
    pstat->is_thread_safe = false;	/* this allocator does not have an internal mutex */
}

static gs_memory_t *
chunk_stable(gs_memory_t * mem)
{
    return mem;
}

static void
chunk_enable_free(gs_memory_t * mem, bool enable)
{
}

static void chunk_set_object_type(gs_memory_t *mem, void *ptr, gs_memory_type_ptr_t type)
{
    chunk_obj_node_t *obj = (chunk_obj_node_t *)(((byte *)ptr) - SIZEOF_ROUND_ALIGN(chunk_obj_node_t));

    if (ptr == 0)
        return;
    obj->type = type;
}

static void chunk_defer_frees(gs_memory_t *mem, int defer)
{
    gs_memory_chunk_t *cmem = (gs_memory_chunk_t *)mem;
    chunk_obj_node_t *n;

    if (defer == 0) {
        /* Run through and finalise everything on the finalize list. This
         * might cause other stuff to be put onto the finalize list. As we
         * finalize stuff we move it to the free list. */
        while (cmem->defer_finalize_list) {
            n = cmem->defer_finalize_list;
            cmem->defer_finalize_list = n->defer_next;
            if (n->type) {
                if (n->type->finalize)
                    n->type->finalize(mem, ((byte *)n) + SIZEOF_ROUND_ALIGN(chunk_obj_node_t));
                n->type = NULL;
            }
            n->defer_next = cmem->defer_free_list;
            cmem->defer_free_list = n;
        }
    }
    cmem->deferring = defer;
    if (defer == 0) {
        /* Now run through and free everything on the free list */
        while (cmem->defer_free_list) {
            n = cmem->defer_free_list;
            cmem->defer_free_list = n->defer_next;
            chunk_free_object(mem, ((byte *)n) + SIZEOF_ROUND_ALIGN(chunk_obj_node_t), "deferred free");
        }
    }
}

static void
chunk_consolidate_free(gs_memory_t *mem)
{
}

/* accessors to get size and type given the pointer returned to the client */
static size_t
chunk_object_size(gs_memory_t * mem, const void *ptr)
{
    if (ptr != NULL) {
        chunk_obj_node_t *obj = (chunk_obj_node_t *)(((byte *)ptr) - SIZEOF_ROUND_ALIGN(chunk_obj_node_t));

        return obj->size - obj->padding;
    }

    return 0;
}

static gs_memory_type_ptr_t
chunk_object_type(const gs_memory_t * mem, const void *ptr)
{
    chunk_obj_node_t *obj = (chunk_obj_node_t *)(((byte *)ptr) - SIZEOF_ROUND_ALIGN(chunk_obj_node_t));
    return obj->type;
}

static int
chunk_register_root(gs_memory_t * mem, gs_gc_root_t ** rp, gs_ptr_type_t ptype,
                 void **up, client_name_t cname)
{
    return 0;
}

static void
chunk_unregister_root(gs_memory_t * mem, gs_gc_root_t * rp, client_name_t cname)
{
}
