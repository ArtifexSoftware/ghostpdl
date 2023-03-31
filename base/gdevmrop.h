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

/* Definitions for device RasterOp implementations. */
/* Requires gxdevmem.h, gsropt.h */

#ifndef gdevmrop_INCLUDED
#  define gdevmrop_INCLUDED

#include "gsropt.h"
#include "gxdevcli.h"
#include "gximage.h"

#ifdef DEBUG
/* Trace a [strip_]copy_rop call. */
void trace_copy_rop(const char *cname, gx_device * dev,
                    const byte * sdata, int sourcex, uint sraster,
                    gx_bitmap_id id, const gx_color_index * scolors,
                    const gx_strip_bitmap * textures,
                    const gx_color_index * tcolors,
                    int x, int y, int width, int height,
                    int phase_x, int phase_y, gs_logical_operation_t lop);
#endif

/*
 * PostScript colors normally act as the texture for RasterOp, with a null
 * (all zeros) source.  For images with CombineWithColor = true, we need
 * a way to use the image data as the source.  We implement this with a
 * device that applies RasterOp with a specified texture to drawing
 * operations, treating the drawing color as source rather than texture.
 * The texture is a gx_device_color; it may be any type of color, even a
 * pattern.
 */

struct gx_device_rop_texture_s {
    gx_device_forward_common;
    gs_logical_operation_t log_op;
    gx_device_color texture;
};

#define private_st_device_rop_texture()	/* in gdevrops.c */\
  gs_private_st_composite_use_final(st_device_rop_texture,\
    gx_device_rop_texture, "gx_device_rop_texture",\
    device_rop_texture_enum_ptrs, device_rop_texture_reloc_ptrs,\
    gx_device_finalize)

/* Create a RasterOp source device. */
int gx_alloc_rop_texture_device(gx_device_rop_texture ** prsdev,
                                gs_memory_t * mem,
                                client_name_t cname);

/* Initialize a RasterOp source device. */
void gx_make_rop_texture_device(gx_device_rop_texture * rsdev,
                                gx_device * target,
                                gs_logical_operation_t lop,
                                const gx_device_color * texture);

#endif /* gdevmrop_INCLUDED */
