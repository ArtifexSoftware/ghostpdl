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


/* pldraw.c */
/* Common drawing routines for PCL5 and PCL XL */
#include "std.h"
#include "gstypes.h"
#include "gserrors.h"
#include "gsmemory.h"
#include "gxdevice.h"
#include "gzstate.h"
#include "gsimage.h"
#include "pldraw.h"

/* the next 3 procedures use an obsolete image api in the
   graphics library.  Clients should use the definition after these
   3. */

/* Begin an image with parameters derived from a graphics state. */
int
pl_begin_image(gs_gstate * pgs, const gs_image_t * pim, void **pinfo)
{
    gx_device *dev = pgs->device;

    if (pim->ImageMask | pim->CombineWithColor) {
        int code = gx_set_dev_color(pgs);

        if (code != 0)
            return code;
    }
    return (*dev_proc(dev, begin_image))
        (dev, (const gs_gstate *)pgs, pim,
         gs_image_format_chunky, (const gs_int_rect *)0,
         gs_currentdevicecolor_inline(pgs), pgs->clip_path, pgs->memory,
         (gx_image_enum_common_t **) pinfo);
}

int
pl_image_data(gs_gstate * pgs, void *info, const byte ** planes,
              int data_x, uint raster, int height)
{
    gx_device *dev = pgs->device;

    return (*dev_proc(dev, image_data))
        (dev, info, planes, data_x, raster, height);
}

int
pl_end_image(gs_gstate * pgs, void *info, bool draw_last)
{
    gx_device *dev = pgs->device;

    return (*dev_proc(dev, end_image)) (dev, info, draw_last);
}

int
pl_begin_image2(gs_image_enum ** ppenum, gs_image_t * pimage, gs_gstate * pgs)
{
    *ppenum = gs_image_enum_alloc(gs_gstate_memory(pgs), "px_paint_pattern");
    if (*ppenum == 0)
        return_error(gs_error_VMerror);

    return gs_image_init(*ppenum, pimage, 0, false, pgs);
}

int
pl_image_data2(gs_image_enum * penum, const byte * row, uint size,
               uint * pused)
{
    return gs_image_next(penum, row, size, pused);
}

int
pl_end_image2(gs_image_enum * penum, gs_gstate * pgs)
{
    return gs_image_cleanup_and_free_enum(penum, pgs);
}
