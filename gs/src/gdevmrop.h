/* Copyright (C) 1995, 1996 Aladdin Enterprises.  All rights reserved.
  
  This file is part of Aladdin Ghostscript.
  
  Aladdin Ghostscript is distributed with NO WARRANTY OF ANY KIND.  No author
  or distributor accepts any responsibility for the consequences of using it,
  or for whether it serves any particular purpose or works at all, unless he
  or she says so in writing.  Refer to the Aladdin Ghostscript Free Public
  License (the "License") for full details.
  
  Every copy of Aladdin Ghostscript must include a copy of the License,
  normally in a plain ASCII text file named PUBLIC.  The License grants you
  the right to copy, modify and redistribute Aladdin Ghostscript, but only
  under certain conditions described in the License.  Among other things, the
  License requires that the copyright notice and this notice be preserved on
  all copies.
*/

/* gdevmrop.h */
/* Interfaces to RasterOp implementation */
/* Requires gxdevmem.h, gsropt.h */

/* Define the table of RasterOp implementation procedures. */
extern const far_data rop_proc rop_proc_table[256];

/* Define the table of RasterOp operand usage. */
extern const far_data byte /*rop_usage_t*/ rop_usage_table[256];

/*
 * PostScript colors normally act as the texture for RasterOp, with a null
 * (all zeros) source.  For images with CombineWithColor = true, we need
 * a way to use the image data as the source.  We implement this with a
 * device that applies RasterOp with a specified texture to drawing
 * operations, treating the drawing color as source rather than texture.
 * The texture is a gx_device_color; it may be any type of PostScript color,
 * even a Pattern.
 */
#ifndef gx_device_color_DEFINED
#  define gx_device_color_DEFINED
typedef struct gx_device_color_s gx_device_color;
#endif

#ifndef gx_device_rop_texture_DEFINED
#  define gx_device_rop_texture_DEFINED
typedef struct gx_device_rop_texture_s gx_device_rop_texture;
#endif

struct gx_device_rop_texture_s {
	gx_device_forward_common;
	gs_logical_operation_t log_op;
	const gx_device_color *texture;
};
extern_st(st_device_rop_texture);
#define public_st_device_rop_texture()\
  gs_public_st_suffix_add1(st_device_rop_texture, gx_device_rop_texture,\
    "gx_device_rop_texture", device_rop_texture_enum_ptrs, device_rop_texture_reloc_ptrs,\
    st_device_forward, texture)

/* Initialize a RasterOp source device. */
void gx_make_rop_texture_device(P4(gx_device_rop_texture *rsdev,
				   gx_device *target,
				   gs_logical_operation_t lop,
				   const gx_device_color *texture));
