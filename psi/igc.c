/* Copyright (C) 2001-2025 Artifex Software, Inc.
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


/* Garbage collector for Ghostscript */
#include "memory_.h"
#include "ghost.h"
#include "ierrors.h"
#include "gsexit.h"
#include "gsmdebug.h"
#include "gsstruct.h"
#include "iastate.h"
#include "isave.h"
#include "isstate.h"
#include "idict.h"
#include "ipacked.h"
#include "istruct.h"
#include "igc.h"
#include "igcstr.h"
#include "inamedef.h"
#include "opdef.h"		/* for marking oparray names */

/* Define whether to force all garbage collections to be global. */
static const bool I_FORCE_GLOBAL_GC = false;

/* Define whether to bypass the collector entirely. */
static const bool I_BYPASS_GC = false;

/* Define an entry on the mark stack. */
typedef struct {
    void *ptr;
    uint index;
    bool is_refs;
} ms_entry;

/* Define (a segment of) the mark stack. */
/* entries[0] has ptr = 0 to indicate the bottom of the stack. */
/* count additional entries follow this structure. */
typedef struct gc_mark_stack_s gc_mark_stack;
struct gc_mark_stack_s {
    gc_mark_stack *prev;
    gc_mark_stack *next;
    uint count;
    bool on_heap;		/* if true, allocated during GC */
    ms_entry entries[1];
};

/* Define the mark stack sizing parameters. */
#define ms_size_default 100	/* default, allocated on C stack */
/* This should probably be defined as a parameter somewhere.... */
#define ms_size_desired		/* for additional allocation */\
 ((max_ushort - sizeof(gc_mark_stack)) / sizeof(ms_entry) - 10)
#define ms_size_min 50		/* min size for segment in free block */

/* Forward references */

static void gc_init_mark_stack(gc_mark_stack *, uint);
static void gc_objects_clear_marks(const gs_memory_t *mem, clump_t *);
static void gc_unmark_names(name_table *, op_array_table *, op_array_table *);
static int gc_trace(gs_gc_root_t *, gc_state_t *, gc_mark_stack *);
static int gc_rescan_clump(clump_t *, gc_state_t *, gc_mark_stack *);
static int gc_trace_clump(const gs_memory_t *mem, clump_t *, gc_state_t *, gc_mark_stack *);
static bool gc_trace_finish(gc_state_t *);
static void gc_clear_reloc(clump_t *);
static void gc_objects_set_reloc(gc_state_t * gcst, clump_t *);
static void gc_do_reloc(clump_t *, gs_ref_memory_t *, gc_state_t *);
static void gc_objects_compact(clump_t *, gc_state_t *);
static void gc_free_empty_clumps(gs_ref_memory_t *);

/* Forward references for pointer types */
static ptr_proc_unmark(ptr_struct_unmark);
static ptr_proc_mark(ptr_struct_mark);
static ptr_proc_unmark(ptr_string_unmark);
static ptr_proc_mark(ptr_string_mark);
static ptr_proc_unmark(ptr_name_index_unmark);
static ptr_proc_mark(ptr_name_index_mark);
/*ptr_proc_unmark(ptr_ref_unmark); *//* in igc.h */
/*ptr_proc_mark(ptr_ref_mark); *//* in igc.h */
static ptr_proc_reloc(igc_reloc_struct_ptr, void);

ptr_proc_reloc(igc_reloc_ref_ptr, ref_packed);	/* in igcref.c */
refs_proc_reloc(igc_reloc_refs);	/* in igcref.c */

/* Define this GC's procedure vector. */
static const gc_procs_with_refs_t igc_procs = {
    igc_reloc_struct_ptr, igc_reloc_string, igc_reloc_const_string,
    igc_reloc_param_string, igc_reloc_ref_ptr, igc_reloc_refs
};

/* Pointer type descriptors. */
/* Note that the trace/mark routine has special knowledge of ptr_ref_type */
/* and ptr_struct_type -- it assumes that no other types have embedded */
/* pointers.  Note also that the reloc procedures for string, ref and name */
/* pointers are never called. */
typedef ptr_proc_reloc((*ptr_proc_reloc_t), void);
const gs_ptr_procs_t ptr_struct_procs =
{ptr_struct_unmark, ptr_struct_mark, (ptr_proc_reloc_t) igc_reloc_struct_ptr};
const gs_ptr_procs_t ptr_string_procs =
{ptr_string_unmark, ptr_string_mark, NULL};
const gs_ptr_procs_t ptr_const_string_procs =
{ptr_string_unmark, ptr_string_mark, NULL};
const gs_ptr_procs_t ptr_ref_procs =
{ptr_ref_unmark, ptr_ref_mark, NULL};
const gs_ptr_procs_t ptr_name_index_procs =
{ptr_name_index_unmark, ptr_name_index_mark, NULL};

/* ------ Main program ------ */

/* Top level of garbage collector. */
#ifdef DEBUG
static void
end_phase(const gs_memory_t *mem, const char *str)
{
    if (gs_debug_c('6')) {
        dmlprintf1(mem, "[6]---------------- end %s ----------------\n",
                   (const char *)str);
        dmflush(mem);
    }
}
static const char *const depth_dots_string = "..........";
static const char *
depth_dots(const ms_entry * sp, const gc_mark_stack * pms)
{
    int depth = sp - pms->entries - 1;
    const gc_mark_stack *pss = pms;

    while ((pss = pss->prev) != 0)
        depth += pss->count - 1;
    return depth_dots_string + (depth >= 10 ? 0 : 10 - depth);
}
static void
gc_validate_spaces(gs_ref_memory_t **spaces, int max_space, gc_state_t *gcst)
{
    int i;
    gs_ref_memory_t *mem;

    for (i = 1; i <= max_space; ++i)
        if ((mem = spaces[i]) != 0)
            ialloc_validate_memory(mem, gcst);
}
#else  /* !DEBUG */
#  define end_phase(mem,str) DO_NOTHING
#endif /* DEBUG */

void
gs_gc_reclaim(vm_spaces * pspaces, bool global)
{
#define nspaces ((i_vm_max + 1) * 2) /* * 2 for stable allocators */

    vm_spaces spaces;
    gs_ref_memory_t *space_memories[nspaces];
    gs_gc_root_t space_roots[nspaces];
    int max_trace;		/* max space_ to trace */
    int min_collect;		/* min space_ to collect */
    int min_collect_vm_space;	/* min VM space to collect */
    int ispace;
    gs_ref_memory_t *mem;
    clump_t *cp;
    gs_gc_root_t *rp;
    gc_state_t state;
    struct _msd {
        gc_mark_stack stack;
        ms_entry body[ms_size_default];
    } ms_default;
    gc_mark_stack *mark_stack = &ms_default.stack;
    const gs_memory_t *cmem;
    clump_splay_walker sw;

    /* Optionally force global GC for debugging. */

    if (I_FORCE_GLOBAL_GC)
        global = true;

    /* Determine which spaces we are tracing and collecting. */

    spaces = *pspaces;
    cmem = space_system->stable_memory;
    space_memories[1] = space_system;
    space_memories[2] = space_global;
    min_collect = max_trace = 2;
    min_collect_vm_space = i_vm_global;
    if (space_global->stable_memory != (gs_memory_t *)space_global)
        space_memories[++max_trace] =
            (gs_ref_memory_t *)space_global->stable_memory;
    if (space_global != space_local) {
        space_memories[++max_trace] = space_local;
        min_collect = max_trace;
        min_collect_vm_space = i_vm_local;
        if (space_local->stable_memory != (gs_memory_t *)space_local)
            space_memories[++max_trace] =
                (gs_ref_memory_t *)space_local->stable_memory;
    }
    if (global)
        min_collect = min_collect_vm_space = 1;

#define for_spaces(i, n)\
  for (i = 1; i <= n; ++i)
#define for_collected_spaces(i)\
  for (i = min_collect; i <= max_trace; ++i)
#define for_space_mems(i, mem)\
  for (mem = space_memories[i]; mem != 0; mem = &mem->saved->state)
#define for_mem_clumps(mem, cp, sw)\
  for (cp = clump_splay_walk_init(sw, mem); cp != 0; cp = clump_splay_walk_fwd(sw))
#define for_space_clumps(i, mem, cp, sw)\
  for_space_mems(i, mem) for_mem_clumps(mem, cp, sw)
#define for_clumps(i, n, mem, cp, sw)\
  for_spaces(i, n) for_space_clumps(i, mem, cp, sw)
#define for_collected_clumps(i, mem, cp, sw)\
  for_collected_spaces(i) for_space_clumps(i, mem, cp, sw)
#define for_roots(i, n, mem, rp)\
  for_spaces(i, n)\
    for (mem = space_memories[i], rp = mem->roots; rp != 0; rp = rp->next)

    /* Initialize the state. */

    state.procs = &igc_procs;
    state.loc.memory = space_global;	/* any one will do */

    state.loc.cp = 0;
    state.spaces = spaces;
    state.min_collect = min_collect_vm_space << r_space_shift;
    state.relocating_untraced = false;
    state.heap = state.loc.memory->non_gc_memory;
    state.ntable = state.heap->gs_lib_ctx->gs_name_table;

    /* Register the allocators themselves as roots, */
    /* so we mark and relocate the change and save lists properly. */

    for_spaces(ispace, max_trace) {
        rp = &space_roots[ispace];
        gs_register_struct_root((gs_memory_t *)space_memories[ispace],
                                &rp,
                                (void **)&space_memories[ispace],
                                "gc_top_level");
    }

    end_phase(state.heap,"register space roots");

#ifdef DEBUG

    /* Pre-validate the state.  This shouldn't be necessary.... */

    gc_validate_spaces(space_memories, max_trace, &state);

    end_phase(state.heap,"pre-validate pointers");

#endif

    if (I_BYPASS_GC) {		/* Don't collect at all. */
        goto no_collect;
    }

    /* Clear marks in spaces to be collected. */

    for_collected_spaces(ispace)
        for_space_clumps(ispace, mem, cp, &sw) {
            gc_objects_clear_marks((const gs_memory_t *)mem, cp);
            gc_strings_set_marks(cp, false);
        }

    end_phase(state.heap,"clear clump marks");

    /* Clear the marks of roots.  We must do this explicitly, */
    /* since some roots are not in any clump. */

    for_roots(ispace, max_trace, mem, rp) {
        enum_ptr_t eptr;

        eptr.ptr = *rp->p;
        if_debug_root('6', (const gs_memory_t *)mem, "[6]unmarking root", rp);
        (*rp->ptype->unmark)(&eptr, &state);
    }

    end_phase(state.heap,"clear root marks");

    if (global) {
        op_array_table *global_ops = get_global_op_array(cmem);
        op_array_table *local_ops  = get_local_op_array(cmem);
        gc_unmark_names(state.ntable, global_ops, local_ops);
    }

    /* Initialize the (default) mark stack. */

    gc_init_mark_stack(&ms_default.stack, ms_size_default);
    ms_default.stack.prev = 0;
    ms_default.stack.on_heap = false;

    /* Add all large-enough free blocks to the mark stack. */
    /* Also initialize the rescan pointers. */

    {
        gc_mark_stack *end = mark_stack;

        for_clumps(ispace, max_trace, mem, cp, &sw) {
            uint avail = cp->ctop - cp->cbot;

            if (avail >= sizeof(gc_mark_stack) + sizeof(ms_entry) *
                ms_size_min &&
                !cp->inner_count
                ) {
                gc_mark_stack *pms = (gc_mark_stack *) cp->cbot;

                gc_init_mark_stack(pms, (avail - sizeof(gc_mark_stack)) /
                                   sizeof(ms_entry));
                end->next = pms;
                pms->prev = end;
                pms->on_heap = false;
                if_debug2m('6', (const gs_memory_t *)mem,
                           "[6]adding free "PRI_INTPTR"(%u) to mark stack\n",
                           (intptr_t)pms, pms->count);
            }
            cp->rescan_bot = cp->cend;
            cp->rescan_top = cp->cbase;
        }
    }

    /* Mark reachable objects. */

    {
        int more = 0;

        /* Mark from roots. */

        for_roots(ispace, max_trace, mem, rp) {
            if_debug_root('6', (const gs_memory_t *)mem, "[6]marking root", rp);
            more |= gc_trace(rp, &state, mark_stack);
        }

        end_phase(state.heap,"mark");

        /* If this is a local GC, mark from non-local clumps. */

        if (!global)
            for_clumps(ispace, min_collect - 1, mem, cp, &sw)
                more |= gc_trace_clump((const gs_memory_t *)mem, cp, &state, mark_stack);

        /* Handle mark stack overflow. */

        while (more < 0) {	/* stack overflowed */
            more = 0;
            for_clumps(ispace, max_trace, mem, cp, &sw)
                more |= gc_rescan_clump(cp, &state, mark_stack);
        }

        end_phase(state.heap,"mark overflow");
    }

    /* Free the mark stack. */

    {
        gc_mark_stack *pms = mark_stack;

        while (pms->next)
            pms = pms->next;
        while (pms) {
            gc_mark_stack *prev = pms->prev;

            if (pms->on_heap)
                gs_free_object(state.heap, pms, "gc mark stack");
            else
                gs_alloc_fill(pms, gs_alloc_fill_free,
                              sizeof(*pms) + sizeof(ms_entry) * pms->count);
            pms = prev;
        }
    }

    end_phase(state.heap,"free mark stack");

    if (global) {
        gc_trace_finish(&state);
        names_trace_finish(state.ntable, &state);

        end_phase(state.heap,"finish trace");
    }

    /* Filter save change lists with removing elements,
       which point to unmarked blocks of refs. */
    {
        int i;

        for_collected_spaces(i) {
            gs_ref_memory_t *mem = space_memories[i];

            alloc_save__filter_changes(mem);
        }
    }
    /* Clear marks and relocation in spaces that are only being traced. */
    /* We have to clear the marks first, because we want the */
    /* relocation to wind up as o_untraced, not o_unmarked. */

    for_clumps(ispace, min_collect - 1, mem, cp, &sw)
        gc_objects_clear_marks((const gs_memory_t *)mem, cp);

    end_phase(state.heap,"post-clear marks");

    for_clumps(ispace, min_collect - 1, mem, cp, &sw)
        gc_clear_reloc(cp);

    end_phase(state.heap,"clear reloc");

    /* Set the relocation of roots outside any clump to o_untraced, */
    /* so we won't try to relocate pointers to them. */
    /* (Currently, there aren't any.) */

    /* Disable freeing in the allocators of the spaces we are */
    /* collecting, so finalization procedures won't cause problems. */
    {
        int i;

        for_collected_spaces(i)
            gs_enable_free((gs_memory_t *)space_memories[i], false);
    }

    /* Compute relocation based on marks, in the spaces */
    /* we are going to compact.  Also finalize freed objects. */
    state.cur_mem = (gs_memory_t *)mem;

    for_collected_clumps(ispace, mem, cp, &sw) {
        gc_objects_set_reloc(&state, cp);
        gc_strings_set_reloc(cp);
    }

    /* Re-enable freeing. */
    {
        int i;

        for_collected_spaces(i)
            gs_enable_free((gs_memory_t *)space_memories[i], true);
    }

    end_phase(state.heap,"set reloc");

    /* Relocate pointers. */

    state.relocating_untraced = true;
    for_clumps(ispace, min_collect - 1, mem, cp, &sw)
        gc_do_reloc(cp, mem, &state);
    state.relocating_untraced = false;
    for_collected_clumps(ispace, mem, cp, &sw)
        gc_do_reloc(cp, mem, &state);

    end_phase(state.heap,"relocate clumps");

    for_roots(ispace, max_trace, mem, rp) {
        if_debug3m('6', (const gs_memory_t *)mem,
                   "[6]relocating root "PRI_INTPTR": "PRI_INTPTR" -> "PRI_INTPTR"\n",
                   (intptr_t)rp, (intptr_t)rp->p, (intptr_t)*rp->p);
        if (rp->ptype == ptr_ref_type) {
            ref *pref = (ref *) * rp->p;

            igc_reloc_refs((ref_packed *) pref,
                           (ref_packed *) (pref + 1),
                           &state);
        } else
            *rp->p = (*rp->ptype->reloc) (*rp->p, &state);
        if_debug3m('6', (const gs_memory_t *)mem,
                   "[6]relocated root "PRI_INTPTR": "PRI_INTPTR" -> "PRI_INTPTR"\n",
                   (intptr_t)rp, (intptr_t)rp->p, (intptr_t)*rp->p);
    }

    end_phase(state.heap,"relocate roots");

    /* Compact data.  We only do this for spaces we are collecting. */

    for_collected_spaces(ispace) {
        for_space_mems(ispace, mem) {
            for_mem_clumps(mem, cp, &sw) {
                if_debug_clump('6', (const gs_memory_t *)mem, "[6]compacting clump", cp);
                gc_objects_compact(cp, &state);
                gc_strings_compact(cp, cmem);
                if_debug_clump('6', (const gs_memory_t *)mem, "[6]after compaction:", cp);
            }
            mem->saved = mem->reloc_saved;
            ialloc_reset_free(mem);
        }
    }

    end_phase(state.heap,"compact");

    /* Free empty clumps. */

    for_collected_spaces(ispace) {
        for_space_mems(ispace, mem) {
            gc_free_empty_clumps(mem);
        }
    }

    end_phase(state.heap,"free empty clumps");

    /*
     * Update previous_status to reflect any freed clumps,
     * and set inherited to the negative of allocated,
     * so it has no effect.  We must update previous_status by
     * working back-to-front along the save chain, using pointer reversal.
     * (We could update inherited in any order, since it only uses
     * information local to the individual save level.)
     */

    for_collected_spaces(ispace) {	/* Reverse the pointers. */
        alloc_save_t *curr;
        alloc_save_t *prev = 0;
        alloc_save_t *next;
        gs_memory_status_t total;

        for (curr = space_memories[ispace]->saved; curr != 0;
             prev = curr, curr = next
            ) {
            next = curr->state.saved;
            curr->state.saved = prev;
        }
        /* Now work the other way, accumulating the values. */
        total.allocated = 0, total.used = 0;
        for (curr = prev, prev = 0; curr != 0;
             prev = curr, curr = next
            ) {
            mem = &curr->state;
            next = mem->saved;
            mem->saved = prev;
            mem->previous_status = total;
            if_debug3m('6', (const gs_memory_t *)mem,
                       "[6]"PRI_INTPTR" previous allocated=%lu, used=%lu\n",
                       (intptr_t)mem, total.allocated, total.used);
            gs_memory_status((gs_memory_t *) mem, &total);
            mem->gc_allocated = mem->allocated + total.allocated;
        }
        mem = space_memories[ispace];
        mem->previous_status = total;
        mem->gc_allocated = mem->allocated + total.allocated;
        if_debug3m('6', (const gs_memory_t *)mem,
                   "[6]"PRI_INTPTR" previous allocated=%lu, used=%lu\n",
                   (intptr_t)mem, total.allocated, total.used);
    }

    end_phase(state.heap,"update stats");

  no_collect:

    /* Unregister the allocator roots. */

    for_spaces(ispace, max_trace)
        gs_unregister_root((gs_memory_t *)space_memories[ispace],
                           &space_roots[ispace], "gc_top_level");

    end_phase(state.heap,"unregister space roots");

#ifdef DEBUG

    /* Validate the state.  This shouldn't be necessary.... */

    gc_validate_spaces(space_memories, max_trace, &state);

    end_phase(state.heap,"validate pointers");

#endif
}

/* ------ Debugging utilities ------ */

/* Validate a pointer to an object header. */
#ifdef DEBUG
#  define debug_check_object(pre, cp, gcst)\
     ialloc_validate_object((pre) + 1, cp, gcst)
#else
#  define debug_check_object(pre, cp, gcst) DO_NOTHING
#endif

/* ------ Unmarking phase ------ */

/* Unmark a single struct. */
static void
ptr_struct_unmark(enum_ptr_t *pep, gc_state_t * ignored)
{
    void *const vptr = (void *)pep->ptr; /* break const */

    if (vptr != 0)
        o_set_unmarked(((obj_header_t *) vptr - 1));
}

/* Unmark a single string. */
static void
ptr_string_unmark(enum_ptr_t *pep, gc_state_t * gcst)
{
    discard(gc_string_mark(pep->ptr, pep->size, false, gcst));
}

/* Unmark a single name. */
static void
ptr_name_index_unmark(enum_ptr_t *pep, gc_state_t * gcst)
{
    /* Do nothing */
}

/* Unmark the objects in a clump. */
static void
gc_objects_clear_marks(const gs_memory_t *mem, clump_t * cp)
{
    if_debug_clump('6', mem, "[6]unmarking clump", cp);
    SCAN_CLUMP_OBJECTS(cp)
        DO_ALL
        struct_proc_clear_marks((*proc)) =
        pre->o_type->clear_marks;
#ifdef DEBUG
    if (pre->o_type != &st_free)
        debug_check_object(pre, cp, NULL);
#endif
    if_debug3m('7', (const gs_memory_t *)mem, " [7](un)marking %s(%lu) "PRI_INTPTR"\n",
               struct_type_name_string(pre->o_type),
               (ulong) size, (intptr_t)pre);
    o_set_unmarked(pre);
    if (proc != 0)
        (*proc) (mem, pre + 1, size, pre->o_type);
    END_OBJECTS_SCAN
}

/* Mark 0- and 1-character names, and those referenced from the */
/* op_array_nx_table, and unmark all the rest. */
static void
gc_unmark_names(name_table * nt, op_array_table *op_array_table_global,
                op_array_table *op_array_table_local)
{
    uint i;

    names_unmark_all(nt);
    for (i = 0; i < op_array_table_global->count; i++) {
        name_index_t nidx = op_array_table_global->nx_table[i];

        names_mark_index(nt, nidx);
    }
    for (i = 0; i < op_array_table_local->count; i++) {
        name_index_t nidx = op_array_table_local->nx_table[i];

        names_mark_index(nt, nidx);
    }
}

/* ------ Marking phase ------ */

/* Initialize (a segment of) the mark stack. */
static void
gc_init_mark_stack(gc_mark_stack * pms, uint count)
{
    pms->next = 0;
    pms->count = count;
    pms->entries[0].ptr = 0;
    pms->entries[0].index = 0;
    pms->entries[0].is_refs = false;
}

/* Mark starting from all marked objects in the interval of a clump */
/* needing rescanning. */
static int
gc_rescan_clump(clump_t * cp, gc_state_t * pstate, gc_mark_stack * pmstack)
{
    byte *sbot = cp->rescan_bot;
    byte *stop = cp->rescan_top;
    gs_gc_root_t root;
    void *comp;
    int more = 0;
    const gs_memory_t *mem = gcst_get_memory_ptr( pstate );

    if (sbot > stop)
        return 0;
    root.p = &comp;
    if_debug_clump('6', mem, "[6]rescanning clump", cp);
    cp->rescan_bot = cp->cend;
    cp->rescan_top = cp->cbase;
    SCAN_CLUMP_OBJECTS(cp)
        DO_ALL
        if ((byte *) (pre + 1) + size < sbot);
    else if ((byte *) (pre + 1) > stop)
        return more;		/* 'break' won't work here */
    else {
        if_debug2m('7', mem, " [7]scanning/marking "PRI_INTPTR"(%lu)\n",
                   (intptr_t)pre, (ulong)size);
        if (pre->o_type == &st_refs) {
            ref_packed *rp = (ref_packed *) (pre + 1);
            char *end = (char *)rp + size;

            root.ptype = ptr_ref_type;
            while ((char *)rp < end) {
                comp = rp;
                if (r_is_packed(rp)) {
                    if (r_has_pmark(rp)) {
                        r_clear_pmark(rp);
                        more |= gc_trace(&root, pstate,
                                         pmstack);
                    }
                    rp++;
                } else {
                    ref *const pref = (ref *)rp;

                    if (r_has_attr(pref, l_mark)) {
                        r_clear_attrs(pref, l_mark);
                        more |= gc_trace(&root, pstate, pmstack);
                    }
                    rp += packed_per_ref;
                }
            }
        } else if (!o_is_unmarked(pre)) {
            struct_proc_clear_marks((*proc)) =
                pre->o_type->clear_marks;
            root.ptype = ptr_struct_type;
            comp = pre + 1;
            if (!o_is_untraced(pre))
                o_set_unmarked(pre);
            if (proc != 0)
                (*proc) (mem, comp, size, pre->o_type);
            more |= gc_trace(&root, pstate, pmstack);
        }
    }
    END_OBJECTS_SCAN
        return more;
}

/* Mark starting from all the objects in a clump. */
/* We assume that pstate->min_collect > avm_system, */
/* so we don't have to trace names. */
static int
gc_trace_clump(const gs_memory_t *mem, clump_t * cp, gc_state_t * pstate, gc_mark_stack * pmstack)
{
    gs_gc_root_t root;
    void *comp;
    int more = 0;
    int min_trace = pstate->min_collect;

    root.p = &comp;
    if_debug_clump('6', mem, "[6]marking from clump", cp);
    SCAN_CLUMP_OBJECTS(cp)
        DO_ALL
    {
        if_debug2m('7', mem, " [7]scanning/marking "PRI_INTPTR"(%lu)\n",
                   (intptr_t)pre, (ulong)size);
        if (pre->o_type == &st_refs) {
            ref_packed *rp = (ref_packed *) (pre + 1);
            char *end = (char *)rp + size;

            root.ptype = ptr_ref_type;
            while ((char *)rp < end) {
                comp = rp;
                if (r_is_packed(rp)) {	/* No packed refs need tracing. */
                    rp++;
                } else {
                    ref *const pref = (ref *)rp;

                    if (r_space(pref) >= min_trace) {
                        r_clear_attrs(pref, l_mark);
                        more |= gc_trace(&root, pstate, pmstack);
                    }
                    rp += packed_per_ref;
                }
            }
        } else if (!o_is_unmarked(pre)) {
            if (!o_is_untraced(pre))
                o_set_unmarked(pre);
            if (pre->o_type != &st_free) {
                struct_proc_clear_marks((*proc)) =
                    pre->o_type->clear_marks;

                root.ptype = ptr_struct_type;
                comp = pre + 1;
                if (proc != 0)
                    (*proc) (mem, comp, size, pre->o_type);
                more |= gc_trace(&root, pstate, pmstack);
            }
        }
    }
    END_OBJECTS_SCAN
        return more;
}

/* Recursively mark from a (root) pointer. */
/* Return -1 if we overflowed the mark stack, */
/* 0 if we completed successfully without marking any new objects, */
/* 1 if we completed and marked some new objects. */
static int gc_extend_stack(gc_mark_stack *, gc_state_t *);
static int
gc_trace(gs_gc_root_t * rp, /* lgtm [cpp/use-of-goto] */
         gc_state_t * pstate, gc_mark_stack * pmstack)
{
    int min_trace = pstate->min_collect;
    gc_mark_stack *pms = pmstack;
    ms_entry *sp = pms->entries + 1;

    /* We stop the mark stack 1 entry early, because we store into */
    /* the entry beyond the top. */
    ms_entry *stop = sp + pms->count - 2;
    int new = 0;
    enum_ptr_t nep;
    void *nptr;
    name_table *nt = pstate->ntable;

#define mark_name(nidx)\
  BEGIN\
    if (names_mark_index(nt, nidx)) {\
        new |= 1;\
        if_debug2m('8', gcst_get_memory_ptr(pstate), "  [8]marked name "PRI_INTPTR"(%u)\n",\
                   (intptr_t)names_index_ptr(nt, nidx), nidx);\
    }\
  END

    nptr = *rp->p;
    if (nptr == 0)
        return 0;

    /* Initialize the stack */
    sp->ptr = nptr;
    if (rp->ptype == ptr_ref_type)
        sp->index = 1, sp->is_refs = true;
    else {
        sp->index = 0, sp->is_refs = false;
        nep.ptr = nptr;
        if ((*rp->ptype->mark) (&nep, pstate))
            new |= 1;
    }
    for (;;) {
        gs_ptr_type_t ptp;

        /*
         * The following should really be an if..else, but that
         * would force unnecessary is_refs tests.
         */
        if (sp->is_refs)
            goto do_refs;

        /* ---------------- Structure ---------------- */

      do_struct:
        {
            obj_header_t *ptr = sp->ptr;

            struct_proc_enum_ptrs((*mproc));

            if (ptr == 0) {	/* We've reached the bottom of a stack segment. */
                pms = pms->prev;
                if (pms == 0)
                    break;	/* all done */
                stop = pms->entries + pms->count - 1;
                sp = stop;
                continue;
            }
            debug_check_object(ptr - 1, NULL, NULL);
          ts:if_debug4m('7', pstate->heap, " [7]%smarking %s "PRI_INTPTR"[%u]",
                        depth_dots(sp, pms),
                        struct_type_name_string(ptr[-1].o_type),
                        (intptr_t)ptr, sp->index);
            mproc = ptr[-1].o_type->enum_ptrs;
            if (mproc == gs_no_struct_enum_ptrs ||
                (ptp = (*mproc)
                 (gcst_get_memory_ptr(pstate), ptr, pre_obj_contents_size(ptr - 1),
                  sp->index, &nep, ptr[-1].o_type, pstate)) == 0
                ) {
                if_debug0m('7', pstate->heap, " - done\n");
                sp--;
                continue;
            }
            /* The cast in the following statement is the one */
            /* place we need to break 'const' to make the */
            /* template for pointer enumeration work. */
            nptr = (void *)nep.ptr;
            sp->index++;
            if_debug1m('7', pstate->heap, " = "PRI_INTPTR"\n", (intptr_t)nptr);
            /* Descend into nep.ptr, whose pointer type is ptp. */
            if (ptp == ptr_struct_type) {
                sp[1].index = 0;
                sp[1].is_refs = false;
                if (sp == stop)
                    goto push;
                if (!ptr_struct_mark(&nep, pstate))
                    goto ts;
                new |= 1;
                (++sp)->ptr = nptr;
                goto do_struct;
            } else if (ptp == ptr_ref_type) {
                sp[1].index = 1;
                sp[1].is_refs = true;
                if (sp == stop)
                    goto push;
                new |= 1;
                (++sp)->ptr = nptr;
                goto do_refs;
            } else {		/* We assume this is some non-pointer- */
                /* containing type. */
                /* This was the only formulation of this call/condition/assignment that valgrind did not complain about */
                new |= ((*ptp->mark) (&nep, pstate)) ? 1 : 0;
               goto ts;
            }
        }

        /* ---------------- Refs ---------------- */

      do_refs:
        {
            ref_packed *pptr = sp->ptr;
            ref *rptr;

          tr:if (!sp->index) {
                --sp;
                continue;
            }
            --(sp->index);
            if_debug3m('8', pstate->heap, "  [8]%smarking refs "PRI_INTPTR"[%u]\n",
                      depth_dots(sp, pms), (intptr_t)pptr, sp->index);
            if (r_is_packed(pptr)) {
                if (!r_has_pmark(pptr)) {
                    r_set_pmark(pptr);
                    new |= 1;
                    if (r_packed_is_name(pptr)) {
                        name_index_t nidx = packed_name_index(pptr);

                        mark_name(nidx);
                    }
                }
                ++pptr;
                goto tr;
            }
            rptr = (ref *) pptr;	/* * const beyond here */
            if (r_has_attr(rptr, l_mark)) {
                pptr = (ref_packed *)(rptr + 1);
                goto tr;
            }
            r_set_attrs(rptr, l_mark);
            new |= 1;
            if (r_space(rptr) < min_trace) {	/* Note that this always picks up all scalars. */
                pptr = (ref_packed *) (rptr + 1);
                goto tr;
            }
            sp->ptr = rptr + 1;
            switch (r_type(rptr)) {
                    /* Struct cases */
                case t_file:
                    nptr = rptr->value.pfile;
                  rs:sp[1].is_refs = false;
                    sp[1].index = 0;
                    if (sp == stop) {
                        ptp = ptr_struct_type;
                        break;
                    }
                    nep.ptr = nptr;
                    if (!ptr_struct_mark(&nep, pstate))
                        goto nr;
                    new |= 1;
                    (++sp)->ptr = nptr;
                    goto do_struct;
                case t_device:
                    nptr = rptr->value.pdevice;
                    goto rs;
                case t_fontID:
                case t_struct:
                case t_astruct:
                case t_pdfctx:
                    nptr = rptr->value.pstruct;
                    goto rs;
                    /* Non-trivial non-struct cases */
                case t_dictionary:
                    nptr = rptr->value.pdict;
                    sp[1].index = sizeof(dict) / sizeof(ref);
                    goto rrp;
                case t_array:
                    nptr = rptr->value.refs;
                  rr:if ((sp[1].index = r_size(rptr)) == 0) {	/* Set the base pointer to 0, */
                        /* so we never try to relocate it. */
                        rptr->value.refs = 0;
                        goto nr;
                    }
                  rrp:
                  rrc:sp[1].is_refs = true;
                    if (sp == stop) {
                        /*
                         * The following initialization is unnecessary:
                         * ptp will not be used if sp[1].is_refs = true.
                         * We put this here solely to get rid of bogus
                         * "possibly uninitialized variable" warnings
                         * from certain compilers.
                         */
                        ptp = ptr_ref_type;
                        break;
                    }
                    new |= 1;
                    (++sp)->ptr = nptr;
                    goto do_refs;
                case t_mixedarray:
                case t_shortarray:
                    nptr = rptr->value.writable_packed;
                    goto rr;
                case t_name:
                    mark_name(names_index(nt, rptr));
                  nr:pptr = (ref_packed *) (rptr + 1);
                    goto tr;
                case t_string:
                    if (gc_string_mark(rptr->value.bytes, r_size(rptr), true, pstate))
                        new |= 1;
                    goto nr;
                case t_oparray:
                    nptr = rptr->value.refs;	/* discard const */
                    sp[1].index = 1;
                    goto rrc;
                default:
                    goto nr;
            }
        }

        /* ---------------- Recursion ---------------- */

      push:
        if (sp == stop) {	/* The current segment is full. */
            int new_added = gc_extend_stack(pms, pstate);

            if (new_added) {
                new |= new_added;
                continue;
            }
            pms = pms->next;
            stop = pms->entries + pms->count - 1;
            pms->entries[1] = sp[1];
            sp = pms->entries;
        }
        /* index and is_refs are already set */
        if (!sp[1].is_refs) {
            nep.ptr = nptr;
            if (!(*ptp->mark) (&nep, pstate))
                continue;
            new |= 1;
        }
        (++sp)->ptr = nptr;
    }
    return new;
}
/* Link to, attempting to allocate if necessary, */
/* another clump of mark stack. */
static int
gc_extend_stack(gc_mark_stack * pms, gc_state_t * pstate)
{
    if (pms->next == 0) {	/* Try to allocate another segment. */
        size_t count;

        for (count = ms_size_desired; count >= ms_size_min; count >>= 1) {
            pms->next = (gc_mark_stack *)
                gs_alloc_bytes_immovable(pstate->heap,
                                         sizeof(gc_mark_stack) +
                                         sizeof(ms_entry) * count,
                                         "gc mark stack");
            if (pms->next != 0)
                break;
        }
        if (pms->next == 0) {	/* The mark stack overflowed. */
            ms_entry *sp = pms->entries + pms->count - 1;
            byte *cptr = sp->ptr;	/* container */
            clump_t *cp = gc_locate(cptr, pstate);
            int new = 1;

            if (cp == 0) {	/* We were tracing outside collectible */
                /* storage.  This can't happen. */
                if_debug1('6', "mark stack overflowed while outside collectible space at "PRI_INTPTR"!\n",
                         (intptr_t)cptr);
                gs_abort(pstate->heap);
            }
            if (cptr < cp->rescan_bot)
                cp->rescan_bot = cptr, new = -1;
            if (cptr > cp->rescan_top)
                cp->rescan_top = cptr, new = -1;
            return new;
        }
        gc_init_mark_stack(pms->next, count);
        pms->next->prev = pms;
        pms->next->on_heap = true;
    }
    return 0;
}

/* Mark a struct.  Return true if new mark. */
static bool
ptr_struct_mark(enum_ptr_t *pep, gc_state_t * ignored)
{
    obj_header_t *ptr = (obj_header_t *)pep->ptr;

    if (ptr == 0)
        return false;
    ptr--;			/* point to header */
    if (!o_is_unmarked(ptr))
        return false;
    o_mark(ptr);
    return true;
}

/* Mark a string.  Return true if new mark. */
static bool
ptr_string_mark(enum_ptr_t *pep, gc_state_t * gcst)
{
    return gc_string_mark(pep->ptr, pep->size, true, gcst);
}

/* Mark a name.  Return true if new mark. */
static bool
ptr_name_index_mark(enum_ptr_t *pep, gc_state_t * gcst)
{
    return names_mark_index(gcst->heap->gs_lib_ctx->gs_name_table, pep->size);
}

/* Finish tracing by marking names. */
static bool
gc_trace_finish(gc_state_t * pstate)
{
    name_table *nt = pstate->ntable;
    name_index_t nidx = 0;
    bool marked = false;

    while ((nidx = names_next_valid_index(nt, nidx)) != 0) {
        name_string_t *pnstr = names_index_string_inline(nt, nidx);

        if (pnstr->mark) {
            enum_ptr_t enst, ensst;

            if (!pnstr->foreign_string &&
                gc_string_mark(pnstr->string_bytes, pnstr->string_size,
                               true, pstate)
                )
                marked = true;
            enst.ptr = names_index_sub_table(nt, nidx);
            ensst.ptr = names_index_string_sub_table(nt, nidx);
            marked |=
                ptr_struct_mark(&enst, pstate) |
                ptr_struct_mark(&ensst, pstate);
        }
    }
    return marked;
}

/* ------ Relocation planning phase ------ */

/* Initialize the relocation information in the clump header. */
static void
gc_init_reloc(clump_t * cp)
{
    clump_head_t *chead = cp->chead;

    chead->dest = cp->cbase;
    chead->free.o_back =
        offset_of(clump_head_t, free) >> obj_back_shift;
    chead->free.o_size = sizeof(obj_header_t);
    chead->free.o_nreloc = 0;
}

/* Set marks and clear relocation for clumps that won't be compacted. */
static void
gc_clear_reloc(clump_t * cp)
{
    byte *pfree = (byte *) & cp->chead->free;

    gc_init_reloc(cp);
    SCAN_CLUMP_OBJECTS(cp)
        DO_ALL
        const struct_shared_procs_t *procs =
    pre->o_type->shared;

    if (procs != 0)
        (*procs->clear_reloc) (pre, size);
    o_set_untraced(pre);
    pre->o_back = ((byte *) pre - pfree) >> obj_back_shift;
    END_OBJECTS_SCAN
        gc_strings_set_marks(cp, true);
    gc_strings_clear_reloc(cp);
}

/* Set the relocation for the objects in a clump. */
/* This will never be called for a clump with any o_untraced objects. */
static void
gc_objects_set_reloc(gc_state_t * gcst, clump_t * cp)
{
    size_t reloc = 0;
    clump_head_t *chead = cp->chead;
    byte *pfree = (byte *) & chead->free;	/* most recent free object */

    if_debug_clump('6', gcst->heap, "[6]setting reloc for clump", cp);
    gc_init_reloc(cp);
    SCAN_CLUMP_OBJECTS(cp)
        DO_ALL
        struct_proc_finalize((*finalize));
    const struct_shared_procs_t *procs =
    pre->o_type->shared;

    if ((procs == 0 ? o_is_unmarked(pre) :
         !(*procs->set_reloc) (pre, reloc, size))
        ) {			/* Free object */
        reloc += sizeof(obj_header_t) + obj_align_round(size);
        if ((finalize = pre->o_type->finalize) != 0) {
            if_debug2m('u', gcst->heap, "[u]GC finalizing %s "PRI_INTPTR"\n",
                       struct_type_name_string(pre->o_type),
                       (intptr_t)(pre + 1));
            (*finalize) (gcst->cur_mem, pre + 1);
        }
        pfree = (byte *) pre;
        pre->o_back = (pfree - (byte *) chead) >> obj_back_shift;
        pre->o_nreloc = reloc;
        if_debug3m('7', gcst->heap, " [7]at "PRI_INTPTR", unmarked %lu, new reloc = %u\n",
                   (intptr_t)pre, (ulong) size, (unsigned int)reloc);
    } else {			/* Useful object */
        debug_check_object(pre, cp, gcst);
        pre->o_back = ((byte *) pre - pfree) >> obj_back_shift;
    }
    END_OBJECTS_SCAN
#ifdef DEBUG
    if (reloc != 0) {
        if_debug1m('6', gcst->heap, "[6]freed %u", (unsigned int)reloc);
        if_debug_clump('6', gcst->heap, " in", cp);
    }
#endif
}

/* ------ Relocation phase ------ */

/* Relocate the pointers in all the objects in a clump. */
static void
gc_do_reloc(clump_t * cp, gs_ref_memory_t * mem, gc_state_t * pstate)
{
    clump_head_t *chead = cp->chead;

    if_debug_clump('6', (const gs_memory_t *)mem, "[6]relocating in clump", cp);
    SCAN_CLUMP_OBJECTS(cp)
        DO_ALL
#ifdef DEBUG
        pstate->container = cp;
#endif
    /* We need to relocate the pointers in an object iff */
    /* it is o_untraced, or it is a useful object. */
    /* An object is free iff its back pointer points to */
    /* the clump_head structure. */
        if (o_is_untraced(pre) ||
            pre->o_back << obj_back_shift != (byte *) pre - (byte *) chead
            ) {
            struct_proc_reloc_ptrs((*proc)) =
                pre->o_type->reloc_ptrs;

            if_debug3m('7', (const gs_memory_t *)mem,
                       " [7]relocating ptrs in %s(%lu) "PRI_INTPTR"\n",
                       struct_type_name_string(pre->o_type),
                       (ulong)size, (intptr_t)pre);
            if (proc != 0)
                (*proc) (pre + 1, size, pre->o_type, pstate);
        }
#ifdef DEBUG
        pstate->container = 0;
#endif
    END_OBJECTS_SCAN
}

/* Print pointer relocation if debugging. */
/* We have to provide this procedure even if DEBUG is not defined, */
/* in case one of the other GC modules was compiled with DEBUG. */
const void *
print_reloc_proc(const void *obj, const char *cname, const void *robj)
{
    if_debug3('9', "  [9]relocate %s * "PRI_INTPTR" to "PRI_INTPTR"\n",
              cname, (intptr_t)obj, (intptr_t)robj);
    return robj;
}

/* Relocate a pointer to an (aligned) object. */
/* See gsmemory.h for why the argument is const and the result is not. */
static void /*obj_header_t */ *
igc_reloc_struct_ptr(const void /*obj_header_t */ *obj, gc_state_t * gcst)
{
    const obj_header_t *const optr = (const obj_header_t *)obj;
    const void *robj;

    if (obj == 0) {
        discard(print_reloc(obj, "NULL", 0));
        return 0;
    }
    debug_check_object(optr - 1, NULL, gcst);
    {
        uint back = optr[-1].o_back;

        if (back == o_untraced)
            robj = obj;
        else {
#ifdef DEBUG
            /* Do some sanity checking. */
            clump_t *cp = gcst->container;

            if (cp != 0 && cp->cbase <= (byte *)obj && (byte *)obj <cp->ctop) {
                if (back > (cp->ctop - cp->cbase) >> obj_back_shift) {
                    if_debug2('6', "Invalid back pointer %u at "PRI_INTPTR"!\n",
                             back, (intptr_t)obj);
                    gs_abort(NULL);
                }
            } else {
                /* Pointed to unknown clump. Can't check it, sorry. */
            }
#endif
            {
                const obj_header_t *pfree = (const obj_header_t *)
                ((const char *)(optr - 1) -
                 (back << obj_back_shift));
                const clump_head_t *chead = (const clump_head_t *)
                ((const char *)pfree -
                 (pfree->o_back << obj_back_shift));

                robj = chead->dest +
                    ((const char *)obj - (const char *)(chead + 1) -
                     pfree->o_nreloc);
            }
        }
    }
    /* Use a severely deprecated pun to remove the const property. */
    {
        union { const void *r; void *w; } u;

        u.r = print_reloc(obj, struct_type_name_string(optr[-1].o_type), robj);
        return u.w;
    }
}

/* ------ Compaction phase ------ */

/* Compact the objects in a clump. */
/* This will never be called for a clump with any o_untraced objects. */
static void
gc_objects_compact(clump_t * cp, gc_state_t * gcst)
{
    clump_head_t *chead = cp->chead;
    obj_header_t *dpre = (obj_header_t *) chead->dest;
    const gs_memory_t *cmem = gcst->spaces.memories.named.system->stable_memory;

    SCAN_CLUMP_OBJECTS(cp)
        DO_ALL
    /* An object is free iff its back pointer points to */
    /* the clump_head structure. */
        if (pre->o_back << obj_back_shift != (byte *) pre - (byte *) chead) {
        const struct_shared_procs_t *procs = pre->o_type->shared;

        debug_check_object(pre, cp, gcst);
        if_debug4m('7', cmem,
                   " [7]compacting %s "PRI_INTPTR"(%lu) to "PRI_INTPTR"\n",
                   struct_type_name_string(pre->o_type),
                   (intptr_t)pre, (ulong)size, (intptr_t)dpre);
        if (procs == 0) {
            if (dpre != pre)
                memmove(dpre, pre,
                        sizeof(obj_header_t) + size);
        } else
            (*procs->compact) (cmem, pre, dpre, size);
        dpre = (obj_header_t *)
            ((byte *) dpre + obj_size_round(size));
    }
    END_OBJECTS_SCAN
        if (cp->outer == 0 && chead->dest != cp->cbase)
        dpre = (obj_header_t *) cp->cbase;	/* compacted this clump into another */
    gs_alloc_fill(dpre, gs_alloc_fill_collected, cp->cbot - (byte *) dpre);
    cp->cbot = (byte *) dpre;
    cp->rcur = 0;
    cp->rtop = 0;		/* just to be sure */
}

/* ------ Cleanup ------ */

static splay_app_result_t
free_if_empty(clump_t *cp, void *arg)
{
    gs_ref_memory_t * mem = (gs_ref_memory_t *)arg;

    if (cp->cbot == cp->cbase && cp->ctop == cp->climit &&
        cp->outer == 0 && cp->inner_count == 0)
    {
        alloc_free_clump(cp, mem);
        if (mem->cc == cp)
            mem->cc = NULL;
    }
    return SPLAY_APP_CONTINUE;
}

/* Free empty clumps. */
static void
gc_free_empty_clumps(gs_ref_memory_t * mem)
{
    /* NOTE: Not in reverse order any more, so potentially
     * not quite as good for crap allocators. */
    clump_splay_app(mem->root, mem, free_if_empty, mem);
}

const gs_memory_t * gcst_get_memory_ptr(gc_state_t *gcst)
{
    vm_spaces spaces = gcst->spaces;
    const gs_memory_t *cmem = space_system->stable_memory;
    return cmem;
}
