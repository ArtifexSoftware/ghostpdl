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
/* Internal definitions for triangle shading rendering */

#ifndef gxshade4_INCLUDED
#  define gxshade4_INCLUDED

/* Configuration flags for development needs only. Users should not modify them. */
#define NEW_SHADINGS 1 /* Old code = 0, new code = 1. */
#define QUADRANGLES 0 /* 0 = decompose by triangles, 1 = by quadrangles. */
#define DIVIDE_BY_PARALLELS 0 /* 1 - divide a triangle by parallels, 0 - in 4 triangles.  */
#define POLYGONAL_WEDGES 0 /* 1 = polygons allowed, 0 = triangles only. */
#define INTERPATCH_PADDING (fixed_1 / 8) /* Emulate a trapping for poorly designed documents. */
#define COLOR_CONTIGUITY 1 /* A smothness divisor for triangulation. */
#define TENSOR_SHADING_DEBUG 0
#define VD_DRAW_CIRCLES 0
#define VD_TRACE_DOWN 0
/* End of configuration flags (we don't mean that users should modify the rest). */

#define mesh_max_depth (16 * 3 + 1)	/* each recursion adds 3 entries */
typedef struct mesh_frame_s {	/* recursion frame */
    mesh_vertex_t va, vb, vc;	/* current vertices */
    bool check_clipping;
} mesh_frame_t;
/****** NEED GC DESCRIPTOR ******/

/*
 * Define the fill state structure for triangle shadings.  This is used
 * both for the Gouraud triangle shading types and for the Coons and
 * tensor patch types.
 *
 * The shading pointer is named pshm rather than psh in case subclasses
 * also want to store a pointer of a more specific type.
 */
#define mesh_fill_state_common\
  shading_fill_state_common;\
  const gs_shading_mesh_t *pshm;\
  gs_fixed_rect rect;\
  int depth;\
  mesh_frame_t frames[mesh_max_depth]
typedef struct mesh_fill_state_s {
    mesh_fill_state_common;
} mesh_fill_state_t;
/****** NEED GC DESCRIPTOR ******/

#if NEW_SHADINGS
/* Define the common state for rendering Coons and tensor patches. */
typedef struct patch_fill_state_s {
    mesh_fill_state_common;
    const gs_function_t *Function;
#if NEW_SHADINGS
    bool vectorization;
#   if POLYGONAL_WEDGES
    gs_fixed_point *wedge_buf;
#   endif
    gs_client_color color_domain;
    fixed fixed_flat;
    double smoothness;
#endif
} patch_fill_state_t;

#endif
/* Define a color to be used in curve rendering. */
/* This may be a real client color, or a parametric function argument. */
typedef struct patch_color_s {
    float t;			/* parametric value */
    gs_client_color cc;
} patch_color_t;

/* Define a structure for mesh or patch vertex. */
struct shading_vertex_s {
    gs_fixed_point p;
    patch_color_t c;
};

#if TENSOR_SHADING_DEBUG
    extern int triangle_cnt, patch_cnt;
#endif

/* Initialize the fill state for triangle shading. */
void mesh_init_fill_state(mesh_fill_state_t * pfs,
			  const gs_shading_mesh_t * psh,
			  const gs_rect * rect,
			  gx_device * dev, gs_imager_state * pis);

/* Fill one triangle in a mesh. */
void mesh_init_fill_triangle(mesh_fill_state_t * pfs,
			     const mesh_vertex_t *va,
			     const mesh_vertex_t *vb,
			     const mesh_vertex_t *vc, bool check_clipping);
int mesh_fill_triangle(mesh_fill_state_t * pfs);

#if NEW_SHADINGS
void init_patch_fill_state(patch_fill_state_t *pfs);

int triangle(patch_fill_state_t *pfs, 
    const shading_vertex_t *p0, const shading_vertex_t *p1, const shading_vertex_t *p2);

int padding(patch_fill_state_t *pfs, const gs_fixed_point *p0, const gs_fixed_point *p1, 
	    const patch_color_t *c0, const patch_color_t *c1);
#endif

#endif /* gxshade4_INCLUDED */
