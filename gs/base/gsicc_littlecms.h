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



void
gscms_transform_color_buffer(gsicc_link_t *icclink, gsicc_bufferdesc_t input_buff_desc,
                             gsicc_bufferdesc_t output_buff_des, 
                             void *inputbuffer,
                             void *outputbuffer,
                             void **contextptr);


int
gscms_get_link(gsicc_link_t *icclink, gsicc_colorspace_t  *input_colorspace, 
                    gsicc_colorspace_t *output_colorspace, 
                    gsicc_rendering_param_t *rendering_params,void **contextptr);



void
gscms_get_named_color(void **contextptr);

void
gscms_create(void **contextptr);

void
gscms_destroy(void **contextptr);

void
gscms_release_link(gsicc_link_t *icclink, void **contextptr);





#endif

