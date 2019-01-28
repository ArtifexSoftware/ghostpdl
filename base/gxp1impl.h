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


/* PatternType 1 implementation interface */
/* Requires gxpcolor.h */

#ifndef gxp1impl_INCLUDED
#  define gxp1impl_INCLUDED

#include "gxdevcli.h"
#include "gxdcolor.h"
#include "gxpcolor.h"

/*
 * Declare the filling algorithms implemented in gxp1fill.c.
 * We use 'masked_fill_rect' instead of 'masked_fill_rectangle'
 * in order to limit identifier lengths to 32 characters.
 */
dev_color_proc_fill_rectangle(gx_dc_pattern_fill_rectangle);
dev_color_proc_fill_rectangle(gx_dc_pure_masked_fill_rect);
dev_color_proc_fill_rectangle(gx_dc_devn_masked_fill_rect);
dev_color_proc_fill_rectangle(gx_dc_binary_masked_fill_rect);
dev_color_proc_fill_rectangle(gx_dc_colored_masked_fill_rect);
dev_color_proc_fill_rectangle(gx_dc_pat_trans_fill_rectangle);

/*
 * Declare the Pattern color mapping procedures exported by gxpcmap.c.
 */
int gx_pattern_load(gx_device_color *, const gs_gstate *,
                    gx_device *, gs_color_select_t);
pattern_proc_remap_color(gs_pattern1_remap_color);

#endif /* gxp1impl_INCLUDED */
