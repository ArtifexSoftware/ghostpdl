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

/* Private definitions for memory devices. */

#ifndef gdevmem_INCLUDED
#  define gdevmem_INCLUDED

#include "gxbitops.h"
#include "gxdevcli.h"

/*
   The representation for a "memory" device is simply a
   contiguous bitmap stored in something like the PostScript
   representation, i.e., each scan line (in left-to-right order), padded
   to a multiple of bitmap_align_mod bytes, followed immediately by
   the next one.

   The representation of strings in the interpreter limits
   the size of a string to 64K-1 bytes, which means we can't simply use
   a string for the contents of a memory device.
   We get around this problem by making the client read out the
   contents of a memory device bitmap in pieces.

   On 80x86 PCs running in 16-bit mode, there may be no way to
   obtain a contiguous block of storage larger than 64K bytes,
   which typically isn't big enough for a full-screen bitmap.
   We take the following compromise position: if the PC is running in
   native mode (pseudo-segmenting), we limit the bitmap to 64K;
   if the PC is running in protected mode (e.g., under MS Windows),
   we assume that blocks larger than 64K have sequential segment numbers,
   and that the client arranges things so that an individual scan line,
   the scan line pointer table, and any single call on a drawing routine
   do not cross a segment boundary.

   Even though the scan lines are stored contiguously, we store a table
   of their base addresses, because indexing into it is faster than
   the multiplication that would otherwise be needed.
 */

/*
 * Macros for scan line access.
 * x_to_byte is different for each number of bits per pixel.
 * Note that these macros depend on the definition of chunk:
 * each procedure that uses the scanning macros should #define
 * (not typedef) chunk as either uint or byte.
 */
#define declare_scan_ptr(ptr)\
        DECLARE_SCAN_PTR_VARS(ptr, chunk *, draster)
#define DECLARE_SCAN_PTR_VARS(ptr, ptype, draster)\
        register ptype ptr;\
        uint draster
#define setup_rect(ptr)\
        SETUP_RECT_VARS(ptr, chunk *, draster)
#define SETUP_RECT_VARS(ptr, ptype, draster)\
        draster = mdev->raster;\
        ptr = (ptype)(scan_line_base(mdev, y) +\
          (x_to_byte(x) & -chunk_align_bytes))

/* ------ Generic macros ------ */

/* Macro for declaring the essential device procedures. */
dev_proc_get_initial_matrix(mem_get_initial_matrix);
dev_proc_close_device(mem_close);
#define declare_mem_map_procs(map_rgb_color, map_color_rgb)\
  static dev_proc_map_rgb_color(map_rgb_color);\
  static dev_proc_map_color_rgb(map_color_rgb)
#define declare_mem_procs(copy_mono, copy_color, fill_rectangle)\
  static dev_proc_copy_mono(copy_mono);\
  static dev_proc_copy_color(copy_color);\
  static dev_proc_fill_rectangle(fill_rectangle)
/*
 * We define one relatively low-usage drawing procedure that is common to
 * all memory devices so that we have a reliable way to implement
 * gs_is_memory_device.  It is equivalent to gx_default_draw_thin_line.
 */
dev_proc_draw_thin_line(mem_draw_thin_line);

/* The following are used for all except planar or word-oriented devices. */
dev_proc_open_device(mem_open);
dev_proc_get_bits_rectangle(mem_get_bits_rectangle);
/* The following are for word-oriented devices. */
#if ARCH_IS_BIG_ENDIAN
#  define mem_word_get_bits_rectangle mem_get_bits_rectangle
#else
dev_proc_get_bits_rectangle(mem_word_get_bits_rectangle);
#endif
/* The following are used for the non-true-color devices. */
dev_proc_map_rgb_color(mem_mapped_map_rgb_color);
dev_proc_map_color_rgb(mem_mapped_map_color_rgb);
/* Default implementations */
dev_proc_strip_copy_rop2(mem_default_strip_copy_rop2);
dev_proc_transform_pixel_region(mem_transform_pixel_region);

/*
 * Macro for generating the device descriptor.
 * Various compilers have problems with the obvious definition
 * for max_value, namely:
 *      (depth >= 8 ? 255 : (1 << depth) - 1)
 * I tried changing (1 << depth) to (1 << (depth & 15)) to forestall bogus
 * error messages about invalid shift counts, but the H-P compiler chokes
 * on this.  Since the only values of depth we ever plan to support are
 * powers of 2 (and 24), we just go ahead and enumerate them.
 */
#define max_value_gray(rgb_depth, gray_depth)\
  (gray_depth ? (1 << gray_depth) - 1 : max_value_rgb(rgb_depth, 0))
#define max_value_rgb(rgb_depth, gray_depth)\
  (rgb_depth >= 8 ? 255 : rgb_depth == 4 ? 15 : rgb_depth == 2 ? 3 :\
   rgb_depth == 1 ? 1 : (1 << gray_depth) - 1)

void mem_initialize_device_procs(gx_device *dev);
void mem_dev_initialize_device_procs(gx_device *dev);
void mem_word_dev_initialize_device_procs(gx_device *dev);

#define mem_device(name, rgb_depth, gray_depth, initialize)\
{	std_device_dci_body(gx_device_memory, initialize, name,\
          0, 0, 72, 72,\
          (rgb_depth ? 3 : 0) + (gray_depth ? 1 : 0),	/* num_components */\
          rgb_depth + gray_depth,	/* depth */\
          max_value_gray(rgb_depth, gray_depth),	/* max_gray */\
          max_value_rgb(rgb_depth, gray_depth),	/* max_color */\
          max_value_gray(rgb_depth, gray_depth) + 1, /* dither_grays */\
          max_value_rgb(rgb_depth, gray_depth) + 1 /* dither_colors */\
        ),\
        { 0 },\
        0,			/* target */\
        mem_device_init_private	/* see gxdevmem.h */\
}

/* Swap a rectangle of bytes, for converting between word- and */
/* byte-oriented representation. */
void mem_swap_byte_rect(byte *, size_t, int, int, int, bool);

/* Copy a rectangle of bytes from a source to a destination. */
#define mem_copy_byte_rect(mdev, base, sourcex, sraster, x, y, w, h)\
  bytes_copy_rectangle(scan_line_base(mdev, y) + x_to_byte(x),\
                       (mdev)->raster,\
                       base + x_to_byte(sourcex), sraster,\
                       x_to_byte(w), h)

/* ------ Implementations ------ */

extern const gx_device_memory mem_mono_device;
extern const gx_device_memory mem_mapped2_device;
extern const gx_device_memory mem_mapped4_device;
extern const gx_device_memory mem_mapped8_device;
extern const gx_device_memory mem_true16_device;
extern const gx_device_memory mem_true24_device;
extern const gx_device_memory mem_true32_device;
extern const gx_device_memory mem_true40_device;
extern const gx_device_memory mem_true48_device;
extern const gx_device_memory mem_true56_device;
extern const gx_device_memory mem_true64_device;
extern const gx_device_memory mem_x_device;
extern const gx_device_memory mem_planar_device;
/*
 * We declare the RasterOp implementation procedures here because they are
 * referenced in several implementation modules.
 */
dev_proc_strip_copy_rop2(mem_mono_strip_copy_rop2);
dev_proc_strip_copy_rop2(mem_mono_strip_copy_rop2_dev);
dev_proc_strip_copy_rop2(mem_gray_strip_copy_rop2);
dev_proc_strip_copy_rop2(mem_gray8_rgb24_strip_copy_rop2);
dev_proc_copy_mono(mem_mono_copy_mono);
dev_proc_fill_rectangle(mem_mono_fill_rectangle);

#if ARCH_IS_BIG_ENDIAN
#  define mem_mono_word_device mem_mono_device
#  define mem_mapped2_word_device mem_mapped2_device
#  define mem_mapped4_word_device mem_mapped4_device
#  define mem_mapped8_word_device mem_mapped8_device
#  define mem_true24_word_device mem_true24_device
#  define mem_true32_word_device mem_true32_device
#  define mem_true40_word_device mem_true40_device
#  define mem_true48_word_device mem_true48_device
#  define mem_true56_word_device mem_true56_device
#  define mem_true64_word_device mem_true64_device
#else
extern const gx_device_memory mem_mono_word_device;
extern const gx_device_memory mem_mapped2_word_device;
extern const gx_device_memory mem_mapped4_word_device;
extern const gx_device_memory mem_mapped8_word_device;
extern const gx_device_memory mem_true24_word_device;
extern const gx_device_memory mem_true32_word_device;
extern const gx_device_memory mem_true40_word_device;
extern const gx_device_memory mem_true48_word_device;
extern const gx_device_memory mem_true56_word_device;
extern const gx_device_memory mem_true64_word_device;

#endif
/* Provide standard palettes for 1-bit devices. */
extern const gs_const_string mem_mono_b_w_palette;	/* black=1, white=0 */
extern const gs_const_string mem_mono_w_b_palette;	/* black=0, white=1 */

#endif /* gdevmem_INCLUDED */
