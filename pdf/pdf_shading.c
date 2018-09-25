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
#include "pdf_func.h"

int pdfi_shading(pdf_context *ctx, pdf_dict *stream_dict, pdf_dict *page_dict)
{
    int code, code1;
    pdf_name *n = NULL;
    pdf_obj *o, *o1;

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

    code = pdfi_dict_get(ctx, (pdf_dict *)o, "ShadingType", &o1);
    if (code == 0) {
        if (o1->type == PDF_INT) {
            switch(((pdf_num *)o1)->value.i){
                case 1:
                    code = pdfi_shading1(ctx, (pdf_dict *)o, stream_dict, page_dict);
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

    code1 = gs_grestore(ctx->pgs);
    if (code1 == 0)
        return code;
    return code1;
}

int pdfi_shading1(pdf_context *ctx, pdf_dict *shading_dict, pdf_dict *stream_dict, pdf_dict *page_dict)
{
    pdf_obj *o = NULL;
    pdf_dict *function_dict = NULL;
    gs_function_t *pfn = NULL;
    int code;

    code = pdfi_dict_get(ctx, shading_dict, "Function", &o);
    if (code < 0)
        return code;
    if (o->type != PDF_DICT) {
        pdfi_countdown(o);
        return_error(gs_error_typecheck);
    }
    code = pdfi_build_function(ctx, &pfn, (pdf_dict *)o, page_dict);
    if (code < 0){
        pdfi_countdown(o);
        return code;
    }
    return 0;
}

int pdfi_shading2(pdf_context *ctx, pdf_dict *shading_dict, pdf_dict *stream_dict, pdf_dict *page_dict)
{
    return 0;
}

int pdfi_shading3(pdf_context *ctx, pdf_dict *shading_dict, pdf_dict *stream_dict, pdf_dict *page_dict)
{
    return 0;
}

int pdfi_shading4(pdf_context *ctx, pdf_dict *shading_dict, pdf_dict *stream_dict, pdf_dict *page_dict)
{
    return 0;
}

int pdfi_shading5(pdf_context *ctx, pdf_dict *shading_dict, pdf_dict *stream_dict, pdf_dict *page_dict)
{
    return 0;
}

int pdfi_shading6(pdf_context *ctx, pdf_dict *shading_dict, pdf_dict *stream_dict, pdf_dict *page_dict)
{
    return 0;
}

int pdfi_shading7(pdf_context *ctx, pdf_dict *shading_dict, pdf_dict *stream_dict, pdf_dict *page_dict)
{
    return 0;
}
