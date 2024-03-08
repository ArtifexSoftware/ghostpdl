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

/* Generic "memory" (stored bitmap) device */
#include "memory_.h"
#include "gx.h"
#include "gsdevice.h"
#include "gserrors.h"
#include "gsrect.h"
#include "gsstruct.h"
#include "gxarith.h"
#include "gxdevice.h"
#include "gxgetbit.h"
#include "gxdevmem.h"		/* semi-public definitions */
#include "gdevmem.h"		/* private definitions */
#include "gstrans.h"

/* Structure descriptor */
public_st_device_memory();

/* GC procedures */
static
ENUM_PTRS_WITH(device_memory_enum_ptrs, gx_device_memory *mptr)
{
    return ENUM_USING(st_device_forward, vptr, sizeof(gx_device_forward), index - 4);
}
case 0: ENUM_RETURN((mptr->foreign_bits ? NULL : (void *)mptr->base));
case 1: ENUM_RETURN((mptr->foreign_line_pointers ? NULL : (void *)mptr->line_ptrs));
ENUM_STRING_PTR(2, gx_device_memory, palette);
case 3: ENUM_RETURN(mptr->owner);
ENUM_PTRS_END
static
RELOC_PTRS_WITH(device_memory_reloc_ptrs, gx_device_memory *mptr)
{
    if (!mptr->foreign_bits) {
        byte *base_old = mptr->base;
        long reloc;
        int y;
        int h = mptr->height;

        if (mptr->num_planar_planes > 1)
            h *= mptr->num_planar_planes;

        RELOC_PTR(gx_device_memory, base);
        reloc = base_old - mptr->base;
        for (y = 0; y < h; y++)
            mptr->line_ptrs[y] -= reloc;
        /* Relocate line_ptrs, which also points into the data area. */
        mptr->line_ptrs = (byte **) ((byte *) mptr->line_ptrs - reloc);
    } else if (!mptr->foreign_line_pointers) {
        RELOC_PTR(gx_device_memory, line_ptrs);
    }
    RELOC_CONST_STRING_PTR(gx_device_memory, palette);
    RELOC_PTR(gx_device_memory, owner);
    RELOC_USING(st_device_forward, vptr, sizeof(gx_device_forward));
}
RELOC_PTRS_END

/* Define the palettes for monobit devices. */
static const byte b_w_palette_string[6] = {
    0xff, 0xff, 0xff, 0, 0, 0
};
const gs_const_string mem_mono_b_w_palette = {
    b_w_palette_string, 6
};
static const byte w_b_palette_string[6] = {
    0, 0, 0, 0xff, 0xff, 0xff
};
const gs_const_string mem_mono_w_b_palette = {
    w_b_palette_string, 6
};

/* ------ Generic code ------ */

/* Return the appropriate memory device for a given */
/* number of bits per pixel (0 if none suitable).
   Greater than 64 occurs for the planar case
   which we will then return a mem_x_device */
static const gx_device_memory *const mem_devices[65] = {
    0, &mem_mono_device, &mem_mapped2_device, 0, &mem_mapped4_device,
    0, 0, 0, &mem_mapped8_device,
    0, 0, 0, 0, 0, 0, 0, &mem_true16_device,
    0, 0, 0, 0, 0, 0, 0, &mem_true24_device,
    0, 0, 0, 0, 0, 0, 0, &mem_true32_device,
    0, 0, 0, 0, 0, 0, 0, &mem_true40_device,
    0, 0, 0, 0, 0, 0, 0, &mem_true48_device,
    0, 0, 0, 0, 0, 0, 0, &mem_true56_device,
    0, 0, 0, 0, 0, 0, 0, &mem_true64_device
};
const gx_device_memory *
gdev_mem_device_for_bits(int bits_per_pixel)
{
    return ((uint)bits_per_pixel > 64 ? &mem_x_device :
            mem_devices[bits_per_pixel]);
}
/* Do the same for a word-oriented device. */
static const gx_device_memory *const mem_word_devices[65] = {
    0, &mem_mono_device, &mem_mapped2_word_device, 0, &mem_mapped4_word_device,
    0, 0, 0, &mem_mapped8_word_device,
    0, 0, 0, 0, 0, 0, 0, 0 /*no 16-bit word device*/,
    0, 0, 0, 0, 0, 0, 0, &mem_true24_word_device,
    0, 0, 0, 0, 0, 0, 0, &mem_true32_word_device,
    0, 0, 0, 0, 0, 0, 0, &mem_true40_word_device,
    0, 0, 0, 0, 0, 0, 0, &mem_true48_word_device,
    0, 0, 0, 0, 0, 0, 0, &mem_true56_word_device,
    0, 0, 0, 0, 0, 0, 0, &mem_true64_word_device
};
const gx_device_memory *
gdev_mem_word_device_for_bits(int bits_per_pixel)
{
    return ((uint)bits_per_pixel > 64 ? (const gx_device_memory *)0 :
            mem_word_devices[bits_per_pixel]);
}

static const gdev_mem_functions *mem_fns[65] = {
    NULL, &gdev_mem_fns_1, &gdev_mem_fns_2, NULL,
                            &gdev_mem_fns_4, NULL, NULL, NULL,
    &gdev_mem_fns_8, NULL, NULL, NULL, NULL, NULL, NULL, NULL,
    &gdev_mem_fns_16, NULL, NULL, NULL, NULL, NULL, NULL, NULL,
    &gdev_mem_fns_24, NULL, NULL, NULL, NULL, NULL, NULL, NULL,
    &gdev_mem_fns_32, NULL, NULL, NULL, NULL, NULL, NULL, NULL,
    &gdev_mem_fns_40, NULL, NULL, NULL, NULL, NULL, NULL, NULL,
    &gdev_mem_fns_48, NULL, NULL, NULL, NULL, NULL, NULL, NULL,
    &gdev_mem_fns_56, NULL, NULL, NULL, NULL, NULL, NULL, NULL,
    &gdev_mem_fns_64
};

const gdev_mem_functions *
gdev_mem_functions_for_bits(int bits_per_pixel)
{
    return ((uint)bits_per_pixel > 64 ? NULL : mem_fns[bits_per_pixel]);
}

static const gdev_mem_functions *mem_word_fns[65] = {
    NULL, &gdev_mem_fns_1, &gdev_mem_fns_2w, NULL,
                            &gdev_mem_fns_4w, NULL, NULL, NULL,
    &gdev_mem_fns_8w, NULL, NULL, NULL, NULL, NULL, NULL, NULL,
    NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL,
    &gdev_mem_fns_24w, NULL, NULL, NULL, NULL, NULL, NULL, NULL,
    &gdev_mem_fns_32w, NULL, NULL, NULL, NULL, NULL, NULL, NULL,
    &gdev_mem_fns_40w, NULL, NULL, NULL, NULL, NULL, NULL, NULL,
    &gdev_mem_fns_48w, NULL, NULL, NULL, NULL, NULL, NULL, NULL,
    &gdev_mem_fns_56w, NULL, NULL, NULL, NULL, NULL, NULL, NULL,
    &gdev_mem_fns_64w
};

const gdev_mem_functions *
gdev_mem_word_functions_for_bits(int bits_per_pixel)
{
    return ((uint)bits_per_pixel > 64 ? NULL : mem_word_fns[bits_per_pixel]);
}

/* Test whether a device is a memory device */
bool
gs_device_is_memory(const gx_device * dev)
{
    /*
     * We use the draw_thin_line procedure to mark memory devices.
     * See gdevmem.h.
     */
    return (dev_proc(dev, draw_thin_line) == mem_draw_thin_line);
}

/* Make a memory device. */
/* Note that the default for monobit devices is white = 0, black = 1. */
void
gs_make_mem_device(gx_device_memory * dev, const gx_device_memory * mdproto,
                   gs_memory_t * mem, int page_device, gx_device * target)
{
    /* Can never fail */
    (void)gx_device_init((gx_device *) dev, (const gx_device *)mdproto,
                         mem, true);
    dev->stype = &st_device_memory;
    switch (page_device) {
        case -1:
            set_dev_proc(dev, get_page_device, gx_default_get_page_device);
            break;
        case 1:
            set_dev_proc(dev, get_page_device, gx_page_device_get_page_device);
            break;
    }
    /* Preload the black and white cache. */
    if (target == NULL) {
        if (dev->color_info.depth == 1) {
            /* The default for black-and-white devices is inverted. */
            dev->cached_colors.black = 1;
            dev->cached_colors.white = 0;
        } else {
            dev->cached_colors.black = 0;
            dev->cached_colors.white = (1 << dev->color_info.depth) - 1;
        }
        dev->graphics_type_tag = GS_UNKNOWN_TAG;
    } else {
        gx_device_set_target((gx_device_forward *)dev, target);
        /* Forward the color mapping operations to the target. */
        gx_device_forward_color_procs((gx_device_forward *) dev);
        gx_device_copy_color_procs((gx_device *)dev, target);
        dev->color_info.separable_and_linear = target->color_info.separable_and_linear;
        dev->cached_colors = target->cached_colors;
        dev->graphics_type_tag = target->graphics_type_tag;	/* initialize to same as target */

        set_dev_proc(dev, put_image, gx_forward_put_image);
    }
    if (dev->color_info.depth == 1) {
        gx_color_value cv[GX_DEVICE_COLOR_MAX_COMPONENTS];
        uchar k;

        if (target != 0) {
            for (k = 0; k < target->color_info.num_components; k++) {
                cv[k] = 0;
            }
        }
       gdev_mem_mono_set_inverted(dev, (target == NULL ||
                                   (*dev_proc(dev, encode_color))((gx_device *)dev, cv) != 0));
    }
    set_dev_proc(dev, dev_spec_op, mem_spec_op);
    check_device_separable((gx_device *)dev);
    gx_device_fill_in_procs((gx_device *)dev);
    dev->band_y = 0;
    dev->owner = target;
}

/* Make a memory device using copydevice, this should replace gs_make_mem_device. */
/* Note that the default for monobit devices is white = 0, black = 1. */
int
gs_make_mem_device_with_copydevice(gx_device_memory ** ppdev,
                                   const gx_device_memory * mdproto,
                                   gs_memory_t * mem,
                                   int page_device,
                                   gx_device * target)
{
    int code;
    gx_device_memory *pdev;

    if (mem == NULL)
        return -1;

    code = gs_copydevice((gx_device **)&pdev,
                         (const gx_device *)mdproto,
                         mem);
    if (code < 0)
        return code;

    switch (page_device) {
        case -1:
            set_dev_proc(pdev, get_page_device, gx_default_get_page_device);
            break;
        case 1:
            set_dev_proc(pdev, get_page_device, gx_page_device_get_page_device);
            break;
    }
    /* Preload the black and white cache. */
    if (target == NULL) {
        if (pdev->color_info.depth == 1) {
            /* The default for black-and-white devices is inverted. */
            pdev->cached_colors.black = 1;
            pdev->cached_colors.white = 0;
        } else {
            pdev->cached_colors.black = 0;
            pdev->cached_colors.white = (1 << pdev->color_info.depth) - 1;
        }
        pdev->graphics_type_tag = GS_UNKNOWN_TAG;
    } else {
        gx_device_set_target((gx_device_forward *)pdev, target);
        /* Forward the color mapping operations to the target. */
        gx_device_forward_color_procs((gx_device_forward *) pdev);
        gx_device_copy_color_procs((gx_device *)pdev, target);
        pdev->cached_colors = target->cached_colors;
        pdev->graphics_type_tag = target->graphics_type_tag;	/* initialize to same as target */
    }
    if (pdev->color_info.depth == 1) {
        gx_color_value cv[3];

       cv[0] = cv[1] = cv[2] = 0;
        gdev_mem_mono_set_inverted(pdev, (target == NULL ||
                                   (*dev_proc(pdev, encode_color))((gx_device *)pdev, cv) != 0));
    }
    check_device_separable((gx_device *)pdev);
    gx_device_fill_in_procs((gx_device *)pdev);
    pdev->band_y = 0;
    *ppdev = pdev;
    return 0;
}

/* Make a monobit memory device using copydevice */
int
gs_make_mem_mono_device_with_copydevice(gx_device_memory ** ppdev, gs_memory_t * mem,
                                        gx_device * target)
{
    int code;
    gx_device_memory *pdev;

    if (mem == NULL)
        return -1;

    code = gs_copydevice((gx_device **)&pdev,
                         (const gx_device *)&mem_mono_device,
                         mem);
    if (code < 0)
        return code;

    set_dev_proc(pdev, get_page_device, gx_default_get_page_device);
    gx_device_set_target((gx_device_forward *)pdev, target);
    /* Should this be forwarding, monochrome profile, or not set? MJV. */
    set_dev_proc(pdev, get_profile, gx_forward_get_profile);

    gdev_mem_mono_set_inverted(pdev, true);
    check_device_separable((gx_device *)pdev);
    gx_device_fill_in_procs((gx_device *)pdev);
    *ppdev = pdev;
    return 0;
}

/* Make a monobit memory device.  This is never a page device. */
/* Note that white=0, black=1. */
void
gs_make_mem_mono_device(gx_device_memory * dev, gs_memory_t * mem,
                        gx_device * target)
{
    /* Can never fail */
    (void)gx_device_init((gx_device *)dev,
                         (const gx_device *)&mem_mono_device,
                         mem, true);
    set_dev_proc(dev, get_page_device, gx_default_get_page_device);
    gx_device_set_target((gx_device_forward *)dev, target);
    dev->raster = gx_device_raster((gx_device *)dev, 1);
    gdev_mem_mono_set_inverted(dev, true);
    check_device_separable((gx_device *)dev);
    gx_device_fill_in_procs((gx_device *)dev);
    /* Should this be forwarding, monochrome profile, or not set? MJV */
    set_dev_proc(dev, get_profile, gx_forward_get_profile);
    set_dev_proc(dev, set_graphics_type_tag, gx_forward_set_graphics_type_tag);
    set_dev_proc(dev, dev_spec_op, mem_spec_op);
    dev->owner = target;
    /* initialize to same tag as target */
    dev->graphics_type_tag = target ? target->graphics_type_tag : GS_UNKNOWN_TAG;
}

/* Define whether a monobit memory device is inverted (black=1). */
void
gdev_mem_mono_set_inverted(gx_device_memory * dev, bool black_is_1)
{
    if (black_is_1)
        dev->palette = mem_mono_b_w_palette;
    else
        dev->palette = mem_mono_w_b_palette;
}

/*
 * Compute the size of the bitmap storage, including the space for the scan
 * line pointer table.  Note that scan lines are padded to a multiple of
 * align_bitmap_mod bytes, and additional padding may be needed if the
 * pointer table must be aligned to an even larger modulus.
 *
 * The computation for planar devices is a little messier.  Each plane
 * must pad its scan lines, and then we must pad again for the pointer
 * tables (one table per plane).
 *
 * Return VMerror if the size exceeds max size_t
 */
int
gdev_mem_bits_size(const gx_device_memory * dev, int width, int height, size_t *psize)
{
    int num_planes;
    gx_render_plane_t plane1;
    const gx_render_plane_t *planes;
    size_t size, alignment = bitmap_raster_pad_align(1, dev->pad, dev->log2_align_mod);
    int pi;

    if (dev->num_planar_planes > 1) {
        num_planes = dev->num_planar_planes;
        planes = dev->planes;
    } else
        planes = &plane1, plane1.depth = dev->color_info.depth, num_planes = 1;
    for (size = 0, pi = 0; pi < num_planes; ++pi) {
        size_t raster = bitmap_raster_pad_align((size_t)width * planes[pi].depth, dev->pad, dev->log2_align_mod);
        if ((planes[pi].depth && width > (SIZE_MAX - alignment) / planes[pi].depth) || raster > SIZE_MAX - size)
            return_error(gs_error_VMerror);
        size += raster;
    }
    if (height != 0)
        if (size > (SIZE_MAX - ARCH_ALIGN_PTR_MOD) / (ulong)height)
            return_error(gs_error_VMerror);
    size = ROUND_UP(size * height, ARCH_ALIGN_PTR_MOD);
    if (dev->log2_align_mod > log2_align_bitmap_mod)
        size += 1<<dev->log2_align_mod;
    *psize = size;
    return 0;
}
size_t
gdev_mem_line_ptrs_size(const gx_device_memory * dev, int width, int height)
{
    int num_planes = 1;
    if (dev->num_planar_planes > 1)
        num_planes = dev->num_planar_planes;
    return (size_t)height * sizeof(byte *) * num_planes;
}
int
gdev_mem_data_size(const gx_device_memory * dev, int width, int height, size_t *psize)
{
    size_t bits_size;
    size_t line_ptrs_size = gdev_mem_line_ptrs_size(dev, width, height);

    if (gdev_mem_bits_size(dev, width, height, &bits_size) < 0 ||
        bits_size > SIZE_MAX - line_ptrs_size)
        return_error(gs_error_VMerror);
    *psize = bits_size + line_ptrs_size;
    return 0;
}
/*
 * Do the inverse computation: given a width (in pixels) and a buffer size,
 * compute the maximum height.
 */
int
gdev_mem_max_height(const gx_device_memory * dev, int width, size_t size,
                    bool page_uses_transparency)
{
    int height;
    size_t max_height;
    size_t data_size = 0;
    bool deep = device_is_deep((const gx_device *)dev);

    if (page_uses_transparency) {
        /*
         * If the device is using PDF 1.4 transparency then we will need to
         * also allocate image buffers for doing the blending operations.
         * We can only estimate the space requirements.  However since it
         * is only an estimate, we may exceed our desired buffer space while
         * processing the file.
         */
            /* FIXME: For a planar device, is dev->color_info.num_components 1 ? Otherwise, aren't we
             * calculating this too large? */
        max_height = size / (bitmap_raster_pad_align(width
            * dev->color_info.depth + ESTIMATED_PDF14_ROW_SPACE(width, dev->color_info.num_components, deep ? 16 : 8),
                dev->pad, dev->log2_align_mod) + sizeof(byte *) * (dev->num_planar_planes ? dev->num_planar_planes : 1));
        height = (int)min(max_height, max_int);
    } else {
        /* For non PDF 1.4 transparency, we can do an exact calculation */
        max_height = size /
            (bitmap_raster_pad_align(width * dev->color_info.depth, dev->pad, dev->log2_align_mod) +
             sizeof(byte *) * (dev->num_planar_planes ? dev->num_planar_planes : 1));
        height = (int)min(max_height, max_int);
        /*
         * Because of alignment rounding, the just-computed height might
         * be too large by a small amount.  Adjust it the easy way.
         */
        do {
            gdev_mem_data_size(dev, width, height, &data_size);
            if (data_size <= size)
                break;
            --height;
        } while (data_size > size);
    }
    return height;
}

/* Open a memory device, allocating the data area if appropriate, */
/* and create the scan line table. */
int
mem_open(gx_device * dev)
{
    gx_device_memory *const mdev = (gx_device_memory *)dev;

    /* Check that we aren't trying to open a planar device as chunky. */
    if (mdev->num_planar_planes > 1)
        return_error(gs_error_rangecheck);
    return gdev_mem_open_scan_lines(mdev, dev->height);
}
int
gdev_mem_open_scan_lines(gx_device_memory *mdev, int setup_height)
{
    return gdev_mem_open_scan_lines_interleaved(mdev, setup_height, 0);
}
int
gdev_mem_open_scan_lines_interleaved(gx_device_memory *mdev,
                                     int setup_height,
                                     int interleaved)
{
    bool line_pointers_adjacent = true;
    size_t size;

    if (setup_height < 0 || setup_height > mdev->height)
        return_error(gs_error_rangecheck);
    if (mdev->bitmap_memory != NULL) {
        int align;
        /* Allocate the data now. */
        if (gdev_mem_bitmap_size(mdev, &size) < 0)
            return_error(gs_error_VMerror);

        mdev->base = gs_alloc_bytes(mdev->bitmap_memory, size,
                                    "mem_open");
        if (mdev->base == NULL)
            return_error(gs_error_VMerror);
#ifdef PACIFY_VALGRIND
        /* If we end up writing the bitmap to the clist, we can get valgrind errors
         * because we write and read the padding at the end of each raster line.
         * Easiest to set the entire block.
         */
        memset(mdev->base, 0x00, size);
#endif
        align = 1<<mdev->log2_align_mod;
        mdev->base += (-(int)(intptr_t)mdev->base) & (align-1);
        mdev->foreign_bits = false;
    } else if (mdev->line_pointer_memory != NULL) {
        /* Allocate the line pointers now. */

        mdev->line_ptrs = (byte **)
            gs_alloc_byte_array(mdev->line_pointer_memory, mdev->height,
                                sizeof(byte *) * (mdev->num_planar_planes ? mdev->num_planar_planes : 1),
                                "gdev_mem_open_scan_lines");
        if (mdev->line_ptrs == NULL)
            return_error(gs_error_VMerror);
        mdev->foreign_line_pointers = false;
        line_pointers_adjacent = false;
    }
    if (line_pointers_adjacent) {
        int code;

        if (mdev->base == NULL)
            return_error(gs_error_rangecheck);

        code = gdev_mem_bits_size(mdev, mdev->width, mdev->height, &size);
        if (code < 0)
            return code;

        mdev->line_ptrs = (byte **)(mdev->base + size);
    }
    mdev->raster = gx_device_raster((gx_device *)mdev, 1);
    return gdev_mem_set_line_ptrs_interleaved(mdev, NULL, 0, NULL,
                                              setup_height,
                                              interleaved);
}
/*
 * Set up the scan line pointers of a memory device.
 * See gxdevmem.h for the detailed specification.
 * Sets or uses line_ptrs, base, raster; uses width, color_info.depth,
 * num_planes, plane_depths, plane_depth.
 */
int
gdev_mem_set_line_ptrs(gx_device_memory *mdev, byte *base, int raster,
                       byte **line_ptrs, int setup_height)
{
    return gdev_mem_set_line_ptrs_interleaved(mdev, base, raster, line_ptrs, setup_height, 0);
}
int
gdev_mem_set_line_ptrs_interleaved(gx_device_memory * mdev, byte * base,
                                   int raster, byte **line_ptrs,
                                   int setup_height, int interleaved)
{
    int num_planes = (mdev->num_planar_planes ? mdev->num_planar_planes : 0);
    byte **pline;
    byte *data;
    int pi;
    int plane_raster;

    /* If we are supplied with line_ptrs, then assume that we don't have
     * any already, and take them on. */
    if (line_ptrs)
        mdev->line_ptrs = line_ptrs;
    pline = mdev->line_ptrs;

    /* If we are supplied a base, then we are supplied a raster. Assume that
     * we don't have any buffer already, and take these on. Assume that the
     * base has been allocated with sufficient padding to allow for any
     * alignment required. */
    if (base == NULL) {
        base = mdev->base;
        raster = mdev->raster;
    } else {
        mdev->base = base;
        mdev->raster = raster;
    }

    /* Now, pad and align as required. */
    if (mdev->log2_align_mod > log2_align_bitmap_mod) {
        int align = 1<<mdev->log2_align_mod;
        align = (-(int)(intptr_t)base) & (align-1);
        data = base + align;
    } else {
        data = mdev->base;
    }

    if (num_planes) {
        if (base && !mdev->plane_depth)
            return_error(gs_error_rangecheck);
    } else {
        num_planes = 1;
    }

    if (interleaved)
        plane_raster = raster, raster *= num_planes;
    else
        plane_raster = raster * mdev->height;
    for (pi = 0; pi < num_planes; ++pi) {
        byte **pptr = pline;
        byte **pend = pptr + setup_height;
        byte *scan_line = data;

        while (pptr < pend) {
            *pptr++ = scan_line;
            scan_line += raster;
        }
        data += plane_raster;
        pline += setup_height;	/* not mdev->height, see gxdevmem.h */
    }

    return 0;
}

/* Return the initial transformation matrix */
void
mem_get_initial_matrix(gx_device * dev, gs_matrix * pmat)
{
    gx_device_memory * const mdev = (gx_device_memory *)dev;

    pmat->xx = mdev->initial_matrix.xx;
    pmat->xy = mdev->initial_matrix.xy;
    pmat->yx = mdev->initial_matrix.yx;
    pmat->yy = mdev->initial_matrix.yy;
    pmat->tx = mdev->initial_matrix.tx;
    pmat->ty = mdev->initial_matrix.ty;
}

/* Close a memory device, freeing the data area if appropriate. */
int
mem_close(gx_device * dev)
{
    gx_device_memory * const mdev = (gx_device_memory *)dev;

    if (mdev->bitmap_memory != 0) {
        gs_free_object(mdev->bitmap_memory, mdev->base, "mem_close");
        /*
         * The following assignment is strictly for the benefit of one
         * client that is sloppy about using is_open properly.
         */
        mdev->base = 0;
    } else if (mdev->line_pointer_memory != 0) {
        gs_free_object(mdev->line_pointer_memory, mdev->line_ptrs,
                       "mem_close");
        mdev->line_ptrs = 0;	/* ibid. */
    }
    return 0;
}

/* Memory devices shouldn't allow their dimensions to change */
static int
mem_put_params(gx_device * dev, gs_param_list * plist)
{
    int code;
    int width = dev->width, height = dev->height;
    float xres = dev->HWResolution[0], yres = dev->HWResolution[1];
    float medw = dev->MediaSize[0], medh = dev->MediaSize[1];

    code = gx_default_put_params(dev, plist);

    dev->width = width;
    dev->height = height;
    dev->HWResolution[0] = xres;
    dev->HWResolution[1] = yres;
    dev->MediaSize[0] = medw;
    dev->MediaSize[1] = medh;

    return code;
}

/* Copy bits to a client. */
#undef chunk
#define chunk byte
int
mem_get_bits_rectangle(gx_device * dev, const gs_int_rect * prect,
                       gs_get_bits_params_t * params)
{
    gx_device_memory * const mdev = (gx_device_memory *)dev;
    gs_get_bits_options_t options = params->options;
    int x = prect->p.x, w = prect->q.x - x, y = prect->p.y, h = prect->q.y - y;

    if (options == 0) {
        params->options =
            (GB_ALIGN_STANDARD | GB_ALIGN_ANY) |
            (GB_RETURN_COPY | GB_RETURN_POINTER) |
            (GB_OFFSET_0 | GB_OFFSET_SPECIFIED | GB_OFFSET_ANY) |
            (GB_RASTER_STANDARD | GB_RASTER_SPECIFIED | GB_RASTER_ANY) |
            GB_PACKING_CHUNKY | GB_COLORS_NATIVE | GB_ALPHA_NONE;
        return_error(gs_error_rangecheck);
    }
    if (mdev->line_ptrs == NULL)
        return_error(gs_error_rangecheck);
    if ((w <= 0) | (h <= 0)) {
        if ((w | h) < 0)
            return_error(gs_error_rangecheck);
        return 0;
    }
    if (x < 0 || w > dev->width - x ||
        y < 0 || h > dev->height - y
        )
        return_error(gs_error_rangecheck);
    {
        gs_get_bits_params_t copy_params;
        byte **base = &scan_line_base(mdev, y);
        int code;

        copy_params.options =
            GB_COLORS_NATIVE | GB_PACKING_CHUNKY | GB_ALPHA_NONE |
            (mdev->raster ==
             bitmap_raster(mdev->width * mdev->color_info.depth) ?
             GB_RASTER_STANDARD : GB_RASTER_SPECIFIED);
        copy_params.raster = mdev->raster;
        code = gx_get_bits_return_pointer(dev, x, h, params,
                                          &copy_params, base);
        if (code >= 0)
            return code;
        return gx_get_bits_copy(dev, x, w, h, params, &copy_params, *base,
                                gx_device_raster(dev, true));
    }
}

#if !ARCH_IS_BIG_ENDIAN

/*
 * Swap byte order in a rectangular subset of a bitmap.  If store = true,
 * assume the rectangle will be overwritten, so don't swap any bytes where
 * it doesn't matter.  The caller has already done a fit_fill or fit_copy.
 * Note that the coordinates are specified in bits, not in terms of the
 * actual device depth.
 */
void
mem_swap_byte_rect(byte * base, size_t raster, int x, int w, int h, bool store)
{
    int xbit = x & 31;

    if (store) {
        if (xbit + w > 64) {	/* Operation spans multiple words. */
            /* Just swap the words at the left and right edges. */
            if (xbit != 0)
                mem_swap_byte_rect(base, raster, x, 1, h, false);
            x += w - 1;
            xbit = x & 31;
            if (xbit == 31)
                return;
            w = 1;
        }
    }
    /* Swap the entire rectangle (or what's left of it). */
    {
        byte *row = base + ((x >> 5) << 2);
        int nw = (xbit + w + 31) >> 5;
        int ny;

        for (ny = h; ny > 0; row += raster, --ny) {
            int nx = nw;
            bits32 *pw = (bits32 *) row;

            do {
                bits32 v = *pw;

                *pw++ = (v >> 24) + ((v >> 8) & 0xff00) +
                    ((v & 0xff00) << 8) + (v << 24);
            }
            while (--nx);
        }
    }
}

/* Copy a word-oriented rectangle to the client, swapping bytes as needed. */
int
mem_word_get_bits_rectangle(gx_device * dev, const gs_int_rect * prect,
                       gs_get_bits_params_t * params)
{
    gx_device_memory * const mdev = (gx_device_memory *)dev;
    byte *src;
    size_t dev_raster = gx_device_raster(dev, 1);
    int x = prect->p.x;
    int w = prect->q.x - x;
    int y = prect->p.y;
    int h = prect->q.y - y;
    int bit_x, bit_w;
    int code;

    fit_fill_xywh(dev, x, y, w, h);
    if (w <= 0 || h <= 0) {
        /*
         * It's easiest to just keep going with an empty rectangle.
         * We pass the original rectangle to mem_get_bits_rectangle.
         */
        x = y = w = h = 0;
    }
    bit_x = x * dev->color_info.depth;
    bit_w = w * dev->color_info.depth;

    if(mdev->line_ptrs == NULL)
        return_error(gs_error_rangecheck);

    src = scan_line_base(mdev, y);
    mem_swap_byte_rect(src, dev_raster, bit_x, bit_w, h, false);
    code = mem_get_bits_rectangle(dev, prect, params);
    mem_swap_byte_rect(src, dev_raster, bit_x, bit_w, h, false);
    return code;
}

#endif /* !ARCH_IS_BIG_ENDIAN */

/* Map a r-g-b color to a color index for a mapped color memory device */
/* (2, 4, or 8 bits per pixel.) */
/* This requires searching the palette. */
gx_color_index
mem_mapped_map_rgb_color(gx_device * dev, const gx_color_value cv[])
{
    gx_device_memory * const mdev = (gx_device_memory *)dev;
    byte br = gx_color_value_to_byte(cv[0]);

    register const byte *pptr = mdev->palette.data;
    int cnt = mdev->palette.size;
    const byte *which = 0;	/* initialized only to pacify gcc */
    int best = 256 * 3;

    if (mdev->color_info.num_components != 1) {
        /* not 1 component, assume three */
        /* The comparison is rather simplistic, treating differences in	*/
        /* all components as equal. Better choices would be 'distance'	*/
        /* in HLS space or other, but these would be much slower.	*/
        /* At least exact matches will be found.			*/
        byte bg = gx_color_value_to_byte(cv[1]);
        byte bb = gx_color_value_to_byte(cv[2]);

        while ((cnt -= 3) >= 0) {
            register int diff = *pptr - br;

            if (diff < 0)
                diff = -diff;
            if (diff < best) {	/* quick rejection */
                    int dg = pptr[1] - bg;

                if (dg < 0)
                    dg = -dg;
                if ((diff += dg) < best) {	/* quick rejection */
                    int db = pptr[2] - bb;

                    if (db < 0)
                        db = -db;
                    if ((diff += db) < best)
                        which = pptr, best = diff;
                }
            }
            if (diff == 0)	/* can't get any better than 0 diff */
                break;
            pptr += 3;
        }
    } else {
        /* Gray scale conversion. The palette is made of three equal	*/
        /* components, so this case is simpler.				*/
        while ((cnt -= 3) >= 0) {
            register int diff = *pptr - br;

            if (diff < 0)
                diff = -diff;
            if (diff < best) {	/* quick rejection */
                which = pptr, best = diff;
            }
            if (diff == 0)
                break;
            pptr += 3;
        }
    }
    return (gx_color_index) ((which - mdev->palette.data) / 3);
}

/* Map a color index to a r-g-b color for a mapped color memory device. */
int
mem_mapped_map_color_rgb(gx_device * dev, gx_color_index color,
                         gx_color_value prgb[3])
{
    gx_device_memory * const mdev = (gx_device_memory *)dev;
    const byte *pptr = mdev->palette.data;

    if (pptr == NULL) {
        color = color * gx_max_color_value / ((1<<mdev->color_info.depth)-1);
        prgb[0] = color;
        prgb[1] = color;
        prgb[2] = color;
    } else {
        pptr += (int)color * 3;

        prgb[0] = gx_color_value_from_byte(pptr[0]);
        prgb[1] = gx_color_value_from_byte(pptr[1]);
        prgb[2] = gx_color_value_from_byte(pptr[2]);
    }
    return 0;
}

/*
 * Implement draw_thin_line using a distinguished procedure that serves
 * as the common marker for all memory devices.
 */
int
mem_draw_thin_line(gx_device *dev, fixed fx0, fixed fy0, fixed fx1, fixed fy1,
                   const gx_drawing_color *pdcolor,
                   gs_logical_operation_t lop,
                   fixed adjustx, fixed adjusty)
{
    return gx_default_draw_thin_line(dev, fx0, fy0, fx1, fy1, pdcolor, lop,
                                     adjustx, adjusty);
}

void mem_initialize_device_procs(gx_device *dev)
{
    set_dev_proc(dev, get_initial_matrix, mem_get_initial_matrix);
    set_dev_proc(dev, sync_output, gx_default_sync_output);
    set_dev_proc(dev, output_page, gx_default_output_page);
    set_dev_proc(dev, close_device, mem_close);
    set_dev_proc(dev, get_params, gx_default_get_params);
    set_dev_proc(dev, put_params, mem_put_params);
    set_dev_proc(dev, get_page_device, gx_forward_get_page_device);
    set_dev_proc(dev, get_alpha_bits, gx_default_get_alpha_bits);
    set_dev_proc(dev, fill_path, gx_default_fill_path);
    set_dev_proc(dev, stroke_path, gx_default_stroke_path);
    set_dev_proc(dev, fill_mask, gx_default_fill_mask);
    set_dev_proc(dev, fill_trapezoid, gx_default_fill_trapezoid);
    set_dev_proc(dev, fill_parallelogram, gx_default_fill_parallelogram);
    set_dev_proc(dev, fill_triangle, gx_default_fill_triangle);
    set_dev_proc(dev, draw_thin_line, mem_draw_thin_line);
    set_dev_proc(dev, get_clipping_box, gx_default_get_clipping_box);
    set_dev_proc(dev, begin_typed_image, gx_default_begin_typed_image);
    set_dev_proc(dev, composite, gx_default_composite);
    set_dev_proc(dev, get_hardware_params, gx_default_get_hardware_params);
    set_dev_proc(dev, text_begin, gx_default_text_begin);
    set_dev_proc(dev, transform_pixel_region, mem_transform_pixel_region);

    /* Defaults that may well get overridden. */
    set_dev_proc(dev, open_device, mem_open);
    set_dev_proc(dev, copy_alpha, gx_default_copy_alpha);
    set_dev_proc(dev, map_cmyk_color, gx_default_map_cmyk_color);
    set_dev_proc(dev, strip_tile_rectangle, gx_default_strip_tile_rectangle);
    set_dev_proc(dev, get_bits_rectangle, mem_get_bits_rectangle);
}

void mem_dev_initialize_device_procs(gx_device *dev)
{
    int depth = dev->color_info.depth;
    const gdev_mem_functions *fns;

    if (dev->num_planar_planes > 1)
        depth /= dev->num_planar_planes;
    fns = gdev_mem_functions_for_bits(depth);

    mem_initialize_device_procs(dev);

    set_dev_proc(dev, map_rgb_color, fns->map_rgb_color);
    set_dev_proc(dev, map_color_rgb, fns->map_color_rgb);
    set_dev_proc(dev, fill_rectangle, fns->fill_rectangle);
    set_dev_proc(dev, copy_mono, fns->copy_mono);
    set_dev_proc(dev, copy_color, fns->copy_color);
    set_dev_proc(dev, copy_alpha, fns->copy_alpha);
    set_dev_proc(dev, strip_copy_rop2, fns->strip_copy_rop2);
    set_dev_proc(dev, strip_tile_rectangle, fns->strip_tile_rectangle);
}

void mem_word_dev_initialize_device_procs(gx_device *dev)
{
    const gdev_mem_functions *fns =
                gdev_mem_word_functions_for_bits(dev->color_info.depth);

    mem_initialize_device_procs(dev);

    set_dev_proc(dev, map_rgb_color, fns->map_rgb_color);
    set_dev_proc(dev, map_color_rgb, fns->map_color_rgb);
    set_dev_proc(dev, fill_rectangle, fns->fill_rectangle);
    set_dev_proc(dev, copy_mono, fns->copy_mono);
    set_dev_proc(dev, copy_color, fns->copy_color);
    set_dev_proc(dev, copy_alpha, fns->copy_alpha);
    set_dev_proc(dev, strip_copy_rop2, fns->strip_copy_rop2);
    set_dev_proc(dev, strip_tile_rectangle, fns->strip_tile_rectangle);
}
