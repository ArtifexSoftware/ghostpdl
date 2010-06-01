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
void
gsicc_init_buffer(gsicc_bufferdesc_t *buffer_desc, unsigned char num_chan, 
                  unsigned char bytes_per_chan, bool has_alpha, bool alpha_first, 
                  bool is_planar, int plane_stride, int row_stride, int num_rows, 
                  int pixels_per_row);
gsicc_link_t* gsicc_get_link(const gs_imager_state * pis, 
                             const gs_color_space  *input_colorspace,
                             gs_color_space *output_colorspace,
                             gsicc_rendering_param_t *rendering_params, 
                             gs_memory_t *memory, bool include_softproof);
gsicc_link_t* gsicc_get_link_profile(gs_imager_state *pis, 
                                     cmm_profile_t *gs_input_profile,
                                     cmm_profile_t *gs_output_profile,
                                     gsicc_rendering_param_t *rendering_params, 
                                     gs_memory_t *memory, bool include_softproof);
void gsicc_release_link(gsicc_link_t *icclink);
void gsicc_get_icc_buff_hash(unsigned char *buffer, int64_t *hash, unsigned int buff_size);
int gsicc_transform_named_color(float tint_value, byte *color_name, uint name_size,
                            gx_color_value device_values[], 
                            const gs_imager_state *pis, 
                            cmm_profile_t *gs_output_profile, 
                            gsicc_rendering_param_t *rendering_params, 
                            bool include_softproof);

#endif

