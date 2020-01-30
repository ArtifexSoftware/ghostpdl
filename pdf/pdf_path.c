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

/* Path operations for the PDF interpreter */

#include "pdf_int.h"
#include "pdf_gstate.h"
#include "pdf_path.h"
#include "pdf_stack.h"
#include "pdf_trans.h"
#include "gstypes.h"
#include "pdf_optcontent.h"

int pdfi_moveto (pdf_context *ctx)
{
    pdf_num *n1, *n2;
    int code;
    double x, y;

    if (ctx->TextBlockDepth != 0)
        ctx->pdf_warnings |= W_PDF_OPINVALIDINTEXT;

    if (pdfi_count_stack(ctx) < 2) {
        pdfi_clearstack(ctx);
        if (ctx->pdfstoponerror)
            return_error(gs_error_stackunderflow);
        else
            return 0;
    }

    n1 = (pdf_num *)ctx->stack_top[-1];
    n2 = (pdf_num *)ctx->stack_top[-2];
    if (n1->type == PDF_INT){
        y = (double)n1->value.i;
    } else{
        if (n1->type == PDF_REAL) {
            y = n1->value.d;
        } else {
            pdfi_pop(ctx, 2);
            if (ctx->pdfstoponerror)
                return_error(gs_error_typecheck);
            return 0;
        }
    }
    if (n2->type == PDF_INT){
        x = (double)n2->value.i;
    } else{
        if (n2->type == PDF_REAL) {
            x = n2->value.d;
        } else {
            pdfi_pop(ctx, 2);
            if (ctx->pdfstoponerror)
                return_error(gs_error_typecheck);
            return 0;
        }
    }

    code = gs_moveto(ctx->pgs, x, y);
    pdfi_pop(ctx, 2);
    if(code < 0 && ctx->pdfstoponerror)
        return code;
    else
        return 0;
}

int pdfi_lineto (pdf_context *ctx)
{
    pdf_num *n1, *n2;
    int code;
    double x, y;

    if (ctx->TextBlockDepth != 0)
        ctx->pdf_warnings |= W_PDF_OPINVALIDINTEXT;

    if (pdfi_count_stack(ctx) < 2) {
        pdfi_clearstack(ctx);
        if (ctx->pdfstoponerror)
            return_error(gs_error_stackunderflow);
        else
            return 0;
    }

    n1 = (pdf_num *)ctx->stack_top[-1];
    n2 = (pdf_num *)ctx->stack_top[-2];
    if (n1->type == PDF_INT){
        y = (double)n1->value.i;
    } else{
        if (n1->type == PDF_REAL) {
            y = n1->value.d;
        } else {
            pdfi_pop(ctx, 2);
            if (ctx->pdfstoponerror)
                return_error(gs_error_typecheck);
            return 0;
        }
    }
    if (n2->type == PDF_INT){
        x = (double)n2->value.i;
    } else{
        if (n2->type == PDF_REAL) {
            x = n2->value.d;
        } else {
            pdfi_pop(ctx, 2);
            if (ctx->pdfstoponerror)
                return_error(gs_error_typecheck);
            return 0;
        }
    }

    code = gs_lineto(ctx->pgs, x, y);
    pdfi_pop(ctx, 2);
    if(code < 0 && ctx->pdfstoponerror)
        return code;
    else
        return 0;
}

static int pdfi_fill_inner(pdf_context *ctx, bool use_eofill)
{
    int code, code1;
    pdfi_trans_state_t state;

    if (ctx->TextBlockDepth != 0)
        ctx->pdf_warnings |= W_PDF_OPINVALIDINTEXT;

    if (pdfi_oc_is_off(ctx)) {
        code = gs_newpath(ctx->pgs);
        return code;
    }

    gs_swapcolors_quick(ctx->pgs);
    code = pdfi_trans_setup(ctx, &state, TRANSPARENCY_Caller_Fill, gs_getfillconstantalpha(ctx->pgs));
    if (code == 0) {
        if (use_eofill)
            code = gs_eofill(ctx->pgs);
        else
            code = gs_fill(ctx->pgs);
        code1 = pdfi_trans_teardown(ctx, &state);
        if (code == 0)
            code = code1;
    }
    gs_swapcolors_quick(ctx->pgs);
    if(code < 0 && ctx->pdfstoponerror)
        return code;
    else
        return 0;
}

int pdfi_fill(pdf_context *ctx)
{
    return pdfi_fill_inner(ctx, false);
}

int pdfi_eofill(pdf_context *ctx)
{
    return pdfi_fill_inner(ctx, true);
}

int pdfi_stroke(pdf_context *ctx)
{
    int code, code1;
    pdfi_trans_state_t state;

    if (ctx->TextBlockDepth != 0)
        ctx->pdf_warnings |= W_PDF_OPINVALIDINTEXT;

    if (pdfi_oc_is_off(ctx)) {
        code = gs_newpath(ctx->pgs);
        return code;
    }

    code = pdfi_trans_setup(ctx, &state, TRANSPARENCY_Caller_Stroke, gs_getstrokeconstantalpha(ctx->pgs));
    if (code == 0) {
        code = gs_stroke(ctx->pgs);
        code1 = pdfi_trans_teardown(ctx, &state);
        if (code == 0)
            code = code1;
    }
    if(code < 0 && ctx->pdfstoponerror)
        return code;
    else
        return 0;
}

int pdfi_closepath_stroke(pdf_context *ctx)
{
    int code;

    if (ctx->TextBlockDepth != 0)
        ctx->pdf_warnings |= W_PDF_OPINVALIDINTEXT;

    code = gs_closepath(ctx->pgs);
    if (code == 0)
        code = pdfi_stroke(ctx);
    if(code < 0 && ctx->pdfstoponerror)
        return code;
    else
        return 0;
}

int pdfi_curveto(pdf_context *ctx)
{
    int i, code;
    pdf_num *num;
    double Values[6];

    if (pdfi_count_stack(ctx) < 6) {
        pdfi_clearstack(ctx);
        if (ctx->pdfstoponerror)
            return_error(gs_error_stackunderflow);
        else
            return 0;
    }

    for (i=0;i < 6;i++){
        num = (pdf_num *)ctx->stack_top[i - 6];
        if (num->type != PDF_INT) {
            if(num->type != PDF_REAL) {
                pdfi_pop(ctx, 6);
                if (ctx->pdfstoponerror)
                    return_error(gs_error_typecheck);
                return 0;
            }
            else
                Values[i] = num->value.d;
        } else {
            Values[i] = (double)num->value.i;
        }
    }

    if (ctx->TextBlockDepth != 0)
        ctx->pdf_warnings |= W_PDF_OPINVALIDINTEXT;

    code = gs_curveto(ctx->pgs, Values[0], Values[1], Values[2], Values[3], Values[4], Values[5]);
    pdfi_pop(ctx, 6);
    if(code < 0 && ctx->pdfstoponerror)
        return code;
    else
        return 0;
}

int pdfi_v_curveto(pdf_context *ctx)
{
    int i, code;
    pdf_num *num;
    double Values[4];
    gs_point pt;

    if (pdfi_count_stack(ctx) < 4) {
        pdfi_clearstack(ctx);
        if (ctx->pdfstoponerror)
            return_error(gs_error_stackunderflow);
        else
            return 0;
    }

    for (i=0;i < 4;i++){
        num = (pdf_num *)ctx->stack_top[i - 4];
        if (num->type != PDF_INT) {
            if(num->type != PDF_REAL) {
                pdfi_pop(ctx, 4);
                if (ctx->pdfstoponerror)
                    return_error(gs_error_typecheck);
                return 0;
            }
            else
                Values[i] = num->value.d;
        } else {
            Values[i] = (double)num->value.i;
        }
    }

    if (ctx->TextBlockDepth != 0)
        ctx->pdf_warnings |= W_PDF_OPINVALIDINTEXT;

    code = gs_currentpoint(ctx->pgs, &pt);
    if (code < 0) {
        pdfi_pop(ctx, 4);
        if (ctx->pdfstoponerror)
            return code;
        else
            return 0;
    }

    code = gs_curveto(ctx->pgs, pt.x, pt.y, Values[0], Values[1], Values[2], Values[3]);
    pdfi_pop(ctx, 4);
    if(code < 0 && ctx->pdfstoponerror)
        return code;
    else
        return 0;
}

int pdfi_y_curveto(pdf_context *ctx)
{
    int i, code;
    pdf_num *num;
    double Values[4];

    if (pdfi_count_stack(ctx) < 4) {
        pdfi_clearstack(ctx);
        if (ctx->pdfstoponerror)
            return_error(gs_error_stackunderflow);
        else
            return 0;
    }

    for (i=0;i < 4;i++){
        num = (pdf_num *)ctx->stack_top[i - 4];
        if (num->type != PDF_INT) {
            if(num->type != PDF_REAL) {
                pdfi_pop(ctx, 4);
                if (ctx->pdfstoponerror)
                    return_error(gs_error_typecheck);
                return 0;
            }
            else
                Values[i] = num->value.d;
        } else {
            Values[i] = (double)num->value.i;
        }
    }

    if (ctx->TextBlockDepth != 0)
        ctx->pdf_warnings |= W_PDF_OPINVALIDINTEXT;

    code = gs_curveto(ctx->pgs, Values[0], Values[1], Values[2], Values[3], Values[2], Values[3]);
    pdfi_pop(ctx, 4);
    if(code < 0 && ctx->pdfstoponerror)
        return code;
    else
        return 0;
}

int pdfi_closepath(pdf_context *ctx)
{
    int code = gs_closepath(ctx->pgs);

    if (ctx->TextBlockDepth != 0)
        ctx->pdf_warnings |= W_PDF_OPINVALIDINTEXT;

    if(code < 0 && ctx->pdfstoponerror)
        return code;
    else
        return 0;
}

int pdfi_newpath(pdf_context *ctx)
{
    int code = gs_newpath(ctx->pgs);

    if (ctx->TextBlockDepth != 0)
        ctx->pdf_warnings |= W_PDF_OPINVALIDINTEXT;

    if(code < 0 && ctx->pdfstoponerror)
        return code;
    else
        return 0;
}

int pdfi_b(pdf_context *ctx)
{
    int code;

    if (ctx->TextBlockDepth != 0)
        ctx->pdf_warnings |= W_PDF_OPINVALIDINTEXT;

    code = gs_closepath(ctx->pgs);
    if (code >= 0)
        code = pdfi_B(ctx);
    if(code < 0 && ctx->pdfstoponerror)
        return code;
    else
        return 0;
}

int pdfi_b_star(pdf_context *ctx)
{
    int code;

    if (ctx->TextBlockDepth != 0)
        ctx->pdf_warnings |= W_PDF_OPINVALIDINTEXT;

    code = gs_closepath(ctx->pgs);
    if (code >= 0)
        code = pdfi_B_star(ctx);
    if(code < 0 && ctx->pdfstoponerror)
        return code;
    else
        return 0;
}

/* common code for B and B* */
static int pdfi_B_inner(pdf_context *ctx, bool use_eofill)
{
    int code, code1;
    pdfi_trans_state_t state;
    bool started_group = false;

    if (ctx->TextBlockDepth != 0)
        ctx->pdf_warnings |= W_PDF_OPINVALIDINTEXT;

    if (pdfi_oc_is_off(ctx)) {
        code = gs_newpath(ctx->pgs);
        return code;
    }

    if (ctx->page_has_transparency) {
        code = gs_setopacityalpha(ctx->pgs, 1.0);
        if (code < 0)
            return code;
        code = pdfi_trans_begin_simple_group(ctx, true, true, true);
        if (code < 0)
            return code;
        started_group = true;
    }

    code = pdfi_gsave(ctx);
    if (code < 0)
        goto exit;
    gs_swapcolors_quick(ctx->pgs);
    code = pdfi_trans_set_params(ctx, gs_getfillconstantalpha(ctx->pgs));
    if (code == 0) {
        if (use_eofill)
            code = gs_eofill(ctx->pgs);
        else
            code = gs_fill(ctx->pgs);
    }
    gs_swapcolors_quick(ctx->pgs);
    code1 = pdfi_grestore(ctx);
    if (code == 0)
        code = code1;
    if (code < 0)
        goto exit;

    code = pdfi_trans_setup(ctx, &state, TRANSPARENCY_Caller_Stroke, gs_getstrokeconstantalpha(ctx->pgs));
    if (code >= 0)
        code = gs_stroke(ctx->pgs);
    code1 = pdfi_trans_teardown(ctx, &state);
    if (code == 0)
        code = code1;

 exit:
    if (started_group)
        code1 = pdfi_trans_end_simple_group(ctx);
    if (code == 0)
        code = code1;

    if (code < 0)
        gs_newpath(ctx->pgs);

    if(code < 0 && ctx->pdfstoponerror)
        return code;
    else
        return 0;
}

int pdfi_B(pdf_context *ctx)
{
    return pdfi_B_inner(ctx, false);
}

int pdfi_B_star(pdf_context *ctx)
{
    return pdfi_B_inner(ctx, true);
}

int pdfi_clip(pdf_context *ctx)
{
    int code = gs_clip(ctx->pgs);

    if (ctx->TextBlockDepth != 0)
        ctx->pdf_warnings |= W_PDF_OPINVALIDINTEXT;

    if(code < 0 && ctx->pdfstoponerror)
        return code;
    else
        return 0;
}

int pdfi_eoclip(pdf_context *ctx)
{
    int code = gs_eoclip(ctx->pgs);

    if (ctx->TextBlockDepth != 0)
        ctx->pdf_warnings |= W_PDF_OPINVALIDINTEXT;

    if(code < 0 && ctx->pdfstoponerror)
        return code;
    else
        return 0;
}

int pdfi_rectpath(pdf_context *ctx)
{
    int i, code;
    pdf_num *num;
    double Values[4];

    if (pdfi_count_stack(ctx) < 4) {
        pdfi_clearstack(ctx);
        if (ctx->pdfstoponerror)
            return_error(gs_error_stackunderflow);
        else
            return 0;
    }

    for (i=0;i < 4;i++){
        num = (pdf_num *)ctx->stack_top[i - 4];
        if (num->type != PDF_INT) {
            if(num->type != PDF_REAL) {
                pdfi_pop(ctx, 4);
                if(ctx->pdfstoponerror)
                    return_error(gs_error_typecheck);
                return 0;
            }
            else
                Values[i] = num->value.d;
        } else {
            Values[i] = (double)num->value.i;
        }
    }

    if (ctx->TextBlockDepth != 0)
        ctx->pdf_warnings |= W_PDF_OPINVALIDINTEXT;

    code = gs_moveto(ctx->pgs, Values[0], Values[1]);
    if (code == 0) {
        code = gs_rlineto(ctx->pgs, Values[2], 0);
        if (code == 0){
            code = gs_rlineto(ctx->pgs, 0, Values[3]);
            if (code == 0) {
                code = gs_rlineto(ctx->pgs, -Values[2], 0);
                if (code == 0){
                    code = gs_closepath(ctx->pgs);
                }
            }
        }
    }
    pdfi_pop(ctx, 4);
    if(code < 0 && ctx->pdfstoponerror)
        return code;
    else
        return 0;
}
