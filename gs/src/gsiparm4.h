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

/*$Id$ */
/* ImageType 4 image parameter definition */

#ifndef gsiparm4_INCLUDED
#  define gsiparm4_INCLUDED

#include "gsiparam.h"

/*
 * See Section 4.3 of the Adobe PostScript Version 3010 Supplement
 * for a definition of ImageType 4 images.
 */

typedef struct gs_image4_s {
    gs_pixel_image_common;
    /*
     * If MaskColor_is_range is false, the first N elements of
     * MaskColor are sample values; if MaskColor_is_range is true,
     * the first 2*N elements are ranges of sample values.
     *
     * Currently, the largest sample values supported by the library are 12
     * bits, but eventually we want to support DevicePixel images with
     * samples up to 32 bits as well.
     */
    bool MaskColor_is_range;
    uint MaskColor[gs_image_max_components * 2];
} gs_image4_t;

#define image4_type_data\
  gx_begin_image4, gx_data_image_source_size, 4
#define image4_enum_procs_data\
  gx_image4_plane_data, gx_image4_end_image

/*
 * Initialize an ImageType 4 image.  Defaults:
 *      MaskColor_is_range = false
 */
void gs_image4_t_init(P2(gs_image4_t * pim, const gs_color_space * color_space));

#endif /* gsiparm4_INCLUDED */
