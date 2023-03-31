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


/*  Header for the profile cache.  Not really a cache but a double linked
    list for now.  We need to see what is going to work best for this.  */

#ifndef gsicc_profilecache_INCLUDED
#  define gsicc_profilecache_INCLUDED

#include "gscms.h"

gsicc_profile_cache_t* gsicc_profilecache_new(gs_memory_t *memory);
gs_color_space* gsicc_find_cs(uint64_t key_test, gs_gstate * pgs);
void gsicc_add_cs(gs_gstate * pgs, gs_color_space * pcs, uint64_t dictkey);

#endif
