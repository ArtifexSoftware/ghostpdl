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
/*  icc profile cache.  at this point not really a cache but 
     a double linked list.  it is not clear to me yet that we
     really need to have a real cache.  */

#include "std.h"
#include "stdpre.h"
#include "gstypes.h"
#include "gsmemory.h"
#include "gsstruct.h"  
#include "scommon.h"
#include "gscms.h"
#include "gsicc_profilecache.h"
#include "gserrors.h"

#define ICC_CACHE_MAXPROFILE 50  

gs_private_st_ptrs3(st_profile_entry, gsicc_profile_entry_t, "gsicc_profile_entry",
		    profile_entry_enum_ptrs, profile_entry_reloc_ptrs,
		    profile, next, prev);

gs_private_st_ptrs1(st_profile_list, gsicc_profile_list_t, "gsicc_profile_list",
		    profile_list_enum_ptrs, profile_list_reloc_ptrs,
		    icc_profile_entry);

/**
 * gsicc_cache_new: Allocate a new ICC cache manager
 * Return value: Pointer to allocated manager, or NULL on failure.
 **/

gsicc_profile_list_t *
gsicc_profilelist_new(gs_memory_t *memory)
{

    gsicc_profile_list_t *result;

    /* We want this to be maintained in stable_memory.  It should not be effected by the 
       save and restores */

    result = gs_alloc_struct(memory->stable_memory, gsicc_profile_list_t, &st_profile_list,
			     "gsicc_profilelist_new");

    if ( result == NULL )
        return(NULL);

 /*   rc_init_free(result, memory->stable_memory, 1, rc_gsicc_profile_list_free);  */

    result->icc_profile_entry = NULL;
    result->num_entries = 0;
    result->memory = memory;

    return(result);

}

static void
rc_gsicc_profile_list_free(gs_memory_t * mem, void *ptr_in, client_name_t cname)
{
    /* Ending the list. */

 /*   gsicc_profile_list_t *profile_list = (gsicc_profile_list_t * ) ptr_in;
    int k;
    gsicc_profile_entry_t *profile_entry;

    profile_entry = gsicc_find_zeroref_list(profile_list);

    for( k = 0; k < profile_list->num_entries; k++){

        if ( profile_entry->next != NULL){

            gsicc_remove_profile(profile_entry,profile_list, mem);

        }

    }

    gs_free_object(mem->stable_memory, profile_list, "rc_gsicc_profile_list_free"); */

}

static void
gsicc_add_profile(gsicc_profile_list_t *profile_list, cmm_profile_t *profile, gs_memory_t *memory)
{

    gsicc_profile_entry_t *result, *nextentry;

    /* The profile has to be added in stable memory. We want them
       to be maintained across the gsave and grestore process */

    result = gs_alloc_struct(memory->stable_memory, gsicc_profile_entry_t, &st_profile_entry,
			     "gsicc_add_profile");
    
    if (profile_list->icc_profile_entry != NULL){

        /* Add where ever we are right
           now.  Later we may want to
           do this differently.  */
        
        nextentry = profile_list->icc_profile_entry->next;
        profile_list->icc_profile_entry->next = result;
        result->prev = profile_list->icc_profile_entry;
        result->next = nextentry;
        
        if (nextentry != NULL){

            nextentry->prev = result;

        }

    } else {

        result->next = NULL;
        result->prev = NULL;
        profile_list->icc_profile_entry = result;
    
    }

    profile_list->num_entries++;

}


static cmm_profile_t*
gsicc_findprofile(int64_t hash, gsicc_profile_list_t *profile_list)
{

    gsicc_profile_entry_t *curr_pos1,*curr_pos2;
    bool foundit = 0;

    /* Look through the cache for the hashcode */

    curr_pos1 = profile_list->icc_profile_entry;
    curr_pos2 = curr_pos1;

    while (curr_pos1 != NULL ){

        if (curr_pos1->profile->hashcode == hash){

            return(curr_pos1->profile);

        }

        curr_pos1 = curr_pos1->prev;

    }

    while (curr_pos2 != NULL ){

        if (curr_pos2->profile->hashcode == hash){

            return(curr_pos2->profile);

        }

        curr_pos2 = curr_pos2->next;

    }

    return(NULL);


}


/* Find entry with zero ref count and remove it */
/* may need to lock cache during this time to avoid */
/* issue in multi-threaded case */

static gsicc_profile_entry_t*
gsicc_find_zeroref_list(gsicc_profile_list_t *profile_list){

    gsicc_profile_entry_t *curr_pos1,*curr_pos2;
    bool foundit = 0;

    /* Look through the cache for zero ref count */

    curr_pos1 = profile_list->icc_profile_entry;
    curr_pos2 = curr_pos1;

    while (curr_pos1 != NULL ){

       curr_pos2 = curr_pos1;
       curr_pos1 = curr_pos1->prev;

    }

    return(curr_pos2);

}

/* Remove link from cache.  Notify CMS and free */

/*
static void
gsicc_remove_link(gsicc_link_t *link, gsicc_link_cache_t *icc_cache, gs_memory_t *memory){


    gsicc_link_t *prevlink,*nextlink;

    prevlink = link->prevlink;
    nextlink = link->nextlink;

    if (prevlink != NULL){
        prevlink->nextlink = nextlink;
    }

    if (nextlink != NULL){
        nextlink->prevlink = prevlink;
    }

    gsicc_link_free(link, memory);

}
*/



