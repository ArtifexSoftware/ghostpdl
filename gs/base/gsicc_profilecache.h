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


/*  Header for the profile cache.  Not really a cache but a double linked
    list for now.  We need to see what is going to work best for this.  */


#ifndef gsicc_profilecache_INCLUDED
#  define gsicc_profilecache_INCLUDED

gsicc_profile_cache_t* gsicc_profilecache_new(gs_memory_t *memory);
gs_color_space* gsicc_find_cs(ulong key_test, gs_state * pgs);
void gsicc_add_cs(gs_state * pgs, gs_color_space * pcs, ulong dictkey);

#endif

