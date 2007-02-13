#include "ghostxps.h"

char *
xps_parse_matrix_transform(xps_context_t *ctx, xps_item_t *root)
{
    if (!strcmp(xps_tag(root), "MatrixTransform"))
	return xps_att(root, "Matrix");
    return NULL;
}

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
	else if (*s >= '0' && *s <= '9' || *s == '.' || *s == '+' || *s == '-')
	{
	    *pargs++ = s;
	    while (*s >= '0' && *s <= '9' || *s == '.' || *s == '+' || *s == '-')
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
	    dprintf1("path command '%s' for quadratic or smooth curve is not implemented\n", cmd);
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

    xps_item_t *clip_node;
    xps_item_t *data_node;
    xps_item_t *fill_node;
    xps_item_t *stroke_node;

    char *opacity;
    char *opacity_mask;
    xps_item_t *opacity_mask_node;

#if 0
    char *stroke_dash_array;
    char *stroke_dash_cap;
    char *stroke_dash_offset;
    char *stroke_end_line_cap;
    char *stroke_start_line_cap;
    char *stroke_line_join;
    char *stroke_miter_limit;
    char *stroke_thickness;
#endif

    transform = xps_att(root, "RenderTransform");
    clip = xps_att(root, "Clip");
    data = xps_att(root, "Data");
    fill = xps_att(root, "Fill");
    stroke = xps_att(root, "Stroke");
    opacity = xps_att(root, "Opacity");
    opacity_mask = xps_att(root, "OpacityMask");

    for (node = xps_down(root); node; node = xps_next(node))
    {
	if (!strcmp(xps_tag(node), "Path.RenderTransform"))
	    transform = xps_parse_matrix_transform(ctx, xps_down(node));

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

    xps_parse_abbreviated_geometry(ctx, data);

    gs_setrgbcolor(ctx->pgs, 0.0, 0.0, 0.0);

    if (fill)
	gs_fill(ctx->pgs);
    else
	gs_stroke(ctx->pgs);

    return 0;
}

