/* Copyright (C) 2001-2021 Artifex Software, Inc.
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

/* Device for erasepage optimization subclass device */

#include "math_.h"
#include "memory_.h"
#include "gx.h"
#include "gserrors.h"
#include "gsparam.h"
#include "gxdevice.h"
#include "gsdevice.h"		/* requires gsmatrix.h */
#include "gxdcolor.h"		/* for gx_device_black/white */
#include "gxiparam.h"		/* for image source size */
#include "gxgstate.h"
#include "gxpaint.h"
#include "gxpath.h"
#include "gxcpath.h"
#include "gxcmap.h"         /* color mapping procs */
#include "gsstype.h"
#include "gdevprn.h"
#include "gdevp14.h"        /* Needed to patch up the procs after compositor creation */
#include "gximage.h"        /* For gx_image_enum */
#include "gdevsclass.h"
#include "gsstate.h"
#include "gxdevsop.h"
#include "gdevepo.h"
#include <stdlib.h>

/* Shorter macros for sanity's sake */
#define DPRINTF(m,f) if_debug0m(gs_debug_flag_epo_details, m,f)
#define DPRINTF1(m,f,a1) if_debug1m(gs_debug_flag_epo_details, m,f, a1)

/* This is only called for debugging */
extern void epo_disable(int flag);

/* Device procedures, we need quite a lot of them */
static dev_proc_output_page(epo_output_page);
static dev_proc_fill_rectangle(epo_fill_rectangle);
static dev_proc_fill_path(epo_fill_path);
static dev_proc_fill_mask(epo_fill_mask);
static dev_proc_fill_trapezoid(epo_fill_trapezoid);
static dev_proc_fill_parallelogram(epo_fill_parallelogram);
static dev_proc_fill_triangle(epo_fill_triangle);
static dev_proc_draw_thin_line(epo_draw_thin_line);
static dev_proc_fill_rectangle_hl_color(epo_fill_rectangle_hl_color);
static dev_proc_fill_linear_color_scanline(epo_fill_linear_color_scanline);
static dev_proc_fill_linear_color_trapezoid(epo_fill_linear_color_trapezoid);
static dev_proc_fill_linear_color_triangle(epo_fill_linear_color_triangle);
static dev_proc_put_image(epo_put_image);
static dev_proc_fillpage(epo_fillpage);
static dev_proc_composite(epo_composite);
static dev_proc_text_begin(epo_text_begin);
static dev_proc_initialize_device_procs(epo_initialize_device_procs);
static dev_proc_begin_typed_image(epo_begin_typed_image);
static dev_proc_stroke_path(epo_stroke_path);
static dev_proc_copy_mono(epo_copy_mono);
static dev_proc_copy_color(epo_copy_color);
static dev_proc_copy_alpha(epo_copy_alpha);
static dev_proc_get_bits_rectangle(epo_get_bits_rectangle);
static dev_proc_strip_tile_rectangle(epo_strip_tile_rectangle);
static dev_proc_strip_copy_rop2(epo_strip_copy_rop2);
static dev_proc_copy_planes(epo_copy_planes);
static dev_proc_copy_alpha_hl_color(epo_copy_alpha_hl_color);
static dev_proc_process_page(epo_process_page);
static dev_proc_transform_pixel_region(epo_transform_pixel_region);
static dev_proc_fill_stroke_path(epo_fill_stroke_path);

/* The device prototype */
#define MAX_COORD (max_int_in_fixed - 1000)
#define MAX_RESOLUTION 4000

#define public_st_epo_device()	/* in gsdevice.c */\
  gs_public_st_complex_only(st_epo_device, gx_device, EPO_DEVICENAME,\
    0, epo_enum_ptrs, epo_reloc_ptrs, default_subclass_finalize)

static
ENUM_PTRS_WITH(epo_enum_ptrs, gx_device *dev);
return 0; /* default case */
case 0:ENUM_RETURN(gx_device_enum_ptr(dev->parent));
case 1:ENUM_RETURN(gx_device_enum_ptr(dev->child));
ENUM_PTRS_END
static RELOC_PTRS_WITH(epo_reloc_ptrs, gx_device *dev)
{
    dev->parent = gx_device_reloc_ptr(dev->parent, gcst);
    dev->child = gx_device_reloc_ptr(dev->child, gcst);
}RELOC_PTRS_END

public_st_epo_device();

const
gx_device_epo gs_epo_device =
{
    std_device_dci_type_body_sc(gx_device_epo, epo_initialize_device_procs,
                        EPO_DEVICENAME, &st_epo_device,
                        MAX_COORD, MAX_COORD,
                        MAX_RESOLUTION, MAX_RESOLUTION,
                        1, 8, 255, 0, 256, 1, NULL, NULL, NULL)
};

#undef MAX_COORD
#undef MAX_RESOLUTION

/* Starting at the top of the device chain (top parent) find	*/
/* and return the epo device, or NULL if not found.		*/
static gx_device *
find_installed_epo_device(gx_device *dev)
{
    gx_device *next_dev = dev;

    while (next_dev->parent != NULL)
        next_dev = next_dev->parent;

    while (next_dev) {
        if (next_dev->procs.fillpage == epo_fillpage) {
            return next_dev;
        }
        next_dev = next_dev->child;
    }
    return NULL;
}

/* See if this is a device we can optimize
 * (currently only the ones that use gx_default_fillpage, which
 * automatically excludes clist and pdfwrite, etc)
 */
static bool
device_wants_optimization(gx_device *dev)
{
    gx_device *terminal = dev;

    while(terminal->child != NULL)
        terminal = terminal->child;

    return (!gs_is_null_device(terminal) && dev_proc(terminal, fillpage) == gx_default_fillpage);
}

/* Use this when debugging to enable/disable epo
 * (1 - disable, 0 - enable)
 */
void
epo_disable(int flag)
{
    gs_debug[gs_debug_flag_epo_disable] = flag;
}

static void
enable_procs(gx_device *dev)
{
    set_dev_proc(dev, output_page, epo_output_page);
    set_dev_proc(dev, fill_rectangle, epo_fill_rectangle);
    set_dev_proc(dev, copy_mono, epo_copy_mono);
    set_dev_proc(dev, copy_color, epo_copy_color);
    set_dev_proc(dev, copy_alpha, epo_copy_alpha);
    set_dev_proc(dev, get_bits_rectangle, epo_get_bits_rectangle);
    set_dev_proc(dev, fill_path, epo_fill_path);
    set_dev_proc(dev, stroke_path, epo_stroke_path);
    set_dev_proc(dev, fill_mask, epo_fill_mask);
    set_dev_proc(dev, fill_trapezoid, epo_fill_trapezoid);
    set_dev_proc(dev, fill_parallelogram, epo_fill_parallelogram);
    set_dev_proc(dev, fill_triangle, epo_fill_triangle);
    set_dev_proc(dev, draw_thin_line, epo_draw_thin_line);
    set_dev_proc(dev, strip_tile_rectangle, epo_strip_tile_rectangle);
    set_dev_proc(dev, begin_typed_image, epo_begin_typed_image);
    set_dev_proc(dev, composite, epo_composite);
    set_dev_proc(dev, text_begin, epo_text_begin);
    set_dev_proc(dev, fill_rectangle_hl_color, epo_fill_rectangle_hl_color);
    set_dev_proc(dev, fill_linear_color_scanline, epo_fill_linear_color_scanline);
    set_dev_proc(dev, fill_linear_color_trapezoid, epo_fill_linear_color_trapezoid);
    set_dev_proc(dev, fill_linear_color_triangle, epo_fill_linear_color_triangle);
    set_dev_proc(dev, fillpage, epo_fillpage);
    set_dev_proc(dev, put_image, epo_put_image);
    set_dev_proc(dev, copy_planes, epo_copy_planes);
    set_dev_proc(dev, strip_copy_rop2, epo_strip_copy_rop2);
    set_dev_proc(dev, copy_alpha_hl_color, epo_copy_alpha_hl_color);
    set_dev_proc(dev, process_page, epo_process_page);
    set_dev_proc(dev, transform_pixel_region, epo_transform_pixel_region);
    set_dev_proc(dev, fill_stroke_path, epo_fill_stroke_path);
}

static void
enable_self(gx_device *dev)
{
    erasepage_subclass_data *data = (erasepage_subclass_data *)dev->subclass_data;

    data->disabled = 0;
    enable_procs(dev);
}

static void
disable_self(gx_device *dev)
{
    erasepage_subclass_data *data = (erasepage_subclass_data *)dev->subclass_data;

    data->disabled = 1;

    set_dev_proc(dev, output_page, default_subclass_output_page);
    set_dev_proc(dev, fill_rectangle, default_subclass_fill_rectangle);
    set_dev_proc(dev, copy_mono, default_subclass_copy_mono);
    set_dev_proc(dev, copy_color, default_subclass_copy_color);
    set_dev_proc(dev, copy_alpha, default_subclass_copy_alpha);
    set_dev_proc(dev, get_bits_rectangle, default_subclass_get_bits_rectangle);
    set_dev_proc(dev, fill_path, default_subclass_fill_path);
    set_dev_proc(dev, stroke_path, default_subclass_stroke_path);
    set_dev_proc(dev, fill_mask, default_subclass_fill_mask);
    set_dev_proc(dev, fill_trapezoid, default_subclass_fill_trapezoid);
    set_dev_proc(dev, fill_parallelogram, default_subclass_fill_parallelogram);
    set_dev_proc(dev, fill_triangle, default_subclass_fill_triangle);
    set_dev_proc(dev, draw_thin_line, default_subclass_draw_thin_line);
    set_dev_proc(dev, strip_tile_rectangle, default_subclass_strip_tile_rectangle);
    set_dev_proc(dev, begin_typed_image, default_subclass_begin_typed_image);
    set_dev_proc(dev, composite, default_subclass_composite);
    set_dev_proc(dev, text_begin, default_subclass_text_begin);
    set_dev_proc(dev, fill_rectangle_hl_color, default_subclass_fill_rectangle_hl_color);
    set_dev_proc(dev, fill_linear_color_scanline, default_subclass_fill_linear_color_scanline);
    set_dev_proc(dev, fill_linear_color_trapezoid, default_subclass_fill_linear_color_trapezoid);
    set_dev_proc(dev, fill_linear_color_triangle, default_subclass_fill_linear_color_triangle);
    /* NOT fillpage! */
    set_dev_proc(dev, put_image, default_subclass_put_image);
    set_dev_proc(dev, copy_planes, default_subclass_copy_planes);
    set_dev_proc(dev, strip_copy_rop2, default_subclass_strip_copy_rop2);
    set_dev_proc(dev, copy_alpha_hl_color, default_subclass_copy_alpha_hl_color);
    set_dev_proc(dev, process_page, default_subclass_process_page);
    set_dev_proc(dev, transform_pixel_region, default_subclass_transform_pixel_region);
    set_dev_proc(dev, fill_stroke_path, default_subclass_fill_stroke_path);
}

int
epo_check_and_install(gx_device *dev)
{
    int code = 0;
    gx_device *installed_epo_device = NULL;
    bool can_optimize = false;

    /* Debugging mode to totally disable this */
    if (gs_debug_c(gs_debug_flag_epo_disable)) {
        return code;
    }

    DPRINTF1(dev->memory, "current device is %s\n", dev->dname);

    installed_epo_device = find_installed_epo_device(dev);

    if (installed_epo_device != NULL) {
        DPRINTF1(dev->memory, "device %s already installed\n", EPO_DEVICENAME);
        /* This is looking for the case where the device
         * changed into something we can't optimize, after it was already installed
         * (could be clist or some other weird thing)
         */
        if (installed_epo_device->child) {
            can_optimize = device_wants_optimization(installed_epo_device->child);
        }
        if (!can_optimize) {
            DPRINTF1(dev->memory, "child %s can't be optimized, uninstalling\n", installed_epo_device->child->dname);
            /* Not doing any pending fillpages because we are about to do a fillpage anyway */
            disable_self(installed_epo_device);
            return code;
        }
    } else {
        can_optimize = device_wants_optimization(dev);
    }

    /* Already installed, nothing to do */
    if (installed_epo_device != NULL) {
        enable_self(installed_epo_device);
        return code;
    }

    /* See if we can optimize */
    if (!can_optimize) {
        DPRINTF(dev->memory, "device doesn't want optimization, not installing\n");
        return code;
    }

    /* Always install us as low down the chain as possible. */
    while (dev->child)
            dev = dev->child;

    /* Install subclass for optimization */
    code = gx_device_subclass(dev, (gx_device *)&gs_epo_device, sizeof(erasepage_subclass_data));
    if (code < 0) {
        DPRINTF1(dev->memory, "ERROR installing device %s\n", EPO_DEVICENAME);
        return code;
    }

    DPRINTF1(dev->memory, "SUCCESS installed device %s\n", dev->dname);
    return code;
}

static int
epo_handle_erase_page(gx_device *dev)
{
    erasepage_subclass_data *data = (erasepage_subclass_data *)dev->subclass_data;
    int code = 0;

    if (data->disabled)
        return 0;

    if (gs_debug_c(gs_debug_flag_epo_install_only)) {
        disable_self(dev);
        DPRINTF1(dev->memory, "Uninstall erasepage, device=%s\n", dev->dname);
        return code;
    }

    DPRINTF1(dev->memory, "Do fillpage, Uninstall erasepage, device %s\n", dev->dname);

    /* Just do a fill_rectangle (using saved color) */
    if (dev->child && data->queued) {
        code = dev_proc(dev->child, fill_rectangle)(dev->child,
                                                    0, 0,
                                                    dev->child->width,
                                                    dev->child->height,
                                                    data->last_color);
    }
    /* Disable the epo device */
    disable_self(dev);

    return code;
}

int epo_fillpage(gx_device *dev, gs_gstate * pgs, gx_device_color *pdevc)
{
    erasepage_subclass_data *data = (erasepage_subclass_data *)dev->subclass_data;

    if (data->disabled || gs_debug_c(gs_debug_flag_epo_install_only)) {
        return default_subclass_fillpage(dev, pgs, pdevc);
    }

    /* If color is not pure, don't defer this, uninstall and do it now */
    if (!color_is_pure(pdevc)) {
        DPRINTF(dev->memory, "epo_fillpage(), color is not pure, uninstalling\n");
        disable_self(dev);
        return dev_proc(dev->child, fillpage)(dev->child, pgs, pdevc);
    }

    /* Save the color being requested, and swallow the fillpage */
    data->last_color = pdevc->colors.pure;
    data->queued = 1;

    DPRINTF(dev->memory, "Swallowing fillpage\n");
    return 0;
}

int epo_output_page(gx_device *dev, int num_copies, int flush)
{
    int code = epo_handle_erase_page(dev);

    if (code != 0)
        return code;
    dev = dev->child;
    return dev_proc(dev, output_page)(dev, num_copies, flush);
}

int epo_fill_rectangle(gx_device *dev, int x, int y, int width, int height, gx_color_index color)
{
    int code = epo_handle_erase_page(dev);

    if (code != 0)
        return code;
    dev = dev->child;
    return dev_proc(dev, fill_rectangle)(dev, x, y, width, height, color);
}

int epo_fill_path(gx_device *dev, const gs_gstate *pgs, gx_path *ppath,
    const gx_fill_params *params,
    const gx_drawing_color *pdcolor, const gx_clip_path *pcpath)
{
    int code = epo_handle_erase_page(dev);

    if (code != 0)
        return code;
    dev = dev->child;
    return dev_proc(dev, fill_path)(dev, pgs, ppath, params, pdcolor, pcpath);
}

int epo_fill_mask(gx_device *dev, const byte *data, int data_x, int raster, gx_bitmap_id id,
    int x, int y, int width, int height,
    const gx_drawing_color *pdcolor, int depth,
    gs_logical_operation_t lop, const gx_clip_path *pcpath)
{
    int code = epo_handle_erase_page(dev);

    if (code != 0)
        return code;
    dev = dev->child;
    return dev_proc(dev, fill_mask)(dev, data, data_x, raster, id, x, y, width, height, pdcolor, depth, lop, pcpath);
}

int epo_fill_trapezoid(gx_device *dev, const gs_fixed_edge *left, const gs_fixed_edge *right,
    fixed ybot, fixed ytop, bool swap_axes,
    const gx_drawing_color *pdcolor, gs_logical_operation_t lop)
{
    int code = epo_handle_erase_page(dev);

    if (code != 0)
        return code;
    dev = dev->child;
    return dev_proc(dev, fill_trapezoid)(dev, left, right, ybot, ytop, swap_axes, pdcolor, lop);
}

int epo_fill_parallelogram(gx_device *dev, fixed px, fixed py, fixed ax, fixed ay, fixed bx, fixed by,
    const gx_drawing_color *pdcolor, gs_logical_operation_t lop)
{
    int code = epo_handle_erase_page(dev);

    if (code != 0)
        return code;
    dev = dev->child;
    return dev_proc(dev, fill_parallelogram)(dev, px, py, ax, ay, bx, by, pdcolor, lop);
}

int epo_fill_triangle(gx_device *dev, fixed px, fixed py, fixed ax, fixed ay, fixed bx, fixed by,
    const gx_drawing_color *pdcolor, gs_logical_operation_t lop)
{
    int code = epo_handle_erase_page(dev);

    if (code != 0)
        return code;
    dev = dev->child;
    return dev_proc(dev, fill_triangle)(dev, px, py, ax, ay, bx, by, pdcolor, lop);
}

int epo_draw_thin_line(gx_device *dev, fixed fx0, fixed fy0, fixed fx1, fixed fy1,
    const gx_drawing_color *pdcolor, gs_logical_operation_t lop,
    fixed adjustx, fixed adjusty)
{
    int code = epo_handle_erase_page(dev);

    if (code != 0)
        return code;
    dev = dev->child;
    return dev_proc(dev, draw_thin_line)(dev, fx0, fy0, fx1, fy1, pdcolor, lop, adjustx, adjusty);
}

int epo_fill_rectangle_hl_color(gx_device *dev, const gs_fixed_rect *rect,
        const gs_gstate *pgs, const gx_drawing_color *pdcolor, const gx_clip_path *pcpath)
{
    int code = epo_handle_erase_page(dev);

    if (code != 0)
        return code;
    dev = dev->child;
    return dev_proc(dev, fill_rectangle_hl_color)(dev, rect, pgs, pdcolor, pcpath);
}

int epo_fill_linear_color_scanline(gx_device *dev, const gs_fill_attributes *fa,
        int i, int j, int w, const frac31 *c0, const int32_t *c0_f, const int32_t *cg_num,
        int32_t cg_den)
{
    int code = epo_handle_erase_page(dev);

    if (code != 0)
        return code;
    dev = dev->child;
    return dev_proc(dev, fill_linear_color_scanline)(dev, fa, i, j, w, c0, c0_f, cg_num, cg_den);
}

int epo_fill_linear_color_trapezoid(gx_device *dev, const gs_fill_attributes *fa,
        const gs_fixed_point *p0, const gs_fixed_point *p1,
        const gs_fixed_point *p2, const gs_fixed_point *p3,
        const frac31 *c0, const frac31 *c1,
        const frac31 *c2, const frac31 *c3)
{
    int code = epo_handle_erase_page(dev);

    if (code != 0)
        return code;
    dev = dev->child;
    return dev_proc(dev, fill_linear_color_trapezoid)(dev, fa, p0, p1, p2, p3, c0, c1, c2, c3);
}

int epo_fill_linear_color_triangle(gx_device *dev, const gs_fill_attributes *fa,
        const gs_fixed_point *p0, const gs_fixed_point *p1,
        const gs_fixed_point *p2, const frac31 *c0, const frac31 *c1, const frac31 *c2)
{
    int code = epo_handle_erase_page(dev);

    if (code != 0)
        return code;
    dev = dev->child;
    return dev_proc(dev, fill_linear_color_triangle)(dev, fa, p0, p1, p2, c0, c1, c2);
}

int epo_put_image(gx_device *dev, gx_device *mdev, const byte **buffers, int num_chan, int x, int y,
            int width, int height, int row_stride,
            int alpha_plane_index, int tag_plane_index)
{
    int code = epo_handle_erase_page(dev);

    if (code != 0)
        return code;
    dev = dev->child;
    return dev_proc(dev, put_image)(dev, mdev, buffers, num_chan, x, y, width, height, row_stride, alpha_plane_index, tag_plane_index);
}

int epo_composite(gx_device *dev, gx_device **pcdev, const gs_composite_t *pcte,
    gs_gstate *pgs, gs_memory_t *memory, gx_device *cdev)
{
    int code = epo_handle_erase_page(dev);

    if (code != 0)
        return code;
    dev = dev->child;
    return dev_proc(dev, composite)(dev, pcdev, pcte, pgs, memory, cdev);
}

int epo_text_begin(gx_device *dev, gs_gstate *pgs, const gs_text_params_t *text,
    gs_font *font, const gx_clip_path *pcpath,
    gs_text_enum_t **ppte)
{
    int code = epo_handle_erase_page(dev);

    if (code != 0)
        return code;
    dev = dev->child;
    return dev_proc(dev, text_begin)(dev, pgs, text, font, pcpath, ppte);
}

static int epo_initialize_device(gx_device *dev)
{
    /* We musn't allow the following pointers to remain shared with the from_dev
       because we're about to tell the caller it's only allowed to copy the prototype
       and free the attempted copy of a non-prototype. If from_dev is the prototype
       these pointers won't be set, anyway.
     */
    dev->child = NULL;
    dev->parent = NULL;
    dev->subclass_data = NULL;
    return 0;
}

void epo_initialize_device_procs(gx_device *dev)
{
    default_subclass_initialize_device_procs(dev);

    set_dev_proc(dev, initialize_device, epo_initialize_device);
    enable_procs(dev);
}

int epo_begin_typed_image(gx_device *dev, const gs_gstate *pgs, const gs_matrix *pmat,
    const gs_image_common_t *pic, const gs_int_rect *prect,
    const gx_drawing_color *pdcolor, const gx_clip_path *pcpath,
    gs_memory_t *memory, gx_image_enum_common_t **pinfo)
{
    int code = epo_handle_erase_page(dev);

    if (code != 0)
        return code;
    dev = dev->child;
    return dev_proc(dev, begin_typed_image)(dev, pgs, pmat, pic, prect, pdcolor, pcpath, memory, pinfo);
}

int epo_stroke_path(gx_device *dev, const gs_gstate *pgs, gx_path *ppath,
    const gx_stroke_params *params,
    const gx_drawing_color *pdcolor, const gx_clip_path *pcpath)
{
    int code = epo_handle_erase_page(dev);

    if (code != 0)
        return code;
    dev = dev->child;
    return dev_proc(dev, stroke_path)(dev, pgs, ppath, params, pdcolor, pcpath);
}

int epo_copy_mono(gx_device *dev, const byte *data, int data_x, int raster, gx_bitmap_id id,
    int x, int y, int width, int height,
    gx_color_index color0, gx_color_index color1)
{
    int code = epo_handle_erase_page(dev);

    if (code != 0)
        return code;
    dev = dev->child;
    return dev_proc(dev, copy_mono)(dev, data, data_x, raster, id, x, y, width, height, color0, color1);
}

int epo_copy_color(gx_device *dev, const byte *data, int data_x, int raster, gx_bitmap_id id,\
    int x, int y, int width, int height)
{
    int code = epo_handle_erase_page(dev);

    if (code != 0)
        return code;
    dev = dev->child;
    return dev_proc(dev, copy_color)(dev, data, data_x, raster, id, x, y, width, height);
}

int epo_get_bits_rectangle(gx_device *dev, const gs_int_rect *prect,
                           gs_get_bits_params_t *params)
{
    int code = epo_handle_erase_page(dev);

    if (code != 0)
        return code;
    dev = dev->child;
    return dev_proc(dev, get_bits_rectangle)(dev, prect, params);
}

int epo_copy_alpha(gx_device *dev, const byte *data, int data_x,
    int raster, gx_bitmap_id id, int x, int y, int width, int height,
    gx_color_index color, int depth)
{
    int code = epo_handle_erase_page(dev);

    if (code != 0)
        return code;
    dev = dev->child;
    return dev_proc(dev, copy_alpha)(dev, data, data_x, raster, id, x, y, width, height, color, depth);
}

int epo_strip_tile_rectangle(gx_device *dev, const gx_strip_bitmap *tiles, int x, int y, int width, int height,
    gx_color_index color0, gx_color_index color1,
    int phase_x, int phase_y)
{
    int code = epo_handle_erase_page(dev);

    if (code != 0)
        return code;
    dev = dev->child;
    return dev_proc(dev, strip_tile_rectangle)(dev, tiles, x, y, width, height, color0, color1, phase_x, phase_y);
}

int epo_strip_copy_rop2(gx_device *dev, const byte *sdata, int sourcex, uint sraster, gx_bitmap_id id,
    const gx_color_index *scolors, const gx_strip_bitmap *textures, const gx_color_index *tcolors,
    int x, int y, int width, int height, int phase_x, int phase_y, gs_logical_operation_t lop, uint planar_height)
{
    int code = epo_handle_erase_page(dev);

    if (code != 0)
        return code;
    dev = dev->child;
    return dev_proc(dev, strip_copy_rop2)(dev, sdata, sourcex, sraster, id, scolors, textures, tcolors, x, y, width, height, phase_x, phase_y, lop, planar_height);
}

int epo_copy_planes(gx_device *dev, const byte *data, int data_x, int raster, gx_bitmap_id id,
    int x, int y, int width, int height, int plane_height)
{
    int code = epo_handle_erase_page(dev);

    if (code != 0)
        return code;
    dev = dev->child;
    return dev_proc(dev, copy_planes)(dev, data, data_x, raster, id, x, y, width, height, plane_height);
}

int epo_copy_alpha_hl_color(gx_device *dev, const byte *data, int data_x,
    int raster, gx_bitmap_id id, int x, int y, int width, int height,
    const gx_drawing_color *pdcolor, int depth)
{
    int code = epo_handle_erase_page(dev);

    if (code != 0)
        return code;
    dev = dev->child;
    return dev_proc(dev, copy_alpha_hl_color)(dev, data, data_x, raster, id, x, y, width, height, pdcolor, depth);
}

int epo_process_page(gx_device *dev, gx_process_page_options_t *options)
{
    int code = epo_handle_erase_page(dev);

    if (code != 0)
        return code;
    dev = dev->child;
    return dev_proc(dev, process_page)(dev, options);
}

int epo_fill_stroke_path(gx_device *dev, const gs_gstate *pgs, gx_path *ppath,
    const gx_fill_params *fill_params, const gx_drawing_color *pdcolor_fill,
    const gx_stroke_params *stroke_params, const gx_drawing_color *pdcolor_stroke,
    const gx_clip_path *pcpath)
{
    int code = epo_handle_erase_page(dev);

    if (code != 0)
        return code;
    dev = dev->child;
    return dev_proc(dev, fill_stroke_path)(dev, pgs, ppath, fill_params, pdcolor_fill,
                                           stroke_params, pdcolor_stroke, pcpath);
}

int epo_transform_pixel_region(gx_device *dev, transform_pixel_region_reason reason, transform_pixel_region_data *data)
{
    int code = epo_handle_erase_page(dev);

    if (code != 0)
        return code;
    dev = dev->child;
    return dev_proc(dev, transform_pixel_region)(dev, reason, data);
}
