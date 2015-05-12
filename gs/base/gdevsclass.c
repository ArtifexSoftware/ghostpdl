/* Copyright (C) 2001-2012 Artifex Software, Inc.
   All Rights Reserved.

   This software is provided AS-IS with no warranty, either express or
   implied.

   This software is distributed under license and may not be copied,
   modified or distributed except as expressly authorized under the terms
   of the license contained in the file LICENSE in this distribution.

   Refer to licensing information at http://www.artifex.com or contact
   Artifex Software, Inc.,  7 Mt. Lassen Drive - Suite A-134, San Rafael,
   CA  94903, U.S.A., +1(415)492-9861, for further information.
*/

/* Common code for subclassing devices */
#include "math_.h"
#include "memory_.h"
#include "gx.h"
#include "gserrors.h"
#include "gsparam.h"
#include "gxdevice.h"
#include "gsdevice.h"		/* requires gsmatrix.h */
#include "gxdcolor.h"		/* for gx_device_black/white */
#include "gxiparam.h"		/* for image source size */
#include "gxistate.h"
#include "gxpaint.h"
#include "gxpath.h"
#include "gxcpath.h"
#include "gxcmap.h"         /* color mapping procs */
#include "gsstype.h"
#include "gdevprn.h"
#include "gdevp14.h"        /* Needed to patch up the procs after compositor creation */
#include "gdevsclass.h"

/* For printing devices the 'open' routine in gdevprn calls gdevprn_allocate_memory
 * which is responsible for creating the page buffer. This *also* fills in some of
 * the device procs, in particular fill_rectangle() so its vitally important that
 * we pass this on.
 */
int default_subclass_open_device(gx_device *dev)
{
    if (dev->child && dev->child->procs.open_device) {
        dev->child->procs.open_device(dev->child);
        dev->child->is_open = true;
        gx_update_from_subclass(dev);
    }

    return 0;
}

void default_subclass_get_initial_matrix(gx_device *dev, gs_matrix *pmat)
{
    if (dev->child && dev->child->procs.get_initial_matrix)
        dev->child->procs.get_initial_matrix(dev->child, pmat);
    else
        gx_default_get_initial_matrix(dev, pmat);
    return;
}

int default_subclass_sync_output(gx_device *dev)
{
    if (dev->child && dev->child->procs.sync_output)
        return dev->child->procs.sync_output(dev->child);
    else
        gx_default_sync_output(dev);

    return 0;
}

int default_subclass_output_page(gx_device *dev, int num_copies, int flush)
{
    if (dev->child && dev->child->procs.output_page)
        return dev->child->procs.output_page(dev->child, num_copies, flush);
    return 0;
}

int default_subclass_close_device(gx_device *dev)
{
    int code;

    if (dev->child && dev->child && dev->child->procs.close_device) {
        code = dev->child->procs.close_device(dev->child);
        dev->is_open = dev->child->is_open = false;
        return code;
    }
    dev->is_open = false;
    return 0;
}

gx_color_index default_subclass_map_rgb_color(gx_device *dev, const gx_color_value cv[])
{
    if (dev->child && dev->child->procs.map_rgb_color)
        return dev->child->procs.map_rgb_color(dev->child, cv);
    else
        gx_error_encode_color(dev, cv);
    return 0;
}

int default_subclass_map_color_rgb(gx_device *dev, gx_color_index color, gx_color_value rgb[3])
{
    if (dev->child && dev->child->procs.map_color_rgb)
        return dev->child->procs.map_color_rgb(dev->child, color, rgb);
    else
        gx_default_map_color_rgb(dev, color, rgb);

    return 0;
}

int default_subclass_fill_rectangle(gx_device *dev, int x, int y, int width, int height, gx_color_index color)
{
    if (dev->child && dev->child->procs.fill_rectangle)
        return dev->child->procs.fill_rectangle(dev->child, x, y, width, height, color);
    return 0;
}

int default_subclass_tile_rectangle(gx_device *dev, const gx_tile_bitmap *tile, int x, int y, int width, int height,
    gx_color_index color0, gx_color_index color1,
    int phase_x, int phase_y)
{
    if (dev->child && dev->child->procs.tile_rectangle)
        return dev->child->procs.tile_rectangle(dev->child, tile, x, y, width, height, color0, color1, phase_x, phase_y);
    return 0;
}

int default_subclass_copy_mono(gx_device *dev, const byte *data, int data_x, int raster, gx_bitmap_id id,
    int x, int y, int width, int height,
    gx_color_index color0, gx_color_index color1)
{
    if (dev->child && dev->child->procs.copy_mono)
        return dev->child->procs.copy_mono(dev->child, data, data_x, raster, id, x, y, width, height, color0, color1);
    return 0;
}

int default_subclass_copy_color(gx_device *dev, const byte *data, int data_x, int raster, gx_bitmap_id id,\
    int x, int y, int width, int height)
{
    if (dev->child && dev->child->procs.copy_color)
        return dev->child->procs.copy_color(dev->child, data, data_x, raster, id, x, y, width, height);
    return 0;
}

int default_subclass_draw_line(gx_device *dev, int x0, int y0, int x1, int y1, gx_color_index color)
{
    if (dev->child && dev->child->procs.obsolete_draw_line)
        return dev->child->procs.obsolete_draw_line(dev->child, x0, y0, x1, y1, color);
    return 0;
}

int default_subclass_get_bits(gx_device *dev, int y, byte *data, byte **actual_data)
{
    if (dev->child && dev->child->procs.get_bits)
        return dev->child->procs.get_bits(dev->child, y, data, actual_data);
    else
        return gx_default_get_bits(dev, y, data, actual_data);
    return 0;
}

int default_subclass_get_params(gx_device *dev, gs_param_list *plist)
{
    if (dev->child && dev->child->procs.get_params)
        return dev->child->procs.get_params(dev->child, plist);
    else
        return gx_default_get_params(dev, plist);

    return 0;
}

int default_subclass_put_params(gx_device *dev, gs_param_list *plist)
{
    int code;

    if (dev->child && dev->child->procs.put_params) {
        code = dev->child->procs.put_params(dev->child, plist);
        /* The child device might have closed itself (yes seriously, this can happen!) */
        dev->is_open = dev->child->is_open;
        gx_update_from_subclass(dev);
        return code;
    }
    else
        return gx_default_put_params(dev, plist);

    return 0;
}

gx_color_index default_subclass_map_cmyk_color(gx_device *dev, const gx_color_value cv[])
{
    if (dev->child && dev->child->procs.map_cmyk_color)
        return dev->child->procs.map_cmyk_color(dev->child, cv);
    else
        return gx_default_map_cmyk_color(dev, cv);

    return 0;
}

const gx_xfont_procs *default_subclass_get_xfont_procs(gx_device *dev)
{
    if (dev->child && dev->child->procs.get_xfont_procs)
        return dev->child->procs.get_xfont_procs(dev->child);
    else
        return gx_default_get_xfont_procs(dev);

    return 0;
}

gx_device *default_subclass_get_xfont_device(gx_device *dev)
{
    if (dev->child && dev->child->procs.get_xfont_device)
        return dev->child->procs.get_xfont_device(dev->child);
    else
        return gx_default_get_xfont_device(dev);

    return 0;
}

gx_color_index default_subclass_map_rgb_alpha_color(gx_device *dev, gx_color_value red, gx_color_value green, gx_color_value blue,
    gx_color_value alpha)
{
    if (dev->child) {
        if (dev->child->procs.map_rgb_alpha_color)
            return dev->child->procs.map_rgb_alpha_color(dev->child, red, green, blue, alpha);
        else
            return gx_default_map_rgb_alpha_color(dev->child, red, green, blue, alpha);
    } else
        return gx_default_map_rgb_alpha_color(dev, red, green, blue, alpha);

    return 0;
}

gx_device *default_subclass_get_page_device(gx_device *dev)
{
    if (dev->child && dev->child->procs.get_page_device)
        return dev->child->procs.get_page_device(dev->child);
    else
        return gx_default_get_page_device(dev);

    return 0;
}

int default_subclass_get_alpha_bits(gx_device *dev, graphics_object_type type)
{
    if (dev->child && dev->child->procs.get_alpha_bits)
        return dev->child->procs.get_alpha_bits(dev->child, type);
    return 0;
}

int default_subclass_copy_alpha(gx_device *dev, const byte *data, int data_x,
    int raster, gx_bitmap_id id, int x, int y, int width, int height,
    gx_color_index color, int depth)
{
    if (dev->child && dev->child->procs.copy_alpha)
        return dev->child->procs.copy_alpha(dev->child, data, data_x, raster, id, x, y, width, height, color, depth);
    return 0;
}

int default_subclass_get_band(gx_device *dev, int y, int *band_start)
{
    if (dev->child && dev->child->procs.get_band)
        return dev->child->procs.get_band(dev->child, y, band_start);
    else
        return gx_default_get_band(dev, y, band_start);
    return 0;
}

int default_subclass_copy_rop(gx_device *dev, const byte *sdata, int sourcex, uint sraster, gx_bitmap_id id,
    const gx_color_index *scolors,
    const gx_tile_bitmap *texture, const gx_color_index *tcolors,
    int x, int y, int width, int height,
    int phase_x, int phase_y, gs_logical_operation_t lop)
{
    if (dev->child) {
        if (dev->child->procs.copy_rop)
            return dev->child->procs.copy_rop(dev->child, sdata, sourcex, sraster, id, scolors, texture, tcolors, x, y, width, height, phase_x, phase_y, lop);
        else
            return gx_default_copy_rop(dev->child, sdata, sourcex, sraster, id, scolors, texture, tcolors, x, y, width, height, phase_x, phase_y, lop);
    } else
        return gx_default_copy_rop(dev, sdata, sourcex, sraster, id, scolors, texture, tcolors, x, y, width, height, phase_x, phase_y, lop);
    return 0;
}

int default_subclass_fill_path(gx_device *dev, const gs_imager_state *pis, gx_path *ppath,
    const gx_fill_params *params,
    const gx_drawing_color *pdcolor, const gx_clip_path *pcpath)
{
    if (dev->child) {
        if (dev->child->procs.fill_path)
            return dev->child->procs.fill_path(dev->child, pis, ppath, params, pdcolor, pcpath);
        else
            return gx_default_fill_path(dev->child, pis, ppath, params, pdcolor, pcpath);
    } else
        return gx_default_fill_path(dev, pis, ppath, params, pdcolor, pcpath);

    return 0;
}

int default_subclass_stroke_path(gx_device *dev, const gs_imager_state *pis, gx_path *ppath,
    const gx_stroke_params *params,
    const gx_drawing_color *pdcolor, const gx_clip_path *pcpath)
{
    if (dev->child) {
        if (dev->child->procs.stroke_path)
            return dev->child->procs.stroke_path(dev->child, pis, ppath, params, pdcolor, pcpath);
        else
            return gx_default_stroke_path(dev->child, pis, ppath, params, pdcolor, pcpath);
    } else
        return gx_default_stroke_path(dev, pis, ppath, params, pdcolor, pcpath);
    return 0;
}

int default_subclass_fill_mask(gx_device *dev, const byte *data, int data_x, int raster, gx_bitmap_id id,
    int x, int y, int width, int height,
    const gx_drawing_color *pdcolor, int depth,
    gs_logical_operation_t lop, const gx_clip_path *pcpath)
{
    if (dev->child) {
        if (dev->child->procs.fill_mask)
            return dev->child->procs.fill_mask(dev->child, data, data_x, raster, id, x, y, width, height, pdcolor, depth, lop, pcpath);
        else
            return gx_default_fill_mask(dev->child, data, data_x, raster, id, x, y, width, height, pdcolor, depth, lop, pcpath);
    } else
        return gx_default_fill_mask(dev, data, data_x, raster, id, x, y, width, height, pdcolor, depth, lop, pcpath);
    return 0;
}

int default_subclass_fill_trapezoid(gx_device *dev, const gs_fixed_edge *left, const gs_fixed_edge *right,
    fixed ybot, fixed ytop, bool swap_axes,
    const gx_drawing_color *pdcolor, gs_logical_operation_t lop)
{
    if (dev->child) {
        if (dev->child->procs.fill_trapezoid)
            return dev->child->procs.fill_trapezoid(dev->child, left, right, ybot, ytop, swap_axes, pdcolor, lop);
        else
            return gx_default_fill_trapezoid(dev->child, left, right, ybot, ytop, swap_axes, pdcolor, lop);
    } else
        return gx_default_fill_trapezoid(dev, left, right, ybot, ytop, swap_axes, pdcolor, lop);
    return 0;
}

int default_subclass_fill_parallelogram(gx_device *dev, fixed px, fixed py, fixed ax, fixed ay, fixed bx, fixed by,
    const gx_drawing_color *pdcolor, gs_logical_operation_t lop)
{
    if (dev->child) {
        if (dev->child->procs.fill_parallelogram)
            return dev->child->procs.fill_parallelogram(dev->child, px, py, ax, ay, bx, by, pdcolor, lop);
        else
            return gx_default_fill_parallelogram(dev->child, px, py, ax, ay, bx, by, pdcolor, lop);
    } else
        return gx_default_fill_parallelogram(dev, px, py, ax, ay, bx, by, pdcolor, lop);
    return 0;
}

int default_subclass_fill_triangle(gx_device *dev, fixed px, fixed py, fixed ax, fixed ay, fixed bx, fixed by,
    const gx_drawing_color *pdcolor, gs_logical_operation_t lop)
{
    if (dev->child) {
        if (dev->child->procs.fill_triangle)
            return dev->child->procs.fill_triangle(dev->child, px, py, ax, ay, bx, by, pdcolor, lop);
        else
            return gx_default_fill_triangle(dev->child, px, py, ax, ay, bx, by, pdcolor, lop);
    } else
        return gx_default_fill_triangle(dev, px, py, ax, ay, bx, by, pdcolor, lop);
    return 0;
}

int default_subclass_draw_thin_line(gx_device *dev, fixed fx0, fixed fy0, fixed fx1, fixed fy1,
    const gx_drawing_color *pdcolor, gs_logical_operation_t lop,
    fixed adjustx, fixed adjusty)
{
    if (dev->child) {
        if (dev->child->procs.draw_thin_line)
            return dev->child->procs.draw_thin_line(dev->child, fx0, fy0, fx1, fy1, pdcolor, lop, adjustx, adjusty);
        else
            return gx_default_draw_thin_line(dev->child, fx0, fy0, fx1, fy1, pdcolor, lop, adjustx, adjusty);
    } else
        return gx_default_draw_thin_line(dev, fx0, fy0, fx1, fy1, pdcolor, lop, adjustx, adjusty);
    return 0;
}

int default_subclass_begin_image(gx_device *dev, const gs_imager_state *pis, const gs_image_t *pim,
    gs_image_format_t format, const gs_int_rect *prect,
    const gx_drawing_color *pdcolor, const gx_clip_path *pcpath,
    gs_memory_t *memory, gx_image_enum_common_t **pinfo)
{
    if (dev->child) {
        if (dev->child->procs.begin_image)
            return dev->child->procs.begin_image(dev->child, pis, pim, format, prect, pdcolor, pcpath, memory, pinfo);
        else
            return gx_default_begin_image(dev->child, pis, pim, format, prect, pdcolor, pcpath, memory, pinfo);
    } else
        return gx_default_begin_image(dev, pis, pim, format, prect, pdcolor, pcpath, memory, pinfo);

    return 0;
}

int default_subclass_image_data(gx_device *dev, gx_image_enum_common_t *info, const byte **planes, int data_x,
    uint raster, int height)
{
    if (dev->child && dev->child->procs.image_data)
        return dev->child->procs.image_data(dev->child, info, planes, data_x, raster, height);
    return 0;
}

int default_subclass_end_image(gx_device *dev, gx_image_enum_common_t *info, bool draw_last)
{
    if (dev->child && dev->child->procs.end_image)
        return dev->child->procs.end_image(dev->child, info, draw_last);
    return 0;
}

int default_subclass_strip_tile_rectangle(gx_device *dev, const gx_strip_bitmap *tiles, int x, int y, int width, int height,
    gx_color_index color0, gx_color_index color1,
    int phase_x, int phase_y)
{
    if (dev->child) {
        if (dev->child->procs.strip_tile_rectangle)
            return dev->child->procs.strip_tile_rectangle(dev->child, tiles, x, y, width, height, color0, color1, phase_x, phase_y);
        else
            return gx_default_strip_tile_rectangle(dev->child, tiles, x, y, width, height, color0, color1, phase_x, phase_y);
    } else
        return gx_default_strip_tile_rectangle(dev, tiles, x, y, width, height, color0, color1, phase_x, phase_y);
    return 0;
}

int default_subclass_strip_copy_rop(gx_device *dev, const byte *sdata, int sourcex, uint sraster, gx_bitmap_id id,
    const gx_color_index *scolors,
    const gx_strip_bitmap *textures, const gx_color_index *tcolors,
    int x, int y, int width, int height,
    int phase_x, int phase_y, gs_logical_operation_t lop)
{
    if (dev->child) {
        if (dev->child->procs.strip_copy_rop)
            return dev->child->procs.strip_copy_rop(dev->child, sdata, sourcex, sraster, id, scolors, textures, tcolors, x, y, width, height, phase_x, phase_y, lop);
        else
            return gx_default_strip_copy_rop(dev->child, sdata, sourcex, sraster, id, scolors, textures, tcolors, x, y, width, height, phase_x, phase_y, lop);
    } else
        return gx_default_strip_copy_rop(dev, sdata, sourcex, sraster, id, scolors, textures, tcolors, x, y, width, height, phase_x, phase_y, lop);
    return 0;
}

void default_subclass_get_clipping_box(gx_device *dev, gs_fixed_rect *pbox)
{
    if (dev->child) {
        if (dev->child->procs.get_clipping_box)
            dev->child->procs.get_clipping_box(dev->child, pbox);
        else
            gx_default_get_clipping_box(dev->child, pbox);
    } else
        gx_default_get_clipping_box(dev, pbox);

    return;
}

int default_subclass_begin_typed_image(gx_device *dev, const gs_imager_state *pis, const gs_matrix *pmat,
    const gs_image_common_t *pic, const gs_int_rect *prect,
    const gx_drawing_color *pdcolor, const gx_clip_path *pcpath,
    gs_memory_t *memory, gx_image_enum_common_t **pinfo)
{
    if (dev->child) {
        if (dev->child->procs.begin_typed_image)
            return dev->child->procs.begin_typed_image(dev->child, pis, pmat, pic, prect, pdcolor, pcpath, memory, pinfo);
        else
            return gx_default_begin_typed_image(dev->child, pis, pmat, pic, prect, pdcolor, pcpath, memory, pinfo);
    } else
        return gx_default_begin_typed_image(dev, pis, pmat, pic, prect, pdcolor, pcpath, memory, pinfo);
    return 0;
}

int default_subclass_get_bits_rectangle(gx_device *dev, const gs_int_rect *prect,
    gs_get_bits_params_t *params, gs_int_rect **unread)
{
    if (dev->child) {
        if (dev->child->procs.get_bits_rectangle)
            return dev->child->procs.get_bits_rectangle(dev->child, prect, params, unread);
        else
            return gx_default_get_bits_rectangle(dev->child, prect, params, unread);
    } else
        return gx_default_get_bits_rectangle(dev, prect, params, unread);

    return 0;
}

int default_subclass_map_color_rgb_alpha(gx_device *dev, gx_color_index color, gx_color_value rgba[4])
{
    if (dev->child) {
        if (dev->child->procs.map_color_rgb_alpha)
            return dev->child->procs.map_color_rgb_alpha(dev->child, color, rgba);
        else
            return gx_default_map_color_rgb_alpha(dev->child, color, rgba);
    } else
        return gx_default_map_color_rgb_alpha(dev, color, rgba);

    return 0;
}

int default_subclass_create_compositor(gx_device *dev, gx_device **pcdev, const gs_composite_t *pcte,
    gs_imager_state *pis, gs_memory_t *memory, gx_device *cdev)
{
    default_subclass_subclass_data *psubclass_data = dev->subclass_data;
    int code;

    if (dev->child && dev->child->procs.create_compositor) {
        /* Some more unpleasantness here. If the child device is a clist, then it will use the first argument
         * that we pass to access its own data (not unreasonably), so we need to make sure we pass in the
         * child device. This has some follow on implications detailed below.
         */
        code = dev->child->procs.create_compositor(dev->child, pcdev, pcte, pis, memory, cdev);
        if (code < 0)
            return code;

        if (*pcdev != dev->child){
            /* If the child created a new compositor, which it wants to be the new 'device' in the
             * graphics state, it sets it in the returned pcdev variable. When we return from this
             * method, if pcdev is not the same as the device in the graphics state then the interpreter
             * sets pcdev as the new device in the graphics state. But because we passed in the child device
             * to the child method, if it did create a compositor it will be a forwarding device, and it will
             * be forwarding to our child, we need it to point to us instead. So if pcdev is not the same as the
             * child device, we fixup the target in the child device to point to us.
             */
            gx_device_forward *fdev = (gx_device_forward *)*pcdev;

            if (fdev->target == dev->child) {
                if (gs_is_pdf14trans_compositor(pcte) != 0 && strncmp(fdev->dname, "pdf14clist", 10) == 0) {
                    pdf14_clist_device *p14dev;

                    p14dev = (pdf14_clist_device *)*pcdev;

                    dev->color_info = dev->child->color_info;

                    psubclass_data->saved_compositor_method = p14dev->procs.create_compositor;
                    p14dev->procs.create_compositor = gx_subclass_create_compositor;
                }

                fdev->target = dev;
                rc_decrement_only(dev->child, "first-last page compositor code");
                rc_increment(dev);
            }
            return gs_error_handled;
        }
        else {
            /* See the 2 comments above. Now, if the child did not create a new compositor (eg its a clist)
             * then it returns pcdev pointing to the passed in device (the child in our case). Now this is a
             * problem, if we return with pcdev == child->dev, and teh current device is 'dev' then the
             * compositor code will think we wanted to push a new device and will select the child device.
             * so here if pcdev == dev->child we change it to be our own device, so that the calling code
             * won't redirect the device in the graphics state.
             */
            *pcdev = dev;
            return code;
        }
    }
    return 0;
}

int default_subclass_get_hardware_params(gx_device *dev, gs_param_list *plist)
{
    if (dev->child) {
        if (dev->child->procs.get_hardware_params)
            return dev->child->procs.get_hardware_params(dev->child, plist);
        else
            return gx_default_get_hardware_params(dev->child, plist);
    } else
        return gx_default_get_hardware_params(dev, plist);

    return 0;
}

int default_subclass_text_begin(gx_device *dev, gs_imager_state *pis, const gs_text_params_t *text,
    gs_font *font, gx_path *path, const gx_device_color *pdcolor, const gx_clip_path *pcpath,
    gs_memory_t *memory, gs_text_enum_t **ppte)
{
    if (dev->child) {
        if (dev->child->procs.text_begin)
            return dev->child->procs.text_begin(dev->child, pis, text, font, path, pdcolor, pcpath, memory, ppte);
        else
            return gx_default_text_begin(dev->child, pis, text, font, path, pdcolor, pcpath, memory, ppte);
    } else
        return gx_default_text_begin(dev, pis, text, font, path, pdcolor, pcpath, memory, ppte);

    return 0;
}

/* This method seems (despite the name) to be intended to allow for
 * devices to initialise data before being invoked. For our subclassed
 * device this should already have been done.
 */
int default_subclass_finish_copydevice(gx_device *dev, const gx_device *from_dev)
{
    return 0;
}

int default_subclass_begin_transparency_group(gx_device *dev, const gs_transparency_group_params_t *ptgp,
    const gs_rect *pbbox, gs_imager_state *pis, gs_memory_t *mem)
{
    if (dev->child && dev->child->procs.begin_transparency_group)
        return dev->child->procs.begin_transparency_group(dev->child, ptgp, pbbox, pis, mem);

    return 0;
}

int default_subclass_end_transparency_group(gx_device *dev, gs_imager_state *pis)
{
    if (dev->child && dev->child->procs.end_transparency_group)
        return dev->child->procs.end_transparency_group(dev->child, pis);

    return 0;
}

int default_subclass_begin_transparency_mask(gx_device *dev, const gx_transparency_mask_params_t *ptmp,
    const gs_rect *pbbox, gs_imager_state *pis, gs_memory_t *mem)
{
    if (dev->child && dev->child->procs.begin_transparency_mask)
        return dev->child->procs.begin_transparency_mask(dev->child, ptmp, pbbox, pis, mem);

    return 0;
}

int default_subclass_end_transparency_mask(gx_device *dev, gs_imager_state *pis)
{
    if (dev->child && dev->child->procs.end_transparency_mask)
        return dev->child->procs.end_transparency_mask(dev->child, pis);
    return 0;
}

int default_subclass_discard_transparency_layer(gx_device *dev, gs_imager_state *pis)
{
    if (dev->child && dev->child->procs.discard_transparency_layer)
        return dev->child->procs.discard_transparency_layer(dev->child, pis);

    return 0;
}

const gx_cm_color_map_procs *default_subclass_get_color_mapping_procs(const gx_device *dev)
{
    if (dev->child) {
        if (dev->child->procs.get_color_mapping_procs)
            return dev->child->procs.get_color_mapping_procs(dev->child);
        else
            return gx_default_DevGray_get_color_mapping_procs(dev->child);
    } else
        return gx_default_DevGray_get_color_mapping_procs(dev);

    return 0;
}

int  default_subclass_get_color_comp_index(gx_device *dev, const char * pname, int name_size, int component_type)
{
    if (dev->child) {
        if (dev->child->procs.get_color_comp_index)
            return dev->child->procs.get_color_comp_index(dev->child, pname, name_size, component_type);
        else
            return gx_error_get_color_comp_index(dev->child, pname, name_size, component_type);
    } else
        return gx_error_get_color_comp_index(dev, pname, name_size, component_type);

    return 0;
}

gx_color_index default_subclass_encode_color(gx_device *dev, const gx_color_value colors[])
{
    if (dev->child) {
        if (dev->child->procs.encode_color)
            return dev->child->procs.encode_color(dev->child, colors);
        else
            return gx_error_encode_color(dev->child, colors);
    } else
        return gx_error_encode_color(dev, colors);

    return 0;
}

int default_subclass_decode_color(gx_device *dev, gx_color_index cindex, gx_color_value colors[])
{
    if (dev->child && dev->child->procs.decode_color)
        return dev->child->procs.decode_color(dev->child, cindex, colors);
    else {
        memset(colors, 0, sizeof(gx_color_value[GX_DEVICE_COLOR_MAX_COMPONENTS]));
    }

    return 0;
}

int default_subclass_pattern_manage(gx_device *dev, gx_bitmap_id id,
                gs_pattern1_instance_t *pinst, pattern_manage_t function)
{
    if (dev->child && dev->child->procs.pattern_manage)
        return dev->child->procs.pattern_manage(dev->child, id, pinst, function);

    return 0;
}

int default_subclass_fill_rectangle_hl_color(gx_device *dev, const gs_fixed_rect *rect,
        const gs_imager_state *pis, const gx_drawing_color *pdcolor, const gx_clip_path *pcpath)
{
    if (dev->child && dev->child->procs.fill_rectangle_hl_color)
        return dev->child->procs.fill_rectangle_hl_color(dev->child, rect, pis, pdcolor, pcpath);
    else
        return_error(gs_error_rangecheck);

    return 0;
}

int default_subclass_include_color_space(gx_device *dev, gs_color_space *cspace, const byte *res_name, int name_length)
{
    if (dev->child && dev->child->procs.include_color_space)
        return dev->child->procs.include_color_space(dev->child, cspace, res_name, name_length);

    return 0;
}

int default_subclass_fill_linear_color_scanline(gx_device *dev, const gs_fill_attributes *fa,
        int i, int j, int w, const frac31 *c0, const int32_t *c0_f, const int32_t *cg_num,
        int32_t cg_den)
{
    if (dev->child) {
        if (dev->child->procs.fill_linear_color_scanline)
            return dev->child->procs.fill_linear_color_scanline(dev->child, fa, i, j, w, c0, c0_f, cg_num, cg_den);
        else
            return gx_default_fill_linear_color_scanline(dev->child, fa, i, j, w, c0, c0_f, cg_num, cg_den);
    } else
        return gx_default_fill_linear_color_scanline(dev, fa, i, j, w, c0, c0_f, cg_num, cg_den);

    return 0;
}

int default_subclass_fill_linear_color_trapezoid(gx_device *dev, const gs_fill_attributes *fa,
        const gs_fixed_point *p0, const gs_fixed_point *p1,
        const gs_fixed_point *p2, const gs_fixed_point *p3,
        const frac31 *c0, const frac31 *c1,
        const frac31 *c2, const frac31 *c3)
{
    if (dev->child) {
        if (dev->child->procs.fill_linear_color_trapezoid)
            return dev->child->procs.fill_linear_color_trapezoid(dev->child, fa, p0, p1, p2, p3, c0, c1, c2, c3);
        else
            return gx_default_fill_linear_color_trapezoid(dev->child, fa, p0, p1, p2, p3, c0, c1, c2, c3);
    } else
        return gx_default_fill_linear_color_trapezoid(dev, fa, p0, p1, p2, p3, c0, c1, c2, c3);

    return 0;
}

int default_subclass_fill_linear_color_triangle(gx_device *dev, const gs_fill_attributes *fa,
        const gs_fixed_point *p0, const gs_fixed_point *p1,
        const gs_fixed_point *p2, const frac31 *c0, const frac31 *c1, const frac31 *c2)
{
    if (dev->child) {
        if (dev->child->procs.fill_linear_color_triangle)
            return dev->child->procs.fill_linear_color_triangle(dev->child, fa, p0, p1, p2, c0, c1, c2);
        else
            return gx_default_fill_linear_color_triangle(dev->child, fa, p0, p1, p2, c0, c1, c2);
    } else
        return gx_default_fill_linear_color_triangle(dev, fa, p0, p1, p2, c0, c1, c2);

    return 0;
}

int default_subclass_update_spot_equivalent_colors(gx_device *dev, const gs_state * pgs)
{
    if (dev->child && dev->child->procs.update_spot_equivalent_colors)
        return dev->child->procs.update_spot_equivalent_colors(dev->child, pgs);

    return 0;
}

gs_devn_params *default_subclass_ret_devn_params(gx_device *dev)
{
    if (dev->child && dev->child->procs.ret_devn_params)
        return dev->child->procs.ret_devn_params(dev->child);

    return 0;
}

int default_subclass_fillpage(gx_device *dev, gs_imager_state * pis, gx_device_color *pdevc)
{
    if (dev->child) {
        if (dev->child->procs.fillpage)
            return dev->child->procs.fillpage(dev->child, pis, pdevc);
        else
            return gx_default_fillpage(dev->child, pis, pdevc);
    } else
        return gx_default_fillpage(dev, pis, pdevc);

    return 0;
}

int default_subclass_push_transparency_state(gx_device *dev, gs_imager_state *pis)
{
    if (dev->child && dev->child->procs.push_transparency_state)
        return dev->child->procs.push_transparency_state(dev->child, pis);

    return 0;
}

int default_subclass_pop_transparency_state(gx_device *dev, gs_imager_state *pis)
{
    if (dev->child && dev->child->procs.push_transparency_state)
        return dev->child->procs.pop_transparency_state(dev->child, pis);

    return 0;
}

int default_subclass_put_image(gx_device *dev, const byte *buffer, int num_chan, int x, int y,
            int width, int height, int row_stride, int plane_stride,
            int alpha_plane_index, int tag_plane_index)
{
    if (dev->child && dev->child->procs.put_image)
        return dev->child->procs.put_image(dev->child, buffer, num_chan, x, y, width, height, row_stride, plane_stride, alpha_plane_index, tag_plane_index);

    return 0;
}

int default_subclass_dev_spec_op(gx_device *dev, int op, void *data, int datasize)
{
    if (dev->child && dev->child->procs.dev_spec_op)
        return dev->child->procs.dev_spec_op(dev->child, op, data, datasize);

    return 0;
}

int default_subclass_copy_planes(gx_device *dev, const byte *data, int data_x, int raster, gx_bitmap_id id,
    int x, int y, int width, int height, int plane_height)
{
    if (dev->child && dev->child->procs.copy_planes)
        return dev->child->procs.copy_planes(dev->child, data, data_x, raster, id, x, y, width, height, plane_height);

    return 0;
}

int default_subclass_get_profile(gx_device *dev, cmm_dev_profile_t **dev_profile)
{
    if (dev->child) {
        if (dev->child->procs.get_profile)
           return dev->child->procs.get_profile(dev->child, dev_profile);
        else
            return gx_default_get_profile(dev->child, dev_profile);
    }
    else
        return gx_default_get_profile(dev, dev_profile);

    return 0;
}

void default_subclass_set_graphics_type_tag(gx_device *dev, gs_graphics_type_tag_t tag)
{
    if (dev->child && dev->child->procs.set_graphics_type_tag)
        dev->child->procs.set_graphics_type_tag(dev->child, tag);

    return;
}

int default_subclass_strip_copy_rop2(gx_device *dev, const byte *sdata, int sourcex, uint sraster, gx_bitmap_id id,
    const gx_color_index *scolors, const gx_strip_bitmap *textures, const gx_color_index *tcolors,
    int x, int y, int width, int height, int phase_x, int phase_y, gs_logical_operation_t lop, uint planar_height)
{
    if (dev->child && dev->child->procs.strip_copy_rop2)
        return dev->child->procs.strip_copy_rop2(dev->child, sdata, sourcex, sraster, id, scolors, textures, tcolors, x, y, width, height, phase_x, phase_y, lop, planar_height);
    else
        return gx_default_strip_copy_rop2(dev->child, sdata, sourcex, sraster, id, scolors, textures, tcolors, x, y, width, height, phase_x, phase_y, lop, planar_height);

    return 0;
}

int default_subclass_strip_tile_rect_devn(gx_device *dev, const gx_strip_bitmap *tiles, int x, int y, int width, int height,
    const gx_drawing_color *pdcolor0, const gx_drawing_color *pdcolor1, int phase_x, int phase_y)
{
    if (dev->child && dev->child->procs.strip_tile_rect_devn)
        return dev->child->procs.strip_tile_rect_devn(dev->child, tiles, x, y, width, height, pdcolor0, pdcolor1, phase_x, phase_y);
    else
        return gx_default_strip_tile_rect_devn(dev->child, tiles, x, y, width, height, pdcolor0, pdcolor1, phase_x, phase_y);

    return 0;
}

int default_subclass_copy_alpha_hl_color(gx_device *dev, const byte *data, int data_x,
    int raster, gx_bitmap_id id, int x, int y, int width, int height,
    const gx_drawing_color *pdcolor, int depth)
{
    if (dev->child && dev->child->procs.copy_alpha_hl_color)
        return dev->child->procs.copy_alpha_hl_color(dev->child, data, data_x, raster, id, x, y, width, height, pdcolor, depth);
    else
        return_error(gs_error_rangecheck);

    return 0;
}

int default_subclass_process_page(gx_device *dev, gx_process_page_options_t *options)
{
    if (dev->child && dev->child->procs.process_page)
        return dev->child->procs.process_page(dev->child, options);

    return 0;
}
