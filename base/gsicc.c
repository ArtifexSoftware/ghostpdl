/* Copyright (C) 2001-2023 Artifex Software, Inc.
   All Rights Reserved.

   This software is provided AS-IS with no warranty, either express or
   implied.

   This software is distributed under license and may not be copied,
   modified or distributed except as expressly authorized under the terms
   of the license contained in the file LICENSE in this distribution.

   Refer to licensing information at http://www.artifex.com or contact
   Artifex Software, Inc.,  39 Mesa Street, Suite 108A, San Francisco,
   CA 94129, USA, for further information.
*/


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
#include "gsicc_cms.h"
#include "gsicc_manage.h"
#include "gxdevice.h"
#include "gsccolor.h"
#include "gxdevsop.h"

#define SAVEICCPROFILE 0

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
static cs_proc_set_overprint(gx_set_overprint_ICC);
static cs_proc_polarity(gx_polarity_ICC);
cs_proc_remap_color(gx_remap_ICC_imagelab);

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
    gx_set_overprint_ICC,           /* set_overprint */
    gx_final_ICC,                   /* final */
    gx_no_adjust_color_count,       /* adjust_color_count */
    gx_serialize_ICC,               /* serialize */
    gx_cspace_is_linear_ICC,
    gx_polarity_ICC
};

static inline void
gsicc_remap_fast(gx_device *dev, unsigned short *psrc, unsigned short *psrc_cm,
                 gsicc_link_t *icc_link)
{
    (icc_link->procs.map_color)(dev, icc_link, psrc, psrc_cm, 2);
}

/* ICC color mapping linearity check, a 2-points case. Check only the 1/2 point */
static int
gx_icc_is_linear_in_line(const gs_color_space *cs, const gs_gstate * pgs,
                        gx_device *dev,
                        const gs_client_color *c0, const gs_client_color *c1,
                        float smoothness, gsicc_link_t *icclink)
{
    int nsrc = cs->type->num_components(cs);
    cmm_dev_profile_t *dev_profile;
    int ndes;
    int code;
    unsigned short src0[GS_CLIENT_COLOR_MAX_COMPONENTS];
    unsigned short src1[GS_CLIENT_COLOR_MAX_COMPONENTS];
    unsigned short src01[GS_CLIENT_COLOR_MAX_COMPONENTS];
    unsigned short des0[GS_CLIENT_COLOR_MAX_COMPONENTS];
    unsigned short des1[GS_CLIENT_COLOR_MAX_COMPONENTS];
    unsigned short des01[GS_CLIENT_COLOR_MAX_COMPONENTS];
    unsigned short interp_des;
    unsigned short max_diff = (unsigned short) max(1, 65535 * smoothness);
    int k;

    code = dev_proc(dev, get_profile)(dev, &(dev_profile));
    if (code < 0)
        return code;
    ndes = gsicc_get_device_profile_comps(dev_profile);

    /* Get us to ushort and get mid point */
    for (k = 0; k < nsrc; k++) {
        src0[k] = (unsigned short) (c0->paint.values[k]*65535);
        src1[k] = (unsigned short) (c1->paint.values[k]*65535);
        src01[k] = ((unsigned int) src0[k] + (unsigned int) src1[k]) >> 1;
    }
    /* Transform the end points and the interpolated point */
    gsicc_remap_fast(dev, &(src0[0]), &(des0[0]), icclink);
    gsicc_remap_fast(dev, &(src1[0]), &(des1[0]), icclink);
    gsicc_remap_fast(dev, &(src01[0]), &(des01[0]), icclink);
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
gx_icc_is_linear_in_triangle(const gs_color_space *cs, const gs_gstate * pgs,
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
    int ndes, code;
    unsigned short max_diff = (unsigned short) max(1, 65535 * smoothness);
    unsigned int interp_des;
    int k;
    cmm_dev_profile_t *dev_profile;

    code = dev_proc(dev, get_profile)(dev, &(dev_profile));
    if (code < 0)
        return code;
    ndes = gsicc_get_device_profile_comps(dev_profile);

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
    gsicc_remap_fast(dev, &(src0[0]), &(des0[0]), icclink);
    gsicc_remap_fast(dev, &(src1[0]), &(des1[0]), icclink);
    gsicc_remap_fast(dev, &(src2[0]), &(des2[0]), icclink);
    gsicc_remap_fast(dev, &(src01[0]), &(des01[0]), icclink);
    gsicc_remap_fast(dev, &(src12[0]), &(des12[0]), icclink);
    gsicc_remap_fast(dev, &(src02[0]), &(des02[0]), icclink);
    gsicc_remap_fast(dev, &(src012[0]), &(des012[0]), icclink);
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
gx_cspace_is_linear_ICC(const gs_color_space *cs, const gs_gstate * pgs,
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

    if (!colors_are_separable_and_linear(&dev->color_info))
        return_error(gs_error_rangecheck);
    if (c2 == NULL)
        return gx_icc_is_linear_in_line(cs, pgs, dev, c0, c1, smoothness, icclink);
    code = gx_icc_is_linear_in_triangle(cs, pgs, dev, c0, c1, c2,
                                            smoothness, icclink);
    if (code <= 0)
        return code;
    if (c3 == NULL)
        return 1;
    return gx_icc_is_linear_in_triangle(cs, pgs, dev, c1, c2, c3,
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

/* Get the polarity of the ICC Based space */
static gx_color_polarity_t
gx_polarity_ICC(const gs_color_space * pcs)
{
    switch (pcs->cmm_icc_profile_data->data_cs) {
        case gsUNDEFINED:
        case gsNAMED:
            return GX_CINFO_POLARITY_UNKNOWN;
        case gsGRAY:
        case gsRGB:
        case gsCIELAB:
        case gsCIEXYZ:
            return GX_CINFO_POLARITY_ADDITIVE;
        case gsCMYK:
        case gsNCHANNEL:
            return GX_CINFO_POLARITY_SUBTRACTIVE;
        default:
            return GX_CINFO_POLARITY_UNKNOWN;
    }
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
        double  v = pcc->paint.values[i];
        double  rmin = ranges[i].rmin, rmax = ranges[i].rmax;

        if (v < rmin)
            pcc->paint.values[i] = rmin;
        else if (v > rmax)
            pcc->paint.values[i] = rmax;
    }
}

static int
gx_remap_concrete_icc_devicen(const gs_color_space * pcs, const frac * pconc,
                              gx_device_color * pdc, const gs_gstate * pgs,
                              gx_device * dev, gs_color_select_t select,
                              const cmm_dev_profile_t *dev_profile)
{
    /* Check if this is a device with a DeviceN ICC profile.  In this case,
       we need to do some special stuff */
    int code = 0;

    if (dev_profile->spotnames != NULL  &&
        !dev_profile->spotnames->equiv_cmyk_set) {
        /* This means that someone has specified a DeviceN (Ncolor)
           ICC destination profile for this device and we still need to set
           up the equivalent CMYK colors for the spot colors that are present.
           This allows us to have some sort of composite viewing of the spot
           colors as they would colorimetrically appear. */
        code = gsicc_set_devicen_equiv_colors(dev, pgs,
                        dev_profile->device_profile[GS_DEFAULT_DEVICE_PROFILE]);
        dev_profile->spotnames->equiv_cmyk_set = true;
    }
    gx_remap_concrete_devicen(pconc, pdc, pgs, dev, select, pcs);
    return code;
}

/* If the color is already concretized, then we are in the color space
   defined by the device profile.  The remaining things to do would
   be to potentially apply alpha, apply the transfer function, and
   do any halftoning.  The remap is based upon the ICC profile defined
   in the device profile entry of the profile manager. */
int
gx_remap_concrete_ICC(const gs_color_space * pcs, const frac * pconc,
                      gx_device_color * pdc, const gs_gstate * pgs,
                      gx_device * dev, gs_color_select_t select,
                      const cmm_dev_profile_t *dev_profile)
{
    switch (gsicc_get_device_profile_comps(dev_profile)) {
        case 1:
            return gx_remap_concrete_DGray(pcs, pconc, pdc, pgs, dev, select, dev_profile);
        case 3:
            return gx_remap_concrete_DRGB(pcs, pconc, pdc, pgs, dev, select, dev_profile);
        case 4:
            return gx_remap_concrete_DCMYK(pcs, pconc, pdc, pgs, dev, select, dev_profile);
        default:
            /* This is a special case where we have a source color and our
               output profile must be DeviceN.   We will need to map our
               colorants to the proper planes */
            return gx_remap_concrete_icc_devicen(pcs, pconc, pdc, pgs, dev, select, dev_profile);
    }
}

/*
 * To device space
 */
int
gx_remap_ICC_with_link(const gs_client_color * pcc, const gs_color_space * pcs,
        gx_device_color * pdc, const gs_gstate * pgs, gx_device * dev,
                gs_color_select_t select, gsicc_link_t *icc_link)
{
    cmm_dev_profile_t *dev_profile;
    unsigned short psrc[GS_CLIENT_COLOR_MAX_COMPONENTS], psrc_cm[GS_CLIENT_COLOR_MAX_COMPONENTS];
    unsigned short *psrc_temp;
    frac conc[GS_CLIENT_COLOR_MAX_COMPONENTS];
    int k,i;
#ifdef DEBUG
    int num_src_comps;
#endif
    int num_des_comps;
    int code;

    code = dev_proc(dev, get_profile)(dev, &dev_profile);
    if (code < 0)
        return code;
    if (dev_profile == NULL)
        return gs_throw(gs_error_Fatal, "Attempting to do ICC remap with no profile");
    if (icc_link == NULL)
        return gs_throw(gs_error_Fatal, "Attempting to do ICC remap with no link");

    /* Need to clear out psrc_cm in case we have separation bands that are
       not color managed */
    memset(psrc_cm,0,sizeof(unsigned short)*GS_CLIENT_COLOR_MAX_COMPONENTS);

     /* This needs to be optimized */
    if (pcs->cmm_icc_profile_data->data_cs == gsCIELAB ||
        pcs->cmm_icc_profile_data->islab) {
        psrc[0] = (unsigned short) (pcc->paint.values[0]*65535.0/100.0);
        psrc[1] = (unsigned short) ((pcc->paint.values[1]+128)/255.0*65535.0);
        psrc[2] = (unsigned short) ((pcc->paint.values[2]+128)/255.0*65535.0);
    } else {
        for (k = 0; k < pcs->cmm_icc_profile_data->num_comps; k++){
            psrc[k] = (unsigned short) (pcc->paint.values[k]*65535.0);
        }
    }
    num_des_comps = gsicc_get_device_profile_comps(dev_profile);
    if (icc_link->is_identity) {
        psrc_temp = &(psrc[0]);
    } else {
        /* Transform the color */
        psrc_temp = &(psrc_cm[0]);
        (icc_link->procs.map_color)(dev, icc_link, psrc, psrc_temp, 2);
    }
#ifdef DEBUG
    if (!icc_link->is_identity) {
        num_src_comps = pcs->cmm_icc_profile_data->num_comps;
        if_debug0m(gs_debug_flag_icc, dev->memory, "[icc] remap [ ");
        for (k = 0; k < num_src_comps; k++) {
            if_debug1m(gs_debug_flag_icc, dev->memory, "%d ", psrc[k]);
        }
        if_debug0m(gs_debug_flag_icc, dev->memory, "] --> [ ");
        for (k = 0; k < num_des_comps; k++) {
            if_debug1m(gs_debug_flag_icc, dev->memory, "%d ", psrc_temp[k]);
        }
        if_debug0m(gs_debug_flag_icc, dev->memory, "]\n");
    } else {
        num_src_comps = pcs->cmm_icc_profile_data->num_comps;
        if_debug0m(gs_debug_flag_icc, dev->memory, "[icc] Identity mapping\n");
        if_debug0m(gs_debug_flag_icc, dev->memory, "[icc] [ ");
        for (k = 0; k < num_src_comps; k++) {
            if_debug1m(gs_debug_flag_icc, dev->memory, "%d ", psrc[k]);
        }
        if_debug0m(gs_debug_flag_icc, dev->memory, "]\n");
    }
#endif
    /* Now do the remap for ICC which amounts to the alpha application
       the transfer function and potentially the halftoning */
    /* Right now we need to go from unsigned short to frac.  I really
       would like to avoid this sort of stuff.  That will come. */
    for (k = 0; k < num_des_comps; k++){
        conc[k] = ushort2frac(psrc_temp[k]);
    }
    /* In case there are extra components beyond the ICC ones */
    for (k = num_des_comps; k < dev->color_info.num_components; k++) {
        conc[k] = 0;
    }
    gx_remap_concrete_ICC(pcs, conc, pdc, pgs, dev, select, dev_profile);

    /* Save original color space and color info into dev color */
    i = pcs->cmm_icc_profile_data->num_comps;
    for (i--; i >= 0; i--)
        pdc->ccolor.paint.values[i] = pcc->paint.values[i];
    pdc->ccolor_valid = true;
    return 0;
}

int
gx_remap_ICC(const gs_client_color * pcc, const gs_color_space * pcs,
        gx_device_color * pdc, const gs_gstate * pgs, gx_device * dev,
                gs_color_select_t select)
{
    gsicc_link_t *icc_link;
    gsicc_rendering_param_t rendering_params;
    cmm_dev_profile_t *dev_profile;
    int code;

    color_replace_t param;
    param.pcc = pcc;
    param.pcs = pcs;
    param.pdc = pdc;
    param.pgs = pgs;
    param.pdf14_iccprofile = NULL;

    /* Try color replacement. If successful (>0) then no
       ICC color management for this color. */
    if (dev_proc(pgs->device, dev_spec_op)(pgs->device,
        gxdso_replacecolor, &param, sizeof(color_replace_t)) > 0)
        return 0;

    code = dev_proc(dev, get_profile)(dev, &dev_profile);
    if (code < 0)
        return code;
    if (dev_profile == NULL)
        return gs_throw(gs_error_Fatal, "Attempting to do ICC remap with no profile");

    rendering_params.black_point_comp = pgs->blackptcomp;
    rendering_params.graphics_type_tag = dev->graphics_type_tag;
    rendering_params.override_icc = false;
    rendering_params.preserve_black = gsBKPRESNOTSPECIFIED;
    rendering_params.rendering_intent = pgs->renderingintent;
    rendering_params.cmm = gsCMM_DEFAULT;
    /* Get a link from the cache, or create if it is not there. Need to get 16 bit profile */
    icc_link = gsicc_get_link(pgs, dev, pcs, NULL, &rendering_params, pgs->memory);
    if (icc_link == NULL) {
#ifdef DEBUG
        gs_warn("Could not create ICC link:  Check profiles");
#endif
        return_error(gs_error_unknownerror);
    }

    code = gx_remap_ICC_with_link(pcc, pcs, pdc, pgs, dev, select, icc_link);
    /* Release the link */
    gsicc_release_link(icc_link);
    return code;
}

/*
 * Same as above, but there is no rescale of CIELAB colors.  This is needed
   since the rescale is not needed when the source data is image based.
   The DeviceN image rendering case uses the remap proc vs. the ICC based method
   which handles the remapping itself.
 */
int
gx_remap_ICC_imagelab(const gs_client_color * pcc, const gs_color_space * pcs,
        gx_device_color * pdc, const gs_gstate * pgs, gx_device * dev,
                gs_color_select_t select)
{
    gsicc_link_t *icc_link;
    gsicc_rendering_param_t rendering_params;
    unsigned short psrc[GS_CLIENT_COLOR_MAX_COMPONENTS], psrc_cm[GS_CLIENT_COLOR_MAX_COMPONENTS];
    unsigned short *psrc_temp;
    frac conc[GS_CLIENT_COLOR_MAX_COMPONENTS];
    int k,i;
    int num_des_comps;
    int code;
    cmm_dev_profile_t *dev_profile;

    code = dev_proc(dev, get_profile)(dev, &dev_profile);
    if (code < 0)
        return code;
    num_des_comps = gsicc_get_device_profile_comps(dev_profile);
    rendering_params.black_point_comp = pgs->blackptcomp;
    rendering_params.graphics_type_tag = dev->graphics_type_tag;
    rendering_params.override_icc = false;
    rendering_params.preserve_black = gsBKPRESNOTSPECIFIED;
    rendering_params.rendering_intent = pgs->renderingintent;
    rendering_params.cmm = gsCMM_DEFAULT;
    /* Need to clear out psrc_cm in case we have separation bands that are
       not color managed */
    memset(psrc_cm, 0, sizeof(unsigned short)*GS_CLIENT_COLOR_MAX_COMPONENTS);

    for (k = 0; k < pcs->cmm_icc_profile_data->num_comps; k++)
        psrc[k] = (unsigned short) (pcc->paint.values[k]*65535.0);

    /* Get a link from the cache, or create if it is not there. Need to get 16 bit profile */
    icc_link = gsicc_get_link(pgs, dev, pcs, NULL, &rendering_params, pgs->memory);
    if (icc_link == NULL) {
#ifdef DEBUG
        gs_warn("Could not create ICC link:  Check profiles");
#endif
        return_error(gs_error_unknownerror);
    }
    if (icc_link->is_identity) {
        psrc_temp = &(psrc[0]);
    } else {
        /* Transform the color */
        psrc_temp = &(psrc_cm[0]);
        (icc_link->procs.map_color)(dev, icc_link, psrc, psrc_temp, 2);
    }
    /* Release the link */
    gsicc_release_link(icc_link);
    /* Now do the remap for ICC which amounts to the alpha application
       the transfer function and potentially the halftoning */
    /* Right now we need to go from unsigned short to frac.  I really
       would like to avoid this sort of stuff.  That will come. */
    for (k = 0; k < num_des_comps; k++){
        conc[k] = ushort2frac(psrc_temp[k]);
    }
    /* We have to worry about extra colorants in the device. */
    for (k = num_des_comps; k < dev->color_info.num_components; k++) {
        conc[k] = 0;
    }
    gx_remap_concrete_ICC(pcs, conc, pdc, pgs, dev, select, dev_profile);

    /* Save original color space and color info into dev color */
    i = pcs->cmm_icc_profile_data->num_comps;
    for (i--; i >= 0; i--)
        pdc->ccolor.paint.values[i] = pcc->paint.values[i];
    pdc->ccolor_valid = true;
    return 0;
}

/* Convert an ICCBased color space to a concrete color space. */

static int
gx_concretize_ICC(
    const gs_client_color * pcc,
    const gs_color_space *  pcs,
    frac *                  pconc,
    const gs_gstate * pgs,
    gx_device *dev)
    {

    gsicc_link_t *icc_link;
    gsicc_rendering_param_t rendering_params;
    unsigned short psrc[GS_CLIENT_COLOR_MAX_COMPONENTS], psrc_cm[GS_CLIENT_COLOR_MAX_COMPONENTS];
    int k;
    unsigned short *psrc_temp;
    int num_des_comps;
    int code;
    cmm_dev_profile_t *dev_profile;

    code = dev_proc(dev, get_profile)(dev, &dev_profile);
    if (code < 0)
        return code;
    num_des_comps = gsicc_get_device_profile_comps(dev_profile);
    /* Define the rendering intents.  */
    rendering_params.black_point_comp = pgs->blackptcomp;
    rendering_params.graphics_type_tag = dev->graphics_type_tag;
    rendering_params.override_icc = false;
    rendering_params.preserve_black = gsBKPRESNOTSPECIFIED;
    rendering_params.rendering_intent = pgs->renderingintent;
    rendering_params.cmm = gsCMM_DEFAULT;
    for (k = 0; k < pcs->cmm_icc_profile_data->num_comps; k++) {
        psrc[k] = (unsigned short) (pcc->paint.values[k]*65535.0);
    }
    /* Get a link from the cache, or create if it is not there. Get 16 bit profile */
    icc_link = gsicc_get_link(pgs, dev, pcs, NULL, &rendering_params, pgs->memory);
    if (icc_link == NULL) {
#ifdef DEBUG
        gs_warn("Could not create ICC link:  Check profiles");
#endif
        return_error(gs_error_unknownerror);
    }
    /* Transform the color */
    if (icc_link->is_identity) {
        psrc_temp = &(psrc[0]);
    } else {
        /* Transform the color */
        psrc_temp = &(psrc_cm[0]);
        (icc_link->procs.map_color)(dev, icc_link, psrc, psrc_temp, 2);
    }
    /* This needs to be optimized */
    for (k = 0; k < num_des_comps; k++) {
        pconc[k] = float2frac(((float) psrc_temp[k])/65535.0);
    }
    /* We have to worry about extra colorants in the device. */
    for (k = num_des_comps; k < dev->color_info.num_components; k++) {
        pconc[k] = 0;
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
gx_final_ICC(gs_color_space * pcs)
{
    if (pcs->cmm_icc_profile_data != NULL) {
        gsicc_adjust_profile_rc(pcs->cmm_icc_profile_data, -1, "gx_final_ICC");
        pcs->cmm_icc_profile_data = NULL;
    }
}

/*
 * Install an ICCBased color space.
 *
 * Note that an ICCBased color space must be installed before it is known if
 * the ICC profile or the alternate color space is to be used.
 */
static int
gx_install_ICC(gs_color_space * pcs, gs_gstate * pgs)
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
    if (!pcspace) {
        return_error(gs_error_VMerror);
    }
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
    code = sputs(s, (byte *)profile__serial, GSICC_SERIALIZED_SIZE, &n);
    return(code);
}

/* Overprint.  Here we may have either spot colors or CMYK colors. */
static int
gx_set_overprint_ICC(const gs_color_space * pcs, gs_gstate * pgs)
{
    gx_device *dev = pgs->device;
    gx_device_color_info *pcinfo = (dev == 0 ? 0 : &dev->color_info);
    bool cs_ok;
    cmm_dev_profile_t *dev_profile;
    bool gray_to_k;
    bool op = pgs->is_fill_color ? pgs->overprint : pgs->stroke_overprint;

    if (dev == 0 || pcinfo == NULL || !op ||
        gx_get_opmsupported(dev) == GX_CINFO_OPMSUPPORTED_NOT)
        return gx_set_no_overprint(pgs);

    dev_proc(dev, get_profile)(dev, &dev_profile);
    gray_to_k = dev_profile->devicegraytok;

    /* Possibly do CMYK based overprinting if profile is CMYK based or if we
       are gray source based and doing gray to k mapping
       (Ghent GWG 3.0 Gray Overprint Patch (030_Gray_K_black_OP_x1a.pdf) */
    cs_ok = ((pcs->cmm_icc_profile_data->data_cs == gsCMYK) ||
        (pcs->cmm_icc_profile_data->data_cs == gsGRAY && gray_to_k));

    if_debug4m(gs_debug_flag_overprint, pgs->memory,
        "[overprint] gx_set_overprint_ICC. cs_ok = %d is_fill_color = %d overprint = %d stroke_overprint = %d \n",
        cs_ok, pgs->is_fill_color, pgs->overprint, pgs->stroke_overprint);

    if (cs_ok)
        return gx_set_overprint_cmyk(pcs, pgs);

    /* In this case, we still need to maintain any spot
       colorant channels.  Per Table 7.14. */
    if (dev_proc(dev, dev_spec_op)(dev, gxdso_supports_devn, NULL, 0))
        return gx_set_spot_only_overprint(pgs);

    return gx_set_no_overprint(pgs);
}

int
gx_default_get_profile(const gx_device *dev, cmm_dev_profile_t **profile)
{
    *profile = dev->icc_struct;
    return 0;
}

/* Adjust the color model of the device to match that of the profile. Used by
   vector based devices and the tiff scaled devices. Only valid for bit depths
   of 8n/component. Note the caller likely will need to update its procs */
int
gx_change_color_model(gx_device *dev, int num_comps, int bit_depth)
{
    int k;

    if (!((num_comps == 1) || (num_comps == 3) || (num_comps == 4)))
        return_error(gs_error_unknownerror);

    dev->color_info.max_components = num_comps;
    dev->color_info.num_components = num_comps;
    dev->color_info.depth = num_comps * bit_depth;

    if (num_comps == 4) {
        dev->color_info.polarity = GX_CINFO_POLARITY_SUBTRACTIVE;
    } else {
        dev->color_info.polarity = GX_CINFO_POLARITY_ADDITIVE;
    }

    for (k = 0; k < num_comps; k++) {
        dev->color_info.comp_shift[k] = bit_depth * (3 - k);
        dev->color_info.comp_bits[k] = bit_depth;
        dev->color_info.comp_mask[k] =
            ((gx_color_index)255) << dev->color_info.comp_shift[k];
    }

    return 0;
}
