#include "ghostxps.h"

static void
xps_grow_rect(gs_rect *rect, float x, float y)
{
    if (x < rect->p.x) rect->p.x = x;
    if (y < rect->p.y) rect->p.y = y;
    if (x > rect->q.x) rect->q.x = x;
    if (y > rect->q.y) rect->q.y = y;
}

void
xps_bounds_in_user_space(xps_context_t *ctx, gs_rect *user)
{
    gs_matrix ctm;
    gs_matrix inv;
    gs_point a, b, c, d;

    gs_currentmatrix(ctx->pgs, &ctm);
    gs_matrix_invert(&ctm, &inv);

    gs_point_transform(ctx->bounds.p.x, ctx->bounds.p.y, &inv, &a);
    gs_point_transform(ctx->bounds.p.x, ctx->bounds.q.y, &inv, &b);
    gs_point_transform(ctx->bounds.q.x, ctx->bounds.q.y, &inv, &c);
    gs_point_transform(ctx->bounds.q.x, ctx->bounds.p.y, &inv, &d);

    user->p.x = MIN(MIN(a.x, b.x), MIN(c.x, d.x));
    user->p.y = MIN(MIN(a.y, b.y), MIN(c.y, d.y));
    user->q.x = MAX(MAX(a.x, b.x), MAX(c.x, d.x));
    user->q.y = MAX(MAX(a.y, b.y), MAX(c.y, d.y));

#if 0
    user->p.x = 0.0;
    user->p.y = 0.0;
    user->q.x = 1000.0;
    user->q.y = 1000.0;
#endif

}

static void
xps_update_bounds(xps_context_t *ctx, gs_rect *save)
{
    segment *seg;
    curve_segment *cseg;
    gs_rect rc;

    save->p.x = ctx->bounds.p.x;
    save->p.y = ctx->bounds.p.y;
    save->q.x = ctx->bounds.q.x;
    save->q.y = ctx->bounds.q.y;

    /* get bounds of current path (that is about to be clipped) */
    /* the coordinates of the path segments are already in device space (yay!) */

    seg = (segment*)ctx->pgs->path->first_subpath;
    rc.p.x = rc.q.x = fixed2float(seg->pt.x);
    rc.p.y = rc.q.y = fixed2float(seg->pt.y);

    while (seg)
    {
	switch (seg->type)
	{
	case s_start:
	    xps_grow_rect(&rc, fixed2float(seg->pt.x), fixed2float(seg->pt.y));
	    break;
	case s_line:
	    xps_grow_rect(&rc, fixed2float(seg->pt.x), fixed2float(seg->pt.y));
	    break;
	case s_line_close:
	    break;
	case s_curve:
	    cseg = (curve_segment*)seg;
	    xps_grow_rect(&rc, fixed2float(cseg->p1.x), fixed2float(cseg->p1.y));
	    xps_grow_rect(&rc, fixed2float(cseg->p2.x), fixed2float(cseg->p2.y));
	    xps_grow_rect(&rc, fixed2float(seg->pt.x), fixed2float(seg->pt.y));
	    break;
	}
	seg = seg->next;
    }

    /* intersect with old bounds, and fix degenerate case */

    rect_intersect(ctx->bounds, rc);

    if (ctx->bounds.q.x < ctx->bounds.p.x)
	ctx->bounds.q.x = ctx->bounds.p.x;
    if (ctx->bounds.q.y < ctx->bounds.p.y)
	ctx->bounds.q.y = ctx->bounds.p.y;
}

static void
xps_restore_bounds(xps_context_t *ctx, gs_rect *save)
{
    ctx->bounds.p.x = save->p.x;
    ctx->bounds.p.y = save->p.y;
    ctx->bounds.q.x = save->q.x;
    ctx->bounds.q.y = save->q.y;
}

void
xps_debug_bounds(xps_context_t *ctx)
{
    gs_matrix mat;

    gs_gsave(ctx->pgs);

    dprintf6("bounds: debug [%g %g %g %g] w=%g h=%g\n",
	    ctx->bounds.p.x, ctx->bounds.p.y,
	    ctx->bounds.q.x, ctx->bounds.q.y,
	    ctx->bounds.q.x - ctx->bounds.p.x,
	    ctx->bounds.q.y - ctx->bounds.p.y);

    gs_make_identity(&mat);
    gs_setmatrix(ctx->pgs, &mat);

    gs_setgray(ctx->pgs, 0.3);
    gs_moveto(ctx->pgs, ctx->bounds.p.x, ctx->bounds.p.y);
    gs_lineto(ctx->pgs, ctx->bounds.q.x, ctx->bounds.q.y);
    gs_moveto(ctx->pgs, ctx->bounds.q.x, ctx->bounds.p.y);
    gs_lineto(ctx->pgs, ctx->bounds.p.x, ctx->bounds.q.y);

    gs_moveto(ctx->pgs, ctx->bounds.p.x, ctx->bounds.p.y);
    gs_lineto(ctx->pgs, ctx->bounds.p.x, ctx->bounds.q.y);
    gs_lineto(ctx->pgs, ctx->bounds.q.x, ctx->bounds.q.y);
    gs_lineto(ctx->pgs, ctx->bounds.q.x, ctx->bounds.p.y);
    gs_closepath(ctx->pgs);

    gs_stroke(ctx->pgs);

    gs_grestore(ctx->pgs);
}

int
xps_unclip(xps_context_t *ctx, gs_rect *saved_bounds)
{
    xps_restore_bounds(ctx, saved_bounds);
    return 0;
}

int
xps_clip(xps_context_t *ctx, gs_rect *saved_bounds)
{
    xps_update_bounds(ctx, saved_bounds);

    if (ctx->fill_rule == 0)
	gs_eoclip(ctx->pgs);
    else
	gs_clip(ctx->pgs);

    gs_newpath(ctx->pgs);

    return 0;
}

int
xps_fill(xps_context_t *ctx)
{
    if (gs_currentopacityalpha(ctx->pgs) < 0.001)
	gs_newpath(ctx->pgs);
    else if (ctx->fill_rule == 0)
	gs_eofill(ctx->pgs);
    else
	gs_fill(ctx->pgs);
    return 0;
}


/* Draw an arc segment transformed by the matrix, we approximate with straight
 * line segments. We cannot use the gs_arc function because they only draw
 * circular arcs, we need to transform the line to make them elliptical but
 * without transforming the line width.
 */
static inline void
xps_draw_arc_segment(xps_context_t *ctx, gs_matrix *mtx, float th0, float th1, int iscw)
{
    float x, y, t, a, d;
    gs_point p;

    th0 = th0 * (M_PI / 180.0);
    th1 = th1 * (M_PI / 180.0);

    while (th1 < th0)
	th1 += M_PI * 2.0;

    d = 1 * (M_PI / 180.0); /* 1-degree precision */

    if (iscw)
    {
	gs_point_transform(cos(th0), sin(th0), mtx, &p);
	gs_lineto(ctx->pgs, p.x, p.y);
	for (t = th0; t < th1; t += d)
	{
	    gs_point_transform(cos(t), sin(t), mtx, &p);
	    gs_lineto(ctx->pgs, p.x, p.y);
	}
	gs_point_transform(cos(th1), sin(th1), mtx, &p);
	gs_lineto(ctx->pgs, p.x, p.y);
    }
    else
    {
	th0 += M_PI * 2;
	gs_point_transform(cos(th0), sin(th0), mtx, &p);
	gs_lineto(ctx->pgs, p.x, p.y);
	for (t = th0; t > th1; t -= d)
	{
	    gs_point_transform(cos(t), sin(t), mtx, &p);
	    gs_lineto(ctx->pgs, p.x, p.y);
	}
	gs_point_transform(cos(th1), sin(th1), mtx, &p);
	gs_lineto(ctx->pgs, p.x, p.y);
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
    return sign * acos(udotv / (magu * magv));
}

int
xps_draw_arc(xps_context_t *ctx,
	float size_x, float size_y, float rotation_angle,
	int is_large_arc, int is_clockwise,
	float point_x, float point_y)
{
    gs_point start, end, size, midpoint, halfdis, thalfdis, tcenter, center;
    gs_matrix rotmat, mtx;
    double correction;
    double sign, start_angle, delta_angle;
    double scale_num, scale_denom, scale;
    int code;

    /* arcs are just too broken for now */
    // gs_lineto(ctx->pgs, point_x, point_y);
    // return 0;

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
    gs_make_rotation(-rotation_angle, &rotmat);
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

#if 0
    /* save the ctm -- umm, this affects line widths */
    gs_currentmatrix(ctx->pgs, &save_ctm);
    gs_translate(ctx->pgs, center.x, center.y);
    gs_scale(ctx->pgs, size.x, size.y);
    gs_rotate(ctx->pgs, rotation_angle);

    {
	floatp th0 = radians_to_degrees * start_angle;
	floatp th1 = radians_to_degrees * (start_angle + delta_angle);
	if (is_clockwise)
	    gs_arc(ctx->pgs, 0, 0, 1, th0, th1);
	else
	    gs_arcn(ctx->pgs, 0, 0, 1, th0, th1);
    }

    /* restore the ctm */
    gs_setmatrix(ctx->pgs, &save_ctm);
#endif

    {
	floatp th0 = radians_to_degrees * start_angle;
	floatp th1 = radians_to_degrees * (start_angle + delta_angle);

	gs_make_identity(&mtx);
	gs_matrix_translate(&mtx, center.x, center.y, &mtx);
	gs_matrix_rotate(&mtx, rotation_angle, &mtx);
	gs_matrix_scale(&mtx, size.x, size.y, &mtx);

	xps_draw_arc_segment(ctx, &mtx, th0, th1, is_clockwise);
    }

    return 0;
}


/*
 * Parse an abbreviated geometry string, and call
 * ghostscript moveto/lineto/curveto functions to
 * build up a path.
 */

int
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
    pargs = args;

    //dprintf1("new path (%.70s)\n", geom);
    gs_newpath(ctx->pgs);

    ctx->fill_rule = 0;

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

    pargs[0] = s;
    pargs[1] = 0;

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
	    ctx->fill_rule = atoi(args[i]);
	    i ++;
	    break;

	case 'M':
	    gs_moveto(ctx->pgs, atof(args[i]), atof(args[i+1]));
	    //dprintf2("moveto %g %g\n", atof(args[i]), atof(args[i+1]));
	    i += 2;
	    break;
	case 'm':
	    gs_rmoveto(ctx->pgs, atof(args[i]), atof(args[i+1]));
	    //dprintf2("rmoveto %g %g\n", atof(args[i]), atof(args[i+1]));
	    i += 2;
	    break;

	case 'L':
	    gs_lineto(ctx->pgs, atof(args[i]), atof(args[i+1]));
	    //dprintf2("lineto %g %g\n", atof(args[i]), atof(args[i+1]));
	    i += 2;
	    break;
	case 'l':
	    gs_rlineto(ctx->pgs, atof(args[i]), atof(args[i+1]));
	    //dprintf2("rlineto %g %g\n", atof(args[i]), atof(args[i+1]));
	    i += 2;
	    break;

	case 'H':
	    gs_currentpoint(ctx->pgs, &pt);
	    gs_lineto(ctx->pgs, atof(args[i]), pt.y);
	    //dprintf1("hlineto %g\n", atof(args[i]));
	    i += 1;
	    break;
	case 'h':
	    gs_rlineto(ctx->pgs, atof(args[i]), 0.0);
	    //dprintf1("rhlineto %g\n", atof(args[i]));
	    i += 1;
	    break;

	case 'V':
	    gs_currentpoint(ctx->pgs, &pt);
	    gs_lineto(ctx->pgs, pt.x, atof(args[i]));
	    //dprintf1("vlineto %g\n", atof(args[i]));
	    i += 1;
	    break;
	case 'v':
	    gs_rlineto(ctx->pgs, 0.0, atof(args[i]));
	    //dprintf1("rvlineto %g\n", atof(args[i]));
	    i += 1;
	    break;

	case 'C':
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
	    break;

	case 'c':
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
	    break;

	case 'S':
	    gs_currentpoint(ctx->pgs, &pt);
	    x1 = atof(args[i+0]);
	    y1 = atof(args[i+1]);
	    x2 = atof(args[i+2]);
	    y2 = atof(args[i+3]);
	    //dprintf2("smooth %g %g\n", smooth_x, smooth_y);
	    gs_curveto(ctx->pgs, pt.x + smooth_x, pt.y + smooth_y, x1, y1, x2, y2);
	    i += 4;
	    reset_smooth = 0;
	    smooth_x = x2 - x1;
	    smooth_y = y2 - y1;
	    break;

	case 's':
	    gs_currentpoint(ctx->pgs, &pt);
	    x1 = atof(args[i+0]) + pt.x;
	    y1 = atof(args[i+1]) + pt.y;
	    x2 = atof(args[i+2]) + pt.x;
	    y2 = atof(args[i+3]) + pt.y;
	    //dprintf2("smooth %g %g\n", smooth_x, smooth_y);
	    gs_curveto(ctx->pgs, pt.x + smooth_x, pt.y + smooth_y, x1, y1, x2, y2);
	    i += 4;
	    reset_smooth = 0;
	    smooth_x = x2 - x1;
	    smooth_y = y2 - y1;
	    break;

	case 'Q':
	    x1 = atof(args[i+0]);
	    y1 = atof(args[i+1]);
	    x2 = atof(args[i+2]);
	    y2 = atof(args[i+3]);
	    //dprintf4("conicto %g %g %g %g\n", x1, y1, x2, y2);
	    gs_curveto(ctx->pgs,
		    (pt.x + 2 * x1) / 3, (pt.y + 2 * y1) / 3,
		    (x2 + 2 * x1) / 3, (y2 + 2 * y1) / 3,
		    x2, y2);
	    i += 4;
	    break;
	case 'q':
	    gs_currentpoint(ctx->pgs, &pt);
	    x1 = atof(args[i+0]) + pt.x;
	    y1 = atof(args[i+1]) + pt.y;
	    x2 = atof(args[i+2]) + pt.x;
	    y2 = atof(args[i+3]) + pt.y;
	    //dprintf4("conicto %g %g %g %g\n", x1, y1, x2, y2);
	    gs_curveto(ctx->pgs,
		    (pt.x + 2 * x1) / 3, (pt.y + 2 * y1) / 3,
		    (x2 + 2 * x1) / 3, (y2 + 2 * y1) / 3,
		    x2, y2);
	    i += 4;
	    break;

	case 'A':
	    xps_draw_arc(ctx,
		    atof(args[i+0]), atof(args[i+1]), atof(args[i+2]),
		    atoi(args[i+3]), atoi(args[i+4]),
		    atof(args[i+5]), atof(args[i+6]));
	    i += 7;
	    break;
	case 'a':
	    gs_currentpoint(ctx->pgs, &pt);
	    xps_draw_arc(ctx,
		    atof(args[i+0]), atof(args[i+1]), atof(args[i+2]),
		    atoi(args[i+3]), atoi(args[i+4]),
		    atof(args[i+5]) + pt.x, atof(args[i+6]) + pt.y);
	    i += 7;
	    break;

	case 'Z':
	case 'z':
	    gs_closepath(ctx->pgs);
	    //dputs("closepath\n");
	    break;

	default:
	    /* eek */
	    break;
	}

	old = cmd;
    }

    xps_free(ctx, args);

    return 0;
}

int
xps_parse_arc_segment(xps_context_t *ctx, xps_item_t *root)
{
    /* ArcSegment pretty much follows the SVG algorithm for converting an
       arc in endpoint representation to an arc in centerpoint
       representation.  Once in centerpoint it can be given to the
       graphics library in the form of a postscript arc.
     */

    float rotation_angle;
    int is_large_arc, is_clockwise;
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

    return xps_draw_arc(ctx, size_x, size_y, rotation_angle,
	    is_large_arc, is_clockwise, point_x, point_y);
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

    ctx->fill_rule = 0;

    gs_newpath(ctx->pgs);

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

    if (fill_rule_att)
    {
	if (!strcmp(fill_rule_att, "NonZero"))
	    ctx->fill_rule = 1;
	if (!strcmp(fill_rule_att, "EvenOdd"))
	    ctx->fill_rule = 0;
    }

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

    gs_line_cap linecap;
    gs_line_join linejoin;
    float linewidth;
    float miterlimit;
    float samples[32];
    gs_color_space *colorspace;

    gs_rect saved_bounds;

    gs_gsave(ctx->pgs);

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
	float dash_array[100];
	float dash_offset = 0.0;
	int dash_count = 0;

	if (stroke_dash_offset_att)
	    dash_offset = atof(stroke_dash_offset_att);

	while (*s)
	{
	    while (*s == ' ')
		s++;
	    dash_array[dash_count++] = atof(s) * linewidth;
	    while (*s && *s != ' ')
		s++;
	}

	gs_setdash(ctx->pgs, dash_array, dash_count, dash_offset);
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
	    xps_parse_path_geometry(ctx, dict, clip_tag);

	xps_clip(ctx, &saved_bounds);
    }

    xps_begin_opacity(ctx, dict, opacity_att, opacity_mask_tag);

    if (fill_att)
    {
	xps_parse_color(ctx, fill_att, &colorspace, samples);
	if (fill_opacity_att)
	    samples[0] = atof(fill_opacity_att);
	xps_set_color(ctx, colorspace, samples);

	if (data_att)
	    xps_parse_abbreviated_geometry(ctx, data_att);
	if (data_tag)
	    xps_parse_path_geometry(ctx, dict, data_tag);

	xps_fill(ctx);
    }

    if (fill_tag)
    {
	if (data_att)
	    xps_parse_abbreviated_geometry(ctx, data_att);
	if (data_tag)
	    xps_parse_path_geometry(ctx, dict, data_tag);

	xps_parse_brush(ctx, dict, fill_tag);
    }

    if (stroke_att)
    {
	xps_parse_color(ctx, stroke_att, &colorspace, samples);
	if (stroke_opacity_att)
	    samples[0] = atof(stroke_opacity_att);
	xps_set_color(ctx, colorspace, samples);

	if (data_att)
	    xps_parse_abbreviated_geometry(ctx, data_att);
	if (data_tag)
	    xps_parse_path_geometry(ctx, dict, data_tag);

	gs_stroke(ctx->pgs);
    }

    if (stroke_tag)
    {
	if (data_att)
	    xps_parse_abbreviated_geometry(ctx, data_att);
	if (data_tag)
	    xps_parse_path_geometry(ctx, dict, data_tag);

	ctx->fill_rule = 1; /* over-ride for stroking */
	gs_strokepath(ctx->pgs);

	xps_parse_brush(ctx, dict, stroke_tag);
    }

    if (clip_att || clip_tag)
    {
	xps_unclip(ctx, &saved_bounds);
    }

    xps_end_opacity(ctx, dict, opacity_att, opacity_mask_tag);

    gs_grestore(ctx->pgs);

    return 0;
}

