/* Copyright (C) 1998 Aladdin Enterprises.  All rights reserved.

   This file is part of Aladdin Ghostscript.

   Aladdin Ghostscript is distributed with NO WARRANTY OF ANY KIND.  No author
   or distributor accepts any responsibility for the consequences of using it,
   or for whether it serves any particular purpose or works at all, unless he
   or she says so in writing.  Refer to the Aladdin Ghostscript Free Public
   License (the "License") for full details.

   Every copy of Aladdin Ghostscript must include a copy of the License,
   normally in a plain ASCII text file named PUBLIC.  The License grants you
   the right to copy, modify and redistribute Aladdin Ghostscript, but only
   under certain conditions described in the License.  Among other things, the
   License requires that the copyright notice and this notice be preserved on
   all copies.
 */


/* Rendering for Gouraud triangle shadings */
#include "memory_.h"
#include "gx.h"
#include "gserrors.h"
#include "gsmatrix.h"		/* for gscoord.h */
#include "gscoord.h"
#include "gxcspace.h"
#include "gxdcolor.h"
#include "gxdevcli.h"
#include "gxistate.h"
#include "gxpath.h"
#include "gxshade.h"
#include "gxshade4.h"

/* ---------------- Triangle mesh filling ---------------- */

/* Initialize the fill state for triangle shading. */
void
mesh_init_fill_state(mesh_fill_state_t * pfs, const gs_shading_mesh_t * psh,
	       const gs_rect * rect, gx_device * dev, gs_imager_state * pis)
{
    shade_init_fill_state((shading_fill_state_t *) pfs,
			  (const gs_shading_t *)psh, dev, pis);
    pfs->pshm = psh;
    shade_bbox_transform2fixed(rect, pis, &pfs->rect);
}

#define SET_MIN_MAX_3(vmin, vmax, a, b, c)\
  if ( a < b ) vmin = a, vmax = b; else vmin = b, vmax = a;\
  if ( c < vmin ) vmin = c; else if ( c > vmax ) vmax = c

private int
mesh_fill_region(const mesh_fill_state_t * pfs, fixed xa, fixed ya, fixed xb,
      fixed yb, fixed xc, fixed yc, const gs_client_color cc[3], bool check)
{
    const gs_shading_mesh_t *psh = pfs->pshm;
    int ci;

    /*
     * Fill the triangle with vertices at x/ya, x/yb, and x/yc
     * with color cc[0].
     * If check is true, check for whether the triangle is entirely
     * inside the rectangle, entirely outside, or partly inside;
     * if check is false, assume the triangle is entirely inside.
     */
    if (check) {
	fixed xmin, ymin, xmax, ymax;

	SET_MIN_MAX_3(xmin, xmax, xa, xb, xc);
	SET_MIN_MAX_3(ymin, ymax, ya, yb, yc);
	if (xmin >= pfs->rect.p.x && xmax <= pfs->rect.q.x &&
	    ymin >= pfs->rect.p.y && ymax <= pfs->rect.q.y
	    ) {
	    /* The triangle is entirely inside the rectangle. */
	    check = false;
	} else if (xmin >= pfs->rect.q.x || xmax <= pfs->rect.p.x ||
		   ymin >= pfs->rect.q.y || ymax <= pfs->rect.p.y
	    ) {
	    /* The triangle is entirely outside the rectangle. */
	    return 0;
	} else {
/****** CLIP HERE ******/
	}
    }
    /* Check whether the colors fall within the smoothness criterion. */
    for (ci = 0; ci < pfs->num_components; ++ci) {
	float
	      c0 = cc[0].paint.values[ci], c1 = cc[1].paint.values[ci],
	      c2 = cc[2].paint.values[ci];
	float cmin, cmax;

	SET_MIN_MAX_3(cmin, cmax, c0, c1, c2);
	if (cmax - cmin > pfs->cc_max_error[ci])
	    goto recur;
    }
    /* Fill the triangle with the color. */
    {
	gx_device_color dev_color;
	const gs_color_space *pcs = psh->params.ColorSpace;
	gs_imager_state *pis = pfs->pis;
	gx_path *ppath;
	gs_client_color fcc;
	int code;

	fcc.paint = cc[0].paint;
	(*pcs->type->restrict_color) (&fcc, pcs);
	(*pcs->type->remap_color) (&fcc, pcs, &dev_color, pis,
				   pfs->dev, gs_color_select_texture);
/****** SHOULD ADD adjust ON ANY OUTSIDE EDGES ******/
#if 0
	ppath = gx_path_alloc(pis->memory, "Gt_fill");
	gx_path_add_point(ppath, xa, ya);
	gx_path_add_line(ppath, xb, yb);
	gx_path_add_line(ppath, xc, yc);
	code = shade_fill_path((const shading_fill_state_t *)pfs,
			       ppath, &dev_color);
	gx_path_free(ppath, "Gt_fill");
#else
	code = (*dev_proc(pfs->dev, fill_triangle))
	    (pfs->dev, xa, ya, xb - xa, yb - ya, xc - xa, yc - ya,
	     &dev_color, pis->log_op);
#endif
	return code;
    }
    /*
     * Subdivide the triangle and recur.  The only subdivision method
     * that doesn't seem to create anomalous shapes divides the
     * triangle in 4, using the midpoints of each side.
     */
  recur:{
#define midpoint_fast(a,b)\
	arith_rshift_1((a) + (b) + 1)
	fixed
	    xab = midpoint_fast(xa, xb), yab = midpoint_fast(ya, yb),
	    xac = midpoint_fast(xa, xc), yac = midpoint_fast(ya, yc),
	    xbc = midpoint_fast(xb, xc), ybc = midpoint_fast(yb, yc);
#undef midpoint_fast
	gs_client_color rcc[5];
	int i;

	for (i = 0; i < pfs->num_components; ++i) {
	    float
	          ta = cc[0].paint.values[i], tb = cc[1].paint.values[i],
	          tc = cc[2].paint.values[i];

	    rcc[1].paint.values[i] = (ta + tb) * 0.5;
	    rcc[2].paint.values[i] = (ta + tc) * 0.5;
	    rcc[3].paint.values[i] = (tb + tc) * 0.5;
	}
	/* Do the "A" triangle. */
	rcc[0].paint = cc[0].paint;	/* rcc: a,ab,ac,bc,- */
	mesh_fill_region(pfs, xa, ya, xab, yab, xac, yac, rcc, check);
	/* Do the central triangle. */
	mesh_fill_region(pfs, xab, yab, xac, yac, xbc, ybc, rcc + 1, check);
	/* Do the "C" triangle. */
	rcc[4].paint = cc[2].paint;	/* rcc: a,ab,ac,bc,c */
	mesh_fill_region(pfs, xac, yac, xbc, ybc, xc, yc, rcc + 2, check);
	/* Do the "B" triangle. */
	rcc[2].paint = cc[1].paint;	/* rcc: a,ab,b,bc,c */
	mesh_fill_region(pfs, xab, yab, xb, yb, xbc, ybc, rcc + 1, check);
	return 0;
    }
}

int
mesh_fill_triangle(const mesh_fill_state_t * pfs, const gs_fixed_point * pa,
	      const float *pca, const gs_fixed_point * pb, const float *pcb,
		   const gs_fixed_point * pc, const float *pcc, bool check)
{
    gs_client_color cc[3];

    memcpy(cc[0].paint.values, pca, sizeof(cc[0].paint.values));
    memcpy(cc[1].paint.values, pcb, sizeof(cc[1].paint.values));
    memcpy(cc[2].paint.values, pcc, sizeof(cc[2].paint.values));
    return mesh_fill_region(pfs, pa->x, pa->y, pb->x, pb->y,
			    pc->x, pc->y, cc, check);
}

/* ---------------- Gouraud triangle shadings ---------------- */

private int
Gt_next_vertex(const gs_shading_mesh_t * psh, shade_coord_stream_t * cs,
	       mesh_vertex_t * vertex)
{
    int code = shade_next_vertex(cs, vertex);

    if (code >= 0 && psh->params.Function) {
	/* Decode the color with the function. */
	gs_function_evaluate(psh->params.Function, vertex->cc, vertex->cc);
    }
    return code;
}

private int
Gt_fill_triangle(const mesh_fill_state_t * pfs, const mesh_vertex_t * va,
		 const mesh_vertex_t * vb, const mesh_vertex_t * vc)
{
    return mesh_fill_triangle(pfs, &va->p, va->cc, &vb->p, vb->cc,
			      &vc->p, vc->cc, true);
}

int
gs_shading_FfGt_fill_rectangle(const gs_shading_t * psh0, const gs_rect * rect,
			       gx_device * dev, gs_imager_state * pis)
{
    const gs_shading_FfGt_t *psh = (const gs_shading_FfGt_t *)psh0;
    mesh_fill_state_t state;
    shade_coord_stream_t cs;
    int num_bits = psh->params.BitsPerFlag;
    int flag;
    mesh_vertex_t va, vb, vc;

    mesh_init_fill_state(&state, (const gs_shading_mesh_t *)psh, rect,
			 dev, pis);
    shade_next_init(&cs, (const gs_shading_mesh_params_t *)&psh->params,
		    pis);
    while ((flag = shade_next_flag(&cs, num_bits)) >= 0) {
	int code;

	switch (flag) {
	    default:
		return_error(gs_error_rangecheck);
	    case 0:
		if ((code = Gt_next_vertex(state.pshm, &cs, &va)) < 0 ||
		    (code = shade_next_flag(&cs, num_bits)) < 0 ||
		    (code = Gt_next_vertex(state.pshm, &cs, &vb)) < 0 ||
		    (code = shade_next_flag(&cs, num_bits)) < 0
		    )
		    return code;
		goto v2;
	    case 1:
		va = vb;
	    case 2:
		vb = vc;
	      v2:if ((code = Gt_next_vertex(state.pshm, &cs, &vc)) < 0)
		    return code;
		code = Gt_fill_triangle(&state, &va, &vb, &vc);
		if (code < 0)
		    return code;
	}
    }
    return 0;
}

int
gs_shading_LfGt_fill_rectangle(const gs_shading_t * psh0, const gs_rect * rect,
			       gx_device * dev, gs_imager_state * pis)
{
    const gs_shading_LfGt_t *psh = (const gs_shading_LfGt_t *)psh0;
    mesh_fill_state_t state;
    shade_coord_stream_t cs;
    mesh_vertex_t *vertex;
    mesh_vertex_t next;
    int per_row = psh->params.VerticesPerRow;
    int i, code;

    mesh_init_fill_state(&state, (const gs_shading_mesh_t *)psh, rect,
			 dev, pis);
    shade_next_init(&cs, (const gs_shading_mesh_params_t *)&psh->params,
		    pis);
    vertex = (mesh_vertex_t *)
	gs_alloc_byte_array(pis->memory, per_row, sizeof(*vertex),
			    "gs_shading_LfGt_render");
    if (vertex == 0)
	return_error(gs_error_VMerror);
    for (i = 0; i < per_row; ++i)
	if ((code = Gt_next_vertex(state.pshm, &cs, &vertex[i])) < 0)
	    goto out;
    while (!seofp(cs.s)) {
	code = Gt_next_vertex(state.pshm, &cs, &next);
	if (code < 0)
	    goto out;
	for (i = 1; i < per_row; ++i) {
	    code = Gt_fill_triangle(&state, &vertex[i - 1], &vertex[i], &next);
	    if (code < 0)
		goto out;
	    vertex[i - 1] = next;
	    code = Gt_next_vertex(state.pshm, &cs, &next);
	    if (code < 0)
		goto out;
	    code = Gt_fill_triangle(&state, &vertex[i], &vertex[i - 1], &next);
	    if (code < 0)
		goto out;
	}
	vertex[per_row - 1] = next;
    }
  out:
    gs_free_object(pis->memory, vertex, "gs_shading_LfGt_render");
    return code;
}
