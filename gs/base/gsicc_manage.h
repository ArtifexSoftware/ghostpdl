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

#define ICC_DUMP 0

/* A temporary definition to allow the icc branch to work with
   trunk */
#define ICCBRANCH
/* Define the default ICC profiles in the file system */
#define DEFAULT_GRAY_ICC  "default_gray.icc"
#define DEFAULT_RGB_ICC   "default_rgb.icc"
#define DEFAULT_CMYK_ICC  "default_cmyk.icc"
#define LAB_ICC           "lab.icc"
#define SMASK_GRAY_ICC    "ps_gray.icc"
#define SMASK_RGB_ICC     "ps_rgb.icc"
#define SMASK_CMYK_ICC    "ps_cmyk.icc"

/* Key names for special common canned profiles.
   These are found in some image file formats as
   a magic number.   */

#define GSICC_STANDARD_PROFILES_KEYS\
  "srgb", "sgray"
#define GSICC_STANDARD_PROFILES\
  "srgb.icc", "sgray.icc"
#define GSICC_NUMBER_STANDARD_PROFILES 2

#include "gsicc_littlecms.h"

/* Prototypes */
void gsicc_profile_reference(cmm_profile_t *icc_profile, int delta);
int gsicc_getsrc_channel_count(cmm_profile_t *icc_profile);
int gsicc_init_iccmanager(gs_state * pgs);
void  gsicc_profile_serialize(gsicc_serialized_profile_t *profile_data, 
                              cmm_profile_t *iccprofile);
int gsicc_init_device_profile(const gs_state *pgs, gx_device * dev);
int gsicc_set_profile(gsicc_manager_t *icc_manager, const char *pname, 
                      int namelen, gsicc_profile_t defaulttype);
cmm_profile_t* gsicc_get_profile_handle_file(const char* pname, int namelen, 
                                             gs_memory_t *mem);
void gsicc_init_profile_info(cmm_profile_t *profile);
gsicc_manager_t* gsicc_manager_new(gs_memory_t *memory);
cmm_profile_t* gsicc_profile_new(stream *s, gs_memory_t *memory, 
                                 const char* pname, int namelen);
int gsicc_set_gscs_profile(gs_color_space *pcs, cmm_profile_t *icc_profile, 
                           gs_memory_t * mem);
cmm_profile_t* gsicc_get_gscs_profile(gs_color_space *gs_colorspace, 
                                      gsicc_manager_t *icc_manager);
void gsicc_init_hash_cs(cmm_profile_t *picc_profile, gs_imager_state *pis);
gcmmhprofile_t gsicc_get_profile_handle_clist(cmm_profile_t *picc_profile, 
                                              gs_memory_t *memory);
gcmmhprofile_t gsicc_get_profile_handle_buffer(unsigned char *buffer, 
                                               int profile_size);
void gsicc_set_icc_directory(const gs_imager_state *pis, const char* pname, 
                             int namelen);
gsicc_smask_t* gsicc_new_iccsmask(gs_memory_t *memory);
int gsicc_initialize_iccsmask(gsicc_manager_t *icc_manager);
unsigned int gsicc_getprofilesize(unsigned char *buffer);
cmm_profile_t* gsicc_read_serial_icc(gx_device * dev, int64_t icc_hashcode);
cmm_profile_t* gsicc_finddevicen(const gs_color_space *pcs, 
                                 gsicc_manager_t *icc_manager);
gs_color_space_index gsicc_get_default_type(cmm_profile_t *profile_data);
#if ICC_DUMP
static void dump_icc_buffer(int buffersize, char filename[],byte *Buffer);
#endif
#endif

