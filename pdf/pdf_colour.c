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

/* colour operations for the PDF interpreter */

#include "pdf_int.h"
#include "pdf_colour.h"
#include "pdf_stack.h"
#include "pdf_array.h"

int pdf_setgraystroke(pdf_context *ctx)
{
    pdf_num *n1;
    int code;
    double d1;

    if (ctx->stack_top - ctx->stack_bot < 1) {
        if (ctx->pdfstoponerror)
            return_error(gs_error_stackunderflow);
        pdf_clearstack(ctx);
        return 0;
    }

    n1 = (pdf_num *)ctx->stack_top[-1];
    if (n1->type == PDF_INT){
        d1 = (double)n1->value.i;
    } else{
        if (n1->type == PDF_REAL) {
            d1 = n1->value.d;
        } else {
            if (ctx->pdfstoponerror)
                return_error(gs_error_typecheck);
            else {
                pdf_pop(ctx, 1);
                return 0;
            }
        }
    }
    code = gs_setgray(ctx->pgs, d1);
    if (code == 0)
        pdf_pop(ctx, 2);
    if(code < 0 && ctx->pdfstoponerror)
        return code;
    else
        return 0;
}

int pdf_setgrayfill(pdf_context *ctx)
{
    pdf_num *n1;
    int code;
    double d1;

    if (ctx->stack_top - ctx->stack_bot < 1) {
        if (ctx->pdfstoponerror)
            return_error(gs_error_stackunderflow);
        pdf_clearstack(ctx);
        return 0;
    }

    n1 = (pdf_num *)ctx->stack_top[-1];
    if (n1->type == PDF_INT){
        d1 = (double)n1->value.i;
    } else{
        if (n1->type == PDF_REAL) {
            d1 = n1->value.d;
        } else {
            if (ctx->pdfstoponerror)
                return_error(gs_error_typecheck);
            else {
                pdf_pop(ctx, 1);
                return 0;
            }
        }
    }
    gs_swapcolors(ctx->pgs);
    code = gs_setgray(ctx->pgs, d1);
    gs_swapcolors(ctx->pgs);
    if (code == 0)
        pdf_pop(ctx, 2);
    if(code < 0 && ctx->pdfstoponerror)
        return code;
    else
        return 0;
}

int pdf_setrgbstroke(pdf_context *ctx)
{
    pdf_num *num;
    double Values[3];
    int i, code;

    if (ctx->stack_top - ctx->stack_bot < 3) {
        if (ctx->pdfstoponerror)
            return_error(gs_error_stackunderflow);
        pdf_clearstack(ctx);
        return 0;
    }

    for (i=0;i < 3;i++){
        num = (pdf_num *)ctx->stack_top[i - 3];
        if (num->type != PDF_INT) {
            if(num->type != PDF_REAL) {
                if (ctx->pdfstoponerror)
                    return_error(gs_error_typecheck);
                else {
                    pdf_pop(ctx, 3);
                    return 0;
                }
            }
            else
                Values[i] = num->value.d;
        } else {
            Values[i] = (double)num->value.i;
        }
    }
    code = gs_setrgbcolor(ctx->pgs, Values[0], Values[1], Values[2]);
    if (code == 0)
        pdf_pop(ctx, 2);
    if(code < 0 && ctx->pdfstoponerror)
        return code;
    else
        return 0;
}

int pdf_setrgbfill(pdf_context *ctx)
{
    pdf_num *num;
    double Values[3];
    int i, code;

    if (ctx->stack_top - ctx->stack_bot < 3) {
        if (ctx->pdfstoponerror)
            return_error(gs_error_stackunderflow);
        pdf_clearstack(ctx);
        return 0;
    }

    for (i=0;i < 3;i++){
        num = (pdf_num *)ctx->stack_top[i - 3];
        if (num->type != PDF_INT) {
            if(num->type != PDF_REAL) {
                if (ctx->pdfstoponerror)
                    return_error(gs_error_typecheck);
                else {
                    pdf_pop(ctx, 3);
                    return 0;
                }
            }
            else
                Values[i] = num->value.d;
        } else {
            Values[i] = (double)num->value.i;
        }
    }
    gs_swapcolors(ctx->pgs);
    code = gs_setrgbcolor(ctx->pgs, Values[0], Values[1], Values[2]);
    gs_swapcolors(ctx->pgs);
    if (code == 0)
        pdf_pop(ctx, 2);
    if(code < 0 && ctx->pdfstoponerror)
        return code;
    else
        return 0;
}

int pdf_setcmykstroke(pdf_context *ctx)
{
    pdf_num *num;
    double Values[4];
    int i, code;

    if (ctx->stack_top - ctx->stack_bot < 4) {
        if (ctx->pdfstoponerror)
            return_error(gs_error_stackunderflow);
        pdf_clearstack(ctx);
        return 0;
    }

    for (i=0;i < 4;i++){
        num = (pdf_num *)ctx->stack_top[i - 4];
        if (num->type != PDF_INT) {
            if(num->type != PDF_REAL) {
                if (ctx->pdfstoponerror)
                    return_error(gs_error_typecheck);
                else {
                    pdf_pop(ctx, 4);
                    return 0;
                }
            }
            else
                Values[i] = num->value.d;
        } else {
            Values[i] = (double)num->value.i;
        }
    }
    code = gs_setcmykcolor(ctx->pgs, Values[0], Values[1], Values[2], Values[3]);
    if (code == 0)
        pdf_pop(ctx, 2);
    if(code < 0 && ctx->pdfstoponerror)
        return code;
    else
        return 0;
}

int pdf_setcmykfill(pdf_context *ctx)
{
    pdf_num *num;
    double Values[4];
    int i, code;

    if (ctx->stack_top - ctx->stack_bot < 4) {
        if (ctx->pdfstoponerror)
            return_error(gs_error_stackunderflow);
        pdf_clearstack(ctx);
        return 0;
    }

    for (i=0;i < 4;i++){
        num = (pdf_num *)ctx->stack_top[i - 4];
        if (num->type != PDF_INT) {
            if(num->type != PDF_REAL) {
                if (ctx->pdfstoponerror)
                    return_error(gs_error_typecheck);
                else {
                    pdf_pop(ctx, 4);
                    return 0;
                }
            }
            else
                Values[i] = num->value.d;
        } else {
            Values[i] = (double)num->value.i;
        }
    }
    gs_swapcolors(ctx->pgs);
    code = gs_setcmykcolor(ctx->pgs, Values[0], Values[1], Values[2], Values[3]);
    gs_swapcolors(ctx->pgs);
    if (code == 0)
        pdf_pop(ctx, 2);
    if(code < 0 && ctx->pdfstoponerror)
        return code;
    else
        return 0;
}

static int setcolorspace(pdf_context *ctx, pdf_array *color_array)
{
    int code;
    pdf_name *space = NULL;

    code = pdf_array_get(color_array, 1, (pdf_obj **)&space);
    if(code == 0) {
        if (space->type == PDF_NAME) {
            code = gs_error_rangecheck;
            switch(space->length) {
                case 3:
                    if (memcmp(space->data, "Lab", space->length) == 0) {
                    }
                    break;
                case 6:
                    if (memcmp(space->data, "CalRGB", space->length) == 0) {
                        code = gs_setrgbcolor(ctx->pgs, 1, 1, 1);
                    }
                    break;
                case 7:
                    if (memcmp(space->data, "CalGray", space->length) == 0) {
                        code = gs_setgray(ctx->pgs, 0);
                    }
                    if (memcmp(space->data, "Pattern", space->length) == 0) {
                    }
                    if (memcmp(space->data, "DeviceN", space->length) == 0) {
                    }
                    if (memcmp(space->data, "Indexed", space->length) == 0) {
                    }
                    break;
                case 8:
                    if (memcmp(space->data, "ICCBased", space->length) == 0) {
                    }
                    break;
                case 9:
                    if (memcmp(space->data, "DeviceRGB", space->length) == 0) {
                        code = gs_setrgbcolor(ctx->pgs, 1, 1, 1);
                    }
                    break;
                case 10:
                    if (memcmp(space->data, "DeviceGray", space->length) == 0) {
                        code = gs_setgray(ctx->pgs, 0);
                    }
                    if (memcmp(space->data, "DeviceCMYK", space->length) == 0) {
                        code = gs_setcmykcolor(ctx->pgs, 0, 0, 0, 1);
                    }
                    if (memcmp(space->data, "Separation", space->length) == 0) {
                    }
                    break;
                default:
                    break;
            }
        } else
            code = gs_error_typecheck;
    }
    pdf_countdown(space);
    if(ctx->pdfstoponerror)
        return code;
    return 0;
}

int pdf_setstrokecolor_space(pdf_context *ctx, pdf_dict *stream_dict, pdf_dict *page_dict)
{
    int code;
    pdf_name *n = NULL;
    pdf_array *a = NULL;

    if (ctx->stack_top - ctx->stack_bot < 1) {
        if (ctx->pdfstoponerror)
            return_error(gs_error_stackunderflow);
        return 0;
    }
    if (ctx->stack_top[-1]->type != PDF_NAME) {
        pdf_pop(ctx, 1);
        if (ctx->pdfstoponerror)
            return_error(gs_error_stackunderflow);
        return 0;
    }
    n = (pdf_name *)ctx->stack_top[-1];
    switch(n->length) {
        case 9:
            if (memcmp(n->data, "DeviceRGB", 9) == 0) {
                code = gs_setrgbcolor(ctx->pgs, 0, 0, 0);
                if (code < 0) {
                    if (ctx->pdfstoponerror)
                        return code;
                }
                return 0;
            }
        case 10:
            if (memcmp(n->data, "DeviceGray", 9) == 0) {
                code = gs_setgray(ctx->pgs, 1);
                if (code < 0) {
                    if (ctx->pdfstoponerror)
                        return code;
                }
                return 0;
            }
            if (memcmp(n->data, "DeviceCMYK", 9) == 0) {
                code = gs_setcmykcolor(ctx->pgs, 0, 0, 0, 1);
                if (code < 0) {
                    if (ctx->pdfstoponerror)
                        return code;
                }
                return 0;
            }
        default:
            break;
    }
    code = pdf_find_resource(ctx, (unsigned char *)"ColorSpace", n, stream_dict, page_dict, (pdf_obj **)&a);
    if (code < 0) {
        pdf_pop(ctx, 1);
        if (ctx->pdfstoponerror)
            return code;
        return 0;
    }
    pdf_pop(ctx, 1);
    if (a->type != PDF_ARRAY) {
        if (ctx->pdfstoponerror)
            return_error(gs_error_typecheck);
        return 0;
    }

    pdf_countdown(a);
    return 0;
}

int pdf_setfillcolor_space(pdf_context *ctx, pdf_dict *stream_dict, pdf_dict *page_dict)
{
    if (ctx->stack_top - ctx->stack_bot >= 1)
        pdf_pop(ctx, 1);
    return 0;
}

int pdf_setstrokecolor(pdf_context *ctx)
{
    const gs_color_space *  pcs = gs_currentcolorspace(ctx->pgs);
    int ncomps, i, code;
    gs_client_color cc;
    pdf_num *n;

    ncomps = cs_num_components(pcs);
    if (ctx->stack_top - ctx->stack_bot <= ncomps) {
        pdf_clearstack(ctx);
        if(ctx->pdfstoponerror)
            return_error(gs_error_stackunderflow);
        return 0;
    }
    for (i=0;i<ncomps;i++){
        n = (pdf_num *)ctx->stack_top[i - ncomps];
        if (n->type == PDF_INT) {
            cc.paint.values[i] = (float)n->value.i;
        } else {
            if (n->type == PDF_REAL) {
                cc.paint.values[i] = n->value.d;
            } else {
                pdf_clearstack(ctx);
                if (ctx->pdfstoponerror)
                    return_error(gs_error_typecheck);
                return 0;
            }
        }
    }
    pdf_pop(ctx, ncomps);
    code = gs_setcolor(ctx->pgs, &cc);
    if (code < 0 && ctx->pdfstoponerror)
        return code;
    return 0;
}

int pdf_setfillcolor(pdf_context *ctx)
{
    const gs_color_space *  pcs = gs_currentcolorspace(ctx->pgs);
    int ncomps, i, code;
    gs_client_color cc;
    pdf_num *n;

    ncomps = cs_num_components(pcs);
    if (ctx->stack_top - ctx->stack_bot <= ncomps) {
        pdf_clearstack(ctx);
        if(ctx->pdfstoponerror)
            return_error(gs_error_stackunderflow);
        return 0;
    }
    for (i=0;i<ncomps;i++){
        n = (pdf_num *)ctx->stack_top[i - ncomps];
        if (n->type == PDF_INT) {
            cc.paint.values[i] = (float)n->value.i;
        } else {
            if (n->type == PDF_REAL) {
                cc.paint.values[i] = n->value.d;
            } else {
                pdf_clearstack(ctx);
                if (ctx->pdfstoponerror)
                    return_error(gs_error_typecheck);
                return 0;
            }
        }
    }
    pdf_pop(ctx, ncomps);
    gs_swapcolors(ctx->pgs);
    code = gs_setcolor(ctx->pgs, &cc);
    gs_swapcolors(ctx->pgs);
    if (code < 0 && ctx->pdfstoponerror)
        return code;
    return 0;
}

int pdf_setstrokecolorN(pdf_context *ctx)
{
    const gs_color_space *  pcs = gs_currentcolorspace(ctx->pgs);
    int ncomps, i, code;
    gs_client_color cc;
    pdf_num *n;

    if (ctx->stack_top - ctx->stack_bot < 1) {
        pdf_clearstack(ctx);
        if(ctx->pdfstoponerror)
            return_error(gs_error_stackunderflow);
        return 0;
    }
    if (ctx->stack_top[-1]->type == PDF_NAME) {
        /* FIXME Patterns */
        pdf_clearstack(ctx);
        return 0;
    }
    ncomps = cs_num_components(pcs);
    if (ctx->stack_top - ctx->stack_bot <= ncomps) {
        pdf_clearstack(ctx);
        if(ctx->pdfstoponerror)
            return_error(gs_error_stackunderflow);
        return 0;
    }
    for (i=0;i<ncomps;i++){
        n = (pdf_num *)ctx->stack_top[i - ncomps];
        if (n->type == PDF_INT) {
            cc.paint.values[i] = (float)n->value.i;
        } else {
            if (n->type == PDF_REAL) {
                cc.paint.values[i] = n->value.d;
            } else {
                pdf_clearstack(ctx);
                if (ctx->pdfstoponerror)
                    return_error(gs_error_typecheck);
                return 0;
            }
        }
    }
    pdf_pop(ctx, ncomps);
    code = gs_setcolor(ctx->pgs, &cc);
    if (code < 0 && ctx->pdfstoponerror)
        return code;
    return 0;
}

int pdf_setfillcolorN(pdf_context *ctx)
{
    const gs_color_space *  pcs = gs_currentcolorspace(ctx->pgs);
    int ncomps, i, code;
    gs_client_color cc;
    pdf_num *n;

    if (ctx->stack_top - ctx->stack_bot < 1) {
        pdf_clearstack(ctx);
        if(ctx->pdfstoponerror)
            return_error(gs_error_stackunderflow);
        return 0;
    }
    if (ctx->stack_top[-1]->type == PDF_NAME) {
        /* FIXME Patterns */
        pdf_clearstack(ctx);
        return 0;
    }
    ncomps = cs_num_components(pcs);
    if (ctx->stack_top - ctx->stack_bot <= ncomps) {
        pdf_clearstack(ctx);
        if(ctx->pdfstoponerror)
            return_error(gs_error_stackunderflow);
        return 0;
    }
    for (i=0;i<ncomps;i++){
        n = (pdf_num *)ctx->stack_top[i - ncomps];
        if (n->type == PDF_INT) {
            cc.paint.values[i] = (float)n->value.i;
        } else {
            if (n->type == PDF_REAL) {
                cc.paint.values[i] = n->value.d;
            } else {
                pdf_clearstack(ctx);
                if (ctx->pdfstoponerror)
                    return_error(gs_error_typecheck);
                return 0;
            }
        }
    }
    pdf_pop(ctx, ncomps);
    gs_swapcolors(ctx->pgs);
    code = gs_setcolor(ctx->pgs, &cc);
    gs_swapcolors(ctx->pgs);
    if (code < 0 && ctx->pdfstoponerror)
        return code;
    return 0;
}

int pdf_ri(pdf_context *ctx)
{
    pdf_name *n;
    int code;

    if (ctx->stack_top - ctx->stack_bot >= 1)
        pdf_pop(ctx, 1);

    if (ctx->stack_top[-1]->type != PDF_NAME) {
        pdf_pop(ctx, 1);
        if (ctx->pdfstoponerror)
            return_error(gs_error_typecheck);
        return 0;
    }
    n = (pdf_name *)ctx->stack_top[-1];
    if (n->length == 10) {
        if (memcmp(n->data, "Perceptual", 10) == 0)
            code = gs_setrenderingintent(ctx->pgs, 0);
        else {
            if (memcmp(n->data, "Saturation", 10) == 0)
                code = gs_setrenderingintent(ctx->pgs, 2);
            else
                code = gs_error_undefined;
        }
    } else {
        if (n->length == 20) {
            if (memcmp(n->data, "RelativeColorimetric", 20) == 0)
                code = gs_setrenderingintent(ctx->pgs, 1);
            else {
                if (memcmp(n->data, "AbsoluteColoimetric", 20) == 0)
                    code = gs_setrenderingintent(ctx->pgs, 3);
                else
                    code = gs_error_undefined;
            }
        } else {
            code = gs_error_undefined;
        }
    }
    pdf_pop(ctx, 1);
    if (code < 0 && ctx->pdfstoponerror)
        return code;
    return 0;
}
