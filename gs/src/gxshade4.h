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

/* Initialize the fill state for triangle shading. */
void mesh_init_fill_state(P5(mesh_fill_state_t * pfs,
			     const gs_shading_mesh_t * psh,
			     const gs_rect * rect,
			     gx_device * dev, gs_imager_state * pis));

/* Fill one triangle in a mesh. */
void mesh_init_fill_triangle(P5(mesh_fill_state_t * pfs,
				const mesh_vertex_t *va,
				const mesh_vertex_t *vb,
				const mesh_vertex_t *vc, bool check_clipping));
int mesh_fill_triangle(P1(mesh_fill_state_t * pfs));

#endif /* gxshade4_INCLUDED */
