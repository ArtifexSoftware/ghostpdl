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

/* pgvector.c */
/* HP-GL/2 vector commands */

#include "stdio_.h"             /* for gdebug.h */
#include "gdebug.h"
#include "pcparse.h"
#include "pgmand.h"
#include "pggeom.h"
#include "pgdraw.h"
#include "pgmisc.h"
#include "gspath.h"
#include "gscoord.h"
#include "math_.h"

/* ------ Internal procedures ------ */

/* Draw an arc (AA, AR). */
 static int
hpgl_arc(hpgl_args_t *pargs, hpgl_state_t *pgls, bool relative)
{
        hpgl_real_t x_center, y_center, sweep, x_current, y_current, chord_angle = 5;
        hpgl_real_t radius, start_angle;

        if ( !hpgl_arg_units(pgls->memory, pargs, &x_center) ||
             !hpgl_arg_units(pgls->memory, pargs, &y_center) ||
             !hpgl_arg_c_real(pgls->memory, pargs, &sweep)
             )
          return e_Range;

        hpgl_arg_c_real(pgls->memory, pargs, &chord_angle);

        if ( current_units_out_of_range(x_center) ||
             current_units_out_of_range(y_center) )
            return 0;

        x_current = pgls->g.pos.x;
        y_current = pgls->g.pos.y;

        if ( relative )
          {
            x_center += x_current;
            y_center += y_current;
          }

        radius =
          hpgl_compute_distance(x_current, y_current, x_center, y_center);

        start_angle = radians_to_degrees *
          hpgl_compute_angle(x_current - x_center, y_current - y_center);

        hpgl_call(hpgl_add_arc_to_path(pgls, x_center, y_center,
                                       radius, start_angle, sweep,
                                       chord_angle,
                                       false,
                                       pgls->g.move_or_draw, true));

        hpgl_call(hpgl_update_carriage_return_pos(pgls));
        return 0;
}

/* Draw a 3-point arc (AT, RT). */
 static int
hpgl_arc_3_point(hpgl_args_t *pargs, hpgl_state_t *pgls, bool relative)
{
        hpgl_real_t x_start = pgls->g.pos.x, y_start = pgls->g.pos.y;
        hpgl_real_t x_inter, y_inter, x_end, y_end;
        hpgl_real_t chord_angle = 5;

        if ( !hpgl_arg_units(pgls->memory, pargs, &x_inter) ||
             !hpgl_arg_units(pgls->memory, pargs, &y_inter) ||
             !hpgl_arg_units(pgls->memory, pargs, &x_end) ||
             !hpgl_arg_units(pgls->memory, pargs, &y_end)
           )
          return e_Range;

        hpgl_arg_c_real(pgls->memory, pargs, &chord_angle);

        if ( relative )
          {
            x_inter += x_start;
            y_inter += y_start;
            x_end += x_start;
            y_end += y_start;
          }

        hpgl_call(hpgl_add_arc_3point_to_path(pgls,
                                              x_start, y_start,
                                              x_inter, y_inter,
                                              x_end, y_end, chord_angle,
                                              pgls->g.move_or_draw));
        hpgl_call(hpgl_update_carriage_return_pos(pgls));
        return 0;
}

/* Draw a Bezier (BR, BZ). */
static int
hpgl_bezier(hpgl_args_t *pargs, hpgl_state_t *pgls, bool relative)
{
        hpgl_real_t x_start, y_start;

        /*
         * Since these commands take an arbitrary number of arguments,
         * we reset the argument bookkeeping after each group.
         */

        for ( ; ; )
          {
            hpgl_real_t coords[6];
            int i;

            for ( i = 0; i < 6 && hpgl_arg_units(pgls->memory, pargs, &coords[i]); ++i )
              ;
            switch ( i )
              {
              case 0:           /* done */
                hpgl_call(hpgl_update_carriage_return_pos(pgls));
                /* draw the path */
                if ( !pgls->g.polygon_mode ) {
                    /* apparently only round and beveled joins are
                       allowed 5 is bevel and 4 is round */
                    int save_join = pgls->g.line.join;
                    if ( pgls->g.line.join != 1 && pgls->g.line.join != 4 )
                        pgls->g.line.join = 1; /* bevel */
                    hpgl_call(hpgl_draw_current_path(pgls, hpgl_rm_vector));
                    pgls->g.line.join = save_join;
                }
                return 0;
              case 6:
                break;
              default:
                hpgl_call(hpgl_update_carriage_return_pos(pgls));
                return e_Range;
              }

            x_start = pgls->g.pos.x;
            y_start = pgls->g.pos.y;

            if ( relative )
              hpgl_call(hpgl_add_bezier_to_path(pgls, x_start, y_start,
                                                x_start + coords[0],
                                                y_start + coords[1],
                                                x_start + coords[2],
                                                y_start + coords[3],
                                                x_start + coords[4],
                                                y_start + coords[5],
                                                pgls->g.move_or_draw));
            else
              hpgl_call(hpgl_add_bezier_to_path(pgls, x_start, y_start,
                                                coords[0], coords[1],
                                                coords[2], coords[3],
                                                coords[4], coords[5],
                                                pgls->g.move_or_draw));
            /* Prepare for the next set of points. */
            hpgl_args_init(pargs);
          }
}

/* Plot points, symbols, or lines (PA, PD, PR, PU). */
static int
hpgl_plot(hpgl_args_t *pargs, hpgl_state_t *pgls,
          hpgl_plot_function_t func, bool is_PA)
{
    /*
     * Since these commands take an arbitrary number of arguments,
     * we reset the argument bookkeeping after each group.
     */
    /*    bool first_loop = true; */
    hpgl_real_t x, y;
    if ( hpgl_plot_is_move(func) && !pgls->g.polygon_mode ) {
        hpgl_call(hpgl_close_path(pgls));
    }
    while ( hpgl_arg_units(pgls->memory, pargs, &x) &&
            hpgl_arg_units(pgls->memory, pargs, &y) ) {

        if ( current_units_out_of_range(x) ||
             current_units_out_of_range(y) )
            return e_Range;

        /* move with arguments closes path */
        if ( pargs->phase == 0
             && (hpgl_plot_is_move(func) || pgls->g.subpolygon_started )) {
            hpgl_call(hpgl_close_path(pgls));
        }
        pargs->phase = 1;       /* we have arguments */
        /* first point of a subpolygon is a pen up - absurd */
        if ( pgls->g.subpolygon_started ) {
            pgls->g.subpolygon_started = false;
            pgls->g.have_drawn_in_path = false;
            hpgl_call(hpgl_add_point_to_path(pgls, x, y,
                                             hpgl_plot_move | pgls->g.relative_coords,
                                             true));
        }
        else {
            hpgl_call(hpgl_add_point_to_path(pgls, x, y, func, true));
            if ( hpgl_plot_is_draw(func) )
                pgls->g.have_drawn_in_path = true;
        }
        /* Prepare for the next set of points. */
        if ( pgls->g.symbol_mode != 0 )
            hpgl_call(hpgl_print_symbol_mode_char(pgls));
        hpgl_args_init(pargs);
    }

    /* Check for no argument, no polygon, absolute PD will cause a
       point to be added to the path.  A PA command without arguments
       will not add a point.  NB this probably needs more testing. */

    if ( !pargs->phase && hpgl_plot_is_absolute(func) && !pgls->g.polygon_mode && !is_PA) {
        gs_point cur_point;
        hpgl_call(hpgl_get_current_position(pgls, &cur_point));
        hpgl_call(hpgl_add_point_to_path(pgls, cur_point.x,
                                         cur_point.y, func, true));
    }
    if ( pgls->g.symbol_mode != 0 )
        hpgl_call(hpgl_print_symbol_mode_char(pgls));

    /* don't update if no arguments */
    if ( pargs->phase )
        hpgl_call(hpgl_update_carriage_return_pos(pgls));
    return 0;
}

/* ------ Commands ------ */
 static int
hpgl_draw_arc(hpgl_state_t *pgls)
{
        if ( !pgls->g.polygon_mode )
          hpgl_call(hpgl_draw_current_path(pgls, hpgl_rm_vector));
        return 0;
}

/* AA xcenter,ycenter,sweep[,chord]; */
 int
hpgl_AA(hpgl_args_t *pargs, hpgl_state_t *pgls)
{
        hpgl_call(hpgl_arc(pargs, pgls, false));
        hpgl_call(hpgl_draw_arc(pgls));
        return 0;
}

/* AR xcenter,ycenter,sweep[,chord]; */
 int
hpgl_AR(hpgl_args_t *pargs, hpgl_state_t *pgls)
{
        hpgl_call(hpgl_arc(pargs, pgls, true));
        hpgl_call(hpgl_draw_arc(pgls));
        return 0;
}

/* AT xinter,yinter,xend,yend[,chord]; */
 int
hpgl_AT(hpgl_args_t *pargs, hpgl_state_t *pgls)
{
        hpgl_call(hpgl_arc_3_point(pargs, pgls, false));
        hpgl_call(hpgl_draw_arc(pgls));
        return 0;
}

/* BR x1,y1,x2,y2,x3,y3...; */
 int
hpgl_BR(hpgl_args_t *pargs, hpgl_state_t *pgls)
{       return hpgl_bezier(pargs, pgls, true);
}

/* BZ x1,y1,x2,y2,x3,y3...; */
int
hpgl_BZ(hpgl_args_t *pargs, hpgl_state_t *pgls)
{       return hpgl_bezier(pargs, pgls, false);
}

/* CI radius[,chord]; */
int
hpgl_CI(hpgl_args_t *pargs, hpgl_state_t *pgls)
{
        hpgl_real_t radius, chord = 5;
        bool reset_ctm = true;
        gs_point pos;

        if ( !hpgl_arg_units(pgls->memory, pargs, &radius) )
            return e_Range;

        /* close existing path iff a draw exists in polygon path */
        if ( pgls->g.have_drawn_in_path && pgls->g.polygon_mode )
            hpgl_call(hpgl_close_subpolygon(pgls));

        /* center; closing subpolygon can move center */
        pos = pgls->g.pos;

        hpgl_arg_c_real(pgls->memory, pargs, &chord);
        /* draw the path here for line type 0, otherwise the first dot
           drawn in the circumference of the circle will be oriented
           in the same direction as the center dot */
        if ( !pgls->g.line.current.is_solid && (pgls->g.line.current.type == 0) )
            hpgl_call(hpgl_draw_current_path(pgls, hpgl_rm_vector));

        /* draw the arc/circle */
        hpgl_call(hpgl_add_arc_to_path(pgls, pos.x, pos.y,
                                       radius, 0.0, 360.0, chord, true,
                                       hpgl_plot_draw_absolute, reset_ctm));
        if ( !pgls->g.polygon_mode )
            hpgl_call(hpgl_draw_arc(pgls));

        /* end path, start new path by moving back to the center */
        hpgl_call(hpgl_close_current_path(pgls));
        hpgl_call(hpgl_add_point_to_path(pgls, pos.x, pos.y,
                                         hpgl_plot_move_absolute,
                                         true));
        pgls->g.have_drawn_in_path = false; /* prevent dot draw on close */
        hpgl_call(hpgl_set_current_position(pgls, &pos));

        if ( !pgls->g.polygon_mode )
            hpgl_call(hpgl_clear_current_path(pgls));

        return 0;
}

/* PA x,y...; */
int
hpgl_PA(hpgl_args_t *pargs, hpgl_state_t *pgls)
{

    if ( pgls->g.relative_coords != hpgl_plot_absolute ) {
        pgls->g.relative_coords = hpgl_plot_absolute;
        if ( !pgls->g.polygon_mode ) {
            hpgl_call(hpgl_draw_current_path(pgls, hpgl_rm_vector));
            hpgl_call(hpgl_clear_current_path(pgls));
        }
    }
    return hpgl_plot(pargs, pgls,
                     pgls->g.move_or_draw | hpgl_plot_absolute,
                     true /* is PA command */);
}

/* PD (d)x,(d)y...; */
int
hpgl_PD(hpgl_args_t *pargs, hpgl_state_t *pgls)
{
        pgls->g.move_or_draw = hpgl_plot_draw;
        return hpgl_plot(pargs, pgls,
                         hpgl_plot_draw | pgls->g.relative_coords,
                         false /* is PA command */);
}

/* PE (flag|value|coord)*; */
/*
 * We record the state of the command in the 'phase' as follows:
 */
enum {
        pe_pen_up = 1,  /* next coordinate are pen-up move */
        pe_absolute = 2,        /* next coordinates are absolute */
        pe_7bit = 4,            /* use 7-bit encoding */
        pe_entered = 8         /* we have entered PE once */
};

/* convert pe fixed to float accounting for fractional bits */
static inline floatp
pe_fixed2float(int32 x, int32 fbits)
{
    return ((floatp)x * (1.0 / pow(2, fbits)));
}

typedef enum {
    pe_okay,
    pe_NeedData,
    pe_SyntaxError,
} hpgl_pe_args_ret_t;

static hpgl_pe_args_ret_t pe_args(const gs_memory_t *mem, hpgl_args_t *, int);

static inline void
pe_clear_args(hpgl_args_t *pargs)
{
    pargs->pe_shift[0]  = pargs->pe_shift[1] = 0;
    pargs->pe_values[0] = pargs->pe_values[1] = 0;
    pargs->pe_indx = 0;
}

int
hpgl_PE(hpgl_args_t *pargs, hpgl_state_t *pgls)
{
    /*
     * To simplify the control structure here, we require that
     * the input buffer be large enough to hold 2 coordinates
     * in the base 32 encoding, i.e., 14 bytes.  We're counting on
     * there not being any whitespace within coordinate values....
     */
    const byte *p = pargs->source.ptr;
    const byte *rlimit = pargs->source.limit;
    /* count points to allow medium size paths, performance optimization
     * point_count_max should be smaller than input buffer
     */
    int point_count = 0;
    static const int point_count_max = 100;

    if ( pargs->phase == 0 ) {
        /* After PE is executed, the previous plotting mode (absolute or
           relative) is restored.  If the finnal move is made with the pen up,
           the pen remains in the up position; otherwise the pen is left in
           the down position.  At least HP documented this bug. */
        hpgl_save_pen_state(pgls, &pgls->g.pen_state, hpgl_pen_relative);
        pargs->phase |= pe_entered;
        pgls->g.fraction_bits = 0;
        pe_clear_args(pargs);
    }
    while ( p < rlimit ) {
        byte ch = *(pargs->source.ptr = ++p);
        switch ( ch & 127 /* per spec */ ) {
        case ';':
            hpgl_call(hpgl_update_carriage_return_pos(pgls));
            if ( pargs->phase & pe_entered )
                hpgl_restore_pen_state(pgls, &pgls->g.pen_state, hpgl_pen_relative);
            /* prevent paths from getting excessively large */
            if ( !pgls->g.polygon_mode )
                hpgl_call(hpgl_draw_current_path(pgls, hpgl_rm_vector));
            return 0;
        case ':':
            if_debug0('I', "\n  PE SP");
            {
                int32 pen;
                hpgl_pe_args_ret_t ret = pe_args(pgls->memory, pargs, 1);
                if ( ret != pe_okay ) {
                    if ( ret == pe_NeedData )
                        pargs->source.ptr = p - 1;
                    break;
                }
                pen = pargs->pe_values[0];
                pe_clear_args(pargs);
                if ( !pgls->g.polygon_mode ) {
                    hpgl_args_t args;
                    hpgl_args_set_int(&args, pen);
                    /* Note SP is illegal in polygon mode we must handle that here */
                    hpgl_call(hpgl_SP(&args, pgls));
                }
            }
            p = pargs->source.ptr;
            continue;
        case '<':
            if_debug0('I', "\n  PE PU");
            pargs->phase |= pe_pen_up;
            continue;
        case '>':
            if_debug0('I', "\n  PE PD");
            {
                int32 fbits;
                hpgl_pe_args_ret_t ret = pe_args(pgls->memory, pargs, 1);
                if ( ret != pe_okay ) {
                        if ( ret == pe_NeedData )
                            pargs->source.ptr = p - 1;
                        break;
                }
                fbits = pargs->pe_values[0];
                pe_clear_args(pargs);
                if ( fbits < -26 || fbits > 26 )
                    return e_Range;
                pgls->g.fraction_bits = fbits;
            }
            p = pargs->source.ptr;
            continue;
        case '=':
            if_debug0('I', "  PE ABS");
            pargs->phase |= pe_absolute;
            continue;
        case '7':
            if_debug0('I', "\n  PE 7bit");
            pargs->phase |= pe_7bit;
            continue;
        case ESC:
            /*
             * This is something of a hack.  Apparently we're supposed
             * to parse all PCL commands embedded in the GL/2 stream,
             * and simply ignore everything except the 3 that end GL/2
             * mode.  Instead, we simply stop parsing PE arguments as
             * soon as we see an ESC.
             */
            if ( ch == ESC ) /* (might be ESC+128) */ {
                pargs->source.ptr = p - 1; /* rescan ESC */
                if ( pargs->phase & pe_entered ) {
                    hpgl_restore_pen_state(pgls, &pgls->g.pen_state, hpgl_pen_relative);
                }
                hpgl_call(hpgl_update_carriage_return_pos(pgls));
                return 0;
            }
            /* falls through */
        default:
            if ( (ch & 127) <= 32 || (ch & 127) == 127 )
                continue;
            pargs->source.ptr = p - 1;
            {
                int32 xy[2];
                hpgl_args_t     args;
                int32 fbits = pgls->g.fraction_bits;
                if ( pe_args(pgls->memory, pargs, 2) != pe_okay )
                    break;
                xy[0] = pargs->pe_values[0];
                xy[1] = pargs->pe_values[1];
                pe_clear_args(pargs);
                if ( pargs->phase & pe_absolute )
                    pgls->g.relative_coords = hpgl_plot_absolute;
                else
                    pgls->g.relative_coords = hpgl_plot_relative;
                hpgl_args_set_real2(&args, pe_fixed2float(xy[0], fbits),
                                           pe_fixed2float(xy[1], fbits));
                if ( pargs->phase & pe_pen_up ) {
                    /* prevent paths from getting excessively large */
                    if ( !pgls->g.polygon_mode && point_count > point_count_max ) {
                        hpgl_call(hpgl_draw_current_path(pgls, hpgl_rm_vector));
                        point_count = 0;
                    }
                    hpgl_PU(&args, pgls);
                }
                else
                    hpgl_PD(&args, pgls);
                point_count++;
            }
            pargs->phase &= ~(pe_pen_up | pe_absolute);
            p = pargs->source.ptr;
            continue;
        }
        break;
    }
    return e_NeedData;
}
/* Get an encoded value from the input. */

static hpgl_pe_args_ret_t
pe_args(const gs_memory_t *mem, hpgl_args_t *pargs, int count)
{       const byte *p = pargs->source.ptr;
        const byte *rlimit = pargs->source.limit;
        int i, code;

        for ( i = pargs->pe_indx; i < count; ++i )
          {

#define VALUE (pargs->pe_values[i])
#define SHIFT (pargs->pe_shift[i])

            for ( ; ; )
              { int ch;

                if ( p >= rlimit ) {
                   pargs->pe_indx = i;
                   pargs->source.ptr = p;
                   return pe_NeedData;
                }
              ch = *++p;
              if ( (ch & 127) <= 32 || (ch & 127) == 127 )
                continue;
              if ( pargs->phase & pe_7bit )
                { ch -= 63;
                if ( ch & ~63 )
                  goto syntax_error;
                VALUE += (int32)(ch & 31) << SHIFT;
                SHIFT += 5;
                if ( ch & 32 )
                  break;
                }
              else
                { ch -= 63;
                if ( ch & ~191 )
                  goto syntax_error;
                VALUE += (int32)(ch & 63) << SHIFT;
                SHIFT += 6;
                if ( ch & 128 )
                  break;
                }
              }
            VALUE = (VALUE & 1 ? -(VALUE >> 1) : VALUE >> 1);
            if_debug1('I', "  [%ld]", (long)VALUE);
          }
        pargs->source.ptr = p;
        return pe_okay;
syntax_error:
        /* Just ignore everything we've parsed up to this point. */
        pargs->source.ptr = p;
        code = gs_note_error(e_Syntax);
        return pe_SyntaxError;
}

/* PR dx,dy...; */
int
hpgl_PR(hpgl_args_t *pargs, hpgl_state_t *pgls)
{
    if ( pgls->g.relative_coords != hpgl_plot_relative ) {
        pgls->g.relative_coords = hpgl_plot_relative;
        if ( !pgls->g.polygon_mode ) {
            hpgl_call(hpgl_draw_current_path(pgls, hpgl_rm_vector));
            hpgl_call(hpgl_clear_current_path(pgls));
        }
    }
    return hpgl_plot(pargs, pgls,
                     pgls->g.move_or_draw | hpgl_plot_relative,
                     false /* is PA command */);
}

/* PU (d)x,(d)y...; */
int
hpgl_PU(hpgl_args_t *pargs, hpgl_state_t *pgls)
{
        pgls->g.move_or_draw = hpgl_plot_move;
        return hpgl_plot(pargs, pgls,
                         hpgl_plot_move | pgls->g.relative_coords,
                         false /* is PA command */);
}

/* RT xinter,yinter,xend,yend[,chord]; */
int
hpgl_RT(hpgl_args_t *pargs, hpgl_state_t *pgls)
{
        hpgl_call(hpgl_arc_3_point(pargs, pgls, true));
        hpgl_call(hpgl_draw_arc(pgls));
        return 0;
}

/* Initialization */
static int
pgvector_do_registration(
    pcl_parser_state_t *pcl_parser_state,
    gs_memory_t *mem
)
{
    /* Register commands */
    DEFINE_HPGL_COMMANDS(mem)
        HPGL_COMMAND('A', 'A',
                     hpgl_AA, hpgl_cdf_polygon|hpgl_cdf_lost_mode_cleared|hpgl_cdf_pcl_rtl_both),
        HPGL_COMMAND('A', 'R',
                     hpgl_AR, hpgl_cdf_polygon|hpgl_cdf_lost_mode_cleared|hpgl_cdf_pcl_rtl_both),
        HPGL_COMMAND('A', 'T',
                     hpgl_AT, hpgl_cdf_polygon|hpgl_cdf_lost_mode_cleared|hpgl_cdf_pcl_rtl_both),
        HPGL_COMMAND('B', 'R',
                     hpgl_BR, hpgl_cdf_polygon|hpgl_cdf_pcl_rtl_both),  /* argument pattern can repeat */
        HPGL_COMMAND('B', 'Z',
                     hpgl_BZ, hpgl_cdf_polygon|hpgl_cdf_pcl_rtl_both),  /* argument pattern can repeat */
        HPGL_COMMAND('C', 'I',
                     hpgl_CI, hpgl_cdf_polygon|hpgl_cdf_lost_mode_cleared|hpgl_cdf_pcl_rtl_both),
        HPGL_COMMAND('P', 'A',
                     hpgl_PA, hpgl_cdf_polygon|hpgl_cdf_pcl_rtl_both),  /* argument pattern can repeat */
        HPGL_COMMAND('P', 'D',
                     hpgl_PD, hpgl_cdf_polygon|hpgl_cdf_pcl_rtl_both),  /* argument pattern can repeat */
        HPGL_COMMAND('P', 'E',
                     hpgl_PE, hpgl_cdf_polygon|hpgl_cdf_pcl_rtl_both),
        HPGL_COMMAND('P', 'R',
                     hpgl_PR, hpgl_cdf_polygon|hpgl_cdf_lost_mode_cleared|hpgl_cdf_pcl_rtl_both),       /* argument pattern can repeat */
        HPGL_COMMAND('P', 'U',
                     hpgl_PU, hpgl_cdf_polygon|hpgl_cdf_pcl_rtl_both),  /* argument pattern can repeat */
        HPGL_COMMAND('R', 'T',
                     hpgl_RT, hpgl_cdf_polygon|hpgl_cdf_lost_mode_cleared|hpgl_cdf_pcl_rtl_both),
        END_HPGL_COMMANDS
        return 0;
}
const pcl_init_t pgvector_init = {
        pgvector_do_registration, 0
};
