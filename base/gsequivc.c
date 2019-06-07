/* Copyright (C) 2001-2019 Artifex Software, Inc.
   All Rights Reserved.

   This software is provided AS-IS with no warranty, either express or
   implied.

   This software is distributed under license and may not be copied,
   modified or distributed except as expressly authorized under the terms
   of the license contained in the file LICENSE in this distribution.

   Refer to licensing information at http://www.artifex.com or contact
   Artifex Software, Inc.,  1305 Grant Avenue - Suite 200, Novato,
   CA 94945, U.S.A., +1(415)492-9861, for further information.
*/


/* Routines for determining equivalent color for spot colors */

#include "math_.h"
#include "gdevprn.h"
#include "gsparam.h"
#include "gstypes.h"
#include "gxdcconv.h"
#include "gdevdevn.h"
#include "gsequivc.h"
#include "gzstate.h"
#include "gsstate.h"
#include "gscspace.h"
#include "gxcspace.h"
#include "gsicc_manage.h"
#include "gxdevsop.h"

/*
 * These routines are part of the logic for determining an equivalent
 * process color model color for a spot color.  The definition of a
 * Separation or DeviceN color space include a tint transform function.
 * This tint transform function is used if the specified spot or separation
 * colorant is not present.  We want to be able to display the spot colors
 * on our RGB or CMYK display.  We are using the tint transform function
 * to determine a CMYK equivalent to the spot color.  Current only CMYK
 * equivalent colors are supported.  This is because the CMYK is the only
 * standard subtractive color space.
 *
 * This process consists of the following steps:
 *
 * 1.  Whenever new spot colors are found, set status flags indicating
 * that we have one or more spot colors for which we need to determine an
 * equivalent color.  New spot colors can either be explicitly specified by
 * the SeparationColorNames device parameter or they may be detected by the
 * device's get_color_comp_index routine.
 *
 * 2.  Whenever a Separation or DeviceN color space is installed, the
 * update_spot_equivalent_colors device proc is called.  This allows the
 * device to check if the color space contains a spot color for which the
 * device does not know the equivalent CMYK color.  The routine
 * update_spot_equivalent_cmyk_colors is provided for this task (and the
 * next item).
 *
 * 3.  For each spot color for which an equivalent color is not known, we
 * do the following:
 *   a.  Create a copy of the color space and change the copy so that it
 *       uses its alternate colorspace.
 *   b.  Create a copy of the current gs_gstate and modify its color
 *       mapping (cmap) procs to use a special set of 'capture' procs.
 *   c.  Based upon the type of color space (Separation or DeviceN) create
 *       a 'color' which consists of the desired spot color set to 100% and
 *       and other components set to zero.
 *   d.  Call the remap_color routine using our modified color space and
 *       state structures.  Since we have forced the use of the alternate
 *       color space, we will eventually execute one of the 'capture' color
 *       space mapping procs.  This will give us either a gray, RGB, or
 *       CMYK color which is equivalent to the original spot color.  If the
 *       color is gray or RGB we convert it to CMYK.
 *   e.  Save the equivalent CMYK color in the device structure.
 *
 * 4.  When a page is to be displayed or a file created, the saved equivalent
 * color is used as desired.  It can be written into the output file.  It
 * may also be used to provide color values which can be combined with the
 * process color model components for a pixel, to correctly display spot
 * colors on a monitor.  (Note:  Overprinting effects with spot colors are
 * not correctly displayed on an RGB monitor if the device simply uses an RGB
 * process color model.  Instead it is necessary to use a subtractive process
 * color model and save both process color and spot color data and then
 * convert the overall result to RGB for display.)
 *
 * For this process to work properly, the following changes need to made to
 * the device.
 *
 * 1.  The device source module needs to include gsequivc.h for a definition
 *     of the relevant structures and routines.  An equivalent_cmyk_color_params
 *     structure needs to be added to the device's structure definition and
 *     it needs to be initialized.  For examples see the definition of the
 *     psd_device structure in src/gdevpsd.c and the definitions of the
 *     gs_psdrgb_device and gs_psdcmyk_devices devices in the same module.
 * 2.  Logic needs to be added to the device's get_color_comp_index and
 *     put_param routines to check if any separations have been added to the
 *     device.  For examples see code fragments in psd_get_color_comp_index and
 *     psd_put_params in src/gdevpsd.c.
 * 3.  The device needs to have its own version of the
 *     update_spot_equivalent_colors routine.  For examples see the definition
 *     of the device_procs macro and the psd_update_spot_equivalent_colors
 *     routine in src/gdevpsd.c.
 * 4.  The device then uses the saved equivalent color values when its output
 *     is created.  For example see the psd_write_header routine in
 *     src/gdevpsd.c.
 */

#define compare_color_names(name, name_size, str, str_size) \
    (name_size == str_size && \
        (strncmp((const char *)name, (const char *)str, name_size) == 0))

static void
update_Separation_spot_equivalent_cmyk_colors(gx_device * pdev,
                    const gs_gstate * pgs, const gs_color_space * pcs,
                    gs_devn_params * pdevn_params,
                    equivalent_cmyk_color_params * pparams)
{
    int i;

    /*
     * Check if the color space's separation name matches any of the
     * separations for which we need an equivalent CMYK color.
     */
    for (i = 0; i < pdevn_params->separations.num_separations; i++) {
        if (pparams->color[i].color_info_valid == false) {
            const devn_separation_name * dev_sep_name =
                            &(pdevn_params->separations.names[i]);
            unsigned int cs_sep_name_size;
            unsigned char * pcs_sep_name;

            pcs_sep_name = (unsigned char *)pcs->params.separation.sep_name;
            cs_sep_name_size = strlen(pcs->params.separation.sep_name);
            if (compare_color_names(dev_sep_name->data, dev_sep_name->size,
                            pcs_sep_name, cs_sep_name_size)) {
                gs_color_space temp_cs = *pcs;
                gs_client_color client_color;

                /*
                 * Create a copy of the color space and then modify it
                 * to force the use of the alternate color space.
                 */
                temp_cs.params.separation.use_alt_cspace = true;
                client_color.paint.values[0] = 1.0;
                capture_spot_equivalent_cmyk_colors(pdev, pgs, &client_color,
                                            &temp_cs, i, pparams);
                break;
            }
        }
    }
}

/* This is used for getting the equivalent CMYK values for the spots that
   may exist in a DeviceN output ICC profile */
static int
update_ICC_spot_equivalent_cmyk_colors(gx_device * pdev,
                    const gs_gstate * pgs, const gs_color_space * pcs,
                    gs_devn_params * pdevn_params,
                    equivalent_cmyk_color_params * pparams)
{
    int i, j;
    cmm_dev_profile_t *dev_profile;
    int code;
    gs_client_color client_color;


    code = dev_proc(pdev, get_profile)(pdev, &dev_profile);
    if (code < 0)
        return code;
    /*
     * Check if the ICC spot names matche any of the
     * separations for which we need an equivalent CMYK color.
     */
    for (i = 0; i < pdevn_params->separations.num_separations; i++) {
        if (pparams->color[i].color_info_valid == false) {
            const devn_separation_name * dev_sep_name =
                            &(pdevn_params->separations.names[i]);
            gsicc_colorname_t *name_entry;

            name_entry = dev_profile->spotnames->head;

            for (j = 0; j < dev_profile->device_profile[0]->num_comps; j++) {
                client_color.paint.values[j] = 0.0;
            }
            for (j = 0; j < dev_profile->spotnames->count; j++) {
                if (compare_color_names(dev_sep_name->data, dev_sep_name->size,
                                name_entry->name, name_entry->length)) {
                    /*
                     * Create a copy of the color space and then modify it
                     * to force the use of the alternate color space.
                     */
                    client_color.paint.values[j] = 1.0;
                    capture_spot_equivalent_cmyk_colors(pdev, pgs, &client_color,
                                                pcs, i, pparams);
                    break;
                }
                /* Did not match, grab the next one */
                name_entry = name_entry->next;
            }
        }
    }
    return 0;
}

static void
update_DeviceN_spot_equivalent_cmyk_colors(gx_device * pdev,
                    const gs_gstate * pgs, const gs_color_space * pcs,
                    gs_devn_params * pdevn_params,
                    equivalent_cmyk_color_params * pparams)
{
    int i;
    unsigned int j;
    unsigned int cs_sep_name_size;
    unsigned char * pcs_sep_name;

    /*
     * Check if the color space contains components named 'None'.  If so then
     * our capture logic does not work properly.  When present, the 'None'
     * components contain alternate color information.  However this info is
     * specified as part of the 'color' and not part of the color space.  Thus
     * we do not have this data when this routine is called.  See the
     * description of DeviceN color spaces in section 4.5 of the PDF spec.
     * In this situation we exit rather than produce invalid values.
     */
     for (j = 0; j < pcs->params.device_n.num_components; j++) {
        pcs_sep_name = (unsigned char *)pcs->params.device_n.names[j];
        cs_sep_name_size = strlen(pcs->params.device_n.names[j]);

        if (compare_color_names("None", 4, pcs_sep_name, cs_sep_name_size)) {
            /* If we are going out to a device that supports devn colors
               then it is possible that any preview that such a device creates
               will be in error due to the lack of the proper mappings from
               the full DeviceN colors (whicn can include one or more \None
               enrties). Display a warning about this if in debug mode */
#ifdef DEBUG
            if (dev_proc(pdev, dev_spec_op)(pdev, gxdso_supports_devn, NULL, 0))
                gs_warn("Separation preview may be inaccurate due to presence of \\None colorants");
#endif
            return;
        }
    }

    /*
     * Check if the color space's separation names matches any of the
     * separations for which we need an equivalent CMYK color.
     */
    for (i = 0; i < pdevn_params->separations.num_separations; i++) {
        if (pparams->color[i].color_info_valid == false) {
            const devn_separation_name * dev_sep_name =
                            &(pdevn_params->separations.names[i]);

            for (j = 0; j < pcs->params.device_n.num_components; j++) {
                pcs_sep_name = (unsigned char *)pcs->params.device_n.names[j];
                cs_sep_name_size = strlen(pcs->params.device_n.names[j]);
                if (compare_color_names(dev_sep_name->data, dev_sep_name->size,
                            pcs_sep_name, cs_sep_name_size)) {
                    gs_color_space temp_cs = *pcs;
                    gs_client_color client_color;

                    /*
                     * Create a copy of the color space and then modify it
                     * to force the use of the alternate color space.
                     */
                    memset(&client_color, 0, sizeof(client_color));
                    temp_cs.params.device_n.use_alt_cspace = true;
                    client_color.paint.values[j] = 1.0;
                    capture_spot_equivalent_cmyk_colors(pdev, pgs, &client_color,
                                            &temp_cs, i, pparams);
                    break;
                }
            }
        }
    }
}

static bool check_all_colors_known(int num_spot,
                equivalent_cmyk_color_params * pparams)
{
    for (num_spot--; num_spot >= 0; num_spot--)
        if (pparams->color[num_spot].color_info_valid == false)
            return false;
    return true;
}

/* If possible, update the equivalent CMYK color for a spot color */
int
update_spot_equivalent_cmyk_colors(gx_device * pdev, const gs_gstate * pgs,
    gs_devn_params * pdevn_params, equivalent_cmyk_color_params * pparams)
{
    const gs_color_space * pcs;
    cmm_dev_profile_t *dev_profile;
    int code;

    code = dev_proc(pdev, get_profile)(pdev, &dev_profile);
    if (code < 0)
        return code;

    /* If all of the color_info is valid then there is nothing to do. */
    if (pparams->all_color_info_valid)
        return 0;

    /* Verify that we really have some separations. */
    if (pdevn_params->separations.num_separations == 0) {
        pparams->all_color_info_valid = true;
        return 0;
    }
    /*
     * Verify that the given color space is a Separation or a DeviceN color
     * space.  If so then when check if the color space contains a separation
     * color for which we need a CMYK equivalent.
     */
    pcs = gs_currentcolorspace_inline(pgs);
    if (pcs != NULL) {
        if (pcs->type->index == gs_color_space_index_Separation) {
            update_Separation_spot_equivalent_cmyk_colors(pdev, pgs, pcs,
                                                pdevn_params, pparams);
            pparams->all_color_info_valid = check_all_colors_known
                    (pdevn_params->separations.num_separations, pparams);
        }
        else if (pcs->type->index == gs_color_space_index_DeviceN) {
            update_DeviceN_spot_equivalent_cmyk_colors(pdev, pgs, pcs,
                                                pdevn_params, pparams);
            pparams->all_color_info_valid = check_all_colors_known
                    (pdevn_params->separations.num_separations, pparams);
        } else if (pcs->type->index == gs_color_space_index_ICC &&
            dev_profile->spotnames != NULL) {
            /* In this case, we are trying to set up the equivalent colors
               for the spots in the output ICC profile */
            code = update_ICC_spot_equivalent_cmyk_colors(pdev, pgs, pcs,
                                                          pdevn_params,
                                                          pparams);
            if (code < 0)
                return code;
            pparams->all_color_info_valid = check_all_colors_known
                    (pdevn_params->separations.num_separations, pparams);
        }
    }
    return 0;
}

static void
save_spot_equivalent_cmyk_color(int sep_num,
                equivalent_cmyk_color_params * pparams, const frac cmyk[4])
{
    pparams->color[sep_num].c = cmyk[0];
    pparams->color[sep_num].m = cmyk[1];
    pparams->color[sep_num].y = cmyk[2];
    pparams->color[sep_num].k = cmyk[3];
    pparams->color[sep_num].color_info_valid = true;
}

/*
 * A structure definition for a device for capturing equivalent colors
 */
typedef struct color_capture_device_s {
    gx_device_common;
    gx_prn_device_common;
    /*        ... device-specific parameters ... */
    /* The following values are needed by the cmap procs for saving data */
    int sep_num;	/* Number of the separation being captured */
    /* Pointer to original device's equivalent CMYK colors */
    equivalent_cmyk_color_params * pequiv_cmyk_colors;
} color_capture_device;

/*
 * Replacement routines for the cmap procs.  These routines will capture the
 * equivalent color.
 */
static cmap_proc_gray(cmap_gray_capture_cmyk_color);
static cmap_proc_rgb(cmap_rgb_capture_cmyk_color);
static cmap_proc_cmyk(cmap_cmyk_capture_cmyk_color);
static cmap_proc_rgb_alpha(cmap_rgb_alpha_capture_cmyk_color);
static cmap_proc_separation(cmap_separation_capture_cmyk_color);
static cmap_proc_devicen(cmap_devicen_capture_cmyk_color);

static const gx_color_map_procs cmap_capture_cmyk_color = {
    cmap_gray_capture_cmyk_color,
    cmap_rgb_capture_cmyk_color,
    cmap_cmyk_capture_cmyk_color,
    cmap_rgb_alpha_capture_cmyk_color,
    cmap_separation_capture_cmyk_color,
    cmap_devicen_capture_cmyk_color
};

static void
cmap_gray_capture_cmyk_color(frac gray, gx_device_color * pdc,
        const gs_gstate * pgs, gx_device * dev, gs_color_select_t select)
{
    equivalent_cmyk_color_params * pparams =
            ((color_capture_device *)dev)->pequiv_cmyk_colors;
    int sep_num = ((color_capture_device *)dev)->sep_num;
    frac cmyk[4];

    cmyk[0] = cmyk[1] = cmyk[2] = frac_0;
    cmyk[3] = frac_1 - gray;
    save_spot_equivalent_cmyk_color(sep_num, pparams, cmyk);
}

static void
cmap_rgb_capture_cmyk_color(frac r, frac g, frac b, gx_device_color * pdc,
     const gs_gstate * pgs, gx_device * dev, gs_color_select_t select)
{
    equivalent_cmyk_color_params * pparams =
            ((color_capture_device *)dev)->pequiv_cmyk_colors;
    int sep_num = ((color_capture_device *)dev)->sep_num;
    frac cmyk[4];

    color_rgb_to_cmyk(r, g, b, pgs, cmyk, dev->memory);
    save_spot_equivalent_cmyk_color(sep_num, pparams, cmyk);
}

static void
cmap_cmyk_capture_cmyk_color(frac c, frac m, frac y, frac k, gx_device_color * pdc,
     const gs_gstate * pgs, gx_device * dev, gs_color_select_t select,
     const gs_color_space *pcs)
{
    equivalent_cmyk_color_params * pparams =
            ((color_capture_device *)dev)->pequiv_cmyk_colors;
    int sep_num = ((color_capture_device *)dev)->sep_num;
    frac cmyk[4];

    cmyk[0] = c;
    cmyk[1] = m;
    cmyk[2] = y;
    cmyk[3] = k;
    save_spot_equivalent_cmyk_color(sep_num, pparams, cmyk);
}

static void
cmap_rgb_alpha_capture_cmyk_color(frac r, frac g, frac b, frac alpha,
        gx_device_color * pdc, const gs_gstate * pgs, gx_device * dev,
                         gs_color_select_t select)
{
    cmap_rgb_capture_cmyk_color(r, g, b, pdc, pgs, dev, select);
}

static void
cmap_separation_capture_cmyk_color(frac all, gx_device_color * pdc,
     const gs_gstate * pgs, gx_device * dev, gs_color_select_t select,
     const gs_color_space *pcs)
{
    dmprintf(pgs->memory, "cmap_separation_capture_cmyk_color - this routine should not be executed\n");
}

/* The call to this is actually going to occur if we happen to be using a 
   named color profile and doing a replacement.  Since the destination profile
   will have been CMYK based during the swap out to find the equivalent color, we can
   go ahead and just grab the cmyk portion */
static void
cmap_devicen_capture_cmyk_color(const frac * pcc, gx_device_color * pdc,
     const gs_gstate * pgs, gx_device * dev, gs_color_select_t select,
     const gs_color_space *pcs)
{
    equivalent_cmyk_color_params * pparams =
        ((color_capture_device *)dev)->pequiv_cmyk_colors;
    int sep_num = ((color_capture_device *)dev)->sep_num;

    save_spot_equivalent_cmyk_color(sep_num, pparams, pcc);
}

/*
 * Note:  The color space (pcs) has already been modified to use the
 * alternate color space.
 */
void
capture_spot_equivalent_cmyk_colors(gx_device * pdev, const gs_gstate * pgs,
    const gs_client_color * pcc, const gs_color_space * pcs,
    int sep_num, equivalent_cmyk_color_params * pparams)
{
    gs_gstate temp_state = *((const gs_gstate *)pgs);
    color_capture_device temp_device = { 0 };
    gx_device_color dev_color;
    gsicc_rendering_param_t render_cond;
    cmm_dev_profile_t *dev_profile;
    cmm_profile_t *curr_output_profile;
    cmm_dev_profile_t temp_profile = {	/* Initialize to 0's/NULL's */
                          { 0 } /* device_profile[] */, 0 /* proof_profile */,
                          0 /* link_profile */, 0 /* oi_profile */,
                          0 /* blend_profile */, 0 /* postren_profile */,
                          { {0} } /* rendercond[] */, 0 /* devicegraytok */,
                          0 /* graydection */, 0 /* pageneutralcolor */,
                          0 /* usefastcolor */, 0 /* supports_devn */,
                          0 /* sim_overprint */, 0 /* spotnames */,
                          0 /* prebandthreshold */, 0 /* memory */,
                          { 0 } /* rc_header */
                          };

    dev_proc(pdev, get_profile)(pdev, &dev_profile);
    gsicc_extract_profile(pdev->graphics_type_tag,
                          dev_profile, &(curr_output_profile), &render_cond);
    /*
     * Create a temp device.  The primary purpose of this device is pass the
     * separation number and a pointer to the original device's equivalent
     * color parameters.  Since we only using this device for a very specific
     * purpose, we only set up the color_info structure and our data.
     */
    temp_device.color_info = pdev->color_info;
    temp_device.sep_num = sep_num;
    temp_device.pequiv_cmyk_colors = pparams;
    temp_device.memory = pgs->memory;

    temp_profile.usefastcolor = false;  /* This avoids a few headaches */
    temp_profile.prebandthreshold = true;
    temp_profile.supports_devn = false;
    temp_profile.rendercond[0] = render_cond;
    temp_profile.rendercond[1] = render_cond;
    temp_profile.rendercond[2] = render_cond;
    temp_profile.rendercond[3] = render_cond;
    temp_device.icc_struct = &temp_profile;

    /* The equivalent CMYK colors are used for
       the preview of the composite image and as such should not rely
       upon the output profile if our output profile is a DeviceN profile.
       For example, if our output profile was CMYK + OG then if we use
       the device profile here we may not get CMYK values that
       even come close to the color we want to simulate.  So, for this
       case we go ahead and use the default CMYK profile and let the user
       beware.  Note that the actual separations will be correct however
       for the CMYK + OG values. */

    if (curr_output_profile->data_cs == gsNCHANNEL) {
        temp_profile.device_profile[0] = temp_state.icc_manager->default_cmyk;
    } else {
        temp_profile.device_profile[0] = curr_output_profile;
    }
    set_dev_proc(&temp_device, get_profile, gx_default_get_profile);

    /*
     * Create a temp copy of the gs_gstate.  We do this so that we
     * can modify the color space mapping (cmap) procs.  We use our
     * replacment procs to capture the color.  The installation of a
     * Separation or DeviceN color space also sets a use_alt_cspace flag
     * in the state.  We also need to set this to use the alternate space.
     */
    temp_state.cmap_procs = &cmap_capture_cmyk_color;
    temp_state.color_component_map.use_alt_cspace = true;

    /* Now capture the color */
    pcs->type->remap_color (pcc, pcs, &dev_color, &temp_state,
                            (gx_device *)&temp_device, gs_color_select_texture);
}

/* Used for detecting if we are trying to find the equivalant color during
   named color replacement */
bool
named_color_equivalent_cmyk_colors(const gs_gstate * pgs)
{
    return pgs->cmap_procs == &cmap_capture_cmyk_color;
}
