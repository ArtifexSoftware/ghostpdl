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

/* 8-bit-per-pixel "memory" (stored bitmap) device */
#include "memory_.h"
#include "gx.h"
#include "gxdevice.h"
#include "gxdevmem.h"           /* semi-public definitions */
#include "gdevmem.h"            /* private definitions */

#define mem_gray8_strip_copy_rop2 mem_gray8_rgb24_strip_copy_rop2

/* ================ Standard (byte-oriented) device ================ */

#undef chunk
#define chunk byte

/* Procedures */
declare_mem_procs(mem_mapped8_copy_mono, mem_mapped8_copy_color, mem_mapped8_fill_rectangle);

/* The device descriptor. */
const gx_device_memory mem_mapped8_device =
    mem_device("image8", 8, 0, mem_dev_initialize_device_procs);

const gdev_mem_functions gdev_mem_fns_8 =
{
    mem_mapped_map_rgb_color,
    mem_mapped_map_color_rgb,
    mem_mapped8_fill_rectangle,
    mem_mapped8_copy_mono,
    mem_mapped8_copy_color,
    gx_default_copy_alpha,
    gx_default_strip_tile_rectangle,
    mem_gray8_strip_copy_rop2,
    mem_get_bits_rectangle
};

/* Convert x coordinate to byte offset in scan line. */
#undef x_to_byte
#define x_to_byte(x) (x)

/* Fill a rectangle with a color. */
static int
mem_mapped8_fill_rectangle(gx_device * dev,
                           int x, int y, int w, int h, gx_color_index color)
{
    gx_device_memory * const mdev = (gx_device_memory *)dev;

    fit_fill(dev, x, y, w, h);
    bytes_fill_rectangle(scan_line_base(mdev, y) + x, mdev->raster,
                         (byte) color, w, h);
    return 0;
}

/* Copy a monochrome bitmap. */
/* We split up this procedure because of limitations in the bcc32 compiler. */
static void mapped8_copy01(chunk *, const byte *, int, int, uint,
                            int, int, byte, byte);
static void mapped8_copyN1(chunk *, const byte *, int, int, uint,
                            int, int, byte);
static void mapped8_copy0N(chunk *, const byte *, int, int, uint,
                            int, int, byte);
static int
mem_mapped8_copy_mono(gx_device * dev,
               const byte * base, int sourcex, int sraster, gx_bitmap_id id,
        int x, int y, int w, int h, gx_color_index zero, gx_color_index one)
{
    gx_device_memory * const mdev = (gx_device_memory *)dev;
    const byte *line;
    int first_bit;

    declare_scan_ptr(dest);
    fit_copy(dev, base, sourcex, sraster, id, x, y, w, h);
    setup_rect(dest);
    line = base + (sourcex >> 3);
    first_bit = sourcex & 7;
#define is_color(c) ((int)(c) != (int)gx_no_color_index)
    if (is_color(one)) {
        if (is_color(zero))
            mapped8_copy01(dest, line, first_bit, sraster, draster,
                           w, h, (byte) zero, (byte) one);
        else
            mapped8_copyN1(dest, line, first_bit, sraster, draster,
                           w, h, (byte) one);
    } else if (is_color(zero))
        mapped8_copy0N(dest, line, first_bit, sraster, draster,
                       w, h, (byte) zero);
#undef is_color
    return 0;
}
/* Halftone coloring */
static void
mapped8_copy01(chunk * dest, const byte * line, int first_bit,
               int sraster, uint draster, int w, int h, byte b0, byte b1)
{
    while ( h-- > 0 ) {
        register byte *pptr = dest;
        const byte *sptr = line;
        register int sbyte = *sptr++;
        register uint bit;
        int count = w + first_bit - 8; /* How many bits after first byte */
        if (count >= 0) {
            switch (first_bit) {
                case 0:
                    goto enter0;
                case 1:
                    goto enter1;
                case 2:
                    goto enter2;
                case 3:
                    goto enter3;
                case 4:
                    goto enter4;
                case 5:
                    goto enter5;
                case 6:
                    goto enter6;
                default:
                    goto enter7;
            }
            do {
                sbyte = *sptr++;
                /* In true gs fashion: Do not be tempted to replace the
                 * following lines with: *pptr++ = (condition ? b1 : b0);
                 * without good reason as gcc on ARM takes 4 cycles to do that
                 * rather than 3 cycles to do the form here. */
                enter0: if (sbyte & 128) *pptr++ = b1; else *pptr++ = b0;
                enter1: if (sbyte &  64) *pptr++ = b1; else *pptr++ = b0;
                enter2: if (sbyte &  32) *pptr++ = b1; else *pptr++ = b0;
                enter3: if (sbyte &  16) *pptr++ = b1; else *pptr++ = b0;
                enter4: if (sbyte &   8) *pptr++ = b1; else *pptr++ = b0;
                enter5: if (sbyte &   4) *pptr++ = b1; else *pptr++ = b0;
                enter6: if (sbyte &   2) *pptr++ = b1; else *pptr++ = b0;
                enter7: if (sbyte &   1) *pptr++ = b1; else *pptr++ = b0;
                count -= 8;
            } while (count >= 0);
            bit = 128;
            count += 8;
            if (count > 0)
                sbyte = *sptr++;
        } else {
            /* Less than 1 byte to do */
            bit = 0x80>>first_bit;
            count += 8-first_bit;
        }
        while (count > 0) {
            if (sbyte & bit) *pptr++ = b1; else *pptr++ = b0;
            bit >>= 1;
            count--;
        }
        line += sraster;
        inc_ptr(dest, draster);
    }
}
/* Stenciling */
static void
mapped8_copyN1(chunk * dest, const byte * line, int first_bit,
               int sraster, uint draster, int w, int h, byte b1)
{
    while ( h-- > 0 ) {
        register byte *pptr = dest;
        const byte *sptr = line;
        register int sbyte = *sptr++;
        register uint bit;
        int count = w + first_bit - 8; /* How many bits after first byte */
        if (count >= 0) {
            switch (first_bit) {
                case 0:
                    goto enter0;
                case 1:
                    goto enter1;
                case 2:
                    goto enter2;
                case 3:
                    goto enter3;
                case 4:
                    goto enter4;
                case 5:
                    goto enter5;
                case 6:
                    goto enter6;
                default:
                    goto enter7;
            }
            do {
                sbyte = *sptr++;
                enter0: if (sbyte & 128)
                            *pptr = b1;
                        pptr++;
                enter1: if (sbyte & 64)
                            *pptr = b1;
                        pptr++;
                enter2: if (sbyte & 32)
                            *pptr = b1;
                        pptr++;
                enter3: if (sbyte & 16)
                            *pptr = b1;
                        pptr++;
                enter4: if (sbyte & 8)
                            *pptr = b1;
                        pptr++;
                enter5: if (sbyte & 4)
                            *pptr = b1;
                        pptr++;
                enter6: if (sbyte & 2)
                            *pptr = b1;
                        pptr++;
                enter7: if (sbyte & 1)
                            *pptr = b1;
                        pptr++;
                count -= 8;
            } while (count >= 0);
            bit = 128;
            count += 8;
            if (count > 0)
                /* read the byte containing the trailing bits */
                sbyte = *sptr++;
        } else {
            /* Less than 1 byte to do */
            bit = 0x80>>first_bit;
            count += 8-first_bit;
        }
        while (count > 0) {
            if (sbyte & bit) *pptr = b1;
            pptr++;
            bit >>= 1;
            count--;
        }
        line += sraster;
        inc_ptr(dest, draster);
    }
}
/* Reverse stenciling */
static void
mapped8_copy0N(chunk * dest, const byte * line, int first_bit,
               int sraster, uint draster, int w, int h, byte b0)
{
    while ( h-- > 0 ) {
        register byte *pptr = dest;
        const byte *sptr = line;
        register int sbyte = *sptr++;
        register uint bit;
        int count = w + first_bit - 8; /* How many bits after first byte */
        if (count >= 0) {
            switch (first_bit) {
                case 0:
                    goto enter0;
                case 1:
                    goto enter1;
                case 2:
                    goto enter2;
                case 3:
                    goto enter3;
                case 4:
                    goto enter4;
                case 5:
                    goto enter5;
                case 6:
                    goto enter6;
                default:
                    goto enter7;
            }
            do {
                enter0: if (!(sbyte & 128))
                            *pptr = b0;
                        pptr++;
                enter1: if (!(sbyte & 64))
                           *pptr = b0;
                        pptr++;
                enter2: if (!(sbyte & 32))
                            *pptr = b0;
                        pptr++;
                enter3: if (!(sbyte & 16))
                            *pptr = b0;
                        pptr++;
                enter4: if (!(sbyte & 8))
                            *pptr = b0;
                        pptr++;
                enter5: if (!(sbyte & 4))
                            *pptr = b0;
                        pptr++;
                enter6: if (!(sbyte & 2))
                            *pptr = b0;
                        pptr++;
                enter7: if (!(sbyte & 1))
                            *pptr = b0;
                        pptr++;
                sbyte = *sptr++;
                count -= 8;
            } while (count >= 0);
            bit = 128;
            count += 8;
        } else {
            /* Less than 1 byte to do */
            bit = 0x80>>first_bit;
            count += 8-first_bit;
        }
        while (count > 0) {
            if (!(sbyte & bit)) *pptr = b0;
            pptr++;
            bit >>= 1;
            count--;
        }
        line += sraster;
        inc_ptr(dest, draster);
    }
}

/* Copy a color bitmap. */
static int
mem_mapped8_copy_color(gx_device * dev,
               const byte * base, int sourcex, int sraster, gx_bitmap_id id,
                       int x, int y, int w, int h)
{
    gx_device_memory * const mdev = (gx_device_memory *)dev;

    fit_copy(dev, base, sourcex, sraster, id, x, y, w, h);
    mem_copy_byte_rect(mdev, base, sourcex, sraster, x, y, w, h);
    return 0;
}

/* ================ "Word"-oriented device ================ */

/* Note that on a big-endian machine, this is the same as the */
/* standard byte-oriented-device. */

#if !ARCH_IS_BIG_ENDIAN

/* Procedures */
declare_mem_procs(mem8_word_copy_mono, mem8_word_copy_color, mem8_word_fill_rectangle);

/* Here is the device descriptor. */
const gx_device_memory mem_mapped8_word_device =
    mem_device("image8w", 8, 0, mem_word_dev_initialize_device_procs);

const gdev_mem_functions gdev_mem_fns_8w =
{
    gx_default_rgb_map_rgb_color,
    gx_default_rgb_map_color_rgb,
    mem8_word_fill_rectangle,
    mem8_word_copy_mono,
    mem8_word_copy_color,
    gx_default_copy_alpha,
    gx_default_strip_tile_rectangle,
    gx_no_strip_copy_rop2,
    mem_word_get_bits_rectangle
};

/* Fill a rectangle with a color. */
static int
mem8_word_fill_rectangle(gx_device * dev, int x, int y, int w, int h,
                         gx_color_index color)
{
    gx_device_memory * const mdev = (gx_device_memory *)dev;
    byte *base;
    size_t raster;

    fit_fill(dev, x, y, w, h);
    base = scan_line_base(mdev, y);
    raster = mdev->raster;
    mem_swap_byte_rect(base, raster, x << 3, w << 3, h, true);
    bytes_fill_rectangle(base + x, raster, (byte) color, w, h);
    mem_swap_byte_rect(base, raster, x << 3, w << 3, h, true);
    return 0;
}

/* Copy a bitmap. */
static int
mem8_word_copy_mono(gx_device * dev,
               const byte * base, int sourcex, int sraster, gx_bitmap_id id,
        int x, int y, int w, int h, gx_color_index zero, gx_color_index one)
{
    gx_device_memory * const mdev = (gx_device_memory *)dev;
    byte *row;
    size_t raster;
    bool store;

    fit_copy(dev, base, sourcex, sraster, id, x, y, w, h);
    row = scan_line_base(mdev, y);
    raster = mdev->raster;
    store = (zero != gx_no_color_index && one != gx_no_color_index);
    mem_swap_byte_rect(row, raster, x << 3, w << 3, h, store);
    mem_mapped8_copy_mono(dev, base, sourcex, sraster, id,
                          x, y, w, h, zero, one);
    mem_swap_byte_rect(row, raster, x << 3, w << 3, h, false);
    return 0;
}

/* Copy a color bitmap. */
static int
mem8_word_copy_color(gx_device * dev,
               const byte * base, int sourcex, int sraster, gx_bitmap_id id,
                     int x, int y, int w, int h)
{
    gx_device_memory * const mdev = (gx_device_memory *)dev;
    byte *row;
    size_t raster;

    fit_copy(dev, base, sourcex, sraster, id, x, y, w, h);
    row = scan_line_base(mdev, y);
    raster = mdev->raster;
    mem_swap_byte_rect(row, raster, x << 3, w << 3, h, true);
    mem_copy_byte_rect(mdev, base, sourcex, sraster, x, y, w, h);
    mem_swap_byte_rect(row, raster, x << 3, w << 3, h, false);
    return 0;
}

#endif /* !ARCH_IS_BIG_ENDIAN */
