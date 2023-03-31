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


/* Internal definitions for clipping */

#ifndef gxclip_INCLUDED
#  define gxclip_INCLUDED

#include "gxdevcli.h"

/*
 * Both rectangle list and mask clipping use callback procedures to process
 * each rectangle selected by the clipping region.  They share both the
 * callback procedures themselves and the structure that provides closure
 * data for these procedures.  We define a single closure structure, rather
 * than one per client/callback, just to reduce source code clutter.  The
 * comments below show which clients use each member.
 */
typedef struct clip_callback_data_s {
    /*
     * The original driver procedure stores the following of its arguments
     * that the callback procedure or the clipping algorithm needs.
     */
    gx_device *tdev;                    /* target device (always set) */
    int x, y, w, h;                     /* (always set) */
    gx_color_index color[2];            /* (all but copy_color) */
    const byte *data;                   /* copy_*, fill_mask */
    const gx_drawing_color *(pdc)[2];      /* strip_tile_rect_devn */
    int sourcex;                        /* ibid. */
    uint raster;                        /* ibid. */
    int depth;                          /* copy_alpha, fill_mask */
    const gx_drawing_color *pdcolor;    /* fill_mask, fill_rectangle_hl_color, fill_path, fill_stroke_path */
    const gx_drawing_color *pstroke_dcolor; /* fill_stroke_path */
    gs_logical_operation_t lop;         /* fill_mask, strip_copy_rop */
    const gx_clip_path *pcpath;         /* fill_mask, fill_rectangle_hl_color*/
    const gx_strip_bitmap *tiles;       /* strip_tile_rectangle */
    gs_int_point phase;                 /* strip_* */
    const gx_color_index *scolors;      /* strip_copy_rop */
    const gx_strip_bitmap *textures;    /* ibid. */
    const gx_color_index *tcolors;      /* ibid. */
    int plane_height;                   /* copy_planes, strip_copy_rop2 */
    const gs_gstate *pgs;               /* fill_path, fill_stroke_path, fill_rectangle_hl_color */
    gx_path *ppath;                     /* fill_path, fill_stroke_path */
    const gx_fill_params *params;       /* fill_path, fill_stroke_path */
    const gx_stroke_params *stroke_params; /* fill_stroke_path */
} clip_callback_data_t;

/* Declare the callback procedures. */
int
    clip_call_fill_rectangle(clip_callback_data_t * pccd,
                             int xc, int yc, int xec, int yec),
    clip_call_copy_mono(clip_callback_data_t * pccd,
                        int xc, int yc, int xec, int yec),
    clip_call_copy_planes(clip_callback_data_t * pccd,
                          int xc, int yc, int xec, int yec),
    clip_call_copy_color(clip_callback_data_t * pccd,
                         int xc, int yc, int xec, int yec),
    clip_call_copy_alpha(clip_callback_data_t * pccd,
                         int xc, int yc, int xec, int yec),
    clip_call_copy_alpha_hl_color(clip_callback_data_t * pccd,
                         int xc, int yc, int xec, int yec),
    clip_call_fill_mask(clip_callback_data_t * pccd,
                        int xc, int yc, int xec, int yec),
    clip_call_strip_tile_rectangle(clip_callback_data_t * pccd,
                                   int xc, int yc, int xec, int yec),
    clip_call_strip_tile_rect_devn(clip_callback_data_t * pccd,
                                   int xc, int yc, int xec, int yec),
    clip_call_strip_copy_rop2(clip_callback_data_t * pccd,
                              int xc, int yc, int xec, int yec),
    clip_call_fill_rectangle_hl_color(clip_callback_data_t * pccd,
                             int xc, int yc, int xec, int yec);

#endif /* gxclip_INCLUDED */
