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


/* Get the link from the CMS. TODO:  Add error checking */
int
gscms_get_link(gsicc_link_t *icclink, gsicc_colorspace_t  *input_colorspace, 
                    gsicc_colorspace_t *output_colorspace, 
                    gsicc_rendering_param_t *rendering_params)
{

    cmsHPROFILE lcms_srchandle, lcms_deshandle;
    cmsHTRANSFORM lcms_link;
    DWORD src_data_type,des_data_type;
    icColorSpaceSignature src_color_space,des_color_space;
    int src_nChannels,des_nChannels;
    void* input_buf = input_colorspace->ProfileData->ProfileRawBuf;
    void* output_buf = output_colorspace->ProfileData->ProfileRawBuf;
    int input_size = input_colorspace->ProfileData->iccheader.size;
    int output_size = output_colorspace->ProfileData->iccheader.size;

/* At this stage we have the ICC profiles as buffers.
    we may later change some of this to deal with 
    streams. */

    lcms_srchandle = cmsOpenProfileFromMem(input_buf,input_size);
    lcms_deshandle = cmsOpenProfileFromMem(output_buf,output_size);

/* Get the data types */

    src_color_space  = cmsGetColorSpace(lcms_srchandle);  
    des_color_space  = cmsGetColorSpace(lcms_deshandle); 

    src_nChannels = _cmsChannelsOf(src_color_space);
    des_nChannels = _cmsChannelsOf(des_color_space);

    src_data_type= (CHANNELS_SH(src_nChannels)|BYTES_SH(rendering_params->input_subpixbytesize)); 
    des_data_type= (CHANNELS_SH(des_nChannels)|BYTES_SH(rendering_params->output_subpixbytesize)); 

/* Create the link */

    /* TODO:  Make intent variable */

    lcms_link = cmsCreateTransform(lcms_srchandle, src_data_type, lcms_deshandle, des_data_type, 
        INTENT_PERCEPTUAL, cmsFLAGS_LOWRESPRECALC);			

/* Close the profile handles. We need to think about if we want to cache these. */

    cmsCloseProfile(lcms_srchandle);
    cmsCloseProfile(lcms_deshandle);
  
return(0);
}



/* Get the link from the CMS, but include proofing. 
    We need to note 
    that as an option in the rendering params.  If we are doing
    transparency, that would only occur at the top of the stack 
TODO:  Add error checking */

int
gscms_get_link_proof(gsicc_link_t *icclink, gsicc_colorspace_t  *input_colorspace, 
                    gsicc_colorspace_t *output_colorspace, gsicc_colorspace_t *proof_colorspace, 
                    gsicc_rendering_param_t *rendering_params)
{

    cmsHPROFILE lcms_srchandle, lcms_deshandle, lcms_proofhandle;
    cmsHTRANSFORM lcms_link;
    DWORD src_data_type,des_data_type;
    icColorSpaceSignature src_color_space,des_color_space;
    int src_nChannels,des_nChannels;
    void* input_buf = input_colorspace->ProfileData->ProfileRawBuf;
    void* output_buf = output_colorspace->ProfileData->ProfileRawBuf;
    void* proof_buf = proof_colorspace->ProfileData->ProfileRawBuf;
    int input_size = input_colorspace->ProfileData->iccheader.size;
    int output_size = output_colorspace->ProfileData->iccheader.size;
    int proof_size = proof_colorspace->ProfileData->iccheader.size;

/* At this stage we have the ICC profiles as buffers.
    we may later change some of this to deal with 
    streams. */

    lcms_srchandle = cmsOpenProfileFromMem(input_buf,input_size);
    lcms_deshandle = cmsOpenProfileFromMem(output_buf,output_size);
    lcms_proofhandle = cmsOpenProfileFromMem(proof_buf,proof_size);


/* Get the data types */

    src_color_space  = cmsGetColorSpace(lcms_srchandle);  
    des_color_space  = cmsGetColorSpace(lcms_deshandle); 

    src_nChannels = _cmsChannelsOf(src_color_space);
    des_nChannels = _cmsChannelsOf(des_color_space);

    src_data_type= (CHANNELS_SH(src_nChannels)|BYTES_SH(rendering_params->input_subpixbytesize)); 
    des_data_type= (CHANNELS_SH(des_nChannels)|BYTES_SH(rendering_params->output_subpixbytesize)); 

/* Create the link.  Note the gamut check alarm */

    lcms_link = cmsCreateProofingTransform(lcms_srchandle, src_data_type, lcms_deshandle, des_data_type, 
        lcms_proofhandle,INTENT_PERCEPTUAL, INTENT_ABSOLUTE_COLORIMETRIC, cmsFLAGS_GAMUTCHECK | cmsFLAGS_SOFTPROOFING );			

/* Close the profile handles. We need to think about if we want to cache these. */

    cmsCloseProfile(lcms_srchandle);
    cmsCloseProfile(lcms_deshandle);
    cmsCloseProfile(lcms_proofhandle);
  
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

    if (icclink->LinkHandle !=NULL )
        cmsDeleteTransform(icclink->LinkHandle);

}





