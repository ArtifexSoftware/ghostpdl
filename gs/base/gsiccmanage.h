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

/* Define the default ICC profiles in the file system */

#define DEFAULT_GRAY_ICC  "iccprofiles/default_gray.icc"
#define DEFAULT_RGB_ICC   "iccprofiles/default_rgb.icc"
#define DEFAULT_CMYK_ICC  "iccprofiles/default_cmyk.icc"
#define LAB_ICC           "iccprofiles/lab.icc"

#include "gsicc_littlecms.h"

/* Prototypes */

void  gsicc_profile_serialize(gsicc_serialized_profile_t *profile_data, cmm_profile_t *iccprofile);

int gsicc_profile_clist_write(cmm_profile_t *icc_profile, uint offset,
    byte *data, uint *psize );

int gsicc_init_device_profile(gs_state * pgs, gx_device * dev);

int gsicc_set_profile(const gs_imager_state * pis, const char *pname, int namelen, gsicc_profile_t defaulttype);

gsicc_manager_t* gsicc_manager_new(gs_memory_t *memory);

cmm_profile_t* gsicc_profile_new(stream *s, gs_memory_t *memory, const char* pname, int namelen);

int gsicc_set_gscs_profile(gs_color_space *pcs, cmm_profile_t *icc_profile, gs_memory_t * mem);

cmm_profile_t* gsicc_get_gscs_profile(gs_color_space *gs_colorspace, gsicc_manager_t *icc_manager);

static void gsicc_set_default_cs_value(cmm_profile_t *picc_profile, gs_imager_state *pis);

void gsicc_init_hash_cs(cmm_profile_t *picc_profile, gs_imager_state *pis);

gcmmhprofile_t gsicc_get_profile_handle_buffer(unsigned char *buffer);

void gsicc_set_icc_directory(const gs_imager_state *pis, const char* pname, int namelen);

unsigned int gsicc_getprofilesize(unsigned char *buffer);

static void rc_free_icc_profile(gs_memory_t * mem, void *ptr_in, client_name_t cname);
static int gsicc_load_profile_buffer(cmm_profile_t *profile, stream *s, gs_memory_t *memory);
static stream* gsicc_open_search(const char* pname, int namelen, gs_memory_t *mem_gc, gsicc_manager_t *icc_manager);
static int gsicc_set_device_profile(gsicc_manager_t *icc_manager, gx_device * pdev, gs_memory_t * mem);
static cmm_profile_t* gsicc_get_profile( gsicc_profile_t profile_type, gsicc_manager_t *icc_manager );

#if ICC_DUMP
static void dump_icc_buffer(int buffersize, char filename[],byte *Buffer);
#endif

#endif

