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
/* Implementation of the ICCBased color space family */

#include "math_.h"
#include "memory_.h"
#include "gx.h"
#include "gserrors.h"
#include "gsstruct.h"
#include "stream.h"
#include "gxcspace.h"		/* for gxcie.c */
#include "gxarith.h"
#include "gxcie.h"
#include "gzstate.h"
#include "gsicc.h"
#include "gsicc_cache.h"
#include "gsicc_littlecms.h"
#include "gxdevice.h"

#define SAVEICCPROFILE 0

/*
 * Discard a gs_cie_icc_s structure. This requires that we call the
 * destructor for ICC profile, lookup, and file objects (which are
 * stored in "foreign" memory).
 *
 * No special action is taken with respect to the stream pointer; that is
 * the responsibility of the client.  */
static void
cie_icc_finalize(void * pvicc_info)
{

    }

/*
 * Color space methods for ICCBased color spaces.
   ICC spaces are now considered to be concrete in that
   they always provide a mapping to a specified destination
   profile.  As such they will have their own remap functions.
   These will simply potentially implement the transfer function,
   apply any alpha value, and or end up going through halftoning. 
   There will not be any heuristic remap of rgb to gray etc */

static cs_proc_num_components(gx_num_components_ICC);
static cs_proc_init_color(gx_init_ICC);
static cs_proc_restrict_color(gx_restrict_ICC);
static cs_proc_concretize_color(gx_concretize_ICC);
static cs_proc_remap_color(gx_remap_ICC);
static cs_proc_install_cspace(gx_install_ICC);
static cs_proc_remap_concrete_color(gx_remap_concrete_ICC);
static cs_proc_final(gx_final_ICC);
static cs_proc_serialize(gx_serialize_ICC);
static cs_proc_is_linear(gx_cspace_is_linear_ICC);

const gs_color_space_type gs_color_space_type_ICC = {
    gs_color_space_index_ICC,       /* index */
    true,                           /* can_be_base_space */
    true,                           /* can_be_alt_space */
    &st_base_color_space,           /* stype - structure descriptor */
    gx_num_components_ICC,          /* num_components */
    gx_init_ICC,                    /* init_color */
    gx_restrict_ICC,                /* restrict_color */
    gx_same_concrete_space,         /* concrete_space */
    gx_concretize_ICC,              /* concreteize_color */
    gx_remap_concrete_ICC,          /* remap_concrete_color */
    gx_remap_ICC,                   /* remap_color */
    gx_install_ICC,                 /* install_cpsace */
    gx_spot_colors_set_overprint,   /* set_overprint */
    gx_final_ICC,                   /* final */
    gx_no_adjust_color_count,       /* adjust_color_count */
    gx_serialize_ICC,               /* serialize */
    gx_cspace_is_linear_ICC
};



static inline void
gsicc_remap_fast(unsigned short *psrc, unsigned short *psrc_cm,
		       gsicc_link_t *icc_link)
{
    gscms_transform_color(icc_link, psrc, psrc_cm, 2, NULL);
}

/* ICC color mapping linearity check, a 2-points case. Check only the 1/2 point */
static int
gx_icc_is_linear_in_line(const gs_color_space *cs, const gs_imager_state * pis,
		gx_device *dev, 
		const gs_client_color *c0, const gs_client_color *c1,
		float smoothness, gsicc_link_t *icclink)
{
    int nsrc = cs->type->num_components(cs);
    int ndes = dev->color_info.num_components;
    unsigned short src0[GS_CLIENT_COLOR_MAX_COMPONENTS];
    unsigned short src1[GS_CLIENT_COLOR_MAX_COMPONENTS];
    unsigned short src01[GS_CLIENT_COLOR_MAX_COMPONENTS];
    unsigned short des0[GS_CLIENT_COLOR_MAX_COMPONENTS];
    unsigned short des1[GS_CLIENT_COLOR_MAX_COMPONENTS];
    unsigned short des01[GS_CLIENT_COLOR_MAX_COMPONENTS];
    unsigned short interp_des;
    unsigned short max_diff = (unsigned short) max(1, 65535 * smoothness);
    int k;

    /* Get us to ushort and get mid point */
    for (k = 0; k < nsrc; k++) {
        src0[k] = (unsigned short) (c0->paint.values[k]*65535);
        src1[k] = (unsigned short) (c1->paint.values[k]*65535);
        src01[k] = ((unsigned int) src0[k] + (unsigned int) src1[k]) >> 1;
    }
    /* Transform the end points and the interpolated point */
    gsicc_remap_fast(&(src0[0]), &(des0[0]), icclink);
    gsicc_remap_fast(&(src1[0]), &(des1[0]), icclink);
    gsicc_remap_fast(&(src01[0]), &(des01[0]), icclink);
    /* Interpolate 1/2 value in des space and compare */
    for (k = 0; k < ndes; k++) {
        interp_des = (des0[k] + des1[k]) >> 1;
        if (any_abs((signed int) interp_des - (signed int) des01[k]) > max_diff)
            return false;
    }
    return 1;
}

/* Default icc color mapping linearity check, a triangle case. */
static int
gx_icc_is_linear_in_triangle(const gs_color_space *cs, const gs_imager_state * pis,
		gx_device *dev, 
		const gs_client_color *c0, const gs_client_color *c1,
		const gs_client_color *c2, float smoothness, gsicc_link_t *icclink)
{
    /* Check 4 points middle points of 3 sides and middle of one side with
       other point.  We avoid divisions this way. */
    unsigned short src0[GS_CLIENT_COLOR_MAX_COMPONENTS];
    unsigned short src1[GS_CLIENT_COLOR_MAX_COMPONENTS];
    unsigned short src2[GS_CLIENT_COLOR_MAX_COMPONENTS];
    unsigned short des0[GS_CLIENT_COLOR_MAX_COMPONENTS];
    unsigned short des1[GS_CLIENT_COLOR_MAX_COMPONENTS];
    unsigned short des2[GS_CLIENT_COLOR_MAX_COMPONENTS];
    unsigned short src01[GS_CLIENT_COLOR_MAX_COMPONENTS];
    unsigned short src12[GS_CLIENT_COLOR_MAX_COMPONENTS];
    unsigned short src02[GS_CLIENT_COLOR_MAX_COMPONENTS];
    unsigned short src012[GS_CLIENT_COLOR_MAX_COMPONENTS];
    unsigned short des01[GS_CLIENT_COLOR_MAX_COMPONENTS];
    unsigned short des12[GS_CLIENT_COLOR_MAX_COMPONENTS];
    unsigned short des02[GS_CLIENT_COLOR_MAX_COMPONENTS];
    unsigned short des012[GS_CLIENT_COLOR_MAX_COMPONENTS];
    int nsrc = cs->type->num_components(cs);
    int ndes = dev->color_info.num_components;
    unsigned short max_diff = (unsigned short) max(1, 65535 * smoothness);
    unsigned int interp_des;
    int k;

    /* This needs to be optimized. And range corrected */
    for (k = 0; k < nsrc; k++){
        src0[k] = (unsigned short) (c0->paint.values[k]*65535);
        src1[k] = (unsigned short) (c1->paint.values[k]*65535);
        src2[k] = (unsigned short) (c2->paint.values[k]*65535);
        src01[k] = (src0[k] + src1[k]) >> 1;
        src02[k] = (src0[k] + src2[k]) >> 1;   
        src12[k] = (src1[k] + src2[k]) >> 1;
        src012[k] = (src12[k] + src0[k]) >> 1;  
    }
    /* Map the points */
    gsicc_remap_fast(&(src0[0]), &(des0[0]), icclink);
    gsicc_remap_fast(&(src1[0]), &(des1[0]), icclink);
    gsicc_remap_fast(&(src2[0]), &(des2[0]), icclink);
    gsicc_remap_fast(&(src01[0]), &(des01[0]), icclink);
    gsicc_remap_fast(&(src12[0]), &(des12[0]), icclink);
    gsicc_remap_fast(&(src02[0]), &(des02[0]), icclink);
    gsicc_remap_fast(&(src012[0]), &(des012[0]), icclink);
    /* Interpolate in des space and check it */
    for (k = 0; k < ndes; k++){
        interp_des = (des0[k] + des1[k]) >> 1;
        if (any_abs((signed int) interp_des - (signed int) des01[k]) > max_diff)
            return false;
        interp_des = (des0[k] + des2[k]) >> 1;
        if (any_abs((signed int) interp_des - (signed int) des02[k]) > max_diff)
            return false;
        interp_des = (des1[k] + des2[k]) >> 1;
        if (any_abs((signed int) interp_des - (signed int) des12[k]) > max_diff)
            return false;
        /* 12 with 0 */
        interp_des = (des0[k] + interp_des) >> 1;
        if (any_abs((signed int) interp_des - (signed int) des012[k]) > max_diff)
            return false;
    }
    return 1;
}

/* ICC color mapping linearity check. */
int
gx_cspace_is_linear_ICC(const gs_color_space *cs, const gs_imager_state * pis,
		gx_device *dev, 
		const gs_client_color *c0, const gs_client_color *c1,
		const gs_client_color *c2, const gs_client_color *c3,
		float smoothness, gsicc_link_t *icclink)
{
    /* Assuming 2 <= nc <= 4. We don't need other cases. */
    /* With nc == 4 assuming a convex plain quadrangle in the client color space. */
    int code;
    
    /* Do a quick check if we are in a halftone situation. If yes,
       then we should not be doing this linear check */
    if (gx_device_must_halftone(dev)) return 0;
    if (icclink->is_identity) return 1; /* Transform is identity, linear! */

    if (dev->color_info.separable_and_linear != GX_CINFO_SEP_LIN)
	return_error(gs_error_rangecheck);
    if (c2 == NULL)
	return gx_icc_is_linear_in_line(cs, pis, dev, c0, c1, smoothness, icclink);
    code = gx_icc_is_linear_in_triangle(cs, pis, dev, c0, c1, c2, 
                                            smoothness, icclink);
    if (code <= 0)
	return code;
    if (c3 == NULL)
	return 1;
    return gx_icc_is_linear_in_triangle(cs, pis, dev, c1, c2, c3, 
                                                smoothness, icclink);
}
/*
 * Return the number of components used by a ICCBased color space - 1, 3, or 4
 */
static int
gx_num_components_ICC(const gs_color_space * pcs)
{
    return pcs->cmm_icc_profile_data->num_comps;
}

/*
 * Set the initial client color for an ICCBased color space. The convention
 * suggested by the ICC specification is to set all components to 0.
 */
static void
gx_init_ICC(gs_client_color * pcc, const gs_color_space * pcs)
{
    int     i, ncomps = pcs->cmm_icc_profile_data->num_comps;

    for (i = 0; i < ncomps; ++i)
	pcc->paint.values[i] = 0.0;

    /* make sure that [ 0, ... 0] is in range */
    gx_restrict_ICC(pcc, pcs);
}

/*
 * Restrict an color to the range specified for an ICCBased color space.
 */
static void
gx_restrict_ICC(gs_client_color * pcc, const gs_color_space * pcs)
{
    int                 i, ncomps = pcs->cmm_icc_profile_data->num_comps;
    const gs_range *    ranges = pcs->cmm_icc_profile_data->Range.ranges;

    for (i = 0; i < ncomps; ++i) {
        floatp  v = pcc->paint.values[i];
        floatp  rmin = ranges[i].rmin, rmax = ranges[i].rmax;

        if (v < rmin)
            pcc->paint.values[i] = rmin;
        else if (v > rmax)
            pcc->paint.values[i] = rmax;
    }
}

/* If the color is already concretized, then we are in the color space 
   defined by the device profile.  The remaining things to do would
   be to potentially apply alpha, apply the transfer function, and
   do any halftoning.  The remap is based upon the ICC profile defined
   in the device profile entry of the profile manager. */
int
gx_remap_concrete_ICC(const frac * pconc, const gs_color_space * pcs,
	gx_device_color * pdc, const gs_imager_state * pis, gx_device * dev,
			  gs_color_select_t select)
{
    const gsicc_manager_t *icc_manager = pis->icc_manager;
    const cmm_profile_t *device_profile = icc_manager->device_profile;
    int num_colorants = device_profile->num_comps;
    int code;

    switch( num_colorants ) {
        case 1: 
            code = gx_remap_concrete_DGray(pconc, pcs, pdc, pis, dev, select);
            break;
        case 3:
            code = gx_remap_concrete_DRGB(pconc, pcs, pdc, pis, dev, select);
            break;
        case 4:
            code = gx_remap_concrete_DCMYK(pconc, pcs, pdc, pis, dev, select);
            break;
        default:
            /* Need to do some work on integrating DeviceN and the new ICC flow */
            /* code = gx_remap_concrete_DeviceN(pconc, pcs, pdc, pis, dev, select); */
            code = -1;
            break;
        }
    return(code);
    }

/*
 * To device space
 */
int
gx_remap_ICC(const gs_client_color * pcc, const gs_color_space * pcs,
	gx_device_color * pdc, const gs_imager_state * pis, gx_device * dev,
		gs_color_select_t select)
{
    gsicc_link_t *icc_link;
    gsicc_rendering_param_t rendering_params;
    unsigned short psrc[GS_CLIENT_COLOR_MAX_COMPONENTS], psrc_cm[GS_CLIENT_COLOR_MAX_COMPONENTS];
    unsigned short *psrc_temp;
    frac conc[GS_CLIENT_COLOR_MAX_COMPONENTS];
    int k,i;
#ifdef DEBUG
    int num_src_comps;
    int num_des_comps;
#endif

    rendering_params.black_point_comp = BP_ON;
    rendering_params.object_type = GS_PATH_TAG;
    rendering_params.rendering_intent = pis->renderingintent;

    /* Need to clear out psrc_cm in case we have separation bands that are
       not color managed */
    memset(psrc_cm,0,sizeof(unsigned short)*GS_CLIENT_COLOR_MAX_COMPONENTS);

     /* This needs to be optimized */
    if (pcs->cmm_icc_profile_data->data_cs == gsCIELAB) {
        psrc[0] = (unsigned short) (pcc->paint.values[0]*65535.0/100.0);
        psrc[1] = (unsigned short) ((pcc->paint.values[1]+128)/255.0*65535.0);
        psrc[2] = (unsigned short) ((pcc->paint.values[2]+128)/255.0*65535.0);
    } else {
        for (k = 0; k < pcs->cmm_icc_profile_data->num_comps; k++){
            psrc[k] = (unsigned short) (pcc->paint.values[k]*65535.0);
}
}
    /* Get a link from the cache, or create if it is not there. Need to get 16 bit profile */
    icc_link = gsicc_get_link(pis, pcs, NULL, &rendering_params, pis->memory, false);
    if (icc_link->is_identity) {
        psrc_temp = &(psrc[0]);
    } else {
        /* Transform the color */
        psrc_temp = &(psrc_cm[0]);
        gscms_transform_color(icc_link, psrc, psrc_temp, 2, NULL);
}
#ifdef DEBUG
    if (!icc_link->is_identity) {
        num_src_comps = pcs->cmm_icc_profile_data->num_comps;
        num_des_comps = pis->icc_manager->device_profile->num_comps;
        if_debug0('c',"[c]ICC remap [ ");
        for (k = 0; k < num_src_comps; k++) {
            if_debug1('c', "%d ",psrc[k]);
}
        if_debug0('c',"] --> [ ");
        for (k = 0; k < num_des_comps; k++) {
            if_debug1('c', "%d ",psrc_temp[k]);
}
        if_debug0('c',"]\n");
}
#endif
    /* Release the link */
    gsicc_release_link(icc_link);
    /* Now do the remap for ICC which amounts to the alpha application
       the transfer function and potentially the halftoning */
    /* Right now we need to go from unsigned short to frac.  I really
       would like to avoid this sort of stuff.  That will come. */
    for ( k = 0; k< pis->icc_manager->device_profile->num_comps; k++){
        conc[k] = ushort2frac(psrc_temp[k]);
    }
    gx_remap_concrete_ICC(conc, pcs, pdc, pis, dev, select);

    /* Save original color space and color info into dev color */
    i = pcs->cmm_icc_profile_data->num_comps;
    for (i--; i >= 0; i--)
	pdc->ccolor.paint.values[i] = pcc->paint.values[i];
    pdc->ccolor_valid = true;
    return 0;
}

    /*
 * Convert an ICCBased color space to a concrete color space.
     */
static int
gx_concretize_ICC(
    const gs_client_color * pcc,
    const gs_color_space *  pcs,
    frac *                  pconc,
    const gs_imager_state * pis )
    {

    gsicc_link_t *icc_link;
    gsicc_rendering_param_t rendering_params;
    unsigned short psrc[GS_CLIENT_COLOR_MAX_COMPONENTS], psrc_cm[GS_CLIENT_COLOR_MAX_COMPONENTS];
    int k;
    unsigned short *psrc_temp;

    /* Define the rendering intents.  MJV to fix */
    rendering_params.black_point_comp = BP_ON;
    rendering_params.object_type = GS_PATH_TAG;
    rendering_params.rendering_intent = pis->renderingintent;
    for (k = 0; k < pcs->cmm_icc_profile_data->num_comps; k++){
        psrc[k] = (unsigned short) (pcc->paint.values[k]*65535.0);
	}
    /* Get a link from the cache, or create if it is not there. Get 16 bit profile */
    icc_link = gsicc_get_link(pis, pcs, NULL, &rendering_params, pis->memory, false);
    /* Transform the color */
    if (icc_link->is_identity) {
        psrc_temp = &(psrc[0]);
    } else {
        /* Transform the color */
        psrc_temp = &(psrc_cm[0]);
        gscms_transform_color(icc_link, psrc, psrc_temp, 2, NULL);
    }
    /* This needs to be optimized */
    for (k = 0; k < pis->icc_manager->device_profile->num_comps; k++){
        pconc[k] = float2frac(((float) psrc_temp[k])/65535.0);
    }
    /* Release the link */
    gsicc_release_link(icc_link);
    return 0;
}

	/*
 * Finalize the contents of an ICC color space. Now that color space
 * objects have straightforward reference counting discipline, there's
 * nothing special about it. In the previous state of affairs, the
 * argument in favor of correct reference counting spoke of "an
 * unintuitive but otherwise legitimate state of affairs".
	 */
static void
gx_final_ICC(const gs_color_space * pcs)
{
  /*  if (pcs->cmm_icc_profile_data != NULL) {
        rc_decrement_only(pcs->cmm_icc_profile_data, "gx_final_ICC");
    } */
    /* rc_decrement_only(pcs->params.icc.picc_info, "gx_final_ICC"); */
    }

/*
 * Install an ICCBased color space.
 *
 * Note that an ICCBased color space must be installed before it is known if
 * the ICC profile or the alternate color space is to be used.
 */
static int
gx_install_ICC(gs_color_space * pcs, gs_state * pgs)
{
    /* update the stub information used by the joint caches */
		return 0;
        }

/*
 * Constructor for ICCBased color space. As with the other color space
 * constructors, this provides only minimal initialization.
 */
int
gs_cspace_build_ICC(
    gs_color_space **   ppcspace,
    void *              client_data,
    gs_memory_t *       pmem )
{
    gs_color_space *pcspace = gs_cspace_alloc(pmem, &gs_color_space_type_ICC);
    *ppcspace = pcspace;

    return 0;
}

/* ---------------- Serialization. -------------------------------- */

static int
gx_serialize_ICC(const gs_color_space * pcs, stream * s)
{
    gsicc_serialized_profile_t *profile__serial;
    uint n;
    int code = gx_serialize_cspace_type(pcs, s);

    if (code < 0)
	return code;
    profile__serial = (gsicc_serialized_profile_t*) pcs->cmm_icc_profile_data;
    code = sputs(s, (byte *)profile__serial, sizeof(gsicc_serialized_profile_t), &n);
    return(code);
}