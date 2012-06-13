/* Copyright (C) 2001-2012 Artifex Software, Inc.
   All Rights Reserved.

   This software is provided AS-IS with no warranty, either express or
   implied.

   This software is distributed under license and may not be copied,
   modified or distributed except as expressly authorized under the terms
   of the license contained in the file LICENSE in this distribution.

   Refer to licensing information at http://www.artifex.com or contact
   Artifex Software, Inc.,  7 Mt. Lassen Drive - Suite A-134, San Rafael,
   CA  94903, U.S.A., +1(415)492-9861, for further information.
*/


/* SVG interpreter - vector art support */

#include "ghostsvg.h"

static void svg_fill(svg_context_t *ctx)
{
    svg_set_fill_color(ctx);
    if (ctx->fill_rule == 0)
        gs_eofill(ctx->pgs);
    else
        gs_fill(ctx->pgs);
}

static void svg_stroke(svg_context_t *ctx)
{
    svg_set_stroke_color(ctx);
    gs_stroke(ctx->pgs);
}

int
svg_parse_rect(svg_context_t *ctx, svg_item_t *node)
{
    char *x_att = svg_att(node, "x");
    char *y_att = svg_att(node, "y");
    char *w_att = svg_att(node, "width");
    char *h_att = svg_att(node, "height");
    char *rx_att = svg_att(node, "rx");
    char *ry_att = svg_att(node, "ry");

    float x = 0;
    float y = 0;
    float w = 0;
    float h = 0;
    float rx = 0;
    float ry = 0;
    int draw_rounded = 1;

    svg_parse_common(ctx, node);

    if (x_att) x = svg_parse_length(x_att, ctx->viewbox_width, 12);
    if (y_att) y = svg_parse_length(y_att, ctx->viewbox_height, 12);
    if (w_att) w = svg_parse_length(w_att, ctx->viewbox_width, 12);
    if (h_att) h = svg_parse_length(h_att, ctx->viewbox_height, 12);
    if (rx_att) rx = svg_parse_length(rx_att, ctx->viewbox_width, 12);
    if (ry_att) ry = svg_parse_length(ry_att, ctx->viewbox_height, 12);

    if (rx_att && !ry_att)
        ry = rx;
    if (ry_att && !rx_att)
        rx = ry;
    if (rx > w * 0.5)
        rx = w * 0.5;
    if (ry > h * 0.5)
        ry = h * 0.5;

    if (w <= 0 || h <= 0)
        return 0;

    /* TODO: we need elliptical arcs to draw rounded corners */

    if (ctx->fill_is_set)
    {
        gs_moveto(ctx->pgs, x, y);
        gs_lineto(ctx->pgs, x + w, y);
        gs_lineto(ctx->pgs, x + w, y + h);
        gs_lineto(ctx->pgs, x, y + h);
        gs_closepath(ctx->pgs);
        svg_fill(ctx);
    }

    if (ctx->stroke_is_set)
    {
        gs_moveto(ctx->pgs, x, y);
        gs_lineto(ctx->pgs, x + w, y);
        gs_lineto(ctx->pgs, x + w, y + h);
        gs_lineto(ctx->pgs, x, y + h);
        gs_closepath(ctx->pgs);
        svg_stroke(ctx);
    }

    return 0;
}

int
svg_parse_circle(svg_context_t *ctx, svg_item_t *node)
{
    char *cx_att = svg_att(node, "cx");
    char *cy_att = svg_att(node, "cy");
    char *r_att = svg_att(node, "r");

    float cx = 0;
    float cy = 0;
    float r = 0;

    svg_parse_common(ctx, node);

    if (cx_att) cx = svg_parse_length(cx_att, ctx->viewbox_width, 12);
    if (cy_att) cy = svg_parse_length(cy_att, ctx->viewbox_height, 12);
    if (r_att) r = svg_parse_length(r_att, ctx->viewbox_size, 12);

    if (r <= 0)
        return 0;

    if (ctx->fill_is_set)
    {
        gs_moveto(ctx->pgs, cx + r, cy);
        gs_arcn(ctx->pgs, cx, cy, r, 0, 90);
        gs_arcn(ctx->pgs, cx, cy, r, 90, 180);
        gs_arcn(ctx->pgs, cx, cy, r, 180, 270);
        gs_arcn(ctx->pgs, cx, cy, r, 270, 360);
        gs_closepath(ctx->pgs);
        svg_fill(ctx);
    }

    if (ctx->stroke_is_set)
    {
        gs_moveto(ctx->pgs, cx + r, cy);
        gs_arcn(ctx->pgs, cx, cy, r, 0, 90);
        gs_arcn(ctx->pgs, cx, cy, r, 90, 180);
        gs_arcn(ctx->pgs, cx, cy, r, 180, 270);
        gs_arcn(ctx->pgs, cx, cy, r, 270, 360);
        gs_closepath(ctx->pgs);
        svg_stroke(ctx);
    }

    return 0;
}

int
svg_parse_ellipse(svg_context_t *ctx, svg_item_t *node)
{
    char *cx_att = svg_att(node, "cx");
    char *cy_att = svg_att(node, "cy");
    char *rx_att = svg_att(node, "rx");
    char *ry_att = svg_att(node, "ry");

    float cx = 0;
    float cy = 0;
    float rx = 0;
    float ry = 0;

    svg_parse_common(ctx, node);

    if (cx_att) cx = svg_parse_length(cx_att, ctx->viewbox_width, 12);
    if (cy_att) cy = svg_parse_length(cy_att, ctx->viewbox_height, 12);
    if (rx_att) rx = svg_parse_length(rx_att, ctx->viewbox_width, 12);
    if (ry_att) ry = svg_parse_length(ry_att, ctx->viewbox_height, 12);

    if (rx <= 0 || ry <= 0)
        return 0;

    /* TODO: we need elliptic arcs */

#if 0
    if (ctx->fill_is_set)
    {
        svg_fill(ctx);
    }

    if (ctx->stroke_is_set)
    {
        svg_stroke(ctx);
    }
#endif

    return 0;
}

int
svg_parse_line(svg_context_t *ctx, svg_item_t *node)
{
    char *x1_att = svg_att(node, "x1");
    char *y1_att = svg_att(node, "y1");
    char *x2_att = svg_att(node, "x2");
    char *y2_att = svg_att(node, "y2");

    float x1 = 0;
    float y1 = 0;
    float x2 = 0;
    float y2 = 0;

    svg_parse_common(ctx, node);

    if (x1_att) x1 = svg_parse_length(x1_att, ctx->viewbox_width, 12);
    if (y1_att) y1 = svg_parse_length(y1_att, ctx->viewbox_height, 12);
    if (x2_att) x2 = svg_parse_length(x2_att, ctx->viewbox_width, 12);
    if (y2_att) y2 = svg_parse_length(y2_att, ctx->viewbox_height, 12);

    if (ctx->stroke_is_set)
    {
        gs_moveto(ctx->pgs, x1, y1);
        gs_lineto(ctx->pgs, x2, y2);

        svg_stroke(ctx);
    }

    return 0;
}

static int
svg_parse_polygon_imp(svg_context_t *ctx, svg_item_t *node, int doclose)
{
    char *str = svg_att(node, "points");
    char number[20];
    int numberlen;
    float args[2];
    int nargs;
    int isfirst;

    if (!str)
        return 0;

    isfirst = 1;
    nargs = 0;

    while (*str)
    {
        while (svg_is_whitespace_or_comma(*str))
            str ++;

        if (svg_is_digit(*str))
        {
            numberlen = 0;
            while (svg_is_digit(*str) && numberlen < sizeof(number) - 1)
                number[numberlen++] = *str++;
            number[numberlen] = 0;
            args[nargs++] = atof(number);
        }

        if (nargs == 2)
        {
            if (isfirst)
            {
                gs_moveto(ctx->pgs, args[0], args[1]);
                isfirst = 0;
            }
            else
            {
                gs_lineto(ctx->pgs, args[0], args[1]);
            }
            nargs = 0;
        }
    }

    return 0;
}

int
svg_parse_polyline(svg_context_t *ctx, svg_item_t *node)
{
    svg_parse_common(ctx, node);

    if (ctx->stroke_is_set)
    {
        svg_parse_polygon_imp(ctx, node, 0);
        svg_stroke(ctx);
    }

    return 0;
}

int
svg_parse_polygon(svg_context_t *ctx, svg_item_t *node)
{
    svg_parse_common(ctx, node);

    if (ctx->fill_is_set)
    {
        svg_parse_polygon_imp(ctx, node, 1);
        svg_fill(ctx);
    }

    if (ctx->stroke_is_set)
    {
        svg_parse_polygon_imp(ctx, node, 1);
        svg_stroke(ctx);
    }

    return 0;
}

static int
svg_parse_path_data(svg_context_t *ctx, char *str)
{
    gs_point pt;
    float x1, y1, x2, y2;

    int cmd;
    int numberlen;
    char number[20];
    float args[6];
    int nargs;

    /* saved control point for smooth curves */
    int reset_smooth = 1;
    float smooth_x = 0.0;
    float smooth_y = 0.0;

    cmd = 0;
    nargs = 0;

    gs_moveto(ctx->pgs, 0.0, 0.0); /* for the case of opening 'm' */

    while (*str)
    {
        while (svg_is_whitespace_or_comma(*str))
            str ++;

        if (svg_is_digit(*str))
        {
            numberlen = 0;
            while (svg_is_digit(*str) && numberlen < sizeof(number) - 1)
                number[numberlen++] = *str++;
            number[numberlen] = 0;
            if (nargs == 6)
                return gs_throw(-1, "stack overflow in path data");
            args[nargs++] = atof(number);
        }
        else if (svg_is_alpha(*str))
        {
            cmd = *str++;
        }
        else if (*str == 0)
        {
            return 0;
        }
        else
        {
            return gs_throw1(-1, "syntax error in path data: '%c'", *str);
        }

        if (reset_smooth)
        {
            smooth_x = 0.0;
            smooth_y = 0.0;
        }

        reset_smooth = 1;

        switch (cmd)
        {
        case 'M':
            if (nargs == 2)
            {
                // dprintf2("moveto %g %g\n", args[0], args[1]);
                gs_moveto(ctx->pgs, args[0], args[1]);
                nargs = 0;
                cmd = 'L'; /* implicit lineto after */
            }
            break;

        case 'm':
            if (nargs == 2)
            {
                // dprintf2("rmoveto %g %g\n", args[0], args[1]);
                gs_rmoveto(ctx->pgs, args[0], args[1]);
                nargs = 0;
                cmd = 'l'; /* implicit lineto after */
            }
            break;

        case 'Z':
        case 'z':
            if (nargs == 0)
            {
                // dprintf("closepath\n");
                gs_closepath(ctx->pgs);
            }
            break;

        case 'L':
            if (nargs == 2)
            {
                // dprintf2("lineto %g %g\n", args[0], args[1]);
                gs_lineto(ctx->pgs, args[0], args[1]);
                nargs = 0;
            }
            break;

        case 'l':
            if (nargs == 2)
            {
                // dprintf2("rlineto %g %g\n", args[0], args[1]);
                gs_rlineto(ctx->pgs, args[0], args[1]);
                nargs = 0;
            }
            break;

        case 'H':
            if (nargs == 1)
            {
                gs_currentpoint(ctx->pgs, &pt);
                // dprintf1("hlineto %g\n", args[0]);
                gs_lineto(ctx->pgs, args[0], pt.y);
                nargs = 0;
            }
            break;

        case 'h':
            if (nargs == 1)
            {
                // dprintf1("rhlineto %g\n", args[0]);
                gs_rlineto(ctx->pgs, args[0], 0.0);
                nargs = 0;
            }
            break;

        case 'V':
            if (nargs == 1)
            {
                gs_currentpoint(ctx->pgs, &pt);
                // dprintf1("vlineto %g\n", args[0]);
                gs_lineto(ctx->pgs, pt.x, args[0]);
                nargs = 0;
            }
            break;

        case 'v':
            if (nargs == 1)
            {
                // dprintf1("rvlineto %g\n", args[0]);
                gs_rlineto(ctx->pgs, 0.0, args[0]);
                nargs = 0;
            }
            break;

        case 'C':
            reset_smooth = 0;
            if (nargs == 6)
            {
                gs_curveto(ctx->pgs, args[0], args[1], args[2], args[3], args[4], args[5]);
                smooth_x = args[4] - args[2];
                smooth_y = args[5] - args[3];
                nargs = 0;
            }
            break;

        case 'c':
            reset_smooth = 0;
            if (nargs == 6)
            {
                gs_rcurveto(ctx->pgs, args[0], args[1], args[2], args[3], args[4], args[5]);
                smooth_x = args[4] - args[2];
                smooth_y = args[5] - args[3];
                nargs = 0;
            }
            break;

        case 'S':
            reset_smooth = 0;
            if (nargs == 4)
            {
                gs_currentpoint(ctx->pgs, &pt);
                gs_curveto(ctx->pgs,
                        pt.x + smooth_x, pt.y + smooth_y,
                        args[0], args[1],
                        args[2], args[3]);
                smooth_x = args[2] - args[0];
                smooth_y = args[3] - args[1];
                nargs = 0;
            }
            break;

        case 's':
            reset_smooth = 0;
            if (nargs == 4)
            {
                gs_currentpoint(ctx->pgs, &pt);
                gs_curveto(ctx->pgs,
                        pt.x + smooth_x, pt.y + smooth_y,
                        pt.x + args[0], pt.y + args[1],
                        pt.x + args[2], pt.y + args[3]);
                smooth_x = args[2] - args[0];
                smooth_y = args[3] - args[1];
                nargs = 0;
            }
            break;

        case 'Q':
            reset_smooth = 0;
            if (nargs == 4)
            {
                gs_currentpoint(ctx->pgs, &pt);
                x1 = args[0];
                y1 = args[1];
                x2 = args[2];
                y2 = args[3];
                gs_curveto(ctx->pgs,
                        (pt.x + 2 * x1) / 3, (pt.y + 2 * y1) / 3,
                        (x2 + 2 * x1) / 3, (y2 + 2 * y1) / 3,
                        x2, y2);
                smooth_x = x2 - x1;
                smooth_y = y2 - y1;
                nargs = 0;
            }
            break;

        case 'q':
            reset_smooth = 0;
            if (nargs == 4)
            {
                gs_currentpoint(ctx->pgs, &pt);
                x1 = args[0] + pt.x;
                y1 = args[1] + pt.y;
                x2 = args[2] + pt.x;
                y2 = args[3] + pt.y;
                gs_curveto(ctx->pgs,
                        (pt.x + 2 * x1) / 3, (pt.y + 2 * y1) / 3,
                        (x2 + 2 * x1) / 3, (y2 + 2 * y1) / 3,
                        x2, y2);
                smooth_x = x2 - x1;
                smooth_y = y2 - y1;
                nargs = 0;
            }
            break;

        case 'T':
            reset_smooth = 0;
            if (nargs == 4)
            {
                gs_currentpoint(ctx->pgs, &pt);
                x1 = pt.x + smooth_x;
                y1 = pt.y + smooth_y;
                x2 = args[0];
                y2 = args[1];
                gs_curveto(ctx->pgs,
                        (pt.x + 2 * x1) / 3, (pt.y + 2 * y1) / 3,
                        (x2 + 2 * x1) / 3, (y2 + 2 * y1) / 3,
                        x2, y2);
                smooth_x = x2 - x1;
                smooth_y = y2 - y1;
                nargs = 0;
            }
            break;

        case 't':
            reset_smooth = 0;
            if (nargs == 4)
            {
                gs_currentpoint(ctx->pgs, &pt);
                x1 = pt.x + smooth_x;
                y1 = pt.y + smooth_y;
                x2 = args[0] + pt.x;
                y2 = args[1] + pt.y;
                gs_curveto(ctx->pgs,
                        (pt.x + 2 * x1) / 3, (pt.y + 2 * y1) / 3,
                        (x2 + 2 * x1) / 3, (y2 + 2 * y1) / 3,
                        x2, y2);
                smooth_x = x2 - x1;
                smooth_y = y2 - y1;
                nargs = 0;
            }
            break;

        case 0:
            if (nargs != 0)
                return gs_throw(-1, "path data must begin with a command");
            break;

        default:
            return gs_throw1(-1, "unrecognized command in path data: '%c'", cmd);
        }
    }

    return 0;
}

int
svg_parse_path(svg_context_t *ctx, svg_item_t *node)
{
    char *d_att = svg_att(node, "d");
    char *path_length_att = svg_att(node, "pathLength");

    svg_parse_common(ctx, node);

    if (d_att)
    {
        if (ctx->fill_is_set)
        {
            svg_parse_path_data(ctx, d_att);
            svg_fill(ctx);
        }

        if (ctx->stroke_is_set)
        {
            svg_parse_path_data(ctx, d_att);
            svg_stroke(ctx);
        }
    }

    return 0;
}
