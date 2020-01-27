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


/* Structures for ICCBased color space parameters */
/* requires: gspsace.h, gscolor2.h */

#ifndef gsicc_INCLUDED
#  define gsicc_INCLUDED

#include "gscie.h"
#include "gxcspace.h"
/*
 * Build an ICCBased color space.
 *
 */
extern  int     gs_cspace_build_ICC( gs_color_space **   ppcspace,
                                        void *              client_data,
                                        gs_memory_t *       pmem );

extern const gs_color_space_type gs_color_space_type_ICC;
extern cs_proc_remap_color(gx_remap_ICC_imagelab);

int gx_change_color_model(gx_device *dev, int num_comps, int bit_depth);

int gx_remap_ICC_with_link(const gs_client_color * pcc, const gs_color_space * pcs,
        gx_device_color * pdc, const gs_gstate * pgs, gx_device * dev,
                gs_color_select_t select, gsicc_link_t *icc_link);

#endif /* gsicc_INCLUDED */
