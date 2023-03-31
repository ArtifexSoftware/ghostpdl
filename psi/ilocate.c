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


/* Object locating and validating for Ghostscript memory manager */
#include "ghost.h"
#include "memory_.h"
#include "ierrors.h"
#include "gsexit.h"
#include "gsstruct.h"
#include "iastate.h"
#include "idict.h"
#include "igc.h"	/* for gc_state_t and gcst_get_memory_ptr() */
#include "igcstr.h"	/* for prototype */
#include "iname.h"
#include "ipacked.h"
#include "isstate.h"
#include "iutil.h"	/* for packed_get */
#include "ivmspace.h"
#include "store.h"

#ifdef DEBUG
static int do_validate_clump(const clump_t * cp, gc_state_t * gcst);
static int do_validate_object(const obj_header_t * ptr, const clump_t * cp,
                              gc_state_t * gcst);
#endif


/* ================ Locating ================ */

/* Locate a pointer in the clumps of a space being collected. */
/* This is only used for string garbage collection and for debugging. */
clump_t *
gc_locate(const void *ptr, gc_state_t * gcst)
{
    gs_ref_memory_t *mem;
    gs_ref_memory_t *other;

    if (clump_locate(ptr, &gcst->loc))
        return gcst->loc.cp;
    mem = gcst->loc.memory;

    /*
     * Try the stable allocator of this space, or, if the current memory
     * is the stable one, the non-stable allocator of this space.
     */

    if ((other = (gs_ref_memory_t *)mem->stable_memory) != mem ||
        (other = gcst->spaces_indexed[mem->space >> r_space_shift]) != mem
        ) {
        gcst->loc.memory = other;
        gcst->loc.cp = 0;
        if (clump_locate(ptr, &gcst->loc))
            return gcst->loc.cp;
    }

    /*
     * Try the other space, if there is one, including its stable allocator
     * and all save levels.  (If the original space is system space, try
     * local space.)
     */

    if (gcst->space_local != gcst->space_global) {
        gcst->loc.memory = other =
            (mem->space == avm_local ? gcst->space_global : gcst->space_local);
        gcst->loc.cp = 0;
        if (clump_locate(ptr, &gcst->loc))
            return gcst->loc.cp;
        /* Try its stable allocator. */
        if (other->stable_memory != (const gs_memory_t *)other) {
            gcst->loc.memory = (gs_ref_memory_t *)other->stable_memory;
            gcst->loc.cp = 0;
            if (clump_locate(ptr, &gcst->loc))
                return gcst->loc.cp;
            gcst->loc.memory = other;
        }
        /* Try other save levels of this space. */
        while (gcst->loc.memory->saved != 0) {
            gcst->loc.memory = &gcst->loc.memory->saved->state;
            gcst->loc.cp = 0;
            if (clump_locate(ptr, &gcst->loc))
                return gcst->loc.cp;
        }
    }

    /*
     * Try system space.  This is simpler because it isn't subject to
     * save/restore and doesn't have a separate stable allocator.
     */

    if (mem != gcst->space_system) {
        gcst->loc.memory = gcst->space_system;
        gcst->loc.cp = 0;
        if (clump_locate(ptr, &gcst->loc))
            return gcst->loc.cp;
    }

    /*
     * Try other save levels of the initial space, or of global space if the
     * original space was system space.  In the latter case, try all
     * levels, and its stable allocator.
     */

    switch (mem->space) {
    default:			/* system */
        other = gcst->space_global;
        if (other->stable_memory != (const gs_memory_t *)other) {
            gcst->loc.memory = (gs_ref_memory_t *)other->stable_memory;
            gcst->loc.cp = 0;
            if (clump_locate(ptr, &gcst->loc))
                return gcst->loc.cp;
        }
        gcst->loc.memory = other;
        break;
    case avm_global:
        gcst->loc.memory = gcst->space_global;
        break;
    case avm_local:
        gcst->loc.memory = gcst->space_local;
        break;
    }
    for (;;) {
        if (gcst->loc.memory != mem) {	/* don't do twice */
            gcst->loc.cp = 0;
            if (clump_locate(ptr, &gcst->loc))
                return gcst->loc.cp;
        }
        if (gcst->loc.memory->saved == 0)
            break;
        gcst->loc.memory = &gcst->loc.memory->saved->state;
    }

    /* Restore locator to a legal state and report failure. */

    gcst->loc.memory = mem;
    gcst->loc.cp = 0;
    return 0;
}

/* ================ Debugging ================ */

#ifdef DEBUG

/* Define the structure for temporarily saving allocator state. */
typedef struct alloc_temp_save_s {
        clump_t *cc;
        obj_size_t rsize;
        ref rlast;
} alloc_temp_save_t;
/* Temporarily save the state of an allocator. */
static void
alloc_temp_save(alloc_temp_save_t *pats, gs_ref_memory_t *mem)
{
    obj_header_t *rcur;

    pats->cc = mem->cc;
    if (mem->cc == NULL)
        return;
    rcur = mem->cc->rcur;
    if (rcur != 0) {
        pats->rsize = rcur[-1].o_size;
        rcur[-1].o_size = mem->cc->rtop - (byte *) rcur;
        /* Create the final ref, reserved for the GC. */
        pats->rlast = ((ref *) mem->cc->rtop)[-1];
        make_mark((ref *) mem->cc->rtop - 1);
    }
}
/* Restore the temporarily saved state. */
static void
alloc_temp_restore(alloc_temp_save_t *pats, gs_ref_memory_t *mem)
{
    obj_header_t *rcur;

    if (mem->cc && (rcur = mem->cc->rcur) != NULL) {
        rcur[-1].o_size = pats->rsize;
        ((ref *) mem->cc->rtop)[-1] = pats->rlast;
    }
    mem->cc = pats->cc;
}

/* Validate the contents of an allocator. */
void
ialloc_validate_spaces(const gs_dual_memory_t * dmem)
{
    int i;
    gc_state_t state;
    alloc_temp_save_t
        save[countof(dmem->spaces_indexed)],
        save_stable[countof(dmem->spaces_indexed)];
    gs_ref_memory_t *mem;

    state.spaces = dmem->spaces;
    state.loc.memory = state.space_local;
    state.loc.cp = 0;
    state.heap = dmem->current->non_gc_memory;  /* valid 'heap' needed for printing */

    /* Save everything we need to reset temporarily. */

    for (i = 0; i < countof(save); i++)
        if ((mem = dmem->spaces_indexed[i]) != 0) {
            alloc_temp_save(&save[i], mem);
            if (mem->stable_memory != (gs_memory_t *)mem)
                alloc_temp_save(&save_stable[i],
                                (gs_ref_memory_t *)mem->stable_memory);
        }

    /* Validate memory. */

    for (i = 0; i < countof(save); i++)
        if ((mem = dmem->spaces_indexed[i]) != 0) {
            ialloc_validate_memory(mem, &state);
            if (mem->stable_memory != (gs_memory_t *)mem)
                ialloc_validate_memory((gs_ref_memory_t *)mem->stable_memory,
                                       &state);
        }

    /* Undo temporary changes. */

    for (i = 0; i < countof(save); i++)
        if ((mem = dmem->spaces_indexed[i]) != 0) {
            if (mem->stable_memory != (gs_memory_t *)mem)
                alloc_temp_restore(&save_stable[i],
                                   (gs_ref_memory_t *)mem->stable_memory);
            alloc_temp_restore(&save[i], mem);
        }
}
void
ialloc_validate_memory(const gs_ref_memory_t * mem, gc_state_t * gcst)
{
    const gs_ref_memory_t *smem;
    int level;

    for (smem = mem, level = 0; smem != 0;
         smem = &smem->saved->state, --level
        ) {
        clump_splay_walker sw;
        const clump_t *cp;
        int i;

        if_debug3m('6', (gs_memory_t *)mem, "[6]validating memory "PRI_INTPTR", space %d, level %d\n",
                   (intptr_t) mem, mem->space, level);
        /* Validate clumps. */
        for (cp = clump_splay_walk_init(&sw, smem); cp != 0; cp = clump_splay_walk_fwd(&sw))
            if (do_validate_clump(cp, gcst)) {
                mlprintf3((gs_memory_t *)mem, "while validating memory "PRI_INTPTR", space %d, level %d\n",
                          (intptr_t) mem, mem->space, level);
                gs_abort(gcst->heap);
            }
        /* Validate freelists. */
        for (i = 0; i < num_freelists; ++i) {
            uint free_size = i << log2_obj_align_mod;
            const obj_header_t *pfree;

            for (pfree = mem->freelists[i]; pfree != 0;
                 pfree = *(const obj_header_t * const *)pfree
                ) {
                obj_size_t size = pfree[-1].o_size;

                if (pfree[-1].o_type != &st_free) {
                    mlprintf3((gs_memory_t *)mem, "Non-free object "PRI_INTPTR"(%u) on freelist %i!\n",
                              (intptr_t) pfree, size, i);
                    break;
                }
                if ((i == LARGE_FREELIST_INDEX && size < max_freelist_size) ||
                 (i != LARGE_FREELIST_INDEX &&
                 (size < free_size - obj_align_mask || size > free_size))) {
                    mlprintf3((gs_memory_t *)mem, "Object "PRI_INTPTR"(%u) size wrong on freelist %i!\n",
                              (intptr_t) pfree, (uint)size, i);
                    break;
                }
                if (pfree == *(const obj_header_t* const*)pfree)
                    break;
            }
        }
    };
}

/* Check the validity of an object's size. */
static inline bool
object_size_valid(const obj_header_t * pre, uint size, const clump_t * cp)
{
    return (pre->o_alone ? (const byte *)pre == cp->cbase :
            size <= cp->ctop - (const byte *)(pre + 1));
}

/* Validate all the objects in a clump. */
#if IGC_PTR_STABILITY_CHECK
void ialloc_validate_pointer_stability(const obj_header_t * ptr_from,
                                   const obj_header_t * ptr_to);
static int ialloc_validate_ref(const ref *, gc_state_t *, const obj_header_t *pre_fr);
static int ialloc_validate_ref_packed(const ref_packed *, gc_state_t *, const obj_header_t *pre_fr);
#else
static int ialloc_validate_ref(const ref *, gc_state_t *);
static int ialloc_validate_ref_packed(const ref_packed *, gc_state_t *);
#endif
static int
do_validate_clump(const clump_t * cp, gc_state_t * gcst)
{
    int ret = 0;

    if_debug_clump('6', gcst->heap, "[6]validating clump", cp);
    SCAN_CLUMP_OBJECTS(cp);
    DO_ALL
        if (pre->o_type == &st_free) {
            if (!object_size_valid(pre, size, cp)) {
                lprintf3("Bad free object "PRI_INTPTR"(%lu), in clump "PRI_INTPTR"!\n",
                         (intptr_t) (pre + 1), (ulong) size, (intptr_t) cp);
                return 1;
            }
        } else if (do_validate_object(pre + 1, cp, gcst)) {
            dmprintf_clump(gcst->heap, "while validating clump", cp);
            return 1;
        }
    if_debug3m('7', gcst->heap, " [7]validating %s(%lu) "PRI_INTPTR"\n",
               struct_type_name_string(pre->o_type),
               (ulong) size, (intptr_t) pre);
    if (pre->o_type == &st_refs) {
        const ref_packed *rp = (const ref_packed *)(pre + 1);
        const char *end = (const char *)rp + size;

        while ((const char *)rp < end) {
#	    if IGC_PTR_STABILITY_CHECK
            ret = ialloc_validate_ref_packed(rp, gcst, pre);
#	    else
            ret = ialloc_validate_ref_packed(rp, gcst);
#	    endif
            if (ret) {
                mlprintf3(gcst->heap, "while validating %s(%lu) "PRI_INTPTR"\n",
                         struct_type_name_string(pre->o_type),
                         (ulong) size, (intptr_t) pre);
                dmprintf_clump(gcst->heap, "in clump", cp);
                return ret;
            }
            rp = packed_next(rp);
        }
    } else {
        struct_proc_enum_ptrs((*proc)) = pre->o_type->enum_ptrs;
        uint index = 0;
        enum_ptr_t eptr;
        gs_ptr_type_t ptype;

        if (proc != gs_no_struct_enum_ptrs)
            for (; (ptype = (*proc) (gcst_get_memory_ptr(gcst),
                                     pre + 1, size, index, &eptr,
                                     pre->o_type, gcst)) != 0; ++index) {
                if (eptr.ptr == 0)
                    DO_NOTHING;
                /* NB check other types ptr_string_type, etc. */
                else if (ptype == ptr_struct_type) {
                    ret = do_validate_object(eptr.ptr, NULL, gcst);
#		    if IGC_PTR_STABILITY_CHECK
                        ialloc_validate_pointer_stability(pre,
                                         (const obj_header_t *)eptr.ptr - 1);
#		    endif
                } else if (ptype == ptr_ref_type)
#		    if IGC_PTR_STABILITY_CHECK
                    ret = ialloc_validate_ref_packed(eptr.ptr, gcst, pre);
#		    else
                    ret = ialloc_validate_ref_packed(eptr.ptr, gcst);
#		    endif
                if (ret) {
                    dmprintf_clump(gcst->heap, "while validating clump", cp);
                    return ret;
                }
            }
    }
    END_OBJECTS_SCAN
    return ret;
}

void
ialloc_validate_clump(const clump_t * cp, gc_state_t * gcst)
{
    if (do_validate_clump(cp, gcst))
        gs_abort(gcst->heap);
}

/* Validate a ref. */
#if IGC_PTR_STABILITY_CHECK
static int
ialloc_validate_ref_packed(const ref_packed * rp, gc_state_t * gcst, const obj_header_t *pre_fr)
{
    const gs_memory_t *cmem = gcst->spaces.memories.named.system->stable_memory;

    if (r_is_packed(rp)) {
        ref unpacked;

        packed_get(cmem, rp, &unpacked);
        return ialloc_validate_ref(&unpacked, gcst, pre_fr);
    } else {
        return ialloc_validate_ref((const ref *)rp, gcst, pre_fr);
    }
}
#else
static int
ialloc_validate_ref_packed(const ref_packed * rp, gc_state_t * gcst)
{
    const gs_memory_t *cmem = gcst->spaces.memories.named.system->stable_memory;

    if (r_is_packed(rp)) {
        ref unpacked;

        packed_get(cmem, rp, &unpacked);
        return ialloc_validate_ref(&unpacked, gcst);
    } else {
        return ialloc_validate_ref((const ref *)rp, gcst);
    }
}
#endif
static int
ialloc_validate_ref(const ref * pref, gc_state_t * gcst
#		    if IGC_PTR_STABILITY_CHECK
                        , const obj_header_t *pre_fr
#		    endif
                    )
{
    const void *optr;
    const ref *rptr;
    const char *tname;
    uint size;
    const gs_memory_t *cmem = gcst->spaces.memories.named.system->stable_memory;
    int ret = 0;

    if (!gs_debug_c('?'))
        return 0;			/* no check */
    if (r_space(pref) == avm_foreign)
        return 0;
    switch (r_type(pref)) {
        case t_file:
            optr = pref->value.pfile;
            goto cks;
        case t_device:
            optr = pref->value.pdevice;
            goto cks;
        case t_fontID:
        case t_struct:
        case t_astruct:
            optr = pref->value.pstruct;
cks:	    if (optr != 0) {
                ret = do_validate_object(optr, NULL, gcst);
#		if IGC_PTR_STABILITY_CHECK
                    ialloc_validate_pointer_stability(pre_fr,
                                             (const obj_header_t *)optr - 1);
#		endif
                if (ret) {
                    lprintf1("while validating 0x%"PRIx64" (fontID/struct/astruct)\n",
                             (uint64_t)pref);
                    return ret;
                }
            }
            break;
        case t_name:
            if (name_index_ptr(cmem, name_index(cmem, pref)) != pref->value.pname) {
                lprintf3("At "PRI_INTPTR", bad name %u, pname = "PRI_INTPTR"\n",
                         (intptr_t) pref, (uint)name_index(cmem, pref),
                         (intptr_t) pref->value.pname);
                ret = 1;
                break;
            } {
                ref sref;

                name_string_ref(cmem, pref, &sref);
                if (r_space(&sref) != avm_foreign &&
                    !gc_locate(sref.value.const_bytes, gcst)
                    ) {
                    lprintf4("At "PRI_INTPTR", bad name %u, pname = "PRI_INTPTR", string "PRI_INTPTR" not in any clump\n",
                             (intptr_t) pref, (uint) r_size(pref),
                             (intptr_t) pref->value.pname,
                             (intptr_t) sref.value.const_bytes);
                    ret = 1;
                }
            }
            break;
        case t_string:
            if (r_size(pref) != 0 && !gc_locate(pref->value.bytes, gcst)) {
                lprintf3("At "PRI_INTPTR", string ptr "PRI_INTPTR"[%u] not in any clump\n",
                         (intptr_t) pref, (intptr_t) pref->value.bytes,
                         (uint) r_size(pref));
                ret = 1;
            }
            break;
        case t_array:
            if (r_size(pref) == 0)
                break;
            rptr = pref->value.refs;
            size = r_size(pref);
            tname = "array";
cka:	    if (!gc_locate(rptr, gcst)) {
                lprintf3("At "PRI_INTPTR", %s "PRI_INTPTR" not in any clump\n",
                         (intptr_t) pref, tname, (intptr_t) rptr);
                ret = 1;
                break;
            } {
                uint i;

                for (i = 0; i < size; ++i) {
                    const ref *elt = rptr + i;

                    if (r_is_packed(elt)) {
                        lprintf5("At "PRI_INTPTR", %s "PRI_INTPTR"[%u] element %u is not a ref\n",
                                 (intptr_t) pref, tname, (intptr_t) rptr, size, i);
                        ret = 1;
                    }
                }
            }
            break;
        case t_shortarray:
        case t_mixedarray:
            if (r_size(pref) == 0)
                break;
            optr = pref->value.packed;
            if (!gc_locate(optr, gcst)) {
                lprintf2("At "PRI_INTPTR", packed array "PRI_INTPTR" not in any clump\n",
                         (intptr_t) pref, (intptr_t) optr);
                ret = 1;
            }
            break;
        case t_dictionary:
            {
                const dict *pdict = pref->value.pdict;

                if (!r_has_type(&pdict->values, t_array) ||
                    !r_is_array(&pdict->keys) ||
                    !r_has_type(&pdict->count, t_integer) ||
                    !r_has_type(&pdict->maxlength, t_integer)
                    ) {
                    lprintf2("At "PRI_INTPTR", invalid dict "PRI_INTPTR"\n",
                             (intptr_t) pref, (intptr_t) pdict);
                    ret = 1;
                }
                rptr = (const ref *)pdict;
            }
            size = sizeof(dict) / sizeof(ref);
            tname = "dict";
            goto cka;
    }
    return ret;
}

#if IGC_PTR_STABILITY_CHECK
/* Validate an pointer stability. */
void
ialloc_validate_pointer_stability(const obj_header_t * ptr_fr,
                                   const obj_header_t * ptr_to)
{
    static const char *sn[] = {"undef", "undef", "system", "undef",
                "global_stable", "global", "local_stable", "local"};

    if (ptr_fr->d.o.space_id < ptr_to->d.o.space_id) {
        const char *sn_fr = (ptr_fr->d.o.space_id < count_of(sn)
                        ? sn[ptr_fr->d.o.space_id] : "unknown");
        const char *sn_to = (ptr_to->d.o.space_id < count_of(sn)
                        ? sn[ptr_to->d.o.space_id] : "unknown");

        lprintf6("Reference to a less stable object "PRI_INTPTR"<%s> "
                 "in the space \'%s\' from "PRI_INTPTR"<%s> in the space \'%s\' !\n",
                 (intptr_t) ptr_to, ptr_to->d.o.t.type->sname, sn_to,
                 (intptr_t) ptr_fr, ptr_fr->d.o.t.type->sname, sn_fr);
    }
}
#endif

/* Validate an object. */
static int
do_validate_object(const obj_header_t * ptr, const clump_t * cp,
                       gc_state_t * gcst)
{
    const obj_header_t *pre = ptr - 1;
    ulong size = pre_obj_contents_size(pre);
    gs_memory_type_ptr_t otype = pre->o_type;
    const char *oname;

    if (!gs_debug_c('?'))
        return 0;			/* no check */
    if (cp == 0 && gcst != 0) {
        gc_state_t st;

        st = *gcst;		/* no side effects! */
        if (!(cp = gc_locate(pre, &st))) {
            mlprintf1(gcst->heap, "Object "PRI_INTPTR" not in any clump!\n",
                      (intptr_t) ptr);
            return 1;		/*gs_abort(); */
        }
    }
    if (otype == &st_free) {
        mlprintf3(gcst->heap, "Reference to free object "PRI_INTPTR"(%lu), in clump "PRI_INTPTR"!\n",
                 (intptr_t) ptr, (ulong) size, (intptr_t) cp);
        return 1;
    }
    if ((cp != 0 && !object_size_valid(pre, size, cp)) ||
        otype->ssize == 0 ||
        (oname = struct_type_name_string(otype),
         *oname < 33 || *oname > 126)
        ) {
        mlprintf2(gcst->heap, "\n Bad object "PRI_INTPTR"(%lu),\n",
                  (intptr_t) ptr, (ulong) size);
        dmprintf2(gcst->heap, " ssize = %u, in clump "PRI_INTPTR"!\n",
                  otype->ssize, (intptr_t) cp);
        return 1;
    }
    if (size % otype->ssize != 0) {
        mlprintf3(gcst->heap, "\n Potentially bad object "PRI_INTPTR"(%lu), in clump "PRI_INTPTR"!\n",
                  (intptr_t) ptr, (ulong) size, (intptr_t) cp);
        dmprintf3(gcst->heap, " structure name = %s, size = %lu, ssize = %u\n",
                  oname, size, otype->ssize);
        dmprintf(gcst->heap, " This can happen (and is benign) if a device has been subclassed\n");
    }

    return 0;
}

void
ialloc_validate_object(const obj_header_t * ptr, const clump_t * cp,
                       gc_state_t * gcst)
{
    if (do_validate_object(ptr, cp, gcst))
        gs_abort(gcst->heap);
}
#else /* !DEBUG */

void
ialloc_validate_spaces(const gs_dual_memory_t * dmem)
{
}

void
ialloc_validate_memory(const gs_ref_memory_t * mem, gc_state_t * gcst)
{
}

void
ialloc_validate_clump(const clump_t * cp, gc_state_t * gcst)
{
}

void
ialloc_validate_object(const obj_header_t * ptr, const clump_t * cp,
                       gc_state_t * gcst)
{
}

#endif /* (!)DEBUG */
