/* Copyright (C) 2001-2025 Artifex Software, Inc.
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


/* Plane extraction device */
#include "gx.h"
#include "gserrors.h"
#include "gsbitops.h"
#include "gsrop.h"		/* for logical op access */
#include "gsstruct.h"
#include "gsutil.h"
#include "gxdcolor.h"
#include "gxcmap.h"		/* requires gxdcolor.h */
#include "gxdevice.h"
#include "gxdevmem.h"
#include "gxdither.h"
#include "gxgetbit.h"
#include "gxiparam.h"
#include "gxgstate.h"
#include "gsstate.h"
#include "gdevplnx.h"

/* Define the size of the locally allocated bitmap buffers. */
#define COPY_COLOR_BUF_SIZE 100
#define TILE_RECTANGLE_BUF_SIZE 100
#define COPY_ROP_SOURCE_BUF_SIZE 100
#define COPY_ROP_TEXTURE_BUF_SIZE 100

/* GC procedures */
static
ENUM_PTRS_WITH(device_plane_extract_enum_ptrs, gx_device_plane_extract *edev)
    ENUM_PREFIX(st_device_forward, 1);
case 0: ENUM_RETURN(gx_device_enum_ptr(edev->target));
ENUM_PTRS_END
static RELOC_PTRS_WITH(device_plane_extract_reloc_ptrs, gx_device_plane_extract *edev)
{
    RELOC_PREFIX(st_device_forward);
    edev->plane_dev = gx_device_reloc_ptr(edev->plane_dev, gcst);
}
RELOC_PTRS_END
public_st_device_plane_extract();

/* Driver procedures */
static dev_proc_open_device(plane_open_device);
static dev_proc_fill_rectangle(plane_fill_rectangle);
static dev_proc_copy_mono(plane_copy_mono);
static dev_proc_copy_color(plane_copy_color);
static dev_proc_copy_alpha(plane_copy_alpha);
static dev_proc_fill_path(plane_fill_path);
static dev_proc_stroke_path(plane_stroke_path);
static dev_proc_fill_mask(plane_fill_mask);
static dev_proc_fill_parallelogram(plane_fill_parallelogram);
static dev_proc_fill_triangle(plane_fill_triangle);
static dev_proc_strip_tile_rectangle(plane_strip_tile_rectangle);
static dev_proc_strip_copy_rop2(plane_strip_copy_rop2);
static dev_proc_begin_typed_image(plane_begin_typed_image);
static dev_proc_get_bits_rectangle(plane_get_bits_rectangle);

/* Device prototype */
static void
plane_initialize_device_procs(gx_device *dev)
{
    set_dev_proc(dev, open_device, plane_open_device);
    set_dev_proc(dev, fill_rectangle, plane_fill_rectangle);
    set_dev_proc(dev, copy_mono, plane_copy_mono);
    set_dev_proc(dev, copy_color, plane_copy_color);
    set_dev_proc(dev, copy_alpha, plane_copy_alpha);
    set_dev_proc(dev, fill_path, plane_fill_path);
    set_dev_proc(dev, stroke_path, plane_stroke_path);
    set_dev_proc(dev, fill_mask, plane_fill_mask);
    set_dev_proc(dev, fill_parallelogram, plane_fill_parallelogram);
    set_dev_proc(dev, fill_triangle, plane_fill_triangle);
    set_dev_proc(dev, strip_tile_rectangle, plane_strip_tile_rectangle);
    set_dev_proc(dev, strip_copy_rop2, plane_strip_copy_rop2);
    set_dev_proc(dev, begin_typed_image, plane_begin_typed_image);
    set_dev_proc(dev, get_bits_rectangle, plane_get_bits_rectangle);
    set_dev_proc(dev, composite, gx_no_composite); /* WRONG */

    /* Ideally the following would be initialized to the defaults
     * automatically, but this does not currently work. */
    set_dev_proc(dev, close_device, gx_default_close_device);
    set_dev_proc(dev, fill_trapezoid, gx_default_fill_trapezoid);
    set_dev_proc(dev, draw_thin_line, gx_default_draw_thin_line);
    set_dev_proc(dev, text_begin, gx_default_text_begin);
    set_dev_proc(dev, fill_rectangle_hl_color, gx_default_fill_rectangle_hl_color);
    set_dev_proc(dev, include_color_space, gx_default_include_color_space);
    set_dev_proc(dev, fill_linear_color_scanline, gx_default_fill_linear_color_scanline);
    set_dev_proc(dev, fill_linear_color_trapezoid, gx_default_fill_linear_color_trapezoid);
    set_dev_proc(dev, fill_linear_color_triangle, gx_default_fill_linear_color_triangle);
    set_dev_proc(dev, update_spot_equivalent_colors, gx_default_update_spot_equivalent_colors);
    set_dev_proc(dev, ret_devn_params, gx_default_ret_devn_params);
    set_dev_proc(dev, fillpage, gx_default_fillpage);
    set_dev_proc(dev, strip_tile_rect_devn, gx_default_strip_tile_rect_devn);
    set_dev_proc(dev, copy_alpha_hl_color, gx_default_copy_alpha_hl_color);
}

static const gx_device_plane_extract gs_plane_extract_device = {
    std_device_std_body(gx_device_plane_extract,
                        plane_initialize_device_procs, "plane_extract",
                        0, 0, 72, 72),
    { 0 },
    /* device-specific members */
    NULL,				/* target */
    NULL,				/* plane_dev */
    { 0 },				/* plane */
    0,					/* plane_white */
    0,					/* plane_mask */
    0,					/* plane_dev_is_memory */
    1 /*true*/				/* any_marks */
};

/* ---------------- Utilities ---------------- */

/* Extract the selected plane from a color (gx_color_index). */
#define COLOR_PIXEL(edev, color)\
  ( ((color) >> (edev)->plane.shift) & (edev)->plane_mask )
/* Do the same if the color might be transparent. */
#define TRANS_COLOR_PIXEL(edev, color)\
 ((color) == gx_no_color_index ? gx_no_color_index : COLOR_PIXEL(edev, color))

/*
 * Reduce the drawing color to one for the selected plane.
 * All we care about is whether the drawing operation should be skipped.
 */
typedef enum {
    REDUCE_SKIP,
    REDUCE_DRAW,
    REDUCE_FAILED			/* couldn't reduce */
} reduced_color_t;
#define REDUCE_PURE(edev, pixel)\
  ((pixel) == (edev)->plane_white && !(edev)->any_marks ?  REDUCE_SKIP :\
   ((edev)->any_marks = true, REDUCE_DRAW))
static reduced_color_t
reduce_drawing_color(gx_device_color *ppdc, gx_device_plane_extract *edev,
                     const gx_drawing_color *pdevc,
                     gs_logical_operation_t *plop)
{
    reduced_color_t reduced;

    if (gx_dc_is_pure(pdevc)) {
        gx_color_index pixel = COLOR_PIXEL(edev, gx_dc_pure_color(pdevc));

        set_nonclient_dev_color(ppdc, pixel);
        reduced = REDUCE_PURE(edev, pixel);
    } else if (gx_dc_is_binary_halftone(pdevc)) {
        gx_color_index pixel0 =
            TRANS_COLOR_PIXEL(edev, gx_dc_binary_color0(pdevc));
        gx_color_index pixel1 =
            TRANS_COLOR_PIXEL(edev, gx_dc_binary_color1(pdevc));

        if (pixel0 == pixel1) {
            set_nonclient_dev_color(ppdc, pixel0);
            reduced = REDUCE_PURE(edev, pixel0);
        } else {
            *ppdc = *pdevc;
            ppdc->colors.binary.color[0] = pixel0;
            ppdc->colors.binary.color[1] = pixel1;
            edev->any_marks = true;
            reduced = REDUCE_DRAW;
        }
    } else if (color_is_colored_halftone(pdevc)) {
        int plane = edev->plane.index;
        int i;

        *ppdc = *pdevc;
        for (i = 0; i < countof(ppdc->colors.colored.c_base); ++i)
            if (i != edev->plane.index) {
                ppdc->colors.colored.c_base[i] = 0;
                ppdc->colors.colored.c_level[i] = 0;
            }
        ppdc->colors.colored.plane_mask &= 1 << plane;
        if (ppdc->colors.colored.c_level[plane] == 0) {
            gx_devn_reduce_colored_halftone(ppdc, (gx_device *)edev);
            ppdc->colors.pure = COLOR_PIXEL(edev, ppdc->colors.pure);
            reduced = REDUCE_PURE(edev, gx_dc_pure_color(ppdc));
        } else {
            gx_devn_reduce_colored_halftone(ppdc, (gx_device *)edev);
            ppdc->colors.binary.color[0] =
                COLOR_PIXEL(edev, ppdc->colors.binary.color[0]);
            ppdc->colors.binary.color[1] =
                COLOR_PIXEL(edev, ppdc->colors.binary.color[1]);
            gx_color_load(ppdc, NULL, (gx_device *)edev);
            edev->any_marks = true;
            reduced = REDUCE_DRAW;
        }
    } else
        return REDUCE_FAILED;		/* can't handle it */
    if (*plop & lop_T_transparent) {
        /*
         * If the logical operation invokes transparency for the texture, we
         * must do some extra work, since a color that was originally opaque
         * may become transparent (white) if reduced to a single plane.  If
         * RasterOp transparency were calculated before halftoning, life
         * would be easy: we would simply turn off texture transparency in
         * the logical operation iff the original (not reduced) color was
         * not white.  Unfortunately, RasterOp transparency is calculated
         * after halftoning.  (This is arguably wrong, but it's how we've
         * defined it.)  Therefore, if transparency is involved with a
         * white color or a halftone that can include white, we must keep
         * the entire pixel together for the RasterOp.
         */
        gx_color_index white = gx_device_white((gx_device *)edev);

        /*
         * Given that we haven't failed, the only possible colors at this
         * point are pure or binary halftone.
         */
        if (gx_dc_is_pure(ppdc)) {
            if (gx_dc_pure_color(pdevc) != white)
                *plop &= ~lop_T_transparent;
            else if (!gx_dc_is_pure(pdevc))
                return REDUCE_FAILED;
        } else {
            if (gx_dc_binary_color0(pdevc) != white &&
                gx_dc_binary_color1(pdevc) != white) {
                *plop &= ~lop_T_transparent;
            } else
                return REDUCE_FAILED;
        }
    }
    return reduced;
}

/*
 * Set up to create the plane-extracted bitmap corresponding to a
 * source or halftone pixmap.  If the bitmap doesn't fit in the locally
 * allocated buffer, we may either do the operation in pieces, or allocate
 * a buffer on the heap.  The control structure is:
 *	begin_tiling(&state, ...);
 *	do {
 *	    extract_partial_tile(&state);
 *	    ... process tile in buffer ...
 *	} while (next_tile(&state));
 *	end_tiling(&state);
 * If partial_ok is false, there is only a single tile, so the do ... while
 * is not used.
 */
typedef struct tiling_state_s {
        /* Save the original operands. */
    const gx_device_plane_extract *edev;
    const byte *data;
    int data_x;
    uint raster;
    int width, height;
    int dest_x;			/* only for copy_color, defaults to 0 */
        /* Define the (aligned) buffer for doing the operation. */
    struct tsb_ {
        byte *data;
        uint size;
        uint raster;
        bool on_heap;
    } buffer;
        /* Record the current tile available for processing. */
        /* The client may read these out. */
    gs_int_point offset;
    gs_int_point size;
        /* Record private tiling parameters. */
    int per_tile_width;
} tiling_state_t;

/*
 * Extract the plane's data from one subrectangle of a source tile.
 */
static inline int /* ignore the return value */
extract_partial_tile(const tiling_state_t *pts)
{
    const gx_device_plane_extract * const edev = pts->edev;
    bits_plane_t dest, source;

    dest.data.write = pts->buffer.data + pts->offset.y * pts->buffer.raster;
    dest.raster = pts->buffer.raster;
    dest.depth = edev->plane.depth;
    dest.x = pts->dest_x;

    source.data.read = pts->data + pts->offset.y * pts->raster;
    source.raster = pts->raster;
    source.depth = edev->color_info.depth;
    source.x = pts->data_x + pts->offset.x;

    bits_extract_plane(&dest, &source, edev->plane.shift,
                       pts->size.x, pts->size.y);
    return 0;
}

/*
 * Set up to start (possibly) tiling.  Return 0 if the entire tile fit,
 * 1 if a partial tile fit, or a negative error code.
 */
static int
begin_tiling(tiling_state_t *pts, gx_device_plane_extract *edev,
    const byte *data, int data_x, uint raster, int width, int height,
    byte *local_buffer, uint buffer_size, bool partial_ok)
{
    uint width_raster =
        bitmap_raster(width * edev->plane_dev->color_info.depth);
    int64_t full_size;

    if (check_64bit_multiply(width_raster, height, &full_size) != 0)
        return gs_note_error(gs_error_undefinedresult);

    pts->edev = edev;
    pts->data = data, pts->data_x = data_x, pts->raster = raster;
    pts->width = width, pts->height = height;
    pts->dest_x = 0;
    if (full_size <= buffer_size) {
        pts->buffer.data = local_buffer;
        pts->buffer.size = buffer_size;
        pts->buffer.raster = width_raster;
        pts->buffer.on_heap = false;
        pts->size.x = width, pts->size.y = height;
    } else if (partial_ok) {
        pts->buffer.data = local_buffer;
        pts->buffer.size = buffer_size;
        pts->buffer.on_heap = false;
        if (buffer_size >= width_raster) {
            pts->buffer.raster = width_raster;
            pts->size.x = width;
            pts->size.y = buffer_size / width_raster;
        } else {
            pts->buffer.raster = buffer_size & -align_bitmap_mod;
            pts->size.x =
                pts->buffer.raster * (8 / edev->plane_dev->color_info.depth);
            pts->size.y = 1;
        }
    } else {
        pts->buffer.data =
            gs_alloc_bytes(edev->memory, full_size, "begin_tiling");
        if (!pts->buffer.data)
            return_error(gs_error_VMerror);
        pts->buffer.size = full_size;
        pts->buffer.raster = width_raster;
        pts->buffer.on_heap = true;
        pts->size.x = width, pts->size.y = height;
    }
    pts->buffer.raster = width_raster;
    pts->offset.x = pts->offset.y = 0;
    pts->per_tile_width = pts->size.x;
    return pts->buffer.size < full_size;
}

/*
 * Advance to the next tile.  Return true if there are more tiles to do.
 */
static bool
next_tile(tiling_state_t *pts)
{
    if ((pts->offset.x += pts->size.x) >= pts->width) {
        if ((pts->offset.y += pts->size.y) >= pts->height)
            return false;
        pts->offset.x = 0;
        pts->size.x = pts->per_tile_width;
        if (pts->offset.y + pts->size.y >= pts->height)
            pts->size.y = pts->height - pts->offset.y;
    } else if (pts->offset.x + pts->size.x >= pts->width)
        pts->size.x = pts->width - pts->offset.x;
    return true;
}

/*
 * Finish tiling by freeing the buffer if necessary.
 */
static void
end_tiling(tiling_state_t *pts)
{
    if (pts->buffer.on_heap)
        gs_free_object(pts->edev->memory, pts->buffer.data, "end_tiling");
}

/* ---------------- Initialization ---------------- */

int
plane_device_init(gx_device_plane_extract *edev, gx_device *target,
    gx_device *plane_dev, const gx_render_plane_t *render_plane, bool clear)
{
    int code;
    /* Check for compatibility of the plane specification. */
    if (render_plane->depth > plane_dev->color_info.depth)
        return_error(gs_error_rangecheck);
    code = gx_device_init((gx_device *)edev,
                          (const gx_device *)&gs_plane_extract_device,
                          edev->memory, true);
    if (code < 0)
        return code;
    check_device_separable((gx_device *)edev);
    gx_device_forward_fill_in_procs((gx_device_forward *)edev);
    gx_device_set_target((gx_device_forward *)edev, target);
    gx_device_copy_params((gx_device *)edev, target);
    edev->plane_dev = plane_dev;
    gx_device_retain(plane_dev, true);
    edev->plane = *render_plane;
    plane_open_device((gx_device *)edev);
    if (clear) {
        dev_proc(plane_dev, fill_rectangle)
            (plane_dev, 0, 0, plane_dev->width, plane_dev->height,
             edev->plane_white);
        edev->any_marks = false;
    }
    return 0;
}

/* ---------------- Driver procedures ---------------- */

static int
plane_open_device(gx_device *dev)
{
    gx_device_plane_extract * const edev = (gx_device_plane_extract *)dev;
    gx_device * const plane_dev = edev->plane_dev;
    int plane_depth = plane_dev->color_info.depth;
    const gdev_mem_functions *fns =
                               gdev_mem_functions_for_bits(plane_depth);

    edev->plane_white = gx_device_white(plane_dev);
    edev->plane_mask = (1 << plane_depth) - 1;
    edev->plane_dev_is_memory = fns != NULL &&
                     dev_proc(plane_dev, copy_color) == fns->copy_color;
    /* We don't set or clear any_marks here: see ...init above. */
    return 0;
}

static int
plane_fill_rectangle(gx_device *dev,
    int x, int y, int w, int h, gx_color_index color)
{
    gx_device_plane_extract * const edev = (gx_device_plane_extract *)dev;
    gx_device * const plane_dev = edev->plane_dev;
    gx_color_index pixel = COLOR_PIXEL(edev, color);

    if (pixel != edev->plane_white)
        edev->any_marks = true;
    else if (!edev->any_marks)
        return 0;
    return dev_proc(plane_dev, fill_rectangle)
        (plane_dev, x, y, w, h, pixel);
}

static int
plane_copy_mono(gx_device *dev,
    const byte *data, int data_x, int raster, gx_bitmap_id id,
    int x, int y, int w, int h,
    gx_color_index color0, gx_color_index color1)
{
    gx_device_plane_extract * const edev = (gx_device_plane_extract *)dev;
    gx_device * const plane_dev = edev->plane_dev;
    gx_color_index pixel0 = TRANS_COLOR_PIXEL(edev, color0);
    gx_color_index pixel1 = TRANS_COLOR_PIXEL(edev, color1);

    if (pixel0 == pixel1)
        return plane_fill_rectangle(dev, x, y, w, h, color0);
    if ((pixel0 == edev->plane_white || pixel0 == gx_no_color_index) &&
        (pixel1 == edev->plane_white || pixel1 == gx_no_color_index)) {
        /* This operation will only write white. */
        if (!edev->any_marks)
            return 0;
    } else
        edev->any_marks = true;
    return dev_proc(plane_dev, copy_mono)
        (plane_dev, data, data_x, raster, id, x, y, w, h, pixel0, pixel1);
}

static int
plane_copy_color(gx_device *dev,
    const byte *data, int data_x, int raster, gx_bitmap_id id,
    int x, int y, int w, int h)
{
    gx_device_plane_extract * const edev = (gx_device_plane_extract *)dev;
    gx_device * const plane_dev = edev->plane_dev;
    tiling_state_t state;
    long buf[COPY_COLOR_BUF_SIZE / sizeof(long)];
    int code;

    if (edev->plane_dev_is_memory) {
        /* Reduce the source directly into the plane device. */
        gx_device_memory * const mdev = (gx_device_memory *)plane_dev;

        fit_copy(edev, data, data_x, raster, id, x, y, w, h);
        code = begin_tiling(&state, edev, data, data_x, raster, w, h,
                            scan_line_base(mdev, y), max_uint, false);
        if (code < 0)
            return code;
        state.dest_x = x;
        state.buffer.raster = mdev->raster;
        extract_partial_tile(&state);
        end_tiling(&state);
        edev->any_marks = true;
        return 0;
    }
    code = begin_tiling(&state, edev, data, data_x, raster,
                        w, h, (byte *)buf, sizeof(buf), true);
    if (code < 0)
        return code;
    do {
        extract_partial_tile(&state);
        code = dev_proc(plane_dev, copy_color)
            (plane_dev, state.buffer.data, 0, state.buffer.raster,
             gx_no_bitmap_id, x + state.offset.x, y + state.offset.y,
             state.size.x, state.size.y);
    } while (code >= 0 && next_tile(&state));
    end_tiling(&state);
    edev->any_marks = true;
    return code;
}

static int
plane_copy_alpha(gx_device *dev, const byte *data, int data_x,
    int raster, gx_bitmap_id id, int x, int y, int w, int h,
    gx_color_index color, int depth)
{
    gx_device_plane_extract * const edev = (gx_device_plane_extract *)dev;
    gx_device * const plane_dev = edev->plane_dev;
    gx_color_index pixel = COLOR_PIXEL(edev, color);

    if (pixel != edev->plane_white)
        edev->any_marks = true;
    else if (!edev->any_marks)
        return 0;
    return dev_proc(plane_dev, copy_alpha)
        (plane_dev, data, data_x, raster, id, x, y, w, h, pixel, depth);
}

static int
plane_fill_path(gx_device *dev,
    const gs_gstate *pgs, gx_path *ppath,
    const gx_fill_params *params,
    const gx_drawing_color *pdevc, const gx_clip_path *pcpath)
{
    gx_device_plane_extract * const edev = (gx_device_plane_extract *)dev;
    gx_device * const plane_dev = edev->plane_dev;
    gs_logical_operation_t lop_orig =
        gs_current_logical_op((const gs_gstate *)pgs);
    gs_logical_operation_t lop = lop_orig;
    gx_device_color dcolor;

    switch (reduce_drawing_color(&dcolor, edev, pdevc, &lop)) {
    case REDUCE_SKIP:
        return 0;
    case REDUCE_DRAW: {
        gs_gstate lopgs;
        const gs_gstate *pgs_draw = pgs;

        if (lop != lop_orig) {
            lopgs = *pgs;
            gs_set_logical_op((gs_gstate *)&lopgs, lop);
            pgs_draw = &lopgs;
        }
        return dev_proc(plane_dev, fill_path)
            (plane_dev, pgs_draw, ppath, params, &dcolor, pcpath);
    }
    default /*REDUCE_FAILED*/:
        return gx_default_fill_path(dev, pgs, ppath, params, pdevc, pcpath);
    }
}

static int
plane_stroke_path(gx_device *dev,
    const gs_gstate *pgs, gx_path *ppath,
    const gx_stroke_params *params,
    const gx_drawing_color *pdevc, const gx_clip_path *pcpath)
{
    gx_device_plane_extract * const edev = (gx_device_plane_extract *)dev;
    gx_device * const plane_dev = edev->plane_dev;
    gs_logical_operation_t lop_orig =
        gs_current_logical_op((const gs_gstate *)pgs);
    gs_logical_operation_t lop = lop_orig;
    gx_device_color dcolor;

    switch (reduce_drawing_color(&dcolor, edev, pdevc, &lop)) {
    case REDUCE_SKIP:
        return 0;
    case REDUCE_DRAW: {
        gs_gstate lopgs;
        const gs_gstate *pgs_draw = pgs;

        if (lop != lop_orig) {
            lopgs = *pgs;
            gs_set_logical_op((gs_gstate *)&lopgs, lop);
            pgs_draw = &lopgs;
        }
        return dev_proc(plane_dev, stroke_path)
            (plane_dev, pgs_draw, ppath, params, &dcolor, pcpath);
    }
    default /*REDUCE_FAILED*/:
        return gx_default_stroke_path(dev, pgs, ppath, params, pdevc, pcpath);
    }
}

static int
plane_fill_mask(gx_device *dev,
    const byte *data, int data_x, int raster, gx_bitmap_id id,
    int x, int y, int w, int h,
    const gx_drawing_color *pdcolor, int depth,
    gs_logical_operation_t lop, const gx_clip_path *pcpath)
{
    gx_device_plane_extract * const edev = (gx_device_plane_extract *)dev;
    gx_device * const plane_dev = edev->plane_dev;
    gx_device_color dcolor;

    switch (reduce_drawing_color(&dcolor, edev, pdcolor, &lop)) {
    case REDUCE_SKIP:
        return 0;
    case REDUCE_DRAW:
        return dev_proc(plane_dev, fill_mask)
            (plane_dev, data, data_x, raster, gx_no_bitmap_id, x, y, w, h,
             &dcolor, depth, lop, pcpath);
    default /*REDUCE_FAILED*/:
        return gx_default_fill_mask(dev, data, data_x, raster, gx_no_bitmap_id,
                                    x, y, w, h, &dcolor, depth, lop, pcpath);
    }
}

static int
plane_fill_parallelogram(gx_device * dev,
    fixed px, fixed py, fixed ax, fixed ay, fixed bx, fixed by,
    const gx_drawing_color * pdcolor, gs_logical_operation_t lop)
{
    gx_device_plane_extract * const edev = (gx_device_plane_extract *)dev;
    gx_device * const plane_dev = edev->plane_dev;
    gx_device_color dcolor;

    switch (reduce_drawing_color(&dcolor, edev, pdcolor, &lop)) {
    case REDUCE_SKIP:
        return 0;
    case REDUCE_DRAW:
        return dev_proc(plane_dev, fill_parallelogram)
            (plane_dev, px, py, ax, ay, bx, by, &dcolor, lop);
    default /*REDUCE_FAILED*/:
        return gx_default_fill_parallelogram(dev, px, py, ax, ay, bx, by,
                                             pdcolor, lop);
    }
}

static int
plane_fill_triangle(gx_device * dev,
    fixed px, fixed py, fixed ax, fixed ay, fixed bx, fixed by,
    const gx_drawing_color * pdcolor, gs_logical_operation_t lop)
{
    gx_device_plane_extract * const edev = (gx_device_plane_extract *)dev;
    gx_device * const plane_dev = edev->plane_dev;
    gx_device_color dcolor;

    switch (reduce_drawing_color(&dcolor, edev, pdcolor, &lop)) {
    case REDUCE_SKIP:
        return 0;
    case REDUCE_DRAW:
        return dev_proc(plane_dev, fill_triangle)
            (plane_dev, px, py, ax, ay, bx, by, &dcolor, lop);
    default /*REDUCE_FAILED*/:
        return gx_default_fill_triangle(dev, px, py, ax, ay, bx, by,
                                        pdcolor, lop);
    }
}

static int
plane_strip_tile_rectangle(gx_device *dev,
    const gx_strip_bitmap *tiles, int x, int y, int w, int h,
    gx_color_index color0, gx_color_index color1,
    int phase_x, int phase_y)
{
    gx_device_plane_extract * const edev = (gx_device_plane_extract *)dev;
    gx_device * const plane_dev = edev->plane_dev;
    gx_color_index pixel0 = TRANS_COLOR_PIXEL(edev, color0);
    gx_color_index pixel1 = TRANS_COLOR_PIXEL(edev, color1);

    if (pixel0 == pixel1) {
        if (pixel0 != gx_no_color_index)
            return plane_fill_rectangle(dev, x, y, w, h, color0);
        /* The tile is a pixmap rather than a bitmap. */
        /* We should use the default implementation if it is small.... */
        {
            gx_strip_bitmap plane_tile;
            tiling_state_t state;
            long buf[TILE_RECTANGLE_BUF_SIZE / sizeof(long)];
            int code = begin_tiling(&state, edev, tiles->data, 0, tiles->raster,
                        tiles->size.x, tiles->size.y,
                                (byte *)buf, sizeof(buf), false);

            if (code < 0)
                return gx_default_strip_tile_rectangle(dev, tiles, x, y, w, h,
                                        color0, color1, phase_x, phase_y);
            extract_partial_tile(&state);
            plane_tile = *tiles;
            plane_tile.data = state.buffer.data;
            plane_tile.raster = state.buffer.raster;
            plane_tile.id = gx_no_bitmap_id;
            code = dev_proc(plane_dev, strip_tile_rectangle)
                (plane_dev, &plane_tile, x, y, w, h, pixel0, pixel1,
                 phase_x, phase_y);
            end_tiling(&state);
            edev->any_marks = true;
            return code;
        }
    }
    if ((pixel0 == edev->plane_white || pixel0 == gx_no_color_index) &&
        (pixel1 == edev->plane_white || pixel1 == gx_no_color_index)) {
        /* This operation will only write white. */
        if (!edev->any_marks)
            return 0;
    } else
        edev->any_marks = true;
    return dev_proc(plane_dev, strip_tile_rectangle)
        (plane_dev, tiles, x, y, w, h, pixel0, pixel1, phase_x, phase_y);
}

static int
plane_strip_copy_rop2(gx_device *dev,
    const byte *sdata, int sourcex, uint sraster, gx_bitmap_id id,
    const gx_color_index *scolors,
    const gx_strip_bitmap *textures, const gx_color_index *tcolors,
    int x, int y, int w, int h,
    int phase_x, int phase_y, gs_logical_operation_t lop,
    uint plane_height)
{
    gx_device_plane_extract * const edev = (gx_device_plane_extract *)dev;
    gx_device * const plane_dev = edev->plane_dev;
    gs_rop3_t rop = lop_sanitize(lop);
    struct crp_ {
        gx_color_index pixels[2];
        gx_color_index *colors;
        tiling_state_t state;
    } source, texture;
    long sbuf[COPY_ROP_SOURCE_BUF_SIZE / sizeof(long)];
    long tbuf[COPY_ROP_TEXTURE_BUF_SIZE / sizeof(long)];
    const byte *plane_source;
    uint plane_raster = 0xbaadf00d; /* Initialize against indeterminizm. */
    gx_strip_bitmap plane_texture;
    const gx_strip_bitmap *plane_textures = NULL;
    int code;

    if (!rop3_uses_S(rop)) {
        sdata = 0;
        source.colors = 0;
    } else if (scolors) {
        source.pixels[0] = COLOR_PIXEL(edev, scolors[0]);
        source.pixels[1] = COLOR_PIXEL(edev, scolors[1]);
        if (source.pixels[0] == source.pixels[1])
            sdata = 0;
        source.colors = source.pixels;
    }
    else
        source.colors = 0;
    if (!rop3_uses_T(rop)) {
        textures = 0;
        texture.colors = 0;
    } else if (tcolors) {
        texture.pixels[0] = COLOR_PIXEL(edev, tcolors[0]);
        texture.pixels[1] = COLOR_PIXEL(edev, tcolors[1]);
        if (texture.pixels[0] == texture.pixels[1])
            textures = 0;
        texture.colors = texture.pixels;
    }
    else
        texture.colors = 0;
    if (sdata) {
        code = begin_tiling(&source.state, edev, sdata, sourcex, sraster, w, y,
                            (byte *)sbuf, sizeof(sbuf), true);
        if (code < 0)
            return gx_default_strip_copy_rop2(dev, sdata, sourcex, sraster, id,
                                              scolors, textures, tcolors,
                                              x, y, w, h, phase_x, phase_y, rop,
                                              plane_height);
        plane_source = source.state.buffer.data;
        plane_raster = source.state.buffer.raster;
    } else
        plane_source = 0;
    if (textures) {
        code = begin_tiling(&texture.state, edev, textures->data, 0,
                            textures->raster, textures->size.x,
                            textures->size.y, (byte *)tbuf, sizeof(tbuf),
                            false);
        if (code < 0) {
            if (plane_source)
                end_tiling(&source.state);
            return code;
        }
        plane_texture = *textures;
        plane_texture.data = texture.state.buffer.data;
        plane_texture.raster = texture.state.buffer.raster;
        plane_textures = &plane_texture;
    }
    if (textures)
        extract_partial_tile(&texture.state);
    do {
        if (sdata)
            extract_partial_tile(&source.state);
        code = dev_proc(plane_dev, strip_copy_rop2)
            (plane_dev, plane_source, sourcex, plane_raster, gx_no_bitmap_id,
             source.colors, plane_textures, texture.colors,
             x, y, w, h, phase_x, phase_y, rop, plane_height);
    } while (code >= 0 && sdata && next_tile(&source.state));
    if (textures)
        end_tiling(&texture.state);
    if (sdata)
        end_tiling(&source.state);
    return code;
}

/* ---------------- Images ---------------- */

/* Define the state for image rendering. */
typedef struct plane_image_enum_s {
    gx_image_enum_common;
    gx_image_enum_common_t *info; /* plane device enumerator */
    gs_gstate *pgs_image;	/* modified gs_gstate state */
} plane_image_enum_t;
/* Note that we include the pgs_image which is 'bytes' type (not gs_gstate) */
/* It still needs to be traced so that a GC won't free it prematurely.      */
gs_private_st_suffix_add2(st_plane_image_enum, plane_image_enum_t,
  "plane_image_enum_t", plane_image_enum_enum_ptrs,
  plane_image_enum_reloc_ptrs, st_gx_image_enum_common, info, pgs_image);

/*
 * Reduce drawing colors returned by color mapping.  Note that these
 * assume that the call of reduce_drawing_color will not fail:
 * plane_begin_typed_image must ensure this.
 *
 * In the gs_gstate passed to these procedures, the client data is
 * the plane_image_enum_t.
 */

static void
plane_cmap_gray(frac gray, gx_device_color * pdc,
    const gs_gstate *pgs_image, gx_device *dev, gs_color_select_t select)
{
    const plane_image_enum_t *ppie =
        (const plane_image_enum_t *)pgs_image->client_data;
    gx_device_plane_extract * const edev =
        (gx_device_plane_extract *)ppie->dev;
    gs_logical_operation_t lop = gs_current_logical_op_inline(pgs_image);
    gx_device_color dcolor;

    gx_remap_concrete_gray(gray, &dcolor, ppie->pgs,
                           (gx_device *)edev, select);
    reduce_drawing_color(pdc, edev, &dcolor, &lop);
}
static void
plane_cmap_rgb(frac r, frac g, frac b, gx_device_color * pdc,
    const gs_gstate *pgs_image, gx_device *dev, gs_color_select_t select)
{
    const plane_image_enum_t *ppie =
        (const plane_image_enum_t *)pgs_image->client_data;
    gx_device_plane_extract * const edev =
        (gx_device_plane_extract *)ppie->dev;
    gs_logical_operation_t lop = gs_current_logical_op_inline(pgs_image);
    gx_device_color dcolor;

    gx_remap_concrete_rgb(r, g, b, &dcolor, ppie->pgs,
                          (gx_device *)edev, select);
    reduce_drawing_color(pdc, edev, &dcolor, &lop);
}
static void
plane_cmap_cmyk(frac c, frac m, frac y, frac k, gx_device_color * pdc,
    const gs_gstate *pgs_image, gx_device *dev, gs_color_select_t select,
    const gs_color_space *source_pcs)
{
    const plane_image_enum_t *ppie =
        (const plane_image_enum_t *)pgs_image->client_data;
    gx_device_plane_extract * const edev =
        (gx_device_plane_extract *)ppie->dev;
    gs_logical_operation_t lop = gs_current_logical_op_inline(pgs_image);
    gx_device_color dcolor;

    gx_remap_concrete_cmyk(c, m, y, k, &dcolor, ppie->pgs,
                           (gx_device *)edev, select, NULL);
    reduce_drawing_color(pdc, edev, &dcolor, &lop);
}
static bool
plane_cmap_is_halftoned(const gs_gstate *pgs_image, gx_device *dev)
{
    return false;
}

static const gx_color_map_procs plane_color_map_procs = {
    plane_cmap_gray, plane_cmap_rgb, plane_cmap_cmyk,
    NULL, NULL, plane_cmap_is_halftoned
};
static const gx_color_map_procs *
plane_get_cmap_procs(const gs_gstate *pgs, const gx_device *dev)
{
    return &plane_color_map_procs;
}

/* Define the image processing procedures. */
static image_enum_proc_plane_data(plane_image_plane_data);
static image_enum_proc_end_image(plane_image_end_image);
static const gx_image_enum_procs_t plane_image_enum_procs = {
    plane_image_plane_data, plane_image_end_image
};

static int
plane_begin_typed_image(gx_device * dev,
                        const gs_gstate * pgs, const gs_matrix * pmat,
                   const gs_image_common_t * pic, const gs_int_rect * prect,
              const gx_drawing_color * pdcolor, const gx_clip_path * pcpath,
                      gs_memory_t * memory, gx_image_enum_common_t ** pinfo)
{
    /*
     * For images, we intercept the gs_gstate's cmap_procs and apply
     * reduce_drawing_color to the colors as they are returned to the image
     * processing code.  For reasons explained above, we can't do this in
     * some cases of RasterOp that include transparency.
     */
    gx_device_plane_extract * const edev = (gx_device_plane_extract *)dev;
    gs_logical_operation_t lop = gs_current_logical_op((const gs_gstate *)pgs);
    const gs_pixel_image_t *pim;
    plane_image_enum_t *info = 0;
    gs_gstate *pgs_image = 0;
    gx_device_color dcolor;
    bool uses_color = false;
    int code;

    /* We can only handle a limited set of image types. */
    switch (pic->type->index) {
    case 1: {
        const gs_image1_t * const pim1 = (const gs_image1_t *)pic;

        if (pim1->Alpha != gs_image_alpha_none)
            goto fail;
        uses_color = pim1->ImageMask;
        break;
        }
    case 3:
    case 4:
        break;
    default:
        goto fail;
    }
    pim = (const gs_pixel_image_t *)pic;
    lop = lop_sanitize(lop);
    if (uses_color || (pim->CombineWithColor && lop_uses_T(lop))) {
        if (reduce_drawing_color(&dcolor, edev, pdcolor, &lop) ==
            REDUCE_FAILED)
            goto fail;
    } else {
        /*
         * The drawing color won't be used, but if RasterOp is involved,
         * it may still be accessed in some anomalous cases.
         */
        set_nonclient_dev_color(&dcolor, (gx_color_index)0);
    }
    info = gs_alloc_struct(memory, plane_image_enum_t, &st_plane_image_enum,
                           "plane_image_begin_typed(info)");
    pgs_image = gs_gstate_copy(pgs, memory);
    if (pgs_image == 0 || info == 0)
        goto fail;
    pgs_image->client_data = info;
    pgs_image->get_cmap_procs = plane_get_cmap_procs;
    code = dev_proc(edev->plane_dev, begin_typed_image)
        (edev->plane_dev, pgs_image, pmat, pic, prect,
         &dcolor, pcpath, memory, &info->info);
    if (code < 0)
        goto fail;
    *((gx_image_enum_common_t *)info) = *info->info;
    info->procs = &plane_image_enum_procs;
    info->dev = (gx_device *)edev;
    info->id = gs_next_ids(memory, 1);
    info->memory = memory;
    info->pgs = pgs;
    info->pgs_level = pgs->level;
    info->pgs_image = pgs_image;
    *pinfo = (gx_image_enum_common_t *)info;
    return code;
fail:
    gs_free_object(memory, pgs_image, "plane_image_begin_typed(pgs_image)");
    gs_free_object(memory, info, "plane_image_begin_typed(info)");
    return gx_default_begin_typed_image(dev, pgs, pmat, pic, prect,
                                        pdcolor, pcpath, memory, pinfo);
}

static int
plane_image_plane_data(gx_image_enum_common_t * info,
                       const gx_image_plane_t * planes, int height,
                       int *rows_used)
{
    plane_image_enum_t * const ppie = (plane_image_enum_t *)info;

    if (info->pgs!= NULL && info->pgs->level < info->pgs_level)
        return_error(gs_error_undefinedresult);

    return gx_image_plane_data_rows(ppie->info, planes, height, rows_used);
}

static int
plane_image_end_image(gx_image_enum_common_t * info, bool draw_last)
{
    plane_image_enum_t * const ppie = (plane_image_enum_t *)info;
    int code = gx_image_end(ppie->info, draw_last);

    ppie->pgs_image->client_data = NULL;       /* this isn't a complete client_data struct */
    gs_free_object(ppie->memory, ppie->pgs_image,
                   "plane_image_end_image(pgs_image)");
    gx_image_free_enum(&info);
    return code;
}

/* ---------------- Reading back bits ---------------- */

static int
plane_get_bits_rectangle(gx_device * dev, const gs_int_rect * prect,
                         gs_get_bits_params_t * params)
{
    gx_device_plane_extract * const edev = (gx_device_plane_extract *)dev;
    gx_device * const plane_dev = edev->plane_dev;
    int plane_index = edev->plane.index;
    gs_get_bits_options_t options = params->options;
    gs_get_bits_params_t plane_params;
    uchar plane;
    int code;

    /*
     * The only real option that this device supports is single-plane
     * retrieval.  However, for the default case of RasterOp, it must be
     * able to return chunky pixels in which the other components are
     * arbitrary (but might as well be zero).
     */
    if ((options & GB_PACKING_PLANAR) && (options & GB_SELECT_PLANES)) {
        if (params->data[plane_index] == 0)
            return gx_default_get_bits_rectangle(dev, prect, params);
        /* If the caller wants any other plane(s), punt. */
        for (plane = 0; plane < dev->color_info.num_components; ++plane)
            if (plane != plane_index && params->data[plane] != 0)
                return gx_default_get_bits_rectangle(dev, prect, params);
        /* Pass the request on to the plane device. */
        plane_params = *params;
        plane_params.options =
            (options & ~(GB_PACKING_ALL | GB_SELECT_PLANES)) |
            GB_PACKING_CHUNKY;
        plane_params.data[0] = params->data[plane_index];
        code = dev_proc(plane_dev, get_bits_rectangle)
            (plane_dev, prect, &plane_params);
        if (code >= 0) {
            *params = plane_params;
            params->options = (params->options & ~GB_PACKING_ALL) |
                (GB_PACKING_PLANAR | GB_SELECT_PLANES);
            params->data[plane_index] = params->data[0];
            for (plane = 0; plane < dev->color_info.num_components; ++plane)
                if (plane != plane_index)
                    params->data[plane] = 0;
        }
    } else if (!(~options & (GB_COLORS_NATIVE | GB_ALPHA_NONE |
                             GB_PACKING_CHUNKY | GB_RETURN_COPY |
                             GB_ALIGN_STANDARD | GB_OFFSET_0 |
                             GB_RASTER_STANDARD))) {
        /* Expand the plane into chunky pixels. */
        bits_plane_t dest, source;

        dest.data.write = params->data[0];
        dest.raster =
            bitmap_raster((prect->q.x - prect->p.x) * dev->color_info.depth);
        dest.depth = edev->color_info.depth;
        dest.x = 0;

        /* not source.data, source.raster, source.x */
        source.depth = plane_dev->color_info.depth;

        plane_params = *params;
        plane_params.options = options &=
            (~(GB_COLORS_ALL | GB_ALPHA_ALL | GB_PACKING_ALL |
               GB_RETURN_ALL | GB_ALIGN_ALL | GB_OFFSET_ALL | GB_RASTER_ALL) |
             GB_COLORS_NATIVE | GB_ALPHA_NONE | GB_PACKING_CHUNKY |
             /* Try for a pointer return the first time. */
             GB_RETURN_POINTER |
             GB_ALIGN_STANDARD |
             (GB_OFFSET_0 | GB_OFFSET_ANY) |
             (GB_RASTER_STANDARD | GB_RASTER_ANY));
        plane_params.raster = gx_device_raster(plane_dev, true);
        code = dev_proc(plane_dev, get_bits_rectangle)
            (plane_dev, prect, &plane_params);
        if (code >= 0) {
            /* Success, expand the plane into pixels. */
            source.data.read = plane_params.data[0];
            source.raster = plane_params.raster;
            source.x = params->x_offset;
            code = bits_expand_plane(&dest, &source, edev->plane.shift,
                                     prect->q.x - prect->p.x,
                                     prect->q.y - prect->p.y);
        }
        params->options = (options & ~GB_RETURN_POINTER) | GB_RETURN_COPY;
    } else
        return gx_default_get_bits_rectangle(dev, prect, params);
    return code;
}
