/* Copyright (C) 2001-2024 Artifex Software, Inc.
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


/* Memory allocator for Ghostscript interpreter */
#include "gx.h"
#include "memory_.h"
#include "gsexit.h"
#include "ierrors.h"
#include "gsstruct.h"
#include "iref.h"		/* must precede iastate.h */
#include "iastate.h"
#include "igc.h"		/* for gs_gc_reclaim */
#include "ipacked.h"
#include "iutil.h"
#include "ivmspace.h"
#include "store.h"

/*
 * Define global and local instances.
 */
public_st_gs_dual_memory();

/* Initialize the allocator */
int
ialloc_init(gs_dual_memory_t *dmem, gs_memory_t * rmem, uint clump_size,
            bool level2)
{
    gs_ref_memory_t *ilmem = ialloc_alloc_state(rmem, clump_size);
    gs_ref_memory_t *ilmem_stable = ialloc_alloc_state(rmem, clump_size);
    gs_ref_memory_t *igmem = 0;
    gs_ref_memory_t *igmem_stable = 0;
    gs_ref_memory_t *ismem = ialloc_alloc_state(rmem, clump_size);
    int i;

    if (ilmem == 0 || ilmem_stable == 0 || ismem == 0)
        goto fail;
    ilmem->stable_memory = (gs_memory_t *)ilmem_stable;
    if (level2) {
        igmem = ialloc_alloc_state(rmem, clump_size);
        igmem_stable = ialloc_alloc_state(rmem, clump_size);
        if (igmem == 0 || igmem_stable == 0)
            goto fail;
        igmem->stable_memory = (gs_memory_t *)igmem_stable;
    } else
        igmem = ilmem, igmem_stable = ilmem_stable;
    for (i = 0; i < countof(dmem->spaces_indexed); i++)
        dmem->spaces_indexed[i] = 0;
    dmem->space_local = ilmem;
    dmem->space_global = igmem;
    dmem->space_system = ismem;
    dmem->spaces.vm_reclaim = gs_gc_reclaim; /* real GC */
    dmem->reclaim = 0;		/* no interpreter GC yet */
    /* Level 1 systems have only local VM. */
    igmem->space = avm_global;
    igmem_stable->space = avm_global;
    ilmem->space = avm_local;	/* overrides if ilmem == igmem */
    ilmem_stable->space = avm_local; /* ditto */
    ismem->space = avm_system;
#   if IGC_PTR_STABILITY_CHECK
    igmem->space_id = (i_vm_global << 1) + 1;
    igmem_stable->space_id = i_vm_global << 1;
    ilmem->space_id = (i_vm_local << 1) + 1;	/* overrides if ilmem == igmem */
    ilmem_stable->space_id = i_vm_local << 1; /* ditto */
    ismem->space_id = (i_vm_system << 1);
#   endif
    ialloc_set_space(dmem, avm_global);
    return 0;
 fail:
    ialloc_free_state(igmem_stable);
    ialloc_free_state(igmem);
    ialloc_free_state(ismem);
    ialloc_free_state(ilmem_stable);
    ialloc_free_state(ilmem);
    return_error(gs_error_VMerror);
}

/* Free the allocator */
void
ialloc_finit(gs_dual_memory_t *mem)
{
    if (mem != NULL) {
        gs_ref_memory_t *ilmem = mem->space_local;
        gs_ref_memory_t *igmem = mem->space_global;
        gs_ref_memory_t *ismem = mem->space_system;

        if (ilmem != NULL) {
            gs_ref_memory_t *ilmem_stable = (gs_ref_memory_t *)(ilmem->stable_memory);
            gs_memory_free_all((gs_memory_t *)ilmem_stable, FREE_ALL_EVERYTHING, "ialloc_finit");
            gs_memory_free_all((gs_memory_t *)ilmem, FREE_ALL_EVERYTHING, "ialloc_finit");
        }

        if (igmem != NULL) {
            gs_ref_memory_t *igmem_stable = (gs_ref_memory_t *)(igmem->stable_memory);
            gs_memory_free_all((gs_memory_t *)igmem_stable, FREE_ALL_EVERYTHING, "ialloc_finit");
            gs_memory_free_all((gs_memory_t *)igmem, FREE_ALL_EVERYTHING, "ialloc_finit");
        }

        if (ismem != NULL)
            gs_memory_free_all((gs_memory_t *)ismem, FREE_ALL_EVERYTHING, "ialloc_finit");
     }
}

/* ================ Local/global VM ================ */

/* Get the space attribute of an allocator */
uint
imemory_space(const gs_ref_memory_t * iimem)
{
    return iimem->space;
}

/* Select the allocation space. */
void
ialloc_set_space(gs_dual_memory_t * dmem, uint space)
{
    gs_ref_memory_t *mem = dmem->spaces_indexed[space >> r_space_shift];

    dmem->current = mem;
    dmem->current_space = mem->space;
}

/* Get the l_new attribute of a current allocator. */
/* (A copy of the new_mask in the gs_dual_memory_t.) */
uint
imemory_new_mask(const gs_ref_memory_t *imem)
{
    return imem->new_mask;
}

/* Get the save level of an allocator. */
int
imemory_save_level(const gs_ref_memory_t *imem)
{
    return imem->save_level;
}

/* Reset the requests. */
void
ialloc_reset_requested(gs_dual_memory_t * dmem)
{
    dmem->space_system->gc_status.requested = 0;
    dmem->space_global->gc_status.requested = 0;
    dmem->space_local->gc_status.requested = 0;
}

/* ================ Refs ================ */

#ifdef DEBUG
static int
ialloc_trace_space(const gs_ref_memory_t *imem)
{
    return imem->space + (imem->stable_memory == (const gs_memory_t *)imem);
}
#endif

/* Register a ref root. */
int
gs_register_ref_root(gs_memory_t *mem, gs_gc_root_t **root,
                     void **pp, client_name_t cname)
{
    return gs_register_root(mem, root, ptr_ref_type, pp, cname);
}

/*
 * As noted in iastate.h, every run of refs has an extra ref at the end
 * to hold relocation information for the garbage collector;
 * since sizeof(ref) % obj_align_mod == 0, we never need to
 * allocate any additional padding space at the end of the block.
 */

/* Allocate an array of refs. */
int
gs_alloc_ref_array(gs_ref_memory_t * mem, ref * parr, uint attrs,
                   uint num_refs, client_name_t cname)
{
    ref *obj;
    int i;

    /* If we're allocating a run of refs already, */
    /* and we aren't about to overflow the maximum run length, use it. */
    if (mem->cc && mem->cc->has_refs == true && mem->cc->rtop == mem->cc->cbot &&
        num_refs < (mem->cc->ctop - mem->cc->cbot) / sizeof(ref) &&
        mem->cc->rtop - (byte *) mem->cc->rcur + num_refs * sizeof(ref) <
        max_size_st_refs
        ) {
        ref *end;

        obj = (ref *) mem->cc->rtop - 1;		/* back up over last ref */
        if_debug4m('A', (const gs_memory_t *)mem, "[a%d:+$ ]%s(%u) = "PRI_INTPTR"\n",
                   ialloc_trace_space(mem), client_name_string(cname),
                   num_refs, (intptr_t)obj);
        mem->cc->rcur[-1].o_size += num_refs * sizeof(ref);
        end = (ref *) (mem->cc->rtop = mem->cc->cbot +=
                       num_refs * sizeof(ref));
        make_mark(end - 1);
    } else {
        /*
         * Allocate a new run.  We have to distinguish 3 cases:
         *      - Same clump: cc unchanged, end == cc->cbot.
         *      - Large clump: cc unchanged, end != cc->cbot.
         *      - New clump: cc changed.
         */
        clump_t *cc = mem->cc;
        ref *end;
        alloc_change_t *cp = 0;
        int code = 0;

        if ((gs_memory_t *)mem != mem->stable_memory) {
            code = alloc_save_change_alloc(mem, "gs_alloc_ref_array", &cp);
            if (code < 0)
                return code;
        }
        obj = gs_alloc_struct_array((gs_memory_t *) mem, num_refs + 1,
                                    ref, &st_refs, cname);
        if (obj == 0) {
            /* We don't have to alloc_save_remove() because the change
               object hasn't been attached to the allocator yet.
             */
            gs_free_object((gs_memory_t *) mem, cp, "gs_alloc_ref_array");
            return_error(gs_error_VMerror);
        }
        /* Set the terminating ref now. */
        end = (ref *) obj + num_refs;
        make_mark(end);
        /* Set has_refs in the clump. */
        if (mem->cc && (mem->cc != cc || mem->cc->cbot == (byte *) (end + 1))) {
            /* Ordinary clump. */
            mem->cc->rcur = (obj_header_t *) obj;
            mem->cc->rtop = (byte *) (end + 1);
            mem->cc->has_refs = true;
        } else {
            /* Large clump. */
            /* This happens only for very large arrays, */
            /* so it doesn't need to be cheap. */
            clump_locator_t cl;

            cl.memory = mem;
            cl.cp = mem->root;
            /* clump_locate_ptr() should *never* fail here */
            if (clump_locate_ptr(obj, &cl)) {
                cl.cp->has_refs = true;
            }
            else {
                gs_abort((gs_memory_t *) mem);
            }
        }
        if (cp) {
            mem->changes = cp;
            cp->where = (ref_packed *)obj;
        }
    }
    for (i = 0; i < num_refs; i++) {
        make_null(&(obj[i]));
    }
    make_array(parr, attrs | mem->space, num_refs, obj);
    return 0;
}

/* Resize an array of refs.  Currently this is only implemented */
/* for shrinking, not for growing. */
int
gs_resize_ref_array(gs_ref_memory_t * mem, ref * parr,
                    uint new_num_refs, client_name_t cname)
{
    uint old_num_refs = r_size(parr);
    uint diff;
    ref *obj = parr->value.refs;

    if (new_num_refs > old_num_refs || !r_has_type(parr, t_array))
        return_error(gs_error_Fatal);
    diff = old_num_refs - new_num_refs;
    /* Check for LIFO.  See gs_free_ref_array for more details. */
    if (mem->cc && mem->cc->rtop == mem->cc->cbot &&
        (byte *) (obj + (old_num_refs + 1)) == mem->cc->rtop
        ) {
        /* Shorten the refs object. */
        ref *end = (ref *) (mem->cc->cbot = mem->cc->rtop -=
                            diff * sizeof(ref));

        if_debug4m('A', (const gs_memory_t *)mem, "[a%d:<$ ]%s(%u) "PRI_INTPTR"\n",
                   ialloc_trace_space(mem), client_name_string(cname), diff,
                   (intptr_t)obj);
        mem->cc->rcur[-1].o_size -= diff * sizeof(ref);
        make_mark(end - 1);
    } else {
        /* Punt. */
        if_debug4m('A', (const gs_memory_t *)mem, "[a%d:<$#]%s(%u) "PRI_INTPTR"\n",
                   ialloc_trace_space(mem), client_name_string(cname), diff,
                   (intptr_t)obj);
        mem->lost.refs += diff * sizeof(ref);
    }
    r_set_size(parr, new_num_refs);
    return 0;
}

/* Deallocate an array of refs.  Only do this if LIFO, or if */
/* the array occupies an entire clump by itself. */
void
gs_free_ref_array(gs_ref_memory_t * mem, ref * parr, client_name_t cname)
{
    uint num_refs = r_size(parr);
    ref *obj = parr->value.refs;

    /*
     * Compute the storage size of the array, and check for LIFO
     * freeing or a separate clump.  Note that the array might be packed;
     * for the moment, if it's anything but a t_array, punt.
     * The +1s are for the extra ref for the GC.
     */
    if (!r_has_type(parr, t_array))
        DO_NOTHING;		/* don't look for special cases */
    else if (mem->cc && mem->cc->rtop == mem->cc->cbot &&
             (byte *) (obj + (num_refs + 1)) == mem->cc->rtop
        ) {
        if ((obj_header_t *) obj == mem->cc->rcur) {
            /* Deallocate the entire refs object. */
            if ((gs_memory_t *)mem != mem->stable_memory)
                alloc_save_remove(mem, (ref_packed *)obj, "gs_free_ref_array");
            gs_free_object((gs_memory_t *) mem, obj, cname);
            mem->cc->rcur = 0;
            mem->cc->rtop = 0;
        } else {
            /* Deallocate it at the end of the refs object. */
            if_debug4m('A', (const gs_memory_t *)mem, "[a%d:-$ ]%s(%u) "PRI_INTPTR"\n",
                       ialloc_trace_space(mem), client_name_string(cname),
                       num_refs, (intptr_t)obj);
            mem->cc->rcur[-1].o_size -= num_refs * sizeof(ref);
            mem->cc->rtop = mem->cc->cbot = (byte *) (obj + 1);
            make_mark(obj);
        }
        return;
    } else if (num_refs >= (mem->large_size / ARCH_SIZEOF_REF - 1)) {
        /* See if this array has a clump all to itself. */
        /* We only make this check when freeing very large objects, */
        /* so it doesn't need to be cheap. */
        clump_locator_t cl;

        cl.memory = mem;
        cl.cp = mem->root;
        if (clump_locate_ptr(obj, &cl) &&
            obj == (ref *) ((obj_header_t *) (cl.cp->cbase) + 1) &&
            (byte *) (obj + (num_refs + 1)) == cl.cp->cend
            ) {
            /* Free the clump. */
            if_debug4m('a', (const gs_memory_t *)mem, "[a%d:-$L]%s(%u) "PRI_INTPTR"\n",
                       ialloc_trace_space(mem), client_name_string(cname),
                       num_refs, (intptr_t)obj);
            if ((gs_memory_t *)mem != mem->stable_memory) {
                alloc_save_remove(mem, (ref_packed *)obj, "gs_free_ref_array");
            }
            alloc_free_clump(cl.cp, mem);
            return;
        }
    }
    /* Punt, but fill the array with nulls so that there won't be */
    /* dangling references to confuse the garbage collector. */
    if_debug4m('A', (const gs_memory_t *)mem, "[a%d:-$#]%s(%u) "PRI_INTPTR"\n",
               ialloc_trace_space(mem), client_name_string(cname), num_refs,
               (intptr_t)obj);
    {
        uint size;

        switch (r_type(parr)) {
            case t_shortarray:
                size = num_refs * sizeof(ref_packed);
                break;
            case t_mixedarray:{
                /* We have to parse the array to compute the storage size. */
                uint i = 0;
                const ref_packed *p = parr->value.packed;

                for (; i < num_refs; ++i)
                    p = packed_next(p);
                size = (const byte *)p - (const byte *)parr->value.packed;
                break;
            }
            case t_array:
                size = num_refs * sizeof(ref);
                break;
            default:
                if_debug3('A', "Unknown type 0x%x in free_ref_array(%u,"PRI_INTPTR")!",
                         r_type(parr), num_refs, (intptr_t)obj);
                return;
        }
        /*
         * If there are any leftover packed elements, we don't
         * worry about them, since they can't be dangling references.
         */
        refset_null_new(obj, size / sizeof(ref), 0);
        mem->lost.refs += size;
    }
}

/* Allocate a string ref. */
int
gs_alloc_string_ref(gs_ref_memory_t * mem, ref * psref,
                    uint attrs, uint nbytes, client_name_t cname)
{
    byte *str = gs_alloc_string((gs_memory_t *) mem, nbytes, cname);

    if (str == 0)
        return_error(gs_error_VMerror);
    make_string(psref, attrs | mem->space, nbytes, str);
    return 0;
}
