/* Copyright (C) 2001-2024 Artifex Software, Inc.
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

/* Default and device-independent RasterOp algorithms */
#include "memory_.h"
#include "gx.h"
#include "gsbittab.h"
#include "gserrors.h"
#include "gsropt.h"
#include "gxcindex.h"
#include "gxdcolor.h"
#include "gxdevice.h"
#include "gxdevmem.h"
#include "gxgetbit.h"
#include "gdevmem.h"            /* for mem_default_strip_copy_rop prototype */
#include "gdevmpla.h"
#include "gdevmrop.h"
#include "gxdevsop.h"
#include "stdint_.h"
#ifdef WITH_CAL
#include "cal.h"
#endif

/*
 * Define the maximum amount of space we are willing to allocate for a
 * multiple-row RasterOp buffer.  (We are always willing to allocate
 * one row, no matter how wide.)
 */
static const uint max_rop_bitmap = 1000;

/* ---------------- Debugging aids ---------------- */

#ifdef DEBUG

void
trace_copy_rop(const char *cname, gx_device * dev,
               const byte * sdata, int sourcex, uint sraster, gx_bitmap_id id,
               const gx_color_index * scolors,
               const gx_strip_bitmap * textures,
               const gx_color_index * tcolors,
               int x, int y, int width, int height,
               int phase_x, int phase_y, gs_logical_operation_t lop)
{
    dmlprintf4(dev->memory, "%s: dev="PRI_INTPTR"(%s) depth=%d\n",
               cname, (intptr_t)dev, dev->dname, dev->color_info.depth);
    dmlprintf4(dev->memory, "  source data="PRI_INTPTR" x=%d raster=%u id=%lu colors=",
               (intptr_t)sdata, sourcex, sraster, (ulong) id);
    if (scolors)
        dmprintf2(dev->memory, "(%"PRIx64",%"PRIx64");\n", (uint64_t)scolors[0], (uint64_t)scolors[1]);
    else
        dmputs(dev->memory, "none;\n");
    if (textures)
        dmlprintf8(dev->memory, "  textures="PRI_INTPTR" size=%dx%d(%dx%d) raster=%u shift=%d(%d)",
                  (intptr_t)textures, textures->size.x, textures->size.y,
                  textures->rep_width, textures->rep_height,
                  textures->raster, textures->shift, textures->rep_shift);
    else
        dmlputs(dev->memory, "  textures=none");
    if (tcolors)
        dmprintf2(dev->memory, " colors=(%"PRIx64",%"PRIx64")\n", (uint64_t)tcolors[0], (uint64_t)tcolors[1]);
    else
        dmputs(dev->memory, " colors=none\n");
    dmlprintf7(dev->memory, "  rect=(%d,%d),(%d,%d) phase=(%d,%d) op=0x%x\n",
              x, y, x + width, y + height, phase_x, phase_y,
              (uint) lop);
    if (gs_debug_c('B')) {
        if (sdata)
            debug_dump_bitmap(dev->memory, sdata, sraster, height, "source bits");
        if (textures && textures->data)
            debug_dump_bitmap(dev->memory, textures->data, textures->raster,
                              textures->size.y, "textures bits");
    }
}

#endif

/* ---------------- Default copy_rop implementations ---------------- */

int
gx_default_strip_copy_rop2(gx_device * dev,
                           const byte * sdata, int sourcex,
                           uint sraster, gx_bitmap_id id,
                           const gx_color_index * scolors,
                           const gx_strip_bitmap * textures,
                           const gx_color_index * tcolors,
                           int x, int y, int width, int height,
                           int phase_x, int phase_y,
                           gs_logical_operation_t lop,
                           uint planar_height)
{
    int depth = dev->color_info.depth;
    gs_memory_t *mem = dev->memory;
    const gx_device_memory *mdproto = gdev_mem_device_for_bits(depth);
    gx_device_memory *pmdev;
    uint draster;
    byte *row = 0;
    gs_int_rect rect;
    int max_height;
    int block_height;
    int code;
    int py;
    int is_planar = 0;
    int plane_depth;

#ifdef DEBUG
    if (gs_debug_c('b'))
        trace_copy_rop("gx_default_strip_copy_rop2",
                       dev, sdata, sourcex, sraster,
                       id, scolors, textures, tcolors,
                       x, y, width, height, phase_x, phase_y, lop);
#endif
    if (mdproto == 0)
        return_error(gs_error_rangecheck);
    if (sdata == 0) {
        fit_fill(dev, x, y, width, height);
    } else {
        fit_copy(dev, sdata, sourcex, sraster, id, x, y, width, height);
    }
    draster = bitmap_raster(width * depth);
    max_height = max_rop_bitmap / draster;
    if (max_height == 0)
        max_height = 1;
    block_height = min(height, max_height);
    if (planar_height > 0)
        block_height = planar_height;
    gs_make_mem_device_with_copydevice(&pmdev, mdproto, mem, -1, dev);
    pmdev->width = width;
    pmdev->height = block_height;
    pmdev->bitmap_memory = mem;
    pmdev->color_info = dev->color_info;
    if (dev->num_planar_planes)
    {
        gx_render_plane_t planes[GX_DEVICE_COLOR_MAX_COMPONENTS];
        uchar num_comp = dev->num_planar_planes;
        uchar i;
        plane_depth = dev->color_info.depth / num_comp;
        for (i = 0; i < num_comp; i++)
        {
            planes[i].shift = plane_depth * (num_comp - 1 - i);
            planes[i].depth = plane_depth;
            planes[i].index = i;
        }
        /* RJW: This code, like most of ghostscripts planar support,
         * will only work if every plane has the same depth. */
        draster = bitmap_raster(width * planes[0].depth);
        code = gdev_mem_set_planar(pmdev, num_comp, planes);
        if (code < 0)
            return code;
        is_planar = 1;
    }
    code = (*dev_proc(pmdev, open_device))((gx_device *)pmdev);
    pmdev->is_open = true; /* not sure why we need this, but we do. */
    if (code < 0)
        return code;
    lop = lop_sanitize(lop);
    if (rop3_uses_D(lop)) {
        row = gs_alloc_bytes(mem, (size_t)draster * block_height, "copy_rop row");
        if (row == 0) {
            code = gs_note_error(gs_error_VMerror);
            goto out;
        }
    }
    rect.p.x = x;
    rect.q.x = x + width;
    for (py = y; py < y + height; py += block_height) {
        if (block_height > y + height - py)
            block_height = y + height - py;
        rect.p.y = py;
        rect.q.y = py + block_height;
        if (row /*uses_d*/) {
            gs_get_bits_params_t bit_params;

            bit_params.options =
                GB_COLORS_NATIVE | GB_ALPHA_NONE | GB_DEPTH_ALL |
                GB_PACKING_CHUNKY | GB_RETURN_ALL | GB_ALIGN_STANDARD |
                GB_OFFSET_0 | GB_OFFSET_ANY | GB_RASTER_STANDARD;
            bit_params.data[0] = row;
            bit_params.x_offset = 0;
            code = (*dev_proc(dev, get_bits_rectangle))
                (dev, &rect, &bit_params);
            if (code < 0)
                break;
            code = (*dev_proc(pmdev, copy_color))
                ((gx_device *)pmdev, bit_params.data[0], bit_params.x_offset,
                 draster, gx_no_bitmap_id, 0, 0, width,
                 block_height);
            if (code < 0)
                return code;
        }
        code = (*dev_proc(pmdev, strip_copy_rop2))
                        ((gx_device *)pmdev,
                         sdata + (py - y) * sraster, sourcex, sraster,
                         gx_no_bitmap_id, scolors, textures, tcolors,
                         0, 0, width, block_height,
                         phase_x + x, phase_y + py,
                         lop, planar_height);
        if (code < 0)
            break;
        if (is_planar) {
            code = (*dev_proc(dev, copy_planes))
                            (dev, scan_line_base(pmdev, 0), 0,
                             draster, gx_no_bitmap_id,
                             x, py, width, block_height, block_height);
        } else {
            code = (*dev_proc(dev, copy_color))
                            (dev, scan_line_base(pmdev, 0), 0,
                             draster, gx_no_bitmap_id,
                             x, py, width, block_height);
        }
        if (code < 0)
            break;
    }
out:
    gs_free_object(mem, row, "copy_rop row");
    gx_device_retain((gx_device *)pmdev, false);
    return code;
}

/* ---------------- Default memory device copy_rop ---------------- */

/* Convert color constants to standard RGB representation. */
static void
unpack_colors_to_standard(gx_device * dev, gx_color_index real_colors[2],
                          const gx_color_index * colors, int depth)
{
    int i;

    for (i = 0; i < 2; ++i) {
        gx_color_value rgb[3];
        gx_color_index pixel;

        (*dev_proc(dev, map_color_rgb)) (dev, colors[i], rgb);
        pixel = gx_color_value_to_byte(rgb[0]);
        if (depth > 8) {
            pixel = (pixel << 16) +
                (gx_color_value_to_byte(rgb[1]) << 8) +
                gx_color_value_to_byte(rgb[2]);
        }
        real_colors[i] = pixel;
    }
}

/*
 * Convert RGB to the device's native format.  We special-case this for
 * 1-bit CMYK devices.
 */
static void
pack_cmyk_1bit_from_standard(gx_device_memory * dev, int y, int destx,
                             const byte * src, int width, int depth,
                             int src_depth)
{
    /*
     * This routine is only called if dev_proc(dev, map_cmyk_color) ==
     * cmyk_1bit_map_cmyk_color (implying depth == 4) and src_depth == 24.
     */
    byte *dest = scan_line_base(dev, y);
    int bit_x = destx * 4;
    byte *dp = dest + (bit_x >> 3);
    bool hi = (bit_x & 4) != 0;  /* true if last nibble filled was hi */
    byte buf = (hi ? *dp & 0xf0 : 0);
    const byte *sp = src;
    int x;

    for (x = width; --x >= 0; sp += 3) {
        byte r = sp[0], g = sp[1], b = sp[2];
        byte pixel =
            (r | g | b ?
             (((r >> 4) & 8) | ((g >> 5) & 4) | ((b >> 6) & 2)) ^ 0xe : 1);

        if ((hi = !hi))
            buf = pixel << 4;
        else
            *dp++ = buf | pixel;
    }
    if (hi && width > 0)
        *dp = buf | (*dp & 0xf);

}

static void
pack_planar_cmyk_1bit_from_standard(gx_device_memory * dev, int y, int destx,
                                    const byte * src, int width, int depth,
                                    int src_depth)
{
    /*
     * This routine is only called if dev_proc(dev, map_cmyk_color) ==
     * cmyk_1bit_map_cmyk_color (implying depth == 4) and src_depth == 24.
     */
    byte *dp[GX_DEVICE_COLOR_MAX_COMPONENTS];
    int shift = destx & 7;
    byte buf[GX_DEVICE_COLOR_MAX_COMPONENTS];
    const byte *sp = src;
    int x, plane;

    for (plane = 0; plane < 4; plane++) {
        byte *dest = scan_line_base(dev, y + plane * dev->height);
        dp[plane] = dest + (destx >> 3);
        buf[plane] = (shift == 0 ? 0 : *dp[plane] & (0xff00 >> shift));
    }

    shift = (0x80>>shift);
    for (x = width; --x >= 0;) {
        byte vr, vg, vb;

        vr = *sp++;
        vg = *sp++;
        vb = *sp++;
        if ((vr | vg | vb) == 0)
            buf[3] += shift;
        else {
            if ((vr & 0x80) == 0)
                buf[0] += shift;
            if ((vg & 0x80) == 0)
                buf[1] += shift;
            if ((vb & 0x80) == 0)
                buf[2] += shift;
        }
        shift >>= 1;
        if (shift == 0) {
            *dp[0]++ = buf[0]; buf[0] = 0;
            *dp[1]++ = buf[1]; buf[1] = 0;
            *dp[2]++ = buf[2]; buf[2] = 0;
            *dp[3]++ = buf[3]; buf[3] = 0;
            shift = 0x80;
        }
    }
    if (shift != 0x80) {
        shift += shift-1;
        *dp[0] = (*dp[0] & shift) + buf[0];
        *dp[1] = (*dp[1] & shift) + buf[1];
        *dp[2] = (*dp[2] & shift) + buf[2];
        *dp[3] = (*dp[3] & shift) + buf[3];
    }
}

static gx_color_index
map_rgb_to_color_via_cmyk(gx_device * dev, const gx_color_value rgbcv[])
{
    gx_color_value cmykcv[4];

    cmykcv[0] = gx_max_color_value - rgbcv[0];
    cmykcv[1] = gx_max_color_value - rgbcv[1];
    cmykcv[2] = gx_max_color_value - rgbcv[2];
    cmykcv[3] = (cmykcv[0] < cmykcv[1] ? min(cmykcv[0], cmykcv[2]) : min(cmykcv[1], cmykcv[2]));

    cmykcv[0] -= cmykcv[3];
    cmykcv[1] -= cmykcv[3];
    cmykcv[2] -= cmykcv[3];

    return (*dev_proc(dev, map_cmyk_color)) (dev, cmykcv);
}
static void
pack_from_standard(gx_device_memory * dev, int y, int destx, const byte * src,
                   int width, int depth, int src_depth)
{
    byte *dest = scan_line_base(dev, y);
    dev_proc_map_rgb_color((*map)) =
        (dev->color_info.num_components == 4 ?
         map_rgb_to_color_via_cmyk : dev_proc(dev, map_rgb_color));
    int bit_x = destx * depth;
    byte *dp = dest + (bit_x >> 3);
    /* RJW: I'm suspicious of this; see how shift = bit_x & 7 in the planar
     * 1bit version above? Has anything ever used the <8 bit code here? */
    int shift = (~bit_x & 7) + 1;
    byte buf = (shift == 8 ? 0 : *dp & (0xff00 >> shift));
    const byte *sp = src;
    int x;

    for (x = width; --x >= 0;) {
        byte vr, vg, vb;
        gx_color_index pixel;
        byte chop = 0x1;

        vr = *sp++;
        if (src_depth > 8) {
            vg = *sp++;
            vb = *sp++;
        } else
            vb = vg = vr;
        /*
         * We have to map back to some pixel value, even if the color
         * isn't accurate.
         */
        for (;;) {
            gx_color_value cv[3];
            cv[0] = gx_color_value_from_byte(vr);
            cv[1] = gx_color_value_from_byte(vg);
            cv[2] = gx_color_value_from_byte(vb);
            pixel = (*map) ((gx_device *)dev, cv);
            if (pixel != gx_no_color_index)
                break;
            /* Reduce the color accuracy and try again. */
            vr = (vr >= 0x80 ? vr | chop : vr & ~chop);
            vg = (vg >= 0x80 ? vg | chop : vg & ~chop);
            vb = (vb >= 0x80 ? vb | chop : vb & ~chop);
            /* Avoid overflow, CID 427553 */
            if (chop & 0x80)
                return;
            chop <<= 1;
        }
        if ((shift -= depth) >= 0)
            buf += (byte)(pixel << shift);
        else {
            switch (depth) {
            default:            /* 1, 2, 4, 8 */
                *dp++ = buf;
                shift += 8;
                buf = (byte)(pixel << shift);
                break;
            case 32:
                *dp++ = (byte)(pixel >> 24);
                *dp++ = (byte)(pixel >> 16);
                /* fall through */
            case 16:
                *dp++ = (byte)(pixel >> 8);
                *dp++ = (byte)pixel;
                shift = 0;
            }
        }
    }
    if (width > 0 && depth <= 8)
        *dp = (shift == 0 ? buf : buf + (*dp & ((1 << shift) - 1)));
}

static void
pack_planar_from_standard(gx_device_memory * dev, int y, int destx,
                          const byte * src, int width, int depth, int src_depth)
{
    /* This code assumes that all planar planes have the same depth */
    dev_proc_map_rgb_color((*map)) =
        (dev->color_info.num_components == 4 ?
         map_rgb_to_color_via_cmyk : dev_proc(dev, map_rgb_color));
    int pdepth = dev->plane_depth;
    int bit_x = destx * pdepth;
    byte *dp[GX_DEVICE_COLOR_MAX_COMPONENTS];
    int shift = (~bit_x & 7) + 1;
    byte buf[GX_DEVICE_COLOR_MAX_COMPONENTS];
    const byte *sp = src;
    int x;
    uchar plane;

    if (pdepth == 1 && dev->color_info.num_components == 4) {
        pack_planar_cmyk_1bit_from_standard(dev, y, destx, src, width,
                                            depth, src_depth);
        return;
    }

    for (plane = 0; plane < dev->color_info.num_components; plane++) {
        byte *dest = scan_line_base(dev, y + plane * dev->height);
        dp[plane] = dest + (bit_x >> 3);
        buf[plane] = (shift == 8 ? 0 : *dp[plane] & (0xff00 >> shift));
    }

    for (x = width; --x >= 0;) {
        byte vr, vg, vb;
        gx_color_index pixel;
        byte chop = 0x1;

        vr = *sp++;
        if (src_depth > 8) {
            vg = *sp++;
            vb = *sp++;
        } else
            vb = vg = vr;
        /*
         * We have to map back to some pixel value, even if the color
         * isn't accurate.
         */
        for (;;) {
            gx_color_value cv[3];
            cv[0] = gx_color_value_from_byte(vr);
            cv[1] = gx_color_value_from_byte(vg);
            cv[2] = gx_color_value_from_byte(vb);
            pixel = (*map) ((gx_device *)dev, cv);
            if (pixel != gx_no_color_index)
                break;
            /* Reduce the color accuracy and try again. */
            vr = (vr >= 0x80 ? vr | chop : vr & ~chop);
            vg = (vg >= 0x80 ? vg | chop : vg & ~chop);
            vb = (vb >= 0x80 ? vb | chop : vb & ~chop);
            /* Avoid overflow, CID 427561 */
            if (chop & 0x80)
                return;
            chop <<= 1;
        }
        switch (depth) {
            case 32:
                *dp[0]++ = (byte)(pixel >> 24);
                *dp[1]++ = (byte)(pixel >> 16);
                *dp[2]++ = (byte)(pixel >> 8);
                *dp[3]++ = (byte)pixel;
                shift = 0;
                break;
            case 24:
                *dp[0]++ = (byte)(pixel >> 16);
                *dp[1]++ = (byte)(pixel >> 8);
                *dp[2]++ = (byte)pixel;
                shift = 0;
                break;
            case 16:
                *dp[0]++ = (byte)(pixel >> 8);
                *dp[1]++ = (byte)pixel;
                shift = 0;
                break;
            default:            /* 1, 2, 4, 8 */
            {
                int pmask = (1<<pdepth)-1;

#ifdef ORIGINAL_CODE_KEPT_FOR_REFERENCE
                /* Original code, kept for reference. I believe this copies
                 * bits in the wrong order (i.e. the 0th component comes from
                 * the lowest bits in pixel, rather than the highest), and
                 * gets them from the wrong place (8 bits apart rather than
                 * pdepth), but as I have no examples that actually tickle
                 * this code, currently, I don't want to throw it away. */
                int pshift = 8-pdepth;
#else
                /* We have pdepth*num_planes bits in 'pixel'. We need to copy
                 * them (topmost bits first) into the buffer, packing them at
                 * shift position. */
                int pshift = pdepth*(dev->color_info.num_components-1);
#endif
                /* Can we fit another pdepth bits into our buffer? */
                shift -= pdepth;
                if (shift < 0) {
                    /* No, so flush the buffer to the planes. */
                    for (plane = 0; plane < dev->color_info.num_components; plane++)
                        *dp[plane]++ = buf[plane];
                    shift += 8;
                }
                /* Copy the next pdepth bits into each planes buffer */
#ifdef ORIGINAL_CODE_KEPT_FOR_REFERENCE
                for (plane = 0; plane < dev->color_info.num_components; pshift+=8,plane++)
                    buf[plane] += (byte)(((pixel>>pshift) & pmask)<<shift);
#else
                for (plane = 0; plane < dev->color_info.num_components; pshift-=pdepth,plane++)
                    buf[plane] += (byte)(((pixel>>pshift) & pmask)<<shift);
#endif
                break;
            }
        }
    }
    if (width > 0 && depth <= 8) {
        if (shift == 0)
            for (plane = 0; plane < dev->color_info.num_components; plane++)
                *dp[plane] = buf[plane];
        else {
            int mask = (1<<shift)-1;
            for (plane = 0; plane < dev->color_info.num_components; plane++)
                *dp[plane] = (*dp[plane] & mask) + buf[plane];
        }
    }
}

/*
 * The default implementation for memory devices uses get_bits_rectangle to
 * read out the pixels and convert them to standard (8-bit gray or 24-bit
 * RGB) representation, the standard memory device implementation to do the
 * operation, pack_from_standard to convert them back to the device
 * representation, and copy_color to write the pixels back.
 */
static int
do_strip_copy_rop(gx_device * dev,
                           const byte * sdata, int sourcex,
                           uint sraster, gx_bitmap_id id,
                           const gx_color_index * scolors,
                           const gx_strip_bitmap * textures,
                           const gx_color_index * tcolors,
                           int x, int y, int width, int height,
                           int phase_x, int phase_y,
                           gs_logical_operation_t olop)
{
    int depth = dev->color_info.depth;
    int rop_depth = (gx_device_has_color(dev) ? 24 : 8);
    void (*pack)(gx_device_memory *, int, int, const byte *, int, int, int);
    const gx_bitmap_format_t no_expand_options =
        GB_COLORS_NATIVE | GB_ALPHA_NONE | GB_DEPTH_ALL |
        GB_PACKING_CHUNKY | GB_RETURN_ALL | GB_ALIGN_STANDARD |
        GB_OFFSET_0 | GB_OFFSET_ANY | GB_RASTER_STANDARD;
    const gx_bitmap_format_t no_expand_t_options =
        GB_COLORS_NATIVE | GB_ALPHA_NONE | GB_DEPTH_ALL |
        GB_RETURN_ALL | GB_ALIGN_STANDARD |
        GB_OFFSET_0 | GB_OFFSET_ANY | GB_RASTER_STANDARD |
        ((textures && textures->num_planes > 1) ? GB_PACKING_PLANAR : GB_PACKING_CHUNKY);
    const gx_bitmap_format_t expand_options =
        (rop_depth > 8 ? GB_COLORS_RGB : GB_COLORS_GRAY) |
        GB_ALPHA_NONE | GB_DEPTH_8 |
        GB_PACKING_CHUNKY | GB_RETURN_COPY | GB_ALIGN_STANDARD |
        GB_OFFSET_0 | GB_RASTER_STANDARD;
    gs_memory_t *mem = dev->memory;
    const gx_device_memory *mdproto = gdev_mem_device_for_bits(rop_depth);
    gx_device_memory mdev;
    union { long l; void *p; } mdev_storage[20];
    uint row_raster = bitmap_raster(width * depth);
    size_t size_from_mem_device;
    gs_logical_operation_t lop = lop_sanitize(olop);
    bool uses_d = rop3_uses_D(lop);
    bool uses_s = rop3_uses_S(lop);
    bool uses_t = rop3_uses_T(lop);
    bool expand_s, expand_t;
    byte *row = 0;
    union { long l; void *p; } dest_buffer[16];
    byte *source_row = 0;
    uint source_row_raster = 0; /* init to quiet compiler warning */
    union { long l; void *p; } source_buffer[16];
    byte *texture_row = 0;
    uint texture_row_raster;
    union { long l; void *p; } texture_buffer[16];
    gx_color_index source_colors[2];
    const gx_color_index *real_scolors = scolors;
    gx_color_index texture_colors[2];
    const gx_color_index *real_tcolors = tcolors;
    gx_strip_bitmap rop_texture;
    const gx_strip_bitmap *real_texture = textures;
    gs_int_rect rect;
    gs_get_bits_params_t bit_params;
    gs_get_bits_params_t expand_params;
    gs_get_bits_params_t no_expand_params;
    gs_get_bits_params_t no_expand_t_params;
    int max_height;
    int block_height, loop_height;
    int code;
    int py;
    gx_device_memory *tdev = (gx_device_memory *)dev;

/*
 * Allocate a temporary row buffer.  Free variables: mem, block_height.
 * Labels used: out.
 */
#define ALLOC_BUF(buf, prebuf, size, cname)\
        BEGIN\
          uint num_bytes = (size) * block_height;\
\
          if (num_bytes <= sizeof(prebuf))\
            buf = (byte *)prebuf;\
          else {\
            buf = gs_alloc_bytes(mem, num_bytes, cname);\
            if (buf == 0) {\
              code = gs_note_error(gs_error_VMerror);\
              goto out;\
            }\
          }\
        END

    /* We know the device is a memory device, so we can store the
     * result directly into its scan lines, unless it is planar. */
    if (!tdev->num_planar_planes || tdev->color_info.num_components <= 1) {
        if ((rop_depth == 24) && (dev_proc(dev, dev_spec_op)(dev,
                                      gxdso_is_std_cmyk_1bit, NULL, 0) > 0)) {
            pack = pack_cmyk_1bit_from_standard;
        } else {
            pack = pack_from_standard;
        }
    } else {
        pack = pack_planar_from_standard;
    }
#ifdef DEBUG
    if (gs_debug_c('b'))
        trace_copy_rop("mem_default_strip_copy_rop",
                       dev, sdata, sourcex, sraster,
                       id, scolors, textures, tcolors,
                       x, y, width, height, phase_x, phase_y, lop);
#endif
    if (mdproto == 0)
        return_error(gs_error_rangecheck);
    if (sdata == 0) {
        fit_fill(dev, x, y, width, height);
    } else {
        fit_copy(dev, sdata, sourcex, sraster, id, x, y, width, height);
    }
    /* Compute max_height conservatively. */
    max_height = max_rop_bitmap / (width * rop_depth);
    if (max_height == 0)
        max_height = 1;
    block_height = min(height, max_height);
    expand_s = scolors == 0 && uses_s;
    expand_t = tcolors == 0 && uses_t;
    no_expand_params.options = no_expand_options;
    no_expand_t_params.options = no_expand_t_options;
    if (expand_t) {
        /*
         * We don't want to wrap around more than once in Y when
         * copying the texture to the intermediate buffer.
         */
        if (textures && textures->size.y < block_height)
            block_height = textures->size.y;
    }
    gs_make_mem_device(&mdev, mdproto, mem, -1, NULL);
    gx_device_retain((gx_device *)&mdev, true); /* prevent freeing */
    mdev.width = width;
    mdev.height = block_height;
    mdev.color_info.num_components = rop_depth >> 3;
    if (gdev_mem_data_size(&mdev, width, block_height, &size_from_mem_device) >= 0 &&
        size_from_mem_device <= sizeof(mdev_storage)) {
        /* Use the locally allocated storage. */
        mdev.base = (byte *)mdev_storage;
        if ((code = gdev_mem_bits_size(&mdev, mdev.width, mdev.height, &size_from_mem_device)) < 0)
            return code;
        mdev.line_ptrs = (byte **) (mdev.base + size_from_mem_device);
    } else {
        mdev.bitmap_memory = mem;
    }
    code = (*dev_proc(&mdev, open_device))((gx_device *)&mdev);
    if (code < 0)
        return code;
    ALLOC_BUF(row, dest_buffer, row_raster, "copy_rop row");
    /* We may need intermediate buffers for all 3 operands. */
    if (expand_s) {
        source_row_raster = bitmap_raster(width * rop_depth);
        ALLOC_BUF(source_row, source_buffer, source_row_raster,
                  "copy_rop source_row");
    }
    if (scolors && uses_s) {
        unpack_colors_to_standard(dev, source_colors, scolors, rop_depth);
        real_scolors = source_colors;
    }
    if (expand_t && textures) {
        texture_row_raster = bitmap_raster(textures->rep_width * rop_depth);
        ALLOC_BUF(texture_row, texture_buffer, texture_row_raster,
                  "copy_rop texture_row");
        rop_texture = *textures;
        rop_texture.data = texture_row;
        rop_texture.raster = texture_row_raster;
        rop_texture.size.x = rop_texture.rep_width;
        rop_texture.id = gs_no_bitmap_id;
        real_texture = &rop_texture;
        if (rop_texture.size.y > rop_texture.rep_height)
            rop_texture.size.y = rop_texture.rep_height;   /* we only allocated one row_raster, no reps */
    }
    if (tcolors && uses_t) {
        unpack_colors_to_standard(dev, texture_colors, tcolors, rop_depth);
        real_tcolors = texture_colors;
    }
    expand_params.options = expand_options;
    expand_params.x_offset = 0;
    rect.p.x = x;
    rect.q.x = x + width;
    for (py = y; py < y + height; py += loop_height) {
        int sx = sourcex;
        const byte *source_data = sdata + (py - y) * sraster;
        uint source_raster = sraster;

        if (block_height > y + height - py)
            block_height = y + height - py;
        rect.p.y = py;
        if (expand_t) {
            int rep_y = (phase_y + py) % rop_texture.rep_height;

            loop_height = min(block_height, rop_texture.size.y - rep_y);
            rect.q.y = py + loop_height;
            expand_params.data[0] = texture_row;
            gx_get_bits_copy(dev, 0, textures->rep_width, loop_height,
                             &expand_params, &no_expand_t_params,
                             textures->data + rep_y * textures->raster,
                             textures->raster);
            /*
             * Compensate for the addition of rep_y * raster
             * in the subsidiary strip_copy_rop call.
             */
            rop_texture.data = texture_row - rep_y * rop_texture.raster;
        } else {
            loop_height = block_height;
            rect.q.y = py + block_height;
        }
        if (uses_d) {
            bit_params.options = expand_options;
            bit_params.data[0] = scan_line_base(&mdev, 0);
            bit_params.x_offset = 0;
            bit_params.raster = mdev.raster;
            code = (*dev_proc(dev, get_bits_rectangle))
                (dev, &rect, &bit_params);
            if (code < 0)
                break;
        }
        /* Convert the source and texture to standard format. */
        if (expand_s) {
            expand_params.data[0] = source_row;
            gx_get_bits_copy(dev, sx, width, loop_height, &expand_params,
                             &no_expand_params, source_data, sraster);
            sx = 0;
            source_data = source_row;
            source_raster = source_row_raster;
        }
        code = (*dev_proc(&mdev, strip_copy_rop2))
            ((gx_device *)&mdev, source_data, sx, source_raster,
             gx_no_bitmap_id, real_scolors, real_texture, real_tcolors,
             0, 0, width, loop_height, phase_x + x, phase_y + py, lop, 0);
        if (code < 0)
            break;
        /* Convert the result back to the device's format. */
        {
            int i;
            const byte *unpacked = scan_line_base(&mdev, 0);

            for (i = 0; i < loop_height; unpacked += mdev.raster, ++i) {
                pack(tdev, py + i, x, unpacked, width, depth, rop_depth);
            }
        }
    }
out:
    if (texture_row != 0 && texture_row != (byte *)texture_buffer)
        gs_free_object(mem, texture_row, "copy_rop texture_row");
    if (source_row != 0 && source_row != (byte *)source_buffer)
        gs_free_object(mem, source_row, "copy_rop source_row");
    if (row != 0 && row != (byte *)dest_buffer)
        gs_free_object(mem, row, "copy_rop row");
    (*dev_proc(&mdev, close_device)) ((gx_device *) & mdev);
    return code;
}

int
mem_default_strip_copy_rop2(gx_device * dev,
                            const byte * sdata, int sourcex,
                            uint sraster, gx_bitmap_id id,
                            const gx_color_index * scolors,
                            const gx_strip_bitmap * textures,
                            const gx_color_index * tcolors,
                            int x, int y, int width, int height,
                            int phase_x, int phase_y,
                            gs_logical_operation_t lop,
                            uint planar_height)
{
    if (planar_height != 0)
    {
        dmlprintf(dev->memory, "mem_default_strip_copy_rop2 should never be called!\n");
        return_error(gs_error_Fatal);
    }
    return do_strip_copy_rop(dev, sdata, sourcex, sraster, id, scolors,
                             textures, tcolors, x, y, width, height,
                             phase_x, phase_y, lop);
}

/* ---------------- Internal routines ---------------- */

typedef enum {
    transform_pixel_region_portrait,
    transform_pixel_region_landscape,
    transform_pixel_region_skew
} transform_pixel_region_posture;

typedef struct mem_transform_pixel_region_state_s mem_transform_pixel_region_state_t;

typedef int (mem_transform_pixel_region_render_fn)(gx_device *dev, mem_transform_pixel_region_state_t *state, const unsigned char **buffer, int data_x, gx_cmapper_t *cmapper, const gs_gstate *pgs);

struct mem_transform_pixel_region_state_s
{
    gs_memory_t *mem;
    gx_dda_fixed_point pixels;
    gx_dda_fixed_point rows;
    gs_int_rect clip;
    int w;
    int h;
    int spp;
    transform_pixel_region_posture posture;
    mem_transform_pixel_region_render_fn *render;
    void *passthru;
#ifdef WITH_CAL
    cal_context *cal_ctx;
    cal_doubler *cal_dbl;
#endif
};

static void
get_portrait_y_extent(mem_transform_pixel_region_state_t *state, int *iy, int *ih)
{
    fixed y0, y1;
    gx_dda_fixed row = state->rows.y;

    y0 = dda_current(row);
    dda_next(row);
    y1 = dda_current(row);

    if (y1 < y0) {
        fixed t = y1; y1 = y0; y0 = t;
    }

    *iy = fixed2int_pixround_perfect(y0);
    *ih = fixed2int_pixround_perfect(y1) - *iy;
}

static void
get_landscape_x_extent(mem_transform_pixel_region_state_t *state, int *ix, int *iw)
{
    fixed x0, x1;
    gx_dda_fixed row = state->rows.x;

    x0 = dda_current(row);
    dda_next(row);
    x1 = dda_current(row);

    if (x1 < x0) {
        fixed t = x1; x1 = x0; x0 = t;
    }

    *ix = fixed2int_pixround_perfect(x0);
    *iw = fixed2int_pixround_perfect(x1) - *ix;
}

static inline int
template_mem_transform_pixel_region_render_portrait(gx_device *dev, mem_transform_pixel_region_state_t *state, const unsigned char **buffer, int data_x, gx_cmapper_t *cmapper, const gs_gstate *pgs, int spp)
{
    gx_device_memory *mdev = (gx_device_memory *)dev;
    gx_dda_fixed_point pnext;
    int vci, vdi;
    int irun;			/* int x/rrun */
    int w = state->w;
    int h = state->h;
    const byte *data = buffer[0] + data_x * spp;
    const byte *bufend = NULL;
    const byte *run;
    int k;
    gx_color_value *conc = &cmapper->conc[0];
    gx_cmapper_fn *mapper = cmapper->set_color;
    byte *out;
    byte *out_row;
    int minx, maxx;

    if (h == 0)
        return 0;

    /* Clip on y */
    get_portrait_y_extent(state, &vci, &vdi);
    if (vci < state->clip.p.y)
        vdi += vci - state->clip.p.y, vci = state->clip.p.y;
    if (vci+vdi > state->clip.q.y)
        vdi = state->clip.q.y - vci;
    if (vdi <= 0)
        return 0;

    pnext = state->pixels;
    dda_translate(pnext.x,  (-fixed_epsilon));
    irun = fixed2int_var_rounded(dda_current(pnext.x));
    if_debug5m('b', dev->memory, "[b]y=%d data_x=%d w=%d xt=%f yt=%f\n",
               vci, data_x, w, fixed2float(dda_current(pnext.x)), fixed2float(dda_current(pnext.y)));

    minx = state->clip.p.x;
    maxx = state->clip.q.x;
    out_row = mdev->base + mdev->raster * vci;
    bufend = data + w * spp;
    while (data < bufend) {
        /* Find the length of the next run. It will either end when we hit
         * the end of the source data, or when the pixel data differs. */
        run = data + spp;
        while (1) {
            dda_next(pnext.x);
            if (run >= bufend)
                break;
            if (memcmp(run, data, spp))
                break;
            run += spp;
        }
        /* So we have a run of pixels from data to run that are all the same. */
        /* This needs to be sped up */
        for (k = 0; k < spp; k++) {
            conc[k] = gx_color_value_from_byte(data[k]);
        }
        mapper(cmapper);
        /* Fill the region between irun and fixed2int_var_rounded(pnext.x) */
        {
            int xi = irun;
            int wi = (irun = fixed2int_var_rounded(dda_current(pnext.x))) - xi;

            if (wi < 0)
                xi += wi, wi = -wi;

            if (xi < minx)
                wi += xi - minx, xi = minx;
            if (xi+wi > maxx)
                wi = maxx - xi;
            if (wi > 0) {
                /* assert(color_is_pure(&cmapper->devc)); */
                out = out_row;
                for (h = vdi; h > 0; h--, out += mdev->raster) {
                    gx_color_index color = cmapper->devc.colors.pure;
                    int xii = xi * spp;
                    int wii = wi;
                    do {
                        /* Excuse the double shifts below, that's to stop the
                         * C compiler complaining if the color index type is
                         * 32 bits. */
                        switch(spp)
                        {
                        case 8: out[xii++] = ((color>>28)>>28) & 0xff;
                        case 7: out[xii++] = ((color>>24)>>24) & 0xff;
                        case 6: out[xii++] = ((color>>24)>>16) & 0xff;
                        case 5: out[xii++] = ((color>>24)>>8) & 0xff;
                        case 4: out[xii++] = (color>>24) & 0xff;
                        case 3: out[xii++] = (color>>16) & 0xff;
                        case 2: out[xii++] = (color>>8) & 0xff;
                        case 1: out[xii++] = color & 0xff;
                        }
                    } while (--wii != 0);
                }
            }
        }
        data = run;
    }
    return 0;
}

static int
mem_transform_pixel_region_render_portrait_1(gx_device *dev, mem_transform_pixel_region_state_t *state, const unsigned char **buffer, int data_x, gx_cmapper_t *cmapper, const gs_gstate *pgs)
{
    return template_mem_transform_pixel_region_render_portrait(dev, state, buffer, data_x, cmapper, pgs, 1);
}

static int
mem_transform_pixel_region_render_portrait_3(gx_device *dev, mem_transform_pixel_region_state_t *state, const unsigned char **buffer, int data_x, gx_cmapper_t *cmapper, const gs_gstate *pgs)
{
    return template_mem_transform_pixel_region_render_portrait(dev, state, buffer, data_x, cmapper, pgs, 3);
}

static int
mem_transform_pixel_region_render_portrait_4(gx_device *dev, mem_transform_pixel_region_state_t *state, const unsigned char **buffer, int data_x, gx_cmapper_t *cmapper, const gs_gstate *pgs)
{
    return template_mem_transform_pixel_region_render_portrait(dev, state, buffer, data_x, cmapper, pgs, 4);
}

static int
mem_transform_pixel_region_render_portrait_n(gx_device *dev, mem_transform_pixel_region_state_t *state, const unsigned char **buffer, int data_x, gx_cmapper_t *cmapper, const gs_gstate *pgs)
{
    return template_mem_transform_pixel_region_render_portrait(dev, state, buffer, data_x, cmapper, pgs, state->spp);
}

static inline int
template_mem_transform_pixel_region_render_portrait_planar(gx_device *dev, mem_transform_pixel_region_state_t *state, const unsigned char **buffer, int data_x, gx_cmapper_t *cmapper, const gs_gstate *pgs, int spp)
{
    gx_device_memory *mdev = (gx_device_memory *)dev;
    gx_dda_fixed_point pnext;
    int vci, vdi;
    int irun;			/* int x/rrun */
    int w = state->w;
    int h = state->h;
    const byte *data = buffer[0] + data_x * spp;
    const byte *bufend = NULL;
    const byte *run;
    int k;
    gx_color_value *conc = &cmapper->conc[0];
    gx_cmapper_fn *mapper = cmapper->set_color;
    int minx, maxx;

    if (h == 0)
        return 0;

    /* Clip on y */
    get_portrait_y_extent(state, &vci, &vdi);
    if (vci < state->clip.p.y)
        vdi += vci - state->clip.p.y, vci = state->clip.p.y;
    if (vci+vdi > state->clip.q.y)
        vdi = state->clip.q.y - vci;
    if (vdi <= 0)
        return 0;

    pnext = state->pixels;
    dda_translate(pnext.x,  (-fixed_epsilon));
    irun = fixed2int_var_rounded(dda_current(pnext.x));
    if_debug5m('b', dev->memory, "[b]y=%d data_x=%d w=%d xt=%f yt=%f\n",
               vci, data_x, w, fixed2float(dda_current(pnext.x)), fixed2float(dda_current(pnext.y)));

    minx = state->clip.p.x;
    maxx = state->clip.q.x;
    bufend = data + w * spp;
    while (data < bufend) {
        /* Find the length of the next run. It will either end when we hit
         * the end of the source data, or when the pixel data differs. */
        run = data + spp;
        while (1) {
            dda_next(pnext.x);
            if (run >= bufend)
                break;
            if (memcmp(run, data, spp))
                break;
            run += spp;
        }
        /* So we have a run of pixels from data to run that are all the same. */
        /* This needs to be sped up */
        for (k = 0; k < spp; k++) {
            conc[k] = gx_color_value_from_byte(data[k]);
        }
        mapper(cmapper);
        /* Fill the region between irun and fixed2int_var_rounded(pnext.x) */
        {
            int xi = irun;
            int wi = (irun = fixed2int_var_rounded(dda_current(pnext.x))) - xi;

            if (wi < 0)
                xi += wi, wi = -wi;

            if (xi < minx)
                wi += xi - minx, xi = minx;
            if (xi+wi > maxx)
                wi = maxx - xi;
            if (wi > 0) {
                /* assert(color_is_pure(&cmapper->devc)); */
                gx_color_index color = cmapper->devc.colors.pure;
                for (k = 0; k < spp; k++) {
                    unsigned char c = (color>>mdev->planes[k].shift) & ((1<<mdev->planes[k].depth)-1);
                    for (h = 0; h < vdi; h++) {
                        byte *out = mdev->line_ptrs[vci + h + k*mdev->height] + xi;
                        memset(out, c, wi);
                    }
                }
            }
        }
        data = run;
    }
    return 0;
}

static int
mem_transform_pixel_region_render_portrait_1p(gx_device *dev, mem_transform_pixel_region_state_t *state, const unsigned char **buffer, int data_x, gx_cmapper_t *cmapper, const gs_gstate *pgs)
{
    return template_mem_transform_pixel_region_render_portrait_planar(dev, state, buffer, data_x, cmapper, pgs, 1);
}

static int
mem_transform_pixel_region_render_portrait_3p(gx_device *dev, mem_transform_pixel_region_state_t *state, const unsigned char **buffer, int data_x, gx_cmapper_t *cmapper, const gs_gstate *pgs)
{
    return template_mem_transform_pixel_region_render_portrait_planar(dev, state, buffer, data_x, cmapper, pgs, 3);
}

static int
mem_transform_pixel_region_render_portrait_4p(gx_device *dev, mem_transform_pixel_region_state_t *state, const unsigned char **buffer, int data_x, gx_cmapper_t *cmapper, const gs_gstate *pgs)
{
    return template_mem_transform_pixel_region_render_portrait_planar(dev, state, buffer, data_x, cmapper, pgs, 4);
}

static int
mem_transform_pixel_region_render_portrait_np(gx_device *dev, mem_transform_pixel_region_state_t *state, const unsigned char **buffer, int data_x, gx_cmapper_t *cmapper, const gs_gstate *pgs)
{
    return template_mem_transform_pixel_region_render_portrait_planar(dev, state, buffer, data_x, cmapper, pgs, state->spp);
}

static int
mem_transform_pixel_region_render_portrait_planar(gx_device *dev, mem_transform_pixel_region_state_t *state, const unsigned char **buffer, int data_x, gx_cmapper_t *cmapper, const gs_gstate *pgs)
{
    switch(state->spp) {
    case 1:
        return mem_transform_pixel_region_render_portrait_1p(dev, state, buffer, data_x, cmapper, pgs);
    case 3:
        return mem_transform_pixel_region_render_portrait_3p(dev, state, buffer, data_x, cmapper, pgs);
    case 4:
        return mem_transform_pixel_region_render_portrait_4p(dev, state, buffer, data_x, cmapper, pgs);
    default:
        return mem_transform_pixel_region_render_portrait_np(dev, state, buffer, data_x, cmapper, pgs);
    }
}

static int
mem_transform_pixel_region_render_portrait(gx_device *dev, mem_transform_pixel_region_state_t *state, const unsigned char **buffer, int data_x, gx_cmapper_t *cmapper, const gs_gstate *pgs)
{
    switch(state->spp) {
    case 1:
        return mem_transform_pixel_region_render_portrait_1(dev, state, buffer, data_x, cmapper, pgs);
    case 3:
        return mem_transform_pixel_region_render_portrait_3(dev, state, buffer, data_x, cmapper, pgs);
    case 4:
        return mem_transform_pixel_region_render_portrait_4(dev, state, buffer, data_x, cmapper, pgs);
    default:
        return mem_transform_pixel_region_render_portrait_n(dev, state, buffer, data_x, cmapper, pgs);
    }
}

static inline int
template_mem_transform_pixel_region_render_portrait_1to1(gx_device *dev, mem_transform_pixel_region_state_t *state, const unsigned char **buffer, int data_x, gx_cmapper_t *cmapper, const gs_gstate *pgs, int spp)
{
    gx_device_memory *mdev = (gx_device_memory *)dev;
    gx_dda_fixed_point pnext;
    int vci, vdi;
    int w = state->w;
    int h = state->h;
    int left, right, oleft;

    if (h == 0)
        return 0;

    /* Clip on y */
    get_portrait_y_extent(state, &vci, &vdi);
    if (vci < state->clip.p.y)
        vdi += vci - state->clip.p.y, vci = state->clip.p.y;
    if (vci+vdi > state->clip.q.y)
        vdi = state->clip.q.y - vci;
    if (vdi <= 0)
        return 0;

    pnext = state->pixels;
    dda_translate(pnext.x,  (-fixed_epsilon));
    left = fixed2int_var_rounded(dda_current(pnext.x));
    if_debug5m('b', dev->memory, "[b]y=%d data_x=%d w=%d xt=%f yt=%f\n",
               vci, data_x, w, fixed2float(dda_current(pnext.x)), fixed2float(dda_current(pnext.y)));
    right = fixed2int_var_rounded(dda_current(pnext.x)) + w;

    if (left > right) {
        int tmp = left; left = right; right = tmp;
    }
    oleft = left;
    if (left < state->clip.p.x)
        left = state->clip.p.x;
    if (right > state->clip.q.x)
        right = state->clip.q.x;
    if (left < right) {
        byte *out = mdev->base + mdev->raster * vci + left * spp;
        const byte *data = buffer[0] + (data_x + left - oleft) * spp;
        right = (right-left)*spp;
        do {
            memcpy(out, data, right);
            out += mdev->raster;
        } while (--vdi);
    }

    return 0;
}

static int
mem_transform_pixel_region_render_portrait_1to1_1(gx_device *dev, mem_transform_pixel_region_state_t *state, const unsigned char **buffer, int data_x, gx_cmapper_t *cmapper, const gs_gstate *pgs)
{
    return template_mem_transform_pixel_region_render_portrait_1to1(dev, state, buffer, data_x, cmapper, pgs, 1);
}

static int
mem_transform_pixel_region_render_portrait_1to1_3(gx_device *dev, mem_transform_pixel_region_state_t *state, const unsigned char **buffer, int data_x, gx_cmapper_t *cmapper, const gs_gstate *pgs)
{
    return template_mem_transform_pixel_region_render_portrait_1to1(dev, state, buffer, data_x, cmapper, pgs, 3);
}

static int
mem_transform_pixel_region_render_portrait_1to1_4(gx_device *dev, mem_transform_pixel_region_state_t *state, const unsigned char **buffer, int data_x, gx_cmapper_t *cmapper, const gs_gstate *pgs)
{
    return template_mem_transform_pixel_region_render_portrait_1to1(dev, state, buffer, data_x, cmapper, pgs, 4);
}

static int
mem_transform_pixel_region_render_portrait_1to1_n(gx_device *dev, mem_transform_pixel_region_state_t *state, const unsigned char **buffer, int data_x, gx_cmapper_t *cmapper, const gs_gstate *pgs)
{
    return template_mem_transform_pixel_region_render_portrait_1to1(dev, state, buffer, data_x, cmapper, pgs, state->spp);
}

static int
mem_transform_pixel_region_render_portrait_1to1(gx_device *dev, mem_transform_pixel_region_state_t *state, const unsigned char **buffer, int data_x, gx_cmapper_t *cmapper, const gs_gstate *pgs)
{
    if (!cmapper->direct)
        return mem_transform_pixel_region_render_portrait(dev, state, buffer, data_x, cmapper, pgs);
    switch(state->spp) {
    case 1:
        return mem_transform_pixel_region_render_portrait_1to1_1(dev, state, buffer, data_x, cmapper, pgs);
    case 3:
        return mem_transform_pixel_region_render_portrait_1to1_3(dev, state, buffer, data_x, cmapper, pgs);
    case 4:
        return mem_transform_pixel_region_render_portrait_1to1_4(dev, state, buffer, data_x, cmapper, pgs);
    default:
        return mem_transform_pixel_region_render_portrait_1to1_n(dev, state, buffer, data_x, cmapper, pgs);
    }
}

#ifdef WITH_CAL
static inline int
template_mem_transform_pixel_region_render_portrait_1to2(gx_device *dev, mem_transform_pixel_region_state_t *state, const unsigned char **buffer, int data_x, gx_cmapper_t *cmapper, const gs_gstate *pgs, int spp)
{
    gx_device_memory *mdev = (gx_device_memory *)dev;
    gx_dda_fixed_point pnext;
    int vci, vdi;
    int w = state->w;
    int h = state->h;
    int oleft, left, right;

    if (h == 0)
        return 0;

    /* Clip on y */
    get_portrait_y_extent(state, &vci, &vdi);
    if (vci < state->clip.p.y)
        vdi += vci - state->clip.p.y, vci = state->clip.p.y;
    if (vci+vdi > state->clip.q.y)
        vdi = state->clip.q.y - vci;
    if (vdi <= 0)
        return 0;

    pnext = state->pixels;
    dda_translate(pnext.x,  (-fixed_epsilon));
    left = fixed2int_var_rounded(dda_current(pnext.x));
    if_debug5m('b', dev->memory, "[b]y=%d data_x=%d w=%d xt=%f yt=%f\n",
               vci, data_x, w, fixed2float(dda_current(pnext.x)), fixed2float(dda_current(pnext.y)));
    right = fixed2int_var_rounded(dda_current(pnext.x)) + w * 2;

    if (left > right) {
        int tmp = left; left = right; right = tmp;
    }
    oleft = left;
    if (left < state->clip.p.x)
        left = state->clip.p.x;
    if (right > state->clip.q.x)
        right = state->clip.q.x;
    if (left < right) {
        byte *out[2];
        const byte *in = buffer[0] + (data_x + left - oleft) * spp;
        int lines_out;
        out[0] = mdev->base + left * spp + mdev->raster * vci;
        out[1] = out[0] + (vdi > 1 ? mdev->raster : 0);
        lines_out = cal_doubler_process(state->cal_dbl, dev->memory,
                                        &in, &out[0]);
        (void)lines_out;
        /* assert(lines_out == 2) */
    }

    return 0;
}

static int
mem_transform_pixel_region_render_portrait_1to2_1(gx_device *dev, mem_transform_pixel_region_state_t *state, const unsigned char **buffer, int data_x, gx_cmapper_t *cmapper, const gs_gstate *pgs)
{
    return template_mem_transform_pixel_region_render_portrait_1to2(dev, state, buffer, data_x, cmapper, pgs, 1);
}

static int
mem_transform_pixel_region_render_portrait_1to2_3(gx_device *dev, mem_transform_pixel_region_state_t *state, const unsigned char **buffer, int data_x, gx_cmapper_t *cmapper, const gs_gstate *pgs)
{
    return template_mem_transform_pixel_region_render_portrait_1to2(dev, state, buffer, data_x, cmapper, pgs, 3);
}

static int
mem_transform_pixel_region_render_portrait_1to2_4(gx_device *dev, mem_transform_pixel_region_state_t *state, const unsigned char **buffer, int data_x, gx_cmapper_t *cmapper, const gs_gstate *pgs)
{
    return template_mem_transform_pixel_region_render_portrait_1to2(dev, state, buffer, data_x, cmapper, pgs, 4);
}

static int
mem_transform_pixel_region_render_portrait_1to2_n(gx_device *dev, mem_transform_pixel_region_state_t *state, const unsigned char **buffer, int data_x, gx_cmapper_t *cmapper, const gs_gstate *pgs)
{
    return template_mem_transform_pixel_region_render_portrait_1to2(dev, state, buffer, data_x, cmapper, pgs, state->spp);
}

static int
mem_transform_pixel_region_render_portrait_1to2(gx_device *dev, mem_transform_pixel_region_state_t *state, const unsigned char **buffer, int data_x, gx_cmapper_t *cmapper, const gs_gstate *pgs)
{
    if (!cmapper->direct)
        return mem_transform_pixel_region_render_portrait(dev, state, buffer, data_x, cmapper, pgs);
    switch(state->spp) {
    case 1:
        return mem_transform_pixel_region_render_portrait_1to2_1(dev, state, buffer, data_x, cmapper, pgs);
    case 3:
        return mem_transform_pixel_region_render_portrait_1to2_3(dev, state, buffer, data_x, cmapper, pgs);
    case 4:
        return mem_transform_pixel_region_render_portrait_1to2_4(dev, state, buffer, data_x, cmapper, pgs);
    default:
        return mem_transform_pixel_region_render_portrait_1to2_n(dev, state, buffer, data_x, cmapper, pgs);
    }
}

static inline int
template_mem_transform_pixel_region_render_portrait_1to4(gx_device *dev, mem_transform_pixel_region_state_t *state, const unsigned char **buffer, int data_x, gx_cmapper_t *cmapper, const gs_gstate *pgs, int spp)
{
    gx_device_memory *mdev = (gx_device_memory *)dev;
    gx_dda_fixed_point pnext;
    int vci, vdi;
    int w = state->w;
    int h = state->h;
    int oleft, left, right;

    if (h == 0)
        return 0;

    /* Clip on y */
    get_portrait_y_extent(state, &vci, &vdi);
    if (vci < state->clip.p.y)
        vdi += vci - state->clip.p.y, vci = state->clip.p.y;
    if (vci+vdi > state->clip.q.y)
        vdi = state->clip.q.y - vci;
    if (vdi <= 0)
        return 0;

    pnext = state->pixels;
    dda_translate(pnext.x,  (-fixed_epsilon));
    left = fixed2int_var_rounded(dda_current(pnext.x));
    if_debug5m('b', dev->memory, "[b]y=%d data_x=%d w=%d xt=%f yt=%f\n",
               vci, data_x, w, fixed2float(dda_current(pnext.x)), fixed2float(dda_current(pnext.y)));
    right = fixed2int_var_rounded(dda_current(pnext.x)) + w * 4 * spp;

    if (left > right) {
        int tmp = left; left = right; right = tmp;
    }
    oleft = left;
    if (left < state->clip.p.x)
        left = state->clip.p.x;
    if (right > state->clip.q.x)
        right = state->clip.q.x;
    if (left < right) {
        byte *out[4];
        const byte *in = buffer[0] + (data_x + left - oleft) * spp;
        int lines_out;
        out[0] = mdev->base + left * spp + mdev->raster * vci;
        out[1] = out[0] + (vdi > 1 ? mdev->raster : 0);
        out[2] = out[1] + (vdi > 2 ? mdev->raster : 0);
        out[3] = out[2] + (vdi > 3 ? mdev->raster : 0);
        lines_out = cal_doubler_process(state->cal_dbl, dev->memory,
                                        &in, &out[0]);
        (void)lines_out;
        /* assert(lines_out == 4) */
    }

    return 0;
}

static int
mem_transform_pixel_region_render_portrait_1to4_1(gx_device *dev, mem_transform_pixel_region_state_t *state, const unsigned char **buffer, int data_x, gx_cmapper_t *cmapper, const gs_gstate *pgs)
{
    return template_mem_transform_pixel_region_render_portrait_1to4(dev, state, buffer, data_x, cmapper, pgs, 1);
}

static int
mem_transform_pixel_region_render_portrait_1to4_3(gx_device *dev, mem_transform_pixel_region_state_t *state, const unsigned char **buffer, int data_x, gx_cmapper_t *cmapper, const gs_gstate *pgs)
{
    return template_mem_transform_pixel_region_render_portrait_1to4(dev, state, buffer, data_x, cmapper, pgs, 3);
}

static int
mem_transform_pixel_region_render_portrait_1to4_4(gx_device *dev, mem_transform_pixel_region_state_t *state, const unsigned char **buffer, int data_x, gx_cmapper_t *cmapper, const gs_gstate *pgs)
{
    return template_mem_transform_pixel_region_render_portrait_1to4(dev, state, buffer, data_x, cmapper, pgs, 4);
}

static int
mem_transform_pixel_region_render_portrait_1to4_n(gx_device *dev, mem_transform_pixel_region_state_t *state, const unsigned char **buffer, int data_x, gx_cmapper_t *cmapper, const gs_gstate *pgs)
{
    return template_mem_transform_pixel_region_render_portrait_1to4(dev, state, buffer, data_x, cmapper, pgs, state->spp);
}

static int
mem_transform_pixel_region_render_portrait_1to4(gx_device *dev, mem_transform_pixel_region_state_t *state, const unsigned char **buffer, int data_x, gx_cmapper_t *cmapper, const gs_gstate *pgs)
{
    if (!cmapper->direct)
        return mem_transform_pixel_region_render_portrait(dev, state, buffer, data_x, cmapper, pgs);
    switch(state->spp) {
    case 1:
        return mem_transform_pixel_region_render_portrait_1to4_1(dev, state, buffer, data_x, cmapper, pgs);
    case 3:
        return mem_transform_pixel_region_render_portrait_1to4_3(dev, state, buffer, data_x, cmapper, pgs);
    case 4:
        return mem_transform_pixel_region_render_portrait_1to4_4(dev, state, buffer, data_x, cmapper, pgs);
    default:
        return mem_transform_pixel_region_render_portrait_1to4_n(dev, state, buffer, data_x, cmapper, pgs);
    }
}

static inline int
template_mem_transform_pixel_region_render_portrait_1to8(gx_device *dev, mem_transform_pixel_region_state_t *state, const unsigned char **buffer, int data_x, gx_cmapper_t *cmapper, const gs_gstate *pgs, int spp)
{
    gx_device_memory *mdev = (gx_device_memory *)dev;
    gx_dda_fixed_point pnext;
    int vci, vdi;
    int w = state->w;
    int h = state->h;
    int oleft, left, right;

    if (h == 0)
        return 0;

    /* Clip on y */
    get_portrait_y_extent(state, &vci, &vdi);
    if (vci < state->clip.p.y)
        vdi += vci - state->clip.p.y, vci = state->clip.p.y;
    if (vci+vdi > state->clip.q.y)
        vdi = state->clip.q.y - vci;
    if (vdi <= 0)
        return 0;

    pnext = state->pixels;
    dda_translate(pnext.x,  (-fixed_epsilon));
    left = fixed2int_var_rounded(dda_current(pnext.x));
    if_debug5m('b', dev->memory, "[b]y=%d data_x=%d w=%d xt=%f yt=%f\n",
               vci, data_x, w, fixed2float(dda_current(pnext.x)), fixed2float(dda_current(pnext.y)));
    right = fixed2int_var_rounded(dda_current(pnext.x)) + w * 8;

    if (left > right) {
        int tmp = left; left = right; right = tmp;
    }
    oleft = left;
    if (left < state->clip.p.x)
        left = state->clip.p.x;
    if (right > state->clip.q.x)
        right = state->clip.q.x;
    if (left < right) {
        byte *out[8];
        const byte *in = buffer[0] + (data_x + left - oleft) * spp;
        int lines_out;
        out[0] = mdev->base + left * spp + mdev->raster * vci;
        out[1] = out[0] + (vdi > 1 ? mdev->raster : 0);
        out[2] = out[1] + (vdi > 2 ? mdev->raster : 0);
        out[3] = out[2] + (vdi > 3 ? mdev->raster : 0);
        out[4] = out[3] + (vdi > 4 ? mdev->raster : 0);
        out[5] = out[4] + (vdi > 5 ? mdev->raster : 0);
        out[6] = out[5] + (vdi > 6 ? mdev->raster : 0);
        out[7] = out[6] + (vdi > 7 ? mdev->raster : 0);
        lines_out = cal_doubler_process(state->cal_dbl, dev->memory,
                                        &in, &out[0]);
        (void)lines_out;
        /* assert(lines_out == 8) */
    }

    return 0;
}

static int
mem_transform_pixel_region_render_portrait_1to8_1(gx_device *dev, mem_transform_pixel_region_state_t *state, const unsigned char **buffer, int data_x, gx_cmapper_t *cmapper, const gs_gstate *pgs)
{
    return template_mem_transform_pixel_region_render_portrait_1to8(dev, state, buffer, data_x, cmapper, pgs, 1);
}

static int
mem_transform_pixel_region_render_portrait_1to8_3(gx_device *dev, mem_transform_pixel_region_state_t *state, const unsigned char **buffer, int data_x, gx_cmapper_t *cmapper, const gs_gstate *pgs)
{
    return template_mem_transform_pixel_region_render_portrait_1to8(dev, state, buffer, data_x, cmapper, pgs, 3);
}

static int
mem_transform_pixel_region_render_portrait_1to8_4(gx_device *dev, mem_transform_pixel_region_state_t *state, const unsigned char **buffer, int data_x, gx_cmapper_t *cmapper, const gs_gstate *pgs)
{
    return template_mem_transform_pixel_region_render_portrait_1to8(dev, state, buffer, data_x, cmapper, pgs, 4);
}

static int
mem_transform_pixel_region_render_portrait_1to8_n(gx_device *dev, mem_transform_pixel_region_state_t *state, const unsigned char **buffer, int data_x, gx_cmapper_t *cmapper, const gs_gstate *pgs)
{
    return template_mem_transform_pixel_region_render_portrait_1to8(dev, state, buffer, data_x, cmapper, pgs, state->spp);
}

static int
mem_transform_pixel_region_render_portrait_1to8(gx_device *dev, mem_transform_pixel_region_state_t *state, const unsigned char **buffer, int data_x, gx_cmapper_t *cmapper, const gs_gstate *pgs)
{
    if (!cmapper->direct)
        return mem_transform_pixel_region_render_portrait(dev, state, buffer, data_x, cmapper, pgs);
    switch(state->spp) {
    case 1:
        return mem_transform_pixel_region_render_portrait_1to8_1(dev, state, buffer, data_x, cmapper, pgs);
    case 3:
        return mem_transform_pixel_region_render_portrait_1to8_3(dev, state, buffer, data_x, cmapper, pgs);
    case 4:
        return mem_transform_pixel_region_render_portrait_1to8_4(dev, state, buffer, data_x, cmapper, pgs);
    default:
        return mem_transform_pixel_region_render_portrait_1to8_n(dev, state, buffer, data_x, cmapper, pgs);
    }
}
#endif

static inline int
template_mem_transform_pixel_region_render_landscape(gx_device *dev, mem_transform_pixel_region_state_t *state, const unsigned char **buffer, int data_x, gx_cmapper_t *cmapper, const gs_gstate *pgs, int spp)
{
    gx_device_memory *mdev = (gx_device_memory *)dev;
    gx_dda_fixed_point pnext;
    int vci, vdi;
    int irun;			/* int x/rrun */
    int w = state->w;
    int h = state->h;
    const byte *data = buffer[0] + data_x * spp;
    const byte *bufend = NULL;
    const byte *run;
    int k;
    gx_color_value *conc = &cmapper->conc[0];
    gx_cmapper_fn *mapper = cmapper->set_color;
    byte *out;
    byte *out_row;
    int miny, maxy;

    if (h == 0)
        return 0;

    /* Clip on x */
    get_landscape_x_extent(state, &vci, &vdi);
    if (vci < state->clip.p.x)
        vdi += vci - state->clip.p.x, vci = state->clip.p.x;
    if (vci+vdi > state->clip.q.x)
        vdi = state->clip.q.x - vci;
    if (vdi <= 0)
        return 0;

    pnext = state->pixels;
    dda_translate(pnext.x,  (-fixed_epsilon));
    irun = fixed2int_var_rounded(dda_current(pnext.y));
    if_debug5m('b', dev->memory, "[b]y=%d data_x=%d w=%d xt=%f yt=%f\n",
               vci, data_x, w, fixed2float(dda_current(pnext.x)), fixed2float(dda_current(pnext.y)));

    miny = state->clip.p.y;
    maxy = state->clip.q.y;
    out_row = mdev->base + vci * spp;
    bufend = data + w * spp;
    while (data < bufend) {
        /* Find the length of the next run. It will either end when we hit
         * the end of the source data, or when the pixel data differs. */
        run = data + spp;
        while (1) {
            dda_next(pnext.y);
            if (run >= bufend)
                break;
            if (memcmp(run, data, spp))
                break;
            run += spp;
        }
        /* So we have a run of pixels from data to run that are all the same. */
        /* This needs to be sped up */
        for (k = 0; k < spp; k++) {
            conc[k] = gx_color_value_from_byte(data[k]);
        }
        mapper(cmapper);
        /* Fill the region between irun and fixed2int_var_rounded(pnext.y) */
        {              /* 90 degree rotated rectangle */
            int yi = irun;
            int hi = (irun = fixed2int_var_rounded(dda_current(pnext.y))) - yi;

            if (hi < 0)
                yi += hi, hi = -hi;

            if (yi < miny)
                hi += yi - miny, yi = miny;
            if (yi+hi > maxy)
                hi = maxy - yi;
            if (hi > 0) {
                /* assert(color_is_pure(&cmapper->devc)); */
                out = out_row + mdev->raster * yi;
                for (h = hi; h > 0; h--, out += mdev->raster) {
                    gx_color_index color = cmapper->devc.colors.pure;
                    int xii = 0;
                    int wii = vdi;
                    do {
                        /* Excuse the double shifts below, that's to stop the
                         * C compiler complaining if the color index type is
                         * 32 bits. */
                        switch(spp)
                        {
                        case 8: out[xii++] = ((color>>28)>>28) & 0xff;
                        case 7: out[xii++] = ((color>>24)>>24) & 0xff;
                        case 6: out[xii++] = ((color>>24)>>16) & 0xff;
                        case 5: out[xii++] = ((color>>24)>>8) & 0xff;
                        case 4: out[xii++] = (color>>24) & 0xff;
                        case 3: out[xii++] = (color>>16) & 0xff;
                        case 2: out[xii++] = (color>>8) & 0xff;
                        case 1: out[xii++] = color & 0xff;
                        }
                    } while (--wii != 0);
                }
            }
        }
        data = run;
    }
    return 1;
}

static int
mem_transform_pixel_region_render_landscape_1(gx_device *dev, mem_transform_pixel_region_state_t *state, const unsigned char **buffer, int data_x, gx_cmapper_t *cmapper, const gs_gstate *pgs)
{
    return template_mem_transform_pixel_region_render_landscape(dev, state, buffer, data_x, cmapper, pgs, 1);
}

static int
mem_transform_pixel_region_render_landscape_3(gx_device *dev, mem_transform_pixel_region_state_t *state, const unsigned char **buffer, int data_x, gx_cmapper_t *cmapper, const gs_gstate *pgs)
{
    return template_mem_transform_pixel_region_render_landscape(dev, state, buffer, data_x, cmapper, pgs, 3);
}

static int
mem_transform_pixel_region_render_landscape_4(gx_device *dev, mem_transform_pixel_region_state_t *state, const unsigned char **buffer, int data_x, gx_cmapper_t *cmapper, const gs_gstate *pgs)
{
    return template_mem_transform_pixel_region_render_landscape(dev, state, buffer, data_x, cmapper, pgs, 4);
}

static int
mem_transform_pixel_region_render_landscape_n(gx_device *dev, mem_transform_pixel_region_state_t *state, const unsigned char **buffer, int data_x, gx_cmapper_t *cmapper, const gs_gstate *pgs)
{
    return template_mem_transform_pixel_region_render_landscape(dev, state, buffer, data_x, cmapper, pgs, state->spp);
}

static int
mem_transform_pixel_region_render_landscape(gx_device *dev, mem_transform_pixel_region_state_t *state, const unsigned char **buffer, int data_x, gx_cmapper_t *cmapper, const gs_gstate *pgs)
{
    switch (state->spp) {
    case 1:
        return mem_transform_pixel_region_render_landscape_1(dev, state, buffer, data_x, cmapper, pgs);
    case 3:
        return mem_transform_pixel_region_render_landscape_3(dev, state, buffer, data_x, cmapper, pgs);
    case 4:
        return mem_transform_pixel_region_render_landscape_4(dev, state, buffer, data_x, cmapper, pgs);
    default:
        return mem_transform_pixel_region_render_landscape_n(dev, state, buffer, data_x, cmapper, pgs);
    }
}

static inline int
template_mem_transform_pixel_region_render_landscape_planar(gx_device *dev, mem_transform_pixel_region_state_t *state, const unsigned char **buffer, int data_x, gx_cmapper_t *cmapper, const gs_gstate *pgs, int spp)
{
    gx_device_memory *mdev = (gx_device_memory *)dev;
    gx_dda_fixed_point pnext;
    int vci, vdi;
    int irun;			/* int x/rrun */
    int w = state->w;
    int h = state->h;
    const byte *data = buffer[0] + data_x * spp;
    const byte *bufend = NULL;
    const byte *run;
    int k;
    gx_color_value *conc = &cmapper->conc[0];
    gx_cmapper_fn *mapper = cmapper->set_color;
    byte *out;
    int miny, maxy;

    if (h == 0)
        return 0;

    /* Clip on x */
    get_landscape_x_extent(state, &vci, &vdi);
    if (vci < state->clip.p.x)
        vdi += vci - state->clip.p.x, vci = state->clip.p.x;
    if (vci+vdi > state->clip.q.x)
        vdi = state->clip.q.x - vci;
    if (vdi <= 0)
        return 0;

    pnext = state->pixels;
    dda_translate(pnext.x,  (-fixed_epsilon));
    irun = fixed2int_var_rounded(dda_current(pnext.y));
    if_debug5m('b', dev->memory, "[b]y=%d data_x=%d w=%d xt=%f yt=%f\n",
               vci, data_x, w, fixed2float(dda_current(pnext.x)), fixed2float(dda_current(pnext.y)));

    miny = state->clip.p.y;
    maxy = state->clip.q.y;
    bufend = data + w * spp;
    while (data < bufend) {
        /* Find the length of the next run. It will either end when we hit
         * the end of the source data, or when the pixel data differs. */
        run = data + spp;
        while (1) {
            dda_next(pnext.y);
            if (run >= bufend)
                break;
            if (memcmp(run, data, spp))
                break;
            run += spp;
        }
        /* So we have a run of pixels from data to run that are all the same. */
        /* This needs to be sped up */
        for (k = 0; k < spp; k++) {
            conc[k] = gx_color_value_from_byte(data[k]);
        }
        mapper(cmapper);
        /* Fill the region between irun and fixed2int_var_rounded(pnext.y) */
        {              /* 90 degree rotated rectangle */
            int yi = irun;
            int hi = (irun = fixed2int_var_rounded(dda_current(pnext.y))) - yi;

            if (hi < 0)
                yi += hi, hi = -hi;

            if (yi < miny)
                hi += yi - miny, yi = miny;
            if (yi+hi > maxy)
                hi = maxy - yi;
            if (hi > 0) {
                /* assert(color_is_pure(&cmapper->devc)); */
                gx_color_index color = cmapper->devc.colors.pure;
                for (k = 0; k < spp; k++) {
                    unsigned char c = (color>>mdev->planes[k].shift) & ((1<<mdev->planes[k].depth)-1);
                    for (h = 0; h < hi; h++) {
                        out = mdev->line_ptrs[yi + h + k * mdev->height] + vci;
                        memset(out, c, vdi);
                    }
                }
            }
        }
        data = run;
    }
    return 1;
}

static int
mem_transform_pixel_region_render_landscape_1p(gx_device *dev, mem_transform_pixel_region_state_t *state, const unsigned char **buffer, int data_x, gx_cmapper_t *cmapper, const gs_gstate *pgs)
{
    return template_mem_transform_pixel_region_render_landscape_planar(dev, state, buffer, data_x, cmapper, pgs, 1);
}

static int
mem_transform_pixel_region_render_landscape_3p(gx_device *dev, mem_transform_pixel_region_state_t *state, const unsigned char **buffer, int data_x, gx_cmapper_t *cmapper, const gs_gstate *pgs)
{
    return template_mem_transform_pixel_region_render_landscape_planar(dev, state, buffer, data_x, cmapper, pgs, 3);
}

static int
mem_transform_pixel_region_render_landscape_4p(gx_device *dev, mem_transform_pixel_region_state_t *state, const unsigned char **buffer, int data_x, gx_cmapper_t *cmapper, const gs_gstate *pgs)
{
    return template_mem_transform_pixel_region_render_landscape_planar(dev, state, buffer, data_x, cmapper, pgs, 4);
}

static int
mem_transform_pixel_region_render_landscape_np(gx_device *dev, mem_transform_pixel_region_state_t *state, const unsigned char **buffer, int data_x, gx_cmapper_t *cmapper, const gs_gstate *pgs)
{
    return template_mem_transform_pixel_region_render_landscape_planar(dev, state, buffer, data_x, cmapper, pgs, state->spp);
}

static int
mem_transform_pixel_region_render_landscape_planar(gx_device *dev, mem_transform_pixel_region_state_t *state, const unsigned char **buffer, int data_x, gx_cmapper_t *cmapper, const gs_gstate *pgs)
{
    switch (state->spp) {
    case 1:
        return mem_transform_pixel_region_render_landscape_1p(dev, state, buffer, data_x, cmapper, pgs);
    case 3:
        return mem_transform_pixel_region_render_landscape_3p(dev, state, buffer, data_x, cmapper, pgs);
    case 4:
        return mem_transform_pixel_region_render_landscape_4p(dev, state, buffer, data_x, cmapper, pgs);
    default:
        return mem_transform_pixel_region_render_landscape_np(dev, state, buffer, data_x, cmapper, pgs);
    }
}

static int
mem_transform_pixel_region_begin(gx_device *dev, int w, int h, int spp,
                      const gx_dda_fixed_point *pixels, const gx_dda_fixed_point *rows,
                      const gs_int_rect *clip, transform_pixel_region_posture posture,
                      mem_transform_pixel_region_state_t **statep)
{
    gx_device_memory *mdev = (gx_device_memory *)dev;
    mem_transform_pixel_region_state_t *state;
    gs_memory_t *mem = dev->memory->non_gc_memory;
    *statep = state = (mem_transform_pixel_region_state_t *)gs_alloc_bytes(mem, sizeof(mem_transform_pixel_region_state_t), "mem_transform_pixel_region_state_t");
    if (state == NULL)
        return gs_error_VMerror;
    state->mem = mem;
    state->rows = *rows;
    state->pixels = *pixels;
    state->clip = *clip;
    if (state->clip.p.x < 0)
        state->clip.p.x = 0;
    if (state->clip.q.x > dev->width)
        state->clip.q.x = dev->width;
    if (state->clip.p.y < 0)
        state->clip.p.y = 0;
    if (state->clip.q.y > dev->height)
        state->clip.q.y = dev->height;
    state->w = w;
    state->h = h;
    state->spp = spp;
    state->posture = posture;
#ifdef WITH_CAL
    state->cal_ctx = NULL;
    state->cal_dbl = NULL;
#endif

    if (state->posture == transform_pixel_region_portrait) {
#ifdef WITH_CAL
        int factor;
        if (mdev->num_planar_planes > 1) {
            goto planar;
        } else if (pixels->x.step.dQ == fixed_1*8 && pixels->x.step.dR == 0 && rows->y.step.dQ == fixed_1*8 && rows->y.step.dR == 0) {
            state->render = mem_transform_pixel_region_render_portrait_1to8;
            factor = 8;
            goto use_doubler;
        } else if (pixels->x.step.dQ == fixed_1*4 && pixels->x.step.dR == 0 && rows->y.step.dQ == fixed_1*4 && rows->y.step.dR == 0) {
            state->render = mem_transform_pixel_region_render_portrait_1to4;
            factor = 4;
            goto use_doubler;
        } else if (pixels->x.step.dQ == fixed_1*2 && pixels->x.step.dR == 0 && rows->y.step.dQ == fixed_1*2 && rows->y.step.dR == 0) {
            unsigned int in_lines;
            int l, r;
            factor = 2;
            state->render = mem_transform_pixel_region_render_portrait_1to2;
        use_doubler:
            l = fixed2int_var_rounded(dda_current(pixels->x));
            r = fixed2int_var_rounded(dda_current(pixels->x) - fixed_epsilon) + w * factor;
            if (l > r) {
                int t = l; l = r; r = t;
            }
            if (l < state->clip.p.x || r > state->clip.q.x)
                goto no_cal;
            state->cal_dbl = cal_doubler_init(mem->gs_lib_ctx->core->cal_ctx,
                                              mem,
                                              w,
                                              h,
                                              factor,
                                              CAL_DOUBLE_NEAREST,
                                              spp,
                                              &in_lines);
            /* assert(in_lines == 1) */
            if (state->cal_dbl == NULL)
                goto no_cal;
        } else
no_cal:
#endif
        if (mdev->num_planar_planes > 1)
#ifdef WITH_CAL
planar:
#endif
            state->render = mem_transform_pixel_region_render_portrait_planar;
        else if (pixels->x.step.dQ == fixed_1 && pixels->x.step.dR == 0)
            state->render = mem_transform_pixel_region_render_portrait_1to1;
        else
            state->render = mem_transform_pixel_region_render_portrait;
    } else if (mdev->num_planar_planes > 1)
        state->render = mem_transform_pixel_region_render_landscape_planar;
    else
        state->render = mem_transform_pixel_region_render_landscape;

    return 0;
}

static void
step_to_next_line(mem_transform_pixel_region_state_t *state)
{
    fixed x = dda_current(state->rows.x);
    fixed y = dda_current(state->rows.y);
    dda_next(state->rows.x);
    dda_next(state->rows.y);
    x = dda_current(state->rows.x) - x;
    y = dda_current(state->rows.y) - y;
    dda_translate(state->pixels.x, x);
    dda_translate(state->pixels.y, y);
}

static int
mem_transform_pixel_region_data_needed(gx_device *dev, mem_transform_pixel_region_state_t *state)
{
    if (state->posture == transform_pixel_region_portrait) {
        int iy, ih;

        get_portrait_y_extent(state, &iy, &ih);

        if (iy + ih < state->clip.p.y || iy >= state->clip.q.y) {
            /* Skip this line. */
            step_to_next_line(state);
            return 0;
        }
    } else if (state->posture == transform_pixel_region_landscape) {
        int ix, iw;

        get_landscape_x_extent(state, &ix, &iw);

        if (ix + iw < state->clip.p.x || ix >= state->clip.q.x) {
            /* Skip this line. */
            step_to_next_line(state);
            return 0;
        }
    }

    return 1;
}

static int
mem_transform_pixel_region_process_data(gx_device *dev, mem_transform_pixel_region_state_t *state, const unsigned char **buffer, int data_x, gx_cmapper_t *cmapper, const gs_gstate *pgs)
{
    int ret = state->render(dev, state, buffer, data_x, cmapper, pgs);

    step_to_next_line(state);

    return ret;
}

static int
mem_transform_pixel_region_end(gx_device *dev, mem_transform_pixel_region_state_t *state)
{
    if (state)
        gs_free_object(state->mem->non_gc_memory, state, "mem_transform_pixel_region_state_t");
    return 0;
}

int mem_transform_pixel_region(gx_device *dev, transform_pixel_region_reason reason, transform_pixel_region_data *data)
{
    mem_transform_pixel_region_state_t *state = (mem_transform_pixel_region_state_t *)data->state;
    transform_pixel_region_posture posture;

    /* Pass through */
    if (reason == transform_pixel_region_begin) {
        const gx_dda_fixed_point *rows = data->u.init.rows;
        const gx_dda_fixed_point *pixels = data->u.init.pixels;
        if (rows->x.step.dQ == 0 && rows->x.step.dR == 0 && pixels->y.step.dQ == 0 && pixels->y.step.dR == 0)
            posture = transform_pixel_region_portrait;
        else if (rows->y.step.dQ == 0 && rows->y.step.dR == 0 && pixels->x.step.dQ == 0 && pixels->x.step.dR == 0)
            posture = transform_pixel_region_landscape;
        else
            posture = transform_pixel_region_skew;

        if (posture == transform_pixel_region_skew || dev->color_info.depth != data->u.init.spp*8 || data->u.init.lop != 0xf0) {
            mem_transform_pixel_region_state_t *state = (mem_transform_pixel_region_state_t *)gs_alloc_bytes(dev->memory->non_gc_memory, sizeof(mem_transform_pixel_region_state_t), "mem_transform_pixel_region_state_t");
            if (state == NULL)
                return gs_error_VMerror;
            state->render = NULL;
            if (gx_default_transform_pixel_region(dev, transform_pixel_region_begin, data) < 0) {
                gs_free_object(dev->memory->non_gc_memory, state, "mem_transform_pixel_region_state_t");
                return gs_error_VMerror;
            }
            state->passthru = data->state;
            data->state = state;
            return 0;
        }
    } else if (state->render == NULL) {
        int ret;
        data->state = state->passthru;
        ret = gx_default_transform_pixel_region(dev, reason, data);
        data->state = state;
        if (reason == transform_pixel_region_end) {
            gs_free_object(dev->memory->non_gc_memory, state, "mem_transform_pixel_region_state_t");
            data->state = NULL;
        }
        return ret;
    }

    /* We can handle this case natively */
    switch(reason)
    {
    case transform_pixel_region_begin:
        return mem_transform_pixel_region_begin(dev, data->u.init.w, data->u.init.h, data->u.init.spp, data->u.init.pixels, data->u.init.rows, data->u.init.clip, posture, (mem_transform_pixel_region_state_t **)&data->state);
    case transform_pixel_region_data_needed:
        return mem_transform_pixel_region_data_needed(dev, state);
    case transform_pixel_region_process_data:
        return mem_transform_pixel_region_process_data(dev, state, data->u.process_data.buffer, data->u.process_data.data_x, data->u.process_data.cmapper, data->u.process_data.pgs);
    case transform_pixel_region_end:
        data->state = NULL;
        return mem_transform_pixel_region_end(dev, state);
    default:
        return gs_error_unknownerror;
    }
}
