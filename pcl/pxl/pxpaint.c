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


/* pxpaint.c */
/* PCL XL painting operators */

#include "math_.h"
#include "stdio_.h"             /* std.h + NULL */
#include "pxoper.h"
#include "pxstate.h"
#include "pxfont.h"             /* for px_text */
#include "gsstate.h"
#include "gscoord.h"
#include "gspaint.h"
#include "gspath.h"
#include "gspath2.h"
#include "gsrop.h"
#include "gxfarith.h"
#include "gxfixed.h"
#include "gxgstate.h"
#include "gxmatrix.h"
#include "gxpath.h"
#include "pxptable.h"
#include "pxgstate.h" /* Prototype for px_high_level_pattern */

/*
 * The H-P documentation says we are supposed to draw rectangles
 * counter-clockwise on the page, which is clockwise in user space.
 * However, the LaserJet 6 (and probably the LJ 5 as well) draw rectangles
 * clockwise!  To draw rectangles clockwise, uncomment the following
 * #define.
 * clj4550 and clj4600 draw counter-clockwise rectangles
 */
/*#define DRAW_RECTANGLES_CLOCKWISE*/
/*
 * The H-P printers do really weird things for arcs, chords, or pies where
 * the width and/or height of the bounding box is negative.  To emulate
 * their behavior, uncomment the following #define.
 */
#define REFLECT_NEGATIVE_ARCS

/* Forward references */
px_operator_proc(pxNewPath);

/* ---------------- Utilities ---------------- */

/* Add lines to the path.  line_proc is gs_lineto or gs_rlineto. */
/* Attributes: pxaEndPoint, pxaNumberOfPoints, pxaPointType. */
static int
add_lines(px_args_t * par, px_state_t * pxs,
          int (*line_proc) (gs_gstate *, double, double))
{
    int code = 0;

    if (par->pv[0]) {           /* Single segment, specified as argument. */
        if (par->pv[1] || par->pv[2])
            return_error(errorIllegalAttributeCombination);
        return (*line_proc) (pxs->pgs, real_value(par->pv[0], 0),
                             real_value(par->pv[0], 1));
    }
    /* Multiple segments, specified in source data. */
    if (!(par->pv[1] && par->pv[2]))
        return_error(errorMissingAttribute);
    {
        int32_t num_points = par->pv[1]->value.i;
        pxeDataType_t type = (pxeDataType_t) par->pv[2]->value.i;
        int point_size = (type == eUByte || type == eSByte ? 2 : 4);

        while (par->source.position < num_points * point_size) {
            const byte *dp = par->source.data;
            int px, py;

            if (par->source.available < point_size) {   /* We don't even have one point's worth of source data. */
                return pxNeedData;
            }
            switch (type) {
                case eUByte:
                    px = dp[0];
                    py = dp[1];
                    break;
                case eSByte:
                    px = (int)(dp[0] ^ 0x80) - 0x80;
                    py = (int)(dp[1] ^ 0x80) - 0x80;
                    break;
                case eUInt16:
                    px = uint16at(dp, pxs->data_source_big_endian);
                    py = uint16at(dp + 2, pxs->data_source_big_endian);
                    break;
                case eSInt16:
                    px = sint16at(dp, pxs->data_source_big_endian);
                    py = sint16at(dp + 2, pxs->data_source_big_endian);
                    break;
                default:       /* can't happen, pacify compiler */
                    return_error(errorIllegalAttributeValue);
            }
            code = (*line_proc) (pxs->pgs, (double) px, (double) py);
            if (code < 0)
                break;
            par->source.position += point_size;
            par->source.available -= point_size;
            par->source.data += point_size;
        }
    }
    return code;
}

/* Add Bezier curves to the path.  curve_proc is gs_curveto or gs_rcurveto. */
/* Attributes: pxaNumberOfPoints, pxaPointType, pxaControlPoint1, */
/* pxaControlPoint2, pxaEndPoint. */
static int
add_curves(px_args_t * par, px_state_t * pxs,
           int (*curve_proc) (gs_gstate *, double, double, double, double,
                              double, double))
{
    int code = 0;

    if (par->pv[2] && par->pv[3] && par->pv[4]) {       /* Single curve, specified as argument. */
        if (par->pv[0] || par->pv[1])
            return_error(errorIllegalAttributeCombination);
        return (*curve_proc) (pxs->pgs, real_value(par->pv[2], 0),
                              real_value(par->pv[2], 1),
                              real_value(par->pv[3], 0),
                              real_value(par->pv[3], 1),
                              real_value(par->pv[4], 0),
                              real_value(par->pv[4], 1));
    }
    /* Multiple segments, specified in source data. */
    else if (par->pv[0] && par->pv[1]) {
        if (par->pv[2] || par->pv[3] || par->pv[4])
            return_error(errorIllegalAttributeCombination);
    } else
        return_error(errorMissingAttribute);
    {
        int32_t num_points = par->pv[0]->value.i;
        pxeDataType_t type = (pxeDataType_t) par->pv[1]->value.i;
        int point_size = (type == eUByte || type == eSByte ? 2 : 4);
        int segment_size = point_size * 3;

        if (num_points % 3)
            return_error(errorIllegalDataLength);
        while (par->source.position < num_points * point_size) {
            const byte *dp = par->source.data;
            int points[6];
            int i;

            if (par->source.available < point_size * 3) {       /* We don't even have one point's worth of source data. */
                return pxNeedData;
            }
            switch (type) {
                case eUByte:
                    for (i = 0; i < 6; ++i)
                        points[i] = dp[i];
                    break;
                case eSByte:
                    for (i = 0; i < 6; ++i)
                        points[i] = (int)(dp[i] ^ 0x80) - 0x80;
                    break;
                case eUInt16:
                    for (i = 0; i < 12; i += 2)
                        points[i >> 1] =
                            uint16at(dp + i, pxs->data_source_big_endian);
                    break;
                case eSInt16:
                    for (i = 0; i < 12; i += 2)
                        points[i >> 1]
                            = sint16at(dp + i, pxs->data_source_big_endian);
                    break;
                default:       /* can't happen, pacify compiler */
                    return_error(errorIllegalAttributeValue);
            }
            code = (*curve_proc) (pxs->pgs,
                                  (double) points[0], (double) points[1],
                                  (double) points[2], (double) points[3],
                                  (double) points[4], (double) points[5]);
            if (code < 0)
                break;
            par->source.position += segment_size;
            par->source.available -= segment_size;
            par->source.data += segment_size;
        }
    }
    return code;
}

/*
 * Set up all the parameters for an arc, chord, ellipse, or pie.  If pp3 and
 * pp4 are NULL, we're filling the entire box.  Store the upper left box
 * corner (for repositioning the cursor), the center, the radius, and the
 * two angles in *params, and return one of the following (or a negative
 * error code):
 */
typedef enum
{
    /*
     * Arc box is degenerate (zero width and/or height).
     * Only origin and center have been set.
     */
    arc_degenerate = 0,
    /*
     * Arc box is square.  No CTM adjustment was required; save_ctm is
     * not set.
     */
    arc_square,
    /*
     * Arc box is rectangular, CTM has been saved and adjusted.
     */
    arc_rectangular
} px_arc_type_t;

typedef struct px_arc_params_s
{
    gs_point origin;
    gs_point center;
    double radius;
    double ang3, ang4;
    gs_matrix save_ctm;
    bool reversed;
} px_arc_params_t;

static int                      /* px_arc_type_t or error code */
setup_arc(px_arc_params_t * params, const px_value_t * pbox,
          const px_value_t * pp3, const px_value_t * pp4,
          const px_state_t * pxs, bool ellipse)
{
    real x1 = real_value(pbox, 0);
    real y1 = real_value(pbox, 1);
    real x2 = real_value(pbox, 2);
    real y2 = real_value(pbox, 3);
    real xc = (x1 + x2) * 0.5;
    real yc = (y1 + y2) * 0.5;
    real xr, yr;
    bool rotated;
    int code;

#ifdef REFLECT_NEGATIVE_ARCS
    rotated = x1 > x2;
    params->reversed = rotated ^ (y1 > y2);
#else
    rotated = false;
    params->reversed = false;
#endif
    if (x1 > x2) {
        real temp = x1;

        x1 = x2;
        x2 = temp;
    }
    if (y1 > y2) {
        real temp = y1;

        y1 = y2;
        y2 = temp;
    }
    params->origin.x = x1;
    params->origin.y = y1;
    xr = (x2 - x1) * 0.5;
    yr = (y2 - y1) * 0.5;
    /* From what we can gather ellipses are degenerate if both
       width and height of the bounding box are 0.  Other objects
       behave as expected.  A 0 area bounding box is degenerate */
    if (ellipse) {
        /* The bounding box is degenerate, set what we can and exit. */
        if (xr == 0 && yr == 0) {
            params->center.x = xc;
            params->center.y = yc;
            return arc_degenerate;
        }
    } else {
        if (xr == 0 || yr == 0) {
            params->center.x = xc;
            params->center.y = yc;
            return arc_degenerate;
        }
    }

    if (pp3 && pp4) {
        real dx3 = real_value(pp3, 0) - xc;
        real dy3 = real_value(pp3, 1) - yc;
        real dx4 = real_value(pp4, 0) - xc;
        real dy4 = real_value(pp4, 1) - yc;

        if ((dx3 == 0 && dy3 == 0) || (dx4 == 0 && dy4 == 0))
            return_error(errorIllegalAttributeValue);
        {
            double ang3 = atan2(dy3 * xr, dx3 * yr) * radians_to_degrees;
            double ang4 = atan2(dy4 * xr, dx4 * yr) * radians_to_degrees;

            if (rotated)
                ang3 += 180, ang4 += 180;
            params->ang3 = ang3;
            params->ang4 = ang4;
        }
    }
    params->radius = yr;
    if (xr == yr) {
        params->center.x = xc;
        params->center.y = yc;
        return arc_square;
    } else {                    /* Must adjust the CTM.  Move the origin to (xc,yc) */
        /* for simplicity. */
        if ((code = gs_currentmatrix(pxs->pgs, &params->save_ctm)) < 0 ||
            (code = gs_translate(pxs->pgs, xc, yc)) < 0 ||
            (code = gs_scale(pxs->pgs, xr, yr)) < 0)
            return code;
        params->center.x = 0;
        params->center.y = 0;
        params->radius = 1.0;
        return arc_rectangular;
    }
}

/* per the nonsense in 5.7.3 (The ROP3 Operands) from the pxl
   reference manual the following rops are allowed for stroking. */
static bool
pxl_allow_rop_for_stroke(gs_gstate * pgs)
{
    gs_rop3_t rop = gs_currentrasterop(pgs);

    if (rop == 0 || rop == 160 || rop == 170 || rop == 240 || rop == 250
        || rop == 255)
        return true;
    return false;
}

/* Paint (stroke and/or fill) the current path. */
static int
paint_path(px_state_t * pxs)
{
    gs_gstate *pgs = pxs->pgs;
    gx_path *ppath = gx_current_path(pgs);
    px_gstate_t *pxgs = pxs->pxgs;
    bool will_stroke = pxgs->pen.type != pxpNull;
    bool will_fill = pxgs->brush.type != pxpNull;

    int code = 0;
    /* nothing to do. */
    if (!will_fill && !will_stroke)
        return 0;

    if (gx_path_is_void(ppath))
        return 0;

    pxs->have_page = true;

    if (will_fill) {
        gx_path *stroke_path = NULL;
        int (*fill_proc) (gs_gstate *) =
            (pxgs->fill_mode == eEvenOdd ? gs_eofill : gs_fill);

        if ((code = px_set_paint(&pxgs->brush, pxs)) < 0)
            return code;

        /* if we are also going to stroke the path, store a copy. */
        if (will_stroke) {
            stroke_path = gx_path_alloc_shared(ppath, pxs->memory, "paint_path(save_for_stroke)");
            if (stroke_path == NULL)
                return_error(errorInsufficientMemory);
            gx_path_assign_preserve(stroke_path, ppath);
        }

        /* Make a reduced version of the path, and put that back. */
        code = gx_path_elide_1d(ppath);
        if (code < 0)
            return code;

        /* exit here if no stroke or the fill failed. */
        code = (*fill_proc) (pgs);
        if (code < 0 || !will_stroke) {
            if (stroke_path)
                gx_path_free(stroke_path, "paint_path(error_with_fill)");
            return code;
        }

        /* restore the path for the stroke, will_stroke and hence
           stroke_path must be set at this point. */
        gx_path_assign_free(ppath, stroke_path);
    }

    /*
     * Per the description in the PCL XL reference documentation,
     * set a standard logical operation and transparency for stroking.
     * will_stroke is asserted true here.
     */
    {
        gs_rop3_t save_rop = gs_currentrasterop(pgs);
        bool save_transparent = gs_currenttexturetransparent(pgs);
        bool need_restore_rop = false;

        if (pxl_allow_rop_for_stroke(pgs) == false) {
            gs_setrasterop(pgs, rop3_T);
            gs_settexturetransparent(pgs, false);
            need_restore_rop = true;
        }
        code = px_set_paint(&pxgs->pen, pxs);
        if (code < 0)
            DO_NOTHING;
        code = gs_stroke(pgs);
        /* Bit hacky. Normally we handle this up at the interpreter level, and for
         * fill (above) that's how it works. However, px_set_paint() will call
         * gs_setpattern, which means that the high level pattern we've saved will
         * not be the one we use here. If we simply returned remap_color, as might be
         * expected, we would throw an error in the interpreter, and even if we didn't,
         * when we came back we would do the fill again, which is wasteful. Instead we
         * will cater for the situation here by calling the high level pattern routine
         * to install the pattern, then do the stroke again.
         */
        if (code == gs_error_Remap_Color) {
            code = px_high_level_pattern(pxs->pgs);
            code = gs_stroke(pgs);
        }
        if (code < 0)
            DO_NOTHING;
        if (need_restore_rop) {
            gs_setrasterop(pgs, save_rop);
            gs_settexturetransparent(pgs, save_transparent);
        }
    }
    return code;
}

/* Paint a shape defined by a one-operator path. */
static int
paint_shape(px_args_t * par, px_state_t * pxs, px_operator_proc((*path_op)))
{

    int code;
    gs_gstate *pgs = pxs->pgs;
    gs_fixed_point fxp;

    /* build the path */
    if ((code = pxNewPath(par, pxs)) < 0 ||
        (code = (*path_op) (par, pxs)) < 0)
        return code;

    /* save position and stroke and or fill the path */
    code = gx_path_current_point(gx_current_path(pxs->pgs), &fxp);
    if (code < 0)
        return code;

    code = paint_path(pxs);
    if (code < 0)
        return code;

    /* restore the saved position, and open a new subpath  */
    code = gx_path_add_point(gx_current_path(pxs->pgs), fxp.x, fxp.y);
    if (code < 0)
        return code;

    return gx_setcurrentpoint_from_path(pgs, gx_current_path(pxs->pgs));
}

/* ---------------- Operators ---------------- */

const byte apxCloseSubPath[] = { 0, 0 };
int
pxCloseSubPath(px_args_t * par, px_state_t * pxs)
{
    return gs_closepath(pxs->pgs);
}

const byte apxNewPath[] = { 0, 0 };
int
pxNewPath(px_args_t * par, px_state_t * pxs)
{
    return gs_newpath(pxs->pgs);
}

/* Unlike painting single objects the PaintPath operator preserves the
   path */
const byte apxPaintPath[] = { 0, 0 };
int
pxPaintPath(px_args_t * par, px_state_t * pxs)
{
    gx_path *ppath = gx_current_path(pxs->pgs);
    gx_path *save_path =
        gx_path_alloc_shared(ppath, pxs->memory, "pxPaintPath");
    int code;

    if (save_path == 0)
        return_error(errorInsufficientMemory);

    gx_path_assign_preserve(save_path, ppath);
    code = paint_path(pxs);
    gx_path_assign_free(ppath, save_path);

    if (code >= 0)
        code = gx_setcurrentpoint_from_path(pxs->pgs, ppath);
    
    return code;
}

const byte apxArcPath[] = {
    pxaBoundingBox, pxaStartPoint, pxaEndPoint, 0,
    pxaArcDirection, 0
};
int
pxArcPath(px_args_t * par, px_state_t * pxs)
{                               /*
                                 * Note that "clockwise" in user space is counter-clockwise on
                                 * the page, because the Y coordinate is inverted.
                                 */
    bool clockwise = (par->pv[3] != 0 && par->pv[3]->value.i == eClockWise);
    px_arc_params_t params;
    int code =
        setup_arc(&params, par->pv[0], par->pv[1], par->pv[2], pxs, false);
    int rcode = code;

    if (code >= 0 && code != arc_degenerate) {
        bool closed = params.ang3 == params.ang4;

        clockwise ^= params.reversed;
        if (closed) {
            if (clockwise)
                params.ang4 += 360;
            else
                params.ang3 += 360;
        }
        code = gs_arc_add(pxs->pgs, !clockwise, params.center.x,
                          params.center.y, params.radius, params.ang3,
                          params.ang4, false);
        if (code >= 0 && closed) {      /* We have to close the path explicitly. */
            code = gs_closepath(pxs->pgs);
        }
    }
    if (rcode == arc_rectangular)
        gs_setmatrix(pxs->pgs, &params.save_ctm);
    return code;
}

const byte apxBezierPath[] = {
    0, pxaNumberOfPoints, pxaPointType, pxaControlPoint1, pxaControlPoint2,
    pxaEndPoint, 0
};
int
pxBezierPath(px_args_t * par, px_state_t * pxs)
{
    return add_curves(par, pxs, gs_curveto);
}

const byte apxBezierRelPath[] = {
    0, pxaNumberOfPoints, pxaPointType, pxaControlPoint1, pxaControlPoint2,
    pxaEndPoint, 0
};
int
pxBezierRelPath(px_args_t * par, px_state_t * pxs)
{
    return add_curves(par, pxs, gs_rcurveto);
}

const byte apxChord[] = {
    pxaBoundingBox, pxaStartPoint, pxaEndPoint, 0, 0
};
px_operator_proc(pxChordPath);
int
pxChord(px_args_t * par, px_state_t * pxs)
{
    return paint_shape(par, pxs, pxChordPath);
}

const byte apxChordPath[] = {
    pxaBoundingBox, pxaStartPoint, pxaEndPoint, 0, 0
};
int
pxChordPath(px_args_t * par, px_state_t * pxs)
{
    px_arc_params_t params;
    int code =
        setup_arc(&params, par->pv[0], par->pv[1], par->pv[2], pxs, false);
    int rcode = code;

    /* See ArcPath above for the meaning of "clockwise". */
    if (code >= 0 && code != arc_degenerate) {
        if (params.ang3 == params.ang4)
            params.ang3 += 360;
        code = gs_arc_add(pxs->pgs, !params.reversed,
                          params.center.x, params.center.y,
                          params.radius, params.ang3, params.ang4, false);
        if (code >= 0)
            code = gs_closepath(pxs->pgs);
    }
    if (rcode == arc_rectangular)
        gs_setmatrix(pxs->pgs, &params.save_ctm);
    if (code >= 0)
        code = gs_moveto(pxs->pgs, params.origin.x, params.origin.y);
    return code;

}

const byte apxEllipse[] = {
    pxaBoundingBox, 0, 0
};
px_operator_proc(pxEllipsePath);
int
pxEllipse(px_args_t * par, px_state_t * pxs)
{
    return paint_shape(par, pxs, pxEllipsePath);
}

const byte apxEllipsePath[] = {
    pxaBoundingBox, 0, 0
};
int
pxEllipsePath(px_args_t * par, px_state_t * pxs)
{
    px_arc_params_t params;
    int code = setup_arc(&params, par->pv[0], NULL, NULL, pxs, true);
    int rcode = code;
    real a_start = 180.0;
    real a_end = -180.0;

    /* swap start and end angles if counter clockwise ellipse */
    if (params.reversed) {
        a_start = -180.0;
        a_end = 180.0;
    }
    /* See ArcPath above for the meaning of "clockwise". */
    if (code < 0 || code == arc_degenerate ||
        (code = gs_arc_add(pxs->pgs, !params.reversed,
                           params.center.x, params.center.y,
                           params.radius, a_start, a_end, false)) < 0 ||
        /* We have to close the path explicitly. */
        (code = gs_closepath(pxs->pgs)) < 0)
        DO_NOTHING;
    if (rcode == arc_rectangular)
        gs_setmatrix(pxs->pgs, &params.save_ctm);
    if (code >= 0)
        code = gs_moveto(pxs->pgs, params.origin.x, params.origin.y);
    return code;
}

const byte apxLinePath[] = {
    0, pxaEndPoint, pxaNumberOfPoints, pxaPointType, 0
};
int
pxLinePath(px_args_t * par, px_state_t * pxs)
{
    return add_lines(par, pxs, gs_lineto);
}

const byte apxLineRelPath[] = {
    0, pxaEndPoint, pxaNumberOfPoints, pxaPointType, 0
};
int
pxLineRelPath(px_args_t * par, px_state_t * pxs)
{
    return add_lines(par, pxs, gs_rlineto);
}

const byte apxPie[] = {
    pxaBoundingBox, pxaStartPoint, pxaEndPoint, 0, 0
};
px_operator_proc(pxPiePath);
int
pxPie(px_args_t * par, px_state_t * pxs)
{
    return paint_shape(par, pxs, pxPiePath);
}

const byte apxPiePath[] = {
    pxaBoundingBox, pxaStartPoint, pxaEndPoint, 0, 0
};
int
pxPiePath(px_args_t * par, px_state_t * pxs)
{
    px_arc_params_t params;
    int code =
        setup_arc(&params, par->pv[0], par->pv[1], par->pv[2], pxs, false);
    int rcode = code;

    /* See ArcPath above for the meaning of "clockwise". */
    if (code >= 0 && code != arc_degenerate) {
        if (params.ang3 == params.ang4)
            params.ang3 += 360;
        code = gs_moveto(pxs->pgs, params.center.x, params.center.y);
        if (code >= 0) {
            code = gs_arc_add(pxs->pgs, !params.reversed,
                              params.center.x, params.center.y,
                              params.radius, params.ang3, params.ang4, true);
        }
    }
    if (rcode == arc_rectangular)
        gs_setmatrix(pxs->pgs, &params.save_ctm);
    if (code < 0 || rcode == arc_degenerate ||
        (code = gs_closepath(pxs->pgs)) < 0 ||
        (code = gs_moveto(pxs->pgs, params.origin.x, params.origin.y)) < 0)
        DO_NOTHING;
    return code;
}

const byte apxRectangle[] = {
    pxaBoundingBox, 0, 0
};
px_operator_proc(pxRectanglePath);
int
pxRectangle(px_args_t * par, px_state_t * pxs)
{
    return paint_shape(par, pxs, pxRectanglePath);
}

const byte apxRectanglePath[] = {
    pxaBoundingBox, 0, 0
};
int
pxRectanglePath(px_args_t * par, px_state_t * pxs)
{
    double x1, y1, x2, y2;
    gs_fixed_point p1;
    gs_gstate *pgs = pxs->pgs;
    gx_path *ppath = gx_current_path(pgs);
    gs_fixed_point lines[3];

#define p2 lines[1]
#define pctm (&((const gs_gstate *)pgs)->ctm)
    int code;

    set_box_value(x1, y1, x2, y2, par->pv[0]);
    /*
     * Rectangles are always drawn in a canonical order.
     */
    if (x1 > x2) {
        double t = x1;

        x1 = x2;
        x2 = t;
    }
    if (y1 > y2) {
        double t = y1;

        y1 = y2;
        y2 = t;
    }
    if ((code = gs_point_transform2fixed(pctm, x1, y1, &p1)) < 0 ||
        (code = gs_point_transform2fixed(pctm, x2, y2, &p2)) < 0 ||
        (code = gs_moveto(pgs, x1, y1)) < 0)
        return code;
#ifdef DRAW_RECTANGLES_CLOCKWISE
    /*
     * DRAW_RECTANGLES_CLOCKWISE means clockwise on the page, which is
     * counter-clockwise in user space.
     */
    if ((code = gs_point_transform2fixed(pctm, x2, y1, &lines[0])) < 0 ||
        (code = gs_point_transform2fixed(pctm, x1, y2, &lines[2])) < 0)
        return code;
#else
    if ((code = gs_point_transform2fixed(pctm, x1, y2, &lines[0])) < 0 ||
        (code = gs_point_transform2fixed(pctm, x2, y1, &lines[2])) < 0)
        return code;
#endif
    if ((code = gx_path_add_lines(ppath, lines, 3)) < 0)
        return code;
    return gs_closepath(pgs);
#undef pctm
#undef p2
}

const byte apxRoundRectangle[] = {
    pxaBoundingBox, pxaEllipseDimension, 0, 0
};
px_operator_proc(pxRoundRectanglePath);
int
pxRoundRectangle(px_args_t * par, px_state_t * pxs)
{
    return paint_shape(par, pxs, pxRoundRectanglePath);
}

const byte apxRoundRectanglePath[] = {
    pxaBoundingBox, pxaEllipseDimension, 0, 0
};
int
pxRoundRectanglePath(px_args_t * par, px_state_t * pxs)
{
    double x1, y1, x2, y2;
    real xr = real_value(par->pv[1], 0) * 0.5;
    real yr = real_value(par->pv[1], 1) * 0.5;
    real xd, yd;
    gs_matrix save_ctm;
    gs_gstate *pgs = pxs->pgs;
    int code;

    set_box_value(x1, y1, x2, y2, par->pv[0]);
    xd = x2 - x1;
    yd = y2 - y1;
    /*
     * H-P printers give an error if the points are specified out
     * of order.
     */
    if (xd < 0 || yd < 0)
        return_error(errorIllegalAttributeValue);
    if (xr == 0 || yr == 0)
        return pxRectanglePath(par, pxs);
    gs_currentmatrix(pgs, &save_ctm);
    gs_translate(pgs, x1, y1);
    if (xr != yr) {             /* Change coordinates so the arcs are circular. */
        double scale = xr / yr;

        if ((code = gs_scale(pgs, scale, 1.0)) < 0)
            return code;
        xd *= yr / xr;
    }
#define r yr
    /*
     * Draw the rectangle counter-clockwise on the page, which is
     * clockwise in user space.  (This may be reversed if the
     * coordinates are specified in a non-standard order.)
     */
    if ((code = gs_moveto(pgs, 0.0, r)) < 0 ||
        (code = gs_arcn(pgs, r, yd - r, r, 180.0, 90.0)) < 0 ||
        (code = gs_arcn(pgs, xd - r, yd - r, r, 90.0, 0.0)) < 0 ||
        (code = gs_arcn(pgs, xd - r, r, r, 0.0, 270.0)) < 0 ||
        (code = gs_arcn(pgs, r, r, r, 270.0, 180.0)) < 0 ||
        (code = gs_closepath(pgs)) < 0 ||
        (code = gs_moveto(pgs, 0.0, 0.0)) < 0)
        return code;
#undef r
    return gs_setmatrix(pgs, &save_ctm);
}

const byte apxText[] = {
    pxaTextData, 0, pxaXSpacingData, pxaYSpacingData, 0
};
int
pxText(px_args_t * par, px_state_t * pxs)
{
  {
      int code = px_set_paint(&pxs->pxgs->brush, pxs);

      if (code < 0)
          return code;
  }
  if (par->pv[0]->value.array.size != 0 && pxs->pxgs->brush.type != pxpNull)
    pxs->have_page = true;

  return px_text(par, pxs, false);
}

const byte apxTextPath[] = {
    pxaTextData, 0, pxaXSpacingData, pxaYSpacingData, 0
};
int
pxTextPath(px_args_t * par, px_state_t * pxs)
{
    int code = px_set_paint(&pxs->pxgs->pen, pxs);

    if (code < 0)
        return code;
    /* NB this should be refactored with pxText (immediately above)
       and it is not a good heuristic for detecting a marked page. */
    if (par->pv[0]->value.array.size != 0 && pxs->pxgs->pen.type != pxpNull)
        pxs->have_page = true;
    return px_text(par, pxs, true);
}
