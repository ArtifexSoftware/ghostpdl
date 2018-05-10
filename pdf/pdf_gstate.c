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

    if (ctx->stack_top - ctx->stack_bot < 6) {
        if (ctx->pdfstoponerror)
            return_error(gs_error_stackunderflow);
        pdf_clearstack(ctx);
        return 0;
    }

    for (i=0;i < 6;i++){
        num = (pdf_num *)ctx->stack_top[i - 6];
        if (num->type != PDF_INT) {
            if(num->type != PDF_REAL) {
                if (ctx->pdfstoponerror)
                    return_error(gs_error_typecheck);
                else {
                    pdf_pop(ctx, 6);
                    return 0;
                }
            }
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
    if(code < 0 && ctx->pdfstoponerror)
        return code;
    else
        return 0;
}

int pdf_gsave(pdf_context *ctx)
{
    int code = gs_gsave(ctx->pgs);

    if(code < 0 && ctx->pdfstoponerror)
        return code;
    else
        return 0;
}

int pdf_grestore(pdf_context *ctx)
{
    int code = gs_grestore(ctx->pgs);

    if(code < 0 && ctx->pdfstoponerror)
        return code;
    else
        return 0;
}

int pdf_setlinewidth(pdf_context *ctx)
{
    int code;
    pdf_num *n1;
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
    code = gs_setlinewidth(ctx->pgs, d1);
    if (code == 0)
        pdf_pop(ctx, 1);
    if(code < 0 && ctx->pdfstoponerror)
        return code;
    else
        return 0;
}

int pdf_setlinejoin(pdf_context *ctx)
{
    int code;
    pdf_num *n1;

    if (ctx->stack_top - ctx->stack_bot < 1) {
        if (ctx->pdfstoponerror)
            return_error(gs_error_stackunderflow);
        pdf_clearstack(ctx);
        return 0;
    }

    n1 = (pdf_num *)ctx->stack_top[-1];
    if (n1->type == PDF_INT){
        code = gs_setlinejoin(ctx->pgs, (gs_line_join)n1->value.d);
    } else {
        if (ctx->pdfstoponerror)
            return_error(gs_error_typecheck);
        else {
            pdf_pop(ctx, 1);
            return 0;
        }
    }
    if (code == 0)
        pdf_pop(ctx, 1);
    if(code < 0 && ctx->pdfstoponerror)
        return code;
    else
        return 0;
}

int pdf_setlinecap(pdf_context *ctx)
{
    int code;
    pdf_num *n1;

    if (ctx->stack_top - ctx->stack_bot < 1) {
        if (ctx->pdfstoponerror)
            return_error(gs_error_stackunderflow);
        pdf_clearstack(ctx);
        return 0;
    }

    n1 = (pdf_num *)ctx->stack_top[-1];
    if (n1->type == PDF_INT){
        code = gs_setlinecap(ctx->pgs, (gs_line_cap)n1->value.d);
    } else {
        if (ctx->pdfstoponerror)
            return_error(gs_error_typecheck);
        else {
            pdf_pop(ctx, 1);
            return 0;
        }
    }
    if (code == 0)
        pdf_pop(ctx, 1);
    if(code < 0 && ctx->pdfstoponerror)
        return code;
    else
        return 0;
}

int pdf_setflat(pdf_context *ctx)
{
    int code;
    pdf_num *n1;
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
    code = gs_setflat(ctx->pgs, d1);
    if (code == 0)
        pdf_pop(ctx, 1);
    if(code < 0 && ctx->pdfstoponerror)
        return code;
    else
        return 0;
}

int pdf_setdash(pdf_context *ctx)
{
    pdf_num *phase;
    pdf_array *a;
    float *dash_array;
    double phase_d, temp;
    int i, code;

    if (ctx->stack_top - ctx->stack_bot < 1) {
        if (ctx->pdfstoponerror)
            return_error(gs_error_stackunderflow);
        pdf_clearstack(ctx);
        return 0;
    }

    phase = (pdf_num *)ctx->stack_top[-1];
    if (phase->type == PDF_INT){
        phase_d = (double)phase->value.i;
    } else{
        if (phase->type == PDF_REAL) {
            phase_d = phase->value.d;
        } else {
            if (ctx->pdfstoponerror)
                return_error(gs_error_typecheck);
            else {
                pdf_pop(ctx, 1);
                return 0;
            }
        }
    }

    a = (pdf_array *)ctx->stack_top[-2];
    if (a->type != PDF_ARRAY) {
        if (ctx->pdfstoponerror)
            return_error(gs_error_typecheck);
        else {
            pdf_pop(ctx, 1);
            return 0;
        }
    }

    dash_array = (float *)gs_alloc_bytes(ctx->memory, a->entries * sizeof (float), "temporary float array for setdash");
    if (dash_array == NULL)
        return_error(gs_error_VMerror);

    for (i=0;i < a->entries;i++){
        code = pdf_array_get_number(ctx, a, (uint64_t)i, &temp);
        if (code < 0) {
            if (ctx->pdfstoponerror) {
                gs_free_object(ctx->memory, dash_array, "error in setdash");
                return code;
            }
            else {
                pdf_pop(ctx, 1);
                gs_free_object(ctx->memory, dash_array, "error in setdash");
                return 0;
            }
        }
        dash_array[i] = (float)temp;
    }
    code = gs_setdash(ctx->pgs, dash_array, a->entries, phase_d);
    gs_free_object(ctx->memory, dash_array, "error in setdash");
    if (code == 0)
        pdf_pop(ctx, 2);
    if(code < 0 && ctx->pdfstoponerror)
        return code;
    else
        return 0;
}

int pdf_setmiterlimit(pdf_context *ctx)
{
    int code;
    pdf_num *n1;
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
    code = gs_setmiterlimit(ctx->pgs, d1);
    if (code == 0)
        pdf_pop(ctx, 1);
    if(code < 0 && ctx->pdfstoponerror)
        return code;
    else
        return 0;
}
