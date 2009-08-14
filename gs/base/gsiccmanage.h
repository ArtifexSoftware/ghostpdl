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

/* Define the default ICC profiles in the file system */

#define DEFAULT_GRAY_ICC  "./iccprofiles/default_gray.icc"
#define DEFAULT_RGB_ICC  "./iccprofiles/default_rgb.icc"
#define DEFAULT_CMYK_ICC  "./iccprofiles/default_cmyk.icc"

/* Prototypes */

#include "gsicc_littlecms.h"

void gsicc_create();
void gsicc_destroy();
void gsicc_init_device_profile(gs_state * pgs, gx_device * dev);
int gsicc_set_profile(const gs_imager_state * pis, const char *pname, int namelen, gsicc_profile_t defaulttype);
void gsicc_set_device_profile(gsicc_manager_t *icc_manager, gx_device * pdev, gs_memory_t * mem);
void gsicc_load_default_device_profile(int numchannels);
void gsicc_load_default_input_profile(int numchannels);

gsicc_manager_t* gsicc_manager_new(gs_memory_t *memory);
cmm_profile_t* gsicc_profile_new(stream *s, gs_memory_t *memory, const char* pname, int namelen);

int gsicc_cs_profile(gs_color_space *pcs, cmm_profile_t *icc_profile, gs_memory_t * mem);

static int gsicc_load_profile_buffer(cmm_profile_t *profile, stream *s, gs_memory_t *memory);
unsigned int gsicc_getprofilesize(unsigned char *buffer);

void gsicc_setbuffer_desc(gsicc_bufferdesc_t *buffer_desc,unsigned char num_chan,
    unsigned char bytes_per_chan,
    bool has_alpha,
    bool alpha_first,
    bool little_endian,
    bool is_planar,
    gsicc_colorbuffer_t buffercolor);

 cmm_profile_t* gsicc_get_cs_profile(gs_color_space *gs_colorspace, gsicc_manager_t *icc_manager);
 gcmmhprofile_t gsicc_get_profile_handle_buffer(unsigned char *buffer);

 static void rc_free_icc_profile(gs_memory_t * mem, void *ptr_in, client_name_t cname);


#endif

