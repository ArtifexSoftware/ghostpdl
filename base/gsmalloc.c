/* Copyright (C) 2001-2019 Artifex Software, Inc.
   All Rights Reserved.

   This software is provided AS-IS with no warranty, either express or
   implied.

   This software is distributed under license and may not be copied,
   modified or distributed except as expressly authorized under the terms
   of the license contained in the file LICENSE in this distribution.

   Refer to licensing information at http://www.artifex.com or contact
   Artifex Software, Inc.,  1305 Grant Avenue - Suite 200, Novato,
   CA 94945, U.S.A., +1(415)492-9861, for further information.
*/


/* C heap allocator */
#include "malloc_.h"
#include "gdebug.h"
#include "gserrors.h"
#include "gstypes.h"
#include "gsmemory.h"
#include "gsmdebug.h"
#include "gsstruct.h"		/* for st_bytes */
#include "gsmalloc.h"
#include "gsmemret.h"		/* retrying wrapper */

/* ------ Heap allocator ------ */

/*
 * An implementation of Ghostscript's memory manager interface
 * that works directly with the C heap.  We keep track of all allocated
 * blocks so we can free them at cleanup time.
 */
/* Raw memory procedures */
static gs_memory_proc_alloc_bytes(gs_heap_alloc_bytes);
static gs_memory_proc_resize_object(gs_heap_resize_object);
static gs_memory_proc_free_object(gs_heap_free_object);
static gs_memory_proc_stable(gs_heap_stable);
static gs_memory_proc_status(gs_heap_status);
static gs_memory_proc_free_all(gs_heap_free_all);

/* Object memory procedures */
static gs_memory_proc_alloc_struct(gs_heap_alloc_struct);
static gs_memory_proc_alloc_byte_array(gs_heap_alloc_byte_array);
static gs_memory_proc_alloc_struct_array(gs_heap_alloc_struct_array);
static gs_memory_proc_object_size(gs_heap_object_size);
static gs_memory_proc_object_type(gs_heap_object_type);
static gs_memory_proc_alloc_string(gs_heap_alloc_string);
static gs_memory_proc_resize_string(gs_heap_resize_string);
static gs_memory_proc_free_string(gs_heap_free_string);
static gs_memory_proc_register_root(gs_heap_register_root);
static gs_memory_proc_unregister_root(gs_heap_unregister_root);
static gs_memory_proc_enable_free(gs_heap_enable_free);
static gs_memory_proc_set_object_type(gs_heap_set_object_type);
static gs_memory_proc_defer_frees(gs_heap_defer_frees);
static const gs_memory_procs_t gs_malloc_memory_procs =
{
    /* Raw memory procedures */
    gs_heap_alloc_bytes,
    gs_heap_resize_object,
    gs_heap_free_object,
    gs_heap_stable,
    gs_heap_status,
    gs_heap_free_all,
    gs_ignore_consolidate_free,
    /* Object memory procedures */
    gs_heap_alloc_bytes,
    gs_heap_alloc_struct,
    gs_heap_alloc_struct,
    gs_heap_alloc_byte_array,
    gs_heap_alloc_byte_array,
    gs_heap_alloc_struct_array,
    gs_heap_alloc_struct_array,
    gs_heap_object_size,
    gs_heap_object_type,
    gs_heap_alloc_string,
    gs_heap_alloc_string,
    gs_heap_resize_string,
    gs_heap_free_string,
    gs_heap_register_root,
    gs_heap_unregister_root,
    gs_heap_enable_free,
    gs_heap_set_object_type,
    gs_heap_defer_frees
};

/* We must make sure that malloc_blocks leave the block aligned. */
/*typedef struct gs_malloc_block_s gs_malloc_block_t; */
#define malloc_block_data\
        gs_malloc_block_t *next;\
        gs_malloc_block_t *prev;\
        size_t size;\
        gs_memory_type_ptr_t type;\
        client_name_t cname
struct malloc_block_data_s {
    malloc_block_data;
};
struct gs_malloc_block_s {
    malloc_block_data;
/* ANSI C does not allow zero-size arrays, so we need the following */
/* unnecessary and wasteful workaround: */
#define _npad (-size_of(struct malloc_block_data_s) & (ARCH_ALIGN_MEMORY_MOD - 1))
    byte _pad[(_npad == 0 ? ARCH_ALIGN_MEMORY_MOD : _npad)];
#undef _npad
};

/* Initialize a malloc allocator. */
static long heap_available(void);
gs_malloc_memory_t *
gs_malloc_memory_init(void)
{
    gs_malloc_memory_t *mem =
        (gs_malloc_memory_t *)Memento_label(malloc(sizeof(gs_malloc_memory_t)), "gs_malloc_memory_t");

    if (mem == NULL)
        return NULL;

    mem->stable_memory = 0;	/* just for tidyness, never referenced */
    mem->procs = gs_malloc_memory_procs;
    mem->allocated = 0;
    mem->limit = max_size_t;
    mem->used = 0;
    mem->max_used = 0;
    mem->gs_lib_ctx = 0;
    mem->non_gc_memory = (gs_memory_t *)mem;
    mem->thread_safe_memory = (gs_memory_t *)mem;	/* this allocator is thread safe */
    /* Allocate a monitor to serialize access to structures within */
    mem->monitor = NULL;	/* prevent use during initial allocation */
    mem->monitor = gx_monitor_label(gx_monitor_alloc((gs_memory_t *)mem), "heap");
    if (mem->monitor == NULL) {
        free(mem);
        return NULL;
    }

    return mem;
}
/*
 * Estimate the amount of available memory by probing with mallocs.
 * We may under-estimate by a lot, but that's better than winding up with
 * a seriously inflated address space.  This is quite a hack!
 */
#define max_malloc_probes 20
#define malloc_probe_size 64000
static long
heap_available()
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
static byte *
gs_heap_alloc_bytes(gs_memory_t * mem, size_t size, client_name_t cname)
{
    gs_malloc_memory_t *mmem = (gs_malloc_memory_t *) mem;
    byte *ptr = 0;

#ifdef DEBUG
    const char *msg;
    static const char *const ok_msg = "OK";

#  define set_msg(str) (msg = (str))
#else
#  define set_msg(str) DO_NOTHING
#endif

        /* Exclusive acces so our decisions and changes are 'atomic' */
    if (mmem->monitor)
        gx_monitor_enter(mmem->monitor);
    if (size > mmem->limit - sizeof(gs_malloc_block_t)) {
        /* Definitely too large to allocate; also avoids overflow. */
        set_msg("exceeded limit");
    } else {
        size_t added = size + sizeof(gs_malloc_block_t);

        if (added <= size || added > mmem->limit || mmem->limit - added < mmem->used)
            set_msg("exceeded limit");
        else if ((ptr = (byte *) Memento_label(malloc(added), cname)) == 0)
            set_msg("failed");
        else {
            gs_malloc_block_t *bp = (gs_malloc_block_t *) ptr;

            /*
             * We would like to check that malloc aligns blocks at least as
             * strictly as the compiler (as defined by ARCH_ALIGN_MEMORY_MOD).
             * However, Microsoft VC 6 does not satisfy this requirement.
             * See gsmemory.h for more explanation.
             */
            set_msg(ok_msg);
            if (mmem->allocated)
                mmem->allocated->prev = bp;
            bp->next = mmem->allocated;
            bp->prev = 0;
            bp->size = size;
            bp->type = &st_bytes;
            bp->cname = cname;
            mmem->allocated = bp;
            ptr = (byte *) (bp + 1);
            mmem->used += size + sizeof(gs_malloc_block_t);
            if (mmem->used > mmem->max_used)
                mmem->max_used = mmem->used;
        }
    }
    if (mmem->monitor)
        gx_monitor_leave(mmem->monitor);	/* Done with exclusive access */
    /* We don't want to 'fill' under mutex to keep the window smaller */
    if (ptr)
        gs_alloc_fill(ptr, gs_alloc_fill_alloc, size);
#ifdef DEBUG
    if (gs_debug_c('a') || msg != ok_msg)
        dmlprintf6(mem, "[a+]gs_malloc(%s)(%"PRIuSIZE") = 0x%lx: %s, used=%ld, max=%ld\n",
                   client_name_string(cname), size, (ulong) ptr, msg, mmem->used, mmem->max_used);
#endif
    return ptr;
#undef set_msg
}
static void *
gs_heap_alloc_struct(gs_memory_t * mem, gs_memory_type_ptr_t pstype,
                     client_name_t cname)
{
    void *ptr =
    gs_heap_alloc_bytes(mem, gs_struct_type_size(pstype), cname);

    if (ptr == 0)
        return 0;
    ((gs_malloc_block_t *) ptr)[-1].type = pstype;
    return ptr;
}
static byte *
gs_heap_alloc_byte_array(gs_memory_t * mem, size_t num_elements, size_t elt_size,
                         client_name_t cname)
{
    size_t lsize = (size_t) num_elements * elt_size;

    if (elt_size != 0 && lsize/elt_size != num_elements)
        return 0;
    return gs_heap_alloc_bytes(mem, (size_t) lsize, cname);
}
static void *
gs_heap_alloc_struct_array(gs_memory_t * mem, size_t num_elements,
                           gs_memory_type_ptr_t pstype, client_name_t cname)
{
    void *ptr =
    gs_heap_alloc_byte_array(mem, num_elements,
                             gs_struct_type_size(pstype), cname);

    if (ptr == 0)
        return 0;
    ((gs_malloc_block_t *) ptr)[-1].type = pstype;
    return ptr;
}
static void *
gs_heap_resize_object(gs_memory_t * mem, void *obj, size_t new_num_elements,
                      client_name_t cname)
{
    gs_malloc_memory_t *mmem = (gs_malloc_memory_t *) mem;
    gs_malloc_block_t *ptr = (gs_malloc_block_t *) obj - 1;
    gs_memory_type_ptr_t pstype = ptr->type;
    size_t old_size = gs_object_size(mem, obj) + sizeof(gs_malloc_block_t);
    size_t new_size =
        gs_struct_type_size(pstype) * new_num_elements +
        sizeof(gs_malloc_block_t);
    gs_malloc_block_t *new_ptr;

    if (new_size == old_size)
        return obj;
    if (mmem->monitor)
        gx_monitor_enter(mmem->monitor);	/* Exclusive access */
    new_ptr = (gs_malloc_block_t *) gs_realloc(ptr, old_size, new_size);
    if (new_ptr == 0)
        return 0;
    if (new_ptr->prev)
        new_ptr->prev->next = new_ptr;
    else
        mmem->allocated = new_ptr;
    if (new_ptr->next)
        new_ptr->next->prev = new_ptr;
    new_ptr->size = new_size - sizeof(gs_malloc_block_t);
    mmem->used -= old_size;
    mmem->used += new_size;
    if (mmem->monitor)
        gx_monitor_leave(mmem->monitor);	/* Done with exclusive access */
    if (new_size > old_size)
        gs_alloc_fill((byte *) new_ptr + old_size,
                      gs_alloc_fill_alloc, new_size - old_size);
    return new_ptr + 1;
}
static size_t
gs_heap_object_size(gs_memory_t * mem, const void *ptr)
{
    return ((const gs_malloc_block_t *)ptr)[-1].size;
}
static gs_memory_type_ptr_t
gs_heap_object_type(const gs_memory_t * mem, const void *ptr)
{
    return ((const gs_malloc_block_t *)ptr)[-1].type;
}
static void
gs_heap_free_object(gs_memory_t * mem, void *ptr, client_name_t cname)
{
    gs_malloc_memory_t *mmem = (gs_malloc_memory_t *) mem;
    gs_malloc_block_t *bp;
    gs_memory_type_ptr_t pstype;
    struct_proc_finalize((*finalize));

    if_debug3m('a', mem, "[a-]gs_free(%s) 0x%lx(%"PRIuSIZE")\n",
               client_name_string(cname), (ulong) ptr,
               (ptr == 0 ? 0 : ((gs_malloc_block_t *) ptr)[-1].size));
    if (ptr == 0)
        return;
    pstype = ((gs_malloc_block_t *) ptr)[-1].type;
    finalize = pstype->finalize;
    if (finalize != 0) {
        if_debug3m('u', mem, "[u]finalizing %s 0x%lx (%s)\n",
                   struct_type_name_string(pstype),
                   (ulong) ptr, client_name_string(cname));
        (*finalize) (mem, ptr);
    }
    if (mmem->monitor)
        gx_monitor_enter(mmem->monitor);	/* Exclusive access */
    /* Previously, we used to search through every allocated block to find
     * the block we are freeing. This gives us safety in that an attempt to
     * free an unallocated block will not go wrong. This does radically
     * slow down frees though, so we replace it with this simpler code; we
     * now assume that the block is valid, and hence avoid the search.
     */
#if 1
    bp = &((gs_malloc_block_t *)ptr)[-1];
    if (bp->prev)
        bp->prev->next = bp->next;
    if (bp->next)
        bp->next->prev = bp->prev;
    if (bp == mmem->allocated) {
        mmem->allocated = bp->next;
        if (mmem->allocated)
            mmem->allocated->prev = NULL;
    }
    mmem->used -= bp->size + sizeof(gs_malloc_block_t);
    if (mmem->monitor)
        gx_monitor_leave(mmem->monitor);	/* Done with exclusive access */
    gs_alloc_fill(bp, gs_alloc_fill_free,
                  bp->size + sizeof(gs_malloc_block_t));
    free(bp);
#else
    bp = mmem->allocated; /* If 'finalize' releases a memory,
                             this function could be called recursively and
                             change mmem->allocated. */
    if (ptr == bp + 1) {
        mmem->allocated = bp->next;
        mmem->used -= bp->size + sizeof(gs_malloc_block_t);

        if (mmem->allocated)
            mmem->allocated->prev = 0;
        if (mmem->monitor)
            gx_monitor_leave(mmem->monitor);	/* Done with exclusive access */
        gs_alloc_fill(bp, gs_alloc_fill_free,
                      bp->size + sizeof(gs_malloc_block_t));
        free(bp);
    } else {
        gs_malloc_block_t *np;

        /*
         * bp == 0 at this point is an error, but we'd rather have an
         * error message than an invalid access.
         */
        if (bp) {
            for (; (np = bp->next) != 0; bp = np) {
                if (ptr == np + 1) {
                    bp->next = np->next;
                    if (np->next)
                        np->next->prev = bp;
                    mmem->used -= np->size + sizeof(gs_malloc_block_t);
                    if (mmem->monitor)
                        gx_monitor_leave(mmem->monitor);	/* Done with exclusive access */
                    gs_alloc_fill(np, gs_alloc_fill_free,
                                  np->size + sizeof(gs_malloc_block_t));
                    free(np);
                    return;
                }
            }
        }
        if (mmem->monitor)
            gx_monitor_leave(mmem->monitor);	/* Done with exclusive access */
        lprintf2("%s: free 0x%lx not found!\n",
                 client_name_string(cname), (ulong) ptr);
        free((char *)((gs_malloc_block_t *) ptr - 1));
    }
#endif
}
static byte *
gs_heap_alloc_string(gs_memory_t * mem, size_t nbytes, client_name_t cname)
{
    return gs_heap_alloc_bytes(mem, nbytes, cname);
}
static byte *
gs_heap_resize_string(gs_memory_t * mem, byte * data, size_t old_num, size_t new_num,
                      client_name_t cname)
{
    if (gs_heap_object_type(mem, data) != &st_bytes)
        lprintf2("%s: resizing non-string 0x%lx!\n",
                 client_name_string(cname), (ulong) data);
    return gs_heap_resize_object(mem, data, new_num, cname);
}
static void
gs_heap_free_string(gs_memory_t * mem, byte * data, size_t nbytes,
                    client_name_t cname)
{
    /****** SHOULD CHECK SIZE IF DEBUGGING ******/
    gs_heap_free_object(mem, data, cname);
}
static int
gs_heap_register_root(gs_memory_t * mem, gs_gc_root_t ** rp,
                      gs_ptr_type_t ptype, void **up, client_name_t cname)
{
    return 0;
}
static void
gs_heap_unregister_root(gs_memory_t * mem, gs_gc_root_t * rp,
                        client_name_t cname)
{
}
static gs_memory_t *
gs_heap_stable(gs_memory_t *mem)
{
    return mem;			/* heap memory is stable */
}

/*
 * NB: In a multi-threaded application, this is only a 'snapshot'
 *     since other threads may change the heap_status. The heap_available()
 *     probe is just an approximation anyway.
 */
static void
gs_heap_status(gs_memory_t * mem, gs_memory_status_t * pstat)
{
    gs_malloc_memory_t *mmem = (gs_malloc_memory_t *) mem;

    pstat->allocated = mmem->used + heap_available();
    pstat->used = mmem->used;
    pstat->max_used = mmem->max_used;
    pstat->is_thread_safe = true;	/* this allocator has a mutex (monitor) and IS thread safe */
}
static void
gs_heap_enable_free(gs_memory_t * mem, bool enable)
{
    if (enable)
        mem->procs.free_object = gs_heap_free_object,
            mem->procs.free_string = gs_heap_free_string;
    else
        mem->procs.free_object = gs_ignore_free_object,
            mem->procs.free_string = gs_ignore_free_string;
}

static void gs_heap_set_object_type(gs_memory_t *mem, void *ptr, gs_memory_type_ptr_t type)
{
    gs_malloc_block_t *bp = (gs_malloc_block_t *) ptr;

    if (ptr == 0)
        return;
    bp[-1].type = type;
}

static void gs_heap_defer_frees(gs_memory_t *mem, int defer)
{
}

/* Release all memory acquired by this allocator. */
static void
gs_heap_free_all(gs_memory_t * mem, uint free_mask, client_name_t cname)
{
    gs_malloc_memory_t *const mmem = (gs_malloc_memory_t *) mem;
    gx_monitor_t *mon = mmem->monitor;

    /*
     * We don't perform locking during this process since the 'monitor'
     * is contained in this allocator, and will get freed along the way.
     * It is only called at exit, and there better not be any threads
     * accessing this allocator.
     */
    mmem->monitor = NULL; 	/* delete reference to this monitor */
    gx_monitor_free(mon);	/* free the monitor */
#ifndef MEMENTO
    /* Normally gs calls this on closedown, and it frees every block that
     * has ever been allocated. This is not helpful for leak checking. */
    if (free_mask & FREE_ALL_DATA) {
        gs_malloc_block_t *bp = mmem->allocated;
        gs_malloc_block_t *np;

        for (; bp != 0; bp = np) {
            np = bp->next;
            if_debug3m('a', mem, "[a]gs_heap_free_all(%s) 0x%lx(%"PRIuSIZE")\n",
                       client_name_string(bp->cname), (ulong) (bp + 1),
                       bp->size);
            gs_alloc_fill(bp + 1, gs_alloc_fill_free, bp->size);
            free(bp);
        }
    }
#endif
    if (free_mask & FREE_ALL_ALLOCATOR)
        free(mem);
}

/* ------ Wrapping ------ */

/* Create the retrying and the locked wrapper for the heap allocator. */
int
gs_malloc_wrap(gs_memory_t **wrapped, gs_malloc_memory_t *contents)
{
#  ifdef USE_RETRY_MEMORY_WRAPPER
    /*
     * This is deprecated since 'retry' for clist reversion/cycling
     * will ONLY work for monochrome, simple PS or PCL, not for a
     * color device and not for PDF or XPS with transparency
     */
    {
        gs_memory_retrying_t *rmem;
        rmem = (gs_memory_retrying_t *)
            gs_alloc_bytes_immovable((gs_memory_t *)lmem,
                                     sizeof(gs_memory_retrying_t),
                                     "gs_malloc_wrap(retrying)");
        if (rmem == 0) {
            gs_free_object(cmem, lmem, "gs_malloc_wrap(locked)");
            return_error(gs_error_VMerror);
        }
        code = gs_memory_retrying_init(rmem, (gs_memory_t *)lmem);
        if (code < 0) {
            gs_free_object((gs_memory_t *)lmem, rmem, "gs_malloc_wrap(retrying)");
            gs_free_object(cmem, lmem, "gs_malloc_wrap(locked)");
            return code;
        }

        *wrapped = (gs_memory_t *)rmem;
    }
#  endif /* retrying */
    return 0;
}

/* Get the wrapped contents. */
gs_malloc_memory_t *
gs_malloc_wrapped_contents(gs_memory_t *wrapped)
{
#ifdef USE_RETRY_MEMORY_WRAPPER
    gs_memory_retrying_t *rmem = (gs_memory_retrying_t *)wrapped;

    return (gs_malloc_memory_t *)gs_memory_retrying_target(rmem);
#else /* retrying */
    return (gs_malloc_memory_t *)wrapped;
#endif /* retrying */
}

/* Free the wrapper, and return the wrapped contents. */
gs_malloc_memory_t *
gs_malloc_unwrap(gs_memory_t *wrapped)
{
#ifdef USE_RETRY_MEMORY_WRAPPER
    gs_memory_retrying_t *rmem = (gs_memory_retrying_t *)wrapped;
    gs_memory_t *contents = gs_memory_retrying_target(rmem);

    gs_free_object(wrapped rmem, "gs_malloc_unwrap(retrying)");
    return (gs_malloc_memory_t *)contents;
#else
    return (gs_malloc_memory_t *)wrapped;
#endif
}

/* Create the default allocator, and return it. */
gs_memory_t *
gs_malloc_init(void)
{
    return gs_malloc_init_with_context(NULL);
}

gs_memory_t *
gs_malloc_init_with_context(gs_lib_ctx_t *ctx)
{
    gs_malloc_memory_t *malloc_memory_default = gs_malloc_memory_init();
    gs_memory_t *memory_t_default;

    if (malloc_memory_default == NULL)
        return NULL;

    if (gs_lib_ctx_init(ctx, (gs_memory_t *)malloc_memory_default) != 0) {
        gs_malloc_release((gs_memory_t *)malloc_memory_default);
        return NULL;
    }

#if defined(USE_RETRY_MEMORY_WRAPPER)
    gs_malloc_wrap(&memory_t_default, malloc_memory_default);
#else
    memory_t_default = (gs_memory_t *)malloc_memory_default;
#endif
    memory_t_default->stable_memory = memory_t_default;
    return memory_t_default;
}

/* Release the default allocator. */
void
gs_malloc_release(gs_memory_t *mem)
{
    gs_malloc_memory_t * malloc_memory_default;

    if (mem == NULL)
        return;

#ifdef USE_RETRY_MEMORY_WRAPPER
    malloc_memory_default = gs_malloc_unwrap(mem);
#else
    malloc_memory_default = (gs_malloc_memory_t *)mem;
#endif
    gs_lib_ctx_fin((gs_memory_t *)malloc_memory_default);

    gs_malloc_memory_release(malloc_memory_default);
}
