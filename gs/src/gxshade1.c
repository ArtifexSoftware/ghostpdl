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
#include "gsptype2.h"
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

/* ================ Specific shadings ================ */

/* ---------------- Function-based shading ---------------- */

typedef struct Fb_frame_s {	/* A rudiment of old code. */
    gs_rect region;
    gs_client_color cc[4];	/* colors at 4 corners */
    int state;
} Fb_frame_t;

typedef struct Fb_fill_state_s {
    shading_fill_state_common;
    const gs_shading_Fb_t *psh;
    gs_matrix_fixed ptm;	/* parameter space -> device space */
    Fb_frame_t frame;
} Fb_fill_state_t;
/****** NEED GC DESCRIPTOR ******/

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
	curve[i].straight = true;
    }
}

private int
Fb_fill_region(Fb_fill_state_t * pfs, const gs_fixed_rect *rect)
{
    patch_fill_state_t pfs1;
    patch_curve_t curve[4];
    Fb_frame_t * fp = &pfs->frame;
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
    pfs1.rect = *rect;
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

int
gs_shading_Fb_fill_rectangle(const gs_shading_t * psh0, const gs_rect * rect, 
			     const gs_fixed_rect * rect_clip,
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
				 state.frame.cc[yi * 2 + xi].paint.values);
	}
    state.frame.region.p.x = x[0];
    state.frame.region.p.y = y[0];
    state.frame.region.q.x = x[1];
    state.frame.region.q.y = y[1];
    return Fb_fill_region(&state, rect_clip);
}

/* ---------------- Axial shading ---------------- */

typedef struct A_fill_state_s {
    shading_fill_state_common;
    const gs_shading_A_t *psh;
    gs_rect rect;		/* bounding rectangle in user space */
    gs_point delta;
    double length;
    double t0, t1;
    double v0, v1, u0, u1;
} A_fill_state_t;
/****** NEED GC DESCRIPTOR ******/

/* Note t0 and t1 vary over [0..1], not the Domain. */

private int
A_fill_region(A_fill_state_t * pfs, const gs_fixed_rect *rect_clip)
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
    pfs1.rect = *rect_clip;
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

private inline int
gs_shading_A_fill_rectangle_aux(const gs_shading_t * psh0, const gs_rect * rect,
			    const gs_fixed_rect *clip_rect,
			    gx_device * dev, gs_imager_state * pis)
{
    const gs_shading_A_t *const psh = (const gs_shading_A_t *)psh0;
    gs_matrix cmat;
    gs_rect t_rect;
    A_fill_state_t state;
    gs_client_color rcc[2];
    float d0 = psh->params.Domain[0], d1 = psh->params.Domain[1];
    float dd = d1 - d0;
    double t0, t1;
    gs_point dist;
    int code = 0;

    shade_init_fill_state((shading_fill_state_t *)&state, psh0, dev, pis);
    state.psh = psh;
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
    t0 = min(max(t_rect.p.y, 0), 1);
    t1 = max(min(t_rect.q.y, 1), 0);
    state.v0 = t0;
    state.v1 = t1;
    state.u0 = t_rect.p.x;
    state.u1 = t_rect.q.x;
    state.t0 = t0 * dd + d0;
    state.t1 = t1 * dd + d0;
    gs_distance_transform(state.delta.x, state.delta.y, &ctm_only(pis),
			  &dist);
    state.length = hypot(dist.x, dist.y);	/* device space line length */
    code = A_fill_region(&state, clip_rect);
    if (psh->params.Extend[0] && t0 > t_rect.p.y) {
	if (code < 0)
	    return code;
	/* Use the general algorithm, because we need the trapping. */
	state.v0 = t_rect.p.y;
	state.v1 = t0;
	state.t0 = state.t1 = t0 * dd + d0;
	code = A_fill_region(&state, clip_rect);
    }
    if (psh->params.Extend[1] && t1 < t_rect.q.y) {
	if (code < 0)
	    return code;
	/* Use the general algorithm, because we need the trapping. */
	state.v0 = t1;
	state.v1 = t_rect.q.y;
	state.t0 = state.t1 = t1 * dd + d0;
	code = A_fill_region(&state, clip_rect);
    }
    return code;
}

int
gs_shading_A_fill_rectangle(const gs_shading_t * psh0, const gs_rect * rect,
			    const gs_fixed_rect * rect_clip,
			    gx_device * dev, gs_imager_state * pis)
{
    int code;

    if (VD_TRACE_AXIAL_PATCH && vd_allowed('s')) {
	vd_get_dc('s');
	vd_set_shift(0, 0);
	vd_set_scale(0.01);
	vd_set_origin(0, 0);
    }
    code = gs_shading_A_fill_rectangle_aux(psh0, rect, rect_clip, dev, pis);
    if (VD_TRACE_AXIAL_PATCH && vd_allowed('s'))
	vd_release_dc;
    return code;
}

/* ---------------- Radial shading ---------------- */

typedef struct R_frame_s {	/* A rudiment of old code. */
    double t0, t1;
    gs_client_color cc[2];	/* color at t0, t1 */
} R_frame_t;

typedef struct R_fill_state_s {
    shading_fill_state_common;
    const gs_shading_R_t *psh;
    gs_rect rect;
    gs_point delta;
    double dr, dd;
    R_frame_t frame;
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
	    curve[j].straight = ((j & 1) != 0);
	}
	curve[(0 + inside) % 4].vertex.cc[0] = t0;
	curve[(1 + inside) % 4].vertex.cc[0] = t0;
	curve[(2 + inside) % 4].vertex.cc[0] = t1;
	curve[(3 + inside) % 4].vertex.cc[0] = t1;
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
			    const gs_fixed_rect *clip_rect,
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
    patch_fill_state_t pfs1;

    shade_init_fill_state((shading_fill_state_t *)&state, psh0, dev, pis);
    state.psh = psh;
    state.rect = *rect;
    /* Compute the parameter range. */
    t[0] = d0;
    t[1] = d1;
    for (i = 0; i < 2; ++i)
	gs_function_evaluate(psh->params.Function, &t[i],
			     rcc[i].paint.values);
    memcpy(state.frame.cc, rcc, sizeof(rcc[0]) * 2);
    state.delta.x = x1 - x0;
    state.delta.y = y1 - y0;
    state.dr = r1 - r0;

    gs_distance_transform(state.delta.x, state.delta.y, &ctm_only(pis), &dev_dpt);
    gs_distance_transform(state.dr, 0, &ctm_only(pis), &dev_dr);
    
    dist_between_circles = hypot(x1-x0, y1-y0);

    state.dd = dd;
    memcpy(&pfs1, (shading_fill_state_t *)&state , sizeof(shading_fill_state_t));
    pfs1.Function = psh->params.Function;
    code = init_patch_fill_state(&pfs1);
    if (code < 0)
	return code;
    pfs1.rect = *clip_rect;
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
    return R_extensions(&pfs1, psh, rect, t[0], t[1], false, psh->params.Extend[1]);
}

int
gs_shading_R_fill_rectangle(const gs_shading_t * psh0, const gs_rect * rect,
			    const gs_fixed_rect * rect_clip,
			    gx_device * dev, gs_imager_state * pis)
{   
    int code;

    if (VD_TRACE_RADIAL_PATCH && vd_allowed('s')) {
	vd_get_dc('s');
	vd_set_shift(0, 0);
	vd_set_scale(0.01);
	vd_set_origin(0, 0);
    }
    code = gs_shading_R_fill_rectangle_aux(psh0, rect, rect_clip, dev, pis);
    if (VD_TRACE_FUNCTIONAL_PATCH && vd_allowed('s'))
	vd_release_dc;
    return code;
}
