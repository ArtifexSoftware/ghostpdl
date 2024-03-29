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

/* Alpha-buffering memory devices */
#include "memory_.h"
#include "gx.h"
#include "gserrors.h"
#include "gxdevice.h"
#include "gxdevmem.h"		/* semi-public definitions */
#include "gdevmem.h"		/* private definitions */
#include "gzstate.h"
#include "gxdevcli.h"
#include "gxdevsop.h"

/* ================ Alpha devices ================ */

/*
 * These devices store 2 or 4 bits of alpha.  They are a hybrid of a
 * monobit device (for color mapping) and a 2- or 4-bit device (for painting).
 * Currently, we only use them for character rasterizing, but they might be
 * useful for other things someday.
 */

/* ================ Alpha-buffer device ================ */

/*
 * This device converts graphics sampled at a higher resolution to
 * alpha values at a lower resolution.  It does this by accumulating
 * the bits of a band and then converting the band to alphas.
 * In order to make this work, the client of the device must promise
 * only to visit each band at most once, except possibly for a single
 * scan line overlapping the adjacent band, and must promise only to write
 * a single color into the output.  In particular, this works
 * within a single call on gx_fill_path (if the fill loop is constrained
 * to process bands of limited height on each pass) or a single masked image
 * scanned in Y order, but not across such calls and not for other
 * kinds of painting operations.
 *
 * We implement this device as a subclass of a monobit memory device.
 * (We put its state in the definition of gx_device_memory just because
 * actual subclassing introduces a lot of needless boilerplate.)
 * We only allocate enough bits for one band.  The height of the band
 * must be a multiple of the Y scale factor; the minimum height
 * of the band is twice the Y scale factor.
 *
 * The bits in storage are actually a sliding window on the true
 * oversampled image.  To avoid having to copy the bits around when we
 * move the window, we adjust the mapping between the client's Y values
 * and our own, as follows:
 *      Client          Stored
 *      ------          ------
 *      y0..y0+m-1      n-m..n-1
 *      y0+m..y0+n-1    0..n-m-1
 * where n and m are multiples of the Y scale factor and 0 <= m <= n <=
 * the height of the band.  (In the device structure, m is called
 * mapped_start and n is called mapped_height.)  This allows us to slide
 * the window incrementally in either direction without copying any bits.
 */

/* Procedures */
static dev_proc_close_device(mem_abuf_close);
static dev_proc_copy_mono(mem_abuf_copy_mono);
static dev_proc_fill_rectangle(mem_abuf_fill_rectangle);
static dev_proc_get_clipping_box(mem_abuf_get_clipping_box);
static dev_proc_fill_rectangle_hl_color(mem_abuf_fill_rectangle_hl_color);
static dev_proc_fill_stroke_path(mem_abuf_fill_stroke_path);

/* The device descriptor. */
static void
mem_alpha_initialize_device_procs(gx_device *dev)
{
    mem_initialize_device_procs(dev);

    set_dev_proc(dev, map_rgb_color, gx_forward_map_rgb_color);
    set_dev_proc(dev, map_color_rgb, gx_forward_map_color_rgb);
    set_dev_proc(dev, fill_rectangle, mem_abuf_fill_rectangle);
    set_dev_proc(dev, copy_mono, mem_abuf_copy_mono);
    set_dev_proc(dev, copy_color, gx_default_copy_color);
    set_dev_proc(dev, strip_copy_rop2, gx_no_strip_copy_rop2);
    set_dev_proc(dev, fill_rectangle_hl_color, mem_abuf_fill_rectangle_hl_color);
    set_dev_proc(dev, fill_stroke_path, mem_abuf_fill_stroke_path);
}

static const gx_device_memory mem_alpha_buffer_device =
   mem_device("image(alpha buffer)", 0, 1, mem_alpha_initialize_device_procs);

/* Make an alpha-buffer memory device. */
/* We use abuf instead of alpha_buffer because */
/* gcc under VMS only retains 23 characters of procedure names. */
void
gs_make_mem_abuf_device(gx_device_memory * adev, gs_memory_t * mem,
                     gx_device * target, const gs_log2_scale_point * pscale,
                        int alpha_bits, int mapped_x, bool devn)
{
    gs_make_mem_device(adev, &mem_alpha_buffer_device, mem, 0, target);
    adev->max_fill_band = 1 << pscale->y;
    adev->log2_scale = *pscale;
    adev->log2_alpha_bits = alpha_bits >> 1;	/* works for 1,2,4 */
    adev->mapped_x = mapped_x;
    set_dev_proc(adev, close_device, mem_abuf_close);
    set_dev_proc(adev, get_clipping_box, mem_abuf_get_clipping_box);
    if (!devn)
        adev->save_hl_color = NULL; /* This is the test for when we flush the
                                       the buffer as to what copy_alpha type
                                       use */
    adev->color_info.anti_alias.text_bits =
      adev->color_info.anti_alias.graphics_bits =
        alpha_bits;
    adev->graphics_type_tag = target->graphics_type_tag;
}

/* Test whether a device is an alpha-buffering device. */
bool
gs_device_is_abuf(const gx_device * dev)
{				/* We can't just compare the procs, or even an individual proc, */
    /* because we might be tracing.  Instead, check the identity of */
    /* the device name. */
    return dev->dname == mem_alpha_buffer_device.dname;
}

/* Internal routine to flush a block of the buffer. */
/* A block is a group of scan lines whose initial Y is a multiple */
/* of the Y scale and whose height is equal to the Y scale. */
static int
abuf_flush_block(gx_device_memory * adev, int y)
{
    gx_device *target = adev->target;
    int block_height = 1 << adev->log2_scale.y;
    int alpha_bits = 1 << adev->log2_alpha_bits;
    int ddepth =
    (adev->width >> adev->log2_scale.x) << adev->log2_alpha_bits;
    uint draster = bitmap_raster(ddepth);
    int buffer_y = y - adev->mapped_y + adev->mapped_start;
    byte *bits;

    if (buffer_y >= adev->height)
        buffer_y -= adev->height;
    bits = scan_line_base(adev, buffer_y);
    {/*
      * Many bits are typically zero.  Save time by computing
      * an accurate X bounding box before compressing.
      * Unfortunately, in order to deal with alpha nibble swapping
      * (see gsbitops.c), we can't expand the box only to pixel
      * boundaries:
          int alpha_mask = -1 << adev->log2_alpha_bits;
      * Instead, we must expand it to byte boundaries,
      */
        int alpha_mask = ~7;
        gs_int_rect bbox;
        int width;

        bits_bounding_box(bits, block_height, adev->raster, &bbox);
        bbox.p.x &= alpha_mask;
        bbox.q.x = (bbox.q.x + ~alpha_mask) & alpha_mask;
        width = bbox.q.x - bbox.p.x;
        bits_compress_scaled(bits, bbox.p.x, width, block_height,
                             adev->raster, bits, draster, &adev->log2_scale,
                             adev->log2_alpha_bits);
        /* Set up with NULL when adev initialized */
        if (adev->save_hl_color == NULL) {
            return (*dev_proc(target, copy_alpha)) (target,
                                              bits, 0, draster, gx_no_bitmap_id,
                                                  (adev->mapped_x + bbox.p.x) >>
                                                    adev->log2_scale.x,
                                                    y >> adev->log2_scale.y,
                                                 width >> adev->log2_scale.x, 1,
                                                  adev->save_color, alpha_bits);
        } else {
            return (*dev_proc(target, copy_alpha_hl_color)) (target,
                                              bits, 0, draster, gx_no_bitmap_id,
                                                  (adev->mapped_x + bbox.p.x) >>
                                                    adev->log2_scale.x,
                                                    y >> adev->log2_scale.y,
                                                 width >> adev->log2_scale.x, 1,
                                                  adev->save_hl_color, alpha_bits);
        }
    }
}
/* Flush the entire buffer. */
static int
abuf_flush(gx_device_memory * adev)
{
    int y, code = 0;
    int block_height = 1 << adev->log2_scale.y;

    for (y = 0; y < adev->mapped_height; y += block_height)
        if ((code = abuf_flush_block(adev, adev->mapped_y + y)) < 0)
            return code;
    adev->mapped_height = adev->mapped_start = 0;
    return 0;
}

/* Close the device, flushing the buffer. */
static int
mem_abuf_close(gx_device * dev)
{
    gx_device_memory * const mdev = (gx_device_memory *)dev;
    int code = abuf_flush(mdev);

    if (code < 0)
        return code;
    return mem_close(dev);
}

/*
 * Framework for mapping a requested imaging operation to the buffer.
 * For now, we assume top-to-bottom transfers and use a very simple algorithm.
 */
typedef struct y_transfer_s {
    int y_next;
    int height_left;
    int transfer_y;
    int transfer_height;
} y_transfer;
static int
y_transfer_init(y_transfer * pyt, gx_device * dev, int ty, int th)
{
    gx_device_memory * const mdev = (gx_device_memory *)dev;
    int bh = 1 << mdev->log2_scale.y;

    if (ty < mdev->mapped_y || ty > mdev->mapped_y + mdev->mapped_height) {
        int code = abuf_flush(mdev);
        if (code < 0)
            return code;
        mdev->mapped_y = ty & -bh;
        mdev->mapped_height = bh;
        memset(scan_line_base(mdev, 0), 0, (size_t)bh * mdev->raster);
    }
    pyt->y_next = ty;
    pyt->height_left = th;
    pyt->transfer_height = 0;

    return 0;
}
/* while ( yt.height_left > 0 ) { y_transfer_next(&yt, mdev); ... } */
static int
y_transfer_next(y_transfer * pyt, gx_device * dev)
{
    gx_device_memory * const mdev = (gx_device_memory *)dev;
    int my = mdev->mapped_y, mh = mdev->mapped_height;
    int ms = mdev->mapped_start;
    int ty = pyt->y_next += pyt->transfer_height;
    int th = pyt->height_left;
    int bh = 1 << mdev->log2_scale.y;

    /* From here on, we know that my <= ty <= my + mh. */
    int tby, tbh;

    if (ty == my + mh) {	/* Add a new block at my1. */
        if (mh == mdev->height) {
            int code = abuf_flush_block(mdev, my);

            if (code < 0)
                return code;
            mdev->mapped_y = my += bh;
            if ((mdev->mapped_start = ms += bh) == mh)
                mdev->mapped_start = ms = 0;
        } else {		/* Because we currently never extend backwards, */
            /* we know we can't wrap around in this case. */
            mdev->mapped_height = mh += bh;
        }
        memset(scan_line_base(mdev, (ms == 0 ? mh : ms) - bh),
               0, (size_t)bh * mdev->raster);
    }
    /* Now we know that my <= ty < my + mh. */
    tby = ty - my + ms;
    if (tby < mdev->height) {
        tbh = mdev->height - ms;
        if (tbh > mh)
            tbh = mh;
        tbh -= tby - ms;
    } else {			/* wrap around */
        tby -= mdev->height;
        tbh = ms + mh - dev->height - tby;
    }
    if_debug7m('V', mdev->memory,
               "[V]abuf: my=%d, mh=%d, ms=%d, ty=%d, th=%d, tby=%d, tbh=%d\n",
               my, mh, ms, ty, th, tby, tbh);
    if (tbh > th)
        tbh = th;
    pyt->height_left = th - tbh;
    pyt->transfer_y = tby;
    pyt->transfer_height = tbh;
    return 0;
}

/* Copy a monobit image. */
static int
mem_abuf_copy_mono(gx_device * dev,
               const byte * base, int sourcex, int sraster, gx_bitmap_id id,
        int x, int y, int w, int h, gx_color_index zero, gx_color_index one)
{
    gx_device_memory * const mdev = (gx_device_memory *)dev;
    y_transfer yt;
    int code;

    if (zero != gx_no_color_index || one == gx_no_color_index)
        return_error(gs_error_undefinedresult);
    x -= mdev->mapped_x;
    fit_copy_xyw(dev, base, sourcex, sraster, id, x, y, w, h);	/* don't limit h */
    if (w <= 0 || h <= 0)
        return 0;
    if (mdev->mapped_height != 0 && mdev->save_color != one) {
        /* Color has changed. Better flush. */
        int code = abuf_flush(mdev);
        if (code < 0)
            return code;
    }
    mdev->save_color = one;
    code = y_transfer_init(&yt, dev, y, h);
    if (code < 0)
        return code;
    while (yt.height_left > 0) {
        code = y_transfer_next(&yt, dev);
        if (code < 0)
            return code;
        code = mem_mono_copy_mono(dev,
                                  base + (yt.y_next - y) * sraster,
                                  sourcex, sraster, gx_no_bitmap_id,
                                  x, yt.transfer_y, w, yt.transfer_height,
                                  gx_no_color_index, (gx_color_index) 1);
        if (code < 0)
            return code;
    }
    return 0;
}

/* Fill a rectangle. */
static int
mem_abuf_fill_rectangle(gx_device * dev, int x, int y, int w, int h,
                        gx_color_index color)
{
    gx_device_memory * const mdev = (gx_device_memory *)dev;
    y_transfer yt;
    int code;

    x -= mdev->mapped_x;
    fit_fill_xy(dev, x, y, w, h);
    fit_fill_w(dev, x, w);	/* don't limit h */
    /* or check w <= 0, h <= 0 */
    if (mdev->mapped_height != 0 && mdev->save_color != color) {
        /* Color has changed. Better flush. */
        int code = abuf_flush(mdev);
        if (code < 0)
            return code;
    }
    mdev->save_color = color;
    code = y_transfer_init(&yt, dev, y, h);
    if (code < 0)
        return code;
    while (yt.height_left > 0) {
        code = y_transfer_next(&yt, dev);
        if (code < 0)
            return code;
        code = mem_mono_fill_rectangle(dev, x, yt.transfer_y,
                                       w, yt.transfer_height,
                                       (gx_color_index) 1);
        if (code < 0)
            return code;
    }
    return 0;
}

/* Fill a rectangle. */
static int
mem_abuf_fill_rectangle_hl_color(gx_device * dev, const gs_fixed_rect *rect,
                                 const gs_gstate *pgs,
                                 const gx_drawing_color *pdcolor,
                                 const gx_clip_path *pcpath)
{
    gx_device_memory * const mdev = (gx_device_memory *)dev;
    y_transfer yt;
    int x = fixed2int(rect->p.x);
    int y = fixed2int(rect->p.y);
    int w = fixed2int(rect->q.x) - x;
    int h = fixed2int(rect->q.y) - y;
    int code;
    (void)pgs;

    x -= mdev->mapped_x;
    fit_fill_xy(dev, x, y, w, h);
    fit_fill_w(dev, x, w);	/* don't limit h */
    /* or check w <= 0, h <= 0 */
    if (mdev->mapped_height != 0 &&
        memcmp(mdev->save_hl_color, pdcolor, sizeof(*pdcolor)) != 0) {
        /* Color has changed. Better flush. */
        int code = abuf_flush(mdev);
        if (code < 0)
            return code;
    }
    mdev->save_hl_color = pdcolor;
    code = y_transfer_init(&yt, dev, y, h);
    if (code < 0)
        return code;
    while (yt.height_left > 0) {
        code = y_transfer_next(&yt, dev);
        if (code < 0)
            return code;
        code = mem_mono_fill_rectangle(dev, x, yt.transfer_y,
                                       w, yt.transfer_height,
                                       (gx_color_index) 1);
        if (code < 0)
            return code;
    }
    return 0;
}

/*
 * Fill/Stroke a path.  This is the default implementation of the driver
 * fill_path procedure.
 */
int
mem_abuf_fill_stroke_path(gx_device * pdev, const gs_gstate * pgs,
                          gx_path * ppath,
                          const gx_fill_params * params_fill,
                          const gx_device_color * pdevc_fill,
                          const gx_stroke_params * params_stroke,
                          const gx_device_color * pdevc_stroke,
                          const gx_clip_path * pcpath)
{
    int code = 0;
    int code1;
    int has_comp = 1;
    overprint_abuf_state_t param;
    gx_device_memory* const mdev = (gx_device_memory*)pdev;

    param.op_trans = OP_FS_TRANS_PREFILL;
    param.pgs = pgs;
    param.pcpath = pcpath;
    param.ppath = ppath;
    param.alpha_buf_path_scale = mdev->log2_scale;

    /* Tell any overprint compositor (maybe a pdf14 device) that's listening to get ready for a fill/stroke. */
    code = dev_proc(pdev, dev_spec_op)(pdev, gxdso_abuf_optrans, &param, sizeof(param));
    if (code == gs_error_undefined)
        has_comp = false; /* No compositor listening. */
    else if (code < 0)
        return code; /* Any other error is real. */

    /* Do the fill. */
    code = dev_proc(pdev, fill_path)(pdev, pgs, ppath, params_fill, pdevc_fill, pcpath);
    if (code < 0) {
        /* If the fill failed do any tidy up necessary. */
        if (has_comp) {
            param.op_trans = OP_FS_TRANS_CLEANUP;
            code1 = dev_proc(pdev, dev_spec_op)(pdev, gxdso_abuf_optrans, &param, sizeof(param));
            if (code1 < 0)
                code = code1; /* If the pdf14 cleanup failed that is (more) fatal! */
        }
        return code;
    }
    abuf_flush(mdev);

    /* Handle stroke */
    gs_swapcolors_quick(pgs);
    if (has_comp) {
        param.op_trans = OP_FS_TRANS_PRESTROKE;
        code = dev_proc(pdev, dev_spec_op)(pdev, gxdso_abuf_optrans, &param, sizeof(param));
        if (code < 0)
        {
            gs_swapcolors_quick(pgs);
            return code;
        }
    }
    code = dev_proc(pdev, stroke_path)(pdev, pgs, ppath, params_stroke, pdevc_stroke, pcpath);
    abuf_flush(mdev);
    gs_swapcolors_quick(pgs);

    /* Tell the compositors we're done. */
    if (has_comp) {
        param.op_trans = OP_FS_TRANS_POSTSTROKE;
        code1 = dev_proc(pdev, dev_spec_op)(pdev, gxdso_abuf_optrans, &param, sizeof(param));
        if (code >= 0)
            code = code1;
    }
    return code;
}

/* Get the clipping box.  We must scale this up by the number of alpha bits. */
static void
mem_abuf_get_clipping_box(gx_device * dev, gs_fixed_rect * pbox)
{
    gx_device_memory * const mdev = (gx_device_memory *)dev;
    gx_device *tdev = mdev->target;

    (*dev_proc(tdev, get_clipping_box)) (tdev, pbox);
    pbox->p.x <<= mdev->log2_scale.x;
    pbox->p.y <<= mdev->log2_scale.y;
    pbox->q.x <<= mdev->log2_scale.x;
    pbox->q.y <<= mdev->log2_scale.y;
}

/*
 * Determine the number of bits of alpha buffer for a stroke or fill.
 * We should do alpha buffering iff this value is >1.
 */
int
alpha_buffer_bits(gs_gstate * pgs)
{
    gx_device *dev;

    dev = gs_currentdevice_inline(pgs);
    if (gs_device_is_abuf(dev)) {
        /* We're already writing into an alpha buffer. */
        return 0;
    }
    return (*dev_proc(dev, get_alpha_bits))
        (dev, (pgs->in_cachedevice ? go_text : go_graphics));
}
