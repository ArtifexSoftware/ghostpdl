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

/* Default implementation of device get_bits[_rectangle] */
#include "memory_.h"
#include "gx.h"
#include "gserrors.h"
#include "gxdevice.h"
#include "gxdevmem.h"
#include "gxgetbit.h"
#include "gxlum.h"
#include "gdevmem.h"
#include "gxdevsop.h"

/*
 * Determine whether we can satisfy a request by simply using the stored
 * representation.  dev is used only for color_info.{num_components, depth}.
 */
static bool
requested_includes_stored(const gx_device *dev,
                          const gs_get_bits_params_t *requested,
                          const gs_get_bits_params_t *stored)
{
    gs_get_bits_options_t both = requested->options & stored->options;

    if (!(both & GB_PACKING_ALL))
        return false;
    if (stored->options & GB_SELECT_PLANES) {
        /*
         * The device only provides a subset of the planes.
         * Make sure it provides all the requested ones.
         */
        int i;
        int n = (stored->options & GB_PACKING_BIT_PLANAR ?
                 dev->color_info.depth : dev->color_info.num_components);

        if (!(requested->options & GB_SELECT_PLANES) ||
            !(both & (GB_PACKING_PLANAR | GB_PACKING_BIT_PLANAR))
            )
            return false;
        for (i = 0; i < n; ++i)
            if (requested->data[i] && !stored->data[i])
                return false;
    }
    if (both & GB_COLORS_NATIVE)
        return true;
    if (both & GB_COLORS_STANDARD_ALL) {
        if ((both & GB_ALPHA_ALL) && (both & GB_DEPTH_ALL))
            return true;
    }
    return false;
}

/*
 * Try to implement get_bits_rectangle by returning a pointer.
 * Note that dev is used only for computing the default raster
 * and for color_info.depth.
 * This routine does not check x or h for validity.
 */
int
gx_get_bits_return_pointer(gx_device * dev, int x, int h,
                           gs_get_bits_params_t *params,
                           const gs_get_bits_params_t *stored,
                           byte **stored_base)
{
    gs_get_bits_options_t options = params->options;
    gs_get_bits_options_t both = options & stored->options;

    if (!(options & GB_RETURN_POINTER) ||
        !requested_includes_stored(dev, params, stored)
        )
        return -1;
    /*
     * See whether we can return the bits in place.  Note that even if
     * OFFSET_ANY isn't set, x_offset and x don't have to be equal: their
     * bit offsets only have to match modulo align_bitmap_mod * 8 (to
     * preserve alignment) if ALIGN_ANY isn't set, or mod 8 (since
     * byte alignment is always required) if ALIGN_ANY is set.
     */
    {
        int depth = dev->color_info.depth;
        /*
         * For PLANAR devices, we assume that each plane consists of
         * depth/num_components bits.  This is wrong in general, but if
         * the device wants something else, it should implement
         * get_bits_rectangle itself.
         */
        uint dev_raster = gx_device_raster(dev, true);
        uint raster =
            (options & (GB_RASTER_STANDARD | GB_RASTER_ANY) ? dev_raster :
             params->raster);
        byte *base;

        if (h <= 1 || raster == dev_raster) {
            int x_offset =
                (options & GB_OFFSET_ANY ? x :
                 options & GB_OFFSET_0 ? 0 : params->x_offset);

            if (x_offset == x) {
                base = stored_base[0];
                params->x_offset = x;
            } else {
                uint align_mod =
                    (options & GB_ALIGN_ANY ? 8 : align_bitmap_mod * 8);
                int bit_offset = x - x_offset;
                int bytes;

                if (bit_offset & (align_mod - 1))
                    return -1;	/* can't align */
                if (depth & (depth - 1)) {
                    /* step = lcm(depth, align_mod) */
                    int step = depth / igcd(depth, align_mod) * align_mod;

                    bytes = bit_offset / step * step;
                } else {
                    /* Use a faster algorithm if depth is a power of 2. */
                    bytes = bit_offset & (-depth & -(int)align_mod);
                }
                base = stored_base[0] + arith_rshift(bytes, 3);
                params->x_offset = (bit_offset - bytes) / depth;
            }
            params->options =
                GB_ALIGN_STANDARD | GB_RETURN_POINTER | GB_RASTER_STANDARD |
                (stored->options & ~GB_PACKING_ALL) /*see below for PACKING*/ |
                (params->x_offset == 0 ? GB_OFFSET_0 : GB_OFFSET_SPECIFIED);
            if (both & GB_PACKING_CHUNKY) {
                params->options |= GB_PACKING_CHUNKY;
                params->data[0] = base;
            } else {
                int n =
                    (stored->options & GB_PACKING_BIT_PLANAR ?
                       (params->options |= GB_PACKING_BIT_PLANAR,
                        dev->color_info.depth) :
                       (params->options |= GB_PACKING_PLANAR,
                        dev->num_planar_planes));
                int i;

                for (i = 0; i < n; ++i) {
                    if (!(both & GB_SELECT_PLANES) || stored->data[i] != 0) {
                        params->data[i] = base;
                    }
                    if (i < n-1) {
                        base += stored_base[dev->height]-stored_base[0];
                        stored_base += dev->height;
                    }
                }
            }
            return 0;
        }
    }
    return -1;
}

/*
 * Implement gx_get_bits_copy (see below) for the case of converting
 * 4-bit CMYK to 24-bit RGB with standard mapping, used heavily by PCL.
 */
static void
gx_get_bits_copy_cmyk_1bit(byte *dest_line, uint dest_raster,
                           const byte *src_line, uint src_raster,
                           int src_bit, int w, int h)
{
    for (; h > 0; dest_line += dest_raster, src_line += src_raster, --h) {
        const byte *src = src_line;
        byte *dest = dest_line;
        bool hi = (src_bit & 4) != 0;  /* last nibble processed was hi */
        int i;

        for (i = w; i > 0; dest += 3, --i) {
            uint pixel =
                ((hi = !hi)? *src >> 4 : *src++ & 0xf);

            if (pixel & 1)
                dest[0] = dest[1] = dest[2] = 0;
            else {
                dest[0] = (byte)((pixel >> 3) - 1);
                dest[1] = (byte)(((pixel >> 2) & 1) - 1);
                dest[2] = (byte)(((pixel >> 1) & 1) - 1);
            }
        }
    }
}

/*
 * Convert pixels between representations, primarily for get_bits_rectangle.
 * stored indicates how the data are actually stored, and includes:
 *      - one option from the GB_PACKING group;
 *      - if h > 1, one option from the GB_RASTER group;
 *      - optionally (and normally), GB_COLORS_NATIVE;
 *      - optionally, one option each from the GB_COLORS_STANDARD, GB_DEPTH,
 *      and GB_ALPHA groups.
 * Note that dev is used only for color mapping.  This routine assumes that
 * the stored data are aligned.
 *
 * Note: this routine does not check x, w, h for validity.
 *
 * The code for converting between standard and native colors has been
 * factored out into single-use procedures strictly for readability.
 * A good optimizing compiler would compile them in-line.
 */
static int
    gx_get_bits_native_to_std(gx_device * dev, int x, int w, int h,
                              gs_get_bits_params_t * params,
                              const gs_get_bits_params_t *stored,
                              const byte * src_base, uint dev_raster,
                              int x_offset, uint raster, uint std_raster);
int
gx_get_bits_copy(gx_device * dev, int x, int w, int h,
                 gs_get_bits_params_t * params,
                 const gs_get_bits_params_t *stored,
                 const byte * src_base, uint dev_raster)
{
    gs_get_bits_options_t options = params->options;
    gs_get_bits_options_t stored_options = stored->options;
    int x_offset = (options & GB_OFFSET_0 ? 0 : params->x_offset);
    int depth = dev->color_info.depth;
    int bit_x = x * depth;
    const byte *src = src_base;
    /*
     * If the stored representation matches a requested representation,
     * we can copy the data without any transformations.
     */
    bool direct_copy = requested_includes_stored(dev, params, stored);
    int code = 0;

    /*
     * The request must include either GB_PACKING_CHUNKY or
     * GB_PACKING_PLANAR + GB_SELECT_PLANES, GB_RETURN_COPY,
     * and an offset and raster specification.  In the planar case,
     * the request must include GB_ALIGN_STANDARD, the stored
     * representation must include GB_PACKING_CHUNKY, and both must
     * include GB_COLORS_NATIVE.
     */
    if ((~options & GB_RETURN_COPY) ||
        !(options & (GB_OFFSET_0 | GB_OFFSET_SPECIFIED)) ||
        !(options & (GB_RASTER_STANDARD | GB_RASTER_SPECIFIED))
        )
        return_error(gs_error_rangecheck);
    if (options & GB_PACKING_CHUNKY) {
        byte *data = params->data[0];
        int end_bit = (x_offset + w) * depth;
        uint std_raster =
            (options & GB_ALIGN_STANDARD ? bitmap_raster(end_bit) :
             (end_bit + 7) >> 3);
        uint raster =
            (options & GB_RASTER_STANDARD ? std_raster : params->raster);
        int dest_bit_x = x_offset * depth;
        int skew = bit_x - dest_bit_x;

        /*
         * If the bit positions line up, use bytes_copy_rectangle.
         * Since bytes_copy_rectangle doesn't require alignment,
         * the bit positions only have to match within a byte,
         * not within align_bitmap_mod bytes.
         */
        if (!(skew & 7) && direct_copy) {
            int bit_w = w * depth;

            bytes_copy_rectangle(data + (dest_bit_x >> 3), raster,
                                 src + (bit_x >> 3), dev_raster,
                              ((bit_x + bit_w + 7) >> 3) - (bit_x >> 3), h);
        } else if (direct_copy) {
            /*
             * Use the logic already in mem_mono_copy_mono to copy the
             * bits to the destination.  We do this one line at a time,
             * to avoid having to allocate a line pointer table.
             */
            gx_device_memory tdev;
            byte *line_ptr = data;
            int bit_w = w * depth;

            tdev.line_ptrs = &tdev.base;
            for (; h > 0; line_ptr += raster, src += dev_raster, --h) {
                /* Make sure the destination is aligned. */
                int align = ALIGNMENT_MOD(line_ptr, align_bitmap_mod);

                tdev.base = line_ptr - align;
                /* set up parameters required by copy_mono's fit_copy */
                tdev.width = dest_bit_x + (align << 3) + bit_w;
                tdev.height = 1;
                code = mem_mono_copy_mono((gx_device *) & tdev, src, bit_x,
                                          dev_raster, gx_no_bitmap_id,
                                          dest_bit_x + (align << 3), 0, bit_w, 1,
                                          (gx_color_index) 0, (gx_color_index) 1);
                if (code < 0)
                    break;
            }
        } else if (options & ~stored_options & GB_COLORS_NATIVE) {
            /* Convert standard colors to native. */
            return_error(gs_error_rangecheck);
        } else {
            /* Convert native colors to standard. */
            code = gx_get_bits_native_to_std(dev, x, w, h, params, stored,
                                             src_base, dev_raster,
                                             x_offset, raster, std_raster);
            options = params->options;
        }
        params->options =
            (options & (GB_COLORS_ALL | GB_ALPHA_ALL)) | GB_PACKING_CHUNKY |
            (options & GB_COLORS_NATIVE ? 0 : options & GB_DEPTH_ALL) |
            (options & GB_ALIGN_STANDARD ? GB_ALIGN_STANDARD : GB_ALIGN_ANY) |
            GB_RETURN_COPY |
            (x_offset == 0 ? GB_OFFSET_0 : GB_OFFSET_SPECIFIED) |
            (raster == std_raster ? GB_RASTER_STANDARD : GB_RASTER_SPECIFIED);
    } else if (!(~options &
                 (GB_PACKING_PLANAR | GB_SELECT_PLANES | GB_ALIGN_STANDARD)) &&
               (stored_options & GB_PACKING_CHUNKY) &&
               ((options & stored_options) & GB_COLORS_NATIVE)
               ) {
        uchar num_planes = dev->color_info.num_components;
        int dest_depth = depth / num_planes;
        bits_plane_t source, dest;
        int plane = -1;
        uchar i;

        /* Make sure only one plane is being requested. */
        for (i = 0; i < num_planes; ++i)
            if (params->data[i] != 0) {
                if (plane >= 0)
                    return_error(gs_error_rangecheck); /* > 1 plane */
                plane = i;
            }
        /* Ensure at least one plane is requested */
        if (plane < 0)
            return_error(gs_error_rangecheck); /* No planes */
        source.data.read = src_base;
        source.raster = dev_raster;
        source.depth = depth;
        source.x = x;
        dest.data.write = params->data[plane];
        dest.raster =
            (options & GB_RASTER_STANDARD ?
             bitmap_raster((x_offset + w) * dest_depth) : params->raster);
        if (dev->color_info.separable_and_linear == GX_CINFO_SEP_LIN)
            dest.depth = dev->color_info.comp_bits[plane];
        else
            dest.depth = dest_depth;
        dest.x = x_offset;
        return bits_extract_plane(&dest, &source,
                                  (num_planes - 1 - plane) * dest_depth,
                                  w, h);
    } else
        return_error(gs_error_rangecheck);
    return code;
}

/*
 * Convert native colors to standard.  Only GB_DEPTH_8 is supported.
 */
static int
gx_get_bits_native_to_std(gx_device * dev, int x, int w, int h,
                          gs_get_bits_params_t * params,
                          const gs_get_bits_params_t *stored,
                          const byte * src_base, uint dev_raster,
                          int x_offset, uint raster, uint std_raster)
{
    int depth = dev->color_info.depth;
    int src_bit_offset = x * depth;
    const byte *src_line = src_base + (src_bit_offset >> 3);
    gs_get_bits_options_t options = params->options;
    int ncomp =
        (options & (GB_ALPHA_FIRST | GB_ALPHA_LAST) ? 4 : 3);
    byte *dest_line = params->data[0] + x_offset * ncomp;
    byte *mapped[16];
    int dest_bytes;
    int i;

    if (!(options & GB_DEPTH_8)) {
        /*
         * We don't support general depths yet, or conversion between
         * different formats.  Punt.
         */
        return_error(gs_error_rangecheck);
    }

    /* Pick the representation that's most likely to be useful. */
    if (options & GB_COLORS_RGB)
        params->options = options &= ~GB_COLORS_STANDARD_ALL | GB_COLORS_RGB,
            dest_bytes = 3;
    else if (options & GB_COLORS_CMYK)
        params->options = options &= ~GB_COLORS_STANDARD_ALL | GB_COLORS_CMYK,
            dest_bytes = 4;
    else if (options & GB_COLORS_GRAY)
        params->options = options &= ~GB_COLORS_STANDARD_ALL | GB_COLORS_GRAY,
            dest_bytes = 1;
    else
        return_error(gs_error_rangecheck);
    /* Recompute the destination raster based on the color space. */
    if (options & GB_RASTER_STANDARD) {
        uint end_byte = (x_offset + w) * dest_bytes;

        raster = std_raster =
            (options & GB_ALIGN_STANDARD ?
             bitmap_raster(end_byte << 3) : end_byte);
    }
    /* Check for the one special case we care about, namely that we have a
     * device that uses cmyk_1bit_map_cmyk_color, or equivalent. We do not
     * check function pointers directly, as this is defeated by forwarding
     * devices, but rather use a dev_spec_op. */
    if (((options & (GB_COLORS_RGB | GB_ALPHA_FIRST | GB_ALPHA_LAST))
           == GB_COLORS_RGB) &&
        (dev_proc(dev, dev_spec_op)(dev, gxdso_is_std_cmyk_1bit, NULL, 0) > 0)) {
        gx_get_bits_copy_cmyk_1bit(dest_line, raster,
                                   src_line, dev_raster,
                                   src_bit_offset & 7, w, h);
        return 0;
    }
    if (options & (GB_ALPHA_FIRST | GB_ALPHA_LAST))
        ++dest_bytes;
    /* Clear the color translation cache. */
    for (i = (depth > 4 ? 16 : 1 << depth); --i >= 0; )
        mapped[i] = 0;
    for (; h > 0; dest_line += raster, src_line += dev_raster, --h) {
        const byte *src = src_line;
        int bit = src_bit_offset & 7;
        byte *dest = dest_line;

        for (i = 0; i < w; ++i) {
            gx_color_index pixel = 0;
            gx_color_value rgba[4];

            if (sizeof(pixel) > 4) {
                if (sample_load_next64((uint64_t *)&pixel, &src, &bit, depth) < 0)
                    return_error(gs_error_rangecheck);
            }
            else {
                if (sample_load_next32((uint32_t *)&pixel, &src, &bit, depth) < 0)
                    return_error(gs_error_rangecheck);
            }
            if (pixel < 16) {
                if (mapped[pixel]) {
                    /* Use the value from the cache. */
                    memcpy(dest, mapped[pixel], dest_bytes);
                    dest += dest_bytes;
                    continue;
                }
                mapped[pixel] = dest;
            }
            (*dev_proc(dev, map_color_rgb)) (dev, pixel, rgba);
            if (options & GB_ALPHA_FIRST)
                *dest++ = 0xff;
            /* Convert to the requested color space. */
            if (options & GB_COLORS_RGB) {
                dest[0] = gx_color_value_to_byte(rgba[0]);
                dest[1] = gx_color_value_to_byte(rgba[1]);
                dest[2] = gx_color_value_to_byte(rgba[2]);
                dest += 3;
            } else if (options & GB_COLORS_CMYK) {
                /* Use the standard RGB to CMYK algorithm, */
                /* with maximum black generation and undercolor removal. */
                gx_color_value white = max(rgba[0], max(rgba[1], rgba[2]));

                dest[0] = gx_color_value_to_byte(white - rgba[0]);
                dest[1] = gx_color_value_to_byte(white - rgba[1]);
                dest[2] = gx_color_value_to_byte(white - rgba[2]);
                dest[3] = gx_color_value_to_byte(gx_max_color_value - white);
                dest += 4;
            } else {	/* GB_COLORS_GRAY */
                /* Use the standard RGB to Gray algorithm. */
                *dest++ = gx_color_value_to_byte(
                                ((rgba[0] * (ulong) lum_red_weight) +
                                 (rgba[1] * (ulong) lum_green_weight) +
                                 (rgba[2] * (ulong) lum_blue_weight) +
                                   (lum_all_weights / 2))
                                / lum_all_weights);
            }
            if (options & GB_ALPHA_LAST)
                *dest++ = 0xff;
        }
    }
    return 0;
}

/* ------ Default implementations of get_bits_rectangle ------ */

int
gx_default_get_bits_rectangle(gx_device * dev, const gs_int_rect * prect,
                       gs_get_bits_params_t * params)
{
    return_error(gs_error_unknownerror);
}

int gx_blank_get_bits_rectangle(gx_device *dev, const gs_int_rect *prect,
                                gs_get_bits_params_t *params)
{
    int supported = GB_COLORS_NATIVE |
                    GB_ALPHA_NONE |
                    GB_DEPTH_8 |
                    GB_PACKING_CHUNKY |
                    GB_RETURN_COPY |
                    GB_ALIGN_STANDARD |
                    GB_OFFSET_0 |
                    GB_RASTER_STANDARD;
    unsigned char *ptr = params->data[0];
    int bytes = (prect->q.x - prect->p.x) * dev->color_info.num_components;
    int col = dev->color_info.num_components > 3 ? 0 : 0xff;
    int raster = bitmap_raster(dev->width * dev->color_info.num_components);
    int y;

    if ((params->options & supported) != supported)
        return_error(gs_error_unknownerror);

    params->options = supported;

    for (y = prect->p.y; y < prect->q.y; y++) {
        memset(ptr, col, bytes);
        ptr += raster;
    }

    return 0;
}
