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

/*Id: gscrd.h */
/* Interface for CIE color rendering dictionary creation */

#ifndef gscrd_INCLUDED
#  define gscrd_INCLUDED

#include "gscie.h"

/*
 * Initialize a CRD given all of the relevant parameters.
 * Any of the pointers except WhitePoint may be zero, meaning
 * use the default values.
 *
 * The actual point, matrix, range, and procedure values are copied into the
 * CRD, but only the pointer to the color lookup table is copied.
 */
int
  gs_CIEABC_render1_initialize(P13(gs_cie_render *pcrd,
				   const gs_vector3 *WhitePoint,
				   const gs_vector3 *BlackPoint,
				   const gs_matrix3 *MatrixPQR,
				   const gs_range3 *RangePQR,
				   const gs_cie_transform_proc3 *TransformPQR,
				   const gs_matrix3 *MatrixLMN,
				   const gs_cie_render_proc3 *EncodeLMN,
				   const gs_range3 *RangeLMN,
				   const gs_matrix3 *MatrixABC,
				   const gs_cie_render_proc3 *EncodeABC,
				   const gs_range3 *RangeABC,
				   const gx_color_lookup_table *RenderTable));

#endif					/* gscrd_INCLUDED */
