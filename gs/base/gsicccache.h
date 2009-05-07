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


/*  Header for the ICC profile link cache */


#ifndef gsicccache_INCLUDED
#  define gsicccache_INCLUDED

#include "gx.h"

gsicc_link_cache_t* gsicc_cache_new(gs_memory_t *memory);

static void gsicc_add_link(gsicc_link_cache_t *link_cache, void *link_handle,
               void *ContextPtr, int64_t link_hashcode, gs_memory_t *memory);

void gsicc_cache_free(gsicc_link_cache_t *icc_cache, gs_memory_t *memory);

void gsicc_link_free(gsicc_link_t *icc_link, gs_memory_t *memory);

int gsicc_get_color_info(gs_color_space *colorspace,unsigned char *color, int *size_color);

static int64_t gsicc_compute_hash(gs_color_space *input_colorspace, 
                   gs_color_space *output_colorspace, 
                   gsicc_rendering_param_t *rendering_params);

static gsicc_link_t* FindCacheLink(int64_t hashcode,gsicc_link_cache_t *icc_cache, 
                                   bool includes_proof);

static gsicc_link_t* gsicc_find_zeroref_cache(gsicc_link_cache_t *icc_cache);

static void gsicc_remove_link(gsicc_link_t *link,gsicc_link_cache_t *icc_cache, 
                              gs_memory_t *memory);

gsicc_link_t* gsicc_get_link(gs_imager_state * pis, gs_color_space  *input_colorspace, 
                    gs_color_space *output_colorspace, 
                    gsicc_rendering_param_t *rendering_params, gs_memory_t *memory, bool include_softproof);

void gsicc_release_link(gsicc_link_t *icclink);


#endif

