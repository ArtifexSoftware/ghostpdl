/* Copyright (C) 1997, 1998 Aladdin Enterprises.  All rights reserved.

   This file is part of Aladdin Ghostscript.

   Aladdin Ghostscript is distributed with NO WARRANTY OF ANY KIND.  No author
   or distributor accepts any responsibility for the consequences of using it,
   or for whether it serves any particular purpose or works at all, unless he
   or she says so in writing.  Refer to the Aladdin Ghostscript Free Public
   License (the "License") for full details.

   Every copy of Aladdin Ghostscript must include a copy of the License,
   normally in a plain ASCII text file named PUBLIC.  The License grants you
   the right to copy, modify and redistribute Aladdin Ghostscript, but only
   under certain conditions described in the License.  Among other things, the
   License requires that the copyright notice and this notice be preserved on
   all copies.
 */


/* Band-processing parameters for Ghostscript */

#ifndef gxband_INCLUDED
#  define gxband_INCLUDED

#include "gxclio.h"

/*
 * Define the parameters controlling banding.
 */
typedef struct gx_band_params_s {
    int BandWidth;		/* (optional) band width in pixels */
    int BandHeight;		/* (optional) */
    long BandBufferSpace;	/* (optional) */
} gx_band_params_t;

#define BAND_PARAMS_INITIAL_VALUES 0, 0, 0

/*
 * Define the information for a saved page.
 */
typedef struct gx_band_page_info_s {
    char cfname[gp_file_name_sizeof];	/* command file name */
    clist_file_ptr cfile;	/* command file, normally 0 */
    char bfname[gp_file_name_sizeof];	/* block file name */
    clist_file_ptr bfile;	/* block file, normally 0 */
    uint tile_cache_size;	/* size of tile cache */
    long bfile_end_pos;		/* ftell at end of bfile */
    gx_band_params_t band_params;  /* parameters used when writing band list */
				/* (actual values, no 0s) */
} gx_band_page_info_t;
#define PAGE_INFO_NULL_VALUES\
  { 0 }, 0, { 0 }, 0, 0, 0, { BAND_PARAMS_INITIAL_VALUES }

/*
 * By convention, the structure member containing the above is called
 * page_info.  Define shorthand accessors for its members.
 */
#define page_cfile page_info.cfile
#define page_cfname page_info.cfname
#define page_bfile page_info.bfile
#define page_bfname page_info.bfname
#define page_tile_cache_size page_info.tile_cache_size
#define page_bfile_end_pos page_info.bfile_end_pos
#define page_band_height page_info.band_params.BandHeight

#endif /* ndef gxband_INCLUDED */
