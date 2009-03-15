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

/* gsicc interface to littleCMS  Initial stubbing of functions.  */


#include "math_.h"
#include "memory_.h"
#include "gx.h"
#include "gserrors.h"
#include "gsiccmanage.h"
#include "lcms.h"

/* Transform an entire buffer */
void
gscms_transform_color_buffer(gsicc_link_t *icclink, gsicc_bufferdesc_t input_buff_desc,
                             gsicc_bufferdesc_t output_buff_des, 
                             void *inputbuffer,
                             void *outputbuffer)
{



}

/* Transform a single color.
   Need to think about this.
   How to do description of color.
   Is it passing a gx_device_color?
   Is it 16 bit, 8 bit, will need
   to know if gray,RGB,CMYK, or N chan
   */
/*
void
gscms_transform_color(gsicc_link_t *icclink,  
                             void *inputcolor,
                             void *outputcolor,
                             void **contextptr)
{


}
*/

/* Get the link from the CMS */
int
gscms_get_link(gsicc_link_t *icclink, gsicc_colorspace_t  *input_colorspace, 
                    gsicc_colorspace_t *output_colorspace, 
                    gsicc_rendering_param_t *rendering_params)
{


  
return(0);
}

/* Get the CIELAB value and if present the device value
   for the device name.  Supply the named color profile.
   Note that this need not be an ICC named color profile
   but can be a proprietary type table */
void
gscms_get_named_color(void **contextptr)  /* need to think about this a bit more */
{


}

/* Do any initialization if needed to the CMS */
void
gscms_create(void **contextptr)
{

}

/* Do any clean up when done with the CMS if needed */

void
gscms_destroy(void **contextptr)
{

}

/* Have the CMS release the link */
void
gscms_release_link(gsicc_link_t *icclink)
{


}





