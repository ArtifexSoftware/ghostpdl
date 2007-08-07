#include "ghostxps.h"

#define MAX_STOPS 100

enum { SPREAD_PAD, SPREAD_REPEAT, SPREAD_REFLECT };

/*
 * Parse a list of GradientStop elements.
 * Fill the offset and color arrays, and
 * return the number of stops parsed.
 */

static int
xps_parse_gradient_stops(xps_context_t *ctx, xps_item_t *node,
	float *offsets, float *colors, int maxcount)
{
    int count = 0;

    while (node && count < maxcount)
    {
	if (!strcmp(xps_tag(node), "GradientStop"))
	{
	    char *offset = xps_att(node, "Offset");
	    char *color = xps_att(node, "Color");
	    if (offset && color)
	    {
		offsets[count] = atof(offset);
		xps_parse_color(ctx, color, colors + count * 4);
		count ++;
	    }
	}
	
	node = xps_next(node);
    }

    return count;
}

static int
xps_gradient_has_transparent_colors(float *offsets, float *colors, int count)
{
    int i;
    for (i = 0; i < count; i++)
	if (colors[i * 4 + 0] < 0.999)
	    return 1;
    return 0;
}

/*
 * Create a Function object to map [0..1] to RGB colors
 * based on the gradient stop arrays.
 *
 * We do this by creating a stitching function that joins
 * a series of linear functions (one linear function
 * for each gradient stop-pair).
 */

static gs_function_t *
xps_create_gradient_stop_function(xps_context_t *ctx,
	float *offsets, float *colors, int count, int opacity_only)
{
    gs_function_1ItSg_params_t sparams;
    gs_function_ElIn_params_t lparams;
    gs_function_t *sfunc;
    gs_function_t *lfunc;

    float *domain, *range, *c0, *c1, *bounds, *encode;
    const gs_function_t **functions;

    int code;
    int k;
    int i;

    k = count - 1; /* number of intervals / functions */

    domain = xps_alloc(ctx, 2 * sizeof(float));
    domain[0] = 0.0;
    domain[1] = 1.0;
    sparams.m = 1;
    sparams.Domain = domain;

    range = xps_alloc(ctx, 6 * sizeof(float));
    range[0] = 0.0;
    range[1] = 1.0;
    range[2] = 0.0;
    range[3] = 1.0;
    range[4] = 0.0;
    range[5] = 1.0;
    sparams.n = 3;
    sparams.Range = range;

    functions = xps_alloc(ctx, k * sizeof(void*));
    bounds = xps_alloc(ctx, (k - 1) * sizeof(float));
    encode = xps_alloc(ctx, (k * 2) * sizeof(float));

    sparams.k = k;
    sparams.Functions = functions;
    sparams.Bounds = bounds;
    sparams.Encode = encode;

    for (i = 0; i < k; i++)
    {
	domain = xps_alloc(ctx, 2 * sizeof(float));
	domain[0] = 0.0;
	domain[1] = 1.0;
	lparams.m = 1;
	lparams.Domain = domain;

	range = xps_alloc(ctx, 6 * sizeof(float));
	range[0] = 0.0;
	range[1] = 1.0;
	range[2] = 0.0;
	range[3] = 1.0;
	range[4] = 0.0;
	range[5] = 1.0;
	lparams.n = 3;
	lparams.Range = range;

	c0 = xps_alloc(ctx, 3 * sizeof(float));
	lparams.C0 = c0;

	c1 = xps_alloc(ctx, 3 * sizeof(float));
	lparams.C1 = c1;

	if (opacity_only)
	{
	    c0[0] = colors[(i + 0) * 4 + 0];
	    c0[1] = colors[(i + 0) * 4 + 0];
	    c0[2] = colors[(i + 0) * 4 + 0];

	    c1[0] = colors[(i + 1) * 4 + 0];
	    c1[1] = colors[(i + 1) * 4 + 0];
	    c1[2] = colors[(i + 1) * 4 + 0];
	}
	else
	{
	    c0[0] = colors[(i + 0) * 4 + 1];
	    c0[1] = colors[(i + 0) * 4 + 2];
	    c0[2] = colors[(i + 0) * 4 + 3];

	    c1[0] = colors[(i + 1) * 4 + 1];
	    c1[1] = colors[(i + 1) * 4 + 2];
	    c1[2] = colors[(i + 1) * 4 + 3];
	}

	lparams.N = 1;

	code = gs_function_ElIn_init(&lfunc, &lparams, ctx->memory);
	if (code < 0)
	{
	    gs_throw(-1, "gs_function_ElIn_init failed");
	    return NULL;
	}

	functions[i] = lfunc;

	if (i > 0)
	    bounds[i - 1] = offsets[(i + 0) + 0];

	encode[i * 2 + 0] = 0.0;
	encode[i * 2 + 1] = 1.0;
    }

    code = gs_function_1ItSg_init(&sfunc, &sparams, ctx->memory);
    if (code < 0)
    {
	gs_throw(-1, "gs_function_1ItSg_init failed");
	return NULL;
    }

    return sfunc;
}

/*
 * Shadings and functions are ghostscript type objects,
 * and as such rely on the garbage collector for cleanup.
 * We can't have none of that here, so we have to
 * write our own destructors.
 */

static void
xps_free_gradient_stop_function(xps_context_t *ctx, gs_function_t *func)
{
    gs_function_t *lfunc;
    gs_function_1ItSg_params_t *sparams;
    gs_function_ElIn_params_t *lparams;
    int i;

    sparams = (gs_function_1ItSg_params_t*) &func->params;
    xps_free(ctx, (void*)sparams->Domain);
    xps_free(ctx, (void*)sparams->Range);

    for (i = 0; i < sparams->k; i++)
    {
	lfunc = sparams->Functions[i];
	lparams = (gs_function_ElIn_params_t*) &lfunc->params;
	xps_free(ctx, (void*)lparams->Domain);
	xps_free(ctx, (void*)lparams->Range);
	xps_free(ctx, (void*)lparams->C0);
	xps_free(ctx, (void*)lparams->C1);
	xps_free(ctx, lfunc);
    }

    xps_free(ctx, (void*)sparams->Bounds);
    xps_free(ctx, (void*)sparams->Encode);
    xps_free(ctx, (void*)sparams->Functions);
    xps_free(ctx, func);
}

/*
 * Radial gradients map more or less to Radial shadings.
 * The inner circle is always a point.
 * The outer circle is actually an ellipse,
 * mess with the transform to squash the circle into the right aspect.
 */

static int
xps_draw_one_radial_gradient(xps_context_t *ctx,
	gs_function_t *func, int extend,
	float *pt0, float rad0,
	float *pt1, float rad1)
{
    gs_memory_t *mem = ctx->memory;
    gs_shading_t *shading;
    gs_shading_R_params_t params;
    gs_color_space *colorspace;
    int code;

    gs_shading_R_params_init(&params);
    {
	colorspace = gs_cspace_new_DeviceRGB(mem);
	if (!colorspace)
	    return gs_throw(-1, "cannot create colorspace for gradient");

	params.ColorSpace = colorspace;

	params.Coords[0] = pt0[0];
	params.Coords[1] = pt0[1];
	params.Coords[2] = rad0;
	params.Coords[3] = pt1[0];
	params.Coords[4] = pt1[1];
	params.Coords[5] = rad1;

	params.Extend[0] = extend;
	params.Extend[1] = extend;

	params.Function = func;
    }

    code = gs_shading_R_init(&shading, &params, mem);
    if (code < 0)
	return gs_throw(-1, "gs_shading_R_init failed");

    code = gs_shfill(ctx->pgs, shading);
    if (code < 0)
    {
	gs_free_object(mem, shading, "gs_shading_R");
	return gs_throw(-1, "gs_shfill failed");
    }

    gs_free_object(mem, shading, "gs_shading_R");

    return 0;
}

/*
 * Linear gradients map to Axial shadings.
 */

static int
xps_draw_one_linear_gradient(xps_context_t *ctx,
	gs_function_t *func, int extend,
	float *pt0, float *pt1)
{
    gs_memory_t *mem = ctx->memory;
    gs_shading_t *shading;
    gs_shading_A_params_t params;
    gs_color_space *colorspace;
    int code;

    gs_shading_A_params_init(&params);
    {
	colorspace = gs_cspace_new_DeviceRGB(mem);
	if (!colorspace)
	    return gs_throw(-1, "cannot create colorspace for gradient");

	params.ColorSpace = colorspace;

	params.Coords[0] = pt0[0];
	params.Coords[1] = pt0[1];
	params.Coords[2] = pt1[0];
	params.Coords[3] = pt1[1];

	params.Extend[0] = extend;
	params.Extend[1] = extend;

	params.Function = func;
    }

    code = gs_shading_A_init(&shading, &params, mem);
    if (code < 0)
	return gs_throw(-1, "gs_shading_A_init failed");

    code = gs_shfill(ctx->pgs, shading);
    if (code < 0)
    {
	gs_free_object(mem, shading, "gs_shading_A");
	return gs_throw(-1, "gs_shfill failed");
    }

    gs_free_object(mem, shading, "gs_shading_A");

    return 0;
}

/*
 * We need to loop and create many shading objects to account
 * for the Repeat and Reflect SpreadMethods.
 *
 * TODO: calculate how many iterations are required to fill
 * the current clipping region.
 */

static int
xps_draw_radial_gradient(xps_context_t *ctx, xps_item_t *root, int spread, gs_function_t *func)
{
    float pt0[2] = { 0, 0 };
    float pt1[2] = { 0, 0 };
    float xrad = 1;
    float yrad = 1;
    float invscale;
    float rad0, rad1;
    float dx, dy;
    int code;
    int i;

    {
	char *center_att = xps_att(root, "Center");
	char *origin_att = xps_att(root, "GradientOrigin");
	char *radius_x_att = xps_att(root, "RadiusX");
	char *radius_y_att = xps_att(root, "RadiusY");

	if (origin_att)
	    sscanf(origin_att, "%g,%g", pt0, pt0 + 1);
	if (center_att)
	    sscanf(center_att, "%g,%g", pt1, pt1 + 1);
	if (radius_x_att)
	    xrad = atof(radius_x_att);
	if (radius_y_att)
	    yrad = atof(radius_y_att);
    }

    /* scale the ctm to make ellipses */
    gs_scale(ctx->pgs, 1.0, yrad / xrad);

    invscale = xrad / yrad;
    pt0[1] *= invscale;
    pt1[1] *= invscale;

    rad0 = 0.0;
    rad1 = xrad;

    dx = pt1[0] - pt0[0];
    dy = pt1[1] - pt0[1];

    if (spread == SPREAD_PAD)
    {
	code = xps_draw_one_radial_gradient(ctx, func, 1, pt0, rad0, pt1, rad1);
	if (code < 0)
	    return gs_rethrow(code, "could not draw axial gradient");
    }
    else
    {
	for (i = 0; i < 10; i++)
	{
	    if (spread == SPREAD_REFLECT && (i & 1))
		code = xps_draw_one_radial_gradient(ctx, func, 0, pt1, rad1, pt0, rad0);
	    else
		code = xps_draw_one_radial_gradient(ctx, func, 0, pt0, rad0, pt1, rad1);

	    if (code < 0)
		return gs_rethrow(code, "could not draw axial gradient");

	    rad0 = rad1;
	    rad1 += xrad;

	    pt0[0] += dx; pt0[1] += dy;
	    pt1[0] += dx; pt1[1] += dy;
	}
    }

    return 0;
}

static int
xps_draw_linear_gradient(xps_context_t *ctx, xps_item_t *root, int spread, gs_function_t *func)
{
    float pt0[2] = { 0, 0 };
    float pt1[2] = { 0, 1 };
    float dx, dy;
    int code;
    int i;

    {
	char *start_point_att = xps_att(root, "StartPoint");
	char *end_point_att = xps_att(root, "EndPoint");

	if (start_point_att)
	    sscanf(start_point_att, "%g,%g", pt0, pt0 + 1);
	if (end_point_att)
	    sscanf(end_point_att, "%g,%g", pt1, pt1 + 1);
    }

    dx = pt1[0] - pt0[0];
    dy = pt1[1] - pt0[1];

    if (spread == SPREAD_PAD)
    {
	code = xps_draw_one_linear_gradient(ctx, func, 1, pt0, pt1);
	if (code < 0)
	    return gs_rethrow(code, "could not draw axial gradient");
    }
    else
    {
	pt0[0] -= 10 * dx; pt0[1] -= 10 * dy;
	pt1[0] -= 10 * dx; pt1[1] -= 10 * dy;

	for (i = 0; i < 20; i++)
	{
	    if (spread == SPREAD_REFLECT && (i & 1))
		code = xps_draw_one_linear_gradient(ctx, func, 0, pt1, pt0);
	    else
		code = xps_draw_one_linear_gradient(ctx, func, 0, pt0, pt1);
	    if (code < 0)
		return gs_rethrow(code, "could not draw axial gradient");

	    pt0[0] += dx; pt0[1] += dy;
	    pt1[0] += dx; pt1[1] += dy;
	}
    }

    return 0;
}

/*
 * Parse XML tag and attributes for a gradient brush, create color/opacity
 * funciton objects and call gradient drawing primitives.
 */

static int
xps_parse_gradient_brush(xps_context_t *ctx, xps_resource_t *dict, xps_item_t *root,
	int (*draw)(xps_context_t *, xps_item_t *, int, gs_function_t *))
{
    xps_item_t *node;

    char *opacity_att;
    char *interpolation_att;
    char *spread_att;
    char *mapping_att;
    char *transform_att;

    xps_item_t *transform_tag = NULL;
    xps_item_t *stop_tag = NULL;

    float stop_offsets[MAX_STOPS];
    float stop_colors[MAX_STOPS * 4];
    int stop_count;
    gs_matrix transform;
    int spread_method;

    gs_rect saved_bounds;

    gs_function_t *color_func;
    gs_function_t *opacity_func;
    int has_opacity = 0;

    opacity_att = xps_att(root, "Opacity");
    interpolation_att = xps_att(root, "ColorInterpolationMode");
    spread_att = xps_att(root, "SpreadMethod");
    mapping_att = xps_att(root, "MappingMode");
    transform_att = xps_att(root, "Transform");

    for (node = xps_down(root); node; node = xps_next(node))
    {
	if (!strcmp(xps_tag(node), "LinearGradientBrush.Transform"))
	    transform_tag = xps_down(node);
	if (!strcmp(xps_tag(node), "RadialGradientBrush.Transform"))
	    transform_tag = xps_down(node);
	if (!strcmp(xps_tag(node), "LinearGradientBrush.GradientStops"))
	    stop_tag = xps_down(node);
	if (!strcmp(xps_tag(node), "RadialGradientBrush.GradientStops"))
	    stop_tag = xps_down(node);
    }

    xps_resolve_resource_reference(ctx, dict, &transform_att, &transform_tag);

    spread_method = SPREAD_PAD;
    if (spread_att)
    {
	if (!strcmp(spread_att, "Pad"))
	    spread_method = SPREAD_PAD;
	if (!strcmp(spread_att, "Reflect"))
	    spread_method = SPREAD_REFLECT;
	if (!strcmp(spread_att, "Repeat"))
	    spread_method = SPREAD_REPEAT;
    }

    gs_make_identity(&transform);
    if (transform_att)
	xps_parse_render_transform(ctx, transform_att, &transform);
    if (transform_tag)
	xps_parse_matrix_transform(ctx, transform_tag, &transform);

    if (!stop_tag)
	return gs_throw(-1, "missing gradient stops tag");

    stop_count = xps_parse_gradient_stops(ctx, stop_tag, stop_offsets, stop_colors, MAX_STOPS);
    if (stop_count == 0)
	return gs_throw(-1, "no gradient stops found");

    color_func = xps_create_gradient_stop_function(ctx, stop_offsets, stop_colors, stop_count, 0);
    if (!color_func)
	return gs_rethrow(-1, "could not create color gradient function");

    opacity_func = xps_create_gradient_stop_function(ctx, stop_offsets, stop_colors, stop_count, 1);
    if (!opacity_func)
	return gs_rethrow(-1, "could not create opacity gradient function");

    has_opacity = xps_gradient_has_transparent_colors(stop_offsets, stop_colors, stop_count);

    xps_clip(ctx, &saved_bounds);

    gs_gsave(ctx->pgs);

    gs_concat(ctx->pgs, &transform);

    xps_begin_opacity(ctx, dict, opacity_att, NULL);

    if (ctx->opacity_only)
    {
	draw(ctx, root, spread_method, opacity_func);
    }
    else
    {
	if (has_opacity)
	{
	    gs_transparency_mask_params_t params;
	    gs_transparency_group_params_t tgp;
	    gs_rect bbox;

	    xps_bounds_in_user_space(ctx, &bbox);

	    gs_trans_mask_params_init(&params, TRANSPARENCY_MASK_Alpha);
	    gs_begin_transparency_mask(ctx->pgs, &params, &bbox, 0);
	    draw(ctx, root, spread_method, opacity_func);
	    gs_end_transparency_mask(ctx->pgs, TRANSPARENCY_CHANNEL_Opacity);

	    gs_trans_group_params_init(&tgp);
	    gs_begin_transparency_group(ctx->pgs, &tgp, &bbox);
	    draw(ctx, root, spread_method, color_func);
	    gs_end_transparency_group(ctx->pgs);
	}
	else
	{
	    draw(ctx, root, spread_method, color_func);
	}
    }

    xps_end_opacity(ctx, dict, opacity_att, NULL);

    gs_grestore(ctx->pgs);

    xps_unclip(ctx, &saved_bounds);

    xps_free_gradient_stop_function(ctx, opacity_func);
    xps_free_gradient_stop_function(ctx, color_func);

    return 0;
}

int
xps_parse_linear_gradient_brush(xps_context_t *ctx, xps_resource_t *dict, xps_item_t *root)
{
    return xps_parse_gradient_brush(ctx, dict, root, xps_draw_linear_gradient);
}

int
xps_parse_radial_gradient_brush(xps_context_t *ctx, xps_resource_t *dict, xps_item_t *root)
{
    return xps_parse_gradient_brush(ctx, dict, root, xps_draw_radial_gradient);
}

