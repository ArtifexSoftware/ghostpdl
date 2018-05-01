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

/* Graphics state operations for the PDF interpreter */

#include "pdf_int.h"
#include "pdf_gstate.h"
#include "gsmatrix.h"
#include "gslparam.h"

int pdf_concat(pdf_context *ctx)
{
    int i, code;
    pdf_num *num;
    double Values[6];
    gs_matrix m;

    if (ctx->stack_top - ctx->stack_bot < 6)
        return_error(gs_error_stackunderflow);

    for (i=0;i < 6;i++){
        num = (pdf_num *)ctx->stack_top[i - 6];
        if (num->type != PDF_INT) {
            if(num->type != PDF_REAL)
                return_error(gs_error_typecheck);
            else
                Values[i] = num->value.d;
        } else {
            Values[i] = (double)num->value.i;
        }
    }
    m.xx = (float)Values[0];
    m.xy = (float)Values[1];
    m.yx = (float)Values[2];
    m.yy = (float)Values[3];
    m.tx = (float)Values[4];
    m.ty = (float)Values[5];
    code = gs_concat(ctx->pgs, (const gs_matrix *)&m);
    if (code == 0)
        pdf_pop(ctx, 6);
    return code;
}

int pdf_gsave(pdf_context *ctx)
{
    return gs_gsave(ctx->pgs);
}

int pdf_grestore(pdf_context *ctx)
{
    return gs_grestore(ctx->pgs);
}

int pdf_setlinewidth(pdf_context *ctx)
{
    int code;
    pdf_num *n1;
    double d1;

    if (ctx->stack_top - ctx->stack_bot < 1)
        return_error(gs_error_stackunderflow);

    n1 = (pdf_num *)ctx->stack_top[-1];
    if (n1->type == PDF_INT){
        d1 = (double)n1->value.i;
    } else{
        if (n1->type == PDF_REAL) {
            d1 = n1->value.d;
        } else
            return_error(gs_error_typecheck);
    }
    code = gs_setlinewidth(ctx->pgs, d1);
    if (code == 0)
        pdf_pop(ctx, 1);
    return code;
}

int pdf_setlinejoin(pdf_context *ctx)
{
    int code;
    pdf_num *n1;

    if (ctx->stack_top - ctx->stack_bot < 1)
        return_error(gs_error_stackunderflow);

    n1 = (pdf_num *)ctx->stack_top[-1];
    if (n1->type == PDF_INT){
        code = gs_setlinejoin(ctx->pgs, (gs_line_join)n1->value.d);
    } else
        return_error(gs_error_typecheck);
    if (code == 0)
        pdf_pop(ctx, 1);
    return code;
}

int pdf_setlinecap(pdf_context *ctx)
{
    int code;
    pdf_num *n1;

    if (ctx->stack_top - ctx->stack_bot < 1)
        return_error(gs_error_stackunderflow);

    n1 = (pdf_num *)ctx->stack_top[-1];
    if (n1->type == PDF_INT){
        code = gs_setlinecap(ctx->pgs, (gs_line_cap)n1->value.d);
    } else
        return_error(gs_error_typecheck);
    if (code == 0)
        pdf_pop(ctx, 1);
    return code;
}

int pdf_setflat(pdf_context *ctx)
{
    int code;
    pdf_num *n1;
    double d1;

    if (ctx->stack_top - ctx->stack_bot < 1)
        return_error(gs_error_stackunderflow);

    n1 = (pdf_num *)ctx->stack_top[-1];
    if (n1->type == PDF_INT){
        d1 = (double)n1->value.i;
    } else{
        if (n1->type == PDF_REAL) {
            d1 = n1->value.d;
        } else
            return_error(gs_error_typecheck);
    }
    code = gs_setflat(ctx->pgs, d1);
    if (code == 0)
        pdf_pop(ctx, 1);
    return code;
}
