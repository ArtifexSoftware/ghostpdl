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

/*Id: gscrd.c  */
/* CIE color rendering dictionary creation */
#include "math_.h"
#include "gx.h"
#include "gserrors.h"
#include "gsmatrix.h"		/* for gscolor2.h */
#include "gxcspace.h"
#include "gscolor2.h"		/* for gs_set/currentcolorrendering */
#include "gscrd.h"

/*
 * Initialize a CRD given all of the relevant parameters.
 * Any of the pointers except WhitePoint may be zero, meaning
 * use the default values.
 *
 * The actual point, matrix, range, and procedure values are copied into the
 * CRD, but only the pointer to the color lookup table is copied.
 */
int
gs_CIEABC_render1_initialize(gs_cie_render * pcrd,
	       const gs_vector3 * WhitePoint, const gs_vector3 * BlackPoint,
		   const gs_matrix3 * MatrixPQR, const gs_range3 * RangePQR,
			     const gs_cie_transform_proc3 * TransformPQR,
	const gs_matrix3 * MatrixLMN, const gs_cie_render_proc3 * EncodeLMN,
			     const gs_range3 * RangeLMN,
	const gs_matrix3 * MatrixABC, const gs_cie_render_proc3 * EncodeABC,
			     const gs_range3 * RangeABC,
			     const gx_color_lookup_table * RenderTable)
{
    pcrd->points.WhitePoint = *WhitePoint;
    pcrd->points.BlackPoint =
	*(BlackPoint ? BlackPoint : &BlackPoint_default);
    pcrd->MatrixPQR = *(MatrixPQR ? MatrixPQR : &Matrix3_default);
    pcrd->RangePQR = *(RangePQR ? RangePQR : &Range3_default);
    pcrd->TransformPQR =
	*(TransformPQR ? TransformPQR : &TransformPQR_default);
    pcrd->MatrixLMN = *(MatrixLMN ? MatrixLMN : &Matrix3_default);
    pcrd->EncodeLMN = *(EncodeLMN ? EncodeLMN : &Encode_default);
    pcrd->RangeLMN = *(RangeLMN ? RangeLMN : &Range3_default);
    pcrd->MatrixABC = *(MatrixABC ? MatrixABC : &Matrix3_default);
    pcrd->EncodeABC = *(EncodeABC ? EncodeABC : &Encode_default);
    pcrd->RangeABC = *(RangeABC ? RangeABC : &Range3_default);
    if (RenderTable)
	pcrd->RenderTable.lookup = *RenderTable;
    else
	pcrd->RenderTable.lookup.table = 0;
    pcrd->RenderTable.T = RenderTableT_default;
    gs_cie_render_init(pcrd);
    return gs_cie_render_complete(pcrd);
}
