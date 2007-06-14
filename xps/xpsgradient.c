#include "ghostxps.h"

#define MAX_STOPS 100

enum { SPREAD_PAD, SPREAD_REPEAT, SPREAD_REFLECT };

/*
 * Parse a list of GradientStop elements.
 * Fill the offset and color arrays, and
 * return the number of stops parsed.
 */

int
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

/*
 * Create a Function object to map [0..1] to RGB colors
 * based on the gradient stop arrays.
 *
 * We do this by creating a stitching function that joins
 * a series of linear functions (one linear function
 * for each gradient stop-pair).
 */

gs_function_t *
xps_create_gradient_stop_function(xps_context_t *ctx,
	float *offsets, float *colors, int count)
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

	/* XXX: ignoring alpha component here */

	c0 = xps_alloc(ctx, 3 * sizeof(float));
	c0[0] = colors[(i + 0) * 4 + 1];
	c0[1] = colors[(i + 0) * 4 + 2];
	c0[2] = colors[(i + 0) * 4 + 3];
	lparams.C0 = c0;

	c1 = xps_alloc(ctx, 3 * sizeof(float));
	c1[0] = colors[(i + 1) * 4 + 1];
	c1[1] = colors[(i + 1) * 4 + 2];
	c1[2] = colors[(i + 1) * 4 + 3];
	lparams.C1 = c1;

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

void
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

int
xps_draw_one_radial_gradient(xps_context_t *ctx,
	gs_function_t *func, int extend,
	float *pt0, float rad0,
	float *pt1, float rad1)
{
    gs_memory_t *mem = ctx->memory;
    gs_state *pgs = ctx->pgs;
    gs_imager_state *pis = (gs_imager_state*)pgs;
    gx_device *dev = gs_currentdevice(pgs);
    gs_shading_t *shading;
    gs_shading_R_params_t params;
    gs_color_space *colorspace;
    gs_fixed_rect rect;
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

    rect.p.x = int2fixed(-10000);
    rect.p.y = int2fixed(-10000);
    rect.q.x = int2fixed(10000);
    rect.q.y = int2fixed(10000);

    code = gs_shfill(ctx->pgs, shading);
    if (code < 0)
    {
	gs_free_object(mem, shading, "gs_shading_R");
	return gs_throw(-1, "gs_shfill failed");
    }

    gs_free_object(mem, shading, "gs_shading_R");

    return gs_okay;
}

/*
 * Linear gradients map to Axial shadings.
 */

int
xps_draw_one_linear_gradient(xps_context_t *ctx,
	gs_function_t *func, int extend,
	float *pt0, float *pt1)
{
    gs_memory_t *mem = ctx->memory;
    gs_state *pgs = ctx->pgs;
    gs_imager_state *pis = (gs_imager_state*)pgs;
    gx_device *dev = gs_currentdevice(pgs);
    gs_shading_t *shading;
    gs_shading_A_params_t params;
    gs_color_space *colorspace;
    gs_fixed_rect rect;
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

    rect.p.x = int2fixed(-10000);
    rect.p.y = int2fixed(-10000);
    rect.q.x = int2fixed(10000);
    rect.q.y = int2fixed(10000);

    code = gs_shfill(ctx->pgs, shading);
    if (code < 0)
    {
	gs_free_object(mem, shading, "gs_shading_A");
	return gs_throw(-1, "gs_shfill failed");
    }

    gs_free_object(mem, shading, "gs_shading_A");

    return gs_okay;
}

/*
 * We need to loop and create many shading objects to account
 * for the Repeat and Reflect SpreadMethods.
 *
 * TODO: calculate how many iterations are required to fill
 * the current clipping region.
 */

int
xps_draw_radial_gradient(xps_context_t *ctx,
	float *pt0, float *pt1,
	float xrad, float yrad,
	int spread,
	float *offsets, float *colors, int count)
{
    gs_function_t *func;
    float invscale;
    float rad0, rad1;
    float dx, dy;
    int code;
    int i;

    gs_gsave(ctx->pgs);

    /* scale the ctm to make ellipses */
    gs_scale(ctx->pgs, 1.0, yrad / xrad);

    invscale = xrad / yrad;
    pt0[1] *= invscale;
    pt1[1] *= invscale;

    func = xps_create_gradient_stop_function(ctx, offsets, colors, count);
    if (!func)
	return gs_rethrow(-1, "could not create gradient function");

    rad0 = 0.0;
    rad1 = xrad;

    dx = pt1[0] - pt0[0];
    dy = pt1[1] - pt0[1];

    if (spread == SPREAD_PAD)
    {
	code = xps_draw_one_radial_gradient(ctx, func, 1, pt0, rad0, pt1, rad1);
	if (code < 0)
	{
	    xps_free_gradient_stop_function(ctx, func);
	    gs_grestore(ctx->pgs);
	    return gs_rethrow(code, "could not draw axial gradient");
	}
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
	    {
		xps_free_gradient_stop_function(ctx, func);
		gs_grestore(ctx->pgs);
		return gs_rethrow(code, "could not draw axial gradient");
	    }

	    rad0 = rad1;
	    rad1 += xrad;

	    pt0[0] += dx; pt0[1] += dy;
	    pt1[0] += dx; pt1[1] += dy;
	}
    }

    xps_free_gradient_stop_function(ctx, func);
    gs_grestore(ctx->pgs);
    return gs_okay;
}


int
xps_draw_linear_gradient(xps_context_t *ctx,
	float *pt0, float *pt1, int spread,
	float *offsets, float *colors, int count)
{
    gs_function_t *func;
    int code;
    int i;

    float dx = pt1[0] - pt0[0];
    float dy = pt1[1] - pt0[1];

    func = xps_create_gradient_stop_function(ctx, offsets, colors, count);
    if (!func)
	return gs_rethrow(-1, "could not create gradient function");

    if (spread == SPREAD_PAD)
    {
	code = xps_draw_one_linear_gradient(ctx, func, 1, pt0, pt1);
	if (code < 0)
	{
	    xps_free_gradient_stop_function(ctx, func);
	    return gs_rethrow(code, "could not draw axial gradient");
	}
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
	    {
		xps_free_gradient_stop_function(ctx, func);
		return gs_rethrow(code, "could not draw axial gradient");
	    }

	    pt0[0] += dx; pt0[1] += dy;
	    pt1[0] += dx; pt1[1] += dy;
	}
    }

    xps_free_gradient_stop_function(ctx, func);
    return gs_okay;
}


/*
 * Parse XML tag and attributes for a gradient brush, and
 * call drawing functions to paint it.
 */

int
xps_parse_radial_gradient_brush(xps_context_t *ctx, xps_resource_t *dict, xps_item_t *root)
{
    xps_item_t *node;
    int code;

    xps_item_t *transform_tag = NULL;
    xps_item_t *stop_tag = NULL;

    char *opacity_att;
    char *interpolation_att;
    char *spread_att;
    char *mapping_att;
    char *transform_att;
    char *center_att;
    char *origin_att;
    char *radius_x_att;
    char *radius_y_att;

    float stop_offsets[MAX_STOPS];
    float stop_colors[MAX_STOPS * 4];
    int stop_count = 0;
    gs_matrix transform;
    int spread_method;
    float center[2];
    float origin[2];
    float radius_x;
    float radius_y;

    gs_clip(ctx->pgs);
    gs_newpath(ctx->pgs);

    dputs("drawing radial gradient brush\n");

    opacity_att = xps_att(root, "Opacity");
    interpolation_att = xps_att(root, "ColorInterpolationMode");
    spread_att = xps_att(root, "SpreadMethod");
    mapping_att = xps_att(root, "MappingMode");
    transform_att = xps_att(root, "Transform");
    center_att = xps_att(root, "Center");
    origin_att = xps_att(root, "GradientOrigin");
    radius_x_att = xps_att(root, "RadiusX");
    radius_y_att = xps_att(root, "RadiusY");

    for (node = xps_down(root); node; node = xps_next(node))
    {
	if (!strcmp(xps_tag(node), "RadialGradientBrush.Transform"))
	    transform_tag = xps_down(node);
	if (!strcmp(xps_tag(node), "RadialGradientBrush.GradientStops"))
	    stop_tag = xps_down(node);
    }

    xps_resolve_resource_reference(ctx, dict, &transform_att, &transform_tag);

    if (!stop_tag || !center_att || !origin_att || !radius_x_att || !radius_y_att)
	return gs_throw(-1, "missing attribute in radial gradient tag");

    stop_count = xps_parse_gradient_stops(ctx, stop_tag, stop_offsets, stop_colors, MAX_STOPS);
    if (stop_count == 0)
	return gs_throw(-1, "no gradient stops found");

    gs_gsave(ctx->pgs);

    gs_make_identity(&transform);
    if (transform_att)
	xps_parse_render_transform(ctx, transform_att, &transform);
    if (transform_tag)
	xps_parse_matrix_transform(ctx, transform_tag, &transform);
    gs_concat(ctx->pgs, &transform);

    sscanf(center_att, "%g,%g", center, center + 1);
    sscanf(origin_att, "%g,%g", origin, origin + 1);
    radius_x = atof(radius_x_att);
    radius_y = atof(radius_y_att);

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

    code = xps_draw_radial_gradient(ctx,
	    center, origin, radius_x, radius_y, spread_method,
	    stop_offsets, stop_colors, stop_count);

    if (code < 0)
    {
	gs_grestore(ctx->pgs);
	return gs_rethrow(-1, "could not draw radial gradient");
    }

    gs_grestore(ctx->pgs);
    return 0;
}

int
xps_parse_linear_gradient_brush(xps_context_t *ctx, xps_resource_t *dict, xps_item_t *root)
{
    xps_item_t *node;
    int code;

    xps_item_t *transform_tag = NULL;
    xps_item_t *stop_tag = NULL;

    char *opacity_att;
    char *interpolation_att;
    char *spread_att;
    char *mapping_att;
    char *transform_att;
    char *start_point_att;
    char *end_point_att;

    float stop_offsets[MAX_STOPS];
    float stop_colors[MAX_STOPS * 4];
    int stop_count;
    gs_matrix transform;
    float start_point[2];
    float end_point[2];
    int spread_method;

    gs_clip(ctx->pgs);
    gs_newpath(ctx->pgs);

    dputs("drawing linear gradient brush\n");

    opacity_att = xps_att(root, "Opacity");
    interpolation_att = xps_att(root, "ColorInterpolationMode");
    spread_att = xps_att(root, "SpreadMethod");
    mapping_att = xps_att(root, "MappingMode");
    transform_att = xps_att(root, "Transform");
    start_point_att = xps_att(root, "StartPoint");
    end_point_att = xps_att(root, "EndPoint");

    for (node = xps_down(root); node; node = xps_next(node))
    {
	if (!strcmp(xps_tag(node), "LinearGradientBrush.Transform"))
	    transform_tag = xps_down(node);
	if (!strcmp(xps_tag(node), "LinearGradientBrush.GradientStops"))
	    stop_tag = xps_down(node);
    }

    xps_resolve_resource_reference(ctx, dict, &transform_att, &transform_tag);

    if (!stop_tag || !start_point_att || !end_point_att)
	return gs_throw(-1, "missing attribute in linear gradient tag");

    stop_count = xps_parse_gradient_stops(ctx, stop_tag, stop_offsets, stop_colors, MAX_STOPS);
    if (stop_count == 0)
	return gs_throw(-1, "no gradient stops found");

    gs_gsave(ctx->pgs);

    gs_make_identity(&transform);
    if (transform_att)
	xps_parse_render_transform(ctx, transform_att, &transform);
    if (transform_tag)
	xps_parse_matrix_transform(ctx, transform_tag, &transform);
    gs_concat(ctx->pgs, &transform);

    sscanf(start_point_att, "%g,%g", start_point, start_point + 1);
    sscanf(end_point_att, "%g,%g", end_point, end_point + 1);

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

    code = xps_draw_linear_gradient(ctx,
	    start_point, end_point, spread_method,
	    stop_offsets, stop_colors, stop_count);

    if (code < 0)
    {
	gs_grestore(ctx->pgs);
	return gs_rethrow(-1, "could not draw linear gradient");
    }

    gs_grestore(ctx->pgs);
    return 0;
}

