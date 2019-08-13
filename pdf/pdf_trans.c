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

#include "gstparam.h"

static int pdfi_trans_set_mask(pdf_context *ctx, pdf_dict *SMask)
{
    int code;
    //    gs_color_space *gray_cs = gs_cspace_new_DeviceGray(ctx->memory);
    gs_color_space *pcs = NULL;
    gs_rect bbox = { { 0, 0} , { 1, 1} };
    gs_transparency_mask_params_t params;
    pdf_array *BBox = NULL;
    pdf_array *a = NULL;
    pdf_dict *G_dict = NULL;
    pdf_name *n = NULL;
    pdf_name *CS = NULL;
    double f;

    code = pdfi_dict_knownget_type(ctx, SMask, "Type", PDF_NAME, (pdf_obj **)&n);
    if (code > 0 && pdfi_name_is(n, "Mask")) {
        code = pdfi_dict_knownget_type(ctx, SMask, "G", PDF_DICT, (pdf_obj **)&G_dict);
        if (code > 0) {
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
            if (code > 0) {
                code = pdfi_array_to_gs_rect(ctx, BBox, &bbox);
                if (code < 0)
                    goto exit;
            }

            gs_trans_mask_params_init(&params, TRANSPARENCY_MASK_Luminosity);
            params.replacing = true;

            /* TODO: GroupGState, GMatrix ? */

            /* TODO: Stuff with colorspace, see .execmaskgroup */
            code = pdfi_dict_knownget_type(ctx, G_dict, "CS", PDF_NAME, (pdf_obj **)&CS);
            if (code < 0)
                goto exit;
            if (code > 0) {
                code = pdfi_create_colorspace(ctx, (pdf_obj *)CS, (pdf_dict *)ctx->main_stream,
                                              ctx->CurrentPageDict, &pcs, false);
                params.ColorSpace = pcs;
                if (code < 0)
                    goto exit;
            } else {
                /* Inherit current colorspace */
                params.ColorSpace = ctx->pgs->color[0].color_space; /* 0 or 1 ? */
            }

            code = gs_begin_transparency_mask(ctx->pgs, &params, &bbox, true);
            if (code < 0)
                goto exit;
            code = pdfi_form_execgroup(ctx, ctx->CurrentPageDict, G_dict);
            code = gs_end_transparency_mask(ctx->pgs, 0);
        }
    } else {
        /* take action on a non-/Mask entry. What does this mean ? What do we need to do */
        dmprintf(ctx->memory, "Warning: Type is not /Mask, entry ignored in pdfi_set_trans_mask\n");
    }

 exit:
    if (pcs)
        rc_decrement_cs(pcs, "pdfi_trans_set_mask");
    pdfi_countdown(n);
    pdfi_countdown(G_dict);
    pdfi_countdown(a);
    pdfi_countdown(BBox);
    pdfi_countdown(CS);
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

int pdfi_trans_begin_group(pdf_context *ctx, pdf_dict *page_dict, pdf_dict *form_dict)
{
    pdf_dict *group_dict = NULL;
    gs_rect bbox;
    int code;

    code = pdfi_dict_get_type(ctx, form_dict, "Group", PDF_DICT, (pdf_obj **)&group_dict);
    if (code < 0)
        return_error(code);

    code = pdfi_gsave(ctx);
    bbox.p.x = ctx->PageSize[0];
    bbox.p.y = ctx->PageSize[1];
    bbox.q.x = ctx->PageSize[2];
    bbox.q.y = ctx->PageSize[3];

    code = pdfi_transparency_group_common(ctx, page_dict, group_dict, &bbox, PDF14_BEGIN_TRANS_GROUP);
    if (code < 0)
        pdfi_grestore(ctx);
    else
        ctx->current_stream_save.group_depth++;

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

int pdfi_trans_set_params(pdf_context *ctx, double alpha)
{
    pdfi_int_gstate *igs = (pdfi_int_gstate *)ctx->pgs->client_data;

    if (ctx->page_has_transparency) {
        if (gs_getalphaisshape(ctx->pgs)) {
            gs_setshapealpha(ctx->pgs, alpha);
            gs_setopacityalpha(ctx->pgs, 1.0);
        } else {
            gs_setshapealpha(ctx->pgs, 1.0);
            gs_setopacityalpha(ctx->pgs, alpha);
        }
        if (igs->SMask) {
            pdfi_trans_set_mask(ctx, igs->SMask);
        }
    }

    return 0;
}
