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


/* Interface to gxdither.c */

#ifndef gxdither_INCLUDED
#  define gxdither_INCLUDED

#include "gxfrac.h"
#include "gsdcolor.h"

/*
 * Render DeviceN possibly by halftoning.
 *  pcolors = pointer to an array color values (as fracs)
 *  pdevc - pointer to device color structure
 *  dev = pointer to device data structure
 *  pht = pointer to halftone data structure
 *  ht_phase  = halftone phase
 *  This is part of a kludge to minimize differences in the
 *  regression testing.
 */
int gx_render_device_DeviceN(frac * pcolor, gx_device_color * pdevc,
    gx_device * dev, gx_device_halftone * pdht, const gs_int_point * ht_phase);
/*
 * Reduce a colored halftone with 0 or 1 varying plane(s) to a pure color
 * or a binary halftone.
 */
int gx_devn_reduce_colored_halftone(gx_device_color *pdevc, gx_device *dev);

#endif /* gxdither_INCLUDED */
