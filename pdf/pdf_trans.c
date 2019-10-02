/* Copyright (C) 2001-2018 Artifex Software, Inc.
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

/* Transparency support */

#include "pdf_int.h"
#include "pdf_stack.h"
#include "pdf_trans.h"
#include "pdf_dict.h"
#include "pdf_colour.h"
#include "pdf_gstate.h"
#include "pdf_array.h"
#include "pdf_image.h"
#include "pdf_device.h"
#include "pdf_misc.h"

#include "gstparam.h"

/* (see pdf_draw.ps/execmaskgroup) */
static int pdfi_trans_set_mask(pdf_context *ctx, pdfi_int_gstate *igs, int colorindex)
{
    int code = 0, code1 = 0;
    pdf_dict *SMask = igs->SMask;
    gs_color_space *pcs = NULL;
    gs_rect bbox;
    gs_transparency_mask_params_t params;
    pdf_array *BBox = NULL;
    pdf_array *Matrix = NULL;
    pdf_array *a = NULL;
    pdf_array *BC = NULL;
    pdf_dict *G_dict = NULL;
    pdf_dict *Group = NULL;
    pdf_name *n = NULL;
    pdf_name *S = NULL;
    pdf_obj *CS = NULL;
    double f;
    gs_matrix save_matrix, GroupMat, group_Matrix;
    gs_transparency_mask_subtype_t subtype = TRANSPARENCY_MASK_Luminosity;
    pdf_bool *Processed = NULL;
    pdf_obj *Key = NULL;

    dbgmprintf(ctx->memory, "pdfi_trans_set_mask (.execmaskgroup) BEGIN\n");

    /* Following the logic of the ps code, cram a /Processed key in the SMask dict to
     * track whether it's already been processed.
     */
    code = pdfi_dict_knownget_type(ctx, SMask, "Processed", PDF_BOOL, (pdf_obj **)&Processed);
    if (code > 0 && Processed->value) {
        dbgmprintf(ctx->memory, "SMask already built, skipping\n");
        goto exit;
    }
    /* If /Processed not in the dict, put it there */
    if (code == 0) {
        /* the cleanup at end of this routine assumes both Key and Processed have a ref */
        code = pdfi_alloc_object(ctx, PDF_BOOL, 0, (pdf_obj **)&Processed);
        if (code < 0)
            goto exit;
        Processed->value = false;
        /* pdfi_alloc_object() doesn't grab a ref */
        pdfi_countup(Processed);
        /* pdfi_make_name() does grab a ref, so no need to countup */
        code = pdfi_make_name(ctx, (byte *)"Processed", strlen("Processed"), &Key);
        if (code < 0)
            goto exit;
        code = pdfi_dict_put(SMask, Key, (pdf_obj *)Processed);
        if (code < 0)
            goto exit;
    }

    /* See pdf1.7 pg 553 (pain in the butt to find this!) */
    code = pdfi_dict_knownget_type(ctx, SMask, "Type", PDF_NAME, (pdf_obj **)&n);
    if (code == 0 || (code > 0 && pdfi_name_is(n, "Mask"))) {
        /* G is transparency group XObject (required) */
        code = pdfi_dict_knownget_type(ctx, SMask, "G", PDF_DICT, (pdf_obj **)&G_dict);
        if (code <= 0) {
            dmprintf(ctx->memory, "WARNING: Missing 'G' in SMask, ignoring.\n");
            pdfi_trans_end_smask_notify(ctx);
            code = 0;
            goto exit;
        }
        /* S is a subtype name (required) */
        code = pdfi_dict_knownget_type(ctx, SMask, "S", PDF_NAME, (pdf_obj **)&S);
        if (code <= 0) {
            dmprintf(ctx->memory, "WARNING: Missing 'S' in SMask (defaulting to Luminosity)\n");
        }
        if (pdfi_name_is(S, "Luminosity")) {
            subtype = TRANSPARENCY_MASK_Luminosity;
        } else if (pdfi_name_is(S, "Alpha")) {
            subtype = TRANSPARENCY_MASK_Alpha;
        } else {
            dmprintf(ctx->memory, "WARNING: Unknown subtype 'S' in SMask (defaulting to Luminosity)\n");
        }

        /* BC is Background Color array (Optional) */
        code = pdfi_dict_knownget_type(ctx, SMask, "BC", PDF_ARRAY, (pdf_obj **)&BC);
        if (code < 0)
            goto exit;

        code = pdfi_dict_knownget_type(ctx, G_dict, "Matte", PDF_ARRAY, (pdf_obj **)&a);
        if (code > 0) {
            int ix;

            for (ix = 0; ix < pdfi_array_size(a); ix++) {
                code = pdfi_array_get_number(ctx, a, (uint64_t)ix, &f);
                if (code < 0)
                    break;
                params.Matte[ix] = f;
            }
            if (ix >= pdfi_array_size(a))
                params.Matte_components = pdfi_array_size(a);
            else
                params.Matte_components = 0;
        }

        code = pdfi_dict_knownget_type(ctx, G_dict, "BBox", PDF_ARRAY, (pdf_obj **)&BBox);
        if (code < 0)
            goto exit;
        code = pdfi_array_to_gs_rect(ctx, BBox, &bbox);
        if (code < 0)
            goto exit;

        gs_trans_mask_params_init(&params, subtype);
        params.replacing = true;

        /* Need to set just the ctm (GroupMat) from the saved GroupGState, to
           have gs_begin_transparency_mask work correctly.  Or at least that's
           what the PS code comments claim (see pdf_draw.ps/.execmaskgroup)
        */
        gs_currentmatrix(ctx->pgs, &save_matrix);
        gs_currentmatrix(igs->GroupGState, &GroupMat);
        gs_setmatrix(ctx->pgs, &GroupMat);

        code = pdfi_dict_knownget_type(ctx, G_dict, "Matrix", PDF_ARRAY, (pdf_obj **)&Matrix);
        if (code < 0)
            goto exit;
        code = pdfi_array_to_gs_matrix(ctx, Matrix, &group_Matrix);
        if (code < 0)
            goto exit;

        /* Transform the BBox by the Matrix */
        pdfi_bbox_transform(ctx, &bbox, &group_Matrix);

        /* CS is in the dict "Group" inside the dict "G" */
        /* TODO: Not sure if this is a required thing or just one possibility */
        code = pdfi_dict_knownget_type(ctx, G_dict, "Group", PDF_DICT, (pdf_obj **)&Group);
        if (code < 0)
            goto exit;
        if (code > 0) {
            /* TODO: Stuff with colorspace, see .execmaskgroup */
            code = pdfi_dict_knownget(ctx, Group, "CS", &CS);
            if (code < 0)
                goto exit;
            if (code > 0) {
                code = pdfi_create_colorspace(ctx, CS, (pdf_dict *)ctx->main_stream,
                                              ctx->CurrentPageDict, &pcs, false);
                params.ColorSpace = pcs;
                if (code < 0)
                    goto exit;
            } else {
                /* Inherit current colorspace */
                params.ColorSpace = ctx->pgs->color[colorindex].color_space;
            }
        } else {
            /* TODO: Is this an error or what?
               Inherit current colorspace
            */
            params.ColorSpace = ctx->pgs->color[colorindex].color_space;
        }

        /* If there's a BC, put it in the params */
        if (BC) {
            int i;
            double num;
            for (i=0; i<pdfi_array_size(BC); i++) {
                if (i > GS_CLIENT_COLOR_MAX_COMPONENTS)
                    break;
                code = pdfi_array_get_number(ctx, BC, i, &num);
                if (code < 0)
                    break;
                params.Background[i] = (float)num;
            }
            params.Background_components = pdfi_array_size(BC);

            /* TODO: Not sure how to handle this...  recheck PS code (pdf_draw.ps/gssmask) */
            /* This should be "currentgray" for the color that we put in params.ColorSpace,
             * It looks super-convoluted to actually get this value.  Really?
             * (see zcurrentgray())
             */
            params.GrayBackground = 0; /* TODO */
        }

        code = gs_begin_transparency_mask(ctx->pgs, &params, &bbox, true);
        if (code < 0)
            goto exit;

        code = pdfi_form_execgroup(ctx, ctx->CurrentPageDict, G_dict, igs->GroupGState, &group_Matrix);
        code1 = gs_end_transparency_mask(ctx->pgs, colorindex);
        if (code != 0)
            code = code1;

        /* Put back the matrix (we couldn't just rely on gsave/grestore for whatever reason,
         * according to PS code anyway...
         */
        if (Processed)
            Processed->value = true;
        gs_setmatrix(ctx->pgs, &save_matrix);
    } else {
        /* take action on a non-/Mask entry. What does this mean ? What do we need to do */
        dmprintf(ctx->memory, "Warning: Type is not /Mask, entry ignored in pdfi_set_trans_mask\n");
    }

 exit:
    if (pcs)
        rc_decrement_cs(pcs, "pdfi_trans_set_mask");
    pdfi_countdown(n);
    pdfi_countdown(S);
    pdfi_countdown(Group);
    pdfi_countdown(G_dict);
    pdfi_countdown(a);
    pdfi_countdown(BC);
    pdfi_countdown(BBox);
    pdfi_countdown(Matrix);
    pdfi_countdown(CS);
    pdfi_countdown(Processed);
    pdfi_countdown(Key);
    dbgmprintf(ctx->memory, "pdfi_trans_set_mask (.execmaskgroup) END\n");
    return code;
}

static int pdfi_transparency_group_common(pdf_context *ctx, pdf_dict *page_dict, pdf_dict *group_dict, gs_rect *bbox, pdf14_compositor_operations group_type)
{
    gs_transparency_group_params_t params;
    pdf_obj *CS = NULL;
    bool b;
    int code;

    gs_trans_group_params_init(&params);
    //    gs_setopacityalpha(ctx->pgs, ctx->pgs->fillconstantalpha);

    /* It seems the flag for Isolated is /I */
    code = pdfi_dict_get_bool(ctx, group_dict, "I", &b);
    if (code < 0 && code != gs_error_undefined)
        return_error(code);
    if (code == gs_error_undefined)
        params.Isolated = false;
    else
        params.Isolated = b;

    /* It seems the flag for Knockout is /K */
    code = pdfi_dict_get_bool(ctx, group_dict, "K", &b);
    if (code < 0 && code != gs_error_undefined)
        return_error(code);
    if (code == gs_error_undefined)
        params.Knockout = false;
    else
        params.Knockout = b;

    params.image_with_SMask = false;
    params.ColorSpace = NULL;

    code = pdfi_dict_knownget(ctx, group_dict, "CS", &CS);
    if (code == 0) {
        /* Didn't find a /CS key, try again using /ColorSpace */
        code = pdfi_dict_knownget(ctx, group_dict, "ColorSpace", &CS);
    }
    if (code > 0) {
        code = pdfi_setcolorspace(ctx, CS, group_dict, page_dict);
        pdfi_countdown(CS);
        if (code < 0)
            return code;
        params.ColorSpace = gs_currentcolorspace(ctx->pgs);
    } else {
        params.ColorSpace = NULL;
    }
    if (code < 0)
        return_error(code);

    return gs_begin_transparency_group(ctx->pgs, &params, (const gs_rect *)bbox, group_type);
}

int pdfi_trans_begin_simple_group(pdf_context *ctx, bool stroked_bbox, bool isolated, bool knockout)
{
    gs_transparency_group_params_t params;
    gs_rect bbox;
    int code;

    gs_trans_group_params_init(&params);
    params.Isolated = isolated;
    params.Knockout = knockout;

    code = pdfi_get_current_bbox(ctx, &bbox, stroked_bbox);
    if (code < 0)
        return code;

    code = gs_begin_transparency_group(ctx->pgs, &params, &bbox, PDF14_BEGIN_TRANS_GROUP);
    if (code >=  0)
        ctx->current_stream_save.group_depth++;
    return code;
}

int pdfi_trans_begin_page_group(pdf_context *ctx, pdf_dict *page_dict, pdf_dict *group_dict)
{
    gs_rect bbox;
    int code;

    if (group_dict == NULL)
        return_error(gs_error_undefined);

    code = pdfi_gsave(ctx);
    bbox.p.x = ctx->PageSize[0];
    bbox.p.y = ctx->PageSize[1];
    bbox.q.x = ctx->PageSize[2];
    bbox.q.y = ctx->PageSize[3];

    code = pdfi_transparency_group_common(ctx, page_dict, group_dict, &bbox, PDF14_BEGIN_TRANS_PAGE_GROUP);
    if (code < 0)
        pdfi_grestore(ctx);
    else
        ctx->current_stream_save.group_depth++;

    return code;
}

int pdfi_trans_begin_form_group(pdf_context *ctx, pdf_dict *page_dict, pdf_dict *form_dict)
{
    pdf_dict *group_dict = NULL;
    gs_rect bbox;
    pdf_array *BBox = NULL;
    int code;

    code = pdfi_dict_get_type(ctx, form_dict, "Group", PDF_DICT, (pdf_obj **)&group_dict);
    if (code < 0)
        return_error(code);

    code = pdfi_gsave(ctx);
    code = pdfi_dict_knownget_type(ctx, form_dict, "BBox", PDF_ARRAY, (pdf_obj **)&BBox);
    if (code < 0)
        goto exit;
    if (code > 0) {
        code = pdfi_array_to_gs_rect(ctx, BBox, &bbox);
        if (code < 0)
            goto exit;
    } else {
        bbox.p.x = 0;
        bbox.p.y = 0;
        bbox.q.x = 0;
        bbox.q.y = 0;
    }

    code = pdfi_transparency_group_common(ctx, page_dict, group_dict, &bbox, PDF14_BEGIN_TRANS_GROUP);
    if (code < 0)
        pdfi_grestore(ctx);
    else
        ctx->current_stream_save.group_depth++;

 exit:
    pdfi_countdown(BBox);
    pdfi_countdown(group_dict);
    return code;
}


int pdfi_trans_end_group(pdf_context *ctx)
{
    int code;

    code = gs_end_transparency_group(ctx->pgs);
    if (code < 0)
        pdfi_grestore(ctx);
    else
        code = pdfi_grestore(ctx);

    ctx->current_stream_save.group_depth--;

    return code;
}

/* Ends group with no grestore */
int pdfi_trans_end_simple_group(pdf_context *ctx)
{
    int code;

    code = gs_end_transparency_group(ctx->pgs);
    ctx->current_stream_save.group_depth--;

    return code;
}


int pdfi_trans_begin_isolated_group(pdf_context *ctx, bool image_with_SMask)
{
    gs_transparency_group_params_t params;
    gs_rect bbox;

    gs_trans_group_params_init(&params);

    params.ColorSpace = NULL;
    params.Isolated = true;
    params.Knockout = false;
    params.image_with_SMask = image_with_SMask;
    bbox.p.x = 0;
    bbox.p.y = 0;
    bbox.q.x = 1;
    bbox.q.y = 1;

    return gs_begin_transparency_group(ctx->pgs, &params, &bbox, PDF14_BEGIN_TRANS_GROUP);
}

int pdfi_trans_end_isolated_group(pdf_context *ctx)
{
    return gs_end_transparency_group(ctx->pgs);
}


/* This notifies the compositor that we're done with an smask.  Seems hacky.
 * See pdf_draw.ps/doimagesmask.
 */
int pdfi_trans_end_smask_notify(pdf_context *ctx)
{
    gs_transparency_mask_params_t params;
    gs_rect bbox;

    gs_trans_mask_params_init(&params, TRANSPARENCY_MASK_None);
    params.replacing = true;

    bbox.p.x = 0;
    bbox.p.y = 0;
    bbox.q.x = 0;
    bbox.q.y = 0;

    return gs_begin_transparency_mask(ctx->pgs, &params, &bbox, false);
}

/* Setup whether or not we need to support overprint (for device)
 * Check for:
 *   1) whether or not it is a CMYK device, and
 *   2) whether it is a device that has transparency support
 * Based on pdf_main.ps/pdfshowpage_finish
 */
void pdfi_trans_set_needs_OP(pdf_context *ctx)
{
    bool is_cmyk;
    bool have_transparency = false;

    /* PS code checks for >= 4 components... */
    is_cmyk = ctx->pgs->device->color_info.num_components >= 4;

    have_transparency = pdfi_device_check_param_bool(ctx->pgs->device, "HaveTransparency");

    if (!is_cmyk || have_transparency)
        ctx->page_needs_OP = false;
    else
        ctx->page_needs_OP = true;
    dbgmprintf1(ctx->memory, "Page %s Overprint\n", ctx->page_needs_OP ?
                "NEEDS" : "does NOT NEED");
}

/* Figures out if current colorspace is okay for Overprint (see pdf_ops.ps/okOPcs and setupOPtrans) */
static bool pdfi_trans_okOPcs(pdf_context *ctx)
{
    gs_color_space_index csi;

    csi = pdfi_currentcolorspace(ctx, 0);

    switch (csi) {
    case gs_color_space_index_DeviceGray:
    case gs_color_space_index_DeviceCMYK:
    case gs_color_space_index_DeviceN:
    case gs_color_space_index_Separation:
        /* These are colorspaces that don't require special handling for overprint.
         * (pdf1.7 pg 259,578 may apply)
         * According to mvrhel, DeviceGray should also be included (see comment in gx_set_overprint_ICC()).
         * Sample: 030_Gray_K_black_OP_x1a.pdf (??)
         */
        dbgmprintf1(ctx->memory, "Colorspace is %d, OKAY for OVERPRINT\n", csi);
        return true;
    default:
        dbgmprintf1(ctx->memory, "Colorspace is %d, NOT OKAY for OVERPRINT\n", csi);
        return false;
    }

    return false;
}

int pdfi_trans_setup(pdf_context *ctx, pdfi_trans_state_t *state,
                     pdfi_transparency_caller_t caller, double alpha)
{
    pdfi_int_gstate *igs = (pdfi_int_gstate *)ctx->pgs->client_data;
    int code;
    bool stroked_bbox;
    bool current_overprint;
    bool okOPcs = false;
    bool ChangeBM = false;
    gs_blend_mode_t mode;
    bool need_group = false;

    memset(state, 0, sizeof(*state));

    if (!ctx->page_has_transparency)
        return 0;

    if (ctx->page_needs_OP) {
        okOPcs = pdfi_trans_okOPcs(ctx);
        if (okOPcs) {
            if (caller == TRANSPARENCY_Caller_Stroke)
                current_overprint = gs_currentstrokeoverprint(ctx->pgs);
            else
                current_overprint = gs_currentfilloverprint(ctx->pgs);
            ChangeBM = current_overprint;
            mode = gs_currentblendmode(ctx->pgs);
            if (mode != BLEND_MODE_Normal && mode != BLEND_MODE_Compatible)
                need_group = ChangeBM;
            else
                need_group = false;
        } else {
            need_group = false;
        }
    } else {
        if (caller == TRANSPARENCY_Caller_Image || igs->SMask == NULL)
            need_group = false;
        else
            need_group = true;
    }


    code = pdfi_trans_set_params(ctx, alpha);
    if (code != 0)
        return 0;

    if (!need_group && !ChangeBM)
        return 0;

    /* TODO: error handling... */
    if (need_group) {
        stroked_bbox = (caller == TRANSPARENCY_Caller_Stroke);
        code = pdfi_trans_begin_simple_group(ctx, stroked_bbox, true, false);
        state->GroupPushed = true;
        state->saveOA = gs_currentopacityalpha(ctx->pgs);
        state->saveSA = gs_currentshapealpha(ctx->pgs);
        code = gs_setopacityalpha(ctx->pgs, 1.0);
        code = gs_setshapealpha(ctx->pgs, 1.0);
    }
    if (ChangeBM) {
        state->saveBM = mode;
        state->ChangeBM = true;
        code = gs_setblendmode(ctx->pgs, BLEND_MODE_CompatibleOverprint);
    }
    return code;
}

int pdfi_trans_teardown(pdf_context *ctx, pdfi_trans_state_t *state)
{
    int code = 0;

    if (!ctx->page_has_transparency)
        return 0;

    if (state->GroupPushed) {
        code = pdfi_trans_end_simple_group(ctx);
        code = gs_setopacityalpha(ctx->pgs, state->saveOA);
        code = gs_setshapealpha(ctx->pgs, state->saveSA);
    }

    if (gs_currentblendmode(ctx->pgs) == BLEND_MODE_CompatibleOverprint)
        code = gs_setblendmode(ctx->pgs, state->saveBM);

    return code;
}

int pdfi_trans_set_params(pdf_context *ctx, double alpha)
{
    pdfi_int_gstate *igs = (pdfi_int_gstate *)ctx->pgs->client_data;
    gs_transparency_channel_selector_t csel;

    if (ctx->page_has_transparency) {
        if (gs_getalphaisshape(ctx->pgs)) {
            gs_setshapealpha(ctx->pgs, alpha);
            gs_setopacityalpha(ctx->pgs, 1.0);
            csel = TRANSPARENCY_CHANNEL_Shape;
        } else {
            gs_setshapealpha(ctx->pgs, 1.0);
            gs_setopacityalpha(ctx->pgs, alpha);
            csel = TRANSPARENCY_CHANNEL_Opacity;
        }
        if (igs->SMask) {
            pdfi_trans_set_mask(ctx, igs, csel);
        }
    }

    return 0;
}
