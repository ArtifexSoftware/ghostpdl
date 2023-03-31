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


/* Definitions and support procedures for higher-level band list commands */
/* Extends (requires) gxcldev.h */

#ifndef gxclpath_INCLUDED
#  define gxclpath_INCLUDED

#include "gxdevcli.h"
#include "gxcldev.h"

/*
 * Define the flags indicating whether a band knows the current values of
 * various miscellaneous parameters (pcls->known).  The first N flags match
 * the mask parameter for cmd_set_misc2 below.
 */
#define cap_join_known          (1<<0)
#define cj_ac_sa_known          (1<<1)
#define flatness_known          (1<<2)
#define line_width_known        (1<<3)
#define miter_limit_known       (1<<4)
#define op_bm_tk_known          (1<<5)
/* segment_notes must fit in the first byte (i.e. be less than 1<<7). */
#define segment_notes_known     (1<<6) /* not used in pcls->known */
/* (flags beyond this point require an extra byte) */
#define ais_known               (1<<7)
#define stroke_alpha_known      (1<<8)
#define fill_alpha_known        (1<<9)
#define misc2_all_known         ((1<<10)-1)
/* End of misc2 flags. */
/* The following bits don't get passed in misc2, so are only limited by sizeof uint */
#define fill_adjust_known       (1<<10)
#define ctm_known               (1<<11)
#define dash_known              (1<<12)
#define clip_path_known         (1<<13)
#define STROKE_ALL_KNOWN        ((1<<14)-1)
#define color_space_known       (1<<14)
/*#define all_known             ((1<<15)-1) */

/* Define the drawing color types for distinguishing different */
/* fill/stroke command variations. */
typedef enum {
    cmd_dc_type_pure = 0,
    cmd_dc_type_ht = 1,
    cmd_dc_type_color = 2
} cmd_dc_type;

/* This is usd for cmd_opv_ext_put_drawing_color so that we know if it
   is assocated with a tile or not and for fill or stroke color */
typedef enum {
    devn_not_tile_fill = 0x00,
    devn_not_tile_stroke = 0x01,
    devn_tile0 = 0x02,
    devn_tile1 = 0x03
} dc_devn_cl_type;
/*
 * Further extended command set. This code always occupies a byte, which
 * is the second byte of a command whose first byte is cmd_opv_extend.
 */
typedef enum {
    cmd_opv_ext_put_params           = 0x00, /* serialized parameter list */
    cmd_opv_ext_composite            = 0x01, /* compositor id,
                                              * serialized compositor */
    cmd_opv_ext_put_halftone         = 0x02, /* length of entire halftone */
    cmd_opv_ext_put_ht_seg           = 0x03, /* segment length,
                                              * halftone segment data */
    cmd_opv_ext_put_fill_dcolor      = 0x04, /* length, color type id,
                                              * serialized color */
    cmd_opv_ext_put_stroke_dcolor    = 0x05, /* length, color type id,
                                              * serialized color */
    cmd_opv_ext_tile_rect_hl         = 0x06, /* Uses devn colors in tiling fill */
    cmd_opv_ext_put_tile_devn_color0 = 0x07, /* Devn color0 for tile filling */
    cmd_opv_ext_put_tile_devn_color1 = 0x08, /* Devn color1 for tile filling */
    cmd_opv_ext_set_color_is_devn    = 0x09, /* Used for overload of copy_color_alpha */
    cmd_opv_ext_unset_color_is_devn  = 0x0a  /* Used for overload of copy_color_alpha */
} gx_cmd_ext_op;

#ifdef DEBUG
#define cmd_extend_op_name_strings \
  "put_params",\
  "composite",\
  "put_halftone",\
  "put_ht_seg",\
  "put_fill_dcolor",\
  "put_stroke_dcolor",\
  "tile_rect_hl",\
  "put_tile_devn_color0",\
  "put_tile_devn_color1",\
  "set_color_is_devn",\
  "unset_color_is_devn"

extern const char *cmd_extend_op_names[256];
#endif

#define cmd_segment_op_num_operands_values\
  2, 2, 1, 1, 4, 6, 6, 6, 4, 4, 4, 4, 2, 2, 0, 0

/*
 * We represent path coordinates as 'fixed' values in a variable-length,
 * relative form (s/t = sign, x/y = integer, f/g = fraction):
 *      00sxxxxx xfffffff ffffftyy yyyygggg gggggggg
 *      01sxxxxx xxxxffff ffffffff
 *      10sxxxxx xxxxxxxx xxxxffff ffffffff
 *      110sxxxx xxxxxxff
 *      111----- (a full-size `fixed' value)
 */
#define is_bits(d, n) !(((d) + ((fixed)1 << ((n) - 1))) & (-(fixed)1 << (n)))

/*
 * Maximum size of a halftone segment. This leaves enough headroom to
 * accommodate any reasonable requirements of the command buffer.
 */
#define cbuf_ht_seg_max_size    (cbuf_size - 32)    /* leave some headroom */

/* ---------------- Driver procedures ---------------- */

/* In gxclpath.c */
dev_proc_fill_path(clist_fill_path);
dev_proc_stroke_path(clist_stroke_path);
dev_proc_fill_stroke_path(clist_fill_stroke_path);
dev_proc_lock_pattern(clist_lock_pattern);
dev_proc_fill_parallelogram(clist_fill_parallelogram);
dev_proc_fill_triangle(clist_fill_triangle);

/* ---------------- Driver procedure support ---------------- */

/* The procedures and macros defined here are used when writing */
/* (gxclimag.c, gxclpath.c). */

/* Compare and update members of the gs_gstate. */
#define state_neq(member)\
  (cdev->gs_gstate.member != pgs->member)
#define state_update(member)\
  (cdev->gs_gstate.member = pgs->member)

/* ------ Exported by gxclpath.c ------ */

/* Compute the colors used by a drawing color. */
gx_color_usage_bits cmd_drawing_color_usage(gx_device_clist_writer *cldev,
                                            const gx_drawing_color *pdcolor);

/*
 * Compute whether a drawing operation will require the slow (full-pixel)
 * RasterOp implementation.  If pdcolor is not NULL, it is the texture for
 * the RasterOp.
 */
bool cmd_slow_rop(gx_device *dev, gs_logical_operation_t lop,
                  const gx_drawing_color *pdcolor);

/* Write out the color for filling, stroking, or masking. */
/* Return a cmd_dc_type. */
int cmd_put_drawing_color(gx_device_clist_writer * cldev,
                          gx_clist_state * pcls,
                          const gx_drawing_color * pdcolor,
                          cmd_rects_enum_t *pre, dc_devn_cl_type devn_type);

/* Clear (a) specific 'known' flag(s) for all bands. */
/* We must do this whenever the value of a 'known' parameter changes. */
void cmd_clear_known(gx_device_clist_writer * cldev, uint known);

/* Compute the written CTM length. */
int cmd_write_ctm_return_length(gx_device_clist_writer * cldev, const gs_matrix *m);
int cmd_write_ctm_return_length_nodevice(const gs_matrix *m);
/* Write out CTM. */
int cmd_write_ctm(const gs_matrix *m, byte *dp, int len);

/* Write out values of any unknown parameters. */
#define cmd_do_write_unknown(cldev, pcls, must_know)\
  ( ~(pcls)->known & (must_know) ?\
    cmd_write_unknown(cldev, pcls, must_know) : 0 )
int cmd_write_unknown(gx_device_clist_writer * cldev, gx_clist_state * pcls,
                      uint must_know);

/* Check whether we need to change the clipping path in the device. */
bool cmd_check_clip_path(gx_device_clist_writer * cldev,
                         const gx_clip_path * pcpath);

#endif /* gxclpath_INCLUDED */
