/* Copyright (C) 2001-2024 Artifex Software, Inc.
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

/* XPS interpreter - path (vector drawing) support */

#include "ghostxps.h"

#define INITIAL_DASH_SIZE 32
#define ADDITIVE_DASH_SIZE 1024

char *
xps_get_real_params(char *s, int num, float *x)
{
    int k = 0;

    if (s != NULL && *s != 0) {
        while (*s)
        {
            while (*s == 0x0d || *s == '\t' || *s == ' ' || *s == 0x0a)
                s++;
            x[k] = (float)strtod(s, &s);
            while (*s == 0x0d || *s == '\t' || *s == ' ' || *s == 0x0a)
                s++;
            if (*s == ',')
                s++;
            if (++k == num)
                break;
        }
        return s;
    } else
        return NULL;
}

char *
xps_get_point(char *s_in, float *x, float *y)
{
    char *s_out = s_in;
    float xy[2];

    s_out = xps_get_real_params(s_out, 2, xy);
    *x = xy[0];
    *y = xy[1];
    return s_out;
}

void
xps_clip(xps_context_t *ctx)
{
    if (ctx->fill_rule == 0)
        gs_eoclip(ctx->pgs);
    else
        gs_clip(ctx->pgs);
    gs_newpath(ctx->pgs);
}

void
xps_fill(xps_context_t *ctx)
{
    if (gs_getfillconstantalpha(ctx->pgs) < 0.001)
        gs_newpath(ctx->pgs);
    else if (ctx->fill_rule == 0) {
        if (gs_eofill(ctx->pgs) == gs_error_Remap_Color){
            ctx->in_high_level_pattern = true;
            xps_high_level_pattern(ctx);
            ctx->in_high_level_pattern = false;
            gs_eofill(ctx->pgs);
        }
    }
    else {
        if (gs_fill(ctx->pgs) == gs_error_Remap_Color){
            ctx->in_high_level_pattern = true;
            xps_high_level_pattern(ctx);
            ctx->in_high_level_pattern = false;
            gs_fill(ctx->pgs);
        }
    }
}

/* Draw an arc segment transformed by the matrix, we approximate with straight
 * line segments. We cannot use the gs_arc function because they only draw
 * circular arcs, we need to transform the line to make them elliptical but
 * without transforming the line width.
 *
 * We are guaranteed that on entry the point is at the point that would be
 * calculated by th0, and on exit, a point is generated for us at th0.
 */
static inline void
xps_draw_arc_segment(xps_context_t *ctx, gs_matrix *mtx, float th0, float th1, int iscw)
{
    float t, d;
    gs_point p;

    while (th1 < th0)
        th1 += (float)(M_PI * 2.0);

    d = (float)(1 * (M_PI / 180.0)); /* 1-degree precision */

    if (iscw)
    {
        for (t = th0 + d; t < th1 - d/2; t += d)
        {
            gs_point_transform(cos(t), sin(t), mtx, &p);
            gs_lineto(ctx->pgs, p.x, p.y);
        }
    }
    else
    {
        th0 += (float)(M_PI * 2);
        for (t = th0 - d; t > th1 + d/2; t -= d)
        {
            gs_point_transform(cos(t), sin(t), mtx, &p);
            gs_lineto(ctx->pgs, p.x, p.y);
        }
    }
}

/* Given two vectors find the angle between them. */
static inline double
angle_between(const gs_point u, const gs_point v)
{
    double det = u.x * v.y - u.y * v.x;
    double sign = (det < 0 ? -1.0 : 1.0);
    double magu = u.x * u.x + u.y * u.y;
    double magv = v.x * v.x + v.y * v.y;
    double udotv = u.x * v.x + u.y * v.y;
    double t = udotv / (magu * magv);
    /* guard against rounding errors when near |1| (where acos will return NaN) */
    if (t < -1.0) t = -1.0;
    if (t > 1.0) t = 1.0;
    return sign * acos(t);
}

static void
xps_draw_arc(xps_context_t *ctx,
        float size_x, float size_y, float rotation_angle,
        int is_large_arc, int is_clockwise,
        float point_x, float point_y)
{
    gs_matrix rotmat, revmat;
    gs_matrix mtx;
    gs_point pt;
    double rx, ry;
    double x1, y1, x2, y2;
    double x1t, y1t;
    double cxt, cyt, cx, cy;
    double t1, t2, t3;
    double sign;
    double th1, dth;

    gs_currentpoint(ctx->pgs, &pt);
    x1 = pt.x;
    y1 = pt.y;
    x2 = point_x;
    y2 = point_y;
    rx = size_x;
    ry = size_y;

    if (is_clockwise != is_large_arc)
        sign = 1;
    else
        sign = -1;

    gs_make_rotation(rotation_angle, &rotmat);
    gs_make_rotation(-rotation_angle, &revmat);

    /* http://www.w3.org/TR/SVG11/implnote.html#ArcImplementationNotes */
    /* Conversion from endpoint to center parameterization */

    /* F.6.6.1 -- ensure radii are positive and non-zero */
    rx = fabsf(rx);
    ry = fabsf(ry);
    if (rx < 0.001 || ry < 0.001 || (x1 == x2 && y1 == y2))
    {
        gs_lineto(ctx->pgs, x2, y2);
        return;
    }

    /* F.6.5.1 */
    gs_distance_transform((x1 - x2) / 2.0, (y1 - y2) / 2.0, &revmat, &pt);
    x1t = pt.x;
    y1t = pt.y;

    /* F.6.6.2 -- ensure radii are large enough */
    t1 = (x1t * x1t) / (rx * rx) + (y1t * y1t) / (ry * ry);
    if (t1 > 1.0)
    {
        rx = rx * sqrtf(t1);
        ry = ry * sqrtf(t1);
    }

    /* F.6.5.2 */
    t1 = (rx * rx * ry * ry) - (rx * rx * y1t * y1t) - (ry * ry * x1t * x1t);
    t2 = (rx * rx * y1t * y1t) + (ry * ry * x1t * x1t);
    t3 = t1 / t2;
    /* guard against rounding errors; sqrt of negative numbers is bad for your health */
    if (t3 < 0.0) t3 = 0.0;
    t3 = sqrtf(t3);

    cxt = sign * t3 * (rx * y1t) / ry;
    cyt = sign * t3 * -(ry * x1t) / rx;

    /* F.6.5.3 */
    gs_distance_transform(cxt, cyt, &rotmat, &pt);
    cx = pt.x + (x1 + x2) / 2;
    cy = pt.y + (y1 + y2) / 2;

    /* F.6.5.4 */
    {
        gs_point coord1, coord2, coord3, coord4;
        coord1.x = 1;
        coord1.y = 0;
        coord2.x = (x1t - cxt) / rx;
        coord2.y = (y1t - cyt) / ry;
        coord3.x = (x1t - cxt) / rx;
        coord3.y = (y1t - cyt) / ry;
        coord4.x = (-x1t - cxt) / rx;
        coord4.y = (-y1t - cyt) / ry;
        th1 = angle_between(coord1, coord2);
        dth = angle_between(coord3, coord4);
        if (dth < 0 && !is_clockwise)
            dth += (degrees_to_radians * 360);
        if (dth > 0 && is_clockwise)
            dth -= (degrees_to_radians * 360);
    }

    gs_make_identity(&mtx);
    gs_matrix_translate(&mtx, cx, cy, &mtx);
    gs_matrix_rotate(&mtx, rotation_angle, &mtx);
    gs_matrix_scale(&mtx, rx, ry, &mtx);
    xps_draw_arc_segment(ctx, &mtx, th1, th1 + dth, is_clockwise);

    gs_lineto(ctx->pgs, point_x, point_y);
}

/*
 * Parse an abbreviated geometry string, and call
 * ghostscript moveto/lineto/curveto functions to
 * build up a path.
 */

void
xps_parse_abbreviated_geometry(xps_context_t *ctx, char *geom)
{
    char **args;
    char **pargs;
    char *s = geom;
    gs_point pt;
    int i, n;
    int cmd, old;
    float x1, y1, x2, y2, x3, y3;
    float smooth_x, smooth_y; /* saved cubic bezier control point for smooth curves */
    int reset_smooth;

    args = xps_alloc(ctx, sizeof(char*) * (strlen(geom) + 1));
    if (!args) {
        gs_throw(gs_error_VMerror, "out of memory: args.\n");
        return;
    }
    pargs = args;

    //dmprintf1(ctx->memory, "new path (%.70s)\n", geom);
    gs_newpath(ctx->pgs);

    while (*s)
    {
        if ((*s >= 'A' && *s <= 'Z') || (*s >= 'a' && *s <= 'z'))
        {
            *pargs++ = s++;
        }
        else if ((*s >= '0' && *s <= '9') || *s == '.' || *s == '+' || *s == '-' || *s == 'e' || *s == 'E')
        {
            *pargs++ = s;
            while ((*s >= '0' && *s <= '9') || *s == '.' || *s == '+' || *s == '-' || *s == 'e' || *s == 'E')
                s ++;
        }
        else
        {
            s++;
        }
    }

    *pargs = s;

    n = pargs - args;
    i = 0;

    old = 0;

    reset_smooth = 1;
    smooth_x = 0.0;
    smooth_y = 0.0;

    while (i < n)
    {
        cmd = args[i][0];
        if (cmd == '+' || cmd == '.' || cmd == '-' || (cmd >= '0' && cmd <= '9'))
            cmd = old; /* it's a number, repeat old command */
        else
            i ++;

        if (reset_smooth)
        {
            smooth_x = 0.0;
            smooth_y = 0.0;
        }

        reset_smooth = 1;

        switch (cmd)
        {
        case 'F':
            if (i + 1 <= n)
            {
                ctx->fill_rule = atoi(args[i]);
                i++;
            }
            break;

        case 'M':
            if (i + 2 <= n)
            {
                gs_moveto(ctx->pgs, atof(args[i]), atof(args[i+1]));
                //dmprintf2(ctx->memory, "moveto %g %g\n", atof(args[i]), atof(args[i+1]));
                i += 2;
            }
            break;
        case 'm':
            if (i + 2 <= n)
            {
                gs_rmoveto(ctx->pgs, atof(args[i]), atof(args[i+1]));
                //dmprintf2(ctx->memory, "rmoveto %g %g\n", atof(args[i]), atof(args[i+1]));
                i += 2;
            }
            break;

        case 'L':
            if (i + 2 <= n)
            {
                gs_lineto(ctx->pgs, atof(args[i]), atof(args[i+1]));
                //dmprintf2(ctx->memory, "lineto %g %g\n", atof(args[i]), atof(args[i+1]));
                i += 2;
            }
            break;
        case 'l':
            if (i + 2 <= n)
            {
                gs_rlineto(ctx->pgs, atof(args[i]), atof(args[i+1]));
                //dmprintf2(ctx->memory, "rlineto %g %g\n", atof(args[i]), atof(args[i+1]));
                i += 2;
            }
            break;

        case 'H':
            if (i + 1 <= n)
            {
                gs_currentpoint(ctx->pgs, &pt);
                gs_lineto(ctx->pgs, atof(args[i]), pt.y);
                //dmprintf1(ctx->memory, "hlineto %g\n", atof(args[i]));
                i += 1;
            }
            break;
        case 'h':
            if (i + 1 <= n)
            {
                gs_rlineto(ctx->pgs, atof(args[i]), 0.0);
                //dmprintf1(ctx->memory, "rhlineto %g\n", atof(args[i]));
                i += 1;
            }
            break;

        case 'V':
            if (i + 1 <= n)
            {
                gs_currentpoint(ctx->pgs, &pt);
                gs_lineto(ctx->pgs, pt.x, atof(args[i]));
                //dmprintf1(ctx->memory, "vlineto %g\n", atof(args[i]));
                i += 1;
            }
            break;
        case 'v':
            if (i + 1 <= n)
            {
                gs_rlineto(ctx->pgs, 0.0, atof(args[i]));
                //dmprintf1(ctx->memory, "rvlineto %g\n", atof(args[i]));
                i += 1;
            }
            break;

        case 'C':
            if (i + 6 <= n)
            {
                x1 = atof(args[i+0]);
                y1 = atof(args[i+1]);
                x2 = atof(args[i+2]);
                y2 = atof(args[i+3]);
                x3 = atof(args[i+4]);
                y3 = atof(args[i+5]);
                gs_curveto(ctx->pgs, x1, y1, x2, y2, x3, y3);
                i += 6;
                reset_smooth = 0;
                smooth_x = x3 - x2;
                smooth_y = y3 - y2;
            }
            break;

        case 'c':
            if (i + 6 <= n)
            {
                gs_currentpoint(ctx->pgs, &pt);
                x1 = atof(args[i+0]) + pt.x;
                y1 = atof(args[i+1]) + pt.y;
                x2 = atof(args[i+2]) + pt.x;
                y2 = atof(args[i+3]) + pt.y;
                x3 = atof(args[i+4]) + pt.x;
                y3 = atof(args[i+5]) + pt.y;
                gs_curveto(ctx->pgs, x1, y1, x2, y2, x3, y3);
                i += 6;
                reset_smooth = 0;
                smooth_x = x3 - x2;
                smooth_y = y3 - y2;
            }
            break;

        case 'S':
            if (i + 4 <= n)
            {
                gs_currentpoint(ctx->pgs, &pt);
                x1 = atof(args[i+0]);
                y1 = atof(args[i+1]);
                x2 = atof(args[i+2]);
                y2 = atof(args[i+3]);
                //dmprintf2(ctx->memory, "smooth %g %g\n", smooth_x, smooth_y);
                gs_curveto(ctx->pgs, pt.x + smooth_x, pt.y + smooth_y, x1, y1, x2, y2);
                i += 4;
                reset_smooth = 0;
                smooth_x = x2 - x1;
                smooth_y = y2 - y1;
            }
            break;

        case 's':
            if (i + 4 <= n)
            {
                gs_currentpoint(ctx->pgs, &pt);
                x1 = atof(args[i+0]) + pt.x;
                y1 = atof(args[i+1]) + pt.y;
                x2 = atof(args[i+2]) + pt.x;
                y2 = atof(args[i+3]) + pt.y;
                //dmprintf2(ctx->memory, "smooth %g %g\n", smooth_x, smooth_y);
                gs_curveto(ctx->pgs, pt.x + smooth_x, pt.y + smooth_y, x1, y1, x2, y2);
                i += 4;
                reset_smooth = 0;
                smooth_x = x2 - x1;
                smooth_y = y2 - y1;
            }
            break;

        case 'Q':
            if (i + 4 <= n)
            {
                gs_currentpoint(ctx->pgs, &pt);
                x1 = atof(args[i+0]);
                y1 = atof(args[i+1]);
                x2 = atof(args[i+2]);
                y2 = atof(args[i+3]);
                //dmprintf4(ctx->memory, "conicto %g %g %g %g\n", x1, y1, x2, y2);
                gs_curveto(ctx->pgs,
                        (pt.x + 2 * x1) / 3, (pt.y + 2 * y1) / 3,
                        (x2 + 2 * x1) / 3, (y2 + 2 * y1) / 3,
                        x2, y2);
                i += 4;
            }
            break;
        case 'q':
            if (i + 4 <= n)
            {
                gs_currentpoint(ctx->pgs, &pt);
                x1 = atof(args[i+0]) + pt.x;
                y1 = atof(args[i+1]) + pt.y;
                x2 = atof(args[i+2]) + pt.x;
                y2 = atof(args[i+3]) + pt.y;
                //dmprintf4(ctx->memory, "conicto %g %g %g %g\n", x1, y1, x2, y2);
                gs_curveto(ctx->pgs,
                        (pt.x + 2 * x1) / 3, (pt.y + 2 * y1) / 3,
                        (x2 + 2 * x1) / 3, (y2 + 2 * y1) / 3,
                        x2, y2);
                i += 4;
            }
            break;

        case 'A':
            if (i + 7 <= n)
            {
                xps_draw_arc(ctx,
                        atof(args[i+0]), atof(args[i+1]), atof(args[i+2]),
                        atoi(args[i+3]), atoi(args[i+4]),
                        atof(args[i+5]), atof(args[i+6]));
                i += 7;
            }
            break;
        case 'a':
            if (i + 7 <= n)
            {
                gs_currentpoint(ctx->pgs, &pt);
                xps_draw_arc(ctx,
                        atof(args[i+0]), atof(args[i+1]), atof(args[i+2]),
                        atoi(args[i+3]), atoi(args[i+4]),
                        atof(args[i+5]) + pt.x, atof(args[i+6]) + pt.y);
                i += 7;
            }
            break;

        case 'Z':
        case 'z':
            gs_closepath(ctx->pgs);
            //dmputs(ctx->memory, "closepath\n");
            break;

        default:
            /* eek */
            if (old == cmd) /* avoid infinite loop */
                i++;
            break;
        }

        old = cmd;
    }

    xps_free(ctx, args);
}

static void
xps_parse_arc_segment(xps_context_t *ctx, xps_item_t *root, int stroking, int *skipped_stroke)
{
    /* ArcSegment pretty much follows the SVG algorithm for converting an
     * arc in endpoint representation to an arc in centerpoint
     * representation. Once in centerpoint it can be given to the
     * graphics library in the form of a postscript arc. */

    float rotation_angle;
    int is_large_arc, is_clockwise;
    float point_x, point_y;
    float size_x, size_y;
    int is_stroked;

    char *point_att = xps_att(root, "Point");
    char *size_att = xps_att(root, "Size");
    char *rotation_angle_att = xps_att(root, "RotationAngle");
    char *is_large_arc_att = xps_att(root, "IsLargeArc");
    char *sweep_direction_att = xps_att(root, "SweepDirection");
    char *is_stroked_att = xps_att(root, "IsStroked");

    if (!point_att || !size_att || !rotation_angle_att || !is_large_arc_att || !sweep_direction_att)
    {
        gs_warn("ArcSegment element is missing attributes");
        return;
    }

    is_stroked = 1;
    if (is_stroked_att && !strcmp(is_stroked_att, "false"))
            is_stroked = 0;
    if (!is_stroked)
        *skipped_stroke = 1;

    xps_get_point(point_att, &point_x, &point_y);
    xps_get_point(size_att, &size_x, &size_y);
    rotation_angle = atof(rotation_angle_att);
    is_large_arc = !strcmp(is_large_arc_att, "true");
    is_clockwise = !strcmp(sweep_direction_att, "Clockwise");

    if (stroking && !is_stroked)
    {
        gs_moveto(ctx->pgs, point_x, point_y);
        return;
    }

    xps_draw_arc(ctx, size_x, size_y, rotation_angle, is_large_arc, is_clockwise, point_x, point_y);
}

static void
xps_parse_poly_quadratic_bezier_segment(xps_context_t *ctx, xps_item_t *root, int stroking, int *skipped_stroke)
{
    char *points_att = xps_att(root, "Points");
    char *is_stroked_att = xps_att(root, "IsStroked");
    float x[2], y[2];
    int is_stroked;
    gs_point pt;
    char *s;
    int n;

    if (!points_att)
    {
        gs_warn("PolyQuadraticBezierSegment element has no points");
        return;
    }

    is_stroked = 1;
    if (is_stroked_att && !strcmp(is_stroked_att, "false"))
            is_stroked = 0;
    if (!is_stroked)
        *skipped_stroke = 1;

    s = points_att;
    n = 0;
    while (*s != 0)
    {
        while (*s == ' ') s++;
        s = xps_get_point(s, &x[n], &y[n]);
        n ++;
        if (n == 2)
        {
            if (stroking && !is_stroked)
            {
                gs_moveto(ctx->pgs, x[1], y[1]);
            }
            else
            {
                gs_currentpoint(ctx->pgs, &pt);
                gs_curveto(ctx->pgs,
                        (pt.x + 2 * x[0]) / 3, (pt.y + 2 * y[0]) / 3,
                        (x[1] + 2 * x[0]) / 3, (y[1] + 2 * y[0]) / 3,
                        x[1], y[1]);
            }
            n = 0;
        }
    }
}

static void
xps_parse_poly_bezier_segment(xps_context_t *ctx, xps_item_t *root, int stroking, int *skipped_stroke)
{
    char *points_att = xps_att(root, "Points");
    char *is_stroked_att = xps_att(root, "IsStroked");
    float x[3], y[3];
    int is_stroked;
    char *s;
    int n;

    if (!points_att)
    {
        gs_warn("PolyBezierSegment element has no points");
        return;
    }

    is_stroked = 1;
    if (is_stroked_att && !strcmp(is_stroked_att, "false"))
            is_stroked = 0;
    if (!is_stroked)
        *skipped_stroke = 1;

    s = points_att;
    n = 0;
    while (*s != 0)
    {
        while (*s == ' ') s++;
        s = xps_get_point(s, &x[n], &y[n]);
        n ++;
        if (n == 3)
        {
            if (stroking && !is_stroked)
                gs_moveto(ctx->pgs, x[2], y[2]);
            else
                gs_curveto(ctx->pgs, x[0], y[0], x[1], y[1], x[2], y[2]);
            n = 0;
        }
    }
}

static void
xps_parse_poly_line_segment(xps_context_t *ctx, xps_item_t *root, int stroking, int *skipped_stroke)
{
    char *points_att = xps_att(root, "Points");
    char *is_stroked_att = xps_att(root, "IsStroked");
    int is_stroked;
    float xy[2];
    char *s;

    if (!points_att)
    {
        gs_warn("PolyLineSegment element has no points");
        return;
    }

    is_stroked = 1;
    if (is_stroked_att && !strcmp(is_stroked_att, "false"))
            is_stroked = 0;
    if (!is_stroked)
        *skipped_stroke = 1;

    s = points_att;
    while (*s != 0)
    {
        s = xps_get_real_params(s, 2, &xy[0]);
        if (stroking && !is_stroked)
            gs_moveto(ctx->pgs, xy[0], xy[1]);
        else
            gs_lineto(ctx->pgs, xy[0], xy[1]);
    }
}

static void
xps_parse_path_figure(xps_context_t *ctx, xps_item_t *root, int stroking)
{
    xps_item_t *node;

    char *is_closed_att;
    char *start_point_att;
    char *is_filled_att;

    int is_closed = 0;
    int is_filled = 1;
    float start_x = 0.0;
    float start_y = 0.0;

    int skipped_stroke = 0;

    is_closed_att = xps_att(root, "IsClosed");
    start_point_att = xps_att(root, "StartPoint");
    is_filled_att = xps_att(root, "IsFilled");

    if (is_closed_att)
        is_closed = !strcmp(is_closed_att, "true");
    if (is_filled_att)
        is_filled = !strcmp(is_filled_att, "true");
    if (start_point_att)
        xps_get_point(start_point_att, &start_x, &start_y);

    if (!stroking && !is_filled) /* not filled, when filling */
        return;

    gs_moveto(ctx->pgs, start_x, start_y);

    for (node = xps_down(root); node; node = xps_next(node))
    {
        if (!strcmp(xps_tag(node), "ArcSegment"))
            xps_parse_arc_segment(ctx, node, stroking, &skipped_stroke);
        if (!strcmp(xps_tag(node), "PolyBezierSegment"))
            xps_parse_poly_bezier_segment(ctx, node, stroking, &skipped_stroke);
        if (!strcmp(xps_tag(node), "PolyLineSegment"))
            xps_parse_poly_line_segment(ctx, node, stroking, &skipped_stroke);
        if (!strcmp(xps_tag(node), "PolyQuadraticBezierSegment"))
            xps_parse_poly_quadratic_bezier_segment(ctx, node, stroking, &skipped_stroke);
    }

    if (is_closed)
    {
        if (stroking && skipped_stroke)
            gs_lineto(ctx->pgs, start_x, start_y); /* we've skipped using gs_moveto... */
        else
            gs_closepath(ctx->pgs); /* no skipped segments, safe to closepath properly */
    }
}

void
xps_parse_path_geometry(xps_context_t *ctx, xps_resource_t *dict, xps_item_t *root, int stroking)
{
    xps_item_t *node;

    char *figures_att;
    char *fill_rule_att;
    char *transform_att;

    xps_item_t *transform_tag = NULL;
    xps_item_t *figures_tag = NULL; /* only used by resource */

    gs_matrix transform;
    gs_matrix saved_transform;

    gs_newpath(ctx->pgs);

    figures_att = xps_att(root, "Figures");
    fill_rule_att = xps_att(root, "FillRule");
    transform_att = xps_att(root, "Transform");

    for (node = xps_down(root); node; node = xps_next(node))
    {
        if (!strcmp(xps_tag(node), "PathGeometry.Transform"))
            transform_tag = xps_down(node);
    }

    xps_resolve_resource_reference(ctx, dict, &transform_att, &transform_tag, NULL);
    xps_resolve_resource_reference(ctx, dict, &figures_att, &figures_tag, NULL);

    if (fill_rule_att)
    {
        if (!strcmp(fill_rule_att, "NonZero"))
            ctx->fill_rule = 1;
        if (!strcmp(fill_rule_att, "EvenOdd"))
            ctx->fill_rule = 0;
    }

    gs_make_identity(&transform);
    if (transform_att || transform_tag)
    {
        if (transform_att)
            xps_parse_render_transform(ctx, transform_att, &transform);
        if (transform_tag)
            xps_parse_matrix_transform(ctx, transform_tag, &transform);
    }

    gs_currentmatrix(ctx->pgs, &saved_transform);
    gs_concat(ctx->pgs, &transform);

    if (figures_att)
    {
        xps_parse_abbreviated_geometry(ctx, figures_att);
    }

    if (figures_tag)
    {
        xps_parse_path_figure(ctx, figures_tag, stroking);
    }

    for (node = xps_down(root); node; node = xps_next(node))
    {
        if (!strcmp(xps_tag(node), "PathFigure"))
            xps_parse_path_figure(ctx, node, stroking);
    }

    gs_setmatrix(ctx->pgs, &saved_transform);
}

static int
xps_parse_line_cap(char *attr)
{
    if (attr)
    {
        if (!strcmp(attr, "Flat")) return gs_cap_butt;
        if (!strcmp(attr, "Square")) return gs_cap_square;
        if (!strcmp(attr, "Round")) return gs_cap_round;
        if (!strcmp(attr, "Triangle")) return gs_cap_triangle;
    }
    return gs_cap_butt;
}

static void
pdfmark_bbox_transform(gs_rect *bbox, gs_matrix *matrix)
{
    gs_point aa, az, za, zz;
    double temp;
    gs_matrix matrix2;

    if (gs_matrix_invert(matrix, &matrix2) < 0)
        return;

    gs_point_transform(bbox->p.x, bbox->p.y, &matrix2, &aa);
    gs_point_transform(bbox->p.x, bbox->q.y, &matrix2, &az);
    gs_point_transform(bbox->q.x, bbox->p.y, &matrix2, &za);
    gs_point_transform(bbox->q.x, bbox->q.y, &matrix2, &zz);

    if ( aa.x > az.x)
        temp = aa.x, aa.x = az.x, az.x = temp;
    if ( za.x > zz.x)
        temp = za.x, za.x = zz.x, zz.x = temp;
    if ( za.x < aa.x)
        aa.x = za.x;  /* min */
    if ( az.x > zz.x)
        zz.x = az.x;  /* max */

    if ( aa.y > az.y)
        temp = aa.y, aa.y = az.y, az.y = temp;
    if ( za.y > zz.y)
        temp = za.y, za.y = zz.y, zz.y = temp;
    if ( za.y < aa.y)
        aa.y = za.y;  /* min */
    if ( az.y > zz.y)
        zz.y = az.y;  /* max */

    bbox->p.x = aa.x;
    bbox->p.y = aa.y;
    bbox->q.x = zz.x;
    bbox->q.y = zz.y;
}

static int check_pdfmark(xps_context_t *ctx, gx_device *dev)
{
    gs_c_param_list list;
    int code = -1;
    dev_param_req_t request;
    char pdfmark[] = "pdfmark";

    /* Check if the device supports pdfmark (pdfwrite) */
    gs_c_param_list_write(&list, ctx->pgs->device->memory);
    request.Param = pdfmark;
    request.list = &list;
    code = dev_proc(dev, dev_spec_op)(dev, gxdso_get_dev_param, &request, sizeof(dev_param_req_t));
    gs_c_param_list_release(&list);
    return code;
}

static int pdfmark_write_param_list_array(xps_context_t *ctx, const gs_param_string_array *array_list)
{
    gs_c_param_list list;
    int code = 0;

    /* Set the list to writeable, and initialise it */
    gs_c_param_list_write(&list, ctx->memory);
    /* We don't want keys to be persistent, as we are going to throw
     * away our array, force them to be copied
     */
    gs_param_list_set_persistent_keys((gs_param_list *) &list, false);

    /* Make really sure the list is writable, but don't initialise it */
    gs_c_param_list_write_more(&list);

    /* Add the param string array to the list */
    code = param_write_string_array((gs_param_list *)&list, "pdfmark", (const gs_param_string_array *)array_list);
    if (code < 0)
        return code;

    /* Set the param list back to readable, so putceviceparams can readit (mad...) */
    gs_c_param_list_read(&list);

    /* and set the actual device parameters */
    code = gs_putdeviceparams(ctx->pgs->device, (gs_param_list *)&list);

    gs_c_param_list_release(&list);
    return code;
}

static int pdfmark_link(xps_context_t *ctx, char *navigate_uri_att, gs_rect *path_bbox, float *samples)
{
    gx_device *dev = ctx->pgs->device;
    int code = 0;

    code = check_pdfmark(ctx, dev);

    if (code >= 0) {
        gs_matrix ctm_placeholder;
        gs_param_string_array array_list;
        gs_param_string *parray = NULL;
        char ctmstr[256];
        char objdef0[] = "/_objdef", objdef1[256], objdef2[] = "/type", objdef3[] = "/dict", objdef4[] = "OBJ";
        char uridef0[] = "/S", uridef1[] = "/URI", uridef2[256], uridef3[] = ".PUTDICT";
        char linkdef0[] = "/A", linkdef1[] = "/Rect", linkdef2[] = "/Subtype", linkdef3[] = "/Link", linkdef4[] = "LNK", linkRect[256];
        char colordef0[] = "/C", colordef1[256];

        parray = (gs_param_string *)gs_alloc_bytes(ctx->memory, 10*sizeof(gs_param_string),
                                                   "pdfi_pdfmark_from_dict(parray)");
        if (parray == NULL) {
            code = gs_note_error(gs_error_VMerror);
            return code;
        }

        gs_currentmatrix(ctx->pgs, &ctm_placeholder);
        gs_snprintf(ctmstr, 256, "[%.4f %.4f %.4f %.4f %.4f %.4f]", ctm_placeholder.xx, ctm_placeholder.xy, ctm_placeholder.yx, ctm_placeholder.yy, ctm_placeholder.tx, ctm_placeholder.ty);

        memset(parray, 0, 10*sizeof(gs_param_string));
        gs_snprintf(objdef1, 256, "{Obj%dG0}", gs_next_ids(ctx->pgs->device->memory, 1));
        parray[0].data = (const byte *)objdef0;
        parray[0].size = strlen(objdef0);
        parray[1].data = (const byte *)objdef1;
        parray[1].size = strlen(objdef1);
        parray[2].data = (const byte *)objdef2;
        parray[2].size = strlen(objdef2);
        parray[3].data = (const byte *)objdef3;
        parray[3].size = strlen(objdef3);
        parray[4].data = (const byte *)ctmstr;
        parray[4].size = strlen(ctmstr);
        parray[5].data = (const byte *)objdef4;
        parray[5].size = strlen(objdef4);

        array_list.data = parray;
        array_list.persistent = false;
        array_list.size = 6;

        code = pdfmark_write_param_list_array(ctx, (const gs_param_string_array *)&array_list);
        if (code < 0)
            goto  exit1;

        gs_snprintf(uridef2, 256, "(%s)", navigate_uri_att);
        memset(parray, 0, 10*sizeof(gs_param_string));
        parray[0].data = (const byte *)objdef1;
        parray[0].size = strlen(objdef1);
        parray[1].data = (const byte *)uridef0;
        parray[1].size = strlen(uridef0);
        parray[2].data = (const byte *)uridef1;
        parray[2].size = strlen(uridef1);
        parray[3].data = (const byte *)uridef1;
        parray[3].size = strlen(uridef1);
        parray[4].data = (const byte *)uridef2;
        parray[4].size = strlen(uridef2);
        parray[5].data = (const byte *)ctmstr;
        parray[5].size = strlen(ctmstr);
        parray[6].data = (const byte *)uridef3;
        parray[6].size = strlen(uridef3);

        array_list.data = parray;
        array_list.persistent = false;
        array_list.size = 7;

        code = pdfmark_write_param_list_array(ctx, (const gs_param_string_array *)&array_list);
        if (code < 0)
            goto  exit1;

        memset(parray, 0, 10*sizeof(gs_param_string));

        pdfmark_bbox_transform(path_bbox, &ctm_placeholder);
        gs_snprintf(linkRect, 256, "[%f %f %f %f]", path_bbox->p.x, path_bbox->p.y, path_bbox->q.x, path_bbox->q.y);
        if (samples[3] == 0x00)
            gs_snprintf(colordef1, 256, "[]");
        else
            gs_snprintf(colordef1, 256, "[%.4f %.4f %.4f]", samples[0], samples[1], samples[2]);
        parray[0].data = (const byte *)linkdef0;
        parray[0].size = strlen(linkdef0);
        parray[1].data = (const byte *)objdef1;
        parray[1].size = strlen(objdef1);
        parray[2].data = (const byte *)linkdef1;
        parray[2].size = strlen(linkdef1);
        parray[3].data = (const byte *)linkRect;
        parray[3].size = strlen(linkRect);
        parray[4].data = (const byte *)colordef0;
        parray[4].size = strlen(colordef0);
        parray[5].data = (const byte *)colordef1;
        parray[5].size = strlen(colordef1);
        parray[6].data = (const byte *)linkdef2;
        parray[6].size = strlen(linkdef2);
        parray[7].data = (const byte *)linkdef3;
        parray[7].size = strlen(linkdef3);
        parray[8].data = (const byte *)ctmstr;
        parray[8].size = strlen(ctmstr);
        parray[9].data = (const byte *)linkdef4;
        parray[9].size = strlen(linkdef4);

        array_list.data = parray;
        array_list.persistent = false;
        array_list.size = 10;

        code = pdfmark_write_param_list_array(ctx, (const gs_param_string_array *)&array_list);

exit1:
        gs_free_object(ctx->memory, parray, "pdfi_pdfmark_from_dict(parray)");
    } else
        code = 0;

    return code;
}

/*
 * Parse an XPS <Path> element, and call relevant ghostscript
 * functions for drawing and/or clipping the child elements.
 */

int
xps_parse_path(xps_context_t *ctx, char *base_uri, xps_resource_t *dict, xps_item_t *root)
{
    xps_item_t *node;
    int code;

    char *fill_uri;
    char *stroke_uri;
    char *opacity_mask_uri;

    char *transform_att;
    char *clip_att;
    char *data_att;
    char *fill_att;
    char *stroke_att;
    char *opacity_att;
    char *opacity_mask_att;
    char *navigate_uri_att;

    xps_item_t *transform_tag = NULL;
    xps_item_t *clip_tag = NULL;
    xps_item_t *data_tag = NULL;
    xps_item_t *fill_tag = NULL;
    xps_item_t *stroke_tag = NULL;
    xps_item_t *opacity_mask_tag = NULL;

    char *fill_opacity_att = NULL;
    char *stroke_opacity_att = NULL;

    char *stroke_dash_array_att;
    char *stroke_dash_cap_att;
    char *stroke_dash_offset_att;
    char *stroke_end_line_cap_att;
    char *stroke_start_line_cap_att;
    char *stroke_line_join_att;
    char *stroke_miter_limit_att;
    char *stroke_thickness_att;

    gs_line_join linejoin;
    float linewidth;
    float miterlimit;
    float samples[XPS_MAX_COLORS] = {0, 0, 0, 0};

    bool opacity_pushed = false;
    bool uses_stroke = false;

    gs_rect path_bbox = {{0.0, 0.0}, {0.0, 0.0}};

    gs_gsave(ctx->pgs);

    ctx->fill_rule = 0;

    /*
     * Extract attributes and extended attributes.
     */

    transform_att = xps_att(root, "RenderTransform");
    clip_att = xps_att(root, "Clip");
    data_att = xps_att(root, "Data");
    fill_att = xps_att(root, "Fill");
    stroke_att = xps_att(root, "Stroke");
    opacity_att = xps_att(root, "Opacity");
    opacity_mask_att = xps_att(root, "OpacityMask");
    navigate_uri_att = xps_att(root, "FixedPage.NavigateUri");

    stroke_dash_array_att = xps_att(root, "StrokeDashArray");
    stroke_dash_cap_att = xps_att(root, "StrokeDashCap");
    stroke_dash_offset_att = xps_att(root, "StrokeDashOffset");
    stroke_end_line_cap_att = xps_att(root, "StrokeEndLineCap");
    stroke_start_line_cap_att = xps_att(root, "StrokeStartLineCap");
    stroke_line_join_att = xps_att(root, "StrokeLineJoin");
    stroke_miter_limit_att = xps_att(root, "StrokeMiterLimit");
    stroke_thickness_att = xps_att(root, "StrokeThickness");

    for (node = xps_down(root); node; node = xps_next(node))
    {
        if (!strcmp(xps_tag(node), "Path.RenderTransform"))
            transform_tag = xps_down(node);

        if (!strcmp(xps_tag(node), "Path.OpacityMask"))
            opacity_mask_tag = xps_down(node);

        if (!strcmp(xps_tag(node), "Path.Clip"))
            clip_tag = xps_down(node);

        if (!strcmp(xps_tag(node), "Path.Fill"))
            fill_tag = xps_down(node);

        if (!strcmp(xps_tag(node), "Path.Stroke"))
            stroke_tag = xps_down(node);

        if (!strcmp(xps_tag(node), "Path.Data"))
            data_tag = xps_down(node);
    }

    fill_uri = base_uri;
    stroke_uri = base_uri;
    opacity_mask_uri = base_uri;

    xps_resolve_resource_reference(ctx, dict, &data_att, &data_tag, NULL);
    xps_resolve_resource_reference(ctx, dict, &clip_att, &clip_tag, NULL);
    xps_resolve_resource_reference(ctx, dict, &transform_att, &transform_tag, NULL);
    xps_resolve_resource_reference(ctx, dict, &fill_att, &fill_tag, &fill_uri);
    xps_resolve_resource_reference(ctx, dict, &stroke_att, &stroke_tag, &stroke_uri);
    xps_resolve_resource_reference(ctx, dict, &opacity_mask_att, &opacity_mask_tag, &opacity_mask_uri);

    /*
     * Act on the information we have gathered:
     */

    if (fill_tag && !strcmp(xps_tag(fill_tag), "SolidColorBrush"))
    {
        fill_opacity_att = xps_att(fill_tag, "Opacity");
        fill_att = xps_att(fill_tag, "Color");
        fill_tag = NULL;
    }

    if (stroke_tag && !strcmp(xps_tag(stroke_tag), "SolidColorBrush"))
    {
        stroke_opacity_att = xps_att(stroke_tag, "Opacity");
        stroke_att = xps_att(stroke_tag, "Color");
        stroke_tag = NULL;
    }

    gs_setlinestartcap(ctx->pgs, xps_parse_line_cap(stroke_start_line_cap_att));
    gs_setlineendcap(ctx->pgs, xps_parse_line_cap(stroke_end_line_cap_att));
    gs_setlinedashcap(ctx->pgs, xps_parse_line_cap(stroke_dash_cap_att));

    linejoin = gs_join_miter;
    if (stroke_line_join_att)
    {
        if (!strcmp(stroke_line_join_att, "Miter")) linejoin = gs_join_miter;
        if (!strcmp(stroke_line_join_att, "Bevel")) linejoin = gs_join_bevel;
        if (!strcmp(stroke_line_join_att, "Round")) linejoin = gs_join_round;
    }
    gs_setlinejoin(ctx->pgs, linejoin);

    miterlimit = 10.0;
    if (stroke_miter_limit_att)
        miterlimit = atof(stroke_miter_limit_att);
    gs_setmiterlimit(ctx->pgs, miterlimit);

    linewidth = 1.0;
    if (stroke_thickness_att)
        linewidth = atof(stroke_thickness_att);
    gs_setlinewidth(ctx->pgs, linewidth);

    if (stroke_dash_array_att)
    {
        char *s = stroke_dash_array_att;
        float *dash_array;
        float dash_offset = 0.0;
        int dash_count = 0;
        int dash_mem_count = 0;

        /* Do an initial reasonable allocation. If that
           runs out, double until we get to a max size
           and then just add that max each overrun. */
        dash_array = xps_alloc(ctx, sizeof(float) * INITIAL_DASH_SIZE);
        if (dash_array == NULL)
        {
            gs_throw(gs_error_VMerror, "out of memory: dash_array.\n");
            return gs_error_VMerror;
        }
        dash_mem_count = INITIAL_DASH_SIZE;

        if (stroke_dash_offset_att)
            dash_offset = atof(stroke_dash_offset_att) * linewidth;

        while (*s)
        {
            while (*s == ' ')
                s++;
            if (*s) /* needed in case of a space before the last quote */
            {
                /* Double up to a max size of ADDITIVE_DASH_SIZE and then add
                   that amount each time */
                if (dash_count > (dash_mem_count - 1))
                {
                    if (dash_mem_count < ADDITIVE_DASH_SIZE)
                        dash_mem_count = dash_mem_count * 2;
                    else
                        dash_mem_count = dash_mem_count + ADDITIVE_DASH_SIZE;
                    dash_array = (float*) xps_realloc(ctx, dash_array, sizeof(float) * dash_mem_count);
                    if (dash_array == NULL)
                    {
                        gs_throw(gs_error_VMerror, "out of memory: dash_array realloc.\n");
                        return gs_error_VMerror;
                    }
                }
                dash_array[dash_count++] = atof(s) * linewidth;
            }
            while (*s && *s != ' ')
                s++;
        }

        if (dash_count > 0)
        {
            float phase_len = 0;
            int i;
            for (i = 0; i < dash_count; ++i)
                phase_len += dash_array[i];
            if (phase_len == 0)
                dash_count = 0;
        }
        gs_setdash(ctx->pgs, dash_array, dash_count, dash_offset);
        xps_free(ctx, dash_array);
    }
    else
    {
        gs_setdash(ctx->pgs, NULL, 0, 0.0);
    }

    if (transform_att || transform_tag)
    {
        gs_matrix transform;

        if (transform_att)
            xps_parse_render_transform(ctx, transform_att, &transform);
        if (transform_tag)
            xps_parse_matrix_transform(ctx, transform_tag, &transform);

        gs_concat(ctx->pgs, &transform);
    }

    if (clip_att || clip_tag)
    {
        if (clip_att)
            xps_parse_abbreviated_geometry(ctx, clip_att);
        if (clip_tag)
            xps_parse_path_geometry(ctx, dict, clip_tag, 0);
        xps_clip(ctx);
    }

#if 0 // XXX
    if (opacity_att || opacity_mask_tag)
    {
        /* clip the bounds with the actual path */
        if (data_att)
            xps_parse_abbreviated_geometry(ctx, data_att);
        if (data_tag)
            xps_parse_path_geometry(ctx, dict, data_tag, 0);
        xps_update_bounds(ctx, &saved_bounds_opacity);
        gs_newpath(ctx->pgs);
    }
#endif
    /* xps_begin_opacity put into the fill_att, etc loops so that we can
       push groups of smaller sizes.  This makes it necessary to add a pushed
       flag so that we don't do multiple pushes if we have a fill and stroke
       attribute for the same group. */
    if (stroke_att || stroke_tag) {
        uses_stroke = true;
    }
    if (fill_att)
    {
        gs_color_space *colorspace;

        if (data_att)
            xps_parse_abbreviated_geometry(ctx, data_att);
        if (data_tag)
            xps_parse_path_geometry(ctx, dict, data_tag, 0);

        if (navigate_uri_att) {
            code = gx_curr_bbox(ctx->pgs, &path_bbox, PATH_FILL);
            if (code < 0)
                navigate_uri_att = NULL;
        }

        code = xps_begin_opacity(ctx, opacity_mask_uri, dict, opacity_att, opacity_mask_tag, true, uses_stroke);
        if (code)
        {
            gs_grestore(ctx->pgs);
            return gs_rethrow(code, "cannot create transparency group");
        }

        /* Color must be set *after* we begin opacity */
        xps_parse_color(ctx, base_uri, fill_att, &colorspace, samples);
        if (fill_opacity_att)
            samples[0] *= atof(fill_opacity_att);
        xps_set_color(ctx, colorspace, samples);
        rc_decrement(colorspace, "xps_parse_path");

        opacity_pushed = true;
        xps_fill(ctx);
    }

    if (fill_tag)
    {
        if (data_att)
            xps_parse_abbreviated_geometry(ctx, data_att);
        if (data_tag)
            xps_parse_path_geometry(ctx, dict, data_tag, 0);

        if (!opacity_pushed) {
            code = xps_begin_opacity(ctx, opacity_mask_uri, dict, opacity_att, opacity_mask_tag, true, uses_stroke);
            if (code)
            {
                gs_grestore(ctx->pgs);
                return gs_rethrow(code, "cannot create transparency group");
            }
            opacity_pushed = true;
        }

        code = xps_parse_brush(ctx, fill_uri, dict, fill_tag);
        if (code < 0)
        {
            xps_end_opacity(ctx, opacity_mask_uri, dict, opacity_att, opacity_mask_tag);
            gs_grestore(ctx->pgs);
            return gs_rethrow(code, "cannot parse fill brush");
        }
    }

    if (stroke_att)
    {
        gs_color_space *colorspace;

        if (data_att)
            xps_parse_abbreviated_geometry(ctx, data_att);
        if (data_tag)
            xps_parse_path_geometry(ctx, dict, data_tag, 1);

        if (!opacity_pushed) {
            code = xps_begin_opacity(ctx, opacity_mask_uri, dict, opacity_att, opacity_mask_tag, true, true);
            if (code)
            {
                gs_grestore(ctx->pgs);
                return gs_rethrow(code, "cannot create transparency group");
            }
            opacity_pushed = true;
        }

        /* Color must be set *after* the group is pushed */
        xps_parse_color(ctx, base_uri, stroke_att, &colorspace, samples);
        if (stroke_opacity_att)
            samples[0] *= atof(stroke_opacity_att);
        xps_set_color(ctx, colorspace, samples);
        rc_decrement(colorspace, "xps_parse_path");

        gs_stroke(ctx->pgs);
    }

    if (stroke_tag)
    {
        if (data_att)
            xps_parse_abbreviated_geometry(ctx, data_att);
        if (data_tag)
            xps_parse_path_geometry(ctx, dict, data_tag, 1);

        if (navigate_uri_att) {
            code = gx_curr_bbox(ctx->pgs, &path_bbox, PATH_FILL);
            if (code < 0)
                navigate_uri_att = NULL;
        }

        if (!opacity_pushed) {
            code = xps_begin_opacity(ctx, opacity_mask_uri, dict, opacity_att, opacity_mask_tag, true, true);
            if (code)
            {
                gs_grestore(ctx->pgs);
                return gs_rethrow(code, "cannot create transparency group");
            }
            opacity_pushed = true;
        }

        ctx->fill_rule = 1; /* over-ride fill rule when converting outline to stroked */
        gs_strokepath2(ctx->pgs);

        code = xps_parse_brush(ctx, stroke_uri, dict, stroke_tag);
        if (code < 0)
        {
            xps_end_opacity(ctx, opacity_mask_uri, dict, opacity_att, opacity_mask_tag);
            gs_grestore(ctx->pgs);
            return gs_rethrow(code, "cannot parse stroke brush");
        }
    }

    xps_end_opacity(ctx, opacity_mask_uri, dict, opacity_att, opacity_mask_tag);

    if (navigate_uri_att)
        (void)pdfmark_link(ctx, navigate_uri_att, &path_bbox, samples);

    gs_grestore(ctx->pgs);
    return 0;
}
