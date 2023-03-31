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


/* Color space operators and support */
#include "memory_.h"
#include "gx.h"
#include "gserrors.h"
#include "gsstruct.h"
#include "gsccolor.h"
#include "gsutil.h"		/* for gs_next_ids */
#include "gxcmap.h"
#include "gxcspace.h"
#include "gxgstate.h"
#include "gsovrc.h"
#include "gsstate.h"
#include "gsdevice.h"
#include "gxdevcli.h"
#include "gzstate.h"
#include "stream.h"
#include "gsnamecl.h"  /* Custom color call back define */
#include "gsicc.h"
#include "gsicc_manage.h"
#include "string_.h"
#include "strmio.h"         /* needed for sfclose */
#include "gsicc_cache.h"    /* Needed for gsicc_get_icc_buff_hash */

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

/* Ghostscript object finalizers can be called many times and hence
 * must be idempotent. */
static void
gs_cspace_final(const gs_memory_t *cmem, void *vptr)
{
    gs_color_space *pcs = (gs_color_space *)vptr;
    (void)cmem; /* unused */

    if (pcs->interpreter_free_cspace_proc != NULL) {
        (*pcs->interpreter_free_cspace_proc) ((gs_memory_t *)cmem, pcs);
        pcs->interpreter_free_cspace_proc = NULL;
    }
    if (pcs->type->final)
        pcs->type->final(pcs);
    if_debug2m('c', cmem, "[c]cspace final "PRI_INTPTR" %d\n", (intptr_t)pcs, (int)pcs->id);
    rc_decrement_only_cs(pcs->base_space, "gs_cspace_final");
    pcs->base_space = NULL;
    if (gs_color_space_get_index(pcs) == gs_color_space_index_DeviceN) {
        if (pcs->params.device_n.devn_process_space != NULL) {
            rc_decrement_only_cs(pcs->params.device_n.devn_process_space, "gs_cspace_final");
            pcs->params.device_n.devn_process_space = NULL;
        }
    }
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
    if_debug3m('c', mem, "[c]cspace alloc "PRI_INTPTR" %s %d\n",
               (intptr_t)pcs, pcstype->stype->sname, pcstype->index);
    pcs->type = pcstype;
    pcs->id = id;
    pcs->base_space = NULL;
    pcs->pclient_color_space_data = NULL;
    pcs->interpreter_data = NULL;
    pcs->interpreter_free_cspace_proc = NULL;
    pcs->cmm_icc_profile_data = NULL;
    pcs->ICC_Alternate_space = gs_ICC_Alternate_None;
    pcs->icc_equivalent = NULL;
    pcs->params.device_n.devn_process_space = NULL;
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
gs_cspace_new_scrgb(gs_memory_t *pmem, gs_gstate * pgs)
{
    gs_color_space *pcspace = gs_cspace_alloc(pmem, &gs_color_space_type_ICC);
    cmm_profile_t *profile;
    stream *str;
    int code;

    if (pcspace == NULL)
        return pcspace;

    code = gsicc_open_search(SCRGB, strlen(SCRGB), pmem, pmem->gs_lib_ctx->profiledir,
        pmem->gs_lib_ctx->profiledir_len, &str);

    if (code < 0 || str == NULL) {
        rc_decrement(pcspace, "gs_cspace_new_scrgb");
        return NULL;
    }

    pcspace->cmm_icc_profile_data = gsicc_profile_new(str, pmem, SCRGB, strlen(SCRGB));
    code = sfclose(str);
    if (pcspace->cmm_icc_profile_data == NULL) {
        rc_decrement(pcspace, "gs_cspace_new_scrgb");
        return NULL;
    }

    /* Get the profile handle */
    pcspace->cmm_icc_profile_data->profile_handle =
        gsicc_get_profile_handle_buffer(pcspace->cmm_icc_profile_data->buffer,
            pcspace->cmm_icc_profile_data->buffer_size, pmem);
    if (!pcspace->cmm_icc_profile_data->profile_handle) {
        rc_decrement(pcspace, "gs_cspace_new_scrgb");
        return NULL;
    }
    profile = pcspace->cmm_icc_profile_data;

    /* Compute the hash code of the profile. Everything in the
    ICC manager will have it's hash code precomputed */
    gsicc_get_icc_buff_hash(profile->buffer, &(profile->hashcode),
        profile->buffer_size);
    profile->hash_is_valid = true;
    profile->num_comps =
        gscms_get_input_channel_count(profile->profile_handle, profile->memory);
    profile->num_comps_out =
        gscms_get_output_channel_count(profile->profile_handle, profile->memory);
    profile->data_cs =
        gscms_get_profile_data_space(profile->profile_handle, profile->memory);
    gsicc_set_icc_range(&profile);
    return pcspace;
}

gs_color_space *
gs_cspace_new_ICC(gs_memory_t *pmem, gs_gstate * pgs, int components)
{
    gsicc_manager_t *icc_manage = pgs->icc_manager;
    int code = 0;
    gs_color_space *pcspace = gs_cspace_alloc(pmem, &gs_color_space_type_ICC);

    if (pcspace == NULL)
        return pcspace;

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
    gsicc_adjust_profile_rc(pcspace->cmm_icc_profile_data, 1, "gs_cspace_new_ICC");
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
gx_install_DeviceGray(gs_color_space * pcs, gs_gstate * pgs)
{
    /* If we already have profile data installed, nothing to do here. */
    if (pcs->cmm_icc_profile_data != NULL)
        return 0;

    /* If we haven't initialised the iccmanager, do it now. */
    if (pgs->icc_manager->default_gray == NULL) {
        int code = gsicc_init_iccmanager(pgs);
        if (code < 0)
            return code;
    }

    /* pcs takes a reference to the default_gray profile data */
    pcs->cmm_icc_profile_data = pgs->icc_manager->default_gray;
    gsicc_adjust_profile_rc(pgs->icc_manager->default_gray, 1, "gx_install_DeviceGray");
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

const gs_color_space *
gs_cspace_devn_process_space(const gs_color_space * pcspace)
{
    return pcspace->params.device_n.devn_process_space;
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

void cs_adjust_counts_icc(gs_gstate *pgs, int delta)
{
    gs_color_space *pcs = gs_currentcolorspace_inline(pgs);

    if (pcs) {
        cs_adjust_color_count(pgs, delta);
        rc_adjust_const(pcs, delta, "cs_adjust_counts_icc");
    }
}

void cs_adjust_swappedcounts_icc(gs_gstate *pgs, int delta)
{
    gs_color_space *pcs = gs_swappedcolorspace_inline(pgs);

    if (pcs) {
        cs_adjust_swappedcolor_count(pgs, delta);
        rc_adjust_const(pcs, delta, "cs_adjust_swappedcounts_icc");
    }
}

/* ------ Other implementation procedures ------ */

/* Null color space installation procedure. */
int
gx_no_install_cspace(gs_color_space * pcs, gs_gstate * pgs)
{
    return 0;
}

/* Install a DeviceRGB color space. */
static int
gx_install_DeviceRGB(gs_color_space * pcs, gs_gstate * pgs)
{
    /* If we already have profile_data, nothing to do here. */
    if (pcs->cmm_icc_profile_data != NULL)
        return 0;

    /* If the icc manager hasn't been set up yet, then set it up. */
    if (pgs->icc_manager->default_rgb == NULL)
        gsicc_init_iccmanager(pgs);

    /* pcs takes a reference to default_rgb */
    pcs->cmm_icc_profile_data = pgs->icc_manager->default_rgb;
    gsicc_adjust_profile_rc(pcs->cmm_icc_profile_data, 1, "gx_install_DeviceRGB");
    pcs->type = &gs_color_space_type_ICC;
    return 0;
}

/* Install a DeviceCMYK color space. */
static int
gx_install_DeviceCMYK(gs_color_space * pcs, gs_gstate * pgs)
{
    /* If we already have profile data, nothing to do here. */
    if (pcs->cmm_icc_profile_data != NULL)
        return 0;

    /* If the icc manager hasn't been set up yet, then set it up. */
    if (pgs->icc_manager->default_cmyk == NULL)
        gsicc_init_iccmanager(pgs);

    /* pcs takes a reference to default_cmyk */
    pcs->cmm_icc_profile_data = pgs->icc_manager->default_cmyk;
    gsicc_adjust_profile_rc(pcs->cmm_icc_profile_data, 1, "gx_install_DeviceCMYK");
    pcs->type = &gs_color_space_type_ICC;
    return 0;
}

/*
 * Communicate to the overprint compositor that this particular
 * state overprint is not enabled.  This could be due to a
 * mismatched color space, or that overprint is false or the
 * device does not support it.
 */
int
gx_set_no_overprint(gs_gstate* pgs)
{
    gs_overprint_params_t   params = { 0 };

    params.retain_any_comps = false;
    params.op_state = OP_STATE_NONE;
    params.is_fill_color = pgs->is_fill_color;
    params.effective_opm = pgs->color[0].effective_opm = 0;

    return gs_gstate_update_overprint(pgs, &params);
}

/* Retain all the spot colorants and not the process
   colorants.  This occurs if we have a process color
   mismatch between the source and the destination but
   the output device supports spot colors */
int
gx_set_spot_only_overprint(gs_gstate* pgs)
{
    gs_overprint_params_t   params = { 0 };
    gx_device* dev = pgs->device;
    gx_color_index drawn_comps = dev == NULL ? 0 : gx_get_process_comps(dev);

    params.retain_any_comps = true;
    params.op_state = OP_STATE_NONE;
    params.is_fill_color = pgs->is_fill_color;
    params.effective_opm = pgs->color[0].effective_opm = 0;
    params.drawn_comps = drawn_comps;

    return gs_gstate_update_overprint(pgs, &params);
}

/*
 * Push an overprint compositor onto the current device indicating that,
 * at most, the spot color parameters are to be preserved.
 *
 * This routine should be used for all Device, CIEBased, and ICCBased
 * color spaces, except for DeviceCMKY.
 */
int
gx_spot_colors_set_overprint(const gs_color_space * pcs, gs_gstate * pgs)
{
    gs_overprint_params_t   params = {0};
    bool op = pgs->is_fill_color ? pgs->overprint : pgs->stroke_overprint;

    if (!op)
        params.retain_any_comps = false;
    else
        params.retain_any_comps = true;

    params.is_fill_color = pgs->is_fill_color;
    params.op_state = OP_STATE_NONE;

    /* Only DeviceCMYK case can have overprint mode set to true */
    params.effective_opm = pgs->color[0].effective_opm = 0;
    return gs_gstate_update_overprint(pgs, &params);
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
    uchar                           ncomps = pcinfo->num_components;
    int                             cyan_c, magenta_c, yellow_c, black_c;
    frac                            frac_14 = frac_1 / 4;
    frac                            out[GX_DEVICE_COLOR_MAX_COMPONENTS];
    gx_color_index                  process_comps;
    const gx_cm_color_map_procs    *cmprocs;
    const gx_device                *cmdev;


    if (pcinfo->num_components < 4                     ||
        pcinfo->polarity == GX_CINFO_POLARITY_ADDITIVE ||
        pcinfo->gray_index == GX_CINFO_COMP_NO_INDEX) {
        pcinfo->opmsupported = GX_CINFO_OPMSUPPORTED_NOT;
        return 0;
    }

    /* check for the appropriate components */
    if ( ncomps < 4                                       ||
         (cyan_c = dev_proc(dev, get_color_comp_index)(
                       dev,
                       "Cyan",
                       sizeof("Cyan") - 1,
                       NO_COMP_NAME_TYPE_OP)) < 0           ||
         cyan_c == GX_DEVICE_COLOR_MAX_COMPONENTS         ||
         (magenta_c = dev_proc(dev, get_color_comp_index)(
                          dev,
                          "Magenta",
                          sizeof("Magenta") - 1,
                          NO_COMP_NAME_TYPE_OP)) < 0        ||
         magenta_c == GX_DEVICE_COLOR_MAX_COMPONENTS      ||
         (yellow_c = dev_proc(dev, get_color_comp_index)(
                        dev,
                        "Yellow",
                        sizeof("Yellow") - 1,
                        NO_COMP_NAME_TYPE_OP)) < 0               ||
         yellow_c == GX_DEVICE_COLOR_MAX_COMPONENTS       ||
         (black_c = dev_proc(dev, get_color_comp_index)(
                        dev,
                        "Black",
                        sizeof("Black") - 1,
                        NO_COMP_NAME_TYPE_OP)) < 0                         ||
         black_c == GX_DEVICE_COLOR_MAX_COMPONENTS          )
        return 0;

    /* check the mapping */
    cmprocs = dev_proc(dev, get_color_mapping_procs)(dev, &cmdev);

    cmprocs->map_cmyk(cmdev, frac_14, frac_0, frac_0, frac_0, out);
    if (!check_single_comp(cyan_c, frac_14, ncomps, out)) {
        pcinfo->opmsupported = GX_CINFO_OPMSUPPORTED_NOT;
        return 0;
    }
    cmprocs->map_cmyk(cmdev, frac_0, frac_14, frac_0, frac_0, out);
    if (!check_single_comp(magenta_c, frac_14, ncomps, out)) {
        pcinfo->opmsupported = GX_CINFO_OPMSUPPORTED_NOT;
        return 0;
    }
    cmprocs->map_cmyk(cmdev, frac_0, frac_0, frac_14, frac_0, out);
    if (!check_single_comp(yellow_c, frac_14, ncomps, out)) {
        pcinfo->opmsupported = GX_CINFO_OPMSUPPORTED_NOT;
        return 0;
    }
    cmprocs->map_cmyk(cmdev, frac_0, frac_0, frac_0, frac_14, out);
    if (!check_single_comp(black_c, frac_14, ncomps, out)) {
        pcinfo->opmsupported = GX_CINFO_OPMSUPPORTED_NOT;
        return 0;
    }

    process_comps =  ((gx_color_index)1 << cyan_c)
                   | ((gx_color_index)1 << magenta_c)
                   | ((gx_color_index)1 << yellow_c)
                   | ((gx_color_index)1 << black_c);
    pcinfo->opmsupported = GX_CINFO_OPMSUPPORTED;
    pcinfo->process_comps = process_comps;
    pcinfo->black_component = black_c;
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
gx_set_overprint_DeviceCMYK(const gs_color_space * pcs, gs_gstate * pgs)
{
    gx_device *             dev = pgs->device;
    gx_device_color_info *  pcinfo = (dev == 0 ? 0 : &dev->color_info);

    /* check if we require special handling */
    if ( !pgs->overprint                      ||
         pgs->overprint_mode != 1             ||
         pcinfo == 0                          ||
         pcinfo->opmsupported == GX_CINFO_OPMSUPPORTED_NOT)
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
   simulation ICC profile that is different than the source profile,
   overprinting is no longer previewed. We follow the same logic here.
   If the source and destination ICC profiles do not match, then there is
   effectively no overprinting enabled.  This is bug 692433.  However,
   even with the mismatch, if the device supports spot colorants, those
   colors should be maintained. This is bug 702725. */
int gx_set_overprint_cmyk(const gs_color_space * pcs, gs_gstate * pgs)
{
    gx_device *             dev = pgs->device;
    gx_color_index          drawn_comps = 0;
    gs_overprint_params_t   params = { 0 };
    gx_device_color        *pdc;
    cmm_dev_profile_t      *dev_profile;
    cmm_profile_t          *output_profile = 0;
    int                     code;
    bool                    profile_ok = false;
    gsicc_rendering_param_t        render_cond;
    bool                    eop;

    if_debug0m(gs_debug_flag_overprint, pgs->memory,
        "[overprint] gx_set_overprint_cmyk\n");

    if (dev) {
        code = dev_proc(dev, get_profile)(dev, &dev_profile);
        if (code < 0)
            return code;

        gsicc_extract_profile(dev->graphics_type_tag, dev_profile, &(output_profile),
                              &render_cond);

        drawn_comps = gx_get_process_comps(dev);
    }

    if_debug1m(gs_debug_flag_overprint, pgs->memory,
        "[overprint] gx_set_overprint_cmyk. drawn_comps = 0x%x\n", (uint)drawn_comps);

    if (drawn_comps == 0)
        return gx_spot_colors_set_overprint(pcs, pgs);

    /* correct for any zero'ed color components.  But only if profiles
       match AND pgs->overprint_mode is true */
    if (pcs->cmm_icc_profile_data != NULL && output_profile != NULL) {
        if (gsicc_profiles_equal(output_profile, pcs->cmm_icc_profile_data)) {
            profile_ok = true;
        }
    }

    eop = gs_currentcolor_eopm(pgs);

    if_debug3m(gs_debug_flag_overprint, pgs->memory,
        "[overprint] gx_set_overprint_cmyk. is_fill_color = %d, pgs->color[0].effective_opm = %d pgs->color[1].effective_opm = %d\n",
        pgs->is_fill_color, pgs->color[0].effective_opm, pgs->color[1].effective_opm);

    if (profile_ok && eop) {
        gx_color_index  nz_comps, one, temp;
        int             code;
        int             num_colorant[4], k;
        bool            colorant_ok;
        dev_color_proc_get_nonzero_comps((*procp));

        if_debug0m(gs_debug_flag_overprint, pgs->memory,
            "[overprint] gx_set_overprint_cmyk. color_is_set, profile_ok and eop\n");

        code = gx_set_dev_color(pgs);
        if (code < 0)
            return code;
        pdc = gs_currentdevicecolor_inline(pgs);
        procp = pdc->type->get_nonzero_comps;
        if (pdc->ccolor_valid) {
            /* If we have the source colors, then use those in making the
               decision as to which ones are non-zero.  Then we avoid
               accidently looking at small values that get quantized to zero
               Note that to get here in the code, the source color data color
               space has to be CMYK. Trick is that we do need to worry about
               the colorant order on the target device */
            num_colorant[0] = (dev_proc(dev, get_color_comp_index))\
                             (dev, "Cyan", strlen("Cyan"), NO_COMP_NAME_TYPE_OP);
            num_colorant[1] = (dev_proc(dev, get_color_comp_index))\
                             (dev, "Magenta", strlen("Magenta"), NO_COMP_NAME_TYPE_OP);
            num_colorant[2] = (dev_proc(dev, get_color_comp_index))\
                             (dev, "Yellow", strlen("Yellow"), NO_COMP_NAME_TYPE_OP);
            num_colorant[3] = (dev_proc(dev, get_color_comp_index))\
                             (dev, "Black", strlen("Black"), NO_COMP_NAME_TYPE_OP);
            nz_comps = 0;
            one = 1;
            colorant_ok = true;
            for (k = 0; k < 4; k++) {
                /* Note: AR assumes the value is zero if it
                   is less than 0.5 out of 255 */
                if (pdc->ccolor.paint.values[k] > (0.5 / 255.0)) {
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
    params.is_fill_color = pgs->is_fill_color;
    params.retain_any_comps = true;
    params.drawn_comps = drawn_comps;
    params.op_state = OP_STATE_NONE;

    if_debug2m(gs_debug_flag_overprint, pgs->memory,
        "[overprint] gx_set_overprint_cmyk. retain_any_comps = %d, drawn_comps = 0x%x\n",
        params.retain_any_comps, (uint)(params.drawn_comps));

    /* We are in CMYK, the profiles match and overprint is true.  Set effective
       overprint mode to overprint mode but only if effective has not already
       been set to 0 */
    params.effective_opm = pgs->color[0].effective_opm =
        pgs->overprint_mode && gs_currentcolor_eopm(pgs);
    return gs_gstate_update_overprint(pgs, &params);
}

/* A stub for a color mapping linearity check, when it is inapplicable. */
int
gx_cspace_no_linear(const gs_color_space *cs, const gs_gstate * pgs,
                gx_device * dev,
                const gs_client_color *c0, const gs_client_color *c1,
                const gs_client_color *c2, const gs_client_color *c3,
                float smoothness, gsicc_link_t *icclink)
{
    return_error(gs_error_rangecheck);
}

static inline int
cc2dc(const gs_color_space *cs, const gs_gstate * pgs, gx_device *dev,
            gx_device_color *dc, const gs_client_color *cc)
{
    return cs->type->remap_color(cc, cs, dc, pgs, dev, gs_color_select_texture);
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
        double t, uchar n, float smoothness)
{
    uchar i;

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
gx_cspace_is_linear_in_line(const gs_color_space *cs, const gs_gstate * pgs,
                gx_device *dev,
                const gs_client_color *c0, const gs_client_color *c1,
                float smoothness)
{
    gs_client_color c01a, c01b;
    gx_device_color d[2], d01a, d01b;
    int n = cs->type->num_components(cs);
    uchar ndev = dev->color_info.num_components;
    int code;

    code = cc2dc(cs, pgs, dev, &d[0], c0);
    if (code < 0)
        return code;
    code = cc2dc(cs, pgs, dev, &d[1], c1);
    if (code < 0)
        return code;
    interpolate_cc(&c01a, c0, c1, 0.3, n);
    code = cc2dc(cs, pgs, dev, &d01a, &c01a);
    if (code < 0)
        return code;
    if (!is_dc_nearly_linear(dev, &d01a, &d[0], &d[1], 0.3, ndev, smoothness))
        return 0;
    interpolate_cc(&c01b, c0, c1, 0.7, n);
    code = cc2dc(cs, pgs, dev, &d01b, &c01b);
    if (code < 0)
        return code;
    if (!is_dc_nearly_linear(dev, &d01b, &d[0], &d[1], 0.7, ndev, smoothness))
        return 0;
    return 1;
}

/* Default color mapping linearity check, a triangle case. */
static int
gx_cspace_is_linear_in_triangle(const gs_color_space *cs, const gs_gstate * pgs,
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
    uchar ndev = dev->color_info.num_components;

    int code;

    code = cc2dc(cs, pgs, dev, &d[0], c0);
    if (code < 0)
        return code;
    code = cc2dc(cs, pgs, dev, &d[1], c1);
    if (code < 0)
        return code;
    code = cc2dc(cs, pgs, dev, &d[2], c2);
    if (code < 0)
        return code;

    interpolate_cc(&c01, c0, c1, 0.5, n);
    code = cc2dc(cs, pgs, dev, &d01, &c01);
    if (code < 0)
        return code;
    if (!is_dc_nearly_linear(dev, &d01, &d[0], &d[1], 0.5, ndev, smoothness))
        return 0;

    interpolate_cc(&c012, c2, &c01, 2.0 / 3, n);
    code = cc2dc(cs, pgs, dev, &d012, &c012);
    if (code < 0)
        return code;
    if (!is_dc_nearly_linear(dev, &d012, &d[2], &d01, 2.0 / 3, ndev, smoothness))
        return 0;

    interpolate_cc(&c12, c1, c2, 0.5, n);
    code = cc2dc(cs, pgs, dev, &d12, &c12);
    if (code < 0)
        return code;
    if (!is_dc_nearly_linear(dev, &d12, &d[1], &d[2], 0.5, ndev, smoothness))
        return 0;

    interpolate_cc(&c20, c2, c0, 0.5, n);
    code = cc2dc(cs, pgs, dev, &d20, &c20);
    if (code < 0)
        return code;
    if (!is_dc_nearly_linear(dev, &d20, &d[2], &d[0], 0.5, ndev, smoothness))
        return 0;
    return 1;
}

/* Default color mapping linearity check. */
int
gx_cspace_is_linear_default(const gs_color_space *cs, const gs_gstate * pgs,
                gx_device *dev,
                const gs_client_color *c0, const gs_client_color *c1,
                const gs_client_color *c2, const gs_client_color *c3,
                float smoothness, gsicc_link_t *icclink)
{
    /* Assuming 2 <= nc <= 4. We don't need other cases. */
    /* With nc == 4 assuming a convex plain quadrangle in the client color space. */
    int code;

    if (!colors_are_separable_and_linear(&dev->color_info))
        return_error(gs_error_rangecheck);
    if (c2 == NULL)
        return gx_cspace_is_linear_in_line(cs, pgs, dev, c0, c1, smoothness);
    code = gx_cspace_is_linear_in_triangle(cs, pgs, dev, c0, c1, c2, smoothness);
    if (code <= 0)
        return code;
    if (c3 == NULL)
        return 1;
    return gx_cspace_is_linear_in_triangle(cs, pgs, dev, c1, c2, c3, smoothness);
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
    if (index == 3) {
        if (gs_color_space_get_index(pcs) == gs_color_space_index_DeviceN)
            return ENUM_OBJ(pcs->params.device_n.devn_process_space);
        else
            return ENUM_OBJ(NULL);
    }

    return ENUM_USING(*pcs->type->stype, vptr, size, index - 4);
    ENUM_PTRS_END_PROC
}
static
RELOC_PTRS_WITH(color_space_reloc_ptrs, gs_color_space *pcs)
{
    RELOC_VAR(pcs->base_space);
    RELOC_VAR(pcs->pclient_color_space_data);
    RELOC_VAR(pcs->icc_equivalent);
    if (gs_color_space_get_index(pcs) == gs_color_space_index_DeviceN)
        RELOC_VAR(pcs->params.device_n.devn_process_space);
    RELOC_USING(*pcs->type->stype, vptr, size);
}
RELOC_PTRS_END
