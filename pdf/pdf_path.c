/* Copyright (C) 2018-2021 Artifex Software, Inc.
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
#include "gspath.h"         /* For gs_moveto() and friends */
#include "gspaint.h"        /* For gs_fill() and friends */

int pdfi_moveto (pdf_context *ctx)
{
    pdf_num *n1, *n2;
    int code;
    double x, y;

    if (ctx->text.BlockDepth != 0)
        pdfi_set_warning(ctx, 0, NULL, W_PDF_OPINVALIDINTEXT, "pdfi_moveto", NULL);

    if (pdfi_count_stack(ctx) < 2) {
        pdfi_clearstack(ctx);
        return_error(gs_error_stackunderflow);
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
            return_error(gs_error_typecheck);
        }
    }
    if (n2->type == PDF_INT){
        x = (double)n2->value.i;
    } else{
        if (n2->type == PDF_REAL) {
            x = n2->value.d;
        } else {
            pdfi_pop(ctx, 2);
            return_error(gs_error_typecheck);
        }
    }

    code = gs_moveto(ctx->pgs, x, y);
    pdfi_pop(ctx, 2);
    return code;
}

int pdfi_lineto (pdf_context *ctx)
{
    pdf_num *n1, *n2;
    int code;
    double x, y;

    if (ctx->text.BlockDepth != 0)
        pdfi_set_warning(ctx, 0, NULL, W_PDF_OPINVALIDINTEXT, "pdfi_lineto", NULL);

    if (pdfi_count_stack(ctx) < 2) {
        pdfi_clearstack(ctx);
        return_error(gs_error_stackunderflow);
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
            return_error(gs_error_typecheck);
        }
    }
    if (n2->type == PDF_INT){
        x = (double)n2->value.i;
    } else{
        if (n2->type == PDF_REAL) {
            x = n2->value.d;
        } else {
            pdfi_pop(ctx, 2);
            return_error(gs_error_typecheck);
        }
    }

    code = gs_lineto(ctx->pgs, x, y);
    pdfi_pop(ctx, 2);
    return code;
}

static int pdfi_fill_inner(pdf_context *ctx, bool use_eofill)
{
    int code=0, code1;
    pdfi_trans_state_t state;

    if (ctx->text.BlockDepth != 0)
        pdfi_set_warning(ctx, 0, NULL, W_PDF_OPINVALIDINTEXT, "pdfi_fill_inner", NULL);

    if (pdfi_oc_is_off(ctx))
        goto exit;

    code = pdfi_gsave(ctx);
    if (code < 0) goto exit;

    code = pdfi_trans_setup(ctx, &state, NULL, TRANSPARENCY_Caller_Fill);
    if (code == 0) {
        if (use_eofill)
            code = gs_eofill(ctx->pgs);
        else
            code = gs_fill(ctx->pgs);
        code1 = pdfi_trans_teardown(ctx, &state);
        if (code == 0) code = code1;
    }

    code1 = pdfi_grestore(ctx);
    if (code == 0) code = code1;

 exit:
    code1 = pdfi_newpath(ctx);
    if (code == 0) code = code1;

    return code;
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
    int code=0, code1;
    pdfi_trans_state_t state;

    if (ctx->text.BlockDepth != 0)
        pdfi_set_warning(ctx, 0, NULL, W_PDF_OPINVALIDINTEXT, "pdfi_stroke", NULL);

    if (pdfi_oc_is_off(ctx))
        goto exit;

    code = pdfi_gsave(ctx);
    if (code < 0) goto exit;

    gs_swapcolors_quick(ctx->pgs);
    code = pdfi_trans_setup(ctx, &state, NULL, TRANSPARENCY_Caller_Stroke);
    if (code == 0) {
        code = gs_stroke(ctx->pgs);
        code1 = pdfi_trans_teardown(ctx, &state);
        if (code == 0) code = code1;
    }
    gs_swapcolors_quick(ctx->pgs);

    code1 = pdfi_grestore(ctx);
    if (code == 0) code = code1;

 exit:
    code1 = pdfi_newpath(ctx);
    if (code == 0) code = code1;

    return code;
}

int pdfi_closepath_stroke(pdf_context *ctx)
{
    int code;

    if (ctx->text.BlockDepth != 0)
        pdfi_set_warning(ctx, 0, NULL, W_PDF_OPINVALIDINTEXT, "pdfi_closepath_stroke", NULL);

    code = gs_closepath(ctx->pgs);
    if (code == 0)
        code = pdfi_stroke(ctx);
    return code;
}

int pdfi_curveto(pdf_context *ctx)
{
    int i, code;
    pdf_num *num;
    double Values[6];

    if (pdfi_count_stack(ctx) < 6) {
        pdfi_clearstack(ctx);
        pdfi_set_error(ctx, 0, NULL, E_PDF_STACKUNDERFLOWERROR, "pdfi_curveto", NULL);
        return_error(gs_error_stackunderflow);
    }

    for (i=0;i < 6;i++){
        num = (pdf_num *)ctx->stack_top[i - 6];
        if (num->type != PDF_INT) {
            if(num->type != PDF_REAL) {
                pdfi_pop(ctx, 6);
                return_error(gs_error_typecheck);
            }
            else
                Values[i] = num->value.d;
        } else {
            Values[i] = (double)num->value.i;
        }
    }

    if (ctx->text.BlockDepth != 0)
        pdfi_set_warning(ctx, 0, NULL, W_PDF_OPINVALIDINTEXT, "pdfi_curveto", NULL);

    code = gs_curveto(ctx->pgs, Values[0], Values[1], Values[2], Values[3], Values[4], Values[5]);
    pdfi_pop(ctx, 6);
    return code;
}

int pdfi_v_curveto(pdf_context *ctx)
{
    int i, code;
    pdf_num *num;
    double Values[4];
    gs_point pt;

    if (pdfi_count_stack(ctx) < 4) {
        pdfi_clearstack(ctx);
        return_error(gs_error_stackunderflow);
    }

    for (i=0;i < 4;i++){
        num = (pdf_num *)ctx->stack_top[i - 4];
        if (num->type != PDF_INT) {
            if(num->type != PDF_REAL) {
                pdfi_pop(ctx, 4);
                return_error(gs_error_typecheck);
            }
            else
                Values[i] = num->value.d;
        } else {
            Values[i] = (double)num->value.i;
        }
    }

    if (ctx->text.BlockDepth != 0)
        pdfi_set_warning(ctx, 0, NULL, W_PDF_OPINVALIDINTEXT, "pdfi_v_curveto", NULL);

    code = gs_currentpoint(ctx->pgs, &pt);
    if (code < 0) {
        pdfi_pop(ctx, 4);
        return code;
    }

    code = gs_curveto(ctx->pgs, pt.x, pt.y, Values[0], Values[1], Values[2], Values[3]);
    pdfi_pop(ctx, 4);
    return code;
}

int pdfi_y_curveto(pdf_context *ctx)
{
    int i, code;
    pdf_num *num;
    double Values[4];

    if (pdfi_count_stack(ctx) < 4) {
        pdfi_clearstack(ctx);
        return_error(gs_error_stackunderflow);
    }

    for (i=0;i < 4;i++){
        num = (pdf_num *)ctx->stack_top[i - 4];
        if (num->type != PDF_INT) {
            if(num->type != PDF_REAL) {
                pdfi_pop(ctx, 4);
                return_error(gs_error_typecheck);
            }
            else
                Values[i] = num->value.d;
        } else {
            Values[i] = (double)num->value.i;
        }
    }

    if (ctx->text.BlockDepth != 0)
        pdfi_set_warning(ctx, 0, NULL, W_PDF_OPINVALIDINTEXT, "pdfi_y_curveto", NULL);

    code = gs_curveto(ctx->pgs, Values[0], Values[1], Values[2], Values[3], Values[2], Values[3]);
    pdfi_pop(ctx, 4);
    return code;
}

int pdfi_closepath(pdf_context *ctx)
{
    int code = gs_closepath(ctx->pgs);

    if (ctx->text.BlockDepth != 0)
        pdfi_set_warning(ctx, 0, NULL, W_PDF_OPINVALIDINTEXT, "pdfi_closepath", NULL);

    return code;
}

int pdfi_newpath(pdf_context *ctx)
{
    int code = 0, code1;

    /* This code is to deal with the wacky W and W* operators */
    if (ctx->pgs->current_point_valid) {
        if (ctx->clip_active) {
            if (ctx->do_eoclip)
                code = gs_eoclip(ctx->pgs);
            else
                code = gs_clip(ctx->pgs);
        }
    }
    ctx->clip_active = false;

    code1 = gs_newpath(ctx->pgs);
    if (code == 0) code = code1;

    if (ctx->text.BlockDepth != 0)
        pdfi_set_warning(ctx, 0, NULL, W_PDF_OPINVALIDINTEXT, "pdfi_newpath", NULL);

    return code;
}

int pdfi_b(pdf_context *ctx)
{
    int code;

    if (ctx->text.BlockDepth != 0)
        pdfi_set_warning(ctx, 0, NULL, W_PDF_OPINVALIDINTEXT, "pdfi_b", NULL);

    code = gs_closepath(ctx->pgs);
    if (code >= 0)
        code = pdfi_B(ctx);
    return code;
}

int pdfi_b_star(pdf_context *ctx)
{
    int code;

    if (ctx->text.BlockDepth != 0)
        pdfi_set_warning(ctx, 0, NULL, W_PDF_OPINVALIDINTEXT, "pdfi_b_star", NULL);

    code = gs_closepath(ctx->pgs);
    if (code >= 0)
        code = pdfi_B_star(ctx);
    return code;
}

/* common code for B and B* */
static int pdfi_B_inner(pdf_context *ctx, bool use_eofill)
{
    int code=0, code1=0;
    pdfi_trans_state_t state;

    if (ctx->text.BlockDepth != 0)
        pdfi_set_warning(ctx, 0, NULL, W_PDF_OPINVALIDINTEXT, "pdfi_B_inner", NULL);

    if (pdfi_oc_is_off(ctx))
        goto exit;

    code = pdfi_gsave(ctx);
    if (code < 0) goto exit;

    code = pdfi_trans_setup(ctx, &state, NULL, TRANSPARENCY_Caller_FillStroke);
    if (code == 0) {
        if (use_eofill)
            code = gs_eofillstroke(ctx->pgs, &code1);
        else
            code = gs_fillstroke(ctx->pgs, &code1);
        code1 = pdfi_trans_teardown(ctx, &state);
        if (code >= 0) code = code1;
    }

    code1 = pdfi_grestore(ctx);
    if (code == 0) code = code1;

 exit:
    code1 = pdfi_newpath(ctx);
    if (code == 0) code = code1;

    return code;
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

    if (ctx->text.BlockDepth != 0)
        pdfi_set_warning(ctx, 0, NULL, W_PDF_OPINVALIDINTEXT, "pdfi_B_clip", NULL);

    return code;
}

int pdfi_eoclip(pdf_context *ctx)
{
    int code = gs_eoclip(ctx->pgs);

    if (ctx->text.BlockDepth != 0)
        pdfi_set_warning(ctx, 0, NULL, W_PDF_OPINVALIDINTEXT, "pdfi_eoclip", NULL);

     return code;
}

int pdfi_rectpath(pdf_context *ctx)
{
    int i, code;
    pdf_num *num;
    double Values[4];

    if (pdfi_count_stack(ctx) < 4) {
        pdfi_clearstack(ctx);
        return_error(gs_error_stackunderflow);
    }

    for (i=0;i < 4;i++){
        num = (pdf_num *)ctx->stack_top[i - 4];
        if (num->type != PDF_INT) {
            if(num->type != PDF_REAL) {
                pdfi_pop(ctx, 4);
                return_error(gs_error_typecheck);
            }
            else
                Values[i] = num->value.d;
        } else {
            Values[i] = (double)num->value.i;
        }
    }

    if (ctx->text.BlockDepth != 0)
        pdfi_set_warning(ctx, 0, NULL, W_PDF_OPINVALIDINTEXT, "pdfi_rectpath", NULL);

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
    return code;
}
