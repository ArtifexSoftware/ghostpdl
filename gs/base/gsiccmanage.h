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


/*  Header for the ICC Manager */


#ifndef gsiccmanage_INCLUDED
#  define gsiccmanage_INCLUDED

/* Prototypes */

void gsicc_create();
void gsicc_destroy();
void gsicc_set_gray_profile(cmm_profile_t graybuffer);
void gsicc_set_rgb_profile(cmm_profile_t rgbbuffer);
void gsicc_set_cmyk_profile(cmm_profile_t cmykbuffer);
void gsicc_set_device_profile(cmm_profile_t deviceprofile);
void gsicc_set_device_named_color_profile(cmm_profile_t nameprofile);
void gsicc_set_device_linked_profile(cmm_profile_t outputlinkedprofile );
void gsicc_set_proof_profile(cmm_profile_t proofprofile );
void gsicc_load_default_device_profile(int numchannels);
void gsicc_load_default_input_profile(int numchannels);

void gsicc_setbuffer_desc(gsicc_bufferdesc_t *buffer_desc,unsigned char num_chan,
    unsigned char bytes_per_chan,
    bool has_alpha,
    bool alpha_first,
    bool little_endian,
    bool is_planar,
    gs_icc_colorbuffer_t buffercolor);

 gcmmhprofile_t GetProfileHandle(gs_color_space *gs_colorspace, gsicc_manager_t *icc_manager);



#endif

