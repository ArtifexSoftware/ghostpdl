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


/* Structure and procedures for memory devices */
/* Requires gxdevice.h */

#ifndef gxdevmem_INCLUDED
#  define gxdevmem_INCLUDED

#include "gxdevcli.h"
#include "gxrplane.h"

/*
 * A 'memory' device is essentially a stored bitmap.
 * There are several different kinds: 1-bit black and white,
 * 2-, 4-, and 8-bit mapped color, 16- and 24-bit RGB color,
 * and 32-bit CMYK color.  (16-bit uses 5/6/5 bits per color.)
 * All use the same structure, since it's so awkward to get the effect of
 * subclasses in C.
 *
 * Memory devices come in two flavors: standard, which always stores bytes
 * big-endian, and word-oriented, which stores bytes in the machine order
 * within 32-bit "words".  The source data for copy_mono and
 * copy_color must be in big-endian order, and since memory devices
 * also are guaranteed to allocate the bitmap consecutively,
 * the bitmap of a standard memory device can serve directly as input
 * to copy_mono or copy_color operations.  This is not true of word-oriented
 * memory devices, which are provided only in response to a request by
 * a customer with their own image processing library that uses this format.
 *
 * In addition to the device structure itself, memory devices require two
 * other pieces of storage: the bitmap, and a table of pointers to the scan
 * lines of the bitmap.  Clients have several options for allocating these:
 *
 *	1) Set bitmap_memory to an allocator before opening the device.
 *	With this option, opening the device allocates the bitmap and the
 *	line pointer table (contiguously), and closing the device frees
 *	them.
 *
 *	2) Set line_pointer_memory to an allocator, base to the base address
 *	of the bitmap, and raster to the length of each scan line (distance
 *	from one scan line to the next) before opening the device.  With
 *	this option, opening the device allocates the line table, but not
 *	the bitmap; closing the device frees the table.
 *
 *	3) Set line_pointer_memory but not base or raster.  Opening /
 *	closing the device will allocate / free the line pointer table, but
 *	the client must set the pointers with a subsequent call of
 *	gdev_mem_set_line_ptrs.
 *
 *	4) Set neither _memory field.  In this case, it's up to the client
 *	to call gdev_mem_set_line_ptrs and to manage storage for the
 *	line pointers and the bitmap.
 *
 * In cases (2) through (4), it is the client's responsibility to set
 * foreign_bits (and foreign_line_pointers, if the line pointers are not
 * contiguous with the bits) to tell the GC whether to trace the pointers.
 * By default, anything allocated by bitmap_memory or line_pointer_memory is
 * assumed GC'able (i.e., case (1) assumes that the bits + line pointers are
 * GC'able, and cases (2) and (3) assume that the line pointers are GC'able,
 * but not the bits), but the client can change the foreign_* flag(s) after
 * opening the device if this is not the case.
 */

struct gx_device_memory_s {
    gx_device_forward_common;	/* (see gxdevice.h) */
    /*
     * The following may be set by the client before or just after
     * opening the device.  See above.
     */
    uint raster;		/* bytes per scan line */
    byte *base;
#define scan_line_base(dev,y) ((dev)->line_ptrs[y])
    gs_memory_t *bitmap_memory;	/* allocator for bits + line pointers */
    bool foreign_bits;		/* if true, bits are not in GC-able space */
    gs_memory_t *line_pointer_memory;  /* allocate for line pointers */
    bool foreign_line_pointers;  /* if true, line_ptrs are not in GC-able space */
    /*
     * The following are only used for planar devices.  num_planes == 0
     * means this is a chunky device.  Note that for planar devices, we
     * require color_info.depth = the sum of the individual plane depths.
     */
    gx_render_plane_t planes[GX_DEVICE_COLOR_MAX_COMPONENTS];
    /*
     * End of client-initializable fields.
     */
    gs_matrix initial_matrix;	/* the initial transformation */
    byte **line_ptrs;		/* scan line pointers */
    /* Following is used for mapped color, */
    /* including 1-bit devices (to specify polarity). */
    gs_const_string palette;	/* RGB triples */
    /* Following is only used for 24-bit color. */
    struct _c24 {
        gx_color_index rgb;	/* cache key */
        bits32 rgbr, gbrg, brgb;	/* cache value */
    } color24;
    /* Following is only used for 40-bit color. */
    struct _c40 {
        gx_color_index abcde;	/* cache key */
        bits32 abcd, bcde, cdea, deab, eabc;	/* cache value */
    } color40;
    /* Following is only used for 48-bit color. */
    struct _c48 {
        gx_color_index abcdef;	/* cache key */
        bits32 abcd, cdef, efab;	/* cache value */
    } color48;
    /* Following is only used for 56-bit color. */
    struct _c56 {
        gx_color_index abcdefg;	/* cache key */
        bits32 abcd, bcde, cdef, defg, efga, fgab, gabc;	/* cache value */
    } color56;
    /* Following is only used for 64-bit color. */
    struct _c64 {
        gx_color_index abcdefgh;	/* cache key */
        bits32 abcd, efgh;	/* cache value */
    } color64;
    /* The following is used to indicate the device that 'owns' this one as
     * a buffer device. Any dev_spec_ops that come to us should be forwarded
     * up to that one if we can't cope with them. */
    gx_device *owner;
    /* Following are only used for alpha buffers. */
    /* The client initializes those marked with $; */
    /* they don't change after initialization. */
    gs_log2_scale_point log2_scale;	/* $ oversampling scale factors */
    int log2_alpha_bits;	/* $ log2 of # of alpha bits being produced */
    int mapped_x;		/* $ X value mapped to buffer X=0 */
    int mapped_y;		/* lowest Y value mapped to buffer */
    int mapped_height;		/* # of Y values mapped to buffer */
    int mapped_start;		/* local Y value corresponding to mapped_y */
    gx_color_index save_color;	/* last (only) color displayed */
    const gx_drawing_color *save_hl_color;
                                /* last (only) hl color displayed */
    /* Following are used only for planar devices. */
    int plane_depth;		/* if non-zero, depth of all planes */
    int band_y; /* Debug purpose only. */
};

extern_st(st_device_memory);
#define public_st_device_memory() /* in gdevmem.c */\
  gs_public_st_composite_use_final(st_device_memory, gx_device_memory,\
    "gx_device_memory", device_memory_enum_ptrs, device_memory_reloc_ptrs,\
    gx_device_finalize)
#define st_device_memory_max_ptrs (st_device_forward_max_ptrs + 2)
#define mem_device_init_private\
        0,			/* raster */\
        (byte *)0,		/* base */\
        0,			/* bitmap_memory */\
        true,			/* foreign_bits (default) */\
        0,			/* line_pointer_memory */\
        true,			/* foreign_line_pointers (default) */\
        { { 0 } },		/* planes (only used for planar) */\
        { identity_matrix_body },	/* initial matrix (filled in) */\
        (byte **)0,		/* line_ptrs (filled in by mem_open) */\
        { (byte *)0, 0 },	/* palette (filled in for color) */\
        { gx_no_color_index },	/* color24 */\
        { gx_no_color_index },	/* color40 */\
        { gx_no_color_index },	/* color48 */\
        { gx_no_color_index },	/* color56 */\
        { gx_no_color_index },	/* color64 */\
        0,			/* owner */\
        { 0, 0 }, 0,		/* scale, log2_alpha_bits */\
        0, 0, 0, 0,		/* mapped_* */\
        gx_no_color_index	/* save_color */

/*
 * Memory devices may have special setup requirements.  In particular, it
 * may not be obvious how much space to allocate for the bitmap.  Here is
 * the routine that computes this from the width and height.  Note that this
 * size includes both the bitmap and the line pointers.
 */
/* bits only */
int gdev_mem_bits_size(const gx_device_memory *mdev, int width,
                         int height, size_t *size);
/* line pointers only */
size_t gdev_mem_line_ptrs_size(const gx_device_memory *mdev, int width,
                              int height);
/* bits + line pointers */
int gdev_mem_data_size(const gx_device_memory *mdev, int width,
                         int height, size_t *size);

#define gdev_mem_bitmap_size(mdev, size)\
  gdev_mem_data_size(mdev, (mdev)->width, (mdev)->height, size)

/*
 * Do the inverse computation: given the device width and a buffer size,
 * compute the maximum height.
 */
int gdev_mem_max_height(const gx_device_memory * dev, int width, size_t size,
                bool page_uses_transparency);

/*
 * Compute the standard raster (data bytes per line) similarly.
 */
#define gdev_mem_raster(mdev)\
  gx_device_raster((const gx_device *)(mdev), true)

typedef struct
{
    dev_proc_map_rgb_color((*map_rgb_color));
    dev_proc_map_color_rgb((*map_color_rgb));
    dev_proc_fill_rectangle((*fill_rectangle));
    dev_proc_copy_mono((*copy_mono));
    dev_proc_copy_color((*copy_color));
    dev_proc_copy_alpha((*copy_alpha));
    dev_proc_strip_tile_rectangle((*strip_tile_rectangle));
    dev_proc_strip_copy_rop2((*strip_copy_rop2));
    dev_proc_get_bits_rectangle((*get_bits_rectangle));
} gdev_mem_functions;

extern const gdev_mem_functions gdev_mem_fns_1;
extern const gdev_mem_functions gdev_mem_fns_2;
extern const gdev_mem_functions gdev_mem_fns_4;
extern const gdev_mem_functions gdev_mem_fns_8;
extern const gdev_mem_functions gdev_mem_fns_16;
extern const gdev_mem_functions gdev_mem_fns_24;
extern const gdev_mem_functions gdev_mem_fns_32;
extern const gdev_mem_functions gdev_mem_fns_40;
extern const gdev_mem_functions gdev_mem_fns_48;
extern const gdev_mem_functions gdev_mem_fns_56;
extern const gdev_mem_functions gdev_mem_fns_64;
#if ARCH_IS_BIG_ENDIAN
#define gdev_mem_fns_1w  gdev_mem_fns_1
#define gdev_mem_fns_2w  gdev_mem_fns_2
#define gdev_mem_fns_4w  gdev_mem_fns_4
#define gdev_mem_fns_8w  gdev_mem_fns_8
#define gdev_mem_fns_24w gdev_mem_fns_24
#define gdev_mem_fns_32w gdev_mem_fns_32
#define gdev_mem_fns_40w gdev_mem_fns_40
#define gdev_mem_fns_48w gdev_mem_fns_48
#define gdev_mem_fns_56w gdev_mem_fns_56
#define gdev_mem_fns_64w gdev_mem_fns_64
#else
extern const gdev_mem_functions gdev_mem_fns_1w;
extern const gdev_mem_functions gdev_mem_fns_2w;
extern const gdev_mem_functions gdev_mem_fns_4w;
extern const gdev_mem_functions gdev_mem_fns_8w;
extern const gdev_mem_functions gdev_mem_fns_24w;
extern const gdev_mem_functions gdev_mem_fns_32w;
extern const gdev_mem_functions gdev_mem_fns_40w;
extern const gdev_mem_functions gdev_mem_fns_48w;
extern const gdev_mem_functions gdev_mem_fns_56w;
extern const gdev_mem_functions gdev_mem_fns_64w;
#endif
const gdev_mem_functions *gdev_mem_functions_for_bits(int bits);
const gdev_mem_functions *gdev_mem_word_functions_for_bits(int bits);

/* Determine the appropriate memory device for a given */
/* number of bits per pixel (0 if none suitable). */
const gx_device_memory *gdev_mem_device_for_bits(int);

/* Determine the word-oriented memory device for a given depth. */
const gx_device_memory *gdev_mem_word_device_for_bits(int);

/*
 * Make a memory device.  The following 4 procedures will be
 * deprecated, use gs_make_mem_*_copydevice() below, for future
 * changes.
 */

/* mem is 0 if the device is temporary and local, */
/* or the allocator that was used to allocate it if it is a real object. */
/* page_device is 1 if the device should be a page device, */
/* 0 if it should propagate this property from its target, or */
/* -1 if it should not be a page device. */
void gs_make_mem_mono_device(gx_device_memory * mdev, gs_memory_t * mem,
                             gx_device * target);
void gs_make_mem_device(gx_device_memory * mdev,
                        const gx_device_memory * mdproto,
                        gs_memory_t * mem, int page_device,
                        gx_device * target);
void gs_make_mem_abuf_device(gx_device_memory * adev, gs_memory_t * mem,
                             gx_device * target,
                             const gs_log2_scale_point * pscale,
                             int alpha_bits, int mapped_x, bool devn);

/*
 * Create memory devices with copydevice.  For now the destructor is
 * simply: gx_device_retain(mdev, false).
 */

int gs_make_mem_mono_device_with_copydevice(gx_device_memory ** mdev,
                                            gs_memory_t * mem,
                                            gx_device * target);

int gs_make_mem_device_with_copydevice(gx_device_memory ** mdev,
                                       const gx_device_memory * mdproto,
                                       gs_memory_t * mem,
                                       int page_device,
                                       gx_device * target);

/*
 * TODO replace gs_make_mem_abuf_device with
 * procedure that uses copydevice.
 */

/*
 * Open a memory device, only setting line pointers to a subset of its
 * scan lines.  Banding devices use this (see gxclread.c).
 */
int gdev_mem_open_scan_lines(gx_device_memory *mdev, int setup_height);
int gdev_mem_open_scan_lines_interleaved(gx_device_memory *mdev,
                                         int setup_height,
                                         int interleaved);

/*
 * Initialize the line pointers of a memory device.  base and/or line_ptrs
 * may be NULL, in which case the value already stored in the device is
 * used; if base is NULL, raster is also ignored and the existing value is
 * used.  Note that this takes raster and setup_height arguments.
 * If the base is not NULL and the device is planar, all planes must have
 * the same depth, since otherwise a single raster value is not sufficient.
 *
 * Note that setup_height may be less than height.  In this case, for
 * planar devices, only setup_height * num_planes line pointers are set,
 * in the expectation that the device's height will be reset to
 * setup_height.
 */
int gdev_mem_set_line_ptrs(gx_device_memory *mdev,
                           byte *base, int raster, byte **line_ptrs,
                           int setup_height);
int
gdev_mem_set_line_ptrs_interleaved(gx_device_memory *mdev,
                                   byte *base, int raster,
                                   byte **line_ptrs, int setup_height,
                                   int interleaved);

/* Define whether a monobit memory device is inverted (black=1). */
void gdev_mem_mono_set_inverted(gx_device_memory * mdev, bool black_is_1);

/* Test whether a device is a memory device. */
bool gs_device_is_memory(const gx_device *);

/* Test whether a device is an alpha-buffering device. */
bool gs_device_is_abuf(const gx_device *);

/* Check for getting the antialiasing bit depth */
int alpha_buffer_bits(gs_gstate * pgs);

/* Dev spec op handler for memory devices. */
dev_proc_dev_spec_op(mem_spec_op);


#endif /* gxdevmem_INCLUDED */
