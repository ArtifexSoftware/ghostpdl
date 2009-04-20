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
/*  GS ICC Manager.  Initial stubbing of functions.  */


/*#include "math_.h"
#include "memory_.h"
#include "gx.h"
#include "gserrors.h"
#include "gsiccmanage.h"
#include "gxistate.h"
#include "gscms.h"
#include "gscie.h"
#include "smd5.h"  
#include "gscspace.h"
*/

//#include "memory_.h"
//#include "gx.h"
//#include "gserrors.h"
//#include "gscspace.h"
//#include "gxcspace.h"
#include "gscms.h"
#include "gsiccmanage.h"
#include "gsicc_littlecms.h"


/*  Initialize the CMS 
    Prepare the ICC Manager
*/
void 
gsicc_create()
{




}


/*  Shut down the  CMS 
    and the ICC Manager
*/
void 
gsicc_destroy()
{


}

/*  This computes the hash code for the
    ICC data and assigns the code
    and the profile to DefaultGray
    member variable in the ICC manager */

void 
gsicc_set_gray_profile(cmm_profile_t graybuffer)
{


}

/*  This computes the hash code for the
    ICC data and assigns the code
    and the profile to DefaultRGB
    member variable in the ICC manager */

void 
gsicc_set_rgb_profile(cmm_profile_t rgbbuffer)
{


}

/*  This computes the hash code for the
    ICC data and assigns the code
    and the profile to DefaultCMYK
    member variable in the ICC manager */

void 
gsicc_set_cmyk_profile(cmm_profile_t cmykbuffer)
{


}

/*  This computes the hash code for the
    device profile and assigns the code
    and the profile to the DeviceProfile
    member variable in the ICC Manager
    This should really occur only one time.
    This is different than gs_set_device_profile
    which sets the profile on the output
    device */

void 
gsicc_set_device_profile(cmm_profile_t deviceprofile)
{



}

/* Set the named color profile in the Device manager */

void 
gsicc_set_device_named_color_profile(cmm_profile_t nameprofile)
{


}

/* Set the ouput device linked profile */

void 
gsicc_set_device_linked_profile(cmm_profile_t outputlinkedprofile )
{

}

/* Set the proofing profile, extract the header and compute
   its hash code */

void 
gsicc_set_proof_profile(cmm_profile_t proofprofile )
{

}

/*  This will occur only if the device did not
    return a profie when asked for one.  This
    will set DeviceProfile member variable to the
    appropriate default device.  If output device
    has more than 4 channels you need to use
    gsicc_set_device_profile with the appropriate
    ICC profile.  */

void 
gsicc_load_default_device_profile(int numchannels)
{



}


/* This is a special case of the functions gsicc_set_gray_profile
    gsicc_set_rgb_profile and gsicc_set_cmyk_profile. In
    this function the number of channels is specified and the 
    appropriate profile contained in Resources/Color/InputProfiles is used */

void 
gsicc_load_default_input_profile(int numchannels)
{


}




void 
gsicc_setbuffer_desc(gsicc_bufferdesc_t *buffer_desc,unsigned char num_chan,
    unsigned char bytes_per_chan,
    bool has_alpha,
    bool alpha_first,
    bool little_endian,
    bool is_planar,
    gs_icc_colorbuffer_t buffercolor)
{
    buffer_desc->num_chan = num_chan;
    buffer_desc->bytes_per_chan = bytes_per_chan;
    buffer_desc->has_alpha = has_alpha;
    buffer_desc->alpha_first = alpha_first;
    buffer_desc->little_endian = little_endian;
    buffer_desc->is_planar = is_planar;
    buffer_desc->buffercolor = buffercolor;

}


 /*  If we have a profile handle in the color space already, then we use that.  
     If we do not have one, then we check if there is data in the buffer.  A
     handle is created from that data and also stored in the gs color space.
     If we do not have a handle nor any ICC data, then we will use data from 
     the ICC manager that is based upon the current color space. */


 gcmmhprofile_t
 GetProfileHandle(gs_color_space *gs_colorspace, gsicc_manager_t *icc_manager)
 {
#if 0 gcmmhprofile_t profilehandle = gs_colorspace->cmm_icc_profile_data->ProfileHandle;
     void *buffer = gs_colorspace->cmm_icc_profile_data->buffer;
     gs_color_space_index color_space_index = gs_colorspace->type->index;

     if( profilehandle != NULL ){
        return(profilehandle);
     }

     if( buffer != NULL){
         return(gscms_get_profile_handle(buffer));
     }

     /* Now get a colorspace handle based upon the colorspace type */

     switch( color_space_index ){

	case gs_color_space_index_DeviceGray:

            return(gs_colorspace->cmm_icc_profile_data = &(icc_manager->DefaultGray));

            break;

	case gs_color_space_index_DeviceRGB:

            return(gs_colorspace->cmm_icc_profile_data = &(icc_manager->DefaultRGB));

            break;

	case gs_color_space_index_DeviceCMYK:

            return(gs_colorspace->cmm_icc_profile_data = &(icc_manager->DefaultCMYK));

            break;

	case gs_color_space_index_DevicePixel:

            /* Not sure yet what our response to 
               this should be */

            return(0);

            break;

       case gs_color_space_index_DeviceN:

            /* If we made it to here, then we will need to use the 
               alternate colorspace */

            return(0);

            break;

       case gs_color_space_index_CIEDEFG:

           /* Need to convert to an ICC form */

            break;

        case gs_color_space_index_CIEDEF:

           /* Need to convert to an ICC form */

            break;

        case gs_color_space_index_CIEABC:

           /* Need to convert to an ICC form */

            break;

        case gs_color_space_index_CIEA:

           /* Need to convert to an ICC form */

            break;

        case gs_color_space_index_Separation:

            /* Caller should use named color path */

            return(0);

            break;

        case gs_color_space_index_Pattern:
        case gs_color_space_index_Indexed:

            /* Caller should use the base space for these */
            return(0);

            break;


        case gs_color_space_index_CIEICC:

            /* This should not occur, as the space
               should have had a populated profile handle */

            return(0);

            break;

     } 
#endif

    return(0);


 }

