/* Copyright (C) 2001-2019 Artifex Software, Inc.
   All Rights Reserved.

   This software is provided AS-IS with no warranty, either express or
   implied.

   This software is distributed under license and may not be copied,
   modified or distributed except as expressly authorized under the terms
   of the license contained in the file LICENSE in this distribution.

   Refer to licensing information at http://www.artifex.com or contact
   Artifex Software, Inc.,  1305 Grant Avenue - Suite 200, Novato,
   CA 94945, U.S.A., +1(415)492-9861, for further information.
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
byte* gsicc_getv2buffer(cmm_profile_t *srcprofile, int *size);
byte* gsicc_create_getv2buffer(const gs_gstate *pgs, 
                                cmm_profile_t *srcprofile, int *size);
#endif
