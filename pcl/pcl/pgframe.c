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


/* pgframe.c */
/* PCL5/HP-GL/2 picture frame commands */
#include "math_.h"
#include "pgmand.h"
#include "pgdraw.h"
#include "pgmisc.h"
#include "gstypes.h"            /* for gsstate.h */
#include "gsmatrix.h"           /* for gsstate.h */
#include "gsmemory.h"           /* for gsstate.h */
#include "gsstate.h"
#include "pcdraw.h"
#include "pcfont.h"             /* for pcl_continue_underline */
#include "pcstate.h"

/* Import the RTL implementation of ESC % # A. */
extern pcl_command_proc(rtl_enter_pcl_mode);

/* This routine should only be used by the pcl reset command.  We
   implicitly enter pcl mode if an ESC E is embedded in the gl/2
   stream. */
int
pcl_implicit_gl2_finish(pcl_state_t * pcs)
{
    pcs->parse_other = 0;
    hpgl_call_mem(pcs->memory, hpgl_draw_current_path(pcs, hpgl_rm_vector));
    return 0;
}

/* Even though these are PCL commands, */
/* they are only relevant to HPGL. */

/* side effects resulting from a change in picture frame size or
   anchor point position */
static int
pcl_set_picture_frame_side_effects(pcl_state_t * pcs)
{
    hpgl_args_t args;

    /* default P1 and P2 */
    hpgl_args_setup(&args);
    hpgl_call_mem(pcs->memory, hpgl_IP(&args, pcs));

    /* default the clipping window */
    hpgl_args_setup(&args);
    hpgl_call_mem(pcs->memory, hpgl_IW(&args, pcs));

    /* clear the polygon buffer */
    hpgl_args_set_int(&args, 0);
    hpgl_call_mem(pcs->memory, hpgl_PM(&args, pcs));

    hpgl_args_set_int(&args, 2);
    hpgl_call_mem(pcs->memory, hpgl_PM(&args, pcs));

    /* NB according to spec should move pen to P1. */
    return 0;
}

int                             /* ESC * c <w_dp> X */
pcl_horiz_pic_frame_size_decipoints(pcl_args_t * pargs, pcl_state_t * pcs)
{
    coord size = (coord) (float_arg(pargs) * 10.0);     /* --> centipoints */

    if (size == 0)
        size = pcs->xfm_state.lp_size.x;
    if (size != pcs->g.picture_frame_width) {
        pcs->g.picture_frame_width = size;
        return pcl_set_picture_frame_side_effects(pcs);
    }
    return 0;
}

int                             /* ESC * c <h_dp> Y */
pcl_vert_pic_frame_size_decipoints(pcl_args_t * pargs, pcl_state_t * pcs)
{
    coord size = (coord) (float_arg(pargs) * 10.0);     /* --> centipoints */

    /* default to pcl logical page */
    if (size == 0) {
        size = pcs->xfm_state.lp_size.y;
        if (pcs->personality != rtl)
            size -= inch2coord(1.0);
    }
    if (size != pcs->g.picture_frame_height) {
        pcs->g.picture_frame_height = size;
        return pcl_set_picture_frame_side_effects(pcs);
    }
    return 0;
}

/*
 * ESC * c 0 T
 */
int
pcl_set_pic_frame_anchor_point(pcl_args_t * pargs, pcl_state_t * pcs)
{
    uint i = uint_arg(pargs);
    gs_point tmp_pt;

    if (i != 0)
        return 0;

    /* The anchor point is in logical page space */
    tmp_pt.x = pcs->cap.x;
    tmp_pt.y = pcs->cap.y;
    pcl_xfm_to_logical_page_space(pcs, &tmp_pt);
    if ((tmp_pt.x != pcs->g.picture_frame.anchor_point.x) ||
        (tmp_pt.y != pcs->g.picture_frame.anchor_point.y)) {
        pcs->g.picture_frame.anchor_point.x = (coord) tmp_pt.x;
        pcs->g.picture_frame.anchor_point.y = (coord) tmp_pt.y;
        return pcl_set_picture_frame_side_effects(pcs);
    }
    return 0;
}

int                             /* ESC * c <w_in> K */
pcl_hpgl_plot_horiz_size(pcl_args_t * pargs, pcl_state_t * pcs)
{
    /* convert to centipoints as to match the picture frame */
    float size = float_arg(pargs) * 7200.0;

    if ((coord) size == 0) {
        size = (float)pcs->g.picture_frame_width;
        pcs->g.plot_size_horizontal_specified = false;
    } else
        pcs->g.plot_size_horizontal_specified = true;

    pcs->g.plot_width = (coord) size;
    return pcl_set_picture_frame_side_effects(pcs);
}

int                             /* ESC * c <h_in> L */
pcl_hpgl_plot_vert_size(pcl_args_t * pargs, pcl_state_t * pcs)
{
    /* convert to centipoints as to match the picture frame */
    float size = float_arg(pargs) * 7200.0;

    if ((coord) size == 0) {
        size = (float)pcs->g.picture_frame_height;
        pcs->g.plot_size_vertical_specified = false;
    } else
        pcs->g.plot_size_vertical_specified = true;
    pcs->g.plot_height = (coord) size;
    return pcl_set_picture_frame_side_effects(pcs);
}

/* We redefine this command so we can draw the current GL path */
/* and, if appropriate, reset the underline bookkeeping. */
static int                      /* ESC % <enum> A */
pcl_enter_pcl_mode(pcl_args_t * pargs, pcl_state_t * pcs)
{
    int code;

    hpgl_call_mem(pcs->memory, hpgl_draw_current_path(pcs, hpgl_rm_vector));
    code = rtl_enter_pcl_mode(pargs, pcs);
    switch (code) {
        default:               /* error */
            return code;
        case 1:                /* CAP changed */
            pcl_continue_underline(pcs);
        case 0:                /* CAP not changed */
            break;
    }
    return 0;
}

/* Initialization */
static int
pgframe_do_registration(pcl_parser_state_t * pcl_parser_state,
                        gs_memory_t * mem)
{
    /* Register commands */
    DEFINE_CLASS('*')
        {'c', 'X',
         PCL_COMMAND("Horizontal Picture Frame Size Decipoints",
                     pcl_horiz_pic_frame_size_decipoints,
                     pca_neg_error | pca_big_error)},
        {'c', 'Y',
         PCL_COMMAND("Vertical Picture Frame Size Decipoints",
                     pcl_vert_pic_frame_size_decipoints,
                     pca_neg_error | pca_big_error)},
        {'c', 'T',
         PCL_COMMAND("Set Picture Frame Anchor Point",
                     pcl_set_pic_frame_anchor_point,
                     pca_neg_error | pca_big_error)},
        {'c', 'K',
         PCL_COMMAND("HP-GL/2 Plot Horizontal Size",
                     pcl_hpgl_plot_horiz_size,
                     pca_neg_error | pca_big_error)},
        {'c', 'L',
         PCL_COMMAND("HP-GL/2 Plot Vertical Size",
                     pcl_hpgl_plot_vert_size,
                     pca_neg_error | pca_big_error)},
    END_CLASS

    DEFINE_CLASS('%')
        {0, 'A',
         PCL_COMMAND("Enter PCL Mode",
                     pcl_enter_pcl_mode,
                     pca_neg_ok | pca_big_ok | pca_in_rtl)},
    END_CLASS
    return 0;
}

const pcl_init_t pgframe_init = {
    pgframe_do_registration, 0
};
