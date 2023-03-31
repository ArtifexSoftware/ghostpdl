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
#include "gximag3x.h"
#include "gdevsclass.h"
#include "gdevoflt.h"
#include "gximag3x.h"

int gs_is_pdf14trans_compositor(const gs_composite_t * pct);

/* GC descriptor */
public_st_device_obj_filter();
/* we need text and image enumerators, because of the crazy way that text and images work */
private_st_obj_filter_text_enum();

/* Device procedures, we need to implement all of them */
static dev_proc_fill_rectangle(obj_filter_fill_rectangle);
static dev_proc_fill_path(obj_filter_fill_path);
static dev_proc_stroke_path(obj_filter_stroke_path);
static dev_proc_fill_mask(obj_filter_fill_mask);
static dev_proc_fill_trapezoid(obj_filter_fill_trapezoid);
static dev_proc_fill_parallelogram(obj_filter_fill_parallelogram);
static dev_proc_fill_triangle(obj_filter_fill_triangle);
static dev_proc_draw_thin_line(obj_filter_draw_thin_line);
static dev_proc_strip_tile_rectangle(obj_filter_strip_tile_rectangle);
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

static void
obj_filter_initialize_device_procs(gx_device *dev)
{
    default_subclass_initialize_device_procs(dev);

    set_dev_proc(dev, fill_rectangle, obj_filter_fill_rectangle);
    set_dev_proc(dev, fill_path, obj_filter_fill_path);
    set_dev_proc(dev, stroke_path, obj_filter_stroke_path);
    set_dev_proc(dev, fill_mask, obj_filter_fill_mask);
    set_dev_proc(dev, fill_trapezoid, obj_filter_fill_trapezoid);
    set_dev_proc(dev, fill_parallelogram, obj_filter_fill_parallelogram);
    set_dev_proc(dev, fill_triangle, obj_filter_fill_triangle);
    set_dev_proc(dev, draw_thin_line, obj_filter_draw_thin_line);
    set_dev_proc(dev, strip_tile_rectangle, obj_filter_strip_tile_rectangle);
    set_dev_proc(dev, begin_typed_image, obj_filter_begin_typed_image);
    set_dev_proc(dev, text_begin, obj_filter_text_begin);
    set_dev_proc(dev, fill_rectangle_hl_color, obj_filter_fill_rectangle_hl_color);
    set_dev_proc(dev, fill_linear_color_scanline, obj_filter_fill_linear_color_scanline);
    set_dev_proc(dev, fill_linear_color_trapezoid, obj_filter_fill_linear_color_trapezoid);
    set_dev_proc(dev, fill_linear_color_triangle, obj_filter_fill_linear_color_triangle);
    set_dev_proc(dev, put_image, obj_filter_put_image);
    set_dev_proc(dev, strip_copy_rop2, obj_filter_strip_copy_rop2);
    set_dev_proc(dev, strip_tile_rect_devn, obj_filter_strip_tile_rect_devn);
    set_dev_proc(dev, fill_stroke_path, obj_filter_fill_stroke_path);
    set_dev_proc(dev, composite, default_subclass_composite_front);
}

const
gx_device_obj_filter gs_obj_filter_device =
{
    /*
     * Define the device as 8-bit gray scale to avoid computing halftones.
     */
    std_device_dci_type_body_sc(gx_device_obj_filter,
                        obj_filter_initialize_device_procs,
                        "object_filter", &st_obj_filter_device,
                        MAX_COORD, MAX_COORD,
                        MAX_RESOLUTION, MAX_RESOLUTION,
                        1, 8, 255, 0, 256, 1, NULL, NULL, NULL)
};

#undef MAX_COORD
#undef MAX_RESOLUTION

int obj_filter_fill_rectangle(gx_device *dev, int x, int y, int width, int height, gx_color_index color)
{
    if ((dev->ObjectFilter & FILTERVECTOR) == 0)
        return default_subclass_fill_rectangle(dev, x, y, width, height, color);
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

int obj_filter_strip_tile_rectangle(gx_device *dev, const gx_strip_bitmap *tiles, int x, int y, int width, int height,
    gx_color_index color0, gx_color_index color1,
    int phase_x, int phase_y)
{
    if ((dev->ObjectFilter & FILTERIMAGE) == 0)
        return default_subclass_strip_tile_rectangle(dev, tiles, x, y, width, height, color0, color1, phase_x, phase_y);
    return 0;
}

typedef struct obj_filter_image_enum_s {
    gx_image_enum_common;
    int y, mask_y;
    int height, mask_height;
    int type;
    int InterleaveType;
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

    if (pie->type == 3 && pie->InterleaveType == interleave_separate_source) {
        pie->y += height;
        pie->mask_y += height;
        *rows_used = height;

        if (pie->y < pie->height || pie->mask_y < pie->mask_height)
            return 0;
        return 1;
    } else {
        if (height > pie->height - pie->y)
            height = pie->height - pie->y;

        pie->y += height;
        *rows_used = height;

        if (pie->y < pie->height)
            return 0;
        return 1;
    }
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
    pie->mask_y = pie->y = 0;
    pie->type = pic->type->index;

    if (pic->type->index == 3) {
        const gs_image3_t *pim = (const gs_image3_t *)pic;

        switch (pim->InterleaveType)
        {
            case interleave_chunky:
                /* Add the mask data to the depth of the image data. */
                pie->num_planes = 1;
                break;
            case interleave_scan_lines:
                /*
                 * There is only 1 plane, with dynamically changing width & depth.
                 * Initialize it for the mask data, since that is what will be
                 * read first.
                 */
                pie->num_planes = 1;
                pie->plane_depths[0] = 1;
                pie->plane_widths[0] = pim->MaskDict.Width;
                break;
            case interleave_separate_source:
                /* Insert the mask data as a separate plane before the image data. */
                pie->num_planes = 2;
                pie->plane_depths[1] = pie->plane_depths[0];
                pie->plane_widths[1] = pie->plane_widths[0];
                pie->plane_widths[0] = pim->MaskDict.Width;
                pie->plane_depths[0] = 1;
                pie->mask_height = pim->MaskDict.Height;
                break;
        }
        pie->InterleaveType = pim->InterleaveType;
    }
    if (pic->type->index == IMAGE3X_IMAGETYPE) {
        const gs_image3x_t *pim = (const gs_image3x_t *)pic;

        if (pim->Opacity.MaskDict.BitsPerComponent != 0) {
            switch(pim->Opacity.InterleaveType) {
            case interleave_separate_source:
                pie->num_planes++;
                pie->plane_depths[1] = pie->plane_depths[0];
                pie->plane_widths[1] = pie->plane_widths[0];
                pie->plane_depths[0] = pim->Opacity.MaskDict.BitsPerComponent;
                pie->plane_widths[0] = pim->Opacity.MaskDict.Width;
                break;
            case interleave_chunky:
                pie->plane_depths[0] += pim->BitsPerComponent;
                break;
            default:		/* can't happen */
                return_error(gs_error_Fatal);
            }
        }
        if (pim->Shape.MaskDict.BitsPerComponent != 0) {
            switch(pim->Shape.InterleaveType) {
            case interleave_separate_source:
                pie->num_planes++;
                pie->plane_depths[1] = pie->plane_depths[0];
                pie->plane_widths[1] = pie->plane_widths[0];
                pie->plane_depths[0] = pim->Shape.MaskDict.BitsPerComponent;
                pie->plane_widths[0] = pim->Shape.MaskDict.Width;
                break;
            case interleave_chunky:
                pie->plane_depths[0] += pim->BitsPerComponent;
                break;
            default:		/* can't happen */
                return_error(gs_error_Fatal);
            }
        }
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
    gs_font *font, const gx_clip_path *pcpath,
    gs_text_enum_t **ppte)
{
    obj_filter_text_enum_t *penum;
    int code = 0;
    gs_memory_t * memory = pgs->memory;

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
        return default_subclass_text_begin(dev, pgs, text, font, pcpath, ppte);

    if ((dev->ObjectFilter & FILTERTEXT) == 0)
        return default_subclass_text_begin(dev, pgs, text, font, pcpath, ppte);

    rc_alloc_struct_1(penum, obj_filter_text_enum_t, &st_obj_filter_text_enum, memory,
                  return_error(gs_error_VMerror), "gdev_obj_filter_text_begin");
    penum->rc.free = rc_free_text_enum;
    code = gs_text_enum_init((gs_text_enum_t *)penum, &obj_filter_text_procs,
                         dev, pgs, text, font, pcpath, memory);
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
