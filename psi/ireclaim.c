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


/* Interpreter's interface to garbage collector */
#include "ghost.h"
#include "ierrors.h"
#include "gsstruct.h"
#include "iastate.h"
#include "icontext.h"
#include "interp.h"
#include "isave.h"		/* for isstate.h */
#include "isstate.h"		/* for mem->saved->state */
#include "dstack.h"		/* for dsbot, dsp, dict_set_top */
#include "estack.h"		/* for esbot, esp */
#include "ostack.h"		/* for osbot, osp */
#include "opdef.h"		/* for defining init procedure */
#include "store.h"		/* for make_array */

/* Import preparation and cleanup routines. */
extern void ialloc_gc_prepare(gs_ref_memory_t *);

/* Forward references */
static int gs_vmreclaim(gs_dual_memory_t *, bool);

/* Initialize the GC hook in the allocator. */
static int ireclaim(gs_dual_memory_t *, int);
static int
ireclaim_init(i_ctx_t *i_ctx_p)
{
    gs_imemory.reclaim = ireclaim;
    return 0;
}

/* GC hook called when the allocator signals a GC is needed (space = -1), */
/* or for vmreclaim (space = the space to collect). */
static int
ireclaim(gs_dual_memory_t * dmem, int space)
{
    bool global;
    gs_ref_memory_t *mem = NULL;
    int code;

    if (space < 0) {
        /* Determine which allocator exceeded the limit. */
        int i;

        for (i = 0; i < countof(dmem->spaces_indexed); i++) {
            mem = dmem->spaces_indexed[i];
            if (mem == 0)
                continue;
            if (mem->gc_status.requested > 0 ||
                ((gs_ref_memory_t *)mem->stable_memory)->gc_status.requested > 0
                )
                break;
        }
        if (!mem) {
            mem = dmem->space_global; /* just in case */
        }
    } else {
        mem = dmem->spaces_indexed[space >> r_space_shift];
    }
    if_debug3m('0', (gs_memory_t *)mem, "[0]GC called, space=%d, requestor=%d, requested=%ld\n",
               space, mem->space, (long)mem->gc_status.requested);
    global = mem->space != avm_local;
    /* Since dmem may move, reset the request now. */
    ialloc_reset_requested(dmem);
    code = gs_vmreclaim(dmem, global);
    if (code < 0)
        return code;
    ialloc_set_limit(mem);
    if (space < 0) {
        gs_memory_status_t stats;
        ulong allocated;

        /* If the ammount still allocated after the GC is complete */
        /* exceeds the max_vm setting, then return a VMerror       */
        gs_memory_status((gs_memory_t *) mem, &stats);
        allocated = stats.allocated;
        if (mem->stable_memory != (gs_memory_t *)mem) {
            gs_memory_status(mem->stable_memory, &stats);
            allocated += stats.allocated;
        }
        if (allocated >= mem->gc_status.max_vm) {
            /* We can't satisfy this request within max_vm. */
            return_error(gs_error_VMerror);
        }
    }
    return 0;
}

/* Interpreter entry to garbage collector. */
static int
gs_vmreclaim(gs_dual_memory_t *dmem, bool global)
{
    /* HACK: we know the gs_dual_memory_t is embedded in a context state. */
    i_ctx_t *i_ctx_p =
        (i_ctx_t *)((char *)dmem - offset_of(i_ctx_t, memory));
    gs_ref_memory_t *lmem = dmem->space_local;
    int code = context_state_store(i_ctx_p);
    gs_ref_memory_t *memories[5];
    gs_ref_memory_t *mem;
    int nmem, i;

    if (code < 0)
        return code;

    memories[0] = dmem->space_system;
    memories[1] = mem = dmem->space_global;
    nmem = 2;
    if (lmem != dmem->space_global)
        memories[nmem++] = lmem;
    for (i = nmem; --i >= 0;) {
        mem = memories[i];
        if (mem->stable_memory != (gs_memory_t *)mem)
            memories[nmem++] = (gs_ref_memory_t *)mem->stable_memory;
    }

    /****** ABORT IF code < 0 ******/
    for (i = nmem; --i >= 0; )
        alloc_close_clump(memories[i]);

    /* Prune the file list so it won't retain potentially collectible */
    /* files. */

    for (i = (global ? i_vm_system : i_vm_local);
         i < countof(dmem->spaces_indexed);
         ++i
         ) {
        gs_ref_memory_t *mem = dmem->spaces_indexed[i];

        if (mem == 0 || (i > 0 && mem == dmem->spaces_indexed[i - 1]))
            continue;
        if (mem->stable_memory != (gs_memory_t *)mem)
            ialloc_gc_prepare((gs_ref_memory_t *)mem->stable_memory);
        for (;; mem = &mem->saved->state) {
            ialloc_gc_prepare(mem);
            if (mem->saved == 0)
                break;
        }
    }

    /* Do the actual collection. */

    {
        void *ctxp = i_ctx_p;
        gs_gc_root_t context_root, *r = &context_root;

        gs_register_struct_root((gs_memory_t *)lmem, &r,
                                &ctxp, "i_ctx_p root");
        GS_RECLAIM(&dmem->spaces, global);
        gs_unregister_root((gs_memory_t *)lmem, r, "i_ctx_p root");
        i_ctx_p = ctxp;
        dmem = &i_ctx_p->memory;
    }

    /* Update caches not handled by context_state_load. */

    *systemdict = *ref_stack_index(&d_stack, ref_stack_count(&d_stack) - 1);

    /* Update the cached value pointers in names. */

    dicts_gc_cleanup();

    /* Reopen the active clumps. */

    for (i = 0; i < nmem; ++i)
        alloc_open_clump(memories[i]);

    /* Reload the context state.  Note this should be done
       AFTER the clumps are reopened, since the context state
       load could do allocations that must remain.
       If it were done while the clumps were still closed,
       we would lose those allocations when the clumps were opened */

    code = context_state_load(i_ctx_p);
    return code;
}

/* ------ Initialization procedure ------ */

const op_def ireclaim_l2_op_defs[] =
{
    op_def_end(ireclaim_init)
};
