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

/*  A cache for icc colorspaces that  were created from PS CIE color
    spaces or from PDF cal color spaces.
*/

#include "std.h"
#include "stdpre.h"
#include "gstypes.h"
#include "gsmemory.h"
#include "gsstruct.h"
#include "scommon.h"
#include "gx.h"
#include "gzstate.h"
#include "gscms.h"
#include "gsicc_profilecache.h"
#include "gserrors.h"
#include "assert_.h"

#define ICC_CACHE_MAXPROFILE 50

/* Static prototypes */
static void rc_gsicc_profile_cache_free(gs_memory_t * mem, void *ptr_in,
                                        client_name_t cname);
static void gsicc_remove_cs_entry(gsicc_profile_cache_t *profile_cache);

gs_private_st_ptrs2(st_profile_entry, gsicc_profile_entry_t,
                    "gsicc_profile_entry", profile_entry_enum_ptrs,
                    profile_entry_reloc_ptrs, color_space, next);
gs_private_st_ptrs1(st_profile_cache, gsicc_profile_cache_t,
                    "gsicc_profile_cache", profile_list_enum_ptrs,
                    profile_list_reloc_ptrs, head);

/**
 * gsicc_cache_new: Allocate a new ICC cache manager
 * Return value: Pointer to allocated manager, or NULL on failure.
 **/
gsicc_profile_cache_t *
gsicc_profilecache_new(gs_memory_t *memory)
{
    gsicc_profile_cache_t *result;

    /* We want this to be maintained in stable_memory.  It should not be effected by the
       save and restores */
    memory = memory->stable_memory;
    result = gs_alloc_struct(memory, gsicc_profile_cache_t,
                             &st_profile_cache, "gsicc_profilecache_new");
    if ( result == NULL )
        return(NULL);
    rc_init_free(result, memory, 1, rc_gsicc_profile_cache_free);
    result->head = NULL;
    result->num_entries = 0;
    result->memory = memory;
    return(result);
}

static void
rc_gsicc_profile_cache_free(gs_memory_t * mem, void *ptr_in, client_name_t cname)
{
    gsicc_profile_cache_t *profile_cache = (gsicc_profile_cache_t * ) ptr_in;
    gsicc_profile_entry_t *curr = profile_cache->head, *next;

    assert(mem->stable_memory == profile_cache->memory);
    while (curr != NULL ){
        next = curr->next;
        rc_decrement(curr->color_space, "rc_gsicc_profile_cache_free");
        gs_free_object(profile_cache->memory, curr,
                       "rc_gsicc_profile_cache_free");
        profile_cache->num_entries--;
        curr = next;
    }
#ifdef DEBUG
    if (profile_cache->num_entries != 0)
        emprintf1(mem,"gsicc_profile_cache_free, num_entries is %d (should be 0).\n",
                  profile_cache->num_entries);
#endif
    gs_free_object(profile_cache->memory, profile_cache,
                   "rc_gsicc_profile_cache_free");
}

void
gsicc_add_cs(gs_gstate * pgs, gs_color_space * colorspace, uint64_t dictkey)
{
    gsicc_profile_entry_t *result;
    gsicc_profile_cache_t *profile_cache = pgs->icc_profile_cache;
    gs_memory_t *memory =  profile_cache->memory;

    if (dictkey == 0)
        return;

    /* The entry has to be added in stable memory. We want them
       to be maintained across the gsave and grestore process */
    result = gs_alloc_struct(memory, gsicc_profile_entry_t,
                             &st_profile_entry, "gsicc_add_cs");
    if (result == NULL)
        return;			/* FIXME */

    /* If needed, remove an entry (the last one) */
    if (profile_cache->num_entries >= ICC_CACHE_MAXPROFILE) {
        gsicc_remove_cs_entry(profile_cache);
    }
    /* Add to the top of the list. That way we find the MRU enty right away.
       Last entry stays the same. */
    result->next = profile_cache->head;
    profile_cache->head = result; /* MRU */
    result->color_space = colorspace;
    rc_increment(colorspace);
    result->key = dictkey;
    if_debug2m(gs_debug_flag_icc, memory,
               "[icc] Add cs to cache = "PRI_INTPTR", hash = %"PRIu64"\n",
               (intptr_t)result->color_space, (uint64_t)result->key);
    profile_cache->num_entries++;
}

gs_color_space*
gsicc_find_cs(uint64_t key_test, gs_gstate * pgs)
{
    gsicc_profile_cache_t *profile_cache = pgs->icc_profile_cache;
    gsicc_profile_entry_t *prev = NULL, *curr = profile_cache->head;

    if (key_test == 0)
        return NULL;

    /* Look through the cache for the key. If found, move to MRU */
    while (curr != NULL ){
        if (curr->key == key_test){
            if_debug2m(gs_debug_flag_icc, pgs->memory,
                       "[icc] Found cs = "PRI_INTPTR", hash = %"PRIu64"\n",
                       (intptr_t)curr->color_space, (uint64_t)curr->key);
            /* If not already at head of list, move this one there */
            if (curr != profile_cache->head) {
                /* We need to move found one to the top of the list. */
                prev->next = curr->next;
                curr->next = profile_cache->head;
                profile_cache->head = curr;
            }
            return(curr->color_space);
        }
        prev = curr;
        curr = curr->next;
    }
    return(NULL);
}

/* Remove the LRU entry, which ideally is at the bottom. Note that there
   is no need to have a ref_count in this structure since the color
   space objects that are the member variables are reference counted themselves */
static void
gsicc_remove_cs_entry(gsicc_profile_cache_t *profile_cache)
{
    gs_memory_t *memory = profile_cache->memory;
    gsicc_profile_entry_t *prev = NULL, *curr = profile_cache->head;

#ifdef DEBUG
    if (curr == NULL) {
        emprintf(memory, " attempt to remove from an empty profile cache.\n");
        return; /* gs_abort(); */
    }
#endif
    while (curr->next != NULL) {
        prev = curr;
        curr = curr->next;
    }
    profile_cache->num_entries--;
    if (prev == NULL) {
        /* No more entries */
        profile_cache->head = NULL;
#ifdef DEBUG
    if (profile_cache->num_entries != 0) {
        emprintf1(memory, "profile cache list empty, but list has num_entries=%d.\n",
                  profile_cache->num_entries);
    }
#endif
    } else {
        prev->next = NULL;	/* new tail */
    }
    /* Decremented, but someone could still be referencing this */
    /* If found again in the source document, it will be regenerated
       and added back into the cache. */
    if_debug2m(gs_debug_flag_icc, memory,
               "[icc] Remove cs from cache = "PRI_INTPTR", hash = %"PRIu64"\n",
               (intptr_t)curr->color_space, (uint64_t)curr->key);
    rc_decrement(curr->color_space, "gsicc_remove_cs_entry");
    gs_free_object(memory, curr, "gsicc_remove_cs_entry");
}
