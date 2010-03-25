/* Copyright (C) 2001-2009 Artifex Software, Inc.
   All Rights Reserved.
  
   This software is provided AS-IS with no warranty, either express or
   implied.

   This software is distributed under license and may not be copied, modified
   or distributed except as expressly authorized under the terms of that
   license.  Refer to licensing information at http://www.artifex.com/
   or contact Artifex Software, Inc.,  7 Mt. Lassen Drive - Suite A-134,
   San Rafael, CA  94903, U.S.A., +1(415)492-9861, for further information.
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

#define ICC_CACHE_MAXPROFILE 50 

/* Static prototypes */
static void rc_gsicc_profile_cache_free(gs_memory_t * mem, void *ptr_in, 
                                        client_name_t cname);
static void gsicc_remove_cs_entry(gsicc_profile_cache_t *profile_cache);

gs_private_st_ptrs3(st_profile_entry, gsicc_profile_entry_t, 
                    "gsicc_profile_entry", profile_entry_enum_ptrs, 
                    profile_entry_reloc_ptrs, color_space, next, prev);
gs_private_st_ptrs2(st_profile_cache, gsicc_profile_cache_t, 
                    "gsicc_profile_cache", profile_list_enum_ptrs, 
                    profile_list_reloc_ptrs, first_entry, last_entry);

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
    result = gs_alloc_struct(memory->stable_memory, gsicc_profile_cache_t, 
                             &st_profile_cache, "gsicc_profilecache_new");
    if ( result == NULL )
        return(NULL);
    rc_init_free(result, memory->stable_memory, 1, rc_gsicc_profile_cache_free);
    result->first_entry = NULL;
    result->last_entry = NULL;
    result->num_entries = 0;
    result->memory = memory;
    return(result);
}

static void
rc_gsicc_profile_cache_free(gs_memory_t * mem, void *ptr_in, client_name_t cname)
{
    gsicc_profile_cache_t *profile_cache = (gsicc_profile_cache_t * ) ptr_in;
    gsicc_profile_entry_t *curr_entry;

    curr_entry = profile_cache->first_entry;
    while (curr_entry != NULL ){
        rc_decrement(curr_entry->color_space, "rc_gsicc_profile_cache_free");
        gs_free_object(mem->stable_memory, curr_entry, 
                       "rc_gsicc_profile_cache_free");
        profile_cache->num_entries--;
        curr_entry = curr_entry->next;
    }
    gs_free_object(mem->stable_memory, profile_cache, 
                   "rc_gsicc_profile_cache_free"); 
}

void
gsicc_add_cs(gs_state * pgs, gs_color_space * colorspace, ulong dictkey)
{
    gsicc_profile_entry_t *result;
    gsicc_profile_cache_t *profile_cache = pgs->icc_profile_cache;
    gs_memory_t *memory =  pgs->memory;

    /* The entry has to be added in stable memory. We want them
       to be maintained across the gsave and grestore process */
    result = gs_alloc_struct(memory->stable_memory, gsicc_profile_entry_t, 
                                &st_profile_entry, "gsicc_add_cs");
    /* If needed, remove an entry */
    if (profile_cache->num_entries > ICC_CACHE_MAXPROFILE) {
        gsicc_remove_cs_entry(profile_cache);
    }
    if (profile_cache->first_entry != NULL){
        /* Add to the top of the list. 
           That way we find the MRU enty right away.
           Last entry stays the same. */
        profile_cache->first_entry->prev = result;
        result->prev = NULL;
        result->next = profile_cache->first_entry;
        profile_cache->first_entry = result; /* MRU */
    } else {
        /* First one to add */
        result->next = NULL;
        result->prev = NULL;
        profile_cache->first_entry = result;
        profile_cache->last_entry = result;
    }
    result->color_space = colorspace;
    rc_increment(colorspace);
    result->key = dictkey;
    profile_cache->num_entries++;
}

gs_color_space*
gsicc_find_cs(ulong key_test, gs_state * pgs)
{
    gsicc_profile_entry_t *curr_pos1,*temp1,*temp2,*last;
    gsicc_profile_cache_t *profile_cache = pgs->icc_profile_cache;

    /* Look through the cache for the key. If found, move to MRU */
    curr_pos1 = profile_cache->first_entry;
    while (curr_pos1 != NULL ){
        if (curr_pos1->key == key_test){
            /* First test for MRU. Also catch case of 1 entry */
            if (curr_pos1 == profile_cache->first_entry) {
                return(curr_pos1->color_space);
            }
            /* We need to move found one to the top of the list. */
            last = profile_cache->last_entry;
            temp1 = curr_pos1->prev;
            temp2 = curr_pos1->next;
            if (temp1 != NULL) {
                temp1->next = temp2;
            } 
            if (temp2 != NULL) {
                temp2->prev = temp1;
            }
            /* Update last entry if we grabbed the bottom one */
            if (curr_pos1 == profile_cache->last_entry) {
                profile_cache->last_entry = profile_cache->last_entry->prev;
            }
            curr_pos1->prev = NULL;
            curr_pos1->next = profile_cache->first_entry;
            profile_cache->first_entry->prev = curr_pos1;
            profile_cache->first_entry = curr_pos1;
            return(curr_pos1->color_space);
        }
        curr_pos1 = curr_pos1->next;
    }
    return(NULL);
}

/* Remove the LRU entry, which ideally is at the bottom */
static void
gsicc_remove_cs_entry(gsicc_profile_cache_t *profile_cache){

    gsicc_profile_entry_t *last_entry = profile_cache->last_entry;
    gs_memory_t *memory = profile_cache->memory;

    if (profile_cache->last_entry->prev != NULL) {
        profile_cache->last_entry->prev->next = NULL;
        profile_cache->last_entry = profile_cache->last_entry->prev;
    } else {
        /* No entries */
        profile_cache->first_entry = NULL;
        profile_cache->last_entry = NULL;
    }
    rc_decrement(last_entry->color_space, "gsicc_remove_cs_entry");
    gs_free_object(memory->stable_memory, last_entry, "gsicc_remove_cs_entry");
    profile_cache->num_entries--;
}