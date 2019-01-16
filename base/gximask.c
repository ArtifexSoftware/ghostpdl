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
#include "gx.h"
#include "memory_.h"
#include "gserrors.h"
#include "gsptype1.h"
#include "gsptype2.h"
#include "gxdevice.h"
#include "gxdcolor.h"
#include "gxcpath.h"
#include "gximask.h"
#include "gzacpath.h"
#include "gzcpath.h"
#include "gxdevsop.h"

/* Functions for masked fill optimization. */
/* Imagemask with a shading color would paint entire shading for each rectangle of the mask.
   These functions convert the mask into a clipping path and then render entire shading
   at once through it.
*/

int
gx_image_fill_masked_start(gx_device *dev, const gx_device_color *pdevc, bool transpose,
                           const gx_clip_path *pcpath, gs_memory_t *mem, gs_logical_operation_t lop,
                           gx_device **cdev)
{
    if ((lop == lop_default) && (gx_dc_is_pattern2_color(pdevc) || gx_dc_is_pattern1_color_clist_based(pdevc))) {
        if (!dev_proc(dev, dev_spec_op)(dev, gxdso_pattern_can_accum, NULL, gs_no_id)) {
            extern_st(st_device_cpath_accum);
            gx_device_cpath_accum *pcdev;
            gs_fixed_rect cbox;

            if (pcpath == NULL)
                return_error(gs_error_nocurrentpoint);	/* close enough if no clip path */
            pcdev =  gs_alloc_struct(mem,
                    gx_device_cpath_accum, &st_device_cpath_accum, "gx_image_fill_masked_start");
            if (pcdev == NULL)
                return_error(gs_error_VMerror);
            gx_cpath_accum_begin(pcdev, mem, transpose);
            gx_cpath_outer_box(pcpath, &cbox);
            gx_cpath_accum_set_cbox(pcdev, &cbox);
            pcdev->rc.memory = mem;
            pcdev->width = dev->width;   /* For gx_default_copy_mono. */
            pcdev->height = dev->height; /* For gx_default_copy_mono. */
            gx_device_retain((gx_device *)pcdev, true);
            *cdev = (gx_device *)pcdev;
        } else{
            *cdev = dev;
        }
    } else
        *cdev = dev;
    return 0;
}

int
gx_image_fill_masked_end(gx_device *dev, gx_device *tdev, const gx_device_color *pdevc)
{
    gx_device_cpath_accum *pcdev = (gx_device_cpath_accum *)dev;
    gx_clip_path cpath;
    gx_clip_path cpath_with_shading_bbox;
    const gx_clip_path *pcpath1 = &cpath;
    gx_device_clip cdev;
    int code, code1;

    gx_cpath_init_local(&cpath, pcdev->memory);
    code = gx_cpath_accum_end(pcdev, &cpath);
    if (code >= 0)
        code = gx_dc_pattern2_clip_with_bbox(pdevc, tdev, &cpath_with_shading_bbox, &pcpath1);
    gx_make_clip_device_on_stack(&cdev, pcpath1, tdev);
    if (code >= 0 && pcdev->bbox.p.x < pcdev->bbox.q.x) {
        code1 = gx_device_color_fill_rectangle(pdevc,
                    pcdev->bbox.p.x, pcdev->bbox.p.y,
                    pcdev->bbox.q.x - pcdev->bbox.p.x,
                    pcdev->bbox.q.y - pcdev->bbox.p.y,
                    (gx_device *)&cdev, lop_default, 0);
        if (code == 0)
            code = code1;
    }
    if (pcpath1 == &cpath_with_shading_bbox)
        gx_cpath_free(&cpath_with_shading_bbox, "s_image_cleanup");
    gx_device_retain((gx_device *)pcdev, false);
    gx_cpath_free(&cpath, "s_image_cleanup");
    return code;
}

int
gx_image_fill_masked(gx_device *dev,
    const byte *data, int data_x, int raster, gx_bitmap_id id,
    int x, int y, int width, int height,
    const gx_device_color *pdc, int depth,
    gs_logical_operation_t lop, const gx_clip_path *pcpath)
{
    gx_device *cdev = dev;
    int code;

    if ((code = gx_image_fill_masked_start(dev, pdc, false, pcpath, dev->memory, lop, &cdev)) < 0)
        return code;

    if (cdev == dev)
        code = (*dev_proc(cdev, fill_mask))(cdev, data, data_x, raster, id,
                            x, y, width, height, pdc, depth, lop, pcpath);
    else {
        /* cdev != dev means that a cpath_accum device was inserted */
        gx_device_color dc_temp;   /* if fill_masked_start did cpath_accum, use pure color */

        set_nonclient_dev_color(&dc_temp, 1);    /* arbitrary color since cpath_accum doesn't use it */
        if ((code = (*dev_proc(cdev, fill_mask))(cdev, data, data_x, raster, id,
                            x, y, width, height, &dc_temp, depth, lop, pcpath)) < 0)
            return code;
        code = gx_image_fill_masked_end(cdev, dev, pdc);    /* fill with the actual device color */
    }
    return code;
}
