#include "std.h"
#include "gsmemory.h"
#include "gsstate.h"
#include "gscoord.h"

#include "gxpath.h"     /* gsshade.h depends on it */
#include "gxfixed.h"    /* gsshade.h depends on it */
#include "gsshade.h"
#include "gsfunc.h"
#include "gsfunc3.h"    /* we use stitching and exponential interp */

#include "gserror.h"
#include "xps_gradient_draw.h"

/*
 * Create a Function object to map [0..1] to RGB colors
 * based on the GradientStops array of (offset, red, green, blue) tuples.
 */

private int
xps_make_function(gs_memory_t *mem, gs_function_t **funcp,
	double *stops, int nstops)
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

    k = nstops - 1; /* number of intervals / functions */

    domain = (void*) gs_alloc_bytes(mem, 2 * sizeof(float), "Domain");
    domain[0] = 0.0;
    domain[1] = 1.0;
    sparams.m = 1;
    sparams.Domain = domain;

    range = (void*) gs_alloc_bytes(mem, 6 * sizeof(float), "Range");
    range[0] = 0.0;
    range[1] = 1.0;
    range[2] = 0.0;
    range[3] = 1.0;
    range[4] = 0.0;
    range[5] = 1.0;
    sparams.n = 3;
    sparams.Range = range;

    sparams.k = k;
    functions = (void*) gs_alloc_bytes(mem, k * sizeof(void*), "Functions");
    bounds = (void*) gs_alloc_bytes(mem, (k - 1) * sizeof(float), "Bounds");
    encode = (void*) gs_alloc_bytes(mem, (k * 2) * sizeof(float), "Encode");

    sparams.Functions = functions;
    sparams.Bounds = bounds;
    sparams.Encode = encode;

    for (i = 0; i < k; i++)
    {
	domain = (void*) gs_alloc_bytes(mem, 2 * sizeof(float), "Domain");
	domain[0] = 0.0;
	domain[1] = 1.0;
	lparams.m = 1;
	lparams.Domain = domain;

	range = (void*) gs_alloc_bytes(mem, 6 * sizeof(float), "Range");
	range[0] = 0.0;
	range[1] = 1.0;
	range[2] = 0.0;
	range[3] = 1.0;
	range[4] = 0.0;
	range[5] = 1.0;
	lparams.n = 3;
	lparams.Range = range;

	c0 = (void*) gs_alloc_bytes(mem, 3 * sizeof(float), "C0");
	c0[0] = stops[(i + 0) * 4 + 1];
	c0[1] = stops[(i + 0) * 4 + 2];
	c0[2] = stops[(i + 0) * 4 + 3];
	lparams.C0 = c0;

	c1 = (void*) gs_alloc_bytes(mem, 3 * sizeof(float), "C1");
	c1[0] = stops[(i + 1) * 4 + 1];
	c1[1] = stops[(i + 1) * 4 + 2];
	c1[2] = stops[(i + 1) * 4 + 3];
	lparams.C1 = c1;

	lparams.N = 1;

	code = gs_function_ElIn_init(&lfunc, &lparams, mem);
	if (code < 0)
	    return gs_throw(-1, "gs_function_ElIn_init failed");

	functions[i] = lfunc;

	if (i > 0)
	    bounds[i - 1] = stops[(i + 0) * 4 + 0];

	encode[i * 2 + 0] = 0.0;
	encode[i * 2 + 1] = 1.0;
    }

    code = gs_function_1ItSg_init(&sfunc, &sparams, mem);
    if (code < 0)
	return gs_throw(-1, "gs_function_1ItSg_init failed");

    *funcp = sfunc;

    return gs_okay;
}

/*
 * Linear gradients map to Axial shadings.
 */

private int
xps_draw_one_linear_gradient(gs_memory_t *mem, gs_state *pgs,
	gs_function_t *func, int extend,
	double *p0, double *p1)
{
    gs_imager_state *pis = (gs_imager_state*)pgs;
    gx_device *dev = gs_currentdevice(pgs);
    gs_shading_t *shading;
    gs_shading_A_params_t params;
    gs_color_space colorspace;
    gs_fixed_rect rect;
    int code;

    gs_shading_A_params_init(&params);
    {
	gs_cspace_init_DeviceRGB(mem, &colorspace);

	params.ColorSpace = &colorspace;

	params.Coords[0] = p0[0];
	params.Coords[1] = p0[1];
	params.Coords[2] = p1[0];
	params.Coords[3] = p1[1];

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

#if 0 /* if ghostscript */
    code = gs_shading_fill_path_adjusted(shading, NULL, &rect, dev, pis, false);
#else /* if ghostpcl */
    code = gs_shading_fill_path(shading, NULL, &rect, dev, pis, false);
#endif
    if (code < 0)
    {
	gs_free_object(mem, shading, "gs_shading_A");
	return gs_throw(-1, "gs_shading_fill_path_adjusted failed");
    }

    gs_free_object(mem, shading, "gs_shading_A");

    return gs_okay;
}


/*
 * Radial gradients map more or less to Radial shadings.
 * The inner circle is always a point.
 * The outer circle is actually an ellipse,
 * mess with the transform to squash the circle into the right aspect.
 */

private int
xps_draw_one_radial_gradient(gs_memory_t *mem, gs_state *pgs,
	gs_function_t *func, int extend,
	double *pt0, double rad0,
	double *pt1, double rad1)
{
    gs_imager_state *pis = (gs_imager_state*)pgs;
    gx_device *dev = gs_currentdevice(pgs);
    gs_shading_t *shading;
    gs_shading_R_params_t params;
    gs_color_space colorspace;
    gs_fixed_rect rect;
    int code;

    gs_shading_R_params_init(&params);
    {
	gs_cspace_init_DeviceRGB(mem, &colorspace);

	params.ColorSpace = &colorspace;

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

#if 0 /* if ghostscript */
    code = gs_shading_fill_path_adjusted(shading, NULL, &rect, dev, pis, false);
#else /* if ghostpcl */
    code = gs_shading_fill_path(shading, NULL, &rect, dev, pis, false);
#endif
    if (code < 0)
    {
	gs_free_object(mem, shading, "gs_shading_R");
	return gs_throw(-1, "gs_shading_fill_path_adjusted failed");
    }

    gs_free_object(mem, shading, "gs_shading_R");

    return gs_okay;
}

/*
 * We need to loop and create many shading objects to account
 * for the Repeat and Reflect SpreadMethods.
 */

int
xps_draw_linear_gradient(gs_memory_t *mem, gs_state *pgs,
	double *pt0, double *pt1,
	int spread, double *stops, int nstops)
{
    gs_function_t *func;
    int code;
    int i;

    float dx = pt1[0] - pt0[0];
    float dy = pt1[1] - pt0[1];

    code = xps_make_function(mem, &func, stops, nstops);
    if (code < 0)
	return gs_rethrow(code, "could not make gradient function");

    if (spread == XPS_PAD)
    {
	code = xps_draw_one_linear_gradient(mem, pgs, func, true, pt0, pt1);
	if (code < 0)
	{
	    gs_free_object(mem, func, "gs_function");
	    return gs_rethrow(code, "could not draw axial gradient");
	}
    }
    else
    {
	pt0[0] -= 10 * dx; pt0[1] -= 10 * dy;
	pt1[0] -= 10 * dx; pt1[1] -= 10 * dy;

	for (i = 0; i < 20; i++)
	{
	    if (spread == XPS_REFLECT && (i & 1))
		code = xps_draw_one_linear_gradient(mem, pgs, func, false, pt1, pt0);
	    else
		code = xps_draw_one_linear_gradient(mem, pgs, func, false, pt0, pt1);
	    if (code < 0)
	    {
		gs_free_object(mem, func, "gs_function");
		return gs_rethrow(code, "could not draw axial gradient");
	    }

	    pt0[0] += dx; pt0[1] += dy;
	    pt1[0] += dx; pt1[1] += dy;
	}
    }

    gs_free_object(mem, func, "gs_function");
    return gs_okay;
}

int
xps_draw_radial_gradient(gs_memory_t *mem, gs_state *pgs,
	double *pt0, double *pt1, double xrad, double yrad,
	int spread, double *stops, int nstops)
{
    gs_function_t *func;
    float invscale;
    float rad0, rad1;
    float dx, dy;
    int code;
    int i;

    gs_gsave(pgs);

    /* scale the ctm to make ellipses */
    gs_scale(pgs, 1.0, yrad / xrad);

    invscale = xrad / yrad;
    pt0[1] *= invscale;
    pt1[1] *= invscale;

    code = xps_make_function(mem, &func, stops, nstops);
    if (code < 0)
	return gs_rethrow(code, "could not make gradient function");

    rad0 = 0.0;
    rad1 = xrad;

    dx = pt1[0] - pt0[0];
    dy = pt1[1] - pt0[1];

    if (spread == XPS_PAD)
    {
	code = xps_draw_one_radial_gradient(mem, pgs, func, true, pt0, rad0, pt1, rad1);
	if (code < 0)
	{
	    gs_free_object(mem, func, "gs_function");
	    gs_grestore(pgs);
	    return gs_rethrow(code, "could not draw axial gradient");
	}
    }
    else
    {
	for (i = 0; i < 20; i++)
	{
	    if (spread == XPS_REFLECT && (i & 1))
		code = xps_draw_one_radial_gradient(mem, pgs, func, false, pt1, rad1, pt0, rad0);
	    else
		code = xps_draw_one_radial_gradient(mem, pgs, func, false, pt0, rad0, pt1, rad1);

	    if (code < 0)
	    {
		gs_free_object(mem, func, "gs_function");
		gs_grestore(pgs);
		return gs_rethrow(code, "could not draw axial gradient");
	    }

	    rad0 = rad1;
	    rad1 += xrad;

	    pt0[0] += dx; pt0[1] += dy;
	    pt1[0] += dx; pt1[1] += dy;
	}
    }

    gs_free_object(mem, func, "gs_function");
    gs_grestore(pgs);
    return gs_okay;
}
