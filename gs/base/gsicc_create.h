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

#ifndef gsicc_create_INCLUDED
#  define gsicc_create_INCLUDED

#include "gscie.h"

void gsicc_create_fromcrd(unsigned char *buffer, gs_memory_t *memory);
int gsicc_create_froma(const gs_color_space *pcs, unsigned char **pp_buffer_in, 
                       int *profile_size_out, gs_memory_t *memory,
                       gx_cie_vector_cache *a_cache, 
                       gx_cie_scalar_cache *lmn_caches);
int gsicc_create_fromabc(const gs_color_space *pcs, unsigned char **buffer, 
                         int *profile_size_out, gs_memory_t *memory,
                         gx_cie_vector_cache *abc_caches, 
                         gx_cie_scalar_cache *lmn_caches, bool *islab);
int gsicc_create_fromdefg(const gs_color_space *pcs, 
                          unsigned char **pp_buffer_in, int *profile_size_out, 
                          gs_memory_t *memory, gx_cie_vector_cache *abc_caches, 
                          gx_cie_scalar_cache *lmn_caches, 
                          gx_cie_scalar_cache *defg_caches);
int gsicc_create_fromdef(const gs_color_space *pcs, unsigned char **pp_buffer_in, 
                         int *profile_size_out, gs_memory_t *memory,
                         gx_cie_vector_cache *abc_caches, 
                         gx_cie_scalar_cache *lmn_caches, 
                         gx_cie_scalar_cache *def_caches);
cmm_profile_t* gsicc_create_from_cal(float *white, float *black, float *gamma, 
                                     float *matrix, gs_memory_t *memory, 
                                     int num_colors);

#endif
