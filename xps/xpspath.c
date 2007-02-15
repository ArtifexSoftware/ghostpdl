#include "ghostxps.h"

int
xps_parse_abbreviated_geometry(xps_context_t *ctx, char *geom)
{
    char *args[strlen(geom) + 1];
    char **pargs = args;
    char *s = geom;
    int fillrule = 0;
    int i, n;

    dputs("new path\n");
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

    while (i < n)
    {
	int cmd = args[i][0];
	switch (cmd)
	{
	case 'F':
	    fillrule = atoi(args[i+1]);
	    i += 2;
	    break;

	case 'M':
	    gs_moveto(ctx->pgs, atof(args[i+1]), atof(args[i+2]));
	    dprintf2("moveto %g %g\n", atof(args[i+1]), atof(args[i+2]));
	    i += 3;
	    break;
	case 'm':
	    dprintf2("rmoveto %g %g\n", atof(args[i+1]), atof(args[i+2]));
	    i += 3;
	    break;

	case 'L':
	    gs_lineto(ctx->pgs, atof(args[i+1]), atof(args[i+2]));
	    dprintf2("lineto %g %g\n", atof(args[i+1]), atof(args[i+2]));
	    i += 3;
	    break;
	case 'l':
	    dprintf2("rlineto %g %g\n", atof(args[i+1]), atof(args[i+2]));
	    i += 3;
	    break;

	case 'H':
	    dprintf1("hlineto %g\n", atof(args[i+1]));
	    i += 2;
	    break;
	case 'h':
	    dprintf1("rhlineto %g\n", atof(args[i+1]));
	    i += 2;
	    break;

	case 'V':
	    dprintf1("vlineto %g\n", atof(args[i+1]));
	    i += 2;
	    break;
	case 'v':
	    dprintf1("rvlineto %g\n", atof(args[i+1]));
	    i += 2;
	    break;

	case 'C':
	    gs_curveto(ctx->pgs,
		    atof(args[i+1]), atof(args[i+2]),
		    atof(args[i+3]), atof(args[i+4]),
		    atof(args[i+5]), atof(args[i+6]));
	    dprintf6("curveto %g %g %g %g %g %g\n",
		    atof(args[i+1]), atof(args[i+2]),
		    atof(args[i+3]), atof(args[i+4]),
		    atof(args[i+5]), atof(args[i+6]));
	    i += 7;
	    break;
	case 'c':
	    dprintf6("rcurveto %g %g %g %g %g %g\n",
		    atof(args[i+1]), atof(args[i+2]),
		    atof(args[i+3]), atof(args[i+4]),
		    atof(args[i+5]), atof(args[i+6]));
	    i += 7;
	    break;

	case 'Q': case 'q':
	case 'S': case 's':
	    dprintf1("path command '%c' for quadratic or smooth curve is not implemented\n", cmd);
	    i += 5;
	    break;

	case 'A': case 'a':
	    dprintf1("path command '%c' for elliptical arc is not implemented\n", cmd);
	    i += 8;
	    break;

	case 'Z':
	case 'z':
	    gs_closepath(ctx->pgs);
	    dputs("closepath\n");
	    i += 1;
	    break;

	default:
	    /* eek */
	    i ++;
	    break;
	}
    }
}

int
xps_parse_path(xps_context_t *ctx, xps_item_t *root)
{
    xps_item_t *node;

    char *transform;
    char *clip;
    char *data;
    char *fill;
    char *stroke;
    char *opacity;
    char *opacity_mask;

    xps_item_t *transform_node = NULL;
    xps_item_t *clip_node = NULL;
    xps_item_t *data_node = NULL;
    xps_item_t *fill_node = NULL;
    xps_item_t *stroke_node = NULL;
    xps_item_t *opacity_mask_node;

    char *stroke_dash_array;
    char *stroke_dash_cap;
    char *stroke_dash_offset;
    char *stroke_end_line_cap;
    char *stroke_start_line_cap;
    char *stroke_line_join;
    char *stroke_miter_limit;
    char *stroke_thickness;

    int saved = 0;

    /*
     * Extract attributes and extended attributes.
     */

    transform = xps_att(root, "RenderTransform");
    clip = xps_att(root, "Clip");
    data = xps_att(root, "Data");
    fill = xps_att(root, "Fill");
    stroke = xps_att(root, "Stroke");
    opacity = xps_att(root, "Opacity");
    opacity_mask = xps_att(root, "OpacityMask");

    stroke_dash_array = xps_att(root, "StrokeDashArray");
    stroke_dash_cap = xps_att(root, "StrokeDashCap");
    stroke_dash_offset = xps_att(root, "StrokeDashOffset");
    stroke_end_line_cap = xps_att(root, "StrokeEndLineCap");
    stroke_start_line_cap = xps_att(root, "StrokeStartLineCap");
    stroke_line_join = xps_att(root, "StrokeLineJoin");
    stroke_miter_limit = xps_att(root, "StrokeMiterLimit");
    stroke_thickness = xps_att(root, "StrokeThickness");

    for (node = xps_down(root); node; node = xps_next(node))
    {
	if (!strcmp(xps_tag(node), "Path.RenderTransform"))
	    transform_node = xps_down(node);

	if (!strcmp(xps_tag(node), "Path.OpacityMask"))
	    opacity_mask_node = xps_down(node);

	if (!strcmp(xps_tag(node), "Path.Clip"))
	    clip_node = xps_down(node);

	if (!strcmp(xps_tag(node), "Path.Fill"))
	    fill_node = xps_down(node);

	if (!strcmp(xps_tag(node), "Path.Stroke"))
	    stroke_node = xps_down(node);

	if (!strcmp(xps_tag(node), "Path.Data"))
	    data_node = xps_down(node);
    }

    /*
     * Act on the information we have gathered:
     */

    if (fill_node && !strcmp(xps_tag(fill_node), "SolidColorBrush"))
    {
	fill = xps_att(fill_node, "Color");
	fill_node = NULL;
    }

    if (stroke_node && !strcmp(xps_tag(stroke_node), "SolidColorBrush"))
    {
	stroke = xps_att(stroke_node, "Color");
	stroke_node = NULL;
    }

    /* if we have a transform, push that on the gstate and remember to restore */

    if (transform || transform_node)
    {
	gs_matrix matrix;

	dputs("  path transform\n");

	if (!saved)
	{
	    gs_gsave(ctx->pgs);
	    saved = 1;
	}

	if (transform)
	    xps_parse_render_transform(ctx, transform, &matrix);
	if (transform_node)
	    xps_parse_matrix_transform(ctx, transform_node, &matrix);

	gs_concat(ctx->pgs, &matrix);
    }

    /* if we have a clip mask, push that on the gstate and remember to restore */

    if (clip || clip_node)
    {
	if (!saved)
	{
	    gs_gsave(ctx->pgs);
	    saved = 1;
	}

	dputs("  path clip\n");

	if (clip)
	{
	    xps_parse_abbreviated_geometry(ctx, clip);
	    gs_clippath(ctx->pgs);
	}
    }

    /* if it's a solid color brush fill/stroke do just that. */

    if (fill)
    {
	float argb[4];
	dputs("  path solid color fill\n");
	xps_parse_color(ctx, fill, argb);
	if (argb[0] > 0.001)
	{
	    gs_setrgbcolor(ctx->pgs, argb[1], argb[2], argb[3]);
	    xps_parse_abbreviated_geometry(ctx, data);
	    gs_fill(ctx->pgs);
	}
    }

    if (stroke)
    {
	float argb[4];
	dputs("  path solid color stroke\n");
	xps_parse_color(ctx, stroke, argb);
	if (argb[0] > 0.001)
	{
	    gs_setrgbcolor(ctx->pgs, argb[1], argb[2], argb[3]);
	    xps_parse_abbreviated_geometry(ctx, data);
	    gs_stroke(ctx->pgs);
	}
    }

    /* if it's a visual brush or image, use the path as a clip mask to paint brush */

    if (fill_node || stroke_node)
    {
	if (!saved)
	{
	    gs_gsave(ctx->pgs);
	    saved = 1;
	}

	dputs("  path complex brush\n");

	gs_setrgbcolor(ctx->pgs, 0.5, 0.0, 0.3);
	xps_parse_abbreviated_geometry(ctx, data);
	gs_fill(ctx->pgs);

	// draw brush
    }

    /* remember to restore if we did a gsave */

    if (saved)
    {
	gs_grestore(ctx->pgs);
    }

    return 0;
}

