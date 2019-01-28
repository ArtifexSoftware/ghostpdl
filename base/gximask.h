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


/* Functions for masked fill optimization. */

#ifndef gximask_INCLUDED
#  define gximask_INCLUDED

#include "gxbitmap.h"
#include "gsropt.h"
#include "gsdevice.h"
#include "gsdcolor.h"
#include "gxpath.h"

int gx_image_fill_masked_start(gx_device *dev, const gx_device_color *pdevc, bool transpose,
                             const gx_clip_path *pcpath, gs_memory_t *mem, gs_logical_operation_t lop,
                             gx_device **cdev);

int gx_image_fill_masked_end(gx_device *dev, gx_device *tdev, const gx_device_color *pdevc);

int gx_image_fill_masked(gx_device *dev,
    const byte *data, int data_x, int raster, gx_bitmap_id id,
    int x, int y, int width, int height,
    const gx_device_color *pdcolor, int depth,
    gs_logical_operation_t lop, const gx_clip_path *pcpath);

#endif /* gximask_INCLUDED */
