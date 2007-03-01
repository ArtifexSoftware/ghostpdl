#include "ghostxps.h"

/*
 * Parse an abbreviated geometry string, and call
 * ghostscript moveto/lineto/curveto functions to
 * build up a path.
 */

int
xps_parse_abbreviated_geometry(xps_context_t *ctx, char *geom)
{
    char *args[strlen(geom) + 1];
    char **pargs = args;
    char *s = geom;
    int fillrule = 0;
    int i, n;
    int cmd, old;

    dprintf1("new path (%.70s)\n", geom);
    gs_newpath(ctx->pgs);

    while (*s)
    {
	if ((*s >= 'A' && *s <= 'Z') || (*s >= 'a' && *s <= 'z'))
	{
	    *pargs++ = s++;
	}
	else if ((*s >= '0' && *s <= '9') || *s == '.' || *s == '+' || *s == '-')
	{
	    *pargs++ = s;
	    while ((*s >= '0' && *s <= '9') || *s == '.' || *s == '+' || *s == '-')
		s ++;
	}
	else
	{
	    s++;
	}
    }

    pargs[0] = s;
    pargs[1] = 0;

    n = pargs - args;
    i = 0;

    old = 0;

    while (i < n)
    {
	cmd = args[i][0];
	if (cmd == '+' || cmd == '.' || cmd == '-' || (cmd >= '0' && cmd <= '9'))
	    cmd = old; /* it's a number, repeat old command */
	else
	    i ++;

	switch (cmd)
	{
	case 'F':
	    fillrule = atoi(args[i]);
	    i ++;
	    break;

	case 'M':
	    gs_moveto(ctx->pgs, atof(args[i]), atof(args[i+1]));
	    dprintf2("moveto %g %g\n", atof(args[i]), atof(args[i+1]));
	    i += 2;
	    break;
	case 'm':
	    dprintf2("rmoveto %g %g\n", atof(args[i]), atof(args[i+1]));
	    i += 2;
	    break;

	case 'L':
	    gs_lineto(ctx->pgs, atof(args[i]), atof(args[i+1]));
	    dprintf2("lineto %g %g\n", atof(args[i]), atof(args[i+1]));
	    i += 2;
	    break;
	case 'l':
	    dprintf2("rlineto %g %g\n", atof(args[i]), atof(args[i+1]));
	    i += 2;
	    break;

	case 'H':
	    dprintf1("hlineto %g\n", atof(args[i]));
	    i += 1;
	    break;
	case 'h':
	    dprintf1("rhlineto %g\n", atof(args[i]));
	    i += 1;
	    break;

	case 'V':
	    dprintf1("vlineto %g\n", atof(args[i]));
	    i += 1;
	    break;
	case 'v':
	    dprintf1("rvlineto %g\n", atof(args[i]));
	    i += 1;
	    break;

	case 'C':
	    gs_curveto(ctx->pgs,
		    atof(args[i]), atof(args[i+1]),
		    atof(args[i+2]), atof(args[i+3]),
		    atof(args[i+4]), atof(args[i+5]));
	    dprintf6("curveto %g %g %g %g %g %g\n",
		    atof(args[i]), atof(args[i+1]),
		    atof(args[i+2]), atof(args[i+3]),
		    atof(args[i+4]), atof(args[i+5]));
	    i += 6;
	    break;
	case 'c':
	    dprintf6("rcurveto %g %g %g %g %g %g\n",
		    atof(args[i]), atof(args[i+1]),
		    atof(args[i+2]), atof(args[i+3]),
		    atof(args[i+4]), atof(args[i+5]));
	    i += 6;
	    break;

	case 'Q': case 'q':
	case 'S': case 's':
	    dprintf1("path command '%c' for quadratic or smooth curve is not implemented\n", cmd);
	    i += 4;
	    break;

	case 'A': case 'a':
	    dprintf1("path command '%c' for elliptical arc is not implemented\n", cmd);
	    i += 7;
	    break;

	case 'Z':
	case 'z':
	    gs_closepath(ctx->pgs);
	    dputs("closepath\n");
	    break;

	default:
	    /* eek */
	    break;
	}

	old = cmd;
    }
}

/*
 * Parse an XPS <Path> element, and call relevant ghostscript
 * functions for drawing and/or clipping the child elements.
 */

int
xps_parse_path(xps_context_t *ctx, xps_item_t *root)
{
    xps_item_t *node;

    char *transform_att;
    char *clip_att;
    char *data_att;
    char *fill_att;
    char *stroke_att;
    char *opacity_att;
    char *opacity_mask_att;

    xps_item_t *transform_tag = NULL;
    xps_item_t *clip_tag = NULL;
    xps_item_t *data_tag = NULL;
    xps_item_t *fill_tag = NULL;
    xps_item_t *stroke_tag = NULL;
    xps_item_t *opacity_mask_tag;

    char *stroke_dash_array_att;
    char *stroke_dash_cap_att;
    char *stroke_dash_offset_att;
    char *stroke_end_line_cap_att;
    char *stroke_start_line_cap_att;
    char *stroke_line_join_att;
    char *stroke_miter_limit_att;
    char *stroke_thickness_att;

    int saved = 0;

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

    /*
     * Act on the information we have gathered:
     */

    if (fill_tag && !strcmp(xps_tag(fill_tag), "SolidColorBrush"))
    {
	fill_att = xps_att(fill_tag, "Color");
	fill_tag = NULL;
    }

    if (stroke_tag && !strcmp(xps_tag(stroke_tag), "SolidColorBrush"))
    {
	stroke_att = xps_att(stroke_tag, "Color");
	stroke_tag = NULL;
    }

    /* if we have a transform, push that on the gstate and remember to restore */

    if (transform_att || transform_tag)
    {
	gs_matrix transform;

	dputs("  path transform\n");

	if (!saved)
	{
	    gs_gsave(ctx->pgs);
	    saved = 1;
	}

	if (transform_att)
	    xps_parse_render_transform(ctx, transform_att, &transform);
	if (transform_tag)
	    xps_parse_matrix_transform(ctx, transform_tag, &transform);

	gs_concat(ctx->pgs, &transform);
    }

    /* if we have a clip mask, push that on the gstate and remember to restore */

    if (clip_att || clip_tag)
    {
	if (!saved)
	{
	    gs_gsave(ctx->pgs);
	    saved = 1;
	}

	dputs("  path clip\n");

	if (clip_att)
	{
	    xps_parse_abbreviated_geometry(ctx, clip_att);
	    gs_clip(ctx->pgs);
	}
    }

    if (!data_att)
    {
	gs_throw(-1, "no abbreviated geometry data for path");
	data_att = "M 0,0 L 0,100 100,100 100,0 Z";
    }

    /* if it's a solid color brush fill/stroke do just that. */

    if (fill_att)
    {
	float argb[4];
	dputs("  path solid color fill\n");
	xps_parse_color(ctx, fill_att, argb);
	if (argb[0] > 0.001)
	{
	    gs_setrgbcolor(ctx->pgs, argb[1], argb[2], argb[3]);
	    xps_parse_abbreviated_geometry(ctx, data_att);
	    gs_fill(ctx->pgs);
	}
    }

    if (stroke_att)
    {
	float argb[4];
	dputs("  path solid color stroke\n");
	xps_parse_color(ctx, stroke_att, argb);
	if (argb[0] > 0.001)
	{
	    gs_setrgbcolor(ctx->pgs, argb[1], argb[2], argb[3]);
	    xps_parse_abbreviated_geometry(ctx, data_att);
	    gs_stroke(ctx->pgs);
	}
    }

    /* if it's a visual brush or image, use the path as a clip mask to paint brush */

    if (fill_tag || stroke_tag)
    {
	if (!saved)
	{
	    gs_gsave(ctx->pgs);
	    saved = 1;
	}

	dputs("  path complex brush\n");

	gs_setrgbcolor(ctx->pgs, 0.5, 0.0, 0.3);
	xps_parse_abbreviated_geometry(ctx, data_att);
	gs_clip(ctx->pgs);
	dputs("clip\n");

	if (fill_tag)
	{
	    if (!strcmp(xps_tag(fill_tag), "ImageBrush"))
		xps_parse_image_brush(ctx, fill_tag);
	    if (!strcmp(xps_tag(fill_tag), "VisualBrush"))
		xps_parse_visual_brush(ctx, fill_tag);
	    if (!strcmp(xps_tag(fill_tag), "LinearGradientBrush"))
		xps_parse_linear_gradient_brush(ctx, fill_tag);
	    if (!strcmp(xps_tag(fill_tag), "RadialGradientBrush"))
		xps_parse_radial_gradient_brush(ctx, fill_tag);
	}
    }

    /* remember to restore if we did a gsave */

    if (saved)
    {
	gs_grestore(ctx->pgs);
    }

    return 0;
}

