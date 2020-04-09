/* Copyright (C) 2001-2020 Artifex Software, Inc.
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


/* pcwhtindx.c - interface to code for finding white index of a palette */

#ifndef pcwhtindx_INCLUDED
#define pcwhtindx_INCLUDED

#include "gx.h"
#include "gsbitmap.h"
#include "pcindxed.h"

/*
 * To implement transparency in PCL colored patterns and rasters, it is
 * necessary to identify those pixels which should be considered white, as
 * those are the pixels which should be transparent.
 *
 * "White" in the sense of PCL means white in the device color space, just
 * prior to dithering. Hence, the effect of normalization, color lookup tables,
 * and conversion from the source to device color lookup table are taken into
 * account before determining if a given pixel is (potentially) transparent.
 *
 * Such a mechanism is, unfortunately, not easily implemented via the existing
 * graphic library, as raster transparency is implemented via PostScript
 * ImageType 4 images. For these images, the single color or range of colors
 * required that identify pixels as transparent are specified in the source
 * color space, before any conversion.
 *
 * In this implementation, normalization of colors for black and white
 * reference points occurs before the raw data is ever considered a color,
 * so at least that part of the problem is handled correctly. Beyond this
 * canonical white--(1.0, 1.0, 1.0) in all color spaces--is assumed to yield
 * white; if it does not (due to color lookup tables), the results will be
 * incorrect.
 *
 * Where appropriate, this implementation also uses the graphic library's
 * support for indexed color spaces. Creating potentially transparent rasters
 * with these color spaces requires us to identify which entries in the
 * color palette are white. When there is only one such entry, this is simple.
 * When several palette entries are involved, life is more difficult. It is
 * then necessary to remap all raster values which map to a white palette
 * entry to the same white entry.
 *
 * For straight rasters, this is can be done in-place, as the raster will
 * never be rendered with another color space. Patterns are not so easily
 * handled, as they may be rendered subsequently with other color spaces.
 * Hence, for those it is necessary to copy the data.
 */

/*
 * Determine the white entry in the color palette of an indexed color space,
 * and perform any remapping that is required.
 *
 * The must_copy operand indicates if the raster data can be overwritten in
 * place (false) or if a new array must be allocated.
 *
 * Returns 0 if successful, < 0 in the event of an error.
 */
int pcl_cmap_map_raster(const pcl_cs_indexed_t * pindexed,
                        int *pfirst_white,
                        const gs_depth_bitmap * pin_pixinfo,
                        gs_depth_bitmap * pout_pixinfo,
                        bool must_copy, gs_memory_t * pmem);

/*
 * An alternative interface to the remapping capability, this one more suited
 * to working with rasters.
 *
 * Because rasters are provided in a large number of pieces (typically one
 * sanline is an individual piece), and all pieces are rendered using the
 * the same color palette, it does not make sense to re-derive the mapping
 * table for each raster. Consequently, the code below can be used to get
 * the mapping table once, re-use it for each piece, and then free it.
 *
 * This code is specifically intended for rasters, and will only create a
 * remap table if:
 *
 *     source transparency is required
 *     the current palette uses an indexed pixel encoding (by plane or
 *         by pixel)
 *     there is more than one "white" in the current palette.
 *
 */
const void *pcl_cmap_create_remap_ary(pcl_state_t * pcs, int *pfirst_white);

void pcl_cmap_apply_remap_ary(const void *vpmap,  /* remap array pointer */
                              byte * prast,       /* array of bytes to be mapped */
                              int b_per_p,        /* bits per pixel */
                              int npixels);

#endif /* pcwhtindx_INCLUDED */
