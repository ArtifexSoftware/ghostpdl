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
/* The code QUADRANGLES 1 appears unuseful without having a low level function
   for filling a trapezoid with a linear color. Maybe we'll define sunch function later,
   therefore we keep this branch. It stores a valuable code for constant_color_quadrangle,
   which decomposes a random quadrangle into 3 or 4 trapezoids.
   Without the linear color function, the color approximation looks
   worse than with triangles, and works some slower.
 */
#define DIVIDE_BY_PARALLELS 0 /* 1 - divide a triangle by parallels, 0 - in 4 triangles.  */
/* The code DIVIDE_BY_PARALLELS 1 appears faster due to a smaller decomposition,
   because it is optimized for constant color areas.
   When smoothness is smaller than the device color resolution,
   it stops the decomposition exactly when riches the smoothness.
   This is faster and is conforming to PLRM, but gives a worse view due to
   the color contiguity is missed. May be preferrable for contone devices.
   We'll use it if we decide to define a new low level device virtual function
   for filling a trapezoid with a linear color.

   This mode doesn't work with LASY_WEDGES because neighbour areas
   may get independent parallels, but make_wedge_median
   assumes a dichotomy. Probably we should remove lazy wedges from 
   divide_triangle, divide_triangle_by_parallels, divide_quadrangle_by_parallels.
 */
#define POLYGONAL_WEDGES 0 /* 1 = polygons allowed, 0 = triangles only. */
/* With POLYGONAL_WEDGES 0 an n-vertex wedge is represented as
   n * log2(n) triangles, and the time consumption for it is O(n * log2(n)).
   With POLYGONAL_WEDGES 1 the time consumption is O(n), but it calls
   a heavy function intersection_of_small_bars, which includes
   a special effort against numeric errors. We're not sure
   that the small speeding up worth the trouble with the numeric error effort.
   Note that intersection_of_small_bars is used with QUADRANGLES 1.
 */
#define INTERPATCH_PADDING (fixed_1 / 8) /* Emulate a trapping for poorly designed documents. */
/* When INTERPATCH_PADDING > 0, it generates paddings between patches.
   This is an emulation of Adobe's trapping.
   The value specifies the width of paddings.
 */
#define COLOR_CONTIGUITY 1 /* A smothness divisor for triangulation. */
/* This is a coefficient, which DIVIDE_BY_PARALLELS 1 uses to rich
   a better color contiguity. The value 1 corresponds to PLRM,
   bigger values mean more contiguity. The spead decreases as
   a square of COLOR_CONTIGUITY.
 */
#define LAZY_WEDGES 1 /* 0 = fill immediately, 1 = fill lazy. */
/* This mode delays creating wedges for a boundary until
   both neoghbour areas are painted. At that moment we can know
   all subdivision points for both rights and left areas,
   and skip wedges for common points. Therefore the number of wadges 
   dramatically reduce, causing a significant speedup.
   The LAZY_WEDGES 0 mode was not systematically tested.

   This mode doesn't work with DIVIDE_BY_PARALLELS because neighbour areas
   may get independent parallels, but make_wedge_median
   assumes a dichotomy. Probably we should remove lazy wedges from 
   divide_triangle, divide_triangle_by_parallels, divide_quadrangle_by_parallels.
 */
#define VD_TRACE_DOWN 0 /* Developer's needs, not important for production. */
#define NOFILL_TEST 0 /* Developer's needs, must be off for production. */
#define SKIP_TEST 0 /* Developer's needs, must be off for production. */
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
typedef struct wedge_vertex_list_elem_s wedge_vertex_list_elem_t;
struct wedge_vertex_list_elem_s {
    gs_fixed_point p;
    wedge_vertex_list_elem_t *next, *prev;
};
typedef struct {
    bool last_side;
    bool divided_left, divided_right;
    wedge_vertex_list_elem_t *beg, *end;    
} wedge_vertex_list_t;

#define LAZY_WEDGES_MAX_LEVEL 9 /* memory consumption is 
    sizeof(wedge_vertex_list_elem_t) * LAZY_WEDGES_MAX_LEVEL * (1 << LAZY_WEDGES_MAX_LEVEL) */

/* Define the common state for rendering Coons and tensor patches. */
typedef struct patch_fill_state_s {
    mesh_fill_state_common;
    const gs_function_t *Function;
#if NEW_SHADINGS
    bool vectorization;
    int n_color_args;
#   if POLYGONAL_WEDGES
        gs_fixed_point *wedge_buf;
#   endif
    fixed max_small_coord; /* Length restriction for intersection_of_small_bars. */
    wedge_vertex_list_elem_t *wedge_vertex_list_elem_buffer;
    wedge_vertex_list_elem_t *free_wedge_vertex;
    int wedge_vertex_list_elem_count;
    int wedge_vertex_list_elem_count_max;
    gs_client_color color_domain;
    fixed fixed_flat;
    double smoothness;
    bool maybe_self_intersecting;
    bool monotonic_color;
#endif
} patch_fill_state_t;
#endif
/* Define a color to be used in curve rendering. */
/* This may be a real client color, or a parametric function argument. */
typedef struct patch_color_s {
    float t[2];			/* parametric value */
    gs_client_color cc;
} patch_color_t;

/* Define a structure for mesh or patch vertex. */
struct shading_vertex_s {
    gs_fixed_point p;
    patch_color_t c;
};

/* Define one segment (vertex and next control points) of a curve. */
typedef struct patch_curve_s {
    mesh_vertex_t vertex;
    gs_fixed_point control[2];
} patch_curve_t;

/* Initialize the fill state for triangle shading. */
void mesh_init_fill_state(mesh_fill_state_t * pfs,
			  const gs_shading_mesh_t * psh,
			  const gs_rect * rect,
			  gx_device * dev, gs_imager_state * pis);

#if !NEW_SHADINGS
/* Fill one triangle in a mesh. */
void mesh_init_fill_triangle(mesh_fill_state_t * pfs,
			     const mesh_vertex_t *va,
			     const mesh_vertex_t *vb,
			     const mesh_vertex_t *vc, bool check_clipping);
int mesh_fill_triangle(mesh_fill_state_t * pfs);
#endif

#if NEW_SHADINGS
int init_patch_fill_state(patch_fill_state_t *pfs);
void term_patch_fill_state(patch_fill_state_t *pfs);

int mesh_triangle(patch_fill_state_t *pfs, 
    const shading_vertex_t *p0, const shading_vertex_t *p1, const shading_vertex_t *p2);

int mesh_padding(patch_fill_state_t *pfs, const gs_fixed_point *p0, const gs_fixed_point *p1, 
	    const patch_color_t *c0, const patch_color_t *c1);

int patch_fill(patch_fill_state_t * pfs, const patch_curve_t curve[4],
	   const gs_fixed_point interior[4],
	   void (*transform) (gs_fixed_point *, const patch_curve_t[4],
			      const gs_fixed_point[4], floatp, floatp));

int wedge_vertex_list_elem_buffer_alloc(patch_fill_state_t *pfs);
void wedge_vertex_list_elem_buffer_free(patch_fill_state_t *pfs);
#endif

#endif /* gxshade4_INCLUDED */
