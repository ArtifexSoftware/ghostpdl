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


/* XPS interpreter - analyze page checking for transparency.
 * This is a stripped down parser that looks for alpha values < 1.0 in
 * any part of the page.
 */

#include "ghostxps.h"

static int xps_brush_has_transparency(xps_context_t *ctx, char *base_uri, xps_item_t *root);
static int xps_glyphs_has_transparency(xps_context_t *ctx, char *base_uri, xps_item_t *root);

static int
xps_remote_resource_dictionary_has_transparency(xps_context_t *ctx, char *base_uri, char *source_att)
{
    char part_name[1024];
    char part_uri[1024];
    xps_part_t *part;
    xps_item_t *xml;
    char *s;
    int has_transparency;

    xps_absolute_path(part_name, base_uri, source_att, sizeof part_name);
    part = xps_read_part(ctx, part_name);
    if (!part)
    {
        return gs_throw1(-1, "cannot find remote resource part '%s'", part_name);
    }

    xml = xps_parse_xml(ctx, part->data, part->size);
    if (!xml)
    {
        xps_free_part(ctx, part);
        return gs_rethrow(-1, "cannot parse xml");
    }

    if (strcmp(xps_tag(xml), "ResourceDictionary"))
    {
        xps_free_item(ctx, xml);
        xps_free_part(ctx, part);
        return gs_throw1(-1, "expected ResourceDictionary element (found %s)", xps_tag(xml));
    }

    gs_strlcpy(part_uri, part_name, sizeof part_uri);
    s = strrchr(part_uri, '/');
    if (s)
        s[1] = 0;

    has_transparency = xps_resource_dictionary_has_transparency(ctx, part_uri, xml);
    xps_free_item(ctx, xml);
    xps_free_part(ctx, part);
    return has_transparency;
}

int
xps_resource_dictionary_has_transparency(xps_context_t *ctx, char *base_uri, xps_item_t *root)
{
    char *source;
    xps_item_t *node;

    source = xps_att(root, "Source");
    if (source)
        return xps_remote_resource_dictionary_has_transparency(ctx, base_uri, source);

    for (node = xps_down(root); node; node = xps_next(node))
    {
        if (!strcmp(xps_tag(node), "RadialGradientBrush") ||
                !strcmp(xps_tag(node), "LinearGradientBrush") ||
                !strcmp(xps_tag(node), "SolidColorBrush") ||
                !strcmp(xps_tag(node), "VisualBrush") ||
                !strcmp(xps_tag(node), "ImageBrush"))
            if (xps_brush_has_transparency(ctx, base_uri, node))
                return 1;
        if (!strcmp(xps_tag(node), "Glyphs"))
            if (xps_glyphs_has_transparency(ctx, base_uri, node))
                return 1;
    }

    return 0;
}

static int
xps_gradient_stops_have_transparency(xps_context_t *ctx, char *base_uri, xps_item_t *root)
{
    xps_item_t *node;
    gs_color_space *colorspace;
    char *color_att;
    float samples[XPS_MAX_COLORS];

    for (node = xps_down(root); node; node = xps_next(node))
    {
        if (!strcmp(xps_tag(node), "GradientStop"))
        {
            color_att = xps_att(node, "Color");
            if (color_att)
            {
                xps_parse_color(ctx, base_uri, color_att, &colorspace, samples);
                if (samples[0] < 1.0)
                {
                    //dmputs(ctx->memory, "page has transparency: GradientStop has alpha\n");
                    return 1;
                }
            }
        }
    }

    return 0;
}

static int
xps_gradient_brush_has_transparency(xps_context_t *ctx, char *base_uri, xps_item_t *root)
{
    xps_item_t *node;
    char *opacity_att;

    opacity_att = xps_att(root, "Opacity");
    if (opacity_att)
    {
        if (atof(opacity_att) < 1.0)
        {
            //dmputs(ctx->memory, "page has transparency: GradientBrush Opacity\n");
            return 1;
        }
    }

    for (node = xps_down(root); node; node = xps_next(node))
    {
        if (!strcmp(xps_tag(node), "RadialGradientBrush.GradientStops"))
        {
            if (xps_gradient_stops_have_transparency(ctx, base_uri, node))
                return 1;
        }
        if (!strcmp(xps_tag(node), "LinearGradientBrush.GradientStops"))
        {
            if (xps_gradient_stops_have_transparency(ctx, base_uri, node))
                return 1;
        }
    }

    return 0;
}

static int
xps_brush_has_transparency(xps_context_t *ctx, char *base_uri, xps_item_t *root)
{
    char *opacity_att;
    char *color_att;
    xps_item_t *node;

    gs_color_space *colorspace;
    float samples[XPS_MAX_COLORS];

    if (!strcmp(xps_tag(root), "SolidColorBrush"))
    {
        opacity_att = xps_att(root, "Opacity");
        if (opacity_att)
        {
            float opacity = atof(opacity_att);
            if (opacity < 1.0 && opacity != 0.0)
            {
                //dmputs(ctx->memory, "page has transparency: SolidColorBrush Opacity\n");
                return 1;
            }
        }

        color_att = xps_att(root, "Color");
        if (color_att)
        {
            xps_parse_color(ctx, base_uri, color_att, &colorspace, samples);
            if (samples[0] < 1.0 && samples[0] != 0.0)
            {
                //dmputs(ctx->memory, "page has transparency: SolidColorBrush Color has alpha\n");
                return 1;
            }
        }
    }

    if (!strcmp(xps_tag(root), "VisualBrush"))
    {
        char *opacity_att = xps_att(root, "Opacity");
        if (opacity_att)
        {
            if (atof(opacity_att) < 1.0)
            {
                //dmputs(ctx->memory, "page has transparency: VisualBrush Opacity\n");
                return 1;
            }
        }

        for (node = xps_down(root); node; node = xps_next(node))
        {
            if (!strcmp(xps_tag(node), "VisualBrush.Visual"))
            {
                if (xps_element_has_transparency(ctx, base_uri, xps_down(node)))
                    return 1;
            }
        }
    }

    if (!strcmp(xps_tag(root), "ImageBrush"))
    {
        if (xps_image_brush_has_transparency(ctx, base_uri, root))
            return 1;
    }

    if (!strcmp(xps_tag(root), "LinearGradientBrush"))
    {
        if (xps_gradient_brush_has_transparency(ctx, base_uri, root))
            return 1;
    }

    if (!strcmp(xps_tag(root), "RadialGradientBrush"))
    {
        if (xps_gradient_brush_has_transparency(ctx, base_uri, root))
            return 1;
    }

    return 0;
}

static int
xps_path_has_transparency(xps_context_t *ctx, char *base_uri, xps_item_t *root)
{
    xps_item_t *node;

    for (node = xps_down(root); node; node = xps_next(node))
    {
        if (!strcmp(xps_tag(node), "Path.OpacityMask"))
        {
            //dmputs(ctx->memory, "page has transparency: Path.OpacityMask\n");
            return 1;
        }

        if (!strcmp(xps_tag(node), "Path.Stroke"))
        {
            if (xps_brush_has_transparency(ctx, base_uri, xps_down(node)))
                return 1;
        }

        if (!strcmp(xps_tag(node), "Path.Fill"))
        {
            if (xps_brush_has_transparency(ctx, base_uri, xps_down(node)))
                return 1;
        }
    }

    return 0;
}

static int
xps_glyphs_has_transparency(xps_context_t *ctx, char *base_uri, xps_item_t *root)
{
    xps_item_t *node;

    for (node = xps_down(root); node; node = xps_next(node))
    {
        if (!strcmp(xps_tag(node), "Glyphs.OpacityMask"))
        {
            //dmputs(ctx->memory, "page has transparency: Glyphs.OpacityMask\n");
            return 1;
        }

        if (!strcmp(xps_tag(node), "Glyphs.Fill"))
        {
            if (xps_brush_has_transparency(ctx, base_uri, xps_down(node)))
                return 1;
        }
    }

    return 0;
}

static int
xps_canvas_has_transparency(xps_context_t *ctx, char *base_uri, xps_item_t *root)
{
    xps_item_t *node;

    for (node = xps_down(root); node; node = xps_next(node))
    {
        if (!strcmp(xps_tag(node), "Canvas.Resources"))
        {
            if (xps_down(node) && xps_resource_dictionary_has_transparency(ctx, base_uri, xps_down(node)))
                return 1;
        }

        if (!strcmp(xps_tag(node), "Canvas.OpacityMask"))
        {
            //dmputs(ctx->memory, "page has transparency: Canvas.OpacityMask\n");
            return 1;
        }

        if (xps_element_has_transparency(ctx, base_uri, node))
            return 1;
    }

    return 0;
}

int
xps_element_has_transparency(xps_context_t *ctx, char *base_uri, xps_item_t *node)
{
    char *opacity_att;
    char *stroke_att;
    char *fill_att;

    gs_color_space *colorspace;
    float samples[XPS_MAX_COLORS];

    stroke_att = xps_att(node, "Stroke");
    if (stroke_att)
    {
        xps_parse_color(ctx, base_uri, stroke_att, &colorspace, samples);
        if (samples[0] < 1.0 && samples[0] != 0.0)
        {
            //dmprintf1(ctx->memory, "page has transparency: Stroke alpha=%g\n", samples[0]);
            return 1;
        }
    }

    fill_att = xps_att(node, "Fill");
    if (fill_att)
    {
        xps_parse_color(ctx, base_uri, fill_att, &colorspace, samples);
        if (samples[0] < 1.0 && samples[0] != 0.0)
        {
            //dmprintf1(ctx->memory, "page has transparency: Fill alpha=%g\n", samples[0]);
            return 1;
        }
    }

    opacity_att = xps_att(node, "Opacity");
    if (opacity_att)
    {
        float opacity = atof(opacity_att);
        if (opacity < 1.0 && opacity != 0.0)
        {
            //dmprintf1(ctx->memory, "page has transparency: Opacity=%g\n", atof(opacity_att));
            return 1;
        }
    }

    if (xps_att(node, "OpacityMask"))
    {
        //dmputs(ctx->memory, "page has transparency: OpacityMask\n");
        return 1;
    }

    if (!strcmp(xps_tag(node), "Path"))
        if (xps_path_has_transparency(ctx, base_uri, node))
            return 1;
    if (!strcmp(xps_tag(node), "Glyphs"))
        if (xps_glyphs_has_transparency(ctx, base_uri, node))
            return 1;
    if (!strcmp(xps_tag(node), "Canvas"))
        if (xps_canvas_has_transparency(ctx, base_uri, node))
            return 1;

    return 0;
}
