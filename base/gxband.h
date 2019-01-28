/* Copyright (C) 2001-2019 Artifex Software, Inc.
   All Rights Reserved.

   This software is provided AS-IS with no warranty, either express or
   implied.

   This software is distributed under license and may not be copied,
   modified or distributed except as expressly authorized under the terms
   of the license contained in the file LICENSE in this distribution.

   Refer to licensing information at http://www.artifex.com or contact
   Artifex Software, Inc.,  1305 Grant Avenue - Suite 200, Novato,
   CA 94945, U.S.A., +1(415)492-9861, for further information.
*/


/* Band-processing parameters for Ghostscript */

#ifndef gxband_INCLUDED
#  define gxband_INCLUDED

#include "gxclio.h"
#include "gxdevcli.h"

/* We hold color usage as a bitfield that needs to be at least as wide as
 * a gx_color_index - so for simplicity define it that way, even though
 * the two are not equal. */
typedef gx_color_index gx_color_usage_bits;

#define gx_color_usage_all(dev) \
  (((gx_color_usage_bits)1 << (dev)->color_info.num_components) - 1)

gx_color_usage_bits gx_color_index2usage(gx_device *dev, gx_color_index);

/*
 * Define information about the colors used on a page.
 */
typedef struct gx_colors_usage_s {
    gx_color_usage_bits or;	/* the "or" of all the used colors */
    bool slow_rop;		/* true if any RasterOps that can't be */
                                /* executed plane-by-plane on CMYK devices */
    gs_int_rect trans_bbox;	/* transparency bbox allows skipping the pdf14 compositor for some bands */
                                /* coordinates are band relative, 0 <= p.y < page_band_height */
} gx_color_usage_t;

/*
 * Define the information for a saved page.
 */
typedef struct gx_band_page_info_s {
    char cfname[gp_file_name_sizeof];	/* command file name */
    clist_file_ptr cfile;	/* command file, normally 0 */
    char bfname[gp_file_name_sizeof];	/* block file name */
    clist_file_ptr bfile;	/* block file, normally 0 */
    const clist_io_procs_t *io_procs;
    uint tile_cache_size;	/* size of tile cache */
    ulong line_ptrs_offset;      /* Offset of line_ptrs within tile cache */
    int64_t bfile_end_pos;		/* ftell at end of bfile */
    gx_band_params_t band_params;  /* parameters used when writing band list */
                                /* (actual values, no 0s) */
} gx_band_page_info_t;
#define PAGE_INFO_NULL_VALUES\
  { 0 }, 0, { 0 }, NULL, 0, 0, 0, 0, { BAND_PARAMS_INITIAL_VALUES }

/*
 * By convention, the structure member containing the above is called
 * page_info.  Define shorthand accessors for its members.
 */
#define page_cfile page_info.cfile
#define page_cfname page_info.cfname
#define page_bfile page_info.bfile
#define page_bfname page_info.bfname
#define page_tile_cache_size page_info.tile_cache_size
#define page_line_ptrs_offset page_info.line_ptrs_offset
#define page_bfile_end_pos page_info.bfile_end_pos
#define page_band_height page_info.band_params.BandHeight

#endif /* ndef gxband_INCLUDED */
