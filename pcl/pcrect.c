/*
 * Copyright (C) 1996, 1997 Aladdin Enterprises.
 * All rights reserved.
 *
 * Unauthorized use, copying, and/or distribution prohibited.
 */

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
#include "gspaint.h"
#include "gsrop.h"

/*
 * ESC * c <dp> H
 */
  private int
pcl_horiz_rect_size_decipoints(
    pcl_args_t *    pargs,
    pcl_state_t *   pcls
)
{
    pcls->rectangle.x = float_arg(pargs) * 10;
    return 0;
}

/*
 * ESC * c <units> A
 */
  private int
pcl_horiz_rect_size_units(
    pcl_args_t *    pargs,
    pcl_state_t *   pcls
)
{
    pcls->rectangle.x = int_arg(pargs) * pcls->uom_cp;
    return 0;
}

/*
 * ESC * c <dp> V
 */
  private int
pcl_vert_rect_size_decipoints(
    pcl_args_t *    pargs,
    pcl_state_t *   pcls
)
{
    pcls->rectangle.y = float_arg(pargs) * 10;
    return 0;
}

/*
 * ESC * c B
 */
  private int
pcl_vert_rect_size_units(
    pcl_args_t *    pargs,
    pcl_state_t *   pcls
)
{
    pcls->rectangle.y = int_arg(pargs) * pcls->uom_cp;
    return 0;
}

/*
 * ESC * c <fill_enum> P
 */
  private int
pcl_fill_rect_area(
    pcl_args_t *            pargs,
    pcl_state_t *           pcls
)
{
    gs_state *              pgs = pcls->pgs;
    pcl_pattern_source_t    type = (pcl_pattern_source_t)int_arg(pargs);
    bool                    pattern_transparent = pcls->pattern_transparent;
    int                     id = pcls->pattern_id;
    int                     code = 0;

    /*
     * Rectangles have two special properties with respect to patterns:
     *
     *     A white rectangle overrides pattern transparency
     *
     *     a non-zero, non-existent pattern specification causes the command
     *        to be ignored, UNLESS this is the current pattern. This only
     *        applies to cross-hatch and user-defined patterns, as the
     *        pattern id. is interpreted as an intensity level for shade
     *        patterns, and patterns >= 100% are considered white.
     */
    if ((type == pcl_pattern_solid_white) || (type == pcl_pattern_solid_frgrnd))
        pcls->pattern_transparent = false;
    else if (type == pcl_pattern_cross_hatch) {
        if (pcl_pattern_get_cross(id) == 0)
            return 0;
    } else if (type == pcl_pattern_user_defined) {
        if (pcl_pattern_get_pcl_uptrn(id) == 0)
            return 0;
    } else if (type == pcl_pattern_current_pattern) {
        type = pcls->pattern_type;
        id = pcls->current_pattern_id;
    } else if (type != pcl_pattern_shading)
        return 0;

    /* set up the graphic state; render the rectangle */
    if ( ((code = pcl_set_drawing_color(pcls, type, id, false)) >= 0) &&
         ((code = pcl_set_graphics_state(pcls)) >= 0)                   ) {
        gs_rect r;

	r.p.x = pcl_cap.x;
	r.p.y = pcl_cap.y;
	r.q.x = r.p.x + pcls->rectangle.x;
	r.q.y = r.p.y + pcls->rectangle.y;
        if (r.q.x > pcls->xfm_state.pd_size.x)
            r.q.x = pcls->xfm_state.pd_size.x;
        if (r.q.y > pcls->xfm_state.pd_size.y)
            r.q.y = pcls->xfm_state.pd_size.y;

        /* the graphic library prints zero-dimension rectangles */
        if ((r.q.x > r.p.x) && (r.q.y > r.p.y)) {
	    code = gs_rectfill(pgs, &r, 1);
            pcls->have_page = true;
        }
    }

    pcls->pattern_transparent = pattern_transparent;
    return code;
}

/*
 * Initialization
 */
  private int
pcrect_do_init(
    gs_memory_t *   mem
)
{
    /* Register commands */
    DEFINE_CLASS('*')
    {
        'c', 'H',
	PCL_COMMAND( "Horizontal Rectangle Size Decipoints",
		     pcl_horiz_rect_size_decipoints,
		     pca_neg_error | pca_big_error
                     )
    },
    {
        'c', 'A',
	PCL_COMMAND( "Horizontal Rectangle Size Units",
		      pcl_horiz_rect_size_units,
		      pca_neg_error | pca_big_error
                      )
    },
    {
        'c', 'V',
	PCL_COMMAND( "Vertical Rectangle Size Decipoint",
		     pcl_vert_rect_size_decipoints,
		     pca_neg_error | pca_big_error
                     )
    },
    {
        'c', 'B',
	PCL_COMMAND( "Vertical Rectangle Size Units",
		     pcl_vert_rect_size_units,
		     pca_neg_error | pca_big_error
                     )
    },
    {
        'c', 'P',
        PCL_COMMAND( "Fill Rectangular Area",
                     pcl_fill_rect_area,
		     pca_neg_ignore | pca_big_ignore
                     )
    },
    END_CLASS
    return 0;
}

  private void
pcrect_do_reset(
    pcl_state_t *       pcls,
    pcl_reset_type_t    type
)
{
    if ( (type & (pcl_reset_initial | pcl_reset_printer)) != 0 ) {
	pcls->rectangle.x = 0;
	pcls->rectangle.y = 0;
    }
}

const pcl_init_t    pcrect_init = { pcrect_do_init, pcrect_do_reset, 0 };
