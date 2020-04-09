/* Copyright (C) 2001-2020 Artifex Software, Inc.
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


/* pginit.c - Initialization and resetting for HP-GL/2. */
#include "gx.h"
#include "gsmatrix.h"           /* for gsstate.h */
#include "gsmemory.h"           /* for gsstate.h */
#include "gsstate.h"            /* for gs_setlimitclamp */
#include "pgfont.h"
#include "pgmand.h"
#include "pginit.h"
#include "pgdraw.h"
#include "pgmisc.h"
#include "pcpatrn.h"

/* ------ Internal procedures ------ */

/*
 * Reset a set of font parameters to their default values.
 */
void
hpgl_default_font_params(pcl_font_selection_t * pfs)
{
    pfs->params.symbol_set = 277;       /* Roman-8 */
    pfs->params.proportional_spacing = false;
    pl_fp_set_pitch_per_inch(&pfs->params, 9);
    pfs->params.height_4ths = (int)(11.5 * 4);
    pfs->params.style = 0;
    pfs->params.stroke_weight = 0;
    pfs->params.typeface_family = 48;   /* stick font */
    pfs->font = 0;              /* not looked up yet */
}

/*
 * the following is not consistant with the general model as we
 * usually depend upon calling the commands directly to update
 * appropriate qtate variables, unfortunately we must guarantee a
 * reasonable picture frame, anchor point, and plot size for the rest
 * of the hpgl/2 code to function properly, and they must be provided
 * all at once.  For example documented side effects of changing the
 * vertical picture frame height are IP;IW;PM;PM2, but these commands
 * do not make sense if the horizontal picture frame has never been
 * set.
 */
static void
hpgl_default_coordinate_system(hpgl_state_t * pcs)
{
    pcs->g.plot_width = pcs->g.picture_frame_width = pcs->xfm_state.lp_size.x;
    pcs->g.plot_height = pcs->g.picture_frame_height
        = pcs->xfm_state.lp_size.y;
    if (pcs->personality == rtl) {
        pcs->g.picture_frame.anchor_point.x = 0;
        pcs->g.picture_frame.anchor_point.y = 0;
    } else {
        pcs->g.picture_frame.anchor_point.x = pcs->margins.left;
        pcs->g.picture_frame.anchor_point.y = pcs->margins.top;
        {
            /* the previous setup subtracted 1" from the plot and
               picture frame height below, presumably derived from
               subtracting 1/2" for top and bottom margin.  A better
               result is to use 2 * top margin.  But this needs more
               testing against the HP printer, the new correction does
               work properly when the default top margin changes (ie. pclxl
               snippet mode). */
            coord margins_extent = (2 * pcs->margins.top);

            pcs->g.plot_height -= margins_extent;
            pcs->g.picture_frame_height -= margins_extent;
        }
    }
    pcs->g.plot_size_vertical_specified = false;
    pcs->g.plot_size_horizontal_specified = false;
    /* The default coordinate system is absolute with the origin at 0,0 */
    pcs->g.move_or_draw = hpgl_plot_move;
    pcs->g.relative_coords = hpgl_plot_absolute;
    {
        gs_point pos;

        pos.x = 0.0;
        pos.y = 0.0;
        (void)hpgl_set_current_position(pcs, &pos);
    }
    pcs->g.scaling_type = hpgl_scaling_none;
    pcs->g.soft_clip_window.rect.p.x = pcs->g.soft_clip_window.rect.p.y = 0;
    pcs->g.soft_clip_window.rect.q.x = pcs->g.soft_clip_window.rect.q.y = 0;
    return;
}

/*
 * Reset all the fill patterns to solid fill.
 */
int
hpgl_default_all_fill_patterns(hpgl_state_t * pgls)
{
    int code = 0;
    int i;

    for (i = 1; i <= 8; ++i) {
        if (((code = pcl_pattern_RF(i, NULL, pgls)) < 0) ||
            ((code = pcl_pattern_RF(-i, NULL, pgls)) < 0))
        {
            return code;
        }
    }
    return code;
}

int
hpgl_do_reset(pcl_state_t * pcs, pcl_reset_type_t type)
{
    int code;
    /* pgframe.c (Chapter 18) */
    hpgl_args_t hpgl_args;

    if ((type & (pcl_reset_initial | pcl_reset_printer | pcl_reset_cold)) !=
        0) {
        if ((type & (pcl_reset_initial | pcl_reset_cold)) != 0) {
            code = gx_path_alloc_contained(&pcs->g.polygon.buffer.path,
                                           pcs->memory,
                                           "hpgl_do_reset polygon buffer");
            if (code < 0)
                return code;
            gs_setlimitclamp(pcs->pgs, true);
        } else {
            code = gx_path_new(&pcs->g.polygon.buffer.path);
            if (code < 0)
                return code;
        }

        /* provide default anchor point, plot size and picture frame size */
        hpgl_default_coordinate_system(pcs);

        /* we should not have a path at this point but we make sure */
        code = hpgl_clear_current_path(pcs);
        if (code < 0)
            return code;

        /* Initialize stick/arc font instances */
        hpgl_initialize_stick_fonts(pcs);

        /* intialize subpolygon started hack flag */
        pcs->g.subpolygon_started = false;

        /* indicates a line down operation has been done in polygon
           mode */
        pcs->g.have_drawn_in_path = false;
        /* execute only the implicit portion of IN */
        code = hpgl_IN_implicit(pcs);
        if (code < 0)
            return code;

        /* we select the default pen 1 here, oddly, IN does not select
           the default pen even though it sets pen widths and units of
           measure */
        pcs->g.pen.selected = 1;
    }
    /* NB check all of these */
    if ((type & pcl_reset_page_params) != 0) {
        /* provide default anchor point, plot size and picture frame
           size.  Oddly HP does not reset the scaling parameters
           when the page size is changed. */
        int scale_type = pcs->g.scaling_type;
        hpgl_scaling_params_t params = pcs->g.scaling_params;

        hpgl_default_coordinate_system(pcs);

        /* restore the scaling parameter. */
        pcs->g.scaling_type = scale_type;
        pcs->g.scaling_params = params;

        hpgl_args_setup(&hpgl_args);
        code = hpgl_IW(&hpgl_args, pcs);
        if (code < 0)
            return code;
        hpgl_args_set_int(&hpgl_args, 0);
        code = hpgl_PM(&hpgl_args, pcs);
        if (code < 0)
            return code;
        hpgl_args_set_int(&hpgl_args, 2);
        code = hpgl_PM(&hpgl_args, pcs);
        if (code < 0)
            return code;
        hpgl_args_setup(&hpgl_args);
        code = hpgl_IP(&hpgl_args, pcs);
        if (code < 0)
            return code;
    }

    if ((type & pcl_reset_picture_frame) != 0) {
        /* this shouldn't happen.  Picture frame side effects are
           handled directly by the command picture frame command. */
        dmprintf(pcs->memory, "PCL reset picture frame received\n");
    }

    if ((type & pcl_reset_overlay) != 0) {
        code = hpgl_reset_overlay(pcs);
        if (code < 0)
            return code;
    }

    if ((type & (pcl_reset_plot_size)) != 0) {
        /* this shouldn't happen.  Plot size side effects are handled
           directly by the command picture frame command. */
        dmprintf(pcs->memory, "PCL reset plot received\n");
    }

    if ((type & (pcl_reset_permanent)) != 0) {
        gx_path_free(&pcs->g.polygon.buffer.path,
                     "hpgl_do_reset polygon buffer");
        /* if we have allocated memory for a stick font free the memory */
        hpgl_free_stick_fonts(pcs);
    }
    return 0;
}

/* ------ Copy the HP-GL/2 state for macro call/overlay/exit. */

static int
hpgl_do_copy(pcl_state_t * psaved,
             const pcl_state_t * pcs, pcl_copy_operation_t operation)
{
    if ((operation & pcl_copy_after) != 0) {
        /* Don't restore the polygon buffer. (Copy from pcs to psaved.)
         * path->segments is reference counted!
         */
        memcpy(&psaved->g.polygon.buffer.path, &pcs->g.polygon.buffer.path,
               sizeof(gx_path));
    }
    return 0;
}

const pcl_init_t pginit_init = { 0, hpgl_do_reset, hpgl_do_copy };
