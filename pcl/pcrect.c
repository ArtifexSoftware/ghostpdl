/* Portions Copyright (C) 2001 artofcode LLC.
   Portions Copyright (C) 1996, 2001 Artifex Software Inc.
   Portions Copyright (C) 1988, 2000 Aladdin Enterprises.
   This software is based in part on the work of the Independent JPEG Group.
   All Rights Reserved.

   This software is distributed under license and may not be copied, modified
   or distributed except as expressly authorized under the terms of that
   license.  Refer to licensing information at http://www.artifex.com/ or
   contact Artifex Software, Inc., 101 Lucas Valley Road #110,
   San Rafael, CA  94903, (415)492-9861, for further information. */
/*$Id$ */

/* pcrect.c - PCL5 rectangular area fill commands */
#include "math_.h"
#include "pcommand.h"
#include "pcstate.h"
#include "pcdraw.h"
#include "pcpatrn.h"
#include "pcbiptrn.h"
#include "pcuptrn.h"
#include "gspath.h"
#include "gspath2.h"
#include "gsmatrix.h"
#include "gscoord.h"
#include "gspaint.h"
#include "gsrop.h"



/*
 * The graphic library is, unfortunately, not equipped to produce accurate
 * PCL rectangles on its own. These rectangles must always be rendered with
 * the same pixel dimensions, regardless of location (ignoring clipping), 
 * in the "grid intersection" pixel placement mode, must always add one
 * additional pixel in the "right" and "down" directions, relative to print
 * direction 0.
 *
 * To accomplish these tasks, it is necessary to properly adjust the rectangle
 * dimensions provided to the graphic layer. That task is accomplished by
 * this routine.
 *
 * Note that this code is designed to work with no fill adjustment in the
 * graphic library.
 */
  static int
adjust_render_rectangle(
    pcl_state_t *           pcs
)
{
    gs_state *              pgs = pcs->pgs;
    const pcl_xfm_state_t * pxfmst = &(pcs->xfm_state);
    coord                   w = pcs->rectangle.x;
    coord                   h = pcs->rectangle.y;
    gs_point                dims;
    gs_rect                 rect;
    gs_matrix               save_mtx, ident_mtx;
    int                     code = 0;

    /* adjust the width and height to reflect the logical page boundaries */
    if (pcs->cap.x + w > pxfmst->pd_size.x)
        w = pxfmst->pd_size.x - pcs->cap.x;
    if (pcs->cap.y + h > pxfmst->pd_size.y)
        h = pxfmst->pd_size.y - pcs->cap.y;

    /* move the current point to an integral pixel location */
    gs_transform(pgs, (floatp)pcs->cap.x, (floatp)pcs->cap.y, &(rect.p));
    rect.p.x = floor(rect.p.x + 0.5);
    rect.p.y = floor(rect.p.y + 0.5);

    /* set the dimensions to be a multiple of pixels */
    gs_dtransform(pgs, (floatp)w, (floatp)h, &dims);
    if (dims.x >= 0)
        rect.q.x = rect.p.x + ceil(dims.x);
    else {
        rect.q.x = rect.p.x;
        rect.p.x += floor(dims.x);
    }
    if (dims.y >= 0)
        rect.q.y = rect.p.y + ceil(dims.y);
    else {
        rect.q.y = rect.p.y;
        rect.p.y += floor(dims.y);
    }

    /*
     * If the pixel-placement mode is 1, decrease the size of the rectangle
     * by one pixel. Note that this occurs only if the rectangle dimension is
     * not 0; rectangles with a zero dimension are never rendered, irrespective
     * of pixle placement mode.
     *
     * This trickiest part of this increase in dimension concerns correction
     * for the device orientation relative to print direction 0. The output
     * produced needs to be the same for pages produced with either long-edge
     * -feed or short-edge-feed printers, so it is not possible to add one
     * pixel in each direction in device space.
     */
    if ((pcs->pp_mode == 1) && (dims.x != 0.0) && (dims.y != 0.0)) {
        gs_matrix   dflt_mtx;
        gs_point    disp;

        gs_defaultmatrix(pgs, &dflt_mtx);
        gs_distance_transform(1.0, 1.0, &dflt_mtx, &disp);
        if (disp.x < 0.0)
            rect.p.x += 1.0;
        else
            rect.q.x -= 1.0;
        if (disp.y < 0.0)
            rect.p.y += 1.0;
        else
            rect.q.y -= 1.0;
    }

    /* rectangles with 0-dimensions are never printed */
    if ((rect.p.x == rect.q.x) || (rect.p.y == rect.q.y))
        return 0;

    /* transform back into pseudo-print-direction space */
    gs_currentmatrix(pgs, &save_mtx);
    gs_make_identity(&ident_mtx);
    gs_setmatrix(pgs, &ident_mtx);
    code = gs_rectfill(pgs, &rect, 1);
    pcs->page_marked = true;
    gs_setmatrix(pgs, &save_mtx);
    return code;
}

/*
 * ESC * c <dp> H
 */
  static int
pcl_horiz_rect_size_decipoints(
    pcl_args_t *    pargs,
    pcl_state_t *   pcs
)
{
    pcs->rectangle.x = any_abs(float_arg(pargs)) * 10;
    return 0;
}

/*
 * ESC * c <units> A
 */
  static int
pcl_horiz_rect_size_units(
    pcl_args_t *    pargs,
    pcl_state_t *   pcs
)
{
    pcs->rectangle.x = any_abs(int_arg(pargs)) * pcs->uom_cp;
    return 0;
}

/*
 * ESC * c <dp> V
 */
  static int
pcl_vert_rect_size_decipoints(
    pcl_args_t *    pargs,
    pcl_state_t *   pcs
)
{
    pcs->rectangle.y = any_abs(float_arg(pargs)) * 10;
    return 0;
}

/*
 * ESC * c B
 */
  static int
pcl_vert_rect_size_units(
    pcl_args_t *    pargs,
    pcl_state_t *   pcs
)
{
    pcs->rectangle.y = any_abs(int_arg(pargs)) * pcs->uom_cp;
    return 0;
}

/*
 * ESC * c <fill_enum> P
 */
  static int
pcl_fill_rect_area(
    pcl_args_t *            pargs,
    pcl_state_t *           pcs
)
{
    pcl_pattern_source_t    type = (pcl_pattern_source_t)int_arg(pargs);
    int                     id = pcs->pattern_id;
    int                     code = 0;

    /*
     * Rectangles have a special property with respect to patterns:
     *
     *     a non-zero, non-existent pattern specification causes the command
     *        to be ignored, UNLESS this is the current pattern. This only
     *        applies to cross-hatch and user-defined patterns, as the
     *        pattern id. is interpreted as an intensity level for shade
     *        patterns, and patterns >= 100% are considered white.
     */
    if (type == pcl_pattern_cross_hatch) {
        if (pcl_pattern_get_cross(pcs, id) == 0)
            return 0;
    } else if (type == pcl_pattern_user_defined) {
        if (pcl_pattern_get_pcl_uptrn(pcs, id) == 0)
            return 0;
    } else if (type == pcl_pattern_current_pattern) {
        type = pcs->pattern_type;
        id = pcs->current_pattern_id;
    } else if (type > pcl_pattern_shading)
        return 0;

    /* set up the graphic state; render the rectangle */
    if ( ((code = pcl_set_drawing_color(pcs, type, id, false)) >= 0) &&
         ((code = pcl_set_graphics_state(pcs)) >= 0)                   )
        code = adjust_render_rectangle(pcs);
    return code;
}

/*
 * Initialization
 */
  static int
pcrect_do_registration(
    pcl_parser_state_t *pcl_parser_state,
    gs_memory_t *   mem
)
{
    /* Register commands */
    DEFINE_CLASS('*')
    {
        'c', 'H',
	PCL_COMMAND( "Horizontal Rectangle Size Decipoints",
		     pcl_horiz_rect_size_decipoints,
		     pca_neg_error | pca_big_error | pca_in_rtl
                     )
    },
    {
        'c', 'A',
	PCL_COMMAND( "Horizontal Rectangle Size Units",
		      pcl_horiz_rect_size_units,
		      pca_neg_error | pca_big_error | pca_in_rtl
                      )
    },
    {
        'c', 'V',
	PCL_COMMAND( "Vertical Rectangle Size Decipoint",
		     pcl_vert_rect_size_decipoints,
		     pca_neg_error | pca_big_error | pca_in_rtl
                     )
    },
    {
        'c', 'B',
	PCL_COMMAND( "Vertical Rectangle Size Units",
		     pcl_vert_rect_size_units,
		     pca_neg_error | pca_big_error | pca_in_rtl
                     )
    },
    {
        'c', 'P',
        PCL_COMMAND( "Fill Rectangular Area",
                     pcl_fill_rect_area,
		     pca_neg_ignore | pca_big_ignore | pca_in_rtl
                     )
    },
    END_CLASS
    return 0;
}

  static void
pcrect_do_reset(
    pcl_state_t *       pcs,
    pcl_reset_type_t    type
)
{
    static  const uint  mask = (  pcl_reset_initial
                                | pcl_reset_printer
                                | pcl_reset_overlay );
    if ((type & mask) != 0) {
	pcs->rectangle.x = 0;
	pcs->rectangle.y = 0;
    }
}

const pcl_init_t    pcrect_init = { pcrect_do_registration, pcrect_do_reset, 0 };
