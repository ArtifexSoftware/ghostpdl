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

/*  Handling of color spaces stored during replacement of all
 *  text with black.
*/

#include "gsmemory.h"
#include "gsstruct.h"
#include "gzstate.h"
#include "gsicc_blacktext.h"
#include "gsicc_cache.h"

/* gsicc_blacktextvec_state_t is going to be storing GCed items
   (color spaces and client colors) and so will need to be GCed */
gs_private_st_ptrs4(st_blacktextvec_state, gsicc_blacktextvec_state_t,
    "gsicc_blacktextvec_state", blacktextvec_state_enum_ptrs,
    blacktextvec_state_reloc_ptrs, pcs, pcs_alt, pcc, pcc_alt);

static void
rc_gsicc_blacktextvec_state_free(gs_memory_t *mem, void *ptr_in,
    client_name_t cname)
{
    gsicc_blacktextvec_state_t *state = (gsicc_blacktextvec_state_t*)ptr_in;

    rc_decrement_cs(state->pcs, "rc_gsicc_blacktextvec_state_free");
    rc_decrement_cs(state->pcs_alt, "rc_gsicc_blacktextvec_state_free");

    gs_free_object(state->memory, state,
        "rc_gsicc_blacktextvec_state_free");
}

gsicc_blacktextvec_state_t*
gsicc_blacktextvec_state_new(gs_memory_t *memory, bool is_text)
{
    gsicc_blacktextvec_state_t *result;

    result = gs_alloc_struct(memory->stable_memory, gsicc_blacktextvec_state_t,
        &st_blacktextvec_state, "gsicc_blacktextvec_state_new");
    if (result == NULL)
        return NULL;
    rc_init_free(result, memory->stable_memory, 1, rc_gsicc_blacktextvec_state_free);
    result->memory = memory->stable_memory;
    result->pcs = NULL;
    result->pcs_alt = NULL;
    result->pcc = NULL;
    result->pcc_alt = NULL;
    result->is_text = is_text;

    return result;
}

/* Crude white color check. Only valid for ICC based RGB, CMYK, Gray, and LAB CS.
  Makes some assumptions about profile.  Also may want some tolerance check. */
bool gsicc_is_white_blacktextvec(gs_gstate *pgs, gx_device *dev, gs_color_space* pcs, gs_client_color* pcc)
{
    double Lstar = 0;
    double astar = 0;
    double bstar = 0;
    cmm_dev_profile_t* dev_profile;

    dev_proc(dev, get_profile)(dev, &dev_profile);

    if (gs_color_space_get_index(pcs) == gs_color_space_index_ICC) {
        if (pcs->cmm_icc_profile_data->data_cs == gsCIELAB) {
            if (pcc->paint.values[0] >= dev_profile->blackthresholdL &&
                fabs(pcc->paint.values[1]) < dev_profile->blackthresholdC &&
                fabs(pcc->paint.values[2]) < dev_profile->blackthresholdC)
                    return true;
            else
                return false;
        }
        /* For all others, lets get to CIELAB value */
        if (pgs->icc_manager->lab_profile != NULL) {
            gsicc_link_t *icc_link;
            gsicc_rendering_param_t rendering_params;
            unsigned short psrc[4];
            unsigned short pdes[3];

            rendering_params.black_point_comp = gsBLACKPTCOMP_ON;
            rendering_params.graphics_type_tag = GS_UNKNOWN_TAG;
            rendering_params.override_icc = false;
            rendering_params.preserve_black = gsBKPRESNOTSPECIFIED;
            rendering_params.rendering_intent = gsRELATIVECOLORIMETRIC;
            rendering_params.cmm = gsCMM_DEFAULT;

            icc_link = gsicc_get_link_profile(pgs, NULL, pcs->cmm_icc_profile_data,
                                          pgs->icc_manager->lab_profile, &rendering_params,
                                          pgs->memory, false);
            if (icc_link == NULL)
                return false;

            switch (pcs->cmm_icc_profile_data->data_cs) {
                case gsGRAY:
                    psrc[0] = (unsigned short)(pcc->paint.values[0] * 65535);
                    break;
                case gsRGB:
                    psrc[0] = (unsigned short)(pcc->paint.values[0] * 65535);
                    psrc[1] = (unsigned short)(pcc->paint.values[1] * 65535);
                    psrc[2] = (unsigned short)(pcc->paint.values[2] * 65535);
                    break;
                case gsCMYK:
                    psrc[0] = (unsigned short)(pcc->paint.values[0] * 65535);
                    psrc[1] = (unsigned short)(pcc->paint.values[1] * 65535);
                    psrc[2] = (unsigned short)(pcc->paint.values[2] * 65535);
                    psrc[3] = (unsigned short)(pcc->paint.values[3] * 65535);
                    break;
                default:
                    gsicc_release_link(icc_link);
                    return false;
            }
            (icc_link->procs.map_color)(NULL, icc_link, psrc, pdes, 2);
            gsicc_release_link(icc_link);

            Lstar = pdes[0] * 100.0 / 65535.0;
            astar = pdes[1] * 256.0 / 65535.0 - 128.0;
            bstar = pdes[2] * 256.0 / 65535.0 - 128.0;

            if (Lstar >= dev_profile->blackthresholdL &&
                fabs(astar) < dev_profile->blackthresholdC &&
                fabs(bstar) < dev_profile->blackthresholdC)
                return true;
            else
                return false;
        } else {
            /* Something to fall back on */
            switch (pcs->cmm_icc_profile_data->data_cs) {
                case gsGRAY:
                    if (pcc->paint.values[0] == 1.0)
                        return true;
                    else
                        return false;
                    break;
                case gsRGB:
                    if (pcc->paint.values[0] == 1.0 && pcc->paint.values[1] == 1.0 &&
                        pcc->paint.values[2] == 1.0)
                        return true;
                    else
                        return false;
                    break;
                case gsCMYK:
                    if (pcc->paint.values[0] == 0.0 && pcc->paint.values[1] == 0.0 &&
                        pcc->paint.values[2] == 0.0 && pcc->paint.values[3] == 0.0)
                        return true;
                    else
                        return false;
                    break;
                default:
                    return false;
            }
        }
    } else
        return false;
}

bool gsicc_setup_blacktextvec(gs_gstate *pgs, gx_device *dev, bool is_text)
{
    gs_color_space *pcs_curr = gs_currentcolorspace_inline(pgs);
    gs_color_space *pcs_alt = gs_swappedcolorspace_inline(pgs);

    /* If neither space is ICC then we are not doing anything */
    if (!gs_color_space_is_ICC(pcs_curr) && !gs_color_space_is_ICC(pcs_alt))
        return false;

    /* Create a new object to hold the cs details */
    pgs->black_textvec_state = gsicc_blacktextvec_state_new(pgs->memory, is_text);
    if (pgs->black_textvec_state == NULL)
        return false; /* No error just move on */

    /* If curr space is ICC then store it */
    if  (gs_color_space_is_ICC(pcs_curr)) {
        rc_increment_cs(pcs_curr);  /* We are storing the cs. Will decrement when structure is released */
        pgs->black_textvec_state->pcs = pcs_curr;
        pgs->black_textvec_state->pcc = pgs->color[0].ccolor;
        cs_adjust_color_count(pgs, 1); /* The set_gray will do a decrement, only need if pattern */
        pgs->black_textvec_state->value[0] = pgs->color[0].ccolor->paint.values[0];

        if (gsicc_is_white_blacktextvec(pgs, dev, pcs_curr, pgs->color[0].ccolor))
            gs_setgray(pgs, 1.0);
        else
            gs_setgray(pgs, 0.0);
    }

    /* If alt space is ICC then store it */
    if (gs_color_space_is_ICC(pcs_alt)) {
        rc_increment_cs(pcs_alt);  /* We are storing the cs. Will decrement when structure is released */
        pgs->black_textvec_state->pcs_alt = pcs_alt;

        gs_swapcolors_quick(pgs);  /* Have to swap for set_gray and adjust color count */
        pgs->black_textvec_state->pcc_alt = pgs->color[0].ccolor;
        cs_adjust_color_count(pgs, 1); /* The set_gray will do a decrement, only need if pattern */
        pgs->black_textvec_state->value[1] = pgs->color[0].ccolor->paint.values[0];

        if (gsicc_is_white_blacktextvec(pgs, dev, pcs_alt, pgs->color[0].ccolor))
            gs_setgray(pgs, 1.0);
        else
            gs_setgray(pgs, 0.0);
        gs_swapcolors_quick(pgs);
    }

    pgs->black_textvec_state->is_fill = pgs->is_fill_color;
    return true; /* Need to clean up */
}

void
gsicc_restore_blacktextvec(gs_gstate *pgs, bool is_text)
{
    gsicc_blacktextvec_state_t *state = pgs->black_textvec_state;
    int code;

    if (state == NULL)
        return;

    if (is_text != state->is_text)
        return;

    /* Make sure state and original are same fill_color condition */
    if (state->rc.ref_count == 1) {
        if ((state->is_fill && pgs->is_fill_color) || (!state->is_fill && !pgs->is_fill_color)) {
            if (pgs->black_textvec_state->pcs != NULL) {
                if ((code = gs_setcolorspace_only(pgs, pgs->black_textvec_state->pcs)) >= 0) {
                    /* current client color is gray.  no need to decrement */
                    pgs->color[0].ccolor = pgs->black_textvec_state->pcc;
                    pgs->color[0].ccolor->paint.values[0] = pgs->black_textvec_state->value[0];
                }
                gx_unset_dev_color(pgs);
            }
            if (pgs->black_textvec_state->pcs_alt != NULL) {
                gs_swapcolors_quick(pgs);
                if ((code = gs_setcolorspace_only(pgs, pgs->black_textvec_state->pcs_alt)) >= 0) {
                    pgs->color[0].ccolor = pgs->black_textvec_state->pcc_alt;
                    pgs->color[0].ccolor->paint.values[0] = pgs->black_textvec_state->value[1];
                }
                gs_swapcolors_quick(pgs);
                gx_unset_alt_dev_color(pgs);
            }
        } else {
            if (pgs->black_textvec_state->pcs_alt != NULL) {
                if ((code = gs_setcolorspace_only(pgs, pgs->black_textvec_state->pcs_alt)) >= 0) {
                    pgs->color[0].ccolor = pgs->black_textvec_state->pcc_alt;
                    pgs->color[0].ccolor->paint.values[0] = pgs->black_textvec_state->value[1];
                }
                gx_unset_dev_color(pgs);
            }
            if (pgs->black_textvec_state->pcs != NULL) {
                gs_swapcolors_quick(pgs);
                if ((code = gs_setcolorspace_only(pgs, pgs->black_textvec_state->pcs)) >= 0) {
                    pgs->color[0].ccolor = pgs->black_textvec_state->pcc;
                    pgs->color[0].ccolor->paint.values[0] = pgs->black_textvec_state->value[0];
                }
                gs_swapcolors_quick(pgs);
                gx_unset_alt_dev_color(pgs);
            }
        }
    }
    rc_decrement(state, "gsicc_restore_black_text");
    pgs->black_textvec_state = NULL;
}