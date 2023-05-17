/* Copyright (C) 2001-2023 Artifex Software, Inc.
   All Rights Reserved.

   This software is provided AS-IS with no warranty, either express or
   implied.

   This software is distributed under license and may not be copied,
   modified or distributed except as expressly authorized under the terms
   of the license contained in the file LICENSE in this distribution.

   Refer to licensing information at http://www.artifex.com or contact
   Artifex Software, Inc.,  39 Mesa Street, Suite 108A, San Francisco,
   CA 94129, USA, for further information.
*/


/* Support for printer devices with planar buffering. */
#include "gdevprn.h"
#include "gdevmpla.h"
#include "gdevppla.h"
#include "gxdevsop.h"

/* Set the buf_procs in a printer device to planar mode. */
int
gdev_prn_set_procs_planar(gx_device *dev)
{
    gx_device_printer * const pdev = (gx_device_printer *)dev;

    pdev->printer_procs.buf_procs.create_buf_device =
        gdev_prn_create_buf_planar;
    pdev->printer_procs.buf_procs.size_buf_device =
        gdev_prn_size_buf_planar;
    if (dev_proc(pdev, dev_spec_op) == gx_default_dev_spec_op)
        set_dev_proc(pdev, dev_spec_op, gdev_prn_dev_spec_op);
    return 0;
}

/* Open a printer device, conditionally setting it to be planar. */
int
gdev_prn_open_planar(gx_device *dev, int num_planar_planes)
{
    if (num_planar_planes) {
        gdev_prn_set_procs_planar(dev);
        dev->num_planar_planes = num_planar_planes;
    }
    return gdev_prn_open(dev);
}

/* Augment get/put_params to add UsePlanarBuffer. */
int
gdev_prn_get_params_planar(gx_device * pdev, gs_param_list * plist,
                           bool *pupb)
{
    int ecode = gdev_prn_get_params(pdev, plist);

    if (ecode < 0)
        return ecode;
    return param_write_bool(plist, "UsePlanarBuffer", pupb);
}
int
gdev_prn_put_params_planar(gx_device * pdev, gs_param_list * plist,
                           bool *pupb)
{
    bool upb = *pupb;
    int ecode = 0, code;

    if (pdev->color_info.num_components > 1)
        ecode = param_read_bool(plist, "UsePlanarBuffer", &upb);
    code = gdev_prn_put_params(pdev, plist);
    if (ecode >= 0)
        ecode = code;
    if (ecode >= 0)
        *pupb = upb;
    return ecode;
}

/* Set the buffer device to planar mode. */
static int
gdev_prn_set_planar(gx_device_memory *mdev, const gx_device *tdev)
{
    int num_comp = tdev->num_planar_planes;
    gx_render_plane_t planes[GX_DEVICE_COLOR_MAX_COMPONENTS];
    int depth = tdev->color_info.depth / num_comp;
    int k;

    if (num_comp < 1 || num_comp > GX_DEVICE_COLOR_MAX_COMPONENTS)
        return_error(gs_error_rangecheck);
    /* Round up the depth per plane to a power of 2. */
    while (depth & (depth - 1))
        --depth, depth = (depth | (depth >> 1)) + 1;

    /* We want the most significant plane to come out first. */
    planes[num_comp-1].shift = 0;
    planes[num_comp-1].depth = depth;
    for (k = (num_comp - 2); k >= 0; k--) {
        planes[k].depth = depth;
        planes[k].shift = planes[k + 1].shift + depth;
    }
    return gdev_mem_set_planar(mdev, num_comp, planes);
}

/* Create a planar buffer device. */
int
gdev_prn_create_buf_planar(gx_device **pbdev, gx_device *target, int y,
                           const gx_render_plane_t *render_plane,
                           gs_memory_t *mem, gx_color_usage_t *for_band)
{
    int code = gx_default_create_buf_device(pbdev, target, y, render_plane, mem,
                                            for_band);

    if (code < 0)
        return code;
    if (gs_device_is_memory(*pbdev) /* == render_plane->index < 0 */) {
        code = gdev_prn_set_planar((gx_device_memory *)*pbdev, *pbdev);
    }
    return code;
}

/* Determine the space needed by a planar buffer device. */
int
gdev_prn_size_buf_planar(gx_device_buf_space_t *space, gx_device *target,
                         const gx_render_plane_t *render_plane,
                         int height, bool for_band)
{
    gx_device_memory mdev = { 0 };
    int code;

    if (render_plane && render_plane->index >= 0)
        return gx_default_size_buf_device(space, target, render_plane,
                                          height, for_band);
    mdev.color_info = target->color_info;
    mdev.pad = target->pad;
    mdev.log2_align_mod = target->log2_align_mod;
    mdev.num_planar_planes = target->num_planar_planes;
    mdev.graphics_type_tag = target->graphics_type_tag;
    code = gdev_prn_set_planar(&mdev, target);
    if (code < 0)
        return code;
    if (gdev_mem_bits_size(&mdev, target->width, height, &(space->bits)) < 0)
        return_error(gs_error_VMerror);
    space->line_ptrs = gdev_mem_line_ptrs_size(&mdev, target->width, height);
    space->raster = bitmap_raster_pad_align(target->width * mdev.planes[0].depth, mdev.pad, mdev.log2_align_mod);
    return 0;
}
