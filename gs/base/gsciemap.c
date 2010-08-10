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
/* CIE color rendering */
#include "math_.h"
#include "gx.h"
#include "gserrors.h"
#include "gxcspace.h"		/* for gxcie.c */
#include "gxarith.h"
#include "gxcie.h"
#include "gxdevice.h"		/* for gxcmap.h */
#include "gxcmap.h"
#include "gxistate.h"
#include "gscolor2.h"
#include "gsicc_create.h"       /* Needed for delayed creation of ICC profiles from CIE color spaces */
#include "gsicc_manage.h"
#include "gsicc.h"
#include "gscspace.h"

/*
 * Compute a cache index as (vin - base) * factor.
 * vin, base, factor, and the result are cie_cached_values.
 * We know that the result doesn't exceed (gx_cie_cache_size - 1) << fbits.
 *
 * Since this operation is extremely time-critical, we don't rely on the
 * compiler providing 'inline'.
 */
#define LOOKUP_INDEX_(vin, pcache, fbits)\
  (cie_cached_value)\
  ((vin) <= (pcache)->vecs.params.base ? 0 :\
   (vin) >= (pcache)->vecs.params.limit ? (gx_cie_cache_size - 1) << (fbits) :\
   cie_cached_product2int( ((vin) - (pcache)->vecs.params.base),\
			   (pcache)->vecs.params.factor, fbits ))
#define LOOKUP_ENTRY_(vin, pcache)\
  (&(pcache)->vecs.values[(int)LOOKUP_INDEX(vin, pcache, 0)])
#ifdef DEBUG
static cie_cached_value
LOOKUP_INDEX(cie_cached_value vin, const gx_cie_vector_cache *pcache,
	     int fbits)
{
    return LOOKUP_INDEX_(vin, pcache, fbits);
}
static const cie_cached_vector3 *
LOOKUP_ENTRY(cie_cached_value vin, const gx_cie_vector_cache *pcache)
{
    return LOOKUP_ENTRY_(vin, pcache);
}
#else  /* !DEBUG */
#  define LOOKUP_INDEX(vin, pcache, fbits)  LOOKUP_INDEX_(vin, pcache, fbits)
#  define LOOKUP_ENTRY(vin, pcache)         LOOKUP_ENTRY_(vin, pcache)
#endif /* DEBUG */

/*
 * Call the remap_finish procedure in the structure without going through
 * the extra level of procedure.
 */
#ifdef DEBUG
#  define GX_CIE_REMAP_FINISH(vec3, pconc, pis, pcs)\
    gx_cie_remap_finish(vec3, pconc, pis, pcs)
#else
#  define GX_CIE_REMAP_FINISH(vec3, pconc, pis, pcs)\
    ((pis)->cie_joint_caches->remap_finish(vec3, pconc, pis, pcs))
#endif

/* Forward references */
static void cie_lookup_mult3(cie_cached_vector3 *,
			      const gx_cie_vector_cache3_t *);

#ifdef DEBUG
static void
cie_lookup_map3(cie_cached_vector3 * pvec,
		const gx_cie_vector_cache3_t * pc, const char *cname)
{
    if_debug5('c', "[c]lookup %s 0x%lx [%g %g %g]\n",
	      (const char *)cname, (ulong) pc,
	      cie_cached2float(pvec->u), cie_cached2float(pvec->v),
	      cie_cached2float(pvec->w));
    cie_lookup_mult3(pvec, pc);
    if_debug3('c', "        =[%g %g %g]\n",
	      cie_cached2float(pvec->u), cie_cached2float(pvec->v),
	      cie_cached2float(pvec->w));
}
#else
#  define cie_lookup_map3(pvec, pc, cname) cie_lookup_mult3(pvec, pc)
#endif

/* Check used for internal ranges to determine if we need to create a
   CLUT for the ICC profile and if we need to rescale the incoming
   CIE data.*/
bool
check_range(gs_range *ranges, int num_colorants)
{
    int k;

    for (k = 0; k < num_colorants; k++) {
        if (ranges[k].rmin != 0) return false;
        if (ranges[k].rmax != 1) return false;
    }
    return(true);
}

static void
rescale_input_color(gs_range *ranges, int num_colorants, gs_client_color *src,
                    gs_client_color *des)
{
    int k;

    for (k = 0; k < num_colorants; k++) {
        des->paint.values[k] = 
            (src->paint.values[k]-ranges[k].rmin)/
            (ranges[k].rmax-ranges[k].rmin);
    }
}


/*
 * Test whether a CIE rendering has been defined; ensure that the joint
 * caches are loaded.  Note that the procedure may return 1 if no rendering
 * has been defined. The 'cie_to_xyz' flag indicates that we don't need a CRD
 */
static inline int 
gx_cie_check_rendering_inline(const gs_color_space * pcs, frac * pconc, const gs_imager_state * pis)
{
    if (pis->cie_render == 0 && !pis->cie_to_xyz) {
	/* No rendering has been defined yet: return black. */
	pconc[0] = pconc[1] = pconc[2] = frac_0;
	return 1;
    }
    if (pis->cie_joint_caches->status == CIE_JC_STATUS_COMPLETED) {
	if (pis->cie_joint_caches->cspace_id != pcs->id)
	    pis->cie_joint_caches->status = CIE_JC_STATUS_BUILT;
    }
    if (pis->cie_joint_caches->status != CIE_JC_STATUS_COMPLETED) {
	int     code = gs_cie_jc_complete(pis, pcs);

	if (code < 0)
	    return code;
    }
    return 0;
}
int 
gx_cie_check_rendering(const gs_color_space * pcs, frac * pconc, const gs_imager_state * pis)
{
    return gx_cie_check_rendering_inline(pcs, pconc, pis);
}

/* Common code shared between remap and concretize for defg */
static int
gx_ciedefg_to_icc(gs_color_space **ppcs_icc, gs_color_space *pcs, gs_memory_t *memory)
{
    int code = 0;
    gs_color_space *palt_cs = pcs->base_space;
    gx_cie_vector_cache *abc_caches = &(pcs->params.abc->caches.DecodeABC.caches[0]);
    gx_cie_scalar_cache    *lmn_caches = &(pcs->params.abc->common.caches.DecodeLMN[0]);
    gx_cie_scalar_cache *defg_caches = &(pcs->params.defg->caches_defg.DecodeDEFG[0]);

    /* build the ICC color space object */
    code = gs_cspace_build_ICC(ppcs_icc, NULL, memory->stable_memory);
    /* record the cie alt space as the icc alternative color space */
    (*ppcs_icc)->base_space = palt_cs;
    rc_increment_cs(palt_cs);
    (*ppcs_icc)->cmm_icc_profile_data = gsicc_profile_new(NULL, memory, NULL, 0);
    code = gsicc_create_fromdefg(pcs, &((*ppcs_icc)->cmm_icc_profile_data->buffer), 
                    &((*ppcs_icc)->cmm_icc_profile_data->buffer_size), memory, 
                    abc_caches, lmn_caches, defg_caches);
    gsicc_init_profile_info((*ppcs_icc)->cmm_icc_profile_data);
    pcs->icc_equivalent = *ppcs_icc;
    return(0);
}

int
gx_remap_CIEDEFG(const gs_client_color * pc, const gs_color_space * pcs,
	gx_device_color * pdc, const gs_imager_state * pis, gx_device * dev,
		gs_color_select_t select)
{
    gs_color_space *pcs_icc;
    int code;
    gs_client_color scale_pc;

    if_debug4('c', "[c]remap CIEDEFG [%g %g %g %g]\n",
	      pc->paint.values[0], pc->paint.values[1],
	      pc->paint.values[2], pc->paint.values[3]);
    /* If we are comming in here then we have not completed
       the conversion of the DEFG space to an ICC type.  We
       will finish that process now. */
    if (pcs->icc_equivalent == NULL) {
        code = gx_ciedefg_to_icc(&pcs_icc, pcs, pis->memory->stable_memory);
    } else {
        pcs_icc = pcs->icc_equivalent;
    }
    /* Rescale the input based upon the input range since profile is
       created to remap this range from 0 to 1 */
    if (check_range(&(pcs->params.defg->RangeDEFG.ranges[0]), 4)) {
        return((pcs_icc->type->remap_color)(pc,pcs_icc,pdc,pis,dev,select));
    }
    /* Do the rescale from 0 to 1 */
    rescale_input_color(&(pcs->params.defg->RangeDEFG.ranges[0]), 4, pc, &scale_pc);
    /* Now the icc remap */
    return((pcs_icc->type->remap_color)(pc,pcs_icc,pdc,pis,dev,select));
}

/* Render a CIEBasedDEFG color. */
int
gx_concretize_CIEDEFG(const gs_client_color * pc, const gs_color_space * pcs,
		      frac * pconc, const gs_imager_state * pis, gx_device *dev)
{
    const gs_cie_defg *pcie = pcs->params.defg;
    int code;
    gs_color_space *pcs_icc;
    gs_client_color scale_pc;

    if_debug4('c', "[c]concretize DEFG [%g %g %g %g]\n",
	      pc->paint.values[0], pc->paint.values[1],
	      pc->paint.values[2], pc->paint.values[3]);
    /* If we are comming in here then we have not completed
       the conversion of the DEFG space to an ICC type.  We
       will finish that process now. */
    if (pcs->icc_equivalent == NULL) {
        code = gx_ciedefg_to_icc(&pcs_icc, pcs, pis->memory->stable_memory);
    } else {
        pcs_icc = pcs->icc_equivalent;
    }
    /* Rescale the input based upon the input range since profile is
       created to remap this range from 0 to 1 */
    if (check_range(&(pcs->params.defg->RangeDEFG.ranges[0]), 4)) {
        return((pcs_icc->type->concretize_color)(pc, pcs_icc, pconc, pis, dev));
    }
    /* Do the rescale from 0 to 1 */
    rescale_input_color(&(pcs->params.defg->RangeDEFG.ranges[0]), 4, pc, &scale_pc);
    /* Now the icc remap */
    return((pcs_icc->type->concretize_color)(pc, pcs_icc, pconc, pis, dev));
}

/* Used for when we have to mash entire transform to CIEXYZ */
int
gx_psconcretize_CIEA(const gs_client_color * pc, const gs_color_space * pcs,
		      frac * pconc, const gs_imager_state * pis)
{
    const gs_cie_a *pcie = pcs->params.a;
    cie_cached_value a = float2cie_cached(pc->paint.values[0]);
    cie_cached_vector3 vlmn;
    int code;

    if_debug1('c', "[c]concretize CIEA %g\n", pc->paint.values[0]);
    code = gx_cie_check_rendering_inline(pcs, pconc, pis);
    if (code < 0)
	return code;
    if (code == 1)
	return 0;

    /* Apply DecodeA and MatrixA. */
    if (!pis->cie_joint_caches->skipDecodeABC)
	vlmn = *LOOKUP_ENTRY(a, &pcie->caches.DecodeA);
    else
	vlmn.u = vlmn.v = vlmn.w = a;
    GX_CIE_REMAP_FINISH(vlmn, pconc, pis, pcs);
    return 0;
}

/* Used for when we have to mash entire transform to CIEXYZ */
int
gx_psconcretize_CIEABC(const gs_client_color * pc, const gs_color_space * pcs,
		      frac * pconc, const gs_imager_state * pis)
{
    const gs_cie_abc *pcie = pcs->params.abc;
    cie_cached_vector3 vec3;
    int code;

    if_debug3('c', "[c]concretize CIEABC [%g %g %g]\n",
	      pc->paint.values[0], pc->paint.values[1],
	      pc->paint.values[2]);
    code = gx_cie_check_rendering_inline(pcs, pconc, pis);
    if (code < 0)
	return code;
    if (code == 1)
	return 0;

    vec3.u = float2cie_cached(pc->paint.values[0]);
    vec3.v = float2cie_cached(pc->paint.values[1]);
    vec3.w = float2cie_cached(pc->paint.values[2]);
    if (!pis->cie_joint_caches->skipDecodeABC)
	cie_lookup_map3(&vec3 /* ABC => LMN */, &pcie->caches.DecodeABC,
			"Decode/MatrixABC");
    GX_CIE_REMAP_FINISH(vec3, pconc, pis, pcs);
    return 0;
}

/* Used for when we have to mash entire transform to CIEXYZ */
int
gx_psconcretize_CIEDEFG(const gs_client_color * pc, const gs_color_space * pcs,
		      frac * pconc, const gs_imager_state * pis)
{
    const gs_cie_defg *pcie = pcs->params.defg;
    int i;
    fixed hijk[4];
    frac abc[3];
    cie_cached_vector3 vec3;
    int code;

    if_debug4('c', "[c]concretize DEFG [%g %g %g %g]\n",
	      pc->paint.values[0], pc->paint.values[1],
	      pc->paint.values[2], pc->paint.values[3]);
    code = gx_cie_check_rendering_inline(pcs, pconc, pis);
    if (code < 0)
	return code;
    if (code == 1)
	return 0;
    /*
     * Apply DecodeDEFG, including restriction to RangeHIJK and scaling to
     * the Table dimensions.
     */
    for (i = 0; i < 4; ++i) {
	int tdim = pcie->Table.dims[i] - 1;
	double factor = pcie->caches_defg.DecodeDEFG[i].floats.params.factor;
	double v0 = pc->paint.values[i];
	const gs_range *const rangeDEFG = &pcie->RangeDEFG.ranges[i];
	double value =
	    (v0 < rangeDEFG->rmin ? 0.0 : factor *
	    (v0 > rangeDEFG->rmax ? rangeDEFG->rmax - rangeDEFG->rmin :
	     v0 - rangeDEFG->rmin ));
	int vi = (int)value;
	double vf = value - vi;
	double v = pcie->caches_defg.DecodeDEFG[i].floats.values[vi];

	if (vf != 0 && vi < factor)
	    v += vf *
		(pcie->caches_defg.DecodeDEFG[i].floats.values[vi + 1] - v);
	v = (v < 0 ? 0 : v > tdim ? tdim : v);
	hijk[i] = float2fixed(v);
    }
    /* Apply Table. */
    gx_color_interpolate_linear(hijk, &pcie->Table, abc);

#define SCALE_TO_RANGE(range, frac) ( \
       float2cie_cached(((range).rmax - (range).rmin) * frac2float(frac) + \
	    (range).rmin) \
    )
    /* Scale the abc[] frac values to RangeABC cie_cached result */
    vec3.u = SCALE_TO_RANGE(pcie->RangeABC.ranges[0], abc[0]); 
    vec3.v = SCALE_TO_RANGE(pcie->RangeABC.ranges[1], abc[1]); 
    vec3.w = SCALE_TO_RANGE(pcie->RangeABC.ranges[2], abc[2]); 
    /* Apply DecodeABC and MatrixABC. */
    if (!pis->cie_joint_caches->skipDecodeABC)
	cie_lookup_map3(&vec3 /* ABC => LMN */, &pcie->caches.DecodeABC,
			"Decode/MatrixABC");
    GX_CIE_REMAP_FINISH(vec3, pconc, pis, pcs);
    return 0;
}

/* Render a CIEBasedDEF color. */
int
gx_psconcretize_CIEDEF(const gs_client_color * pc, const gs_color_space * pcs,
		     frac * pconc, const gs_imager_state * pis)
{
    const gs_cie_def *pcie = pcs->params.def;
    int i;
    fixed hij[3];
    frac abc[3];
    cie_cached_vector3 vec3;
    int code;

    if_debug3('c', "[c]concretize DEF [%g %g %g]\n",
	      pc->paint.values[0], pc->paint.values[1],
	      pc->paint.values[2]);
    code = gx_cie_check_rendering_inline(pcs, pconc, pis);
    if (code < 0)
	return code;
    if (code == 1)
	return 0;

    /*
     * Apply DecodeDEF, including restriction to RangeHIJ and scaling to
     * the Table dimensions.
     */
    for (i = 0; i < 3; ++i) {
	int tdim = pcie->Table.dims[i] - 1;
	double factor = pcie->caches_def.DecodeDEF[i].floats.params.factor;
	double v0 = pc->paint.values[i];
	const gs_range *const rangeDEF = &pcie->RangeDEF.ranges[i];
	double value =
	    (v0 < rangeDEF->rmin ? 0.0 : factor *
	    (v0 > rangeDEF->rmax ? rangeDEF->rmax - rangeDEF->rmin :
	     v0 - rangeDEF->rmin ));
	int vi = (int)value;
	double vf = value - vi;
	double v = pcie->caches_def.DecodeDEF[i].floats.values[vi];

	if (vf != 0 && vi < factor)
	    v += vf *
		(pcie->caches_def.DecodeDEF[i].floats.values[vi + 1] - v);
	v = (v < 0 ? 0 : v > tdim ? tdim : v);
	hij[i] = float2fixed(v);
    }
    /* Apply Table. */
    gx_color_interpolate_linear(hij, &pcie->Table, abc);
    /* Scale the abc[] frac values to RangeABC cie_cached result */
    vec3.u = SCALE_TO_RANGE(pcie->RangeABC.ranges[0], abc[0]); 
    vec3.v = SCALE_TO_RANGE(pcie->RangeABC.ranges[1], abc[1]); 
    vec3.w = SCALE_TO_RANGE(pcie->RangeABC.ranges[2], abc[2]); 
    /* Apply DecodeABC and MatrixABC. */
    if (!pis->cie_joint_caches->skipDecodeABC)
	cie_lookup_map3(&vec3 /* ABC => LMN */, &pcie->caches.DecodeABC,
			"Decode/MatrixABC");
    GX_CIE_REMAP_FINISH(vec3, pconc, pis, pcs);
    return 0;
}
#undef SCALE_TO_RANGE

/* Common code shared between remap and concretize for def */
static int
gx_ciedef_to_icc(gs_color_space **ppcs_icc, gs_color_space *pcs, gs_memory_t *memory)
{
    int code = 0;
    gs_color_space *palt_cs = pcs->base_space;
    gx_cie_vector_cache *abc_caches = &(pcs->params.abc->caches.DecodeABC.caches[0]);
    gx_cie_scalar_cache    *lmn_caches = &(pcs->params.abc->common.caches.DecodeLMN[0]);
    gx_cie_scalar_cache *def_caches = &(pcs->params.def->caches_def.DecodeDEF[0]);

    /* build the ICC color space object */
    code = gs_cspace_build_ICC(ppcs_icc, NULL, memory->stable_memory);
    /* record the cie alt space as the icc alternative color space */
    (*ppcs_icc)->base_space = palt_cs;
    rc_increment_cs(palt_cs);
    (*ppcs_icc)->cmm_icc_profile_data = gsicc_profile_new(NULL, memory, NULL, 0);
    code = gsicc_create_fromdef(pcs, &((*ppcs_icc)->cmm_icc_profile_data->buffer), 
                    &((*ppcs_icc)->cmm_icc_profile_data->buffer_size), memory, 
                    abc_caches, lmn_caches, def_caches);
    gsicc_init_profile_info((*ppcs_icc)->cmm_icc_profile_data);
    /* Assign to the icc_equivalent member variable */
    pcs->icc_equivalent = *ppcs_icc;
    return(0);
    }

int
gx_remap_CIEDEF(const gs_client_color * pc, const gs_color_space * pcs,
	gx_device_color * pdc, const gs_imager_state * pis, gx_device * dev,
		gs_color_select_t select)
{
    gs_color_space *pcs_icc;
    gs_client_color scale_pc;

    if_debug3('c', "[c]remap CIEDEF [%g %g %g]\n",
	      pc->paint.values[0], pc->paint.values[1],
	      pc->paint.values[2]);
    /* If we are comming in here then we have not completed
       the conversion of the DEF space to an ICC type.  We
       will finish that process now. */
    if (pcs->icc_equivalent == NULL) {
        gx_ciedef_to_icc(&pcs_icc, pcs, pis->memory->stable_memory);
    } else {
        pcs_icc = pcs->icc_equivalent;
    }
    /* Rescale the input based upon the input range since profile is
       created to remap this range from 0 to 1 */
    if (check_range(&(pcs->params.def->RangeDEF.ranges[0]), 3)) {
        return((pcs_icc->type->remap_color)(pc,pcs_icc,pdc,pis,dev,select));
}
    /* Do the rescale from 0 to 1 */
    rescale_input_color(&(pcs->params.def->RangeDEF.ranges[0]), 3, pc, &scale_pc);
    /* Now the icc remap */
    return((pcs_icc->type->remap_color)(&scale_pc,pcs_icc,pdc,pis,dev,select));
}

/* Render a CIEBasedDEF color. */
int
gx_concretize_CIEDEF(const gs_client_color * pc, const gs_color_space * pcs,
		     frac * pconc, const gs_imager_state * pis, gx_device *dev)
{
    const gs_cie_def *pcie = pcs->params.def;
    int code;
    gs_color_space *pcs_icc;
    gs_client_color scale_pc;

    if_debug3('c', "[c]concretize DEF [%g %g %g]\n",
	      pc->paint.values[0], pc->paint.values[1],
	      pc->paint.values[2]);
    /* If we are comming in here then we have not completed
       the conversion of the DEF space to an ICC type.  We
       will finish that process now. */
    if (pcs->icc_equivalent == NULL) {
        code = gx_ciedef_to_icc(&pcs_icc, pcs, pis->memory->stable_memory);
    } else {
        pcs_icc = pcs->icc_equivalent;
    }
    /* Rescale the input based upon the input range since profile is
       created to remap this range from 0 to 1 */
    if (check_range(&(pcs->params.def->RangeDEF.ranges[0]), 3)) {
        return((pcs_icc->type->concretize_color)(pc, pcs_icc, pconc, pis, dev));
}
    /* Do the rescale from 0 to 1 */
    rescale_input_color(&(pcs->params.def->RangeDEF.ranges[0]), 3, pc, &scale_pc);
    /* Now the icc remap */
    return((pcs_icc->type->concretize_color)(&scale_pc, pcs_icc, pconc, pis, dev));
}
#undef SCALE_TO_RANGE

/* Common code shared between remap and concretize */
static int
gx_cieabc_to_icc(gs_color_space **ppcs_icc, gs_color_space *pcs, bool *islab,
                 gs_memory_t *memory)
{
    int code = 0;
    gs_color_space *palt_cs = pcs->base_space;
    gx_cie_vector_cache *abc_caches = &(pcs->params.abc->caches.DecodeABC.caches[0]);
    gx_cie_scalar_cache *lmn_caches = &(pcs->params.abc->common.caches.DecodeLMN[0]); 

    /* build the ICC color space object */
    code = gs_cspace_build_ICC(ppcs_icc, NULL, memory);
    /* record the cie alt space as the icc alternative color space */
    (*ppcs_icc)->base_space = palt_cs;
    rc_increment_cs(palt_cs);
    (*ppcs_icc)->cmm_icc_profile_data = gsicc_profile_new(NULL, memory, NULL, 0);
    code = gsicc_create_fromabc(pcs, &((*ppcs_icc)->cmm_icc_profile_data->buffer), 
                    &((*ppcs_icc)->cmm_icc_profile_data->buffer_size), memory, 
                    abc_caches, lmn_caches, islab);
    gsicc_init_profile_info((*ppcs_icc)->cmm_icc_profile_data);
    /* Assign to the icc_equivalent member variable */
    pcs->icc_equivalent = *ppcs_icc;
                                return(0);
    }

/* Render a CIEBasedABC color. */
/* We provide both remap and concretize, but only the former */
/* needs to be efficient. */
int
gx_remap_CIEABC(const gs_client_color * pc, const gs_color_space * pcs,
	gx_device_color * pdc, const gs_imager_state * pis, gx_device * dev,
		gs_color_select_t select)
{
    gs_color_space *pcs_icc;
    gs_client_color scale_pc;
    bool islab;

    if_debug3('c', "[c]remap CIEABC [%g %g %g]\n",
	      pc->paint.values[0], pc->paint.values[1],
	      pc->paint.values[2]);
    /* If we are comming in here then we have not completed
       the conversion of the ABC space to an ICC type.  We
       will finish that process now. */
    if (pcs->icc_equivalent == NULL) {
        gx_cieabc_to_icc(&pcs_icc, pcs, &islab, pis->memory->stable_memory);
    } else {
        pcs_icc = pcs->icc_equivalent;
	}
    /* Rescale the input based upon the input range since profile is
       created to remap this range from 0 to 1 */
    if (check_range(&(pcs->params.abc->RangeABC.ranges[0]), 3)) {
        return((pcs_icc->type->remap_color)(pc,pcs_icc,pdc,pis,dev,select));
    }
    /* Do the rescale from 0 to 1 */
    rescale_input_color(&(pcs->params.abc->RangeABC.ranges[0]), 3, pc, &scale_pc);
    /* Now the icc remap */
    return((pcs_icc->type->remap_color)(&scale_pc,pcs_icc,pdc,pis,dev,select));
}

int
gx_concretize_CIEABC(const gs_client_color * pc, const gs_color_space * pcs,
		     frac * pconc, const gs_imager_state * pis, gx_device *dev)
{
    gs_color_space *pcs_icc;
    gs_client_color scale_pc;
    bool islab;

    if_debug3('c', "[c]concretize CIEABC [%g %g %g]\n",
	      pc->paint.values[0], pc->paint.values[1],
	      pc->paint.values[2]);
    /* If we are comming in here then we have not completed
       the conversion of the ABC space to an ICC type.  We
       will finish that process now. */
    if (pcs->icc_equivalent == NULL) {
        gx_cieabc_to_icc(&pcs_icc, pcs, &islab, pis->memory->stable_memory);
    } else {
        pcs_icc = pcs->icc_equivalent;
    }
    /* Rescale the input based upon the input range since profile is
       created to remap this range from 0 to 1 */
    if (check_range(&(pcs->params.abc->RangeABC.ranges[0]), 3)) {
        return((pcs_icc->type->concretize_color)(pc, pcs_icc, pconc, pis, dev));
    }
    /* Do the rescale from 0 to 1 */
    rescale_input_color(&(pcs->params.abc->RangeABC.ranges[0]), 3, pc, &scale_pc);
    /* Now the icc remap */
    return((pcs_icc->type->concretize_color)(&scale_pc, pcs_icc, pconc, pis, dev));
}

/* Common code shared between remap and concretize */
static int
gx_ciea_to_icc(gs_color_space **ppcs_icc, gs_color_space *pcs, gs_memory_t *memory)
{
    int code = 0;
    gs_color_space *palt_cs = pcs->base_space;
    gx_cie_vector_cache *a_cache = &(pcs->params.a->caches.DecodeA);
    gx_cie_scalar_cache    *lmn_caches = &(pcs->params.a->common.caches.DecodeLMN[0]);

    /* build the ICC color space object */
    code = gs_cspace_build_ICC(ppcs_icc, NULL, memory);
    /* record the cie alt space as the icc alternative color space */
    (*ppcs_icc)->base_space = palt_cs;
    rc_increment_cs(palt_cs);
    (*ppcs_icc)->cmm_icc_profile_data = gsicc_profile_new(NULL, memory, NULL, 0);
    code = gsicc_create_froma(pcs, &((*ppcs_icc)->cmm_icc_profile_data->buffer), 
                    &((*ppcs_icc)->cmm_icc_profile_data->buffer_size), memory, 
                    a_cache, lmn_caches);
    gsicc_init_profile_info((*ppcs_icc)->cmm_icc_profile_data);
    /* Assign to the icc_equivalent member variable */
    pcs->icc_equivalent = *ppcs_icc;
    return(code);
}

int
gx_remap_CIEA(const gs_client_color * pc, const gs_color_space * pcs,
	gx_device_color * pdc, const gs_imager_state * pis, gx_device * dev,
		gs_color_select_t select)
{
    int code;
    gs_color_space *pcs_icc;
    gs_client_color scale_pc;

    if_debug1('c', "[c]remap CIEA [%g]\n",pc->paint.values[0]);
   /* If we are coming in here then we may have not completed
       the conversion of the CIE A space to an ICC type.  We
       will finish that process now. */
    if (pcs->icc_equivalent == NULL) {
        code = gx_ciea_to_icc(&pcs_icc, pcs, pis->memory->stable_memory);
    } else {
        /* Once the ICC color space is set, we should be doing all the remaps through the ICC equivalent */
        pcs_icc = pcs->icc_equivalent;
    }
    /* Rescale the input based upon the input range since profile is
       created to remap this range from 0 to 1 */
    if (check_range(&(pcs->params.a->RangeA), 1)) {
        return((pcs_icc->type->remap_color)(pc,pcs_icc,pdc,pis,dev,select));
    }
    /* Do the rescale from 0 to 1 */
    rescale_input_color(&(pcs->params.a->RangeA), 1, pc, &scale_pc);
    /* Now the icc remap */
    return((pcs_icc->type->remap_color)(&scale_pc,pcs_icc,pdc,pis,dev,select));
}

/* Render a CIEBasedA color. */
int
gx_concretize_CIEA(const gs_client_color * pc, const gs_color_space * pcs,
		   frac * pconc, const gs_imager_state * pis, gx_device *dev)
{
    int code;
    gs_color_space *pcs_icc;
    gs_client_color scale_pc;

    if_debug1('c', "[c]concretize CIEA %g\n", pc->paint.values[0]);
    /* If we are comming in here then we have not completed
       the conversion of the CIE A space to an ICC type.  We
       will finish that process now. */
    if (pcs->icc_equivalent == NULL) {
        code = gx_ciea_to_icc(&pcs_icc, pcs, pis->memory->stable_memory);
    } else {
        /* Once the ICC color space is set, we should be doing all the remaps through the ICC equivalent */
        pcs_icc = pcs->icc_equivalent;
    }
    /* Rescale the input based upon the input range since profile is
       created to remap this range from 0 to 1 */
    if (check_range(&(pcs->params.a->RangeA), 1)) {
        return((pcs_icc->type->concretize_color)(pc, pcs_icc, pconc, pis, dev));
    }
    /* Do the rescale from 0 to 1 */
    rescale_input_color(&(pcs->params.a->RangeA), 1, pc, &scale_pc);
    /* Now the icc remap */
    return((pcs_icc->type->concretize_color)(&scale_pc, pcs_icc, pconc, pis, dev));
}

/* Call for cases where the equivalent icc color space needs to be set */
int
gs_colorspace_set_icc_equivalent(gs_color_space *pcs, bool *islab, 
                                 gs_memory_t *memory)
{
     gs_color_space_index color_space_index = gs_color_space_get_index(pcs);
     gs_color_space *picc_cs;

     *islab = false;  /* For non CIEABC cases */
     if (pcs->icc_equivalent != NULL || !gs_color_space_is_PSCIE(pcs)) {
         return(0);
}
     switch( color_space_index ) {
       case gs_color_space_index_CIEDEFG:
            gx_ciedefg_to_icc(&picc_cs, pcs, memory->stable_memory);
            break;
        case gs_color_space_index_CIEDEF:
            gx_ciedef_to_icc(&picc_cs, pcs, memory->stable_memory);
            break;
        case gs_color_space_index_CIEABC:
            gx_cieabc_to_icc(&picc_cs, pcs, islab, memory->stable_memory);
            break;
        case gs_color_space_index_CIEA:
            gx_ciea_to_icc(&picc_cs, pcs, memory->stable_memory);
            break;
        default:
             /* do nothing.  Sould never happen */
             break;
     } 
    return(0);
}

/* Call the remap_finish procedure in the joint_caches structure. */
int
gx_cie_remap_finish(cie_cached_vector3 vec3, frac * pconc,
		    const gs_imager_state * pis,
		    const gs_color_space *pcs)
{
    return pis->cie_joint_caches->remap_finish(vec3, pconc, pis, pcs);
}

/* Finish remapping a CIEBased color. */
/* Return 3 if RGB, 4 if CMYK. */
/* this procedure is exported for the benefit of gsicc.c */
int
gx_cie_real_remap_finish(cie_cached_vector3 vec3, frac * pconc,
			 const gs_imager_state * pis,
			 const gs_color_space *pcs)
{
    const gs_cie_render *pcrd = pis->cie_render;
    const gx_cie_joint_caches *pjc = pis->cie_joint_caches;
    const gs_const_string *table = pcrd->RenderTable.lookup.table;
    int tabc[3];		/* indices for final EncodeABC lookup */

    /* Apply DecodeLMN, MatrixLMN(decode), and MatrixPQR. */
    if (!pjc->skipDecodeLMN)
	cie_lookup_map3(&vec3 /* LMN => PQR */, &pjc->DecodeLMN,
			"Decode/MatrixLMN+MatrixPQR");

    /* Apply TransformPQR, MatrixPQR', and MatrixLMN(encode). */
    if (!pjc->skipPQR)
	cie_lookup_map3(&vec3 /* PQR => LMN */, &pjc->TransformPQR,
			"Transform/Matrix'PQR+MatrixLMN");

    /* Apply EncodeLMN and MatrixABC(encode). */
    if (!pjc->skipEncodeLMN)
	cie_lookup_map3(&vec3 /* LMN => ABC */, &pcrd->caches.EncodeLMN,
			"EncodeLMN+MatrixABC");

    /* MatrixABCEncode includes the scaling of the EncodeABC */
    /* cache index. */
#define SET_TABC(i, t)\
  BEGIN\
    tabc[i] = cie_cached2int(vec3 /*ABC*/.t - pcrd->EncodeABC_base[i],\
			     _cie_interpolate_bits);\
    if ((uint)tabc[i] > (gx_cie_cache_size - 1) << _cie_interpolate_bits)\
	tabc[i] = (tabc[i] < 0 ? 0 :\
		   (gx_cie_cache_size - 1) << _cie_interpolate_bits);\
  END
    SET_TABC(0, u);
    SET_TABC(1, v);
    SET_TABC(2, w);
#undef SET_TABC
    if (table == 0) {
	/*
	 * No further transformation.
	 * The final mapping step includes both restriction to
	 * the range [0..1] and conversion to fracs.
	 */
#define EABC(i)\
  cie_interpolate_fracs(pcrd->caches.EncodeABC[i].fixeds.fracs.values, tabc[i])
	pconc[0] = EABC(0);
	pconc[1] = EABC(1);
	pconc[2] = EABC(2);
#undef EABC
	return 3;
    } else {
	/*
	 * Use the RenderTable.
	 */
	int m = pcrd->RenderTable.lookup.m;

#define RT_LOOKUP(j, i) pcrd->caches.RenderTableT[j].fracs.values[i]
#ifdef CIE_RENDER_TABLE_INTERPOLATE

	/*
	 * The final mapping step includes restriction to the
	 * ranges [0..dims[c]] as ints with interpolation bits.
	 */
	fixed rfix[3];
	const int s = _fixed_shift - _cie_interpolate_bits;

#define EABC(i)\
  cie_interpolate_fracs(pcrd->caches.EncodeABC[i].fixeds.ints.values, tabc[i])
#define FABC(i, s)\
  ((s) > 0) ? (EABC(i) << (s)) : (EABC(i) >> -(s))
	rfix[0] = FABC(0, s);
	rfix[1] = FABC(1, s);
	rfix[2] = FABC(2, s);
#undef FABC
#undef EABC
	if_debug6('c', "[c]ABC=%g,%g,%g => iabc=%g,%g,%g\n",
		  cie_cached2float(vec3.u), cie_cached2float(vec3.v),
		  cie_cached2float(vec3.w), fixed2float(rfix[0]),
		  fixed2float(rfix[1]), fixed2float(rfix[2]));
	gx_color_interpolate_linear(rfix, &pcrd->RenderTable.lookup,
				    pconc);
	if_debug3('c', "[c]  interpolated => %g,%g,%g\n",
		  frac2float(pconc[0]), frac2float(pconc[1]),
		  frac2float(pconc[2]));
	if (!pcrd->caches.RenderTableT_is_identity) {
	    /* Map the interpolated values. */
#define frac2cache_index(v) frac2bits(v, gx_cie_log2_cache_size)
	    pconc[0] = RT_LOOKUP(0, frac2cache_index(pconc[0]));
	    pconc[1] = RT_LOOKUP(1, frac2cache_index(pconc[1]));
	    pconc[2] = RT_LOOKUP(2, frac2cache_index(pconc[2]));
	    if (m > 3)
		pconc[3] = RT_LOOKUP(3, frac2cache_index(pconc[3]));
#undef frac2cache_index
	}

#else /* !CIE_RENDER_TABLE_INTERPOLATE */

	/*
	 * The final mapping step includes restriction to the ranges
	 * [0..dims[c]], plus scaling of the indices in the strings.
	 */
#define RI(i)\
  pcrd->caches.EncodeABC[i].ints.values[tabc[i] >> _cie_interpolate_bits]
	int ia = RI(0);
	int ib = RI(1);		/* pre-multiplied by m * NC */
	int ic = RI(2);		/* pre-multiplied by m */
	const byte *prtc = table[ia].data + ib + ic;

	/* (*pcrd->RenderTable.T)(prtc, m, pcrd, pconc); */

	if_debug6('c', "[c]ABC=%g,%g,%g => iabc=%d,%d,%d\n",
		  cie_cached2float(vec3.u), cie_cached2float(vec3.v),
		  cie_cached2float(vec3.w), ia, ib, ic);
	if (pcrd->caches.RenderTableT_is_identity) {
	    pconc[0] = byte2frac(prtc[0]);
	    pconc[1] = byte2frac(prtc[1]);
	    pconc[2] = byte2frac(prtc[2]);
	    if (m > 3)
		pconc[3] = byte2frac(prtc[3]);
	} else {
#if gx_cie_log2_cache_size == 8
#  define byte2cache_index(b) (b)
#else
# if gx_cie_log2_cache_size > 8
#  define byte2cache_index(b)\
    ( ((b) << (gx_cie_log2_cache_size - 8)) +\
      ((b) >> (16 - gx_cie_log2_cache_size)) )
# else				/* < 8 */
#  define byte2cache_index(b) ((b) >> (8 - gx_cie_log2_cache_size))
# endif
#endif
	    pconc[0] = RT_LOOKUP(0, byte2cache_index(prtc[0]));
	    pconc[1] = RT_LOOKUP(1, byte2cache_index(prtc[1]));
	    pconc[2] = RT_LOOKUP(2, byte2cache_index(prtc[2]));
	    if (m > 3)
		pconc[3] = RT_LOOKUP(3, byte2cache_index(prtc[3]));
#undef byte2cache_index
	}

#endif /* !CIE_RENDER_TABLE_INTERPOLATE */
#undef RI
#undef RT_LOOKUP
	return m;
    }
}

/*
 * Finish "remapping" a CIEBased color only to the XYZ intermediate values.
 * Note that we can't currently represent values outside the range [0..1]:
 * this is a bug that we will have to address someday.
 */
static frac
float2frac_clamp(floatp x)
{
    return float2frac((x <= 0 ? 0 : x >= 1 ? 1 : x));
}
int
gx_cie_xyz_remap_finish(cie_cached_vector3 vec3, frac * pconc,
			const gs_imager_state * pis,
			const gs_color_space *pcs)
{
    const gx_cie_joint_caches *pjc = pis->cie_joint_caches;

    /*
     * All the steps through DecodeABC/MatrixABC have been applied, i.e.,
     * vec3 is LMN values.  Just apply DecodeLMN/MatrixLMN.
     */
    if (!pjc->skipDecodeLMN)
	cie_lookup_map3(&vec3 /* LMN => XYZ */, &pjc->DecodeLMN,
			"Decode/MatrixLMN");


    pconc[0] = float2frac_clamp(cie_cached2float(vec3.u));
    pconc[1] = float2frac_clamp(cie_cached2float(vec3.v));
    pconc[2] = float2frac_clamp(cie_cached2float(vec3.w));
    return 3;
}

/* Look up 3 values in a cache, with cached post-multiplication. */
static void
cie_lookup_mult3(cie_cached_vector3 * pvec,
		 const gx_cie_vector_cache3_t * pc)
{
#ifdef CIE_CACHE_INTERPOLATE
    cie_cached_value u, v, w;

#ifdef CIE_CACHE_USE_FIXED
#  define LOOKUP_INTERPOLATE_BETWEEN(v0, v1, i)\
     cie_interpolate_between(v0, v1, i)
#else
    float ftemp;

#  define LOOKUP_INTERPOLATE_BETWEEN(v0, v1, i)\
     ((v0) + ((v1) - (v0)) *\
      ((ftemp = float_rshift(i, _cie_interpolate_bits)), ftemp - (int)ftemp))
#endif

	 /*
	  * Defining a macro for the entire component calculation would
	  * minimize source code, but it would make the result impossible
	  * to trace or debug.  We use smaller macros instead, and run
	  * the usual risks associated with having 3 copies of the code.
	  * Note that pvec and pc are free variables in these macros.
	  */

#define I_IN_RANGE(j, n)\
  (pvec->n >= pc->interpolation_ranges[j].rmin &&\
   pvec->n < pc->interpolation_ranges[j].rmax)
#define I_INDEX(j, n)\
  LOOKUP_INDEX(pvec->n, &pc->caches[j], _cie_interpolate_bits)
#define I_ENTRY(i, j)\
  &pc->caches[j].vecs.values[(int)cie_cached_rshift(i, _cie_interpolate_bits)]
#define I_ENTRY1(i, p)\
  (i >= (gx_cie_cache_size - 1) << _cie_interpolate_bits ? p : p + 1)

    if (I_IN_RANGE(0, u)) {
	cie_cached_value i = I_INDEX(0, u);
	const cie_cached_vector3 *p = I_ENTRY(i, 0);
	const cie_cached_vector3 *p1 = I_ENTRY1(i, p);

	if_debug0('C', "[c]Interpolating u.\n");
	u = LOOKUP_INTERPOLATE_BETWEEN(p->u, p1->u, i);
	v = LOOKUP_INTERPOLATE_BETWEEN(p->v, p1->v, i);
	w = LOOKUP_INTERPOLATE_BETWEEN(p->w, p1->w, i);
    } else {
	const cie_cached_vector3 *p = LOOKUP_ENTRY(pvec->u, &pc->caches[0]);

	if_debug0('C', "[c]Not interpolating u.\n");
	u = p->u, v = p->v, w = p->w;
    }

    if (I_IN_RANGE(1, v)) {
	cie_cached_value i = I_INDEX(1, v);
	const cie_cached_vector3 *p = I_ENTRY(i, 1);
	const cie_cached_vector3 *p1 = I_ENTRY1(i, p);

	if_debug0('C', "[c]Interpolating v.\n");
	u += LOOKUP_INTERPOLATE_BETWEEN(p->u, p1->u, i);
	v += LOOKUP_INTERPOLATE_BETWEEN(p->v, p1->v, i);
	w += LOOKUP_INTERPOLATE_BETWEEN(p->w, p1->w, i);
    } else {
	const cie_cached_vector3 *p = LOOKUP_ENTRY(pvec->v, &pc->caches[1]);

	if_debug0('C', "[c]Not interpolating v.\n");
	u += p->u, v += p->v, w += p->w;
    }

    if (I_IN_RANGE(2, w)) {
	cie_cached_value i = I_INDEX(2, w);
	const cie_cached_vector3 *p = I_ENTRY(i, 2);
	const cie_cached_vector3 *p1 = I_ENTRY1(i, p);

	if_debug0('C', "[c]Interpolating w.\n");
	u += LOOKUP_INTERPOLATE_BETWEEN(p->u, p1->u, i);
	v += LOOKUP_INTERPOLATE_BETWEEN(p->v, p1->v, i);
	w += LOOKUP_INTERPOLATE_BETWEEN(p->w, p1->w, i);
    } else {
	const cie_cached_vector3 *p = LOOKUP_ENTRY(pvec->w, &pc->caches[2]);

	if_debug0('C', "[c]Not interpolating w.\n");
	u += p->u, v += p->v, w += p->w;
    }

#undef I_IN_RANGE
#undef I_INDEX
#undef I_ENTRY
#undef I_ENTRY1

    pvec->u = u;
    pvec->v = v;
    pvec->w = w;

#else  /* no interpolation */

    const cie_cached_vector3 *pu = LOOKUP_ENTRY(pvec->u, &pc->caches[0]);
    const cie_cached_vector3 *pv = LOOKUP_ENTRY(pvec->v, &pc->caches[1]);
    const cie_cached_vector3 *pw = LOOKUP_ENTRY(pvec->w, &pc->caches[2]);

    if_debug0('C', "[c]Not interpolating.\n");

    pvec->u = pu->u + pv->u + pw->u;
    pvec->v = pu->v + pv->v + pw->v;
    pvec->w = pu->w + pv->w + pw->w;

#endif /* (no) interpolation */
}
