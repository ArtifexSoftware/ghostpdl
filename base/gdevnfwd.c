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

/* Null and forwarding device implementation */
#include "gx.h"
#include "gserrors.h"
#include "gxdevice.h"
#include "gxcmap.h"
#include "memory_.h"
#include "gxdevsop.h"

/* ---------------- Forwarding procedures ---------------- */

/* Additional finalization for forwarding devices. */
static void
gx_device_forward_finalize(gx_device *dev)
{
    gx_device *target = ((gx_device_forward *)dev)->target;

    ((gx_device_forward *)dev)->target = 0;
    rc_decrement_only(target, "gx_device_forward_finalize");
}

/* Set the target of a forwarding device. */
void
gx_device_set_target(gx_device_forward *fdev, gx_device *target)
{
    /*
     * ****** HACK: if this device doesn't have special finalization yet,
     * make it decrement the reference count of the target.
     */
    if (target && !fdev->finalize)
        fdev->finalize = gx_device_forward_finalize;
    rc_assign(fdev->target, target, "gx_device_set_target");
    /* try to initialize to same as target, otherwise UNKNOWN */
    fdev->graphics_type_tag = target != NULL ? target->graphics_type_tag : GS_UNKNOWN_TAG;
    fdev->interpolate_control = target != NULL ? target->interpolate_control : 1;	/* the default */
}

/* Fill in NULL procedures in a forwarding device procedure record. */
/* We don't fill in: open_device, close_device, or the lowest-level */
/* drawing operations. */
void
gx_device_forward_fill_in_procs(register gx_device_forward * dev)
{
    /* NOT open_device */
    fill_dev_proc(dev, get_initial_matrix, gx_forward_get_initial_matrix);
    fill_dev_proc(dev, sync_output, gx_forward_sync_output);
    fill_dev_proc(dev, output_page, gx_forward_output_page);
    /* NOT close_device */
    fill_dev_proc(dev, map_rgb_color, gx_forward_map_rgb_color);
    fill_dev_proc(dev, map_color_rgb, gx_forward_map_color_rgb);
    /* NOT fill_rectangle */
    /* NOT copy_mono */
    /* NOT copy_color */
    fill_dev_proc(dev, get_params, gx_forward_get_params);
    fill_dev_proc(dev, put_params, gx_forward_put_params);
    fill_dev_proc(dev, map_cmyk_color, gx_forward_map_cmyk_color);
    fill_dev_proc(dev, get_page_device, gx_forward_get_page_device);
    fill_dev_proc(dev, get_alpha_bits, gx_forward_get_alpha_bits);
    /* NOT copy_alpha */
    fill_dev_proc(dev, fill_path, gx_forward_fill_path);
    fill_dev_proc(dev, stroke_path, gx_forward_stroke_path);
    fill_dev_proc(dev, fill_mask, gx_forward_fill_mask);
    fill_dev_proc(dev, fill_trapezoid, gx_forward_fill_trapezoid);
    fill_dev_proc(dev, fill_parallelogram, gx_forward_fill_parallelogram);
    fill_dev_proc(dev, fill_triangle, gx_forward_fill_triangle);
    fill_dev_proc(dev, draw_thin_line, gx_forward_draw_thin_line);
    /* NOT strip_tile_rectangle */
    fill_dev_proc(dev, get_clipping_box, gx_forward_get_clipping_box);
    fill_dev_proc(dev, begin_typed_image, gx_forward_begin_typed_image);
    fill_dev_proc(dev, get_bits_rectangle, gx_forward_get_bits_rectangle);
    fill_dev_proc(dev, composite, gx_no_composite);
    fill_dev_proc(dev, get_hardware_params, gx_forward_get_hardware_params);
    fill_dev_proc(dev, text_begin, gx_forward_text_begin);
    fill_dev_proc(dev, get_color_mapping_procs, gx_forward_get_color_mapping_procs);
    fill_dev_proc(dev, get_color_comp_index, gx_forward_get_color_comp_index);
    fill_dev_proc(dev, encode_color, gx_forward_encode_color);
    fill_dev_proc(dev, decode_color, gx_forward_decode_color);
    fill_dev_proc(dev, dev_spec_op, gx_forward_dev_spec_op);
    fill_dev_proc(dev, fill_rectangle_hl_color, gx_forward_fill_rectangle_hl_color);
    fill_dev_proc(dev, include_color_space, gx_forward_include_color_space);
    fill_dev_proc(dev, fill_linear_color_scanline, gx_forward_fill_linear_color_scanline);
    fill_dev_proc(dev, fill_linear_color_trapezoid, gx_forward_fill_linear_color_trapezoid);
    fill_dev_proc(dev, fill_linear_color_triangle, gx_forward_fill_linear_color_triangle);
    fill_dev_proc(dev, update_spot_equivalent_colors, gx_forward_update_spot_equivalent_colors);
    fill_dev_proc(dev, ret_devn_params, gx_forward_ret_devn_params);
    fill_dev_proc(dev, fillpage, gx_forward_fillpage);
    fill_dev_proc(dev, put_image, gx_forward_put_image);
    fill_dev_proc(dev, get_profile, gx_forward_get_profile);
    fill_dev_proc(dev, set_graphics_type_tag, gx_forward_set_graphics_type_tag);
    fill_dev_proc(dev, strip_copy_rop2, gx_forward_strip_copy_rop2);
    fill_dev_proc(dev, strip_tile_rect_devn, gx_forward_strip_tile_rect_devn);
    fill_dev_proc(dev, strip_tile_rect_devn, gx_forward_strip_tile_rect_devn);
    fill_dev_proc(dev, transform_pixel_region, gx_forward_transform_pixel_region);
    fill_dev_proc(dev, fill_stroke_path, gx_forward_fill_stroke_path);
    fill_dev_proc(dev, lock_pattern, gx_forward_lock_pattern);
    gx_device_fill_in_procs((gx_device *) dev);
}

/* Forward the color mapping procedures from a device to its target. */
void
gx_device_forward_color_procs(gx_device_forward * dev)
{
    set_dev_proc(dev, map_rgb_color, gx_forward_map_rgb_color);
    set_dev_proc(dev, map_color_rgb, gx_forward_map_color_rgb);
    set_dev_proc(dev, map_cmyk_color, gx_forward_map_cmyk_color);
    set_dev_proc(dev, get_color_mapping_procs, gx_forward_get_color_mapping_procs);
    set_dev_proc(dev, get_color_comp_index, gx_forward_get_color_comp_index);
    set_dev_proc(dev, encode_color, gx_forward_encode_color);
    set_dev_proc(dev, decode_color, gx_forward_decode_color);
    set_dev_proc(dev, get_profile, gx_forward_get_profile);
    /* Not strictly a color proc, but may affect color encoding */
    fill_dev_proc(dev, set_graphics_type_tag, gx_forward_set_graphics_type_tag);
    fill_dev_proc(dev, dev_spec_op, gx_forward_dev_spec_op);
}

int
gx_forward_close_device(gx_device * dev)
{
    gx_device_forward * const fdev = (gx_device_forward *)dev;
    gx_device *tdev = fdev->target;
    int code = (tdev == 0) ? gx_default_close_device(dev)
                       : dev_proc(tdev, close_device)(tdev);

    if (tdev)
        tdev->is_open = false;          /* flag corresponds to the state */
    return code;
}

void
gx_forward_get_initial_matrix(gx_device * dev, gs_matrix * pmat)
{
    gx_device_forward * const fdev = (gx_device_forward *)dev;
    gx_device *tdev = fdev->target;

    if (tdev == 0)
        gx_default_get_initial_matrix(dev, pmat);
    else
        dev_proc(tdev, get_initial_matrix)(tdev, pmat);
}

int
gx_forward_sync_output(gx_device * dev)
{
    gx_device_forward * const fdev = (gx_device_forward *)dev;
    gx_device *tdev = fdev->target;

    return (tdev == 0 ? gx_default_sync_output(dev) :
            dev_proc(tdev, sync_output)(tdev));
}

int
gx_forward_output_page(gx_device * dev, int num_copies, int flush)
{
    gx_device_forward * const fdev = (gx_device_forward *)dev;
    gx_device *tdev = fdev->target;
    int code =
        (tdev == 0 ? gx_default_output_page(dev, num_copies, flush) :
         dev_proc(tdev, output_page)(tdev, num_copies, flush));

    if (code >= 0 && tdev != 0)
        dev->PageCount = tdev->PageCount;
    return code;
}

gx_color_index
gx_forward_map_rgb_color(gx_device * dev, const gx_color_value cv[])
{
    gx_device_forward * const fdev = (gx_device_forward *)dev;
    gx_device *tdev = fdev->target;

    return (tdev == 0 ? gx_error_encode_color(dev, cv) :
            dev_proc(tdev, map_rgb_color)(tdev, cv));
}

int
gx_forward_map_color_rgb(gx_device * dev, gx_color_index color,
                         gx_color_value prgb[3])
{
    gx_device_forward * const fdev = (gx_device_forward *)dev;
    gx_device *tdev = fdev->target;

    return (tdev == 0 ? gx_default_map_color_rgb(dev, color, prgb) :
            dev_proc(tdev, map_color_rgb)(tdev, color, prgb));
}

int
gx_forward_fill_rectangle(gx_device * dev, int x, int y, int w, int h,
                          gx_color_index color)
{
    gx_device_forward * const fdev = (gx_device_forward *)dev;
    gx_device *tdev = fdev->target;

    if (tdev == 0)
        return_error(gs_error_Fatal);
    return dev_proc(tdev, fill_rectangle)(tdev, x, y, w, h, color);
}

int
gx_forward_copy_mono(gx_device * dev, const byte * data,
                     int dx, int raster, gx_bitmap_id id,
                     int x, int y, int w, int h,
                     gx_color_index zero, gx_color_index one)
{
    gx_device_forward * const fdev = (gx_device_forward *)dev;
    gx_device *tdev = fdev->target;

    if (tdev == 0)
        return_error(gs_error_Fatal);
    return dev_proc(tdev, copy_mono)
        (tdev, data, dx, raster, id, x, y, w, h, zero, one);
}

int
gx_forward_copy_alpha(gx_device * dev, const byte * data, int data_x,
           int raster, gx_bitmap_id id, int x, int y, int width, int height,
                      gx_color_index color, int depth)
{
    gx_device_forward * const fdev = (gx_device_forward *)dev;
    gx_device *tdev = fdev->target;

    if (tdev == 0)
        return_error(gs_error_Fatal);
    return dev_proc(tdev, copy_alpha)
        (tdev, data, data_x, raster, id, x, y, width, height, color, depth);
}

int
gx_forward_copy_color(gx_device * dev, const byte * data,
                      int dx, int raster, gx_bitmap_id id,
                      int x, int y, int w, int h)
{
    gx_device_forward * const fdev = (gx_device_forward *)dev;
    gx_device *tdev = fdev->target;

    if (tdev == 0)
        return_error(gs_error_Fatal);
    return dev_proc(tdev, copy_color)
        (tdev, data, dx, raster, id, x, y, w, h);
}

int
gx_forward_copy_planes(gx_device * dev, const byte * data,
                       int dx, int raster, gx_bitmap_id id,
                       int x, int y, int w, int h, int plane_height)
{
    gx_device_forward * const fdev = (gx_device_forward *)dev;
    gx_device *tdev = fdev->target;

    if (tdev == 0)
        return_error(gs_error_Fatal);
    return dev_proc(tdev, copy_planes)
        (tdev, data, dx, raster, id, x, y, w, h, plane_height);
}

int
gx_forward_get_params(gx_device * dev, gs_param_list * plist)
{
    gx_device_forward * const fdev = (gx_device_forward *)dev;
    gx_device *tdev = fdev->target;

    return (tdev == 0 ? gx_default_get_params(dev, plist) :
            dev_proc(tdev, get_params)(tdev, plist));
}

int
gx_forward_put_params(gx_device * dev, gs_param_list * plist)
{
    gx_device_forward * const fdev = (gx_device_forward *)dev;
    gx_device *tdev = fdev->target;
    bool was_open;
    int code;

    if (tdev == 0)
        return gx_default_put_params(dev, plist);
    was_open = tdev->is_open;
    code = dev_proc(tdev, put_params)(tdev, plist);
    if (code == 0 && !tdev->is_open) {
            code = was_open ? 1 : 0;   /* target device closed */
    }
    if (code >= 0)
        gx_device_decache_colors(dev);
    return code;
}

gx_color_index
gx_forward_map_cmyk_color(gx_device * dev, const gx_color_value cv[])
{
    gx_device_forward * const fdev = (gx_device_forward *)dev;
    gx_device *tdev = fdev->target;

    return (tdev == 0 ? gx_default_map_cmyk_color(dev, cv) :
            dev_proc(tdev, map_cmyk_color)(tdev, cv));
}

gx_device *
gx_forward_get_page_device(gx_device * dev)
{
    gx_device_forward * const fdev = (gx_device_forward *)dev;
    gx_device *tdev = fdev->target;
    gx_device *pdev;

    if (tdev == 0)
        return gx_default_get_page_device(dev);
    pdev = dev_proc(tdev, get_page_device)(tdev);
    return pdev;
}

int
gx_forward_fill_path(gx_device * dev, const gs_gstate * pgs,
                     gx_path * ppath, const gx_fill_params * params,
                     const gx_drawing_color * pdcolor,
                     const gx_clip_path * pcpath)
{
    gx_device_forward * const fdev = (gx_device_forward *)dev;
    gx_device *tdev = fdev->target;
    dev_proc_fill_path((*proc)) =
        (tdev == 0 ? (tdev = dev, gx_default_fill_path) :
         dev_proc(tdev, fill_path));

    return proc(tdev, pgs, ppath, params, pdcolor, pcpath);
}

int
gx_forward_stroke_path(gx_device * dev, const gs_gstate * pgs,
                       gx_path * ppath, const gx_stroke_params * params,
                       const gx_drawing_color * pdcolor,
                       const gx_clip_path * pcpath)
{
    gx_device_forward * const fdev = (gx_device_forward *)dev;
    gx_device *tdev = fdev->target;
    dev_proc_stroke_path((*proc)) =
        (tdev == 0 ? (tdev = dev, gx_default_stroke_path) :
         dev_proc(tdev, stroke_path));

    return proc(tdev, pgs, ppath, params, pdcolor, pcpath);
}

int
gx_forward_fill_stroke_path(gx_device * dev, const gs_gstate * pgs,
                            gx_path * ppath,
                            const gx_fill_params * params_fill,
                            const gx_drawing_color * pdcolor_fill,
                            const gx_stroke_params * params_stroke,
                            const gx_drawing_color * pdcolor_stroke,
                            const gx_clip_path * pcpath)
{
    gx_device_forward * const fdev = (gx_device_forward *)dev;
    gx_device *tdev = fdev->target;
    dev_proc_fill_stroke_path((*proc)) =
        (tdev == 0 ? (tdev = dev, gx_default_fill_stroke_path) :
         dev_proc(tdev, fill_stroke_path));

    return proc(tdev, pgs, ppath, params_fill, pdcolor_fill, params_stroke, pdcolor_stroke, pcpath);
}

int
gx_forward_lock_pattern(gx_device * dev, gs_gstate * pgs, gs_id pattern_id, int lock)
{
    gx_device_forward * const fdev = (gx_device_forward *)dev;
    gx_device *tdev = fdev->target;
    dev_proc_lock_pattern((*proc)) =
        (tdev == 0 ? (tdev = dev, gx_default_lock_pattern) :
         dev_proc(tdev, lock_pattern));

    return proc(tdev, pgs, pattern_id, lock);

}

int
gx_forward_fill_mask(gx_device * dev,
                     const byte * data, int dx, int raster, gx_bitmap_id id,
                     int x, int y, int w, int h,
                     const gx_drawing_color * pdcolor, int depth,
                     gs_logical_operation_t lop, const gx_clip_path * pcpath)
{
    gx_device_forward * const fdev = (gx_device_forward *)dev;
    gx_device *tdev = fdev->target;
    dev_proc_fill_mask((*proc)) =
        (tdev == 0 ? (tdev = dev, gx_default_fill_mask) :
         dev_proc(tdev, fill_mask));

    return proc(tdev, data, dx, raster, id, x, y, w, h, pdcolor, depth,
                lop, pcpath);
}

int
gx_forward_fill_trapezoid(gx_device * dev,
                          const gs_fixed_edge * left,
                          const gs_fixed_edge * right,
                          fixed ybot, fixed ytop, bool swap_axes,
                          const gx_drawing_color * pdcolor,
                          gs_logical_operation_t lop)
{
    gx_device_forward * const fdev = (gx_device_forward *)dev;
    gx_device *tdev = fdev->target;
    dev_proc_fill_trapezoid((*proc)) =
        (tdev == 0 ? (tdev = dev, gx_default_fill_trapezoid) :
         dev_proc(tdev, fill_trapezoid));

    return proc(tdev, left, right, ybot, ytop, swap_axes, pdcolor, lop);
}

int
gx_forward_fill_parallelogram(gx_device * dev,
                 fixed px, fixed py, fixed ax, fixed ay, fixed bx, fixed by,
               const gx_drawing_color * pdcolor, gs_logical_operation_t lop)
{
    gx_device_forward * const fdev = (gx_device_forward *)dev;
    gx_device *tdev = fdev->target;
    dev_proc_fill_parallelogram((*proc)) =
        (tdev == 0 ? (tdev = dev, gx_default_fill_parallelogram) :
         dev_proc(tdev, fill_parallelogram));

    return proc(tdev, px, py, ax, ay, bx, by, pdcolor, lop);
}

int
gx_forward_fill_triangle(gx_device * dev,
                 fixed px, fixed py, fixed ax, fixed ay, fixed bx, fixed by,
               const gx_drawing_color * pdcolor, gs_logical_operation_t lop)
{
    gx_device_forward * const fdev = (gx_device_forward *)dev;
    gx_device *tdev = fdev->target;
    dev_proc_fill_triangle((*proc)) =
        (tdev == 0 ? (tdev = dev, gx_default_fill_triangle) :
         dev_proc(tdev, fill_triangle));

    return proc(tdev, px, py, ax, ay, bx, by, pdcolor, lop);
}

int
gx_forward_draw_thin_line(gx_device * dev,
                          fixed fx0, fixed fy0, fixed fx1, fixed fy1,
               const gx_drawing_color * pdcolor, gs_logical_operation_t lop,
                          fixed adjustx, fixed adjusty)
{
    gx_device_forward * const fdev = (gx_device_forward *)dev;
    gx_device *tdev = fdev->target;
    dev_proc_draw_thin_line((*proc)) =
        (tdev == 0 ? (tdev = dev, gx_default_draw_thin_line) :
         dev_proc(tdev, draw_thin_line));

    return proc(tdev, fx0, fy0, fx1, fy1, pdcolor, lop, adjustx, adjusty);
}

int
gx_forward_strip_tile_rectangle(gx_device * dev, const gx_strip_bitmap * tiles,
   int x, int y, int w, int h, gx_color_index color0, gx_color_index color1,
                                int px, int py)
{
    gx_device_forward * const fdev = (gx_device_forward *)dev;
    gx_device *tdev = fdev->target;
    dev_proc_strip_tile_rectangle((*proc)) =
        (tdev == 0 ? (tdev = dev, gx_default_strip_tile_rectangle) :
         dev_proc(tdev, strip_tile_rectangle));

    return proc(tdev, tiles, x, y, w, h, color0, color1, px, py);
}

int
gx_forward_strip_copy_rop2(gx_device * dev, const byte * sdata, int sourcex,
                           uint sraster, gx_bitmap_id id,
                           const gx_color_index * scolors,
                           const gx_strip_bitmap * textures,
                           const gx_color_index * tcolors,
                           int x, int y, int width, int height,
                           int phase_x, int phase_y, gs_logical_operation_t lop,
                           uint planar_height)
{
    gx_device_forward * const fdev = (gx_device_forward *)dev;
    gx_device *tdev = fdev->target;

    dev_proc_strip_copy_rop2((*proc2)) =
            (tdev == 0 ? (tdev = dev, gx_default_strip_copy_rop2) :
             dev_proc(tdev, strip_copy_rop2));

    return proc2(tdev, sdata, sourcex, sraster, id, scolors,
                 textures, tcolors, x, y, width, height,
                 phase_x, phase_y, lop, planar_height);
}

int
gx_forward_strip_tile_rect_devn(gx_device * dev, const gx_strip_bitmap * tiles,
   int x, int y, int w, int h, const gx_drawing_color * pdcolor0,
   const gx_drawing_color * pdcolor1, int px, int py)
{
    gx_device_forward * const fdev = (gx_device_forward *)dev;
    gx_device *tdev = fdev->target;

    if (tdev == 0)
        return gx_default_strip_tile_rect_devn(dev, tiles, x, y, w, h, pdcolor0,
                                               pdcolor1, px, py);
    else
        return dev_proc(tdev, strip_tile_rect_devn)(tdev, tiles, x, y, w, h,
                                                    pdcolor0, pdcolor1, px, py);
}

void
gx_forward_get_clipping_box(gx_device * dev, gs_fixed_rect * pbox)
{
    gx_device_forward * const fdev = (gx_device_forward *)dev;
    gx_device *tdev = fdev->target;

    if (tdev == 0)
        gx_default_get_clipping_box(dev, pbox);
    else
        dev_proc(tdev, get_clipping_box)(tdev, pbox);
}

int
gx_forward_begin_typed_image(gx_device * dev, const gs_gstate * pgs,
                             const gs_matrix * pmat,
                             const gs_image_common_t * pim,
                             const gs_int_rect * prect,
                             const gx_drawing_color * pdcolor,
                             const gx_clip_path * pcpath,
                             gs_memory_t * memory,
                             gx_image_enum_common_t ** pinfo)
{
    gx_device_forward * const fdev = (gx_device_forward *)dev;
    gx_device *tdev = fdev->target;
    dev_proc_begin_typed_image((*proc)) =
        (tdev == 0 ? (tdev = dev, gx_default_begin_typed_image) :
         dev_proc(tdev, begin_typed_image));

    return proc(tdev, pgs, pmat, pim, prect, pdcolor, pcpath,
                memory, pinfo);
}

int
gx_forward_get_bits_rectangle(gx_device * dev, const gs_int_rect * prect,
                       gs_get_bits_params_t * params)
{
    gx_device_forward * const fdev = (gx_device_forward *)dev;
    gx_device *tdev = fdev->target;
    dev_proc_get_bits_rectangle((*proc)) =
        (tdev == 0 ? (tdev = dev, gx_default_get_bits_rectangle) :
         dev_proc(tdev, get_bits_rectangle));

    return proc(tdev, prect, params);
}

int
gx_forward_get_hardware_params(gx_device * dev, gs_param_list * plist)
{
    gx_device_forward * const fdev = (gx_device_forward *)dev;
    gx_device *tdev = fdev->target;

    return (tdev == 0 ? gx_default_get_hardware_params(dev, plist) :
            dev_proc(tdev, get_hardware_params)(tdev, plist));
}

int
gx_forward_text_begin(gx_device * dev, gs_gstate * pgs,
                      const gs_text_params_t * text, gs_font * font,
                      const gx_clip_path * pcpath,
                      gs_text_enum_t ** ppenum)
{
    gx_device_forward * const fdev = (gx_device_forward *)dev;
    gx_device *tdev = fdev->target;
    dev_proc_text_begin((*proc)) =
        (tdev == 0 ? (tdev = dev, gx_default_text_begin) :
         dev_proc(tdev, text_begin));

    return proc(tdev, pgs, text, font, pcpath, ppenum);
}

/* Forwarding device color mapping procs. */

/*
 * We need to forward the color mapping to the target device.
 */
static void
fwd_map_gray_cs(const gx_device * dev, frac gray, frac out[])
{
    gx_device_forward * const fdev = (gx_device_forward *)dev;
    gx_device * tdev = fdev->target;
    const gx_device *cmdev;
    const gx_cm_color_map_procs *cmprocs;

    if (tdev) {
        cmprocs = dev_proc(tdev, get_color_mapping_procs)(tdev, &cmdev);
        cmprocs->map_gray(cmdev, gray, out);
    }
    else
        gray_cs_to_gray_cm(tdev, gray, out);   /* if all else fails */
}

/*
 * We need to forward the color mapping to the target device.
 */
static void
fwd_map_rgb_cs(const gx_device * dev, const gs_gstate *pgs,
                                   frac r, frac g, frac b, frac out[])
{
    gx_device_forward * const fdev = (gx_device_forward *)dev;
    gx_device * tdev = fdev->target;
    const gx_device *cmdev;
    const gx_cm_color_map_procs *cmprocs;

    if (tdev) {
        cmprocs = dev_proc(tdev, get_color_mapping_procs)(tdev, &cmdev);
        cmprocs->map_rgb(cmdev, pgs, r, g, b, out);
    }
    else
        rgb_cs_to_rgb_cm(tdev, pgs, r, g, b, out);   /* if all else fails */
}

/*
 * We need to forward the color mapping to the target device.
 */
static void
fwd_map_cmyk_cs(const gx_device * dev, frac c, frac m, frac y, frac k, frac out[])
{
    gx_device_forward * const fdev = (gx_device_forward *)dev;
    gx_device * tdev = fdev->target;
    const gx_device *cmdev;
    const gx_cm_color_map_procs *cmprocs;

    if (tdev) {
        cmprocs = dev_proc(tdev, get_color_mapping_procs)(tdev, &cmdev);
        cmprocs->map_cmyk(cmdev, c, m, y, k, out);
    }
    else
        cmyk_cs_to_cmyk_cm(tdev, c, m, y, k, out);   /* if all else fails */
}

static const gx_cm_color_map_procs FwdDevice_cm_map_procs = {
    fwd_map_gray_cs, fwd_map_rgb_cs, fwd_map_cmyk_cs
};
/*
 * Instead of returning the target device's mapping procs, we need
 * to return a list of forwarding wrapper procs for the color mapping
 * procs.  This is required because the target device mapping procs
 * may also need the target device pointer (instead of the forwarding
 * device pointer).
 */
const gx_cm_color_map_procs *
gx_forward_get_color_mapping_procs(const gx_device * dev, const gx_device **map_dev)
{
    const gx_device_forward * fdev = (const gx_device_forward *)dev;
    gx_device * tdev = fdev->target;

    if (tdev)
        return dev_proc(tdev, get_color_mapping_procs)(tdev, map_dev);

    /* Testing in the cluster seems to indicate that we never get here,
     * but we've written the code now... */
    *map_dev = dev;
    return &FwdDevice_cm_map_procs;
}

int
gx_forward_get_color_comp_index(gx_device * dev, const char * pname,
                                        int name_size, int component_type)
{
    const gx_device_forward * fdev = (const gx_device_forward *)dev;
    gx_device *tdev = fdev->target;

    return (tdev == 0
        ? gx_error_get_color_comp_index(dev, pname,
                                name_size, component_type)
        : dev_proc(tdev, get_color_comp_index)(tdev, pname,
                                name_size, component_type));
}

gx_color_index
gx_forward_encode_color(gx_device * dev, const gx_color_value colors[])
{
    gx_device_forward * const fdev = (gx_device_forward *)dev;
    gx_device *tdev = fdev->target;

    return (tdev == 0 ? gx_error_encode_color(dev, colors)
                      : dev_proc(tdev, encode_color)(tdev, colors));
}

int
gx_forward_decode_color(gx_device * dev, gx_color_index cindex, gx_color_value colors[])
{
    gx_device_forward * const fdev = (gx_device_forward *)dev;
    gx_device *tdev = fdev->target;

    if (tdev == 0)      /* If no device - just clear the color values */
        memset(colors, 0, sizeof(gx_color_value[GX_DEVICE_COLOR_MAX_COMPONENTS]));
    else
        dev_proc(tdev, decode_color)(tdev, cindex, colors);
    return 0;
}

int
gx_forward_dev_spec_op(gx_device * dev, int dev_spec_op, void *data, int size)
{
    gx_device_forward * const fdev = (gx_device_forward *)dev;
    gx_device *tdev = fdev->target;

    /* Note that clist sets fdev->target == fdev,
       so this function is unapplicable to clist. */
    if (tdev == 0) {
        if (dev_spec_op == gxdso_pattern_shfill_doesnt_need_path) {
            return (dev_proc(dev, fill_path) == gx_default_fill_path);
        }
        return_error(gs_error_undefined);
    } else if (dev_spec_op == gxdso_pattern_handles_clip_path) {
        if (dev_proc(dev, fill_path) == gx_default_fill_path)
            return 0;
    } else if (dev_spec_op == gxdso_device_child) {
        gxdso_device_child_request *d = (gxdso_device_child_request *)data;
        if (d->target == dev) {
            d->target = fdev->target;
            return 1;
        }
    } else if (dev_spec_op == gxdso_device_insert_child) {
        fdev->target = (gx_device *)data;
        rc_increment(fdev->target);
        rc_decrement_only(tdev, "gx_forward_device");
        return 0;
    }
    return dev_proc(tdev, dev_spec_op)(tdev, dev_spec_op, data, size);
}

int
gx_forward_fill_rectangle_hl_color(gx_device *dev,
    const gs_fixed_rect *rect,
    const gs_gstate *pgs, const gx_drawing_color *pdcolor,
    const gx_clip_path *pcpath)
{
    gx_device_forward * const fdev = (gx_device_forward *)dev;
    gx_device *tdev = fdev->target;

    /* Note that clist sets fdev->target == fdev,
       so this function is unapplicable to clist. */
    if (tdev == 0)
        return_error(gs_error_rangecheck);
    else
        return dev_proc(tdev, fill_rectangle_hl_color)(tdev, rect,
                                                pgs, pdcolor, pcpath);
}

int
gx_forward_copy_alpha_hl_color(gx_device * dev, const byte * data, int data_x,
           int raster, gx_bitmap_id id, int x, int y, int width, int height,
                      const gx_drawing_color *pdcolor, int depth)
{
    gx_device_forward * const fdev = (gx_device_forward *)dev;
    gx_device *tdev = fdev->target;

    if (tdev == 0)
        return_error(gs_error_rangecheck);
    else
        return dev_proc(tdev, copy_alpha_hl_color)
        (tdev, data, data_x, raster, id, x, y, width, height, pdcolor, depth);
}

int
gx_forward_include_color_space(gx_device *dev, gs_color_space *cspace,
            const byte *res_name, int name_length)
{
    gx_device_forward * const fdev = (gx_device_forward *)dev;
    gx_device *tdev = fdev->target;

    /* Note that clist sets fdev->target == fdev,
       so this function is unapplicable to clist. */
    if (tdev == 0)
        return 0;
    else
        return dev_proc(tdev, include_color_space)(tdev, cspace, res_name, name_length);
}

int
gx_forward_fill_linear_color_scanline(gx_device *dev, const gs_fill_attributes *fa,
        int i, int j, int w,
        const frac31 *c, const int32_t *addx, const int32_t *mulx, int32_t divx)
{
    gx_device_forward * const fdev = (gx_device_forward *)dev;
    gx_device *tdev = fdev->target;
    dev_proc_fill_linear_color_scanline((*proc)) =
        (tdev == 0 ? (tdev = dev, gx_default_fill_linear_color_scanline) :
         dev_proc(tdev, fill_linear_color_scanline));
    return proc(tdev, fa, i, j, w, c, addx, mulx, divx);
}

int
gx_forward_fill_linear_color_trapezoid(gx_device *dev, const gs_fill_attributes *fa,
        const gs_fixed_point *p0, const gs_fixed_point *p1,
        const gs_fixed_point *p2, const gs_fixed_point *p3,
        const frac31 *c0, const frac31 *c1,
        const frac31 *c2, const frac31 *c3)
{
    gx_device_forward * const fdev = (gx_device_forward *)dev;
    gx_device *tdev = fdev->target;
    dev_proc_fill_linear_color_trapezoid((*proc)) =
        (tdev == 0 ? (tdev = dev, gx_default_fill_linear_color_trapezoid) :
         dev_proc(tdev, fill_linear_color_trapezoid));
    return proc(tdev, fa, p0, p1, p2, p3, c0, c1, c2, c3);
}

int
gx_forward_fill_linear_color_triangle(gx_device *dev, const gs_fill_attributes *fa,
        const gs_fixed_point *p0, const gs_fixed_point *p1,
        const gs_fixed_point *p2,
        const frac31 *c0, const frac31 *c1, const frac31 *c2)
{
    gx_device_forward * const fdev = (gx_device_forward *)dev;
    gx_device *tdev = fdev->target;
    dev_proc_fill_linear_color_triangle((*proc)) =
        (tdev == 0 ? (tdev = dev, gx_default_fill_linear_color_triangle) :
         dev_proc(tdev, fill_linear_color_triangle));
    return proc(tdev, fa, p0, p1, p2, c0, c1, c2);
}

int
gx_forward_update_spot_equivalent_colors(gx_device *dev, const gs_gstate * pgs, const gs_color_space *pcs)
{
    gx_device_forward * const fdev = (gx_device_forward *)dev;
    gx_device *tdev = fdev->target;
    int code = 0;

    if (tdev != NULL)
        code = dev_proc(tdev, update_spot_equivalent_colors)(tdev, pgs, pcs);
    return code;
}

gs_devn_params *
gx_forward_ret_devn_params(gx_device *dev)
{
    gx_device_forward * const fdev = (gx_device_forward *)dev;
    gx_device *tdev = fdev->target;

    if (tdev != NULL)
        return dev_proc(tdev, ret_devn_params)(tdev);
    return NULL;
}

int
gx_forward_fillpage(gx_device *dev, gs_gstate * pgs, gx_device_color *pdevc)
{
    gx_device_forward * const fdev = (gx_device_forward *)dev;
    gx_device *tdev = fdev->target;
    dev_proc_fillpage((*proc)) =
        (tdev == 0 ? (tdev = dev, gx_default_fillpage) :
         dev_proc(tdev, fillpage));
    return proc(tdev, pgs, pdevc);
}

int
gx_forward_composite(gx_device * dev, gx_device ** pcdev,
                        const gs_composite_t * pcte,
                        gs_gstate * pgs, gs_memory_t * memory,
                        gx_device *cdev)
{
    gx_device_forward * const fdev = (gx_device_forward *)dev;
    gx_device *tdev = fdev->target;
    int code;

    if (tdev == 0)
        return gx_no_composite(dev, pcdev, pcte, pgs, memory, cdev);
    /* else do the compositor action */
    code = dev_proc(tdev, composite)(tdev, pcdev, pcte, pgs, memory, cdev);
    /* the compositor may have changed color_info. Pick up the new value */
    dev->color_info = tdev->color_info;
    if (code == 1) {
        /* If a new compositor was made that wrapped tdev, then that
         * compositor should be our target now. */
        gx_device_set_target((gx_device_forward *)dev, *pcdev);
        code = 0; /* We have not made a new compositor that wrapped dev. */
    }
    return code;
}

int
gx_forward_get_profile(const gx_device *dev,  cmm_dev_profile_t **profile)
{
    gx_device_forward * const fdev = (gx_device_forward *)dev;
    const gx_device *tdev = fdev->target;

    if (tdev == NULL)
        return gx_default_get_profile(dev, profile);
    return dev_proc(tdev, get_profile)(tdev, profile);
}

void
gx_forward_set_graphics_type_tag(gx_device *dev, gs_graphics_type_tag_t graphics_type_tag)
{
    gx_device_forward * const fdev = (gx_device_forward *)dev;
    gx_device *tdev = fdev->target;

    if (tdev != 0)
        dev_proc(tdev, set_graphics_type_tag)(tdev, graphics_type_tag);
    /* Keep copy in this device current, and preserve GS_DEVICE_ENCODES_TAGS */
    dev->graphics_type_tag = (dev->graphics_type_tag & GS_DEVICE_ENCODES_TAGS) | graphics_type_tag;
}

int
gx_forward_put_image(gx_device *pdev, gx_device *mdev, const byte **buffers, int num_chan, int xstart,
              int ystart, int width, int height, int row_stride,
              int alpha_plane_index, int tag_plane_index)
{
    gx_device_forward * const fdev = (gx_device_forward *)pdev;
    gx_device *tdev = fdev->target;

    if (tdev != 0)
        return dev_proc(tdev, put_image)(tdev, mdev, buffers, num_chan, xstart, ystart, width, height, row_stride, alpha_plane_index, tag_plane_index);
    else
        return gx_default_put_image(tdev, mdev, buffers, num_chan, xstart, ystart, width, height, row_stride, alpha_plane_index, tag_plane_index);
}

int
gx_forward_transform_pixel_region(gx_device *pdev, transform_pixel_region_reason reason, transform_pixel_region_data *data)
{
    gx_device_forward * const fdev = (gx_device_forward *)pdev;
    gx_device *tdev = fdev->target;

    if (tdev != 0)
        return dev_proc(tdev, transform_pixel_region)(tdev, reason, data);
    else
        return gx_default_transform_pixel_region(pdev, reason, data);
}

/* ---------------- The null device(s) ---------_plane_index------- */

static dev_proc_get_initial_matrix(gx_forward_upright_get_initial_matrix);
static dev_proc_fill_rectangle(null_fill_rectangle);
static dev_proc_copy_mono(null_copy_mono);
static dev_proc_copy_color(null_copy_color);
static dev_proc_put_params(null_put_params);
static dev_proc_copy_alpha(null_copy_alpha);
static dev_proc_fill_path(null_fill_path);
static dev_proc_stroke_path(null_stroke_path);
static dev_proc_fill_trapezoid(null_fill_trapezoid);
static dev_proc_fill_parallelogram(null_fill_parallelogram);
static dev_proc_fill_triangle(null_fill_triangle);
static dev_proc_draw_thin_line(null_draw_thin_line);
static dev_proc_decode_color(null_decode_color);
/* We would like to have null implementations of begin/data/end image, */
/* but we can't do this, because image_data must keep track of the */
/* Y position so it can return 1 when done. */
static dev_proc_strip_copy_rop2(null_strip_copy_rop2);
static dev_proc_strip_tile_rect_devn(null_strip_tile_rect_devn);
static dev_proc_fill_rectangle_hl_color(null_fill_rectangle_hl_color);
static dev_proc_dev_spec_op(null_spec_op);

static void
null_initialize_device_procs(gx_device *dev)
{
    set_dev_proc(dev, get_initial_matrix, gx_forward_upright_get_initial_matrix);
    set_dev_proc(dev, get_page_device, gx_default_get_page_device);
    set_dev_proc(dev, map_rgb_color, gx_forward_map_rgb_color);
    set_dev_proc(dev, map_color_rgb, gx_forward_map_color_rgb);
    set_dev_proc(dev, fill_rectangle, null_fill_rectangle);
    set_dev_proc(dev, copy_mono, null_copy_mono);
    set_dev_proc(dev, copy_color, null_copy_color);
    set_dev_proc(dev, get_params, gx_forward_get_params);
    set_dev_proc(dev, put_params, null_put_params);
    set_dev_proc(dev, map_cmyk_color, gx_forward_map_cmyk_color);
    set_dev_proc(dev, copy_alpha, null_copy_alpha);
    set_dev_proc(dev, fill_path, null_fill_path);
    set_dev_proc(dev, stroke_path, null_stroke_path);
    set_dev_proc(dev, fill_trapezoid, null_fill_trapezoid);
    set_dev_proc(dev, fill_parallelogram, null_fill_parallelogram);
    set_dev_proc(dev, fill_triangle, null_fill_triangle);
    set_dev_proc(dev, draw_thin_line, null_draw_thin_line);
    set_dev_proc(dev, composite, gx_non_imaging_composite);
    set_dev_proc(dev, get_hardware_params, gx_forward_get_hardware_params);
    set_dev_proc(dev, get_color_mapping_procs, gx_default_DevGray_get_color_mapping_procs);
    set_dev_proc(dev, get_color_comp_index, gx_default_DevGray_get_color_comp_index);
    set_dev_proc(dev, encode_color, gx_default_gray_fast_encode);
    set_dev_proc(dev, decode_color, null_decode_color);
    set_dev_proc(dev, fill_rectangle_hl_color, null_fill_rectangle_hl_color);
    set_dev_proc(dev, dev_spec_op, null_spec_op);
    set_dev_proc(dev, strip_copy_rop2, null_strip_copy_rop2);
    set_dev_proc(dev, strip_tile_rect_devn, null_strip_tile_rect_devn);
}

static void
nullpage_initialize_device_procs(gx_device *dev)
{
    null_initialize_device_procs(dev);

    set_dev_proc(dev, get_initial_matrix, gx_forward_get_initial_matrix);
    set_dev_proc(dev, get_page_device, gx_page_device_get_page_device);
}

#define NULLD_X_RES 72
#define NULLD_Y_RES 72

const gx_device_null gs_null_device = {
    std_device_std_body_type_open(gx_device_null,
                                  null_initialize_device_procs,
                                  "null", &st_device_null,
                                  0, 0, NULLD_X_RES, NULLD_Y_RES)
};

const gx_device_null gs_nullpage_device = {
std_device_std_body_type_open(gx_device_null,
                              nullpage_initialize_device_procs,
                              "nullpage", &st_device_null,
                              (int)((float)(DEFAULT_WIDTH_10THS * NULLD_X_RES) / 10),
                              (int)((float)(DEFAULT_HEIGHT_10THS * NULLD_Y_RES) / 10),
                              NULLD_X_RES, NULLD_Y_RES)
};

static void
gx_forward_upright_get_initial_matrix(gx_device * dev, gs_matrix * pmat)
{
    gx_device_forward * const fdev = (gx_device_forward *)dev;
    gx_device *tdev = fdev->target;

    if (tdev == 0)
        gx_upright_get_initial_matrix(dev, pmat);
    else
        dev_proc(tdev, get_initial_matrix)(tdev, pmat);
}

static int
null_decode_color(gx_device * dev, gx_color_index cindex, gx_color_value colors[])
{
    colors[0] = (cindex & 1) ?  gx_max_color_value : 0;
    return 0;
}

static int
null_fill_rectangle(gx_device * dev, int x, int y, int w, int h,
                    gx_color_index color)
{
    return 0;
}
static int
null_copy_mono(gx_device * dev, const byte * data, int dx, int raster,
               gx_bitmap_id id, int x, int y, int w, int h,
               gx_color_index zero, gx_color_index one)
{
    return 0;
}
static int
null_copy_color(gx_device * dev, const byte * data,
                int data_x, int raster, gx_bitmap_id id,
                int x, int y, int width, int height)
{
    return 0;
}
static int
null_put_params(gx_device * dev, gs_param_list * plist)
{
    int code;
    cmm_dev_profile_t *iccs = dev->icc_struct;

    /*
     * If this is not a page device, we must defeat attempts to reset
     * the size; otherwise this is equivalent to gx_forward_put_params.
     * Equally, we don't want it to unexpectectly error out on certain
     * ICC parameters - so defeat those, too.
     */
    dev->icc_struct = NULL;
    code = gx_forward_put_params(dev, plist);
    rc_decrement(dev->icc_struct, "null_put_params");
    dev->icc_struct = iccs;

    if (code < 0 || dev_proc(dev, get_page_device)(dev) == dev)
        return code;
    dev->width = dev->height = 0;
    return code;
}
static int
null_copy_alpha(gx_device * dev, const byte * data, int data_x, int raster,
                gx_bitmap_id id, int x, int y, int width, int height,
                gx_color_index color, int depth)
{
    return 0;
}
static int
null_fill_path(gx_device * dev, const gs_gstate * pgs,
               gx_path * ppath, const gx_fill_params * params,
               const gx_drawing_color * pdcolor, const gx_clip_path * pcpath)
{
    return 0;
}
static int
null_stroke_path(gx_device * dev, const gs_gstate * pgs,
                 gx_path * ppath, const gx_stroke_params * params,
                 const gx_drawing_color * pdcolor, const gx_clip_path * pcpath)
{
    return 0;
}
static int
null_fill_trapezoid(gx_device * dev,
                    const gs_fixed_edge * left, const gs_fixed_edge * right,
                    fixed ybot, fixed ytop, bool swap_axes,
                    const gx_drawing_color * pdcolor,
                    gs_logical_operation_t lop)
{
    return 0;
}
static int
null_fill_parallelogram(gx_device * dev, fixed px, fixed py,
                        fixed ax, fixed ay, fixed bx, fixed by,
                        const gx_drawing_color * pdcolor,
                        gs_logical_operation_t lop)
{
    return 0;
}
static int
null_fill_triangle(gx_device * dev,
                   fixed px, fixed py, fixed ax, fixed ay, fixed bx, fixed by,
                   const gx_drawing_color * pdcolor,
                   gs_logical_operation_t lop)
{
    return 0;
}
static int
null_draw_thin_line(gx_device * dev,
                    fixed fx0, fixed fy0, fixed fx1, fixed fy1,
                    const gx_drawing_color * pdcolor,
                    gs_logical_operation_t lop,
                    fixed adjustx, fixed adjusty)
{
    return 0;
}
static int
null_strip_copy_rop2(gx_device * dev, const byte * sdata, int sourcex,
                     uint sraster, gx_bitmap_id id,
                     const gx_color_index * scolors,
                     const gx_strip_bitmap * textures,
                     const gx_color_index * tcolors,
                     int x, int y, int width, int height,
                     int phase_x, int phase_y, gs_logical_operation_t lop,
                     uint plane_height)
{
    return 0;
}

static int
null_strip_tile_rect_devn(gx_device * dev, const gx_strip_bitmap * tiles,
   int x, int y, int w, int h, const gx_drawing_color * pdcolor0,
   const gx_drawing_color * pdcolor1, int px, int py)
{
    return 0;
}

/* We use this to erase a pattern background, if pdfwrite has pushed
 * the NULL device to dispense with text (because its a strinwidth)
 * we don't want to throw an error when erasing the pattern which
 * fills the text, because pdfwrite manages the pattern itself and
 * we don't need to erase hte background. More generally, since the
 * null device is a bit bucket, it shouldn't really throw errors
 * when asked to render anything at all.
 */
static int null_fill_rectangle_hl_color(gx_device *pdev,
    const gs_fixed_rect *rect,
    const gs_gstate *pgs, const gx_drawing_color *pdcolor,
    const gx_clip_path *pcpath)
{
    return 0;
}

static int
null_spec_op(gx_device *pdev, int dev_spec_op, void *data, int size)
{
    /* Defeat the ICC profile components check, which we want to do since
       we also short-circuit ICC device parameters - see null_put_params.
     */
    if (dev_spec_op == gxdso_skip_icc_component_validation)
        return 1;
    if (dev_spec_op == gxdso_is_null_device)
        return 1;
    return gx_default_dev_spec_op(pdev, dev_spec_op, data, size);
}

void gx_forward_device_initialize_procs(gx_device *dev)
{
    fill_dev_proc(dev, close_device, gx_forward_close_device);
    fill_dev_proc(dev, get_initial_matrix, gx_forward_get_initial_matrix);
    fill_dev_proc(dev, sync_output, gx_forward_sync_output);
    fill_dev_proc(dev, output_page, gx_forward_output_page);
    fill_dev_proc(dev, map_rgb_color, gx_forward_map_rgb_color);
    fill_dev_proc(dev, map_color_rgb, gx_forward_map_color_rgb);
    fill_dev_proc(dev, fill_rectangle, gx_forward_fill_rectangle);
    fill_dev_proc(dev, copy_mono, gx_forward_copy_mono);
    fill_dev_proc(dev, copy_color, gx_forward_copy_color);
    fill_dev_proc(dev, get_params, gx_forward_get_params);
    fill_dev_proc(dev, put_params, gx_forward_put_params);
    fill_dev_proc(dev, map_cmyk_color, gx_forward_map_cmyk_color);
    fill_dev_proc(dev, get_page_device, gx_forward_get_page_device);
    fill_dev_proc(dev, get_alpha_bits, gx_forward_get_alpha_bits);
    fill_dev_proc(dev, copy_alpha, gx_forward_copy_alpha);
    fill_dev_proc(dev, fill_path, gx_forward_fill_path);
    fill_dev_proc(dev, stroke_path, gx_forward_stroke_path);
    fill_dev_proc(dev, fill_mask, gx_forward_fill_mask);
    fill_dev_proc(dev, fill_trapezoid, gx_forward_fill_trapezoid);
    fill_dev_proc(dev, fill_parallelogram, gx_forward_fill_parallelogram);
    fill_dev_proc(dev, fill_triangle, gx_forward_fill_triangle);
    fill_dev_proc(dev, draw_thin_line, gx_forward_draw_thin_line);
    fill_dev_proc(dev, strip_tile_rectangle, gx_forward_strip_tile_rectangle);
    fill_dev_proc(dev, get_clipping_box, gx_forward_get_clipping_box);
    fill_dev_proc(dev, begin_typed_image, gx_forward_begin_typed_image);
    fill_dev_proc(dev, get_bits_rectangle, gx_forward_get_bits_rectangle);
    /* There is no forward_composite (see Drivers.htm). */
    fill_dev_proc(dev, get_hardware_params, gx_forward_get_hardware_params);
    fill_dev_proc(dev, text_begin, gx_forward_text_begin);
    fill_dev_proc(dev, get_color_mapping_procs, gx_forward_get_color_mapping_procs);
    fill_dev_proc(dev, get_color_comp_index, gx_forward_get_color_comp_index);
    fill_dev_proc(dev, encode_color, gx_forward_encode_color);
    fill_dev_proc(dev, decode_color, gx_forward_decode_color);
    fill_dev_proc(dev, dev_spec_op, gx_forward_dev_spec_op);
    fill_dev_proc(dev, fill_rectangle_hl_color, gx_forward_fill_rectangle_hl_color);
    fill_dev_proc(dev, include_color_space, gx_forward_include_color_space);
    fill_dev_proc(dev, fill_linear_color_scanline, gx_forward_fill_linear_color_scanline);
    fill_dev_proc(dev, fill_linear_color_trapezoid, gx_forward_fill_linear_color_trapezoid);
    fill_dev_proc(dev, fill_linear_color_triangle, gx_forward_fill_linear_color_triangle);
    fill_dev_proc(dev, update_spot_equivalent_colors, gx_forward_update_spot_equivalent_colors);
    fill_dev_proc(dev, ret_devn_params, gx_forward_ret_devn_params);
    fill_dev_proc(dev, fillpage, gx_forward_fillpage);
    fill_dev_proc(dev, put_image, gx_forward_put_image);
    fill_dev_proc(dev, copy_planes, gx_forward_copy_planes);
    fill_dev_proc(dev, composite, gx_forward_composite);
    fill_dev_proc(dev, get_profile, gx_forward_get_profile);
    fill_dev_proc(dev, set_graphics_type_tag, gx_forward_set_graphics_type_tag);
    fill_dev_proc(dev, strip_copy_rop2, gx_forward_strip_copy_rop2);
    fill_dev_proc(dev, strip_tile_rect_devn, gx_forward_strip_tile_rect_devn);
    fill_dev_proc(dev, copy_alpha_hl_color, gx_forward_copy_alpha_hl_color);
    fill_dev_proc(dev, transform_pixel_region, gx_forward_transform_pixel_region);
    fill_dev_proc(dev, fill_stroke_path, gx_forward_fill_stroke_path);
}

#ifdef DEBUG
static void do_device_dump(gx_device *dev, int n)
{
    int i, ret;
    gxdso_device_child_request data;

    /* Dump the details of device dev */
    for (i = 0; i < n; i++)
        dmlprintf(dev->memory, " ");
    if (dev == NULL) {
        dmlprintf(dev->memory, "NULL\n");
        return;
    }
    dmlprintf3(dev->memory, PRI_INTPTR"(%ld) = '%s'\n", (intptr_t)dev, dev->rc.ref_count, dev->dname);

    data.n = 0;
    do {
        data.target = dev;
        ret = dev_proc(dev, dev_spec_op)(dev, gxdso_device_child, &data, sizeof(data));
        if (ret > 0)
            do_device_dump(data.target, n+1);
    } while ((ret > 0) && (data.n != 0));
}

void gx_device_dump(gx_device *dev, const char *text)
{
    dmlprintf1(dev->memory, "%s", text);
    do_device_dump(dev, 0);
}
#endif
