/* Copyright (C) 1996, 1997, 1998 Aladdin Enterprises.  All rights
   reserved.  Unauthorized use, copying, and/or distribution
   prohibited.  */

/* pginit.c - Initialization and resetting for HP-GL/2. */
#include "gx.h"
#include "gsmatrix.h"		/* for gsstate.h */
#include "gsmemory.h"		/* for gsstate.h */
#include "gsstate.h"            /* for gs_setlimitclamp */
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
hpgl_default_font_params(
    pcl_font_selection_t *  pfs
)
{
    pfs->params.symbol_set = 277;	/* Roman-8 */
    pfs->params.proportional_spacing = false;
    pl_fp_set_pitch_per_inch(&pfs->params, 9);
    pfs->params.height_4ths = (int)(11.5*4);
    pfs->params.style = 0;
    pfs->params.stroke_weight = 0;
    pfs->params.typeface_family = 48;	/* stick font */
    pfs->font = 0;			/* not looked up yet */
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
  private void
hpgl_default_coordinate_system(
    hpgl_state_t *  pcls
)
{
    pcls->g.plot_width = pcls->g.picture_frame_width
                       = pcls->xfm_state.lp_size.x;
    pcls->g.plot_height = pcls->g.picture_frame_height = pcls->margins.length;
    pcls->g.picture_frame.anchor_point.x = pcls->margins.left;
    pcls->g.picture_frame.anchor_point.y = pcls->margins.top;

    /* The default coordinate system is absolute with the origin at 0,0 */
    pcls->g.move_or_draw = hpgl_plot_move;
    pcls->g.relative_coords = hpgl_plot_absolute;
    pcls->g.pos.x = 0.0;
    pcls->g.pos.y = 0.0;
    pcls->g.scaling_type = hpgl_scaling_none;
    pcls->g.plot_size_vertical_specified = false;
    pcls->g.plot_size_horizontal_specified = false;
    return;
}

/*
 * Reset all the fill patterns to solid fill.
 */
  void
hpgl_default_all_fill_patterns(
    hpgl_state_t *  pgls
)
{
    int             i;

    for (i = 1; i <= 8; ++i)
        (void)pcl_pattern_RF(i, NULL, pgls);
}

  void
hpgl_do_reset(
    pcl_state_t *       pcls,
    pcl_reset_type_t    type
)
{
    /* pgframe.c (Chapter 18) */
    hpgl_args_t         hpgl_args;

    if ((type & (pcl_reset_initial | pcl_reset_printer | pcl_reset_cold)) != 0 ) {
        if ((type & (pcl_reset_initial | pcl_reset_cold)) != 0) {
            gx_path_alloc_contained( &pcls->g.polygon.buffer.path,
                                     pcls->memory,
                                     "hpgl_do_reset"
                                     );

	    /*
             * HAS This is required for GL/2 but probably should
             * be maintained locally in gl/2's state machinery
             */
	    gs_setlimitclamp(pcls->pgs, true);
	} else
            gx_path_new(&pcls->g.polygon.buffer.path);

	/* provide default anchor point, plot size and picture frame size */
	hpgl_default_coordinate_system(pcls);

	/* we should not have a path at this point but we make sure */
	hpgl_clear_current_path(pcls);

	/* Initialize stick/arc font instances */
	pcls->g.stick_font[0][0].pfont =
	  pcls->g.stick_font[0][1].pfont =
	  pcls->g.stick_font[1][0].pfont =
	  pcls->g.stick_font[1][1].pfont = 0;

	/* execute only the implicit portion of IN */
	hpgl_IN_implicit(pcls);
    }

    if ((type & pcl_reset_page_params) != 0) {
	hpgl_default_coordinate_system(pcls);
	hpgl_args_setup(&hpgl_args);
	hpgl_IW(&hpgl_args, pcls);
	hpgl_args_set_int(&hpgl_args,0);
	hpgl_PM(&hpgl_args, pcls);
	hpgl_args_set_int(&hpgl_args,2);
	hpgl_PM(&hpgl_args, pcls);
    }

    if ((type & pcl_reset_picture_frame) != 0) {
	hpgl_args_setup(&hpgl_args);
	hpgl_IP(&hpgl_args, pcls);
	hpgl_args_setup(&hpgl_args);
	hpgl_IW(&hpgl_args, pcls);
	hpgl_args_set_int(&hpgl_args,0);
	hpgl_PM(&hpgl_args, pcls);
	hpgl_args_set_int(&hpgl_args,2);
	hpgl_PM(&hpgl_args, pcls);
    }

    if ((type & pcl_reset_overlay) != 0) 
        hpgl_reset_overlay(pcls);

    if ((type & (pcl_reset_plot_size)) != 0) {
	/* HAS reset picture frame scaling factors */
    }

    return;
}

/* ------ Copy the HP-GL/2 state for macro call/overlay/exit. */

  private int
hpgl_do_copy(
    pcl_state_t *           psaved,
    const pcl_state_t *     pcls,
    pcl_copy_operation_t    operation
)
{
    if ((operation & pcl_copy_after) != 0) {
         /* Don't restore the polygon buffer. (Copy from pcls to psaved.) */
    }
    return 0;
}

const pcl_init_t    pginit_init = { 0, hpgl_do_reset, hpgl_do_copy };
