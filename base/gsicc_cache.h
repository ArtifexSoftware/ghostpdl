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


/*  Header for the ICC profile link cache */
#ifndef gsicccache_INCLUDED
#  define gsicccache_INCLUDED

#include "gsgstate.h"
#include "gscms.h"
#include "gxcvalue.h"

/* Used in named color handling */
typedef struct gsicc_namedcolor_s {
    char *colorant_name;            /* The name */
    unsigned int name_size;         /* size of name */
    unsigned short lab[3];          /* CIELAB D50 values */
} gsicc_namedcolor_t;

gsicc_link_cache_t* gsicc_cache_new(gs_memory_t *memory);
gsicc_link_t* gsicc_findcachelink(gsicc_hashlink_t hashcode,
                                  gsicc_link_cache_t *icc_link_cache,
                                  bool includes_proof, bool includes_devlink);
void gsicc_init_buffer(gsicc_bufferdesc_t *buffer_desc, unsigned char num_chan,
                  unsigned char bytes_per_chan, bool has_alpha, bool alpha_first,
                  bool is_planar, int plane_stride, int row_stride, int num_rows,
                  int pixels_per_row);
bool gsicc_alloc_link_entry(gsicc_link_cache_t *icc_link_cache,
                            gsicc_link_t **ret_link, gsicc_hashlink_t hash,
                            bool include_softproof, bool include_devlink);
gsicc_link_t* gsicc_get_link(const gs_gstate * pgs, gx_device *dev,
                             const gs_color_space  *input_colorspace,
                             gs_color_space *output_colorspace,
                             gsicc_rendering_param_t *rendering_params,
                             gs_memory_t *memory);
gsicc_link_t* gsicc_get_link_profile(const gs_gstate *pgs, gx_device *dev,
                                     cmm_profile_t *gs_input_profile,
                                     cmm_profile_t *gs_output_profile,
                                     gsicc_rendering_param_t *rendering_params,
                                     gs_memory_t *memory, bool devicegraytok);
void gsicc_release_link(gsicc_link_t *icclink);
void gsicc_link_free(gsicc_link_t *icc_link);
bool gsicc_profiles_equal(cmm_profile_t *profile1, cmm_profile_t *profile2);
void gsicc_get_icc_buff_hash(unsigned char *buffer, int64_t *hash, unsigned int buff_size);
int64_t gsicc_get_hash(cmm_profile_t *profile);
int gsicc_transform_named_color(const float tint_values[],
                            gsicc_namedcolor_t color_names[],
                            uint num_names,
                            gx_color_value device_values[],
                            const gs_gstate *pgs, gx_device *dev,
                            cmm_profile_t *gs_output_profile,
                            gsicc_rendering_param_t *rendering_params);
bool gsicc_support_named_color(const gs_color_space *pcs, const gs_gstate *pgs);
int  gsicc_get_device_profile_comps(const cmm_dev_profile_t *dev_profile);
gsicc_link_t * gsicc_alloc_link_dev(gs_memory_t *memory, cmm_profile_t *src_profile,
    cmm_profile_t *des_profile, gsicc_rendering_param_t *rendering_params);
void gsicc_free_link_dev(gsicc_link_t *link);
#endif
