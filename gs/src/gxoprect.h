/* Copyright (C) 2002 Aladdin Enterprises.  All rights reserved.
  
  This file is part of AFPL Ghostscript.
  
  AFPL Ghostscript is distributed with NO WARRANTY OF ANY KIND.  No author or
  distributor accepts any responsibility for the consequences of using it, or
  for whether it serves any particular purpose or works at all, unless he or
  she says so in writing.  Refer to the Aladdin Free Public License (the
  "License") for full details.
  
  Every copy of AFPL Ghostscript must include a copy of the License, normally
  in a plain ASCII text file named PUBLIC.  The License grants you the right
  to copy, modify and redistribute AFPL Ghostscript, but only under certain
  conditions described in the License.  Among other things, the License
  requires that the copyright notice and this notice be preserved on all
  copies.
*/

/* $Id$ */
/* geneic overprint fill rectangle interface */

#ifndef gxoprect_INCLUDED
#define gxoprect_INCLUDED

/*
 * Perform the fill rectangle operation for a non-separable color encoding
 * that requires overprint support.
 *
 * Returns 0 on success, < 0 in the event of an error.
 */
extern  int     gx_overprint_generic_fill_rectangle(
    gx_device *             tdev,
    gx_color_index          drawn_comps,
    int                     x,
    int                     y,
    int                     w,
    int                     h,
    gx_color_index          color,
    gs_memory_t *           mem );

/*
 * Perform the fill rectangle operation of a separable color encoding.
 * There are two versions of this routine: ..._1 for cases in which the
 * color depth is a divisor of 8 * sizeof(mono_fill_chunk), and ..._2 if
 * this is not the case (most typically if the depth == 24).
 *
 * For both cases, the color and retain_mask values passed to this
 * procedure are expected to be already swapped as required for a byte-
 * oriented bitmap. This consideration affects only little-endian
 * machines. For those machines, if depth > 9 the color passed to these
 * two procedures will not be the same as that passed to
 * gx_overprint_generic_fill_rectangle.
 *
 * Returns 0 on success, < 0 in the event of an error.
 */
extern  int     gx_overprint_sep_fill_rectangle_1(
    gx_device *             tdev,
    gx_color_index          retain_mask,    /* already swapped */
    int                     x,
    int                     y,
    int                     w,
    int                     h,
    gx_color_index          color,          /* already swapped */
    gs_memory_t *           mem );

extern  int     gx_overprint_sep_fill_rectangle_2(
    gx_device *             tdev,
    gx_color_index          retain_mask,    /* already swapped */
    int                     x,
    int                     y,
    int                     w,
    int                     h,
    gx_color_index          color,          /* already swapped */
    gs_memory_t *           mem );

#endif  /* gxoprect_INCLUDED */
