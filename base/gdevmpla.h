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

/* Interface to planar memory devices. */

#ifndef gdevmpla_INCLUDED
#  define gdevmpla_INCLUDED

#include "gsdevice.h"
#include "gxrplane.h"

/*
 * Planar memory devices store the bits by planes instead of by chunks.
 * The plane corresponding to the least significant bits of the color index
 * is stored first.  Each plane may store a different number of bits,
 * but the depth of each plane must be an allowable one for a memory
 * device and not greater than 16 (currently, 1, 2, 4, 8, or 16), and the
 * total must not exceed the size of gx_color_index.
 *
 * Planar devices store the data for each plane contiguously, as though
 * each plane were a separate device.  There is an array of line pointers
 * for each plane (num_planes arrays in all).
 */

/*
 * Set up a planar memory device, after calling gs_make_mem_device but
 * before opening the device.  The pre-existing device provides the color
 * mapping procedures, but not the drawing procedures.  Requires: num_planes
 * > 0, plane_depths[0 ..  num_planes - 1] > 0, sum of plane_depths <=
 * mdev->color_info.depth.
 */
int gdev_mem_set_planar(gx_device_memory * mdev, int num_planes,
                        const gx_render_plane_t *planes /*[num_planes]*/);

#endif /* gdevmpla_INCLUDED */
