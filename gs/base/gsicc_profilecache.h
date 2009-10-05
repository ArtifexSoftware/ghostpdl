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


/*  Header for the profile cache.  Not really a cache in but a double linked
    list for now.  We need to see what is going to work best for this.  */


#ifndef gsicc_profilecache_INCLUDED
#  define gsicc_profilecache_INCLUDED


gsicc_profile_list_t * gsicc_profilelist_new(gs_memory_t *memory);

static void rc_gsicc_profile_list_free(gs_memory_t * mem, void *ptr_in, client_name_t cname);

static void gsicc_add_profile(gsicc_profile_list_t *profile_list, 
                              cmm_profile_t *profile, gs_memory_t *memory);

static cmm_profile_t* gsicc_findprofile(int64_t hash, gsicc_profile_list_t *profile_list);

static gsicc_profile_entry_t* gsicc_find_zeroref_list(gsicc_profile_list_t *profile_list);

static void gsicc_remove_profile(gsicc_profile_entry_t *profile_entry, 
        gsicc_profile_list_t *profile_list, gs_memory_t *memory);
#endif

