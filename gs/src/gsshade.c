/* Copyright (C) 1997, 1998 Aladdin Enterprises.  All rights reserved.

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

/*$Id$ */
/* Constructors for shadings */
#include "gx.h"
#include "gscspace.h"
#include "gserrors.h"
#include "gsstruct.h"		/* for extern_st */
#include "gxshade.h"

/* ---------------- Generic services ---------------- */

/* GC descriptors */
private_st_shading();
private_st_shading_mesh();

/* Check ColorSpace, BBox, and Function (if present). */
/* Free variables: params. */
#define check_CBFD(function, domain, m)\
  BEGIN\
    int code = check_CBFD_proc((const gs_shading_params_t *)(params),\
			       function, domain, m);\
    if ( code < 0 ) return code;\
  END
#define check_CBF(m) check_CBFD(params->Function, params->Domain, m)
private int
check_CBFD_proc(const gs_shading_params_t * params,
		const gs_function_t * function, const float *domain, int m)
{
    int ncomp = gs_color_space_num_components(params->ColorSpace);

    if (ncomp < 0 ||
	(params->have_BBox &&
	 (params->BBox.p.x > params->BBox.q.x ||
	  params->BBox.p.y > params->BBox.q.y))
	)
	return_error(gs_error_rangecheck);
    if (function != 0) {
	if (function->params.m != m || function->params.n != ncomp)
	    return_error(gs_error_rangecheck);
	/*
	 * The Adobe documentation says that the function's domain must
	 * be a superset of the domain defined in the shading dictionary.
	 * However, Adobe implementations apparently don't necessarily
	 * check this ahead of time; therefore, we do the same.
	 */
#if 0				/*************** */
	{
	    int i;

	    for (i = 0; i < m; ++i)
		if (function->params.Domain[2 * i] > domain[2 * i] ||
		    function->params.Domain[2 * i + 1] < domain[2 * i + 1]
		    )
		    return_error(gs_error_rangecheck);
	}
#endif /*************** */
    }
    return 0;
}

/* Check parameters for a mesh shading. */
/* Free variables: params. */
#define check_mesh()\
  BEGIN\
    int code = check_mesh_proc((const gs_shading_mesh_params_t *)(params),\
			       (params)->Function, (params)->Decode);\
    if ( code < 0 ) return code;\
  END
private int
check_mesh_proc(const gs_shading_mesh_params_t * params,
		const gs_function_t * function, const float *decode)
{
    if (!data_source_is_array(params->DataSource)) {
	check_CBFD(function, decode, 1);
	switch (params->BitsPerCoordinate) {
	    case 1:
	    case 2:
	    case 4:
	    case 8:
	    case 12:
	    case 16:
	    case 24:
	    case 32:
		break;
	    default:
		return_error(gs_error_rangecheck);
	}
	switch (params->BitsPerComponent) {
	    case 1:
	    case 2:
	    case 4:
	    case 8:
	    case 12:
	    case 16:
		break;
	    default:
		return_error(gs_error_rangecheck);
	}
    }
    return 0;
}

/* Allocate and initialize a shading. */
/* Free variables: mem, params, ppsh. */
#define alloc_shading(shtype, sttype, stype, sfrproc, cname)\
  BEGIN\
    shtype *psh = gs_alloc_struct(mem, shtype, sttype, cname);\
    if ( psh == 0 )\
      return_error(gs_error_VMerror);\
    psh->head.type = stype;\
    psh->head.fill_rectangle = sfrproc;\
    psh->params = *params;\
    *ppsh = (gs_shading_t *)psh;\
  END
#define alloc_return_shading(shtype, sttype, stype, sfrproc, cname)\
  BEGIN\
    alloc_shading(shtype, sttype, stype, sfrproc, cname);\
    return 0;\
  END

/* ---------------- Function-based shading ---------------- */

private_st_shading_Fb();

/* Allocate and initialize a Function-based shading. */
int
gs_shading_Fb_init(gs_shading_t ** ppsh,
		   const gs_shading_Fb_params_t * params, gs_memory_t * mem)
{
    gs_matrix imat;

    check_CBF(2);
    if (gs_matrix_invert(&params->Matrix, &imat) < 0)
	return_error(gs_error_rangecheck);
    alloc_return_shading(gs_shading_Fb_t, &st_shading_Fb,
			 shading_type_Function_based,
			 gs_shading_Fb_fill_rectangle,
			 "gs_shading_Fb_init");
}

/* ---------------- Axial shading ---------------- */

private_st_shading_A();

/* Allocate and initialize an Axial shading. */
int
gs_shading_A_init(gs_shading_t ** ppsh,
		  const gs_shading_A_params_t * params, gs_memory_t * mem)
{
    check_CBF(1);
    alloc_return_shading(gs_shading_A_t, &st_shading_A,
			 shading_type_Axial,
			 gs_shading_A_fill_rectangle,
			 "gs_shading_A_init");
}

/* ---------------- Radial shading ---------------- */

private_st_shading_R();

/* Allocate and initialize a Radial shading. */
int
gs_shading_R_init(gs_shading_t ** ppsh,
		  const gs_shading_R_params_t * params, gs_memory_t * mem)
{
    check_CBF(1);
    if ((params->Domain != 0 && params->Domain[0] == params->Domain[1]) ||
	params->Coords[2] < 0 || params->Coords[5] < 0
	)
	return_error(gs_error_rangecheck);
    alloc_return_shading(gs_shading_R_t, &st_shading_R,
			 shading_type_Radial,
			 gs_shading_R_fill_rectangle,
			 "gs_shading_R_init");
}

/* ---------------- Free-form Gouraud triangle mesh shading ---------------- */

private_st_shading_FfGt();

/* Allocate and return a shading with BitsPerFlag. */
#define alloc_return_BPF_shading(shtype, sttype, stype, sfrproc, bpf, cname)\
  BEGIN\
    alloc_shading(shtype, sttype, stype, sfrproc, cname);\
    ((shtype *)(*ppsh))->params.BitsPerFlag = bpf;\
    return 0;\
  END

/* Allocate and initialize a Free-form Gouraud triangle mesh shading. */
int
gs_shading_FfGt_init(gs_shading_t ** ppsh,
		 const gs_shading_FfGt_params_t * params, gs_memory_t * mem)
{
    int bpf;

    if (params->Decode != 0 && params->Decode[0] == params->Decode[1])
	return_error(gs_error_rangecheck);
    check_mesh();
    if (!data_source_is_array(params->DataSource))
	switch (bpf = params->BitsPerFlag) {
	    case 2:
	    case 4:
	    case 8:
		break;
	    default:
		return_error(gs_error_rangecheck);
    } else
	bpf = 2;
    alloc_return_BPF_shading(gs_shading_FfGt_t, &st_shading_FfGt,
			     shading_type_Free_form_Gouraud_triangle,
			     gs_shading_FfGt_fill_rectangle, bpf,
			     "gs_shading_FfGt_init");
}

/* -------------- Lattice-form Gouraud triangle mesh shading -------------- */

private_st_shading_LfGt();

/* Allocate and initialize a Lattice-form Gouraud triangle mesh shading. */
int
gs_shading_LfGt_init(gs_shading_t ** ppsh,
		 const gs_shading_LfGt_params_t * params, gs_memory_t * mem)
{
    check_mesh();
    if (params->VerticesPerRow < 2)
	return_error(gs_error_rangecheck);
    alloc_return_shading(gs_shading_LfGt_t, &st_shading_LfGt,
			 shading_type_Lattice_form_Gouraud_triangle,
			 gs_shading_LfGt_fill_rectangle,
			 "gs_shading_LfGt_init");
}

/* ---------------- Coons patch mesh shading ---------------- */

private_st_shading_Cp();

/* Allocate and initialize a Coons patch mesh shading. */
int
gs_shading_Cp_init(gs_shading_t ** ppsh,
		   const gs_shading_Cp_params_t * params, gs_memory_t * mem)
{
    int bpf;

    check_mesh();
    if (!data_source_is_array(params->DataSource))
	switch (bpf = params->BitsPerFlag) {
	    case 2:
	    case 4:
	    case 8:
		break;
	    default:
		return_error(gs_error_rangecheck);
    } else
	bpf = 2;
    alloc_return_BPF_shading(gs_shading_Cp_t, &st_shading_Cp,
			     shading_type_Coons_patch,
			     gs_shading_Cp_fill_rectangle, bpf,
			     "gs_shading_Cp_init");
}

/* ---------------- Tensor product patch mesh shading ---------------- */

private_st_shading_Tpp();

/* Allocate and initialize a Tensor product patch mesh shading. */
int
gs_shading_Tpp_init(gs_shading_t ** ppsh,
		  const gs_shading_Tpp_params_t * params, gs_memory_t * mem)
{
    int bpf;

    check_mesh();
    if (!data_source_is_array(params->DataSource))
	switch (bpf = params->BitsPerFlag) {
	    case 2:
	    case 4:
	    case 8:
		break;
	    default:
		return_error(gs_error_rangecheck);
    } else
	bpf = 2;
    alloc_return_BPF_shading(gs_shading_Tpp_t, &st_shading_Tpp,
			     shading_type_Tensor_product_patch,
			     gs_shading_Tpp_fill_rectangle, bpf,
			     "gs_shading_Tpp_init");
}
