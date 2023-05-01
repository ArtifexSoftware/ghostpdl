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


/* Implementation of transparency, other than rendering */
#include "math_.h"
#include "memory_.h"
#include "gx.h"
#include "gserrors.h"
#include "gstrans.h"
#include "gsutil.h"
#include "gzstate.h"
#include "gxdevcli.h"
#include "gdevdevn.h"
#include "gxblend.h"
#include "gdevp14.h"
#include "gscspace.h"
#include "gxarith.h"
#include "gxclist.h"
#include "gsicc_manage.h"
#include "gsicc_cache.h"
#include "gxdevsop.h"

/* ------ Transparency-related graphics state elements ------ */

int
gs_setblendmode(gs_gstate *pgs, gs_blend_mode_t mode)
{
#ifdef DEBUG
    if (gs_debug_c('v')) {
        static const char *const bm_names[] = { GS_BLEND_MODE_NAMES };

        dmlprintf1(pgs->memory, "[v]("PRI_INTPTR")blend_mode = ", (intptr_t)pgs);
        if (mode >= 0 && mode < countof(bm_names))
            dmprintf1(pgs->memory, "%s\n", bm_names[mode]);
        else
            dmprintf1(pgs->memory, "%d??\n", (int)mode);
    }
#endif
    /* Map Compatible to Normal so other code treats Compatible as Normal */
    /* Often BLEND_MODE_Normal is checked for optimized handling, and     */
    /* Compatible is now specified to be the same.                        */
    if (mode == BLEND_MODE_Compatible)
        mode = BLEND_MODE_Normal;
    if ((int)mode < 0 || (int)mode > MAX_BLEND_MODE)
        return_error(gs_error_rangecheck);
    pgs->blend_mode = mode;
    return 0;
}

gs_blend_mode_t
gs_currentblendmode(const gs_gstate *pgs)
{
    return pgs->blend_mode;
}

int
gs_settextknockout(gs_gstate *pgs, bool knockout)
{
    if_debug2m('v', pgs->memory, "[v]("PRI_INTPTR")text_knockout = %s\n",
              (intptr_t)pgs, (knockout ? "true" : "false"));
    pgs->text_knockout = knockout;
    return 0;
}

bool
gs_currenttextknockout(const gs_gstate *pgs)
{
    return pgs->text_knockout;
}

/* This is used to keep pdf14 compositor actions from the interpreter from
   corrupting pattern renderings.  For example, if the file has a softmask,
   the intrepter will send push and pop transparency state commands when
   q and Q operations are encountered.  If we are writing out to a pattern
   clist that has no trasparency we do not want these state changes to
   be entered as compositor actions in the pattern clist */

static int
check_for_nontrans_pattern(gs_gstate *pgs, unsigned char *comp_name)
{
    gx_device * dev = pgs->device;
    bool is_patt_clist = gx_device_is_pattern_clist(dev);
    bool is_patt_acum = gx_device_is_pattern_accum(dev);

    /* Check if we are collecting data for a pattern that has no
       transparency.  In that case, we need to ignore the state changes */
    if (is_patt_clist || is_patt_acum) {
        if (is_patt_clist) {
            gx_device_clist_writer *clwdev = (gx_device_clist_writer*) dev;
            const gs_pattern1_instance_t *pinst = clwdev->pinst;

            if (!(pinst->templat.uses_transparency)) {
                if_debug1m('v', pgs->memory,
                           "[v]%s NOT sending in pattern\n",comp_name);
                return(1);
            }
        }
        if (is_patt_acum) {
            gx_device_pattern_accum *padev = (gx_device_pattern_accum*) dev;
            const gs_pattern1_instance_t *pinst = padev->instance;

            if (!(pinst->templat.uses_transparency)) {
                if_debug1m('v', pgs->memory,
                           "[v]%s NOT sending in pattern\n",comp_name);
                return(1);
            }
        }
    }
    return(0);
}

/*
 * Push a PDF 1.4 transparency compositor onto the current device. Note that
 * if the current device already is a PDF 1.4 transparency compositor, the
 * composite will update its parameters but not create a new
 * compositor device.
 */
static int
gs_gstate_update_pdf14trans2(gs_gstate * pgs, gs_pdf14trans_params_t * pparams, bool retain_on_create)
{
    gx_device * dev = pgs->device;
    gx_device *pdf14dev = NULL;
    int code;
    int curr_num = dev->color_info.num_components;

    /*
     * Send the PDF 1.4 create compositor action specified by the parameters.
     */
    code = send_pdf14trans(pgs, dev, &pdf14dev, pparams, pgs->memory);
    if (code < 0)
        return code;
    /*
     * If we created a new PDF 1.4 compositor device then we need to install it
     * into the graphics state.
     */
    if (code == 1) {
        gx_set_device_only(pgs, pdf14dev);
        gx_device_retain(pdf14dev, retain_on_create);
        code = 0;
    }

    /* If we had a color space change and we are in overprint, then we need to
       update the drawn_comps */
    if (pgs->overprint && curr_num != pdf14dev->color_info.num_components) {
        code = gs_do_set_overprint(pgs);
    }

    return code;
}

static int
gs_gstate_update_pdf14trans(gs_gstate * pgs, gs_pdf14trans_params_t * pparams)
{
    return gs_gstate_update_pdf14trans2(pgs, pparams, true);
}

void
gs_trans_group_params_init(gs_transparency_group_params_t *ptgp, float opacity)
{
    ptgp->ColorSpace = NULL;    /* bogus, but can't do better */
    ptgp->Isolated = false;
    ptgp->Knockout = false;
    ptgp->page_group = false;
    ptgp->text_group = PDF14_TEXTGROUP_NO_BT;
    ptgp->image_with_SMask = false;
    ptgp->mask_id = 0;
    ptgp->iccprofile = NULL;
    ptgp->group_opacity = opacity;
    ptgp->group_shape = 1.0;
    ptgp->shade_group = false;
}

int
gs_update_trans_marking_params(gs_gstate * pgs)
{
    gs_pdf14trans_params_t params = { 0 };
    int code;

    if_debug0m('v', pgs->memory, "[v]gs_update_trans_marking_params\n");
    params.pdf14_op = PDF14_SET_BLEND_PARAMS;
    code = gs_gstate_update_pdf14trans(pgs, &params);
    if (code == gs_error_unregistered)
        code = 0;
    return code;
}

int
gs_begin_transparency_group(gs_gstate *pgs,
                            const gs_transparency_group_params_t *ptgp,
                            const gs_rect *pbbox, pdf14_compositor_operations group_type)
{
    gs_pdf14trans_params_t params = { 0 };
    const gs_color_space *blend_color_space;
    cmm_profile_t *profile;

    if (check_for_nontrans_pattern(pgs,
                  (unsigned char *)"gs_begin_transparency_group")) {
        return(0);
    }
    /*
     * Put parameters into a compositor parameter and then call the
     * composite.  This will pass the data to the PDF 1.4
     * transparency device.
     */
    params.pdf14_op = group_type;
    params.Isolated = ptgp->Isolated;
    params.Knockout = ptgp->Knockout;
    if (group_type == PDF14_BEGIN_TRANS_PAGE_GROUP)
        params.page_group = true;
    params.image_with_SMask = ptgp->image_with_SMask;
    params.opacity = ptgp->group_opacity;
    params.shape = ptgp->group_shape;
    params.blend_mode = pgs->blend_mode;
    params.text_group = ptgp->text_group;
    params.shade_group = ptgp->shade_group;
    params.ColorSpace = ptgp->ColorSpace;
    /* This function is called during the c-list writer side.
       Store some information so that we know what the color space is
       so that we can adjust according later during the clist reader.
       We currently will use the concrete space for any space other than a
       device space.  However, if the device is a sep device it will blend
       in DeviceN color space as required.  */
    blend_color_space = gs_currentcolorspace_inline(pgs);
    if (gs_color_space_get_index(blend_color_space) > gs_color_space_index_DeviceCMYK) {
        /* ICC and PS CIE based case.  Note that unidirectional PS CIE color
           spaces should not be allowed but end up occuring when processing
           PDF files with -dUseCIEColor.  We will end up using the appropriate
           ICC default color space in these cases. */
        blend_color_space = gs_currentcolorspace_inline(pgs);
    } else {
        blend_color_space = cs_concrete_space(blend_color_space, pgs);
        if (!blend_color_space)
            return_error(gs_error_undefined);
    }
    /* Note that if the /CS parameter was NOT present in the push
       of the transparency group, then we must actually inherent
       the previous group color space, or the color space of the
       target device (process color model).  Here we just want
       to set it as a unknown type for clist writing, as we will take care
       of using the parent group color space later during clist reading.
       Also, if the group was not isolated we MUST use the parent group
       color space regardless of what the group color space is specified to be.
       Note that the page group should always be isolated */
    if (group_type == PDF14_BEGIN_TRANS_PAGE_GROUP)
        params.Isolated = true;

    if (ptgp->ColorSpace == NULL || params.Isolated != true) {
        params.group_color_type = UNKNOWN;
        params.group_color_numcomps = 0;
    } else {
        /* The /CS parameter was present.  Use what was set.  Currently
           all our Device spaces are actually ICC based.  The other options
           are if -dUseCIEColor is set, in which case it could be
           coming in as a PS CIE color space, which should not be allowed
           but should default to one of the default ICC color spaces.  Note
           that CalRGB and CalGray, which are valid bidirectional color spaces
           are converted to ICC profiles during installation. PS CIE building
           to ICC is delayed. */
        if ( gs_color_space_is_ICC(blend_color_space) ) {
            /* Blending space is ICC based.  If we are doing c-list rendering
               we will need to write this color space into the clist.
               */
            params.group_color_type = ICC;
            params.group_color_numcomps =
                blend_color_space->cmm_icc_profile_data->num_comps;
            /* Get the ICC profile */
            params.iccprofile = blend_color_space->cmm_icc_profile_data;
            params.icc_hash = gsicc_get_hash(blend_color_space->cmm_icc_profile_data);
        } else {
            /* Color space was NOT ICC based.  PS CIE space and DeviceN are the only
               other option.  Use the ICC default based upon the component count. */
            switch (cs_num_components(blend_color_space)) {
                case 1:
                    profile =  pgs->icc_manager->default_gray;
                    break;
                case 3:
                    profile =  pgs->icc_manager->default_rgb;
                    break;
                case 4:
                    profile =  pgs->icc_manager->default_cmyk;
                break;
                default:
                    /* We can end up here if we are in a deviceN color space and
                       we have a sep output device */
                    profile = NULL;
                    params.group_color_type = DEVICEN;
                    params.group_color_numcomps = cs_num_components(blend_color_space);
                break;
            }
            if (profile != NULL) {
                params.group_color_type = ICC;
                params.group_color_numcomps = profile->num_comps;
                params.iccprofile = profile;
                params.icc_hash = gsicc_get_hash(profile);
            }
        }
    }
#ifdef DEBUG
    if (gs_debug_c('v')) {
        static const char *const cs_names[] = {
            GS_COLOR_SPACE_TYPE_NAMES
        };
        dmlprintf6(pgs->memory, "[v]("PRI_INTPTR")begin_transparency_group [%g %g %g %g] Num_grp_clr_comp = %d\n",
                   (intptr_t)pgs, pbbox->p.x, pbbox->p.y, pbbox->q.x, pbbox->q.y,params.group_color_numcomps);
        if (ptgp->ColorSpace)
            dmprintf1(pgs->memory, "     CS = %s",
                cs_names[(int)gs_color_space_get_index(ptgp->ColorSpace)]);
        else
            dmputs(pgs->memory, "     (no CS)");

        dmprintf4(pgs->memory, "  Isolated = %d  Knockout = %d text_group = %d page_group = %d\n",
                 ptgp->Isolated, ptgp->Knockout, ptgp->text_group, ptgp->page_group);
    }
#endif
    params.bbox = *pbbox;
    return gs_gstate_update_pdf14trans(pgs, &params);
}

int
gx_begin_transparency_group(gs_gstate * pgs, gx_device * pdev,
                                const gs_pdf14trans_params_t * pparams)
{
    gs_transparency_group_params_t tgp = {0};
    gs_rect bbox;

    if (pparams->Background_components != 0 &&
        pparams->Background_components != pdev->color_info.num_components)
        return_error(gs_error_rangecheck);
    tgp.Isolated = pparams->Isolated;
    tgp.Knockout = pparams->Knockout;
    tgp.page_group = pparams->page_group;
    tgp.idle = pparams->idle;
    tgp.mask_id = pparams->mask_id;
    tgp.text_group = pparams->text_group;
    tgp.shade_group = pparams->shade_group;

    /* Needed so that we do proper blending */
    tgp.group_color_type = pparams->group_color_type;
    tgp.group_color_numcomps = pparams->group_color_numcomps;
    tgp.iccprofile = pparams->iccprofile;
    tgp.icc_hashcode = pparams->icc_hash;

    tgp.group_opacity = pparams->opacity;
    tgp.group_shape = pparams->shape;

    if (tgp.Knockout && tgp.text_group == PDF14_TEXTGROUP_BT_PUSHED &&
        ((pgs->overprint && pgs->is_fill_color) || (pgs->stroke_overprint && !pgs->is_fill_color)))
        pgs->blend_mode = BLEND_MODE_CompatibleOverprint;
    else
        pgs->blend_mode = pparams->blend_mode;
    bbox = pparams->bbox;
#ifdef DEBUG
    if (gs_debug_c('v')) {
        static const char *const cs_names[] = {
            GS_COLOR_SPACE_TYPE_NAMES
        };
        dmlprintf6(pdev->memory, "[v]("PRI_INTPTR")gx_begin_transparency_group [%g %g %g %g] Num_grp_clr_comp = %d\n",
                   (intptr_t)pgs, bbox.p.x, bbox.p.y, bbox.q.x, bbox.q.y,
                   pparams->group_color_numcomps);
        dmlprintf2(pdev->memory, "     opacity = %g shape = %g\n", pparams->opacity, pparams->shape);
        if (tgp.ColorSpace)
            dmprintf1(pdev->memory, "     CS = %s",
                cs_names[(int)gs_color_space_get_index(tgp.ColorSpace)]);
        else
            dmputs(pdev->memory, "     (no CS)");
        dmprintf3(pdev->memory, "  Isolated = %d  Knockout = %d  page_group = %d\n",
                 tgp.Isolated, tgp.Knockout, tgp.page_group);
        if (tgp.iccprofile)
            dmprintf(pdev->memory, "     Have ICC Profile for blending\n");

    }
#endif
    return (*dev_proc(pdev, begin_transparency_group)) (pdev, &tgp, &bbox, pgs,
                                                            NULL);
}

int
gs_end_transparency_group(gs_gstate *pgs)
{
    gs_pdf14trans_params_t params = { 0 };

    if (check_for_nontrans_pattern(pgs,
                  (unsigned char *)"gs_end_transparency_group")) {
        return(0);
    }
    if_debug0m('v', pgs->memory, "[v]gs_end_transparency_group\n");
    params.pdf14_op = PDF14_END_TRANS_GROUP;  /* Other parameters not used */
    return gs_gstate_update_pdf14trans(pgs, &params);
}

int
gs_end_transparency_text_group(gs_gstate *pgs)
{
    gs_pdf14trans_params_t params = { 0 };

    if (check_for_nontrans_pattern(pgs,
        (unsigned char *)"gs_end_transparency_text_group")) {
        return(0);
    }
    if_debug0m('v', pgs->memory, "[v]gs_end_transparency_text_group\n");
    params.pdf14_op = PDF14_END_TRANS_TEXT_GROUP;  /* Other parameters not used */
    return gs_gstate_update_pdf14trans(pgs, &params);
}

int
gs_begin_transparency_text_group(gs_gstate *pgs)
{
    gs_pdf14trans_params_t params = { 0 };

    if (check_for_nontrans_pattern(pgs,
        (unsigned char *)"gs_begin_transparency_text_group")) {
        return(0);
    }
    if_debug0m('v', pgs->memory, "[v]gs_begin_transparency_text_group\n");
    params.pdf14_op = PDF14_BEGIN_TRANS_TEXT_GROUP;  /* Other parameters not used */
    return gs_gstate_update_pdf14trans(pgs, &params);
}

int
gx_end_transparency_group(gs_gstate * pgs, gx_device * pdev)
{
    if_debug0m('v', pgs->memory, "[v]gx_end_transparency_group\n");
    return (*dev_proc(pdev, end_transparency_group)) (pdev, pgs);
}

/* Commands for handling q softmask Q in graphic states */

int
gs_push_transparency_state(gs_gstate *pgs)
{
    gs_pdf14trans_params_t params = { 0 };
    int code;

    if (check_for_nontrans_pattern(pgs,
                  (unsigned char *)"gs_push_transparency_state")) {
        return(0);
    }
    /* Set the pending flag to true, which indicates
       that we need to watch for end transparency
       soft masks when we are at this graphic state
       level */
    /* pgs->trans_flags.xstate_pending = true; */
    /* Actually I believe the above flag is not
       needed.  We really should be watching for
       the softmask even at the base level.  What
       we need to watch for are q operations after
       a soft mask end has occured. */
    /* Check if we have a change flag set to true.
       this indicates that a softmask is present.
       We will need to send a push state to save
       the current soft mask, so that we can
       restore it later */
    if (pgs->trans_flags.xstate_change) {
        if_debug0m('v', pgs->memory, "[v]gs_push_transparency_state sending\n");
        params.pdf14_op = PDF14_PUSH_TRANS_STATE;
        code = gs_gstate_update_pdf14trans(pgs, &params);
        if (code < 0)
            return(code);
    } else {
        if_debug0m('v', pgs->memory, "[v]gs_push_transparency_state NOT sending\n");
    }
    return(0);
}

int
gs_pop_transparency_state(gs_gstate *pgs, bool force)
{
    gs_pdf14trans_params_t params = { 0 };
    int code;

    if (check_for_nontrans_pattern(pgs,
                  (unsigned char *)"gs_pop_transparency_state")) {
        return(0);
    }
    /* Check if flag is set, which indicates that we have
       an active softmask for the graphic state.  We
       need to communicate to the compositor to pop
       the softmask */
    if ( pgs->trans_flags.xstate_change || force) {
        if_debug0m('v', pgs->memory, "[v]gs_pop_transparency_state sending\n");
        params.pdf14_op = PDF14_POP_TRANS_STATE;
        code = gs_gstate_update_pdf14trans(pgs, &params);
        if ( code < 0 )
            return (code);
    } else {
        if_debug0m('v', pgs->memory, "[v]gs_pop_transparency_state NOT sending\n");
    }
    /* There is no reason to reset any of the flags since
       they will be reset by the graphic state restore */
    return(0);
}

int
gx_pop_transparency_state(gs_gstate * pgs, gx_device * pdev)
{
    if_debug0m('v', pgs->memory, "[v]gx_pop_transparency_state\n");
    return (*dev_proc(pdev, pop_transparency_state)) (pdev, pgs);
}

int
gx_push_transparency_state(gs_gstate * pgs, gx_device * pdev)
{
    if_debug0m('v', pgs->memory, "[v]gx_push_transparency_state\n");
    return (*dev_proc(pdev, push_transparency_state)) (pdev, pgs);
}

/*
 * Handler for identity mask transfer functions.
 */
static int
mask_transfer_identity(double in, float *out, void *proc_data)
{
    *out = (float) in;
    return 0;
}

void
gs_trans_mask_params_init(gs_transparency_mask_params_t *ptmp,
                          gs_transparency_mask_subtype_t subtype)
{
    ptmp->ColorSpace = 0;
    ptmp->subtype = subtype;
    ptmp->Background_components = 0;
    ptmp->Matte_components = 0;
    ptmp->GrayBackground = 0.0;
    ptmp->TransferFunction = mask_transfer_identity;
    ptmp->TransferFunction_data = 0;
    ptmp->replacing = false;
    ptmp->iccprofile = NULL;
}

int
gs_begin_transparency_mask(gs_gstate * pgs,
                           const gs_transparency_mask_params_t * ptmp,
                           const gs_rect * pbbox, bool mask_is_image)
{
    gs_pdf14trans_params_t params = { 0 };
    gs_pdf14trans_params_t params_color = { 0 };
    const int l = sizeof(params.Background[0]) * ptmp->Background_components;
    const int m = sizeof(params.Matte[0]) * ptmp->Matte_components;
    int i, code;
    gs_color_space *blend_color_space;
    gsicc_manager_t *icc_manager = pgs->icc_manager;
    bool deep = device_is_deep(pgs->device);

    if (check_for_nontrans_pattern(pgs,
                  (unsigned char *)"gs_pop_transparency_state")) {
        return(0);
    }
    params.pdf14_op = PDF14_BEGIN_TRANS_MASK;
    params.bbox = *pbbox;
    params.subtype = ptmp->subtype;
    params.Background_components = ptmp->Background_components;
    memcpy(params.Background, ptmp->Background, l);
    params.ColorSpace = ptmp->ColorSpace;
    params.Matte_components = ptmp->Matte_components;
    memcpy(params.Matte, ptmp->Matte, m);
    params.GrayBackground = ptmp->GrayBackground;
    params.transfer_function = ptmp->TransferFunction_data;
    params.function_is_identity =
            (ptmp->TransferFunction == mask_transfer_identity);
    params.mask_is_image = mask_is_image;
    params.replacing = ptmp->replacing;

    /* The eventual state that we want this smask to be moved to
       is always gray.  This should provide us with a significant
       speed improvement over the old code.  This does not keep us
       from having groups within the softmask getting blended in different
       color spaces, it just makes the final space be gray, which is what
       we will need to get to eventually anyway. In this way we avoid a
       final color conversion on a potentially large buffer. */
    /* Also check if we have loaded in the transparency icc profiles.  If not
       go ahead and take care of that now */
    if (icc_manager->smask_profiles == NULL) {
        code = gsicc_initialize_iccsmask(icc_manager);
        if (code < 0)
            return(code);
    }
    /* A new soft mask group,  make sure the profiles are set */
    if_debug0m('v', pgs->memory, "[v]pushing soft mask color sending\n");
    if (params.subtype != TRANSPARENCY_MASK_None) {
        params_color.pdf14_op = PDF14_PUSH_SMASK_COLOR;
        code = gs_gstate_update_pdf14trans(pgs, &params_color);
        if (code < 0)
            return(code);
        blend_color_space = gs_cspace_new_DeviceGray(pgs->memory);
        if (blend_color_space == NULL)
            return_error(gs_error_VMerror);
        blend_color_space->cmm_icc_profile_data = pgs->icc_manager->default_gray;
        gsicc_adjust_profile_rc(blend_color_space->cmm_icc_profile_data, 1, "gs_begin_transparency_mask");
        if_debug9m('v', pgs->memory, "[v]("PRI_INTPTR")gs_begin_transparency_mask [%g %g %g %g]\n"
                   "      subtype = %d  Background_components = %d, Matte_components = %d, %s\n",
                  (intptr_t)pgs, pbbox->p.x, pbbox->p.y, pbbox->q.x, pbbox->q.y,
                  (int)ptmp->subtype, ptmp->Background_components,
                  ptmp->Matte_components,
                  (ptmp->TransferFunction == mask_transfer_identity ? "no TR" :
                   "has TR"));
        /* Sample the transfer function */
        /* For non-deep cases, we sample at 00,01,02..fe,ff.
         * For deep cases, we sample from 0000,0100,0200..fe00,ff00 and a final one at ffff.
         * This enables us to interpolate easily.
         */
        if (deep) {
            uint16_t *trans16 = (uint16_t *)params.transfer_fn;
            float out;
            for (i = 0; i < MASK_TRANSFER_FUNCTION_SIZE; i++) {
                float in = (float)(i * (1.0 / MASK_TRANSFER_FUNCTION_SIZE));

                ptmp->TransferFunction(in, &out, ptmp->TransferFunction_data);
                trans16[i] = (uint16_t)floor((double)(out * 65535 + 0.5));
            }
            ptmp->TransferFunction(1.0, &out, ptmp->TransferFunction_data);
            trans16[MASK_TRANSFER_FUNCTION_SIZE] = (uint16_t)floor((double)(out * 65535 + 0.5));
        } else {
            for (i = 0; i < MASK_TRANSFER_FUNCTION_SIZE; i++) {
                float in = (float)(i * (1.0 / (MASK_TRANSFER_FUNCTION_SIZE - 1)));
                float out;

                ptmp->TransferFunction(in, &out, ptmp->TransferFunction_data);
                params.transfer_fn[i] = (byte)floor((double)(out * 255 + 0.5));
            }
        }
        /* Note:  This function is called during the c-list writer side. */
        if ( blend_color_space->cmm_icc_profile_data != NULL ) {
        /* Blending space is ICC based.  If we are doing c-list rendering we will
           need to write this color space into the clist. */
            params.group_color_type = ICC;
            params.group_color_numcomps =
                    blend_color_space->cmm_icc_profile_data->num_comps;
            /* Get the ICC profile */
            /* We don't reference count this - see comment in
             * pdf14_update_device_color_procs_pop_c()
             */
            params.iccprofile = blend_color_space->cmm_icc_profile_data;
            params.icc_hash = gsicc_get_hash(blend_color_space->cmm_icc_profile_data);
        } else {
            params.group_color_type = GRAY_SCALE;
            params.group_color_numcomps = 1;  /* Need to check */
        }
        /* Explicitly decrement the profile data since blend_color_space may not
         * be an ICC color space object.
         */
        gsicc_adjust_profile_rc(blend_color_space->cmm_icc_profile_data, -1, "gs_begin_transparency_mask");
        rc_decrement_only_cs(blend_color_space, "gs_begin_transparency_mask");
    }
    return gs_gstate_update_pdf14trans(pgs, &params);
}

/* This occurs on the c-list reader side */

int
gx_begin_transparency_mask(gs_gstate * pgs, gx_device * pdev,
                                const gs_pdf14trans_params_t * pparams)
{
    gx_transparency_mask_params_t tmp;
    const int l = sizeof(pparams->Background[0]) * pparams->Background_components;
    const int m = sizeof(pparams->Matte[0]) * pparams->Matte_components;

    tmp.group_color_type = pparams->group_color_type;
    tmp.subtype = pparams->subtype;
    tmp.group_color_numcomps = pparams->group_color_numcomps;
    tmp.Background_components = pparams->Background_components;
    memcpy(tmp.Background, pparams->Background, l);
    tmp.Matte_components = pparams->Matte_components;
    memcpy(tmp.Matte, pparams->Matte, m);
    tmp.GrayBackground = pparams->GrayBackground;
    tmp.function_is_identity = pparams->function_is_identity;
    tmp.idle = pparams->idle;
    tmp.replacing = pparams->replacing;
    tmp.mask_id = pparams->mask_id;

    if (tmp.group_color_type == ICC ) {
        /* Do I need to ref count here? */
        tmp.iccprofile = pparams->iccprofile;
        tmp.icc_hashcode = pparams->icc_hash;
    } else {
        tmp.iccprofile = NULL;
        tmp.icc_hashcode = 0;
    }
    memcpy(tmp.transfer_fn, pparams->transfer_fn, size_of(tmp.transfer_fn));
    if_debug10m('v', pgs->memory,
               "[v]("PRI_INTPTR")gx_begin_transparency_mask [%g %g %g %g]\n"
               "      subtype = %d  Background_components = %d Matte_components = %d Num_grp_clr_comp = %d %s\n",
              (intptr_t)pgs, pparams->bbox.p.x, pparams->bbox.p.y,
              pparams->bbox.q.x, pparams->bbox.q.y,
              (int)tmp.subtype, tmp.Background_components, tmp.Matte_components,
              tmp.group_color_numcomps,
              (tmp.function_is_identity ? "no TR" :
               "has TR"));
    return (*dev_proc(pdev, begin_transparency_mask))
                        (pdev, &tmp, &(pparams->bbox), pgs, NULL);
}

int
gs_end_transparency_mask(gs_gstate *pgs,
                         gs_transparency_channel_selector_t csel)
{
    gs_pdf14trans_params_t params = { 0 };
    gs_pdf14trans_params_t params_color = { 0 };
    int code;

    if (check_for_nontrans_pattern(pgs,
                  (unsigned char *)"gs_end_transparency_mask")) {
        return(0);
    }
    /* If we have done a q then set a flag to watch for any Qs */
   /* if (pgs->trans_flags.xstate_pending)
        pgs->trans_flags.xstate_change = true; */
    /* This should not depend upon if we have encountered a q
       operation.  We could be setting a softmask, before
       there is any q operation.  Unlikely but it could happen.
       Then if we encouter a q operation (and this flag
       is true) we will need to
       push the mask graphic state (PDF14_PUSH_TRANS_STATE). */
    pgs->trans_flags.xstate_change = true;
    if_debug1m('v', pgs->memory,
               "[v]xstate_changed set true, gstate level is %d\n", pgs->level);
    if_debug2m('v', pgs->memory,
               "[v]("PRI_INTPTR")gs_end_transparency_mask(%d)\n", (intptr_t)pgs,
               (int)csel);
    params.pdf14_op = PDF14_END_TRANS_MASK;  /* Other parameters not used */
    params.csel = csel;
    /* If this is the outer end then return us to our normal defaults */
    if_debug0m('v', pgs->memory, "[v]popping soft mask color sending\n");
    params_color.pdf14_op = PDF14_POP_SMASK_COLOR;
    code = gs_gstate_update_pdf14trans(pgs, &params_color);
    if (code < 0)
        return(code);
    return gs_gstate_update_pdf14trans(pgs, &params);
}

int
gx_end_transparency_mask(gs_gstate * pgs, gx_device * pdev,
                                const gs_pdf14trans_params_t * pparams)
{
    if_debug2m('v', pgs->memory,
               "[v]("PRI_INTPTR")gx_end_transparency_mask(%d)\n", (intptr_t)pgs,
               (int)pparams->csel);
    return (*dev_proc(pdev, end_transparency_mask)) (pdev, pgs);
}

/*
 * We really only care about the number of spot colors when we have
 * a device which supports spot colors.  With the other devices we use
 * the tint transform function for DeviceN and Separation color spaces
 * and convert spot colors into process colors.
 */
static int
get_num_pdf14_spot_colors(gs_gstate * pgs)
{
    gx_device * dev = pgs->device;
    gs_devn_params * pclist_devn_params = dev_proc(dev, ret_devn_params)(dev);

    /*
     * Devices which support spot colors store the PageSpotColors device
     * parameter inside their devn_params structure.  (This is done by the
     * devn_put_params routine.)  The PageSpotColors device parameter is
     * set by pdf_main whenever a PDF page is being processed.  See
     * countspotcolors in lib/pdf_main.ps.
     */
    if (pclist_devn_params != NULL) {
        /* If the sep order names were specified, then we should only allocate
           for those.  But only the nonstandard colorants that are stored
           in num_separations.  See devn_put_params for details on this.
           Right now, the PDF14 device will always include CMYK.  A future
           optimization is to be able to NOT have those included in the buffer
           allocations if we don't specify them.  It would then be possible to
           output 8 separations at a time without using compressed color. */
        if (pclist_devn_params->num_separation_order_names == 0) {
            return pclist_devn_params->page_spot_colors;
        }
        return (pclist_devn_params->separations.num_separations);
    }
    return 0;
}

int
gs_push_pdf14trans_device(gs_gstate * pgs, bool is_pattern, bool retain,
                          int depth, int spot_color_count)
{
    gs_pdf14trans_params_t params = { 0 };
    cmm_profile_t *icc_profile;
    gsicc_rendering_param_t render_cond;
    int code;
    cmm_dev_profile_t *dev_profile;
    unsigned char pattern_opsim_setting[2];

    code = dev_proc(pgs->device, get_profile)(pgs->device,  &dev_profile);
    if (code < 0)
        return code;
    gsicc_extract_profile(GS_UNKNOWN_TAG, dev_profile, &icc_profile,
                          &render_cond);
    params.pdf14_op = PDF14_PUSH_DEVICE;
    /*
     * We really only care about the number of spot colors when we have
     * a device which supports spot colors.  With the other devices we use
     * the tint transform function for DeviceN and Separation color spaces
     * and convert spot colors into process colors.
     */
    params.num_spot_colors = get_num_pdf14_spot_colors(pgs);
    params.is_pattern = is_pattern;

    /* If pattern, get overprint simulation information from
       the pattern accumulators target device */
    if (is_pattern && dev_proc(pgs->device, dev_spec_op)(pgs->device, gxdso_overprintsim_state, &pattern_opsim_setting, sizeof(pattern_opsim_setting))) {
        /* Use the target device setting */
        params.overprint_sim_push = pattern_opsim_setting[0];
        params.num_spot_colors_int = pattern_opsim_setting[1];
    } else {
        /* Use information from interpreter */
        params.num_spot_colors_int = spot_color_count;
        if (depth < 0)
            params.overprint_sim_push = true;
    }

    /* If we have an NCLR ICC profile, the extra spot colorants do
    *  get included in the transparency buffers. Trying to avoid
    * including them became a rube goldberg mess in terms of knowing
    * which colorants are on the page vs what has been specified and
    * any aliasing between these two.  Just too many things to go wrong.
    * So we allocate all and carry around. If you are doing special
    * spot handling with transparency this is the cost. */

    if (dev_profile->spotnames != NULL && dev_profile->spotnames->count > 4) {
        /* Making an assumption here, that list is CMYK + extra.
           An error should have been thrown by the target device if not. */
        int avail_page_spots = pgs->device->color_info.num_components - 4;
        params.num_spot_colors_int = avail_page_spots;
        params.num_spot_colors = avail_page_spots;

        /* This should not be possible, but lets be safe. We can't have a negative
          number of source spots to carry forward, so apply threshold. */
        if (params.num_spot_colors_int < 0)
            params.num_spot_colors_int = 0;
        if (params.num_spot_colors < 0)
            params.num_spot_colors = 0;
    }

    /* If we happen to be in a situation where we are going out to a device
       whose profile is CIELAB then we will need to make sure that we
       do our blending in RGB and convert to CIELAB when we do the put_image
       command */
    if (icc_profile->data_cs == gsCIELAB ||
        icc_profile->islab) {
        params.iccprofile = pgs->icc_manager->default_rgb;
    }
    /* Note: Other parameters not used */
    return gs_gstate_update_pdf14trans2(pgs, &params, retain);
}

int
gs_pop_pdf14trans_device(gs_gstate * pgs, bool is_pattern)
{
    gs_pdf14trans_params_t params = { 0 };

    params.is_pattern = is_pattern;
    params.pdf14_op = PDF14_POP_DEVICE;  /* Other parameters not used */
    return gs_gstate_update_pdf14trans(pgs, &params);
}

int
gs_abort_pdf14trans_device(gs_gstate * pgs)
{
    gs_pdf14trans_params_t params = { 0 };

    params.pdf14_op = PDF14_ABORT_DEVICE;  /* Other parameters not used */
    return gs_gstate_update_pdf14trans(pgs, &params);
}

/* Something has gone wrong have the device clean up everything */

int
gx_abort_trans_device(gs_gstate * pgs, gx_device * pdev)
{
    if_debug1m('v', pgs->memory, "[v]("PRI_INTPTR")gx_abort_trans_device\n", (intptr_t)pgs);
    return (*dev_proc(pdev, discard_transparency_layer)) (pdev, pgs);
}

int gs_setstrokeconstantalpha(gs_gstate *pgs, float alpha)
{
    pgs->strokeconstantalpha = alpha;
    return 0;
}

float gs_getstrokeconstantalpha(const gs_gstate *pgs)
{
    return pgs->strokeconstantalpha;
}

int gs_setfillconstantalpha(gs_gstate *pgs, float alpha)
{
    pgs->fillconstantalpha = (float)alpha;
    return 0;
}

float gs_getfillconstantalpha(const gs_gstate *pgs)
{
    return pgs->fillconstantalpha;
}

int gs_setalphaisshape(gs_gstate *pgs, bool AIS)
{
    pgs->alphaisshape = AIS;
    return 0;
}

bool gs_getalphaisshape(gs_gstate *pgs)
{
    return pgs->alphaisshape;
}
