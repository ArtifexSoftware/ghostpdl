/* Copyright (C) 1998, 1999 Aladdin Enterprises.  All rights reserved.
  
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
/* Rendering for Coons and tensor patch shadings */
#include "memory_.h"
#include "gx.h"
#include "gserrors.h"
#include "gsmatrix.h"		/* for gscoord.h */
#include "gscoord.h"
#include "gxcspace.h"
#include "gxdcolor.h"
#include "gxistate.h"
#include "gxshade.h"
#include "gxshade4.h"
#include "gxdevcli.h"
#include "gzpath.h"
#include "stdint_.h"
#include "vdtrace.h"

#define NEW_TENSOR_SHADING 0 /* Old code = 0, new code = 1. */

/* ================ Utilities ================ */

/* Define one segment (vertex and next control points) of a curve. */
typedef struct patch_curve_s {
    mesh_vertex_t vertex;
    gs_fixed_point control[2];
} patch_curve_t;

/* Get colors for patch vertices. */
private int
shade_next_colors(shade_coord_stream_t * cs, patch_curve_t * curves,
		  int num_vertices)
{
    int i, code = 0;

    for (i = 0; i < num_vertices && code >= 0; ++i)
	code = shade_next_color(cs, curves[i].vertex.cc);
    return code;
}

/* Get a Bezier or tensor patch element. */
private int
shade_next_curve(shade_coord_stream_t * cs, patch_curve_t * curve)
{
    int code = shade_next_coords(cs, &curve->vertex.p, 1);

    if (code >= 0)
	code = shade_next_coords(cs, curve->control,
				 countof(curve->control));
    return code;
}

/* Define a color to be used in curve rendering. */
/* This may be a real client color, or a parametric function argument. */
typedef struct patch_color_s {
    float t;			/* parametric value */
    gs_client_color cc;
} patch_color_t;

/*
 * Parse the next patch out of the input stream.  Return 1 if done,
 * 0 if patch, <0 on error.
 */
private int
shade_next_patch(shade_coord_stream_t * cs, int BitsPerFlag,
patch_curve_t curve[4], gs_fixed_point interior[4] /* 0 for Coons patch */ )
{
    int flag = shade_next_flag(cs, BitsPerFlag);
    int num_colors, code;

    if (flag < 0)
	return 1;		/* no more data */
    switch (flag & 3) {
	default:
	    return_error(gs_error_rangecheck);	/* not possible */
	case 0:
	    if ((code = shade_next_curve(cs, &curve[0])) < 0 ||
		(code = shade_next_coords(cs, &curve[1].vertex.p, 1)) < 0
		)
		return code;
	    num_colors = 4;
	    goto vx;
	case 1:
	    curve[0] = curve[1], curve[1].vertex = curve[2].vertex;
	    goto v3;
	case 2:
	    curve[0] = curve[2], curve[1].vertex = curve[3].vertex;
	    goto v3;
	case 3:
	    curve[1].vertex = curve[0].vertex, curve[0] = curve[3];
v3:	    num_colors = 2;
vx:	    if ((code = shade_next_coords(cs, curve[1].control, 2)) < 0 ||
		(code = shade_next_curve(cs, &curve[2])) < 0 ||
		(code = shade_next_curve(cs, &curve[3])) < 0 ||
		(interior != 0 &&
		 (code = shade_next_coords(cs, interior, 4)) < 0) ||
		(code = shade_next_colors(cs, &curve[4 - num_colors],
					  num_colors)) < 0
		)
		return code;
    }
    return 0;
}

/* Define the common state for rendering Coons and tensor patches. */
typedef struct patch_fill_state_s {
    mesh_fill_state_common;
    const gs_function_t *Function;
#if NEW_TENSOR_SHADING
    bool vectorization;
    gs_fixed_point *wedge_buf;
    gs_client_color color_domain;
    fixed fixed_flat;
#endif
} patch_fill_state_t;

#if NEW_TENSOR_SHADING
private void
init_patch_fill_state(patch_fill_state_t *pfs)
{
    const gs_color_space *pcs = pfs->direct_space;
    gs_client_color fcc0, fcc1;
    int n = pcs->type->num_components(pcs), i;
    double m;

    for (i = 0; i < n; i++) {
	fcc0.paint.values[i] = -1000000;
	fcc1.paint.values[i] = 1000000;
    }
    pcs->type->restrict_color(&fcc0, pcs);
    pcs->type->restrict_color(&fcc1, pcs);
    for (i = 0; i < n; i++)
	pfs->color_domain.paint.values[i] = max(fcc1.paint.values[i] - fcc0.paint.values[i], 1);
    pfs->vectorization = false; /* A stub for a while. Will use with pclwrite. */
    pfs->wedge_buf = NULL;
    pfs->fixed_flat = float2fixed(pfs->pis->flatness);
}
#endif

/*
 * Calculate the interpolated color at a given point.
 * Note that we must do this twice for bilinear interpolation.
 * We use the name ppcr rather than ppc because an Apple compiler defines
 * ppc when compiling on PowerPC systems (!).
 */
private void
patch_interpolate_color(patch_color_t * ppcr, const patch_color_t * ppc0,
       const patch_color_t * ppc1, const patch_fill_state_t * pfs, floatp t)
{
    if (pfs->Function)
	ppcr->t = ppc0->t + t * (ppc1->t - ppc0->t);
    else {
	int ci;

	for (ci = pfs->num_components - 1; ci >= 0; --ci)
	    ppcr->cc.paint.values[ci] =
		ppc0->cc.paint.values[ci] +
		t * (ppc1->cc.paint.values[ci] - ppc0->cc.paint.values[ci]);
    }
}

/* Resolve a patch color using the Function if necessary. */
inline private void
patch_resolve_color(patch_color_t * ppcr, const patch_fill_state_t * pfs)
{
    if (pfs->Function)
	gs_function_evaluate(pfs->Function, &ppcr->t, ppcr->cc.paint.values);
}

/* ================ Specific shadings ================ */

/*
 * The curves are stored in a clockwise or counter-clockwise order that maps
 * to the patch definition in a non-intuitive way.  The documentation
 * (pp. 277-281 of the PostScript Language Reference Manual, Third Edition)
 * says that the curves are in the order D1, C2, D2, C1.
 */
/* The starting points of the curves: */
#define D1START 0
#define C2START 1
#define D2START 3
#define C1START 0
/* The control points of the curves (x means reversed order): */
#define D1CTRL 0
#define C2CTRL 1
#define D2XCTRL 2
#define C1XCTRL 3
/* The end points of the curves: */
#define D1END 1
#define C2END 2
#define D2END 2
#define C1END 3

/* ---------------- Common code ---------------- */

/* Evaluate a curve at a given point. */
private void
curve_eval(gs_fixed_point * pt, const gs_fixed_point * p0,
	   const gs_fixed_point * p1, const gs_fixed_point * p2,
	   const gs_fixed_point * p3, floatp t)
{
    fixed a, b, c, d;
    fixed t01, t12;

    d = p0->x;
    curve_points_to_coefficients(d, p1->x, p2->x, p3->x,
				 a, b, c, t01, t12);
    pt->x = (fixed) (((a * t + b) * t + c) * t + d);
    d = p0->y;
    curve_points_to_coefficients(d, p1->y, p2->y, p3->y,
				 a, b, c, t01, t12);
    pt->y = (fixed) (((a * t + b) * t + c) * t + d);
    if_debug3('2', "[2]t=%g => (%g,%g)\n", t, fixed2float(pt->x),
	      fixed2float(pt->y));
}

/*
 * Merge two arrays of splits, sorted in increasing order.
 * Return the number of entries in the result, which might be less than
 * n1 + n2 (if an a1 entry is equal to an a2 entry).
 * a1 or a2 may overlap out as long as a1 - out >= n2 or a2 - out >= n1
 * respectively.
 */
private int
merge_splits(double *out, const double *a1, int n1, const double *a2, int n2)
{
    double *p = out;
    int i1 = 0, i2 = 0;

    /*
     * We would like to write the body of the loop as an assignement
     * with a conditional expression on the right, but gcc 2.7.2.3
     * generates incorrect code if we do this.
     */
    while (i1 < n1 || i2 < n2)
	if (i1 == n1)
	    *p++ = a2[i2++];
	else if (i2 == n2 || a1[i1] < a2[i2])
	    *p++ = a1[i1++];
	else if (a1[i1] > a2[i2])
	    *p++ = a2[i2++];
	else
	    i1++, *p++ = a2[i2++];
    return p - out;
}

/*
 * Split a curve in both X and Y.  Return the number of split points.
 * swap = 0 if the control points are in order, 1 if reversed.
 */
private int
split_xy(double out[4], const gs_fixed_point *p0, const gs_fixed_point *p1,
	 const gs_fixed_point *p2, const gs_fixed_point *p3)
{
    double tx[2], ty[2];

    return merge_splits(out, tx,
			gx_curve_monotonic_points(p0->x, p1->x, p2->x, p3->x,
						  tx),
			ty,
			gx_curve_monotonic_points(p0->y, p1->y, p2->y, p3->y,
						  ty));
}

/*
 * Compute the joint split points of 2 curves.
 * swap = 0 if the control points are in order, 1 if reversed.
 * Return the number of split points.
 */
inline private int
split2_xy(double out[8], const gs_fixed_point *p10, const gs_fixed_point *p11,
	  const gs_fixed_point *p12, const gs_fixed_point *p13,
	  const gs_fixed_point *p20, const gs_fixed_point *p21,
	  const gs_fixed_point *p22, const gs_fixed_point *p23)
{
    double t1[4], t2[4];

    return merge_splits(out, t1, split_xy(t1, p10, p11, p12, p13),
			t2, split_xy(t2, p20, p21, p22, p23));
}

#if NEW_TENSOR_SHADING
private int patch_fill(patch_fill_state_t * pfs, const patch_curve_t curve[4],
	   const gs_fixed_point interior[4],
	   void (*transform) (gs_fixed_point *, const patch_curve_t[4],
			      const gs_fixed_point[4], floatp, floatp));
#endif

#if !NEW_TENSOR_SHADING

private int
patch_fill(patch_fill_state_t * pfs, const patch_curve_t curve[4],
	   const gs_fixed_point interior[4],
	   void (*transform) (gs_fixed_point *, const patch_curve_t[4],
			      const gs_fixed_point[4], floatp, floatp))
{	/*
	 * The specification says the output must appear to be produced in
	 * order of increasing values of v, and for equal v, in order of
	 * increasing u.  However, all we actually have to do is follow this
	 * order with respect to sub-patches that might overlap, which can
	 * only occur as a result of non-monotonic curves; we can render
	 * each monotonic sub-patch in any order we want.  Therefore, we
	 * begin by breaking up the patch into pieces that are monotonic
	 * with respect to all 4 edges.  Since each edge has no more than
	 * 2 X and 2 Y split points (for a total of 4), taking both edges
	 * together we have a maximum of 8 split points for each axis.
	 */
    double su[9], sv[9];
    int nu = split2_xy(su, &curve[C1START].vertex.p,&curve[C1XCTRL].control[1],
		       &curve[C1XCTRL].control[0], &curve[C1END].vertex.p,
		       &curve[C2START].vertex.p, &curve[C2CTRL].control[0],
		       &curve[C2CTRL].control[1], &curve[C2END].vertex.p);
    int nv = split2_xy(sv, &curve[D1START].vertex.p, &curve[D1CTRL].control[0],
		       &curve[D1CTRL].control[1], &curve[D1END].vertex.p,
		       &curve[D2START].vertex.p, &curve[D2XCTRL].control[1],
		       &curve[D2XCTRL].control[0], &curve[D2END].vertex.p);
    int iu, iv, ju, jv, ku, kv;
    double du, dv;
    double v0, v1, vn, u0, u1, un;
    patch_color_t c00, c01, c10, c11;
    /*
     * At some future time, we should set check = false if the curves
     * fall entirely within the bounding rectangle.  (Only a small
     * performance optimization, to avoid making this check for each
     * triangle.)
     */
    bool check = true;

#ifdef DEBUG
    if (gs_debug_c('2')) {
	int k;

	dlputs("[2]patch curves:\n");
	for (k = 0; k < 4; ++k)
	    dprintf6("        (%g,%g) (%g,%g)(%g,%g)\n",
		     fixed2float(curve[k].vertex.p.x),
		     fixed2float(curve[k].vertex.p.y),
		     fixed2float(curve[k].control[0].x),
		     fixed2float(curve[k].control[0].y),
		     fixed2float(curve[k].control[1].x),
		     fixed2float(curve[k].control[1].y));
	if (nu > 1) {
	    dlputs("[2]Splitting u");
	    for (k = 0; k < nu; ++k)
		dprintf1(", %g", su[k]);
	    dputs("\n");
	}
	if (nv > 1) {
	    dlputs("[2]Splitting v");
	    for (k = 0; k < nv; ++k)
		dprintf1(", %g", sv[k]);
	    dputs("\n");
	}
    }
#endif
    /* Add boundary values to simplify the iteration. */
    su[nu] = 1;
    sv[nv] = 1;

    /*
     * We're going to fill the curves by flattening them and then filling
     * the resulting triangles.  Start by computing the number of
     * segments required for flattening each side of the patch.
     */
    {
	fixed flatness = float2fixed(pfs->pis->flatness);
	int i;
	int log2_k[4];

	for (i = 0; i < 4; ++i) {
	    curve_segment cseg;

	    cseg.p1 = curve[i].control[0];
	    cseg.p2 = curve[i].control[1];
	    cseg.pt = curve[(i + 1) & 3].vertex.p;
	    log2_k[i] =
		gx_curve_log2_samples(curve[i].vertex.p.x, curve[i].vertex.p.y,
				      &cseg, flatness);
	}
	ku = 1 << max(log2_k[1], log2_k[3]);
	kv = 1 << max(log2_k[0], log2_k[2]);
    }

    /* Precompute the colors at the corners. */

#define PATCH_SET_COLOR(c, v)\
  if ( pfs->Function ) c.t = v.cc[0];\
  else memcpy(c.cc.paint.values, v.cc, sizeof(c.cc.paint.values))

    PATCH_SET_COLOR(c00, curve[D1START].vertex); /* = C1START */
    PATCH_SET_COLOR(c01, curve[D1END].vertex); /* = C2START */
    PATCH_SET_COLOR(c11, curve[C2END].vertex); /* = D2END */
    PATCH_SET_COLOR(c10, curve[C1END].vertex); /* = D2START */

#undef PATCH_SET_COLOR

    /*
     * Since ku and kv are powers of 2, and since log2(k) is surely less
     * than the number of bits in the mantissa of a double, 1/k ...
     * (k-1)/k can all be represented exactly as doubles.
     */
    du = 1.0 / ku;
    dv = 1.0 / kv;

    /* Now iterate over the sub-patches. */
    for (iv = 0, jv = 0, v0 = 0, v1 = vn = dv; jv < kv; v0 = v1, v1 = vn) {
	patch_color_t c0v0, c0v1, c1v0, c1v1;

	/* Subdivide the interval if it crosses a split point. */

#define CHECK_SPLIT(ix, jx, x1, xn, dx, ax)\
  if (x1 > ax[ix])\
      x1 = ax[ix++];\
  else {\
      xn += dx;\
      jx++;\
      if (x1 == ax[ix])\
	  ix++;\
  }

	CHECK_SPLIT(iv, jv, v1, vn, dv, sv);

	patch_interpolate_color(&c0v0, &c00, &c01, pfs, v0);
	patch_interpolate_color(&c0v1, &c00, &c01, pfs, v1);
	patch_interpolate_color(&c1v0, &c10, &c11, pfs, v0);
	patch_interpolate_color(&c1v1, &c10, &c11, pfs, v1);

	for (iu = 0, ju = 0, u0 = 0, u1 = un = du; ju < ku; u0 = u1, u1 = un) {
	    patch_color_t cu0v0, cu1v0, cu0v1, cu1v1;
	    int code;

	    CHECK_SPLIT(iu, ju, u1, un, du, su);

#undef CHECK_SPLIT

	    patch_interpolate_color(&cu0v0, &c0v0, &c1v0, pfs, u0);
	    patch_resolve_color(&cu0v0, pfs);
	    patch_interpolate_color(&cu1v0, &c0v0, &c1v0, pfs, u1);
	    patch_resolve_color(&cu1v0, pfs);
	    patch_interpolate_color(&cu0v1, &c0v1, &c1v1, pfs, u0);
	    patch_resolve_color(&cu0v1, pfs);
	    patch_interpolate_color(&cu1v1, &c0v1, &c1v1, pfs, u1);
	    patch_resolve_color(&cu1v1, pfs);
	    if_debug6('2', "[2]u[%d]=[%g .. %g], v[%d]=[%g .. %g]\n",
		      iu, u0, u1, iv, v0, v1);

	    /* Fill the sub-patch given by ((u0,v0),(u1,v1)). */
	    {
		mesh_vertex_t mu0v0, mu1v0, mu1v1, mu0v1;

		(*transform)(&mu0v0.p, curve, interior, u0, v0);
		(*transform)(&mu1v0.p, curve, interior, u1, v0);
		(*transform)(&mu1v1.p, curve, interior, u1, v1);
		(*transform)(&mu0v1.p, curve, interior, u0, v1);
		if_debug4('2', "[2]  => (%g,%g), (%g,%g),\n",
			  fixed2float(mu0v0.p.x), fixed2float(mu0v0.p.y),
			  fixed2float(mu1v0.p.x), fixed2float(mu1v0.p.y));
		if_debug4('2', "[2]     (%g,%g), (%g,%g)\n",
			  fixed2float(mu1v1.p.x), fixed2float(mu1v1.p.y),
			  fixed2float(mu0v1.p.x), fixed2float(mu0v1.p.y));
		memcpy(mu0v0.cc, cu0v0.cc.paint.values, sizeof(mu0v0.cc));
		memcpy(mu1v0.cc, cu1v0.cc.paint.values, sizeof(mu1v0.cc));
		memcpy(mu1v1.cc, cu1v1.cc.paint.values, sizeof(mu1v1.cc));
		memcpy(mu0v1.cc, cu0v1.cc.paint.values, sizeof(mu0v1.cc));
/* Make this a procedure later.... */
#define FILL_TRI(pva, pvb, pvc)\
  BEGIN\
    mesh_init_fill_triangle((mesh_fill_state_t *)pfs, pva, pvb, pvc, check);\
    code = mesh_fill_triangle((mesh_fill_state_t *)pfs);\
    if (code < 0)\
	return code;\
  END
#if 0
		FILL_TRI(&mu0v0, &mu1v1, &mu1v0);
		FILL_TRI(&mu0v0, &mu1v1, &mu0v1);
#else
		{
		    mesh_vertex_t mmid;
		    int ci;

		    (*transform)(&mmid.p, curve, interior,
				 (u0 + u1) * 0.5, (v0 + v1) * 0.5);
		    for (ci = 0; ci < pfs->num_components; ++ci)
			mmid.cc[ci] =
			    (mu0v0.cc[ci] + mu1v0.cc[ci] +
			     mu1v1.cc[ci] + mu0v1.cc[ci]) * 0.25;
		    FILL_TRI(&mu0v0, &mu1v0, &mmid);
		    FILL_TRI(&mu1v0, &mu1v1, &mmid);
		    FILL_TRI(&mu1v1, &mu0v1, &mmid);
		    FILL_TRI(&mu0v1, &mu0v0, &mmid);
		}
#endif
	    }
	}
    }
    return 0;
}
#endif

/* ---------------- Coons patch shading ---------------- */

/* Calculate the device-space coordinate corresponding to (u,v). */
private void
Cp_transform(gs_fixed_point * pt, const patch_curve_t curve[4],
	     const gs_fixed_point ignore_interior[4], floatp u, floatp v)
{
    double co_u = 1.0 - u, co_v = 1.0 - v;
    gs_fixed_point c1u, d1v, c2u, d2v;

    curve_eval(&c1u, &curve[C1START].vertex.p,
	       &curve[C1XCTRL].control[1], &curve[C1XCTRL].control[0],
	       &curve[C1END].vertex.p, u);
    curve_eval(&d1v, &curve[D1START].vertex.p,
	       &curve[D1CTRL].control[0], &curve[D1CTRL].control[1],
	       &curve[D1END].vertex.p, v);
    curve_eval(&c2u, &curve[C2START].vertex.p,
	       &curve[C2CTRL].control[0], &curve[C2CTRL].control[1],
	       &curve[C2END].vertex.p, u);
    curve_eval(&d2v, &curve[D2START].vertex.p,
	       &curve[D2XCTRL].control[1], &curve[D2XCTRL].control[0],
	       &curve[D2END].vertex.p, v);
#define COMPUTE_COORD(xy)\
    pt->xy = (fixed)\
	((co_v * c1u.xy + v * c2u.xy) + (co_u * d1v.xy + u * d2v.xy) -\
	 (co_v * (co_u * curve[C1START].vertex.p.xy +\
		  u * curve[C1END].vertex.p.xy) +\
	  v * (co_u * curve[C2START].vertex.p.xy +\
	       u * curve[C2END].vertex.p.xy)))
    COMPUTE_COORD(x);
    COMPUTE_COORD(y);
#undef COMPUTE_COORD
    if_debug4('2', "[2](u=%g,v=%g) => (%g,%g)\n",
	      u, v, fixed2float(pt->x), fixed2float(pt->y));
}

int
gs_shading_Cp_fill_rectangle(const gs_shading_t * psh0, const gs_rect * rect,
			     gx_device * dev, gs_imager_state * pis)
{
    const gs_shading_Cp_t * const psh = (const gs_shading_Cp_t *)psh0;
    patch_fill_state_t state;
    shade_coord_stream_t cs;
    patch_curve_t curve[4];
    int code;

    mesh_init_fill_state((mesh_fill_state_t *) & state,
			 (const gs_shading_mesh_t *)psh0, rect, dev, pis);
    state.Function = psh->params.Function;
#if NEW_TENSOR_SHADING
    init_patch_fill_state(&state);
#endif
    shade_next_init(&cs, (const gs_shading_mesh_params_t *)&psh->params,
		    pis);
    while ((code = shade_next_patch(&cs, psh->params.BitsPerFlag,
				    curve, NULL)) == 0 &&
	   (code = patch_fill(&state, curve, NULL, Cp_transform)) >= 0
	) {
	DO_NOTHING;
	#if NEW_TENSOR_SHADING
	    break; /* Temporary disabled for a debug purpose. */
	#endif
    }
    return min(code, 0);
}

/* ---------------- Tensor product patch shading ---------------- */

/* Calculate the device-space coordinate corresponding to (u,v). */
private void
Tpp_transform(gs_fixed_point * pt, const patch_curve_t curve[4],
	      const gs_fixed_point interior[4], floatp u, floatp v)
{
    double Bu[4], Bv[4];
    gs_fixed_point pts[4][4];
    int i, j;
    double x = 0, y = 0;

    /* Compute the Bernstein polynomials of u and v. */
    {
	double u2 = u * u, co_u = 1.0 - u, co_u2 = co_u * co_u;
	double v2 = v * v, co_v = 1.0 - v, co_v2 = co_v * co_v;

	Bu[0] = co_u * co_u2, Bu[1] = 3 * u * co_u2,
	    Bu[2] = 3 * u2 * co_u, Bu[3] = u * u2;
	Bv[0] = co_v * co_v2, Bv[1] = 3 * v * co_v2,
	    Bv[2] = 3 * v2 * co_v, Bv[3] = v * v2;
    }

    /* Arrange the points into an indexable order. */
    pts[0][0] = curve[0].vertex.p;
    pts[0][1] = curve[0].control[0];
    pts[0][2] = curve[0].control[1];
    pts[0][3] = curve[1].vertex.p;
    pts[1][3] = curve[1].control[0];
    pts[2][3] = curve[1].control[1];
    pts[3][3] = curve[2].vertex.p;
    pts[3][2] = curve[2].control[0];
    pts[3][1] = curve[2].control[1];
    pts[3][0] = curve[3].vertex.p;
    pts[2][0] = curve[3].control[0];
    pts[1][0] = curve[3].control[1];
    pts[1][1] = interior[0];
    pts[2][1] = interior[1];
    pts[2][2] = interior[2];
    pts[1][2] = interior[3];

    /* Now compute the actual point. */
    for (i = 0; i < 4; ++i)
	for (j = 0; j < 4; ++j) {
	    double coeff = Bu[i] * Bv[j];

	    x += pts[i][j].x * coeff, y += pts[i][j].y * coeff;
	}
    pt->x = (fixed)x, pt->y = (fixed)y;
}

int
gs_shading_Tpp_fill_rectangle(const gs_shading_t * psh0, const gs_rect * rect,
			      gx_device * dev, gs_imager_state * pis)
{
    const gs_shading_Tpp_t * const psh = (const gs_shading_Tpp_t *)psh0;
    patch_fill_state_t state;
    shade_coord_stream_t cs;
    patch_curve_t curve[4];
    gs_fixed_point interior[4];
    int code;

    mesh_init_fill_state((mesh_fill_state_t *) & state,
			 (const gs_shading_mesh_t *)psh0, rect, dev, pis);
    state.Function = psh->params.Function;
#if NEW_TENSOR_SHADING
    init_patch_fill_state(&state);
#endif
    shade_next_init(&cs, (const gs_shading_mesh_params_t *)&psh->params,
		    pis);
    while ((code = shade_next_patch(&cs, psh->params.BitsPerFlag,
				    curve, interior)) == 0) {
	/*
	 * The order of points appears to be consistent with that for Coons
	 * patches, which is different from that documented in Red Book 3.
	 */
	gs_fixed_point swapped_interior[4];

	swapped_interior[0] = interior[0];
	swapped_interior[1] = interior[3];
	swapped_interior[2] = interior[2];
	swapped_interior[3] = interior[1];
	code = patch_fill(&state, curve, swapped_interior, Tpp_transform);
	if (code < 0)
	    break;
    }
    return min(code, 0);
}

#if NEW_TENSOR_SHADING

/* fixme :
   The current code allocates up to 64 instances of tensor_patch on C stack, 
   when executes fill_stripe, decompose_stripe, fill_quadrangle.
   Our estimation of the stack size id about 10K.
   Maybe it needs to reduce with a heap-allocated stack.
*/

typedef struct {
    gs_fixed_point pole[4][4]; /* [v][u] */
    patch_color_t c[2][2];     /* [v][u] */
} tensor_patch;

private void
draw_patch(tensor_patch *p, bool interior, ulong rgbcolor)
{
    int i, step = (interior ? 1 : 3);

    return;
    for (i = 0; i < 4; i += step) {
	vd_curve(p->pole[i][0].x, p->pole[i][0].y, 
		 p->pole[i][1].x, p->pole[i][1].y, 
		 p->pole[i][2].x, p->pole[i][2].y, 
		 p->pole[i][3].x, p->pole[i][3].y, 
		 0, rgbcolor);
	vd_curve(p->pole[0][i].x, p->pole[0][i].y, 
		 p->pole[1][i].x, p->pole[1][i].y, 
		 p->pole[2][i].x, p->pole[2][i].y, 
		 p->pole[3][i].x, p->pole[3][i].y, 
		 0, rgbcolor);
    }
}

private inline void
draw_quadrangle(tensor_patch *p, ulong rgbcolor)
{
    return;
    vd_quad(p->pole[0][0].x, p->pole[0][0].y, 
	    p->pole[0][3].x, p->pole[0][3].y,
	    p->pole[3][3].x, p->pole[3][3].y,
	    p->pole[3][0].x, p->pole[3][0].y,
	    0, rgbcolor);
}

private inline int
curve_samples(const gs_fixed_point *pole, int pole_step, fixed fixed_flat)
{
    curve_segment s;

    s.p1.x = pole[pole_step].x;
    s.p1.y = pole[pole_step].y;
    s.p2.x = pole[pole_step * 2].x;
    s.p2.y = pole[pole_step * 2].y;
    s.pt.x = pole[pole_step * 3].x;
    s.pt.y = pole[pole_step * 3].y;
    return 1 << gx_curve_log2_samples(pole[0].x, pole[0].y, &s, fixed_flat);
}

private bool 
intersection_of_big_bars(const gs_fixed_point q[4], int i0, int i1, int i2, int i3, fixed *ry, fixed *ey)
{
    fixed dx1 = q[i1].x - q[i0].x, dy1 = q[i1].y - q[i0].y;
    fixed dx2 = q[i2].x - q[i0].x, dy2 = q[i2].y - q[i0].y;
    fixed dx3 = q[i3].x - q[i0].x, dy3 = q[i3].y - q[i0].y;
    int64_t vp2a, vp2b, vp3a, vp3b;
    int s2, s3;

    if (dx1 == 0 && dy1 == 0)
	return false; /* Zero length bars are out of interest. */
    if (dx2 == 0 && dy2 == 0)
	return false; /* Contacting ends are out of interest. */
    if (dx3 == 0 && dy3 == 0)
	return false; /* Contacting ends are out of interest. */
    if (dx2 == dx1 && dy2 == dx1)
	return false; /* Contacting ends are out of interest. */
    if (dx3 == dx1 && dy3 == dx1)
	return false; /* Contacting ends are out of interest. */
    if (dx2 == dx3 && dy2 == dy3)
	return false; /* Zero length bars are out of interest. */
    vp2a = (int64_t)dx1 * dy2;
    vp2b = (int64_t)dy1 * dx2; 
    /* vp2 = vp2a - vp2b; It can overflow int64_t, but we only need the sign. */
    if (vp2a > vp2b)
	s2 = 1;
    else if (vp2a < vp2b)
	s2 = -1;
    else 
	s2 = 0;
    vp3a = (int64_t)dx1 * dy3;
    vp3b = (int64_t)dy1 * dx3; 
    /* vp3 = vp3a - vp3b; It can overflow int64_t, but we only need the sign. */
    if (vp3a > vp3b)
	s3 = 1;
    else if (vp3a < vp3b)
	s3 = -1;
    else 
	s3 = 0;
    if (s2 = 0) {
	if (s3 == 0)
	    return false; /* Collinear bars - out of interest. */
	if (0 <= dx2 && dx2 <= dx1 && 0 <= dy2 && dy2 <= dy1) {
	    /* The start of the bar 2 is in the bar 1. */
	    *ry = q[i2].y;
	    *ey = 0;
	    return true;
	}
    } else if (s3 == 0) {
	if (0 <= dx3 && dx3 <= dx1 && 0 <= dy3 && dy3 <= dy1) {
	    /* The end of the bar 2 is in the bar 1. */
	    *ry = q[i3].y;
	    *ey = 0;
	    return true;
	}
    } else if (s2 * s3 < 0) {
	/* The intersection definitely exists, so the determinant isn't zero.  */
	/* This branch is passed only with wedges,
	   which have at least 3 segments. 
	   Therefore the determinant can't overflow int64_t. */
	/* The determinant can't compute in double due to 
	   possible loss of all significant bits when subtracting the 
	   trucnated prodicts. But after we subtract in int64_t,
	   it converts to 'double' with a reasonable truncation. */
	fixed d23x = dx3 - dx2, d23y = dy3 - dy2;
	int64_t det = (int64_t)dx1 * d23y - (int64_t)dy1 * d23x;
	int64_t mul = (int64_t)dy2 * d23x - (int64_t)dx2 * d23y;
	double dy = dy1 * (double)mul / (double)det;
	*ry = q[i0].y + (fixed)dy; /* Drop the fraction part, no rounding. */
	*ey = (dy > (fixed)dy ? 1 : 0);
	return true;
    }
    return false;
}

private bool
intersection_of_small_bars(const gs_fixed_point q[4], int i0, int i1, int i2, int i3, fixed *ry, fixed *ey)
{
    /* fixme : optimize assuming the XY span lesser than sqrt(max_fixed).  */
    return intersection_of_big_bars(q, i0, i1, i2, i3, ry, ey);
}


private inline int 
gx_shade_trapezoid(patch_fill_state_t *pfs, const gs_fixed_point q[4], 
	int vi0, int vi1, int vi2, int vi3, fixed ybot, fixed ytop, 
	bool swap_axes, const gx_device_color *pdevc, bool orient)
{
    gs_fixed_edge le, ri;

    if (ybot > ytop)
	return 0;
    if (!orient) {
	le.start = q[vi0];
	le.end = q[vi1];
	ri.start = q[vi2];
	ri.end = q[vi3];
    } else {
	le.start = q[vi2];
	le.end = q[vi3];
	ri.start = q[vi0];
	ri.end = q[vi1];
    }
    return dev_proc(pfs->dev, fill_trapezoid)(pfs->dev,
	    &le, &ri, ybot, ytop, swap_axes, pdevc, pfs->pis->log_op);
}

private void
patch_color_to_device_color(const patch_fill_state_t * pfs, const patch_color_t *c, gx_device_color *pdevc)
{
    /* A code fragment copied from mesh_fill_triangle. */
    const gs_color_space *pcs = pfs->direct_space;
    gs_client_color fcc;
    int n = pcs->type->num_components(pcs);

    memcpy(fcc.paint.values, c->cc.paint.values, sizeof(fcc.paint.values[0]) * n);
    pcs->type->restrict_color(&fcc, pcs);
    pcs->type->remap_color(&fcc, pcs, pdevc, pfs->pis,
			      pfs->dev, gs_color_select_texture);
}

private inline bool
is_color_span_big(const patch_fill_state_t * pfs, const patch_color_t *c0, const patch_color_t *c1)
{
    const gs_color_space *pcs = pfs->direct_space;
    patch_color_t cc0 = *c0, cc1 = *c1;
    int n = pcs->type->num_components(pcs), i;
    double m;

    patch_resolve_color(&cc0, pfs);
    patch_resolve_color(&cc1, pfs);
    m = any_abs(cc1.cc.paint.values[0] - cc0.cc.paint.values[0]) / pfs->color_domain.paint.values[0];
    for (i = 1; i < n; i++)
	m = max(m, any_abs(cc1.cc.paint.values[i] - cc0.cc.paint.values[i]) / pfs->color_domain.paint.values[i]);
    return m > pfs->pis->smoothness;
}

private inline bool
is_color_monotonic(const patch_fill_state_t * pfs, const patch_color_t *c0, const patch_color_t *c1)
{
    if (!pfs->Function)
	return true;
    return gs_function_is_monotonic(pfs->Function, &c0->t, &c1->t, EFFORT_MODERATE);
}

private int
constant_color_wedge_trap(patch_fill_state_t *pfs, 
	const gs_fixed_point q[4], fixed ybot, fixed ytop, const patch_color_t *c, bool swap_axes)
{
    gx_device_color dc;
    fixed ry, ey;
    int code;
    patch_color_t c1 = *c;
    fixed dx1 = q[1].x - q[0].x, dy1 = q[1].y - q[0].y;
    fixed dx2 = q[2].x - q[0].x, dy2 = q[2].y - q[0].y;
    bool orient;

    patch_resolve_color(&c1, pfs);
    patch_color_to_device_color(pfs, &c1, &dc);
    if (intersection_of_big_bars(q, 0, 1, 2, 3, &ry, &ey)) {
	orient = ((int64_t)dx1 * dy2 > (int64_t)dy1 * dx2);
	code = gx_shade_trapezoid(pfs, q, 2, 3, 0, 1, ybot, ry + ey, swap_axes, &dc, orient);
	if (code < 0)
	    return code;
	return gx_shade_trapezoid(pfs, q, 0, 1, 2, 3, ry, ytop, swap_axes, &dc, orient);
    } else if ((int64_t)dx1 * dy2 != (int64_t)dy1 * dx2) {
	orient = ((int64_t)dx1 * dy2 > (int64_t)dy1 * dx2);
	return gx_shade_trapezoid(pfs, q, 0, 1, 2, 3, ybot, ytop, swap_axes, &dc, orient);
    } else {
	fixed dx3 = q[3].x - q[0].x, dy3 = q[3].y - q[0].y;

	orient = ((int64_t)dx1 * dy3 > (int64_t)dy1 * dx3);
	return gx_shade_trapezoid(pfs, q, 0, 1, 2, 3, ybot, ytop, swap_axes, &dc, orient);
    }
}

private inline bool
covers_pixel_centers(fixed ybot, fixed ytop)
{
    return fixed_pixround(ybot) < fixed_pixround(ytop);
}

private int
wedge_trap_decompose(patch_fill_state_t *pfs, gs_fixed_point p[4],
	fixed ybot, fixed ytop, const patch_color_t *c0, const patch_color_t *c1, bool swap_axes)
{
    patch_color_t c;
    int code;
    if (!pfs->vectorization && !covers_pixel_centers(ybot, ytop))
	return 0;
    /* Use the recursive decomposition due to is_color_monotonic
       based on fn_is_monotonic_proc_t is_monotonic,
       which applies to intervals. */
    patch_interpolate_color(&c, c0, c1, pfs, 0.5);
    if (ytop - ybot < pfs->fixed_flat) /* Prevent an infinite color decomposition. */
	return constant_color_wedge_trap(pfs, p, ybot, ytop, &c, swap_axes);
    else if (!is_color_monotonic(pfs, c0, c1) || is_color_span_big(pfs, c0, c1) || 
		ytop - ybot > sqrt(max_fixed)) {
	fixed y = (ybot + ytop) / 2;
    
	code = wedge_trap_decompose(pfs, p, ybot, y, c0, &c, swap_axes);
	if (code < 0)
	    return code;
	return wedge_trap_decompose(pfs, p, y, ytop, &c, c1, swap_axes);
    } else
	return constant_color_wedge_trap(pfs, p, ybot, ytop, &c, swap_axes);
}

private int
fill_wedge_trap(patch_fill_state_t *pfs, const gs_fixed_point *p0, const gs_fixed_point *p1, 
	    const gs_fixed_point q[2], const patch_color_t *c0, const patch_color_t *c1, 
	    bool swap_axes)
{
    /* We assume that the width of the wedge is close to zero,
       so we can ignore the slope when computing transversal distances. */
    gs_fixed_point p[4];

    if (p0->y < p1->y) {
	p[0] = *p0;
	p[1] = *p1;
    } else {
	p[1] = *p1;
	p[0] = *p0;
    }
    p[2] = q[0];
    p[3] = q[1];
    return wedge_trap_decompose(pfs, p, p[0].y, p[1].y, c0, c1, swap_axes);
}

private void
split_curve_s(const gs_fixed_point *pole, gs_fixed_point *q0, gs_fixed_point *q1, int pole_step)
{
    /*	This copies a code fragment from split_curve_midpoint,
        substituting another data type.
     */				
    /*
     * We have to define midpoint carefully to avoid overflow.
     * (If it overflows, something really pathological is going
     * on, but we could get infinite recursion that way....)
     */
#define midpoint(a,b)\
  (arith_rshift_1(a) + arith_rshift_1(b) + ((a) & (b) & 1) + 1)
    fixed x12 = midpoint(pole[1 * pole_step].x, pole[2 * pole_step].x);
    fixed y12 = midpoint(pole[1 * pole_step].y, pole[2 * pole_step].y);

    /* q[0] and q[1] must not be the same as pole. */
    q0[1 * pole_step].x = midpoint(pole[0 * pole_step].x, pole[1 * pole_step].x);
    q0[1 * pole_step].y = midpoint(pole[0 * pole_step].y, pole[1 * pole_step].y);
    q1[2 * pole_step].x = midpoint(pole[2 * pole_step].x, pole[3 * pole_step].x);
    q1[2 * pole_step].y = midpoint(pole[2 * pole_step].y, pole[3 * pole_step].y);
    q0[2 * pole_step].x = midpoint(q0[1 * pole_step].x, x12);
    q0[2 * pole_step].y = midpoint(q0[1 * pole_step].y, y12);
    q1[1 * pole_step].x = midpoint(x12, q1[2 * pole_step].x);
    q1[1 * pole_step].y = midpoint(y12, q1[2 * pole_step].y);
    q0[0 * pole_step].x = pole[0 * pole_step].x;
    q0[0 * pole_step].y = pole[0 * pole_step].y;
    q0[3 * pole_step].x = q1[0 * pole_step].x = midpoint(q0[2 * pole_step].x, q1[1 * pole_step].x);
    q0[3 * pole_step].y = q1[0 * pole_step].y = midpoint(q0[2 * pole_step].y, q1[1 * pole_step].y);
    q1[3 * pole_step].x = pole[3 * pole_step].x;
    q1[3 * pole_step].y = pole[3 * pole_step].y;
#undef midpoint
}

private void
split_curve(const gs_fixed_point pole[4], gs_fixed_point q0[4], gs_fixed_point q1[4])
{
    split_curve_s(pole, q0, q1, 1);
}


private void
generate_inner_vertices(gs_fixed_point *p, const gs_fixed_point pole[4], int k)
{
    /* Recure to get exactly same points as when devided a patch. */
    /* An iteration can't give them preciselly. */
    if (k > 1) {
	gs_fixed_point q[2][4];

	split_curve(pole, q[0], q[1]);
	p[k / 2 + 1] = q[0][3];
	generate_inner_vertices(p, q[0], k / 2);
	generate_inner_vertices(p + k / 2, q[1], k / 2);
    }
}

private inline void
do_swap_axes(gs_fixed_point *p, int k)
{
    int i;

    for (i = 0; i < k; i++) {
	p[i].x ^= p[i].y; p[i].y ^= p[i].x; p[i].x ^= p[i].y;
    }
}

private inline void
y_extreme_vertice(gs_fixed_point *q, const gs_fixed_point *p, int k, int minmax)
{
    int i;
    gs_fixed_point r = *p;

    for (i = 1; i < k; i++)
	if ((p[i].y - r.y) * minmax < 0)
	    r = p[i];
    *q = r;	
}

private inline fixed
span_x(gs_fixed_point *p, int k)
{
    int i;
    fixed xmin = p[0].x, xmax = p[0].x;

    for (i = 1; i < k; i++) {
	xmin = min(xmin, p[i].x);
	xmax = max(xmax, p[i].x);
    }
    return xmax - xmin;
}

private inline fixed
span_y(gs_fixed_point *p, int k)
{
    int i;
    fixed ymin = p[0].y, ymax = p[0].y;

    for (i = 1; i < k; i++) {
	ymin = min(ymin, p[i].y);
	ymax = max(ymax, p[i].y);
    }
    return ymax - ymin;
}

private int
fill_wedge(patch_fill_state_t *pfs, int ka, 
	const gs_fixed_point pole[4], const patch_color_t *c0, const patch_color_t *c1)
{
    patch_color_t ca, cb, *pca = &ca, *pcb = &cb, *pcc;
    fixed dx, dy;
    bool swap_axes;
    int k1 = ka + 1, i, code;
    gs_fixed_point q[2], *p = pfs->wedge_buf;

    p[0] = pole[0];
    p[k1] = pole[3];
    generate_inner_vertices(p, pole, ka); /* ka >= 2, see fill_wedges */
    dx = span_x(p, k1);
    dy = span_y(p, k1);
    swap_axes = (dx > dy);
    if (swap_axes)
	do_swap_axes(p, k1);
    /* We assume that the width of the wedge is close to zero.
       An exception is a too big setflat, which we assume to be
       a bad document design, which we don't care of.
       Therefore we don't bother with exact coverage.
       We generate a simple coverage within the convex hull. */
    y_extreme_vertice(&q[0], p, k1, -1);
    y_extreme_vertice(&q[1], p, k1, 1);
    *pca = *c0;
    for (i = 1; i < k1; i++) {
	patch_interpolate_color(pcb, c0, c1, pfs, (i - 1.0) / (ka - 1.0)); /* ka >= 2, see fill_wedges */
	code = fill_wedge_trap(pfs, &p[i - 1], &p[i], q, pca, pcb, swap_axes);
	if (code < 0)
	    return code;
	pcc = pca; pca = pcb; pcb = pcc;
    }
    if (p[0].x == q[0].x && p[0].y == q[0].y && 
	    p[ka].x == q[1].x && p[ka].y == q[1].y)
	return 0;
    return fill_wedge_trap(pfs, &p[0], &p[ka], q, c0, c1, swap_axes);
}

private int
fill_wedges_aux(patch_fill_state_t *pfs, int k, int ka, 
	const gs_fixed_point pole[4], const patch_color_t *c0, const patch_color_t *c1)
{
    if (k > 1) {
	gs_fixed_point q[2][4];
	patch_color_t c;
	int code;

	patch_interpolate_color(&c, c0, c1, pfs, 0.5);
	split_curve(pole, q[0], q[1]);
	code = fill_wedges_aux(pfs, k / 2, ka, q[0], c0, &c);
	if (code < 0)
	    return code;
	return fill_wedges_aux(pfs, k / 2, ka, q[1], &c, c1);
   } else
	return fill_wedge(pfs, ka, pole, c0, c1);
}

private int
fill_wedges(patch_fill_state_t *pfs, int k0, int k1, 
	const gs_fixed_point *pole, int pole_step, const patch_color_t *c0, const patch_color_t *c1)
{
    /* Generate wedges between 2 variants of a curve flattening. */
    /* k0, k1 is a power of 2. */
    int i;
    gs_fixed_point p[4];

    return 0; /* fixme : temporary disabled for a debug purpose. */
    if (k0 == k1)
	return 0; /* Wedges are zero area. */
    if (k0 > k1) {
	k0 ^=k1; k1 ^=k0; k0 ^=k1;
    }
    p[0] = pole[0];
    p[1] = pole[pole_step];
    p[2] = pole[pole_step * 2];
    p[3] = pole[pole_step * 3];
    return fill_wedges_aux(pfs, k0, k1 / k0, p, c0, c1);
}

private inline void
make_vertices(gs_fixed_point q[4], const tensor_patch *p)
{
    q[0] = p->pole[0][0];
    q[1] = p->pole[0][3];
    q[2] = p->pole[3][3];
    q[3] = p->pole[3][0];
}

private inline void
wrap_vertices_by_y(gs_fixed_point q[4], const gs_fixed_point s[4])
{
    fixed y = s[0].y;
    int i = 0;

    if (y > s[1].y)
	i = 1, y = s[1].y;
    if (y > s[2].y)
	i = 2, y = s[2].y;
    if (y > s[3].y)
	i = 3, y = s[3].y;
    q[0] = s[(i + 0) % 4];
    q[1] = s[(i + 1) % 4];
    q[2] = s[(i + 2) % 4];
    q[3] = s[(i + 3) % 4];
}

private int 
constant_color_quadrangle(patch_fill_state_t * pfs, const tensor_patch *p)
{
    /* Assuming the XY span lesser than sqrt(max_fixed). 
       It helps for intersection_of_small_bars to comppute faster. */
    gs_fixed_point q[4];
    fixed ry, ey;
    int code;
    bool swap_axes;
    gx_device_color dc;
    patch_color_t c1, c2, c;
    bool orient;

    patch_interpolate_color(&c1, &p->c[0][0], &p->c[0][1], pfs, 0.5);
    patch_interpolate_color(&c2, &p->c[1][0], &p->c[1][1], pfs, 0.5);
    patch_interpolate_color(&c, &c1, &c2, pfs, 0.5);
    patch_resolve_color(&c1, pfs);
    patch_color_to_device_color(pfs, &c, &dc);
    {	fixed dx, dy;
	gs_fixed_point qq[4];

	make_vertices(qq, p);
	dx = span_x(qq, 4);
	dy = span_y(qq, 4);
	swap_axes = (dy < dx);
	if (swap_axes)
	    do_swap_axes(qq, 4);
	wrap_vertices_by_y(q, qq);
    }
    {	fixed dx1 = q[1].x - q[0].x, dy1 = q[1].y - q[0].y;
	fixed dx3 = q[3].x - q[0].x, dy3 = q[3].y - q[0].y;
	fixed g13 = dx1 * dy3, h13 = dy1 * dx3;

	if (g13 == h13) {
	    fixed dx2 = q[2].x - q[0].x, dy2 = q[2].y - q[0].y;
	    fixed g23 = dx2 * dy3, h23 = dy2 * dx3;

	    if (g23 != h23) {
		orient = (g23 > h23);
		if (q[2].y <= q[3].y) {
		    if ((code = gx_shade_trapezoid(pfs, q, 1, 2, 0, 3, q[1].y, q[2].y, swap_axes, &dc, orient)) < 0)
			return code;
		    return gx_shade_trapezoid(pfs, q, 2, 3, 0, 3, q[2].y, q[3].y, swap_axes, &dc, orient);
		} else {
		    if ((code = gx_shade_trapezoid(pfs, q, 1, 2, 0, 3, q[1].y, q[3].y, swap_axes, &dc, orient)) < 0)
			return code;
		    return gx_shade_trapezoid(pfs, q, 1, 2, 3, 2, q[3].y, q[2].y, swap_axes, &dc, orient);
		}
	    } else {
		orient = (dx1 * dy2 > dy1 * dx2);
		if (q[1].y <= q[2].y) {
		    if ((code = gx_shade_trapezoid(pfs, q, 0, 1, 3, 2, q[0].y, q[1].y, swap_axes, &dc, orient)) < 0)
			return code;
		    return gx_shade_trapezoid(pfs, q, 1, 2, 3, 2, q[1].y, q[2].y, swap_axes, &dc, orient);
		} else {
		    if ((code = gx_shade_trapezoid(pfs, q, 0, 1, 3, 2, q[0].y, q[2].y, swap_axes, &dc, orient)) < 0)
			return code;
		    return gx_shade_trapezoid(pfs, q, 0, 1, 2, 1, q[2].y, q[1].y, swap_axes, &dc, orient);
		}
	    }
	}
	orient = (dx1 * dy3 > dy1 * dx3);
    }
    /* fixme: case dx2 * dy3 == dy2 * dx3, etc. */
    if (q[1].y <= q[2].y && q[2].y <= q[3].y) {
	if (intersection_of_small_bars(q, 0, 3, 1, 2, &ry, &ey)) {
	    if ((code = gx_shade_trapezoid(pfs, q, 0, 1, 0, 3, q[0].y, q[1].y, swap_axes, &dc, orient)) < 0)
		return code;
	    if ((code = gx_shade_trapezoid(pfs, q, 0, 3, 1, 2, q[1].y, ry + ey, swap_axes, &dc, orient)) < 0)
		return code;
	    if ((code = gx_shade_trapezoid(pfs, q, 1, 2, 0, 3, ry, q[2].y, swap_axes, &dc, orient)) < 0)
		return code;
	    return gx_shade_trapezoid(pfs, q, 0, 3, 2, 3, q[2].y, q[3].y, swap_axes, &dc, orient);
	} else {
	    if ((code = gx_shade_trapezoid(pfs, q, 0, 1, 0, 3, q[0].y, q[1].y, swap_axes, &dc, orient)) < 0)
		return code;
	    if ((code = gx_shade_trapezoid(pfs, q, 1, 2, 0, 3, q[1].y, q[2].y, swap_axes, &dc, orient)) < 0)
		return code;
	    return gx_shade_trapezoid(pfs, q, 2, 3, 0, 3, q[2].y, q[3].y, swap_axes, &dc, orient);
	}
    } else if (q[1].y <= q[3].y && q[3].y <= q[2].y) {
	if (intersection_of_small_bars(q, 0, 3, 1, 2, &ry, &ey)) {
	    if ((code = gx_shade_trapezoid(pfs, q, 0, 1, 0, 3, q[0].y, q[1].y, swap_axes, &dc, orient)) < 0)
		return code;
	    if ((code = gx_shade_trapezoid(pfs, q, 1, 2, 0, 3, q[1].y, ry + ey, swap_axes, &dc, orient)) < 0)
		return code;
	    if ((code = gx_shade_trapezoid(pfs, q, 0, 3, 1, 2, ry, q[3].y, swap_axes, &dc, orient)) < 0)
		return code;
	    return gx_shade_trapezoid(pfs, q, 3, 2, 1, 2, q[3].y, q[2].y, swap_axes, &dc, orient);
	} else {
	    if ((code = gx_shade_trapezoid(pfs, q, 0, 1, 0, 3, q[0].y, q[1].y, swap_axes, &dc, orient)) < 0)
		return code;
	    if ((code = gx_shade_trapezoid(pfs, q, 1, 2, 0, 3, q[1].y, q[3].y, swap_axes, &dc, orient)) < 0)
		return code;
	    return gx_shade_trapezoid(pfs, q, 1, 2, 3, 2, q[3].y, q[2].y, swap_axes, &dc, orient);
	}
    } else if (q[2].y <= q[1].y && q[1].y <= q[3].y) {
	if (intersection_of_small_bars(q, 0, 1, 2, 3, &ry, &ey)) {
	    if ((code = gx_shade_trapezoid(pfs, q, 0, 1, 0, 3, q[0].y, ry + ey, swap_axes, &dc, orient)) < 0)
		return code;
	    if ((code = gx_shade_trapezoid(pfs, q, 2, 1, 2, 3, q[2].y, ry + ey, swap_axes, &dc, orient)) < 0)
		return code;
	    if ((code = gx_shade_trapezoid(pfs, q, 2, 1, 0, 1, ry, q[1].y, swap_axes, &dc, orient)) < 0)
		return code;
	    return gx_shade_trapezoid(pfs, q, 2, 3, 0, 3, ry, q[3].y, swap_axes, &dc, orient);
	} else if (intersection_of_small_bars(q, 0, 3, 1, 2, &ry, &ey)) {
	    if ((code = gx_shade_trapezoid(pfs, q, 0, 1, 0, 3, q[0].y, ry + ey, swap_axes, &dc, orient)) < 0)
		return code;
	    if ((code = gx_shade_trapezoid(pfs, q, 2, 1, 2, 3, q[2].y, ry + ey, swap_axes, &dc, orient)) < 0)
		return code;
	    if ((code = gx_shade_trapezoid(pfs, q, 0, 1, 2, 1, ry, q[1].y, swap_axes, &dc, orient)) < 0)
		return code;
	    return gx_shade_trapezoid(pfs, q, 0, 3, 2, 3, ry, q[3].y, swap_axes, &dc, orient);
	} else {
	    if ((code = gx_shade_trapezoid(pfs, q, 0, 1, 0, 3, q[0].y, q[1].y, swap_axes, &dc, orient)) < 0)
		return code;
	    if ((code = gx_shade_trapezoid(pfs, q, 2, 3, 2, 1, q[2].y, q[1].y, swap_axes, &dc, orient)) < 0)
		return code;
	    return gx_shade_trapezoid(pfs, q, 2, 3, 0, 3, q[1].y, q[3].y, swap_axes, &dc, orient);
	}
    } else if (q[2].y <= q[3].y && q[3].y <= q[1].y) {
	if (intersection_of_small_bars(q, 0, 1, 2, 3, &ry, &ey)) {
	    /* Same code as someone above. */
	    if ((code = gx_shade_trapezoid(pfs, q, 0, 1, 0, 3, q[0].y, ry + ey, swap_axes, &dc, orient)) < 0)
		return code;
	    if ((code = gx_shade_trapezoid(pfs, q, 2, 1, 2, 3, q[2].y, ry + ey, swap_axes, &dc, orient)) < 0)
		return code;
	    if ((code = gx_shade_trapezoid(pfs, q, 2, 1, 0, 1, ry, q[1].y, swap_axes, &dc, orient)) < 0)
		return code;
	    return gx_shade_trapezoid(pfs, q, 2, 3, 0, 3, ry, q[3].y, swap_axes, &dc, orient);
	} else if (intersection_of_small_bars(q, 0, 3, 2, 1, &ry, &ey)) {
	    /* Same code as someone above. */
	    if ((code = gx_shade_trapezoid(pfs, q, 0, 1, 0, 3, q[0].y, ry + ey, swap_axes, &dc, orient)) < 0)
		return code;
	    if ((code = gx_shade_trapezoid(pfs, q, 2, 1, 2, 3, q[2].y, ry + ey, swap_axes, &dc, orient)) < 0)
		return code;
	    if ((code = gx_shade_trapezoid(pfs, q, 0, 1, 2, 1, ry, q[1].y, swap_axes, &dc, orient)) < 0)
		return code;
	    return gx_shade_trapezoid(pfs, q, 0, 3, 2, 3, ry, q[3].y, swap_axes, &dc, orient);
	} else {
	    if ((code = gx_shade_trapezoid(pfs, q, 0, 1, 0, 3, q[0].y, q[2].y, swap_axes, &dc, orient)) < 0)
		return code;
	    if ((code = gx_shade_trapezoid(pfs, q, 2, 3, 0, 3, q[2].y, q[3].y, swap_axes, &dc, orient)) < 0)
		return code;
	    return gx_shade_trapezoid(pfs, q, 0, 1, 2, 1, q[2].y, q[1].y, swap_axes, &dc, orient);
	}
    } else if (q[3].y <= q[1].y && q[1].y <= q[2].y) {
	if (intersection_of_small_bars(q, 0, 1, 3, 2, &ry, &ey)) {
	    if ((code = gx_shade_trapezoid(pfs, q, 0, 1, 0, 3, q[0].y, q[3].y, swap_axes, &dc, orient)) < 0)
		return code;
	    if ((code = gx_shade_trapezoid(pfs, q, 0, 1, 3, 2, q[3].y, ry + ey, swap_axes, &dc, orient)) < 0)
		return code;
	    if ((code = gx_shade_trapezoid(pfs, q, 3, 2, 0, 1, ry, q[1].y, swap_axes, &dc, orient)) < 0)
		return code;
	    return gx_shade_trapezoid(pfs, q, 3, 2, 1, 2, q[1].y, q[2].y, swap_axes, &dc, orient);
	} else {
	    if ((code = gx_shade_trapezoid(pfs, q, 0, 1, 0, 3, q[0].y, q[3].y, swap_axes, &dc, orient)) < 0)
		return code;
	    if ((code = gx_shade_trapezoid(pfs, q, 0, 1, 3, 2, q[3].y, q[1].y, swap_axes, &dc, orient)) < 0)
		return code;
	    return gx_shade_trapezoid(pfs, q, 1, 2, 3, 2, q[1].y, q[2].y, swap_axes, &dc, orient);
	}
    } else if (q[3].y <= q[2].y && q[2].y <= q[1].y) {
	if (intersection_of_small_bars(q, 0, 1, 2, 3, &ry, &ey)) {
	    if ((code = gx_shade_trapezoid(pfs, q, 0, 1, 0, 3, q[0].y, q[3].y, swap_axes, &dc, orient)) < 0)
		return code;
	    if ((code = gx_shade_trapezoid(pfs, q, 0, 1, 3, 2, q[3].y, ry + ey, swap_axes, &dc, orient)) < 0)
		return code;
	    if ((code = gx_shade_trapezoid(pfs, q, 3, 2, 0, 1, ry, q[2].y, swap_axes, &dc, orient)) < 0)
		return code;
	    return gx_shade_trapezoid(pfs, q, 2, 1, 0, 1, q[2].y, q[1].y, swap_axes, &dc, orient);
	} else {
	    if ((code = gx_shade_trapezoid(pfs, q, 0, 1, 0, 3, q[0].y, q[3].y, swap_axes, &dc, orient)) < 0)
		return code;
	    if ((code = gx_shade_trapezoid(pfs, q, 0, 1, 3, 2, q[3].y, q[2].y, swap_axes, &dc, orient)) < 0)
		return code;
	    return gx_shade_trapezoid(pfs, q, 0, 1, 2, 1, q[2].y, q[1].y, swap_axes, &dc, orient);
	}
    } else {
	/* Impossible. */
	return_error(gs_error_unregistered);
    }
}

private inline void
divide_quadrangle_by_v(patch_fill_state_t * pfs, tensor_patch *s0, tensor_patch *s1, 
	    const tensor_patch *p)
{
    s0->pole[0][0] = p->pole[0][0];
    s0->pole[0][3] = p->pole[0][3];
    s0->pole[3][0].x = s1->pole[0][0].x = (p->pole[0][0].x + p->pole[3][0].x) / 2;
    s0->pole[3][3].x = s1->pole[0][3].x = (p->pole[0][3].x + p->pole[3][3].x) / 2;
    s0->pole[3][0].y = s1->pole[0][0].y = (p->pole[0][0].y + p->pole[3][0].y) / 2;
    s0->pole[3][3].y = s1->pole[0][3].y = (p->pole[0][3].y + p->pole[3][3].y) / 2;
    s1->pole[3][0] = p->pole[3][0];
    s1->pole[3][3] = p->pole[3][3];
    s0->c[0][0] = p->c[0][0];
    s0->c[0][1] = p->c[0][1];
    patch_interpolate_color(&s0->c[1][0], &p->c[0][0], &p->c[1][0], pfs, 0.5);
    patch_interpolate_color(&s0->c[1][1], &p->c[0][1], &p->c[1][1], pfs, 0.5);
    s1->c[0][0] = s0->c[1][0];
    s1->c[0][1] = s0->c[1][1];
    s1->c[1][0] = p->c[1][0];
    s1->c[1][1] = p->c[1][1];
}

private inline void
divide_quadrangle_by_u(patch_fill_state_t * pfs, tensor_patch *s0, tensor_patch *s1, 
	    const tensor_patch *p)
{
    s0->pole[0][0] = p->pole[0][0];
    s0->pole[3][0] = p->pole[3][0];
    s0->pole[0][3].x = s1->pole[0][0].x = (p->pole[0][0].x + p->pole[0][3].x) / 2;
    s0->pole[3][3].x = s1->pole[3][0].x = (p->pole[3][0].x + p->pole[3][3].x) / 2;
    s0->pole[0][3].y = s1->pole[0][0].y = (p->pole[0][0].y + p->pole[0][3].y) / 2;
    s0->pole[3][3].y = s1->pole[3][0].y = (p->pole[3][0].y + p->pole[3][3].y) / 2;
    s1->pole[0][3] = p->pole[0][3];
    s1->pole[3][3] = p->pole[3][3];
    s0->c[0][0] = p->c[0][0];
    s0->c[1][0] = p->c[1][0];
    patch_interpolate_color(&s0->c[0][1], &p->c[0][0], &p->c[0][1], pfs, 0.5);
    patch_interpolate_color(&s0->c[1][1], &p->c[1][0], &p->c[1][1], pfs, 0.5);
    s1->c[0][0] = s0->c[0][1];
    s1->c[1][0] = s0->c[1][1];
    s1->c[0][1] = p->c[0][1];
    s1->c[1][1] = p->c[1][1];
}

private inline bool
is_color_span_v_big(const patch_fill_state_t * pfs, const tensor_patch *p)
{
    if (is_color_span_big(pfs, &p->c[0][0], &p->c[1][0]))
	return true;
    if (is_color_span_big(pfs, &p->c[0][1], &p->c[1][1]))
	return true;
    return false;
}

private inline bool
is_color_span_u_big(const patch_fill_state_t * pfs, const tensor_patch *p)
{
    if (is_color_span_big(pfs, &p->c[0][0], &p->c[0][1]))
	return true;
    if (is_color_span_big(pfs, &p->c[1][0], &p->c[1][1]))
	return true;
    return false;
}

private inline bool
is_color_monotonic_by_v(const patch_fill_state_t * pfs, const tensor_patch *p) 
{
    if (!is_color_monotonic(pfs, &p->c[0][0], &p->c[1][0]))
	return false;
    if (!is_color_monotonic(pfs, &p->c[0][1], &p->c[1][1]))
	return false;
    return true;
}

private inline bool
is_color_monotonic_by_u(const patch_fill_state_t * pfs, const tensor_patch *p) 
{
    if (!is_color_monotonic(pfs, &p->c[0][0], &p->c[0][1]))
	return false;
    if (!is_color_monotonic(pfs, &p->c[1][0], &p->c[1][1]))
	return false;
    return true;
}

private inline bool
quadrangle_x_span_covers_pixel_centers(const tensor_patch *p)
{
    fixed xbot = min(min(p->pole[0][0].x, p->pole[0][3].x),
		     min(p->pole[3][0].x, p->pole[3][3].x));
    fixed xtop = max(max(p->pole[0][0].x, p->pole[0][3].x),
		     max(p->pole[3][0].x, p->pole[3][3].x));
    return covers_pixel_centers(xbot, xtop);
}

private inline bool
quadrangle_y_span_covers_pixel_centers(const tensor_patch *p)
{
    fixed ybot = min(min(p->pole[0][0].y, p->pole[0][3].y),
		     min(p->pole[3][0].y, p->pole[3][3].y));
    fixed ytop = max(max(p->pole[0][0].y, p->pole[0][3].y),
		     max(p->pole[3][0].y, p->pole[3][3].y));
    return covers_pixel_centers(ybot, ytop);
}

private inline bool
quadrangle_bbox_covers_pixel_centers(const tensor_patch *p)
{
    if (quadrangle_x_span_covers_pixel_centers(p))
	return true;
    if (quadrangle_y_span_covers_pixel_centers(p))
	return true;
    return false;
}

private int 
fill_quadrangle(patch_fill_state_t * pfs, const tensor_patch *p)
{
    /* The quadrangle is flattened enough by V and U, so ignore inner poles. */
    tensor_patch s0, s1;
    int code;
    bool is_big_u = false, is_big_v = false;

    if (!pfs->vectorization && !quadrangle_bbox_covers_pixel_centers(p))
	return 0;
    if (!is_big_u && any_abs(p->pole[0][0].x - p->pole[0][3].x) > pfs->fixed_flat)
	is_big_u = true;
    if (!is_big_u && any_abs(p->pole[3][0].x - p->pole[3][3].x) > pfs->fixed_flat)
	is_big_u = true;
    if (!is_big_u && any_abs(p->pole[0][0].y - p->pole[0][3].y) > pfs->fixed_flat)
	is_big_u = true;
    if (!is_big_u && any_abs(p->pole[3][0].y - p->pole[3][3].y) > pfs->fixed_flat)
	is_big_u = true;
    if (!is_big_v && any_abs(p->pole[0][0].x - p->pole[3][0].x) > pfs->fixed_flat)
	is_big_v = true;
    if (!is_big_v && any_abs(p->pole[0][3].x - p->pole[3][3].x) > pfs->fixed_flat)
	is_big_v = true;
    if (!is_big_v && any_abs(p->pole[0][0].y - p->pole[3][0].y) > pfs->fixed_flat)
	is_big_v = true;
    if (!is_big_v && any_abs(p->pole[0][3].y - p->pole[3][3].y) > pfs->fixed_flat)
	is_big_v = true;
    if (!is_big_v && !is_big_u)
	return constant_color_quadrangle(pfs, p);
    else if (is_big_v && (!is_color_monotonic_by_v(pfs, p) || is_color_span_v_big(pfs, p))) {
	fill_wedges(pfs, 2, 1, p->pole[0], 1, &p->c[0][0], &p->c[0][1]);
	fill_wedges(pfs, 2, 1, p->pole[3], 1, &p->c[1][0], &p->c[1][1]);
	divide_quadrangle_by_v(pfs, &s0, &s1, p);
	draw_quadrangle(&s0, RGB(255, 0, 0));
	draw_quadrangle(&s1, RGB(255, 0, 0));
	code = fill_quadrangle(pfs, &s0);
	if (code < 0)
	    return code;
	return fill_quadrangle(pfs, &s1);
    } else if (is_big_u && (!is_color_monotonic_by_u(pfs, p) || is_color_span_u_big(pfs, p))) {
	fill_wedges(pfs, 2, 1, &p->pole[0][0], 4, &p->c[0][0], &p->c[1][0]);
	fill_wedges(pfs, 2, 1, &p->pole[0][3], 4, &p->c[0][1], &p->c[1][1]);
	divide_quadrangle_by_u(pfs, &s0, &s1, p);
	draw_quadrangle(&s0, RGB(255, 0, 0));
	draw_quadrangle(&s1, RGB(255, 0, 0));
	code = fill_quadrangle(pfs, &s0);
	if (code < 0)
	    return code;
	return fill_quadrangle(pfs, &s1);
    } else
	return constant_color_quadrangle(pfs, p);
}

private inline void
split_stripe(patch_fill_state_t * pfs, tensor_patch *s0, tensor_patch *s1, const tensor_patch *p)
{
    split_curve_s(p->pole[0], s0->pole[0], s1->pole[0], 1);
    split_curve_s(p->pole[1], s0->pole[1], s1->pole[1], 1);
    split_curve_s(p->pole[2], s0->pole[2], s1->pole[2], 1);
    split_curve_s(p->pole[3], s0->pole[3], s1->pole[3], 1);
    s0->c[0][0] = p->c[0][0];
    s0->c[1][0] = p->c[1][0];
    patch_interpolate_color(&s0->c[0][1], &p->c[0][0], &p->c[0][1], pfs, 0.5);
    patch_interpolate_color(&s0->c[1][1], &p->c[1][0], &p->c[1][1], pfs, 0.5);
    s1->c[0][0] = s0->c[0][1];
    s1->c[1][0] = s0->c[1][1];
    s1->c[0][1] = p->c[0][1];
    s1->c[1][1] = p->c[1][1];
}

private inline void
split_patch(patch_fill_state_t * pfs, tensor_patch *s0, tensor_patch *s1, const tensor_patch *p)
{
    split_curve_s(&p->pole[0][0], &s0->pole[0][0], &s1->pole[0][0], 4);
    split_curve_s(&p->pole[0][1], &s0->pole[0][1], &s1->pole[0][1], 4);
    split_curve_s(&p->pole[0][2], &s0->pole[0][2], &s1->pole[0][2], 4);
    split_curve_s(&p->pole[0][3], &s0->pole[0][3], &s1->pole[0][3], 4);
    s0->c[0][0] = p->c[0][0];
    s0->c[0][1] = p->c[0][1];
    patch_interpolate_color(&s0->c[1][0], &p->c[0][0], &p->c[1][0], pfs, 0.5);
    patch_interpolate_color(&s0->c[1][1], &p->c[0][1], &p->c[1][1], pfs, 0.5);
    s1->c[0][0] = s0->c[1][0];
    s1->c[0][1] = s0->c[1][1];
    s1->c[1][0] = p->c[1][0];
    s1->c[1][1] = p->c[1][1];
}

private int 
decompose_stripe(patch_fill_state_t * pfs, const tensor_patch *p, int ku)
{
    if (ku > 1) {
	tensor_patch s0, s1;
	int code;

	split_stripe(pfs, &s0, &s1, p);
	draw_patch(&s0, true, RGB(0, 128, 128));
	draw_patch(&s1, true, RGB(0, 128, 128));
	code = decompose_stripe(pfs, &s0, ku / 2);
	if (code < 0)
	    return code;
	return decompose_stripe(pfs, &s1, ku / 2);
    } else
	return fill_quadrangle(pfs, p);
}

private int 
fill_stripe(patch_fill_state_t * pfs, const tensor_patch *p, int kum)
{
    /* The stripe is flattened enough by V, so ignore inner poles. */
    int ku[4], kum1, code;

    /* We would like to apply iterations for enumerating the kum curve parts,
       but the roundinmg errors would be too complicated due to
       the dependence on the direction. Note that neigbour
       patches may use the opposite direction for same bounding curve.
       We apply the recursive dichotomy, in which 
       the rounding errors do not depend on the direction. */
    ku[0] = curve_samples(p->pole[0], 1, pfs->fixed_flat);
    ku[3] = curve_samples(p->pole[0], 1, pfs->fixed_flat);
    kum1 = max(ku[0], ku[3]);
    code = fill_wedges(pfs, ku[0], kum, &p->pole[0][0], 4, &p->c[0][0], &p->c[0][1]);
    if (code < 0)
	return code;
    code = fill_wedges(pfs, ku[3], kum, &p->pole[0][3], 4, &p->c[1][0], &p->c[1][1]);
    if (code < 0)
	return code;
    return decompose_stripe(pfs, p, kum1);
}

private inline bool
is_curve_x_monotonic(const gs_fixed_point *pole, int pole_step)
{   /* true = monotonic, false = don't know. */
    return (pole[0 * pole_step].x <= pole[1 * pole_step].x && 
	    pole[1 * pole_step].x <= pole[2 * pole_step].x &&
	    pole[2 * pole_step].x <= pole[3 * pole_step].x) ||
	   (pole[0 * pole_step].x >= pole[1 * pole_step].x && 
	    pole[1 * pole_step].x >= pole[2 * pole_step].x &&
	    pole[2 * pole_step].x >= pole[3 * pole_step].x);
}

private inline bool
is_curve_y_monotonic(const gs_fixed_point *pole, int pole_step)
{   /* true = monotonic, false = don't know. */
    return (pole[0 * pole_step].y <= pole[1 * pole_step].y && 
	    pole[1 * pole_step].y <= pole[2 * pole_step].y &&
	    pole[2 * pole_step].y <= pole[3 * pole_step].y) ||
	   (pole[0 * pole_step].y >= pole[1 * pole_step].y && 
	    pole[1 * pole_step].y >= pole[2 * pole_step].y &&
	    pole[2 * pole_step].y >= pole[3 * pole_step].y);
}

private inline bool
is_xy_monotonic_by_v(const tensor_patch *p)
{   /* true = monotonic, false = don't know. */
    if (!is_curve_x_monotonic(&p->pole[0][0], 4))
	return false;
    if (!is_curve_x_monotonic(&p->pole[0][1], 4))
	return false;
    if (!is_curve_x_monotonic(&p->pole[0][2], 4))
	return false;
    if (!is_curve_x_monotonic(&p->pole[0][3], 4))
	return false;
    if (!is_curve_y_monotonic(&p->pole[0][0], 4))
	return false;
    if (!is_curve_y_monotonic(&p->pole[0][1], 4))
	return false;
    if (!is_curve_y_monotonic(&p->pole[0][2], 4))
	return false;
    if (!is_curve_y_monotonic(&p->pole[0][3], 4))
	return false;
    return true;
}

private inline bool
is_curve_x_small(const gs_fixed_point *pole, int pole_step, fixed fixed_flat)
{   /* true = monotonic, false = don't know. */
    return any_abs(pole[0 * pole_step].x - pole[1 * pole_step].x) < fixed_flat &&
	   any_abs(pole[1 * pole_step].x - pole[2 * pole_step].x) < fixed_flat &&
	   any_abs(pole[2 * pole_step].x - pole[3 * pole_step].x) < fixed_flat &&
	   any_abs(pole[0 * pole_step].x - pole[3 * pole_step].x) < fixed_flat;
}

private inline bool
is_curve_y_small(const gs_fixed_point *pole, int pole_step, fixed fixed_flat)
{   /* true = monotonic, false = don't know. */
    return any_abs(pole[0 * pole_step].y - pole[1 * pole_step].y) < fixed_flat &&
	   any_abs(pole[1 * pole_step].y - pole[2 * pole_step].y) < fixed_flat &&
	   any_abs(pole[2 * pole_step].y - pole[3 * pole_step].y) < fixed_flat &&
	   any_abs(pole[0 * pole_step].y - pole[3 * pole_step].y) < fixed_flat;
}

private inline bool
is_patch_narrow(const patch_fill_state_t * pfs, const tensor_patch *p)
{
    if (!is_curve_x_small(&p->pole[0][0], 4, pfs->fixed_flat))
	return false;
    if (!is_curve_x_small(&p->pole[0][1], 4, pfs->fixed_flat))
	return false;
    if (!is_curve_x_small(&p->pole[0][2], 4, pfs->fixed_flat))
	return false;
    if (!is_curve_x_small(&p->pole[0][3], 4, pfs->fixed_flat))
	return false;
    if (!is_curve_y_small(&p->pole[0][0], 4, pfs->fixed_flat))
	return false;
    if (!is_curve_y_small(&p->pole[0][1], 4, pfs->fixed_flat))
	return false;
    if (!is_curve_y_small(&p->pole[0][2], 4, pfs->fixed_flat))
	return false;
    if (!is_curve_y_small(&p->pole[0][3], 4, pfs->fixed_flat))
	return false;
    return true;
}

private int 
fill_patch(patch_fill_state_t * pfs, const tensor_patch *p, int kv, int kum)
{
    if (kv <= 1 && (is_patch_narrow(pfs, p) || is_xy_monotonic_by_v(p)))
	return fill_stripe(pfs, p, kum);
    else {
	tensor_patch s0, s1;
	int code;

	if (kv <= 1) {
	    fill_wedges(pfs, 2, 1, p->pole[0], 1, &p->c[0][0], &p->c[0][1]);
	    fill_wedges(pfs, 2, 1, p->pole[3], 1, &p->c[1][0], &p->c[1][1]);
	} else {
	    /* Nothing to do, because patch_fill processed wedges over kvm. */
	    /* The wedges over kvm are not processed here,
	       because with a non-monotonic curve it would cause 
	       redundant filling of the wedge parts. 
	       We prefer to consider such wedge as a poligon
	       rather than a set of overlapping triangles. */
	}
	split_patch(pfs, &s0, &s1, p);
	draw_patch(&s0, true, RGB(0, 128, 0));
	draw_patch(&s1, true, RGB(0, 128, 0));
	code = fill_patch(pfs, &s0, kv / 2, kum);
	if (code < 0)
	    return code;
	return fill_patch(pfs, &s1, kv / 2, kum);
    }
}

private inline fixed
lcp1(fixed p0, fixed p3)
{   /* Computing the 1st pole of a 3d order besier, which applears a line. */
    return (p0 + p0 + p3) / 3;
}
lcp2(fixed p0, fixed p3)
{   /* Computing the 2nd pole of a 3d order besier, which applears a line. */
    return (p0 + p3 + p3) / 3;
}

private inline void
patch_set_color(const patch_fill_state_t * pfs, patch_color_t *c, const float *cc)
{
    const gs_color_space *pcs = pfs->direct_space;
    int n = pcs->type->num_components(pcs);

    if (pfs->Function) 
	c->t = cc[0];\
    else 
	memcpy(c->cc.paint.values, cc, sizeof(c->cc.paint.values[0]) * n);
}

private void
make_tensor_patch(const patch_fill_state_t * pfs, tensor_patch *p, const patch_curve_t curve[4],
	   const gs_fixed_point interior[4])
{
    p->pole[0][0] = curve[0].vertex.p;
    p->pole[0][1] = curve[0].control[0];
    p->pole[0][2] = curve[0].control[1];
    p->pole[0][3] = curve[1].vertex.p;
    p->pole[1][3] = curve[1].control[0];
    p->pole[2][3] = curve[1].control[1];
    p->pole[3][3] = curve[2].vertex.p;
    p->pole[3][2] = curve[2].control[0];
    p->pole[3][1] = curve[2].control[1];
    p->pole[3][0] = curve[3].vertex.p;
    p->pole[2][0] = curve[3].control[0];
    p->pole[1][0] = curve[3].control[1];
    if (interior != NULL) {
	p->pole[1][1] = interior[0];
	p->pole[2][1] = interior[1];
	p->pole[2][2] = interior[2];
	p->pole[1][2] = interior[3];
    } else {
	p->pole[1][1].x = lcp1(p->pole[0][1].x, p->pole[3][1].x) +
			  lcp1(p->pole[1][0].x, p->pole[1][3].x) -
			  lcp1(lcp1(p->pole[0][0].x, p->pole[0][3].x),
			       lcp1(p->pole[3][0].x, p->pole[3][3].x));
	p->pole[1][2].x = lcp1(p->pole[0][2].x, p->pole[3][2].x) +
			  lcp2(p->pole[1][0].x, p->pole[1][3].x) -
			  lcp1(lcp2(p->pole[0][0].x, p->pole[0][3].x),
			       lcp2(p->pole[3][0].x, p->pole[3][3].x));
	p->pole[2][1].x = lcp2(p->pole[0][1].x, p->pole[3][1].x) +
			  lcp1(p->pole[2][0].x, p->pole[2][3].x) -
			  lcp2(lcp1(p->pole[0][0].x, p->pole[0][3].x),
			       lcp1(p->pole[3][0].x, p->pole[3][3].x));
	p->pole[2][2].x = lcp2(p->pole[0][2].x, p->pole[3][2].x) +
			  lcp2(p->pole[2][0].x, p->pole[2][3].x) -
			  lcp2(lcp2(p->pole[0][0].x, p->pole[0][3].x),
			       lcp2(p->pole[3][0].x, p->pole[3][3].x));

	p->pole[1][1].y = lcp1(p->pole[0][1].y, p->pole[3][1].y) +
			  lcp1(p->pole[1][0].y, p->pole[1][3].y) -
			  lcp1(lcp1(p->pole[0][0].y, p->pole[0][3].y),
			       lcp1(p->pole[3][0].y, p->pole[3][3].y));
	p->pole[1][2].y = lcp1(p->pole[0][2].y, p->pole[3][2].y) +
			  lcp2(p->pole[1][0].y, p->pole[1][3].y) -
			  lcp1(lcp2(p->pole[0][0].y, p->pole[0][3].y),
			       lcp2(p->pole[3][0].y, p->pole[3][3].y));
	p->pole[2][1].y = lcp2(p->pole[0][1].y, p->pole[3][1].y) +
			  lcp1(p->pole[2][0].y, p->pole[2][3].y) -
			  lcp2(lcp1(p->pole[0][0].y, p->pole[0][3].y),
			       lcp1(p->pole[3][0].y, p->pole[3][3].y));
	p->pole[2][2].y = lcp2(p->pole[0][2].y, p->pole[3][2].y) +
			  lcp2(p->pole[2][0].y, p->pole[2][3].y) -
			  lcp2(lcp2(p->pole[0][0].y, p->pole[0][3].y),
			       lcp2(p->pole[3][0].y, p->pole[3][3].y));

    }
    patch_set_color(pfs, &p->c[0][0], curve[0].vertex.cc);
    patch_set_color(pfs, &p->c[0][1], curve[1].vertex.cc);
    patch_set_color(pfs, &p->c[1][1], curve[2].vertex.cc);
    patch_set_color(pfs, &p->c[1][0], curve[3].vertex.cc);
}

private int
patch_fill(patch_fill_state_t * pfs, const patch_curve_t curve[4],
	   const gs_fixed_point interior[4],
	   void (*transform) (gs_fixed_point *, const patch_curve_t[4],
			      const gs_fixed_point[4], floatp, floatp))
{
    tensor_patch p;
    int kv[4], kvm, vi, ku[4], kum, km;
    int code = 0;
    gs_fixed_point buf[33];
    gs_memory_t *memory = pfs->pis->memory;

    vd_get_dc('s');
    vd_set_shift(0, 0);
    vd_set_scale(0.01);
    vd_set_origin(0, 0);
    vd_erase(RGB(192, 192, 192));
    /* We decompose the patch into tiny quadrangles,
       possibly inserting wedges between them against a dropout. */
    make_tensor_patch(pfs, &p, curve, interior);
    draw_patch(&p, true, RGB(0, 0, 0));
    kv[0] = curve_samples(p.pole[0], 1, pfs->fixed_flat);
    kv[1] = curve_samples(p.pole[1], 1, pfs->fixed_flat);
    kv[2] = curve_samples(p.pole[2], 1, pfs->fixed_flat);
    kv[3] = curve_samples(p.pole[3], 1, pfs->fixed_flat);
    kvm = max(max(kv[0], kv[1]), max(kv[2], kv[3]));
    ku[0] = curve_samples(&p.pole[0][0], 4, pfs->fixed_flat);
    ku[3] = curve_samples(&p.pole[0][3], 4, pfs->fixed_flat);
    kum = max(ku[0], ku[3]);
    km = max(kvm, kum);
    if (km + 1 > count_of(buf)) {
	/* km may be randomly big with a patch which looks as a triangle in XY. */
	pfs->wedge_buf = (gs_fixed_point *)gs_alloc_bytes(memory, 
		sizeof(gs_fixed_point) * (km + 1), "patch_fill");
	if (pfs->wedge_buf == NULL)
	    return_error(gs_error_VMerror);
    } else
	pfs->wedge_buf = buf;
    code = fill_wedges(pfs, kv[0], kvm, p.pole[0], 1, &p.c[0][0], &p.c[0][1]);
    if (code >= 0)
	code = fill_wedges(pfs, kv[3], kvm, p.pole[3], 1, &p.c[1][0], &p.c[1][1]);
    if (code >= 0) {
	/* We would like to apply iterations for enumerating the kvm curve parts,
	   but the roundinmg errors would be too complicated due to
	   the dependence on the direction. Note that neigbour
	   patches may use the opposite direction for same bounding curve.
	   We apply the recursive dichotomy, in which 
	   the rounding errors do not depend on the direction. */
	code = fill_patch(pfs, &p, kvm, kum);
    }
    if (km + 1 > count_of(buf))
	gs_free_object(memory, pfs->wedge_buf, "patch_fill");
    pfs->wedge_buf = NULL;
    vd_release_dc;
    return code;
}

#endif
