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


/*  Header for the GS interface to littleCMS  */

#ifndef gsicc_littlecms_INCLUDED
#  define gsicc_littlecms_INCLUDED

#include "gxcvalue.h"
#include "gscms.h"

typedef void* gcmmhprofile_t;

typedef void* gcmmhlink_t;

/* Made to match lcms settings */
#define GSICC_HIGHACCURACY cmsFLAGS_HIGHRESPRECALC
#define GSICC_MEDACCURACY 0
#define GSICC_LOWACCURACY cmsFLAGS_LOWRESPRECALC

/* Prototypes */
bool gsicc_mcm_monitor_rgb(void *inputcolor, int num_bytes);
bool gsicc_mcm_monitor_cmyk(void *inputcolor, int num_bytes);
bool gsicc_mcm_monitor_lab(void *inputcolor, int num_bytes);
void gsicc_mcm_set_link(gsicc_link_t* link);
int gsicc_mcm_end_monitor(gsicc_link_cache_t *cache, gx_device *dev);
int gsicc_mcm_begin_monitor(gsicc_link_cache_t *cache, gx_device *dev);
gsicc_link_t* gsicc_rcm_get_link(const gs_gstate *pgs, gx_device *dev,
                                 gsicc_colorbuffer_t data_cs);
gsicc_link_t* gsicc_nocm_get_link(const gs_gstate *pgs, gx_device *dev,
                                  int num_input );
gcmmhprofile_t gscms_get_profile_handle_mem(unsigned char *buffer,
                                            unsigned int input_size,
                                            gs_memory_t *mem);
gcmmhprofile_t gscms_get_profile_handle_file(const char *filename,
                                             gs_memory_t *mem);
int gscms_transform_color_buffer(gx_device *dev, gsicc_link_t *icclink,
                                 gsicc_bufferdesc_t *input_buff_desc,
                                 gsicc_bufferdesc_t *output_buff_desc,
                                 void *inputbuffer, void *outputbuffer);
int gscms_get_channel_count(gcmmhprofile_t profile, gs_memory_t *memory);
int gscms_get_pcs_channel_count(gcmmhprofile_t profile, gs_memory_t *memory);
char* gscms_get_clrtname(gcmmhprofile_t profile, int colorcount, gs_memory_t *memory);
int gscms_get_numberclrtnames(gcmmhprofile_t profile, gs_memory_t *memory);
bool gscms_is_device_link(gcmmhprofile_t profile, gs_memory_t *memory);
int gscms_get_device_class(gcmmhprofile_t profile, gs_memory_t *memory);
bool gscms_is_input(gcmmhprofile_t profile, gs_memory_t *memory);
gsicc_colorbuffer_t gscms_get_profile_data_space(gcmmhprofile_t profile,
                                                 gs_memory_t *memory);
int gscms_transform_color(gx_device *dev, gsicc_link_t *icclink, void *inputcolor,
                          void *outputcolor, int num_bytes);
int gscms_transform_color_const(const gx_device *dev, gsicc_link_t *icclink, void *inputcolor,
                          void *outputcolor, int num_bytes);
gcmmhlink_t gscms_get_link(gcmmhprofile_t lcms_srchandle,
                           gcmmhprofile_t lcms_deshandle,
                           gsicc_rendering_param_t *rendering_params,
                           int cmm_flags,
                           gs_memory_t *memory);
gcmmhlink_t gscms_get_link_proof_devlink(gcmmhprofile_t lcms_srchandle,
                                         gcmmhprofile_t lcms_proofhandle,
                                         gcmmhprofile_t lcms_deshandle,
                                         gcmmhprofile_t lcms_devlinkhandle,
                                         gsicc_rendering_param_t *rendering_params,
                                         bool src_dev_link, int cmm_flags,
                                         gs_memory_t *memory);
void *gscms_create(gs_memory_t *memory);
void gscms_destroy(void *);
void gscms_release_link(gsicc_link_t *icclink);
void gscms_release_profile(void *profile, gs_memory_t *memory);
int gscms_transform_named_color(gsicc_link_t *icclink,  float tint_value,
                                const char* ColorName,
                                gx_color_value device_values[]);
void gscms_get_name2device_link(gsicc_link_t *icclink,
                                gcmmhprofile_t lcms_srchandle,
                                gcmmhprofile_t lcms_deshandle,
                                gcmmhprofile_t lcms_proofhandle,
                                gsicc_rendering_param_t *rendering_params);
int gscms_get_input_channel_count(gcmmhprofile_t profile, gs_memory_t *memory);
int gscms_get_output_channel_count(gcmmhprofile_t profile, gs_memory_t *memory);
void gscms_get_link_dim(gcmmhlink_t link, int *num_inputs, int *num_outputs, gs_memory_t *memory);
int gscms_avoid_white_fix_flag(gs_memory_t *memory);
bool gscms_is_threadsafe(void);
#endif
