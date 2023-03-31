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


/* Mask clipping device */
#include "memory_.h"
#include "gx.h"
#include "gsbittab.h"
#include "gxdevice.h"
#include "gxdevmem.h"
#include "gxclipm.h"
#include "gxdcolor.h"

/* Device procedures */
static dev_proc_fill_rectangle(mask_clip_fill_rectangle);
static dev_proc_fill_rectangle_hl_color(mask_clip_fill_rectangle_hl_color);
static dev_proc_copy_mono(mask_clip_copy_mono);
static dev_proc_copy_color(mask_clip_copy_color);
static dev_proc_copy_alpha(mask_clip_copy_alpha);
static dev_proc_copy_alpha_hl_color(mask_clip_copy_alpha_hl_color);
static dev_proc_strip_tile_rectangle(mask_clip_strip_tile_rectangle);
static dev_proc_strip_tile_rect_devn(mask_clip_strip_tile_rect_devn);
static dev_proc_strip_copy_rop2(mask_clip_strip_copy_rop2);
static dev_proc_get_clipping_box(mask_clip_get_clipping_box);

/* The device descriptor. */

static void
mask_clip_initialize_device_procs(gx_device *dev)
{
    set_dev_proc(dev, get_initial_matrix, gx_forward_get_initial_matrix);
    set_dev_proc(dev, map_rgb_color, gx_forward_map_rgb_color);
    set_dev_proc(dev, map_color_rgb, gx_forward_map_color_rgb);
    set_dev_proc(dev, fill_rectangle, mask_clip_fill_rectangle);
    set_dev_proc(dev, copy_mono, mask_clip_copy_mono);
    set_dev_proc(dev, copy_color, mask_clip_copy_color);
    set_dev_proc(dev, get_params, gx_forward_get_params);
    set_dev_proc(dev, put_params, gx_forward_put_params);
    set_dev_proc(dev, map_cmyk_color, gx_forward_map_cmyk_color);
    set_dev_proc(dev, get_page_device, gx_forward_get_page_device);
    set_dev_proc(dev, get_alpha_bits, gx_forward_get_alpha_bits);
    set_dev_proc(dev, copy_alpha, mask_clip_copy_alpha);
    set_dev_proc(dev, strip_tile_rectangle, mask_clip_strip_tile_rectangle);
    set_dev_proc(dev, get_clipping_box, mask_clip_get_clipping_box);
    set_dev_proc(dev, get_bits_rectangle, gx_forward_get_bits_rectangle);
    set_dev_proc(dev, composite, gx_no_composite);
    set_dev_proc(dev, get_hardware_params, gx_forward_get_hardware_params);
    set_dev_proc(dev, get_color_mapping_procs, gx_forward_get_color_mapping_procs);
    set_dev_proc(dev, get_color_comp_index, gx_forward_get_color_comp_index);
    set_dev_proc(dev, encode_color, gx_forward_encode_color);
    set_dev_proc(dev, decode_color, gx_forward_decode_color);
    set_dev_proc(dev, fill_rectangle_hl_color, mask_clip_fill_rectangle_hl_color);
    set_dev_proc(dev, include_color_space, gx_forward_include_color_space);
    set_dev_proc(dev, fill_linear_color_scanline, gx_forward_fill_linear_color_scanline);
    set_dev_proc(dev, fill_linear_color_trapezoid, gx_forward_fill_linear_color_trapezoid);
    set_dev_proc(dev, fill_linear_color_triangle, gx_forward_fill_linear_color_triangle);
    set_dev_proc(dev, update_spot_equivalent_colors, gx_forward_update_spot_equivalent_colors);
    set_dev_proc(dev, ret_devn_params, gx_forward_ret_devn_params);
    set_dev_proc(dev, fillpage, gx_forward_fillpage);
    set_dev_proc(dev, dev_spec_op, gx_forward_dev_spec_op);
    set_dev_proc(dev, get_profile, gx_forward_get_profile);
    set_dev_proc(dev, set_graphics_type_tag, gx_forward_set_graphics_type_tag);
    set_dev_proc(dev, strip_copy_rop2, mask_clip_strip_copy_rop2);
    set_dev_proc(dev, strip_tile_rect_devn, mask_clip_strip_tile_rect_devn);
    set_dev_proc(dev, copy_alpha_hl_color, mask_clip_copy_alpha_hl_color);
    set_dev_proc(dev, transform_pixel_region, gx_default_transform_pixel_region);
    set_dev_proc(dev, fill_stroke_path, gx_forward_fill_stroke_path);
    set_dev_proc(dev, lock_pattern, gx_forward_lock_pattern);

    /* Ideally these defaults would be set up automatically for us. */
    set_dev_proc(dev, open_device, gx_default_open_device);
    set_dev_proc(dev, sync_output, gx_default_sync_output);
    set_dev_proc(dev, output_page, gx_default_output_page);
    set_dev_proc(dev, close_device, gx_default_close_device);
    set_dev_proc(dev, fill_path, gx_default_fill_path);
    set_dev_proc(dev, stroke_path, gx_default_stroke_path);
    set_dev_proc(dev, fill_mask, gx_default_fill_mask);
    set_dev_proc(dev, fill_trapezoid, gx_default_fill_trapezoid);
    set_dev_proc(dev, fill_parallelogram, gx_default_fill_parallelogram);
    set_dev_proc(dev, fill_triangle, gx_default_fill_triangle);
    set_dev_proc(dev, draw_thin_line, gx_default_draw_thin_line);
    set_dev_proc(dev, begin_typed_image, gx_default_begin_typed_image);
    set_dev_proc(dev, text_begin, gx_default_text_begin);

 }

const gx_device_mask_clip gs_mask_clip_device =
{std_device_std_body_open(gx_device_mask_clip,
                          mask_clip_initialize_device_procs,
                          "mask clipper",
                          0, 0, 1, 1)
};

/* Fill a rectangle with a hl color, painting through the mask */
static int
mask_clip_fill_rectangle_hl_color(gx_device *dev,
    const gs_fixed_rect *rect,
    const gs_gstate *pgs, const gx_drawing_color *pdcolor,
    const gx_clip_path *pcpath)
{
    gx_device_mask_clip *cdev = (gx_device_mask_clip *) dev;
    gx_device *tdev = cdev->target;
    int x, y, w, h;
    int mx0, mx1, my0, my1;

    x = fixed2int(rect->p.x);
    y = fixed2int(rect->p.y);
    w = fixed2int(rect->q.x) - x;
    h = fixed2int(rect->q.y) - y;

    /* Clip the rectangle to the region covered by the mask. */
    mx0 = x + cdev->phase.x;
    my0 = y + cdev->phase.y;
    mx1 = mx0 + w;
    my1 = my0 + h;

    if (mx0 < 0)
        mx0 = 0;
    if (my0 < 0)
        my0 = 0;
    if (mx1 > cdev->tiles.size.x)
        mx1 = cdev->tiles.size.x;
    if (my1 > cdev->tiles.size.y)
        my1 = cdev->tiles.size.y;
    /* It would be nice to have a higher level way to do this operation
       like a copy_mono_hl_color */
    return (pdcolor->type->fill_masked)
            (pdcolor, cdev->tiles.data + my0 * cdev->tiles.raster, mx0,
             cdev->tiles.raster, cdev->tiles.id, mx0 - cdev->phase.x,
             my0 - cdev->phase.y, mx1 - mx0, my1 - my0,
             tdev, lop_default, false);
}

/* Fill a rectangle by painting through the mask. */
static int
mask_clip_fill_rectangle(gx_device * dev, int x, int y, int w, int h,
                         gx_color_index color)
{
    gx_device_mask_clip *cdev = (gx_device_mask_clip *) dev;
    gx_device *tdev = cdev->target;

    /* Clip the rectangle to the region covered by the mask. */
    int mx0 = x + cdev->phase.x, my0 = y + cdev->phase.y;
    int mx1 = mx0 + w, my1 = my0 + h;

    if (mx0 < 0)
        mx0 = 0;
    if (my0 < 0)
        my0 = 0;
    if (mx1 > cdev->tiles.size.x)
        mx1 = cdev->tiles.size.x;
    if (my1 > cdev->tiles.size.y)
        my1 = cdev->tiles.size.y;
    return (*dev_proc(tdev, copy_mono))
        (tdev, cdev->tiles.data + my0 * cdev->tiles.raster, mx0,
         cdev->tiles.raster, cdev->tiles.id,
         mx0 - cdev->phase.x, my0 - cdev->phase.y,
         mx1 - mx0, my1 - my0, gx_no_color_index, color);
}

/*
 * Clip the rectangle for a copy operation.
 * Sets m{x,y}{0,1} to the region in the mask coordinate system;
 * subtract cdev->phase.{x,y} to get target coordinates.
 * Sets sdata, sx to adjusted values of data, sourcex.
 * References cdev, data, sourcex, raster, x, y, w, h.
 */
#define DECLARE_MASK_COPY\
        const byte *sdata;\
        int sx, mx0, my0, mx1, my1
#define FIT_MASK_COPY(data, sourcex, raster, vx, vy, vw, vh)\
        BEGIN\
          sdata = data, sx = sourcex;\
          mx0 = vx + cdev->phase.x, my0 = vy + cdev->phase.y;\
          mx1 = mx0 + vw, my1 = my0 + vh;\
          if ( mx0 < 0 )\
            sx -= mx0, mx0 = 0;\
          if ( my0 < 0 )\
            sdata -= my0 * raster, my0 = 0;\
          if ( mx1 > cdev->tiles.size.x )\
            mx1 = cdev->tiles.size.x;\
          if ( my1 > cdev->tiles.size.y )\
            my1 = cdev->tiles.size.y;\
        END

/* Copy a monochrome bitmap by playing Boolean games. */
static int
mask_clip_copy_mono(gx_device * dev,
                const byte * data, int sourcex, int raster, gx_bitmap_id id,
                    int x, int y, int w, int h,
                    gx_color_index color0, gx_color_index color1)
{
    gx_device_mask_clip *cdev = (gx_device_mask_clip *) dev;
    gx_device *tdev = cdev->target;
    gx_color_index color, mcolor0, mcolor1;

    DECLARE_MASK_COPY;
    int cy, ny;
    int code;

    setup_mask_copy_mono(cdev, color, mcolor0, mcolor1);
    FIT_MASK_COPY(data, sourcex, raster, x, y, w, h);
    for (cy = my0; cy < my1; cy += ny) {
        int ty = cy - cdev->phase.y;
        int cx, nx;

        ny = my1 - cy;
        if (ny > cdev->mdev.height)
            ny = cdev->mdev.height;
        for (cx = mx0; cx < mx1; cx += nx) {
            int tx = cx - cdev->phase.x;

            nx = mx1 - cx;	/* also should be min */
            /* Copy a tile slice to the memory device buffer. */
            memcpy(cdev->buffer.bytes,
                   cdev->tiles.data + cy * cdev->tiles.raster,
                   (size_t)cdev->tiles.raster * ny);
            /* Intersect the tile with the source data. */
            /* mcolor0 and mcolor1 invert the data if needed. */
            /* This call can't fail. */
            (*dev_proc(&cdev->mdev, copy_mono)) ((gx_device *) & cdev->mdev,
                                     sdata + (ty - y) * raster, sx + tx - x,
                                                 raster, gx_no_bitmap_id,
                                           cx, 0, nx, ny, mcolor0, mcolor1);
            /* Now copy the color through the double mask. */
            code = (*dev_proc(tdev, copy_mono)) (cdev->target,
                                 cdev->buffer.bytes, cx, cdev->tiles.raster,
                                                 gx_no_bitmap_id,
                                  tx, ty, nx, ny, gx_no_color_index, color);
            if (code < 0)
                return code;
        }
    }
    return 0;
}

#ifdef PACIFY_VALGRIND
static inline byte trim(int cx, int mx1, byte v)
{
    int mask = 8-(mx1-cx); /* mask < 8 */
    mask = (mask > 0 ? (0xff<<mask) : 0xff)>>(cx & 7);
    return v & mask;
}
#else
#define trim(cx,mx1,v) (v)
#endif

/*
 * Define the run enumerator for the other copying operations.  We can't use
 * the BitBlt tricks: we have to scan for runs of 1s.  There are obvious
 * ways to speed this up; we'll implement some if we need to.
 */
static int
clip_runs_enumerate(gx_device_mask_clip * cdev,
                    int (*process) (clip_callback_data_t * pccd, int xc, int yc, int xec, int yec),
                    clip_callback_data_t * pccd)
{
    DECLARE_MASK_COPY;
    int cy;
    const byte *tile_row;
    gs_int_rect prev;
    int code;

    FIT_MASK_COPY(pccd->data, pccd->sourcex, pccd->raster,
                  pccd->x, pccd->y, pccd->w, pccd->h);
    tile_row = cdev->tiles.data + my0 * cdev->tiles.raster + (mx0 >> 3);
    prev.p.x = 0;	/* arbitrary */
    prev.q.x = prev.p.x - 1;	/* an impossible rectangle */
    prev.p.y = prev.q.y = -1;	/* arbitrary */
    for (cy = my0; cy < my1; cy++) {
        int cx = mx0;
        const byte *tp = tile_row;

        if_debug1m('B', cdev->memory, "[B]clip runs y=%d:", cy - cdev->phase.y);
        while (cx < mx1) {
            int len;
            int tx1, tx, ty;

            /* Skip a run of 0s. */
            len = byte_bit_run_length[cx & 7][trim(cx, mx1, *tp) ^ 0xff];
            if (len < 8) {
                cx += len;
                if (cx >= mx1)
                    break;
            } else {
                cx += len - 8;
                tp++;
                while (cx < mx1 && trim(cx, mx1, *tp) == 0)
                    cx += 8, tp++;
                if (cx >= mx1)
                    break;
                cx += byte_bit_run_length_0[trim(cx, mx1, *tp) ^ 0xff];
                if (cx >= mx1)
                    break;
            }
            tx1 = cx - cdev->phase.x;
            /* Scan a run of 1s. */
            len = byte_bit_run_length[cx & 7][trim(cx, mx1, *tp)];
            if (len < 8) {
                cx += len;
                if (cx > mx1)
                    cx = mx1;
            } else {
                cx += len - 8;
                tp++;
                while (cx < mx1 && trim(cx, mx1, *tp) == 0xff)
                    cx += 8, tp++;
                if (cx > mx1)
                    cx = mx1;
                else {
                    cx += byte_bit_run_length_0[trim(cx, mx1, *tp)];
#undef trim
                    if (cx > mx1)
                        cx = mx1;
                }
            }
            tx = cx - cdev->phase.x;
            if_debug2m('B', cdev->memory, " %d-%d,", tx1, tx);
            ty = cy - cdev->phase.y;
            /* Detect vertical rectangles. */
            if (prev.p.x == tx1 && prev.q.x == tx && prev.q.y == ty)
                prev.q.y = ty + 1;
            else {
                if (prev.q.y > prev.p.y) {
                    code = (*process)(pccd, prev.p.x, prev.p.y, prev.q.x, prev.q.y);
                    if (code < 0)
                        return code;
                }
                prev.p.x = tx1;
                prev.p.y = ty;
                prev.q.x = tx;
                prev.q.y = ty + 1;
            }
        }
        if_debug0m('B', cdev->memory, "\n");
        tile_row += cdev->tiles.raster;
    }
    if (prev.q.y > prev.p.y) {
        code = (*process)(pccd, prev.p.x, prev.p.y, prev.q.x, prev.q.y);
        if (code < 0)
            return code;
    }
    return 0;
}

/* Copy a color rectangle */
static int
mask_clip_copy_color(gx_device * dev,
                const byte * data, int sourcex, int raster, gx_bitmap_id id,
                     int x, int y, int w, int h)
{
    gx_device_mask_clip *cdev = (gx_device_mask_clip *) dev;
    clip_callback_data_t ccdata;

    ccdata.tdev = cdev->target;
    ccdata.data = data, ccdata.sourcex = sourcex, ccdata.raster = raster;
    ccdata.x = x, ccdata.y = y, ccdata.w = w, ccdata.h = h;
    return clip_runs_enumerate(cdev, clip_call_copy_color, &ccdata);
}

/* Copy a rectangle with alpha */
static int
mask_clip_copy_alpha(gx_device * dev,
                const byte * data, int sourcex, int raster, gx_bitmap_id id,
                int x, int y, int w, int h, gx_color_index color, int depth)
{
    gx_device_mask_clip *cdev = (gx_device_mask_clip *) dev;
    clip_callback_data_t ccdata;

    ccdata.tdev = cdev->target;
    ccdata.data = data, ccdata.sourcex = sourcex, ccdata.raster = raster;
    ccdata.x = x, ccdata.y = y, ccdata.w = w, ccdata.h = h;
    ccdata.color[0] = color, ccdata.depth = depth;
    return clip_runs_enumerate(cdev, clip_call_copy_alpha, &ccdata);
}

static int
mask_clip_copy_alpha_hl_color(gx_device * dev,
                const byte * data, int sourcex, int raster, gx_bitmap_id id,
                int x, int y, int w, int h, const gx_drawing_color *pdcolor,
                int depth)
{
    gx_device_mask_clip *cdev = (gx_device_mask_clip *) dev;
    clip_callback_data_t ccdata;

    ccdata.tdev = cdev->target;
    ccdata.data = data, ccdata.sourcex = sourcex, ccdata.raster = raster;
    ccdata.x = x, ccdata.y = y, ccdata.w = w, ccdata.h = h;
    ccdata.pdcolor = pdcolor, ccdata.depth = depth;
    return clip_runs_enumerate(cdev, clip_call_copy_alpha_hl_color, &ccdata);
}

static int
mask_clip_strip_tile_rectangle(gx_device * dev, const gx_strip_bitmap * tiles,
                               int x, int y, int w, int h,
                               gx_color_index color0, gx_color_index color1,
                               int phase_x, int phase_y)
{
    gx_device_mask_clip *cdev = (gx_device_mask_clip *) dev;
    clip_callback_data_t ccdata;

    ccdata.tdev = cdev->target;
    ccdata.x = x, ccdata.y = y, ccdata.w = w, ccdata.h = h;
    ccdata.tiles = tiles;
    ccdata.color[0] = color0, ccdata.color[1] = color1;
    ccdata.phase.x = phase_x, ccdata.phase.y = phase_y;
    return clip_runs_enumerate(cdev, clip_call_strip_tile_rectangle, &ccdata);
}

static int
mask_clip_strip_tile_rect_devn(gx_device * dev, const gx_strip_bitmap * tiles,
                               int x, int y, int w, int h,
                               const gx_drawing_color *pdcolor0,
                               const gx_drawing_color *pdcolor1,
                               int phase_x, int phase_y)
{
    gx_device_mask_clip *cdev = (gx_device_mask_clip *) dev;
    clip_callback_data_t ccdata;

    ccdata.tdev = cdev->target;
    ccdata.x = x, ccdata.y = y, ccdata.w = w, ccdata.h = h;
    ccdata.tiles = tiles;
    ccdata.pdc[0] = pdcolor0, ccdata.pdc[1] = pdcolor1;
    ccdata.phase.x = phase_x, ccdata.phase.y = phase_y;
    return clip_runs_enumerate(cdev, clip_call_strip_tile_rect_devn, &ccdata);
}

static int
mask_clip_strip_copy_rop2(gx_device * dev,
               const byte * data, int sourcex, uint raster, gx_bitmap_id id,
                         const gx_color_index * scolors,
           const gx_strip_bitmap * textures, const gx_color_index * tcolors,
                         int x, int y, int w, int h,
                       int phase_x, int phase_y, gs_logical_operation_t lop,
                       uint planar_height)
{
    gx_device_mask_clip *cdev = (gx_device_mask_clip *) dev;
    clip_callback_data_t ccdata;

    ccdata.tdev = cdev->target;
    ccdata.x = x, ccdata.y = y, ccdata.w = w, ccdata.h = h;
    ccdata.data = data, ccdata.sourcex = sourcex, ccdata.raster = raster;
    ccdata.scolors = scolors, ccdata.textures = textures,
        ccdata.tcolors = tcolors;
    ccdata.phase.x = phase_x, ccdata.phase.y = phase_y, ccdata.lop = lop;
    ccdata.plane_height = planar_height;
    return clip_runs_enumerate(cdev, clip_call_strip_copy_rop2, &ccdata);
}

static void
mask_clip_get_clipping_box(gx_device * dev, gs_fixed_rect * pbox)
{
    gx_device_mask_clip *cdev = (gx_device_mask_clip *) dev;
    gx_device *tdev = cdev->target;
    gs_fixed_rect tbox;

    (*dev_proc(tdev, get_clipping_box)) (tdev, &tbox);
    pbox->p.x = tbox.p.x - cdev->phase.x;
    pbox->p.y = tbox.p.y - cdev->phase.y;
    pbox->q.x = tbox.q.x - cdev->phase.x;
    pbox->q.y = tbox.q.y - cdev->phase.y;
}
