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

/* Derived from gdevflp.c */
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
#include "gdevoflt.h"

int gs_is_pdf14trans_compositor(const gs_composite_t * pct);

/* GC descriptor */
public_st_device_obj_filter();
/* we need text and image enumerators, because of the crazy way that text and images work */
private_st_obj_filter_text_enum();

/* Device procedures, we need to implement all of them */
static dev_proc_fill_rectangle(obj_filter_fill_rectangle);
static dev_proc_tile_rectangle(obj_filter_tile_rectangle);
static dev_proc_draw_line(obj_filter_draw_line);
static dev_proc_fill_path(obj_filter_fill_path);
static dev_proc_stroke_path(obj_filter_stroke_path);
static dev_proc_fill_mask(obj_filter_fill_mask);
static dev_proc_fill_trapezoid(obj_filter_fill_trapezoid);
static dev_proc_fill_parallelogram(obj_filter_fill_parallelogram);
static dev_proc_fill_triangle(obj_filter_fill_triangle);
static dev_proc_draw_thin_line(obj_filter_draw_thin_line);
static dev_proc_begin_image(obj_filter_begin_image);
static dev_proc_image_data(obj_filter_image_data);
static dev_proc_end_image(obj_filter_end_image);
static dev_proc_strip_tile_rectangle(obj_filter_strip_tile_rectangle);
static dev_proc_strip_copy_rop(obj_filter_strip_copy_rop);
static dev_proc_begin_typed_image(obj_filter_begin_typed_image);
static dev_proc_text_begin(obj_filter_text_begin);
static dev_proc_fill_rectangle_hl_color(obj_filter_fill_rectangle_hl_color);
static dev_proc_fill_linear_color_scanline(obj_filter_fill_linear_color_scanline);
static dev_proc_fill_linear_color_trapezoid(obj_filter_fill_linear_color_trapezoid);
static dev_proc_fill_linear_color_triangle(obj_filter_fill_linear_color_triangle);
static dev_proc_put_image(obj_filter_put_image);
static dev_proc_strip_copy_rop2(obj_filter_strip_copy_rop2);
static dev_proc_strip_tile_rect_devn(obj_filter_strip_tile_rect_devn);
static dev_proc_fill_stroke_path(obj_filter_fill_stroke_path);

/* The device prototype */
#define MAX_COORD (max_int_in_fixed - 1000)
#define MAX_RESOLUTION 4000

#define public_st_obj_filter_device()	/* in gsdevice.c */\
  gs_public_st_complex_only(st_obj_filter_device, gx_device, "object filter",\
    0, obj_filter_enum_ptrs, obj_filter_reloc_ptrs, default_subclass_finalize)

static
ENUM_PTRS_WITH(obj_filter_enum_ptrs, gx_device *dev);
return 0; /* default case */
case 0:ENUM_RETURN(gx_device_enum_ptr(dev->parent));
case 1:ENUM_RETURN(gx_device_enum_ptr(dev->child));
ENUM_PTRS_END
static RELOC_PTRS_WITH(obj_filter_reloc_ptrs, gx_device *dev)
{
    dev->parent = gx_device_reloc_ptr(dev->parent, gcst);
    dev->child = gx_device_reloc_ptr(dev->child, gcst);
}
RELOC_PTRS_END

public_st_obj_filter_device();

const
gx_device_obj_filter gs_obj_filter_device =
{
    /*
     * Define the device as 8-bit gray scale to avoid computing halftones.
     */
    std_device_dci_type_body(gx_device_obj_filter, 0, "object_filter", &st_obj_filter_device,
                        MAX_COORD, MAX_COORD,
                        MAX_RESOLUTION, MAX_RESOLUTION,
                        1, 8, 255, 0, 256, 1),
    {default_subclass_open_device,
     default_subclass_get_initial_matrix,
     default_subclass_sync_output,			/* sync_output */
     default_subclass_output_page,
     default_subclass_close_device,
     default_subclass_map_rgb_color,
     default_subclass_map_color_rgb,
     obj_filter_fill_rectangle,
     obj_filter_tile_rectangle,			/* tile_rectangle */
     default_subclass_copy_mono,
     default_subclass_copy_color,
     obj_filter_draw_line,			/* draw_line */
     default_subclass_get_bits,			/* get_bits */
     default_subclass_get_params,
     default_subclass_put_params,
     default_subclass_map_cmyk_color,
     default_subclass_get_xfont_procs,			/* get_xfont_procs */
     default_subclass_get_xfont_device,			/* get_xfont_device */
     default_subclass_map_rgb_alpha_color,
     default_subclass_get_page_device,
     default_subclass_get_alpha_bits,			/* get_alpha_bits */
     default_subclass_copy_alpha,
     default_subclass_get_band,			/* get_band */
     default_subclass_copy_rop,			/* copy_rop */
     obj_filter_fill_path,
     obj_filter_stroke_path,
     obj_filter_fill_mask,
     obj_filter_fill_trapezoid,
     obj_filter_fill_parallelogram,
     obj_filter_fill_triangle,
     obj_filter_draw_thin_line,
     obj_filter_begin_image,
     obj_filter_image_data,			/* image_data */
     obj_filter_end_image,			/* end_image */
     obj_filter_strip_tile_rectangle,
     obj_filter_strip_copy_rop,
     default_subclass_get_clipping_box,			/* get_clipping_box */
     obj_filter_begin_typed_image,
     default_subclass_get_bits_rectangle,			/* get_bits_rectangle */
     default_subclass_map_color_rgb_alpha,
     default_subclass_create_compositor,
     default_subclass_get_hardware_params,			/* get_hardware_params */
     obj_filter_text_begin,
     default_subclass_finish_copydevice,			/* finish_copydevice */
     default_subclass_begin_transparency_group,			/* begin_transparency_group */
     default_subclass_end_transparency_group,			/* end_transparency_group */
     default_subclass_begin_transparency_mask,			/* begin_transparency_mask */
     default_subclass_end_transparency_mask,			/* end_transparency_mask */
     default_subclass_discard_transparency_layer,			/* discard_transparency_layer */
     default_subclass_get_color_mapping_procs,			/* get_color_mapping_procs */
     default_subclass_get_color_comp_index,			/* get_color_comp_index */
     default_subclass_encode_color,			/* encode_color */
     default_subclass_decode_color,			/* decode_color */
     default_subclass_pattern_manage,			/* pattern_manage */
     obj_filter_fill_rectangle_hl_color,			/* fill_rectangle_hl_color */
     default_subclass_include_color_space,			/* include_color_space */
     obj_filter_fill_linear_color_scanline,			/* fill_linear_color_scanline */
     obj_filter_fill_linear_color_trapezoid,			/* fill_linear_color_trapezoid */
     obj_filter_fill_linear_color_triangle,			/* fill_linear_color_triangle */
     default_subclass_update_spot_equivalent_colors,			/* update_spot_equivalent_colors */
     default_subclass_ret_devn_params,			/* ret_devn_params */
     default_subclass_fillpage,		/* fillpage */
     default_subclass_push_transparency_state,                      /* push_transparency_state */
     default_subclass_pop_transparency_state,                      /* pop_transparency_state */
     obj_filter_put_image,                      /* put_image */
     default_subclass_dev_spec_op,                      /* dev_spec_op */
     default_subclass_copy_planes,                      /* copy_planes */
     default_subclass_get_profile,                      /* get_profile */
     default_subclass_set_graphics_type_tag,                      /* set_graphics_type_tag */
     obj_filter_strip_copy_rop2,
     obj_filter_strip_tile_rect_devn,
     default_subclass_copy_alpha_hl_color,
     default_subclass_process_page,
     default_subclass_transform_pixel_region,
     obj_filter_fill_stroke_path,
    }
};

#undef MAX_COORD
#undef MAX_RESOLUTION

int obj_filter_fill_rectangle(gx_device *dev, int x, int y, int width, int height, gx_color_index color)
{
    if ((dev->ObjectFilter & FILTERVECTOR) == 0)
        return default_subclass_fill_rectangle(dev, x, y, width, height, color);
    return 0;
}

int obj_filter_tile_rectangle(gx_device *dev, const gx_tile_bitmap *tile, int x, int y, int width, int height,
    gx_color_index color0, gx_color_index color1,
    int phase_x, int phase_y)
{
    if ((dev->ObjectFilter & FILTERVECTOR) == 0)
        return default_subclass_tile_rectangle(dev, tile, x, y, width, height, color0, color1, phase_x, phase_y);
    return 0;
}

int obj_filter_draw_line(gx_device *dev, int x0, int y0, int x1, int y1, gx_color_index color)
{
    if ((dev->ObjectFilter & FILTERVECTOR) == 0)
        return default_subclass_draw_line(dev, x0, y0, x1, y1, color);
    return 0;
}

int obj_filter_fill_path(gx_device *dev, const gs_gstate *pgs, gx_path *ppath,
    const gx_fill_params *params,
    const gx_drawing_color *pdcolor, const gx_clip_path *pcpath)
{
    if ((dev->ObjectFilter & FILTERVECTOR) == 0)
        return default_subclass_fill_path(dev, pgs, ppath, params, pdcolor, pcpath);
    return 0;
}

int obj_filter_stroke_path(gx_device *dev, const gs_gstate *pgs, gx_path *ppath,
    const gx_stroke_params *params,
    const gx_drawing_color *pdcolor, const gx_clip_path *pcpath)
{
    if ((dev->ObjectFilter & FILTERVECTOR) == 0)
        return default_subclass_stroke_path(dev, pgs, ppath, params, pdcolor, pcpath);
    return 0;
}

int obj_filter_fill_stroke_path(gx_device *dev, const gs_gstate *pgs, gx_path *ppath,
        const gx_fill_params *fill_params, const gx_drawing_color *pdcolor_fill,
        const gx_stroke_params *stroke_params, const gx_drawing_color *pdcolor_stroke,
        const gx_clip_path *pcpath)
{
    if ((dev->ObjectFilter & FILTERVECTOR) == 0)
        return default_subclass_fill_stroke_path(dev, pgs, ppath, fill_params, pdcolor_fill, stroke_params, pdcolor_stroke, pcpath);
    return 0;
}

int obj_filter_fill_mask(gx_device *dev, const byte *data, int data_x, int raster, gx_bitmap_id id,
    int x, int y, int width, int height,
    const gx_drawing_color *pdcolor, int depth,
    gs_logical_operation_t lop, const gx_clip_path *pcpath)
{
    if ((dev->ObjectFilter & FILTERIMAGE) == 0)
        return default_subclass_fill_mask(dev, data, data_x, raster, id, x, y, width, height, pdcolor, depth, lop, pcpath);
    return 0;
}

int obj_filter_fill_trapezoid(gx_device *dev, const gs_fixed_edge *left, const gs_fixed_edge *right,
    fixed ybot, fixed ytop, bool swap_axes,
    const gx_drawing_color *pdcolor, gs_logical_operation_t lop)
{
    if ((dev->ObjectFilter & FILTERVECTOR) == 0)
        return default_subclass_fill_trapezoid(dev, left, right, ybot, ytop, swap_axes, pdcolor, lop);
    return 0;
}

int obj_filter_fill_parallelogram(gx_device *dev, fixed px, fixed py, fixed ax, fixed ay, fixed bx, fixed by,
    const gx_drawing_color *pdcolor, gs_logical_operation_t lop)
{
    if ((dev->ObjectFilter & FILTERVECTOR) == 0)
        return default_subclass_fill_parallelogram(dev, px, py, ax, ay, bx, by, pdcolor, lop);
    return 0;
}

int obj_filter_fill_triangle(gx_device *dev, fixed px, fixed py, fixed ax, fixed ay, fixed bx, fixed by,
    const gx_drawing_color *pdcolor, gs_logical_operation_t lop)
{
    if ((dev->ObjectFilter & FILTERVECTOR) == 0)
        return default_subclass_fill_triangle(dev, px, py, ax, ay, bx, by, pdcolor, lop);
    return 0;
}

int obj_filter_draw_thin_line(gx_device *dev, fixed fx0, fixed fy0, fixed fx1, fixed fy1,
    const gx_drawing_color *pdcolor, gs_logical_operation_t lop,
    fixed adjustx, fixed adjusty)
{
    if ((dev->ObjectFilter & FILTERVECTOR) == 0)
        return default_subclass_draw_thin_line(dev, fx0, fy0, fx1, fy1, pdcolor, lop, adjustx, adjusty);
    return 0;
}

int obj_filter_begin_image(gx_device *dev, const gs_gstate *pgs, const gs_image_t *pim,
    gs_image_format_t format, const gs_int_rect *prect,
    const gx_drawing_color *pdcolor, const gx_clip_path *pcpath,
    gs_memory_t *memory, gx_image_enum_common_t **pinfo)
{
    if ((dev->ObjectFilter & FILTERIMAGE) == 0)
        return default_subclass_begin_image(dev, pgs, pim, format, prect, pdcolor, pcpath, memory, pinfo);
    return 0;
}

int obj_filter_image_data(gx_device *dev, gx_image_enum_common_t *info, const byte **planes, int data_x,
    uint raster, int height)
{
    if ((dev->ObjectFilter & FILTERIMAGE) == 0)
        return default_subclass_image_data(dev, info, planes, data_x, raster, height);
    return 0;
}

int obj_filter_end_image(gx_device *dev, gx_image_enum_common_t *info, bool draw_last)
{
    if ((dev->ObjectFilter & FILTERIMAGE) == 0)
        return default_subclass_end_image(dev, info, draw_last);
    return 0;
}

int obj_filter_strip_tile_rectangle(gx_device *dev, const gx_strip_bitmap *tiles, int x, int y, int width, int height,
    gx_color_index color0, gx_color_index color1,
    int phase_x, int phase_y)
{
    if ((dev->ObjectFilter & FILTERIMAGE) == 0)
        return default_subclass_strip_tile_rectangle(dev, tiles, x, y, width, height, color0, color1, phase_x, phase_y);
    return 0;
}

int obj_filter_strip_copy_rop(gx_device *dev, const byte *sdata, int sourcex, uint sraster, gx_bitmap_id id,
    const gx_color_index *scolors,
    const gx_strip_bitmap *textures, const gx_color_index *tcolors,
    int x, int y, int width, int height,
    int phase_x, int phase_y, gs_logical_operation_t lop)
{
    if ((dev->ObjectFilter & FILTERIMAGE) == 0)
        return default_subclass_strip_copy_rop(dev, sdata, sourcex, sraster, id, scolors, textures, tcolors, x, y, width, height, phase_x, phase_y, lop);
    return 0;
}

typedef struct obj_filter_image_enum_s {
    gx_image_enum_common;
    int y;
    int height;
} obj_filter_image_enum;
gs_private_st_composite(st_obj_filter_image_enum, obj_filter_image_enum, "obj_filter_image_enum",
  obj_filter_image_enum_enum_ptrs, obj_filter_image_enum_reloc_ptrs);

static ENUM_PTRS_WITH(obj_filter_image_enum_enum_ptrs, obj_filter_image_enum *pie)
    (void)pie; /* Silence unused var warning */
    return ENUM_USING_PREFIX(st_gx_image_enum_common, 0);
ENUM_PTRS_END
static RELOC_PTRS_WITH(obj_filter_image_enum_reloc_ptrs, obj_filter_image_enum *pie)
{
    (void)pie; /* Silence unused var warning */
    RELOC_USING(st_gx_image_enum_common, vptr, size);
}
RELOC_PTRS_END

static int
obj_filter_image_plane_data(gx_image_enum_common_t * info,
                     const gx_image_plane_t * planes, int height,
                     int *rows_used)
{
    obj_filter_image_enum *pie = (obj_filter_image_enum *)info;

    if (height > pie->height - pie->y)
        height = pie->height - pie->y;

    pie->y += height;
    *rows_used = height;

    if (pie->y < pie->height)
        return 0;
    return 1;
}

static int
obj_filter_image_end_image(gx_image_enum_common_t * info, bool draw_last)
{
    return 0;
}

static const gx_image_enum_procs_t obj_filter_image_enum_procs = {
    obj_filter_image_plane_data,
    obj_filter_image_end_image
};

int obj_filter_begin_typed_image(gx_device *dev, const gs_gstate *pgs, const gs_matrix *pmat,
    const gs_image_common_t *pic, const gs_int_rect *prect,
    const gx_drawing_color *pdcolor, const gx_clip_path *pcpath,
    gs_memory_t *memory, gx_image_enum_common_t **pinfo)
{
    obj_filter_image_enum *pie;
    const gs_pixel_image_t *pim = (const gs_pixel_image_t *)pic;
    int num_components;

    if ((dev->ObjectFilter & FILTERIMAGE) == 0)
        return default_subclass_begin_typed_image(dev, pgs, pmat, pic, prect, pdcolor, pcpath, memory, pinfo);

    if (pic->type->index == 1) {
        const gs_image_t *pim1 = (const gs_image_t *)pic;

        if (pim1->ImageMask)
            num_components = 1;
        else
            num_components = gs_color_space_num_components(pim->ColorSpace);
    } else {
        num_components = gs_color_space_num_components(pim->ColorSpace);
    }

    pie = gs_alloc_struct(memory, obj_filter_image_enum, &st_obj_filter_image_enum,
                        "obj_filter_begin_image");
    if (pie == 0)
        return_error(gs_error_VMerror);
    memset(pie, 0, sizeof(*pie)); /* cleanup entirely for GC to work in all cases. */
    *pinfo = (gx_image_enum_common_t *) pie;
    gx_image_enum_common_init(*pinfo, (const gs_data_image_t *) pim, &obj_filter_image_enum_procs,
                        (gx_device *)dev, num_components, pim->format);
    pie->memory = memory;
    pie->skipping = true;
    pie->height = pim->Height;
    pie->y = 0;

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
static text_enum_proc_process(obj_filter_text_process);
static int
obj_filter_text_resync(gs_text_enum_t *pte, const gs_text_enum_t *pfrom)
{
    return 0;
}
int
obj_filter_text_process(gs_text_enum_t *pte)
{
    return 0;
}
static bool
obj_filter_text_is_width_only(const gs_text_enum_t *pte)
{
    return false;
}
static int
obj_filter_text_current_width(const gs_text_enum_t *pte, gs_point *pwidth)
{
    return 0;
}
static int
obj_filter_text_set_cache(gs_text_enum_t *pte, const double *pw,
                   gs_text_cache_control_t control)
{
    return 0;
}
static int
obj_filter_text_retry(gs_text_enum_t *pte)
{
    return 0;
}
static void
obj_filter_text_release(gs_text_enum_t *pte, client_name_t cname)
{
    gx_default_text_release(pte, cname);
}

static const gs_text_enum_procs_t obj_filter_text_procs = {
    obj_filter_text_resync, obj_filter_text_process,
    obj_filter_text_is_width_only, obj_filter_text_current_width,
    obj_filter_text_set_cache, obj_filter_text_retry,
    obj_filter_text_release
};

/* The device method which we do actually need to define. Either we are skipping the page,
 * in which case we create a text enumerator with our dummy procedures, or we are leaving it
 * up to the device, in which case we simply pass on the 'begin' method to the device.
 */
int obj_filter_text_begin(gx_device *dev, gs_gstate *pgs, const gs_text_params_t *text,
    gs_font *font, gx_path *path, const gx_device_color *pdcolor, const gx_clip_path *pcpath,
    gs_memory_t *memory, gs_text_enum_t **ppte)
{
    obj_filter_text_enum_t *penum;
    int code = 0;

    /* We don't want to simply ignore stringwidth for 2 reasons;
     * firstly because following elelments may be positioned based on the value returned
     * secondly  because op_show_restore executes an unconditional grestore, assuming
     * that a gsave has been done simply *because* its a tringwidth operation !
     */
    if ((text->operation & TEXT_DO_NONE) && (text->operation & TEXT_RETURN_WIDTH) && pgs->text_rendering_mode != 3)
        /* Note that the high level devices *must* be given the opportunity to 'see' the
         * stringwidth operation, or they won;t be able to cache the glyphs properly.
         * So always pass stringwidth operations to the child.
         */
        return default_subclass_text_begin(dev, pgs, text, font, path, pdcolor, pcpath, memory, ppte);

    if ((dev->ObjectFilter & FILTERTEXT) == 0)
        return default_subclass_text_begin(dev, pgs, text, font, path, pdcolor, pcpath, memory, ppte);

    rc_alloc_struct_1(penum, obj_filter_text_enum_t, &st_obj_filter_text_enum, memory,
                  return_error(gs_error_VMerror), "gdev_obj_filter_text_begin");
    penum->rc.free = rc_free_text_enum;
    code = gs_text_enum_init((gs_text_enum_t *)penum, &obj_filter_text_procs,
                         dev, pgs, text, font, path, pdcolor, pcpath, memory);
    if (code < 0) {
        gs_free_object(memory, penum, "gdev_obj_filter_text_begin");
        return code;
    }
    *ppte = (gs_text_enum_t *)penum;

    return 0;
}

int obj_filter_fill_rectangle_hl_color(gx_device *dev, const gs_fixed_rect *rect,
        const gs_gstate *pgs, const gx_drawing_color *pdcolor, const gx_clip_path *pcpath)
{
    if ((dev->ObjectFilter & FILTERVECTOR) == 0)
        return default_subclass_fill_rectangle_hl_color(dev, rect, pgs, pdcolor, pcpath);
    return 0;
}

int obj_filter_fill_linear_color_scanline(gx_device *dev, const gs_fill_attributes *fa,
        int i, int j, int w, const frac31 *c0, const int32_t *c0_f, const int32_t *cg_num,
        int32_t cg_den)
{
    if ((dev->ObjectFilter & FILTERVECTOR) == 0)
        return default_subclass_fill_linear_color_scanline(dev, fa, i, j, w, c0, c0_f, cg_num, cg_den);
    return 0;
}

int obj_filter_fill_linear_color_trapezoid(gx_device *dev, const gs_fill_attributes *fa,
        const gs_fixed_point *p0, const gs_fixed_point *p1,
        const gs_fixed_point *p2, const gs_fixed_point *p3,
        const frac31 *c0, const frac31 *c1,
        const frac31 *c2, const frac31 *c3)
{
    if ((dev->ObjectFilter & FILTERVECTOR) == 0)
        return default_subclass_fill_linear_color_trapezoid(dev, fa, p0, p1, p2, p3, c0, c1, c2, c3);
    return 0;
}

int obj_filter_fill_linear_color_triangle(gx_device *dev, const gs_fill_attributes *fa,
        const gs_fixed_point *p0, const gs_fixed_point *p1,
        const gs_fixed_point *p2, const frac31 *c0, const frac31 *c1, const frac31 *c2)
{
    if ((dev->ObjectFilter & FILTERVECTOR) == 0)
        return default_subclass_fill_linear_color_triangle(dev, fa, p0, p1, p2, c0, c1, c2);
    return 0;
}

int obj_filter_put_image(gx_device *dev, gx_device *mdev, const byte **buffers, int num_chan, int x, int y,
            int width, int height, int row_stride,
            int alpha_plane_index, int tag_plane_index)
{
    if ((dev->ObjectFilter & FILTERIMAGE) == 0)
        return default_subclass_put_image(dev, mdev, buffers, num_chan, x, y, width, height, row_stride, alpha_plane_index, tag_plane_index);
    return 0;
}

int obj_filter_strip_copy_rop2(gx_device *dev, const byte *sdata, int sourcex, uint sraster, gx_bitmap_id id,
    const gx_color_index *scolors, const gx_strip_bitmap *textures, const gx_color_index *tcolors,
    int x, int y, int width, int height, int phase_x, int phase_y, gs_logical_operation_t lop, uint planar_height)
{
    if ((dev->ObjectFilter & FILTERIMAGE) == 0)
        return default_subclass_strip_copy_rop2(dev, sdata, sourcex, sraster, id, scolors, textures, tcolors, x, y, width, height, phase_x, phase_y, lop, planar_height);
    return 0;
}

int obj_filter_strip_tile_rect_devn(gx_device *dev, const gx_strip_bitmap *tiles, int x, int y, int width, int height,
    const gx_drawing_color *pdcolor0, const gx_drawing_color *pdcolor1, int phase_x, int phase_y)
{
    if ((dev->ObjectFilter & FILTERIMAGE) == 0)
        return default_subclass_strip_tile_rect_devn(dev, tiles, x, y, width, height, pdcolor0, pdcolor1, phase_x, phase_y);
    return 0;
}
