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


/* Implementation of (path-based) clipping */
#include "gx.h"
#include "gxdevice.h"
#include "gxclip.h"
#include "gxpath.h"
#include "gxcpath.h"
#include "gzcpath.h"

/* Define whether to look for vertical clipping regions. */
#define CHECK_VERTICAL_CLIPPING

/* ------ Rectangle list clipper ------ */

/* Device for clipping with a region. */
/* We forward non-drawing operations, but we must be sure to intercept */
/* all drawing operations. */
static dev_proc_open_device(clip_open);
static dev_proc_fill_rectangle(clip_fill_rectangle);
static dev_proc_fill_rectangle_hl_color(clip_fill_rectangle_hl_color);
static dev_proc_copy_mono(clip_copy_mono);
static dev_proc_copy_planes(clip_copy_planes);
static dev_proc_copy_color(clip_copy_color);
static dev_proc_copy_alpha(clip_copy_alpha);
static dev_proc_copy_alpha_hl_color(clip_copy_alpha_hl_color);
static dev_proc_fill_mask(clip_fill_mask);
static dev_proc_strip_tile_rectangle(clip_strip_tile_rectangle);
static dev_proc_strip_tile_rect_devn(clip_strip_tile_rect_devn);
static dev_proc_strip_copy_rop(clip_strip_copy_rop);
static dev_proc_strip_copy_rop2(clip_strip_copy_rop2);
static dev_proc_get_clipping_box(clip_get_clipping_box);
static dev_proc_get_bits_rectangle(clip_get_bits_rectangle);
static dev_proc_fill_path(clip_fill_path);
static dev_proc_transform_pixel_region(clip_transform_pixel_region);
static dev_proc_fill_stroke_path(clip_fill_stroke_path);

/* The device descriptor. */
static const gx_device_clip gs_clip_device =
{std_device_std_body(gx_device_clip, 0, "clipper",
                     0, 0, 1, 1),
 {clip_open,
  gx_forward_get_initial_matrix,
  gx_default_sync_output,
  gx_default_output_page,
  gx_default_close_device,
  gx_forward_map_rgb_color,
  gx_forward_map_color_rgb,
  clip_fill_rectangle,
  gx_default_tile_rectangle,
  clip_copy_mono,
  clip_copy_color,
  gx_default_draw_line,
  gx_default_get_bits,
  gx_forward_get_params,
  gx_forward_put_params,
  gx_forward_map_cmyk_color,
  gx_forward_get_xfont_procs,
  gx_forward_get_xfont_device,
  gx_forward_map_rgb_alpha_color,
  gx_forward_get_page_device,
  gx_forward_get_alpha_bits,
  clip_copy_alpha,
  gx_forward_get_band,
  gx_default_copy_rop,
  clip_fill_path,
  gx_default_stroke_path,
  clip_fill_mask,
  gx_default_fill_trapezoid,
  gx_default_fill_parallelogram,
  gx_default_fill_triangle,
  gx_default_draw_thin_line,
  gx_default_begin_image,
  gx_default_image_data,
  gx_default_end_image,
  clip_strip_tile_rectangle,
  clip_strip_copy_rop,
  clip_get_clipping_box,
  gx_default_begin_typed_image,
  clip_get_bits_rectangle,
  gx_forward_map_color_rgb_alpha,
  gx_forward_create_compositor,
  gx_forward_get_hardware_params,
  gx_default_text_begin,
  gx_default_finish_copydevice,
  NULL,			/* begin_transparency_group */
  NULL,			/* end_transparency_group */
  NULL,			/* begin_transparency_mask */
  NULL,			/* end_transparency_mask */
  NULL,			/* discard_transparency_layer */
  gx_forward_get_color_mapping_procs,
  gx_forward_get_color_comp_index,
  gx_forward_encode_color,
  gx_forward_decode_color,
  NULL,
  clip_fill_rectangle_hl_color,
  gx_forward_include_color_space,
  gx_default_fill_linear_color_scanline,
  gx_default_fill_linear_color_trapezoid,
  gx_default_fill_linear_color_triangle,
  gx_forward_update_spot_equivalent_colors,
  gx_forward_ret_devn_params,
  gx_forward_fillpage,
  NULL,                      /* push_transparency_state */
  NULL,                      /* pop_transparency_state */
  NULL,                      /* put_image */
  gx_forward_dev_spec_op,
  clip_copy_planes,          /* copy planes */
  gx_forward_get_profile,
  gx_forward_set_graphics_type_tag,
  clip_strip_copy_rop2,
  clip_strip_tile_rect_devn,
  clip_copy_alpha_hl_color,
  NULL,						/* process_page */
  clip_transform_pixel_region,
  clip_fill_stroke_path,
 }
};

/* Make a clipping device. */
void
gx_make_clip_device_on_stack(gx_device_clip * dev, const gx_clip_path *pcpath, gx_device *target)
{
    gx_device_init_on_stack((gx_device *)dev, (const gx_device *)&gs_clip_device, target->memory);
    dev->cpath = pcpath;
    dev->list = *gx_cpath_list(pcpath);
    dev->translation.x = 0;
    dev->translation.y = 0;
    dev->HWResolution[0] = target->HWResolution[0];
    dev->HWResolution[1] = target->HWResolution[1];
    dev->sgr = target->sgr;
    dev->target = target;
    dev->pad = target->pad;
    dev->log2_align_mod = target->log2_align_mod;
    dev->is_planar = target->is_planar;
    dev->graphics_type_tag = target->graphics_type_tag;	/* initialize to same as target */
    /* There is no finalization for device on stack so no rc increment */
    (*dev_proc(dev, open_device)) ((gx_device *)dev);
}

void
gx_destroy_clip_device_on_stack(gx_device_clip * dev)
{
    if (dev->cpath)
        ((gx_clip_path *)dev->cpath)->cached = (dev->current == &dev->list.single ? NULL : dev->current); /* Cast away const */
}

gx_device *
gx_make_clip_device_on_stack_if_needed(gx_device_clip * dev, const gx_clip_path *pcpath, gx_device *target, gs_fixed_rect *rect)
{
    /* Reduce area if possible */
    if (rect->p.x < pcpath->outer_box.p.x)
        rect->p.x = pcpath->outer_box.p.x;
    if (rect->q.x > pcpath->outer_box.q.x)
        rect->q.x = pcpath->outer_box.q.x;
    if (rect->p.y < pcpath->outer_box.p.y)
        rect->p.y = pcpath->outer_box.p.y;
    if (rect->q.y > pcpath->outer_box.q.y)
        rect->q.y = pcpath->outer_box.q.y;
    /* Check for area being trivially clipped away. */
    if (rect->p.x >= rect->q.x || rect->p.y >= rect->q.y)
        return NULL;
    if (pcpath->inner_box.p.x <= rect->p.x && pcpath->inner_box.p.y <= rect->p.y &&
        pcpath->inner_box.q.x >= rect->q.x && pcpath->inner_box.q.y >= rect->q.y)
    {
        /* Area is trivially included. No need for clip. */
        return target;
    }
    gx_device_init_on_stack((gx_device *)dev, (const gx_device *)&gs_clip_device, target->memory);
    dev->list = *gx_cpath_list(pcpath);
    dev->translation.x = 0;
    dev->translation.y = 0;
    dev->HWResolution[0] = target->HWResolution[0];
    dev->HWResolution[1] = target->HWResolution[1];
    dev->sgr = target->sgr;
    dev->target = target;
    dev->pad = target->pad;
    dev->log2_align_mod = target->log2_align_mod;
    dev->is_planar = target->is_planar;
    dev->graphics_type_tag = target->graphics_type_tag;	/* initialize to same as target */
    /* There is no finalization for device on stack so no rc increment */
    (*dev_proc(dev, open_device)) ((gx_device *)dev);
    return (gx_device *)dev;
}
void
gx_make_clip_device_in_heap(gx_device_clip * dev, const gx_clip_path *pcpath, gx_device *target,
                              gs_memory_t *mem)
{
    gx_device_init((gx_device *)dev, (const gx_device *)&gs_clip_device, mem, true);
    dev->list = *gx_cpath_list(pcpath);
    dev->translation.x = 0;
    dev->translation.y = 0;
    dev->HWResolution[0] = target->HWResolution[0];
    dev->HWResolution[1] = target->HWResolution[1];
    dev->sgr = target->sgr;
    dev->pad = target->pad;
    dev->log2_align_mod = target->log2_align_mod;
    dev->is_planar = target->is_planar;
    gx_device_set_target((gx_device_forward *)dev, target);
    gx_device_retain((gx_device *)dev, true); /* will free explicitly */
    (*dev_proc(dev, open_device)) ((gx_device *)dev);
}
/* Define debugging statistics for the clipping loops. */
#if defined(DEBUG) && !defined(GS_THREADSAFE)
struct stats_clip_s {
    long
         loops, out, in_y, in, in1, down, up, x, no_x;
} stats_clip;

static const uint clip_interval = 10000;

# define INCR(v) (++(stats_clip.v))
# define INCR_THEN(v, e) (INCR(v), (e))
#else
# define INCR(v) DO_NOTHING
# define INCR_THEN(v, e) (e)
#endif

/*
 * Enumerate the rectangles of the x,w,y,h argument that fall within
 * the clipping region.
 * NB: if the clip list is transposed, then x, y, xe, and ye are already
 *     transposed and will need to be switched for the call to "process"
 */
static int
clip_enumerate_rest(gx_device_clip * rdev,
                    int x, int y, int xe, int ye,
                    int (*process)(clip_callback_data_t * pccd,
                                   int xc, int yc, int xec, int yec),
                    clip_callback_data_t * pccd)
{
    gx_clip_rect *rptr = rdev->current;		/* const within algorithm */
    int yc;
    int code;

#if defined(DEBUG) && !defined(GS_THREADSAFE)
    if (INCR(loops) % clip_interval == 0 && gs_debug_c('q')) {
        dmprintf5(rdev->memory,
                  "[q]loops=%ld out=%ld in_y=%ld in=%ld in1=%ld\n",
                  stats_clip.loops, stats_clip.out, stats_clip.in,
                  stats_clip.in_y, stats_clip.in1);
        dmprintf4(rdev->memory,
                  "[q]   down=%ld up=%ld x=%ld no_x=%ld\n",
                  stats_clip.down, stats_clip.up, stats_clip.x,
                  stats_clip.no_x);
    }
#endif
    /*
     * Warp the cursor forward or backward to the first rectangle row
     * that could include a given y value.  Assumes rptr is set, and
     * updates it.  Specifically, after this loop, either rptr == 0 (if
     * the y value is greater than all y values in the list), or y <
     * rptr->ymax and either rptr->prev == 0 or y >= rptr->prev->ymax.
     * Note that y <= rptr->ymin is possible.
     *
     * In the first case below, the while loop is safe because if there
     * is more than one rectangle, there is a 'stopper' at the end of
     * the list.
     */
    if (y >= rptr->ymax) {
        if ((rptr = rptr->next) != 0)
            while (INCR_THEN(up, y >= rptr->ymax))
                rptr = rptr->next;
    } else
        while (rptr->prev != 0 && y < rptr->prev->ymax)
            INCR_THEN(down, rptr = rptr->prev);
    if (rptr == 0 || (yc = rptr->ymin) >= ye) {
        INCR(out);
        if (rdev->list.count > 1)
            rdev->current =
                (rptr != 0 ? rptr :
                 y >= rdev->current->ymax ? rdev->list.tail :
                 rdev->list.head);
        return 0;
    }
    rdev->current = rptr;
    if (yc < y)
        yc = y;

    do {
        const int ymax = rptr->ymax;
        int yec = min(ymax, ye);

        if_debug2m('Q', rdev->memory, "[Q]yc=%d yec=%d\n", yc, yec);
        do {
            int xc = rptr->xmin;
            int xec = rptr->xmax;

            if (xc < x)
                xc = x;
            if (xec > xe)
                xec = xe;
            if (xec > xc) {
                clip_rect_print('Q', "match", rptr);
                if_debug2m('Q', rdev->memory, "[Q]xc=%d xec=%d\n", xc, xec);
                INCR(x);
/*
 * Conditionally look ahead to detect unclipped vertical strips.  This is
 * really only valuable for 90 degree rotated images or (nearly-)vertical
 * lines with convex clipping regions; if we ever change images to use
 * source buffering and destination-oriented enumeration, we could probably
 * take out the code here with no adverse effects.
 */
#ifdef CHECK_VERTICAL_CLIPPING
                if (xec - xc == pccd->w) {	/* full width */
                    /* Look ahead for a vertical swath. */
                    while ((rptr = rptr->next) != 0 &&
                           rptr->ymin == yec &&
                           rptr->ymax <= ye &&
                           rptr->xmin <= x &&
                           rptr->xmax >= xe
                           )
                        yec = rptr->ymax;
                } else
                    rptr = rptr->next;
#else
                rptr = rptr->next;
#endif
                if (rdev->list.transpose)
                    code = process(pccd, yc, xc, yec, xec);
                else
                    code = process(pccd, xc, yc, xec, yec);
                if (code < 0)
                    return code;
            } else {
                INCR_THEN(no_x, rptr = rptr->next);
            }
            if (rptr == 0)
                return 0;
        }
        while (rptr->ymax == ymax);
    } while ((yc = rptr->ymin) < ye);
    return 0;
}

static int
clip_enumerate(gx_device_clip * rdev, int x, int y, int w, int h,
               int (*process)(clip_callback_data_t * pccd,
                              int xc, int yc, int xec, int yec),
               clip_callback_data_t * pccd)
{
    int xe, ye;
    const gx_clip_rect *rptr = rdev->current;

    if (w <= 0 || h <= 0)
        return 0;
    pccd->tdev = rdev->target;
    x += rdev->translation.x;
    xe = x + w;
    y += rdev->translation.y;
    ye = y + h;
    /* pccd is non-transposed */
    pccd->x = x, pccd->y = y;
    pccd->w = w, pccd->h = h;
    /* transpose x, y, xe, ye for clip checking */
    if (rdev->list.transpose) {
        x = pccd->y;
        y = pccd->x;
        xe = x + h;
        ye = y + w;
    }
    /* Check for the region being entirely within the current rectangle. */
    if (y >= rptr->ymin && ye <= rptr->ymax &&
        x >= rptr->xmin && xe <= rptr->xmax
        ) {
        if (rdev->list.transpose) {
            return INCR_THEN(in, process(pccd, y, x, ye, xe));
        } else {
            return INCR_THEN(in, process(pccd, x, y, xe, ye));
        }
    }
    return clip_enumerate_rest(rdev, x, y, xe, ye, process, pccd);
}

/* Open a clipping device */
static int
clip_open(gx_device * dev)
{
    gx_device_clip *const rdev = (gx_device_clip *) dev;
    gx_device *tdev = rdev->target;

    /* Initialize the cursor. */
    rdev->current =
        (rdev->list.head == 0 ? &rdev->list.single : (rdev->cpath && rdev->cpath->cached ? rdev->cpath->cached : rdev->list.head));
    rdev->color_info = tdev->color_info;
    rdev->cached_colors = tdev->cached_colors;
    rdev->width = tdev->width;
    rdev->height = tdev->height;
    gx_device_copy_color_procs(dev, tdev);
    rdev->clipping_box_set = false;
    rdev->memory = tdev->memory;
    return 0;
}

/* Fill a rectangle */
int
clip_call_fill_rectangle(clip_callback_data_t * pccd, int xc, int yc, int xec, int yec)
{
    return (*dev_proc(pccd->tdev, fill_rectangle))
        (pccd->tdev, xc, yc, xec - xc, yec - yc, pccd->color[0]);
}
static int
clip_fill_rectangle_t0(gx_device * dev, int x, int y, int w, int h,
                       gx_color_index color)
{
    gx_device_clip *rdev = (gx_device_clip *) dev;
    clip_callback_data_t ccdata;
    /* We handle the fastest cases in-line here. */
    gx_device *tdev = rdev->target;
    /*const*/ gx_clip_rect *rptr = rdev->current;
    int xe, ye;

    if (w <= 0 || h <= 0)
        return 0;
    x += rdev->translation.x;
    xe = x + w;
    y += rdev->translation.y;
    ye = y + h;
    /* ccdata is non-transposed */
    ccdata.x = x, ccdata.y = y;
    ccdata.w = w, ccdata.h = h;
    /* We open-code the most common cases here. */
    if ((y >= rptr->ymin && ye <= rptr->ymax) ||
        ((rptr = rptr->next) != 0 &&
         y >= rptr->ymin && ye <= rptr->ymax)
        ) {
        rdev->current = rptr;	/* may be redundant, but awkward to avoid */
        INCR(in_y);
        if (x >= rptr->xmin && xe <= rptr->xmax) {
            INCR(in);
            return dev_proc(tdev, fill_rectangle)(tdev, x, y, w, h, color);
        }
        else if ((rptr->prev == 0 || rptr->prev->ymax != rptr->ymax) &&
                 (rptr->next == 0 || rptr->next->ymax != rptr->ymax)
                 ) {
            INCR(in1);
            if (x < rptr->xmin)
                x = rptr->xmin;
            if (xe > rptr->xmax)
                xe = rptr->xmax;
            if (x >= xe)
                 return 0;
            return dev_proc(tdev, fill_rectangle)(tdev, x, y, xe - x, h, color);
        }
    }
    ccdata.tdev = tdev;
    ccdata.color[0] = color;
    return clip_enumerate_rest(rdev, x, y, xe, ye, clip_call_fill_rectangle, &ccdata);
}

static int
clip_fill_rectangle_t1(gx_device * dev, int x, int y, int w, int h,
                       gx_color_index color)
{
    gx_device_clip *rdev = (gx_device_clip *) dev;
    clip_callback_data_t ccdata;
    /* We handle the fastest cases in-line here. */
    gx_device *tdev = rdev->target;
    /*const*/ gx_clip_rect *rptr = rdev->current;
    int xe, ye;

    if (w <= 0 || h <= 0)
        return 0;
    x += rdev->translation.x;
    y += rdev->translation.y;
    /* ccdata is non-transposed */
    ccdata.x = x, ccdata.y = y;
    ccdata.w = w, ccdata.h = h;
    x = ccdata.y;
    y = ccdata.x;
    xe = x + h;
    ye = y + w;
    /* We open-code the most common cases here. */
    if ((y >= rptr->ymin && ye <= rptr->ymax) ||
        ((rptr = rptr->next) != 0 &&
         y >= rptr->ymin && ye <= rptr->ymax)
        ) {
        rdev->current = rptr;	/* may be redundant, but awkward to avoid */
        INCR(in_y);
        if (x >= rptr->xmin && xe <= rptr->xmax) {
            INCR(in);
            return dev_proc(tdev, fill_rectangle)(tdev, y, x, w, h, color);
        }
        else if ((rptr->prev == 0 || rptr->prev->ymax != rptr->ymax) &&
                 (rptr->next == 0 || rptr->next->ymax != rptr->ymax)
                 ) {
            INCR(in1);
            if (x < rptr->xmin)
                x = rptr->xmin;
            if (xe > rptr->xmax)
                xe = rptr->xmax;
            if (x >= xe)
                 return 0;
            return dev_proc(tdev, fill_rectangle)(tdev, y, x, w, xe - x, color);
        }
    }
    ccdata.tdev = tdev;
    ccdata.color[0] = color;
    return clip_enumerate_rest(rdev, x, y, xe, ye, clip_call_fill_rectangle, &ccdata);
}

static int
clip_fill_rectangle_s0(gx_device * dev, int x, int y, int w, int h,
                       gx_color_index color)
{
    gx_device_clip *rdev = (gx_device_clip *) dev;
    gx_device *tdev = rdev->target;

    if (w <= 0 || h <= 0)
        return 0;
    x += rdev->translation.x;
    w += x;
    y += rdev->translation.y;
    h += y;
    if (x < rdev->list.single.xmin)
        x = rdev->list.single.xmin;
    if (w > rdev->list.single.xmax)
        w = rdev->list.single.xmax;
    if (y < rdev->list.single.ymin)
        y = rdev->list.single.ymin;
    if (h > rdev->list.single.ymax)
        h = rdev->list.single.ymax;
    w -= x;
    h -= y;
    if (w <= 0 || h <= 0)
        return 0;
    return dev_proc(tdev, fill_rectangle)(tdev, x, y, w, h, color);
}

static int
clip_fill_rectangle_s1(gx_device * dev, int x, int y, int w, int h,
                       gx_color_index color)
{
    gx_device_clip *rdev = (gx_device_clip *) dev;
    gx_device *tdev = rdev->target;

    if (w <= 0 || h <= 0)
        return 0;
    x += rdev->translation.x;
    w += x;
    y += rdev->translation.y;
    h += y;
    /* Now, treat x/w as y/h and vice versa as transposed. */
    if (x < rdev->list.single.ymin)
        x = rdev->list.single.ymin;
    if (w > rdev->list.single.ymax)
        w = rdev->list.single.ymax;
    if (y < rdev->list.single.xmin)
        y = rdev->list.single.xmin;
    if (h > rdev->list.single.xmax)
        h = rdev->list.single.xmax;
    w -= x;
    h -= y;
    if (w <= 0 || h <= 0)
        return 0;
    return dev_proc(tdev, fill_rectangle)(tdev, x, y, w, h, color);
}
static int
clip_fill_rectangle(gx_device * dev, int x, int y, int w, int h,
                    gx_color_index color)
{
    gx_device_clip *rdev = (gx_device_clip *) dev;

    if (rdev->list.transpose) {
        if (rdev->list.count == 1)
            dev_proc(rdev, fill_rectangle) = clip_fill_rectangle_s1;
        else
            dev_proc(rdev, fill_rectangle) = clip_fill_rectangle_t1;
    } else {
        if (rdev->list.count == 1)
            dev_proc(rdev, fill_rectangle) = clip_fill_rectangle_s0;
        else
            dev_proc(rdev, fill_rectangle) = clip_fill_rectangle_t0;
    }
    return dev_proc(rdev, fill_rectangle)(dev, x, y, w, h, color);
}

int
clip_call_fill_rectangle_hl_color(clip_callback_data_t * pccd, int xc, int yc, 
                                  int xec, int yec)
{
    gs_fixed_rect rect;

    rect.p.x = int2fixed(xc);
    rect.p.y = int2fixed(yc);
    rect.q.x = int2fixed(xec);
    rect.q.y = int2fixed(yec);
    return (*dev_proc(pccd->tdev, fill_rectangle_hl_color))
        (pccd->tdev, &rect, pccd->pgs, pccd->pdcolor, pccd->pcpath);
}

static int
clip_fill_rectangle_hl_color_t0(gx_device *dev, const gs_fixed_rect *rect,
    const gs_gstate *pgs, const gx_drawing_color *pdcolor,
    const gx_clip_path *pcpath)
{
    gx_device_clip *rdev = (gx_device_clip *) dev;
    clip_callback_data_t ccdata;
    gx_device *tdev = rdev->target;
    gx_clip_rect *rptr = rdev->current;
    int xe, ye;
    int w, h, x, y;
    gs_fixed_rect newrect;

    x = fixed2int(rect->p.x);
    y = fixed2int(rect->p.y);
    w = fixed2int(rect->q.x) - x;
    h = fixed2int(rect->q.y) - y;

    if (w <= 0 || h <= 0)
        return 0;
    x += rdev->translation.x;
    xe = x + w;
    y += rdev->translation.y;
    ye = y + h;
    /* ccdata is non-transposed */
    ccdata.x = x, ccdata.y = y;
    ccdata.w = w, ccdata.h = h;
    /* We open-code the most common cases here. */
    if ((y >= rptr->ymin && ye <= rptr->ymax) ||
        ((rptr = rptr->next) != 0 &&
         y >= rptr->ymin && ye <= rptr->ymax)
        ) {
        rdev->current = rptr;	/* may be redundant, but awkward to avoid */
        INCR(in_y);
        if (x >= rptr->xmin && xe <= rptr->xmax) {
            INCR(in);
            newrect.p.x = int2fixed(x);
            newrect.p.y = int2fixed(y);
            newrect.q.x = int2fixed(x + w);
            newrect.q.y = int2fixed(y + h);
            return dev_proc(tdev, fill_rectangle_hl_color)(tdev, &newrect, pgs,
                                                           pdcolor, pcpath);
        }
        else if ((rptr->prev == 0 || rptr->prev->ymax != rptr->ymax) &&
                 (rptr->next == 0 || rptr->next->ymax != rptr->ymax)
                 ) {
            INCR(in1);
            if (x < rptr->xmin)
                x = rptr->xmin;
            if (xe > rptr->xmax)
                xe = rptr->xmax;
            if (x >= xe)
                return 0;
            else {
                newrect.p.x = int2fixed(x);
                newrect.p.y = int2fixed(y);
                newrect.q.x = int2fixed(xe);
                newrect.q.y = int2fixed(y + h);
                return dev_proc(tdev, fill_rectangle_hl_color)(tdev, &newrect, pgs,
                                                               pdcolor, pcpath);
            }
        }
    }
    ccdata.tdev = tdev;
    ccdata.pdcolor = pdcolor;
    ccdata.pgs = pgs;
    ccdata.pcpath = pcpath;
    return clip_enumerate_rest(rdev, x, y, xe, ye,
                               clip_call_fill_rectangle_hl_color, &ccdata);
}

static int
clip_fill_rectangle_hl_color_t1(gx_device *dev, const gs_fixed_rect *rect,
    const gs_gstate *pgs, const gx_drawing_color *pdcolor,
    const gx_clip_path *pcpath)
{
    gx_device_clip *rdev = (gx_device_clip *) dev;
    clip_callback_data_t ccdata;
    gx_device *tdev = rdev->target;
    gx_clip_rect *rptr = rdev->current;
    int xe, ye;
    int w, h, x, y;
    gs_fixed_rect newrect;

    x = fixed2int(rect->p.x);
    y = fixed2int(rect->p.y);
    w = fixed2int(rect->q.x) - x;
    h = fixed2int(rect->q.y) - y;

    if (w <= 0 || h <= 0)
        return 0;
    x += rdev->translation.x;
    y += rdev->translation.y;
    /* ccdata is non-transposed */
    ccdata.x = x, ccdata.y = y;
    ccdata.w = w, ccdata.h = h;
    /* transpose x, y, xe, ye for clip checking */
    x = ccdata.y;
    y = ccdata.x;
    xe = x + h;
    ye = y + w;
    /* We open-code the most common cases here. */
    if ((y >= rptr->ymin && ye <= rptr->ymax) ||
        ((rptr = rptr->next) != 0 &&
         y >= rptr->ymin && ye <= rptr->ymax)
        ) {
        rdev->current = rptr;	/* may be redundant, but awkward to avoid */
        INCR(in_y);
        if (x >= rptr->xmin && xe <= rptr->xmax) {
            INCR(in);
            newrect.p.x = int2fixed(y);
            newrect.p.y = int2fixed(x);
            newrect.q.x = int2fixed(y + h);
            newrect.q.y = int2fixed(x + w);
            return dev_proc(tdev, fill_rectangle_hl_color)(tdev, &newrect, pgs,
                                                           pdcolor, pcpath);
        }
        else if ((rptr->prev == 0 || rptr->prev->ymax != rptr->ymax) &&
                 (rptr->next == 0 || rptr->next->ymax != rptr->ymax)
                 ) {
            INCR(in1);
            if (x < rptr->xmin)
                x = rptr->xmin;
            if (xe > rptr->xmax)
                xe = rptr->xmax;
            if (x >= xe)
                return 0;
            else {
                newrect.p.x = int2fixed(y);
                newrect.p.y = int2fixed(x);
                newrect.q.x = int2fixed(y+h);
                newrect.q.y = int2fixed(xe);
                return dev_proc(tdev, fill_rectangle_hl_color)(tdev, &newrect, pgs,
                                                               pdcolor, pcpath);
            }
        }
    }
    ccdata.tdev = tdev;
    ccdata.pdcolor = pdcolor;
    ccdata.pgs = pgs;
    ccdata.pcpath = pcpath;
    return clip_enumerate_rest(rdev, x, y, xe, ye,
                               clip_call_fill_rectangle_hl_color, &ccdata);
}

static int
clip_fill_rectangle_hl_color_s0(gx_device *dev, const gs_fixed_rect *rect,
    const gs_gstate *pgs, const gx_drawing_color *pdcolor,
    const gx_clip_path *pcpath)
{
    gx_device_clip *rdev = (gx_device_clip *) dev;
    gx_device *tdev = rdev->target;
    int w, h, x, y;
    gs_fixed_rect newrect;

    x = fixed2int(rect->p.x);
    y = fixed2int(rect->p.y);
    w = fixed2int(rect->q.x) - x;
    h = fixed2int(rect->q.y) - y;

    if (w <= 0 || h <= 0)
        return 0;
    x += rdev->translation.x;
    w += x;
    y += rdev->translation.y;
    h += y;
    if (x < rdev->list.single.xmin)
        x = rdev->list.single.xmin;
    if (w > rdev->list.single.xmax)
        w = rdev->list.single.xmax;
    if (y < rdev->list.single.ymin)
        y = rdev->list.single.ymin;
    if (h > rdev->list.single.ymax)
        h = rdev->list.single.ymax;
    w -= x;
    h -= y;
    if (w <= 0 || h <= 0)
        return 0;
    newrect.p.x = int2fixed(x);
    newrect.p.y = int2fixed(y);
    newrect.q.x = int2fixed(x + w);
    newrect.q.y = int2fixed(y + h);
    return dev_proc(tdev, fill_rectangle_hl_color)(tdev, &newrect, pgs,
                                                   pdcolor, pcpath);
}

static int
clip_fill_rectangle_hl_color_s1(gx_device *dev, const gs_fixed_rect *rect,
    const gs_gstate *pgs, const gx_drawing_color *pdcolor,
    const gx_clip_path *pcpath)
{
    gx_device_clip *rdev = (gx_device_clip *) dev;
    gx_device *tdev = rdev->target;
    int w, h, x, y;
    gs_fixed_rect newrect;

    x = fixed2int(rect->p.x);
    y = fixed2int(rect->p.y);
    w = fixed2int(rect->q.x) - x;
    h = fixed2int(rect->q.y) - y;

    if (w <= 0 || h <= 0)
        return 0;
    x += rdev->translation.x;
    w += x;
    y += rdev->translation.y;
    h += y;
    /* Now, treat x/w as y/h and vice versa as transposed. */
    if (x < rdev->list.single.ymin)
        x = rdev->list.single.ymin;
    if (w > rdev->list.single.ymax)
        w = rdev->list.single.ymax;
    if (y < rdev->list.single.xmin)
        y = rdev->list.single.xmin;
    if (h > rdev->list.single.xmax)
        h = rdev->list.single.xmax;
    w -= x;
    h -= y;
    if (w <= 0 || h <= 0)
        return 0;
    newrect.p.x = int2fixed(y);
    newrect.p.y = int2fixed(x);
    newrect.q.x = int2fixed(y + h);
    newrect.q.y = int2fixed(x + w);
    return dev_proc(tdev, fill_rectangle_hl_color)(tdev, &newrect, pgs,
                                                   pdcolor, pcpath);
}

static int
clip_fill_rectangle_hl_color(gx_device *dev, const gs_fixed_rect *rect,
    const gs_gstate *pgs, const gx_drawing_color *pdcolor,
    const gx_clip_path *pcpath)
{
    gx_device_clip *rdev = (gx_device_clip *) dev;

    if (rdev->list.transpose) {
        if (rdev->list.count == 1)
            dev_proc(rdev, fill_rectangle_hl_color) = clip_fill_rectangle_hl_color_s1;
        else
            dev_proc(rdev, fill_rectangle_hl_color) = clip_fill_rectangle_hl_color_t1;
    } else {
        if (rdev->list.count == 1)
            dev_proc(rdev, fill_rectangle_hl_color) = clip_fill_rectangle_hl_color_s0;
        else
            dev_proc(rdev, fill_rectangle_hl_color) = clip_fill_rectangle_hl_color_t0;
    }
    return dev_proc(rdev, fill_rectangle_hl_color)(dev, rect, pgs, pdcolor, pcpath);
}

/* Copy a monochrome rectangle */
int
clip_call_copy_mono(clip_callback_data_t * pccd, int xc, int yc, int xec, int yec)
{
    return (*dev_proc(pccd->tdev, copy_mono))
        (pccd->tdev, pccd->data + (yc - pccd->y) * pccd->raster,
         pccd->sourcex + xc - pccd->x, pccd->raster, gx_no_bitmap_id,
         xc, yc, xec - xc, yec - yc, pccd->color[0], pccd->color[1]);
}
static int
clip_copy_mono_t0(gx_device * dev,
                  const byte * data, int sourcex, int raster, gx_bitmap_id id,
                  int x, int y, int w, int h,
                  gx_color_index color0, gx_color_index color1)
{
    gx_device_clip *rdev = (gx_device_clip *) dev;
    clip_callback_data_t ccdata;
    /* We handle the fastest case in-line here. */
    gx_device *tdev = rdev->target;
    const gx_clip_rect *rptr = rdev->current;
    int xe, ye;

    if (w <= 0 || h <= 0)
        return 0;
    x += rdev->translation.x;
    xe = x + w;
    y += rdev->translation.y;
    ye = y + h;
    /* ccdata is non-transposed */
    ccdata.x = x, ccdata.y = y;
    ccdata.w = w, ccdata.h = h;
    if (y >= rptr->ymin && ye <= rptr->ymax) {
        INCR(in_y);
        if (x >= rptr->xmin && xe <= rptr->xmax) {
            INCR(in);
            return dev_proc(tdev, copy_mono)
                    (tdev, data, sourcex, raster, id, x, y, w, h, color0, color1);
        }
    }
    ccdata.tdev = tdev;
    ccdata.data = data, ccdata.sourcex = sourcex, ccdata.raster = raster;
    ccdata.color[0] = color0, ccdata.color[1] = color1;
    return clip_enumerate_rest(rdev, x, y, xe, ye,
                               clip_call_copy_mono, &ccdata);
}
static int
clip_copy_mono_t1(gx_device * dev,
                  const byte * data, int sourcex, int raster, gx_bitmap_id id,
                  int x, int y, int w, int h,
                  gx_color_index color0, gx_color_index color1)
{
    gx_device_clip *rdev = (gx_device_clip *) dev;
    clip_callback_data_t ccdata;
    /* We handle the fastest case in-line here. */
    gx_device *tdev = rdev->target;
    const gx_clip_rect *rptr = rdev->current;
    int xe, ye;

    if (w <= 0 || h <= 0)
        return 0;
    x += rdev->translation.x;
    y += rdev->translation.y;
    /* ccdata is non-transposed */
    ccdata.x = x, ccdata.y = y;
    ccdata.w = w, ccdata.h = h;
    x = ccdata.y;
    y = ccdata.x;
    xe = x + h;
    ye = y + w;
    if (y >= rptr->ymin && ye <= rptr->ymax) {
        INCR(in_y);
        if (x >= rptr->xmin && xe <= rptr->xmax) {
            INCR(in);
            return dev_proc(tdev, copy_mono)
                    (tdev, data, sourcex, raster, id, y, x, h, w, color0, color1);
        }
    }
    ccdata.tdev = tdev;
    ccdata.data = data, ccdata.sourcex = sourcex, ccdata.raster = raster;
    ccdata.color[0] = color0, ccdata.color[1] = color1;
    return clip_enumerate_rest(rdev, x, y, xe, ye,
                               clip_call_copy_mono, &ccdata);
}
static int
clip_copy_mono_s0(gx_device * dev,
                  const byte * data, int sourcex, int raster, gx_bitmap_id id,
                  int x, int y, int w, int h,
                  gx_color_index color0, gx_color_index color1)
{
    gx_device_clip *rdev = (gx_device_clip *) dev;
    gx_device *tdev = rdev->target;

    if (w <= 0 || h <= 0)
        return 0;
    x += rdev->translation.x;
    w += x;
    y += rdev->translation.y;
    h += y;
    if (x < rdev->list.single.xmin)
    {
        sourcex += (rdev->list.single.xmin - x);
        x = rdev->list.single.xmin;
    }
    if (w > rdev->list.single.xmax)
        w = rdev->list.single.xmax;
    if (y < rdev->list.single.ymin)
    {
        data += (rdev->list.single.ymin - y) * raster;
        y = rdev->list.single.ymin;
    }
    if (h > rdev->list.single.ymax)
        h = rdev->list.single.ymax;
    w -= x;
    h -= y;
    if (w <= 0 || h <= 0)
        return 0;
    return dev_proc(tdev, copy_mono)
                    (tdev, data, sourcex, raster, id, x, y, w, h, color0, color1);
}
static int
clip_copy_mono_s1(gx_device * dev,
                  const byte * data, int sourcex, int raster, gx_bitmap_id id,
                  int x, int y, int w, int h,
                  gx_color_index color0, gx_color_index color1)
{
    gx_device_clip *rdev = (gx_device_clip *) dev;
    gx_device *tdev = rdev->target;

    if (w <= 0 || h <= 0)
        return 0;
    x += rdev->translation.x;
    w += x;
    y += rdev->translation.y;
    h += y;
    /* Now, treat x/w as y/h and vice versa as transposed. */
    if (x < rdev->list.single.ymin)
    {
        data += (rdev->list.single.ymin - x) * raster;
        x = rdev->list.single.ymin;
    }
    if (w > rdev->list.single.ymax)
        w = rdev->list.single.ymax;
    if (y < rdev->list.single.xmin)
    {
        sourcex += (rdev->list.single.xmin - y);
        y = rdev->list.single.xmin;
    }
    if (h > rdev->list.single.xmax)
        h = rdev->list.single.xmax;
    w -= x;
    h -= y;
    if (w <= 0 || h <= 0)
        return 0;
    return dev_proc(tdev, copy_mono)
                    (tdev, data, sourcex, raster, id, y, x, h, w, color0, color1);
}
static int
clip_copy_mono(gx_device * dev,
               const byte * data, int sourcex, int raster, gx_bitmap_id id,
               int x, int y, int w, int h,
               gx_color_index color0, gx_color_index color1)
{
    gx_device_clip *rdev = (gx_device_clip *) dev;

    if (rdev->list.transpose) {
        if (rdev->list.count == 1)
            dev_proc(rdev, copy_mono) = clip_copy_mono_s1;
        else
            dev_proc(rdev, copy_mono) = clip_copy_mono_t1;
    } else {
        if (rdev->list.count == 1)
            dev_proc(rdev, copy_mono) = clip_copy_mono_s0;
        else
            dev_proc(rdev, copy_mono) = clip_copy_mono_t0;
    }
    return dev_proc(rdev, copy_mono)(dev, data, sourcex, raster, id, x, y, w, h, color0, color1);
}

/* Copy a plane */
int
clip_call_copy_planes(clip_callback_data_t * pccd, int xc, int yc, int xec, int yec)
{
    return (*dev_proc(pccd->tdev, copy_planes))
        (pccd->tdev, pccd->data + (yc - pccd->y) * pccd->raster,
         pccd->sourcex + xc - pccd->x, pccd->raster, gx_no_bitmap_id,
         xc, yc, xec - xc, yec - yc, pccd->plane_height);
}
static int
clip_copy_planes_t0(gx_device * dev,
                    const byte * data, int sourcex, int raster, gx_bitmap_id id,
                    int x, int y, int w, int h, int plane_height)
{
    gx_device_clip *rdev = (gx_device_clip *) dev;
    clip_callback_data_t ccdata;
    /* We handle the fastest case in-line here. */
    gx_device *tdev = rdev->target;
    const gx_clip_rect *rptr = rdev->current;
    int xe, ye;

    if (w <= 0 || h <= 0)
        return 0;
    x += rdev->translation.x;
    xe = x + w;
    y += rdev->translation.y;
    ye = y + h;
    /* ccdata is non-transposed */
    ccdata.x = x, ccdata.y = y;
    ccdata.w = w, ccdata.h = h;
    if (y >= rptr->ymin && ye <= rptr->ymax) {
        INCR(in_y);
        if (x >= rptr->xmin && xe <= rptr->xmax) {
            INCR(in);
            return dev_proc(tdev, copy_planes)
                    (tdev, data, sourcex, raster, id, x, y, w, h, plane_height);
        }
    }
    ccdata.tdev = tdev;
    ccdata.data = data, ccdata.sourcex = sourcex, ccdata.raster = raster;
    ccdata.plane_height = plane_height;
    return clip_enumerate_rest(rdev, x, y, xe, ye,
                               clip_call_copy_planes, &ccdata);
}
static int
clip_copy_planes_t1(gx_device * dev,
                    const byte * data, int sourcex, int raster, gx_bitmap_id id,
                    int x, int y, int w, int h, int plane_height)
{
    gx_device_clip *rdev = (gx_device_clip *) dev;
    clip_callback_data_t ccdata;
    /* We handle the fastest case in-line here. */
    gx_device *tdev = rdev->target;
    const gx_clip_rect *rptr = rdev->current;
    int xe, ye;

    if (w <= 0 || h <= 0)
        return 0;
    x += rdev->translation.x;
    y += rdev->translation.y;
    /* ccdata is non-transposed */
    ccdata.x = x, ccdata.y = y;
    ccdata.w = w, ccdata.h = h;
    /* transpose x, y, xe, ye for clip checking */
    x = ccdata.y;
    y = ccdata.x;
    xe = x + h;
    ye = y + w;
    if (y >= rptr->ymin && ye <= rptr->ymax) {
        INCR(in_y);
        if (x >= rptr->xmin && xe <= rptr->xmax) {
            INCR(in);
            return dev_proc(tdev, copy_planes)
                    (tdev, data, sourcex, raster, id, y, x, h, w, plane_height);
        }
    }
    ccdata.tdev = tdev;
    ccdata.data = data, ccdata.sourcex = sourcex, ccdata.raster = raster;
    ccdata.plane_height = plane_height;
    return clip_enumerate_rest(rdev, x, y, xe, ye,
                               clip_call_copy_planes, &ccdata);
}
static int
clip_copy_planes_s0(gx_device * dev,
                    const byte * data, int sourcex, int raster, gx_bitmap_id id,
                    int x, int y, int w, int h, int plane_height)
{
    gx_device_clip *rdev = (gx_device_clip *) dev;
    gx_device *tdev = rdev->target;

    x += rdev->translation.x;
    w += x;
    y += rdev->translation.y;
    h += y;
    if (x < rdev->list.single.xmin)
    {
        sourcex += rdev->list.single.xmin - x;
        x = rdev->list.single.xmin;
    }
    if (w > rdev->list.single.xmax)
        w = rdev->list.single.xmax;
    if (y < rdev->list.single.ymin)
    {
        data += (rdev->list.single.ymin - y) * raster;
        y = rdev->list.single.ymin;
    }
    if (h > rdev->list.single.ymax)
        h = rdev->list.single.ymax;
    w -= x;
    h -= y;
    if (w <= 0 || h <= 0)
        return 0;
    return dev_proc(tdev, copy_planes)
                    (tdev, data, sourcex, raster, id, x, y, w, h, plane_height);
}
static int
clip_copy_planes_s1(gx_device * dev,
                    const byte * data, int sourcex, int raster, gx_bitmap_id id,
                    int x, int y, int w, int h, int plane_height)
{
    gx_device_clip *rdev = (gx_device_clip *) dev;
    gx_device *tdev = rdev->target;

    x += rdev->translation.x;
    w += x;
    y += rdev->translation.y;
    h += y;
    /* Now, treat x/w as y/h and vice versa as transposed. */
    if (x < rdev->list.single.ymin)
    {
        data += (rdev->list.single.ymin - x) * raster;
        x = rdev->list.single.ymin;
    }
    if (w > rdev->list.single.ymax)
        w = rdev->list.single.ymax;
    if (y < rdev->list.single.xmin)
    {
        sourcex += rdev->list.single.xmin - y;
        y = rdev->list.single.xmin;
    }
    if (h > rdev->list.single.xmax)
        h = rdev->list.single.xmax;
    w -= x;
    h -= y;
    if (w <= 0 || h <= 0)
        return 0;
    return dev_proc(tdev, copy_planes)
                    (tdev, data, sourcex, raster, id, y, x, h, w, plane_height);
}
static int
clip_copy_planes(gx_device * dev,
                 const byte * data, int sourcex, int raster, gx_bitmap_id id,
                 int x, int y, int w, int h, int plane_height)
{
    gx_device_clip *rdev = (gx_device_clip *) dev;

    if (rdev->list.transpose) {
        if (rdev->list.count == 1)
            dev_proc(rdev, copy_planes) = clip_copy_planes_s1;
        else
            dev_proc(rdev, copy_planes) = clip_copy_planes_t1;
    } else {
        if (rdev->list.count == 1)
            dev_proc(rdev, copy_planes) = clip_copy_planes_s0;
        else
            dev_proc(rdev, copy_planes) = clip_copy_planes_t0;
    }
    return dev_proc(rdev, copy_planes)(dev, data, sourcex, raster, id, x, y, w, h, plane_height);
}

/* Copy a color rectangle */
int
clip_call_copy_color(clip_callback_data_t * pccd, int xc, int yc, int xec, int yec)
{
    return (*dev_proc(pccd->tdev, copy_color))
        (pccd->tdev, pccd->data + (yc - pccd->y) * pccd->raster,
         pccd->sourcex + xc - pccd->x, pccd->raster, gx_no_bitmap_id,
         xc, yc, xec - xc, yec - yc);
}
static int
clip_copy_color(gx_device * dev,
                const byte * data, int sourcex, int raster, gx_bitmap_id id,
                int x, int y, int w, int h)
{
    gx_device_clip *rdev = (gx_device_clip *) dev;
    clip_callback_data_t ccdata;

    ccdata.data = data, ccdata.sourcex = sourcex, ccdata.raster = raster;
    return clip_enumerate(rdev, x, y, w, h, clip_call_copy_color, &ccdata);
}

/* Copy a rectangle with alpha */
int
clip_call_copy_alpha(clip_callback_data_t * pccd, int xc, int yc, int xec, int yec)
{
    return (*dev_proc(pccd->tdev, copy_alpha))
        (pccd->tdev, pccd->data + (yc - pccd->y) * pccd->raster,
         pccd->sourcex + xc - pccd->x, pccd->raster, gx_no_bitmap_id,
         xc, yc, xec - xc, yec - yc, pccd->color[0], pccd->depth);
}
static int
clip_copy_alpha(gx_device * dev,
                const byte * data, int sourcex, int raster, gx_bitmap_id id,
                int x, int y, int w, int h,
                gx_color_index color, int depth)
{
    gx_device_clip *rdev = (gx_device_clip *) dev;
    clip_callback_data_t ccdata;

    ccdata.data = data, ccdata.sourcex = sourcex, ccdata.raster = raster;
    ccdata.color[0] = color, ccdata.depth = depth;
    return clip_enumerate(rdev, x, y, w, h, clip_call_copy_alpha, &ccdata);
}

int
clip_call_copy_alpha_hl_color(clip_callback_data_t * pccd, int xc, int yc, 
                              int xec, int yec)
{
    return (*dev_proc(pccd->tdev, copy_alpha_hl_color))
        (pccd->tdev, pccd->data + (yc - pccd->y) * pccd->raster,
         pccd->sourcex + xc - pccd->x, pccd->raster, gx_no_bitmap_id,
         xc, yc, xec - xc, yec - yc, pccd->pdcolor, pccd->depth);
}

static int
clip_copy_alpha_hl_color(gx_device * dev,
                const byte * data, int sourcex, int raster, gx_bitmap_id id,
                int x, int y, int w, int h,
                const gx_drawing_color *pdcolor, int depth)
{
    gx_device_clip *rdev = (gx_device_clip *) dev;
    clip_callback_data_t ccdata;

    ccdata.data = data, ccdata.sourcex = sourcex, ccdata.raster = raster;
    ccdata.pdcolor = pdcolor, ccdata.depth = depth;
    return clip_enumerate(rdev, x, y, w, h, clip_call_copy_alpha_hl_color, &ccdata);
}

/* Fill a region defined by a mask. */
int
clip_call_fill_mask(clip_callback_data_t * pccd, int xc, int yc, int xec, int yec)
{
    return (*dev_proc(pccd->tdev, fill_mask))
        (pccd->tdev, pccd->data + (yc - pccd->y) * pccd->raster,
         pccd->sourcex + xc - pccd->x, pccd->raster, gx_no_bitmap_id,
         xc, yc, xec - xc, yec - yc, pccd->pdcolor, pccd->depth,
         pccd->lop, NULL);
}
static int
clip_fill_mask(gx_device * dev,
               const byte * data, int sourcex, int raster, gx_bitmap_id id,
               int x, int y, int w, int h,
               const gx_drawing_color * pdcolor, int depth,
               gs_logical_operation_t lop, const gx_clip_path * pcpath)
{
    gx_device_clip *rdev = (gx_device_clip *) dev;
    clip_callback_data_t ccdata;

    if (pcpath != 0)
        return gx_default_fill_mask(dev, data, sourcex, raster, id,
                                    x, y, w, h, pdcolor, depth, lop,
                                    pcpath);
    ccdata.data = data, ccdata.sourcex = sourcex, ccdata.raster = raster;
    ccdata.pdcolor = pdcolor, ccdata.depth = depth, ccdata.lop = lop;
    return clip_enumerate(rdev, x, y, w, h, clip_call_fill_mask, &ccdata);
}

/* Strip-tile a rectangle with devn colors. */
int
clip_call_strip_tile_rect_devn(clip_callback_data_t * pccd, int xc, int yc, int xec, int yec)
{
    return (*dev_proc(pccd->tdev, strip_tile_rect_devn))
        (pccd->tdev, pccd->tiles, xc, yc, xec - xc, yec - yc,
         pccd->pdc[0], pccd->pdc[1], pccd->phase.x, pccd->phase.y);
}
static int
clip_strip_tile_rect_devn(gx_device * dev, const gx_strip_bitmap * tiles,
                                int x, int y, int w, int h,
                                const gx_drawing_color *pdcolor0, 
                                const gx_drawing_color *pdcolor1, int phase_x, 
                                int phase_y)
{
    gx_device_clip *rdev = (gx_device_clip *) dev;
    clip_callback_data_t ccdata;

    ccdata.tiles = tiles;
    ccdata.pdc[0] = pdcolor0;
    ccdata.pdc[1] = pdcolor1;
    ccdata.phase.x = phase_x, ccdata.phase.y = phase_y;
    return clip_enumerate(rdev, x, y, w, h, clip_call_strip_tile_rect_devn, &ccdata);
}

/* Strip-tile a rectangle. */
int
clip_call_strip_tile_rectangle(clip_callback_data_t * pccd, int xc, int yc, int xec, int yec)
{
    return (*dev_proc(pccd->tdev, strip_tile_rectangle))
        (pccd->tdev, pccd->tiles, xc, yc, xec - xc, yec - yc,
         pccd->color[0], pccd->color[1], pccd->phase.x, pccd->phase.y);
}
static int
clip_strip_tile_rectangle(gx_device * dev, const gx_strip_bitmap * tiles,
                          int x, int y, int w, int h,
     gx_color_index color0, gx_color_index color1, int phase_x, int phase_y)
{
    gx_device_clip *rdev = (gx_device_clip *) dev;
    clip_callback_data_t ccdata;

    ccdata.tiles = tiles;
    ccdata.color[0] = color0, ccdata.color[1] = color1;
    ccdata.phase.x = phase_x, ccdata.phase.y = phase_y;
    return clip_enumerate(rdev, x, y, w, h, clip_call_strip_tile_rectangle, &ccdata);
}

/* Copy a rectangle with RasterOp and strip texture. */
int
clip_call_strip_copy_rop(clip_callback_data_t * pccd, int xc, int yc, int xec, int yec)
{
    return (*dev_proc(pccd->tdev, strip_copy_rop))
        (pccd->tdev, pccd->data + (yc - pccd->y) * pccd->raster,
         pccd->sourcex + xc - pccd->x, pccd->raster, gx_no_bitmap_id,
         pccd->scolors, pccd->textures, pccd->tcolors,
         xc, yc, xec - xc, yec - yc, pccd->phase.x, pccd->phase.y,
         pccd->lop);
}
static int
clip_strip_copy_rop(gx_device * dev,
              const byte * sdata, int sourcex, uint raster, gx_bitmap_id id,
                    const gx_color_index * scolors,
           const gx_strip_bitmap * textures, const gx_color_index * tcolors,
                    int x, int y, int w, int h,
                    int phase_x, int phase_y, gs_logical_operation_t lop)
{
    gx_device_clip *rdev = (gx_device_clip *) dev;
    clip_callback_data_t ccdata;

    ccdata.data = sdata, ccdata.sourcex = sourcex, ccdata.raster = raster;
    ccdata.scolors = scolors, ccdata.textures = textures,
        ccdata.tcolors = tcolors;
    ccdata.phase.x = phase_x, ccdata.phase.y = phase_y, ccdata.lop = lop;
    return clip_enumerate(rdev, x, y, w, h, clip_call_strip_copy_rop, &ccdata);
}

/* Copy a rectangle with RasterOp and strip texture. */
int
clip_call_strip_copy_rop2(clip_callback_data_t * pccd, int xc, int yc, int xec, int yec)
{
    return (*dev_proc(pccd->tdev, strip_copy_rop2))
        (pccd->tdev, pccd->data + (yc - pccd->y) * pccd->raster,
         pccd->sourcex + xc - pccd->x, pccd->raster, gx_no_bitmap_id,
         pccd->scolors, pccd->textures, pccd->tcolors,
         xc, yc, xec - xc, yec - yc, pccd->phase.x, pccd->phase.y,
         pccd->lop, pccd->plane_height);
}
static int
clip_strip_copy_rop2(gx_device * dev,
              const byte * sdata, int sourcex, uint raster, gx_bitmap_id id,
                    const gx_color_index * scolors,
           const gx_strip_bitmap * textures, const gx_color_index * tcolors,
                    int x, int y, int w, int h,
                    int phase_x, int phase_y, gs_logical_operation_t lop,
                    uint planar_height)
{
    gx_device_clip *rdev = (gx_device_clip *) dev;
    clip_callback_data_t ccdata;

    ccdata.data = sdata, ccdata.sourcex = sourcex, ccdata.raster = raster;
    ccdata.scolors = scolors, ccdata.textures = textures,
        ccdata.tcolors = tcolors;
    ccdata.phase.x = phase_x, ccdata.phase.y = phase_y, ccdata.lop = lop;
    ccdata.plane_height = planar_height;
    return clip_enumerate(rdev, x, y, w, h, clip_call_strip_copy_rop2, &ccdata);
}

/* Get the (outer) clipping box, in client coordinates. */
static void
clip_get_clipping_box(gx_device * dev, gs_fixed_rect * pbox)
{
    gx_device_clip *const rdev = (gx_device_clip *) dev;

    if (!rdev->clipping_box_set) {
        gx_device *tdev = rdev->target;
        gs_fixed_rect tbox;

        (*dev_proc(tdev, get_clipping_box)) (tdev, &tbox);
        if (rdev->list.count != 0) {
            gs_fixed_rect cbox;

            if (rdev->list.count == 1) {
                cbox.p.x = int2fixed(rdev->list.single.xmin);
                cbox.p.y = int2fixed(rdev->list.single.ymin);
                cbox.q.x = int2fixed(rdev->list.single.xmax);
                cbox.q.y = int2fixed(rdev->list.single.ymax);
            } else {
                /* The head and tail elements are dummies.... */
                gx_clip_rect *curr = rdev->list.head->next;

                cbox.p.x = cbox.p.y = max_int;
                cbox.q.x = cbox.q.y = min_int;
                /* scan the list for the outer bbox */
                while (curr->next != NULL) {	/* stop before tail */
                    if (curr->xmin < cbox.p.x)
                        cbox.p.x = curr->xmin;
                    if (curr->xmax > cbox.q.x)
                        cbox.q.x = curr->xmax;
                    if (curr->ymin < cbox.p.y)
                        cbox.p.y = curr->ymin;
                    if (curr->ymax > cbox.q.y)
                        cbox.q.y = curr->ymax;
                    curr = curr->next;
                }
                cbox.p.x = int2fixed(cbox.p.x);
                cbox.q.x = int2fixed(cbox.q.x);
                cbox.p.y = int2fixed(cbox.p.y);
                cbox.q.y = int2fixed(cbox.q.y);
            }
            if (rdev->list.transpose) {
                fixed temp = cbox.p.x;

                cbox.p.x = cbox.p.y;
                cbox.p.y = temp;
                temp = cbox.q.x;
                cbox.q.x = cbox.q.y;
                cbox.q.y = temp;
            }
            rect_intersect(tbox, cbox);
        }
        if (rdev->translation.x | rdev->translation.y) {
            fixed tx = int2fixed(rdev->translation.x),
                ty = int2fixed(rdev->translation.y);

            if (tbox.p.x != min_fixed)
                tbox.p.x -= tx;
            if (tbox.p.y != min_fixed)
                tbox.p.y -= ty;
            if (tbox.q.x != max_fixed)
                tbox.q.x -= tx;
            if (tbox.q.y != max_fixed)
                tbox.q.y -= ty;
        }
        rdev->clipping_box = tbox;
        rdev->clipping_box_set = true;
    }
    *pbox = rdev->clipping_box;
}

/* Get bits back from the device. */
static int
clip_get_bits_rectangle(gx_device * dev, const gs_int_rect * prect,
                        gs_get_bits_params_t * params, gs_int_rect ** unread)
{
    gx_device_clip *rdev = (gx_device_clip *) dev;
    gx_device *tdev = rdev->target;
    int tx = rdev->translation.x, ty = rdev->translation.y;
    gs_int_rect rect;
    int code;

    rect.p.x = prect->p.x - tx, rect.p.y = prect->p.y - ty;
    rect.q.x = prect->q.x - tx, rect.q.y = prect->q.y - ty;
    code = (*dev_proc(tdev, get_bits_rectangle))
        (tdev, &rect, params, unread);
    if (code > 0) {
        /* Adjust unread rectangle coordinates */
        gs_int_rect *list = *unread;
        int i;

        for (i = 0; i < code; ++list, ++i) {
            list->p.x += tx, list->p.y += ty;
            list->q.x += tx, list->q.y += ty;
        }
    }
    return code;
}

static int
clip_call_fill_path(clip_callback_data_t * pccd, int xc, int yc, int xec, int yec)
{
    gx_device *tdev = pccd->tdev;
    dev_proc_fill_path((*proc));
    int code;
    gx_clip_path cpath_intersection;
    gx_clip_path *pcpath = (gx_clip_path *)pccd->pcpath;

    /* Previously the code here tested for pcpath != NULL, but
     * we can commonly (such as from clist_playback_band) be
     * called with a non-NULL, but still invalid clip path.
     * Detect this by the list having at least one entry in it. */
    if (pcpath != NULL && pcpath->rect_list->list.count != 0) {
        gx_path rect_path;
        code = gx_cpath_init_local_shared_nested(&cpath_intersection, pcpath, pccd->ppath->memory, 1);
        if (code < 0)
            return code;
        gx_path_init_local(&rect_path, pccd->ppath->memory);
        code = gx_path_add_rectangle(&rect_path, int2fixed(xc), int2fixed(yc), int2fixed(xec), int2fixed(yec));
        if (code < 0)
            return code;
        code = gx_cpath_intersect(&cpath_intersection, &rect_path,
                                  gx_rule_winding_number, (gs_gstate *)(pccd->pgs));
        gx_path_free(&rect_path, "clip_call_fill_path");
    } else {
        gs_fixed_rect clip_box;
        clip_box.p.x = int2fixed(xc);
        clip_box.p.y = int2fixed(yc);
        clip_box.q.x = int2fixed(xec);
        clip_box.q.y = int2fixed(yec);
        gx_cpath_init_local(&cpath_intersection, pccd->ppath->memory);
        code = gx_cpath_from_rectangle(&cpath_intersection, &clip_box);
    }
    if (code < 0)
        return code;
    proc = dev_proc(tdev, fill_path);
    if (proc == NULL)
        proc = gx_default_fill_path;
    code = (*proc)(pccd->tdev, pccd->pgs, pccd->ppath, pccd->params,
                   pccd->pdcolor, &cpath_intersection);
    gx_cpath_free(&cpath_intersection, "clip_call_fill_path");
    return code;
}
static int
clip_fill_path(gx_device * dev, const gs_gstate * pgs,
               gx_path * ppath, const gx_fill_params * params,
               const gx_drawing_color * pdcolor,
               const gx_clip_path * pcpath)
{
    gx_device_clip *rdev = (gx_device_clip *) dev;
    clip_callback_data_t ccdata;
    gs_fixed_rect box;

    ccdata.pgs = pgs;
    ccdata.ppath = ppath;
    ccdata.params = params;
    ccdata.pdcolor = pdcolor;
    ccdata.pcpath = pcpath;
    clip_get_clipping_box(dev, &box);
    return clip_enumerate(rdev,
                          fixed2int(box.p.x),
                          fixed2int(box.p.y),
                          fixed2int(box.q.x - box.p.x),
                          fixed2int(box.q.y - box.p.y),
                          clip_call_fill_path, &ccdata);
}

typedef struct  {
    int use_default;
    void *child_state;
} clip_transform_pixel_region_data;

static int
clip_transform_pixel_region(gx_device *dev, transform_pixel_region_reason reason, transform_pixel_region_data *data)
{
    clip_transform_pixel_region_data *state = (clip_transform_pixel_region_data *)data->state;
    gx_device_clip *cdev = (gx_device_clip *)dev;
    transform_pixel_region_data local_data;
    gs_int_rect local_clip;
    int ret;

    if (reason == transform_pixel_region_begin) {
        int skewed = 1;
        if (data->u.init.pixels->y.step.dQ == 0 && data->u.init.pixels->y.step.dR == 0 &&
            data->u.init.rows->x.step.dQ == 0 && data->u.init.rows->x.step.dR == 0)
            skewed = 0;
        else if (data->u.init.pixels->x.step.dQ == 0 && data->u.init.pixels->x.step.dR == 0 &&
                 data->u.init.rows->y.step.dQ == 0 && data->u.init.rows->y.step.dR == 0)
            skewed = 0;
        state = (clip_transform_pixel_region_data *)gs_alloc_bytes(dev->memory->non_gc_memory, sizeof(*state), "clip_transform_pixel_region_data");
        if (state == NULL)
            return gs_error_VMerror;
        local_data = *data;
        if (cdev->list.count == 1 && skewed == 0) {
            /* Single unskewed rectangle - we can use the underlying device direct */
            local_data.u.init.clip = &local_clip;
            local_clip = *data->u.init.clip;
            if (cdev->list.transpose) {
                if (local_clip.p.x < cdev->current->ymin)
                    local_clip.p.x = cdev->current->ymin;
                if (local_clip.q.x > cdev->current->ymax)
                    local_clip.q.x = cdev->current->ymax;
                if (local_clip.p.y < cdev->current->xmin)
                    local_clip.p.y = cdev->current->xmin;
                if (local_clip.q.y > cdev->current->xmax)
                    local_clip.q.y = cdev->current->xmax;
            } else {
                if (local_clip.p.x < cdev->current->xmin)
                    local_clip.p.x = cdev->current->xmin;
                if (local_clip.q.x > cdev->current->xmax)
                    local_clip.q.x = cdev->current->xmax;
                if (local_clip.p.y < cdev->current->ymin)
                    local_clip.p.y = cdev->current->ymin;
                if (local_clip.q.y > cdev->current->ymax)
                    local_clip.q.y = cdev->current->ymax;
            }
            state->use_default = 0;
            ret = dev_proc(cdev->target, transform_pixel_region)(cdev->target, reason, &local_data);
        } else {
            /* Multiple rectangles - we need to use the default */
            state->use_default = 1;
            ret = gx_default_transform_pixel_region(dev, reason, &local_data);
        }
        state->child_state = local_data.state;
        data->state = state;
        return ret;
    }

    data->state = state->child_state;
    if (state->use_default)
        ret = gx_default_transform_pixel_region(dev, reason, data);
    else
        ret = dev_proc(cdev->target, transform_pixel_region)(cdev->target, reason, data);

    if (reason == transform_pixel_region_end) {
        gs_free_object(dev->memory->non_gc_memory, state, "clip_transform_pixel_region_data");
        state = NULL;
    }
    data->state = state;

    return ret;
}

static int
clip_call_fill_stroke_path(clip_callback_data_t * pccd, int xc, int yc, int xec, int yec)
{
    gx_device *tdev = pccd->tdev;
    dev_proc_fill_stroke_path((*proc));
    int code;
    gx_clip_path cpath_intersection;
    gx_clip_path *pcpath = (gx_clip_path *)pccd->pcpath;

    /* Previously the code here tested for pcpath != NULL, but
     * we can commonly (such as from clist_playback_band) be
     * called with a non-NULL, but still invalid clip path.
     * Detect this by the list having at least one entry in it. */
    if (pcpath != NULL && pcpath->rect_list->list.count != 0) {
        gx_path rect_path;
        code = gx_cpath_init_local_shared_nested(&cpath_intersection, pcpath, pccd->ppath->memory, 1);
        if (code < 0)
            return code;
        gx_path_init_local(&rect_path, pccd->ppath->memory);
        code = gx_path_add_rectangle(&rect_path, int2fixed(xc), int2fixed(yc), int2fixed(xec), int2fixed(yec));
        if (code < 0)
            return code;
        code = gx_cpath_intersect(&cpath_intersection, &rect_path,
                                  gx_rule_winding_number, (gs_gstate *)(pccd->pgs));
        gx_path_free(&rect_path, "clip_call_fill_stroke_path");
    } else {
        gs_fixed_rect clip_box;
        clip_box.p.x = int2fixed(xc);
        clip_box.p.y = int2fixed(yc);
        clip_box.q.x = int2fixed(xec);
        clip_box.q.y = int2fixed(yec);
        gx_cpath_init_local(&cpath_intersection, pccd->ppath->memory);
        code = gx_cpath_from_rectangle(&cpath_intersection, &clip_box);
    }
    if (code < 0)
        return code;
    proc = dev_proc(tdev, fill_stroke_path);
    if (proc == NULL)
        proc = gx_default_fill_stroke_path;
    code = (*proc)(pccd->tdev, pccd->pgs, pccd->ppath, pccd->params,
                   pccd->pdcolor, pccd->stroke_params, pccd->pstroke_dcolor,
                   &cpath_intersection);
    gx_cpath_free(&cpath_intersection, "clip_call_fill_stroke_path");
    return code;
}

static int
clip_fill_stroke_path(gx_device *dev, const gs_gstate *pgs,
               gx_path *ppath,
               const gx_fill_params *params,
               const gx_drawing_color *pdcolor,
               const gx_stroke_params *stroke_params,
               const gx_drawing_color *pstroke_dcolor,
               const gx_clip_path *pcpath)
{
    gx_device_clip *rdev = (gx_device_clip *) dev;
    clip_callback_data_t ccdata;
    gs_fixed_rect box;

    ccdata.pgs = pgs;
    ccdata.ppath = ppath;
    ccdata.params = params;
    ccdata.pdcolor = pdcolor;
    ccdata.stroke_params = stroke_params;
    ccdata.pstroke_dcolor = pstroke_dcolor;
    ccdata.pcpath = pcpath;
    clip_get_clipping_box(dev, &box);
    return clip_enumerate(rdev,
                          fixed2int(box.p.x),
                          fixed2int(box.p.y),
                          fixed2int(box.q.x - box.p.x),
                          fixed2int(box.q.y - box.p.y),
                          clip_call_fill_stroke_path, &ccdata);
}
