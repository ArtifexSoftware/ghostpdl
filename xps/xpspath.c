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
    gs_point pt;
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
	    gs_rmoveto(ctx->pgs, atof(args[i]), atof(args[i+1]));
	    dprintf2("rmoveto %g %g\n", atof(args[i]), atof(args[i+1]));
	    i += 2;
	    break;

	case 'L':
	    gs_lineto(ctx->pgs, atof(args[i]), atof(args[i+1]));
	    dprintf2("lineto %g %g\n", atof(args[i]), atof(args[i+1]));
	    i += 2;
	    break;
	case 'l':
	    gs_rlineto(ctx->pgs, atof(args[i]), atof(args[i+1]));
	    dprintf2("rlineto %g %g\n", atof(args[i]), atof(args[i+1]));
	    i += 2;
	    break;

	case 'H':
	    gs_currentpoint(ctx->pgs, &pt);
	    gs_lineto(ctx->pgs, atof(args[i]), pt.y);
	    dprintf1("hlineto %g\n", atof(args[i]));
	    i += 1;
	    break;
	case 'h':
	    gs_rlineto(ctx->pgs, atof(args[i]), 0.0);
	    dprintf1("rhlineto %g\n", atof(args[i]));
	    i += 1;
	    break;

	case 'V':
	    gs_currentpoint(ctx->pgs, &pt);
	    gs_lineto(ctx->pgs, pt.x, atof(args[i]));
	    dprintf1("vlineto %g\n", atof(args[i]));
	    i += 1;
	    break;
	case 'v':
	    gs_rlineto(ctx->pgs, 0.0, atof(args[i]));
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
	    gs_rcurveto(ctx->pgs,
		    atof(args[i]), atof(args[i+1]),
		    atof(args[i+2]), atof(args[i+3]),
		    atof(args[i+4]), atof(args[i+5]));
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

    return 0;
}

/*
 * Parse the verbose XML representation of path data.
 *
 * TODO: what's with the IsFilled and IsStroked attributes?
 */

/* given two vectors find the angle between */
static inline double
angle_between(const gs_point u, const gs_point v)
{
    double det = u.x * v.y - u.y * v.x;
    double sign = (det < 0 ? -1.0 : 1.0);
    double magu = u.x * u.x + u.y * u.y;
    double magv = v.x * v.x + v.y * v.y;
    double udotv = u.x * v.x + u.y * v.y;
    return sign * acos(udotv / (magu * magv));
}

int
xps_parse_arc_segment(xps_context_t *ctx, xps_item_t *root)
{
    /* ArcSegment pretty much follows the SVG algorithm for converting an
       arc in endpoint representation to an arc in centerpoint
       representation.  Once in centerpoint it can be given to the
       graphics library in the form of a postscript arc.
     */

    int rotation_angle, is_large_arc, is_clockwise;
    float point_x, point_y;
    float size_x, size_y;

    char *point_att = xps_att(root, "Point");
    char *size_att = xps_att(root, "Size");
    char *rotation_angle_att = xps_att(root, "RotationAngle");
    char *is_large_arc_att = xps_att(root, "IsLargeArc");
    char *sweep_direction_att = xps_att(root, "SweepDirection");
    char *is_stroked_att = xps_att(root, "IsStroked");

    if (!point_att || !size_att || !rotation_angle_att || !is_large_arc_att || !sweep_direction_att)
	return gs_throw(-1, "ArcSegment element is missing attributes");

    sscanf(point_att, "%g,%g", &point_x, &point_y);
    sscanf(size_att, "%g,%g", &size_x, &size_y);
    rotation_angle = atof(rotation_angle_att);
    is_large_arc = !strcmp(is_large_arc_att, "true");
    is_clockwise = !strcmp(sweep_direction_att, "Clockwise");

    {
	gs_point start, end, size, midpoint, halfdis, thalfdis, tcenter, center;
	gs_matrix rotmat, save_ctm;
	double correction;
	double sign, rot, start_angle, delta_angle;
	double scale_num, scale_denom, scale;
	int code;

	sign = is_clockwise == is_large_arc ? -1.0 : 1.0;

	gs_currentpoint(ctx->pgs, &start);
	end.x = point_x; end.y = point_y;
	size.x = size_x; size.y = size_y;

#define SQR(x) ((x)*(x))
#define MULTOFSQUARES(x, y) (SQR((x)) * SQR((y)))

	gs_make_rotation(-rotation_angle, &rotmat);

	/* get the distance of the radical line */
	halfdis.x = (start.x - end.x) / 2.0;
	halfdis.y = (start.y - end.y) / 2.0;

	/* rotate the halfdis vector so x axis parallel ellipse x axis */
	gs_make_rotation(-rot, &rotmat);
	if ((code = gs_distance_transform(halfdis.x, halfdis.y, &rotmat, &thalfdis)) < 0)
	    gs_rethrow(code, "transform failed");

	/* correct radii if necessary */
	correction = (SQR(thalfdis.x) / SQR(size.x)) + (SQR(thalfdis.y) / SQR(size.y));
	if (correction > 1.0)
	{
	    size.x *= sqrt(correction);
	    size.y *= sqrt(correction);
	}

	scale_num = (MULTOFSQUARES(size.x, size.y)) -
	    (MULTOFSQUARES(size.x, thalfdis.y)) -
	    (MULTOFSQUARES(size.y, thalfdis.x));
	scale_denom = MULTOFSQUARES(size.x, thalfdis.y) + MULTOFSQUARES(size.y, thalfdis.x);
	scale = sign * sqrt(((scale_num / scale_denom) < 0) ? 0 : (scale_num / scale_denom));

	/* translated center */
	tcenter.x = scale * ((size.x * thalfdis.y)/size.y);
	tcenter.y = scale * ((-size.y * thalfdis.x)/size.x);

	/* of the original radical line */
	midpoint.x = (end.x + start.x) / 2.0;
	midpoint.y = (end.y + start.y) / 2.0;

	center.x = tcenter.x + midpoint.x;
	center.y = tcenter.y + midpoint.y;
	{
	    gs_point coord1, coord2, coord3, coord4;
	    coord1.x = 1;
	    coord1.y = 0;
	    coord2.x = (thalfdis.x - tcenter.x) / size.x; 
	    coord2.y = (thalfdis.y - tcenter.y) / size.y;
	    coord3.x = (thalfdis.x - tcenter.x) / size.x;
	    coord3.y = (thalfdis.y - tcenter.y) / size.y;
	    coord4.x = (-thalfdis.x - tcenter.x) / size.x;
	    coord4.y = (-thalfdis.y - tcenter.y) / size.y;
	    start_angle = angle_between(coord1, coord2);
	    delta_angle = angle_between(coord3, coord4);
	    if (delta_angle < 0 && !is_clockwise)
		delta_angle += (degrees_to_radians * 360);
	    if (delta_angle > 0 && is_clockwise)
		delta_angle -= (degrees_to_radians * 360);
	}

	/* save the ctm */
	gs_currentmatrix(ctx->pgs, &save_ctm);
	gs_translate(ctx->pgs, center.x, center.y);
	gs_rotate(ctx->pgs, rot);
	gs_scale(ctx->pgs, size.x, size.y);

	if ((code = gs_arcn(ctx->pgs, 0, 0, 1,
			radians_to_degrees * start_angle,
			radians_to_degrees * (start_angle + delta_angle))) < 0)
	    return gs_rethrow(code, "arc failed");

	/* restore the ctm */
	gs_setmatrix(ctx->pgs, &save_ctm);
    }

    return 0;
}

int
xps_parse_poly_quadratic_bezier_segment(xps_context_t *ctx, xps_item_t *root)
{
    char *points_att = xps_att(root, "Points");
    char *is_stroked_att = xps_att(root, "IsStroked");
    float x[2], y[2];
    gs_point pt;
    char *s;
    int n;

    if (!points_att)
	return gs_throw(-1, "PolyQuadraticBezierSegment element has no points");

    s = points_att;
    n = 0;
    while (*s != 0)
    {
	while (*s == ' ') s++;
	sscanf(s, "%g,%g", &x[n], &y[n]);
	while (*s != ' ' && *s != 0) s++;
	n ++;
	if (n == 2)
	{
	    gs_currentpoint(ctx->pgs, &pt);
	    gs_curveto(ctx->pgs,
		    (pt.x + 2 * x[0]) / 3, (pt.y + 2 * y[0]) / 3,
		    (x[1] + 2 * x[0]) / 3, (y[1] + 2 * y[0]) / 3,
		    x[1], y[1]);
	    n = 0;
	}
    }

    return 0;
}

int
xps_parse_poly_bezier_segment(xps_context_t *ctx, xps_item_t *root)
{
    char *points_att = xps_att(root, "Points");
    char *is_stroked_att = xps_att(root, "IsStroked");
    float x[3], y[3];
    char *s;
    int n;

    if (!points_att)
	return gs_throw(-1, "PolyBezierSegment element has no points");

    s = points_att;
    n = 0;
    while (*s != 0)
    {
	while (*s == ' ') s++;
	sscanf(s, "%g,%g", &x[n], &y[n]);
	while (*s != ' ' && *s != 0) s++;
	n ++;
	if (n == 3)
	{
	    gs_curveto(ctx->pgs, x[0], y[0], x[1], y[1], x[2], y[2]);
	    n = 0;
	}
    }

    return 0;
}

int
xps_parse_poly_line_segment(xps_context_t *ctx, xps_item_t *root)
{
    char *points_att = xps_att(root, "Points");
    char *is_stroked_att = xps_att(root, "IsStroked");
    float x, y;
    char *s;

    if (!points_att)
	return gs_throw(-1, "PolyLineSegment element has no points");

    s = points_att;
    while (*s != 0)
    {
	while (*s == ' ') s++;
	sscanf(s, "%g,%g", &x, &y);
	gs_lineto(ctx->pgs, x, y);
	while (*s != ' ' && *s != 0) s++;
    }

    return 0;
}

int
xps_parse_path_figure(xps_context_t *ctx, xps_item_t *root)
{
    xps_item_t *node;

    char *is_closed_att;
    char *start_point_att;
    char *is_filled_att;

    int is_closed = 0;
    int is_filled = 1;
    float start_x = 0.0;
    float start_y = 0.0;

    is_closed_att = xps_att(root, "IsClosed");
    start_point_att = xps_att(root, "StartPoint");
    is_filled_att = xps_att(root, "IsFilled");

    if (is_closed_att)
	is_closed = !strcmp(is_closed_att, "true");
    if (is_filled_att)
	is_filled = !strcmp(is_filled_att, "true");
    if (start_point_att)
	sscanf(start_point_att, "%g,%g", &start_x, &start_y);

    gs_moveto(ctx->pgs, start_x, start_y);

    for (node = xps_down(root); node; node = xps_next(node))
    {
	if (!strcmp(xps_tag(node), "ArcSegment"))
	    xps_parse_arc_segment(ctx, node);
	if (!strcmp(xps_tag(node), "PolyBezierSegment"))
	    xps_parse_poly_bezier_segment(ctx, node);
	if (!strcmp(xps_tag(node), "PolyLineSegment"))
	    xps_parse_poly_line_segment(ctx, node);
	if (!strcmp(xps_tag(node), "PolyQuadraticBezierSegment"))
	    xps_parse_poly_quadratic_bezier_segment(ctx, node);
    }

    if (is_closed)
	gs_closepath(ctx->pgs);

    return 0;
}

int
xps_parse_path_geometry(xps_context_t *ctx, xps_resource_t *dict, xps_item_t *root)
{
    xps_item_t *node;

    char *figures_att;
    char *fill_rule_att;
    char *transform_att;

    xps_item_t *transform_tag = NULL;
    xps_item_t *figures_tag = NULL; /* only used by resource */

    gs_matrix transform;
    int even_odd = 0;
    int saved = 0;

    figures_att = xps_att(root, "Figures");
    fill_rule_att = xps_att(root, "FillRule");
    transform_att = xps_att(root, "Transform");

    for (node = xps_down(root); node; node = xps_next(node))
    {
	if (!strcmp(xps_tag(node), "PathGeometry.Transform"))
	    transform_tag = xps_down(node);
    }

    xps_resolve_resource_reference(ctx, dict, &transform_att, &transform_tag);
    xps_resolve_resource_reference(ctx, dict, &figures_att, &figures_tag);

    if (transform_att || transform_tag)
    {
	if (!saved)
	{
	    saved = 1;
	    gs_gsave(ctx->pgs);
	}
	if (transform_att)
	    xps_parse_render_transform(ctx, transform_att, &transform);
	if (transform_tag)
	    xps_parse_matrix_transform(ctx, transform_tag, &transform);
	gs_concat(ctx->pgs, &transform);
    }

    if (figures_att)
    {
	xps_parse_abbreviated_geometry(ctx, figures_att);
    }

    if (figures_tag)
    {
	xps_parse_path_figure(ctx, figures_tag);
    }

    for (node = xps_down(root); node; node = xps_next(node))
    {
	if (!strcmp(xps_tag(node), "PathFigure"))
	    xps_parse_path_figure(ctx, node);
    }

    if (saved)
    {
	gs_grestore(ctx->pgs);
    }

    return 0;
}

/*
 * Parse an XPS <Path> element, and call relevant ghostscript
 * functions for drawing and/or clipping the child elements.
 */

int
xps_parse_path(xps_context_t *ctx, xps_resource_t *dict, xps_item_t *root)
{
    xps_item_t *node;
    xps_item_t *rsrc;

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

    gs_line_cap linecap;
    gs_line_join linejoin;
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

    xps_resolve_resource_reference(ctx, dict, &data_att, &data_tag);
    xps_resolve_resource_reference(ctx, dict, &clip_att, &clip_tag);
    xps_resolve_resource_reference(ctx, dict, &transform_att, &transform_tag);
    xps_resolve_resource_reference(ctx, dict, &fill_att, &fill_tag);
    xps_resolve_resource_reference(ctx, dict, &stroke_att, &stroke_tag);
    xps_resolve_resource_reference(ctx, dict, &opacity_mask_att, &opacity_mask_tag);

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

    /* TODO: stroke_end_line_cap_att */
    /* TODO: stroke_dash_cap_att */

    linecap = gs_cap_butt;
    if (stroke_start_line_cap_att)
    {
	if (!strcmp(stroke_start_line_cap_att, "Flat")) linecap = gs_cap_butt;
	if (!strcmp(stroke_start_line_cap_att, "Square")) linecap = gs_cap_square;
	if (!strcmp(stroke_start_line_cap_att, "Round")) linecap = gs_cap_round;
	if (!strcmp(stroke_start_line_cap_att, "Triangle")) linecap = gs_cap_triangle;
    }
    gs_setlinecap(ctx->pgs, linecap);

    linejoin = gs_join_miter;
    if (stroke_line_join_att)
    {
	if (!strcmp(stroke_line_join_att, "Miter")) linejoin = gs_join_miter;
	if (!strcmp(stroke_line_join_att, "Bevel")) linejoin = gs_join_bevel;
	if (!strcmp(stroke_line_join_att, "Round")) linejoin = gs_join_round;
    }
    gs_setlinejoin(ctx->pgs, linecap);

    if (stroke_miter_limit_att)
	gs_setmiterlimit(ctx->pgs, atof(stroke_miter_limit_att));

    if (stroke_thickness_att)
	gs_setlinewidth(ctx->pgs, atof(stroke_thickness_att));

    if (stroke_dash_array_att)
    {
	float dash_array[100];
	float dash_offset = 0.0;
	int dash_count = 0;

	if (stroke_dash_offset_att)
	    dash_offset = atof(stroke_dash_offset_att);

	/* TODO: parse array of space separated numbers */
	dash_array[0] = 10.0;
	dash_array[1] = 20.0;
	dash_array[2] = 5.0;
	dash_array[3] = 20.0;
	dash_count = 4;

	dputs(".... dashed line ....\n");

	gs_setdash(ctx->pgs, dash_array, dash_count, dash_offset);
    }
    else
    {
	gs_setdash(ctx->pgs, NULL, 0, 0.0);
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
	    xps_parse_abbreviated_geometry(ctx, clip_att);
	if (clip_tag)
	    xps_parse_path_geometry(ctx, dict, clip_tag);

	gs_clip(ctx->pgs);
	gs_newpath(ctx->pgs);
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

    /* if it's a solid color brush fill/stroke do just that. */

    if (fill_att)
    {
	float argb[4];
	dputs("  path solid color fill\n");
	xps_parse_color(ctx, fill_att, argb);
	if (argb[0] > 0.001)
	{
	    gs_setrgbcolor(ctx->pgs, argb[1], argb[2], argb[3]);
	    if (data_att)
		xps_parse_abbreviated_geometry(ctx, data_att);
	    if (data_tag)
		xps_parse_path_geometry(ctx, dict, data_tag);
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
	    if (data_att)
		xps_parse_abbreviated_geometry(ctx, data_att);
	    if (data_tag)
		xps_parse_path_geometry(ctx, dict, data_tag);
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
	if (data_att)
	    xps_parse_abbreviated_geometry(ctx, data_att);
	if (data_tag)
	    xps_parse_path_geometry(ctx, dict, data_tag);
	gs_clip(ctx->pgs);
	gs_newpath(ctx->pgs);

	dputs("clip\n");

	if (fill_tag)
	{
	    if (!strcmp(xps_tag(fill_tag), "ImageBrush"))
		xps_parse_image_brush(ctx, dict, fill_tag);
	    if (!strcmp(xps_tag(fill_tag), "VisualBrush"))
		xps_parse_visual_brush(ctx, dict, fill_tag);
	    if (!strcmp(xps_tag(fill_tag), "LinearGradientBrush"))
		xps_parse_linear_gradient_brush(ctx, dict, fill_tag);
	    if (!strcmp(xps_tag(fill_tag), "RadialGradientBrush"))
		xps_parse_radial_gradient_brush(ctx, dict, fill_tag);
	}
    }

    /* remember to restore if we did a gsave */

    if (saved)
    {
	gs_grestore(ctx->pgs);
    }

    return 0;
}

