/* Copyright (C) 1999, 2000 Aladdin Enterprises.  All rights reserved.

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
/* Color space management and color value writing for pdfwrite driver */
#include "math_.h"
#include "string_.h"
#include "gx.h"
#include "gscspace.h"		/* for gscie.h */
#include "gscdevn.h"
#include "gscie.h"
#include "gscindex.h"
#include "gscsepr.h"
#include "gserrors.h"
#include "gsiparm3.h"		/* for pattern colors */
#include "gsmatrix.h"		/* for gspcolor.h */
#include "gxcolor2.h"		/* for gxpcolor.h */
#include "gxdcolor.h"		/* for gxpcolor.h */
#include "gxpcolor.h"		/* for pattern device color types */
#include "gdevpdfx.h"
#include "gdevpdfg.h"
#include "gdevpdfo.h"
#include "stream.h"
#include "strimpl.h"
#include "sstring.h"
#include "szlibx.h"

/* ---------------- Color spaces ---------------- */

/* ------ CIE space testing ------ */

/* Test whether a cached CIE procedure is the identity function. */
#define cie_cache_is_identity(pc)\
  ((pc)->floats.params.is_identity)
#define cie_cache3_is_identity(pca)\
  (cie_cache_is_identity(&(pca)[0]) &&\
   cie_cache_is_identity(&(pca)[1]) &&\
   cie_cache_is_identity(&(pca)[2]))

/*
 * Test whether a cached CIE procedure is an exponential.  A cached
 * procedure is exponential iff f(x) = k*(x^p).  We make a very cursory
 * check for this: we require that f(0) = 0, set k = f(1), set p =
 * log[a](f(a)/k), and then require that f(b) = k*(b^p), where a and b are
 * two arbitrarily chosen values between 0 and 1.  Naturally all this is
 * done with some slop.
 */
#define ia (gx_cie_cache_size / 3)
#define ib (gx_cie_cache_size * 2 / 3)
#define iv(i) ((i) / (double)(gx_cie_cache_size - 1))
#define a iv(ia)
#define b iv(ib)

private bool
cie_values_are_exponential(floatp va, floatp vb, floatp k,
			   float *pexpt)
{
    double p;

    if (fabs(k) < 0.001)
	return false;
    if (va == 0 || (va > 0) != (k > 0))
	return false;
    p = log(va / k) / log(a);
    if (fabs(vb - k * pow(b, p)) >= 0.001)
	return false;
    *pexpt = p;
    return true;
}

private bool
cie_scalar_cache_is_exponential(const gx_cie_scalar_cache * pc, float *pexpt)
{
    double k, va, vb;

    if (fabs(pc->floats.values[0]) >= 0.001)
	return false;
    k = pc->floats.values[gx_cie_cache_size - 1];
    va = pc->floats.values[ia];
    vb = pc->floats.values[ib];
    return cie_values_are_exponential(va, vb, k, pexpt);
}
#define cie_scalar3_cache_is_exponential(pca, expts)\
  (cie_scalar_cache_is_exponential(&(pca)[0], &(expts).u) &&\
   cie_scalar_cache_is_exponential(&(pca)[1], &(expts).v) &&\
   cie_scalar_cache_is_exponential(&(pca)[2], &(expts).w))

private bool
cie_vector_cache_is_exponential(const gx_cie_vector_cache * pc, float *pexpt)
{
    double k, va, vb;

    if (fabs(pc->vecs.values[0].u) >= 0.001)
	return false;
    k = pc->vecs.values[gx_cie_cache_size - 1].u;
    va = pc->vecs.values[ia].u;
    vb = pc->vecs.values[ib].u;
    return cie_values_are_exponential(va, vb, k, pexpt);
}
#define cie_vector3_cache_is_exponential(pca, expts)\
  (cie_vector_cache_is_exponential(&(pca)[0], &(expts).u) &&\
   cie_vector_cache_is_exponential(&(pca)[1], &(expts).v) &&\
   cie_vector_cache_is_exponential(&(pca)[2], &(expts).w))

#undef ia
#undef ib
#undef iv
#undef a
#undef b

/* ------ Color space writing ------ */

/* Define standard and short color space names. */
const pdf_color_space_names_t pdf_color_space_names = {
    PDF_COLOR_SPACE_NAMES
};
const pdf_color_space_names_t pdf_color_space_names_short = {
    PDF_COLOR_SPACE_NAMES_SHORT
};

/*
 * Create a local Device{Gray,RGB,CMYK} color space corresponding to the
 * given number of components.
 */
int
pdf_cspace_init_Device(gs_color_space *pcs, int num_components)
{
    switch (num_components) {
    case 1: gs_cspace_init_DeviceGray(pcs); break;
    case 3: gs_cspace_init_DeviceRGB(pcs); break;
    case 4: gs_cspace_init_DeviceCMYK(pcs); break;
    default: return_error(gs_error_rangecheck);
    }
    return 0;
}

/* Add a 3-element vector to a Cos array or dictionary. */
private int
cos_array_add_vector3(cos_array_t *pca, const gs_vector3 *pvec)
{
    int code = cos_array_add_real(pca, pvec->u);

    if (code >= 0)
	code = cos_array_add_real(pca, pvec->v);
    if (code >= 0)
	code = cos_array_add_real(pca, pvec->w);
    return code;
}
private int
cos_dict_put_c_key_vector3(cos_dict_t *pcd, const char *key,
			   const gs_vector3 *pvec)
{
    cos_array_t *pca = cos_array_alloc(pcd->pdev, "cos_array_from_vector3");
    int code;

    if (pca == 0)
	return_error(gs_error_VMerror);
    code = cos_array_add_vector3(pca, pvec);
    if (code < 0) {
	COS_FREE(pca, "cos_array_from_vector3");
	return code;
    }
    return cos_dict_put_c_key_object(pcd, key, COS_OBJECT(pca));
}

/* Create a Separation or DeviceN color space (internal). */
private int
pdf_separation_color_space(gx_device_pdf *pdev,
			   cos_array_t *pca, const char *csname,
			   const cos_value_t *snames,
			   const gs_color_space *alt_space,
			   const gs_function_t *pfn,
			   const pdf_color_space_names_t *pcsn)
{
    cos_value_t v;
    int code;

    if ((code = cos_array_add(pca, cos_c_string_value(&v, csname))) < 0 ||
	(code = cos_array_add_no_copy(pca, snames)) < 0 ||
	(code = pdf_color_space(pdev, &v, alt_space, pcsn, false)) < 0 ||
	(code = cos_array_add(pca, &v)) < 0 ||
	(code = pdf_function(pdev, pfn, &v)) < 0 ||
	(code = cos_array_add(pca, &v)) < 0
	)
	return code;
    return 0;
}

/*
 * Create a PDF color space corresponding to a PostScript color space.
 * For parameterless color spaces, set *pvalue to a (literal) string with
 * the color space name; for other color spaces, create a cos_dict_t if
 * necessary and set *pvalue to refer to it.
 */
int
pdf_color_space(gx_device_pdf *pdev, cos_value_t *pvalue,
		const gs_color_space *pcs,
		const pdf_color_space_names_t *pcsn,
		bool by_name)
{
    gs_memory_t *mem = pdev->pdf_memory;
    gs_color_space_index csi = gs_color_space_get_index(pcs);
    cos_array_t *pca;
    cos_dict_t *pcd;
    cos_value_t v;
    const gs_cie_common *pciec;
    gs_function_t *pfn;
    int code;

    switch (csi) {
    case gs_color_space_index_DeviceGray:
	cos_c_string_value(pvalue, pcsn->DeviceGray);
	return 0;
    case gs_color_space_index_DeviceRGB:
	cos_c_string_value(pvalue, pcsn->DeviceRGB);
	return 0;
    case gs_color_space_index_DeviceCMYK:
	cos_c_string_value(pvalue, pcsn->DeviceCMYK);
	return 0;
    case gs_color_space_index_Pattern:
	if (!pcs->params.pattern.has_base_space) {
	    cos_c_string_value(pvalue, "/Pattern");
	    return 0;
	}
	break;
    default:
	break;
    }
    /* Space has parameters -- create an array. */
    pca = cos_array_alloc(pdev, "pdf_color_space");
    if (pca == 0)
	return_error(gs_error_VMerror);
    switch (csi) {
    case gs_color_space_index_CIEA: {
	/* Check that we can represent this as a CalGray space. */
	const gs_cie_a *pcie = pcs->params.a;
	gs_vector3 expts;

	if (!(pcie->MatrixA.u == 1 && pcie->MatrixA.v == 1 &&
	      pcie->MatrixA.w == 1 &&
	      pcie->common.MatrixLMN.is_identity))
	    return_error(gs_error_rangecheck);
	if (cie_cache_is_identity(&pcie->caches.DecodeA) &&
	    cie_scalar3_cache_is_exponential(pcie->common.caches.DecodeLMN, expts) &&
	    expts.v == expts.u && expts.w == expts.u
	    ) {
	    DO_NOTHING;
	} else if (cie_cache3_is_identity(pcie->common.caches.DecodeLMN) &&
		   cie_vector_cache_is_exponential(&pcie->caches.DecodeA, &expts.u)
		   ) {
	    DO_NOTHING;
	} else
	    return_error(gs_error_rangecheck);
	code = cos_array_add(pca, cos_c_string_value(&v, "/CalGray"));
	if (code < 0)
	    return code;
	pcd = cos_dict_alloc(pdev, "pdf_color_space(dict)");
	if (pcd == 0)
	    return_error(gs_error_VMerror);
	if (expts.u != 1) {
	    code = cos_dict_put_c_key_real(pcd, "/Gamma", expts.u);
	    if (code < 0)
		return code;
	}
	pciec = (const gs_cie_common *)pcie;
    }
    cal:
    code = cos_dict_put_c_key_vector3(pcd, "/WhitePoint",
				      &pciec->points.WhitePoint);
    if (code < 0)
	return code;
    if (pciec->points.BlackPoint.u != 0 ||
	pciec->points.BlackPoint.v != 0 ||
	pciec->points.BlackPoint.w != 0
	) {
	code = cos_dict_put_c_key_vector3(pcd, "/BlackPoint",
					  &pciec->points.BlackPoint);
	if (code < 0)
	    return code;
    }
    code = cos_array_add(pca, COS_OBJECT_VALUE(&v, pcd));
    break;
    case gs_color_space_index_CIEABC: {
	/* Check that we can represent this as a CalRGB space. */
	const gs_cie_abc *pcie = pcs->params.abc;
	gs_vector3 expts;
	const gs_matrix3 *pmat;

	if (pcie->common.MatrixLMN.is_identity &&
	    cie_cache3_is_identity(pcie->common.caches.DecodeLMN) &&
	    cie_vector3_cache_is_exponential(pcie->caches.DecodeABC, expts)
	    )
	    pmat = &pcie->MatrixABC;
	else if (pcie->MatrixABC.is_identity &&
		 cie_cache3_is_identity(pcie->caches.DecodeABC) &&
		 cie_scalar3_cache_is_exponential(pcie->common.caches.DecodeLMN, expts)
		 )
	    pmat = &pcie->common.MatrixLMN;
	else
	    return_error(gs_error_rangecheck);
	code = cos_array_add(pca, cos_c_string_value(&v, "/CalRGB"));
	if (code < 0)
	    return code;
	pcd = cos_dict_alloc(pdev, "pdf_color_space(dict)");
	if (pcd == 0)
	    return_error(gs_error_VMerror);
	if (expts.u != 1 || expts.v != 1 || expts.w != 1) {
	    code = cos_dict_put_c_key_vector3(pcd, "/Gamma", &expts);
	    if (code < 0)
		return code;
	}
	if (!pmat->is_identity) {
	    cos_array_t *pcma =
		cos_array_alloc(pdev, "pdf_color_space(Matrix)");

	    if (pcma == 0)
		return_error(gs_error_VMerror);
	    if ((code = cos_array_add_vector3(pcma, &pmat->cu)) < 0 ||
		(code = cos_array_add_vector3(pcma, &pmat->cv)) < 0 ||
		(code = cos_array_add_vector3(pcma, &pmat->cw)) < 0 ||
		(code = cos_dict_put(pcd, (const byte *)"/Matrix", 7,
				     COS_OBJECT_VALUE(&v, pcma))) < 0
		)
		return code;
	}
	pciec = (const gs_cie_common *)pcie;
    }
    goto cal;
    case gs_color_space_index_Indexed: {
	const gs_indexed_params *pip = &pcs->params.indexed;
	const gs_color_space *base_space =
	    (const gs_color_space *)&pip->base_space;
	int num_entries = pip->hival + 1;
	int num_components = gs_color_space_num_components(base_space);
	uint table_size = num_entries * num_components;
	/* Guess at the extra space needed for ASCII85 encoding. */
	uint string_size = 1 + table_size * 2 + table_size / 30 + 2;
	uint string_used;
	byte buf[100];		/* arbitrary */
	stream_AXE_state st;
	stream s, es;
	byte *table =
	    gs_alloc_string(mem, string_size, "pdf_color_space(table)");
	byte *palette =
	    gs_alloc_string(mem, table_size, "pdf_color_space(palette)");
	gs_color_space cs_gray;

	if (table == 0 || palette == 0) {
	    gs_free_string(mem, palette, table_size,
			   "pdf_color_space(palette)");
	    gs_free_string(mem, table, string_size,
			   "pdf_color_space(table)");
	    return_error(gs_error_VMerror);
	}
	swrite_string(&s, table, string_size);
	s_init(&es, NULL);
	s_init_state((stream_state *)&st, &s_AXE_template, NULL);
	s_init_filter(&es, (stream_state *)&st, buf, sizeof(buf), &s);
	sputc(&s, '<');
	if (pcs->params.indexed.use_proc) {
	    gs_client_color cmin, cmax;
	    byte *pnext = palette;

	    int i, j;

	    /* Find the legal range for the color components. */
	    for (j = 0; j < num_components; ++j)
		cmin.paint.values[j] = min_long,
		    cmax.paint.values[j] = max_long;
	    gs_color_space_restrict_color(&cmin, base_space);
	    gs_color_space_restrict_color(&cmax, base_space);
	    /*
	     * Compute the palette values, with the legal range for each
	     * one mapped to [0 .. 255].
	     */
	    for (i = 0; i < num_entries; ++i) {
		gs_client_color cc;

		gs_cspace_indexed_lookup(&pcs->params.indexed, i, &cc);
		for (j = 0; j < num_components; ++j) {
		    float v = (cc.paint.values[j] - cmin.paint.values[j])
			* 255 / (cmax.paint.values[j] - cmin.paint.values[j]);

		    *pnext++ = (v <= 0 ? 0 : v >= 255 ? 255 : (byte)v);
		}
	    }
	} else
	    memcpy(palette, pip->lookup.table.data, table_size);
	if (gs_color_space_get_index(base_space) ==
	    gs_color_space_index_DeviceRGB
	    ) {
	    /* Check for an all-gray palette. */
	    int i;

	    for (i = table_size; (i -= 3) >= 0; )
		if (palette[i] != palette[i + 1] ||
		    palette[i] != palette[i + 2]
		    )
		    break;
	    if (i < 0) {
		/* Change the color space to DeviceGray. */
		for (i = 0; i < num_entries; ++i)
		    palette[i] = palette[i * 3];
		table_size = num_entries;
		gs_cspace_init_DeviceGray(&cs_gray);
		base_space = &cs_gray;
	    }
	}
	pwrite(&es, palette, table_size);
	gs_free_string(mem, palette, table_size, "pdf_color_space(palette)");
	sclose(&es);
	sflush(&s);
	string_used = (uint)stell(&s);
	table = gs_resize_string(mem, table, string_size, string_used,
				 "pdf_color_space(table)");
	/*
	 * Since the array is always referenced by name as a resource
	 * rather than being written as a value, even for in-line images,
	 * always use the full name for the color space.
	 */
	if ((code = pdf_color_space(pdev, pvalue, base_space,
				    &pdf_color_space_names, false)) < 0 ||
	    (code = cos_array_add(pca,
			cos_c_string_value(&v, 
					   pdf_color_space_names.Indexed
					   /*pcsn->Indexed*/))) < 0 ||
	    (code = cos_array_add(pca, pvalue)) < 0 ||
	    (code = cos_array_add_int(pca, pip->hival)) < 0 ||
	    (code = cos_array_add_no_copy(pca,
					  cos_string_value(&v, table,
							   string_used))) < 0
	    )
	    return code;
    }
    break;
    case gs_color_space_index_DeviceN:
	pfn = gs_cspace_get_devn_function(pcs);
	/****** CURRENTLY WE ONLY HANDLE Functions ******/
	if (pfn == 0)
	    return_error(gs_error_rangecheck);
	{
	    cos_array_t *psna = 
		cos_array_alloc(pdev, "pdf_color_space(DeviceN)");
	    int i;

	    if (psna == 0)
		return_error(gs_error_VMerror);
	    for (i = 0; i < pcs->params.device_n.num_components; ++i) {
		code = pdf_separation_name(pdev, &v,
					   pcs->params.device_n.names[i]);
		if (code < 0 ||
		    (code = cos_array_add_no_copy(psna, &v)) < 0)
		    return code;
	    }
	    COS_OBJECT_VALUE(&v, psna);
	    if ((code = pdf_separation_color_space(pdev, pca, "/DeviceN", &v,
						   (const gs_color_space *)
					&pcs->params.device_n.alt_space,
					pfn, &pdf_color_space_names)) < 0)
		return code;
	}
	break;
    case gs_color_space_index_Separation:
	pfn = gs_cspace_get_sepr_function(pcs);
	/****** CURRENTLY WE ONLY HANDLE Functions ******/
	if (pfn == 0)
	    return_error(gs_error_rangecheck);
	if ((code = pdf_separation_name(pdev, &v,
					pcs->params.separation.sname)) < 0 ||
	    (code = pdf_separation_color_space(pdev, pca, "/Separation", &v,
					       (const gs_color_space *)
					&pcs->params.separation.alt_space,
					pfn, &pdf_color_space_names)) < 0)
	    return code;
	break;
    case gs_color_space_index_Pattern:
	if ((code = pdf_color_space(pdev, pvalue,
				    (const gs_color_space *)
				    &pcs->params.pattern.base_space,
				    &pdf_color_space_names, false)) < 0 ||
	    (code = cos_array_add(pca,
				  cos_c_string_value(&v, "/Pattern"))) < 0 ||
	    (code = cos_array_add(pca, pvalue)) < 0
	    )
	    return code;
	break;
    default:
	return_error(gs_error_rangecheck);
    }
    /*
     * Register the color space as a resource, since it must be referenced
     * by name rather than directly.
     */
    {
	pdf_resource_t *pres;

	code =
	    pdf_alloc_resource(pdev, resourceColorSpace, gs_no_id, &pres, 0L);
	if (code < 0) {
	    COS_FREE(pca, "pdf_color_space");
	    return code;
	}
	pca->id = pres->object->id;
	COS_FREE(pres->object, "pdf_color_space");
	pres->object = (cos_object_t *)pca;
	cos_write_object(COS_OBJECT(pca), pdev);
    }
    if (by_name) {
	/* Return a resource name rather than an object reference. */
	discard(COS_RESOURCE_VALUE(pvalue, pca));
    } else
	discard(COS_OBJECT_VALUE(pvalue, pca));
    return 0;
}

/* Create colored and uncolored Pattern color spaces. */
private int
pdf_pattern_space(gx_device_pdf *pdev, cos_value_t *pvalue,
		  pdf_resource_t **ppres, const char *cs_name)
{
    if (!*ppres) {
	int code = pdf_begin_resource_body(pdev, resourceColorSpace, gs_no_id,
					   ppres);

	if (code < 0)
	    return code;
	pprints1(pdev->strm, "%s\n", cs_name);
	pdf_end_resource(pdev);
	(*ppres)->object->written = true; /* don't write at end */
    }
    cos_resource_value(pvalue, (*ppres)->object);
    return 0;
}
int
pdf_cs_Pattern_colored(gx_device_pdf *pdev, cos_value_t *pvalue)
{
    return pdf_pattern_space(pdev, pvalue, &pdev->cs_Patterns[0],
			     "[/Pattern]");
}
int
pdf_cs_Pattern_uncolored(gx_device_pdf *pdev, cos_value_t *pvalue)
{
    int ncomp = pdev->color_info.num_components;
    static const char *const pcs_names[5] = {
	0, "[/Pattern /DeviceGray]", 0, "[/Pattern /DeviceRGB]",
	"[/Pattern /DeviceCMYK]"
    };

    return pdf_pattern_space(pdev, pvalue, &pdev->cs_Patterns[ncomp],
			     pcs_names[ncomp]);
}

/* ---------------- Color values ---------------- */

/*
 * Create a Pattern resource referencing an image (currently only an XObject).
 * p_tile is NULL for uncolored patterns or the NULL pattern.
 * m_tile is NULL for colored patterns that fill their bounding box,
 * including the NULL pattern.
 ****** WE DON'T HANDLE NULL PATTERNS YET ******
 */
private uint
tile_size(const gx_strip_bitmap *tile, int depth)
{
    return (tile->rep_width * depth + 7) / 8 * tile->rep_height;
}
private int
pdf_pattern(gx_device_pdf *pdev, const gx_drawing_color *pdc,
	    const gx_color_tile *p_tile, const gx_color_tile *m_tile,
	    cos_stream_t *pcs_image, pdf_resource_t **ppres)
{
    pdf_resource_t *pres;
    int code = pdf_alloc_resource(pdev, resourcePattern, pdc->mask.id, ppres,
				  0L);
    cos_stream_t *pcos;
    cos_dict_t *pcd;
    cos_dict_t *pcd_Resources = cos_dict_alloc(pdev, "pdf_pattern(Resources)");
    const gx_color_tile *tile = (p_tile ? p_tile : m_tile);
    const gx_strip_bitmap *btile = (p_tile ? &p_tile->tbits : &m_tile->tmask);
    uint p_size =
	(p_tile == 0 ? 0 : tile_size(&p_tile->tbits, pdev->color_info.depth));
    uint m_size =
	(m_tile == 0 ? 0 : tile_size(&m_tile->tmask, 1));
    bool mask = p_tile == 0;
    gs_point scale, step;
    char str_matrix[100];	/****** ARBITRARY ******/

    if (code < 0)
	return code;
    /*
     * Acrobat Reader can't handle image Patterns with more than
     * 64K of data.  :-(
     */
    if (max(p_size, m_size) > 65500)
	return_error(gs_error_limitcheck);
    /*
     * We currently can't handle Patterns whose X/Y step isn't parallel
     * to the coordinate axes.
     */
    if (is_xxyy(&tile->step_matrix))
	step.x = tile->step_matrix.xx, step.y = tile->step_matrix.yy;
    else if (is_xyyx(&tile->step_matrix))
	step.x = tile->step_matrix.yx, step.y = tile->step_matrix.xy;
    else
	return_error(gs_error_rangecheck);
    if (pcd_Resources == 0)
	return_error(gs_error_VMerror);
    scale.x = btile->rep_width / (pdev->HWResolution[0] / 72.0);
    scale.y = btile->rep_height / (pdev->HWResolution[1] / 72.0);
    pres = *ppres;
    {
	stream s;

	swrite_string(&s, (byte *)str_matrix, sizeof(str_matrix) - 1);
	pprintg2(&s, "[%g 0 0 %g 0 0]", scale.x, scale.y);
	str_matrix[stell(&s)] = 0;
    }
    {
	cos_dict_t *pcd_XObject = cos_dict_alloc(pdev, "pdf_pattern(XObject)");
	char key[MAX_REF_CHARS + 3];
	cos_value_t v;

	if (pcd_XObject == 0)
	    return_error(gs_error_VMerror);
	sprintf(key, "/R%ld", pcs_image->id);
	COS_OBJECT_VALUE(&v, pcs_image);
	if ((code = cos_dict_put(pcd_XObject, (byte *)key, strlen(key), &v)) < 0 ||
	    (code = cos_dict_put_c_key_object(pcd_Resources, "/XObject",
					      COS_OBJECT(pcd_XObject))) < 0
	    )
	    return code;
    }
    if ((code = cos_dict_put_c_strings(pcd_Resources, "/ProcSet",
				       (mask ? "[/PDF/ImageB]" :
					"[/PDF/ImageC]"))) < 0)
	return code;
    cos_become(pres->object, cos_type_stream);
    pcos = (cos_stream_t *)pres->object;
    pcd = cos_stream_dict(pcos);
    if ((code = cos_dict_put_c_key_int(pcd, "/PatternType", 1)) < 0 ||
	(code = cos_dict_put_c_key_int(pcd, "/PaintType",
				       (mask ? 2 : 1))) < 0 ||
	(code = cos_dict_put_c_key_int(pcd, "/TilingType",
				       tile->tiling_type)) < 0 ||
	(code = cos_dict_put_c_key_object(pcd, "/Resources",
					  COS_OBJECT(pcd_Resources))) < 0 ||
	(code = cos_dict_put_c_strings(pcd, "/BBox", "[0 0 1 1]")) < 0 ||
	(code = cos_dict_put_c_key_string(pcd, "/Matrix", (byte *)str_matrix,
					  strlen(str_matrix))) < 0 ||
	(code = cos_dict_put_c_key_real(pcd, "/XStep", step.x / btile->rep_width)) < 0 ||
	(code = cos_dict_put_c_key_real(pcd, "/YStep", step.y / btile->rep_height)) < 0
	) {
	return code;
    }

    {
	char buf[MAX_REF_CHARS + 6 + 1]; /* +6 for /R# Do\n */

	sprintf(buf, "/R%ld Do\n", pcs_image->id);
	cos_stream_add_bytes(pcos, (const byte *)buf, strlen(buf));
    }

    return 0;
}

/* Set the ImageMatrix, Width, and Height for a Pattern image. */
private void
pdf_set_pattern_image(gs_data_image_t *pic, const gx_strip_bitmap *tile)
{
    pic->ImageMatrix.xx = pic->Width = tile->rep_width;
    pic->ImageMatrix.yy = pic->Height = tile->rep_height;
}

/* Write the mask for a Pattern. */
private int
pdf_put_pattern_mask(gx_device_pdf *pdev, const gx_color_tile *m_tile,
		     cos_stream_t **ppcs_mask)
{
    int w = m_tile->tmask.rep_width, h = m_tile->tmask.rep_height;
    gs_image1_t image;
    pdf_image_writer writer;
    cos_stream_t *pcs_image;
    long pos;
    int code;

    gs_image_t_init_mask_adjust(&image, true, false);
    pdf_set_pattern_image((gs_data_image_t *)&image, &m_tile->tmask);
    if ((code = pdf_begin_write_image(pdev, &writer, gs_no_id, w, h, NULL, false)) < 0 ||
	(pdev->params.MonoImage.Encode &&
	 (code = psdf_CFE_binary(&writer.binary, w, h, true)) < 0) ||
	(code = pdf_begin_image_data(pdev, &writer, (const gs_pixel_image_t *)&image, NULL)) < 0
	)
	return code;
    pcs_image = (cos_stream_t *)writer.pres->object;
    pos = stell(pdev->streams.strm);
    /* Pattern masks are specified in device coordinates, so invert Y. */
    if ((code = pdf_copy_mask_bits(writer.binary.strm, m_tile->tmask.data + (h - 1) * m_tile->tmask.raster, 0, -m_tile->tmask.raster, w, h, 0)) < 0 ||
	(code = cos_stream_add_since(pcs_image, pos)) < 0 ||
	(code = pdf_end_image_binary(pdev, &writer, h)) < 0 ||
	(code = pdf_end_write_image(pdev, &writer)) < 0
	)
	return code;
    *ppcs_mask = pcs_image;
    return 0;
}

/* Write a color value.  rgs is "rg" for fill, "RG" for stroke. */
/* We break out some cases into single-use procedures for readability. */
private int
pdf_put_colored_pattern(gx_device_pdf *pdev, const gx_drawing_color *pdc,
			const psdf_set_color_commands_t *ppscc,
			pdf_resource_t **ppres)
{
    const gx_color_tile *m_tile = pdc->mask.m_tile;
    const gx_color_tile *p_tile = pdc->colors.pattern.p_tile;
    int w = p_tile->tbits.rep_width, h = p_tile->tbits.rep_height;
    gs_color_space cs_Device;
    cos_value_t cs_value;
    pdf_image_writer writer;
    gs_image1_t image;
    cos_stream_t *pcs_image;
    cos_stream_t *pcs_mask = 0;
    cos_value_t v;
    long pos;
    int code = pdf_cs_Pattern_colored(pdev, &v);
    gx_device_psdf ipdev;	/* copied device for image params */

    if (code < 0)
	return code;
    pdf_cspace_init_Device(&cs_Device, pdev->color_info.num_components);
    code = pdf_color_space(pdev, &cs_value, &cs_Device,
			   &pdf_color_space_names, true);
    if (code < 0)
	return code;
    gs_image_t_init_adjust(&image, &cs_Device, false);
    image.BitsPerComponent = 8;
    pdf_set_pattern_image((gs_data_image_t *)&image, &p_tile->tbits);
    if (m_tile) {
	if ((code = pdf_put_pattern_mask(pdev, m_tile, &pcs_mask)) < 0)
	    return code;
    }
    /*
     * Set up a device with modified parameters for computing the image
     * compression filters.  Don't allow downsampling or lossy compression.
     */
    ipdev = *(const gx_device_psdf *)pdev;
    ipdev.params.ColorImage.AutoFilter = false;
    ipdev.params.ColorImage.Downsample = false;
    ipdev.params.ColorImage.Filter = "FlateEncode";
    ipdev.params.ColorImage.filter_template = &s_zlibE_template;
    ipdev.params.ConvertCMYKImagesToRGB = false;
    ipdev.params.GrayImage.AutoFilter = false;
    ipdev.params.GrayImage.Downsample = false;
    ipdev.params.GrayImage.Filter = "FlateEncode";
    ipdev.params.GrayImage.filter_template = &s_zlibE_template;
    if ((code = pdf_begin_write_image(pdev, &writer, gs_no_id, w, h, NULL, false)) < 0 ||
	(code = psdf_setup_image_filters(&ipdev, &writer.binary,
					 (gs_pixel_image_t *)&image,
					 NULL, NULL)) < 0 ||
	(code = pdf_begin_image_data(pdev, &writer, (const gs_pixel_image_t *)&image, &cs_value)) < 0
	)
	return code;
    pcs_image = (cos_stream_t *)writer.pres->object;
    pos = stell(pdev->streams.strm);
    /* Pattern masks are specified in device coordinates, so invert Y. */
    if ((code = pdf_copy_color_bits(writer.binary.strm, p_tile->tbits.data + (h - 1) * p_tile->tbits.raster, 0, -p_tile->tbits.raster, w, h, pdev->color_info.depth >> 3)) < 0 ||
	(code = cos_stream_add_since(pcs_image, pos)) < 0 ||
	(code = pdf_end_image_binary(pdev, &writer, h)) < 0
	)
	return code;
    pcs_image = (cos_stream_t *)writer.pres->object;
    if ((pcs_mask != 0 &&
	 (code = cos_dict_put_c_key_object(cos_stream_dict(pcs_image), "/Mask",
					   COS_OBJECT(pcs_mask))) < 0) ||
	(code = pdf_end_write_image(pdev, &writer)) < 0
	)
	return code;
    code = pdf_pattern(pdev, pdc, p_tile, m_tile, pcs_image, ppres);
    if (code < 0)
	return code;
    cos_value_write(&v, pdev);
    pprints1(pdev->strm, " %s", ppscc->setcolorspace);
    return 0;
}
private int
pdf_put_uncolored_pattern(gx_device_pdf *pdev, const gx_drawing_color *pdc,
			  const psdf_set_color_commands_t *ppscc,
			  pdf_resource_t **ppres)
{
    const gx_color_tile *m_tile = pdc->mask.m_tile;
    cos_value_t v;
    stream *s = pdev->strm;
    int code = pdf_cs_Pattern_uncolored(pdev, &v);
    cos_stream_t *pcs_image;
    gx_drawing_color dc_pure;
    static const psdf_set_color_commands_t no_scc = {0, 0, 0};

    if (code < 0 ||
	(code = pdf_put_pattern_mask(pdev, m_tile, &pcs_image)) < 0 ||
	(code = pdf_pattern(pdev, pdc, NULL, m_tile, pcs_image, ppres)) < 0
	)
	return code;
    cos_value_write(&v, pdev);
    pprints1(s, " %s ", ppscc->setcolorspace);
    color_set_pure(&dc_pure, gx_dc_pure_color(pdc));
    psdf_set_color((gx_device_vector *)pdev, &dc_pure, &no_scc);
    return 0;
}
int
pdf_put_drawing_color(gx_device_pdf *pdev, const gx_drawing_color *pdc,
		      const psdf_set_color_commands_t *ppscc)
{
    if (gx_dc_is_pure(pdc))
	return psdf_set_color((gx_device_vector *) pdev, pdc, ppscc);
    else {
	/* We never halftone, so this must be a Pattern. */
	int code;
	pdf_resource_t *pres;
	cos_value_t v;

	if (pdc->type == gx_dc_type_pattern)
	    code = pdf_put_colored_pattern(pdev, pdc, ppscc, &pres);
	else if (pdc->type == &gx_dc_pure_masked)
	    code = pdf_put_uncolored_pattern(pdev, pdc, ppscc, &pres);
	else
	    return_error(gs_error_rangecheck);
	if (code < 0)
	    return code;
	cos_value_write(cos_resource_value(&v, pres->object), pdev);
	pprints1(pdev->strm, " %s\n", ppscc->setcolorn);
	return 0;
    }
}

/* ---------------- Miscellaneous ---------------- */

/* Set the ProcSets bits corresponding to an image color space. */
void
pdf_color_space_procsets(gx_device_pdf *pdev, const gs_color_space *pcs)
{
    const gs_color_space *pbcs = pcs;

 csw:
    switch (gs_color_space_get_index(pbcs)) {
    case gs_color_space_index_DeviceGray:
    case gs_color_space_index_CIEA:
	/* We only handle CIEBasedA spaces that map to CalGray. */
	pdev->procsets |= ImageB;
	break;
    case gs_color_space_index_Indexed:
	pdev->procsets |= ImageI;
	pbcs = (const gs_color_space *)&pcs->params.indexed.base_space;
	goto csw;
    default:
	pdev->procsets |= ImageC;
	break;
    }
}
