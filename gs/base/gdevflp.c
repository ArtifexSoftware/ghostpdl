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

/* Device for first/last page handling device */
/* This device is the first 'subclassing' device; the intention of subclassing
 * is to allow us to develop a 'chain' or 'pipeline' of devices, each of which
 * can process some aspect of the graphics methods before passing them on to the
 * next device in the chain.
 *
 * This device's purpose is to implement the 'FirstPage' and 'LastPage' parameters
 * in Ghostscript. Initially only implemented in the PDF interpreter this functionality
 * has been shifted internally so that it can be implemented in all the interpreters.
 * The approach is pretty simple, we modify gdevprn.c and gdevvec.c so that if -dFirstPage
 * or -dLastPage is defined the device in question is subclassed and this device inserted.
 * This device then 'black hole's any graphics operations until we reach 'FirstPage'. We then
 * allow graphics to pass to the device until we reach the end of 'LastPage' at which time we
 * discard operations again until we reach the end, and close the device.
 */

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
#include "gdevflp.h"

/* GC descriptor */
public_st_device_flp();
/* we need text and image enumerators, because of the crazy way that text and images work */
private_st_flp_text_enum();

/* Device procedures, we need quite a lot of them */
static dev_proc_output_page(flp_output_page);
static dev_proc_fill_rectangle(flp_fill_rectangle);
static dev_proc_tile_rectangle(flp_tile_rectangle);
static dev_proc_copy_mono(flp_copy_mono);
static dev_proc_copy_color(flp_copy_color);
static dev_proc_draw_line(flp_draw_line);
static dev_proc_get_bits(flp_get_bits);
//static dev_proc_get_params(flp_get_params);
static dev_proc_get_params(flp_put_params);
static dev_proc_get_alpha_bits(flp_get_alpha_bits);
static dev_proc_copy_alpha(flp_copy_alpha);
static dev_proc_get_band(flp_get_band);
static dev_proc_copy_rop(flp_copy_rop);
static dev_proc_fill_path(flp_fill_path);
static dev_proc_stroke_path(flp_stroke_path);
static dev_proc_fill_mask(flp_fill_mask);
static dev_proc_fill_trapezoid(flp_fill_trapezoid);
static dev_proc_fill_parallelogram(flp_fill_parallelogram);
static dev_proc_fill_triangle(flp_fill_triangle);
static dev_proc_draw_thin_line(flp_draw_thin_line);
static dev_proc_begin_image(flp_begin_image);
static dev_proc_image_data(flp_image_data);
static dev_proc_end_image(flp_end_image);
static dev_proc_strip_tile_rectangle(flp_strip_tile_rectangle);
static dev_proc_strip_copy_rop(flp_strip_copy_rop);
static dev_proc_begin_typed_image(flp_begin_typed_image);
static dev_proc_get_bits_rectangle(flp_get_bits_rectangle);
static dev_proc_create_compositor(flp_create_compositor);
static dev_proc_text_begin(flp_text_begin);
static dev_proc_begin_transparency_group(flp_begin_transparency_group);
static dev_proc_end_transparency_group(flp_end_transparency_group);
static dev_proc_begin_transparency_mask(flp_begin_transparency_mask);
static dev_proc_end_transparency_mask(flp_end_transparency_mask);
static dev_proc_discard_transparency_layer(flp_discard_transparency_layer);
static dev_proc_pattern_manage(flp_pattern_manage);
static dev_proc_fill_rectangle_hl_color(flp_fill_rectangle_hl_color);
static dev_proc_fill_linear_color_scanline(flp_fill_linear_color_scanline);
static dev_proc_fill_linear_color_trapezoid(flp_fill_linear_color_trapezoid);
static dev_proc_fill_linear_color_triangle(flp_fill_linear_color_triangle);
static dev_proc_fillpage(flp_fillpage);
static dev_proc_push_transparency_state(flp_push_transparency_state);
static dev_proc_pop_transparency_state(flp_pop_transparency_state);
static dev_proc_put_image(flp_put_image);
static dev_proc_copy_planes(flp_copy_planes);
static dev_proc_set_graphics_type_tag(flp_set_graphics_type_tag);
static dev_proc_strip_copy_rop2(flp_strip_copy_rop2);
static dev_proc_strip_tile_rect_devn(flp_strip_tile_rect_devn);
static dev_proc_copy_alpha_hl_color(flp_copy_alpha_hl_color);
static dev_proc_process_page(flp_process_page);

/* The device prototype */
#define MAX_COORD (max_int_in_fixed - 1000)
#define MAX_RESOLUTION 4000

#define public_st_flp_device()	/* in gsdevice.c */\
  gs_public_st_complex_only(st_flp_device, gx_device, "first_lastpage",\
    0, flp_enum_ptrs, flp_reloc_ptrs, gx_device_finalize)

static
ENUM_PTRS_WITH(flp_enum_ptrs, gx_device *dev);
return 0; /* default case */
case 0:ENUM_RETURN(gx_device_enum_ptr(dev->parent));
case 1:ENUM_RETURN(gx_device_enum_ptr(dev->child));
ENUM_PTRS_END
static RELOC_PTRS_WITH(flp_reloc_ptrs, gx_device *dev)
{
    dev->parent = gx_device_reloc_ptr(dev->parent, gcst);
    dev->child = gx_device_reloc_ptr(dev->child, gcst);
}RELOC_PTRS_END

public_st_flp_device();

const
gx_device_flp gs_flp_device =
{
    std_device_dci_type_body(gx_device_flp, 0, "first_lastpage", &st_flp_device,
                        MAX_COORD, MAX_COORD,
                        MAX_RESOLUTION, MAX_RESOLUTION,
                        1, 8, 255, 0, 256, 1),
    {default_subclass_open_device,
     default_subclass_get_initial_matrix,
     default_subclass_sync_output,			/* sync_output */
     flp_output_page,
     default_subclass_close_device,
     default_subclass_map_rgb_color,
     default_subclass_map_color_rgb,
     flp_fill_rectangle,
     flp_tile_rectangle,			/* tile_rectangle */
     flp_copy_mono,
     flp_copy_color,
     flp_draw_line,			/* draw_line */
     flp_get_bits,			/* get_bits */
     default_subclass_get_params,
     flp_put_params,
     default_subclass_map_cmyk_color,
     default_subclass_get_xfont_procs,			/* get_xfont_procs */
     default_subclass_get_xfont_device,			/* get_xfont_device */
     default_subclass_map_rgb_alpha_color,
     default_subclass_get_page_device,
     flp_get_alpha_bits,			/* get_alpha_bits */
     flp_copy_alpha,
     flp_get_band,			/* get_band */
     flp_copy_rop,			/* copy_rop */
     flp_fill_path,
     flp_stroke_path,
     flp_fill_mask,
     flp_fill_trapezoid,
     flp_fill_parallelogram,
     flp_fill_triangle,
     flp_draw_thin_line,
     flp_begin_image,
     flp_image_data,			/* image_data */
     flp_end_image,			/* end_image */
     flp_strip_tile_rectangle,
     flp_strip_copy_rop,
     default_subclass_get_clipping_box,			/* get_clipping_box */
     flp_begin_typed_image,
     flp_get_bits_rectangle,			/* get_bits_rectangle */
     default_subclass_map_color_rgb_alpha,
     flp_create_compositor,
     default_subclass_get_hardware_params,			/* get_hardware_params */
     flp_text_begin,
     default_subclass_finish_copydevice,			/* finish_copydevice */
     flp_begin_transparency_group,			/* begin_transparency_group */
     flp_end_transparency_group,			/* end_transparency_group */
     flp_begin_transparency_mask,			/* begin_transparency_mask */
     flp_end_transparency_mask,			/* end_transparency_mask */
     flp_discard_transparency_layer,			/* discard_transparency_layer */
     default_subclass_get_color_mapping_procs,			/* get_color_mapping_procs */
     default_subclass_get_color_comp_index,			/* get_color_comp_index */
     default_subclass_encode_color,			/* encode_color */
     default_subclass_decode_color,			/* decode_color */
     flp_pattern_manage,			/* pattern_manage */
     flp_fill_rectangle_hl_color,			/* fill_rectangle_hl_color */
     default_subclass_include_color_space,			/* include_color_space */
     flp_fill_linear_color_scanline,			/* fill_linear_color_scanline */
     flp_fill_linear_color_trapezoid,			/* fill_linear_color_trapezoid */
     flp_fill_linear_color_triangle,			/* fill_linear_color_triangle */
     default_subclass_update_spot_equivalent_colors,			/* update_spot_equivalent_colors */
     default_subclass_ret_devn_params,			/* ret_devn_params */
     flp_fillpage,		/* fillpage */
     flp_push_transparency_state,                      /* push_transparency_state */
     flp_pop_transparency_state,                      /* pop_transparency_state */
     flp_put_image,                      /* put_image */
     default_subclass_dev_spec_op,                      /* dev_spec_op */
     flp_copy_planes,                      /* copy_planes */
     default_subclass_get_profile,                      /* get_profile */
     flp_set_graphics_type_tag,                      /* set_graphics_type_tag */
     flp_strip_copy_rop2,
     flp_strip_tile_rect_devn,
     flp_copy_alpha_hl_color,
     flp_process_page
    }
};

#undef MAX_COORD
#undef MAX_RESOLUTION

int flp_output_page(gx_device *dev, int num_copies, int flush)
{
    first_last_subclass_data *psubclass_data = dev->subclass_data;

    psubclass_data->PageCount++;

    if (psubclass_data->PageCount >= dev->FirstPage || dev->DisablePageHandler) {
        if (!dev->LastPage || psubclass_data->PageCount <= dev->LastPage || dev->DisablePageHandler) {
            return default_subclass_output_page(dev, num_copies, flush);
        }
    }

    return 0;
}

int flp_fill_rectangle(gx_device *dev, int x, int y, int width, int height, gx_color_index color)
{
    first_last_subclass_data *psubclass_data = dev->subclass_data;

    if (dev->DisablePageHandler || psubclass_data->PageCount >= dev->FirstPage - 1) {
        if (dev->DisablePageHandler || !dev->LastPage || psubclass_data->PageCount <= dev->LastPage - 1)
            return default_subclass_fill_rectangle(dev, x, y, width, height, color);
    }

    return 0;
}

int flp_tile_rectangle(gx_device *dev, const gx_tile_bitmap *tile, int x, int y, int width, int height,
    gx_color_index color0, gx_color_index color1,
    int phase_x, int phase_y)
{
    first_last_subclass_data *psubclass_data = dev->subclass_data;

    if (dev->DisablePageHandler || psubclass_data->PageCount >= dev->FirstPage - 1) {
        if (dev->DisablePageHandler || !dev->LastPage || psubclass_data->PageCount <= dev->LastPage - 1)
            return default_subclass_tile_rectangle(dev, tile, x, y, width, height, color0, color1, phase_x, phase_y);
    }

    return 0;
}

int flp_copy_mono(gx_device *dev, const byte *data, int data_x, int raster, gx_bitmap_id id,
    int x, int y, int width, int height,
    gx_color_index color0, gx_color_index color1)
{
    first_last_subclass_data *psubclass_data = dev->subclass_data;

    if (dev->DisablePageHandler || psubclass_data->PageCount >= dev->FirstPage - 1) {
        if (dev->DisablePageHandler || !dev->LastPage || psubclass_data->PageCount <= dev->LastPage - 1)
            return default_subclass_copy_mono(dev, data, data_x, raster, id, x, y, width, height, color0, color1);
    }

    return 0;
}

int flp_copy_color(gx_device *dev, const byte *data, int data_x, int raster, gx_bitmap_id id,\
    int x, int y, int width, int height)
{
    first_last_subclass_data *psubclass_data = dev->subclass_data;

    if (dev->DisablePageHandler || psubclass_data->PageCount >= dev->FirstPage - 1) {
        if (dev->DisablePageHandler || !dev->LastPage || psubclass_data->PageCount <= dev->LastPage - 1)
            return default_subclass_copy_color(dev, data, data_x, raster, id, x, y, width, height);
    }

    return 0;
}

int flp_draw_line(gx_device *dev, int x0, int y0, int x1, int y1, gx_color_index color)
{
    first_last_subclass_data *psubclass_data = dev->subclass_data;

    if (dev->DisablePageHandler || psubclass_data->PageCount >= dev->FirstPage - 1) {
        if (dev->DisablePageHandler || !dev->LastPage || psubclass_data->PageCount <= dev->LastPage - 1)
            return default_subclass_draw_line(dev, x0, y0, x1, y1, color);
    }
    return 0;
}

int flp_get_bits(gx_device *dev, int y, byte *data, byte **actual_data)
{
    first_last_subclass_data *psubclass_data = dev->subclass_data;

    if (dev->DisablePageHandler || psubclass_data->PageCount >= dev->FirstPage - 1) {
        if (dev->DisablePageHandler || !dev->LastPage || psubclass_data->PageCount <= dev->LastPage - 1)
            return default_subclass_get_bits(dev, y, data, actual_data);
    }
    return gx_default_get_bits(dev, y, data, actual_data);
}

int
flp_put_params(gx_device * dev, gs_param_list * plist)
{
    bool temp_bool = false;
    int code;

    code = param_read_bool(plist, "DisablePageHandler", &temp_bool);
    if (code < 0)
        return code;
    if (code == 0)
        dev->DisablePageHandler = temp_bool;

    code = default_subclass_put_params(dev, plist);

    if (code < 0)
        return code;

    return code;
}

int flp_get_alpha_bits(gx_device *dev, graphics_object_type type)
{
    first_last_subclass_data *psubclass_data = dev->subclass_data;

    if (dev->DisablePageHandler || psubclass_data->PageCount >= dev->FirstPage - 1) {
        if (dev->DisablePageHandler || !dev->LastPage || psubclass_data->PageCount <= dev->LastPage - 1)
            return default_subclass_get_alpha_bits(dev, type);
    }
    return 0;
}

int flp_copy_alpha(gx_device *dev, const byte *data, int data_x,
    int raster, gx_bitmap_id id, int x, int y, int width, int height,
    gx_color_index color, int depth)
{
    first_last_subclass_data *psubclass_data = dev->subclass_data;

    if (dev->DisablePageHandler || psubclass_data->PageCount >= dev->FirstPage - 1) {
        if (dev->DisablePageHandler || !dev->LastPage || psubclass_data->PageCount <= dev->LastPage - 1)
            return default_subclass_copy_alpha(dev, data, data_x, raster, id, x, y, width, height, color, depth);
    }
    return 0;
}

int flp_get_band(gx_device *dev, int y, int *band_start)
{
    first_last_subclass_data *psubclass_data = dev->subclass_data;

    if (dev->DisablePageHandler || psubclass_data->PageCount >= dev->FirstPage - 1) {
        if (dev->DisablePageHandler || !dev->LastPage || psubclass_data->PageCount <= dev->LastPage - 1)
            return default_subclass_get_band(dev, y, band_start);
    }
    return gx_default_get_band(dev, y, band_start);
}

int flp_copy_rop(gx_device *dev, const byte *sdata, int sourcex, uint sraster, gx_bitmap_id id,
    const gx_color_index *scolors,
    const gx_tile_bitmap *texture, const gx_color_index *tcolors,
    int x, int y, int width, int height,
    int phase_x, int phase_y, gs_logical_operation_t lop)
{
    first_last_subclass_data *psubclass_data = dev->subclass_data;

    if (dev->DisablePageHandler || psubclass_data->PageCount >= dev->FirstPage - 1) {
        if (dev->DisablePageHandler || !dev->LastPage || psubclass_data->PageCount <= dev->LastPage - 1)
            return default_subclass_copy_rop(dev, sdata, sourcex, sraster, id, scolors, texture, tcolors, x, y, width, height, phase_x, phase_y, lop);
    }
    return 0;
}

int flp_fill_path(gx_device *dev, const gs_imager_state *pis, gx_path *ppath,
    const gx_fill_params *params,
    const gx_drawing_color *pdcolor, const gx_clip_path *pcpath)
{
    first_last_subclass_data *psubclass_data = dev->subclass_data;

    if (dev->DisablePageHandler || psubclass_data->PageCount >= dev->FirstPage - 1) {
        if (dev->DisablePageHandler || !dev->LastPage || psubclass_data->PageCount <= dev->LastPage - 1)
            return default_subclass_fill_path(dev, pis, ppath, params, pdcolor, pcpath);
    }
    return 0;
}

int flp_stroke_path(gx_device *dev, const gs_imager_state *pis, gx_path *ppath,
    const gx_stroke_params *params,
    const gx_drawing_color *pdcolor, const gx_clip_path *pcpath)
{
    first_last_subclass_data *psubclass_data = dev->subclass_data;

    if (dev->DisablePageHandler || psubclass_data->PageCount >= dev->FirstPage - 1) {
        if (dev->DisablePageHandler || !dev->LastPage || psubclass_data->PageCount <= dev->LastPage - 1)
            return default_subclass_stroke_path(dev, pis, ppath, params, pdcolor, pcpath);
    }
    return 0;
}

int flp_fill_mask(gx_device *dev, const byte *data, int data_x, int raster, gx_bitmap_id id,
    int x, int y, int width, int height,
    const gx_drawing_color *pdcolor, int depth,
    gs_logical_operation_t lop, const gx_clip_path *pcpath)
{
    first_last_subclass_data *psubclass_data = dev->subclass_data;

    if (dev->DisablePageHandler || psubclass_data->PageCount >= dev->FirstPage - 1) {
        if (dev->DisablePageHandler || !dev->LastPage || psubclass_data->PageCount <= dev->LastPage - 1)
            return default_subclass_fill_mask(dev, data, data_x, raster, id, x, y, width, height, pdcolor, depth, lop, pcpath);
    }
    return 0;
}

int flp_fill_trapezoid(gx_device *dev, const gs_fixed_edge *left, const gs_fixed_edge *right,
    fixed ybot, fixed ytop, bool swap_axes,
    const gx_drawing_color *pdcolor, gs_logical_operation_t lop)
{
    first_last_subclass_data *psubclass_data = dev->subclass_data;

    if (dev->DisablePageHandler || psubclass_data->PageCount >= dev->FirstPage - 1) {
        if (dev->DisablePageHandler || !dev->LastPage || psubclass_data->PageCount <= dev->LastPage - 1)
            return default_subclass_fill_trapezoid(dev, left, right, ybot, ytop, swap_axes, pdcolor, lop);
    }
    return 0;
}

int flp_fill_parallelogram(gx_device *dev, fixed px, fixed py, fixed ax, fixed ay, fixed bx, fixed by,
    const gx_drawing_color *pdcolor, gs_logical_operation_t lop)
{
    first_last_subclass_data *psubclass_data = dev->subclass_data;

    if (dev->DisablePageHandler || psubclass_data->PageCount >= dev->FirstPage - 1) {
        if (dev->DisablePageHandler || !dev->LastPage || psubclass_data->PageCount <= dev->LastPage - 1)
            return default_subclass_fill_parallelogram(dev, px, py, ax, ay, bx, by, pdcolor, lop);
    }
    return 0;
}

int flp_fill_triangle(gx_device *dev, fixed px, fixed py, fixed ax, fixed ay, fixed bx, fixed by,
    const gx_drawing_color *pdcolor, gs_logical_operation_t lop)
{
    first_last_subclass_data *psubclass_data = dev->subclass_data;

    if (dev->DisablePageHandler || psubclass_data->PageCount >= dev->FirstPage - 1) {
        if (dev->DisablePageHandler || !dev->LastPage || psubclass_data->PageCount <= dev->LastPage - 1)
            return default_subclass_fill_triangle(dev, px, py, ax, ay, bx, by, pdcolor, lop);
    }
    return 0;
}

int flp_draw_thin_line(gx_device *dev, fixed fx0, fixed fy0, fixed fx1, fixed fy1,
    const gx_drawing_color *pdcolor, gs_logical_operation_t lop,
    fixed adjustx, fixed adjusty)
{
    first_last_subclass_data *psubclass_data = dev->subclass_data;

    if (dev->DisablePageHandler || psubclass_data->PageCount >= dev->FirstPage - 1) {
        if (dev->DisablePageHandler || !dev->LastPage || psubclass_data->PageCount <= dev->LastPage - 1)
            return default_subclass_draw_thin_line(dev, fx0, fy0, fx1, fy1, pdcolor, lop, adjustx, adjusty);
    }
    return 0;
}

int flp_begin_image(gx_device *dev, const gs_imager_state *pis, const gs_image_t *pim,
    gs_image_format_t format, const gs_int_rect *prect,
    const gx_drawing_color *pdcolor, const gx_clip_path *pcpath,
    gs_memory_t *memory, gx_image_enum_common_t **pinfo)
{
    first_last_subclass_data *psubclass_data = dev->subclass_data;

    if (dev->DisablePageHandler || psubclass_data->PageCount >= dev->FirstPage - 1) {
        if (dev->DisablePageHandler || !dev->LastPage || psubclass_data->PageCount <= dev->LastPage - 1)
            return default_subclass_begin_image(dev, pis, pim, format, prect, pdcolor, pcpath, memory, pinfo);
    }
    return 0;
}

int flp_image_data(gx_device *dev, gx_image_enum_common_t *info, const byte **planes, int data_x,
    uint raster, int height)
{
    first_last_subclass_data *psubclass_data = dev->subclass_data;

    if (dev->DisablePageHandler || psubclass_data->PageCount >= dev->FirstPage - 1) {
        if (dev->DisablePageHandler || !dev->LastPage || psubclass_data->PageCount <= dev->LastPage - 1)
            return default_subclass_image_data(dev, info, planes, data_x, raster, height);
    }
    return 0;
}

int flp_end_image(gx_device *dev, gx_image_enum_common_t *info, bool draw_last)
{
    first_last_subclass_data *psubclass_data = dev->subclass_data;

    if (dev->DisablePageHandler || psubclass_data->PageCount >= dev->FirstPage - 1) {
        if (dev->DisablePageHandler || !dev->LastPage || psubclass_data->PageCount <= dev->LastPage - 1)
            return default_subclass_end_image(dev, info, draw_last);
    }
    return 0;
}

int flp_strip_tile_rectangle(gx_device *dev, const gx_strip_bitmap *tiles, int x, int y, int width, int height,
    gx_color_index color0, gx_color_index color1,
    int phase_x, int phase_y)
{
    first_last_subclass_data *psubclass_data = dev->subclass_data;

    if (dev->DisablePageHandler || psubclass_data->PageCount >= dev->FirstPage - 1) {
        if (dev->DisablePageHandler || !dev->LastPage || psubclass_data->PageCount <= dev->LastPage - 1)
            return default_subclass_strip_tile_rectangle(dev, tiles, x, y, width, height, color0, color1, phase_x, phase_y);
    }
    return 0;
}

int flp_strip_copy_rop(gx_device *dev, const byte *sdata, int sourcex, uint sraster, gx_bitmap_id id,
    const gx_color_index *scolors,
    const gx_strip_bitmap *textures, const gx_color_index *tcolors,
    int x, int y, int width, int height,
    int phase_x, int phase_y, gs_logical_operation_t lop)
{
    first_last_subclass_data *psubclass_data = dev->subclass_data;

    if (dev->DisablePageHandler || psubclass_data->PageCount >= dev->FirstPage - 1) {
        if (dev->DisablePageHandler || !dev->LastPage || psubclass_data->PageCount <= dev->LastPage - 1)
            return default_subclass_strip_copy_rop(dev, sdata, sourcex, sraster, id, scolors, textures, tcolors, x, y, width, height, phase_x, phase_y, lop);
    }
    return 0;
}

typedef struct flp_image_enum_s {
    gx_image_enum_common;
} flp_image_enum;
gs_private_st_composite(st_flp_image_enum, flp_image_enum, "flp_image_enum",
flp_image_enum_enum_ptrs, flp_image_enum_reloc_ptrs);

static ENUM_PTRS_WITH(flp_image_enum_enum_ptrs, flp_image_enum *pie)
    return ENUM_USING_PREFIX(st_gx_image_enum_common, 0);
ENUM_PTRS_END
static RELOC_PTRS_WITH(flp_image_enum_reloc_ptrs, flp_image_enum *pie)
{
    RELOC_USING(st_gx_image_enum_common, vptr, size);
}
RELOC_PTRS_END

static int
flp_image_plane_data(gx_image_enum_common_t * info,
                     const gx_image_plane_t * planes, int height,
                     int *rows_used)
{
    return 0;
}

static int
flp_image_end_image(gx_image_enum_common_t * info, bool draw_last)
{
    return 0;
}

static const gx_image_enum_procs_t flp_image_enum_procs = {
    flp_image_plane_data,
    flp_image_end_image
};

int flp_begin_typed_image(gx_device *dev, const gs_imager_state *pis, const gs_matrix *pmat,
    const gs_image_common_t *pic, const gs_int_rect *prect,
    const gx_drawing_color *pdcolor, const gx_clip_path *pcpath,
    gs_memory_t *memory, gx_image_enum_common_t **pinfo)
{
    first_last_subclass_data *psubclass_data = dev->subclass_data;
    flp_image_enum *pie;
    const gs_pixel_image_t *pim = (const gs_pixel_image_t *)pic;
    int num_components;


    if (dev->DisablePageHandler || psubclass_data->PageCount >= dev->FirstPage - 1) {
        if (dev->DisablePageHandler || !dev->LastPage || psubclass_data->PageCount <= dev->LastPage - 1)
            return default_subclass_begin_typed_image(dev, pis, pmat, pic, prect, pdcolor, pcpath, memory, pinfo);
    }

    if (pic->type->index == 1) {
        const gs_image_t *pim1 = (const gs_image_t *)pic;

        if (pim1->ImageMask)
            num_components = 1;
        else
            num_components = gs_color_space_num_components(pim->ColorSpace);
    } else {
        num_components = gs_color_space_num_components(pim->ColorSpace);
    }

    pie = gs_alloc_struct(memory, flp_image_enum, &st_flp_image_enum,
                        "flp_begin_image");
    if (pie == 0)
        return_error(gs_error_VMerror);
    memset(pie, 0, sizeof(*pie)); /* cleanup entirely for GC to work in all cases. */
    *pinfo = (gx_image_enum_common_t *) pie;
    gx_image_enum_common_init(*pinfo, (const gs_data_image_t *) pim, &flp_image_enum_procs,
                        (gx_device *)dev, num_components, pim->format);
    pie->memory = memory;
    pie->skipping = true;

    return 0;
}

int flp_get_bits_rectangle(gx_device *dev, const gs_int_rect *prect,
    gs_get_bits_params_t *params, gs_int_rect **unread)
{
    first_last_subclass_data *psubclass_data = dev->subclass_data;

    if (dev->DisablePageHandler || psubclass_data->PageCount >= dev->FirstPage - 1) {
        if (dev->DisablePageHandler || !dev->LastPage || psubclass_data->PageCount <= dev->LastPage - 1)
            return default_subclass_get_bits_rectangle(dev, prect, params, unread);
    }
    return gx_default_get_bits_rectangle(dev->child, prect, params, unread);
}

int flp_create_compositor(gx_device *dev, gx_device **pcdev, const gs_composite_t *pcte,
    gs_imager_state *pis, gs_memory_t *memory, gx_device *cdev)
{
    first_last_subclass_data *psubclass_data = dev->subclass_data;

    if (dev->DisablePageHandler || psubclass_data->PageCount >= dev->FirstPage - 1) {
        if (dev->DisablePageHandler || !dev->LastPage || psubclass_data->PageCount <= dev->LastPage - 1)
            return default_subclass_create_compositor(dev, pcdev, pcte, pis, memory, cdev);
    }

    return 0;
}

/* Text processing (like images) works differently to other device
 * methods. Instead of the interpreter calling a device method, only
 * the 'begin' method is called, this creates a text enumerator which
 * it fills in (in part with the routines for processing text) and returns
 * to the interpreter. The interpreter then calls the methods defined in
 * the text enumerator to process the text.
 * Mad as a fish.....
 */

/* For our purposes if we are handling the text its because we are not
 * printing the page, so we cna afford to ignore all the text processing.
 * A more complex device might need to define real handlers for these, and
 * pass them on to the subclassed device.
 */
static text_enum_proc_process(flp_text_process);
static int
flp_text_resync(gs_text_enum_t *pte, const gs_text_enum_t *pfrom)
{
    return 0;
}
int
flp_text_process(gs_text_enum_t *pte)
{
    return 0;
}
static bool
flp_text_is_width_only(const gs_text_enum_t *pte)
{
    return false;
}
static int
flp_text_current_width(const gs_text_enum_t *pte, gs_point *pwidth)
{
    return 0;
}
static int
flp_text_set_cache(gs_text_enum_t *pte, const double *pw,
                   gs_text_cache_control_t control)
{
    return 0;
}
static int
flp_text_retry(gs_text_enum_t *pte)
{
    return 0;
}
static void
flp_text_release(gs_text_enum_t *pte, client_name_t cname)
{
    gx_default_text_release(pte, cname);
}

static const gs_text_enum_procs_t flp_text_procs = {
    flp_text_resync, flp_text_process,
    flp_text_is_width_only, flp_text_current_width,
    flp_text_set_cache, flp_text_retry,
    flp_text_release
};

/* The device method which we do actually need to define. Either we are skipping the page,
 * in which case we create a text enumerator with our dummy procedures, or we are leaving it
 * up to the device, in which case we simply pass on the 'begin' method to the device.
 */
int flp_text_begin(gx_device *dev, gs_imager_state *pis, const gs_text_params_t *text,
    gs_font *font, gx_path *path, const gx_device_color *pdcolor, const gx_clip_path *pcpath,
    gs_memory_t *memory, gs_text_enum_t **ppte)
{
    flp_text_enum_t *penum;
    int code;
    first_last_subclass_data *psubclass_data = dev->subclass_data;

    /* We don't want to simply ignore stringwidth for 2 reasons;
     * firstly because following elelments may be positioned based on the value returned
     * secondly  because op_show_restore executes an unconditional grestore, assuming
     * that a gsave has been done simply *because* its a tringwidth operation !
     */
    if (dev->DisablePageHandler || ((text->operation & TEXT_DO_NONE) && (text->operation & TEXT_RETURN_WIDTH) && pis->text_rendering_mode != 3))
        /* Note that the high level devices *must* be given the opportunity to 'see' the
         * stringwidth operation, or they won;t be able to cache the glyphs properly.
         * So always pass stringwidth operations to the child.
         */
        return default_subclass_text_begin(dev, pis, text, font, path, pdcolor, pcpath, memory, ppte);

    if (psubclass_data->PageCount >= dev->FirstPage - 1 && (!dev->LastPage || psubclass_data->PageCount <= dev->LastPage - 1))
        return default_subclass_text_begin(dev, pis, text, font, path, pdcolor, pcpath, memory, ppte);

    rc_alloc_struct_1(penum, flp_text_enum_t, &st_flp_text_enum, memory,
                  return_error(gs_error_VMerror), "gdev_flp_text_begin");
    penum->rc.free = rc_free_text_enum;
    code = gs_text_enum_init((gs_text_enum_t *)penum, &flp_text_procs,
                         dev, pis, text, font, path, pdcolor, pcpath, memory);
    if (code < 0) {
        gs_free_object(memory, penum, "gdev_flp_text_begin");
        return code;
    }
    *ppte = (gs_text_enum_t *)penum;

    return 0;
}

int flp_begin_transparency_group(gx_device *dev, const gs_transparency_group_params_t *ptgp,
    const gs_rect *pbbox, gs_imager_state *pis, gs_memory_t *mem)
{
    first_last_subclass_data *psubclass_data = dev->subclass_data;

    if (psubclass_data->PageCount >= dev->FirstPage - 1) {
        if (!dev->LastPage || psubclass_data->PageCount <= dev->LastPage - 1)
            return default_subclass_begin_transparency_group(dev, ptgp, pbbox, pis, mem);
    }
    return 0;
}

int flp_end_transparency_group(gx_device *dev, gs_imager_state *pis)
{
    first_last_subclass_data *psubclass_data = dev->subclass_data;

    if (dev->DisablePageHandler || psubclass_data->PageCount >= dev->FirstPage - 1) {
        if (dev->DisablePageHandler || !dev->LastPage || psubclass_data->PageCount <= dev->LastPage - 1)
            return default_subclass_end_transparency_group(dev, pis);
    }
    return 0;
}

int flp_begin_transparency_mask(gx_device *dev, const gx_transparency_mask_params_t *ptmp,
    const gs_rect *pbbox, gs_imager_state *pis, gs_memory_t *mem)
{
    first_last_subclass_data *psubclass_data = dev->subclass_data;

    if (dev->DisablePageHandler || psubclass_data->PageCount >= dev->FirstPage - 1) {
        if (dev->DisablePageHandler || !dev->LastPage || psubclass_data->PageCount <= dev->LastPage - 1)
            return default_subclass_begin_transparency_mask(dev, ptmp, pbbox, pis, mem);
    }
    return 0;
}

int flp_end_transparency_mask(gx_device *dev, gs_imager_state *pis)
{
    first_last_subclass_data *psubclass_data = dev->subclass_data;

    if (dev->DisablePageHandler || psubclass_data->PageCount >= dev->FirstPage - 1) {
        if (dev->DisablePageHandler || !dev->LastPage || psubclass_data->PageCount <= dev->LastPage - 1)
            return default_subclass_end_transparency_mask(dev, pis);
    }
    return 0;
}

int flp_discard_transparency_layer(gx_device *dev, gs_imager_state *pis)
{
    first_last_subclass_data *psubclass_data = dev->subclass_data;

    if (dev->DisablePageHandler || psubclass_data->PageCount >= dev->FirstPage - 1) {
        if (dev->DisablePageHandler || !dev->LastPage || psubclass_data->PageCount <= dev->LastPage - 1)
            return default_subclass_discard_transparency_layer(dev, pis);
    }
    return 0;
}

int flp_pattern_manage(gx_device *dev, gx_bitmap_id id,
                gs_pattern1_instance_t *pinst, pattern_manage_t function)
{
    first_last_subclass_data *psubclass_data = dev->subclass_data;

    if (dev->DisablePageHandler || psubclass_data->PageCount >= dev->FirstPage - 1) {
        if (dev->DisablePageHandler || !dev->LastPage || psubclass_data->PageCount <= dev->LastPage - 1)
            return default_subclass_pattern_manage(dev, id, pinst, function);
    }
    return 0;
}

int flp_fill_rectangle_hl_color(gx_device *dev, const gs_fixed_rect *rect,
        const gs_imager_state *pis, const gx_drawing_color *pdcolor, const gx_clip_path *pcpath)
{
    first_last_subclass_data *psubclass_data = dev->subclass_data;

    if (dev->DisablePageHandler || psubclass_data->PageCount >= dev->FirstPage - 1) {
        if (dev->DisablePageHandler || !dev->LastPage || psubclass_data->PageCount <= dev->LastPage - 1)
            return default_subclass_fill_rectangle_hl_color(dev, rect, pis, pdcolor, pcpath);
    }
    return 0;
}

int flp_fill_linear_color_scanline(gx_device *dev, const gs_fill_attributes *fa,
        int i, int j, int w, const frac31 *c0, const int32_t *c0_f, const int32_t *cg_num,
        int32_t cg_den)
{
    first_last_subclass_data *psubclass_data = dev->subclass_data;

    if (dev->DisablePageHandler || psubclass_data->PageCount >= dev->FirstPage - 1) {
        if (dev->DisablePageHandler || !dev->LastPage || psubclass_data->PageCount <= dev->LastPage - 1)
            return default_subclass_fill_linear_color_scanline(dev, fa, i, j, w, c0, c0_f, cg_num, cg_den);
    }
    return 0;
}

int flp_fill_linear_color_trapezoid(gx_device *dev, const gs_fill_attributes *fa,
        const gs_fixed_point *p0, const gs_fixed_point *p1,
        const gs_fixed_point *p2, const gs_fixed_point *p3,
        const frac31 *c0, const frac31 *c1,
        const frac31 *c2, const frac31 *c3)
{
    first_last_subclass_data *psubclass_data = dev->subclass_data;

    if (dev->DisablePageHandler || psubclass_data->PageCount >= dev->FirstPage - 1) {
        if (dev->DisablePageHandler || !dev->LastPage || psubclass_data->PageCount <= dev->LastPage - 1)
            return default_subclass_fill_linear_color_trapezoid(dev, fa, p0, p1, p2, p3, c0, c1, c2, c3);
    }
    return 0;
}

int flp_fill_linear_color_triangle(gx_device *dev, const gs_fill_attributes *fa,
        const gs_fixed_point *p0, const gs_fixed_point *p1,
        const gs_fixed_point *p2, const frac31 *c0, const frac31 *c1, const frac31 *c2)
{
    first_last_subclass_data *psubclass_data = dev->subclass_data;

    if (dev->DisablePageHandler || psubclass_data->PageCount >= dev->FirstPage - 1) {
        if (dev->DisablePageHandler || !dev->LastPage || psubclass_data->PageCount <= dev->LastPage - 1)
            return default_subclass_fill_linear_color_triangle(dev, fa, p0, p1, p2, c0, c1, c2);
    }
    return 0;
}

int flp_fillpage(gx_device *dev, gs_imager_state * pis, gx_device_color *pdevc)
{
    first_last_subclass_data *psubclass_data = dev->subclass_data;

    if (dev->DisablePageHandler || psubclass_data->PageCount >= dev->FirstPage - 1) {
        if (dev->DisablePageHandler || !dev->LastPage || psubclass_data->PageCount <= dev->LastPage - 1)
            return default_subclass_fillpage(dev, pis, pdevc);
    }
    return 0;
}

int flp_push_transparency_state(gx_device *dev, gs_imager_state *pis)
{
    first_last_subclass_data *psubclass_data = dev->subclass_data;

    if (dev->DisablePageHandler || psubclass_data->PageCount >= dev->FirstPage - 1) {
        if (dev->DisablePageHandler || !dev->LastPage || psubclass_data->PageCount <= dev->LastPage - 1)
            return default_subclass_push_transparency_state(dev, pis);
    }
    return 0;
}

int flp_pop_transparency_state(gx_device *dev, gs_imager_state *pis)
{
    first_last_subclass_data *psubclass_data = dev->subclass_data;

    if (dev->DisablePageHandler || psubclass_data->PageCount >= dev->FirstPage - 1) {
        if (dev->DisablePageHandler || !dev->LastPage || psubclass_data->PageCount <= dev->LastPage - 1)
            return default_subclass_pop_transparency_state(dev, pis);
    }
    return 0;
}

int flp_put_image(gx_device *dev, const byte *buffer, int num_chan, int x, int y,
            int width, int height, int row_stride, int plane_stride,
            int alpha_plane_index, int tag_plane_index)
{
    first_last_subclass_data *psubclass_data = dev->subclass_data;

    if (dev->DisablePageHandler || psubclass_data->PageCount >= dev->FirstPage - 1) {
        if (dev->DisablePageHandler || !dev->LastPage || psubclass_data->PageCount <= dev->LastPage - 1)
            return default_subclass_put_image(dev, buffer, num_chan, x, y, width, height, row_stride, plane_stride, alpha_plane_index, tag_plane_index);
    }
    return 0;
}

int flp_copy_planes(gx_device *dev, const byte *data, int data_x, int raster, gx_bitmap_id id,
    int x, int y, int width, int height, int plane_height)
{
    first_last_subclass_data *psubclass_data = dev->subclass_data;

    if (dev->DisablePageHandler || psubclass_data->PageCount >= dev->FirstPage - 1) {
        if (dev->DisablePageHandler || !dev->LastPage || psubclass_data->PageCount <= dev->LastPage - 1)
            return default_subclass_copy_planes(dev, data, data_x, raster, id, x, y, width, height, plane_height);
    }
    return 0;
}

void flp_set_graphics_type_tag(gx_device *dev, gs_graphics_type_tag_t tag)
{
    first_last_subclass_data *psubclass_data = dev->subclass_data;

    if (dev->DisablePageHandler || psubclass_data->PageCount >= dev->FirstPage - 1) {
        if (dev->DisablePageHandler || !dev->LastPage || psubclass_data->PageCount <= dev->LastPage - 1)
            default_subclass_set_graphics_type_tag(dev, tag);
    }
    return;
}

int flp_strip_copy_rop2(gx_device *dev, const byte *sdata, int sourcex, uint sraster, gx_bitmap_id id,
    const gx_color_index *scolors, const gx_strip_bitmap *textures, const gx_color_index *tcolors,
    int x, int y, int width, int height, int phase_x, int phase_y, gs_logical_operation_t lop, uint planar_height)
{
    first_last_subclass_data *psubclass_data = dev->subclass_data;

    if (dev->DisablePageHandler || psubclass_data->PageCount >= dev->FirstPage - 1) {
        if (dev->DisablePageHandler || !dev->LastPage || psubclass_data->PageCount <= dev->LastPage - 1)
            return default_subclass_strip_copy_rop2(dev, sdata, sourcex, sraster, id, scolors, textures, tcolors, x, y, width, height, phase_x, phase_y, lop, planar_height);
    }
    return 0;
}

int flp_strip_tile_rect_devn(gx_device *dev, const gx_strip_bitmap *tiles, int x, int y, int width, int height,
    const gx_drawing_color *pdcolor0, const gx_drawing_color *pdcolor1, int phase_x, int phase_y)
{
    first_last_subclass_data *psubclass_data = dev->subclass_data;

    if (dev->DisablePageHandler || psubclass_data->PageCount >= dev->FirstPage - 1) {
        if (dev->DisablePageHandler || !dev->LastPage || psubclass_data->PageCount <= dev->LastPage - 1)
            return default_subclass_strip_tile_rect_devn(dev, tiles, x, y, width, height, pdcolor0, pdcolor1, phase_x, phase_y);
    }
    return 0;
}

int flp_copy_alpha_hl_color(gx_device *dev, const byte *data, int data_x,
    int raster, gx_bitmap_id id, int x, int y, int width, int height,
    const gx_drawing_color *pdcolor, int depth)
{
    first_last_subclass_data *psubclass_data = dev->subclass_data;

    if (dev->DisablePageHandler || psubclass_data->PageCount >= dev->FirstPage - 1) {
        if (dev->DisablePageHandler || !dev->LastPage || psubclass_data->PageCount <= dev->LastPage - 1)
            return default_subclass_copy_alpha_hl_color(dev, data, data_x, raster, id, x, y, width, height, pdcolor, depth);
    }

    return 0;
}

int flp_process_page(gx_device *dev, gx_process_page_options_t *options)
{
    first_last_subclass_data *psubclass_data = dev->subclass_data;

    if (dev->DisablePageHandler || psubclass_data->PageCount >= dev->FirstPage - 1) {
        if (dev->DisablePageHandler || !dev->LastPage || psubclass_data->PageCount <= dev->LastPage - 1)
            return default_subclass_process_page(dev, options);
    }
    return 0;
}
