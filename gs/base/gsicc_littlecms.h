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


/*  Header for the GS interface to littleCMS  */


#ifndef gsicc_littlecms_INCLUDED
#  define gsicc_littlecms_INCLUDED

#include "gxcvalue.h"
#include "gscms.h"

#ifndef cmm_gcmmhprofile_DEFINED
    #define cmm_gcmmhprofile_DEFINED
    typedef void* gcmmhprofile_t;
#endif

#ifndef cmm_gcmmhlink_DEFINED
    #define cmm_gcmmhlink_DEFINED
    typedef void* gcmmhlink_t;
#endif

/* Prototypes */

gcmmhprofile_t gscms_get_profile_handle_mem(unsigned char *buffer, 
                                            unsigned int input_size);
gcmmhprofile_t gscms_get_profile_handle_file(const char *filename);
void gscms_transform_color_buffer(gsicc_link_t *icclink, 
                                  gsicc_bufferdesc_t *input_buff_desc,
                             gsicc_bufferdesc_t *output_buff_desc, 
                             void *inputbuffer,
                             void *outputbuffer);
int gscms_get_input_channel_count(gcmmhprofile_t profile);
int gscms_get_output_channel_count(gcmmhprofile_t profile);
char* gscms_get_clrtname(gcmmhprofile_t profile, int colorcount);
int gscms_get_numberclrtnames(gcmmhprofile_t profile);
gsicc_colorbuffer_t gscms_get_profile_data_space(gcmmhprofile_t profile);
void gscms_transform_color(gsicc_link_t *icclink,  
                             void *inputcolor,
                             void *outputcolor,
                             int num_bytes,
                             void **contextptr);
gcmmhlink_t gscms_get_link(gcmmhprofile_t  lcms_srchandle, 
                    gcmmhprofile_t lcms_deshandle, 
                    gsicc_rendering_param_t *rendering_params);
gcmmhlink_t gscms_get_link_proof(gcmmhprofile_t  lcms_srchandle, 
                    gcmmhprofile_t lcms_deshandle, 
                    gcmmhprofile_t lcms_proofhandle, 
                    gsicc_rendering_param_t *rendering_params);
void gscms_create(void **contextptr);
void gscms_destroy(void **contextptr);
void gscms_release_link(gsicc_link_t *icclink);
void gscms_release_profile(void *profile);
int gscms_transform_named_color(gsicc_link_t *icclink,  float tint_value, 
                                const char* ColorName, 
                        gx_color_value device_values[] ); 
void gscms_get_name2device_link(gsicc_link_t *icclink, gcmmhprofile_t  lcms_srchandle, 
                    gcmmhprofile_t lcms_deshandle, gcmmhprofile_t lcms_proofhandle, 
                    gsicc_rendering_param_t *rendering_params);
#endif

