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

/* Shading operations for the PDF interpreter */

#include "pdf_int.h"
#include "pdf_stack.h"
#include "pdf_shading.h"


#include "gxshade.h"
#include "gsptype2.h"

static int shading_float_array_from_dict_key(pdf_context *ctx, float *parray, pdf_dict *dict, const char *Key)
{
    int code, i;
    pdf_array *a;
    float *arr;

    code = pdfi_dict_get(ctx, dict, Key, (pdf_obj *)&a);
    if (code < 0)
        return code;
    if (a->type != PDF_ARRAY) {
        pdfi_countdown(a);
        return_error(gs_error_typecheck);
    }
    if (a->entries & 1) {
        pdfi_countdown(a);
        return_error(gs_error_rangecheck);
    }

    for (i=0;i< a->entries;i+=2) {
        if (a->values[i]->type != PDF_INT && a->values[i]->type != PDF_REAL) {
            pdfi_countdown(a);
            return_error(gs_error_typecheck);
        }
        if (a->values[i]->type == PDF_INT)
            parray[i] = (float)((pdf_num *)a->values[i])->value.i;
        else
            parray[i] = (float)((pdf_num *)a->values[i])->value.d;
    }
    pdfi_countdown(a);
    return a->entries;
}

static int shading_matrix_from_dict(pdf_context *ctx, float *parray, pdf_dict *dict)
{
    int code, i;
    pdf_array *a;
    float *arr;

    code = pdfi_dict_get(ctx, dict, "Matrix", (pdf_obj *)&a);
    if (code < 0)
        return code;
    if (a->type != PDF_ARRAY) {
        pdfi_countdown(a);
        return_error(gs_error_typecheck);
    }
    if (a->entries != 6) {
        pdfi_countdown(a);
        return_error(gs_error_rangecheck);
    }

    for (i=0;i< a->entries;i++) {
        if (a->values[i]->type != PDF_INT && a->values[i]->type != PDF_REAL) {
            pdfi_countdown(a);
            return_error(gs_error_typecheck);
        }
        if (a->values[i]->type == PDF_INT)
            parray[i] = (float)((pdf_num *)a->values[i])->value.i;
        else
            parray[i] = (float)((pdf_num *)a->values[i])->value.d;
    }
    pdfi_countdown(a);
    return a->entries;
}

static int pdfi_shading1(pdf_context *ctx, gs_shading_params_t *pcommon,
                  gs_shading_t **ppsh,
                  pdf_dict *shading_dict, pdf_dict *stream_dict, pdf_dict *page_dict)
{
    pdf_obj *o = NULL;
    gs_function_t *pfn = NULL;
    int code, i;
    gs_shading_Fb_params_t params;
    static const float default_Domain[4] = {0, 1, 0, 1};

    *(gs_shading_params_t *)&params = *pcommon;
    gs_make_identity(&params.Matrix);
    params.Function = 0;

    code = shading_float_array_from_dict_key(ctx, (float *)&params.Domain, shading_dict, "Domain");
    if (code < 0) {
        if (code == gs_error_undefined) {
            for (i = 0; i < 4; i++) {
                (float)params.Domain[i] = default_Domain[i];
            }
        } else
            return code;
    }

    code = shading_matrix_from_dict(ctx, (float *)&params.Matrix, shading_dict);
    if (code < 0)
        return code;

    code = pdfi_dict_get(ctx, shading_dict, "Function", &o);
    if (code < 0)
        return code;

    if (o->type != PDF_DICT) {
        pdfi_countdown(o);
        return_error(gs_error_typecheck);
    }
    code = pdfi_build_function(ctx, &params.Function, (pdf_dict *)o, page_dict);
    if (code < 0){
        pdfi_countdown(o);
        return code;
    }
    code = gs_shading_Fb_init(ppsh, &params, ctx->memory);
    if (code < 0) {
        gs_free_object(ctx->memory, params.Function, "Function");
        pdfi_countdown(o);
    }
    return code;
}

static int pdfi_shading2(pdf_context *ctx, pdf_dict *shading_dict, pdf_dict *stream_dict, pdf_dict *page_dict)
{
    return 0;
}

static int pdfi_shading3(pdf_context *ctx, pdf_dict *shading_dict, pdf_dict *stream_dict, pdf_dict *page_dict)
{
    return 0;
}

static int pdfi_shading4(pdf_context *ctx, pdf_dict *shading_dict, pdf_dict *stream_dict, pdf_dict *page_dict)
{
    return 0;
}

static int pdfi_shading5(pdf_context *ctx, pdf_dict *shading_dict, pdf_dict *stream_dict, pdf_dict *page_dict)
{
    return 0;
}

static int pdfi_shading6(pdf_context *ctx, pdf_dict *shading_dict, pdf_dict *stream_dict, pdf_dict *page_dict)
{
    return 0;
}

static int pdfi_shading7(pdf_context *ctx, pdf_dict *shading_dict, pdf_dict *stream_dict, pdf_dict *page_dict)
{
    return 0;
}

int pdfi_shading(pdf_context *ctx, pdf_dict *stream_dict, pdf_dict *page_dict)
{
    int code, code1;
    pdf_name *n = NULL;
    pdf_obj *o, *o1, *cspace;
    gs_shading_params_t params;
    gs_shading_t *psh;

    if (ctx->loop_detection == NULL) {
        pdfi_init_loop_detector(ctx);
        pdfi_loop_detector_mark(ctx);
    } else {
        pdfi_loop_detector_mark(ctx);
    }

    if (ctx->stack_top - ctx->stack_bot < 1) {
        pdfi_loop_detector_cleartomark(ctx);
        return_error(gs_error_stackunderflow);
    }

    n = (pdf_name *)ctx->stack_top[-1];
    if (n->type != PDF_NAME) {
        pdfi_pop(ctx, 1);
        pdfi_loop_detector_cleartomark(ctx);
        return_error(gs_error_typecheck);
    }

    code = pdfi_find_resource(ctx, (unsigned char *)"Shading", n, stream_dict, page_dict, &o);
    if (code < 0) {
        pdfi_pop(ctx, 1);
        pdfi_loop_detector_cleartomark(ctx);
        if (ctx->pdfstoponerror)
            return code;
        return 0;
    }
    if (o->type != PDF_DICT) {
        pdfi_pop(ctx, 1);
        pdfi_loop_detector_cleartomark(ctx);
        pdfi_countdown(o);
        return_error(gs_error_typecheck);
    }

    code = gs_gsave(ctx->pgs);
    if (code < 0) {
        pdfi_pop(ctx, 1);
        pdfi_loop_detector_cleartomark(ctx);
        pdfi_countdown(o);
        return code;
    }

    params.ColorSpace = 0;
    params.cie_joint_caches = 0;
    params.Background = 0;
    params.have_BBox = 0;
    params.AntiAlias = 0;

    code = pdfi_dict_get(ctx, (pdf_dict *)o, "ColorSpace", &cspace);
    if (code < 0) {
        (void)gs_grestore(ctx->pgs);
        pdfi_pop(ctx, 1);
        pdfi_loop_detector_cleartomark(ctx);
        pdfi_countdown(o);
        return code;
    }

    code = pdfi_setcolorspace(ctx, cspace, stream_dict, page_dict);
    if (code < 0) {
        (void)gs_grestore(ctx->pgs);
        pdfi_pop(ctx, 1);
        pdfi_loop_detector_cleartomark(ctx);
        pdfi_countdown(o);
        return code;
    }

    /* Collect parameters common to all shading types. */
    {
        gs_color_space *pcs = gs_currentcolorspace(ctx->pgs);
        int num_comp = gs_color_space_num_components(pcs);

        if (num_comp < 0) {	/* Pattern color space */
            pdfi_pop(ctx, 1);
            pdfi_loop_detector_cleartomark(ctx);
            pdfi_countdown(o);
            return_error(gs_error_typecheck);
        }
        params.ColorSpace = pcs;
        rc_increment_cs(pcs);
    }

    code = pdfi_dict_get(ctx, (pdf_dict *)o, "ShadingType", &o1);
    if (code == 0) {
        if (o1->type == PDF_INT) {
            switch(((pdf_num *)o1)->value.i){
                case 1:
                    code = pdfi_shading1(ctx, &params, &psh, (pdf_dict *)o, stream_dict, page_dict);
                    break;
                case 2:
                    code = pdfi_shading2(ctx, (pdf_dict *)o, stream_dict, page_dict);
                    break;
                case 3:
                    code = pdfi_shading3(ctx, (pdf_dict *)o, stream_dict, page_dict);
                    break;
                case 4:
                    code = pdfi_shading4(ctx, (pdf_dict *)o, stream_dict, page_dict);
                    break;
                case 5:
                    code = pdfi_shading5(ctx, (pdf_dict *)o, stream_dict, page_dict);
                    break;
                case 6:
                    code = pdfi_shading6(ctx, (pdf_dict *)o, stream_dict, page_dict);
                    break;
                case 7:
                    code = pdfi_shading7(ctx, (pdf_dict *)o, stream_dict, page_dict);
                    break;
                default:
                    code = gs_error_rangecheck;
                    break;
            }
            pdfi_countdown(o1);
        } else {
            pdfi_countdown(o1);
            code = gs_error_typecheck;
        }
    }

    pdfi_countdown(o);
    pdfi_pop(ctx, 1);
    pdfi_loop_detector_cleartomark(ctx);

    if (code >= 0) {
        code = gs_shfill(ctx->pgs, psh);
    }

    code1 = gs_grestore(ctx->pgs);
    if (code1 == 0)
        return code;
    return code1;
}
