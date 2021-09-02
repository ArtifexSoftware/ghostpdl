/* Copyright (C) 2019-2021 Artifex Software, Inc.
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
#include "pdf_func.h"

#include "gstparam.h"
#include "gsicc_manage.h"       /* For gs_currentoverrideicc() */
#include "gscoord.h"            /* For gs_setmatrix()*/
#include "gsstate.h"            /* For gs_currentstrokeoverprint() and others */
#include "gspath.h"             /* For gs_clippath() */

/* Implement the TransferFunction using a Function. */
static int
pdfi_tf_using_function(double in_val, float *out, void *proc_data)
{
    float in = in_val;
    gs_function_t *const pfn = proc_data;

    return gs_function_evaluate(pfn, &in, out);
}

static void
pdfi_set_GrayBackground(gs_transparency_mask_params_t *params)
{
    float num;

    /* This uses definition from PLRM2 6.2.1 and 6.2.2 */
    /* TODO: We are assuming 3 components is RGB and 4 components is CMYK,
       which might not strictly be true?  But it provides a better
       estimated value than doing nothing.
    */
    switch(params->Background_components) {
    case 3:
        /* RGB: currentgray = 0.3*R + 0.59*G + 0.11*B */
        params->GrayBackground = (0.3 * params->Background[0] +
                                  0.59 * params->Background[1] +
                                  0.11 * params->Background[2]);
        break;
    case 4:
        /* CMYK: currentgray = 1.0 – min (1.0, .3*C + .59*M + .11*Y + K)
        */
        num = 0.3*params->Background[0] + 0.59*params->Background[1] +
            0.11*params->Background[2] + params->Background[3];
        if (num > 1)
            num = 1;
        params->GrayBackground = 1 - num;
        break;
    case 1:
        params->GrayBackground = params->Background[0];
        break;
    default:
        /* No clue... */
        params->GrayBackground = 0;
    }
}

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
    pdf_stream *G_stream = NULL;
    pdf_dict *G_stream_dict = NULL;
    pdf_dict *Group = NULL;
    pdf_obj *TR = NULL;
    gs_function_t *gsfunc = NULL;
    pdf_name *n = NULL;
    pdf_name *S = NULL;
    pdf_obj *CS = NULL;
    double f;
    gs_matrix save_matrix, GroupMat, group_Matrix;
    gs_transparency_mask_subtype_t subtype = TRANSPARENCY_MASK_Luminosity;
    pdf_bool *Processed = NULL;
    bool save_OverrideICC = gs_currentoverrideicc(ctx->pgs);

#if DEBUG_TRANSPARENCY
    dbgmprintf(ctx->memory, "pdfi_trans_set_mask (.execmaskgroup) BEGIN\n");
#endif
    memset(&params, 0, sizeof(params));

    /* Following the logic of the ps code, cram a /Processed key in the SMask dict to
     * track whether it's already been processed.
     */
    code = pdfi_dict_knownget_type(ctx, SMask, "Processed", PDF_BOOL, (pdf_obj **)&Processed);
    if (code > 0 && Processed->value) {
#if DEBUG_TRANSPARENCY
        dbgmprintf(ctx->memory, "SMask already built, skipping\n");
#endif
        goto exit;
    }

    gs_setoverrideicc(ctx->pgs, true);

    /* If /Processed not in the dict, put it there */
    if (code == 0) {
        /* the cleanup at end of this routine assumes Processed has a ref */
        code = pdfi_object_alloc(ctx, PDF_BOOL, 0, (pdf_obj **)&Processed);
        if (code < 0)
            goto exit;
        Processed->value = false;
        /* pdfi_object_alloc() doesn't grab a ref */
        pdfi_countup(Processed);
        code = pdfi_dict_put(ctx, SMask, "Processed", (pdf_obj *)Processed);
        if (code < 0)
            goto exit;
    }

    /* See pdf1.7 pg 553 (pain in the butt to find this!) */
    code = pdfi_dict_knownget_type(ctx, SMask, "Type", PDF_NAME, (pdf_obj **)&n);
    if (code == 0 || (code > 0 && pdfi_name_is(n, "Mask"))) {
        /* G is transparency group XObject (required) */
        code = pdfi_dict_knownget_type(ctx, SMask, "G", PDF_STREAM, (pdf_obj **)&G_stream);
        if (code <= 0) {
            dmprintf(ctx->memory, "WARNING: Missing 'G' in SMask, ignoring.\n");
            pdfi_trans_end_smask_notify(ctx);
            code = 0;
            goto exit;
        }

        code = pdfi_dict_from_obj(ctx, (pdf_obj *)G_stream, &G_stream_dict);
        if (code < 0)
            goto exit;

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

        /* TR is transfer function (Optional) */
        code = pdfi_dict_knownget(ctx, SMask, "TR", (pdf_obj **)&TR);
        if (code > 0) {
            if (TR->type == PDF_DICT || TR->type == PDF_STREAM) {
                code = pdfi_build_function(ctx, &gsfunc, NULL, 1, TR, NULL);
                if (code < 0)
                    goto exit;
            } else if (TR->type == PDF_NAME) {
                if (!pdfi_name_is((pdf_name *)TR, "Identity")) {
                    dmprintf(ctx->memory, "WARNING: Unknown TR in SMask\n");
                }
            } else {
                dmprintf(ctx->memory, "WARNING: Ignoring invalid TR in SMask\n");
            }
        }

        /* BC is Background Color array (Optional) */
        code = pdfi_dict_knownget_type(ctx, SMask, "BC", PDF_ARRAY, (pdf_obj **)&BC);
        if (code < 0)
            goto exit;

        code = pdfi_dict_knownget_type(ctx, G_stream_dict, "Matte", PDF_ARRAY, (pdf_obj **)&a);
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

        code = pdfi_dict_knownget_type(ctx, G_stream_dict, "BBox", PDF_ARRAY, (pdf_obj **)&BBox);
        if (code < 0)
            goto exit;
        code = pdfi_array_to_gs_rect(ctx, BBox, &bbox);
        if (code < 0)
            goto exit;

        gs_trans_mask_params_init(&params, subtype);
        params.replacing = true;
        if (gsfunc) {
            params.TransferFunction = pdfi_tf_using_function;
            params.TransferFunction_data = gsfunc;
        }

        /* Need to set just the ctm (GroupMat) from the saved GroupGState, to
           have gs_begin_transparency_mask work correctly.  Or at least that's
           what the PS code comments claim (see pdf_draw.ps/.execmaskgroup)
        */
        gs_currentmatrix(ctx->pgs, &save_matrix);
        gs_currentmatrix(igs->GroupGState, &GroupMat);
        gs_setmatrix(ctx->pgs, &GroupMat);

        code = pdfi_dict_knownget_type(ctx, G_stream_dict, "Matrix", PDF_ARRAY, (pdf_obj **)&Matrix);
        if (code < 0)
            goto exit;
        code = pdfi_array_to_gs_matrix(ctx, Matrix, &group_Matrix);
        if (code < 0)
            goto exit;

        /* Transform the BBox by the Matrix */
        pdfi_bbox_transform(ctx, &bbox, &group_Matrix);

        /* CS is in the dict "Group" inside the dict "G" */
        /* TODO: Not sure if this is a required thing or just one possibility */
        code = pdfi_dict_knownget_type(ctx, G_stream_dict, "Group", PDF_DICT, (pdf_obj **)&Group);
        if (code < 0)
            goto exit;
        if (code > 0) {
            /* TODO: Stuff with colorspace, see .execmaskgroup */
            code = pdfi_dict_knownget(ctx, Group, "CS", &CS);
            if (code < 0)
                goto exit;
            if (code > 0) {
                code = pdfi_create_colorspace(ctx, CS, (pdf_dict *)ctx->main_stream,
                                              ctx->page.CurrentPageDict, &pcs, false);
                params.ColorSpace = pcs;
                if (code < 0)
                    goto exit;
            } else {
                /* Inherit current colorspace */
                params.ColorSpace = ctx->pgs->color[colorindex].color_space;
            }
        } else {
            /* GS and Adobe will ignore the whole mask in this case, so we do the same.
            */
            pdfi_set_error(ctx, 0, NULL, E_PDF_INVALID_TRANS_XOBJECT, "pdfi_trans_set_mask", (char *)"*** Error: Ignoring a transparency group XObject without /Group attribute");
            goto exit;
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
             * For now, use simple definition from PLRM2 and assume it is RGB or CMYK
             */
            pdfi_set_GrayBackground(&params);
        }

        code = gs_begin_transparency_mask(ctx->pgs, &params, &bbox, false);
        if (code < 0)
            goto exit;

        code = pdfi_form_execgroup(ctx, ctx->page.CurrentPageDict, G_stream,
                                   igs->GroupGState, NULL, &group_Matrix);
        code1 = gs_end_transparency_mask(ctx->pgs, colorindex);
        if (code != 0)
            code = code1;

        /* Put back the matrix (we couldn't just rely on gsave/grestore for whatever reason,
         * according to PS code anyway...
         */
        gs_setmatrix(ctx->pgs, &save_matrix);

        /* Set Processed flag */
        if (code == 0 && Processed)
            Processed->value = true;
    } else {
        /* take action on a non-/Mask entry. What does this mean ? What do we need to do */
        dmprintf(ctx->memory, "Warning: Type is not /Mask, entry ignored in pdfi_set_trans_mask\n");
    }

 exit:
    gs_setoverrideicc(ctx->pgs, save_OverrideICC);
    if (gsfunc)
        pdfi_free_function(ctx, gsfunc);
    if (pcs)
        rc_decrement_cs(pcs, "pdfi_trans_set_mask");
    pdfi_countdown(n);
    pdfi_countdown(S);
    pdfi_countdown(Group);
    pdfi_countdown(G_stream);
    pdfi_countdown(a);
    pdfi_countdown(BC);
    pdfi_countdown(TR);
    pdfi_countdown(BBox);
    pdfi_countdown(Matrix);
    pdfi_countdown(CS);
    pdfi_countdown(Processed);
#if DEBUG_TRANSPARENCY
    dbgmprintf(ctx->memory, "pdfi_trans_set_mask (.execmaskgroup) END\n");
#endif
    return code;
}

/* Wrapper around gs call to setup the transparency params correctly */
static int pdfi_gs_begin_transparency_group(gs_gstate * pgs,
                                       gs_transparency_group_params_t *params,
                                       const gs_rect *pbbox, pdf14_compositor_operations group_type)
{
    if (gs_getalphaisshape(pgs)) {
        params->group_shape = gs_getfillconstantalpha(pgs);
        params->group_opacity = 1.0;
    } else {
        params->group_opacity = gs_getfillconstantalpha(pgs);
        params->group_shape = 1.0;
    }

    return gs_begin_transparency_group(pgs, params, pbbox, group_type);
}

static int pdfi_transparency_group_common(pdf_context *ctx, pdf_dict *page_dict,
                                          pdf_dict *group_dict,
                                          gs_rect *bbox, pdf14_compositor_operations group_type)
{
    gs_transparency_group_params_t params;
    pdf_obj *CS = NULL;
    bool b;
    int code;

    gs_trans_group_params_init(&params, 1.0);
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
        goto exit;
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
    if (code > 0 && CS->type != PDF_NULL) {
        code = pdfi_setcolorspace(ctx, CS, group_dict, page_dict);
        if (code < 0)
            goto exit;
        params.ColorSpace = gs_currentcolorspace(ctx->pgs);
    } else {
        params.ColorSpace = NULL;
    }

 exit:
    pdfi_countdown(CS);
    if (code < 0)
        return_error(code);

    return pdfi_gs_begin_transparency_group(ctx->pgs, &params, (const gs_rect *)bbox, group_type);
}

/* Begin a simple group
 * pathbbox -- bbox to use, but can be NULL
 */
int pdfi_trans_begin_simple_group(pdf_context *ctx, gs_rect *pathbbox,
                                  bool stroked_bbox, bool isolated, bool knockout)
{
    gs_transparency_group_params_t params;
    gs_rect bbox;
    int code;

    gs_trans_group_params_init(&params, 1.0);
    params.Isolated = isolated;
    params.Knockout = knockout;

    if (!pathbbox) {
        code = pdfi_get_current_bbox(ctx, &bbox, stroked_bbox);
        if (code < 0)
            return code;
        pathbbox = &bbox;
    }

    code = pdfi_gs_begin_transparency_group(ctx->pgs, &params, pathbbox, PDF14_BEGIN_TRANS_GROUP);
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
    bbox.p.x = ctx->page.Size[0];
    bbox.p.y = ctx->page.Size[1];
    bbox.q.x = ctx->page.Size[2];
    bbox.q.y = ctx->page.Size[3];

    code = pdfi_transparency_group_common(ctx, page_dict, group_dict, &bbox, PDF14_BEGIN_TRANS_PAGE_GROUP);
    if (code < 0)
        pdfi_grestore(ctx);
    else
        ctx->current_stream_save.group_depth++;

    return code;
}

int pdfi_trans_begin_form_group(pdf_context *ctx, pdf_dict *page_dict, pdf_dict *form_dict)
{
    pdf_obj *group_obj = NULL;
    gs_rect bbox;
    pdf_array *BBox = NULL;
    int code;
    pdf_dict *group_dict = NULL;

    /* TODO: Maybe sometimes this is actually a stream?
     * Otherwise should just fetch it as a dict.
     * Anyway this will work for either dict or stream
     */
    code = pdfi_dict_get(ctx, form_dict, "Group", &group_obj);
    if (code < 0)
        return_error(code);

    code = pdfi_dict_from_obj(ctx, (pdf_obj *)group_obj, &group_dict);
    if (code < 0)
        goto exit;

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
    pdfi_countdown(group_obj);
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

    gs_trans_group_params_init(&params, 1.0);

    params.ColorSpace = NULL;
    params.Isolated = true;
    params.Knockout = false;
    params.image_with_SMask = image_with_SMask;
    bbox.p.x = 0;
    bbox.p.y = 0;
    bbox.q.x = 1;
    bbox.q.y = 1;

    return pdfi_gs_begin_transparency_group(ctx->pgs, &params, &bbox, PDF14_BEGIN_TRANS_GROUP);
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
 *   1) how many components the device has (CMYK or other)
 *   2) whether it is a device that has transparency support
 *   3) Overprint mode (enable, disable, simulate)
 * Based on pdf_main.ps/pdfshowpage_finish
 * Test file (run with -dOverprint=/simulate)
 *    tests_private/pdf/uploads/op_trans_spot_testing.pdf
 */
void pdfi_trans_set_needs_OP(pdf_context *ctx)
{
    bool is_cmyk;
    bool device_transparency = false;

    /* PS code checks for >= 4 components... */
    is_cmyk = ctx->pgs->device->color_info.num_components >= 4;

    device_transparency = pdfi_device_check_param_bool(ctx->pgs->device, "HaveTransparency");

    ctx->page.needs_OP = false;
    ctx->page.simulate_op = false;
    switch(ctx->args.overprint_control) {
    case PDF_OVERPRINT_DISABLE:
        /* Use defaults */
        break;
    case PDF_OVERPRINT_SIMULATE:
        if (!device_transparency && ctx->page.has_OP) {
            if (is_cmyk) {
                if (ctx->page.num_spots > 0) {
                    ctx->page.needs_OP = true;
                    ctx->page.simulate_op = true;
                }
            } else {
                ctx->page.needs_OP = true;
                ctx->page.simulate_op = true;
            }
        }
        break;
    case PDF_OVERPRINT_ENABLE:
    default:
        if (!is_cmyk || device_transparency)
            ctx->page.needs_OP = false;
        else
            ctx->page.needs_OP = true;
        break;
    }

    if(ctx->args.pdfdebug)
        dbgmprintf2(ctx->memory, "Page %s Overprint, Simulate is %s\n",
                    ctx->page.needs_OP ? "NEEDS" : "does NOT NEED",
                    ctx->page.simulate_op ? "TRUE" : "FALSE");
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
#if DEBUG_TRANSPARENCY
        dbgmprintf1(ctx->memory, "Colorspace is %d, OKAY for OVERPRINT\n", csi);
#endif
        return true;
    default:
#if DEBUG_TRANSPARENCY
        dbgmprintf1(ctx->memory, "Colorspace is %d, NOT OKAY for OVERPRINT\n", csi);
#endif
        return false;
    }

    return false;
}

int pdfi_trans_setup(pdf_context *ctx, pdfi_trans_state_t *state, gs_rect *bbox,
                           pdfi_transparency_caller_t caller)
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

    if (!ctx->page.has_transparency)
        return 0;

    if (ctx->page.needs_OP) {
        okOPcs = pdfi_trans_okOPcs(ctx);
        if (okOPcs) {
            if (caller == TRANSPARENCY_Caller_Stroke)
                current_overprint = gs_currentstrokeoverprint(ctx->pgs);
            else {
                current_overprint = gs_currentfilloverprint(ctx->pgs);
                if (caller == TRANSPARENCY_Caller_FillStroke)
                    current_overprint |= gs_currentstrokeoverprint(ctx->pgs);
            }
            ChangeBM = current_overprint;
            mode = gs_currentblendmode(ctx->pgs);
            if (mode != BLEND_MODE_Normal && mode != BLEND_MODE_Compatible)
                need_group = ChangeBM;
            else
                need_group = false;
        } else {
            need_group = false;
        }
        need_group = need_group || (igs->SMask != NULL);
    } else {
        if (caller == TRANSPARENCY_Caller_Image || igs->SMask == NULL)
            need_group = false;
        else
            need_group = true;
    }

    code = pdfi_trans_set_params(ctx);
    if (code != 0)
        return 0;

    if (!need_group && !ChangeBM)
        return 0;

    /* TODO: error handling... */
    if (need_group) {
        stroked_bbox = (caller == TRANSPARENCY_Caller_Stroke || caller == TRANSPARENCY_Caller_FillStroke);
        /* When changing to compatible overprint bm, the group pushed must be non-isolated. The exception
           is if we have a softmask.  See /setupOPtrans in pdf_ops.ps */
        code = pdfi_trans_begin_simple_group(ctx, bbox, stroked_bbox, igs->SMask != NULL, false);
        state->GroupPushed = true;
        state->saveStrokeAlpha = gs_getstrokeconstantalpha(ctx->pgs);
        state->saveFillAlpha = gs_getfillconstantalpha(ctx->pgs);
        code = gs_setfillconstantalpha(ctx->pgs, 1.0);
        code = gs_setstrokeconstantalpha(ctx->pgs, 1.0);
    }
    if (ChangeBM) {
        state->saveBM = mode;
        state->ChangeBM = true;
        code = gs_setblendmode(ctx->pgs, BLEND_MODE_CompatibleOverprint);
    }
    return code;
}

int pdfi_trans_setup_text(pdf_context *ctx, pdfi_trans_state_t *state, bool is_show)
{
    int Trmode = gs_currenttextrenderingmode(ctx->pgs);
    int code, code1;
    gs_rect bbox;

    code = gs_gsave(ctx->pgs);
    if (code < 0) goto exit;

    if (is_show) {
        code = gs_clippath(ctx->pgs);
    } else {
        code = gs_strokepath(ctx->pgs);
    }
    if (code >= 0)
        code = gs_upathbbox(ctx->pgs, &bbox, false);
    if (code < 0) {
        /* Just set bbox to [0,0,0,0] */
        bbox.p.x = bbox.p.y = bbox.q.x = bbox.q.y = 0.0;
        code = 0;
    }
    code1 = gs_grestore(ctx->pgs);
    if (code == 0) code = code1;
    if (code < 0) goto exit;

    switch (Trmode) {
    case 0:
        code = pdfi_trans_setup(ctx, state, &bbox, TRANSPARENCY_Caller_Fill);
        break;
    default:
        /* TODO: All the others */
        code = pdfi_trans_setup(ctx, state, &bbox, TRANSPARENCY_Caller_Fill);
        break;
    }

 exit:
    return code;
}

int pdfi_trans_teardown_text(pdf_context *ctx, pdfi_trans_state_t *state)
{
    int code = 0;

    code = pdfi_trans_teardown(ctx, state);

    return code;
}

int pdfi_trans_teardown(pdf_context *ctx, pdfi_trans_state_t *state)
{
    int code = 0;

    if (!ctx->page.has_transparency)
        return 0;

    if (state->GroupPushed) {
        code = pdfi_trans_end_simple_group(ctx);
        code = gs_setstrokeconstantalpha(ctx->pgs, state->saveStrokeAlpha);
        code = gs_setfillconstantalpha(ctx->pgs, state->saveFillAlpha);
    }

    if (gs_currentblendmode(ctx->pgs) == BLEND_MODE_CompatibleOverprint)
        code = gs_setblendmode(ctx->pgs, state->saveBM);

    return code;
}

int pdfi_trans_set_params(pdf_context *ctx)
{
    pdfi_int_gstate *igs = (pdfi_int_gstate *)ctx->pgs->client_data;
    gs_transparency_channel_selector_t csel;

    if (ctx->page.has_transparency) {
        if (gs_getalphaisshape(ctx->pgs))
            csel = TRANSPARENCY_CHANNEL_Shape;
        else
            csel = TRANSPARENCY_CHANNEL_Opacity;
        if (igs->SMask) {
            pdfi_trans_set_mask(ctx, igs, csel);
        }
    }

    return 0;
}
