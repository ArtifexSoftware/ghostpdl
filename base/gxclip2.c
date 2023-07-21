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


/* Mask clipping for patterns */
#include "memory_.h"
#include "gx.h"
#include "gpcheck.h"
#include "gserrors.h"
#include "gsstruct.h"
#include "gxdevice.h"
#include "gxdevmem.h"
#include "gxclip2.h"
#include "gxdcolor.h"
#include "gdevmpla.h"

private_st_device_tile_clip();

/* Device procedures */
static dev_proc_fill_rectangle(tile_clip_fill_rectangle);
static dev_proc_fill_rectangle_hl_color(tile_clip_fill_rectangle_hl_color);
static dev_proc_copy_mono(tile_clip_copy_mono);
static dev_proc_copy_color(tile_clip_copy_color);
static dev_proc_copy_planes(tile_clip_copy_planes);
static dev_proc_copy_alpha(tile_clip_copy_alpha);
static dev_proc_copy_alpha_hl_color(tile_clip_copy_alpha_hl_color);
static dev_proc_strip_copy_rop2(tile_clip_strip_copy_rop2);

/* The device descriptor. */
static void
tile_clipper_initialize_device_procs(gx_device *dev)
{
    set_dev_proc(dev, get_initial_matrix, gx_forward_get_initial_matrix);
    set_dev_proc(dev, map_rgb_color, gx_forward_map_rgb_color);
    set_dev_proc(dev, map_color_rgb, gx_forward_map_color_rgb);
    set_dev_proc(dev, fill_rectangle, tile_clip_fill_rectangle);
    set_dev_proc(dev, copy_mono, tile_clip_copy_mono);
    set_dev_proc(dev, copy_color, tile_clip_copy_color);
    set_dev_proc(dev, get_params, gx_forward_get_params);
    set_dev_proc(dev, put_params, gx_forward_put_params);
    set_dev_proc(dev, map_cmyk_color, gx_forward_map_cmyk_color);
    set_dev_proc(dev, get_page_device, gx_forward_get_page_device);
    set_dev_proc(dev, get_alpha_bits, gx_forward_get_alpha_bits);
    set_dev_proc(dev, copy_alpha, tile_clip_copy_alpha);
    set_dev_proc(dev, get_clipping_box, gx_forward_get_clipping_box);
    set_dev_proc(dev, get_bits_rectangle, gx_forward_get_bits_rectangle);
    set_dev_proc(dev, composite, gx_no_composite);
    set_dev_proc(dev, get_hardware_params, gx_forward_get_hardware_params);
    set_dev_proc(dev, get_color_mapping_procs, gx_forward_get_color_mapping_procs);
    set_dev_proc(dev, get_color_comp_index, gx_forward_get_color_comp_index);
    set_dev_proc(dev, encode_color, gx_forward_encode_color);
    set_dev_proc(dev, decode_color, gx_forward_decode_color);
    set_dev_proc(dev, fill_rectangle_hl_color, tile_clip_fill_rectangle_hl_color);
    set_dev_proc(dev, include_color_space, gx_forward_include_color_space);
    set_dev_proc(dev, fill_linear_color_scanline, gx_forward_fill_linear_color_scanline);
    set_dev_proc(dev, fill_linear_color_trapezoid, gx_forward_fill_linear_color_trapezoid);
    set_dev_proc(dev, fill_linear_color_triangle, gx_forward_fill_linear_color_triangle);
    set_dev_proc(dev, update_spot_equivalent_colors, gx_forward_update_spot_equivalent_colors);
    set_dev_proc(dev, ret_devn_params, gx_forward_ret_devn_params);
    set_dev_proc(dev, fillpage, gx_forward_fillpage);
    set_dev_proc(dev, dev_spec_op, gx_forward_dev_spec_op);
    set_dev_proc(dev, copy_planes, tile_clip_copy_planes);
    set_dev_proc(dev, strip_copy_rop2, tile_clip_strip_copy_rop2);
    set_dev_proc(dev, copy_alpha_hl_color, tile_clip_copy_alpha_hl_color);

    /* Ideally the following defaults would be set up for us, but this
     * does not currently work. */
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
    set_dev_proc(dev, strip_tile_rectangle, gx_default_strip_tile_rectangle);
    set_dev_proc(dev, begin_typed_image, gx_default_begin_typed_image);
    set_dev_proc(dev, text_begin, gx_default_text_begin);
    set_dev_proc(dev, strip_tile_rect_devn, gx_default_strip_tile_rect_devn);
}

static const gx_device_tile_clip gs_tile_clip_device =
{std_device_std_body_open(gx_device_tile_clip,
                          tile_clipper_initialize_device_procs,
                          "tile clipper",
                          0, 0, 1, 1)
};

/* Initialize a tile clipping device from a mask. */
int
tile_clip_initialize(gx_device_tile_clip * cdev, const gx_strip_bitmap * tiles,
                     gx_device * tdev, int px, int py)
{
    int code =
    gx_mask_clip_initialize(cdev, &gs_tile_clip_device,
                            (const gx_bitmap *)tiles,
                            tdev, 0, 0, NULL);	/* phase will be reset */

    if (code >= 0) {
        cdev->tiles = *tiles;
        tile_clip_set_phase(cdev, px, py);
    }
    return code;
}

void
tile_clip_free(gx_device_tile_clip *cdev)
{
    /* release the target reference */
    if(cdev->finalize)
        cdev->finalize((gx_device *)cdev);  /* this also sets the target to NULL */
    gs_free_object(cdev->memory, cdev, "tile_clip_free(cdev)");
}

/* Set the phase of the tile. */
void
tile_clip_set_phase(gx_device_tile_clip * cdev, int px, int py)
{
    cdev->phase.x = px;
    cdev->phase.y = py;
}

/* Fill a rectangle with high level devn color by tiling with the mask. */
static int
tile_clip_fill_rectangle_hl_color(gx_device *dev, const gs_fixed_rect *rect,
                const gs_gstate *pgs, const gx_drawing_color *pdcolor,
                const gx_clip_path *pcpath)
{
    gx_device_tile_clip *cdev = (gx_device_tile_clip *) dev;
    gx_device *tdev = cdev->target;
    int x, y, w, h;
    gx_device_color dcolor0, dcolor1;
    int k;

    /* Have to pack the no color index into the pure device type */
    dcolor0.type = gx_dc_type_pure;
    dcolor0.colors.pure = gx_no_color_index;
    /* Have to set the dcolor1 to a non mask type */
    dcolor1.type = gx_dc_type_devn;
    for (k = 0; k < GS_CLIENT_COLOR_MAX_COMPONENTS; k++) {
        dcolor1.colors.devn.values[k] = pdcolor->colors.devn.values[k];
    }
    x = fixed2int(rect->p.x);
    y = fixed2int(rect->p.y);
    w = fixed2int(rect->q.x) - x;
    h = fixed2int(rect->q.y) - y;
    return (*dev_proc(tdev, strip_tile_rect_devn))(tdev, &cdev->tiles,
                                                    x, y, w, h, &dcolor0, &dcolor1,
                                                    cdev->phase.x, cdev->phase.y);
}

/* Fill a rectangle by tiling with the mask. */
static int
tile_clip_fill_rectangle(gx_device * dev, int x, int y, int w, int h,
                         gx_color_index color)
{
    gx_device_tile_clip *cdev = (gx_device_tile_clip *) dev;
    gx_device *tdev = cdev->target;

    return (*dev_proc(tdev, strip_tile_rectangle)) (tdev, &cdev->tiles,
                                                    x, y, w, h,
                    gx_no_color_index, color, cdev->phase.x, cdev->phase.y);
}

/* Calculate the X offset corresponding to a given Y, taking the phase */
/* and shift into account. */
#define x_offset(ty, cdev)\
  ((cdev)->phase.x + (((ty) + (cdev)->phase.y) / (cdev)->tiles.rep_height) *\
   (cdev)->tiles.rep_shift)

/* Copy a monochrome bitmap.  We divide it up into maximal chunks */
/* that line up with a single tile, and then do the obvious Boolean */
/* combination of the tile mask and the source. */
static int
tile_clip_copy_mono(gx_device * dev,
                const byte * data, int sourcex, int raster, gx_bitmap_id id,
                    int x, int y, int w, int h,
                    gx_color_index color0, gx_color_index color1)
{
    gx_device_tile_clip *cdev = (gx_device_tile_clip *) dev;
    gx_color_index color, mcolor0, mcolor1;
    int ty, ny;
    int code;

    setup_mask_copy_mono(cdev, color, mcolor0, mcolor1);
    for (ty = y; ty < y + h; ty += ny) {
        int tx, nx;
        int cy;
        int xoff;

        if (cdev->tiles.rep_height == 0 || cdev->tiles.rep_width == 0)
            return 0;

        cy = (ty + cdev->phase.y) % cdev->tiles.rep_height;
        xoff = x_offset(ty, cdev);

        ny = min(y + h - ty, cdev->tiles.size.y - cy);
        if (ny > cdev->mdev.height)
            ny = cdev->mdev.height;
        for (tx = x; tx < x + w; tx += nx) {
            int cx = (tx + xoff) % cdev->tiles.rep_width;

            nx = min(x + w - tx, cdev->tiles.size.x - cx);
            /* Copy a tile slice to the memory device buffer. */
            memcpy(cdev->buffer.bytes,
                   cdev->tiles.data + cy * cdev->tiles.raster,
                   (size_t)cdev->tiles.raster * ny);
            /* Intersect the tile with the source data. */
            /* mcolor0 and mcolor1 invert the data if needed. */
            /* This call can't fail. */
            (*dev_proc(&cdev->mdev, copy_mono)) ((gx_device *) & cdev->mdev,
                                 data + (ty - y) * raster, sourcex + tx - x,
                                                 raster, gx_no_bitmap_id,
                                           cx, 0, nx, ny, mcolor0, mcolor1);
            /* Now copy the color through the double mask. */
            code = (*dev_proc(cdev->target, copy_mono)) (cdev->target,
                                 cdev->buffer.bytes, cx, cdev->tiles.raster,
                                                         gx_no_bitmap_id,
                                  tx, ty, nx, ny, gx_no_color_index, color);
            if (code < 0)
                return code;
        }
    }
    return 0;
}

/*
 * Define the skeleton for the other copying operations.  We can't use the
 * BitBlt tricks: we have to scan for runs of 1s.  There are many obvious
 * ways to speed this up; we'll implement some if we need to.  The schema
 * is:
 *      FOR_RUNS(data_row, tx1, tx, ty) {
 *        ... process the run ([tx1,tx),ty) ...
 *      } END_FOR_RUNS();
 * Free variables: cdev, data, sourcex, raster, x, y, w, h.
 */
#define t_next(tx)\
  BEGIN {\
    if ( ++cx == cdev->tiles.size.x )\
      cx = 0, tp = tile_row, tbit = 0x80;\
    else if ( (tbit >>= 1) == 0 )\
      tp++, tbit = 0x80;\
    tx++;\
  } END
#define FOR_RUNS(data_row, tx1, tx, ty)\
        const byte *data_row = data;\
        int cy;\
        byte *tile_row;\
         int ty;\
\
        if (cdev->tiles.rep_height == 0 || cdev->tiles.rep_width == 0)\
            return 0;\
        cy = imod(y + cdev->phase.y, cdev->tiles.rep_height);\
        tile_row = cdev->tiles.data + cy * cdev->tiles.raster;\
\
        for ( ty = y; ty < y + h; ty++, data_row += raster ) {\
          int cx = imod(x + x_offset(ty, cdev), cdev->tiles.rep_width);\
          const byte *tp = tile_row + (cx >> 3);\
          byte tbit = 0x80 >> (cx & 7);\
          int tx;\
\
          for ( tx = x; tx < x + w; ) {\
            int tx1;\
\
            /* Skip a run of 0s. */\
            while ( tx < x + w && (*tp & tbit) == 0 )\
              t_next(tx);\
            if ( tx == x + w )\
              break;\
            /* Scan a run of 1s. */\
            tx1 = tx;\
            do {\
              t_next(tx);\
            } while ( tx < x + w && (*tp & tbit) != 0 );\
            if_debug3m('T', cdev->memory, "[T]run x=(%d,%d), y=%d\n", tx1, tx, ty);
/* (body goes here) */
#define END_FOR_RUNS()\
          }\
          if ( ++cy == cdev->tiles.size.y )\
            cy = 0, tile_row = cdev->tiles.data;\
          else\
            tile_row += cdev->tiles.raster;\
        }

/* Copy a color rectangle. */
static int
tile_clip_copy_color(gx_device * dev,
                const byte * data, int sourcex, int raster, gx_bitmap_id id,
                     int x, int y, int w, int h)
{
    gx_device_tile_clip *cdev = (gx_device_tile_clip *) dev;

    fit_copy(dev, data, sourcex, raster, id, x, y, w, h);
    {
        FOR_RUNS(data_row, txrun, tx, ty) {
            /* Copy the run. */
            int code = (*dev_proc(cdev->target, copy_color))
            (cdev->target, data_row, sourcex + txrun - x, raster,
             gx_no_bitmap_id, txrun, ty, tx - txrun, 1);

            if (code < 0)
                return code;
        }
        END_FOR_RUNS();
    }
    return 0;
}

/* Copy a color rectangle. */
static int
tile_clip_copy_planes(gx_device * dev,
                const byte * data, int sourcex, int raster, gx_bitmap_id id,
                     int x, int y, int w, int h, int plane_height)
{
    gx_device_tile_clip *cdev = (gx_device_tile_clip *) dev;

    fit_copy(dev, data, sourcex, raster, id, x, y, w, h);
    {
        FOR_RUNS(data_row, txrun, tx, ty) {
            /* Copy the run. */
            int code = (*dev_proc(cdev->target, copy_planes))
            (cdev->target, data_row, sourcex + txrun - x, raster,
             gx_no_bitmap_id, txrun, ty, tx - txrun, 1, plane_height);

            if (code < 0)
                return code;
        }
        END_FOR_RUNS();
    }
    return 0;
}


/* Copy an alpha rectangle similarly. */
static int
tile_clip_copy_alpha(gx_device * dev,
                const byte * data, int sourcex, int raster, gx_bitmap_id id,
                int x, int y, int w, int h, gx_color_index color, int depth)
{
    gx_device_tile_clip *cdev = (gx_device_tile_clip *) dev;

    fit_copy(dev, data, sourcex, raster, id, x, y, w, h);
    {
        FOR_RUNS(data_row, txrun, tx, ty) {
            /* Copy the run. */
            int code = (*dev_proc(cdev->target, copy_alpha))
            (cdev->target, data_row, sourcex + txrun - x, raster,
             gx_no_bitmap_id, txrun, ty, tx - txrun, 1, color, depth);

            if (code < 0)
                return code;
        }
        END_FOR_RUNS();
    }
    return 0;
}

static int
tile_clip_copy_alpha_hl_color(gx_device * dev,
                const byte * data, int sourcex, int raster, gx_bitmap_id id,
                int x, int y, int w, int h, const gx_drawing_color *pdcolor,
                int depth)
{
    gx_device_tile_clip *cdev = (gx_device_tile_clip *) dev;

    fit_copy(dev, data, sourcex, raster, id, x, y, w, h);
    {
        FOR_RUNS(data_row, txrun, tx, ty) {
            /* Copy the run. */
            int code = (*dev_proc(cdev->target, copy_alpha_hl_color))
            (cdev->target, data_row, sourcex + txrun - x, raster,
             gx_no_bitmap_id, txrun, ty, tx - txrun, 1, pdcolor, depth);

            if (code < 0)
                return code;
        }
        END_FOR_RUNS();
    }
    return 0;
}

static int
tile_clip_strip_copy_rop2(gx_device * dev,
               const byte * data, int sourcex, uint raster, gx_bitmap_id id,
                         const gx_color_index * scolors,
           const gx_strip_bitmap * textures, const gx_color_index * tcolors,
                         int x, int y, int w, int h,
                       int phase_x, int phase_y, gs_logical_operation_t lop,
                       uint planar_height)
{
    gx_device_tile_clip *cdev = (gx_device_tile_clip *) dev;

    fit_copy(dev, data, sourcex, raster, id, x, y, w, h);
    {
        FOR_RUNS(data_row, txrun, tx, ty) {
            /* Copy the run. */
            int code = (*dev_proc(cdev->target, strip_copy_rop2))
            (cdev->target, data_row, sourcex + txrun - x, raster,
             gx_no_bitmap_id, scolors, textures, tcolors,
             txrun, ty, tx - txrun, 1, phase_x, phase_y, lop,
             planar_height);

            if (code < 0)
                return code;
        }
        END_FOR_RUNS();
    }
    return 0;
}
