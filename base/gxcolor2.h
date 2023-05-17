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


/* Internal definitions for Level 2 color routines */
/* Requires gsstruct.h, gxfixed.h */

#ifndef gxcolor2_INCLUDED
#  define gxcolor2_INCLUDED

#include "gscolor2.h"
#include "gsmatrix.h"		/* for step_matrix */
#include "gsrefct.h"
#include "gxbitmap.h"
#include "gxdevcli.h"

/* Cache for Indexed color with procedure, or Separation color. */
struct gs_indexed_map_s {
    rc_header rc;
    union {
        int (*lookup_index)(const gs_color_space *, int, float *);
        int (*tint_transform)(const gs_separation_params *, double, float *);
    } proc;
    void *proc_data;
    uint num_values;	/* base_space->type->num_components * (hival + 1) */
    float *values;	/* actually [num_values] */
};
#define private_st_indexed_map() /* in gscolor2.c */\
  gs_private_st_ptrs2(st_indexed_map, gs_indexed_map, "gs_indexed_map",\
    indexed_map_enum_ptrs, indexed_map_reloc_ptrs, proc_data, values)

/* Define a lookup_index procedure that just returns the map values. */
int lookup_indexed_map(const gs_color_space *, int, float *);

/* Allocate an indexed map and its values. */
/* The initial reference count is 1. */
int alloc_indexed_map(gs_indexed_map ** ppmap, int num_values,
                      gs_memory_t * mem, client_name_t cname);

/* Free an indexed map and its values when the reference count goes to 0. */
rc_free_proc(free_indexed_map);

/**************** TO gxptype1.h ****************/

/*
 * We define 'tiling space' as the space in which (0,0) is the origin of
 * the key pattern cell and in which coordinate (i,j) is displaced by
 * i * XStep + j * YStep from the origin.  In this space, it is easy to
 * compute a (rectangular) set of tile copies that cover a (rectangular)
 * region to be tiled.  Note that since all we care about is that the
 * stepping matrix (the transformation from tiling space to device space)
 * yield the right set of coordinates for integral X and Y values, we can
 * adjust it to make the tiling computation easier; in particular, we can
 * arrange it so that all 4 transformation factors are non-negative.
 */

struct gs_pattern1_instance_s {
    gs_pattern_instance_common;	/* must be first */
    gs_pattern1_template_t templat;
    /* Following are created by makepattern */
    gs_matrix step_matrix;	/* tiling space -> device space */
    gs_rect bbox;		/* bbox of tile in tiling space */
    bool is_simple;		/* true if xstep/ystep = tile size */
    bool has_overlap;           /* true if step is smaller than bbox size */
                                /* This is used to detect if we must do */
                                /* blending if we have transparency */
    int num_planar_planes;      /* During clist writing we "lose" information
                                   about any intervening transparency device */
    /*
     * uses_mask is always true for PostScript patterns, but is false
     * for bitmap patterns that don't have explicit transparent pixels.
     */
    bool uses_mask;	        /* if true, pattern mask must be created */
    bool is_clist;		/* if false, automatically determine and set, if true, use_clist */
    gs_int_point size;		/* in device coordinates */
    gx_bitmap_id id;		/* key for cached bitmap (= id of mask) */
};

#define public_st_pattern1_instance() /* in gsptype1.c */\
  gs_public_st_composite(st_pattern1_instance, gs_pattern1_instance_t,\
    "gs_pattern1_instance_t", pattern1_instance_enum_ptrs,\
    pattern1_instance_reloc_ptrs)

/* This is used for the case where we have float outputs due to the use of a procedure in
   the indexed image, but we desire to have byte outputs.  Used with interpolated images. */

#define float_color_to_byte_color(float_color) ( (0.0 < float_color && float_color < 1.0) ? \
    ((unsigned char) (float_color*255.0)) :  ( (float_color <= 0.0) ? 0x00 : 0xFF  ))

#define float_color_to_color16(float_color) ( (0.0 < float_color && float_color < 1.0) ? \
    ((uint16_t) (float_color*65535.0)) :  ( (float_color <= 0.0) ? 0x00 : 0xFFFF  ))

#endif /* gxcolor2_INCLUDED */
