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


/* Mask clipping device and interface */
/* Requires gxdevice.h, gxdevmem.h */

#ifndef gxmclip_INCLUDED
#  define gxmclip_INCLUDED

#include "gxclip.h"
#include "gxdevmem.h"

/*
 * ImageType 3 images and Patterns that don't completely fill their
 * bounding box require the ability to clip against a mask.
 * The interface declared here doesn't take a position on whether
 * the mask will be used only in one position (ImageType 3) or in
 * multiple positions for tiling (Patterns).
 *
 * All the information in this file is logically private, but we must expose
 * the structure definition so that clients can allocate instances in the
 * stack frame.
 */

#define tile_clip_buffer_request 32768	/* enough for 2400 dpi up to 109" wide */
#define tile_clip_buffer_size\
  ((tile_clip_buffer_request / ARCH_SIZEOF_LONG) * ARCH_SIZEOF_LONG)
typedef struct gx_device_mask_clip_s {
    gx_device_forward_common;	/* target is set by client */
    gx_strip_bitmap tiles;
    gx_device_memory mdev;	/* for tile buffer for copy_mono */
    gs_int_point phase;		/* device space origin relative */
                                /* to tile (backwards from gstate phase) */
    /* Ensure that the buffer is long-aligned. */
    union _b {
        byte bytes[tile_clip_buffer_size];
        ulong longs[tile_clip_buffer_size / ARCH_SIZEOF_LONG];
    } buffer;
} gx_device_mask_clip;

extern_st(st_device_mask_clip);
#define public_st_device_mask_clip()	/* in gxmclip.c */\
  gs_public_st_composite_use_final(st_device_mask_clip, gx_device_mask_clip,\
    "gx_device_mask_clip", device_mask_clip_enum_ptrs,\
    device_mask_clip_reloc_ptrs, gx_device_finalize)

/*
 * Internal routine to initialize a mask clipping device.
 * We supply an explicit device space origin or phase.
 * Note that this procedure does not set cdev->tiles.
 * If mem is sent as NULL, then the initialised device structure will not be
 * freed when the refcount reaches 0 (suitable for devices on stack, or within
 * other structures).
 */
int gx_mask_clip_initialize(gx_device_mask_clip * cdev,
                            const gx_device_mask_clip * proto,
                            const gx_bitmap * bits, gx_device * tdev,
                            int tx, int ty, gs_memory_t *mem);

/*
 * Prepare colors for a copy_mono operation.
 * The arguments of copy_mono are free variables:
 *   dev, data, sourcex, raster, id, x, y, w, y, color0, color1.
 */
#define setup_mask_copy_mono(cdev, color, mcolor0, mcolor1)\
        BEGIN\
          if ( cdev->mdev.base == 0 ) {\
            /*\
             * The tile was too large for us to buffer even one scan line.\
             * Punt to the very, very slow default implementation of\
             * copy_mono.\
             */\
            return gx_default_copy_mono(dev, data, sourcex, raster, id,\
                                        x, y, w, h, color0, color1);\
          }\
          if ( color1 != gx_no_color_index ) {\
            if ( color0 != gx_no_color_index ) {\
              /* Pre-fill with color0. */\
              code =\
                (*dev_proc(dev, fill_rectangle))(dev, x, y, w, h, color0);\
              if ( code < 0 )\
                return code;\
            }\
            color = color1;\
            mcolor0 = 0, mcolor1 = gx_no_color_index;\
          } else if ( color0 != gx_no_color_index ) {\
            color = color0;\
            mcolor0 = gx_no_color_index, mcolor1 = 0;\
          } else\
            return 0;\
        END

#endif /* gxmclip_INCLUDED */
