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

/* Default device bitmap copying implementation */
#include "gx.h"
#include "gpcheck.h"
#include "gserrors.h"
#include "gsbittab.h"
#include "gsrect.h"
#include "gsropt.h"
#include "gxdcolor.h"
#include "gxdevice.h"
#include "gxdevmem.h"
#include "gdevmem.h"
#include "gxgetbit.h"
#undef mdev
#include "gxcpath.h"

/* Implement copy_mono by filling lots of small rectangles. */
/* This is very inefficient, but it works as a default. */
int
gx_default_copy_mono(gx_device * dev, const byte * data,
            int dx, int raster, gx_bitmap_id id, int x, int y, int w, int h,
                     gx_color_index zero, gx_color_index one)
{
    bool invert;
    gx_color_index color;
    gx_device_color devc;

    if (!data)
        return gs_throw_code(gs_error_unknownerror);
    fit_copy(dev, data, dx, raster, id, x, y, w, h);
    if (!data)
        return gs_throw_code(gs_error_unknownerror);
    if (one != gx_no_color_index) {
        invert = false;
        color = one;
        if (zero != gx_no_color_index) {
            int code = (*dev_proc(dev, fill_rectangle))
            (dev, x, y, w, h, zero);

            if (code < 0)
                return code;
        }
    } else {
        invert = true;
        color = zero;
    }
    if (!data)
        return gs_throw_code(gs_error_unknownerror);
    set_nonclient_dev_color(&devc, color);
    if (!data)
        return gs_throw_code(gs_error_unknownerror);
    return gx_dc_default_fill_masked
        (&devc, data, dx, raster, id, x, y, w, h, dev, rop3_T, invert);
}

/* Implement copy_color by filling lots of small rectangles. */
/* This is very inefficient, but it works as a default. */
int
gx_default_copy_color(gx_device * dev, const byte * data,
                      int dx, int raster, gx_bitmap_id id,
                      int x, int y, int w, int h)
{
    int depth = dev->color_info.depth;
    byte mask;

    dev_proc_fill_rectangle((*fill));
    const byte *row;
    int iy;

    if (depth == 1)
        return (*dev_proc(dev, copy_mono)) (dev, data, dx, raster, id,
                                            x, y, w, h,
                                    (gx_color_index) 0, (gx_color_index) 1);
    fit_copy(dev, data, dx, raster, id, x, y, w, h);
    fill = dev_proc(dev, fill_rectangle);
    mask = (byte) ((1 << depth) - 1);
    for (row = data, iy = 0; iy < h; row += raster, ++iy) {
        int ix;
        gx_color_index c0 = gx_no_color_index;
        const byte *ptr = row + ((dx * depth) >> 3);
        int i0;

        for (i0 = ix = 0; ix < w; ++ix) {
            gx_color_index color;

            if (depth >= 8) {
                color = *ptr++;
                switch (depth) {
                    case 64:
                        color = (color << 8) + *ptr++;
                    case 56:
                        color = (color << 8) + *ptr++;
                    case 48:
                        color = (color << 8) + *ptr++;
                    case 40:
                        color = (color << 8) + *ptr++;
                    case 32:
                        color = (color << 8) + *ptr++;
                    case 24:
                        color = (color << 8) + *ptr++;
                    case 16:
                        color = (color << 8) + *ptr++;
                }
            } else {
                uint dbit = (-(ix + dx + 1) * depth) & 7;

                color = (*ptr >> dbit) & mask;
                if (dbit == 0)
                    ptr++;
            }
            if (color != c0) {
                if (ix > i0) {
                    int code = (*fill)
                    (dev, i0 + x, iy + y, ix - i0, 1, c0);

                    if (code < 0)
                        return code;
                }
                c0 = color;
                i0 = ix;
            }
        }
        if (ix > i0) {
            int code = (*fill) (dev, i0 + x, iy + y, ix - i0, 1, c0);

            if (code < 0)
                return code;
        }
    }
    return 0;
}

int
gx_no_copy_alpha(gx_device * dev, const byte * data, int data_x,
           int raster, gx_bitmap_id id, int x, int y, int width, int height,
                 gx_color_index color, int depth)
{
    return_error(gs_error_unknownerror);
}

/* Currently we really should only be here if the target device is planar
   AND it supports devn colors AND is 8 or 16 bit.  For example tiffsep
   and psdcmyk may make use of this if AA is enabled.  It is basically
   designed for devices that need more than 64 bits for color support

   So that I can follow things and  make it readable for future generations,
   I am not using the macro nightmare that default_copy_alpha uses. */
int
gx_default_copy_alpha_hl_color(gx_device * dev, const byte * data, int data_x,
           int raster, gx_bitmap_id id, int x, int y, int width, int height,
                      const gx_drawing_color *pdcolor, int depth)
{
    const byte *row_alpha;
    gs_memory_t *mem = dev->memory;
    int bpp = dev->color_info.depth;
    uchar ncomps = dev->color_info.num_components;
    uint out_raster;
    int code = 0;
    gx_color_value src_cv[GS_CLIENT_COLOR_MAX_COMPONENTS];
    gx_color_value curr_cv[GS_CLIENT_COLOR_MAX_COMPONENTS];
    gx_color_value blend_cv[GS_CLIENT_COLOR_MAX_COMPONENTS];
    int ry;
    uchar k, j;
    gs_get_bits_params_t gb_params;
    byte *src_planes[GS_CLIENT_COLOR_MAX_COMPONENTS];
    gs_int_rect gb_rect;
    int byte_depth;
    int shift, word_width;
    gx_color_value *composite;
    byte *gb_buff;
    int x_curr, w_curr, gb_buff_start;

    byte_depth = bpp / ncomps;
    shift = 16 - byte_depth;
    word_width = byte_depth >> 3;

    fit_copy(dev, data, data_x, raster, id, x, y, width, height);
    row_alpha = data;
    out_raster = bitmap_raster(width * byte_depth);
    gb_buff = gs_alloc_bytes(mem, out_raster * ncomps, "copy_alpha_hl_color(gb_buff)");
    if (gb_buff == 0) {
        code = gs_note_error(gs_error_VMerror);
        return code;
    }
    for (k = 0; k < ncomps; k++) {
        src_cv[k] = pdcolor->colors.devn.values[k];
    }
    /* Initialize the get_bits parameters. Here we just get a plane at a  time. */
    gb_params.options =  GB_COLORS_NATIVE
                       | GB_ALPHA_NONE
                       | GB_DEPTH_ALL
                       | GB_PACKING_PLANAR
                       | GB_RETURN_COPY
                       | GB_ALIGN_STANDARD
                       | GB_OFFSET_0
                       | GB_RASTER_STANDARD
                       | GB_SELECT_PLANES;
    gb_rect.p.x = x;
    gb_rect.q.x = x + width;
    for (ry = y; ry < y + height; row_alpha += raster, ++ry) {
        int sx, rx;

        gb_rect.p.y = ry;
        gb_rect.q.y = ry+1;
        for (k = 0; k < ncomps; k++) {
            /* First set the params to zero for all planes except the one we want */
            /* I am not sure why get_bits_rectangle for the planar device can
               not hand back the data in a proper planar form.  To get the
               individual planes seems that I need to jump through some hoops
               here */
            for (j = 0; j < ncomps; j++)
                gb_params.data[j] = 0;
            gb_params.data[k] = gb_buff + k * out_raster;
            code = dev_proc(dev, get_bits_rectangle) (dev, &gb_rect,
                                                      &gb_params);
            src_planes[k] = gb_params.data[k];
            if (code < 0) {
                gs_free_object(mem, gb_buff, "copy_alpha_hl_color");
                return code;
            }
        }
        /* At this point we have to carry around some additional variables
           so that we can handle any buffer flushes due to alpha == 0 values.
           See below why this is needed */
        x_curr = x;
        w_curr = 0;
        gb_buff_start = 0;
        for (sx = data_x, rx = x; sx < data_x + width; ++sx, ++rx) {
            int alpha2, alpha;

            w_curr += 1;
            switch (depth)
            {
            case 2:
                alpha = ((row_alpha[sx >> 2] >> ((3 - (sx & 3)) << 1)) & 3) * 85;
                break;
            case 4:
                alpha2 = row_alpha[sx >> 1];
                alpha = (sx & 1 ? alpha2 & 0xf : alpha2 >> 4) * 17;
                break;
            case 8:
                alpha = row_alpha[sx];
                break;
            default:
                return_error(gs_error_rangecheck);
            }

            if (alpha == 0) {
                /* With alpha 0 we want to avoid writing out this value.
                 * While it is true that writting it out leaves the color
                 * unchanged,  any device that's watching what pixels are
                 * written (such as the pattern tile devices) may have problems.
                 * As in gx_default_copy_alpha the right thing to do is to write
                 * out what we have so far and then continue to collect when we
                 * get back to non zero alpha.  */
                code = dev_proc(dev, copy_planes)(dev, &(gb_buff[gb_buff_start]),
                                                  0, out_raster, gs_no_bitmap_id,
                                                  x_curr, ry, w_curr-1, 1, 1);
                if (code < 0) {
                    gs_free_object(mem, gb_buff, "copy_alpha_hl_color");
                    return code;
                }
                /* reset ourselves */
                gb_buff_start = gb_buff_start + w_curr * word_width;
                w_curr = 0;
                x_curr = rx + 1;
            } else {
                if (alpha == 255) {
                    /* Just use the new color. */
                    composite = &(src_cv[0]);
                } else {
                    /* We need to do the weighting by the alpha value */
                    alpha += (alpha>>7); /* Expand from 0..255->0..256 */
                    /* First get the old color */
                    for (k = 0; k < ncomps; k++) {
                        /* We only have 8 and 16 bit depth to worry about.
                           However, this stuff should really be done with
                           the device encode/decode procedure. */
                        byte *ptr = ((src_planes[k]) + (sx - data_x) * word_width);
                        curr_cv[k] = 0;
                        switch (word_width) {
                            case 2:
                                curr_cv[k] += (*ptr++ << 8);
                                curr_cv[k] += *ptr;
                                break;
                            case 1:
                                curr_cv[k] += *ptr;
                                curr_cv[k] += curr_cv[k] << 8;
                        }
                        /* Now compute the new color which is a blend of
                           the old and the new */
                        blend_cv[k] =  ((curr_cv[k]<<8) +
                                        (((long) src_cv[k] - (long) curr_cv[k]) * alpha))>>8;
                        composite = &(blend_cv[0]);
                    }
                }
                /* Update our plane data buffers.  Just reuse the current one */
                for (k = 0; k < ncomps; k++) {
                    byte *ptr = ((src_planes[k]) + (sx - data_x) * word_width);
                    switch (word_width) {
                        case 2:
                            *ptr++ = composite[k] >> 8;
                        case 1:
                            *ptr++ = composite[k] >> shift;
                    }
                }
            } /* else on alpha != 0 */
        } /* loop on x */
        /* Flush what ever we have left.  We may only have a partial due to
           the presence of alpha = 0 values */
        code = dev_proc(dev, copy_planes)(dev, &(gb_buff[gb_buff_start]),
                                          0, out_raster, gs_no_bitmap_id,
                                          x_curr, ry, w_curr, 1, 1);
    } /* loop on y */
    gs_free_object(mem, gb_buff, "copy_alpha_hl_color");
    return code;
}

int
gx_default_copy_alpha(gx_device * dev, const byte * data, int data_x,
           int raster, gx_bitmap_id id, int x, int y, int width, int height,
                      gx_color_index color, int depth)
{				/* This might be called with depth = 1.... */
    if (depth == 1)
        return (*dev_proc(dev, copy_mono)) (dev, data, data_x, raster, id,
                                            x, y, width, height,
                                            gx_no_color_index, color);
    /*
     * Simulate alpha by weighted averaging of RGB values.
     * This is very slow, but functionally correct.
     */
    {
        const byte *row;
        gs_memory_t *mem = dev->memory;
        int bpp = dev->color_info.depth;
        uchar ncomps = dev->color_info.num_components;
        uint in_size = gx_device_raster_chunky(dev, false);
        byte *lin;
        uint out_size;
        byte *lout;
        int code = 0;
        gx_color_value color_cv[GX_DEVICE_COLOR_MAX_COMPONENTS];
        int ry, lx;
        gs_int_rect rect;

        fit_copy(dev, data, data_x, raster, id, x, y, width, height);
        row = data;
        out_size = bitmap_raster(width * bpp);
        lin = gs_alloc_bytes(mem, in_size, "copy_alpha(lin)");
        lout = gs_alloc_bytes(mem, out_size, "copy_alpha(lout)");
        if (lin == 0 || lout == 0) {
            code = gs_note_error(gs_error_VMerror);
            goto out;
        }
        (*dev_proc(dev, decode_color)) (dev, color, color_cv);
        rect.p.x = 0;
        rect.q.x = dev->width;
        for (ry = y; ry < y + height; row += raster, ++ry) {
            byte *line;
            int sx, rx;

            byte *l_dptr = lout;
            int l_dbit = 0;
            byte l_dbyte = ((l_dbit) ? (byte)(*(l_dptr) & (0xff00 >> (l_dbit))) : 0);
            int l_xprev = x;
            gs_get_bits_params_t params;

            params.options = (GB_ALIGN_STANDARD |
                              (GB_RETURN_COPY | GB_RETURN_POINTER) |
                              GB_OFFSET_0 |
                              GB_RASTER_STANDARD | GB_PACKING_CHUNKY |
                              GB_COLORS_NATIVE | GB_ALPHA_NONE);
            params.x_offset = 0;
            params.raster = bitmap_raster(dev->width * dev->color_info.depth);
            params.data[0] = lin;
            rect.p.y = ry;
            rect.q.y = ry+1;
            code = (*dev_proc(dev, get_bits_rectangle))(dev, &rect,
                                                        &params);
            if (code < 0)
                break;
            line = params.data[0];
            lx = x;
            for (sx = data_x, rx = x; sx < data_x + width; ++sx, ++rx) {
                gx_color_index previous = gx_no_color_index;
                gx_color_index composite;
                int alpha2, alpha;

                switch(depth)
                {
                case 2:
                    /* map 0 - 3 to 0 - 15 */
                    alpha = ((row[sx >> 2] >> ((3 - (sx & 3)) << 1)) & 3) * 85;
                    break;
                case 4:
                    alpha2 = row[sx >> 1],
                        alpha = (sx & 1 ? alpha2 & 0xf : alpha2 >> 4) * 17;
                    break;
                case 8:
                    alpha = row[sx];
                    break;
                default:
                    return_error(gs_error_rangecheck);
                }
              blend:
                if (alpha == 0) {
                    /* Previously the code used to just write out the previous
                     * colour when the alpha was 0, but that's wrong. It leaves
                     * the underlying colour unchanged, but has the effect of
                     * making this pixel appear solid in any device that's
                     * watching what pixels are written (such as the pattern
                     * tile devices). The right thing to do is to write out
                     * the buffered accumulator, and skip over any pixels that
                     * are completely clear. */
                    if (rx > l_xprev ) {
                        sample_store_flush(l_dptr, l_dbit, l_dbyte);
                        code = (*dev_proc(dev, copy_color))
                          (dev, lout, l_xprev - (lx), out_size,
                           gx_no_bitmap_id, l_xprev, ry, (rx) - l_xprev, 1);
                        if ( code < 0 )
                          return code;
                    }
                    l_dptr = lout;
                    l_dbit = 0;
                    l_dbyte = (l_dbit ? (byte)(*l_dptr & (0xff00 >> l_dbit)) : 0);
                    l_xprev = rx+1;
                    lx = rx+1;
                } else {
                    if (alpha == 255) {	/* Just write the new color. */
                        composite = color;
                    } else {
                        gx_color_value cv[GX_DEVICE_COLOR_MAX_COMPONENTS];
                        uchar i;
                        int alpha2 = alpha + (alpha>>7);

                        if (previous == gx_no_color_index) {	/* Extract the old color. */
                            if (bpp < 8) {
                                const uint bit = rx * bpp;
                                const byte *src = line + (bit >> 3);

                                previous =
                                    (*src >> (8 - ((bit & 7) + bpp))) &
                                    ((1 << bpp) - 1);
                            } else {
                                const byte *src = line + (rx * (bpp >> 3));

                                previous = 0;
                                switch (bpp >> 3) {
                                    case 8:
                                        previous += (gx_color_index) * src++
                                            << SAMPLE_BOUND_SHIFT(previous, 56);
                                    case 7:
                                        previous += (gx_color_index) * src++
                                            << SAMPLE_BOUND_SHIFT(previous, 48);
                                    case 6:
                                        previous += (gx_color_index) * src++
                                            << SAMPLE_BOUND_SHIFT(previous, 40);
                                    case 5:
                                        previous += (gx_color_index) * src++
                                            << SAMPLE_BOUND_SHIFT(previous, 32);
                                    case 4:
                                        previous += (gx_color_index) * src++ << 24;
                                    case 3:
                                        previous += (gx_color_index) * src++ << 16;
                                    case 2:
                                        previous += (gx_color_index) * src++ << 8;
                                    case 1:
                                        previous += *src++;
                                }
                            }
                        }
                        (*dev_proc(dev, decode_color)) (dev, previous, cv);
#if ARCH_INTS_ARE_SHORT
#  define b_int long
#else
#  define b_int int
#endif
#define make_shade(old, clr, alpha) \
  (((((b_int)(old))<<8) + (((b_int)(clr) - (b_int)(old)) * (alpha)))>>8)
                        for (i=0; i<ncomps; i++)
                            cv[i] = make_shade(cv[i], color_cv[i], alpha2);
#undef b_int
#undef make_shade
                        composite =
                            (*dev_proc(dev, encode_color)) (dev, cv);
                        if (composite == gx_no_color_index) {	/* The device can't represent this color. */
                            /* Move the alpha value towards 0 or 1. */
                            if (alpha == 127)	/* move 1/2 towards 1 */
                                ++alpha;
                            alpha = (alpha & 128) | (alpha >> 1);
                            goto blend;
                        }
                    }
                    if (sizeof(composite) > 4) {
                        if (sample_store_next64(composite, &l_dptr, &l_dbit, bpp, &l_dbyte) < 0)
                            return_error(gs_error_rangecheck);
                    }
                    else {
                        if (sample_store_next32(composite, &l_dptr, &l_dbit, bpp, &l_dbyte) < 0)
                            return_error(gs_error_rangecheck);
                    }
                }
            }
            if ( rx > l_xprev ) {
                sample_store_flush(l_dptr, l_dbit, l_dbyte);
                code = (*dev_proc(dev, copy_color))
                  (dev, lout, l_xprev - lx, out_size,
                   gx_no_bitmap_id, l_xprev, ry, rx - l_xprev, 1);
                if (code < 0)
                    return code;
            }
        }
      out:gs_free_object(mem, lout, "copy_alpha(lout)");
        gs_free_object(mem, lin, "copy_alpha(lin)");
        return code;
    }
}

int
gx_default_fill_mask(gx_device * orig_dev,
                     const byte * data, int dx, int raster, gx_bitmap_id id,
                     int x, int y, int w, int h,
                     const gx_drawing_color * pdcolor, int depth,
                     gs_logical_operation_t lop, const gx_clip_path * pcpath)
{
    gx_device *dev = orig_dev;
    gx_device_clip cdev;

    if (w == 0 || h == 0)
        return 0;

    if (pcpath != 0)
    {
        gs_fixed_rect rect;
        int tmp;

        rect.p.x = int2fixed(x);
        rect.p.y = int2fixed(y);
        rect.q.x = int2fixed(x+w);
        rect.q.y = int2fixed(y+h);
        dev = gx_make_clip_device_on_stack_if_needed(&cdev, pcpath, dev, &rect);
        if (dev == NULL)
            return 0;
        /* Clip region if possible */
        tmp = fixed2int(rect.p.x);
        if (tmp > x)
        {
            dx += tmp-x;
            x = tmp;
        }
        tmp = fixed2int(rect.q.x);
        if (tmp < x+w)
            w = tmp-x;
        tmp = fixed2int(rect.p.y);
        if (tmp > y)
        {
            data += (tmp-y) * raster;
            y = tmp;
        }
        tmp = fixed2int(rect.q.y);
        if (tmp < y+h)
            h = tmp-y;
    }
    if (depth > 1) {
        /****** CAN'T DO ROP OR HALFTONE WITH ALPHA ******/
        return (*dev_proc(dev, copy_alpha))
            (dev, data, dx, raster, id, x, y, w, h,
             gx_dc_pure_color(pdcolor), depth);
    } else
        return pdcolor->type->fill_masked(pdcolor, data, dx, raster, id,
                                          x, y, w, h, dev, lop, false);
}

/* Default implementation of strip_tile_rect_devn.  With the current design
   only devices that support devn color will be making use of this
   procedure and those are planar devices.  So we have an implemenation
   for planar devices and not a default implemenetation at this time. */
int
gx_default_strip_tile_rect_devn(gx_device * dev, const gx_strip_bitmap * tiles,
   int x, int y, int w, int h, const gx_drawing_color * pdcolor0,
   const gx_drawing_color * pdcolor1, int px, int py)
{
    return_error(gs_error_unregistered);
}

/* Default implementation of strip_tile_rectangle */
int
gx_default_strip_tile_rectangle(gx_device * dev, const gx_strip_bitmap * tiles,
   int x, int y, int w, int h, gx_color_index color0, gx_color_index color1,
                                int px, int py)
{				/* Fill the rectangle in chunks. */
    int width = tiles->size.x;
    int height = tiles->size.y;
    int raster = tiles->raster;
    int rwidth = tiles->rep_width;
    int rheight = tiles->rep_height;
    int shift = tiles->shift;
    gs_id tile_id = tiles->id;

    if (rwidth == 0 || rheight == 0)
        return_error(gs_error_unregistered); /* Must not happen. */
    fit_fill_xy(dev, x, y, w, h);

#ifdef DEBUG
    if (gs_debug_c('t')) {
        int ptx, pty;
        const byte *ptp = tiles->data;

        dmlprintf4(dev->memory, "[t]tile %dx%d raster=%d id=%lu;",
                   tiles->size.x, tiles->size.y, tiles->raster, tiles->id);
        dmlprintf6(dev->memory, " x,y=%d,%d w,h=%d,%d p=%d,%d\n",
                   x, y, w, h, px, py);
        dmlputs(dev->memory, "");
        for (pty = 0; pty < tiles->size.y; pty++) {
            dmprintf(dev->memory, "   ");
            for (ptx = 0; ptx < tiles->raster; ptx++)
                dmprintf1(dev->memory, "%3x", *ptp++);
        }
        dmputc(dev->memory, '\n');
    }
#endif

    {				/*
                                 * Note: we can't do the following computations until after
                                 * the fit_fill_xy.
                                 */
        int xoff =
        (shift == 0 ? px :
         px + (y + py) / rheight * tiles->rep_shift);
        int irx = ((rwidth & (rwidth - 1)) == 0 ?	/* power of 2 */
                   (x + xoff) & (rwidth - 1) :
                   (x + xoff) % rwidth);
        int ry = ((rheight & (rheight - 1)) == 0 ?	/* power of 2 */
                  (y + py) & (rheight - 1) :
                  (y + py) % rheight);
        int icw = width - irx;
        int ch = height - ry;
        byte *row = tiles->data + ry * raster;

        dev_proc_copy_mono((*proc_mono));
        dev_proc_copy_color((*proc_color));
        dev_proc_copy_planes((*proc_planes));
        int code = 0;

        if (color0 == gx_no_color_index && color1 == gx_no_color_index) {
            if (tiles->num_planes > 1) {
                proc_mono = 0;
                proc_color = 0;
                proc_planes = dev_proc(dev, copy_planes);
            } else {
                proc_planes = 0;
                proc_color = dev_proc(dev, copy_color);
                proc_mono = 0;
            }
        } else {
            proc_planes = 0;
            proc_color = 0;
            proc_mono = dev_proc(dev, copy_mono);
        }

#define GX_DEFAULT_COPY_TILE(dev, srcx, tx, ty, tw, th, tid) do {\
                if_debug6m('t', (dev)->memory, "   copy id=%lu sx=%d => x=%d y=%d w=%d h=%d\n", tid, srcx, tx, ty, tw, th);\
                if (tiles->num_planes > 1) {\
                    if (proc_planes)\
                        code = (*proc_planes)(dev, row, srcx, raster, tid, tx, ty, tw, th, height);\
                } else {\
                    if (proc_color != 0) {\
                        code = (*proc_color)(dev, row, srcx, raster, tid, tx, ty, tw, th);\
                    } else {\
                        if (proc_mono)\
                            code = (*proc_mono)(dev, row, srcx, raster, tid, tx, ty, tw, th, color0, color1);\
                         else code = 0;\
                    }\
                }\
                if (code < 0) return_error(code);\
                } while (0);


        if (ch >= h) {		/* Shallow operation */
            if (icw >= w) {	/* Just one (partial) tile to transfer. */
                GX_DEFAULT_COPY_TILE(dev, irx, x, y, w, h, (w == width && h == height ? tile_id : gs_no_bitmap_id));
            } else {
                int ex = x + w;
                int fex = ex - width;
                int cx = x + icw;
                ulong id = (h == height ? tile_id : gs_no_bitmap_id);

                GX_DEFAULT_COPY_TILE(dev, irx, x, y, icw, h, gs_no_bitmap_id);
                while (cx <= fex) {
                    GX_DEFAULT_COPY_TILE(dev, 0, cx, y, width, h, id);
                    cx += width;
                }
                if (cx < ex) {
                    GX_DEFAULT_COPY_TILE(dev, 0, cx, y, ex - cx, h, gs_no_bitmap_id);
                }
            }
        } else if (icw >= w && shift == 0) {
            /* Narrow operation, no shift */
            int ey = y + h;
            int fey = ey - height;
            int cy = y + ch;
            ulong id = (w == width ? tile_id : gs_no_bitmap_id);

            GX_DEFAULT_COPY_TILE(dev, irx, x, y, w, ch, (ch == height ? id : gs_no_bitmap_id));
            row = tiles->data;
            do {
                ch = (cy > fey ? ey - cy : height);
                GX_DEFAULT_COPY_TILE(dev, irx, x, cy, w, ch,
                          (ch == height ? id : gs_no_bitmap_id));
            }
            while ((cy += ch) < ey);
        } else {
            /* Full operation.  If shift != 0, some scan lines */
            /* may be narrow.  We could test shift == 0 in advance */
            /* and use a slightly faster loop, but right now */
            /* we don't bother. */
            int ex = x + w, ey = y + h;
            int fex = ex - width, fey = ey - height;
            int cx, cy;

            for (cy = y;;) {
                ulong id = (ch == height ? tile_id : gs_no_bitmap_id);

                if (icw >= w) {
                    GX_DEFAULT_COPY_TILE(dev, irx, x, cy, w, ch,
                              (w == width ? id : gs_no_bitmap_id));
                } else {
                    GX_DEFAULT_COPY_TILE(dev, irx, x, cy, icw, ch, gs_no_bitmap_id);
                    cx = x + icw;
                    while (cx <= fex) {
                        GX_DEFAULT_COPY_TILE(dev, 0, cx, cy, width, ch, id);
                        cx += width;
                    }
                    if (cx < ex) {
                        GX_DEFAULT_COPY_TILE(dev, 0, cx, cy, ex - cx, ch, gs_no_bitmap_id);
                    }
                }
                if ((cy += ch) >= ey)
                    break;
                ch = (cy > fey ? ey - cy : height);
                if ((irx += shift) >= rwidth)
                    irx -= rwidth;
                icw = width - irx;
                row = tiles->data;
            }
        }
#undef GX_DEFAULT_COPY_TILE
    }
    return 0;
}

int
gx_no_strip_copy_rop2(gx_device * dev,
             const byte * sdata, int sourcex, uint sraster, gx_bitmap_id id,
                     const gx_color_index * scolors,
           const gx_strip_bitmap * textures, const gx_color_index * tcolors,
                     int x, int y, int width, int height,
                     int phase_x, int phase_y, gs_logical_operation_t lop,
                     uint planar_height)
{
    return_error(gs_error_unknownerror);	/* not implemented */
}

/* ---------------- Unaligned copy operations ---------------- */

/*
 * Implementing unaligned operations in terms of the standard aligned
 * operations requires adjusting the bitmap origin and/or the raster to be
 * aligned.  Adjusting the origin is simple; adjusting the raster requires
 * doing the operation one scan line at a time.
 */
int
gx_copy_mono_unaligned(gx_device * dev, const byte * data,
            int dx, int raster, gx_bitmap_id id, int x, int y, int w, int h,
                       gx_color_index zero, gx_color_index one)
{
    dev_proc_copy_mono((*copy_mono)) = dev_proc(dev, copy_mono);
    uint offset = ALIGNMENT_MOD(data, align_bitmap_mod);
    int step = raster & (align_bitmap_mod - 1);

    /* Adjust the origin. */
    data -= offset;
    dx += offset << 3;

    /* Adjust the raster. */
    if (!step) {		/* No adjustment needed. */
        return (*copy_mono) (dev, data, dx, raster, id,
                             x, y, w, h, zero, one);
    }
    /* Do the transfer one scan line at a time. */
    {
        const byte *p = data;
        int d = dx;
        int code = 0;
        int i;

        for (i = 0; i < h && code >= 0;
             ++i, p += raster - step, d += step << 3
            )
            code = (*copy_mono) (dev, p, d, raster, gx_no_bitmap_id,
                                 x, y + i, w, 1, zero, one);
        return code;
    }
}

int
gx_copy_color_unaligned(gx_device * dev, const byte * data,
                        int data_x, int raster, gx_bitmap_id id,
                        int x, int y, int width, int height)
{
    dev_proc_copy_color((*copy_color)) = dev_proc(dev, copy_color);
    int depth = dev->color_info.depth;
    uint offset = (uint) (data - (const byte *)0) & (align_bitmap_mod - 1);
    int step = raster & (align_bitmap_mod - 1);

    /*
     * Adjust the origin.
     * We have to do something very special for 24-bit data,
     * because that is the only depth that doesn't divide
     * align_bitmap_mod exactly.  In particular, we need to find
     * M*B + R == 0 mod 3, where M is align_bitmap_mod, R is the
     * offset value just calculated, and B is an integer unknown;
     * the new value of offset will be M*B + R.
     */
    if (depth == 24)
        offset += (offset % 3) *
            (align_bitmap_mod * (3 - (align_bitmap_mod % 3)));
    data -= offset;
    data_x += (offset << 3) / depth;

    /* Adjust the raster. */
    if (!step) {		/* No adjustment needed. */
        return (*copy_color) (dev, data, data_x, raster, id,
                              x, y, width, height);
    }
    /* Do the transfer one scan line at a time. */
    {
        const byte *p = data;
        int d = data_x;
        int dstep = (step << 3) / depth;
        int code = 0;
        int i;

        for (i = 0; i < height && code >= 0;
             ++i, p += raster - step, d += dstep
            )
            code = (*copy_color) (dev, p, d, raster, gx_no_bitmap_id,
                                  x, y + i, width, 1);
        return code;
    }
}

int
gx_copy_alpha_unaligned(gx_device * dev, const byte * data, int data_x,
           int raster, gx_bitmap_id id, int x, int y, int width, int height,
                        gx_color_index color, int depth)
{
    dev_proc_copy_alpha((*copy_alpha)) = dev_proc(dev, copy_alpha);
    uint offset = (uint) (data - (const byte *)0) & (align_bitmap_mod - 1);
    int step = raster & (align_bitmap_mod - 1);

    /* Adjust the origin. */
    data -= offset;
    data_x += (offset << 3) / depth;

    /* Adjust the raster. */
    if (!step) {		/* No adjustment needed. */
        return (*copy_alpha) (dev, data, data_x, raster, id,
                              x, y, width, height, color, depth);
    }
    /* Do the transfer one scan line at a time. */
    {
        const byte *p = data;
        int d = data_x;
        int dstep = (step << 3) / depth;
        int code = 0;
        int i;

        for (i = 0; i < height && code >= 0;
             ++i, p += raster - step, d += dstep
            )
            code = (*copy_alpha) (dev, p, d, raster, gx_no_bitmap_id,
                                  x, y + i, width, 1, color, depth);
        return code;
    }
}
