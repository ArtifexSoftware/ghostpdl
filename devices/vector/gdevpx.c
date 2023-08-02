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


/* H-P PCL XL driver */
#include "math_.h"
#include "memory_.h"
#include "gx.h"
#include "gserrors.h"
#include "gsccolor.h"
#include "gsdcolor.h"
#include "gxiparam.h"
#include "gxcspace.h"           /* for color mapping for images */
#include "gxdevice.h"
#include "gxpath.h"
#include "gdevvec.h"
#include "gdevmrop.h"
#include "strimpl.h"
#include "srlx.h"
#include "jpeglib_.h"
#include "sdct.h"
#include "sjpeg.h"
#include "gdevpxat.h"
#include "gdevpxen.h"
#include "gdevpxop.h"
#include "gdevpxut.h"
#include "gxlum.h"
#include "gdevpcl.h"            /* for gdev_pcl_mode3compress() */
#include "gsicc_manage.h"
#include "gsicc_cache.h"
#include <stdlib.h>             /* abs() */

#include "gxfcache.h"
#include "gxfont.h"

/* ---------------- Device definition ---------------- */

/* Define the default resolution. */
#ifndef X_DPI
#  define X_DPI 600
#endif
#ifndef Y_DPI
#  define Y_DPI 600
#endif

/* Structure definition */
#define NUM_POINTS 40           /* must be >= 3 and <= 255 */
typedef enum {
    POINTS_NONE,
    POINTS_LINES,
    POINTS_CURVES
} point_type_t;
typedef struct gx_device_pclxl_s {
    gx_device_vector_common;
    /* Additional state information */
    pxeMediaSize_t media_size;
    bool ManualFeed;            /* map ps setpage commands to pxl */
    bool ManualFeed_set;
    int MediaPosition_old;      /* old Position attribute - for duplex detection */
    int MediaPosition;          /* MediaPosition attribute */
    int MediaPosition_set;
    char MediaType_old[64];     /* old MediaType attribute - for duplex detection */
    char MediaType[64];         /* MediaType attribute */
    int MediaType_set;
    int page;                   /* Page number starting at 0 */
    bool Duplex;                /* Duplex attribute */
    bool Staple;                /* Staple attribute */
    bool Tumble;                /* Tumble attribute */
    gx_path_type_t fill_rule;   /* ...winding_number or ...even_odd  */
    gx_path_type_t clip_rule;   /* ditto */
    pxeColorSpace_t color_space;
    struct pal_ {
        int size;               /* # of bytes */
        byte data[256 * 3];     /* up to 8-bit samples */
    } palette;
    struct pts_ {
        /* buffer for accumulating path points */
        gs_int_point current;   /* current point as of start of data */
        point_type_t type;
        int count;
        gs_int_point data[NUM_POINTS];
    } points;
    struct ch_ {
    /* cache for downloaded characters */
#define MAX_CACHED_CHARS 400
#define MAX_CHAR_DATA 500000
#define MAX_CHAR_SIZE 5000
#define CHAR_HASH_FACTOR 247
        ushort table[MAX_CACHED_CHARS * 3 / 2];
        struct cd_ {
            gs_id id;           /* key */
            uint size;
        } data[MAX_CACHED_CHARS];
        int next_in;            /* next data element to fill in */
        int next_out;           /* next data element to discard */
        int count;              /* of occupied data elements */
        ulong used;
    } chars;
    bool font_set;
    int state_rotated;          /* 0, 1, 2, -1, mutiple of 90 deg */
    int CompressMode;           /* std PXL enum: None=0, RLE=1, JPEG=2, DeltaRow=3 */
    bool scaled;
    double x_scale;             /* chosen so that max(x) is scaled to 0x7FFF, to give max distinction between x values */
    double y_scale;
    bool pen_null;
    bool brush_null;
    bool iccTransform;
} gx_device_pclxl;

gs_public_st_suffix_add0_final(st_device_pclxl, gx_device_pclxl,
                               "gx_device_pclxl",
                               device_pclxl_enum_ptrs,
                               device_pclxl_reloc_ptrs, gx_device_finalize,
                               st_device_vector);

#define pclxl_device_body(dname, depth, init)\
  std_device_dci_type_body(gx_device_pclxl, init, dname, &st_device_pclxl,\
                           DEFAULT_WIDTH_10THS * X_DPI / 10,\
                           DEFAULT_HEIGHT_10THS * Y_DPI / 10,\
                           X_DPI, Y_DPI,\
                           (depth > 8 ? 3 : 1), depth,\
                           (depth > 1 ? 255 : 1), (depth > 8 ? 255 : 0),\
                           (depth > 1 ? 256 : 2), (depth > 8 ? 256 : 1))

/* Driver procedures */
static dev_proc_open_device(pclxl_open_device);
static dev_proc_output_page(pclxl_output_page);
static dev_proc_close_device(pclxl_close_device);
static dev_proc_copy_mono(pclxl_copy_mono);
static dev_proc_copy_color(pclxl_copy_color);
static dev_proc_fill_mask(pclxl_fill_mask);

static dev_proc_get_params(pclxl_get_params);
static dev_proc_put_params(pclxl_put_params);
static dev_proc_text_begin(pclxl_text_begin);

/*static dev_proc_draw_thin_line(pclxl_draw_thin_line); */
static dev_proc_begin_typed_image(pclxl_begin_typed_image);
static dev_proc_strip_copy_rop2(pclxl_strip_copy_rop2);

static void
pclxl_initialize_device_procs(gx_device *dev,
                              dev_proc_map_rgb_color(map_rgb_color),
                              dev_proc_map_color_rgb(map_color_rgb))
{
    set_dev_proc(dev, open_device, pclxl_open_device);
    set_dev_proc(dev, output_page, pclxl_output_page);
    set_dev_proc(dev, close_device, pclxl_close_device);
    set_dev_proc(dev, map_rgb_color, map_rgb_color);
    set_dev_proc(dev, map_color_rgb, map_color_rgb);
    set_dev_proc(dev, fill_rectangle, gdev_vector_fill_rectangle);
    set_dev_proc(dev, copy_mono, pclxl_copy_mono);
    set_dev_proc(dev, copy_color, pclxl_copy_color);
    set_dev_proc(dev, get_params, pclxl_get_params);
    set_dev_proc(dev, put_params, pclxl_put_params);
    set_dev_proc(dev, get_page_device, gx_page_device_get_page_device);
    set_dev_proc(dev, fill_path, gdev_vector_fill_path);
    set_dev_proc(dev, stroke_path, gdev_vector_stroke_path);
    set_dev_proc(dev, fill_mask, pclxl_fill_mask);
    set_dev_proc(dev, fill_trapezoid, gdev_vector_fill_trapezoid);
    set_dev_proc(dev, fill_parallelogram, gdev_vector_fill_parallelogram);
    set_dev_proc(dev, fill_triangle, gdev_vector_fill_triangle);
    set_dev_proc(dev, begin_typed_image, pclxl_begin_typed_image);
    set_dev_proc(dev, strip_copy_rop2, pclxl_strip_copy_rop2);
    set_dev_proc(dev, text_begin, pclxl_text_begin);
}

static void
pxlmono_initialize_device_procs(gx_device *dev)
{
    pclxl_initialize_device_procs(dev,
                                  gx_default_gray_map_rgb_color,
                                  gx_default_gray_map_color_rgb);
}

static void
pxlcolor_initialize_device_procs(gx_device *dev)
{
    pclxl_initialize_device_procs(dev,
                                  gx_default_rgb_map_rgb_color,
                                  gx_default_rgb_map_color_rgb);
}

const gx_device_pclxl gs_pxlmono_device = {
    pclxl_device_body("pxlmono", 8, pxlmono_initialize_device_procs)
};

const gx_device_pclxl gs_pxlcolor_device = {
    pclxl_device_body("pxlcolor", 24, pxlcolor_initialize_device_procs)
};

/* ---------------- Other utilities ---------------- */

static inline stream *
pclxl_stream(gx_device_pclxl * xdev)
{
    return gdev_vector_stream((gx_device_vector *) xdev);
}

/* Initialize for a page. */
static void
pclxl_page_init(gx_device_pclxl * xdev)
{
    gdev_vector_init((gx_device_vector *) xdev);
    xdev->in_page = false;
    xdev->fill_rule = gx_path_type_winding_number;
    xdev->clip_rule = gx_path_type_winding_number;
    xdev->color_space = eNoColorSpace;
    xdev->palette.size = 0;
    xdev->font_set = false;
    xdev->state_rotated = 0;
    xdev->scaled = false;
    xdev->x_scale = 1;
    xdev->y_scale = 1;
    xdev->pen_null = false;
    xdev->brush_null = false;
}

/* Test whether a RGB color is actually a gray shade. */
#define RGB_IS_GRAY(ci) ((ci) >> 8 == ((ci) & 0xffff))

/* Set the color space and (optionally) palette. */
static void
pclxl_set_color_space(gx_device_pclxl * xdev, pxeColorSpace_t color_space)
{
    if (xdev->color_space != color_space) {
        stream *s = pclxl_stream(xdev);

        px_put_ub(s, (byte) color_space);
        px_put_ac(s, pxaColorSpace, pxtSetColorSpace);
        xdev->color_space = color_space;
        xdev->palette.size = 0; /* purge the cached palette */
    }
}
static void
pclxl_set_color_palette(gx_device_pclxl * xdev, pxeColorSpace_t color_space,
                        const byte * palette, uint palette_size)
{
    if (xdev->color_space != color_space ||
        xdev->palette.size != palette_size ||
        memcmp(xdev->palette.data, palette, palette_size)
        ) {
        stream *s = pclxl_stream(xdev);

        static const byte csp_[] = {
            DA(pxaColorSpace),
            DUB(e8Bit), DA(pxaPaletteDepth),
            pxt_ubyte_array
        };

        px_put_ub(s, (byte) color_space);
        PX_PUT_LIT(s, csp_);
        px_put_u(s, palette_size);
        px_put_bytes(s, palette, palette_size);
        px_put_ac(s, pxaPaletteData, pxtSetColorSpace);
        xdev->color_space = color_space;
        xdev->palette.size = palette_size;
        memcpy(xdev->palette.data, palette, palette_size);
    }
}

/* For caching either NullPen or NullBrush, which happens a lot for
 * drawing masks in the PS3 CET test set.
 *
 * The expected null_source/op combos are:
 * pxaNullPen/pxtSetPenSource and pxaNullBrush/pxtSetBrushSource
 */
static int
pclxl_set_cached_nulls(gx_device_pclxl * xdev, px_attribute_t null_source,
                       px_tag_t op)
{
    stream *s = pclxl_stream(xdev);

    if (op == pxtSetPenSource) {
        if (xdev->pen_null)
            return 0;
        else
            xdev->pen_null = true;
    }
    if (op == pxtSetBrushSource) {
        if (xdev->brush_null)
            return 0;
        else
            xdev->brush_null = true;
    }
    px_put_uba(s, 0, (byte) null_source);
    spputc(s, (byte) op);
    return 0;
}

/* Set a drawing RGB color. */
static int
pclxl_set_color(gx_device_pclxl * xdev, const gx_drawing_color * pdc,
                px_attribute_t null_source, px_tag_t op)
{
    stream *s = pclxl_stream(xdev);

    if (gx_dc_is_pure(pdc)) {
        gx_color_index color = gx_dc_pure_color(pdc);

        if (op == pxtSetPenSource)
            xdev->pen_null = false;
        if (op == pxtSetBrushSource)
            xdev->brush_null = false;

        if (xdev->color_info.num_components == 1 || RGB_IS_GRAY(color)) {
            pclxl_set_color_space(xdev, eGray);
            px_put_uba(s, (byte) color, pxaGrayLevel);
        } else {
            pclxl_set_color_space(xdev, eRGB);
            spputc(s, pxt_ubyte_array);
            px_put_ub(s, 3);
            spputc(s, (byte) (color >> 16));
            spputc(s, (byte) (color >> 8));
            spputc(s, (byte) color);
            px_put_a(s, pxaRGBColor);
        }
    } else if (gx_dc_is_null(pdc) || !color_is_set(pdc)) {
        if (op == pxtSetPenSource || op == pxtSetBrushSource)
            return pclxl_set_cached_nulls(xdev, null_source, op);
        else
            px_put_uba(s, 0, null_source);
    } else
        return_error(gs_error_rangecheck);
    spputc(s, (byte) op);
    return 0;
}

/* Test whether we can handle a given color space in an image. */
/* We cannot handle ICCBased color spaces. */
static bool
pclxl_can_handle_color_space(const gs_color_space * pcs)
{
    gs_color_space_index index;

    /* an image with no colorspace info arrived; cannot handle */
    if (!pcs)
        return false;
    index = gs_color_space_get_index(pcs);

    if (index == gs_color_space_index_Indexed) {
        if (pcs->params.indexed.use_proc)
            return false;
        index =
            gs_color_space_get_index(gs_color_space_indexed_base_space(pcs));
    } else if (index == gs_color_space_index_ICC) {
        index = gsicc_get_default_type(pcs->cmm_icc_profile_data);
        return ((index < gs_color_space_index_DevicePixel) ? true : false);
    }

    return !(index == gs_color_space_index_Separation ||
             index == gs_color_space_index_Pattern ||
             index == gs_color_space_index_DeviceN ||
             index == gs_color_space_index_ICC ||
             (index <= gs_color_space_index_CIEA &&
             index >= gs_color_space_index_CIEDEFG)
             );
}

/* Test whether we can icclink-transform an image. */
static bool
pclxl_can_icctransform(const gs_image_t * pim)
{
    const gs_color_space *pcs = pim->ColorSpace;
    int bits_per_pixel;

    /* an image with no colorspace info arrived; cannot transform */
    if (!pcs)
        return false;
    bits_per_pixel =
        (pim->ImageMask ? 1 :
         pim->BitsPerComponent * gs_color_space_num_components(pcs));

    if ((gs_color_space_get_index(pcs) == gs_color_space_index_ICC)
        && (bits_per_pixel == 24 || bits_per_pixel == 32))
        return true;

    return false;
}

/*
 * Avoid PXL high level images if a transfer function has been set.
 * Allow the graphics library to render to a lower level
 * representation with the function applied to the colors.
 */

static bool
pclxl_nontrivial_transfer(const gs_gstate * pgs)
{
    gx_transfer_map *red = pgs->set_transfer.red;
    gx_transfer_map *green = pgs->set_transfer.green;
    gx_transfer_map *blue = pgs->set_transfer.blue;

    return (red || green || blue);

}
/* Set brush, pen, and mode for painting a path. */
static void
pclxl_set_paints(gx_device_pclxl * xdev, gx_path_type_t type)
{
    stream *s = pclxl_stream(xdev);
    gx_path_type_t rule = type & gx_path_type_rule;

    if (!(type & gx_path_type_fill) &&
        (color_is_set(&xdev->saved_fill_color.saved_dev_color) ||
         !gx_dc_is_null(&xdev->saved_fill_color.saved_dev_color)
        )
        ) {
        pclxl_set_cached_nulls(xdev, pxaNullBrush, pxtSetBrushSource);
        color_set_null(&xdev->saved_fill_color.saved_dev_color);
        if (rule != xdev->fill_rule) {
            px_put_ub(s, (byte) (rule == gx_path_type_even_odd ? eEvenOdd :
                                 eNonZeroWinding));
            px_put_ac(s, pxaFillMode, pxtSetFillMode);
            xdev->fill_rule = rule;
        }
    }
    if (!(type & gx_path_type_stroke) &&
        (color_is_set(&xdev->saved_stroke_color.saved_dev_color) ||
         !gx_dc_is_null(&xdev->saved_stroke_color.saved_dev_color)
        )
        ) {
        pclxl_set_cached_nulls(xdev, pxaNullPen, pxtSetPenSource);
        color_set_null(&xdev->saved_stroke_color.saved_dev_color);
    }
}

static void
pclxl_set_page_origin(stream * s, int x, int y)
{
    px_put_ssp(s, x, y);
    px_put_ac(s, pxaPageOrigin, pxtSetPageOrigin);
    return;
}

static void
pclxl_set_page_scale(gx_device_pclxl * xdev, double x_scale, double y_scale)
{
    stream *s = pclxl_stream(xdev);

    if (xdev->scaled) {
        xdev->x_scale = x_scale;
        xdev->y_scale = y_scale;
        px_put_rp(s, x_scale, y_scale);
        px_put_ac(s, pxaPageScale, pxtSetPageScale);
    }
    return;
}

static void
pclxl_unset_page_scale(gx_device_pclxl * xdev)
{
    stream *s = pclxl_stream(xdev);

    if (xdev->scaled) {
        px_put_rp(s, 1 / xdev->x_scale, 1 / xdev->y_scale);
        px_put_ac(s, pxaPageScale, pxtSetPageScale);
        xdev->scaled = false;
        xdev->x_scale = 1;
        xdev->y_scale = 1;
    }
    return;
}

/* Set the cursor. */
static int
pclxl_set_cursor(gx_device_pclxl * xdev, int x, int y)
{
    stream *s = pclxl_stream(xdev);
    double x_scale = 1;
    double y_scale = 1;

    /* Points must be one of ubyte/uint16/sint16;
       Here we play with PageScale (one of ubyte/uint16/real32_xy) to go higher.
       This gives us 32768 x 3.4e38 in UnitsPerMeasure.
       If we ever need to go higher, we play with UnitsPerMeasure. */
    if (abs(x) > 0x7FFF) {
        x_scale = ((double)abs(x)) / 0x7FFF;
        x = (x > 0 ? 0x7FFF : -0x7FFF);
        xdev->scaled = true;
    }
    if (abs(y) > 0x7FFF) {
        y_scale = ((double)abs(y)) / 0x7FFF;
        y = (y > 0 ? 0x7FFF : -0x7FFF);
        xdev->scaled = true;
    }
    pclxl_set_page_scale(xdev, x_scale, y_scale);
    px_put_ssp(s, x, y);
    px_put_ac(s, pxaPoint, pxtSetCursor);
    pclxl_unset_page_scale(xdev);
    return 0;
}

/* ------ Paths ------ */

/* Flush any buffered path points. */
static void
px_put_np(stream * s, int count, pxeDataType_t dtype)
{
    px_put_uba(s, (byte) count, pxaNumberOfPoints);
    px_put_uba(s, (byte) dtype, pxaPointType);
}
static int
pclxl_flush_points(gx_device_pclxl * xdev)
{
    int count = xdev->points.count;

    if (count) {
        stream *s = pclxl_stream(xdev);
        px_tag_t op;
        int x = xdev->points.current.x, y = xdev->points.current.y;
        int uor = 0, sor = 0;
        pxeDataType_t data_type;
        int i, di;
        byte diffs[NUM_POINTS * 2];
        double x_scale = 1;
        double y_scale = 1;
        int temp_origin_x = 0, temp_origin_y = 0;
        int count_smalls = 0;

        if (xdev->points.type != POINTS_NONE) {
            for (i = 0; i < count; ++i) {
                if ((abs(xdev->points.data[i].x) > 0x7FFF)
                    || (abs(xdev->points.data[i].y) > 0x7FFF))
                    xdev->scaled = true;
                if ((abs(xdev->points.data[i].x) < 0x8000)
                    && (abs(xdev->points.data[i].y) < 0x8000)) {
                    if ((temp_origin_x != xdev->points.data[i].x)
                        || (temp_origin_y != xdev->points.data[i].y)) {
                        temp_origin_x = xdev->points.data[i].x;
                        temp_origin_y = xdev->points.data[i].y;
                        count_smalls++;
                    }
                }
            }
            if (xdev->scaled) {
                /* if there are some points with small co-ordinates, we set origin to it
                   before scaling, an unset afterwards. This works around problems
                   for small co-ordinates being moved snapped to 32767 x 32767 grid points;
                   if there are more than 1, the other points
                   will be in-accurate, unfortunately */
                if (count_smalls) {
                    pclxl_set_page_origin(s, temp_origin_x, temp_origin_y);
                }
                for (i = 0; i < count; ++i) {
                    x_scale = max(((double)
                                   abs(xdev->points.data[i].x -
                                       temp_origin_x)) / 0x7FFF, x_scale);
                    y_scale = max(((double)
                                   abs(xdev->points.data[i].y -
                                       temp_origin_y)) / 0x7FFF, y_scale);
                }
                for (i = 0; i < count; ++i) {
                    xdev->points.data[i].x =
                        (int)((xdev->points.data[i].x -
                               temp_origin_x) / x_scale + 0.5);
                    xdev->points.data[i].y =
                        (int)((xdev->points.data[i].y -
                               temp_origin_y) / y_scale + 0.5);
                }
                x = (int)((x - temp_origin_x) / x_scale + 0.5);
                y = (int)((y - temp_origin_y) / y_scale + 0.5);
                pclxl_set_page_scale(xdev, x_scale, y_scale);
            } else {
                /* don't reset origin if we did not scale */
                count_smalls = 0;
            }
        }
        /*
         * Writing N lines using a point list requires 11 + 4*N or 11 +
         * 2*N bytes, as opposed to 8*N bytes using separate commands;
         * writing N curves requires 11 + 12*N or 11 + 6*N bytes
         * vs. 22*N.  So it's always shorter to write curves with a
         * list (except for N = 1 with full-size coordinates, but since
         * the difference is only 1 byte, we don't bother to ever use
         * the non-list form), but lines are shorter only if N >= 3
         * (again, with a 1-byte difference if N = 2 and byte
         * coordinates).
         */
        switch (xdev->points.type) {
            case POINTS_NONE:
                return 0;
            case POINTS_LINES:
                op = pxtLinePath;
                if (count < 3) {
                    for (i = 0; i < count; ++i) {
                        px_put_ssp(s, xdev->points.data[i].x,
                                   xdev->points.data[i].y);
                        px_put_a(s, pxaEndPoint);
                        spputc(s, (byte) op);
                    }
                    pclxl_unset_page_scale(xdev);
                    if (count_smalls)
                        pclxl_set_page_origin(s, -temp_origin_x,
                                              -temp_origin_y);
                    goto zap;
                }
                /* See if we can use byte values. */
                for (i = di = 0; i < count; ++i, di += 2) {
                    int dx = xdev->points.data[i].x - x;
                    int dy = xdev->points.data[i].y - y;

                    diffs[di] = (byte) dx;
                    diffs[di + 1] = (byte) dy;
                    uor |= dx | dy;
                    sor |= (dx + 0x80) | (dy + 0x80);
                    x += dx, y += dy;
                }
                if (!(uor & ~0xff))
                    data_type = eUByte;
                else if (!(sor & ~0xff))
                    data_type = eSByte;
                else
                    break;
                op = pxtLineRelPath;
                /* Use byte values. */
              useb:px_put_np(s, count, data_type);
                spputc(s, (byte) op);
                px_put_data_length(s, count * 2);       /* 2 bytes per point */
                px_put_bytes(s, diffs, count * 2);
                pclxl_unset_page_scale(xdev);
                if (count_smalls)
                    pclxl_set_page_origin(s, -temp_origin_x, -temp_origin_y);
                goto zap;
            case POINTS_CURVES:
                op = pxtBezierPath;
                /* See if we can use byte values. */
                for (i = di = 0; i < count; i += 3, di += 6) {
                    int dx1 = xdev->points.data[i].x - x;
                    int dy1 = xdev->points.data[i].y - y;
                    int dx2 = xdev->points.data[i + 1].x - x;
                    int dy2 = xdev->points.data[i + 1].y - y;
                    int dx = xdev->points.data[i + 2].x - x;
                    int dy = xdev->points.data[i + 2].y - y;

                    diffs[di] = (byte) dx1;
                    diffs[di + 1] = (byte) dy1;
                    diffs[di + 2] = (byte) dx2;
                    diffs[di + 3] = (byte) dy2;
                    diffs[di + 4] = (byte) dx;
                    diffs[di + 5] = (byte) dy;
                    uor |= dx1 | dy1 | dx2 | dy2 | dx | dy;
                    sor |= (dx1 + 0x80) | (dy1 + 0x80) |
                        (dx2 + 0x80) | (dy2 + 0x80) |
                        (dx + 0x80) | (dy + 0x80);
                    x += dx, y += dy;
                }
                if (!(uor & ~0xff))
                    data_type = eUByte;
                else if (!(sor & ~0xff))
                    data_type = eSByte;
                else
                    break;
                op = pxtBezierRelPath;
                goto useb;
            default:           /* can't happen */
                return_error(gs_error_unknownerror);
        }
        px_put_np(s, count, eSInt16);
        spputc(s, (byte) op);
        px_put_data_length(s, count * 4);       /* 2 UInt16s per point */
        for (i = 0; i < count; ++i) {
            px_put_s(s, xdev->points.data[i].x);
            px_put_s(s, xdev->points.data[i].y);
        }
        pclxl_unset_page_scale(xdev);
        if (count_smalls)
            pclxl_set_page_origin(s, -temp_origin_x, -temp_origin_y);
      zap:xdev->points.type = POINTS_NONE;
        xdev->points.count = 0;
    }
    return 0;
}

/* ------ Images ------ */

static image_enum_proc_plane_data(pclxl_image_plane_data);
static image_enum_proc_end_image(pclxl_image_end_image);

static const gx_image_enum_procs_t pclxl_image_enum_procs = {
    pclxl_image_plane_data, pclxl_image_end_image
};

/* Begin an image. */
static void
pclxl_write_begin_image(gx_device_pclxl * xdev, uint width, uint height,
                        uint dest_width, uint dest_height)
{
    stream *s = pclxl_stream(xdev);

    px_put_usa(s, width, pxaSourceWidth);
    px_put_usa(s, height, pxaSourceHeight);
    px_put_usp(s, dest_width, dest_height);
    px_put_ac(s, pxaDestinationSize, pxtBeginImage);
}

/* Write rows of an image. */
/****** IGNORES data_bit ******/
/* 2009: we try to cope with the case of data_bit being multiple of 8 now */
/* RLE version */
static void
pclxl_write_image_data_RLE(gx_device_pclxl * xdev, const byte * base,
                           int data_bit, uint raster, uint width_bits, int y,
                           int height)
{
    stream *s = pclxl_stream(xdev);
    uint width_bytes = (width_bits + 7) >> 3;
    uint num_bytes = ROUND_UP(width_bytes, 4) * height;
    bool compress = num_bytes >= 8;
    int i;
    int code;

    /* cannot handle data_bit not multiple of 8, but we don't invoke this routine that way */
    int offset = data_bit >> 3;
    const byte *data = base + offset;

    px_put_usa(s, y, pxaStartLine);
    px_put_usa(s, height, pxaBlockHeight);
    if (compress) {
        stream_RLE_state rlstate;
        stream_cursor_write w;
        stream_cursor_read r;

        /*
         * H-P printers require that all the data for an operator be
         * contained in a single data block.  Thus, we must allocate a
         * temporary buffer for the compressed data.  Currently we don't go
         * to the trouble of doing two passes if we can't allocate a buffer
         * large enough for the entire transfer.
         */
        byte *buf = gs_alloc_bytes(xdev->v_memory, num_bytes,
                                   "pclxl_write_image_data");

        if (buf == 0)
            goto nc;
        s_RLE_set_defaults_inline(&rlstate);
        rlstate.EndOfData = false;
        rlstate.omitEOD = true;
        s_RLE_init_inline(&rlstate);
        w.ptr = buf - 1;
        w.limit = w.ptr + num_bytes;
        /*
         * If we ever overrun the buffer, it means that the compressed
         * data was larger than the uncompressed.  If this happens,
         * write the data uncompressed.
         */
        for (i = 0; i < height; ++i) {
            r.ptr = data + i * raster - 1;
            r.limit = r.ptr + width_bytes;
            if ((*s_RLE_template.process)
                ((stream_state *) & rlstate, &r, &w, false) != 0 ||
                r.ptr != r.limit)
                goto ncfree;
            r.ptr = (const byte *)"\000\000\000\000\000";
            r.limit = r.ptr + (-(int)width_bytes & 3);
            if ((*s_RLE_template.process)
                ((stream_state *) & rlstate, &r, &w, false) != 0 ||
                r.ptr != r.limit)
                goto ncfree;
        }
        r.ptr = r.limit;
        code = (*s_RLE_template.process)
            ((stream_state *) & rlstate, &r, &w, true);
        if (code != EOFC && code != 0)
            goto ncfree;
        {
            uint count = w.ptr + 1 - buf;

            px_put_ub(s, eRLECompression);
            px_put_ac(s, pxaCompressMode, pxtReadImage);
            px_put_data_length(s, count);
            px_put_bytes(s, buf, count);
        }
        gs_free_object(xdev->v_memory, buf, "pclxl_write_image_data");
        return;
      ncfree:gs_free_object(xdev->v_memory, buf,
                       "pclxl_write_image_data");
    }
  nc:
    /* Write the data uncompressed. */
    px_put_ub(s, eNoCompression);
    px_put_ac(s, pxaCompressMode, pxtReadImage);
    px_put_data_length(s, num_bytes);
    for (i = 0; i < height; ++i) {
        px_put_bytes(s, data + i * raster, width_bytes);
        px_put_bytes(s, (const byte *)"\000\000\000\000",
                     -(int)width_bytes & 3);
    }
}

static void
pclxl_write_image_data_JPEG(gx_device_pclxl * xdev, const byte * base,
                            int data_bit, uint raster, uint width_bits, int y,
                            int height)
{
    stream *s = pclxl_stream(xdev);
    uint width_bytes = (width_bits + 7) >> 3;
    int i;
    int count;
    int code;

    /* cannot handle data_bit not multiple of 8, but we don't invoke this routine that way */
    int offset = data_bit >> 3;
    const byte *data = base + offset;
    jpeg_compress_data *jcdp =
        gs_alloc_struct_immovable(xdev->v_memory, jpeg_compress_data,
                                  &st_jpeg_compress_data,
                                  "pclxl_write_image_data_JPEG(jpeg_compress_data)");
    stream_DCT_state state;
    stream_cursor_read r;
    stream_cursor_write w;

    /* Approx. The worse case is ~ header + width_bytes * height.
       Apparently minimal SOI/DHT/DQT/SOS/EOI is 341 bytes. TO CHECK. */
    int buffersize = 341 + width_bytes * height;

    byte *buf = gs_alloc_bytes(xdev->v_memory, buffersize,
                               "pclxl_write_image_data_JPEG(buf)");

    /* RLE can write uncompressed without extra-allocation */
    if ((buf == 0) || (jcdp == 0)) {
        goto failed_so_use_rle_instead;
    }
    /* Create the DCT encoder state. */
    jcdp->templat = s_DCTE_template;
    s_init_state((stream_state *) & state, &jcdp->templat, 0);
    if (state.templat->set_defaults) {
        state.memory = xdev->v_memory;
        (*state.templat->set_defaults) ((stream_state *) & state);
        state.memory = NULL;
    }
    state.ColorTransform = (xdev->color_info.num_components == 3 ? 1 : 0);
    state.data.compress = jcdp;
    state.icc_profile = NULL;
    /* state.memory needs set for creation..... */
    state.memory = jcdp->memory = state.jpeg_memory = xdev->v_memory;
    if ((code = gs_jpeg_create_compress(&state)) < 0)
        goto cleanup_and_use_rle;
    /* .... and NULL after, so we don't try to free the stack based "state" */
    state.memory = NULL;
    /* image-specific info */
    jcdp->cinfo.image_width = width_bytes / xdev->color_info.num_components;
    jcdp->cinfo.image_height = height;
    switch (xdev->color_info.num_components) {
        case 3:
            jcdp->cinfo.input_components = 3;
            jcdp->cinfo.in_color_space = JCS_RGB;
            break;
        case 1:
            jcdp->cinfo.input_components = 1;
            jcdp->cinfo.in_color_space = JCS_GRAYSCALE;
            break;
        default:
            goto cleanup_and_use_rle;
            break;
    }
    /* Set compression parameters. */
    if ((code = gs_jpeg_set_defaults(&state)) < 0)
        goto cleanup_and_use_rle;

    if (state.templat->init)
        (*state.templat->init) ((stream_state *) & state);
    state.scan_line_size = jcdp->cinfo.input_components *
        jcdp->cinfo.image_width;
    jcdp->templat.min_in_size =
        max(s_DCTE_template.min_in_size, state.scan_line_size);
    jcdp->templat.min_out_size =
        max(s_DCTE_template.min_out_size, state.Markers.size);

    w.ptr = buf - 1;
    w.limit = w.ptr + buffersize;
    for (i = 0; i < height; ++i) {
        r.ptr = data + i * raster - 1;
        r.limit = r.ptr + width_bytes;
        if (((code = (*state.templat->process)
              ((stream_state *) & state, &r, &w, false)) != 0 && code != EOFC)
            || r.ptr != r.limit)
            goto cleanup_and_use_rle;
    }
    count = w.ptr + 1 - buf;
    px_put_usa(s, y, pxaStartLine);
    px_put_usa(s, height, pxaBlockHeight);
    px_put_ub(s, eJPEGCompression);
    px_put_ac(s, pxaCompressMode, pxtReadImage);
    px_put_data_length(s, count);
    px_put_bytes(s, buf, count);

    gs_free_object(xdev->v_memory, buf, "pclxl_write_image_data_JPEG(buf)");
    if (jcdp)
        gs_jpeg_destroy(&state);        /* frees *jcdp */
    return;

  cleanup_and_use_rle:
    /* cleans up - something went wrong after allocation */
    gs_free_object(xdev->v_memory, buf, "pclxl_write_image_data_JPEG(buf)");
    if (jcdp)
        gs_jpeg_destroy(&state);        /* frees *jcdp */
    /* fall through to redo in RLE */
  failed_so_use_rle_instead:
    /* the RLE routine can write without new allocation - use as fallback. */
    pclxl_write_image_data_RLE(xdev, data, data_bit, raster, width_bits, y,
                               height);
    return;
}

/* DeltaRow compression (also called "mode 3"):
   drawn heavily from gdevcljc.c:cljc_print_page(),
   This is simplier since PCL XL does not allow
   compression mix-and-match.

   Worse case of RLE is + 1/128, but worse case of DeltaRow is + 1/8
 */
static void
pclxl_write_image_data_DeltaRow(gx_device_pclxl * xdev, const byte * base,
                                int data_bit, uint raster, uint width_bits,
                                int y, int height)
{
    stream *s = pclxl_stream(xdev);
    uint width_bytes = (width_bits + 7) >> 3;
    int worst_case_comp_size = width_bytes + (width_bytes / 8) + 1;
    byte *cdata = 0;
    byte *prow = 0;
    int i;
    int count;

    /* cannot handle data_bit not multiple of 8, but we don't invoke this routine that way */
    int offset = data_bit >> 3;
    const byte *data = base + offset;

    /* allocate the worst case scenario; PCL XL has an extra 2 byte per row compared to PCL5 */
    byte *buf =
        gs_alloc_bytes(xdev->v_memory, (worst_case_comp_size + 2) * height,
                       "pclxl_write_image_data_DeltaRow(buf)");

    prow =
        gs_alloc_bytes(xdev->v_memory, width_bytes,
                       "pclxl_write_image_data_DeltaRow(prow)");
    /* the RLE routine can write uncompressed without extra-allocation */
    if ((buf == 0) || (prow == 0)) {
        pclxl_write_image_data_RLE(xdev, data, data_bit, raster, width_bits,
                                   y, height);
        return;
    }
    /* initialize the seed row */
    memset(prow, 0, width_bytes);
    cdata = buf;
    for (i = 0; i < height; i++) {
        int compressed_size =
            gdev_pcl_mode3compress(width_bytes, data + i * raster, prow,
                                   cdata + 2);

        /* PCL XL prepends row data with byte count */
        *cdata = compressed_size & 0xff;
        *(cdata + 1) = compressed_size >> 8;
        cdata += compressed_size + 2;
    }
    px_put_usa(s, y, pxaStartLine);
    px_put_usa(s, height, pxaBlockHeight);
    px_put_ub(s, eDeltaRowCompression);
    px_put_ac(s, pxaCompressMode, pxtReadImage);
    count = cdata - buf;
    px_put_data_length(s, count);
    px_put_bytes(s, buf, count);

    gs_free_object(xdev->v_memory, buf,
                   "pclxl_write_image_data_DeltaRow(buf)");
    gs_free_object(xdev->v_memory, prow,
                   "pclxl_write_image_data_DeltaRow(prow)");
    return;
}

/* calling from copy_mono/copy_color/fill_mask should never do lossy compression */
static void
pclxl_write_image_data(gx_device_pclxl * xdev, const byte * data,
                       int data_bit, uint raster, uint width_bits, int y,
                       int height, bool allow_lossy)
{
    /* If we only have 1 line, it does not make sense to do JPEG/DeltaRow */
    if (height < 2) {
        pclxl_write_image_data_RLE(xdev, data, data_bit, raster, width_bits,
                                   y, height);
        return;
    }

    switch (xdev->CompressMode) {
        case eDeltaRowCompression:
            pclxl_write_image_data_DeltaRow(xdev, data, data_bit, raster,
                                            width_bits, y, height);
            break;
        case eJPEGCompression:
            /* JPEG should not be used for mask or other data */
            if (allow_lossy)
                pclxl_write_image_data_JPEG(xdev, data, data_bit, raster,
                                            width_bits, y, height);
            else
                pclxl_write_image_data_RLE(xdev, data, data_bit, raster,
                                           width_bits, y, height);
            break;
        case eRLECompression:
        default:
            pclxl_write_image_data_RLE(xdev, data, data_bit, raster,
                                       width_bits, y, height);
            break;
    }
}

/* End an image. */
static void
pclxl_write_end_image(gx_device_pclxl * xdev)
{
    spputc(xdev->strm, pxtEndImage);
}

/* ------ Fonts ------ */

/* Write a string (single- or double-byte). */
static void
px_put_string(stream * s, const byte * data, uint len, bool wide)
{
    if (wide) {
        spputc(s, pxt_uint16_array);
        px_put_u(s, len);
        px_put_bytes(s, data, len * 2);
    } else {
        spputc(s, pxt_ubyte_array);
        px_put_u(s, len);
        px_put_bytes(s, data, len);
    }
}

/* Write a 16-bit big-endian value. */
static void
px_put_us_be(stream * s, uint i)
{
    spputc(s, (byte) (i >> 8));
    spputc(s, (byte) i);
}

/* Define a bitmap font.  The client must call px_put_string */
/* with the font name immediately before calling this procedure. */
static void
pclxl_define_bitmap_font(gx_device_pclxl * xdev)
{
    stream *s = pclxl_stream(xdev);

    static const byte bfh_[] = {
        DA(pxaFontName), DUB(0), DA(pxaFontFormat),
        pxtBeginFontHeader,
        DUS(8 + 6 + 4 + 6), DA(pxaFontHeaderLength),
        pxtReadFontHeader,
        pxt_dataLengthByte, 8 + 6 + 4 + 6,
        0, 0, 0, 0,
        254, 0, (MAX_CACHED_CHARS + 255) >> 8, 0,
        'B', 'R', 0, 0, 0, 4
    };
    static const byte efh_[] = {
        0xff, 0xff, 0, 0, 0, 0,
        pxtEndFontHeader
    };

    PX_PUT_LIT(s, bfh_);
    px_put_us_be(s, (uint) (xdev->HWResolution[0] + 0.5));
    px_put_us_be(s, (uint) (xdev->HWResolution[1] + 0.5));
    PX_PUT_LIT(s, efh_);
}

/* Set the font.  The client must call px_put_string */
/* with the font name immediately before calling this procedure. */
static void
pclxl_set_font(gx_device_pclxl * xdev)
{
    stream *s = pclxl_stream(xdev);

    static const byte sf_[] = {
        DA(pxaFontName), DUB(1), DA(pxaCharSize), DUS(0), DA(pxaSymbolSet),
        pxtSetFont
    };

    PX_PUT_LIT(s, sf_);
}

/* Define a character in a bitmap font.  The client must call px_put_string */
/* with the font name immediately before calling this procedure. */
static void
pclxl_define_bitmap_char(gx_device_pclxl * xdev, uint ccode,
                         const byte * data, uint raster, uint width_bits,
                         uint height)
{
    stream *s = pclxl_stream(xdev);
    uint width_bytes = (width_bits + 7) >> 3;
    uint size = 10 + width_bytes * height;
    uint i;

    px_put_ac(s, pxaFontName, pxtBeginChar);
    px_put_u(s, ccode);
    px_put_a(s, pxaCharCode);
    if (size > 0xffff) {
        spputc(s, pxt_uint32);
        px_put_l(s, (ulong) size);
    } else
        px_put_us(s, size);
    px_put_ac(s, pxaCharDataSize, pxtReadChar);
    px_put_data_length(s, size);
    px_put_bytes(s, (const byte *)"\000\000\000\000\000\000", 6);
    px_put_us_be(s, width_bits);
    px_put_us_be(s, height);
    for (i = 0; i < height; ++i)
        px_put_bytes(s, data + i * raster, width_bytes);
    spputc(s, pxtEndChar);
}

/* Write the name of the only font we define. */
static void
pclxl_write_font_name(gx_device_pclxl * xdev)
{
    stream *s = pclxl_stream(xdev);

    px_put_string(s, (const byte *)"@", 1, false);
}

/* Look up a bitmap id, return the index in the character table. */
/* If the id is missing, return an index for inserting. */
static int
pclxl_char_index(gx_device_pclxl * xdev, gs_id id)
{
    int i, i_empty = -1;
    uint ccode;

    for (i = (id * CHAR_HASH_FACTOR) % countof(xdev->chars.table);;
         i = (i == 0 ? countof(xdev->chars.table) : i) - 1) {
        ccode = xdev->chars.table[i];
        if (ccode == 0)
            return (i_empty >= 0 ? i_empty : i);
        else if (ccode == 1) {
            if (i_empty < 0)
                i_empty = i;
            else if (i == i_empty)      /* full table */
                return i;
        } else if (xdev->chars.data[ccode].id == id)
            return i;
    }
}

/* Remove the character table entry at a given index. */
static void
pclxl_remove_char(gx_device_pclxl * xdev, int index)
{
    uint ccode = xdev->chars.table[index];
    int i;

    if (ccode < 2)
        return;
    xdev->chars.count--;
    xdev->chars.used -= xdev->chars.data[ccode].size;
    xdev->chars.table[index] = 1;       /* mark as deleted */
    i = (index == 0 ? countof(xdev->chars.table) : index) - 1;
    if (xdev->chars.table[i] == 0) {
        /* The next slot in probe order is empty. */
        /* Mark this slot and any deleted predecessors as empty. */
        for (i = index; xdev->chars.table[i] == 1;
             i = (i == countof(xdev->chars.table) - 1 ? 0 : i + 1)
            )
            xdev->chars.table[i] = 0;
    }
}

/* Write a bitmap as a text character if possible. */
/* The caller must set the color, cursor, and RasterOp. */
/* We know id != gs_no_id. */
static int
pclxl_copy_text_char(gx_device_pclxl * xdev, const byte * data,
                     int raster, gx_bitmap_id id, int w, int h)
{
    uint width_bytes = (w + 7) >> 3;
    uint size = width_bytes * h;
    int index;
    uint ccode;
    stream *s = pclxl_stream(xdev);

    if (size > MAX_CHAR_SIZE)
        return -1;
    index = pclxl_char_index(xdev, id);
    if ((ccode = xdev->chars.table[index]) < 2) {
        /* Enter the character in the table. */
        while (xdev->chars.used + size > MAX_CHAR_DATA ||
               xdev->chars.count >= MAX_CACHED_CHARS - 2) {
            ccode = xdev->chars.next_out;
            index = pclxl_char_index(xdev, xdev->chars.data[ccode].id);
            pclxl_remove_char(xdev, index);
            xdev->chars.next_out =
                (ccode == MAX_CACHED_CHARS - 1 ? 2 : ccode + 1);
        }
        index = pclxl_char_index(xdev, id);
        ccode = xdev->chars.next_in;
        xdev->chars.data[ccode].id = id;
        xdev->chars.data[ccode].size = size;
        xdev->chars.table[index] = ccode;
        xdev->chars.next_in = (ccode == MAX_CACHED_CHARS - 1 ? 2 : ccode + 1);
        if (!xdev->chars.count++) {
            /* This is the very first character. */
            pclxl_write_font_name(xdev);
            pclxl_define_bitmap_font(xdev);
        }
        xdev->chars.used += size;
        pclxl_write_font_name(xdev);
        pclxl_define_bitmap_char(xdev, ccode, data, raster, w, h);
    }
    if (!xdev->font_set) {
        pclxl_write_font_name(xdev);
        pclxl_set_font(xdev);
        xdev->font_set = true;
    }
    {
        byte cc_bytes[2];

        cc_bytes[0] = (byte) ccode;
        cc_bytes[1] = ccode >> 8;
        px_put_string(s, cc_bytes, 1, cc_bytes[1] != 0);
    }
    px_put_ac(s, pxaTextData, pxtText);
    return 0;
}

/* ---------------- Vector implementation procedures ---------------- */

static int
pclxl_beginpage(gx_device_vector * vdev)
{
    gx_device_pclxl *const xdev = (gx_device_pclxl *) vdev;

    /*
     * We can't use gdev_vector_stream here, because this may be called
     * from there before in_page is set.
     */
    stream *s = vdev->strm;
    byte media_source = eAutoSelect;    /* default */

    xdev->page++;               /* even/odd for duplex front/back */

/*
    errprintf(vdev->memory, "PAGE: %d %d\n", xdev->page, xdev->NumCopies);
    errprintf(vdev->memory, "INFO: Printing page %d...\n", xdev->page);
    errflush(vdev->memory);
*/

    px_write_page_header(s, (const gx_device *)vdev);

    if (xdev->ManualFeed_set && xdev->ManualFeed)
        media_source = 2;
    else if (xdev->MediaPosition_set && xdev->MediaPosition >= 0)
        media_source = xdev->MediaPosition;

    px_write_select_media(s, (const gx_device *)vdev, &xdev->media_size,
                          &media_source,
                          xdev->page, xdev->Duplex, xdev->Tumble,
                          xdev->MediaType_set, xdev->MediaType);

    spputc(s, pxtBeginPage);
    return 0;
}

static int
pclxl_setlinewidth(gx_device_vector * vdev, double width)
{
    stream *s = gdev_vector_stream(vdev);

    px_put_us(s, (uint) (width + 0.5));
    px_put_ac(s, pxaPenWidth, pxtSetPenWidth);
    return 0;
}

static int
pclxl_setlinecap(gx_device_vector * vdev, gs_line_cap cap)
{
    stream *s = gdev_vector_stream(vdev);

    /* The PCL XL cap styles just happen to be identical to PostScript. */
    px_put_ub(s, (byte) cap);
    px_put_ac(s, pxaLineCapStyle, pxtSetLineCap);
    return 0;
}

static int
pclxl_setlinejoin(gx_device_vector * vdev, gs_line_join join)
{
    stream *s = gdev_vector_stream(vdev);

    if (((int)join < 0) || ((int)join > 3)) {
        emprintf1(vdev->memory,
                  "Igoring invalid linejoin enumerator %d\n", join);
        return 0;
    }
    /* The PCL XL join styles just happen to be identical to PostScript. */
    px_put_ub(s, (byte) join);
    px_put_ac(s, pxaLineJoinStyle, pxtSetLineJoin);
    return 0;
}

static int
pclxl_setmiterlimit(gx_device_vector * vdev, double limit)
{
    stream *s = gdev_vector_stream(vdev);

    /*
     * Amazingly enough, the PCL XL specification doesn't allow real
     * numbers for the miter limit.
     */
    int i_limit = (int)(limit + 0.5);

    px_put_u(s, max(i_limit, 1));
    px_put_ac(s, pxaMiterLength, pxtSetMiterLimit);
    return 0;
}

/*
 * The number of elements in the dash pattern array is device
 * dependent but a maximum of 20 has been observed on several HP
 * printers.
 */

#define MAX_DASH_ELEMENTS 20

static int
pclxl_setdash(gx_device_vector * vdev, const float *pattern, uint count,
              double offset)
{
    stream *s = gdev_vector_stream(vdev);

    if (count == 0) {
        static const byte nac_[] = {
            DUB(0), DA(pxaSolidLine)
        };

        PX_PUT_LIT(s, nac_);
    } else if (count > MAX_DASH_ELEMENTS)
        return_error(gs_error_limitcheck);
    else {
        uint i;
        uint pattern_length = 0;
        /*
         * Astoundingly, PCL XL doesn't allow real numbers here.
         * Do the best we can.
         */

        /* check if the resulting total pattern length will be 0 */
        for (i = 0; i < count; ++i)
            pattern_length += (uint) (pattern[i]);
        if (pattern_length == 0)
            return_error(gs_error_rangecheck);

        spputc(s, pxt_uint16_array);
        px_put_ub(s, (byte) count);
        for (i = 0; i < count; ++i)
            px_put_s(s, (uint) pattern[i]);
        px_put_a(s, pxaLineDashStyle);
        if (offset != 0)
            px_put_usa(s, (uint) offset, pxaDashOffset);
    }
    spputc(s, pxtSetLineDash);
    return 0;
}

static int
pclxl_setlogop(gx_device_vector * vdev, gs_logical_operation_t lop,
               gs_logical_operation_t diff)
{
    stream *s = gdev_vector_stream(vdev);

    if (diff & lop_S_transparent) {
        px_put_ub(s, (byte) (lop & lop_S_transparent ? 1 : 0));
        px_put_ac(s, pxaTxMode, pxtSetSourceTxMode);
    }
    if (diff & lop_T_transparent) {
        px_put_ub(s, (byte) (lop & lop_T_transparent ? 1 : 0));
        px_put_ac(s, pxaTxMode, pxtSetPaintTxMode);
    }
    if (lop_rop(diff)) {
        px_put_ub(s, (byte) lop_rop(lop));
        px_put_ac(s, pxaROP3, pxtSetROP);
    }
    return 0;
}

static int
pclxl_can_handle_hl_color(gx_device_vector * vdev, const gs_gstate * pgs,
                          const gx_drawing_color * pdc)
{
    return false;
}

static int
pclxl_setfillcolor(gx_device_vector * vdev, const gs_gstate * pgs,
                   const gx_drawing_color * pdc)
{
    gx_device_pclxl *const xdev = (gx_device_pclxl *) vdev;

    return pclxl_set_color(xdev, pdc, pxaNullBrush, pxtSetBrushSource);
}

static int
pclxl_setstrokecolor(gx_device_vector * vdev, const gs_gstate * pgs,
                     const gx_drawing_color * pdc)
{
    gx_device_pclxl *const xdev = (gx_device_pclxl *) vdev;

    return pclxl_set_color(xdev, pdc, pxaNullPen, pxtSetPenSource);
}

static int
pclxl_dorect(gx_device_vector * vdev, fixed x0, fixed y0, fixed x1,
             fixed y1, gx_path_type_t type)
{
    gx_device_pclxl *const xdev = (gx_device_pclxl *) vdev;
    stream *s = gdev_vector_stream(vdev);

    /* Check for out-of-range points. */
#define OUT_OF_RANGE(v) (v < 0 || v >= int2fixed(0x10000))
    if (OUT_OF_RANGE(x0) || OUT_OF_RANGE(y0) ||
        OUT_OF_RANGE(x1) || OUT_OF_RANGE(y1)
        )
        return_error(gs_error_rangecheck);
#undef OUT_OF_RANGE
    if (type & (gx_path_type_fill | gx_path_type_stroke)) {
        pclxl_set_paints(xdev, type);
        px_put_usq_fixed(s, x0, y0, x1, y1);
        px_put_ac(s, pxaBoundingBox, pxtRectangle);
    }
    if (type & gx_path_type_clip) {
        static const byte cr_[] = {
            DA(pxaBoundingBox),
            DUB(eInterior), DA(pxaClipRegion),
            pxtSetClipRectangle
        };

        px_put_usq_fixed(s, x0, y0, x1, y1);
        PX_PUT_LIT(s, cr_);
    }
    return 0;
}

static int
pclxl_beginpath(gx_device_vector * vdev, gx_path_type_t type)
{
    gx_device_pclxl *const xdev = (gx_device_pclxl *) vdev;
    stream *s = gdev_vector_stream(vdev);

    spputc(s, pxtNewPath);
    xdev->points.type = POINTS_NONE;
    xdev->points.count = 0;
    return 0;
}

static int
pclxl_moveto(gx_device_vector * vdev, double x0, double y0, double x,
             double y, gx_path_type_t type)
{
    gx_device_pclxl *const xdev = (gx_device_pclxl *) vdev;
    int code = pclxl_flush_points(xdev);

    if (code < 0)
        return code;
    return pclxl_set_cursor(xdev,
                            xdev->points.current.x = (int)(x + 0.5),
                            xdev->points.current.y = (int)(y + 0.5));
}

static int
pclxl_lineto(gx_device_vector * vdev, double x0, double y0, double x,
             double y, gx_path_type_t type)
{
    gx_device_pclxl *const xdev = (gx_device_pclxl *) vdev;

    if (xdev->points.type != POINTS_LINES || xdev->points.count >= NUM_POINTS - 2) {
        if (xdev->points.type != POINTS_NONE) {
            int code = pclxl_flush_points(xdev);

            if (code < 0)
                return code;
        }
        xdev->points.current.x = (int)(x0 + 0.5);
        xdev->points.current.y = (int)(y0 + 0.5);
        xdev->points.type = POINTS_LINES;

        /* This can only happen if we get two 'POINTS_NONE' in a row, in which case
         * just overwrite the previous one below.
         */
        if (xdev->points.count >= NUM_POINTS - 1)
            xdev->points.count--;
    }
    {
        gs_int_point *ppt = &xdev->points.data[xdev->points.count++];

        ppt->x = (int)(x + 0.5), ppt->y = (int)(y + 0.5);
    }
    return 0;
}

static int
pclxl_curveto(gx_device_vector * vdev, double x0, double y0,
              double x1, double y1, double x2, double y2, double x3,
              double y3, gx_path_type_t type)
{
    gx_device_pclxl *const xdev = (gx_device_pclxl *) vdev;

    if (xdev->points.type != POINTS_CURVES ||
        xdev->points.count >= NUM_POINTS - 4) {
        if (xdev->points.type != POINTS_NONE) {
            int code = pclxl_flush_points(xdev);

            if (code < 0)
                return code;
        }
        xdev->points.current.x = (int)(x0 + 0.5);
        xdev->points.current.y = (int)(y0 + 0.5);
        xdev->points.type = POINTS_CURVES;

        /* This can only happen if we get multiple 'POINTS_NONE' in a row, in which case
         * just overwrite the previous one below.
         */
        if (xdev->points.count >= NUM_POINTS - 3)
            xdev->points.count -= 3;
    }
    {
        gs_int_point *ppt = &xdev->points.data[xdev->points.count];

        ppt->x = (int)(x1 + 0.5), ppt->y = (int)(y1 + 0.5), ++ppt;
        ppt->x = (int)(x2 + 0.5), ppt->y = (int)(y2 + 0.5), ++ppt;
        ppt->x = (int)(x3 + 0.5), ppt->y = (int)(y3 + 0.5);
    }
    xdev->points.count += 3;
    return 0;
}

static int
pclxl_closepath(gx_device_vector * vdev, double x, double y,
                double x_start, double y_start, gx_path_type_t type)
{
    gx_device_pclxl *const xdev = (gx_device_pclxl *) vdev;
    stream *s = gdev_vector_stream(vdev);
    int code = pclxl_flush_points(xdev);

    if (code < 0)
        return code;
    spputc(s, pxtCloseSubPath);
    xdev->points.current.x = (int)(x_start + 0.5);
    xdev->points.current.y = (int)(y_start + 0.5);
    return 0;
}

static int
pclxl_endpath(gx_device_vector * vdev, gx_path_type_t type)
{
    gx_device_pclxl *const xdev = (gx_device_pclxl *) vdev;
    stream *s = gdev_vector_stream(vdev);
    int code = pclxl_flush_points(xdev);
    gx_path_type_t rule = type & gx_path_type_rule;

    if (code < 0)
        return code;
    if (type & (gx_path_type_fill | gx_path_type_stroke)) {
        if (rule != xdev->fill_rule) {
            px_put_ub(s, (byte) (rule == gx_path_type_even_odd ? eEvenOdd :
                                 eNonZeroWinding));
            px_put_ac(s, pxaFillMode, pxtSetFillMode);
            xdev->fill_rule = rule;
        }
        pclxl_set_paints(xdev, type);
        spputc(s, pxtPaintPath);
    }
    if (type & gx_path_type_clip) {
        static const byte scr_[] = {
            DUB(eInterior), DA(pxaClipRegion), pxtSetClipReplace
        };

        if (rule != xdev->clip_rule) {
            px_put_ub(s, (byte) (rule == gx_path_type_even_odd ? eEvenOdd :
                                 eNonZeroWinding));
            px_put_ac(s, pxaClipMode, pxtSetClipMode);
            xdev->clip_rule = rule;
        }
        PX_PUT_LIT(s, scr_);
    }
    return 0;
}

/* Vector implementation procedures */

static const gx_device_vector_procs pclxl_vector_procs = {
    /* Page management */
    pclxl_beginpage,
    /* Imager state */
    pclxl_setlinewidth,
    pclxl_setlinecap,
    pclxl_setlinejoin,
    pclxl_setmiterlimit,
    pclxl_setdash,
    gdev_vector_setflat,
    pclxl_setlogop,
    /* Other state */
    pclxl_can_handle_hl_color,
    pclxl_setfillcolor,
    pclxl_setstrokecolor,
    /* Paths */
    gdev_vector_dopath,
    pclxl_dorect,
    pclxl_beginpath,
    pclxl_moveto,
    pclxl_lineto,
    pclxl_curveto,
    pclxl_closepath,
    pclxl_endpath
};

/* ---------------- Driver procedures ---------------- */

/* ------ Open/close/page ------ */

/* Open the device. */
static int
pclxl_open_device(gx_device * dev)
{
    gx_device_vector *vdev = (gx_device_vector *) dev;
    gx_device_pclxl *xdev = (gx_device_pclxl *) dev;
    int code;

    vdev->v_memory = dev->memory->stable_memory;
    vdev->vec_procs = &pclxl_vector_procs;
    code = gdev_vector_open_file_options(vdev, 512,
                                         VECTOR_OPEN_FILE_SEQUENTIAL);
    if (code < 0)
        return code;

    while (dev->child)
        dev = dev->child;
    vdev = (gx_device_vector *) dev;
    xdev = (gx_device_pclxl *) dev;

    pclxl_page_init(xdev);
    px_write_file_header(vdev->strm, dev, xdev->Staple);
    xdev->media_size = pxeMediaSize_next;       /* no size selected */
    memset(&xdev->chars, 0, sizeof(xdev->chars));
    xdev->chars.next_in = xdev->chars.next_out = 2;
    /* xdev->iccTransform = false; *//* set true/false here to ignore command line */
    return 0;
}

/* Wrap up ("output") a page. */
/* We only support flush = true */
static int
pclxl_output_page(gx_device * dev, int num_copies, int flush)
{
    gx_device_pclxl *const xdev = (gx_device_pclxl *) dev;
    stream *s;
    int code;

    /* Note that unlike close_device, end_page must not omit blank pages. */
    if (!xdev->in_page)
        pclxl_beginpage((gx_device_vector *) dev);
    s = xdev->strm;
    px_put_usa(s, (uint) num_copies, pxaPageCopies);    /* num_copies */
    spputc(s, pxtEndPage);
    sflush(s);
    pclxl_page_init(xdev);
    if (gp_ferror(xdev->file))
        return_error(gs_error_ioerror);
    if ((code = gx_finish_output_page(dev, num_copies, flush)) < 0)
        return code;
    /* Check if we need to change the output file for separate pages */
    if (gx_outputfile_is_separate_pages
        (((gx_device_vector *) dev)->fname, dev->memory)) {
        if ((code = pclxl_close_device(dev)) < 0)
            return code;
        code = pclxl_open_device(dev);
    }
    return code;
}

/* Close the device. */
/* Note that if this is being called as a result of finalization, */
/* the stream may no longer exist. */
static int
pclxl_close_device(gx_device * dev)
{
    gx_device_pclxl *const xdev = (gx_device_pclxl *) dev;
    gp_file *file = xdev->file;

    if (xdev->strm != NULL)
        sflush(xdev->strm);
    if (xdev->in_page)
        gp_fputc(pxtEndPage, file);
    px_write_file_trailer(file);
    return gdev_vector_close_file((gx_device_vector *) dev);
}

/* ------ One-for-one images ------ */

static const byte eBit_values[] = {
    0, e1Bit, 0, 0, e4Bit, 0, 0, 0, e8Bit
};

/* Copy a monochrome bitmap. */
static int
pclxl_copy_mono(gx_device * dev, const byte * data, int data_x, int raster,
                gx_bitmap_id id, int x, int y, int w, int h,
                gx_color_index zero, gx_color_index one)
{
    gx_device_vector *const vdev = (gx_device_vector *) dev;
    gx_device_pclxl *const xdev = (gx_device_pclxl *) dev;
    int code;
    stream *s;
    gx_color_index color0 = zero, color1 = one;
    gx_color_index white = ((gx_color_index)1 << dev->color_info.depth) - 1;
    gx_color_index black = 0;
    gs_logical_operation_t lop;
    byte palette[2 * 3];
    int palette_size;
    pxeColorSpace_t color_space;

    fit_copy(dev, data, data_x, raster, id, x, y, w, h);
    code = gdev_vector_update_clip_path(vdev, NULL);
    if (code < 0)
        return code;

    /* write_image_data() needs byte-alignment,
     * and gx_default_copy_* is more efficient than pxlcl_*
     * for small rasters. See details in copy_color().
     */
    if (((data_x & 7) != 0) || (h == 1) || (w == 1))
        return gx_default_copy_mono(dev, data, data_x, raster, id,
                                    x, y, w, h, zero, one);

    pclxl_set_cursor(xdev, x, y);
    if (id != gs_no_id && zero == gx_no_color_index &&
        one != gx_no_color_index && data_x == 0) {
        gx_drawing_color dcolor;

        code = gdev_vector_update_log_op(vdev, rop3_T | lop_T_transparent);
        if (code < 0)
            return 0;

        set_nonclient_dev_color(&dcolor, one);
        pclxl_setfillcolor(vdev, NULL, &dcolor);
        if (pclxl_copy_text_char(xdev, data, raster, id, w, h) >= 0)
            return 0;
    }
    /*
     * The following doesn't work if we're writing white with a mask.
     * We'll fix it eventually.
     *
     * The logic goes like this: non-white + mask (transparent)
     * works by setting the mask color to white and also declaring
     * white-is-transparent. This doesn't work for drawing white + mask,
     * since everything is then white+white-and-transparent. So instead
     * we set mask color to black, invert and draw the destination/background
     * through it, as well as drawing the white color.
     *
     * In rop3 terms, this is (D & ~S) | S
     *
     * This also only works in the case of the drawing color is white,
     * because we need the inversion to not draw anything, (especially
     * not the complimentary color/shade). So we have two different code paths,
     * white + mask and non-whites + mask.
     *
     * There is a further refinement to this algorithm - it appears that
     * black+mask is treated specially by the vector driver core (rendered
     * as transparent on white), and does not work as non-white + mask.
     * So Instead we set mask color to white and do (S & D) (i.e. draw
     * background on mask, instead of transparent on mask).
     *
     */
    if (zero == gx_no_color_index) {
        if (one == gx_no_color_index)
            return 0;
        if (one != white) {
            if (one == black) {
                lop = (rop3_S & rop3_D);
            } else {
                lop = rop3_S | lop_S_transparent;
            }
            color0 = white;
        } else {
            lop = rop3_S | (rop3_D & rop3_not(rop3_S));
            color0 = black;
        }
    } else if (one == gx_no_color_index) {
        if (zero != white) {
            if (zero == black) {
                lop = (rop3_S & rop3_D);
            } else {
                lop = rop3_S | lop_S_transparent;
            }
            color1 = white;
        } else {
            lop = rop3_S | (rop3_D & rop3_not(rop3_S));
            color1 = black;
        }
    } else {
        lop = rop3_S;
    }

    if (dev->color_info.num_components == 1 ||
        (RGB_IS_GRAY(color0) && RGB_IS_GRAY(color1))
        ) {
        palette[0] = (byte) color0;
        palette[1] = (byte) color1;
        palette_size = 2;
        color_space = eGray;
        if_debug2m('b', dev->memory, "color palette %02X %02X\n",
                   palette[0], palette[1]);
    } else {
        palette[0] = (byte) (color0 >> 16);
        palette[1] = (byte) (color0 >> 8);
        palette[2] = (byte) color0;
        palette[3] = (byte) (color1 >> 16);
        palette[4] = (byte) (color1 >> 8);
        palette[5] = (byte) color1;
        palette_size = 6;
        color_space = eRGB;
    }
    code = gdev_vector_update_log_op(vdev, lop);
    if (code < 0)
        return 0;
    pclxl_set_color_palette(xdev, color_space, palette, palette_size);
    s = pclxl_stream(xdev);
    {
        static const byte mi_[] = {
            DUB(e1Bit), DA(pxaColorDepth),
            DUB(eIndexedPixel), DA(pxaColorMapping)
        };

        PX_PUT_LIT(s, mi_);
    }
    pclxl_write_begin_image(xdev, w, h, w, h);
    pclxl_write_image_data(xdev, data, data_x, raster, w, 0, h, false);
    pclxl_write_end_image(xdev);
    return 0;
}

/* Copy a color bitmap. */
static int
pclxl_copy_color(gx_device * dev,
                 const byte * base, int sourcex, int raster, gx_bitmap_id id,
                 int x, int y, int w, int h)
{
    gx_device_vector *const vdev = (gx_device_vector *) dev;
    gx_device_pclxl *const xdev = (gx_device_pclxl *) dev;
    stream *s;
    uint source_bit;
    int code;

    fit_copy(dev, base, sourcex, raster, id, x, y, w, h);
    code = gdev_vector_update_clip_path(vdev, NULL);
    if (code < 0)
        return code;

    source_bit = sourcex * dev->color_info.depth;

    /* side-effect from fill/stroke may have set color space to eGray */
    if (dev->color_info.num_components == 3)
        pclxl_set_color_space(xdev, eRGB);
    else if (dev->color_info.num_components == 1)
        pclxl_set_color_space(xdev, eGray);

    /* write_image_data() needs byte-alignment,
     * and gx_default_copy_* is more efficient than pxlcl_*
     * for small rasters.
     *
     * SetBrushSource + Rectangle = 21 byte for 1x1 RGB
     * SetCursor+ BeginImage + ReadImage + EndImage = 56 bytes for 1x1 RGB
     * 3x1 RGB at 3 different colors takes 62 bytes for pxlcl_*
     * but gx_default_copy_* uses 63 bytes. Below 3 pixels, gx_default_copy_*
     * is better than pxlcl_*; above 3 pixels, it is less clear;
     * in practice, single-lines seems better coded as painted rectangles
     * than images.
     */
    if (((source_bit & 7) != 0) || (w == 1) || (h == 1))
        return gx_default_copy_color(dev, base, sourcex, raster, id,
                                     x, y, w, h);
    code = gdev_vector_update_log_op(vdev, rop3_S);
    if (code < 0)
        return 0;
    pclxl_set_cursor(xdev, x, y);
    s = pclxl_stream(xdev);
    {
        static const byte ci_[] = {
            DA(pxaColorDepth),
            DUB(eDirectPixel), DA(pxaColorMapping)
        };

        px_put_ub(s, eBit_values[dev->color_info.depth /
                                 dev->color_info.num_components]);
        PX_PUT_LIT(s, ci_);
    }
    pclxl_write_begin_image(xdev, w, h, w, h);
    pclxl_write_image_data(xdev, base, source_bit, raster,
                           w * dev->color_info.depth, 0, h, false);
    pclxl_write_end_image(xdev);
    return 0;
}

/* Fill a mask. */
static int
pclxl_fill_mask(gx_device * dev,
                const byte * data, int data_x, int raster, gx_bitmap_id id,
                int x, int y, int w, int h,
                const gx_drawing_color * pdcolor, int depth,
                gs_logical_operation_t lop, const gx_clip_path * pcpath)
{
    gx_device_vector *const vdev = (gx_device_vector *) dev;
    gx_device_pclxl *const xdev = (gx_device_pclxl *) dev;
    int code;
    stream *s;
    gx_color_index foreground;

    fit_copy(dev, data, data_x, raster, id, x, y, w, h);
    /* write_image_data() needs byte-alignment,
     * and gx_default_copy_* is more efficient than pxlcl_*
     * for small rasters. See details in copy_color().
     */
    if ((data_x & 7) != 0 || !gx_dc_is_pure(pdcolor) || depth > 1 || (w == 1)
        || (h == 1))
        return gx_default_fill_mask(dev, data, data_x, raster, id, x, y, w, h,
                                    pdcolor, depth, lop, pcpath);
    code = gdev_vector_update_clip_path(vdev, pcpath);
    foreground = gx_dc_pure_color(pdcolor);
    if (code < 0)
        return code;
    code = gdev_vector_update_fill_color(vdev, NULL, pdcolor);
    if (code < 0)
        return 0;
    pclxl_set_cursor(xdev, x, y);
    if (id != gs_no_id && data_x == 0) {
        code = gdev_vector_update_log_op(vdev, lop);
        if (code < 0)
            return 0;
        if (pclxl_copy_text_char(xdev, data, raster, id, w, h) >= 0)
            return 0;
    }
    /* This is similiar to the copy_mono white-on-mask,
     * except we are drawing white on the black of a black/white mask,
     * so we invert source, compared to copy_mono */
    if (foreground == ((gx_color_index)1 << dev->color_info.depth) - 1) {       /* white */
        lop = rop3_not(rop3_S) | (rop3_D & rop3_S);
    } else if (foreground == 0) {       /* black */
        lop = (rop3_S & rop3_D);
    } else
        lop |= rop3_S | lop_S_transparent;

    code = gdev_vector_update_log_op(vdev, lop);
    if (code < 0)
        return 0;
    pclxl_set_color_palette(xdev, eGray, (const byte *)"\377\000", 2);
    s = pclxl_stream(xdev);
    {
        static const byte mi_[] = {
            DUB(e1Bit), DA(pxaColorDepth),
            DUB(eIndexedPixel), DA(pxaColorMapping)
        };

        PX_PUT_LIT(s, mi_);
    }
    pclxl_write_begin_image(xdev, w, h, w, h);
    pclxl_write_image_data(xdev, data, data_x, raster, w, 0, h, false);
    pclxl_write_end_image(xdev);
    return 0;
}

/* Do a RasterOp. */
static int
pclxl_strip_copy_rop2(gx_device * dev, const byte * sdata, int sourcex,
                     uint sraster, gx_bitmap_id id,
                     const gx_color_index * scolors,
                     const gx_strip_bitmap * textures,
                     const gx_color_index * tcolors,
                     int x, int y, int width, int height,
                     int phase_x, int phase_y, gs_logical_operation_t lop,
                     uint plane_height)
{
    lop = lop_sanitize(lop);
    /* Improvements possible here using PXL ROP3
       for some combinations of args; use gx_default for now */
    if (!rop3_uses_D(lop))  /* gx_default() cannot cope with D ops */
        return gx_default_strip_copy_rop2(dev, sdata, sourcex,
                                          sraster, id,
                                          scolors,
                                          textures,
                                          tcolors,
                                          x, y, width, height,
                                          phase_x, phase_y, lop,
                                          plane_height);
    return 0;
}

/* ------ High-level images ------ */

#define MAX_ROW_DATA 500000     /* arbitrary */
typedef struct pclxl_image_enum_s {
    gdev_vector_image_enum_common;
    gs_matrix mat;
    struct ir_ {
        byte *data;
        int num_rows;           /* # of allocated rows */
        int first_y;
        uint raster;
    } rows;
    bool flipped;
    gsicc_link_t *icclink;
} pclxl_image_enum_t;

gs_private_st_suffix_add2(st_pclxl_image_enum, pclxl_image_enum_t,
                          "pclxl_image_enum_t", pclxl_image_enum_enum_ptrs,
                          pclxl_image_enum_reloc_ptrs, st_vector_image_enum,
                          rows.data, icclink);

/* Start processing an image. */
static int
pclxl_begin_typed_image(gx_device * dev,
                  const gs_gstate * pgs,
                  const gs_matrix *pmat,
                  const gs_image_common_t * pic,
                  const gs_int_rect * prect,
                  const gx_drawing_color * pdcolor,
                  const gx_clip_path * pcpath, gs_memory_t * mem,
                  gx_image_enum_common_t ** pinfo)
{
    gx_device_vector *const vdev = (gx_device_vector *) dev;
    gx_device_pclxl *const xdev = (gx_device_pclxl *) dev;
    const gs_image_t *pim = (const gs_image_t *)pic;
    const gs_color_space *pcs;
    pclxl_image_enum_t *pie;
    byte *row_data;
    int num_rows;
    uint row_raster;
    int bits_per_pixel;
    gs_matrix mat;
    int code;

    /* We only cope with image type 1 here. */
    if (pic->type->index != 1)
        goto use_default;

    pcs = pim->ColorSpace;
    /*
     * Following should divide by num_planes, but we only handle chunky
     * images, i.e., num_planes = 1.
     */
    bits_per_pixel =
        (pim->ImageMask ? 1 :
         pim->BitsPerComponent * gs_color_space_num_components(pcs));

    /*
     * Check whether we can handle this image.  PCL XL 1.0 and 2.0 only
     * handle orthogonal transformations.
     */
    code = gs_matrix_invert(&pim->ImageMatrix, &mat);
    if (code < 0)
        goto use_default;
    if (pmat == NULL)
        pmat = &ctm_only(pgs);
    gs_matrix_multiply(&mat, pmat, &mat);

    if (pclxl_nontrivial_transfer(pgs))
        goto use_default;

    if (pim->Width == 0 || pim->Height == 0)
        goto use_default;

    if (bits_per_pixel == 32) {
        /*
           32-bit cmyk depends on transformable to 24-bit rgb.
           Some 32-bit aren't cmyk's. (e.g. DeviceN)
         */
        if (!pclxl_can_icctransform(pim))
            goto use_default;

        /*
           Strictly speaking, regardless of bits_per_pixel,
           we cannot handle non-Identity Decode array.
           Historically we did (wrongly), so this is inside
           a 32-bit conditional to avoid regression, but shouldn't.
         */
        if (pim->Decode[0] != 0 || pim->Decode[1] != 1 ||
            pim->Decode[2] != 0 || pim->Decode[3] != 1 ||
            pim->Decode[4] != 0 || pim->Decode[5] != 1)
            goto use_default;
    }

    /*
     * NOTE: this predicate should be fixed to be readable and easily
     * debugged.  Each condition should be separate.  See the large
     * similar conditional in clist_begin_typed_image which has
     * already been reworked.  We can handle rotations of 90 degs +
     * scaling + reflections.  * These have one of the diagonals being
     * zeros * (and the other diagonals having non-zeros).
     */
    if ((!((mat.xx * mat.yy != 0) && (mat.xy == 0) && (mat.yx == 0)) &&
         !((mat.xx == 0) && (mat.yy == 0) && (mat.xy * mat.yx != 0))) ||
        (pim->ImageMask ?
         (!gx_dc_is_pure(pdcolor) || pim->CombineWithColor) :
         ((!pclxl_can_handle_color_space(pcs) ||
           (bits_per_pixel != 1 && bits_per_pixel != 4 &&
            bits_per_pixel != 8 && bits_per_pixel != 24 &&
            bits_per_pixel != 32))
          && !(pclxl_can_icctransform(pim) && xdev->iccTransform))) ||
        pim->format != gs_image_format_chunky || pim->Interpolate || prect)
        goto use_default;
    row_raster = (bits_per_pixel * pim->Width + 7) >> 3;
    num_rows = MAX_ROW_DATA / row_raster;
    if (num_rows > pim->Height)
        num_rows = pim->Height;
    if (num_rows <= 0)
        num_rows = 1;
    pie = gs_alloc_struct(mem, pclxl_image_enum_t, &st_pclxl_image_enum,
                          "pclxl_begin_image");
    row_data = gs_alloc_bytes(mem, num_rows * row_raster,
                              "pclxl_begin_image(rows)");
    if (pie == 0 || row_data == 0) {
        code = gs_note_error(gs_error_VMerror);
        goto fail;
    }
    code = gdev_vector_begin_image(vdev, pgs, pim, pim->format, prect,
                                   pdcolor, pcpath, mem,
                                   &pclxl_image_enum_procs,
                                   (gdev_vector_image_enum_t *) pie);
    if (code < 0)
        goto fail;

    /* emit a PXL XL rotation and adjust mat correspondingly */
    pie->flipped = false;
    if (mat.xx * mat.yy > 0) {
        if (mat.xx < 0) {
            stream *s = pclxl_stream(xdev);

            mat.xx = -mat.xx;
            mat.yy = -mat.yy;
            mat.tx = -mat.tx;
            mat.ty = -mat.ty;
            px_put_ss(s, 180);
            xdev->state_rotated = 2;
            px_put_ac(s, pxaPageAngle, pxtSetPageRotation);
        }
        /* leave the matrix alone if it is portrait */
    } else if (mat.xx * mat.yy < 0) {
        pie->flipped = true;
        if (mat.xx < 0) {
            stream *s = pclxl_stream(xdev);

            mat.xx = -mat.xx;
            mat.tx = -mat.tx;
            px_put_ss(s, +180);
            xdev->state_rotated = +2;
            px_put_ac(s, pxaPageAngle, pxtSetPageRotation);
        } else {
            mat.yy = -mat.yy;
            mat.ty = -mat.ty;
        }
    } else if (mat.xy * mat.yx < 0) {
        /* rotate +90 or -90 */
        float tmpf;
        stream *s = pclxl_stream(xdev);

        if (mat.xy > 0) {
            mat.xx = mat.xy;
            mat.yy = -mat.yx;
            tmpf = mat.tx;
            mat.tx = mat.ty;
            mat.ty = -tmpf;
            px_put_ss(s, -90);
            xdev->state_rotated = -1;
        } else {
            mat.xx = -mat.xy;
            mat.yy = mat.yx;
            tmpf = mat.tx;
            mat.tx = -mat.ty;
            mat.ty = tmpf;
            px_put_ss(s, +90);
            xdev->state_rotated = +1;
        }
        mat.xy = mat.yx = 0;
        px_put_ac(s, pxaPageAngle, pxtSetPageRotation);
    } else if (mat.xy * mat.yx > 0) {
        float tmpf;
        stream *s = pclxl_stream(xdev);

        pie->flipped = true;
        if (mat.xy > 0) {
            mat.xx = mat.xy;
            mat.yy = mat.yx;
            tmpf = mat.tx;
            mat.tx = mat.ty;
            mat.ty = tmpf;
            px_put_ss(s, -90);
            xdev->state_rotated = -1;
        } else {
            mat.xx = -mat.xy;
            mat.yy = -mat.yx;
            tmpf = mat.tx;
            mat.tx = -mat.ty;
            mat.ty = -tmpf;
            px_put_ss(s, +90);
            xdev->state_rotated = +1;
        }
        mat.xy = mat.yx = 0;
        px_put_ac(s, pxaPageAngle, pxtSetPageRotation);
    }

    pie->mat = mat;
    pie->rows.data = row_data;
    pie->rows.num_rows = num_rows;
    pie->rows.first_y = 0;
    pie->rows.raster = row_raster;
    /*
       pclxl_can_handle_color_space() is here to avoid regression,
       to avoid icc if traditional code path works (somewhat).
       It is flawed in that it claims to handles device colors with
       icc profiles (and output device colours); so that "can transform"
       won't transform; we need to override it for the case of
       32-bit colour, except when usefastcolor is specified.

       "can_icctransform" should have been accompanied by a
       "will tranform" (xdev->iccTransform) check. However, since
       we want to transform for CMYK regardless, so just as well
       there was not such a check, and we are not adding one now.

       In other words, the bizarre logic in the middle is to keep
       the current workflow as much as possible, but force icc mode
       or confirm fastcolor mode only for 32-bit image, regardless
       of whether xdev->iccTransform is on.

       Whole-sale icc mode is to simply remove entire middle part,
       a risky change.
     */
    if (!pim->ImageMask && (!pclxl_can_handle_color_space(pcs)
                            || (bits_per_pixel == 32 && dev->icc_struct
                                && !dev->icc_struct->usefastcolor))
        && pclxl_can_icctransform(pim) && pcs->cmm_icc_profile_data) {
        gsicc_rendering_param_t rendering_params;

        rendering_params.black_point_comp = pgs->blackptcomp;
        rendering_params.graphics_type_tag = GS_IMAGE_TAG;
        rendering_params.rendering_intent = pgs->renderingintent;
        pie->icclink = gsicc_get_link(pgs, dev, pcs, NULL /*des */ ,
                                      &rendering_params, pgs->memory);
    } else
        pie->icclink = NULL;
    *pinfo = (gx_image_enum_common_t *) pie;
    {
        gs_logical_operation_t lop = pgs->log_op;

        if (pim->ImageMask) {
            const byte *palette = (const byte *)
                (pim->Decode[0] ? "\377\000" : "\000\377");
            gx_color_index foreground = gx_dc_pure_color(pdcolor);

            code = gdev_vector_update_fill_color(vdev, NULL,    /* use process color */
                                                 pdcolor);
            if (code < 0)
                goto fail;
            /* This is similiar to the copy_mono white-on-mask,
             * except we are drawing white on the black of a black/white mask,
             * so we invert source, compared to copy_mono */
            if (foreground == ((gx_color_index)1 << dev->color_info.depth) - 1) {       /* white */
                lop = rop3_not(rop3_S) | (rop3_D & rop3_S);
            } else if (foreground == 0) {       /* black */
                lop = (rop3_S & rop3_D);
            } else
                lop |= rop3_S | lop_S_transparent;

            code = gdev_vector_update_log_op(vdev, lop);
            if (code < 0)
                goto fail;
            pclxl_set_color_palette(xdev, eGray, palette, 2);
        } else {
            if (bits_per_pixel == 24 || bits_per_pixel == 32) {
                code = gdev_vector_update_log_op
                    (vdev,
                     (pim->CombineWithColor ? lop : rop3_know_T_0(lop)));
                if (code < 0)
                    goto fail;
                if (dev->color_info.num_components == 1) {
                    pclxl_set_color_space(xdev, eGray);
                } else {
                    pclxl_set_color_space(xdev, eRGB);
                }
            } else {
                int bpc = pim->BitsPerComponent;
                int num_components =
                    pie->plane_depths[0] * pie->num_planes / bpc;
                int sample_max = (1 << bpc) - 1;
                byte palette[256 * 3];
                int i;

                code = gdev_vector_update_log_op
                    (vdev,
                     (pim->CombineWithColor ? lop : rop3_know_T_0(lop)));
                if (code < 0)
                    goto fail;
                for (i = 0; i < 1 << bits_per_pixel; ++i) {
                    gs_client_color cc;
                    gx_device_color devc;
                    int cv = i, j;
                    gx_color_index ci;

                    for (j = num_components - 1; j >= 0; cv >>= bpc, --j)
                        cc.paint.values[j] = pim->Decode[j * 2] +
                            (cv & sample_max) *
                            (pim->Decode[j * 2 + 1] - pim->Decode[j * 2]) /
                            sample_max;
                    (*pcs->type->remap_color)
                        (&cc, pcs, &devc, pgs, dev, gs_color_select_source);
                    if (!gx_dc_is_pure(&devc))
                        return_error(gs_error_Fatal);
                    ci = gx_dc_pure_color(&devc);
                    if (dev->color_info.num_components == 1) {
                        palette[i] = (byte) ci;
                    } else {
                        byte *ppal = &palette[i * 3];

                        ppal[0] = (byte) (ci >> 16);
                        ppal[1] = (byte) (ci >> 8);
                        ppal[2] = (byte) ci;
                    }
                }
                if (dev->color_info.num_components == 1)
                    pclxl_set_color_palette(xdev, eGray, palette,
                                            1 << bits_per_pixel);
                else
                    pclxl_set_color_palette(xdev, eRGB, palette,
                                            3 << bits_per_pixel);
            }
        }
    }
    return 0;
  fail:
    gs_free_object(mem, row_data, "pclxl_begin_image(rows)");
    gs_free_object(mem, pie, "pclxl_begin_image");
  use_default:
    if (dev->color_info.num_components == 1)
        pclxl_set_color_space(xdev, eGray);
    else
        pclxl_set_color_space(xdev, eRGB);
    return gx_default_begin_typed_image(dev, pgs, pmat, pic, prect,
                                        pdcolor, pcpath, mem, pinfo);
}

/* Write one strip of an image, from pie->rows.first_y to pie->y. */
static int
image_transform_x(const pclxl_image_enum_t * pie, int sx)
{
    return (int)((pie->mat.tx + sx * pie->mat.xx + 0.5) /
                 ((const gx_device_pclxl *)pie->dev)->scale.x);
}
static int
image_transform_y(const pclxl_image_enum_t * pie, int sy)
{
    return (int)((pie->mat.ty + sy * pie->mat.yy + 0.5) /
                 ((const gx_device_pclxl *)pie->dev)->scale.y);
}

static int
pclxl_image_write_rows(pclxl_image_enum_t * pie)
{
    gx_device_pclxl *const xdev = (gx_device_pclxl *) pie->dev;
    stream *s = pclxl_stream(xdev);
    int y = pie->rows.first_y;
    int h = pie->y - y;
    int xo = image_transform_x(pie, 0);
    int yo = image_transform_y(pie, y);
    int dw = image_transform_x(pie, pie->width) - xo;
    int dh = image_transform_y(pie, y + h) - yo;
    int rows_raster = pie->rows.raster;
    int offset_lastflippedstrip = 0;

    if (pie->flipped) {
        yo = -yo - dh;
        if (!pie->icclink)
            offset_lastflippedstrip =
                pie->rows.raster * (pie->rows.num_rows - h);
        else
            offset_lastflippedstrip =
                (pie->rows.raster / (pie->bits_per_pixel >> 3))
                * xdev->color_info.num_components * (pie->rows.num_rows - h);
    };

    if (dw <= 0 || dh <= 0)
        return 0;
    pclxl_set_cursor(xdev, xo, yo);
    if (pie->bits_per_pixel == 24) {
        static const byte ci_[] = {
            DA(pxaColorDepth),
            DUB(eDirectPixel), DA(pxaColorMapping)
        };

        px_put_ub(s, eBit_values[8]);
        PX_PUT_LIT(s, ci_);
        if (xdev->color_info.depth == 8) {
            rows_raster /= 3;
            if (!pie->icclink) {
                byte *in = pie->rows.data + offset_lastflippedstrip;
                byte *out = pie->rows.data + offset_lastflippedstrip;
                int i;
                int j;

                for (j = 0; j < h; j++) {
                    for (i = 0; i < rows_raster; i++) {
                        *out =
                            (byte) (((*(in + 0) * (ulong) lum_red_weight) +
                                     (*(in + 1) * (ulong) lum_green_weight) +
                                     (*(in + 2) * (ulong) lum_blue_weight) +
                                     (lum_all_weights / 2)) /
                                    lum_all_weights);
                        in += 3;
                        out++;
                    }
                }
            }
        }
    } else if (pie->bits_per_pixel == 32) {
        static const byte ci_[] = {
            DA(pxaColorDepth),
            DUB(eDirectPixel), DA(pxaColorMapping)
        };

        px_put_ub(s, eBit_values[8]);
        PX_PUT_LIT(s, ci_);
        if (xdev->color_info.depth == 8) {
            /* CMYK to Gray */
            rows_raster /= 4;
            if (!pie->icclink) {
                byte *in = pie->rows.data + offset_lastflippedstrip;
                byte *out = pie->rows.data + offset_lastflippedstrip;
                int i;
                int j;

                for (j = 0; j < h; j++) {
                    for (i = 0; i < rows_raster; i++) {
                        /* DeviceCMYK to DeviceGray */
                        int v =
                            (255 - (*(in + 3))) * (int)lum_all_weights
                            + (lum_all_weights / 2)
                            - (*(in + 0) * (int)lum_red_weight)
                            - (*(in + 1) * (int)lum_green_weight)
                            - (*(in + 2) * (int)lum_blue_weight);

                        *out = max(v, 0) / lum_all_weights;
                        in += 4;
                        out++;
                    }
                }
            }
        } else {
            /* CMYK to RGB */
            rows_raster /= 4;
            if (!pie->icclink) {
                byte *in = pie->rows.data + offset_lastflippedstrip;
                byte *out = pie->rows.data + offset_lastflippedstrip;
                int i;
                int j;

                for (j = 0; j < h; j++) {
                    for (i = 0; i < rows_raster; i++) {
                        /* DeviceCMYK to DeviceRGB */
                        int r = (int)255 - (*(in + 0)) - (*(in + 3));
                        int g = (int)255 - (*(in + 1)) - (*(in + 3));
                        int b = (int)255 - (*(in + 2)) - (*(in + 3));

                        /* avoid trashing the first 12->9 conversion */
                        *out = max(r, 0);
                        out++;
                        *out = max(g, 0);
                        out++;
                        *out = max(b, 0);
                        out++;
                        in += 4;
                    }
                }
            }
            rows_raster *= 3;
        }
    } else {
        static const byte ii_[] = {
            DA(pxaColorDepth),
            DUB(eIndexedPixel), DA(pxaColorMapping)
        };
        px_put_ub(s, eBit_values[pie->bits_per_pixel]);
        PX_PUT_LIT(s, ii_);
    }
    pclxl_write_begin_image(xdev, pie->width, h, dw, dh);
    /* 8-bit gray image may compress with jpeg, but we
       cannot tell if it is 8-bit gray or 8-bit indexed */
    pclxl_write_image_data(xdev, pie->rows.data + offset_lastflippedstrip, 0,
                           rows_raster, rows_raster << 3, 0, h,
                           ((pie->bits_per_pixel == 24
                             || pie->bits_per_pixel == 32) ? true : false));
    pclxl_write_end_image(xdev);
    return 0;
}

/* Process the next piece of an image. */
static int
pclxl_image_plane_data(gx_image_enum_common_t * info,
                       const gx_image_plane_t * planes, int height,
                       int *rows_used)
{
    pclxl_image_enum_t *pie = (pclxl_image_enum_t *) info;
    int data_bit = planes[0].data_x * info->plane_depths[0];
    int width_bits = pie->width * info->plane_depths[0];
    int i;

    /****** SHOULD HANDLE NON-BYTE-ALIGNED DATA ******/
    if (width_bits != pie->bits_per_row || (data_bit & 7) != 0)
        return_error(gs_error_rangecheck);
    if (height > pie->height - pie->y)
        height = pie->height - pie->y;
    for (i = 0; i < height; pie->y++, ++i) {
        int flipped_strip_offset;

        if (pie->y - pie->rows.first_y == pie->rows.num_rows) {
            int code = pclxl_image_write_rows(pie);

            if (code < 0)
                return code;
            pie->rows.first_y = pie->y;
        }
        flipped_strip_offset = (pie->flipped ?
                                (pie->rows.num_rows -
                                 (pie->y - pie->rows.first_y) -
                                 1) : (pie->y - pie->rows.first_y));
        if (!pie->icclink)
            memcpy(pie->rows.data +
                   pie->rows.raster * flipped_strip_offset,
                   planes[0].data + planes[0].raster * i + (data_bit >> 3),
                   pie->rows.raster);
        else {
            gsicc_bufferdesc_t input_buff_desc;
            gsicc_bufferdesc_t output_buff_desc;
            int pixels_per_row =
                pie->rows.raster / (pie->bits_per_pixel >> 3);
            int out_raster_stride =
                pixels_per_row * info->dev->color_info.num_components;
            gsicc_init_buffer(&input_buff_desc,
                              (pie->bits_per_pixel >> 3) /*num_chan */ ,
                              1 /*bytes_per_chan */ ,
                              false /*has_alpha */ , false /*alpha_first */ ,
                              false /*is_planar */ ,
                              0 /*plane_stride */ ,
                              pie->rows.raster /*row_stride */ ,
                              1 /*num_rows */ ,
                              pixels_per_row /*pixels_per_row */ );
            gsicc_init_buffer(&output_buff_desc,
                              info->dev->color_info.num_components, 1, false,
                              false, false, 0, out_raster_stride, 1,
                              pixels_per_row);
            gscms_transform_color_buffer(info->dev, pie->icclink,
                                         &input_buff_desc, &output_buff_desc,
                                         (void *)(planes[0].data +
                                                  planes[0].raster * i +
                                                  (data_bit >> 3)) /*src */ ,
                                         pie->rows.data + out_raster_stride * flipped_strip_offset      /*des */
                                         );
        }
    }
    *rows_used = height;
    return pie->y >= pie->height;
}

/* Clean up by releasing the buffers. */
static int
pclxl_image_end_image(gx_image_enum_common_t * info, bool draw_last)
{
    pclxl_image_enum_t *pie = (pclxl_image_enum_t *) info;
    int code = 0;

    /* Write the final strip, if any. */
    if (pie->y > pie->rows.first_y && draw_last)
        code = pclxl_image_write_rows(pie);
    if (draw_last) {
        gx_device_pclxl *xdev = (gx_device_pclxl *) info->dev;
        stream *s = pclxl_stream(xdev);

        switch (xdev->state_rotated) {
            case 1:
                xdev->state_rotated = 0;
                px_put_ss(s, -90);
                px_put_ac(s, pxaPageAngle, pxtSetPageRotation);
                break;
            case -1:
                xdev->state_rotated = 0;
                px_put_ss(s, +90);
                px_put_ac(s, pxaPageAngle, pxtSetPageRotation);
                break;
            case 2:
                xdev->state_rotated = 0;
                px_put_ss(s, -180);
                px_put_ac(s, pxaPageAngle, pxtSetPageRotation);
                break;
            case 0:
            default:
                /* do nothing */
                break;
        }
    }
    if (pie->icclink)
        gsicc_release_link(pie->icclink);
    gs_free_object(pie->memory, pie->rows.data, "pclxl_end_image(rows)");
    gx_image_free_enum(&info);
    return code;
}

/*
 * 'pclxl_get_params()' - Get pagedevice parameters.
 */

static int                      /* O - Error status */
pclxl_get_params(gx_device * dev,       /* I - Device info */
                 gs_param_list * plist)
{                               /* I - Parameter list */
    gx_device_pclxl *xdev;      /* PCL XL device */
    int code;                   /* Return code */
    gs_param_string s;          /* Temporary string value */

    /*
     * First process the "standard" page device parameters...
     */

    if ((code = gdev_vector_get_params(dev, plist)) < 0)
        return (code);

    /*
     * Then write the PCL-XL parameters...
     */

    xdev = (gx_device_pclxl *) dev;

    if ((code = param_write_bool(plist, "Duplex", &(xdev->Duplex))) < 0)
        return (code);

    if ((code = param_write_bool(plist, "ManualFeed",
				 &(xdev->ManualFeed))) < 0)
        return (code);

    if ((code = param_write_int(plist, "MediaPosition",
				&(xdev->MediaPosition))) < 0)
        return (code);

    param_string_from_string(s, xdev->MediaType);

    if ((code = param_write_string(plist, "MediaType", &s)) < 0)
        return (code);

    if ((code = param_write_bool(plist, "Staple", &(xdev->Staple))) < 0)
        return (code);

    if ((code = param_write_bool(plist, "Tumble", &(xdev->Tumble))) < 0)
        return (code);

    if ((code = param_write_int(plist, "CompressMode",
                                &(xdev->CompressMode))) < 0)
        return (code);

    if ((code =
         param_write_bool(plist, "iccTransform", &(xdev->iccTransform))) < 0)
        return (code);

    return (0);
}

/*
 * 'pclxl_put_params()' - Set pagedevice parameters.
 */

static int                      /* O - Error status */
pclxl_put_params(gx_device * dev,       /* I - Device info */
                 gs_param_list * plist)
{                               /* I - Parameter list */
    gx_device_pclxl *xdev;      /* PCL XL device */
    int code;                   /* Error code */
    int intval;                 /* Integer value */
    bool boolval;               /* Boolean value */
    gs_param_string stringval;  /* String value */
    bool ManualFeed;
    bool ManualFeed_set = false;
    int MediaPosition;
    bool MediaPosition_set = false;

    /*
     * Process PCL-XL driver parameters...
     */

    xdev = (gx_device_pclxl *) dev;

#define intoption(name, sname, type) \
  if ((code = param_read_int(plist, sname, &intval)) < 0) \
  { \
    if_debug1('|', "Error setting %s\n", sname); \
    param_signal_error(plist, sname, code); \
    return (code); \
  } \
  else if (code == 0) \
  { \
    if_debug2('|', "setting %s to %d\n", sname, intval); \
    xdev->name = (type)intval; \
  }

#define booloption(name, sname) \
  if ((code = param_read_bool(plist, sname, &boolval)) < 0) \
  { \
    if_debug1('|', "Error setting bool %s\n", sname);    \
    if ((code = param_read_null(plist, sname)) < 0) \
    { \
      if_debug1('|', "Error setting bool %s null\n", sname);     \
      param_signal_error(plist, sname, code); \
      return (code); \
    } \
    if (code == 0) \
      xdev->name = false; \
  } \
  else if (code == 0) {                                   \
    if_debug2('|', "setting %s to %d\n", sname, boolval); \
    xdev->name = (bool)boolval; \
  }

#define stringoption(name, sname)                                \
  if ((code = param_read_string(plist, sname, &stringval)) < 0)  \
    {                                                            \
      if_debug1('|', "Error setting %s string\n", sname);        \
      if ((code = param_read_null(plist, sname)) < 0)            \
        {                                                        \
          if_debug1('|', "Error setting %s null\n", sname);      \
          param_signal_error(plist, sname, code);                \
          return (code);                                         \
        }                                                        \
      if (code == 0) {                                           \
        if_debug1('|', "setting %s to empty\n", sname);          \
        xdev->name[0] = '\0';                                    \
      }                                                          \
    }                                                            \
  else if (code == 0) {                                          \
    strncpy(xdev->name, (const char *)(stringval.data),          \
            stringval.size);                                     \
    xdev->name[stringval.size] = '\0';                           \
    if_debug2('|', "setting %s to %s\n", sname, xdev->name);     \
  }

    /* We need to have *_set to distinguish defaults from explicitly sets */
    booloption(Duplex, "Duplex");
    if (code == 0)
        if (xdev->Duplex) {
            if_debug0('|', "round up page count\n");
            xdev->page = (xdev->page + 1) & ~1;
        }
    code = param_read_bool(plist, "ManualFeed", &ManualFeed);
    if (code == 0)
        ManualFeed_set = true;
    if (code >= 0) {
        code = param_read_int(plist, "MediaPosition", &MediaPosition);
        if (code == 0)
            MediaPosition_set = true;
        else if (code < 0) {
            if (param_read_null(plist, "MediaPosition") == 0) {
                code = 0;
            }
        }
    }
    stringoption(MediaType, "MediaType");
    if (code == 0) {
        xdev->MediaType_set = true;
        /* round up for duplex */
        if (strcmp(xdev->MediaType_old, xdev->MediaType)) {
            if_debug0('|', "round up page count\n");
            xdev->page = (xdev->page + 1) & ~1;
            strcpy(xdev->MediaType_old, xdev->MediaType);
        }
    }
    booloption(Staple, "Staple");
    booloption(Tumble, "Tumble");
    intoption(CompressMode, "CompressMode", int);

    booloption(iccTransform, "iccTransform");

    /*
     * Then process standard page device parameters...
     */

    if (code >= 0)
        if ((code = gdev_vector_put_params(dev, plist)) < 0)
            return (code);

    if (code >= 0) {
        if (ManualFeed_set) {
            xdev->ManualFeed = ManualFeed;
            xdev->ManualFeed_set = true;
        }
        if (MediaPosition_set) {
            xdev->MediaPosition = MediaPosition;
            xdev->MediaPosition_set = true;
            if (xdev->MediaPosition_old != xdev->MediaPosition) {
                if_debug0('|', "round up page count\n");
                xdev->page = (xdev->page+1) & ~1 ;
                xdev->MediaPosition_old = xdev->MediaPosition;
            }
        }
    }

    return (0);
}

static int pclxl_text_begin(gx_device * dev, gs_gstate * pgs,
                    const gs_text_params_t *text, gs_font * font,
                    const gx_clip_path * pcpath,
                    gs_text_enum_t ** ppte)
{
    font->dir->ccache.upper = 0;
    return gx_default_text_begin(dev, pgs, text, font, pcpath, ppte);
}
