/* Copyright (C) 1998, 2000 Aladdin Enterprises.  All rights reserved.
  
  This software is provided AS-IS with no warranty, either express or
  implied.
  
  This software is distributed under license and may not be copied,
  modified or distributed except as expressly authorized under the terms
  of the license contained in the file LICENSE in this distribution.
  
  For more information about licensing, please refer to
  http://www.ghostscript.com/licensing/. For information on
  commercial licensing, go to http://www.artifex.com/licensing/ or
  contact Artifex Software, Inc., 101 Lucas Valley Road #110,
  San Rafael, CA  94903, U.S.A., +1(415)492-9861.
*/

/* $Id$ */
/* Rendering for non-mesh shadings */
#include "math_.h"
#include "memory_.h"
#include "gx.h"
#include "gserrors.h"
#include "gsmatrix.h"		/* for gscoord.h */
#include "gscoord.h"
#include "gspath.h"
#include "gxcspace.h"
#include "gxdcolor.h"
#include "gxfarith.h"
#include "gxfixed.h"
#include "gxistate.h"
#include "gxpath.h"
#include "gxshade.h"
#include "gxshade4.h"
#include "gxdevcli.h"
#include "vdtrace.h"
#include <assert.h>

#define VD_TRACE_AXIAL_PATCH 1
#define VD_TRACE_RADIAL_PATCH 1
#define VD_TRACE_FUNCTIONAL_PATCH 1


/* ================ Utilities ================ */

/* Check whether 2 colors fall within the smoothness criterion. */
private bool
shade_colors2_converge(const gs_client_color cc[2],
		       const shading_fill_state_t * pfs)
{
    int ci;

    for (ci = pfs->num_components - 1; ci >= 0; --ci)
	if (fabs(cc[1].paint.values[ci] - cc[0].paint.values[ci]) >
	    pfs->cc_max_error[ci]
	    )
	    return false;
    return true;
}

#if !NEW_SHADINGS 
/* Fill a user space rectangle that is also a device space rectangle. */
private int
shade_fill_device_rectangle(const shading_fill_state_t * pfs,
			    const gs_fixed_point * p0,
			    const gs_fixed_point * p1,
			    gx_device_color * pdevc)
{
    gs_imager_state *pis = pfs->pis;
    fixed xmin, ymin, xmax, ymax;
    int x, y;

    if (p0->x < p1->x)
	xmin = p0->x, xmax = p1->x;
    else
	xmin = p1->x, xmax = p0->x;
    if (p0->y < p1->y)
	ymin = p0->y, ymax = p1->y;
    else
	ymin = p1->y, ymax = p0->y;

    /* See gx_default_fill_path for an explanation of the tweak below. */
    xmin -= pis->fill_adjust.x;
    if (pis->fill_adjust.x == fixed_half)
	xmin += fixed_epsilon;
    xmax += pis->fill_adjust.x;
    ymin -= pis->fill_adjust.y;
    if (pis->fill_adjust.y == fixed_half)
	ymin += fixed_epsilon;
    ymax += pis->fill_adjust.y;
    x = fixed2int_var_pixround(xmin);
    y = fixed2int_var_pixround(ymin);
    return
	gx_fill_rectangle_device_rop(x, y,
				     fixed2int_var_pixround(xmax) - x,
				     fixed2int_var_pixround(ymax) - y,
				     pdevc, pfs->dev, pis->log_op);
}
#endif

/* ================ Specific shadings ================ */

/* ---------------- Function-based shading ---------------- */

#define Fb_max_depth 32		/* 16 bits each in X and Y */
typedef struct Fb_frame_s {	/* recursion frame */
    gs_rect region;
    gs_client_color cc[4];	/* colors at 4 corners */
    gs_paint_color c_min, c_max; /* estimated color range for the region */
    bool painted;
    bool divide_X;
    int state;
} Fb_frame_t;

typedef struct Fb_fill_state_s {
    shading_fill_state_common;
    const gs_shading_Fb_t *psh;
    gs_matrix_fixed ptm;	/* parameter space -> device space */
    bool orthogonal;		/* true iff ptm is xxyy or xyyx */
    int depth;			/* 1 <= depth < Fb_max_depth */
    bool painted;               /* the last region is painted */
    gs_paint_color c_min, c_max; /* estimated color range for the last region */
    Fb_frame_t frames[Fb_max_depth];
} Fb_fill_state_t;
/****** NEED GC DESCRIPTOR ******/

#define CheckRET(a) { int code = a; if (code < 0) return code; }
#define Exch(t,a,b) { t x; x = a; a = b; b = x; }

#if !NEW_SHADINGS 

private bool
Fb_build_color_range(const Fb_fill_state_t * pfs, const Fb_frame_t * fp, 
		     gs_paint_color * c_min, gs_paint_color * c_max)
{
    int ci;
    const gs_client_color *cc = fp->cc;
    bool big = false;
    for (ci = 0; ci < pfs->num_components; ++ci) {
	float c0 = cc[0].paint.values[ci], c1 = cc[1].paint.values[ci],
	      c2 = cc[2].paint.values[ci], c3 = cc[3].paint.values[ci];
	float min01, max01, min23, max23;
	if (c0 < c1)
	    min01 = c0, max01 = c1;
	else
	    min01 = c1, max01 = c0;
	if (c2 < c3)
	    min23 = c2, max23 = c3;
	else
	    min23 = c3, max23 = c2;
	c_max->values[ci] = max(max01, max23);
	c_min->values[ci] = min(min01, min23);
	big |= ((c_max->values[ci] - c_min->values[ci]) > pfs->cc_max_error[ci]);
    }
    return !big;
}

private bool 
Fb_unite_color_range(const Fb_fill_state_t * pfs, 
		     const gs_paint_color * c_min0, const gs_paint_color * c_max0, 
		     gs_paint_color * c_min1, gs_paint_color * c_max1)
{   int ci;
    bool big = false;
    for (ci = 0; ci < pfs->num_components; ++ci) {
	c_min1->values[ci] = min(c_min1->values[ci], c_min0->values[ci]);
	c_max1->values[ci] = max(c_max1->values[ci], c_max0->values[ci]);
	big |= ((c_max1->values[ci] - c_min1->values[ci]) > pfs->cc_max_error[ci]);
    }
    return !big;
}

private int
Fb_build_half_region(Fb_fill_state_t * pfs, int h, bool use_old)
{   Fb_frame_t * fp0 = &pfs->frames[pfs->depth];
    Fb_frame_t * fp1 = &pfs->frames[pfs->depth + 1];
    gs_function_t *pfn = pfs->psh->params.Function;
    const double x0 = fp0->region.p.x, y0 = fp0->region.p.y,
		 x1 = fp0->region.q.x, y1 = fp0->region.q.y;
    float v[2];

    if (fp0->divide_X) {
	double xm = (x0 + x1) * 0.5;
	int h10 = (!h ? 1 : 0), h32 = (!h ? 3 : 2);
	int h01 = (!h ? 0 : 1), h23 = (!h ? 2 : 3);
	if_debug1('|', "[|]dividing at x=%g\n", xm);
	if (use_old) {
	    fp1->cc[h10].paint = fp1->cc[h01].paint;
	    fp1->cc[h32].paint = fp1->cc[h23].paint;
	} else {
	    v[0] = xm, v[1] = y0;
	    CheckRET(gs_function_evaluate(pfn, v, fp1->cc[h10].paint.values));
	    v[1] = y1;
	    CheckRET(gs_function_evaluate(pfn, v, fp1->cc[h32].paint.values));
	}
	fp1->cc[h01].paint = fp0->cc[h01].paint;
	fp1->cc[h23].paint = fp0->cc[h23].paint;
	if (!h) {
	    fp1->region.p.x = x0, fp1->region.p.y = y0;
	    fp1->region.q.x = xm, fp1->region.q.y = y1;
	} else {
	    fp1->region.p.x = xm, fp1->region.p.y = y0;
	    fp1->region.q.x = x1, fp1->region.q.y = y1;
	}
    } else {
	double ym = (y0 + y1) * 0.5;
	int h20 = (!h ? 2 : 0), h31 = (!h ? 3 : 1);
	int h02 = (!h ? 0 : 2), h13 = (!h ? 1 : 3);
	if_debug1('|', "[|]dividing at y=%g\n", ym);
	if (use_old) {
	    fp1->cc[h20].paint = fp1->cc[h02].paint;
	    fp1->cc[h31].paint = fp1->cc[h13].paint;
	} else {
	    v[0] = x0, v[1] = ym;
	    CheckRET(gs_function_evaluate(pfn, v, fp1->cc[h20].paint.values));
	    v[0] = x1;
	    CheckRET(gs_function_evaluate(pfn, v, fp1->cc[h31].paint.values));
	}
	fp1->cc[h02].paint = fp0->cc[h02].paint;
	fp1->cc[h13].paint = fp0->cc[h13].paint;
	if (!h) {
	    fp1->region.p.x = x0, fp1->region.p.y = y0;
	    fp1->region.q.x = x1, fp1->region.q.y = ym;
	} else {
	    fp1->region.p.x = x0, fp1->region.p.y = ym;
	    fp1->region.q.x = x1, fp1->region.q.y = y1;
	}
    }
    return 0;
}

private int
Fb_fill_region_with_constant_color(const Fb_fill_state_t * pfs, const Fb_frame_t * fp, gs_paint_color * c_min, gs_paint_color * c_max)
{
    const gs_shading_Fb_t * const psh = pfs->psh;
    gx_device_color dev_color;
    const gs_color_space *pcs = psh->params.ColorSpace;
    gs_client_color cc;
    gs_fixed_point pts[4];
    int code, ci;

    if_debug0('|', "[|]... filling region\n");
    cc = fp->cc[0];
    for (ci = 0; ci < pfs->num_components; ++ci)
	cc.paint.values[ci] = (c_min->values[ci] + c_max->values[ci]) / 2;
    (*pcs->type->restrict_color)(&cc, pcs);
    (*pcs->type->remap_color)(&cc, pcs, &dev_color, pfs->pis,
			      pfs->dev, gs_color_select_texture);
    gs_point_transform2fixed(&pfs->ptm, fp->region.p.x, fp->region.p.y, &pts[0]);
    gs_point_transform2fixed(&pfs->ptm, fp->region.q.x, fp->region.q.y, &pts[2]);
    if (pfs->orthogonal) {
	code = shade_fill_device_rectangle((const shading_fill_state_t *)pfs,
					   &pts[0], &pts[2], &dev_color);
    } else {
	gx_path *ppath = gx_path_alloc(pfs->pis->memory, "Fb_fill");

	gs_point_transform2fixed(&pfs->ptm, fp->region.q.x, fp->region.p.y, &pts[1]);
	gs_point_transform2fixed(&pfs->ptm, fp->region.p.x, fp->region.q.y, &pts[3]);
	gx_path_add_point(ppath, pts[0].x, pts[0].y);
	gx_path_add_lines(ppath, pts + 1, 3);
	code = shade_fill_path((const shading_fill_state_t *)pfs,
			       ppath, &dev_color, &pfs->pis->fill_adjust);
	gx_path_free(ppath, "Fb_fill");
    }
    return code;
}

private int
Fb_fill_region_lazy(Fb_fill_state_t * pfs)
{   fixed minsize = fixed_1 * 7 / 10; /* pixels */
    float min_extreme_dist = 4; /* points */
    fixed min_edist_x = float2fixed(min_extreme_dist * pfs->dev->HWResolution[0] / 72); 
    fixed min_edist_y = float2fixed(min_extreme_dist * pfs->dev->HWResolution[1] / 72); 
    min_edist_x = max(min_edist_x, minsize);
    min_edist_y = max(min_edist_y, minsize);
    while (pfs->depth >= 0) {
	Fb_frame_t * fp = &pfs->frames[pfs->depth];
	fixed size_x, size_y;
	bool single_extreme, single_pixel, small_color_diff = false;
	switch (fp->state) {
	    case 0:
		fp->painted = false;
		{   /* Region size in device space : */
		    gs_point p, q;
		    CheckRET(gs_distance_transform(fp->region.q.x - fp->region.p.x, 0, (gs_matrix *)&pfs->ptm, &p));
		    CheckRET(gs_distance_transform(0, fp->region.q.y - fp->region.p.y, (gs_matrix *)&pfs->ptm, &q));
		    size_x = float2fixed(hypot(p.x, p.y));
		    size_y = float2fixed(hypot(q.x, q.y));
		    single_extreme = (float2fixed(any_abs(p.x) + any_abs(q.x)) < min_edist_x && 
		                      float2fixed(any_abs(p.y) + any_abs(q.y)) < min_edist_y);
		    single_pixel = (size_x < minsize && size_y < minsize);
		    /* Note: single_pixel implies single_extreme. */
		}
		if (single_extreme || pfs->depth >= Fb_max_depth - 1)
		    small_color_diff = Fb_build_color_range(pfs, fp, &pfs->c_min, &pfs->c_max);
		if ((single_extreme && small_color_diff) || 
		    single_pixel ||
		    pfs->depth >= Fb_max_depth - 1) {
		    Fb_build_color_range(pfs, fp, &pfs->c_min, &pfs->c_max);
		    -- pfs->depth;
		    pfs->painted = false;
		    break;
		}
		fp->state = 1;
		fp->divide_X = (size_x > size_y);
		CheckRET(Fb_build_half_region(pfs, 0, false));
		++ pfs->depth; /* Do recur, left branch. */
		pfs->frames[pfs->depth].state = 0;
		break;
	    case 1:
		fp->painted = pfs->painted; /* Save return value. */
		fp->c_min = pfs->c_min;	    /* Save return value. */
		fp->c_max = pfs->c_max;	    /* Save return value. */
		CheckRET(Fb_build_half_region(pfs, 1, true));
		fp->state = 2;
		++ pfs->depth; /* Do recur, right branch. */
		pfs->frames[pfs->depth].state = 0;
		break;
	    case 2:
		if (fp->painted && pfs->painted) { 
		    /* Both branches are painted - do nothing */
		} else if (fp->painted && !pfs->painted) { 
		    CheckRET(Fb_fill_region_with_constant_color(pfs, fp + 1, &pfs->c_min, &pfs->c_max));
		    pfs->painted = true;
		} else if (!fp->painted && pfs->painted) { 
		    CheckRET(Fb_build_half_region(pfs, 0, true));
		    CheckRET(Fb_fill_region_with_constant_color(pfs, fp + 1, &fp->c_min, &fp->c_max));
		} else {
		    gs_paint_color c_min = pfs->c_min, c_max = pfs->c_max;
		    if (Fb_unite_color_range(pfs, &fp->c_min, &fp->c_max, &pfs->c_min, &pfs->c_max)) {
			/* Both halfs are not painted, and color range is small - do nothing */
			/* return: pfs->painted = false; pfs->c_min, pfs->c_max are united. */
		    } else {
			/* Color range too big, paint each part with its color : */
			CheckRET(Fb_fill_region_with_constant_color(pfs, fp + 1, &c_min, &c_max));
			CheckRET(Fb_build_half_region(pfs, 0, true));
			CheckRET(Fb_fill_region_with_constant_color(pfs, fp + 1, &fp->c_min, &fp->c_max));
			pfs->painted = true;
		    }
		}
		-- pfs->depth;
		break;
	}
    }
    return 0;
}

private int
Fb_fill_region(Fb_fill_state_t * pfs)
{   
    pfs->depth = 0;
    pfs->frames[0].state = 0;
    CheckRET(Fb_fill_region_lazy(pfs));
    if(!pfs->painted)
	CheckRET(Fb_fill_region_with_constant_color(pfs, &pfs->frames[0], &pfs->c_min, &pfs->c_max));
    return 0;
}
#endif

#if NEW_SHADINGS 
private inline void
make_other_poles(patch_curve_t curve[4])
{
    int i, j;

    for (i = 0; i < 4; i++) {
	j = (i + 1) % 4;
	curve[i].control[0].x = (curve[i].vertex.p.x * 2 + curve[j].vertex.p.x) / 3;
	curve[i].control[0].y = (curve[i].vertex.p.y * 2 + curve[j].vertex.p.y) / 3;
	curve[i].control[1].x = (curve[i].vertex.p.x + curve[j].vertex.p.x * 2) / 3;
	curve[i].control[1].y = (curve[i].vertex.p.y + curve[j].vertex.p.y * 2) / 3;
    }
}

private int
Fb_fill_region(Fb_fill_state_t * pfs, const gs_rect *rect)
{
    patch_fill_state_t pfs1;
    patch_curve_t curve[4];
    Fb_frame_t * fp = &pfs->frames[0];
    int code;

    if (VD_TRACE_FUNCTIONAL_PATCH && vd_allowed('s')) {
	vd_get_dc('s');
	vd_set_shift(0, 0);
	vd_set_scale(0.01);
	vd_set_origin(0, 0);
    }
    memcpy(&pfs1, (shading_fill_state_t *)pfs, sizeof(shading_fill_state_t));
    pfs1.Function = pfs->psh->params.Function;
    code = init_patch_fill_state(&pfs1);
    if (code < 0)
	return code;
    pfs1.maybe_self_intersecting = false;
    pfs1.n_color_args = 2;
    shade_bbox_transform2fixed(rect, pfs->pis, &pfs1.rect);
    gs_point_transform2fixed(&pfs->ptm, fp->region.p.x, fp->region.p.y, &curve[0].vertex.p);
    gs_point_transform2fixed(&pfs->ptm, fp->region.q.x, fp->region.p.y, &curve[1].vertex.p);
    gs_point_transform2fixed(&pfs->ptm, fp->region.q.x, fp->region.q.y, &curve[2].vertex.p);
    gs_point_transform2fixed(&pfs->ptm, fp->region.p.x, fp->region.q.y, &curve[3].vertex.p);
    make_other_poles(curve);
    curve[0].vertex.cc[0] = fp->region.p.x;   curve[0].vertex.cc[1] = fp->region.p.y;
    curve[1].vertex.cc[0] = fp->region.q.x;   curve[1].vertex.cc[1] = fp->region.p.y;
    curve[2].vertex.cc[0] = fp->region.q.x;   curve[2].vertex.cc[1] = fp->region.q.y;
    curve[3].vertex.cc[0] = fp->region.p.x;   curve[3].vertex.cc[1] = fp->region.q.y;
    code = patch_fill(&pfs1, curve, NULL, NULL);
    term_patch_fill_state(&pfs1);
    if (VD_TRACE_FUNCTIONAL_PATCH && vd_allowed('s'))
	vd_release_dc;
    return code;
}
#endif

int
gs_shading_Fb_fill_rectangle(const gs_shading_t * psh0, const gs_rect * rect,
			     gx_device * dev, gs_imager_state * pis)
{
    const gs_shading_Fb_t * const psh = (const gs_shading_Fb_t *)psh0;
    gs_matrix save_ctm;
    int xi, yi;
    float x[2], y[2];
    Fb_fill_state_t state;

    shade_init_fill_state((shading_fill_state_t *) & state, psh0, dev, pis);
    state.psh = psh;
    /****** HACK FOR FIXED-POINT MATRIX MULTIPLY ******/
    gs_currentmatrix((gs_state *) pis, &save_ctm);
    gs_concat((gs_state *) pis, &psh->params.Matrix);
    state.ptm = pis->ctm;
    gs_setmatrix((gs_state *) pis, &save_ctm);
    state.orthogonal = is_xxyy(&state.ptm) || is_xyyx(&state.ptm);
    /* Compute the parameter X and Y ranges. */
    {
	gs_rect pbox;

	gs_bbox_transform_inverse(rect, &psh->params.Matrix, &pbox);
	x[0] = max(pbox.p.x, psh->params.Domain[0]);
	x[1] = min(pbox.q.x, psh->params.Domain[1]);
	y[0] = max(pbox.p.y, psh->params.Domain[2]);
	y[1] = min(pbox.q.y, psh->params.Domain[3]);
    }
    for (xi = 0; xi < 2; ++xi)
	for (yi = 0; yi < 2; ++yi) {
	    float v[2];

	    v[0] = x[xi], v[1] = y[yi];
	    gs_function_evaluate(psh->params.Function, v,
				 state.frames[0].cc[yi * 2 + xi].paint.values);
	}
    state.frames[0].region.p.x = x[0];
    state.frames[0].region.p.y = y[0];
    state.frames[0].region.q.x = x[1];
    state.frames[0].region.q.y = y[1];
    return Fb_fill_region(&state, rect);
}

/* ---------------- Axial shading ---------------- */

/*
 * Note that the max recursion depth and the frame structure are shared
 * with radial shading.
 */
#define AR_max_depth 16
typedef struct AR_frame_s {	/* recursion frame */
    double t0, t1;
    gs_client_color cc[2];	/* color at t0, t1 */
} AR_frame_t;

#define A_max_depth AR_max_depth
typedef AR_frame_t A_frame_t;

typedef struct A_fill_state_s {
    shading_fill_state_common;
    const gs_shading_A_t *psh;
    bool orthogonal;		/* true iff ctm is xxyy or xyyx */
    gs_rect rect;		/* bounding rectangle in user space */
    gs_point delta;
    double length, dd;
    int depth;
#   if NEW_SHADINGS
    double t0, t1;
    double v0, v1, u0, u1;
#   else
    A_frame_t frames[A_max_depth];
#   endif
} A_fill_state_t;
/****** NEED GC DESCRIPTOR ******/

/* Note t0 and t1 vary over [0..1], not the Domain. */

#if !NEW_SHADINGS
private int
A_fill_stripe(const A_fill_state_t * pfs, gs_client_color *pcc,
	      floatp t0, floatp t1)
{
    const gs_shading_A_t * const psh = pfs->psh;
    gx_device_color dev_color;
    const gs_color_space *pcs = psh->params.ColorSpace;
    gs_imager_state *pis = pfs->pis;
    double
	x0 = psh->params.Coords[0] + pfs->delta.x * t0,
	y0 = psh->params.Coords[1] + pfs->delta.y * t0;
    double
	x1 = psh->params.Coords[0] + pfs->delta.x * t1,
	y1 = psh->params.Coords[1] + pfs->delta.y * t1;
    gs_fixed_point pts[4];
    int code;

    (*pcs->type->restrict_color)(pcc, pcs);
    (*pcs->type->remap_color)(pcc, pcs, &dev_color, pis,
			      pfs->dev, gs_color_select_texture);
    if (x0 == x1 && pfs->orthogonal) {
	/*
	 * Stripe is horizontal in user space and horizontal or vertical
	 * in device space.
	 */
	x0 = pfs->rect.p.x;
	x1 = pfs->rect.q.x;
    } else if (y0 == y1 && pfs->orthogonal) {
	/*
	 * Stripe is vertical in user space and horizontal or vertical
	 * in device space.
	 */
	y0 = pfs->rect.p.y;
	y1 = pfs->rect.q.y;
    } else {
	/*
	 * Stripe is neither horizontal nor vertical in user space.
	 * Extend it to the edges of the (user-space) rectangle.
	 */
	gx_path *ppath = gx_path_alloc(pis->memory, "A_fill");
	if (fabs(pfs->delta.x) < fabs(pfs->delta.y)) {
	    /*
	     * Calculate intersections with vertical sides of rect.
	     */
	    double slope = pfs->delta.x / pfs->delta.y;
	    double yi = y0 - slope * (pfs->rect.p.x - x0);

	    gs_point_transform2fixed(&pis->ctm, pfs->rect.p.x, yi, &pts[0]);
	    yi = y1 - slope * (pfs->rect.p.x - x1);
	    gs_point_transform2fixed(&pis->ctm, pfs->rect.p.x, yi, &pts[1]);
	    yi = y1 - slope * (pfs->rect.q.x - x1);
	    gs_point_transform2fixed(&pis->ctm, pfs->rect.q.x, yi, &pts[2]);
	    yi = y0 - slope * (pfs->rect.q.x - x0);
	    gs_point_transform2fixed(&pis->ctm, pfs->rect.q.x, yi, &pts[3]);
	}
	else {
	    /*
	     * Calculate intersections with horizontal sides of rect.
	     */
	    double slope = pfs->delta.y / pfs->delta.x;
	    double xi = x0 - slope * (pfs->rect.p.y - y0);

	    gs_point_transform2fixed(&pis->ctm, xi, pfs->rect.p.y, &pts[0]);
	    xi = x1 - slope * (pfs->rect.p.y - y1);
	    gs_point_transform2fixed(&pis->ctm, xi, pfs->rect.p.y, &pts[1]);
	    xi = x1 - slope * (pfs->rect.q.y - y1);
	    gs_point_transform2fixed(&pis->ctm, xi, pfs->rect.q.y, &pts[2]);
	    xi = x0 - slope * (pfs->rect.q.y - y0);
	    gs_point_transform2fixed(&pis->ctm, xi, pfs->rect.q.y, &pts[3]);
	}
	gx_path_add_point(ppath, pts[0].x, pts[0].y);
	gx_path_add_lines(ppath, pts + 1, 3);
	code = shade_fill_path((const shading_fill_state_t *)pfs,
			       ppath, &dev_color, &pfs->pis->fill_adjust);
	gx_path_free(ppath, "A_fill");
	return code;
    }
    /* Stripe is horizontal or vertical in both user and device space. */
    gs_point_transform2fixed(&pis->ctm, x0, y0, &pts[0]);
    gs_point_transform2fixed(&pis->ctm, x1, y1, &pts[1]);
    return
	shade_fill_device_rectangle((const shading_fill_state_t *)pfs,
				    &pts[0], &pts[1], &dev_color);
}

private int
A_fill_region(A_fill_state_t * pfs, const gs_rect *rect)
{
    const gs_shading_A_t * const psh = pfs->psh;
    gs_function_t * const pfn = psh->params.Function;
    A_frame_t *fp = &pfs->frames[pfs->depth - 1];

    for (;;) {
	double t0 = fp->t0, t1 = fp->t1;
	float ft0, ft1;

	if ((!(pfn->head.is_monotonic > 0 ||
	       (ft0 = (float)t0, ft1 = (float)t1,
		gs_function_is_monotonic(pfn, &ft0, &ft1) > 0)) ||
	     !shade_colors2_converge(fp->cc,
				     (const shading_fill_state_t *)pfs)) &&
	     /*
	      * The function isn't monotonic, or the colors don't converge.
	      * Is the stripe less than 1 pixel wide?
	      */
	    pfs->length * (t1 - t0) > 1 &&
	    fp < &pfs->frames[countof(pfs->frames) - 1]
	    ) {
	    /* Subdivide the interval and recur.  */
	    double tm = (t0 + t1) * 0.5;
	    float dm = tm * pfs->dd + psh->params.Domain[0];

	    gs_function_evaluate(pfn, &dm, fp[1].cc[1].paint.values);
	    fp[1].cc[0].paint = fp->cc[0].paint;
	    fp[1].t0 = t0;
	    fp[1].t1 = fp->t0 = tm;
	    fp->cc[0].paint = fp[1].cc[1].paint;
	    ++fp;
	} else {
	    /* Fill the region with the color. */
	    int code = A_fill_stripe(pfs, &fp->cc[0], t0, t1);

	    if (code < 0 || fp == &pfs->frames[0])
		return code;
	    --fp;
	}
    }
}
#else
private int
A_fill_region(A_fill_state_t * pfs, const gs_rect *rect)
{
    const gs_shading_A_t * const psh = pfs->psh;
    gs_function_t * const pfn = psh->params.Function;
    double x0 = psh->params.Coords[0] + pfs->delta.x * pfs->v0;
    double y0 = psh->params.Coords[1] + pfs->delta.y * pfs->v0;
    double x1 = psh->params.Coords[0] + pfs->delta.x * pfs->v1;
    double y1 = psh->params.Coords[1] + pfs->delta.y * pfs->v1;
    double h0 = pfs->u0, h1 = pfs->u1;
    patch_curve_t curve[4];
    patch_fill_state_t pfs1;
    int code;

    memcpy(&pfs1, (shading_fill_state_t *)pfs, sizeof(shading_fill_state_t));
    pfs1.Function = pfn;
    code = init_patch_fill_state(&pfs1);
    if (code < 0)
	return code;
    shade_bbox_transform2fixed(rect, pfs->pis, &pfs1.rect);
    pfs1.maybe_self_intersecting = false;
    gs_point_transform2fixed(&pfs->pis->ctm, x0 + pfs->delta.y * h0, y0 - pfs->delta.x * h0, &curve[0].vertex.p);
    gs_point_transform2fixed(&pfs->pis->ctm, x1 + pfs->delta.y * h0, y1 - pfs->delta.x * h0, &curve[1].vertex.p);
    gs_point_transform2fixed(&pfs->pis->ctm, x1 + pfs->delta.y * h1, y1 - pfs->delta.x * h1, &curve[2].vertex.p);
    gs_point_transform2fixed(&pfs->pis->ctm, x0 + pfs->delta.y * h1, y0 - pfs->delta.x * h1, &curve[3].vertex.p);
    curve[0].vertex.cc[0] = curve[0].vertex.cc[1] = pfs->t0; /* The element cc[1] is set to a dummy value against */
    curve[1].vertex.cc[0] = curve[1].vertex.cc[1] = pfs->t1; /* interrupts while an idle priocessing in gxshade.6.c .  */
    curve[2].vertex.cc[0] = curve[2].vertex.cc[1] = pfs->t1;
    curve[3].vertex.cc[0] = curve[3].vertex.cc[1] = pfs->t0;
    make_other_poles(curve);
    code = patch_fill(&pfs1, curve, NULL, NULL);
    term_patch_fill_state(&pfs1);
    return code;
}
#endif

private inline int
gs_shading_A_fill_rectangle_aux(const gs_shading_t * psh0, const gs_rect * rect,
			    gx_device * dev, gs_imager_state * pis)
{
    const gs_shading_A_t *const psh = (const gs_shading_A_t *)psh0;
    gs_matrix cmat;
    gs_rect t_rect;
    A_fill_state_t state;
#   if !NEW_SHADINGS
    gs_client_color rcc[2];
#   endif
    float d0 = psh->params.Domain[0], d1 = psh->params.Domain[1];
    float dd = d1 - d0;
    float t0, t1;
#   if !NEW_SHADINGS
    float t[2];
#   endif
    gs_point dist;
#   if !NEW_SHADINGS
    int i;
#   endif
    int code = 0;

    shade_init_fill_state((shading_fill_state_t *)&state, psh0, dev, pis);
    state.psh = psh;
    state.orthogonal = is_xxyy(&pis->ctm) || is_xyyx(&pis->ctm);
    state.rect = *rect;
    /*
     * Compute the parameter range.  We construct a matrix in which
     * (0,0) corresponds to t = 0 and (0,1) corresponds to t = 1,
     * and use it to inverse-map the rectangle to be filled.
     */
    cmat.tx = psh->params.Coords[0];
    cmat.ty = psh->params.Coords[1];
    state.delta.x = psh->params.Coords[2] - psh->params.Coords[0];
    state.delta.y = psh->params.Coords[3] - psh->params.Coords[1];
    cmat.yx = state.delta.x;
    cmat.yy = state.delta.y;
    cmat.xx = cmat.yy;
    cmat.xy = -cmat.yx;
    gs_bbox_transform_inverse(rect, &cmat, &t_rect);
#   if NEW_SHADINGS
	t0 = max(t_rect.p.y, 0);
	t1 = min(t_rect.q.y, 1);
	state.v0 = t0;
	state.v1 = t1;
	state.u0 = t_rect.p.x;
	state.u1 = t_rect.q.x;
	state.t0 = t0 * dd + d0;
	state.t1 = t1 * dd + d0;
#   else
	state.frames[0].t0 = t0 = max(t_rect.p.y, 0);
	t[0] = t0 * dd + d0;
	state.frames[0].t1 = t1 = min(t_rect.q.y, 1);
	t[1] = t1 * dd + d0;
	for (i = 0; i < 2; ++i) {
	    gs_function_evaluate(psh->params.Function, &t[i],
				 rcc[i].paint.values);
	}
	memcpy(state.frames[0].cc, rcc, sizeof(rcc[0]) * 2);
#   endif
    gs_distance_transform(state.delta.x, state.delta.y, &ctm_only(pis),
			  &dist);
    state.length = hypot(dist.x, dist.y);	/* device space line length */
    state.dd = dd;
    state.depth = 1;
    code = A_fill_region(&state, rect);
#   if NEW_SHADINGS
    if (psh->params.Extend[0] && t0 > t_rect.p.y) {
	if (code < 0)
	    return code;
	/* Use the general algorithm, because we need the trapping. */
	state.v0 = t_rect.p.y;
	state.v1 = t0;
	state.t0 = state.t1 = t0 * dd + d0;
	code = A_fill_region(&state, rect);
    }
    if (psh->params.Extend[1] && t1 < t_rect.q.y) {
	if (code < 0)
	    return code;
	/* Use the general algorithm, because we need the trapping. */
	state.v0 = t1;
	state.v1 = t_rect.q.y;
	state.t0 = state.t1 = t1 * dd + d0;
	code = A_fill_region(&state, rect);
    }
#   else
    if (psh->params.Extend[0] && t0 > t_rect.p.y) {
	if (code < 0)
	    return code;
	code = A_fill_stripe(&state, &rcc[0], t_rect.p.y, t0);
    }
    if (psh->params.Extend[1] && t1 < t_rect.q.y) {
	if (code < 0)
	    return code;
	code = A_fill_stripe(&state, &rcc[1], t1, t_rect.q.y);
    }
#   endif
    return code;
}

int
gs_shading_A_fill_rectangle(const gs_shading_t * psh0, const gs_rect * rect,
			    gx_device * dev, gs_imager_state * pis)
{
    int code;

    if (VD_TRACE_AXIAL_PATCH && vd_allowed('s')) {
	vd_get_dc('s');
	vd_set_shift(0, 0);
	vd_set_scale(0.01);
	vd_set_origin(0, 0);
    }
    code = gs_shading_A_fill_rectangle_aux(psh0, rect, dev, pis);
    if (VD_TRACE_AXIAL_PATCH && vd_allowed('s'))
	vd_release_dc;
    return code;
}

/* ---------------- Radial shading ---------------- */

#define R_max_depth AR_max_depth
typedef AR_frame_t R_frame_t;

typedef struct R_fill_state_s {
    shading_fill_state_common;
    const gs_shading_R_t *psh;
    gs_rect rect;
    gs_point delta;
    double dr, width, dd;
    int depth;
    R_frame_t frames[R_max_depth];
} R_fill_state_t;
/****** NEED GC DESCRIPTOR ******/

/* Note t0 and t1 vary over [0..1], not the Domain. */

private int
R_fill_annulus(const R_fill_state_t * pfs, gs_client_color *pcc,
	       floatp t0, floatp t1, floatp r0, floatp r1, const gs_fixed_point *fill_adjust)
{
    const gs_shading_R_t * const psh = pfs->psh;
    gx_device_color dev_color;
    const gs_color_space *pcs = psh->params.ColorSpace;
    gs_imager_state *pis = pfs->pis;
    double
	x0 = psh->params.Coords[0] + pfs->delta.x * t0,
	y0 = psh->params.Coords[1] + pfs->delta.y * t0;
    double
	x1 = psh->params.Coords[0] + pfs->delta.x * t1,
	y1 = psh->params.Coords[1] + pfs->delta.y * t1;
    gx_path *ppath = gx_path_alloc(pis->memory, "R_fill");
    int code;

    (*pcs->type->restrict_color)(pcc, pcs);
    (*pcs->type->remap_color)(pcc, pcs, &dev_color, pis,
			      pfs->dev, gs_color_select_texture);
    if ((code = gs_imager_arc_add(ppath, pis, false, x0, y0, r0,
				  0.0, 360.0, false)) >= 0 &&
	(code = gs_imager_arc_add(ppath, pis, true, x1, y1, r1,
				  360.0, 0.0, false)) >= 0
	) {
	code = shade_fill_path((const shading_fill_state_t *)pfs,
			       ppath, &dev_color, fill_adjust);
    }
    gx_path_free(ppath, "R_fill");
    return code;
}

private int
R_fill_triangle(const R_fill_state_t * pfs, gs_client_color *pcc,
	       floatp x0, floatp y0, floatp x1, floatp y1, floatp x2, floatp y2)
{
    const gs_shading_R_t * const psh = pfs->psh;
    gx_device_color dev_color;
    const gs_color_space *pcs = psh->params.ColorSpace;
    gs_imager_state *pis = pfs->pis;
    gs_fixed_point pts[3];
    int code;
    gx_path *ppath = gx_path_alloc(pis->memory, "R_fill");

    (*pcs->type->restrict_color)(pcc, pcs);
    (*pcs->type->remap_color)(pcc, pcs, &dev_color, pis,
			      pfs->dev, gs_color_select_texture);

    gs_point_transform2fixed(&pfs->pis->ctm, x0, y0, &pts[0]);
    gs_point_transform2fixed(&pfs->pis->ctm, x1, y1, &pts[1]);
    gs_point_transform2fixed(&pfs->pis->ctm, x2, y2, &pts[2]);

    gx_path_add_point(ppath, pts[0].x, pts[0].y);
    gx_path_add_lines(ppath, pts+1, 2);

    code = shade_fill_path((const shading_fill_state_t *)pfs,
			   ppath, &dev_color, &pfs->pis->fill_adjust);

    gx_path_free(ppath, "R_fill");
    return code;
}

private int
R_fill_region(R_fill_state_t * pfs)
{
    const gs_shading_R_t * const psh = pfs->psh;
    gs_function_t *pfn = psh->params.Function;
    R_frame_t *fp = &pfs->frames[pfs->depth - 1];
    gs_fixed_point zero = {0, 0};
    int code;

    code = R_fill_annulus(pfs, &fp->cc[0], fp->t0, fp->t0,
			      psh->params.Coords[2] + pfs->dr * fp->t0,
			      psh->params.Coords[2] + pfs->dr * fp->t0, &pfs->pis->fill_adjust);
    if (code < 0)
	return code;
    code = R_fill_annulus(pfs, &fp->cc[0], fp->t1, fp->t1,
			      psh->params.Coords[2] + pfs->dr * fp->t1,
			      psh->params.Coords[2] + pfs->dr * fp->t1, &pfs->pis->fill_adjust);
    if (code < 0)
	return code;
    for (;;) {
	double t0 = fp->t0, t1 = fp->t1;
	float ft0, ft1;

	if ((!(pfn->head.is_monotonic > 0 ||
	       (ft0 = (float)t0, ft1 = (float)t1,
		gs_function_is_monotonic(pfn, &ft0, &ft1) > 0)) ||
	     !shade_colors2_converge(fp->cc,
				     (const shading_fill_state_t *)pfs)) &&
	    /*
	     * The function isn't monotonic, or the colors don't converge.
	     * Is the annulus less than 1 pixel wide?
	     */
	    pfs->width * (t1 - t0) > 1 &&
	    fp < &pfs->frames[countof(pfs->frames) - 1]
	   ) {
	    /* Subdivide the interval and recur.  */
	    float tm = (t0 + t1) * 0.5;
	    float dm = tm * pfs->dd + psh->params.Domain[0];

	    gs_function_evaluate(pfn, &dm, fp[1].cc[1].paint.values);
	    fp[1].cc[0].paint = fp->cc[0].paint;
	    fp[1].t0 = t0;
	    fp[1].t1 = fp->t0 = tm;
	    fp->cc[0].paint = fp[1].cc[1].paint;
	    ++fp;
	} else {
	    /* Fill the region with the color. */
	    code = R_fill_annulus(pfs, &fp->cc[0], t0, t1,
				      psh->params.Coords[2] + pfs->dr * t0,
				      psh->params.Coords[2] + pfs->dr * t1, &zero);
	    if (code < 0 || fp == &pfs->frames[0])
		return code;
	    --fp;
	}
    }
}

private int 
R_tensor_annulus(patch_fill_state_t *pfs, const gs_rect *rect,
    double x0, double y0, double r0, double t0,
    double x1, double y1, double r1, double t1)
{   
    double dx = x1 - x0, dy = y1 - y0;
    double d = hypot(dx, dy);
    gs_point p0, p1, pc0, pc1;
    int k, j, code;
    bool inside = 0;

    pc0.x = x0, pc0.y = y0; 
    pc1.x = x1, pc1.y = y1;
    if (r0 + d <= r1 || r1 + d <= r0) {
	/* One circle is inside another one. 
	   Use any subdivision, 
	   but don't depend on dx, dy, which may be too small. */
	p0.x = 0, p0.y = -1;
	/* Align stripes along radii for faster triangulation : */
	inside = 1;
    } else {
        /* Must generate canonic quadrangle arcs,
	   because we approximate them with curves. */
	if(any_abs(dx) >= any_abs(dy)) {
	    if (dx > 0)
		p0.x = 0, p0.y = -1;
	    else
		p0.x = 0, p0.y = 1;
	} else {
	    if (dy > 0)
		p0.x = 1, p0.y = 0;
	    else
		p0.x = -1, p0.y = 0;
	}
    }
    /* fixme: wish: cut invisible parts off. 
       Note : when r0 != r1 the invisible part is not a half circle. */
    for (k = 0; k < 4; k++, p0 = p1) {
	gs_point p[12];
	patch_curve_t curve[4];

	p1.x = -p0.y; p1.y = p0.x;
	if ((k & 1) == k >> 1) {
	    make_quadrant_arc(p + 0, &pc0, &p1, &p0, r0);
	    make_quadrant_arc(p + 6, &pc1, &p0, &p1, r1);
	} else {
	    make_quadrant_arc(p + 0, &pc0, &p0, &p1, r0);
	    make_quadrant_arc(p + 6, &pc1, &p1, &p0, r1);
	}
	p[4].x = (p[3].x * 2 + p[6].x) / 3;
	p[4].y = (p[3].y * 2 + p[6].y) / 3;
	p[5].x = (p[3].x + p[6].x * 2) / 3;
	p[5].y = (p[3].y + p[6].y * 2) / 3;
	p[10].x = (p[9].x * 2 + p[0].x) / 3;
	p[10].y = (p[9].y * 2 + p[0].y) / 3;
	p[11].x = (p[9].x + p[0].x * 2) / 3;
	p[11].y = (p[9].y + p[0].y * 2) / 3;
	for (j = 0; j < 4; j++) {
	    int jj = (j + inside) % 4;

	    code = gs_point_transform2fixed(&pfs->pis->ctm, 
			p[j * 3 + 0].x, p[j * 3 + 0].y, &curve[jj].vertex.p);
	    if (code < 0)
		return code;
	    code = gs_point_transform2fixed(&pfs->pis->ctm, 
			p[j * 3 + 1].x, p[j * 3 + 1].y, &curve[jj].control[0]);
	    if (code < 0)
		return code;
	    code = gs_point_transform2fixed(&pfs->pis->ctm, 
			p[j * 3 + 2].x, p[j * 3 + 2].y, &curve[jj].control[1]);
	    if (code < 0)
		return code;
	}
#	if NEW_RADIAL_SHADINGS
	    curve[(0 + inside) % 4].vertex.cc[0] = t0;
	    curve[(1 + inside) % 4].vertex.cc[0] = t0;
	    curve[(2 + inside) % 4].vertex.cc[0] = t1;
	    curve[(3 + inside) % 4].vertex.cc[0] = t1;
#	else
	    curve[0].vertex.cc[0] = curve[1].vertex.cc[0] = 0; /* stub. */
	    curve[2].vertex.cc[0] = curve[3].vertex.cc[0] = 0; /* stub. */
#	endif
	curve[0].vertex.cc[1] = curve[1].vertex.cc[1] = 0; /* Initialize against FPE. */
	curve[2].vertex.cc[1] = curve[3].vertex.cc[1] = 0; /* Initialize against FPE. */
	code = patch_fill(pfs, curve, NULL, NULL);
	if (code < 0)
	    return code;
    }
    return 0;
}


private void
R_outer_circle(patch_fill_state_t *pfs, const gs_rect *rect, 
	double x0, double y0, double r0, 
	double x1, double y1, double r1, 
	double *x2, double *y2, double *r2)
{
    double dx = x1 - x0, dy = y1 - y0;
    double sp, sq, s;

    /* Compute a cone circle, which contacts the rect externally. */
    /* Don't bother with all 4 sides of the rect, 
       just do with the X or Y span only,
       so it's not an exact contact, sorry. */
    if (any_abs(dx) > any_abs(dy)) {
	/* Solving :
	    x0 + (x1 - x0) * s - r0 - (r1 - r0) * s == bbox_x
	    (x1 - x0) * s - (r1 - r0) * s == bbox_x - x0 + r0
	    s = (bbox_x - x0 + r0) / (x1 - x0 - r1 + r0)
	 */
	assert(x1 - x0 + r1 - r0); /* We checked for obtuse cone. */
	sp = (rect->p.x - x0 + r0) / (x1 - x0 - r1 + r0);
	sq = (rect->q.x - x0 + r0) / (x1 - x0 - r1 + r0);
    } else {
	/* Same by Y. */
	sp = (rect->p.y - y0 + r0) / (y1 - y0 - r1 + r0);
	sq = (rect->q.y - y0 + r0) / (y1 - y0 - r1 + r0);
    }
    if (sp >= 1 && sq >= 1)
	s = min(sp, sq);
    else if(sp >= 1)
	s = sp;
    else if (sq >= 1)
	s = sq;
    else {
	/* The circle 1 is outside the rect, use it. */
        s = 1;
    }
    if (r0 + (r1 - r0) * s < 0) {
	/* Passed the cone apex, use the apex. */
	s = r0 / (r0 - r1);
	*r2 = 0;
    } else
	*r2 = r0 + (r1 - r0) * s;
    *x2 = x0 + (x1 - x0) * s;
    *y2 = y0 + (y1 - y0) * s;
}

private double 
R_rect_radius(const gs_rect *rect, double x0, double y0)
{
    double d, dd;

    dd = hypot(rect->p.x - x0, rect->p.y - y0);
    d = hypot(rect->p.x - x0, rect->q.y - y0);
    dd = max(dd, d);
    d = hypot(rect->q.x - x0, rect->q.y - y0);
    dd = max(dd, d);
    d = hypot(rect->q.x - x0, rect->p.y - y0);
    dd = max(dd, d);
    return dd;
}

private int
R_fill_triangle_new(patch_fill_state_t *pfs, const gs_rect *rect, 
    double x0, double y0, double x1, double y1, double x2, double y2, double t)
{
    shading_vertex_t p0, p1, p2;
    int code;

    code = gs_point_transform2fixed(&pfs->pis->ctm, x0, y0, &p0.p);
    if (code < 0)
	return code;
    code = gs_point_transform2fixed(&pfs->pis->ctm, x1, y1, &p1.p);
    if (code < 0)
	return code;
    code = gs_point_transform2fixed(&pfs->pis->ctm, x2, y2, &p2.p);
    if (code < 0)
	return code;
    p0.c.t[0] = p0.c.t[1] = t;
    p1.c.t[0] = p1.c.t[1] = t;
    p2.c.t[0] = p2.c.t[1] = t;
    patch_resolve_color(&p0.c, pfs);
    patch_resolve_color(&p1.c, pfs);
    patch_resolve_color(&p2.c, pfs);
    return mesh_triangle(pfs, &p0, &p1, &p2);
}

private bool 
R_is_covered(double ax, double ay, 
	const gs_point *p0, const gs_point *p1, const gs_point *p)
{
    double dx0 = p0->x - ax, dy0 = p0->y - ay;
    double dx1 = p1->x - ax, dy1 = p1->y - ay;
    double dx = p->x - ax, dy = p->y - ay;
    double vp0 = dx0 * dy - dy0 * dx;
    double vp1 = dx * dy1 - dy * dx1;

    return vp0 >= 0 && vp1 >= 0;
}

private int
R_obtuse_cone(patch_fill_state_t *pfs, const gs_rect *rect,
	double x0, double y0, double r0, 
	double x1, double y1, double r1, double t1, double r)
{
    double dx = x1 - x0, dy = y1 - y0, dr = any_abs(r1 - r0);
    double d = hypot(dx, dy);
    double ax, ay, as; /* Cone apex. */
    gs_point p0, p1; /* Tangent limits. */
    gs_point cp[4]; /* Corners.. */
    gs_point rp[4]; /* Covered corners.. */
    gs_point pb;
    int rp_count = 0, cp_start, i, code;
    bool covered[4];

    as = r0 / (r0 - r1);
    ax = x0 + (x1 - x0) * as;
    ay = y0 + (y1 - y0) * as;

    if (any_abs(d - dr) < 1e-7 * (d + dr)) {
	/* Nearly degenerate, replace with half-plane. */
	p0.x = ax - dy * r / d;
	p0.y = ay + dx * r / d;
	p1.x = ax + dy * r / d;
	p1.y = ay - dx * r / d;
    } else {
	/* Tangent limits by proportional triangles. */
	double da = hypot(ax - x0, ay - y0);
	double h = r * r0 / da, g;

	assert(h <= r);
	g = sqrt(r * r - h * h);
	p0.x = ax - dx * g / d - dy * h / d;
	p0.y = ay - dy * g / d + dx * h / d;
	p1.x = ax - dx * g / d + dy * h / d;
	p1.y = ay - dy * g / d - dx * h / d;
    }
    /* Now we have 2 limited tangents, and 4 corners of the rect. 
       Need to know what corners are covered. */
    cp[0].x = rect->p.x, cp[0].y = rect->p.y;
    cp[1].x = rect->q.x, cp[1].y = rect->p.y;
    cp[2].x = rect->q.x, cp[2].y = rect->q.y;
    cp[3].x = rect->p.x, cp[3].y = rect->q.y;
    covered[0] = R_is_covered(ax, ay, &p0, &p1, &cp[0]);
    covered[1] = R_is_covered(ax, ay, &p0, &p1, &cp[1]);
    covered[2] = R_is_covered(ax, ay, &p0, &p1, &cp[2]);
    covered[3] = R_is_covered(ax, ay, &p0, &p1, &cp[3]);
    if (!covered[0] && !covered[1] && !covered[2] && !covered[3]) {
	return R_fill_triangle_new(pfs, rect, ax, ay, p0.x, p0.y, p1.x, p1.y, t1);
    } 
    if (!covered[0] && covered[1])
	cp_start = 1;
    else if (!covered[1] && covered[2])
	cp_start = 2;
    else if (!covered[2] && covered[3])
	cp_start = 3;
    else if (!covered[3] && covered[0])
	cp_start = 0;
    else {
	/* Must not happen, handle somehow for safety. */
	cp_start = 0;
    }
    for (i = cp_start; i < cp_start + 4 && covered[i % 4]; i++) {
	rp[rp_count] = cp[i % 4];
	rp_count++;
    }
    /* Do paint. */
    pb = p0;
    for (i = 0; i < rp_count; i++) {
	code = R_fill_triangle_new(pfs, rect, ax, ay, pb.x, pb.y, rp[i].x, rp[i].y, t1);
	if (code < 0)
	    return code;
	pb = rp[i];
    }
    return R_fill_triangle_new(pfs, rect, ax, ay, pb.x, pb.y, p1.x, p1.y, t1);
}

private int
R_tensor_cone_apex(patch_fill_state_t *pfs, const gs_rect *rect,
	double x0, double y0, double r0, 
	double x1, double y1, double r1, double t)
{
    double as = r0 / (r0 - r1);
    double ax = x0 + (x1 - x0) * as;
    double ay = y0 + (y1 - y0) * as;

    return R_tensor_annulus(pfs, rect, x1, y1, r1, t, ax, ay, 0, t);
}


private int
R_extensions(patch_fill_state_t *pfs, const gs_shading_R_t *psh, const gs_rect *rect, 
	double t0, double t1, bool Extend0, bool Extend1)
{
    float x0 = psh->params.Coords[0], y0 = psh->params.Coords[1];
    floatp r0 = psh->params.Coords[2];
    float x1 = psh->params.Coords[3], y1 = psh->params.Coords[4];
    floatp r1 = psh->params.Coords[5];
    double dx = x1 - x0, dy = y1 - y0, dr = any_abs(r1 - r0);
    double d = hypot(dx, dy), r;
    int code;

    if (dr >= d - 1e-7 * (d + dr)) {
	/* Nested circles, or degenerate. */
	if (r0 > r1) {
	    if (Extend0) {
		r = R_rect_radius(rect, x0, y0);
		if (r > r0) {
		    code = R_tensor_annulus(pfs, rect, x0, y0, r, t0, x0, y0, r0, t0);
		    if (code < 0)
			return code;
		}
	    }
	    if (Extend1 && r1 > 0)
		return R_tensor_annulus(pfs, rect, x1, y1, r1, t1, x1, y1, 0, t1);
	} else {
	    if (Extend1) {
		r = R_rect_radius(rect, x1, y1);
		if (r > r1) {
		    code = R_tensor_annulus(pfs, rect, x1, y1, r, t1, x1, y1, r1, t1);
		    if (code < 0)
			return code;
		}
	    }
	    if (Extend0 && r0 > 0)
		return R_tensor_annulus(pfs, rect, x0, y0, r0, t0, x0, y0, 0, t0);
	}
    } else if (dr > d / 3) {
	/* Obtuse cone. */
	if (r0 > r1) {
	    if (Extend0) {
		r = R_rect_radius(rect, x0, y0);
		code = R_obtuse_cone(pfs, rect, x0, y0, r0, x1, y1, r1, t0, r);
		if (code < 0)
		    return code;
	    }
	    if (Extend1 && r1 != 0)
		return R_tensor_cone_apex(pfs, rect, x0, y0, r0, x1, y1, r1, t1);
	    return 0;
	} else {
	    if (Extend1) {
		r = R_rect_radius(rect, x1, y1);
		code = R_obtuse_cone(pfs, rect, x1, y1, r1, x0, y0, r0, t1, r);
		if (code < 0)
		    return code;
	    }
	    if (Extend0 && r0 != 0)
		return R_tensor_cone_apex(pfs, rect, x1, y1, r1, x0, y0, r0, t0);
	}
    } else {
	/* Acute cone or cylinder. */
	double x2, y2, r2, x3, y3, r3;

	if (Extend0) {
	    R_outer_circle(pfs, rect, x1, y1, r1, x0, y0, r0, &x3, &y3, &r3);
	    if (x3 != x1 || y3 != y1) {
		code = R_tensor_annulus(pfs, rect, x0, y0, r0, t0, x3, y3, r3, t0);
		if (code < 0)
		    return code;
	    }
	}
	if (Extend1) {
	    R_outer_circle(pfs, rect, x0, y0, r0, x1, y1, r1, &x2, &y2, &r2);
	    if (x2 != x0 || y2 != y0) {
		code = R_tensor_annulus(pfs, rect, x1, y1, r1, t1, x2, y2, r2, t1);
		if (code < 0)
		    return code;
	    }
	}
    }
    return 0;
}


private double
R_compute_radius(floatp x, floatp y, const gs_rect *rect)
{
    double x0 = rect->p.x - x, y0 = rect->p.y - y,
	x1 = rect->q.x - x, y1 = rect->q.y - y;
    double r00 = hypot(x0, y0), r01 = hypot(x0, y1),
	r10 = hypot(x1, y0), r11 = hypot(x1, y1);
    double rm0 = max(r00, r01), rm1 = max(r10, r11);

    return max(rm0, rm1);
}

/*
 * For differnt radii, compute the coords for /Extend option.
 * r0 MUST be greater than r1.
 *
 * The extension is an area which is bounded by the two exterior common
 * tangent of the given circles except the area between the circles.
 *
 * Therefore we can make the extension with the contact points between
 * the tangent lines and circles, and the intersection point of
 * the lines (Note that r0 is greater than r1, therefore the exterior common 
 * tangent for the two circles always intersect at one point.
 * (The case when both radii are same is handled by 'R_compute_extension_bar')
 * 
 * A brief algorithm is following.
 *
 * Let C0, C1 be the given circle with r0, r1 as radii.
 * There exist two contact points for each circles and
 * say them p0, p1 for C0 and q0, q1 for C1.
 *
 * First we compute the intersection point of both tangent lines (isecx, isecy).
 * Then we get the angle between a tangent line and the line which penentrates
 * the centers of circles.
 *
 * Then we can compute 4 contact points between two tangent lines and two circles,
 * and 2 points outside the cliping area on the tangent lines.
 */

private void
R_compute_extension_cone(floatp x0, floatp y0, floatp r0, 
			 floatp x1, floatp y1, floatp r1, 
			 floatp max_ext, floatp coord[7][2])
{
    floatp isecx, isecy;
	floatp dist_c0_isec;
    floatp dist_c1_isec;
	floatp dist_p0_isec;
    floatp dist_q0_isec;
    floatp cost, sint;
    floatp dx0, dy0, dx1, dy1;

    isecx = (x1-x0)*r0 / (r0-r1) + x0;
    isecy = (y1-y0)*r0 / (r0-r1) + y0;

	dist_c0_isec = hypot(x0-isecx, y0-isecy);
    dist_c1_isec = hypot(x1-isecx, y1-isecy);
	dist_p0_isec = sqrt(dist_c0_isec*dist_c0_isec - r0*r0);
	dist_q0_isec = sqrt(dist_c1_isec*dist_c1_isec - r1*r1);    
    cost = dist_p0_isec / dist_c0_isec;
    sint = r0 / dist_c0_isec;

    dx0 = ((x0-isecx)*cost - (y0-isecy)*sint) / dist_c0_isec;
    dy0 = ((x0-isecx)*sint + (y0-isecy)*cost) / dist_c0_isec;
    sint = -sint;
    dx1 = ((x0-isecx)*cost - (y0-isecy)*sint) / dist_c0_isec;
    dy1 = ((x0-isecx)*sint + (y0-isecy)*cost) / dist_c0_isec;

	coord[0][0] = isecx;
	coord[0][1] = isecy;
    coord[1][0] = isecx + dx0 * dist_q0_isec;
    coord[1][1] = isecy + dy0 * dist_q0_isec;
    coord[2][0] = isecx + dx1 * dist_q0_isec;
    coord[2][1] = isecy + dy1 * dist_q0_isec;

    coord[3][0] = isecx + dx0 * dist_p0_isec;
    coord[3][1] = isecy + dy0 * dist_p0_isec;
    coord[4][0] = isecx + dx0 * max_ext;
    coord[4][1] = isecy + dy0 * max_ext;
    coord[5][0] = isecx + dx1 * dist_p0_isec;
    coord[5][1] = isecy + dy1 * dist_p0_isec;
    coord[6][0] = isecx + dx1 * max_ext;
    coord[6][1] = isecy + dy1 * max_ext;
}

/* for same radii, compute the coords for one side extension */
private void
R_compute_extension_bar(floatp x0, floatp y0, floatp x1, 
			floatp y1, floatp radius, 
			floatp max_ext, floatp coord[4][2])
{
    floatp dis;

    dis = hypot(x1-x0, y1-y0);
    coord[0][0] = x0 + (y0-y1) / dis * radius;
    coord[0][1] = y0 - (x0-x1) / dis * radius;
    coord[1][0] = coord[0][0] + (x0-x1) / dis * max_ext;
    coord[1][1] = coord[0][1] + (y0-y1) / dis * max_ext;
    coord[2][0] = x0 - (y0-y1) / dis * radius;
    coord[2][1] = y0 + (x0-x1) / dis * radius;
    coord[3][0] = coord[2][0] + (x0-x1) / dis * max_ext;
    coord[3][1] = coord[2][1] + (y0-y1) / dis * max_ext;
}

private int
gs_shading_R_fill_rectangle_aux(const gs_shading_t * psh0, const gs_rect * rect,
			    gx_device * dev, gs_imager_state * pis)
{
    const gs_shading_R_t *const psh = (const gs_shading_R_t *)psh0;
    R_fill_state_t state;
    gs_client_color rcc[2];
    float d0 = psh->params.Domain[0], d1 = psh->params.Domain[1];
    float dd = d1 - d0;
    float x0 = psh->params.Coords[0], y0 = psh->params.Coords[1];
    floatp r0 = psh->params.Coords[2];
    float x1 = psh->params.Coords[3], y1 = psh->params.Coords[4];
    floatp r1 = psh->params.Coords[5];
    float t[2];
    int i;
    int code;
    float dist_between_circles;
    gs_point dev_dpt;
    gs_point dev_dr;

    shade_init_fill_state((shading_fill_state_t *)&state, psh0, dev, pis);
    state.psh = psh;
    state.rect = *rect;
    /* Compute the parameter range. */
    t[0] = d0;
    t[1] = d1;
    for (i = 0; i < 2; ++i)
	gs_function_evaluate(psh->params.Function, &t[i],
			     rcc[i].paint.values);
    memcpy(state.frames[0].cc, rcc, sizeof(rcc[0]) * 2);
    state.delta.x = x1 - x0;
    state.delta.y = y1 - y0;
    state.dr = r1 - r0;

    /* Now the annulus width is the distance 
     * between two circles in output device unit.
     * This is just used for a conservative check and
     * also pretty crude but now it works for circles
     * with same or not so different radii.
     */
    gs_distance_transform(state.delta.x, state.delta.y, &ctm_only(pis), &dev_dpt);
    gs_distance_transform(state.dr, 0, &ctm_only(pis), &dev_dr);
    
    state.width = hypot(dev_dpt.x, dev_dpt.y) + hypot(dev_dr.x, dev_dr.y);

    dist_between_circles = hypot(x1-x0, y1-y0);

    state.dd = dd;

    if (NEW_RADIAL_SHADINGS) {
	patch_fill_state_t pfs1;

	memcpy(&pfs1, (shading_fill_state_t *)&state , sizeof(shading_fill_state_t));
	pfs1.Function = psh->params.Function;
	code = init_patch_fill_state(&pfs1);
	if (code < 0)
	    return code;
	shade_bbox_transform2fixed(rect, pis, &pfs1.rect);
	pfs1.maybe_self_intersecting = false;

	code = R_extensions(&pfs1, psh, rect, t[0], t[1], psh->params.Extend[0], false);
	if (code < 0)
	    return code;
	{
	    float x0 = psh->params.Coords[0], y0 = psh->params.Coords[1];
	    floatp r0 = psh->params.Coords[2];
	    float x1 = psh->params.Coords[3], y1 = psh->params.Coords[4];
	    floatp r1 = psh->params.Coords[5];
	    
	    code = R_tensor_annulus(&pfs1, rect, x0, y0, r0, t[0], x1, y1, r1, t[1]);
	    if (code < 0)
		return code;
	}
	code = R_extensions(&pfs1, psh, rect, t[0], t[1], false, psh->params.Extend[1]);
    } else {
	/*
	    For a faster painting, we apply fill adjustment to outer annula only.
	    Inner annula are painted with no fill adjustment.
	    It can't cause a dropout, because the outer side of an annulus
	    gets same flattening as the inner side of the neighbour outer annulus.
	 */
	if (psh->params.Extend[0]) {
	    floatp max_extension;
	    gs_point p, q;
	    p = rect->p; q = rect->q;
	    max_extension = hypot(p.x-q.x, p.y-q.y)*2;

	    if (r0 < r1) {
		if ( (r1-r0) < dist_between_circles) {
		    floatp coord[7][2];
		    R_compute_extension_cone(x1, y1, r1, x0, y0, r0, 
					     max_extension, coord);
		    code = R_fill_triangle(&state, &rcc[0], x1, y1, 
					    coord[0][0], coord[0][1], 
					    coord[1][0], coord[1][1]);
		    if (code < 0)
			return code;
		    code = R_fill_triangle(&state, &rcc[0], x1, y1, 
					    coord[0][0], coord[0][1], 
					    coord[2][0], coord[2][1]);
		} else {
		    code = R_fill_annulus(&state, &rcc[0], 0.0, 0.0, 0.0, r0, &pis->fill_adjust);
		}
	    }
	    else if (r0 > r1) {
		if ( (r0-r1) < dist_between_circles) {
		    floatp coord[7][2];
		    R_compute_extension_cone(x0, y0, r0, x1, y1, r1, 
					     max_extension, coord);
		    code = R_fill_triangle(&state, &rcc[0], 
					    coord[3][0], coord[3][1], 
					    coord[4][0], coord[4][1], 
					    coord[6][0], coord[6][1]);
		    if (code < 0)
			return code;
		    code = R_fill_triangle(&state, &rcc[0], 
					    coord[3][0], coord[3][1], 
					    coord[5][0], coord[5][1], 
					    coord[6][0], coord[6][1]);
		} else {
		    code = R_fill_annulus(&state, &rcc[0], 0.0, 0.0, r0,
					  R_compute_radius(x0, y0, rect), &pis->fill_adjust);
		}
	    } else { /* equal radii */
		floatp coord[4][2];
		R_compute_extension_bar(x0, y0, x1, y1, r0, max_extension, coord);
		code = R_fill_triangle(&state, &rcc[0], 
					coord[0][0], coord[0][1], 
					coord[1][0], coord[1][1], 
					coord[2][0], coord[2][1]);
		if (code < 0)
		    return code;
		code = R_fill_triangle(&state, &rcc[0], 
					coord[2][0], coord[2][1], 
					coord[3][0], coord[3][1], 
					coord[1][0], coord[1][1]);
	    }
	    if (code < 0)
		return code;
	}

	state.depth = 1;
	state.frames[0].t0 = (t[0] - d0) / dd;
	state.frames[0].t1 = (t[1] - d0) / dd;
        code = R_fill_region(&state);
	if (psh->params.Extend[1]) {
	    floatp max_extension;
	    gs_point p, q;
	    p = rect->p; q = rect->q;
	    max_extension = hypot(p.x-q.x, p.y-q.y)*2;

	    if (code < 0)
		return code;
	    if (r0 < r1) {
		if ( (r1-r0) < dist_between_circles) {
		    floatp coord[7][2];
		    R_compute_extension_cone(x1, y1, r1, x0, y0, r0, 
					     max_extension, coord);
		    code = R_fill_triangle(&state, &rcc[1], 
					    coord[3][0], coord[3][1], 
					    coord[4][0], coord[4][1], 
					    coord[6][0], coord[6][1]);
		    if (code < 0)
			return code;
		    code = R_fill_triangle(&state, &rcc[1], 
					    coord[3][0], coord[3][1], 
					    coord[5][0], coord[5][1], 
					    coord[6][0], coord[6][1]);
		    if (code < 0)
			return code;
		    code = R_fill_annulus(&state, &rcc[1], 1.0, 1.0, 0.0, r1, &pis->fill_adjust);
		} else {
		    code = R_fill_annulus(&state, &rcc[1], 1.0, 1.0, r1,
					  R_compute_radius(x1, y1, rect), &pis->fill_adjust);
		}
	    }
	    else if (r0 > r1)
	    {
		if ( (r0-r1) < dist_between_circles) {
		    floatp coord[7][2];
		    R_compute_extension_cone(x0, y0, r0, x1, y1, r1, 
					     max_extension, coord);
		    code = R_fill_triangle(&state, &rcc[1], x1, y1, 
					    coord[0][0], coord[0][1], 
					    coord[1][0], coord[1][1]);
		    if (code < 0)
			return code;
		    code = R_fill_triangle(&state, &rcc[1], x1, y1, 
					    coord[0][0], coord[0][1], 
					    coord[2][0], coord[2][1]);
		}
		code = R_fill_annulus(&state, &rcc[1], 1.0, 1.0, 0.0, r1, &pis->fill_adjust);
	    } else { /* equal radii */
		floatp coord[4][2];
		R_compute_extension_bar(x1, y1, x0, y0, r0, max_extension, coord);
		code = R_fill_triangle(&state, &rcc[1], 
					coord[0][0], coord[0][1], 
					coord[1][0], coord[1][1], 
					coord[2][0], coord[2][1]);
		if (code < 0)
		    return code;
		code = R_fill_triangle(&state, &rcc[1], 
					coord[2][0], coord[2][1], 
					coord[3][0], coord[3][1], 
					coord[1][0], coord[1][1]);
		if (code < 0)
		    return code;
		code = R_fill_annulus(&state, &rcc[1], 1.0, 1.0, 0.0, r1, &pis->fill_adjust);
	    }
	}
    }
    return code;
}

int
gs_shading_R_fill_rectangle(const gs_shading_t * psh0, const gs_rect * rect,
			    gx_device * dev, gs_imager_state * pis)
{   
    int code;

    if (VD_TRACE_RADIAL_PATCH && vd_allowed('s')) {
	vd_get_dc('s');
	vd_set_shift(0, 0);
	vd_set_scale(0.01);
	vd_set_origin(0, 0);
    }
    code = gs_shading_R_fill_rectangle_aux(psh0, rect, dev, pis);
    if (VD_TRACE_FUNCTIONAL_PATCH && vd_allowed('s'))
	vd_release_dc;
    return code;
}
