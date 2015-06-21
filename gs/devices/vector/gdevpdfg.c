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


/* Graphics state management for pdfwrite driver */
#include "math_.h"
#include "string_.h"
#include "memory_.h"
#include "gx.h"
#include "gserrors.h"
#include "gsfunc0.h"
#include "gsstate.h"
#include "gxbitmap.h"		/* for gxhttile.h in gzht.h */
#include "gxdht.h"
#include "gxfarith.h"		/* for gs_sin/cos_degrees */
#include "gxfmap.h"
#include "gxht.h"
#include "gxistate.h"
#include "gxdcolor.h"
#include "gxpcolor.h"
#include "gsptype2.h"
#include "gzht.h"
#include "gdevpdfx.h"
#include "gdevpdfg.h"
#include "gdevpdfo.h"
#include "gscspace.h"
#include "gsicc_manage.h"
#include "gsicc_cache.h"
#include "gsccolor.h"
#include "gxcdevn.h"
#include "gscie.h"

/* ------ Exported by gdevpdfc.c for gdevpdfg.c ------ */
int pdf_make_sampled_base_space_function(gx_device_pdf *pdev, gs_function_t **pfn,
                                        int nSrcComp, int nDstComp, byte *data);
int pdf_delete_sampled_base_space_function(gx_device_pdf *pdev, gs_function_t *pfn);
int pdf_make_base_space_function(gx_device_pdf *pdev, gs_function_t **pfn,
                                        int ncomp, float *data_low, float *data_high);
int pdf_delete_base_space_function(gx_device_pdf *pdev, gs_function_t *pfn);

/* ---------------- Miscellaneous ---------------- */

/* Save the viewer's graphic state. */
int
pdf_save_viewer_state(gx_device_pdf *pdev, stream *s)
{
    const int i = pdev->vgstack_depth;

    if (pdev->vgstack_depth >= pdev->vgstack_size) {
        pdf_viewer_state *new_vgstack = (pdf_viewer_state *)gs_alloc_bytes(pdev->pdf_memory,
            (pdev->vgstack_size + 5) * sizeof(pdf_viewer_state), "increase graphics state stack size");
        if (new_vgstack == 0)
            return_error(gs_error_VMerror);
        memset(new_vgstack, 0x00, (pdev->vgstack_size + 5) * sizeof(pdf_viewer_state));
        memcpy(new_vgstack, pdev->vgstack, pdev->vgstack_size * sizeof(pdf_viewer_state));
        gs_free_object(pdev->pdf_memory, pdev->vgstack, "resize graphics state stack, free old stack)");
        pdev->vgstack = new_vgstack;
        pdev->vgstack_size += 5;
    }

    pdev->vgstack[i].transfer_ids[0] = pdev->transfer_ids[0];
    pdev->vgstack[i].transfer_ids[1] = pdev->transfer_ids[1];
    pdev->vgstack[i].transfer_ids[2] = pdev->transfer_ids[2];
    pdev->vgstack[i].transfer_ids[3] = pdev->transfer_ids[3];
    pdev->vgstack[i].transfer_not_identity = pdev->transfer_not_identity;
    pdev->vgstack[i].opacity_alpha = pdev->state.opacity.alpha;
    pdev->vgstack[i].shape_alpha = pdev->state.shape.alpha;
    pdev->vgstack[i].blend_mode = pdev->state.blend_mode;
    pdev->vgstack[i].halftone_id = pdev->halftone_id;
    pdev->vgstack[i].black_generation_id = pdev->black_generation_id;
    pdev->vgstack[i].undercolor_removal_id = pdev->undercolor_removal_id;
    pdev->vgstack[i].overprint_mode = pdev->overprint_mode;
    pdev->vgstack[i].smoothness = pdev->state.smoothness;
    pdev->vgstack[i].flatness = pdev->state.flatness;
    pdev->vgstack[i].text_knockout = pdev->state.text_knockout;
    pdev->vgstack[i].fill_overprint = pdev->fill_overprint;
    pdev->vgstack[i].stroke_overprint = pdev->stroke_overprint;
    pdev->vgstack[i].stroke_adjust = pdev->state.stroke_adjust;
    pdev->vgstack[i].fill_used_process_color = pdev->fill_used_process_color;
    pdev->vgstack[i].stroke_used_process_color = pdev->stroke_used_process_color;
    pdev->vgstack[i].saved_fill_color = pdev->saved_fill_color;
    pdev->vgstack[i].saved_stroke_color = pdev->saved_stroke_color;
    pdev->vgstack[i].line_params = pdev->state.line_params;
    pdev->vgstack[i].line_params.dash.pattern = 0; /* Use pdev->dash_pattern instead. */
    if (pdev->dash_pattern) {
        if (pdev->vgstack[i].dash_pattern)
            gs_free_object(pdev->memory->non_gc_memory, pdev->vgstack[i].dash_pattern, "free gstate copy dash");
        pdev->vgstack[i].dash_pattern = (float *)gs_alloc_bytes(pdev->memory->non_gc_memory, pdev->dash_pattern_size * sizeof(float), "gstate copy dash");
        memcpy(pdev->vgstack[i].dash_pattern, pdev->dash_pattern, pdev->dash_pattern_size * sizeof(float));
        pdev->vgstack[i].dash_pattern_size = pdev->dash_pattern_size;
    } else {
        if (pdev->vgstack[i].dash_pattern) {
            gs_free_object(pdev->memory->non_gc_memory, pdev->vgstack[i].dash_pattern, "free gstate copy dash");
            pdev->vgstack[i].dash_pattern = 0;
            pdev->vgstack[i].dash_pattern_size = 0;
        }
    }
    pdev->vgstack_depth++;
    if (s)
        stream_puts(s, "q\n");
    return 0;
}

/* Load the viewer's graphic state. */
static void
pdf_load_viewer_state(gx_device_pdf *pdev, pdf_viewer_state *s)
{
    pdev->transfer_ids[0] = s->transfer_ids[0];
    pdev->transfer_ids[1] = s->transfer_ids[1];
    pdev->transfer_ids[2] = s->transfer_ids[2];
    pdev->transfer_ids[3] = s->transfer_ids[3];
    pdev->transfer_not_identity = s->transfer_not_identity;
    pdev->state.opacity.alpha = s->opacity_alpha;
    pdev->state.shape.alpha = s->shape_alpha;
    pdev->state.blend_mode = s->blend_mode;
    pdev->halftone_id = s->halftone_id;
    pdev->black_generation_id = s->black_generation_id;
    pdev->undercolor_removal_id = s->undercolor_removal_id;
    pdev->overprint_mode = s->overprint_mode;
    pdev->state.smoothness = s->smoothness;
    pdev->state.flatness = s->flatness;
    pdev->state.text_knockout = s->text_knockout;
    pdev->fill_overprint = s->fill_overprint;
    pdev->stroke_overprint = s->stroke_overprint;
    pdev->state.stroke_adjust = s->stroke_adjust;
    pdev->fill_used_process_color = s->fill_used_process_color;
    pdev->stroke_used_process_color = s->stroke_used_process_color;
    pdev->saved_fill_color = s->saved_fill_color;
    pdev->saved_stroke_color = s->saved_stroke_color;
    pdev->state.line_params = s->line_params;
    if (s->dash_pattern) {
        if (pdev->dash_pattern)
            gs_free_object(pdev->memory->stable_memory, pdev->dash_pattern, "vector free dash pattern");
        pdev->dash_pattern = (float *)gs_alloc_bytes(pdev->memory->stable_memory, s->dash_pattern_size * sizeof(float), "vector allocate dash pattern");
        pdev->dash_pattern_size  = s->dash_pattern_size;
    } else {
        if (pdev->dash_pattern) {
            gs_free_object(pdev->memory->stable_memory, pdev->dash_pattern, "vector free dash pattern");
            pdev->dash_pattern = 0;
            pdev->dash_pattern_size = 0;
        }
    }
}

/* Restore the viewer's graphic state. */
int
pdf_restore_viewer_state(gx_device_pdf *pdev, stream *s)
{   const int i = --pdev->vgstack_depth;

    if (i < pdev->vgstack_bottom || i < 0)
        return_error(gs_error_unregistered); /* Must not happen. */
    if (s)
        stream_puts(s, "Q\n");
    pdf_load_viewer_state(pdev, pdev->vgstack + i);
    return 0;
}

/* Set initial color. */
void
pdf_set_initial_color(gx_device_pdf * pdev, gx_hl_saved_color *saved_fill_color,
                    gx_hl_saved_color *saved_stroke_color,
                    bool *fill_used_process_color, bool *stroke_used_process_color)
{
    gx_device_color black;

    pdev->black = gx_device_black((gx_device *)pdev);
    pdev->white = gx_device_white((gx_device *)pdev);
    set_nonclient_dev_color(&black, pdev->black);
    gx_hld_save_color(NULL, &black, saved_fill_color);
    gx_hld_save_color(NULL, &black, saved_stroke_color);
    *fill_used_process_color = true;
    *stroke_used_process_color = true;
}

/* Prepare intitial values for viewer's graphics state parameters. */
static void
pdf_viewer_state_from_imager_state_aux(pdf_viewer_state *pvs, const gs_imager_state *pis)
{
    pvs->transfer_not_identity =
            (pis->set_transfer.red   != NULL ? pis->set_transfer.red->proc   != gs_identity_transfer : 0) * 1 +
            (pis->set_transfer.green != NULL ? pis->set_transfer.green->proc != gs_identity_transfer : 0) * 2 +
            (pis->set_transfer.blue  != NULL ? pis->set_transfer.blue->proc  != gs_identity_transfer : 0) * 4 +
            (pis->set_transfer.gray  != NULL ? pis->set_transfer.gray->proc  != gs_identity_transfer : 0) * 8;
    pvs->transfer_ids[0] = (pis->set_transfer.red != NULL ? pis->set_transfer.red->id : 0);
    pvs->transfer_ids[1] = (pis->set_transfer.green != NULL ? pis->set_transfer.green->id : 0);
    pvs->transfer_ids[2] = (pis->set_transfer.blue != NULL ? pis->set_transfer.blue->id : 0);
    pvs->transfer_ids[3] = (pis->set_transfer.gray != NULL ? pis->set_transfer.gray->id : 0);
    pvs->opacity_alpha = pis->opacity.alpha;
    pvs->shape_alpha = pis->shape.alpha;
    pvs->blend_mode = pis->blend_mode;
    pvs->halftone_id = (pis->dev_ht != 0 ? pis->dev_ht->id : 0);
    pvs->black_generation_id = (pis->black_generation != 0 ? pis->black_generation->id : 0);
    pvs->undercolor_removal_id = (pis->undercolor_removal != 0 ? pis->undercolor_removal->id : 0);
    pvs->overprint_mode = 0;
    pvs->flatness = pis->flatness;
    pvs->smoothness = pis->smoothness;
    pvs->text_knockout = pis->text_knockout;
    pvs->fill_overprint = false;
    pvs->stroke_overprint = false;
    pvs->stroke_adjust = false;
    pvs->line_params.half_width = 0.5;
    pvs->line_params.start_cap = 0;
    pvs->line_params.end_cap = 0;
    pvs->line_params.dash_cap = 0;
    pvs->line_params.join = 0;
    pvs->line_params.curve_join = 0;
    pvs->line_params.miter_limit = 10.0;
    pvs->line_params.miter_check = 0;
    pvs->line_params.dot_length = pis->line_params.dot_length;
    pvs->line_params.dot_length_absolute = pis->line_params.dot_length_absolute;
    pvs->line_params.dot_orientation = pis->line_params.dot_orientation;
    memset(&pvs->line_params.dash, 0 , sizeof(pvs->line_params.dash));
    pvs->dash_pattern = 0;
    pvs->dash_pattern_size = 0;
}

/* Copy viewer state from images state. */
void
pdf_viewer_state_from_imager_state(gx_device_pdf * pdev,
        const gs_imager_state *pis, const gx_device_color *pdevc)
{
    pdf_viewer_state vs;

    pdf_viewer_state_from_imager_state_aux(&vs, pis);
    gx_hld_save_color(pis, pdevc, &vs.saved_fill_color);
    gx_hld_save_color(pis, pdevc, &vs.saved_stroke_color);
    vs.fill_used_process_color = 0;
    vs.stroke_used_process_color = 0;
    pdf_load_viewer_state(pdev, &vs);
}

/* Prepare intitial values for viewer's graphics state parameters. */
void
pdf_prepare_initial_viewer_state(gx_device_pdf * pdev, const gs_imager_state *pis)
{
    /* Parameter values, which are specified in PDF spec, are set here.
     * Parameter values, which are specified in PDF spec as "installation dependent",
     * are set here to intial values used with PS interpreter.
     * This allows to write differences to the output file
     * and skip initial values.
     */

    pdf_set_initial_color(pdev, &pdev->vg_initial.saved_fill_color, &pdev->vg_initial.saved_stroke_color,
            &pdev->vg_initial.fill_used_process_color, &pdev->vg_initial.stroke_used_process_color);
    pdf_viewer_state_from_imager_state_aux(&pdev->vg_initial, pis);
    pdev->vg_initial_set = true;
    /*
     * Some parameters listed in PDF spec are missed here :
     * text state - it is initialized per page.
     * rendering intent - not sure why, fixme.
     */
}

/* Reset the graphics state parameters to initial values. */
/* Used if pdf_prepare_initial_viewer_state was not callad. */
static void
pdf_reset_graphics_old(gx_device_pdf * pdev)
{

    pdf_set_initial_color(pdev, &pdev->saved_fill_color, &pdev->saved_stroke_color,
                                &pdev->fill_used_process_color, &pdev->stroke_used_process_color);
    pdev->state.flatness = -1;
    {
        static const gx_line_params lp_initial = {
            gx_line_params_initial
        };

        pdev->state.line_params = lp_initial;
    }
    pdev->fill_overprint = false;
    pdev->stroke_overprint = false;
    pdev->remap_fill_color = false;
    pdev->remap_stroke_color = false;
    pdf_reset_text(pdev);
}

/* Reset the graphics state parameters to initial values. */
void
pdf_reset_graphics(gx_device_pdf * pdev)
{
    if (pdev->vg_initial_set)
        pdf_load_viewer_state(pdev, &pdev->vg_initial);
    else
        pdf_reset_graphics_old(pdev);
    pdf_reset_text(pdev);
}

/* Write client color. */
static int
pdf_write_ccolor(gx_device_pdf * pdev, const gs_imager_state * pis,
                const gs_client_color *pcc)
{
    int i, n = gx_hld_get_number_color_components(pis);

    pprintg1(pdev->strm, "%g", psdf_round(pcc->paint.values[0], 255, 8));
    for (i = 1; i < n; i++) {
        pprintg1(pdev->strm, " %g", psdf_round(pcc->paint.values[i], 255, 8));
    }
    return 0;
}

static inline bool
is_cspace_allowed_in_strategy(gx_device_pdf * pdev, gs_color_space_index csi)
{
    if (pdev->params.ColorConversionStrategy == ccs_CMYK &&
            csi != gs_color_space_index_DeviceCMYK &&
            csi != gs_color_space_index_DeviceGray)
        return false;
    if (pdev->params.ColorConversionStrategy == ccs_sRGB &&
            csi != gs_color_space_index_DeviceRGB &&
            csi != gs_color_space_index_DeviceGray)
        return false;
    if (pdev->params.ColorConversionStrategy == ccs_RGB &&
            csi != gs_color_space_index_DeviceRGB &&
            csi != gs_color_space_index_DeviceGray)
        return false;
    if (pdev->params.ColorConversionStrategy == ccs_Gray &&
            csi != gs_color_space_index_DeviceGray)
        return false;
    return true;
}

static inline bool
is_pattern2_allowed_in_strategy(gx_device_pdf * pdev, const gx_drawing_color *pdc)
{
    const gs_color_space *pcs2 = gx_dc_pattern2_get_color_space(pdc);
    gs_color_space_index csi = gs_color_space_get_index(pcs2);

    return is_cspace_allowed_in_strategy(pdev, csi);
}


static int write_color_as_process(gx_device_pdf * pdev, const gs_imager_state * pis, const gs_color_space *pcs,
                        const gx_drawing_color *pdc, bool *used_process_color,
                        const psdf_set_color_commands_t *ppscc, gs_client_color *pcc)
{
    int code, i;
    frac conc[GS_CLIENT_COLOR_MAX_COMPONENTS];
    gs_color_space_index csi, csi2;
    gs_color_space *pcs2 = (gs_color_space *)pcs;
    gx_drawing_color dc;
    int num_des_comps;
    cmm_dev_profile_t *dev_profile;

    dc.type = gx_dc_type_pure;
    dc.colors.pure = 0;
    csi = gs_color_space_get_index(pcs);

    if (csi == gs_color_space_index_Indexed ||
        csi == gs_color_space_index_DeviceN ||
        csi == gs_color_space_index_Separation) {
        const char *command = NULL;

        *used_process_color = true;

        memset (&conc, 0x00, sizeof(frac) * GS_CLIENT_COLOR_MAX_COMPONENTS);
        pcs->type->concretize_color(pcc, pcs, conc, pis, (gx_device *)pdev);

        do{
            pcs2 = pcs2->base_space;
            csi2 = gs_color_space_get_index(pcs2);
        } while(csi2 != gs_color_space_index_ICC && pcs2->base_space);
        csi2 = gs_color_space_get_index(pcs2);

        switch (csi2) {
            case gs_color_space_index_DeviceGray:
            case gs_color_space_index_DeviceRGB:
            case gs_color_space_index_DeviceCMYK:
                switch (pdev->color_info.num_components) {
                    case 1:
                        command = ppscc->setgray;
                        break;
                    case 3:
                        command = ppscc->setrgbcolor;
                        break;
                    case 4:
                        command = ppscc->setcmykcolor;
                        break;
                    default:
                        /* Can't happen since we already check the colour space */
                        return gs_error_rangecheck;
                }
                pprintg1(pdev->strm, "%g", psdf_round(frac2float(conc[0]), 255, 8));
                for (i = 1; i < pdev->color_info.num_components; i++) {
                    pprintg1(pdev->strm, " %g", psdf_round(frac2float(conc[i]), 255, 8));
                }
                pprints1(pdev->strm, " %s\n", command);
                return 0;
                break;
            case gs_color_space_index_CIEDEFG:
            case gs_color_space_index_CIEDEF:
            case gs_color_space_index_CIEABC:
            case gs_color_space_index_CIEA:
            case gs_color_space_index_ICC:
                code = dev_proc((gx_device *)pdev, get_profile)((gx_device *)pdev, &dev_profile);
                if (code < 0)
                    return code;
                num_des_comps = gsicc_get_device_profile_comps(dev_profile);
                for (i = 0;i < num_des_comps;i++)
                    dc.colors.pure = (dc.colors.pure << 8) + (int)(frac2float(conc[i]) * 255);
                code = psdf_set_color((gx_device_vector *)pdev, &dc, ppscc, pdev->UseOldColor);
                return code;
                break;
            default:    /* can't happen, simply silences compiler warnings */
                break;
        }
        pcs = pcs2;
    } else {
        if (csi >= gs_color_space_index_CIEDEFG &&
            csi <= gs_color_space_index_CIEA) {
                memset (&conc, 0x00, sizeof(frac) * GS_CLIENT_COLOR_MAX_COMPONENTS);
                pcs->type->concretize_color(pcc, pcs, conc, pis, (gx_device *)pdev);
                code = dev_proc((gx_device *)pdev, get_profile)((gx_device *)pdev, &dev_profile);
                if (code < 0)
                    return code;
                num_des_comps = gsicc_get_device_profile_comps(dev_profile);
                for (i = 0;i < num_des_comps;i++)
                    dc.colors.pure = (dc.colors.pure << 8) + (int)(frac2float(conc[i]) * 255);
                code = psdf_set_color((gx_device_vector *)pdev, &dc, ppscc, pdev->UseOldColor);
                *used_process_color = true;
                return code;
        } else {
            memset (&conc, 0x00, sizeof(frac) * GS_CLIENT_COLOR_MAX_COMPONENTS);
            /* Special case handling for Lab spaces */
            if (pcs->cmm_icc_profile_data->data_cs == gsCIELAB || pcs->cmm_icc_profile_data->islab) {
                gs_client_color cc;
                /* Get the data in a form that is concrete for the CMM */
                cc.paint.values[0] = pcc->paint.values[0] / 100.0;
                cc.paint.values[1] = (pcc->paint.values[1]+128)/255.0;
                cc.paint.values[2] = (pcc->paint.values[2]+128)/255.0;
                pcs->type->concretize_color((const gs_client_color *)&cc, pcs, conc, pis, (gx_device *)pdev);
            } else
                pcs->type->concretize_color(pcc, pcs, conc, pis, (gx_device *)pdev);
            code = dev_proc((gx_device *)pdev, get_profile)((gx_device *)pdev, &dev_profile);
            if (code < 0)
                return code;
            num_des_comps = gsicc_get_device_profile_comps(dev_profile);
            for (i = 0;i < num_des_comps;i++)
                dc.colors.pure = (dc.colors.pure << 8) + (int)(frac2float(conc[i]) * 255);
            code = psdf_set_color((gx_device_vector *)pdev, &dc, ppscc, pdev->UseOldColor);
            return code;
        }
    }
/*    rendering_params.black_point_comp = pis->blackptcomp;
    rendering_params.graphics_type_tag = pdev->graphics_type_tag;
    rendering_params.override_icc = false;
    rendering_params.preserve_black = gsBKPRESNOTSPECIFIED;
    rendering_params.rendering_intent = pis->renderingintent;
    rendering_params.cmm = gsCMM_DEFAULT;
    icc_link = gsicc_get_link(pis, (gx_device *)pdev, pcs,
                                      NULL, &rendering_params,
                                      pis->memory);
    (icc_link->procs.map_color)((gx_device *)pdev, icc_link, Source, Converted, 2);
    gsicc_release_link(icc_link);
    ((gx_drawing_color *)pdc)->colors.pure = 0;
    for (i = 0;i < pdev->color_info.num_components;i++)
         dc.colors.pure = (dc.colors.pure << 8) + (Converted[i] / 256);
    code = psdf_set_color((gx_device_vector *)pdev, &dc, ppscc, pdev->UseOldColor);
    if (code < 0)
        return code;
    *used_process_color = true;
    return 0;*/
    return gs_error_unknownerror;
}

static int write_color_unchanged(gx_device_pdf * pdev, const gs_imager_state * pis,
                          gs_client_color *pcc, gx_hl_saved_color *current,
                          gx_hl_saved_color * psc, const psdf_set_color_commands_t *ppscc,
                          bool *used_process_color, const gs_color_space *pcs,
                          const gx_drawing_color *pdc)
{
    gs_color_space_index csi, csi2;
    int code;
    const char *command = NULL;
    gs_range_t *ranges = 0;

    csi = csi2 = gs_color_space_get_index(pcs);
    if (csi == gs_color_space_index_ICC) {
        csi2 = gsicc_get_default_type(pcs->cmm_icc_profile_data);
    }

    switch (csi2) {
        case gs_color_space_index_DeviceGray:
            command = ppscc->setgray;
            code = pdf_write_ccolor(pdev, pis, pcc);
            if (code < 0)
                return code;
            pprints1(pdev->strm, " %s\n", command);
            break;
        case gs_color_space_index_DeviceRGB:
            command = ppscc->setrgbcolor;
            code = pdf_write_ccolor(pdev, pis, pcc);
            if (code < 0)
                return code;
            pprints1(pdev->strm, " %s\n", command);
            break;
        case gs_color_space_index_DeviceCMYK:
            command = ppscc->setcmykcolor;
            code = pdf_write_ccolor(pdev, pis, pcc);
            if (code < 0)
                return code;
            pprints1(pdev->strm, " %s\n", command);
            break;
        default:
            csi = gs_color_space_get_index(pcs);
            if (!gx_hld_saved_color_same_cspace(current, psc) || (csi2 >= gs_color_space_index_CIEDEFG && csi2 <= gs_color_space_index_CIEA)) {
                cos_value_t cs_value;

                code = pdf_color_space_named(pdev, pis, &cs_value, (const gs_range_t **)&ranges, pcs,
                                &pdf_color_space_names, true, NULL, 0, false);
                /* fixme : creates redundant PDF objects. */
                if (code == gs_error_rangecheck) {
                    *used_process_color = true;
                    if (pdev->ForOPDFRead) {
                    int save = 0;
                    /* The color space can't write to PDF. This should never happen */
                    save = pdev->UseOldColor;
                    pdev->UseOldColor = 1;
                    code = psdf_set_color((gx_device_vector *)pdev, pdc, ppscc, pdev->UseOldColor);
                    pdev->UseOldColor = save;
                    } else {
                    code = write_color_as_process(pdev, pis, pcs, pdc, used_process_color, ppscc, pcc);
                    }
                    return code;
                }
                if (code < 0)
                    return code;
                code = cos_value_write(&cs_value, pdev);
                if (code < 0)
                    return code;
                pprints1(pdev->strm, " %s\n", ppscc->setcolorspace);
                if (ranges && (csi2 >= gs_color_space_index_CIEDEFG && csi2 <= gs_color_space_index_CIEA)) {
                    gs_client_color dcc = *pcc;
                    switch (csi2) {
                        case gs_color_space_index_CIEDEFG:
                            rescale_cie_color(ranges, 4, pcc, &dcc);
                            break;
                        case gs_color_space_index_CIEDEF:
                            rescale_cie_color(ranges, 3, pcc, &dcc);
                            break;
                        case gs_color_space_index_CIEABC:
                            rescale_cie_color(ranges, 3, pcc, &dcc);
                            break;
                        case gs_color_space_index_CIEA:
                            rescale_cie_color(ranges, 1, pcc, &dcc);
                            break;
                        default:
                            /* can't happen but silences compiler warnings */
                            break;
                    }
                    code = pdf_write_ccolor(pdev, pis, &dcc);
                } else {
                    code = pdf_write_ccolor(pdev, pis, pcc);
                }
                *used_process_color = false;
                if (code < 0)
                    return code;
                pprints1(pdev->strm, " %s\n", ppscc->setcolorn);
            } else if (*used_process_color) {
                    *used_process_color = true;
                    if (pdev->ForOPDFRead) {
                    int save = 0;
                    /* The color space can't write to PDF. This should never happen */
                    save = pdev->UseOldColor;
                    pdev->UseOldColor = 1;
                    code = psdf_set_color((gx_device_vector *)pdev, pdc, ppscc, pdev->UseOldColor);
                    pdev->UseOldColor = save;
                    } else {
                    code = write_color_as_process(pdev, pis, pcs, pdc, used_process_color, ppscc, pcc);
                    }
                    return code;
            }
            else {
                code = pdf_write_ccolor(pdev, pis, pcc);
                if (code < 0)
                    return code;
                pprints1(pdev->strm, " %s\n", ppscc->setcolorn);
            }
            break;
    }
    *used_process_color = false;

    return 0;
}

static int write_color_as_process_ICC(gx_device_pdf * pdev, const gs_imager_state * pis, const gs_color_space *pcs,
                        const gx_drawing_color *pdc, gx_hl_saved_color * psc, bool *used_process_color,
                        const psdf_set_color_commands_t *ppscc, gs_client_color *pcc,
                        gx_hl_saved_color *current)
{
    int i, code;
    cos_value_t cs_value;

    if (!gx_hld_saved_color_same_cspace(current, psc)) {
        code = pdf_color_space_named(pdev, pis, &cs_value, NULL, pcs,
                        &pdf_color_space_names, true, NULL, 0, true);
        /* fixme : creates redundant PDF objects. */
        if (code == gs_error_rangecheck) {
            /* The color space can't write to PDF. This should never happen */
            return write_color_as_process(pdev, pis, pcs, pdc, used_process_color, ppscc, pcc);
        }
        if (code < 0)
            return code;
        code = cos_value_write(&cs_value, pdev);
        if (code < 0)
            return code;
        pprints1(pdev->strm, " %s\n", ppscc->setcolorspace);
        *used_process_color = false;
        pprintg1(pdev->strm, "%g", psdf_round(pcc->paint.values[0], 255, 8));
        for (i = 1; i < pcs->type->num_components(pcs); i++) {
            pprintg1(pdev->strm, " %g", psdf_round(pcc->paint.values[i], 255, 8));
        }
        pprints1(pdev->strm, " %s\n", ppscc->setcolorn);
    } else {
        *used_process_color = false;
        pprintg1(pdev->strm, "%g", psdf_round(pcc->paint.values[0], 255, 8));
        for (i = 1; i < pcs->type->num_components(pcs); i++) {
            pprintg1(pdev->strm, " %g", psdf_round(pcc->paint.values[i], 255, 8));
        }
        pprints1(pdev->strm, " %s\n", ppscc->setcolorn);
    }
    return 0;
}

int convert_DeviceN_alternate(gx_device_pdf * pdev, const gs_imager_state * pis, const gs_color_space *pcs,
                        const gx_drawing_color *pdc, bool *used_process_color,
                        const psdf_set_color_commands_t *ppscc, gs_client_color *pcc, cos_value_t *pvalue, bool by_name)
{
    gs_color_space_index csi;
    gs_function_t *new_pfn = 0;
    int code, i, samples=0, loop;
    cos_array_t *pca, *pca1;
    cos_value_t v;
    byte *data_buff;
    pdf_resource_t *pres = NULL;
    gs_color_space *pcs_save = NULL;

    csi = gs_color_space_get_index(pcs);
    if (csi == gs_color_space_index_Indexed) {
        pcs_save = (gs_color_space *)pcs;
        pcs = pcs->base_space;
    }

    pca = cos_array_alloc(pdev, "pdf_color_space");
    if (pca == 0)
        return_error(gs_error_VMerror);

    samples = (unsigned int)pow(2, pcs->params.device_n.num_components);
    data_buff = gs_alloc_bytes(pdev->memory, pdev->color_info.num_components * samples, "Convert DeviceN");
    if (data_buff == 0) {
        COS_FREE(pca, "convert DeviceN");
        return_error(gs_error_VMerror);
    }
    memset(data_buff, 0x00, pdev->color_info.num_components * samples);

    {
        frac conc[GS_CLIENT_COLOR_MAX_COMPONENTS];
        gs_client_color cc;
        int i;
        gs_color_space *icc_space = (gs_color_space *)pcs;
        gs_color_space *sep_space = (gs_color_space *)pcs;
        gs_color_space_index csi2;

        csi = gs_color_space_get_index(pcs);
        if (csi == gs_color_space_index_Indexed)
            sep_space = pcs->base_space;

        do{
            icc_space = icc_space->base_space;
            csi2 = gs_color_space_get_index(icc_space);
        } while(csi2 != gs_color_space_index_ICC && icc_space->base_space);

        memset(&cc.paint.values, 0x00, GS_CLIENT_COLOR_MAX_COMPONENTS);

        for (loop=0;loop < samples;loop++) {
            if (loop > 0) {
                if (cc.paint.values[0] == 0)
                    cc.paint.values[0] = 1;
                else {
                    int cascade = 0;
                    while (cc.paint.values[cascade] == 1 && cascade < samples) {
                        cc.paint.values[cascade++] = 0;
                    }
                    cc.paint.values[cascade] = 1;
                }
            }


            memset (&conc, 0x00, sizeof(frac) * GS_CLIENT_COLOR_MAX_COMPONENTS);
            sep_space->type->concretize_color(&cc, sep_space, conc, pis, (gx_device *)pdev);
            for (i = 0;i < pdev->color_info.num_components;i++)
                data_buff[(loop * pdev->color_info.num_components) + i] = (int)(frac2float(conc[i]) * 255);
        }
    }

    switch(pdev->params.ColorConversionStrategy) {
        case ccs_Gray:
            code = pdf_make_sampled_base_space_function(pdev, &new_pfn, pcs->params.device_n.num_components, 1, data_buff);
            break;
        case ccs_sRGB:
        case ccs_RGB:
            code = pdf_make_sampled_base_space_function(pdev, &new_pfn, pcs->params.device_n.num_components, 3, data_buff);
            break;
        case ccs_CMYK:
            code = pdf_make_sampled_base_space_function(pdev, &new_pfn, pcs->params.device_n.num_components, 4, data_buff);
            break;
        default:
            code = gs_error_rangecheck;
            break;
    }
    gs_free_object(pdev->memory, data_buff, "Convert DeviceN");
    if (code < 0) {
        COS_FREE(pca, "convert DeviceN");
        return code;
    }

    code = cos_array_add(pca, cos_c_string_value(&v, "/DeviceN"));
    if (code < 0) {
        COS_FREE(pca, "pdf_color_space");
        return code;
    }

    if (code >= 0) {
        byte *name_string;
        uint name_string_length;
        cos_value_t v_attriburtes, *va = NULL;
        cos_array_t *psna =
                cos_array_alloc(pdev, "pdf_color_space(DeviceN)");

        if (psna == 0) {
            COS_FREE(pca, "convert DeviceN");
            return_error(gs_error_VMerror);
        }

        for (i = 0; i < pcs->params.device_n.num_components; ++i) {
            code = pcs->params.device_n.get_colorname_string(
                              pdev->memory,
                              pcs->params.device_n.names[i], &name_string,
                              &name_string_length);
            if (code < 0) {
                COS_FREE(pca, "convert DeviceN");
                return code;
            }
            code = pdf_string_to_cos_name(pdev, name_string,
                              name_string_length, &v);
            if (code < 0) {
                COS_FREE(pca, "convert DeviceN");
                return code;
            }
            code = cos_array_add(psna, &v);
            if (code < 0) {
                COS_FREE(pca, "convert DeviceN");
                return code;
            }
        }
        COS_OBJECT_VALUE(&v, psna);
        code = cos_array_add(pca, &v);
        if (code <0) {
            COS_FREE(pca, "convert DeviceN");
            return_error(gs_error_VMerror);
        }

        if (pcs->params.device_n.colorants != NULL) {
            cos_dict_t *colorants  = cos_dict_alloc(pdev, "pdf_color_space(DeviceN)");
            cos_value_t v_colorants, v_separation, v_colorant_name;
            const gs_device_n_attributes *csa;
            pdf_resource_t *pres_attributes;

            if (colorants == NULL)
                return_error(gs_error_VMerror);
            code = pdf_alloc_resource(pdev, resourceOther, 0, &pres_attributes, -1);
            if (code < 0) {
                COS_FREE(pca, "convert DeviceN");
                return code;
            }
            cos_become(pres_attributes->object, cos_type_dict);
            COS_OBJECT_VALUE(&v_colorants, colorants);
            code = cos_dict_put((cos_dict_t *)pres_attributes->object,
                (const byte *)"/Colorants", 10, &v_colorants);
            if (code < 0){
                COS_FREE(pca, "convert DeviceN");
                return code;
            }
            for (csa = pcs->params.device_n.colorants; csa != NULL; csa = csa->next) {
                code = pcs->params.device_n.get_colorname_string(pdev->memory,
                              csa->colorant_name, &name_string, &name_string_length);
                if (code < 0) {
                    COS_FREE(pca, "convert DeviceN");
                    return code;
                }
                code = pdf_color_space_named(pdev, pis, &v_separation, NULL, csa->cspace, &pdf_color_space_names, false, NULL, 0, false);
                if (code < 0) {
                    COS_FREE(pca, "convert DeviceN");
                    return code;
                }
                code = pdf_string_to_cos_name(pdev, name_string, name_string_length, &v_colorant_name);
                if (code < 0) {
                    COS_FREE(pca, "convert DeviceN");
                    return code;
                }
                code = cos_dict_put(colorants, v_colorant_name.contents.chars.data,
                                    v_colorant_name.contents.chars.size, &v_separation);
                if (code < 0) {
                    COS_FREE(pca, "convert DeviceN");
                    return code;
                }
            }
            code = pdf_substitute_resource(pdev, &pres_attributes, resourceOther, NULL, true);
            if (code < 0) {
                COS_FREE(pca, "convert DeviceN");
                return code;
            }
            pres_attributes->where_used |= pdev->used_mask;
            va = &v_attriburtes;
            COS_OBJECT_VALUE(va, pres_attributes->object);
            code = cos_array_add(pca, va);
            if (code < 0) {
                COS_FREE(pca, "convert DeviceN");
                return code;
            }
        }

        switch(pdev->params.ColorConversionStrategy) {
            case ccs_Gray:
                cos_c_string_value(&v, (const char *)pdf_color_space_names.DeviceGray);
                break;
            case ccs_sRGB:
            case ccs_RGB:
                cos_c_string_value(&v, (const char *)pdf_color_space_names.DeviceRGB);
                break;
            case ccs_CMYK:
                cos_c_string_value(&v, (const char *)pdf_color_space_names.DeviceCMYK);
                break;
            default:
                break;
        }
        code = cos_array_add(pca, &v);
        if (code >= 0) {
            code = pdf_function_scaled(pdev, new_pfn, 0x00, &v);
            if (code >= 0)
                code = cos_array_add(pca, &v);
            else {
                COS_FREE(pca, "convert DeviceN");
                return code;
            }
        }
    }
    pdf_delete_sampled_base_space_function(pdev, new_pfn);

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
        ppcs = (pdf_color_space_t *)pres;
        ppcs->serialized = NULL;
        ppcs->serialized_size = 0;

        ppcs->ranges = 0;
        pca->id = pres->object->id;
        COS_FREE(pres->object, "pdf_color_space");
        pres->object = (cos_object_t *)pca;
        cos_write_object(COS_OBJECT(pca), pdev, resourceColorSpace);
        csi = gs_color_space_get_index(pcs);
        if (pcs_save == NULL && ppscc != NULL)
            pprints1(pdev->strm, "/%s", ppcs->rname);
    }
    if (pres != NULL) {
        pres->where_used |= pdev->used_mask;
        code = pdf_add_resource(pdev, pdev->substream_Resources, "/ColorSpace", pres);
        if (code < 0)
            return code;
    }

    if (pcs_save != NULL) {
        cos_value_t value;

        pcs = pcs_save;
        discard(COS_OBJECT_VALUE(&value, pca));
        pca1 = cos_array_alloc(pdev, "pdf_color_space");
        code = pdf_indexed_color_space(pdev, pis, &value, pcs, pca1, (cos_value_t *)&value);
        pca = pca1;

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
            ppcs = (pdf_color_space_t *)pres;
            ppcs->serialized = NULL;
            ppcs->serialized_size = 0;

            ppcs->ranges = 0;
            pca->id = pres->object->id;
            COS_FREE(pres->object, "pdf_color_space");
            pres->object = (cos_object_t *)pca;
            cos_write_object(COS_OBJECT(pca), pdev, resourceColorSpace);
            if (ppscc != NULL)
                pprints1(pdev->strm, "/%s", ppcs->rname);
        }
        if (pres != NULL) {
            pres->where_used |= pdev->used_mask;
            code = pdf_add_resource(pdev, pdev->substream_Resources, "/ColorSpace", pres);
            if (code < 0)
                return code;
        }
    }

    if (ppscc != NULL) {
        pprints1(pdev->strm, " %s\n", ppscc->setcolorspace);
        *used_process_color = false;
        if (pcs_save == NULL) {
            for (i = 0; i < pcs->params.device_n.num_components; ++i)
                pprintg1(pdev->strm, "%g ", psdf_round(pcc->paint.values[i], 255, 8));
        } else
            pprintg1(pdev->strm, "%g ", psdf_round(pcc->paint.values[0], 255, 8));
        pprints1(pdev->strm, "%s\n", ppscc->setcolorn);
    }
    if (pvalue != NULL) {
        if (by_name) {
            /* Return a resource name rather than an object reference. */
            discard(COS_RESOURCE_VALUE(pvalue, pca));
        } else
            discard(COS_OBJECT_VALUE(pvalue, pca));
    }
    return 0;
}

int convert_separation_alternate(gx_device_pdf * pdev, const gs_imager_state * pis, const gs_color_space *pcs,
                        const gx_drawing_color *pdc, bool *used_process_color,
                        const psdf_set_color_commands_t *ppscc, gs_client_color *pcc, cos_value_t *pvalue, bool by_name)
{
    gs_color_space_index csi;
    gs_function_t *new_pfn = 0;
    float out_low[4];
    float out_high[4];
    int code;
    cos_array_t *pca, *pca1;
    cos_value_t v;
    byte *name_string;
    uint name_string_length;
    pdf_resource_t *pres = NULL;

    pca = cos_array_alloc(pdev, "pdf_color_space");
    if (pca == 0)
        return_error(gs_error_VMerror);

    {
    frac conc[GS_CLIENT_COLOR_MAX_COMPONENTS];
    gs_client_color cc;
    int i;
    gs_color_space *icc_space = (gs_color_space *)pcs;
    gs_color_space *sep_space = (gs_color_space *)pcs;
    gs_color_space_index csi2;

        csi = gs_color_space_get_index(pcs);
        if (csi == gs_color_space_index_Indexed)
            sep_space = pcs->base_space;

        do{
            icc_space = icc_space->base_space;
            csi2 = gs_color_space_get_index(icc_space);
        } while(csi2 != gs_color_space_index_ICC && icc_space->base_space);

        memset(&cc.paint.values, 0x00, GS_CLIENT_COLOR_MAX_COMPONENTS);
        cc.paint.values[0] = 0;
        memset (&conc, 0x00, sizeof(frac) * GS_CLIENT_COLOR_MAX_COMPONENTS);
        sep_space->type->concretize_color(&cc, sep_space, conc, pis, (gx_device *)pdev);
        for (i = 0;i < pdev->color_info.num_components;i++)
            out_low[i] = frac2float(conc[i]);

        cc.paint.values[0] = 1;
        memset (&conc, 0x00, sizeof(frac) * GS_CLIENT_COLOR_MAX_COMPONENTS);
        sep_space->type->concretize_color(&cc, sep_space, conc, pis, (gx_device *)pdev);
        for (i = 0;i < pdev->color_info.num_components;i++)
            out_high[i] = frac2float(conc[i]);
    }
    switch(pdev->params.ColorConversionStrategy) {
        case ccs_Gray:
            code = pdf_make_base_space_function(pdev, &new_pfn, 1, out_low, out_high);
            break;
        case ccs_sRGB:
        case ccs_RGB:
            code = pdf_make_base_space_function(pdev, &new_pfn, 3, out_low, out_high);
            break;
        case ccs_CMYK:
            code = pdf_make_base_space_function(pdev, &new_pfn, 4, out_low, out_high);
            break;
        default:
            code = gs_error_rangecheck;
            break;
    }

    if (code < 0) {
        COS_FREE(pca, "pdf_color_space");
        return code;
    }

    code = cos_array_add(pca, cos_c_string_value(&v, "/Separation"));
    if (code < 0) {
        COS_FREE(pca, "pdf_color_space");
        return code;
    }

    if (code >= 0) {
        csi = gs_color_space_get_index(pcs);
        if (csi == gs_color_space_index_Indexed)
            code = pcs->base_space->params.separation.get_colorname_string(
                          pdev->memory,
                          pcs->base_space->params.separation.sep_name, &name_string,
                          &name_string_length);
        else
            code = pcs->params.separation.get_colorname_string(
                          pdev->memory,
                          pcs->params.separation.sep_name, &name_string,
                          &name_string_length);
        if (code < 0) {
            COS_FREE(pca, "pdf_color_space");
            return code;
        }

        code = pdf_string_to_cos_name(pdev, name_string,
                              name_string_length, &v);
        if (code < 0) {
            COS_FREE(pca, "pdf_color_space");
            return code;
        }

        code = cos_array_add(pca, &v);
        if (code < 0) {
            COS_FREE(pca, "pdf_color_space");
            return code;
        }
        if (code >= 0) {
            switch(pdev->params.ColorConversionStrategy) {
                case ccs_Gray:
                    cos_c_string_value(&v, (const char *)pdf_color_space_names.DeviceGray);
                    break;
                case ccs_RGB:
                case ccs_sRGB:
                    cos_c_string_value(&v, (const char *)pdf_color_space_names.DeviceRGB);
                    break;
                case ccs_CMYK:
                    cos_c_string_value(&v, (const char *)pdf_color_space_names.DeviceCMYK);
                    break;
                default:
                    break;
            }
            code = cos_array_add(pca, &v);
            if (code >= 0) {
                code = pdf_function_scaled(pdev, new_pfn, 0x00, &v);
                if (code >= 0) {
                    code = cos_array_add(pca, &v);
                }
            }
        }
    }
    pdf_delete_base_space_function(pdev, new_pfn);

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
        ppcs = (pdf_color_space_t *)pres;
        ppcs->serialized = NULL;
        ppcs->serialized_size = 0;

        ppcs->ranges = 0;
        pca->id = pres->object->id;
        COS_FREE(pres->object, "pdf_color_space");
        pres->object = (cos_object_t *)pca;
        cos_write_object(COS_OBJECT(pca), pdev, resourceColorSpace);
        csi = gs_color_space_get_index(pcs);
        if (csi != gs_color_space_index_Indexed && ppscc != NULL)
            pprints1(pdev->strm, "/%s", ppcs->rname);
    }
    pres->where_used |= pdev->used_mask;
    code = pdf_add_resource(pdev, pdev->substream_Resources, "/ColorSpace", pres);
    if (code < 0)
        return code;

    csi = gs_color_space_get_index(pcs);
    if (csi == gs_color_space_index_Indexed) {
        cos_value_t value;

        discard(COS_OBJECT_VALUE(&value, pca));
        pca1 = cos_array_alloc(pdev, "pdf_color_space");
        code = pdf_indexed_color_space(pdev, pis, &value, pcs, pca1, (cos_value_t *)&value);
        pca = pca1;

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
            if (pres != NULL) {
            ppcs = (pdf_color_space_t *)pres;
            ppcs->serialized = NULL;
            ppcs->serialized_size = 0;

            ppcs->ranges = 0;
            pca->id = pres->object->id;
            COS_FREE(pres->object, "pdf_color_space");
            pres->object = (cos_object_t *)pca;
            cos_write_object(COS_OBJECT(pca), pdev, resourceColorSpace);
            if (ppscc != NULL)
                pprints1(pdev->strm, "/%s", ppcs->rname);
        }
        pres->where_used |= pdev->used_mask;
        code = pdf_add_resource(pdev, pdev->substream_Resources, "/ColorSpace", pres);
        if (code < 0)
            return code;
    }

    if (ppscc != NULL) {
        pprints1(pdev->strm, " %s\n", ppscc->setcolorspace);
        *used_process_color = false;
        pprintg1(pdev->strm, "%g", psdf_round(pcc->paint.values[0], 255, 8));
        pprints1(pdev->strm, " %s\n", ppscc->setcolorn);
    }
    if (pvalue != NULL) {
        if (by_name) {
            /* Return a resource name rather than an object reference. */
            discard(COS_RESOURCE_VALUE(pvalue, pca));
        } else
            discard(COS_OBJECT_VALUE(pvalue, pca));
    }
    return 0;
}

void
rescale_cie_color(gs_range_t *ranges, int num_colorants,
                    const gs_client_color *src, gs_client_color *des)
{
    int k;

    for (k = 0; k < num_colorants; k++) {
        des->paint.values[k] =
            (src->paint.values[k]-ranges[k].rmin)/
            (ranges[k].rmax-ranges[k].rmin);
    }
}

static int new_pdf_reset_color(gx_device_pdf * pdev, const gs_imager_state * pis,
                const gx_drawing_color *pdc, gx_hl_saved_color * psc,
                bool *used_process_color,
                const psdf_set_color_commands_t *ppscc)
{
    int code=0, code1=0;
    gx_hl_saved_color temp;
    gs_client_color *pcc; /* fixme: not needed due to gx_hld_get_color_component. */
    cos_value_t cs_value;
    gs_color_space_index csi;
    gs_color_space_index csi2;
    const gs_color_space *pcs, *pcs2;

    if (pdev->skip_colors)
        return 0;

    /* Get a copy of the current colour so we can examine it. */
    gx_hld_save_color(pis, pdc, &temp);

    /* Since pdfwrite never applies halftones and patterns, but monitors
     * halftone/pattern IDs separately, we don't need to compare
     * halftone/pattern bodies here.
     */
    if (gx_hld_saved_color_equal(&temp, psc))
        /* New colour (and space) same as old colour, no need to do anything */
        return 0;

    /* Do we have a Pattern colour space ? */
    switch (gx_hld_get_color_space_and_ccolor(pis, pdc, &pcs, (const gs_client_color **)&pcc)) {
        case pattern_color_space:
            {
                pdf_resource_t *pres;

                if (pdc->type == gx_dc_type_pattern) {
                    /* Can't handle tiling patterns in levels 1.0 and 1.1, and
                     * unlike shading patterns we have no fallback.
                     */
                    if (pdev->CompatibilityLevel < 1.2) {
                        return gs_error_undefined;
                    }
                    code = pdf_put_colored_pattern(pdev, pdc, pcs,
                                ppscc, pis, &pres);
                }
                else if (pdc->type == &gx_dc_pure_masked) {
                    code = pdf_put_uncolored_pattern(pdev, pdc, pcs,
                                ppscc, pis, &pres);
                    if (code < 0 || pres == 0) {
                        /* replaced a pattern with a flat fill, but we still
                         * need to change the 'saved' colour or we will
                         * get out of step with the PDF content.
                         */
                        *psc = temp;
                        return code;
                    }
                    if (pis && pis->have_pattern_streams)
                        code = pdf_write_ccolor(pdev, pis, pcc);
                } else if (pdc->type == &gx_dc_pattern2) {
                    if (pdev->CompatibilityLevel <= 1.2)
                        return_error(gs_error_rangecheck);
                    if (!is_pattern2_allowed_in_strategy(pdev, pdc))
                        return_error(gs_error_rangecheck);
                    code1 = pdf_put_pattern2(pdev, pis, pdc, ppscc, &pres);
                    if (code1 < 0)
                        return code1;
                } else
                    return_error(gs_error_rangecheck);
                if (code < 0)
                    return code;
                code = pdf_add_resource(pdev, pdev->substream_Resources, "/Pattern", pres);
                if (code >= 0) {
                    cos_value_write(cos_resource_value(&cs_value, pres->object), pdev);
                    pprints1(pdev->strm, " %s\n", ppscc->setcolorn);
                }
                else {
                    pres->where_used = 0;
                    return code;
                }
                *used_process_color = false;
            }
            break;
        case non_pattern_color_space:
            pcs2 = pcs;
            csi = csi2 = gs_color_space_get_index(pcs);
            if (csi == gs_color_space_index_ICC) {
                csi2 = gsicc_get_default_type(pcs->cmm_icc_profile_data);
            }
            /* Figure out what to do if we are outputting to really ancient versions of PDF */
            /* NB ps2write sets CompatibilityLevel to 1.2 so we cater for it here */
            if (pdev->CompatibilityLevel <= 1.2) {

                /* If we have an /Indexed space, we need to look at the base space */
                if (csi2 == gs_color_space_index_Indexed) {
                    pcs2 = pcs->base_space;
                    csi2 = gs_color_space_get_index(pcs2);
                }

                switch (csi2) {
                    case gs_color_space_index_DeviceGray:
                        if (pdev->params.ColorConversionStrategy == ccs_LeaveColorUnchanged ||
                            pdev->params.ColorConversionStrategy == ccs_Gray)
                            code = write_color_unchanged(pdev, pis, pcc, &temp, psc, ppscc, used_process_color, pcs, pdc);
                        else
                            code = write_color_as_process(pdev, pis, pcs, pdc, used_process_color, ppscc, pcc);
                        break;
                    case gs_color_space_index_DeviceRGB:
                        if (pdev->params.ColorConversionStrategy == ccs_LeaveColorUnchanged ||
                            pdev->params.ColorConversionStrategy == ccs_RGB)
                            code = write_color_unchanged(pdev, pis, pcc, &temp, psc, ppscc, used_process_color, pcs, pdc);
                        else
                            code = write_color_as_process(pdev, pis, pcs, pdc, used_process_color, ppscc, pcc);
                        break;
                    case gs_color_space_index_DeviceCMYK:
                        if (pdev->params.ColorConversionStrategy == ccs_LeaveColorUnchanged ||
                            pdev->params.ColorConversionStrategy == ccs_CMYK)
                            code = write_color_unchanged(pdev, pis, pcc, &temp, psc, ppscc, used_process_color, pcs, pdc);
                        else
                            code = write_color_as_process(pdev, pis, pcs, pdc, used_process_color, ppscc, pcc);
                        break;
                    case gs_color_space_index_CIEA:
                    case gs_color_space_index_CIEABC:
                    case gs_color_space_index_CIEDEF:
                    case gs_color_space_index_CIEDEFG:
                        if (pdev->ForOPDFRead) {
                            switch (pdev->params.ColorConversionStrategy) {
                                case ccs_ByObjectType:
                                    /* Object type not implemented yet */
                                case ccs_UseDeviceDependentColor:
                                    /* DeviceDependentColor deprecated */
                                case ccs_UseDeviceIndependentColorForImages:
                                    /* If only correcting images, then leave unchanged */
                                case ccs_LeaveColorUnchanged:
                                    code = write_color_unchanged(pdev, pis, pcc, &temp, psc, ppscc, used_process_color, pcs, pdc);
                                    break;
                                default:
                                    code = write_color_as_process(pdev, pis, pcs, pdc, used_process_color, ppscc, pcc);
                                    break;
                            }
                        }
                        else
                            code = write_color_as_process(pdev, pis, pcs, pdc, used_process_color, ppscc, pcc);
                        break;
                    case gs_color_space_index_Separation:
                        switch (pdev->params.ColorConversionStrategy) {
                            case ccs_ByObjectType:
                                /* Object type not implemented yet */
                            case ccs_UseDeviceDependentColor:
                                /* DeviceDependentColor deprecated */
                            case ccs_UseDeviceIndependentColorForImages:
                                /* If only correcting images, then leave unchanged */
                            case ccs_LeaveColorUnchanged:
                                code = write_color_unchanged(pdev, pis, pcc, &temp, psc, ppscc, used_process_color, pcs, pdc);
                                break;
                            case ccs_UseDeviceIndependentColor:
                                code = write_color_as_process(pdev, pis, pcs, pdc, used_process_color, ppscc, pcc);
                                break;
                            case ccs_sRGB:
                            default:
                                code = convert_separation_alternate(pdev, pis, pcs, pdc, used_process_color, ppscc, pcc, NULL, false);
                                break;
                        }
                        break;
                    case gs_color_space_index_DeviceN:
                        switch (pdev->params.ColorConversionStrategy) {
                            case ccs_ByObjectType:
                                /* Object type not implemented yet */
                            case ccs_UseDeviceDependentColor:
                                /* DeviceDependentColor deprecated */
                            case ccs_UseDeviceIndependentColorForImages:
                                /* If only correcting images, then leave unchanged */
                            case ccs_LeaveColorUnchanged:
                                code = write_color_unchanged(pdev, pis, pcc, &temp, psc, ppscc, used_process_color, pcs, pdc);
                                break;
                            case ccs_UseDeviceIndependentColor:
                                code = write_color_as_process(pdev, pis, pcs, pdc, used_process_color, ppscc, pcc);
                                break;
                            case ccs_sRGB:
                            default:
                                code = convert_DeviceN_alternate(pdev, pis, pcs, pdc, used_process_color, ppscc, pcc, NULL, false);
                                break;
                        }
                        break;
                    case gs_color_space_index_ICC:
                        /* Note that if csi is ICC, check to see if this was one of
                           the default substitutes that we introduced for DeviceGray,
                           DeviceRGB or DeviceCMYK.  If it is, then just write
                           the default color.  Depending upon the flavor of PDF,
                           or other options, we may want to actually have all
                           the colors defined by ICC profiles and not do the following
                           substituion of the Device space. */
                        csi2 = gsicc_get_default_type(pcs2->cmm_icc_profile_data);

                        switch (csi2) {
                            case gs_color_space_index_DeviceGray:
                                if (pdev->params.ColorConversionStrategy == ccs_Gray ||
                                    pdev->params.ColorConversionStrategy == ccs_LeaveColorUnchanged) {
                                    code = write_color_unchanged(pdev, pis, pcc, &temp, psc, ppscc, used_process_color, pcs, pdc);
                                    *psc = temp;
                                    return code;
                                }
                                break;
                            case gs_color_space_index_DeviceRGB:
                                if (pdev->params.ColorConversionStrategy == ccs_RGB ||
                                    pdev->params.ColorConversionStrategy == ccs_LeaveColorUnchanged) {
                                    code = write_color_unchanged(pdev, pis, pcc, &temp, psc, ppscc, used_process_color, pcs, pdc);
                                    *psc = temp;
                                    return code;
                                }
                                break;
                            case gs_color_space_index_DeviceCMYK:
                                if (pdev->params.ColorConversionStrategy == ccs_CMYK ||
                                    pdev->params.ColorConversionStrategy == ccs_LeaveColorUnchanged) {
                                    code = write_color_unchanged(pdev, pis, pcc, &temp, psc, ppscc, used_process_color, pcs, pdc);
                                    *psc = temp;
                                    return code;
                                }
                                break;
                            default:
                                break;
                        }
                        /* Fall through if its not a device substitute, or not one we want to preserve */
                    case gs_color_space_index_DevicePixel:
                    case gs_color_space_index_Indexed:
                        code = write_color_as_process(pdev, pis, pcs, pdc, used_process_color, ppscc, pcc);
                        break;
                    default:
                        return (gs_note_error(gs_error_rangecheck));
                        break;
                }
            } else {
                switch(pdev->params.ColorConversionStrategy) {
                    case ccs_ByObjectType:
                        /* Object type not implemented yet */
                    case ccs_UseDeviceDependentColor:
                        /* DeviceDependentCOlor deprecated */
                    case ccs_UseDeviceIndependentColorForImages:
                        /* If only correcting images, then leave unchanged */
                    case ccs_LeaveColorUnchanged:
                        code = write_color_unchanged(pdev, pis, pcc, &temp, psc, ppscc, used_process_color, pcs, pdc);
                        break;
                    case ccs_UseDeviceIndependentColor:
                            code = write_color_as_process_ICC(pdev, pis, pcs, pdc, psc, used_process_color, ppscc, pcc, &temp);
                        break;
                    case ccs_CMYK:
                        switch(csi2) {
                            case gs_color_space_index_DeviceGray:
                            case gs_color_space_index_DeviceCMYK:
                                code = write_color_unchanged(pdev, pis, pcc, &temp, psc, ppscc, used_process_color, pcs, pdc);
                                break;
                            case gs_color_space_index_Separation:
                                pcs2 = pcs2->base_space;
                                csi = gs_color_space_get_index(pcs2);
                                if (csi == gs_color_space_index_ICC)
                                    csi = gsicc_get_default_type(pcs2->cmm_icc_profile_data);
                                if (csi != gs_color_space_index_DeviceCMYK)
                                    code = convert_separation_alternate(pdev, pis, pcs, pdc, used_process_color, ppscc, pcc, NULL, false);
                                else
                                    code = write_color_unchanged(pdev, pis, pcc, &temp, psc, ppscc, used_process_color, pcs, pdc);
                                break;
                            case gs_color_space_index_DeviceN:
                                pcs2 = pcs2->base_space;
                                csi = gs_color_space_get_index(pcs2);
                                if (csi == gs_color_space_index_ICC)
                                    csi = gsicc_get_default_type(pcs2->cmm_icc_profile_data);
                                if (csi != gs_color_space_index_DeviceCMYK)
                                    code = convert_DeviceN_alternate(pdev, pis, pcs, pdc, used_process_color, ppscc, pcc, NULL, false);
                                else
                                    code = write_color_unchanged(pdev, pis, pcc, &temp, psc, ppscc, used_process_color, pcs, pdc);
                                break;
                            case gs_color_space_index_Indexed:
                                pcs2 = pcs->base_space;
                                csi = gs_color_space_get_index(pcs2);
                                if (csi == gs_color_space_index_ICC)
                                    csi = gsicc_get_default_type(pcs2->cmm_icc_profile_data);
                                switch(csi) {
                                    case gs_color_space_index_DeviceGray:
                                    case gs_color_space_index_DeviceCMYK:
                                        code = write_color_unchanged(pdev, pis, pcc, &temp, psc, ppscc, used_process_color, pcs, pdc);
                                        break;
                                    case gs_color_space_index_Separation:
                                        pcs2 = pcs2->base_space;
                                        csi = gs_color_space_get_index(pcs2);
                                        if (csi == gs_color_space_index_ICC)
                                            csi = gsicc_get_default_type(pcs2->cmm_icc_profile_data);
                                        if (csi != gs_color_space_index_DeviceCMYK)
                                            code = convert_separation_alternate(pdev, pis, pcs, pdc, used_process_color, ppscc, pcc, NULL, false);
                                        else
                                            code = write_color_unchanged(pdev, pis, pcc, &temp, psc, ppscc, used_process_color, pcs, pdc);
                                        break;
                                    case gs_color_space_index_DeviceN:
                                        pcs2 = pcs2->base_space;
                                        csi = gs_color_space_get_index(pcs2);
                                        if (csi == gs_color_space_index_ICC)
                                            csi = gsicc_get_default_type(pcs2->cmm_icc_profile_data);
                                        if (csi != gs_color_space_index_DeviceCMYK)
                                            code = convert_DeviceN_alternate(pdev, pis, pcs, pdc, used_process_color, ppscc, pcc, NULL, false);
                                        else
                                            code = write_color_unchanged(pdev, pis, pcc, &temp, psc, ppscc, used_process_color, pcs, pdc);
                                        break;
                                    default:
                                        code = write_color_as_process(pdev, pis, pcs, pdc, used_process_color, ppscc, pcc);
                                        break;
                                }
                                break;
                            default:
                                code = write_color_as_process(pdev, pis, pcs, pdc, used_process_color, ppscc, pcc);
                                break;
                        }
                        break;
                    case ccs_Gray:
                        switch(csi2) {
                            case gs_color_space_index_DeviceGray:
                                code = write_color_unchanged(pdev, pis, pcc, &temp, psc, ppscc, used_process_color, pcs, pdc);
                                break;
                            case gs_color_space_index_Separation:
                                pcs2 = pcs2->base_space;
                                csi = gs_color_space_get_index(pcs2);
                                if (csi == gs_color_space_index_ICC)
                                    csi = gsicc_get_default_type(pcs2->cmm_icc_profile_data);
                                if (csi != gs_color_space_index_DeviceGray)
                                    code = convert_separation_alternate(pdev, pis, pcs, pdc, used_process_color, ppscc, pcc, NULL, false);
                                else
                                    code = write_color_unchanged(pdev, pis, pcc, &temp, psc, ppscc, used_process_color, pcs, pdc);
                                break;
                            case gs_color_space_index_DeviceN:
                                pcs2 = pcs2->base_space;
                                csi = gs_color_space_get_index(pcs2);
                                if (csi == gs_color_space_index_ICC)
                                    csi = gsicc_get_default_type(pcs2->cmm_icc_profile_data);
                                if (csi != gs_color_space_index_DeviceGray)
                                    code = convert_DeviceN_alternate(pdev, pis, pcs, pdc, used_process_color, ppscc, pcc, NULL, false);
                                else
                                    code = write_color_unchanged(pdev, pis, pcc, &temp, psc, ppscc, used_process_color, pcs, pdc);
                                break;
                            case gs_color_space_index_Indexed:
                                pcs2 = pcs->base_space;
                                csi = gs_color_space_get_index(pcs2);
                                if (csi == gs_color_space_index_ICC)
                                    csi = gsicc_get_default_type(pcs2->cmm_icc_profile_data);
                                switch(csi) {
                                    case gs_color_space_index_DeviceGray:
                                        code = write_color_unchanged(pdev, pis, pcc, &temp, psc, ppscc, used_process_color, pcs, pdc);
                                        break;
                                    case gs_color_space_index_Separation:
                                        pcs2 = pcs2->base_space;
                                        csi = gs_color_space_get_index(pcs2);
                                        if (csi == gs_color_space_index_ICC)
                                            csi = gsicc_get_default_type(pcs2->cmm_icc_profile_data);
                                        if (csi != gs_color_space_index_DeviceGray)
                                            code = convert_separation_alternate(pdev, pis, pcs, pdc, used_process_color, ppscc, pcc, NULL, false);
                                        else
                                            code = write_color_unchanged(pdev, pis, pcc, &temp, psc, ppscc, used_process_color, pcs, pdc);
                                        break;
                                    case gs_color_space_index_DeviceN:
                                        pcs2 = pcs2->base_space;
                                        csi = gs_color_space_get_index(pcs2);
                                        if (csi == gs_color_space_index_ICC)
                                            csi = gsicc_get_default_type(pcs2->cmm_icc_profile_data);
                                        if (csi != gs_color_space_index_DeviceGray)
                                            code = convert_DeviceN_alternate(pdev, pis, pcs, pdc, used_process_color, ppscc, pcc, NULL, false);
                                        else
                                            code = write_color_unchanged(pdev, pis, pcc, &temp, psc, ppscc, used_process_color, pcs, pdc);
                                        break;
                                    default:
                                        code = write_color_as_process(pdev, pis, pcs, pdc, used_process_color, ppscc, pcc);
                                        break;
                                }
                                break;
                            default:
                                code = write_color_as_process(pdev, pis, pcs, pdc, used_process_color, ppscc, pcc);
                                break;
                        }
                        break;
                    case ccs_sRGB:
                    case ccs_RGB:
                        switch(csi2) {
                            case gs_color_space_index_DeviceGray:
                            case gs_color_space_index_DeviceRGB:
                                code = write_color_unchanged(pdev, pis, pcc, &temp, psc, ppscc, used_process_color, pcs, pdc);
                                break;
                            case gs_color_space_index_Separation:
                                pcs2 = pcs2->base_space;
                                csi = gs_color_space_get_index(pcs2);
                                if (csi == gs_color_space_index_ICC)
                                    csi = gsicc_get_default_type(pcs2->cmm_icc_profile_data);
                                if (csi != gs_color_space_index_DeviceRGB)
                                    code = convert_separation_alternate(pdev, pis, pcs, pdc, used_process_color, ppscc, pcc, NULL, false);
                                else
                                    code = write_color_unchanged(pdev, pis, pcc, &temp, psc, ppscc, used_process_color, pcs, pdc);
                                break;
                            case gs_color_space_index_DeviceN:
                                pcs2 = pcs2->base_space;
                                csi = gs_color_space_get_index(pcs2);
                                if (csi == gs_color_space_index_ICC)
                                    csi = gsicc_get_default_type(pcs2->cmm_icc_profile_data);
                                if (csi != gs_color_space_index_DeviceRGB)
                                    code = convert_DeviceN_alternate(pdev, pis, pcs, pdc, used_process_color, ppscc, pcc, NULL, false);
                                else
                                    code = write_color_unchanged(pdev, pis, pcc, &temp, psc, ppscc, used_process_color, pcs, pdc);
                                break;
                            case gs_color_space_index_Indexed:
                                pcs2 = pcs->base_space;
                                csi = gs_color_space_get_index(pcs2);
                                if (csi == gs_color_space_index_ICC)
                                    csi = gsicc_get_default_type(pcs2->cmm_icc_profile_data);
                                switch(csi) {
                                    case gs_color_space_index_DeviceGray:
                                    case gs_color_space_index_DeviceRGB:
                                        code = write_color_unchanged(pdev, pis, pcc, &temp, psc, ppscc, used_process_color, pcs, pdc);
                                        break;
                                    case gs_color_space_index_Separation:
                                        pcs2 = pcs2->base_space;
                                        csi = gs_color_space_get_index(pcs2);
                                        if (csi == gs_color_space_index_ICC)
                                            csi = gsicc_get_default_type(pcs2->cmm_icc_profile_data);
                                        if (csi != gs_color_space_index_DeviceRGB)
                                            code = convert_separation_alternate(pdev, pis, pcs, pdc, used_process_color, ppscc, pcc, NULL, false);
                                        else
                                            code = write_color_unchanged(pdev, pis, pcc, &temp, psc, ppscc, used_process_color, pcs, pdc);
                                        break;
                                    case gs_color_space_index_DeviceN:
                                        pcs2 = pcs2->base_space;
                                        csi = gs_color_space_get_index(pcs2);
                                        if (csi == gs_color_space_index_ICC)
                                            csi = gsicc_get_default_type(pcs2->cmm_icc_profile_data);
                                        if (csi != gs_color_space_index_DeviceRGB)
                                            code = convert_DeviceN_alternate(pdev, pis, pcs, pdc, used_process_color, ppscc, pcc, NULL, false);
                                        else
                                            code = write_color_unchanged(pdev, pis, pcc, &temp, psc, ppscc, used_process_color, pcs, pdc);
                                        break;
                                    default:
                                        code = write_color_as_process(pdev, pis, pcs, pdc, used_process_color, ppscc, pcc);
                                        break;
                                }
                                break;
                            default:
                                code = write_color_as_process(pdev, pis, pcs, pdc, used_process_color, ppscc, pcc);
                                break;
                        }
                        break;
                    default:
                        break;
                }
            }
            break;
        default: /* must not happen. */
        case use_process_color:
            code = psdf_set_color((gx_device_vector *)pdev, pdc, ppscc, pdev->UseOldColor);
            if (code < 0)
                return code;
            *used_process_color = true;
            break;
    }
    *psc = temp;
    return code;
}

/* Set the fill or stroke color. */
int
pdf_reset_color(gx_device_pdf * pdev, const gs_imager_state * pis,
                const gx_drawing_color *pdc, gx_hl_saved_color * psc,
                bool *used_process_color,
                const psdf_set_color_commands_t *ppscc)
{
    int code = 0;
    gx_hl_saved_color temp;
    const gs_color_space *pcs, *pcs2;
    const gs_client_color *pcc; /* fixme: not needed due to gx_hld_get_color_component. */
    cos_value_t cs_value;
    const char *command;
    int code1 = 0;
    gs_color_space_index csi;
    gs_color_space_index csi2;
    gs_range_t *ranges = 0;


    if (!pdev->UseOldColor)
        return new_pdf_reset_color(pdev, pis, pdc, psc, used_process_color, ppscc);

    if (pdev->skip_colors)
        return 0;
    gx_hld_save_color(pis, pdc, &temp);
    /* Since pdfwrite never applies halftones and patterns, but monitors
     * halftone/pattern IDs separately, we don't need to compare
     * halftone/pattern bodies here.
     */
    if (gx_hld_saved_color_equal(&temp, psc))
        return 0;

    switch (gx_hld_get_color_space_and_ccolor(pis, pdc, &pcs, &pcc)) {
        case non_pattern_color_space:
            csi2 = gs_color_space_get_index(pcs);
            if (csi2 == gs_color_space_index_ICC) {
                csi2 = gsicc_get_default_type(pcs->cmm_icc_profile_data);
            }
            switch (csi2) {
                case gs_color_space_index_DeviceGray:
                    command = ppscc->setgray;
                    break;
                case gs_color_space_index_DeviceRGB:
                    if (pdev->params.ColorConversionStrategy == ccs_CMYK)
                        goto write_process_color;
                    if (pdev->params.ColorConversionStrategy == ccs_Gray)
                        goto write_process_color;
                    command = ppscc->setrgbcolor;
                    break;
                case gs_color_space_index_DeviceCMYK:
                    if (pdev->params.ColorConversionStrategy == ccs_sRGB)
                        goto write_process_color;
                    if (pdev->params.ColorConversionStrategy == ccs_Gray)
                        goto write_process_color;
                    command = ppscc->setcmykcolor;
                    break;
                case gs_color_space_index_Indexed:
                    if (pdev->CompatibilityLevel <= 1.2) {
                        pcs2 = pcs->base_space;
                        csi = gs_color_space_get_index(pcs2);
                        if (!is_cspace_allowed_in_strategy(pdev, csi))
                            goto write_process_color;
                        if (csi == gs_color_space_index_Separation) {
                            pcs2 = pcs->base_space;
                            goto check_pcs2;
                        }
                        goto check_pcs2;
                    }
                    goto scn;
                case gs_color_space_index_Separation:
                    if (pdev->CompatibilityLevel <= 1.2) {
                        pcs2 = pcs->base_space;
                        check_pcs2:
                        csi = gs_color_space_get_index(pcs2);
                        if (!is_cspace_allowed_in_strategy(pdev, csi))
                            goto write_process_color;
                        switch(gs_color_space_get_index(pcs2)) {
                            case gs_color_space_index_DevicePixel :
                            case gs_color_space_index_DeviceN:
                            case gs_color_space_index_ICC:
                                goto write_process_color;
                            default:
                                DO_NOTHING;
                        }
                    }
                    goto scn;
                case gs_color_space_index_ICC:
                case gs_color_space_index_DevicePixel:
                case gs_color_space_index_DeviceN:
                    if (pdev->CompatibilityLevel <= 1.2)
                        goto write_process_color;
                    goto scn;
                default :
                    if (pdev->params.ColorConversionStrategy == ccs_CMYK)
                        goto write_process_color;
                    if (pdev->params.ColorConversionStrategy == ccs_sRGB)
                        goto write_process_color;
                    if (pdev->params.ColorConversionStrategy == ccs_Gray)
                        goto write_process_color;
                scn:
                    command = ppscc->setcolorn;
                    if (!gx_hld_saved_color_same_cspace(&temp, psc)) {
                        code = pdf_color_space_named(pdev, pis, &cs_value, (const gs_range_t **)&ranges, pcs,
                                        &pdf_color_space_names, true, NULL, 0, false);
                        /* fixme : creates redundant PDF objects. */
                        if (code == gs_error_rangecheck) {
                            /* The color space can't write to PDF. */
                            goto write_process_color;
                        }
                        if (code < 0)
                            return code;
                        code = cos_value_write(&cs_value, pdev);
                        if (code < 0)
                            return code;
                        pprints1(pdev->strm, " %s\n", ppscc->setcolorspace);
                        if (ranges && (csi2 >= gs_color_space_index_CIEDEFG && csi2 <= gs_color_space_index_CIEA)) {
                            gs_client_color *dcc = (gs_client_color *)pcc;
                            switch (csi2) {
                                case gs_color_space_index_CIEDEFG:
                                    rescale_cie_color(ranges, 4, pcc, dcc);
                                    break;
                                case gs_color_space_index_CIEDEF:
                                    rescale_cie_color(ranges, 3, pcc, dcc);
                                    break;
                                case gs_color_space_index_CIEABC:
                                    rescale_cie_color(ranges, 3, pcc, dcc);
                                    break;
                                case gs_color_space_index_CIEA:
                                    rescale_cie_color(ranges, 1, pcc, dcc);
                                    break;
                                default:
                                    /* Can't happen, but silences compiler warnings */
                                    break;
                            }
                        }
                    } else if (*used_process_color)
                        goto write_process_color;
                    break;
            }
            *used_process_color = false;
            code = pdf_write_ccolor(pdev, pis, pcc);
            if (code < 0)
                return code;
            pprints1(pdev->strm, " %s\n", command);
            break;
        case pattern_color_space:
            {	pdf_resource_t *pres;

                if (pdc->type == gx_dc_type_pattern)
                    code = pdf_put_colored_pattern(pdev, pdc, pcs,
                                ppscc, pis, &pres);
                else if (pdc->type == &gx_dc_pure_masked) {
                    code = pdf_put_uncolored_pattern(pdev, pdc, pcs,
                                ppscc, pis, &pres);
                    if (code < 0 || pres == 0) {
                        /* replaced a pattern with a flat fill, but we still
                         * need to change the 'saved' colour or we will
                         * get out of step with the PDF content.
                         */
                        *psc = temp;
                        return code;
                    }
                    if (pis->have_pattern_streams)
                        code = pdf_write_ccolor(pdev, pis, pcc);
                } else if (pdc->type == &gx_dc_pattern2) {
                    if (pdev->CompatibilityLevel <= 1.2)
                        return_error(gs_error_rangecheck);
                    if (!is_pattern2_allowed_in_strategy(pdev, pdc))
                        return_error(gs_error_rangecheck);
                    code1 = pdf_put_pattern2(pdev, pis, pdc, ppscc, &pres);
                } else
                    return_error(gs_error_rangecheck);
                if (code < 0)
                    return code;
                code = pdf_add_resource(pdev, pdev->substream_Resources, "/Pattern", pres);
                if (code1 != gs_error_rangecheck) {
                    cos_value_write(cos_resource_value(&cs_value, pres->object), pdev);
                    pprints1(pdev->strm, " %s\n", ppscc->setcolorn);
                }
                else
                    pres->where_used = 0;
                if (code < 0)
                    return code;
            }
            *used_process_color = false;
            break;
        default: /* must not happen. */
        case use_process_color:
        write_process_color:
            code = psdf_set_color((gx_device_vector *)pdev, pdc, ppscc, pdev->UseOldColor);
            if (code < 0)
                return code;
            *used_process_color = true;
    }
    *psc = temp;
    return code1;
}
int
pdf_set_drawing_color(gx_device_pdf * pdev, const gs_imager_state * pis,
                      const gx_drawing_color *pdc,
                      gx_hl_saved_color * psc,
                      bool *used_process_color,
                      const psdf_set_color_commands_t *ppscc)
{
    gx_hl_saved_color temp;
    int code;

    /* This section of code was in pdf_reset_color above, but was moved into this
     * routine (and below) in order to isolate the switch to a stream context. This
     * now allows us the opportunity to write colours in any context, in particular
     * when in a text context, by using pdf_reset_color.
     */
    if (pdev->skip_colors)
        return 0;
    gx_hld_save_color(pis, pdc, &temp);
    /* Since pdfwrite never applies halftones and patterns, but monitors
     * halftone/pattern IDs separately, we don't need to compare
     * halftone/pattern bodies here.
     */
    if (gx_hld_saved_color_equal(&temp, psc))
        return 0;
    /*
     * In principle, we can set colors in either stream or text
     * context.  However, since we currently enclose all text
     * strings inside a gsave/grestore, this causes us to lose
     * track of the color when we leave text context.  Therefore,
     * we require stream context for setting colors.
     */
    code = pdf_open_page(pdev, PDF_IN_STREAM);
    if (code < 0)
        return code;

    return pdf_reset_color(pdev, pis, pdc, psc, used_process_color, ppscc);
}
int
pdf_set_pure_color(gx_device_pdf * pdev, gx_color_index color,
                   gx_hl_saved_color * psc,
                   bool *used_process_color,
                   const psdf_set_color_commands_t *ppscc)
{
    gx_drawing_color dcolor;
    gx_hl_saved_color temp;
    int code;

    set_nonclient_dev_color(&dcolor, color);

    if (pdev->skip_colors)
        return 0;
    gx_hld_save_color(NULL, &dcolor, &temp);
    /* Since pdfwrite never applies halftones and patterns, but monitors
     * halftone/pattern IDs separately, we don't need to compare
     * halftone/pattern bodies here.
     */
    if (gx_hld_saved_color_equal(&temp, psc))
        return 0;
    /*
     * In principle, we can set colors in either stream or text
     * context.  However, since we currently enclose all text
     * strings inside a gsave/grestore, this causes us to lose
     * track of the color when we leave text context.  Therefore,
     * we require stream context for setting colors.
     */
    code = pdf_open_page(pdev, PDF_IN_STREAM);
    if (code < 0)
        return code;

    return pdf_reset_color(pdev, NULL, &dcolor, psc, used_process_color, ppscc);
}

/*
 * Convert a string into cos name.
 */
int
pdf_string_to_cos_name(gx_device_pdf *pdev, const byte *str, uint len,
                       cos_value_t *pvalue)
{
    byte *chars = gs_alloc_string(pdev->pdf_memory, len + 1,
                                  "pdf_string_to_cos_name");

    if (chars == 0)
        return_error(gs_error_VMerror);
    chars[0] = '/';
    memcpy(chars + 1, str, len);
    cos_string_value(pvalue, chars, len + 1);
    return 0;
}

/* ---------------- Graphics state updating ---------------- */

/* ------ Functions ------ */

/* Define the maximum size of a Function reference. */
#define MAX_FN_NAME_CHARS 9	/* /Default, /Identity */
#define MAX_FN_CHARS max(MAX_REF_CHARS + 4, MAX_FN_NAME_CHARS)

/*
 * Create and write a Function for a gx_transfer_map.  We use this for
 * transfer, BG, and UCR functions.  If check_identity is true, check for
 * an identity map.  Return 1 if the map is the identity map, otherwise
 * return 0.
 */
static data_source_proc_access(transfer_map_access); /* check prototype */
static int
transfer_map_access(const gs_data_source_t *psrc, ulong start, uint length,
                    byte *buf, const byte **ptr)
{
    const gx_transfer_map *map = (const gx_transfer_map *)psrc->data.str.data;
    uint i;

    if (ptr)
        *ptr = buf;
    for (i = 0; i < length; ++i)
        buf[i] = frac2byte(map->values[(uint)start + i]);
    return 0;
}
static int
transfer_map_access_signed(const gs_data_source_t *psrc,
                           ulong start, uint length,
                           byte *buf, const byte **ptr)
{
    /* To prevent numeric errors, we need to map 0 to an integer.
     * We can't apply a general expression, because Decode isn't accessible here.
     * Assuming this works for UCR only.
     * Assuming the range of UCR is always [-1, 1].
     * Assuming BitsPerSample = 8.
     */
    const gx_transfer_map *map = (const gx_transfer_map *)psrc->data.str.data;
    uint i;

    *ptr = buf;
    for (i = 0; i < length; ++i)
        buf[i] = (byte)
            ((frac2float(map->values[(uint)start + i]) + 1) * 127);
    return 0;
}
static int
pdf_write_transfer_map(gx_device_pdf *pdev, const gx_transfer_map *map,
                       int range0, bool check_identity,
                       const char *key, char *ids)
{
    gs_memory_t *mem = pdev->pdf_memory;
    gs_function_Sd_params_t params;
    static const float domain01[2] = { 0, 1 };
    static const int size = transfer_map_size;
    float range01[2], decode[2];
    gs_function_t *pfn;
    long id;
    int code;

    if (map == 0) {
        *ids = 0;		/* no map */
        return 1;
    }
    if (check_identity) {
        /* Check for an identity map. */
        int i;

        if (map->proc == gs_identity_transfer)
            i = transfer_map_size;
        else
            for (i = 0; i < transfer_map_size; ++i) {
                fixed d = map->values[i] - bits2frac(i, log2_transfer_map_size);
                if (any_abs(d) > fixed_epsilon) /* ignore small noise */
                    break;
            }
        if (i == transfer_map_size) {
            strcpy(ids, key);
            strcat(ids, "/Identity");
            return 1;
        }
    }
    params.m = 1;
    params.Domain = domain01;
    params.n = 1;
    range01[0] = (float)range0, range01[1] = 1.0;
    params.Range = range01;
    params.Order = 1;
    params.DataSource.access =
        (range0 < 0 ? transfer_map_access_signed : transfer_map_access);
    params.DataSource.data.str.data = (const byte *)map; /* bogus */
    /* DataSource */
    params.BitsPerSample = 8;	/* could be 16 */
    params.Encode = 0;
    if (range01[0] < 0 && range01[1] > 0) {
        /* This works for UCR only.
         * Map 0 to an integer.
         * Rather the range of UCR is always [-1, 1],
         * we prefer a general expression.
         */
        int r0 = (int)( -range01[0] * ((1 << params.BitsPerSample) - 1)
                        / (range01[1] - range01[0]) ); /* Round down. */
        float r1 = r0 * range01[1] / -range01[0]; /* r0 + r1 <= (1 << params.BitsPerSample) - 1 */

        decode[0] = range01[0];
        decode[1] = range01[0] + (range01[1] - range01[0]) * ((1 << params.BitsPerSample) - 1)
                                    / (r0 + r1);
        params.Decode = decode;
    } else
        params.Decode = 0;
    params.Size = &size;
    code = gs_function_Sd_init(&pfn, &params, mem);
    if (code < 0)
        return code;
    code = pdf_write_function(pdev, pfn, &id);
    gs_function_free(pfn, false, mem);
    if (code < 0)
        return code;
    gs_sprintf(ids, "%s%s%ld 0 R", key, (key[0] && key[0] != ' ' ? " " : ""), id);
    return 0;
}
static int
pdf_write_transfer(gx_device_pdf *pdev, const gx_transfer_map *map,
                   const char *key, char *ids)
{
    return pdf_write_transfer_map(pdev, map, 0, true, key, ids);
}

/* ------ Halftones ------ */

/*
 * Recognize the predefined PDF halftone functions.  Note that because the
 * corresponding PostScript functions use single-precision floats, the
 * functions used for testing must do the same in order to get identical
 * results.  Currently we only do this for a few of the functions.
 */
#define HT_FUNC(name, expr)\
  static double name(double xd, double yd) {\
    float x = (float)xd, y = (float)yd;\
    return d2f(expr);\
  }

/*
 * In most versions of gcc (e.g., 2.7.2.3, 2.95.4), return (float)xxx
 * doesn't actually do the coercion.  Force this here.  Note that if we
 * use 'inline', it doesn't work.
 */
static float
d2f(double d)
{
    float f = (float)d;
    return f;
}
static double
ht_Round(double xf, double yf)
{
    float x = (float)xf, y = (float)yf;
    float xabs = fabs(x), yabs = fabs(y);

    if (d2f(xabs + yabs) <= 1)
        return d2f(1 - d2f(d2f(x * x) + d2f(y * y)));
    xabs -= 1, yabs -= 1;
    return d2f(d2f(d2f(xabs * xabs) + d2f(yabs * yabs)) - 1);
}
static double
ht_Diamond(double xf, double yf)
{
    float x = (float)xf, y = (float)yf;
    float xabs = fabs(x), yabs = fabs(y);

    if (d2f(xabs + yabs) <= 0.75)
        return d2f(1 - d2f(d2f(x * x) + d2f(y * y)));
    if (d2f(xabs + yabs) <= d2f(1.23))
        return d2f(1 - d2f(d2f(d2f(0.85) * xabs) + yabs));
    xabs -= 1, yabs -= 1;
    return d2f(d2f(d2f(xabs * xabs) + d2f(yabs * yabs)) - 1);
}
static double
ht_Ellipse(double xf, double yf)
{
    float x = (float)xf, y = (float)yf;
    float xabs = fabs(x), yabs = fabs(y);
    /*
     * The PDF Reference, 2nd edition, incorrectly specifies the
     * computation w = 4 * |x| + 3 * |y| - 3.  The PostScript code in the
     * same book correctly implements w = 3 * |x| + 4 * |y| - 3.
     */
    float w = (float)(d2f(d2f(3 * xabs) + d2f(4 * yabs)) - 3);

    if (w < 0) {
        yabs /= 0.75;
        return d2f(1 - d2f((d2f(x * x) + d2f(yabs * yabs)) / 4));
    }
    if (w > 1) {
        xabs = 1 - xabs, yabs = d2f(1 - yabs) / 0.75;
        return d2f(d2f((d2f(xabs * xabs) + d2f(yabs * yabs)) / 4) - 1);
    }
    return d2f(0.5 - w);
}
/*
 * Most of these are recognized properly even without d2f.  We've only
 * added d2f where it apparently makes a difference.
 */
static float
d2fsin_d(double x) {
    return d2f(gs_sin_degrees(d2f(x)));
}
static float
d2fcos_d(double x) {
    return d2f(gs_cos_degrees(d2f(x)));
}
HT_FUNC(ht_EllipseA, 1 - (x * x + 0.9 * y * y))
HT_FUNC(ht_InvertedEllipseA, x * x + 0.9 * y * y - 1)
HT_FUNC(ht_EllipseB, 1 - sqrt(x * x + 0.625 * y * y))
HT_FUNC(ht_EllipseC, 1 - (0.9 * x * x + y * y))
HT_FUNC(ht_InvertedEllipseC, 0.9 * x * x + y * y - 1)
HT_FUNC(ht_Line, -fabs((x - x) + y)) /* quiet compiler (unused variable x) */
HT_FUNC(ht_LineX, (y - y) + x) /* quiet compiler (unused variable y) */
HT_FUNC(ht_LineY, (x - x) + y) /* quiet compiler (unused variable x) */
HT_FUNC(ht_Square, -max(fabs(x), fabs(y)))
HT_FUNC(ht_Cross, -min(fabs(x), fabs(y)))
HT_FUNC(ht_Rhomboid, (0.9 * fabs(x) + fabs(y)) / 2)
HT_FUNC(ht_DoubleDot, (d2fsin_d(x * 360) + d2fsin_d(y * 360)) / 2)
HT_FUNC(ht_InvertedDoubleDot, -(d2fsin_d(x * 360) + d2fsin_d(y * 360)) / 2)
HT_FUNC(ht_SimpleDot, 1 - d2f(d2f(x * x) + d2f(y * y)))
HT_FUNC(ht_InvertedSimpleDot, d2f(d2f(x * x) + d2f(y * y)) - 1)
HT_FUNC(ht_CosineDot, (d2fcos_d(x * 180) + d2fcos_d(y * 180)) / 2)
HT_FUNC(ht_Double, (d2fsin_d(x * 180) + d2fsin_d(y * 360)) / 2)
HT_FUNC(ht_InvertedDouble, -(d2fsin_d(x * 180) + d2fsin_d(y * 360)) / 2)
typedef struct ht_function_s {
    const char *fname;
    double (*proc)(double, double);
} ht_function_t;
static const ht_function_t ht_functions[] = {
    {"Round", ht_Round},
    {"Diamond", ht_Diamond},
    {"Ellipse", ht_Ellipse},
    {"EllipseA", ht_EllipseA},
    {"InvertedEllipseA", ht_InvertedEllipseA},
    {"EllipseB", ht_EllipseB},
    {"EllipseC", ht_EllipseC},
    {"InvertedEllipseC", ht_InvertedEllipseC},
    {"Line", ht_Line},
    {"LineX", ht_LineX},
    {"LineY", ht_LineY},
    {"Square", ht_Square},
    {"Cross", ht_Cross},
    {"Rhomboid", ht_Rhomboid},
    {"DoubleDot", ht_DoubleDot},
    {"InvertedDoubleDot", ht_InvertedDoubleDot},
    {"SimpleDot", ht_SimpleDot},
    {"InvertedSimpleDot", ht_InvertedSimpleDot},
    {"CosineDot", ht_CosineDot},
    {"Double", ht_Double},
    {"InvertedDouble", ht_InvertedDouble}
};

/* Write each kind of halftone. */
static int
pdf_write_spot_function(gx_device_pdf *pdev, const gx_ht_order *porder,
                        long *pid)
{
    /****** DOESN'T HANDLE STRIP HALFTONES ******/
    int w = porder->width, h = porder->height;
    uint num_bits = porder->num_bits;
    gs_function_Sd_params_t params;
    static const float domain_spot[4] = { -1, 1, -1, 1 };
    static const float range_spot[4] = { -1, 1 };
    int size[2];
    gs_memory_t *mem = pdev->pdf_memory;
    /*
     * Even though the values are logically ushort, we must always store
     * them in big-endian order, so we access them as bytes.
     */
    byte *values;
    gs_function_t *pfn;
    uint i;
    int code = 0;

    params.array_size = 0;
    params.m = 2;
    params.Domain = domain_spot;
    params.n = 1;
    params.Range = range_spot;
    params.Order = 0;		/* default */
    /*
     * We could use 8, 16, or 32 bits per sample to save space, but for
     * simplicity, we always use 16.
     */
    if (num_bits > 0x10000)
        /* rangecheck is a 'special case' in gdev_pdf_fill_path, if this error is encountered
         * then it 'falls back' to a different method assuming its handling transparency in an
         * old PDF output version. But if we fail to write the halftone, we want to abort
         * so use limitcheck instead.
         */
        return_error(gs_error_limitcheck);
    params.BitsPerSample = 16;
    params.Encode = 0;
    /*
     * The default Decode array maps the actual data values [1 .. w*h] to a
     * sub-interval of the Range, but that's OK, since all that matters is
     * the relative values, not the absolute values.
     */
    params.Decode = 0;
    size[0] = w;
    size[1] = h;
    params.Size = size;
    /* Create the (temporary) threshold array. */
    values = gs_alloc_byte_array(mem, num_bits, 2, "pdf_write_spot_function");
    if (values == 0)
        return_error(gs_error_VMerror);
    for (i = 0; i < num_bits; ++i) {
        gs_int_point pt;
        int value;

        if ((code = porder->procs->bit_index(porder, i, &pt)) < 0)
            break;
        value = pt.y * w + pt.x;
        /* Always store the values in big-endian order. */
        values[i * 2] = (byte)(value >> 8);
        values[i * 2 + 1] = (byte)value;
    }
    data_source_init_bytes(&params.DataSource, (const byte *)values,
                           sizeof(*values) * num_bits);
    if (code >= 0 &&
    /* Warning from COverity that params.array_size is uninitialised. Correct */
    /* but immeidiately after copying the data Sd_init sets the copied value  */
    /* to zero, so it is not actually used uninitialised. */
        (code = gs_function_Sd_init(&pfn, &params, mem)) >= 0
        ) {
        code = pdf_write_function(pdev, pfn, pid);
        gs_function_free(pfn, false, mem);
    }
    gs_free_object(mem, values, "pdf_write_spot_function");
    return code;
}

/* if (memcmp(order.levels, porder->levels, order.num_levels * sizeof(*order.levels))) */ 
static int
compare_gx_ht_order_levels(const gx_ht_order *order1, const gx_ht_order *order2) {
  int i;
  for (i=0;  i<order1->num_levels;  i++) {
    if (order1->levels[i] != order2->levels[i])
      return(1);
  }
  return(0);
}

static int
pdf_write_spot_halftone(gx_device_pdf *pdev, const gs_spot_halftone *psht,
                        const gx_ht_order *porder, long *pid)
{
    char trs[17 + MAX_FN_CHARS + 1];
    int code = pdf_write_transfer(pdev, porder->transfer, "/TransferFunction",
                                  trs);
    long spot_id;
    stream *s;
    int i = countof(ht_functions);
    gs_memory_t *mem = pdev->pdf_memory;

    if (code < 0)
        return code;
    /*
     * See if we can recognize the spot function, by comparing its sampled
     * values against those in the order.
     */
    {	gs_screen_enum senum;
        gx_ht_order order;
        int code;

        order = *porder;
        code = gs_screen_order_alloc(&order, mem);
        if (code < 0)
            goto notrec;
        for (i = 0; i < countof(ht_functions); ++i) {
            double (*spot_proc)(double, double) = ht_functions[i].proc;
            gs_point pt;

            gs_screen_enum_init_memory(&senum, &order, NULL, &psht->screen,
                                       mem);
            while ((code = gs_screen_currentpoint(&senum, &pt)) == 0 &&
                   gs_screen_next(&senum, spot_proc(pt.x, pt.y)) >= 0)
                DO_NOTHING;
            if (code < 0)
                continue;
            /* Compare the bits and levels arrays. */
            if (compare_gx_ht_order_levels(&order,porder))
                continue;
            if (memcmp(order.bit_data, porder->bit_data,
                       order.num_bits * porder->procs->bit_data_elt_size))
                continue;
            /* We have a match. */
            break;
        }
        gx_ht_order_release(&order, mem, false);
    }
 notrec:
    if (i == countof(ht_functions)) {
        /* Create and write a Function for the spot function. */
        code = pdf_write_spot_function(pdev, porder, &spot_id);
        if (code < 0)
            return code;
    }
    *pid = pdf_begin_separate(pdev, resourceHalftone);
    s = pdev->strm;
    /* Use the original, requested frequency and angle. */
    pprintg2(s, "<</Type/Halftone/HalftoneType 1/Frequency %g/Angle %g",
             psht->screen.frequency, psht->screen.angle);
    if (i < countof(ht_functions))
        pprints1(s, "/SpotFunction/%s", ht_functions[i].fname);
    else
        pprintld1(s, "/SpotFunction %ld 0 R", spot_id);
    stream_puts(s, trs);
    if (psht->accurate_screens)
        stream_puts(s, "/AccurateScreens true");
    stream_puts(s, ">>\n");
    return pdf_end_separate(pdev, resourceHalftone);
}
static int
pdf_write_screen_halftone(gx_device_pdf *pdev, const gs_screen_halftone *psht,
                          const gx_ht_order *porder, long *pid)
{
    gs_spot_halftone spot;

    spot.screen = *psht;
    spot.accurate_screens = false;
    spot.transfer = 0;
    spot.transfer_closure.proc = 0;
    return pdf_write_spot_halftone(pdev, &spot, porder, pid);
}
static int
pdf_write_colorscreen_halftone(gx_device_pdf *pdev,
                               const gs_colorscreen_halftone *pcsht,
                               const gx_device_halftone *pdht, long *pid)
{
    int i;
    stream *s;
    long ht_ids[4];

    for (i = 0; i < pdht->num_comp ; ++i) {
        int code = pdf_write_screen_halftone(pdev, &pcsht->screens.indexed[i],
                                             &pdht->components[i].corder,
                                             &ht_ids[i]);
        if (code < 0)
            return code;
    }
    *pid = pdf_begin_separate(pdev, resourceHalftone);
    s = pdev->strm;
    /* Use Black, Gray as the Default unless we are in RGB colormodel */
    /* (num_comp < 4) in which case we use Green (arbitrarily) */
    pprintld1(s, "<</Type/Halftone/HalftoneType 5/Default %ld 0 R\n",
              pdht->num_comp > 3 ? ht_ids[3] : ht_ids[1]);
    pprintld2(s, "/Red %ld 0 R/Cyan %ld 0 R", ht_ids[0], ht_ids[0]);
    pprintld2(s, "/Green %ld 0 R/Magenta %ld 0 R", ht_ids[1], ht_ids[1]);
    pprintld2(s, "/Blue %ld 0 R/Yellow %ld 0 R", ht_ids[2], ht_ids[2]);
    if (pdht->num_comp > 3)
    pprintld2(s, "/Gray %ld 0 R/Black %ld 0 R", ht_ids[3], ht_ids[3]);
    stream_puts(s, ">>\n");
    return pdf_end_separate(pdev, resourceHalftone);
}

#define CHECK(expr)\
  BEGIN if ((code = (expr)) < 0) return code; END

static int
pdf_write_threshold_halftone(gx_device_pdf *pdev,
                             const gs_threshold_halftone *ptht,
                             const gx_ht_order *porder, long *pid)
{
    char trs[17 + MAX_FN_CHARS + 1];
    pdf_data_writer_t writer;
    int code = pdf_write_transfer(pdev, porder->transfer, "",
                                  trs);

    if (code < 0)
        return code;
    CHECK(pdf_begin_data(pdev, &writer));
    *pid = writer.pres->object->id;
    CHECK(cos_dict_put_c_strings((cos_dict_t *)writer.pres->object,
        "/Type", "/Halftone"));
    CHECK(cos_dict_put_c_strings((cos_dict_t *)writer.pres->object,
        "/HalftoneType", "6"));
    CHECK(cos_dict_put_c_key_int((cos_dict_t *)writer.pres->object,
        "/Width", ptht->width));
    CHECK(cos_dict_put_c_key_int((cos_dict_t *)writer.pres->object,
        "/Height", ptht->height));
    if (*trs != 0)
        CHECK(cos_dict_put_c_strings((cos_dict_t *)writer.pres->object,
            "/TransferFunction", trs));
    stream_write(writer.binary.strm, ptht->thresholds.data, ptht->thresholds.size);
    return pdf_end_data(&writer);
}
static int
pdf_write_threshold2_halftone(gx_device_pdf *pdev,
                              const gs_threshold2_halftone *ptht,
                              const gx_ht_order *porder, long *pid)
{
    char trs[17 + MAX_FN_CHARS + 1];
    stream *s;
    pdf_data_writer_t writer;
    int code = pdf_write_transfer(pdev, porder->transfer, "/TransferFunction",
                                  trs);

    if (code < 0)
        return code;
    CHECK(pdf_begin_data(pdev, &writer));
    *pid = writer.pres->object->id;
    CHECK(cos_dict_put_c_strings((cos_dict_t *)writer.pres->object,
        "/Type", "/Halftone"));
    CHECK(cos_dict_put_c_strings((cos_dict_t *)writer.pres->object,
        "/HalftoneType", "16"));
    CHECK(cos_dict_put_c_key_int((cos_dict_t *)writer.pres->object,
        "/Width", ptht->width));
    CHECK(cos_dict_put_c_key_int((cos_dict_t *)writer.pres->object,
        "/Height", ptht->height));
    if (ptht->width2 && ptht->height2) {
        CHECK(cos_dict_put_c_key_int((cos_dict_t *)writer.pres->object,
            "/Width2", ptht->width2));
        CHECK(cos_dict_put_c_key_int((cos_dict_t *)writer.pres->object,
            "/Height2", ptht->height2));
    }
    if (*trs != 0)
        CHECK(cos_dict_put_c_strings((cos_dict_t *)writer.pres->object,
            "/TransferFunction", trs));
    s = writer.binary.strm;
    if (ptht->bytes_per_sample == 2)
        stream_write(s, ptht->thresholds.data, ptht->thresholds.size);
    else {
        /* Expand 1-byte to 2-byte samples. */
        int i;

        for (i = 0; i < ptht->thresholds.size; ++i) {
            byte b = ptht->thresholds.data[i];

            stream_putc(s, b);
            stream_putc(s, b);
        }
    }
    return pdf_end_data(&writer);
}
static int
pdf_get_halftone_component_index(const gs_multiple_halftone *pmht,
                                 const gx_device_halftone *pdht,
                                 int dht_index)
{
    int j;

    for (j = 0; j < pmht->num_comp; j++)
        if (pmht->components[j].comp_number == dht_index)
            break;
    if (j == pmht->num_comp) {
        /* Look for Default. */
        for (j = 0; j < pmht->num_comp; j++)
            if (pmht->components[j].comp_number == GX_DEVICE_COLOR_MAX_COMPONENTS)
                break;
        if (j == pmht->num_comp)
            return_error(gs_error_undefined);
    }
    return j;
}
static int
pdf_write_multiple_halftone(gx_device_pdf *pdev,
                            const gs_multiple_halftone *pmht,
                            const gx_device_halftone *pdht, long *pid)
{
    stream *s;
    int i, code, last_comp = 0;
    gs_memory_t *mem = pdev->pdf_memory;
    long *ids;
    bool done_Default = false;

    ids = (long *)gs_alloc_byte_array(mem, pmht->num_comp, sizeof(long),
                                      "pdf_write_multiple_halftone");
    if (ids == 0)
        return_error(gs_error_VMerror);
    for (i = 0; i < pdht->num_comp; ++i) {
        const gs_halftone_component *phtc;
        const gx_ht_order *porder;

        code = pdf_get_halftone_component_index(pmht, pdht, i);
        if (code < 0)
            return code;
        if (pmht->components[code].comp_number == GX_DEVICE_COLOR_MAX_COMPONENTS) {
            if (done_Default)
                continue;
            done_Default = true;
        }
        phtc = &pmht->components[code];
        porder = (pdht->components == 0 ? &pdht->order :
                       &pdht->components[i].corder);
        switch (phtc->type) {
        case ht_type_spot:
            code = pdf_write_spot_halftone(pdev, &phtc->params.spot,
                                           porder, &ids[i]);
            break;
        case ht_type_threshold:
            code = pdf_write_threshold_halftone(pdev, &phtc->params.threshold,
                                                porder, &ids[i]);
            break;
        case ht_type_threshold2:
            code = pdf_write_threshold2_halftone(pdev,
                                                 &phtc->params.threshold2,
                                                 porder, &ids[i]);
            break;
        default:
            code = gs_note_error(gs_error_rangecheck);
        }
        if (code < 0) {
            gs_free_object(mem, ids, "pdf_write_multiple_halftone");
            return code;
        }
    }
    *pid = pdf_begin_separate(pdev, resourceHalftone);
    s = pdev->strm;
    stream_puts(s, "<</Type/Halftone/HalftoneType 5\n");
    done_Default = false;
    for (i = 0; i < pdht->num_comp; ++i) {
        const gs_halftone_component *phtc;
        byte *str;
        uint len;
        cos_value_t value;

        code = pdf_get_halftone_component_index(pmht, pdht, i);
        if (code < 0)
            return code;
        if (pmht->components[code].comp_number == GX_DEVICE_COLOR_MAX_COMPONENTS) {
            if (done_Default)
                continue;
            done_Default = true;
        }
        phtc = &pmht->components[code];
        if ((code = pmht->get_colorname_string(pdev->memory, phtc->cname, &str, &len)) < 0 ||
            (code = pdf_string_to_cos_name(pdev, str, len, &value)) < 0)
            return code;
        cos_value_write(&value, pdev);
        gs_free_string(mem, value.contents.chars.data,
                       value.contents.chars.size,
                       "pdf_write_multiple_halftone");
        pprintld1(s, " %ld 0 R\n", ids[i]);
        last_comp = i;
    }
    if (!done_Default) {
        /*
         * BOGUS: Type 5 halftones must contain Default component.
         * Perhaps we have no way to obtain it,
         * because pdht contains ProcessColorModel components only.
         * We copy the last component as Default one.
         */
        pprintld1(s, " /Default %ld 0 R\n", ids[last_comp]);
    }
    stream_puts(s, ">>\n");
    gs_free_object(mem, ids, "pdf_write_multiple_halftone");
    return pdf_end_separate(pdev, resourceHalftone);
}

/*
 * Update the halftone.  This is a separate procedure only for
 * readability.
 */
static int
pdf_update_halftone(gx_device_pdf *pdev, const gs_imager_state *pis,
                    char *hts)
{
    const gs_halftone *pht = pis->halftone;
    const gx_device_halftone *pdht = pis->dev_ht;
    int code;
    long id;

    switch (pht->type) {
    case ht_type_screen:
        code = pdf_write_screen_halftone(pdev, &pht->params.screen,
                                         &pdht->components[0].corder, &id);
        break;
    case ht_type_colorscreen:
        code = pdf_write_colorscreen_halftone(pdev, &pht->params.colorscreen,
                                              pdht, &id);
        break;
    case ht_type_spot:
        code = pdf_write_spot_halftone(pdev, &pht->params.spot,
                                       &pdht->components[0].corder, &id);
        break;
    case ht_type_threshold:
        code = pdf_write_threshold_halftone(pdev, &pht->params.threshold,
                                            &pdht->components[0].corder, &id);
        break;
    case ht_type_threshold2:
        code = pdf_write_threshold2_halftone(pdev, &pht->params.threshold2,
                                             &pdht->components[0].corder, &id);
        break;
    case ht_type_multiple:
    case ht_type_multiple_colorscreen:
        code = pdf_write_multiple_halftone(pdev, &pht->params.multiple,
                                           pdht, &id);
        break;
    default:
        return_error(gs_error_rangecheck);
    }
    if (code < 0)
        return code;
    gs_sprintf(hts, "%ld 0 R", id);
    pdev->halftone_id = pis->dev_ht->id;
    return code;
}

/* ------ Graphics state updating ------ */

static inline cos_dict_t *
resource_dict(pdf_resource_t *pres)
{
    return (cos_dict_t *)pres->object;
}

/* Open an ExtGState. */
static int
pdf_open_gstate(gx_device_pdf *pdev, pdf_resource_t **ppres)
{
    int code;

    if (*ppres)
        return 0;
    /*
     * We write gs command only in stream context.
     * If we are clipped, and the clip path is about to change,
     * the old clipping must be undone before writing gs.
     */
    if (pdev->context != PDF_IN_STREAM) {
        /* We apparently use gs_error_interrupt as a request to change context. */
        return gs_error_interrupt;
    }
    code = pdf_alloc_resource(pdev, resourceExtGState, gs_no_id, ppres, -1L);
    if (code < 0)
        return code;
    cos_become((*ppres)->object, cos_type_dict);
    code = cos_dict_put_c_key_string(resource_dict(*ppres), "/Type", (const byte *)"/ExtGState", 10);
    if (code < 0)
        return code;
    return 0;
}

/* Finish writing an ExtGState. */
int
pdf_end_gstate(gx_device_pdf *pdev, pdf_resource_t *pres)
{
    if (pres) {
        int code = pdf_substitute_resource(pdev, &pres, resourceExtGState, NULL, true);

        if (code < 0)
            return code;
        pres->where_used |= pdev->used_mask;
        code = pdf_open_page(pdev, PDF_IN_STREAM);
        if (code < 0)
            return code;
        code = pdf_add_resource(pdev, pdev->substream_Resources, "/ExtGState", pres);
        if (code < 0)
            return code;
        pprintld1(pdev->strm, "/R%ld gs\n", pdf_resource_id(pres));
        pres->where_used |= pdev->used_mask;
    }
    return 0;
}

/*
 * Update the transfer functions(s).  This is a separate procedure only
 * for readability.
 */
static int
pdf_update_transfer(gx_device_pdf *pdev, const gs_imager_state *pis,
                    char *trs)
{
    int i, pi = -1;
    bool multiple = false, update = false;
    gs_id transfer_ids[4];
    int code = 0;
    const gx_transfer_map *tm[4];

    tm[0] = pis->set_transfer.red;
    tm[1] = pis->set_transfer.green;
    tm[2] = pis->set_transfer.blue;
    tm[3] = pis->set_transfer.gray;
    for (i = 0; i < 4; ++i)
        if (tm[i] != NULL) {
            transfer_ids[i] = tm[i]->id;
            if (pdev->transfer_ids[i] != tm[i]->id)
                update = true;
            if (pi != -1 && transfer_ids[i] != transfer_ids[pi])
                multiple = true;
            pi = i;
        } else
            transfer_ids[i] = -1;
    if (update) {
        int mask;

        if (!multiple) {
            code = pdf_write_transfer(pdev, tm[pi], "", trs);
            if (code < 0)
                return code;
            mask = code == 0;
        } else {
            strcpy(trs, "[");
            mask = 0;
            for (i = 0; i < 4; ++i)
                if (tm[i] != NULL) {
                    code = pdf_write_transfer_map(pdev,
                                                  tm[i],
                                                  0, true, " ", trs + strlen(trs));
                    if (code < 0)
                        return code;
                    mask |= (code == 0) << i;
                }
            strcat(trs, "]");
        }
        memcpy(pdev->transfer_ids, transfer_ids, sizeof(pdev->transfer_ids));
        pdev->transfer_not_identity = mask;
    }
    return code;
}

/*
 * Update the current alpha if necessary.  Note that because Ghostscript
 * stores separate opacity and shape alpha, a rangecheck will occur if
 * both are different from the current setting.
 */
static int
pdf_update_alpha(gx_device_pdf *pdev, const gs_imager_state *pis,
                 pdf_resource_t **ppres)
{
    bool ais;
    double alpha;
    int code;

    if (pdev->state.soft_mask_id != pis->soft_mask_id) {
        char buf[20];

        if (pis->soft_mask_id == 0)
            strcpy(buf, "/None");
        else
            gs_sprintf(buf, "%ld 0 R", pis->soft_mask_id);
        code = pdf_open_gstate(pdev, ppres);
        if (code < 0)
            return code;
        code = cos_dict_put_c_key_string(resource_dict(*ppres),
                    "/SMask", (byte *)buf, strlen(buf));
        if (code < 0)
            return code;
        pdev->state.soft_mask_id = pis->soft_mask_id;
    }
    if (pdev->state.opacity.alpha != pis->opacity.alpha) {
        if (pdev->state.shape.alpha != pis->shape.alpha)
            return_error(gs_error_rangecheck);
        ais = false;
        alpha = pdev->state.opacity.alpha = pis->opacity.alpha;
    } else if (pdev->state.shape.alpha != pis->shape.alpha) {
        ais = true;
        alpha = pdev->state.shape.alpha = pis->shape.alpha;
    } else
        return 0;
    code = pdf_open_gstate(pdev, ppres);
    if (code < 0)
        return code;
    code = cos_dict_put_c_key_bool(resource_dict(*ppres), "/AIS", ais);
    if (code < 0)
        return code;
    /* we never do the 'both' operations (b, B, b*, B*) so we set both */
    /* CA and ca the same so that we stay in sync with state.*.alpha   */
    code = cos_dict_put_c_key_real(resource_dict(*ppres), "/CA", alpha);
    if (code < 0)
        return code;
    return cos_dict_put_c_key_real(resource_dict(*ppres), "/ca", alpha);
}

/*
 * Update the graphics subset common to all high-level drawing operations.
 */
int
pdf_prepare_drawing(gx_device_pdf *pdev, const gs_imager_state *pis,
                    pdf_resource_t **ppres)
{
    int code = 0;
    int bottom;

    if (pdev->CompatibilityLevel >= 1.4) {
        if (pdev->state.blend_mode != pis->blend_mode) {
            static const char *const bm_names[] = { GS_BLEND_MODE_NAMES };
            char buf[20];

            code = pdf_open_gstate(pdev, ppres);
            if (code < 0)
                return code;
            buf[0] = '/';
            strncpy(buf + 1, bm_names[pis->blend_mode], sizeof(buf) - 2);
            code = cos_dict_put_string_copy(resource_dict(*ppres), "/BM", buf);
            if (code < 0)
                return code;
            pdev->state.blend_mode = pis->blend_mode;
        }
        code = pdf_update_alpha(pdev, pis, ppres);
        if (code < 0)
            return code;
    } else {
        /*
         * If the graphics state calls for any transparency functions,
         * we can't represent them, so return a rangecheck.
         */
        if (pis->opacity.alpha != 1 ||
            pis->shape.alpha != 1)
            return_error(gs_error_rangecheck);
    }
    /*
     * We originally thought the remaining items were only needed for
     * fill and stroke, but in fact they are needed for images as well.
     */
    /*
     * Update halftone, transfer function, black generation, undercolor
     * removal, halftone phase, overprint mode, smoothness, blend mode, text
     * knockout.
     */
    bottom = (pdev->ResourcesBeforeUsage ? 1 : 0);
    /* When ResourcesBeforeUsage != 0, one sbstack element
       appears from the page contents stream. */
    if (pdev->sbstack_depth == bottom) {
        gs_int_point phase, dev_phase;
        char hts[5 + MAX_FN_CHARS + 1],
            trs[5 + MAX_FN_CHARS * 4 + 6 + 1],
            bgs[5 + MAX_FN_CHARS + 1],
            ucrs[6 + MAX_FN_CHARS + 1];

        hts[0] = trs[0] = bgs[0] = ucrs[0] = 0;
        if (pdev->params.PreserveHalftoneInfo &&
            pdev->halftone_id != pis->dev_ht->id &&
            !pdev->PDFX
            ) {
            code = pdf_update_halftone(pdev, pis, hts);
            if (code < 0)
                return code;
        }
        if (pdev->params.TransferFunctionInfo == tfi_Preserve &&
            !pdev->PDFX && pdev->PDFA == 0
            ) {
            code = pdf_update_transfer(pdev, pis, trs);
            if (code < 0)
                return code;
        }
        if (pdev->params.UCRandBGInfo == ucrbg_Preserve) {
            if (pis->black_generation && pdev->black_generation_id != pis->black_generation->id) {
                code = pdf_write_transfer_map(pdev, pis->black_generation,
                                              0, false, "", bgs);
                if (code < 0)
                    return code;
                pdev->black_generation_id = pis->black_generation->id;
            }
            if (pis->undercolor_removal && pdev->undercolor_removal_id != pis->undercolor_removal->id) {
                code = pdf_write_transfer_map(pdev, pis->undercolor_removal,
                                              -1, false, "", ucrs);
                if (code < 0)
                    return code;
                pdev->undercolor_removal_id = pis->undercolor_removal->id;
            }
        }
        if (hts[0] || trs[0] || bgs[0] || ucrs[0]) {
            code = pdf_open_gstate(pdev, ppres);
            if (code < 0)
                return code;
        }
        if (hts[0]) {
            code = cos_dict_put_string_copy(resource_dict(*ppres), "/HT", hts);
            if (code < 0)
                return code;
        }
        if (trs[0]) {
            code = cos_dict_put_string_copy(resource_dict(*ppres), "/TR", trs);
            if (code < 0)
                return code;
        }
        if (bgs[0]) {
            code = cos_dict_put_string_copy(resource_dict(*ppres), "/BG", bgs);
            if (code < 0)
                return code;
        }
        if (ucrs[0]) {
            code = cos_dict_put_string_copy(resource_dict(*ppres), "/UCR", ucrs);
            if (code < 0)
                return code;
        }
        if (!pdev->PDFX) {
            gs_currentscreenphase_pis(pis, &phase, 0);
            gs_currentscreenphase_pis(&pdev->state, &dev_phase, 0);
            if (dev_phase.x != phase.x || dev_phase.y != phase.y) {
                char buf[sizeof(int) * 3 + 5];

                code = pdf_open_gstate(pdev, ppres);
                if (code < 0)
                    return code;
                gs_sprintf(buf, "[%d %d]", phase.x, phase.y);
                code = cos_dict_put_string_copy(resource_dict(*ppres), "/HTP", buf);
                if (code < 0)
                    return code;
                gx_imager_setscreenphase(&pdev->state, phase.x, phase.y,
                                         gs_color_select_all);
            }
        }
    }
    if (pdev->CompatibilityLevel >= 1.3 && pdev->sbstack_depth == bottom) {
        if (pdev->overprint_mode != pdev->params.OPM) {
            code = pdf_open_gstate(pdev, ppres);
            if (code < 0)
                return code;
            code = cos_dict_put_c_key_int(resource_dict(*ppres), "/OPM", pdev->params.OPM);
            if (code < 0)
                return code;
            pdev->overprint_mode = pdev->params.OPM;
        }
        if (pdev->state.smoothness != pis->smoothness) {
            code = pdf_open_gstate(pdev, ppres);
            if (code < 0)
                return code;
            code = cos_dict_put_c_key_real(resource_dict(*ppres), "/SM", pis->smoothness);
            if (code < 0)
                return code;
            pdev->state.smoothness = pis->smoothness;
        }
        if (pdev->CompatibilityLevel >= 1.4) {
            if (pdev->state.text_knockout != pis->text_knockout) {
                code = pdf_open_gstate(pdev, ppres);
                if (code < 0)
                    return code;
                code = cos_dict_put_c_key_bool(resource_dict(*ppres), "/TK", pis->text_knockout);
                if (code < 0)
                    return code;
                pdev->state.text_knockout = pis->text_knockout;
            }
        }
    }
    return code;
}

/* Update the graphics state for filling. */
int
pdf_try_prepare_fill(gx_device_pdf *pdev, const gs_imager_state *pis)
{
    pdf_resource_t *pres = 0;
    int code = pdf_prepare_drawing(pdev, pis, &pres);

    if (code < 0)
        return code;
    /* Update overprint. */
    if (pdev->params.PreserveOverprintSettings &&
        (pdev->fill_overprint != pis->overprint ||
        pdev->font3) &&	!pdev->skip_colors
        ) {
        code = pdf_open_gstate(pdev, &pres);
        if (code < 0)
            return code;
        /* PDF 1.2 only has a single overprint setting. */
        if (pdev->CompatibilityLevel < 1.3) {
            code = cos_dict_put_c_key_bool(resource_dict(pres), "/OP", pis->overprint);
            if (code < 0)
                return code;
            pdev->stroke_overprint = pis->overprint;
        } else {
            code = cos_dict_put_c_key_bool(resource_dict(pres), "/op", pis->overprint);
            if (code < 0)
                return code;
        }
        pdev->fill_overprint = pis->overprint;
    }
    return pdf_end_gstate(pdev, pres);
}
int
pdf_prepare_fill(gx_device_pdf *pdev, const gs_imager_state *pis)
{
    int code;

    if (pdev->context != PDF_IN_STREAM) {
        code = pdf_try_prepare_fill(pdev, pis);
        if (code != gs_error_interrupt) /* See pdf_open_gstate */
            return code;
        code = pdf_open_contents(pdev, PDF_IN_STREAM);
        if (code < 0)
            return code;
    }
    return pdf_try_prepare_fill(pdev, pis);
}

/* Update the graphics state for stroking. */
static int
pdf_try_prepare_stroke(gx_device_pdf *pdev, const gs_imager_state *pis)
{
    pdf_resource_t *pres = 0;
    int code = pdf_prepare_drawing(pdev, pis, &pres);

    if (code < 0)
        return code;
    /* Update overprint, stroke adjustment. */
    if (pdev->params.PreserveOverprintSettings &&
        pdev->stroke_overprint != pis->overprint &&
        !pdev->skip_colors
        ) {
        code = pdf_open_gstate(pdev, &pres);
        if (code < 0)
            return code;
        code = cos_dict_put_c_key_bool(resource_dict(pres), "/OP", pis->overprint);
        if (code < 0)
            return code;
        pdev->stroke_overprint = pis->overprint;
        if (pdev->CompatibilityLevel < 1.3) {
            /* PDF 1.2 only has a single overprint setting. */
            pdev->fill_overprint = pis->overprint;
        } else {
            /* According to PDF>=1.3 spec, OP also sets op,
               if there is no /op in same garphic state object.
               We don't write /op, so monitor the viewer's state here : */
            pdev->fill_overprint = pis->overprint;
        }
    }
    if (pdev->state.stroke_adjust != pis->stroke_adjust) {
        code = pdf_open_gstate(pdev, &pres);
        if (code < 0)
            return code;
        code = cos_dict_put_c_key_bool(resource_dict(pres), "/SA", pis->stroke_adjust);
        if (code < 0)
            return code;
        pdev->state.stroke_adjust = pis->stroke_adjust;
    }
    return pdf_end_gstate(pdev, pres);
}
int
pdf_prepare_stroke(gx_device_pdf *pdev, const gs_imager_state *pis)
{
    int code;

    if (pdev->context != PDF_IN_STREAM) {
        code = pdf_try_prepare_stroke(pdev, pis);
        if (code != gs_error_interrupt) /* See pdf_open_gstate */
            return code;
        code = pdf_open_contents(pdev, PDF_IN_STREAM);
        if (code < 0)
            return code;
    }
    return pdf_try_prepare_stroke(pdev, pis);
}

/* Update the graphics state for an image other than an ImageType 1 mask. */
int
pdf_prepare_image(gx_device_pdf *pdev, const gs_imager_state *pis)
{
    /*
     * As it turns out, this requires updating the same parameters as for
     * filling.
     */
    return pdf_prepare_fill(pdev, pis);
}

/* Update the graphics state for an ImageType 1 mask. */
int
pdf_prepare_imagemask(gx_device_pdf *pdev, const gs_imager_state *pis,
                      const gx_drawing_color *pdcolor)
{
    int code = pdf_prepare_image(pdev, pis);

    if (code < 0)
        return code;
    return pdf_set_drawing_color(pdev, pis, pdcolor, &pdev->saved_fill_color,
                                 &pdev->fill_used_process_color,
                                 &psdf_set_fill_color_commands);
}
