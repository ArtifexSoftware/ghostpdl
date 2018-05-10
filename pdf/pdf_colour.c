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
