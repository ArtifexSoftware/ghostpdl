/* Copyright (C) 2000 Aladdin Enterprises.  All rights reserved.

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

/*$Id$ */
/* Transparency parameter definitions */

#ifndef gstparam_INCLUDED
#  define gstparam_INCLUDED

#include "gsiparam.h"		/* for soft masks */
#include "gsrefct.h"		/* for soft masks */
#include "stream.h"		/* for soft masks */

/* Define the names of the known blend modes. */
typedef enum {
    BLEND_MODE_Compatible,
    BLEND_MODE_Normal,
    BLEND_MODE_Multiply,
    BLEND_MODE_Screen,
    BLEND_MODE_Difference,
    BLEND_MODE_Darken,
    BLEND_MODE_Lighten,
    BLEND_MODE_ColorDodge,
    BLEND_MODE_ColorBurn,
    BLEND_MODE_Exclusion,
    BLEND_MODE_HardLight,
    BLEND_MODE_Overlay,
    BLEND_MODE_SoftLight,
    BLEND_MODE_Luminosity,
    BLEND_MODE_Hue,
    BLEND_MODE_Saturation,
    BLEND_MODE_Color
#define MAX_BLEND_MODE BLEND_MODE_Color
} gs_blend_mode_t;
#define GS_BLEND_MODE_NAMES\
  "Compatible", "Normal", "Multiply", "Screen", "Difference",\
  "Darken", "Lighten", "ColorDodge", "ColorBurn", "Exclusion",\
  "HardLight", "Overlay", "SoftLight", "Luminosity", "Hue",\
  "Saturation", "Color"

/*
 * Define the structure for a soft mask (opacity or shape).  A soft mask
 * consists of an ImageType 1 image plus a DataSource, which must be a
 * reusable stream (a readable, positionable stream that doesn't close
 * itself when it reaches EOD).
 *
 * Note that soft masks are reference counted, and must therefore be
 * allocate with rc_alloc_struct_#.  Normally # = 0, since setting the
 * mask in the graphics state increments the reference count.  Note also
 * that they have a unique ID, which must be updated if they are modified.
 * Normally this is not an issue, since clients normally construct them
 * and then never modify them.
 */
typedef struct gs_soft_mask_s {
    rc_header rc;
    gs_id id;
    /*
     * In the image structure, ColorSpace is ignored (but it must be a
     * valid 1-component color space); ImageMask must be false; Alpha
     * must be "none".
     */
    gs_image1_t image;
    stream *DataSource;
} gs_soft_mask_t;
#define private_st_gs_soft_mask()\
  gs_private_st_suffix_add1(st_gs_soft_mask, gs_soft_mask_t, "gs_soft_mask_t",\
    soft_mask_enum_ptrs, soft_mask_reloc_ptrs, st_gs_image1, DataSource)

#endif /* gstparam_INCLUDED */
