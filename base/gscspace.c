/* Copyright (C) 2001-2012 Artifex Software, Inc.
   All Rights Reserved.

   This software is provided AS-IS with no warranty, either express or
   implied.

   This software is distributed under license and may not be copied,
   modified or distributed except as expressly authorized under the terms
   of the license contained in the file LICENSE in this distribution.

   Refer to licensing information at http://www.artifex.com or contact
   Artifex Software, Inc.,  7 Mt. Lassen Drive - Suite A-134, San Rafael,
   CA  94903, U.S.A., +1(415)492-9861, for further information.
*/


/* Color space operators and support */
#include "memory_.h"
#include "gx.h"
#include "gserrors.h"
#include "gsstruct.h"
#include "gsccolor.h"
#include "gsutil.h"		/* for gs_next_ids */
#include "gxcmap.h"
#include "gxcspace.h"
#include "gxistate.h"
#include "gsovrc.h"
#include "gsstate.h"
#include "gsdevice.h"
#include "gxdevcli.h"
#include "gzstate.h"
#include "stream.h"
#include "gsnamecl.h"  /* Custom color call back define */
#include "gsicc.h"
#include "gsicc_manage.h"

static cs_proc_install_cspace(gx_install_DeviceGray);
static cs_proc_install_cspace(gx_install_DeviceRGB);
static cs_proc_install_cspace(gx_install_DeviceCMYK);
/*
 * Define the standard color space types.  We include DeviceCMYK in the base
 * build because it's too awkward to omit it, but we don't provide any of
 * the PostScript operator procedures (setcmykcolor, etc.) for dealing with
 * it.
 */
static const gs_color_space_type gs_color_space_type_DeviceGray = {
    gs_color_space_index_DeviceGray, true, true,
    &st_base_color_space, gx_num_components_1,
    gx_init_paint_1, gx_restrict01_paint_1,
    gx_same_concrete_space,
    gx_concretize_DeviceGray, gx_remap_concrete_DGray,
    gx_remap_DeviceGray, gx_install_DeviceGray,
    gx_spot_colors_set_overprint,
    NULL, gx_no_adjust_color_count,
    gx_serialize_cspace_type,
    gx_cspace_is_linear_default, gx_polarity_additive
};
static const gs_color_space_type gs_color_space_type_DeviceRGB = {
    gs_color_space_index_DeviceRGB, true, true,
    &st_base_color_space, gx_num_components_3,
    gx_init_paint_3, gx_restrict01_paint_3,
    gx_same_concrete_space,
    gx_concretize_DeviceRGB, gx_remap_concrete_DRGB,
    gx_remap_DeviceRGB, gx_install_DeviceRGB,
    gx_spot_colors_set_overprint,
    NULL, gx_no_adjust_color_count,
    gx_serialize_cspace_type,
    gx_cspace_is_linear_default, gx_polarity_additive
};

static cs_proc_set_overprint(gx_set_overprint_DeviceCMYK);

static const gs_color_space_type gs_color_space_type_DeviceCMYK = {
    gs_color_space_index_DeviceCMYK, true, true,
    &st_base_color_space, gx_num_components_4,
    gx_init_paint_4, gx_restrict01_paint_4,
    gx_same_concrete_space,
    gx_concretize_DeviceCMYK, gx_remap_concrete_DCMYK,
    gx_remap_DeviceCMYK, gx_install_DeviceCMYK,
    gx_set_overprint_DeviceCMYK,
    NULL, gx_no_adjust_color_count,
    gx_serialize_cspace_type,
    gx_cspace_is_linear_default, gx_polarity_subtractive
};

/* Structure descriptors */
public_st_color_space();
public_st_base_color_space();

/* ------ Create/copy/destroy ------ */

static void
gs_cspace_final(const gs_memory_t *cmem, void *vptr)
{
    gs_color_space *pcs = (gs_color_space *)vptr;
    (void)cmem; /* unused */

    if (pcs->type->final)
        pcs->type->final(pcs);
    if_debug2m('c', cmem, "[c]cspace final %p %d\n", pcs, (int)pcs->id);
    rc_decrement_only_cs(pcs->base_space, "gs_cspace_final");

    /* No need to decrement the ICC profile data.  It is handled
       by the finalize of the ICC space which is called above using
       pcs->type->final(pcs);  */

}

static gs_color_space *
gs_cspace_alloc_with_id(gs_memory_t *mem, ulong id,
                   const gs_color_space_type *pcstype)
{
    gs_color_space *pcs;

    rc_alloc_struct_1(pcs, gs_color_space, &st_color_space, mem, return NULL,
                      "gs_cspace_alloc_with_id");
    if_debug3m('c', mem, "[c]cspace alloc %p %s %d\n",
               pcs, pcstype->stype->sname, pcstype->index);
    pcs->type = pcstype;
    pcs->id = id;
    pcs->base_space = NULL;
    pcs->pclient_color_space_data = NULL;
    pcs->cmm_icc_profile_data = NULL;
    pcs->icc_equivalent = NULL;
    return pcs;
}

static cs_proc_install_cspace(gx_install_DeviceGray);
static cs_proc_install_cspace(gx_install_DeviceRGB);
static cs_proc_install_cspace(gx_install_DeviceCMYK);

/*
 * Generic allocation function for colorspace implementations. Return
 * NULL on allocation failure.
 */
gs_color_space *
gs_cspace_alloc(gs_memory_t *mem, const gs_color_space_type *pcstype)
{
    return gs_cspace_alloc_with_id(mem, gs_next_ids(mem, 1), pcstype);
}

/* Constructors for simple device color spaces. */

gs_color_space *
gs_cspace_new_DeviceGray(gs_memory_t *mem)
{
    return gs_cspace_alloc_with_id(mem, cs_DeviceGray_id,
                                   &gs_color_space_type_DeviceGray);
}

gs_color_space *
gs_cspace_new_DeviceRGB(gs_memory_t *mem)
{
    return gs_cspace_alloc_with_id(mem, cs_DeviceRGB_id,
                                   &gs_color_space_type_DeviceRGB);
}
gs_color_space *
gs_cspace_new_DeviceCMYK(gs_memory_t *mem)
{
    return gs_cspace_alloc_with_id(mem, cs_DeviceCMYK_id,
                                   &gs_color_space_type_DeviceCMYK);
}

/* For use in initializing ICC color spaces for XPS */
gs_color_space *
gs_cspace_new_ICC(gs_memory_t *pmem, gs_state * pgs, int components)
{
    gsicc_manager_t *icc_manage = pgs->icc_manager;
    int code = 0;
    gs_color_space *pcspace = gs_cspace_alloc(pmem, &gs_color_space_type_ICC);

    switch (components) {
        case -1: /* alpha case */
            if (icc_manage->smask_profiles == NULL) {
                code = gsicc_initialize_iccsmask(icc_manage);
            }
            if (code == 0) {
                pcspace->cmm_icc_profile_data =
                    icc_manage->smask_profiles->smask_gray;
            } else {
                pcspace->cmm_icc_profile_data = icc_manage->default_gray;
            }
            break;
        case -3: /* alpha case.  needs linear RGB */
            if (icc_manage->smask_profiles == NULL) {
                code = gsicc_initialize_iccsmask(icc_manage);
            }
            if (code == 0) {
                pcspace->cmm_icc_profile_data =
                    icc_manage->smask_profiles->smask_rgb;
            } else {
                pcspace->cmm_icc_profile_data = icc_manage->default_rgb;
            }
            break;
        case 1: pcspace->cmm_icc_profile_data = icc_manage->default_gray; break;
        case 3: pcspace->cmm_icc_profile_data = icc_manage->default_rgb; break;
        case 4: pcspace->cmm_icc_profile_data = icc_manage->default_cmyk; break;
        default: rc_decrement(pcspace,"gs_cspace_new_ICC"); return NULL;
    }
    rc_increment(pcspace->cmm_icc_profile_data);
    return pcspace;
}

/* ------ Accessors ------ */

/* Get the index of a color space. */
gs_color_space_index
gs_color_space_get_index(const gs_color_space * pcs)
{
    return pcs->type->index;
}

/* See if the space is CIE based */
bool gs_color_space_is_CIE(const gs_color_space * pcs)
{
    switch(gs_color_space_get_index(pcs)){
        case gs_color_space_index_CIEDEFG:
        case gs_color_space_index_CIEDEF:
        case gs_color_space_index_CIEABC:
        case gs_color_space_index_CIEA:
        case gs_color_space_index_ICC:
            return true;
        break;
        default:
            return false;
    }
}

/* See if the space is Postscript CIE based */
bool gs_color_space_is_PSCIE(const gs_color_space * pcs)
{
    switch(gs_color_space_get_index(pcs)){
        case gs_color_space_index_CIEDEFG:
        case gs_color_space_index_CIEDEF:
        case gs_color_space_index_CIEABC:
        case gs_color_space_index_CIEA:
            return true;
        break;
        default:
            return false;
}
}

/* See if the space is ICC based */
bool gs_color_space_is_ICC(const gs_color_space * pcs)
{
    return(gs_color_space_get_index(pcs) == gs_color_space_index_ICC);
}

/* Get the number of components in a color space. */
int
gs_color_space_num_components(const gs_color_space * pcs)
{
    return cs_num_components(pcs);
}

/* Restrict a color to its legal range. */
void
gs_color_space_restrict_color(gs_client_color *pcc, const gs_color_space *pcs)
{
    cs_restrict_color(pcc, pcs);
}

/* Install a DeviceGray color space. */
static int
gx_install_DeviceGray(gs_color_space * pcs, gs_state * pgs)
{
    /* If we already have profile data installed, nothing to do here. */
    if (pcs->cmm_icc_profile_data != NULL)
        return 0;

    /* If we haven't initialised the iccmanager, do it now. */
    if (pgs->icc_manager->default_gray == NULL)
        gsicc_init_iccmanager(pgs);

    /* pcs takes a reference to the default_gray profile data */
    pcs->cmm_icc_profile_data = pgs->icc_manager->default_gray;
    rc_increment(pgs->icc_manager->default_gray);
    pcs->type = &gs_color_space_type_ICC;
    return 0;
}

int
gx_num_components_1(const gs_color_space * pcs)
{
    return 1;
}
int
gx_num_components_3(const gs_color_space * pcs)
{
    return 3;
}
int
gx_num_components_4(const gs_color_space * pcs)
{
    return 4;
}

gx_color_polarity_t
gx_polarity_subtractive(const gs_color_space * pcs)
{
    return GX_CINFO_POLARITY_SUBTRACTIVE;
}

gx_color_polarity_t
gx_polarity_additive(const gs_color_space * pcs)
{
    return GX_CINFO_POLARITY_ADDITIVE;
}

gx_color_polarity_t
gx_polarity_unknown(const gs_color_space * pcs)
{
    return GX_CINFO_POLARITY_UNKNOWN;
}

/*
 * For color spaces that have a base or alternative color space, return that
 * color space. Otherwise return null.
 */
const gs_color_space *
gs_cspace_base_space(const gs_color_space * pcspace)
{
    return pcspace->base_space;
}

/* Abstract the reference counting for color spaces
   so that we can also increment the ICC profile
   if there is one associated with the color space */

void rc_increment_cs(gs_color_space *pcs)
{
    rc_increment(pcs);
}

void rc_decrement_cs(gs_color_space *pcs, const char *cname) {

    if (pcs) {
        rc_decrement(pcs, cname);
    }
}

void rc_decrement_only_cs(gs_color_space *pcs, const char *cname)
{
    if (pcs) {
        rc_decrement_only(pcs, cname);
    }
}

void cs_adjust_counts_icc(gs_state *pgs, int delta)
{
    gs_color_space *pcs = gs_currentcolorspace_inline(pgs);

    if (pcs) {
        cs_adjust_counts(pgs, delta);
    }
}

/* ------ Other implementation procedures ------ */

/* Null color space installation procedure. */
int
gx_no_install_cspace(gs_color_space * pcs, gs_state * pgs)
{
    return 0;
}

/* Install a DeviceRGB color space. */
static int
gx_install_DeviceRGB(gs_color_space * pcs, gs_state * pgs)
{
    /* If we already have profile_data, nothing to do here. */
    if (pcs->cmm_icc_profile_data != NULL)
        return 0;

    /* If the icc manager hasn't been set up yet, then set it up. */
    if (pgs->icc_manager->default_rgb == NULL)
        gsicc_init_iccmanager(pgs);

    /* pcs takes a reference to default_rgb */
    pcs->cmm_icc_profile_data = pgs->icc_manager->default_rgb;
    rc_increment(pcs->cmm_icc_profile_data);
    pcs->type = &gs_color_space_type_ICC;
    return 0;
}

/* Install a DeviceCMYK color space. */
static int
gx_install_DeviceCMYK(gs_color_space * pcs, gs_state * pgs)
{
    /* If we already have profile data, nothing to do here. */
    if (pcs->cmm_icc_profile_data != NULL)
        return 0;

    /* If the icc manager hasn't been set up yet, then set it up. */
    if (pgs->icc_manager->default_cmyk == NULL)
        gsicc_init_iccmanager(pgs);

    /* pcs takes a reference to default_cmyk */
    pcs->cmm_icc_profile_data = pgs->icc_manager->default_cmyk;
    rc_increment(pcs->cmm_icc_profile_data);
    pcs->type = &gs_color_space_type_ICC;
    return 0;
}

/*
 * Push an overprint compositor onto the current device indicating that,
 * at most, the spot color parameters are to be preserved.
 *
 * This routine should be used for all Device, CIEBased, and ICCBased
 * color spaces, except for DeviceCMKY. Also, it would not be used for
 * DeviceRGB if we have simulated overprint turned on.
 * These latter cases requires a
 * special verson that supports overprint mode.
 */
int
gx_spot_colors_set_overprint(const gs_color_space * pcs, gs_state * pgs)
{
    gs_imager_state *       pis = (gs_imager_state *)pgs;
    gs_overprint_params_t   params;

    if ((params.retain_any_comps = pis->overprint))
        params.retain_spot_comps = true;
    pgs->effective_overprint_mode = 0;
    params.k_value = 0;
    params.blendspot = false;
    return gs_state_update_overprint(pgs, &params);
}

/*
 * Push an overprint compositor onto the current device indicating that
 * incoming CMYK values should be blended to simulate overprinting.  This
 * allows us to get simulated overprinting of spot colors on standard CMYK
 * devices
 */
int
gx_simulated_set_overprint(const gs_color_space * pcs, gs_state * pgs)
{
    gs_imager_state *       pis = (gs_imager_state *)pgs;
    gs_overprint_params_t   params;

    if ((params.retain_any_comps = pis->overprint))
        params.retain_spot_comps = true;
    pgs->effective_overprint_mode = 0;
    params.k_value = 0;
    params.blendspot = true;
    return gs_state_update_overprint(pgs, &params);
}

static bool
check_single_comp(int comp, frac targ_val, int ncomps, const frac * pval)
{
    int     i;

    for (i = 0; i < ncomps; i++) {
        if ( (i != comp && pval[i] != frac_0)  ||
             (i == comp && pval[i] != targ_val)  )
            return false;
    }
    return true;
}

/*
 * Determine if the current color model is a "DeviceCMYK" color model, and
 * if so what are its process color components. This information is required
 * when PLRM defines special rules for CMYK devices. This includes:
 * 1. DeviceGray to CMYK color conversion
 * 2. when overprint is true and overprint mode is set to 1.
 *
 * A color model is considered a "DeviceCMYK" color model if it supports the
 * cyan, magenta, yellow, and black color components, and maps the DeviceCMYK
 * color model components directly to these color components. Note that this
 * does not require any particular component order, allows for additional
 * spot color components, and does admit DeviceN color spaces if they have
 * the requisite behavior.
 *
 * If the color model is a "DeviceCMYK" color model, return the set of
 * process color components; otherwise return 0.
 */
gx_color_index
check_cmyk_color_model_comps(gx_device * dev)
{
    gx_device_color_info *          pcinfo = &dev->color_info;
    int                             ncomps = pcinfo->num_components;
    int                             cyan_c, magenta_c, yellow_c, black_c;
    const gx_cm_color_map_procs *   pprocs;
    cm_map_proc_cmyk((*map_cmyk));
    frac                            frac_14 = frac_1 / 4;
    frac                            out[GX_DEVICE_COLOR_MAX_COMPONENTS];
    gx_color_index                  process_comps;

    /* check for the appropriate components */
    if ( ncomps < 4                                       ||
         (cyan_c = dev_proc(dev, get_color_comp_index)(
                       dev,
                       "Cyan",
                       sizeof("Cyan") - 1,
                       NO_COMP_NAME_TYPE )) < 0           ||
         cyan_c == GX_DEVICE_COLOR_MAX_COMPONENTS         ||
         (magenta_c = dev_proc(dev, get_color_comp_index)(
                          dev,
                          "Magenta",
                          sizeof("Magenta") - 1,
                          NO_COMP_NAME_TYPE )) < 0        ||
         magenta_c == GX_DEVICE_COLOR_MAX_COMPONENTS      ||
         (yellow_c = dev_proc(dev, get_color_comp_index)(
                        dev,
                        "Yellow",
                        sizeof("Yellow") - 1,
                        NO_COMP_NAME_TYPE )) < 0               ||
         yellow_c == GX_DEVICE_COLOR_MAX_COMPONENTS       ||
         (black_c = dev_proc(dev, get_color_comp_index)(
                        dev,
                        "Black",
                        sizeof("Black") - 1,
                        NO_COMP_NAME_TYPE )) < 0                         ||
         black_c == GX_DEVICE_COLOR_MAX_COMPONENTS          )
        return 0;

    /* check the mapping */
    pprocs = get_color_mapping_procs_subclass(dev);

    if ( pprocs == 0 ||
         (map_cmyk = pprocs->map_cmyk) == 0                            )
        return 0;

    map_cmyk_subclass(pprocs, dev, frac_14, frac_0, frac_0, frac_0, out);
    if (!check_single_comp(cyan_c, frac_14, ncomps, out))
        return 0;
    map_cmyk_subclass(pprocs, dev, frac_0, frac_14, frac_0, frac_0, out);
    if (!check_single_comp(magenta_c, frac_14, ncomps, out))
        return 0;
    map_cmyk_subclass(pprocs, dev, frac_0, frac_0, frac_14, frac_0, out);
    if (!check_single_comp(yellow_c, frac_14, ncomps, out))
        return false;
    map_cmyk_subclass(pprocs, dev, frac_0, frac_0, frac_0, frac_14, out);
    if (!check_single_comp(black_c, frac_14, ncomps, out))
        return 0;

    process_comps =  ((gx_color_index)1 << cyan_c)
                   | ((gx_color_index)1 << magenta_c)
                   | ((gx_color_index)1 << yellow_c)
                   | ((gx_color_index)1 << black_c);
    pcinfo->opmode = GX_CINFO_OPMODE;
    pcinfo->process_comps = process_comps;
    pcinfo->black_component = black_c;
    return process_comps;
}

/* This is used in the RGB simulation overprint case */

gx_color_index
check_rgb_color_model_comps(gx_device * dev)
{
    gx_device_color_info *          pcinfo = &dev->color_info;
    int                             ncomps = pcinfo->num_components;
    int                             red_c, green_c, blue_c;
    const gx_cm_color_map_procs *   pprocs;
    cm_map_proc_rgb((*map_rgb));
    frac                            frac_14 = frac_1 / 4;
    frac                            out[GX_DEVICE_COLOR_MAX_COMPONENTS];
    gx_color_index                  process_comps;

    /* check for the appropriate components */
    if ( ncomps < 3                                       ||
         (red_c = dev_proc(dev, get_color_comp_index)(
                       dev,
                       "Red",
                       sizeof("Red") - 1,
                       NO_COMP_NAME_TYPE )) < 0           ||
         red_c == GX_DEVICE_COLOR_MAX_COMPONENTS         ||
         (green_c = dev_proc(dev, get_color_comp_index)(
                          dev,
                          "Green",
                          sizeof("Green") - 1,
                          NO_COMP_NAME_TYPE )) < 0        ||
         green_c == GX_DEVICE_COLOR_MAX_COMPONENTS      ||
         (blue_c = dev_proc(dev, get_color_comp_index)(
                        dev,
                        "Blue",
                        sizeof("Blue") - 1,
                        NO_COMP_NAME_TYPE )) < 0               ||
         blue_c == GX_DEVICE_COLOR_MAX_COMPONENTS        )
        return 0;

    /* check the mapping */
    pprocs = get_color_mapping_procs_subclass(dev);
    if ( pprocs == 0 ||
         (map_rgb = pprocs->map_rgb) == 0                            )
        return 0;

    map_rgb_subclass(pprocs, dev, NULL, frac_14, frac_0, frac_0, out);
    if (!check_single_comp(red_c, frac_14, ncomps, out))
        return 0;
    map_rgb_subclass(pprocs, dev, NULL, frac_0, frac_14, frac_0, out);
    if (!check_single_comp(green_c, frac_14, ncomps, out))
        return 0;
    map_rgb_subclass(pprocs, dev, NULL, frac_0, frac_0, frac_14, out);
    if (!check_single_comp(blue_c, frac_14, ncomps, out))
        return 0;

    process_comps =  ((gx_color_index)1 << red_c)
                   | ((gx_color_index)1 << green_c)
                   | ((gx_color_index)1 << blue_c);
    pcinfo->opmode = GC_CINFO_OPMODE_RGB_SET;
    pcinfo->process_comps = process_comps;
    return process_comps;
}

/*
 * This set_overprint method is unique. If overprint is true, overprint
 * mode is set to 1, the process color model has DeviceCMYK behavior (see
 * the comment ahead of gx_is_cmyk_color_model above), and the device
 * color is set, the device color needs to be considered in setting up
 * the set of drawn components.
 */
static int
gx_set_overprint_DeviceCMYK(const gs_color_space * pcs, gs_state * pgs)
{
    gx_device *             dev = pgs->device;
    gx_device_color_info *  pcinfo = (dev == 0 ? 0 : &dev->color_info);

    /* check if we require special handling */
    if ( !pgs->overprint                      ||
         pgs->overprint_mode != 1             ||
         pcinfo == 0                          ||
         pcinfo->opmode == GX_CINFO_OPMODE_NOT  )
        return gx_spot_colors_set_overprint(pcs, pgs);
    /* Share code with CMYK ICC case */
    return gx_set_overprint_cmyk(pcs, pgs);
}

/* A few comments about ICC profiles and overprint simulation.  In order
   to do proper overprint simulation, the source ICC profile and the
   destination ICC profile must be the same.  If they are not, then
   we end up mapping the source CMYK data to a different CMYK value.  In
   this case, the non-zero components, which with overprint mode = 1 specify
   which are to be overprinted will not be correct to produce the proper
   overprint simulation.  This is seen with AR when doing output preview,
   overprint simulation enabled of the file overprint_icc.pdf (see our
   test files) which has SWOP ICC based CMYK fills.  In AR, if we use a
   simluation ICC profile that is different than the source profile,
   overprinting is no longer previewed. We follow the same logic here.
   If the source and destination ICC profiles do not match, then there is
   effectively no overprinting enabled.  This is bug 692433 */
int gx_set_overprint_cmyk(const gs_color_space * pcs, gs_state * pgs)
{
    gx_device *             dev = pgs->device;
    gx_device_color_info *  pcinfo = (dev == 0 ? 0 : &dev->color_info);
    gx_color_index          drawn_comps = 0;
    gs_overprint_params_t   params;
    gx_device_color        *pdc;
    cmm_dev_profile_t      *dev_profile;
    cmm_profile_t          *output_profile = 0;
    int                     code;
    bool                    profile_ok = false;
    gsicc_rendering_param_t        render_cond;

    if (dev) {
        code = dev_proc(dev, get_profile)(dev, &dev_profile);
        if (code < 0)
            return code;

        gsicc_extract_profile(dev->graphics_type_tag, dev_profile, &(output_profile),
                              &render_cond);

        /* check if color model behavior must be determined */
        if (pcinfo->opmode == GX_CINFO_OPMODE_UNKNOWN)
            drawn_comps = check_cmyk_color_model_comps(dev);
        else
            drawn_comps = pcinfo->process_comps;
    }
    if (drawn_comps == 0)
        return gx_spot_colors_set_overprint(pcs, pgs);

    /* correct for any zero'ed color components.  But only if profiles
       match */
    if (pcs->cmm_icc_profile_data != NULL && output_profile != NULL) {
        if (output_profile->hashcode ==
            pcs->cmm_icc_profile_data->hashcode) {
            profile_ok = true;
        }
    }

    pgs->effective_overprint_mode = 1;
    pdc = gs_currentdevicecolor_inline(pgs);
    if (color_is_set(pdc) && profile_ok) {
        gx_color_index  nz_comps, one, temp;
        int             code;
        int             num_colorant[4], k;
        bool            colorant_ok;

        dev_color_proc_get_nonzero_comps((*procp));

        procp = pdc->type->get_nonzero_comps;
        if (pdc->ccolor_valid) {
            /* If we have the source colors, then use those in making the
               decision as to which ones are non-zero.  Then we avoid
               accidently looking at small values that get quantized to zero
               Note that to get here in the code, the source color data color
               space has to be CMYK. Trick is that we do need to worry about
               the colorant order on the target device */
            num_colorant[0] = (dev_proc(dev, get_color_comp_index))\
                             (dev, "Cyan", strlen("Cyan"), NO_COMP_NAME_TYPE);
            num_colorant[1] = (dev_proc(dev, get_color_comp_index))\
                             (dev, "Magenta", strlen("Magenta"), NO_COMP_NAME_TYPE);
            num_colorant[2] = (dev_proc(dev, get_color_comp_index))\
                             (dev, "Yellow", strlen("Yellow"), NO_COMP_NAME_TYPE);
            num_colorant[3] = (dev_proc(dev, get_color_comp_index))\
                             (dev, "Black", strlen("Black"), NO_COMP_NAME_TYPE);
            nz_comps = 0;
            one = 1;
            colorant_ok = true;
            for (k = 0; k < 4; k++) {
                if (pdc->ccolor.paint.values[k] != 0) {
                    if (num_colorant[k] == -1) {
                        colorant_ok = false;
                    } else {
                        temp = one << num_colorant[k];
                        nz_comps = nz_comps | temp;
                    }
                }
            }
            /* For some reason we don't have one of the standard colorants */
            if (!colorant_ok) {
                if ((code = procp(pdc, dev, &nz_comps)) < 0)
                    return code;
            }
        } else {
            if ((code = procp(pdc, dev, &nz_comps)) < 0)
                return code;
        }
        drawn_comps &= nz_comps;
    }
    params.retain_any_comps = true;
    params.retain_spot_comps = false;
    params.drawn_comps = drawn_comps;
    params.k_value = 0;
    params.blendspot = false;
    return gs_state_update_overprint(pgs, &params);
}

/* This is used for the case where we have an RGB based device, but we want
   to simulate CMY overprinting.  Color management is pretty much thrown out
   the window when doing this. */

int gx_set_overprint_rgb(const gs_color_space * pcs, gs_state * pgs)
{
    gx_device *             dev = pgs->device;
    gx_device_color_info *  pcinfo = (dev == 0 ? 0 : &dev->color_info);
    gx_color_index          drawn_comps = 0;
    gs_overprint_params_t   params;
    gx_device_color        *pdc;

    /* check if color model behavior must be determined.  This is why we
       need the GX_CINFO_OPMODE_RGB and GX_CINFO_OPMODE_RGB_SET.
       We only need to do this once */
    if (dev) {
        if (pcinfo->opmode == GX_CINFO_OPMODE_RGB)
            drawn_comps = check_rgb_color_model_comps(dev);
        else
            drawn_comps = pcinfo->process_comps;
    }
    if (drawn_comps == 0)
        return gx_spot_colors_set_overprint(pcs, pgs);

    /* correct for any zero'ed color components.  Note that matching of
       ICC profiles as a condition is not possible here, since the source
       will be CMYK and the destination RGB */
    pgs->effective_overprint_mode = 1;
    pdc = gs_currentdevicecolor_inline(pgs);
    params.k_value = 0;
    params.blendspot = false;
    if (color_is_set(pdc)) {
        gx_color_index  nz_comps, one, temp;
        int             code;
        int             num_colorant[3], k;
        bool            colorant_ok;

        dev_color_proc_get_nonzero_comps((*procp));

        procp = pdc->type->get_nonzero_comps;
        if (pdc->ccolor_valid) {
            /* If we have the source colors, then use those in making the
               decision as to which ones are non-zero.  Then we avoid
               accidently looking at small values that get quantized to zero
               Note that to get here in the code, the source color data color
               space has to be CMYK. Trick is that we do need to worry about
               the RGB colorant order on the target device */
            num_colorant[0] = (dev_proc(dev, get_color_comp_index))\
                             (dev, "Red", strlen("Red"), NO_COMP_NAME_TYPE);
            num_colorant[1] = (dev_proc(dev, get_color_comp_index))\
                             (dev, "Green", strlen("Green"), NO_COMP_NAME_TYPE);
            num_colorant[2] = (dev_proc(dev, get_color_comp_index))\
                             (dev, "Blue", strlen("Blue"), NO_COMP_NAME_TYPE);
            nz_comps = 0;
            one = 1;
            colorant_ok = true;
            for (k = 0; k < 3; k++) {
                if (pdc->ccolor.paint.values[k] != 0) {
                    if (num_colorant[k] == -1) {
                        colorant_ok = false;
                    } else {
                        temp = one << num_colorant[k];
                        nz_comps = nz_comps | temp;
                    }
                }
            }
            /* Check for the case where we have a K component.  In this case
               we need to fudge things a bit.  And we will end up needing
               to do some special stuff in the overprint compositor's rect
               fill to reduce the destination RGB values of the ones
               that we are not blowing away with the source values.  Those
               that have the source value will have already been reduced */
            params.k_value = (unsigned short) (pdc->ccolor.paint.values[3] * 256);
            /* For some reason we don't have one of the standard colorants */
            if (!colorant_ok) {
                if ((code = procp(pdc, dev, &nz_comps)) < 0)
                    return code;
            }
        } else {
            if ((code = procp(pdc, dev, &nz_comps)) < 0)
                return code;
        }
        drawn_comps &= nz_comps;
    }
    params.retain_any_comps = true;
    params.retain_spot_comps = false;
    params.drawn_comps = drawn_comps;
    return gs_state_update_overprint(pgs, &params);
}

/* A stub for a color mapping linearity check, when it is inapplicable. */
int
gx_cspace_no_linear(const gs_color_space *cs, const gs_imager_state * pis,
                gx_device * dev,
                const gs_client_color *c0, const gs_client_color *c1,
                const gs_client_color *c2, const gs_client_color *c3,
                float smoothness, gsicc_link_t *icclink)
{
    return_error(gs_error_rangecheck);
}

static inline int
cc2dc(const gs_color_space *cs, const gs_imager_state * pis, gx_device *dev,
            gx_device_color *dc, const gs_client_color *cc)
{
    return cs->type->remap_color(cc, cs, dc, pis, dev, gs_color_select_texture);
}

static inline void
interpolate_cc(gs_client_color *c,
        const gs_client_color *c0, const gs_client_color *c1, double t, int n)
{
    int i;

    for (i = 0; i < n; i++)
        c->paint.values[i] = c0->paint.values[i] * t + c1->paint.values[i] * (1 - t);
}

static inline bool
is_dc_nearly_linear(const gx_device *dev, const gx_device_color *c,
        const gx_device_color *c0, const gx_device_color *c1,
        double t, int n, float smoothness)
{
    int i;

    if (c0->type == &gx_dc_type_data_pure) {
        gx_color_index pure0 = c0->colors.pure;
        gx_color_index pure1 = c1->colors.pure;
        gx_color_index pure = c->colors.pure;

        for (i = 0; i < n; i++) {
            int shift = dev->color_info.comp_shift[i];
            int mask = (1 << dev->color_info.comp_bits[i]) - 1;
            int max_color = (i == dev->color_info.gray_index ? dev->color_info.max_gray
                                                             : dev->color_info.max_color);
            float max_diff = max(1, max_color * smoothness);
            int b0 = (pure0 >> shift) & mask, b1 = (pure1 >> shift) & mask;
            int b = (pure >> shift) & mask;
            double bb = b0 * t + b1 * (1 - t);

            if (any_abs(b - bb) > max_diff)
                return false;
        }
        return true;
    } else if (c0->type == &gx_dc_type_data_devn) {
        for (i = 0; i < n; i++) {
            int max_color = (i == dev->color_info.gray_index ? dev->color_info.max_gray
                : dev->color_info.max_color);
            double max_diff = max(1, max_color * smoothness);
            /* Color values are 16 bit.  We are basing the smoothness on the
               device bit depth.  So make sure to adjust the above max diff
               based upon our device bit depth */
            double ratio = (double)max_color / (double)gx_max_color_value;
            double b0 = (c0->colors.devn.values[i]) * ratio;
            double b1 = (c1->colors.devn.values[i]) * ratio;
            double b = (c->colors.devn.values[i]) * ratio;
            double bb = b0 * t + b1 * (1 - t);
            if (any_abs(b - bb) > max_diff)
                return false;
        }
        return true;
    } else {
        /* Halftones must not paint with fill_linear_color_*. */
        return false;
    }
}

/* Default color mapping linearity check, a 2-points case. */
static int
gx_cspace_is_linear_in_line(const gs_color_space *cs, const gs_imager_state * pis,
                gx_device *dev,
                const gs_client_color *c0, const gs_client_color *c1,
                float smoothness)
{
    gs_client_color c01a, c01b;
    gx_device_color d[2], d01a, d01b;
    int n = cs->type->num_components(cs);
    int ndev = dev->color_info.num_components;
    int code;

    code = cc2dc(cs, pis, dev, &d[0], c0);
    if (code < 0)
        return code;
    code = cc2dc(cs, pis, dev, &d[1], c1);
    if (code < 0)
        return code;
    interpolate_cc(&c01a, c0, c1, 0.3, n);
    code = cc2dc(cs, pis, dev, &d01a, &c01a);
    if (code < 0)
        return code;
    if (!is_dc_nearly_linear(dev, &d01a, &d[0], &d[1], 0.3, ndev, smoothness))
        return 0;
    interpolate_cc(&c01b, c0, c1, 0.7, n);
    code = cc2dc(cs, pis, dev, &d01b, &c01b);
    if (code < 0)
        return code;
    if (!is_dc_nearly_linear(dev, &d01b, &d[0], &d[1], 0.7, ndev, smoothness))
        return 0;
    return 1;
}

/* Default color mapping linearity check, a triangle case. */
static int
gx_cspace_is_linear_in_triangle(const gs_color_space *cs, const gs_imager_state * pis,
                gx_device *dev,
                const gs_client_color *c0, const gs_client_color *c1,
                const gs_client_color *c2, float smoothness)
{
    /* We check 4 points - the median center, and middle points of 3 sides.
       Hopely this is enough for reasonable color spaces and color renderings.
       Note it gives 7 points for a quadrangle. */
    gs_client_color c01, c12, c20, c012;
    gx_device_color d[3], d01, d12, d20, d012;

    /* Note that the device and the client color space
       can have a different number of components */

    int n = cs->type->num_components(cs);
    int ndev = dev->color_info.num_components;

    int code;

    code = cc2dc(cs, pis, dev, &d[0], c0);
    if (code < 0)
        return code;
    code = cc2dc(cs, pis, dev, &d[1], c1);
    if (code < 0)
        return code;
    code = cc2dc(cs, pis, dev, &d[2], c2);
    if (code < 0)
        return code;

    interpolate_cc(&c01, c0, c1, 0.5, n);
    code = cc2dc(cs, pis, dev, &d01, &c01);
    if (code < 0)
        return code;
    if (!is_dc_nearly_linear(dev, &d01, &d[0], &d[1], 0.5, ndev, smoothness))
        return 0;

    interpolate_cc(&c012, c2, &c01, 2.0 / 3, n);
    code = cc2dc(cs, pis, dev, &d012, &c012);
    if (code < 0)
        return code;
    if (!is_dc_nearly_linear(dev, &d012, &d[2], &d01, 2.0 / 3, ndev, smoothness))
        return 0;

    interpolate_cc(&c12, c1, c2, 0.5, n);
    code = cc2dc(cs, pis, dev, &d12, &c12);
    if (code < 0)
        return code;
    if (!is_dc_nearly_linear(dev, &d12, &d[1], &d[2], 0.5, ndev, smoothness))
        return 0;

    interpolate_cc(&c20, c2, c0, 0.5, n);
    code = cc2dc(cs, pis, dev, &d20, &c20);
    if (code < 0)
        return code;
    if (!is_dc_nearly_linear(dev, &d20, &d[2], &d[0], 0.5, ndev, smoothness))
        return 0;
    return 1;
}

/* Default color mapping linearity check. */
int
gx_cspace_is_linear_default(const gs_color_space *cs, const gs_imager_state * pis,
                gx_device *dev,
                const gs_client_color *c0, const gs_client_color *c1,
                const gs_client_color *c2, const gs_client_color *c3,
                float smoothness, gsicc_link_t *icclink)
{
    /* Assuming 2 <= nc <= 4. We don't need other cases. */
    /* With nc == 4 assuming a convex plain quadrangle in the client color space. */
    int code;

    if (dev->color_info.separable_and_linear != GX_CINFO_SEP_LIN)
        return_error(gs_error_rangecheck);
    if (c2 == NULL)
        return gx_cspace_is_linear_in_line(cs, pis, dev, c0, c1, smoothness);
    code = gx_cspace_is_linear_in_triangle(cs, pis, dev, c0, c1, c2, smoothness);
    if (code <= 0)
        return code;
    if (c3 == NULL)
        return 1;
    return gx_cspace_is_linear_in_triangle(cs, pis, dev, c1, c2, c3, smoothness);
}

/* Serialization. */
int
gx_serialize_cspace_type(const gs_color_space * pcs, stream * s)
{
    const gs_color_space_type * type = pcs->type;
    uint n;
    return sputs(s, (const byte *)&type->index, sizeof(type->index), &n);
}

/* GC procedures */

static
ENUM_PTRS_BEGIN_PROC(color_space_enum_ptrs)
{
    EV_CONST gs_color_space *pcs = vptr;

    if (index == 0)
        return ENUM_OBJ(pcs->base_space);
    if (index == 1)
        return ENUM_OBJ(pcs->pclient_color_space_data);
    if (index == 2)
        return ENUM_OBJ(pcs->icc_equivalent);
    return ENUM_USING(*pcs->type->stype, vptr, size, index - 3);
    ENUM_PTRS_END_PROC
}
static
RELOC_PTRS_WITH(color_space_reloc_ptrs, gs_color_space *pcs)
{
    RELOC_VAR(pcs->base_space);
    RELOC_VAR(pcs->pclient_color_space_data);
    RELOC_VAR(pcs->icc_equivalent);
    RELOC_USING(*pcs->type->stype, vptr, size);
}
RELOC_PTRS_END
