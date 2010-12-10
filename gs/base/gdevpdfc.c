/* Copyright (C) 2001-2006 Artifex Software, Inc.
   All Rights Reserved.
  
   This software is provided AS-IS with no warranty, either express or
   implied.

   This software is distributed under license and may not be copied, modified
   or distributed except as expressly authorized under the terms of that
   license.  Refer to licensing information at http://www.artifex.com/
   or contact Artifex Software, Inc.,  7 Mt. Lassen Drive - Suite A-134,
   San Rafael, CA  94903, U.S.A., +1(415)492-9861, for further information.
*/

/* $Id$ */
/* Color space management and writing for pdfwrite driver */
#include "math_.h"
#include "memory_.h"
#include "gx.h"
#include "gscspace.h"		/* for gscie.h */
#include "gscdevn.h"
#include "gscie.h"
#include "gscindex.h"
#include "gscsepr.h"
#include "stream.h"
#include "gsicc.h"
#include "gserrors.h"
#include "gsfunc.h"		/* required for colour space function evaluation */
#include "gsfunc3.h"		/* Required to create a replacement lineat interpolation function */
#include "gdevpdfx.h"
#include "gdevpdfg.h"
#include "gdevpdfc.h"
#include "gdevpdfo.h"
#include "strimpl.h"
#include "sstring.h"
#include "gxcspace.h"
#include "gxcdevn.h"
#include "gscspace.h"
#include "gsicc_manage.h"

/*
 * PDF doesn't have general CIEBased color spaces.  However, it provides
 * two methods for handling general CIE spaces:
 *
 *	- For PDF 1.2 and above, we note that the transformation from L*a*b*
 *	space to XYZ space is invertible, so we can handle any PostScript
 *	CIEBased space by transforming color values in that space to XYZ,
 *	then inverse-transforming them to L*a*b* and using a PDF Lab space
 *	with the same WhitePoint and BlackPoint and appropriate ranges for
 *	a and b.  This approach has the drawback that Y values outside the
 *	range [0..1] can't be represented: we just clamp them.
 *
 *	- For PDF 1.3 and above, we can create an ICCBased space.  This is
 *	actually necessary, not just an option, because for shadings (also
 *	introduced in PDF 1.3), we want color interpolation to occur in the
 *	original space.
 *
 * The Lab approach is not currently implemented, because it requires
 * transforming all the sample values of images.  The ICCBased approach is
 * implemented for color spaces whose ranges lie within [0..1], which are
 * the only ranges supported by the ICC standard: we think that removing
 * this limitation would also require transforming image sample values.
 */

/* GC descriptors */
public_st_pdf_color_space();

/* ------ CIE space testing ------ */

/* Test whether a cached CIE procedure is the identity function. */
#define CIE_CACHE_IS_IDENTITY(pc)\
  ((pc)->floats.params.is_identity)
#define CIE_CACHE3_IS_IDENTITY(pca)\
  (CIE_CACHE_IS_IDENTITY(&(pca)[0]) &&\
   CIE_CACHE_IS_IDENTITY(&(pca)[1]) &&\
   CIE_CACHE_IS_IDENTITY(&(pca)[2]))

/*
 * Test whether a cached CIE procedure is an exponential.  A cached
 * procedure is exponential iff f(x) = k*(x^p).  We make a very cursory
 * check for this: we require that f(0) = 0, set k = f(1), set p =
 * log[a](f(a)/k), and then require that f(b) = k*(b^p), where a and b are
 * two arbitrarily chosen values between 0 and 1.  Naturally all this is
 * done with some slop.
 */
#define CC_INDEX_A (gx_cie_cache_size / 3)
#define CC_INDEX_B (gx_cie_cache_size * 2 / 3)
#define CC_INDEX_1 (gx_cie_cache_size - 1)
#define CC_KEY(i) ((i) / (double)CC_INDEX_1)
#define CC_KEY_A CC_KEY(CC_INDEX_A)
#define CC_KEY_B CC_KEY(CC_INDEX_B)

static bool
cie_values_are_exponential(floatp v0, floatp va, floatp vb, floatp k,
			   float *pexpt)
{
    double p;

    if (fabs(v0) >= 0.001 || fabs(k) < 0.001)
	return false;
    if (va == 0 || (va > 0) != (k > 0))
	return false;
    p = log(va / k) / log(CC_KEY_A);
    if (fabs(vb - k * pow(CC_KEY_B, p)) >= 0.001)
	return false;
    *pexpt = p;
    return true;
}

static bool
cie_scalar_cache_is_exponential(const gx_cie_scalar_cache * pc, float *pexpt)
{
    return cie_values_are_exponential(pc->floats.values[0],
				      pc->floats.values[CC_INDEX_A],
				      pc->floats.values[CC_INDEX_B],
				      pc->floats.values[CC_INDEX_1],
				      pexpt);
}
#define CIE_SCALAR3_CACHE_IS_EXPONENTIAL(pca, expts)\
  (cie_scalar_cache_is_exponential(&(pca)[0], &(expts).u) &&\
   cie_scalar_cache_is_exponential(&(pca)[1], &(expts).v) &&\
   cie_scalar_cache_is_exponential(&(pca)[2], &(expts).w))

static bool
cie_vector_cache_is_exponential(const gx_cie_vector_cache * pc, float *pexpt)
{
    return cie_values_are_exponential(pc->vecs.values[0].u,
				      pc->vecs.values[CC_INDEX_A].u,
				      pc->vecs.values[CC_INDEX_B].u,
				      pc->vecs.values[CC_INDEX_1].u,
				      pexpt);
}
#define CIE_VECTOR3_CACHE_IS_EXPONENTIAL(pca, expts)\
  (cie_vector_cache_is_exponential(&(pca)[0], &(expts).u) &&\
   cie_vector_cache_is_exponential(&(pca)[1], &(expts).v) &&\
   cie_vector_cache_is_exponential(&(pca)[2], &(expts).w))

#undef CC_INDEX_A
#undef CC_INDEX_B
#undef CC_KEY_A
#undef CC_KEY_B

/*
 * Test whether a cached CIEBasedABC space consists only of a single
 * Decode step followed by a single Matrix step.
 */
static cie_cache_one_step_t
cie_cached_abc_is_one_step(const gs_cie_abc *pcie, const gs_matrix3 **ppmat)
{
    /* The order of steps is, DecodeABC, MatrixABC, DecodeLMN, MatrixLMN. */
    
    if (CIE_CACHE3_IS_IDENTITY(pcie->common.caches.DecodeLMN)) {
	if (pcie->MatrixABC.is_identity) {
	    *ppmat = &pcie->common.MatrixLMN;
	    return ONE_STEP_ABC;
	}
	if (pcie->common.MatrixLMN.is_identity) {
	    *ppmat = &pcie->MatrixABC;
	    return ONE_STEP_ABC;
	}
    }
    if (CIE_CACHE3_IS_IDENTITY(pcie->caches.DecodeABC.caches)) {
	if (pcie->MatrixABC.is_identity) {
	    *ppmat = &pcie->common.MatrixLMN;
	    return ONE_STEP_LMN;
	}
    }
    return ONE_STEP_NOT;
}

/*
 * Test whether a cached CIEBasedABC space is a L*a*b* space.
 */
static bool
cie_scalar_cache_is_lab_lmn(const gs_cie_abc *pcie, int i)
{
    double k = CC_KEY(i);
    double g = (k >= 6.0 / 29 ? k * k * k :
		(k - 4.0 / 29) * (108.0 / 841));

#define CC_V(j,i) (pcie->common.caches.DecodeLMN[j].floats.values[i])
#define CC_WP(uvw) (pcie->common.points.WhitePoint.uvw)
  
    return (fabs(CC_V(0, i) - g * CC_WP(u)) < 0.001 &&
	    fabs(CC_V(1, i) - g * CC_WP(v)) < 0.001 &&
	    fabs(CC_V(2, i) - g * CC_WP(w)) < 0.001
	    );

#undef CC_V
#undef CC_WP
}
static bool
cie_vector_cache_is_lab_abc(const gx_cie_vector_cache3_t *pvc, int i)
{
    const gx_cie_vector_cache *const pc3 = pvc->caches;
    double k = CC_KEY(i);
    double l0 = pc3[0].vecs.params.base,
	l = l0 + k * (pc3[0].vecs.params.limit - l0);
    double a0 = pc3[1].vecs.params.base,
	a = a0 + k * (pc3[1].vecs.params.limit - a0);
    double b0 = pc3[2].vecs.params.base,
	b = b0 + k * (pc3[2].vecs.params.limit - b0);

    return (fabs(cie_cached2float(pc3[0].vecs.values[i].u) -
		 (l + 16) / 116) < 0.001 &&
	    fabs(cie_cached2float(pc3[1].vecs.values[i].u) -
		 a / 500) < 0.001 &&
	    fabs(cie_cached2float(pc3[2].vecs.values[i].w) -
		 b / -200) < 0.001
	    );
}

static bool
cie_is_lab(const gs_cie_abc *pcie)
{
    int i;

    /* Check MatrixABC and MatrixLMN. */
    if (!(pcie->MatrixABC.cu.u == 1 && pcie->MatrixABC.cu.v == 1 &&
	  pcie->MatrixABC.cu.w == 1 &&
	  pcie->MatrixABC.cv.u == 1 && pcie->MatrixABC.cv.v == 0 &&
	  pcie->MatrixABC.cv.w == 0 &&
	  pcie->MatrixABC.cw.u == 0 && pcie->MatrixABC.cw.v == 0 &&
	  pcie->MatrixABC.cw.w == -1 &&
	  pcie->common.MatrixLMN.is_identity
	  ))
	return false;

    /* Check DecodeABC and DecodeLMN. */
    for (i = 0; i <= CC_INDEX_1; ++i)
	if (!(cie_vector_cache_is_lab_abc(&pcie->caches.DecodeABC, i) &&
	      cie_scalar_cache_is_lab_lmn(pcie, i)
	      ))
	    return false;

    return true;
}

#undef CC_INDEX_1
#undef CC_KEY

/* Test whether one or more CIE-based ranges are [0..1]. */
static bool
cie_ranges_are_0_1(const gs_range *prange, int n)
{
    int i;

    for (i = 0; i < n; ++i)
	if (prange[i].rmin != 0 || prange[i].rmax != 1)
	    return false;
    return true;
}

/* ------ Utilities ------ */

/* Add a 3-element vector to a Cos array or dictionary. */
static int
cos_array_add_vector3(cos_array_t *pca, const gs_vector3 *pvec)
{
    int code = cos_array_add_real(pca, pvec->u);

    if (code >= 0)
	code = cos_array_add_real(pca, pvec->v);
    if (code >= 0)
	code = cos_array_add_real(pca, pvec->w);
    return code;
}
static int
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

/*
 * Finish creating a CIE-based color space (Calxxx or Lab.)
 * This procedure is exported for gdevpdfk.c.
 */
int
pdf_finish_cie_space(cos_array_t *pca, cos_dict_t *pcd,
		     const gs_cie_common *pciec)
{
    int code = cos_dict_put_c_key_vector3(pcd, "/WhitePoint",
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
    return cos_array_add_object(pca, COS_OBJECT(pcd));
}

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
pdf_cspace_init_Device(gs_memory_t *mem, gs_color_space **ppcs,
		       int num_components)
{
    switch (num_components) {
    case 1: *ppcs = gs_cspace_new_DeviceGray(mem); break;
    case 3: *ppcs = gs_cspace_new_DeviceRGB(mem); break;
    case 4: *ppcs = gs_cspace_new_DeviceCMYK(mem); break;
    default: return_error(gs_error_rangecheck);
    }
    return 0;
}

static int pdf_delete_base_space_function(gx_device_pdf *pdev, gs_function_t *pfn)
{
    gs_function_ElIn_params_t *params = (gs_function_ElIn_params_t *)&pfn->params;

    gs_free_object(pdev->memory, (void *)params->Domain, "pdf_delete_function");
    gs_free_object(pdev->memory, (void *)params->Range, "pdf_delete_function");
    gs_free_object(pdev->memory, (void *)params->C0, "pdf_delete_function");
    gs_free_object(pdev->memory, (void *)params->C1, "pdf_delete_function");
    gs_free_object(pdev->memory, (void *)pfn, "pdf_delete_function");
    return 0;
}

static int pdf_make_base_space_function(gx_device_pdf *pdev, gs_function_t **pfn,
					int ncomp, float *data_low, float *data_high)
{
    gs_function_ElIn_params_t params;
    float *ptr1, *ptr2;
    int i, code;

    ptr1 = (float *)
        gs_alloc_byte_array(pdev->memory, 2, sizeof(float), "pdf_make_function(Domain)");
    if (ptr1 == 0) {
	return gs_note_error(gs_error_VMerror);
    }
    ptr2 = (float *)
        gs_alloc_byte_array(pdev->memory, 2 * ncomp, sizeof(float), "pdf_make_function(Range)");
    if (ptr2 == 0) {
	gs_free_object(pdev->memory, (void *)ptr1, "pdf_make_function(Range)");
	return gs_note_error(gs_error_VMerror);
    }
    params.m = 1;
    params.n = ncomp;
    params.N = 1.0f;
    ptr1[0] = 0.0f;
    ptr1[1] = 1.0f;
    for (i=0;i<ncomp;i++) {
        ptr2[i*2] = 0.0f;
        ptr2[(i*2) + 1] = 1.0f;
    }
    params.Domain = ptr1;
    params.Range = ptr2;

    ptr1 = (float *)gs_alloc_byte_array(pdev->memory, ncomp, sizeof(float), "pdf_make_function(C0)");
    if (ptr1 == 0) {
	gs_free_object(pdev->memory, (void *)params.Domain, "pdf_make_function(C0)");
	gs_free_object(pdev->memory, (void *)params.Range, "pdf_make_function(C0)");
	return gs_note_error(gs_error_VMerror);
    }
    ptr2 = (float *)gs_alloc_byte_array(pdev->memory, ncomp, sizeof(float), "pdf_make_function(C1)");
    if (ptr2 == 0) {
	gs_free_object(pdev->memory, (void *)params.Domain, "pdf_make_function(C1)");
	gs_free_object(pdev->memory, (void *)params.Range, "pdf_make_function(C1)");
	gs_free_object(pdev->memory, (void *)ptr1, "pdf_make_function(C1)");
	return gs_note_error(gs_error_VMerror);
    }

    for (i=0;i<ncomp;i++) {
	ptr1[i] = data_low[i];
	ptr2[i] = data_high[i];
    }
    params.C0 = ptr1;
    params.C1 = ptr2;
    code = gs_function_ElIn_init(pfn, &params, pdev->memory);
    if (code < 0) {
	gs_free_object(pdev->memory, (void *)params.Domain, "pdf_make_function");
	gs_free_object(pdev->memory, (void *)params.Range, "pdf_make_function");
	gs_free_object(pdev->memory, (void *)params.C0, "pdf_make_function");
	gs_free_object(pdev->memory, (void *)params.C1, "pdf_make_function");
    }
    return code;
}

static void pdf_SepRGB_ConvertToCMYK (float *in, float *out)
{
    float CMYK[4];
    int i;

    if (in[0] <= in[1] && in[0] <= in[2]) {
        CMYK[3] = 1.0 - in[0];
    } else {
        if (in[1]<= in[0] && in[1] <= in[2]) {
    	    CMYK[3] = 1.0 - in[1];
	} else {
	    CMYK[3] = 1.0 - in[2];
	}
    }
    CMYK[0] = 1.0 - in[0] - CMYK[3];
    CMYK[1] = 1.0 - in[1] - CMYK[3];
    CMYK[2] = 1.0 - in[2] - CMYK[3];
    for (i=0;i<4;i++)
        out[i] = CMYK[i];
}

static void pdf_SepCMYK_ConvertToRGB (float *in, float *out)
{
    float RGB[3];

    RGB[0] = in[0] + in[4];
    RGB[1] = in[1] + in[4];
    RGB[2] = in[2] + in[4];

    if (RGB[0] > 1)
	out[0] = 0.0f;
    else
	out[0] = 1 - RGB[0];
    if (RGB[1] > 1)
	out[1] = 0.0f;
    else
	out[1] = 1 - RGB[1];
    if (RGB[2] > 1)
	out[2] = 0.0f;
    else
	out[2] = 1 - RGB[2];
}

/* Create a Separation or DeviceN color space (internal). */
static int
pdf_separation_color_space(gx_device_pdf *pdev,
			   cos_array_t *pca, const char *csname,
			   const cos_value_t *snames,
			   const gs_color_space *alt_space,
			   const gs_function_t *pfn,
			   const pdf_color_space_names_t *pcsn,
			   const cos_value_t *v_attributes)
{
    cos_value_t v;
    const gs_range_t *ranges;
    int code, csi;

    /* We need to think about the alternate space. If we are producing 
     * PDF/X or PDF/A we can't produce some device spaces, and the code in 
     * pdf_color_space_named always allows device spaces. We could alter
     * that code, but by then we don't know its an Alternate space, and have
     * lost the tin transform procedure. So instead we check here.
     */
    csi = gs_color_space_get_index(alt_space);
    /* Note that if csi is ICC, check to see if this was one of 
       the default substitutes that we introduced for DeviceGray,
       DeviceRGB or DeviceCMYK.  If it is, then just write
       the default color.  Depending upon the flavor of PDF,
       or other options, we may want to actually have all
       the colors defined by ICC profiles and not do the following
       substituion of the Device space. */
    if (csi == gs_color_space_index_ICC) {
        csi = gsicc_get_default_type(alt_space->cmm_icc_profile_data);
    }
    if (csi == gs_color_space_index_DeviceRGB && (pdev->PDFX || 
	(pdev->PDFA && (pdev->pcm_color_info_index == gs_color_space_index_DeviceCMYK)))) {

	/* We have a DeviceRGB alternate, but are producing either PDF/X or
	 * PDF/A with a DeviceCMYK process color model. So we need to convert
	 * the alternate space into CMYK. We do this by evaluating the function
	 * at each end of the Separation space (0 and 1), convert the resulting
	 * RGB colours into CMYK and create a new function which linearly 
	 * interpolates between these points.
	 */
	gs_function_t *new_pfn = 0;
        float in[1] = {0.0f};
	float out_low[4];
	float out_high[4];

	code = gs_function_evaluate(pfn, in, out_low);
	if (code < 0)
	    return code;
	pdf_SepRGB_ConvertToCMYK((float *)&out_low, (float *)&out_low);

	in[0] = 1.0f;
	code = gs_function_evaluate(pfn, in, out_high);
	if (code < 0)
	    return code;
	pdf_SepRGB_ConvertToCMYK((float *)&out_high, (float *)&out_high);
	
	code = pdf_make_base_space_function(pdev, &new_pfn, 4, out_low, out_high);
	if (code < 0)
	    return code;
	if ((code = cos_array_add(pca, cos_c_string_value(&v, csname))) < 0 ||
	    (code = cos_array_add_no_copy(pca, snames)) < 0 ||
	    (code = (int)cos_c_string_value(&v, (const char *)pcsn->DeviceCMYK)) < 0 ||
	    (code = cos_array_add(pca, &v)) < 0 ||
	    (code = pdf_function_scaled(pdev, new_pfn, 0x00, &v)) < 0 ||
	    (code = cos_array_add(pca, &v)) < 0 ||
	    (v_attributes != NULL ? code = cos_array_add(pca, v_attributes) : 0) < 0
	    ) {}
	pdf_delete_base_space_function(pdev, new_pfn);
	return code;
    }
    if (csi == gs_color_space_index_DeviceCMYK &&
	(pdev->PDFA && (pdev->pcm_color_info_index == gs_color_space_index_DeviceRGB))) {
	/* We have a DeviceCMYK alternate, but are producingPDF/A with a 
	 * DeviceRGB process color model. See comment above re DviceRGB.
	 */
	gs_function_t *new_pfn = 0;
        float in[1] = {0.0f};
	float out_low[4];
	float out_high[4];

	code = gs_function_evaluate(pfn, in, out_low);
	if (code < 0)
	    return code;
	pdf_SepCMYK_ConvertToRGB((float *)&out_low, (float *)&out_low);

        in[0] = 1.0f;
	code = gs_function_evaluate(pfn, in, out_high);
	if (code < 0)
	    return code;
	pdf_SepCMYK_ConvertToRGB((float *)&out_high, (float *)&out_high);

	code = pdf_make_base_space_function(pdev, &new_pfn, 3, out_low, out_high);
	if (code < 0)
	    return code;
	if ((code = cos_array_add(pca, cos_c_string_value(&v, csname))) < 0 ||
	    (code = cos_array_add_no_copy(pca, snames)) < 0 ||
	    (code = (int)cos_c_string_value(&v, pcsn->DeviceRGB)) < 0 ||
	    (code = cos_array_add(pca, &v)) < 0 ||
	    (code = pdf_function_scaled(pdev, new_pfn, 0x00, &v)) < 0 ||
	    (code = cos_array_add(pca, &v)) < 0 ||
	    (v_attributes != NULL ? code = cos_array_add(pca, v_attributes) : 0) < 0
	    ) {}
	pdf_delete_base_space_function(pdev, new_pfn);
	return code;
    }

    if ((code = cos_array_add(pca, cos_c_string_value(&v, csname))) < 0 ||
	(code = cos_array_add_no_copy(pca, snames)) < 0 ||
	(code = pdf_color_space_named(pdev, &v, &ranges, alt_space, pcsn, false, NULL, 0)) < 0 ||
	(code = cos_array_add(pca, &v)) < 0 ||
	(code = pdf_function_scaled(pdev, pfn, ranges, &v)) < 0 ||
	(code = cos_array_add(pca, &v)) < 0 ||
	(v_attributes != NULL ? code = cos_array_add(pca, v_attributes) : 0) < 0
	)
	return code;
    return 0;
}

/*
 * Create an Indexed color space.  This is a single-use procedure,
 * broken out only for readability.
 */
static int
pdf_indexed_color_space(gx_device_pdf *pdev, cos_value_t *pvalue,
			const gs_color_space *pcs, cos_array_t *pca)
{
    const gs_indexed_params *pip = &pcs->params.indexed;
    const gs_color_space *base_space = pcs->base_space;
    int num_entries = pip->hival + 1;
    int num_components = gs_color_space_num_components(base_space);
    uint table_size = num_entries * num_components;
    /* Guess at the extra space needed for PS string encoding. */
    uint string_size = 2 + table_size * 4;
    uint string_used;
    byte buf[100];		/* arbitrary */
    stream_AXE_state st;
    stream s, es;
    gs_memory_t *mem = pdev->pdf_memory;
    byte *table;
    byte *palette;
    cos_value_t v;
    int code;

    /* PDF doesn't support Indexed color spaces with more than 256 entries. */
    if (num_entries > 256)
	return_error(gs_error_rangecheck);
    if (pdev->CompatibilityLevel < 1.3 && !pdev->ForOPDFRead) {
	switch (gs_color_space_get_index(pcs)) {
	    case gs_color_space_index_Pattern:
	    case gs_color_space_index_Separation:
	    case gs_color_space_index_Indexed:
	    case gs_color_space_index_DeviceN:
		return_error(gs_error_rangecheck);
	    default: DO_NOTHING; 
	}

    }
    table = gs_alloc_string(mem, string_size, "pdf_color_space(table)");
    palette = gs_alloc_string(mem, table_size, "pdf_color_space(palette)");
    if (table == 0 || palette == 0) {
	gs_free_string(mem, palette, table_size,
		       "pdf_color_space(palette)");
	gs_free_string(mem, table, string_size,
		       "pdf_color_space(table)");
	return_error(gs_error_VMerror);
    }
    s_init(&s, mem);
    swrite_string(&s, table, string_size);
    s_init(&es, mem);
    s_init_state((stream_state *)&st, &s_PSSE_template, NULL);
    s_init_filter(&es, (stream_state *)&st, buf, sizeof(buf), &s);
    sputc(&s, '(');
    if (pcs->params.indexed.use_proc) {
	gs_client_color cmin, cmax;
	byte *pnext = palette;
	int i, j;

	/* Find the legal range for the color components. */
	for (j = 0; j < num_components; ++j)
	    cmin.paint.values[j] = (float)min_long,
		cmax.paint.values[j] = (float)max_long;
	gs_color_space_restrict_color(&cmin, base_space);
	gs_color_space_restrict_color(&cmax, base_space);
	/*
	 * Compute the palette values, with the legal range for each
	 * one mapped to [0 .. 255].
	 */
	for (i = 0; i < num_entries; ++i) {
	    gs_client_color cc;

	    gs_cspace_indexed_lookup(pcs, i, &cc);
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
	/* Check for an all-gray palette3. */
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
	    base_space = gs_cspace_new_DeviceGray(mem);
	}
    }
    stream_write(&es, palette, table_size);
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
     *
     * We don't have to worry about the range of the base space:
     * in PDF, unlike PostScript, the values from the lookup table are
     * scaled automatically.
     */
    if ((code = pdf_color_space_named(pdev, pvalue, NULL, base_space,
				&pdf_color_space_names, false, NULL, 0)) < 0 ||
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
    return 0;
}

/* 
 * Find a color space resource by seriialized data. 
 */
static pdf_resource_t *
pdf_find_cspace_resource(gx_device_pdf *pdev, const byte *serialized, uint serialized_size) 
{
    pdf_resource_t **pchain = pdev->resources[resourceColorSpace].chains;
    pdf_resource_t *pres;
    int i;
    
    for (i = 0; i < NUM_RESOURCE_CHAINS; i++) {
	for (pres = pchain[i]; pres != 0; pres = pres->next) {
	    const pdf_color_space_t *const ppcs =
		(const pdf_color_space_t *)pres;
	    if (ppcs->serialized_size != serialized_size)
		continue;
	    if (!memcmp(ppcs->serialized, serialized, ppcs->serialized_size))
		return pres;
	}
    }
    return NULL;
}


/*
 * Create a PDF color space corresponding to a PostScript color space.
 * For parameterless color spaces, set *pvalue to a (literal) string with
 * the color space name; for other color spaces, create a cos_array_t if
 * necessary and set *pvalue to refer to it.  In the latter case, if
 * by_name is true, return a string /Rxxxx rather than a reference to
 * the actual object.
 *
 * If ppranges is not NULL, then if  the domain of the color space had
 * to be scaled (to convert a CIEBased space to ICCBased), store a pointer
 * to the ranges in *ppranges, otherwise set *ppranges to 0.
 */
int
pdf_color_space_named(gx_device_pdf *pdev, cos_value_t *pvalue,
		const gs_range_t **ppranges,
		const gs_color_space *pcs_in,
		const pdf_color_space_names_t *pcsn,
		bool by_name, const byte *res_name, int name_length)
{
    const gs_color_space *pcs;
    gs_color_space_index csi;
    cos_array_t *pca;
    cos_dict_t *pcd;
    cos_value_t v;
    const gs_cie_common *pciec;
    gs_function_t *pfn;
    const gs_range_t *ranges = 0;
    uint serialized_size;
    byte *serialized = NULL, serialized0[100];
    pdf_resource_t *pres = NULL;
    int code;
#if 0
    bool islab = false;
#endif

    /* If color space is CIE based and we have compatibility then go ahead and use the ICC alternative */
    if ((pdev->CompatibilityLevel < 1.3) || !gs_color_space_is_PSCIE(pcs_in) ) {
        pcs = pcs_in;
    } else {
        pcs = pcs_in;
        /* The snippet below creates an ICC equivalent profile for the PS
           color space.  This is disabled until I add the capability to
           specify the profile version to ensure compatability with
           the PDF versions */
#if 0
        if (pcs_in->icc_equivalent != NULL) {
            pcs = pcs_in->icc_equivalent;
        } else {
            /* Need to create the equivalent object */
            gs_colorspace_set_icc_equivalent((gs_color_space *)pcs_in, &islab, pdev->memory);
            pcs = pcs_in->icc_equivalent;
        } 
#endif
    }
    csi = gs_color_space_get_index(pcs);
    /* Note that if csi is ICC, check to see if this was one of 
       the default substitutes that we introduced for DeviceGray,
       DeviceRGB or DeviceCMYK.  If it is, then just write
       the default color.  Depending upon the flavor of PDF,
       or other options, we may want to actually have all
       the colors defined by ICC profiles and not do the following
       substituion of the Device space. */
    if (csi == gs_color_space_index_ICC) {
        csi = gsicc_get_default_type(pcs->cmm_icc_profile_data);
    }
    if (ppranges)
	*ppranges = 0;		/* default */
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
    case gs_color_space_index_ICC:
        /*
	 * Take a special early exit for unrecognized ICCBased color spaces,
	 * or for PDF 1.2 output (ICCBased color spaces date from PDF 1.3).
	 */

        if (pcs->cmm_icc_profile_data == NULL ||
	    pdev->CompatibilityLevel < 1.3
	    ) {
	    if (res_name != NULL)
		return 0; /* Ignore .includecolorspace */
            if (pcs->base_space != NULL) {
            return pdf_color_space_named( pdev, pvalue, ppranges,
                                    pcs->base_space,
                                    pcsn, by_name, NULL, 0);
            } else {
                /* Base space is NULL, use appropriate device space */
                switch( cs_num_components(pcs) )  {
                    case 1:
	                cos_c_string_value(pvalue, pcsn->DeviceGray);
	                return 0;
                    case 3:
	                cos_c_string_value(pvalue, pcsn->DeviceRGB);
	                return 0;
                    case 4:
	                cos_c_string_value(pvalue, pcsn->DeviceCMYK);
	                return 0;
                    default:
                        break;
	}
            }
	}

        break;
    default:
	break;
    }
    if (pdev->params.ColorConversionStrategy == ccs_CMYK && 
	    csi != gs_color_space_index_DeviceCMYK &&
	    csi != gs_color_space_index_DeviceGray &&
	    csi != gs_color_space_index_Pattern)
	return_error(gs_error_rangecheck);
    if (pdev->params.ColorConversionStrategy == ccs_sRGB && 
	    csi != gs_color_space_index_DeviceRGB && 
	    csi != gs_color_space_index_DeviceGray &&
	    csi != gs_color_space_index_Pattern)
	return_error(gs_error_rangecheck);
    if (pdev->params.ColorConversionStrategy == ccs_Gray && 
	    csi != gs_color_space_index_DeviceGray &&
	    csi != gs_color_space_index_Pattern)
	return_error(gs_error_rangecheck);
    /* Check whether we already have a PDF object for this color space. */
    if (pcs->id != gs_no_id)
	pres = pdf_find_resource_by_gs_id(pdev, resourceColorSpace, pcs->id);
    if (pres == NULL) {
	stream s;

	s_init(&s, pdev->memory);
	swrite_position_only(&s);
	code = cs_serialize(pcs, &s);
	if (code < 0)
	    return_error(gs_error_unregistered); /* Must not happen. */
	serialized_size = stell(&s);
	sclose(&s);
	if (serialized_size <= sizeof(serialized0))
	    serialized = serialized0;
	else {
	    serialized = gs_alloc_bytes(pdev->pdf_memory, serialized_size, "pdf_color_space");
	    if (serialized == NULL)
		return_error(gs_error_VMerror);
	}
	swrite_string(&s, serialized, serialized_size);
	code = cs_serialize(pcs, &s);
	if (code < 0)
	    return_error(gs_error_unregistered); /* Must not happen. */
	if (stell(&s) != serialized_size) 
	    return_error(gs_error_unregistered); /* Must not happen. */
	sclose(&s);
	pres = pdf_find_cspace_resource(pdev, serialized, serialized_size);
	if (pres != NULL) {
	    if (serialized != serialized0)
		gs_free_object(pdev->pdf_memory, serialized, "pdf_color_space");
	    serialized = NULL;
	}
    }
    if (pres) {
	const pdf_color_space_t *const ppcs =
	    (const pdf_color_space_t *)pres;

	if (ppranges != 0 && ppcs->ranges != 0)
	    *ppranges = ppcs->ranges;
	pca = (cos_array_t *)pres->object;
	goto ret;
    }

    /* Space has parameters -- create an array. */
    pca = cos_array_alloc(pdev, "pdf_color_space");
    if (pca == 0)
	return_error(gs_error_VMerror);

    switch (csi) {

    case gs_color_space_index_ICC:
	code = pdf_iccbased_color_space(pdev, pvalue, pcs, pca);
        break;

    case gs_color_space_index_CIEA: {
	/* Check that we can represent this as a CalGray space. */
	const gs_cie_a *pcie = pcs->params.a;
	bool unitary = cie_ranges_are_0_1(&pcie->RangeA, 1);
	bool identityA = (pcie->MatrixA.u == 1 && pcie->MatrixA.v == 1 && 
	                  pcie->MatrixA.w == 1);
	gs_vector3 expts;

	pciec = (const gs_cie_common *)pcie;
	if (!pcie->common.MatrixLMN.is_identity) {
	    code = pdf_convert_cie_space(pdev, pca, pcs, "GRAY", pciec,
					 &pcie->RangeA, ONE_STEP_NOT, NULL,
					 &ranges);
	    break;
	}
	if (unitary && identityA &&
	    CIE_CACHE_IS_IDENTITY(&pcie->caches.DecodeA) &&
	    CIE_SCALAR3_CACHE_IS_EXPONENTIAL(pcie->common.caches.DecodeLMN, expts) &&
	    expts.v == expts.u && expts.w == expts.u
	    ) {
	    DO_NOTHING;
	} else if (unitary && identityA &&
		   CIE_CACHE3_IS_IDENTITY(pcie->common.caches.DecodeLMN) &&
		   cie_vector_cache_is_exponential(&pcie->caches.DecodeA, &expts.u)
		   ) {
	    DO_NOTHING;
	} else {
	    code = pdf_convert_cie_space(pdev, pca, pcs, "GRAY", pciec,
					 &pcie->RangeA, ONE_STEP_NOT, NULL,
					 &ranges);
	    break;
	}
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
    }
    cal:
    /* Finish handling a CIE-based color space (Calxxx or Lab). */
    if (code < 0)
	return code;
    code = pdf_finish_cie_space(pca, pcd, pciec);
    break;

    case gs_color_space_index_CIEABC: {
	/* Check that we can represent this as a CalRGB space. */
	const gs_cie_abc *pcie = pcs->params.abc;
	bool unitary = cie_ranges_are_0_1(pcie->RangeABC.ranges, 3);
	gs_vector3 expts;
	const gs_matrix3 *pmat = NULL;
	cie_cache_one_step_t one_step =
	    cie_cached_abc_is_one_step(pcie, &pmat);

	pciec = (const gs_cie_common *)pcie;
	if (unitary) {
	    switch (one_step) {
	    case ONE_STEP_ABC:
		if (CIE_VECTOR3_CACHE_IS_EXPONENTIAL(pcie->caches.DecodeABC.caches, expts))
		    goto calrgb;
		break;
	    case ONE_STEP_LMN:
		if (CIE_SCALAR3_CACHE_IS_EXPONENTIAL(pcie->common.caches.DecodeLMN, expts))
		    goto calrgb;
	    default:
		break;
	    }
	}
	if (cie_is_lab(pcie)) {
	    /* Represent this as a Lab space. */
	    pcd = cos_dict_alloc(pdev, "pdf_color_space(dict)");
	    if (pcd == 0)
		return_error(gs_error_VMerror);
	    code = pdf_put_lab_color_space(pca, pcd, pcie->RangeABC.ranges);
	    goto cal;
	} else {
	    code = pdf_convert_cie_space(pdev, pca, pcs, "RGB ", pciec,
					 pcie->RangeABC.ranges,
					 one_step, pmat, &ranges);
	    break;
	}
    calrgb:
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
    }
    goto cal;

    case gs_color_space_index_CIEDEF:
	code = pdf_convert_cie_space(pdev, pca, pcs, "RGB ",
				     (const gs_cie_common *)pcs->params.def,
				     pcs->params.def->RangeDEF.ranges,
				     ONE_STEP_NOT, NULL, &ranges);
	break;

    case gs_color_space_index_CIEDEFG:
	code = pdf_convert_cie_space(pdev, pca, pcs, "CMYK",
				     (const gs_cie_common *)pcs->params.defg,
				     pcs->params.defg->RangeDEFG.ranges,
				     ONE_STEP_NOT, NULL, &ranges);
	break;

    case gs_color_space_index_Indexed:
	code = pdf_indexed_color_space(pdev, pvalue, pcs, pca);
	break;

    case gs_color_space_index_DeviceN:
	if (!pdev->PreserveDeviceN)
	    return_error(gs_error_rangecheck);
        if (pdev->CompatibilityLevel < 1.3)
	    return_error(gs_error_rangecheck);
	pfn = gs_cspace_get_devn_function(pcs);
	/****** CURRENTLY WE ONLY HANDLE Functions ******/
	if (pfn == 0)
	    return_error(gs_error_rangecheck);
	{
	    cos_array_t *psna = 
		cos_array_alloc(pdev, "pdf_color_space(DeviceN)");
	    int i;
	    byte *name_string;
	    uint name_string_length;
	    cos_value_t v_attriburtes, *va = NULL;

	    if (psna == 0)
		return_error(gs_error_VMerror);
	    for (i = 0; i < pcs->params.device_n.num_components; ++i) {
	 	if ((code = pcs->params.device_n.get_colorname_string(
				  pdev->memory,
				  pcs->params.device_n.names[i], &name_string, 
				  &name_string_length)) < 0 ||
		    (code = pdf_string_to_cos_name(pdev, name_string, 
				  name_string_length, &v)) < 0 ||
		    (code = cos_array_add_no_copy(psna, &v)) < 0)
		    return code;
	    }
	    COS_OBJECT_VALUE(&v, psna);
	    if (pcs->params.device_n.colorants != NULL) {
		cos_dict_t *colorants  = cos_dict_alloc(pdev, "pdf_color_space(DeviceN)");
		cos_value_t v_colorants, v_separation, v_colorant_name;
		const gs_device_n_attributes *csa;
		pdf_resource_t *pres_attributes;

		if (colorants == NULL)
		    return_error(gs_error_VMerror);
		code = pdf_alloc_resource(pdev, resourceOther, 0, &pres_attributes, -1);
		if (code < 0)
		    return code;
		cos_become(pres_attributes->object, cos_type_dict);
		COS_OBJECT_VALUE(&v_colorants, colorants);
		code = cos_dict_put((cos_dict_t *)pres_attributes->object, 
		    (const byte *)"/Colorants", 10, &v_colorants);
		if (code < 0)
		    return code;
		for (csa = pcs->params.device_n.colorants; csa != NULL; csa = csa->next) {
	 	    code = pcs->params.device_n.get_colorname_string(pdev->memory,
				  csa->colorant_name, &name_string, &name_string_length);
		    if (code < 0)
			return code;
		    code = pdf_color_space_named(pdev, &v_separation, NULL, csa->cspace, pcsn, false, NULL, 0);
		    if (code < 0)
			return code;
		    code = pdf_string_to_cos_name(pdev, name_string, name_string_length, &v_colorant_name);
		    if (code < 0)
			return code;
		    code = cos_dict_put(colorants, v_colorant_name.contents.chars.data, 
					v_colorant_name.contents.chars.size, &v_separation);
		    if (code < 0)
			return code;
		}
    		code = pdf_substitute_resource(pdev, &pres_attributes, resourceOther, NULL, true);
		if (code < 0)
		    return code;
		pres_attributes->where_used |= pdev->used_mask;
		va = &v_attriburtes;
		COS_OBJECT_VALUE(va, pres_attributes->object);
	    }
	    if ((code = pdf_separation_color_space(pdev, pca, "/DeviceN", &v,
						   pcs->base_space,
					pfn, &pdf_color_space_names, va)) < 0)
		return code;
	}
	break;

    case gs_color_space_index_Separation:
	if (!pdev->PreserveSeparation)
	    return_error(gs_error_rangecheck);
	pfn = gs_cspace_get_sepr_function(pcs);
	/****** CURRENTLY WE ONLY HANDLE Functions ******/
	if (pfn == 0)
	    return_error(gs_error_rangecheck);
	{
	    byte *name_string;
	    uint name_string_length;
	    if ((code = pcs->params.separation.get_colorname_string(
				  pdev->memory, 
				  pcs->params.separation.sep_name, &name_string, 
				  &name_string_length)) < 0 ||
		(code = pdf_string_to_cos_name(pdev, name_string, 
				      name_string_length, &v)) < 0 ||
		(code = pdf_separation_color_space(pdev, pca, "/Separation", &v,
					    pcs->base_space,
					    pfn, &pdf_color_space_names, NULL)) < 0)
		return code;
	}
	break;

    case gs_color_space_index_Pattern:
	if ((code = pdf_color_space_named(pdev, pvalue, ppranges,
				    pcs->base_space,
				    &pdf_color_space_names, false, NULL, 0)) < 0 ||
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
	pdf_color_space_t *ppcs;

	if (code < 0 ||
	    (code = pdf_alloc_resource(pdev, resourceColorSpace, pcs->id,
				       &pres, -1)) < 0
	    ) {
	    COS_FREE(pca, "pdf_color_space");
	    return code;
	}
	pdf_reserve_object_id(pdev, pres, 0);
	if (res_name != NULL) {
	    int l = min(name_length, sizeof(pres->rname) - 1);
	    
	    memcpy(pres->rname, res_name, l);
	    pres->rname[l] = 0;
	}
	ppcs = (pdf_color_space_t *)pres;
	if (serialized == serialized0) {
	    serialized = gs_alloc_bytes(pdev->pdf_memory, serialized_size, "pdf_color_space");
	    if (serialized == NULL)
		return_error(gs_error_VMerror);
	    memcpy(serialized, serialized0, serialized_size);
	}
	ppcs->serialized = serialized;
	ppcs->serialized_size = serialized_size;
	if (ranges) {
	    int num_comp = gs_color_space_num_components(pcs);
	    gs_range_t *copy_ranges = (gs_range_t *)
		gs_alloc_byte_array(pdev->pdf_memory, num_comp,
				    sizeof(gs_range_t), "pdf_color_space");

	    if (copy_ranges == 0) {
		COS_FREE(pca, "pdf_color_space");
		return_error(gs_error_VMerror);
	    }
	    memcpy(copy_ranges, ranges, num_comp * sizeof(gs_range_t));
	    ppcs->ranges = copy_ranges;
	    if (ppranges)
		*ppranges = copy_ranges;
	} else
	    ppcs->ranges = 0;
	pca->id = pres->object->id;
	COS_FREE(pres->object, "pdf_color_space");
	pres->object = (cos_object_t *)pca;
	cos_write_object(COS_OBJECT(pca), pdev, resourceColorSpace);
    }
 ret:
    if (by_name) {
	/* Return a resource name rather than an object reference. */
	discard(COS_RESOURCE_VALUE(pvalue, pca));
    } else
	discard(COS_OBJECT_VALUE(pvalue, pca));
    if (pres != NULL) {
	pres->where_used |= pdev->used_mask;
	code = pdf_add_resource(pdev, pdev->substream_Resources, "/ColorSpace", pres);
	if (code < 0)
	    return code;
    }
    return 0;
}

/* ---------------- Miscellaneous ---------------- */

/* Create colored and uncolored Pattern color spaces. */
static int
pdf_pattern_space(gx_device_pdf *pdev, cos_value_t *pvalue,
		  pdf_resource_t **ppres, const char *cs_name)
{
    int code;

    if (!*ppres) {
	int code = pdf_begin_resource_body(pdev, resourceColorSpace, gs_no_id,
					   ppres);

	if (code < 0)
	    return code;
	pprints1(pdev->strm, "%s\n", cs_name);
	pdf_end_resource(pdev, resourceColorSpace);
	(*ppres)->object->written = true; /* don't write at end */
	((pdf_color_space_t *)*ppres)->ranges = 0;
	((pdf_color_space_t *)*ppres)->serialized = 0;
    }
    code = pdf_add_resource(pdev, pdev->substream_Resources, "/ColorSpace", *ppres);
    if (code < 0)
	return code;
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
    /* Only for process colors. */
    int ncomp = pdev->color_info.num_components;
    static const char *const pcs_names[5] = {
	0, "[/Pattern /DeviceGray]", 0, "[/Pattern /DeviceRGB]",
	"[/Pattern /DeviceCMYK]"
    };

    return pdf_pattern_space(pdev, pvalue, &pdev->cs_Patterns[ncomp],
			     pcs_names[ncomp]);
}
int
pdf_cs_Pattern_uncolored_hl(gx_device_pdf *pdev, 
		const gs_color_space *pcs, cos_value_t *pvalue)
{
    /* Only for high level colors. */
    return pdf_color_space_named(pdev, pvalue, NULL, pcs, &pdf_color_space_names, true, NULL, 0);
}

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
	pbcs = pcs->base_space;
	goto csw;
    default:
	pdev->procsets |= ImageC;
	break;
    }
}
