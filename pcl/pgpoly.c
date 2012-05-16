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

/* pgpoly.c -  HP-GL/2 polygon commands */

#include "std.h"
#include "pcparse.h"
#include "pgmand.h"
#include "pgdraw.h"
#include "pggeom.h"
#include "pgmisc.h"
#include "pcpatrn.h"
#include "gspath.h"
#include "gscoord.h"

/* ------ Internal procedures ------ */

/* Define fill/edge and absolute/relative flags. */
#define DO_EDGE 1
#define DO_RELATIVE 2

/* clear the polygon buffer by entering and exiting polygon mode. */
static int
hpgl_clear_polygon_buffer(hpgl_args_t *pargs, hpgl_state_t *pgls)
{
    hpgl_args_setup(pargs);
    hpgl_call(hpgl_PM(pargs, pgls));
    hpgl_args_set_int(pargs,2);
    hpgl_call(hpgl_PM(pargs, pgls));
    return 0;
}

/* Build a rectangle in polygon mode used by (EA, ER, RA, RR). */
static int
hpgl_rectangle(hpgl_args_t *pargs,  hpgl_state_t *pgls, int flags, bool do_poly)
{	hpgl_real_t x2, y2;
        if ( !hpgl_arg_units(pgls->memory, pargs, &x2) ||
             !hpgl_arg_units(pgls->memory, pargs, &y2) ||
             current_units_out_of_range(x2) ||
             current_units_out_of_range(y2) ) {
            hpgl_call(hpgl_clear_polygon_buffer(pargs, pgls));
            return e_Range;
        }

        if ( flags & DO_RELATIVE )
          {
            x2 += pgls->g.pos.x;
            y2 += pgls->g.pos.y;
          }

        if ( do_poly ) {
            hpgl_args_setup(pargs);
            /* enter polygon mode. */
            hpgl_call(hpgl_PM(pargs, pgls));
        }

        /* do the rectangle */
        {
          hpgl_real_t x1 = pgls->g.pos.x;
          hpgl_real_t y1 = pgls->g.pos.y;

          hpgl_call(hpgl_add_point_to_path(pgls, x1, y1, hpgl_plot_move_absolute, true));
          hpgl_call(hpgl_add_point_to_path(pgls, x2, y1, hpgl_plot_draw_absolute, true));
          hpgl_call(hpgl_add_point_to_path(pgls, x2, y2, hpgl_plot_draw_absolute, true));
          hpgl_call(hpgl_add_point_to_path(pgls, x1, y2, hpgl_plot_draw_absolute, true));
          hpgl_call(hpgl_close_current_path(pgls));
        }

        /* exit polygon mode PM2 */
        if ( do_poly ) {
            hpgl_args_set_int(pargs,2);
            hpgl_call(hpgl_PM(pargs, pgls));
        }
        return 0;
}

/* Fill or edge a wedge (EW, WG). */
static int
hpgl_wedge(hpgl_args_t *pargs, hpgl_state_t *pgls)
{	hpgl_real_t radius, start, sweep, chord = 5.0;

        if ( !hpgl_arg_units(pgls->memory, pargs, &radius) ||
             !hpgl_arg_c_real(pgls->memory, pargs, &start) ||
             !hpgl_arg_c_real(pgls->memory, pargs, &sweep) || sweep < -360 || sweep > 360
             ) {
            hpgl_call(hpgl_clear_polygon_buffer(pargs, pgls));
            return e_Range;
        }

        hpgl_arg_c_real(pgls->memory, pargs, &chord);

        /* enter polygon mode */
        hpgl_args_setup(pargs);
        hpgl_call(hpgl_PM(pargs, pgls));

        if ( sweep > 359.9 || sweep < -359.9) {
            floatp num_chordsf = 360 / chord;
            /* match hp 4600 rounding, precompute since regular arc rounding is different. */
            floatp intpart;
            int num_chords = (int)((modf(num_chordsf, &intpart) < 0.06) ? intpart : intpart+1);
            floatp integral_chord_angle = fabs(sweep / num_chords);

            hpgl_call(hpgl_add_arc_to_path(pgls, pgls->g.pos.x, pgls->g.pos.y,
                                           radius, start, 360.0, integral_chord_angle, true,
                                           hpgl_plot_draw_absolute, true));
        }
        else
        /* draw the 2 lines and the arc using 3 point this does seem
           convoluted but it does guarantee that the endpoint lines
           for the vectors and the arc endpoints are coincident. */
          {
            hpgl_real_t x1, y1, x2, y2, x3, y3;
            hpgl_compute_vector_endpoints(radius, pgls->g.pos.x, pgls->g.pos.y,
                                          start, &x1, &y1);
            hpgl_compute_vector_endpoints(radius, pgls->g.pos.x, pgls->g.pos.y,
                                          (start + (sweep / 2.0)), &x2, &y2);
            hpgl_compute_vector_endpoints(radius, pgls->g.pos.x, pgls->g.pos.y,
                                          (start + sweep), &x3, &y3);
            hpgl_call(hpgl_add_point_to_path(pgls, pgls->g.pos.x, pgls->g.pos.y,
                                             hpgl_plot_move_absolute, true));
            hpgl_call(hpgl_add_point_to_path(pgls, x1, y1,
                                             hpgl_plot_draw_absolute, true));
            hpgl_call(hpgl_add_arc_3point_to_path(pgls,
                                                  x1, y1,
                                                  x2, y2,
                                                  x3, y3, chord,
                                                  hpgl_plot_draw_absolute));
          }

        hpgl_call(hpgl_close_current_path(pgls));
        hpgl_args_set_int(pargs,2);
        hpgl_call(hpgl_PM(pargs, pgls));

        return 0;
}

/* ------ Commands ------ */

/* EA x,y; */
int
hpgl_EA(hpgl_args_t *pargs, hpgl_state_t *pgls)
{
        hpgl_call(hpgl_rectangle(pargs, pgls, DO_EDGE, true));
        hpgl_call(hpgl_copy_polygon_buffer_to_current_path(pgls));
        gs_sethpglpathmode(pgls->pgs, true);
        hpgl_call(hpgl_draw_current_path(pgls, hpgl_rm_vector));
        gs_sethpglpathmode(pgls->pgs, false);
        return 0;
}

/* EP; */
int
hpgl_EP(hpgl_args_t *pargs, hpgl_state_t *pgls)
{
        /* preserve the current path and copy the polygon buffer to
           the current path */
        hpgl_call(hpgl_gsave(pgls));
        hpgl_call(hpgl_copy_polygon_buffer_to_current_path(pgls));
        gs_sethpglpathmode(pgls->pgs, true);
        hpgl_call(hpgl_draw_current_path(pgls, hpgl_rm_vector_no_close));
        gs_sethpglpathmode(pgls->pgs, false);
        hpgl_call(hpgl_grestore(pgls));
        return 0;
}

/* ER dx,dy; */
int
hpgl_ER(hpgl_args_t *pargs, hpgl_state_t *pgls)
{
        hpgl_call(hpgl_rectangle(pargs, pgls, DO_RELATIVE, true));
        hpgl_call(hpgl_copy_polygon_buffer_to_current_path(pgls));
        gs_sethpglpathmode(pgls->pgs, true);
        hpgl_call(hpgl_draw_current_path(pgls, hpgl_rm_vector));
        gs_sethpglpathmode(pgls->pgs, false);
        return 0;
}

/* EW radius,astart,asweep[,achord]; */
int
hpgl_EW(hpgl_args_t *pargs, hpgl_state_t *pgls)
{
        hpgl_call(hpgl_wedge(pargs, pgls));
        hpgl_call(hpgl_copy_polygon_buffer_to_current_path(pgls));
        gs_sethpglpathmode(pgls->pgs, true);
        hpgl_call(hpgl_draw_current_path(pgls, hpgl_rm_vector));
        gs_sethpglpathmode(pgls->pgs, false);
        return 0;
}

static hpgl_rendering_mode_t
hpgl_get_poly_render_mode(
    hpgl_state_t *              pgls
)
{
    hpgl_FT_pattern_source_t    type = pgls->g.fill.type;

    return ( ((type == hpgl_FT_pattern_one_line) ||
              (type == hpgl_FT_pattern_two_lines)  )
                ? hpgl_rm_clip_and_fill_polygon
                : hpgl_rm_polygon );
}

/* FP method; */
/* FP; */
int
hpgl_FP(hpgl_args_t *pargs, hpgl_state_t *pgls)
{	int method = 0;

    if ( hpgl_arg_c_int(pgls->memory, pargs, &method) && (method & ~1) ) {
        hpgl_call(hpgl_clear_polygon_buffer(pargs, pgls));
        return e_Range;
    }
    pgls->g.fill_type = (method == 0) ?
        hpgl_even_odd_rule : hpgl_winding_number_rule;
    hpgl_call(hpgl_copy_polygon_buffer_to_current_path(pgls));
    gs_sethpglpathmode(pgls->pgs, true);
    hpgl_call(hpgl_draw_current_path(pgls,
                                     hpgl_get_poly_render_mode(pgls)));
    gs_sethpglpathmode(pgls->pgs, false);
    return 0;
}

/* close a subpolygon; PM1 or CI inside of a polygon */
int
hpgl_close_subpolygon(hpgl_state_t *pgls)
{
    gs_point first, last;
    gs_point point;
    gs_fixed_point first_device;

    if ( pgls->g.polygon_mode ) {

        if ( gx_path_subpath_start_point(gx_current_path(pgls->pgs),
                                         &first_device) >= 0 ) {
            first.x = fixed2float(first_device.x);
            first.y = fixed2float(first_device.y);

            /* get gl/2 current position -- always current units */
            hpgl_call(hpgl_get_current_position(pgls, &last));
            /* convert to device space using the current ctm */
            hpgl_call(gs_transform(pgls->pgs, last.x, last.y, &last));
            /*
             * if the first and last are the same close the path (i.e
             * force gs to apply join and miter)
             */
            if (equal(first.x, last.x) && equal(first.y, last.y)) {
                hpgl_call(gs_closepath(pgls->pgs));
            }
            else {
                /* explicitly close the path if the pen has been down */
                if ( pgls->g.have_drawn_in_path ) {
                    hpgl_call(hpgl_close_current_path(pgls));

                /* update current position to the first point in sub-path,
                 * should be the same as last point after close path
                 * needed for relative moves after close of unclosed polygon
                 */
                hpgl_call(gs_itransform(pgls->pgs, first.x, first.y, &point));
                hpgl_call(hpgl_set_current_position(pgls, &point));
                hpgl_call(hpgl_update_carriage_return_pos(pgls));
                }
            }
            /* remain in poly mode, this shouldn't be necessary */
            pgls->g.polygon_mode = true;
        }
        pgls->g.subpolygon_started = true;
    }
    return 0;
}

/* PM op; */
int
hpgl_PM(hpgl_args_t *pargs, hpgl_state_t *pgls)
{	int op;

        if ( hpgl_arg_c_int(pgls->memory, pargs, &op) == 0 )
          op = 0;

        switch( op )
          {
          case 0 :
            /* draw the current path if there is one */
            hpgl_call(hpgl_draw_current_path(pgls, hpgl_rm_vector));
            /* clear the polygon buffer as well */
            gx_path_new(&pgls->g.polygon.buffer.path);
            /* global flag to indicate that we are in polygon mode */
            pgls->g.polygon_mode = true;
            /* save the pen state, to be restored by PM2 */
            hpgl_save_pen_state(pgls,
                                &pgls->g.polygon.pen_state,
                                hpgl_pen_down | hpgl_pen_pos);
            gs_sethpglpathmode(pgls->pgs, true);
            break;
          case 1 :
              hpgl_call(hpgl_close_subpolygon(pgls));
              break;
          case 2 :
              if ( pgls->g.polygon_mode ) {
                  /* explicitly close the path if the pen is down */
                  if ( pgls->g.move_or_draw == hpgl_pen_down
                       && pgls->g.have_drawn_in_path )
                      hpgl_call(hpgl_close_current_path(pgls));
                  /* make a copy of the path and clear the current path */
                  hpgl_call(hpgl_copy_current_path_to_polygon_buffer(pgls));
                  hpgl_call(hpgl_clear_current_path(pgls));
                  /* return to vector mode */
                  pgls->g.polygon_mode = false;
                  /* restore the pen state */
                  hpgl_restore_pen_state(pgls,
                                         &pgls->g.polygon.pen_state,
                                         hpgl_pen_down | hpgl_pen_pos);
              }
              gs_sethpglpathmode(pgls->pgs, false);
              break;
          default:
            return e_Range;
          }
        return 0;
}

/* RA x,y; */
int
hpgl_RA(hpgl_args_t *pargs, hpgl_state_t *pgls)
{
        hpgl_call(hpgl_rectangle(pargs, pgls, 0, true));
        hpgl_call(hpgl_copy_polygon_buffer_to_current_path(pgls));
        gs_sethpglpathmode(pgls->pgs, true);
        hpgl_call(hpgl_draw_current_path(pgls,
                                         hpgl_get_poly_render_mode(pgls)));
        gs_sethpglpathmode(pgls->pgs, false);
        return 0;
}

/* RR dx,dy;*/
int
hpgl_RR(hpgl_args_t *pargs, hpgl_state_t *pgls)
{
        hpgl_call(hpgl_rectangle(pargs, pgls, DO_RELATIVE, true));
        hpgl_call(hpgl_copy_polygon_buffer_to_current_path(pgls));
        gs_sethpglpathmode(pgls->pgs, true);
        hpgl_call(hpgl_draw_current_path(pgls,
                                         hpgl_get_poly_render_mode(pgls)));
        gs_sethpglpathmode(pgls->pgs, false);
        return 0;
}

/* RQ dx,dy;*/
int
hpgl_RQ(hpgl_args_t *pargs, hpgl_state_t *pgls)
{
    /* contary to the specification HP uses default pixel placement
       with RQ */
    byte save_pp = pgls->pp_mode;
    pgls->pp_mode = 0;
    hpgl_call(hpgl_rectangle(pargs, pgls, DO_RELATIVE, false));
    gs_sethpglpathmode(pgls->pgs, true);
    hpgl_call(hpgl_draw_current_path(pgls,
                                     hpgl_get_poly_render_mode(pgls)));
    /* restore saved pixel placement mode */
    gs_sethpglpathmode(pgls->pgs, false);
    pgls->pp_mode = save_pp;
    return 0;
}

/* WG radius,astart,asweep[,achord]; */
int
hpgl_WG(hpgl_args_t *pargs, hpgl_state_t *pgls)
{
        hpgl_call(hpgl_wedge(pargs, pgls));
        hpgl_call(hpgl_copy_polygon_buffer_to_current_path(pgls));
        gs_sethpglpathmode(pgls->pgs, true);
        hpgl_call(hpgl_draw_current_path(pgls,
                                         hpgl_get_poly_render_mode(pgls)));
        gs_sethpglpathmode(pgls->pgs, false);
        return 0;
}

/* Initialization */
static int
pgpoly_do_registration(
    pcl_parser_state_t *pcl_parser_state,
    gs_memory_t *mem)
{		/* Register commands */
    DEFINE_HPGL_COMMANDS(mem)
          HPGL_COMMAND('E', 'A', hpgl_EA, hpgl_cdf_lost_mode_cleared|hpgl_cdf_pcl_rtl_both),
          HPGL_COMMAND('E', 'P', hpgl_EP, hpgl_cdf_pcl_rtl_both),
          HPGL_COMMAND('E', 'R', hpgl_ER, hpgl_cdf_lost_mode_cleared|hpgl_cdf_pcl_rtl_both),
          HPGL_COMMAND('E', 'W', hpgl_EW, hpgl_cdf_lost_mode_cleared|hpgl_cdf_pcl_rtl_both),
          HPGL_COMMAND('F', 'P', hpgl_FP, hpgl_cdf_pcl_rtl_both),
          HPGL_COMMAND('P', 'M', hpgl_PM, hpgl_cdf_polygon|hpgl_cdf_lost_mode_cleared|hpgl_cdf_pcl_rtl_both),
          HPGL_COMMAND('R', 'A', hpgl_RA, hpgl_cdf_lost_mode_cleared|hpgl_cdf_pcl_rtl_both),
          HPGL_COMMAND('R', 'R', hpgl_RR, hpgl_cdf_lost_mode_cleared|hpgl_cdf_pcl_rtl_both),
          HPGL_COMMAND('R', 'Q', hpgl_RQ, hpgl_cdf_lost_mode_cleared|hpgl_cdf_pcl_rtl_both),
          HPGL_COMMAND('W', 'G', hpgl_WG, hpgl_cdf_lost_mode_cleared|hpgl_cdf_pcl_rtl_both),
        END_HPGL_COMMANDS
        return 0;
}
const pcl_init_t pgpoly_init = {
  pgpoly_do_registration, 0
};
