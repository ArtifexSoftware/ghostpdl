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

typedef cmsHPROFILE gcmmhprofile_t;

/* Prototypes */
static unsigned int cmsGetProfileSizeFromMem(void *buffer);

cmsHPROFILE gscms_get_profile_handle(unsigned char *buffer);

void gscms_transform_color_buffer(gsicc_link_t *icclink, gsicc_bufferdesc_t *input_buff_desc,
                             gsicc_bufferdesc_t *output_buff_desc, 
                             void *inputbuffer,
                             void *outputbuffer);


void gscms_transform_color(gsicc_link_t *icclink,  
                             void *inputcolor,
                             void *outputcolor,
                             void **contextptr);

int gscms_get_link(gsicc_link_t *icclink, gcmmhprofile_t  lcms_srchandle, 
                    gcmmhprofile_t lcms_deshandle, 
                    gsicc_rendering_param_t *rendering_params, gsicc_manager_t *icc_manager);



int gscms_get_link_proof(gsicc_link_t *icclink, gcmmhprofile_t  lcms_srchandle, 
                    gcmmhprofile_t lcms_deshandle, gcmmhprofile_t lcms_proofhandle, 
                    gsicc_rendering_param_t *rendering_params, gsicc_manager_t *icc_manager);

void gscms_create(void **contextptr);

void gscms_destroy(void **contextptr);

void gscms_release_link(gsicc_link_t *icclink);

int gscms_xform_named_color(gsicc_link_t *icclink,  float tint_value, const char* ColorName, 
                        gx_color_value device_values[] ); 

void gscms_get_name2device_link(gsicc_link_t *icclink, gcmmhprofile_t  lcms_srchandle, 
                    gcmmhprofile_t lcms_deshandle, gcmmhprofile_t lcms_proofhandle, 
                    gsicc_rendering_param_t *rendering_params, gsicc_manager_t *icc_manager);


#endif

