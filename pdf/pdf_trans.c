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

#include "gstparam.h"

static int pdfi_transparency_group_common(pdf_context *ctx, pdf_dict *page_dict, pdf_dict *group_dict, gs_rect *bbox, pdf14_compositor_operations group_type)
{
    gs_transparency_group_params_t params;
    pdf_obj *CS = NULL;
    bool b;
    gs_color_space  *pcs = NULL;
    int code;

    gs_trans_group_params_init(&params);
    gs_setopacityalpha(ctx->pgs, 1.0);
    gs_setblendmode(ctx->pgs, BLEND_MODE_Normal);

    code = pdfi_dict_get_bool(ctx, group_dict, "Isolated", &b);
    if (code < 0 && code != gs_error_undefined)
        return_error(code);
    if (code == gs_error_undefined)
        params.Isolated = false;
    else
        params.Isolated = b;

    code = pdfi_dict_get_bool(ctx, group_dict, "Knockout", &b);
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

int pdfi_begin_page_group(pdf_context *ctx, pdf_dict *page_dict)
{
    pdf_dict *group_dict = NULL;
    gs_rect bbox;
    int code;

    code = pdfi_dict_get_type(ctx, page_dict, "Group", PDF_DICT, (pdf_obj **)&group_dict);
    if (code < 0)
        return_error(code);

    code = gs_gsave(ctx->pgs);
    bbox.p.x = ctx->PageSize[0];
    bbox.p.y = ctx->PageSize[1];
    bbox.q.x = ctx->PageSize[2];
    bbox.q.y = ctx->PageSize[3];

    code = pdfi_transparency_group_common(ctx, page_dict, group_dict, &bbox, PDF14_BEGIN_TRANS_PAGE_GROUP);
    pdfi_countdown(group_dict);
    return code;
}

int pdfi_begin_group(pdf_context *ctx, pdf_dict *page_dict, pdf_dict *form_dict)
{
    pdf_dict *group_dict = NULL;
    gs_rect bbox;
    int code;

    code = pdfi_dict_get_type(ctx, form_dict, "Group", PDF_DICT, (pdf_obj **)&group_dict);
    if (code < 0)
        return_error(code);

    code = gs_gsave(ctx->pgs);
    bbox.p.x = ctx->PageSize[0];
    bbox.p.y = ctx->PageSize[1];
    bbox.q.x = ctx->PageSize[2];
    bbox.q.y = ctx->PageSize[3];

    code = pdfi_transparency_group_common(ctx, page_dict, group_dict, &bbox, PDF14_BEGIN_TRANS_PAGE_GROUP);
    pdfi_countdown(group_dict);
    return code;
}


int pdfi_end_transparency_group(pdf_context *ctx)
{
    int code;

    code = gs_grestore(ctx->pgs);
    return gs_end_transparency_group(ctx->pgs);
}
