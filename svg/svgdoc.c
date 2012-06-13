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


/* SVG interpreter - document parsing */

#include "ghostsvg.h"

int
svg_parse_common(svg_context_t *ctx, svg_item_t *node)
{
    char *fill_att = svg_att(node, "fill");
    char *stroke_att = svg_att(node, "stroke");
    char *transform_att = svg_att(node, "transform");
    char *fill_rule_att = svg_att(node, "fill-rule");
    char *stroke_width_att = svg_att(node, "stroke-width");
    char *stroke_linecap_att = svg_att(node, "stroke-linecap");
    char *stroke_linejoin_att = svg_att(node, "stroke-linejoin");
    char *stroke_miterlimit_att = svg_att(node, "stroke-miterlimit");

    int code;

    if (fill_att)
    {
        if (!strcmp(fill_att, "none"))
        {
            ctx->fill_is_set = 0;
        }
        else
        {
            ctx->fill_is_set = 1;
            code = svg_parse_color(fill_att, ctx->fill_color);
            if (code < 0)
                return gs_rethrow(code, "cannot parse fill attribute");
        }
    }

    if (stroke_att)
    {
        if (!strcmp(stroke_att, "none"))
        {
            ctx->stroke_is_set = 0;
        }
        else
        {
            ctx->stroke_is_set = 1;
            code = svg_parse_color(stroke_att, ctx->stroke_color);
            if (code < 0)
                return gs_rethrow(code, "cannot parse stroke attribute");
        }
    }

    if (transform_att)
    {
        code = svg_parse_transform(ctx, transform_att);
        if (code < 0)
            return gs_rethrow(code, "cannot parse transform attribute");
    }

    if (fill_rule_att)
    {
        if (!strcmp(fill_rule_att, "nonzero"))
            ctx->fill_rule = 1;
        if (!strcmp(fill_rule_att, "evenodd"))
            ctx->fill_rule = 0;
    }

    if (stroke_width_att)
    {
        if (!strcmp(stroke_width_att, "inherit"))
            ;
        else
            gs_setlinewidth(ctx->pgs, svg_parse_length(stroke_width_att, ctx->viewbox_size, 12));
    }
    else
    {
        gs_setlinewidth(ctx->pgs, 1);
    }

    if (stroke_linecap_att)
    {
        if (!strcmp(stroke_linecap_att, "butt"))
            gs_setlinecap(ctx->pgs, gs_cap_butt);
        if (!strcmp(stroke_linecap_att, "round"))
            gs_setlinecap(ctx->pgs, gs_cap_round);
        if (!strcmp(stroke_linecap_att, "square"))
            gs_setlinecap(ctx->pgs, gs_cap_square);
    }
    else
    {
        gs_setlinecap(ctx->pgs, gs_cap_butt);
    }

    if (stroke_linejoin_att)
    {
        if (!strcmp(stroke_linejoin_att, "miter"))
            gs_setlinejoin(ctx->pgs, gs_join_miter);
        if (!strcmp(stroke_linejoin_att, "round"))
            gs_setlinejoin(ctx->pgs, gs_join_round);
        if (!strcmp(stroke_linejoin_att, "bevel"))
            gs_setlinejoin(ctx->pgs, gs_join_bevel);
    }
    else
    {
        gs_setlinejoin(ctx->pgs, gs_join_miter);
    }

    if (stroke_miterlimit_att)
    {
        if (!strcmp(stroke_miterlimit_att, "inherit"))
            ;
        else
            gs_setmiterlimit(ctx->pgs, svg_parse_length(stroke_miterlimit_att, ctx->viewbox_size, 12));
    }
    else
    {
        gs_setmiterlimit(ctx->pgs, 4.0);
    }

    return 0;
}

static int
svg_parse_g(svg_context_t *ctx, svg_item_t *root)
{
    gs_matrix mtx;
    svg_item_t *node;
    int code;

    gs_currentmatrix(ctx->pgs, &mtx);

    svg_parse_common(ctx, root);

    for (node = svg_down(root); node; node = svg_next(node))
    {
        code = svg_parse_element(ctx, node);
        if (code < 0)
            return gs_rethrow(code, "cannot parse <g> element");
    }

    gs_setmatrix(ctx->pgs, &mtx);

    return 0;
}

int
svg_parse_element(svg_context_t *ctx, svg_item_t *root)
{
    char *tag = svg_tag(root);
    int code;

    if (!strcmp(tag, "g"))
        code = svg_parse_g(ctx, root);

    else if (!strcmp(tag, "title"))
        return 0;
    else if (!strcmp(tag, "desc"))
        return 0;

#if 0
    else if (!strcmp(tag, "defs"))
        code = svg_parse_defs(ctx, root);
    else if (!strcmp(tag, "symbol"))
        code = svg_parse_symbol(ctx, root);
    else if (!strcmp(tag, "use"))
        code = svg_parse_use(ctx, root);

    else if (!strcmp(tag, "image"))
        code = svg_parse_image(ctx, root);
#endif

    else if (!strcmp(tag, "path"))
        code = svg_parse_path(ctx, root);
    else if (!strcmp(tag, "rect"))
        code = svg_parse_rect(ctx, root);
    else if (!strcmp(tag, "circle"))
        code = svg_parse_circle(ctx, root);
    else if (!strcmp(tag, "ellipse"))
        code = svg_parse_ellipse(ctx, root);
    else if (!strcmp(tag, "line"))
        code = svg_parse_line(ctx, root);
    else if (!strcmp(tag, "polyline"))
        code = svg_parse_polyline(ctx, root);
    else if (!strcmp(tag, "polygon"))
        code = svg_parse_polygon(ctx, root);

#if 0
    else if (!strcmp(tag, "text"))
        code = svg_parse_text(ctx, root);
    else if (!strcmp(tag, "tspan"))
        code = svg_parse_text_span(ctx, root);
    else if (!strcmp(tag, "tref"))
        code = svg_parse_text_ref(ctx, root);
    else if (!strcmp(tag, "textPath"))
        code = svg_parse_text_path(ctx, root);
#endif

    else
    {
        /* debug print unrecognized tags */
        code = 0;
        svg_debug_item(root, 0);
    }

    if (code < 0)
        return gs_rethrow(code, "cannot parse svg element");

    return 0;
}

int
svg_parse_document(svg_context_t *ctx, svg_item_t *root)
{
    char *version_att;
    char *width_att;
    char *height_att;
    char *view_box_att;

    int use_transparency = 0;
    svg_item_t *node;
    int code;

    int version;
    int width;
    int height;

    float vb_min_x = 0;
    float vb_min_y = 0;
    float vb_width = 595;
    float vb_height = 842;

    if (strcmp(svg_tag(root), "svg"))
        return gs_throw1(-1, "expected svg element (found %s)", svg_tag(root));

    version_att = svg_att(root, "version");
    width_att = svg_att(root, "width");
    height_att = svg_att(root, "height");
    view_box_att = svg_att(root, "viewBox");

    version = 10;
    if (version_att)
        version = atof(version_att) * 10;

    if (view_box_att)
    {
        sscanf(view_box_att, "%g %g %g %g",
                &vb_min_x, &vb_min_y,
                &vb_width, &vb_height);
    }

    width = vb_width;
    if (!width_att)
        width = svg_parse_length(width_att, vb_width, 12);

    height = vb_height;
    if (!height_att)
        height = svg_parse_length(height_att, vb_height, 12);

    if (version > 12)
        gs_warn("svg document version is newer than we support");

    /* Setup new page */
    {
        gs_memory_t *mem = ctx->memory;
        gs_state *pgs = ctx->pgs;
        gx_device *dev = gs_currentdevice(pgs);
        gs_param_float_array fa;
        float fv[2];
        gs_c_param_list list;
        int code;

        gs_c_param_list_write(&list, mem);

        fv[0] = width;
        fv[1] = height;
        fa.persistent = false;
        fa.data = fv;
        fa.size = 2;

        code = param_write_float_array((gs_param_list *)&list, ".MediaSize", &fa);
        if ( code >= 0 )
        {
            gs_c_param_list_read(&list);
            code = gs_putdeviceparams(dev, (gs_param_list *)&list);
        }
        gs_c_param_list_release(&list);

        /* nb this is for the demo it is wrong and should be removed */
        gs_initgraphics(pgs);

        /* put the origin at the top of the page */

        gs_initmatrix(pgs);

        code = gs_scale(pgs, 1.0, -1.0);
        if (code < 0)
            return gs_rethrow(code, "cannot set page transform");

        code = gs_translate(pgs, 0.0, -height);
        if (code < 0)
            return gs_rethrow(code, "cannot set page transform");

        code = gs_erasepage(pgs);
        if (code < 0)
            return gs_rethrow(code, "cannot clear page");

        gs_setcolorspace(ctx->pgs, ctx->srgb);
    }

    /* Need viewbox dimensions for percentage based units */
    ctx->viewbox_width = vb_width;
    ctx->viewbox_height = vb_height;
    ctx->viewbox_size = sqrt(vb_width * vb_width + vb_height * vb_height) / sqrt(2.0);

    /* save the state with the original device before we push */
    gs_gsave(ctx->pgs);

    if (use_transparency)
    {
        code = gs_push_pdf14trans_device(ctx->pgs, false);
        if (code < 0)
            return gs_rethrow(code, "cannot install transparency device");
    }

    /* Draw contents */

    for (node = svg_down(root); node; node = svg_next(node))
    {
        code = svg_parse_element(ctx, node);
        if (code < 0)
            break;
    }

    if (use_transparency)
    {
        code = gs_pop_pdf14trans_device(ctx->pgs, false);
        if (code < 0)
            return gs_rethrow(code, "cannot uninstall transparency device");
    }

    /* Flush page */
    {
        code = svg_show_page(ctx, 1, true); /* copies, flush */
        if (code < 0)
            return gs_rethrow(code, "cannot flush page");
    }

    /* restore the original device, discarding the pdf14 compositor */
    gs_grestore(ctx->pgs);

    return 0;
}
