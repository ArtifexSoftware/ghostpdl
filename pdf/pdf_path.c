/* Copyright (C) 2018-2022 Artifex Software, Inc.
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
#include "pdf_font_types.h"
#include "pdf_gstate.h"
#include "pdf_path.h"
#include "pdf_stack.h"
#include "pdf_trans.h"
#include "gstypes.h"
#include "pdf_optcontent.h"
#include "gspath.h"         /* For gs_moveto() and friends */
#include "gspaint.h"        /* For gs_fill() and friends */

typedef enum path_segment_e {
    pdfi_moveto_seg,
    pdfi_lineto_seg,
    pdfi_curveto_seg,
    pdfi_re_seg,
    pdfi_v_curveto_seg,
    pdfi_y_curveto_seg,
    pdfi_closepath_seg
} pdfi_path_segment;

static int StorePathSegment(pdf_context *ctx, pdfi_path_segment segment, double *pts)
{
    int size = sizeof(char);
    char *op;
    double *dpts;

    switch (segment)
    {
        case pdfi_moveto_seg:
        case pdfi_lineto_seg:
            size += 2 * sizeof(double);
            break;
        case pdfi_re_seg:
        case pdfi_v_curveto_seg:
        case pdfi_y_curveto_seg:
            size += 4 * sizeof(double);
                break;
        case pdfi_curveto_seg:
            size += 6 * sizeof(double);
            break;
        case pdfi_closepath_seg:
            break;
    }
    if (ctx->PathBottom == NULL) {
        ctx->PathBottom = (char *)gs_alloc_bytes(ctx->memory, 4096, "StorePathSegment");
        if (ctx->PathBottom < 0)
            return_error(gs_error_VMerror);

        ctx->PathAccumulator = ctx->PathBottom;
        ctx->PathTop = ctx->PathBottom + 4096;
    }
    if ((char *)ctx->PathAccumulator + size > ctx->PathTop) {
        char *new_accumulator = NULL;
        uint64_t old_size;

        old_size = ctx->PathTop - ctx->PathBottom;
        new_accumulator = (char *)gs_alloc_bytes(ctx->memory, old_size + 4096, "StorePathSegment");
        if (new_accumulator == NULL)
            return_error(gs_error_VMerror);

        memcpy(new_accumulator, ctx->PathBottom, old_size);
        ctx->PathAccumulator = new_accumulator + (ctx->PathAccumulator - ctx->PathBottom);
        gs_free_object(ctx->memory, ctx->PathBottom, "StorePathSegment");
        ctx->PathBottom = new_accumulator;
        ctx->PathTop = ctx->PathBottom + old_size + 4096;
    }

    op = ctx->PathAccumulator;
    dpts = (double *)(op + 1);

    *op = (char)segment;
    switch (segment)
    {
        case pdfi_moveto_seg:
        case pdfi_lineto_seg:
            memcpy(dpts, pts, 2 * sizeof(double));
            dpts += 2;
            break;
        case pdfi_re_seg:
        case pdfi_v_curveto_seg:
        case pdfi_y_curveto_seg:
            memcpy(dpts, pts, 4 * sizeof(double));
            dpts += 4;
            break;
        case pdfi_curveto_seg:
            memcpy(dpts, pts, 6 * sizeof(double));
            dpts += 6;
            break;
        case pdfi_closepath_seg:
            break;
    }
    ctx->PathAccumulator = (void *)dpts;
    return 0;
}

static int ApplyStoredPath(pdf_context *ctx)
{
    int code = 0;
    char *op = NULL;
    double *dpts = NULL;

    if (ctx->PathBottom == NULL)
        return 0;

    if (ctx->pgs->current_point_valid) {
        code = gs_newpath(ctx->pgs);
        if (code < 0)
            return code;
    }

    op = ctx->PathBottom;
    while (op < ctx->PathAccumulator) {
        switch(*op++) {
            case pdfi_moveto_seg:
                dpts = (double *)op;
                op = (char *)(dpts + 2);
                code = gs_moveto(ctx->pgs, dpts[0], dpts[1]);
                break;
            case pdfi_lineto_seg:
                dpts = (double *)op;
                op = (char *)(dpts + 2);
                code = gs_lineto(ctx->pgs, dpts[0], dpts[1]);
                break;
            case pdfi_re_seg:
                dpts = (double *)op;
                op = (char *)(dpts + 4);
                code = gs_moveto(ctx->pgs, dpts[0], dpts[1]);
                if (code >= 0) {
                    code = gs_rlineto(ctx->pgs, dpts[2], 0);
                    if (code >= 0) {
                        code = gs_rlineto(ctx->pgs, 0, dpts[3]);
                        if (code >= 0) {
                            code = gs_rlineto(ctx->pgs, -dpts[2], 0);
                            if (code >= 0)
                                code = gs_closepath(ctx->pgs);
                        }
                    }
                }
                break;
            case pdfi_v_curveto_seg:
                {
                    gs_point pt;

                    code = gs_currentpoint(ctx->pgs, &pt);
                    if (code >= 0) {
                        dpts = (double *)op;
                        op = (char *)(dpts + 4);
                        code = gs_curveto(ctx->pgs, pt.x, pt.y, dpts[0], dpts[1], dpts[2], dpts[3]);
                    }
                }
                break;
            case pdfi_y_curveto_seg:
                dpts = (double *)op;
                op = (char *)(dpts + 4);
                code = gs_curveto(ctx->pgs, dpts[0], dpts[1], dpts[2], dpts[3], dpts[2], dpts[3]);
                break;
            case pdfi_curveto_seg:
                dpts = (double *)op;
                op = (char *)(dpts + 6);
                code = gs_curveto(ctx->pgs, dpts[0], dpts[1], dpts[2], dpts[3], dpts[4], dpts[5]);
                break;
            case pdfi_closepath_seg:
                code = gs_closepath(ctx->pgs);
                break;
            default:
                code = gs_note_error(gs_error_rangecheck);
                break;
        }
        if (code < 0)
            break;
    }

    gs_free_object(ctx->memory, ctx->PathBottom, "ApplyStoredPath");
    ctx->PathTop = ctx->PathAccumulator = ctx->PathBottom = NULL;
    return code;
}

int pdfi_moveto (pdf_context *ctx)
{
    int code;
    double xy[2];

    if (ctx->text.BlockDepth != 0)
        pdfi_set_warning(ctx, 0, NULL, W_PDF_OPINVALIDINTEXT, "pdfi_moveto", NULL);

    code = pdfi_destack_reals(ctx, xy, 2);
    if (code < 0)
        return code;

    return StorePathSegment(ctx, pdfi_moveto_seg, (double *)&xy);
}

int pdfi_lineto (pdf_context *ctx)
{
    int code;
    double xy[2];

    if (ctx->text.BlockDepth != 0)
        pdfi_set_warning(ctx, 0, NULL, W_PDF_OPINVALIDINTEXT, "pdfi_lineto", NULL);

    code = pdfi_destack_reals(ctx, xy, 2);
    if (code < 0)
        return code;

    return StorePathSegment(ctx, pdfi_lineto_seg, (double *)&xy);
}

static int pdfi_fill_inner(pdf_context *ctx, bool use_eofill)
{
    int code=0, code1;
    pdfi_trans_state_t state;

    if (ctx->text.BlockDepth != 0)
        pdfi_set_warning(ctx, 0, NULL, W_PDF_OPINVALIDINTEXT, "pdfi_fill_inner", NULL);

    if (pdfi_oc_is_off(ctx))
        goto exit;

    code = ApplyStoredPath(ctx);
    if (code < 0)
        return code;

    code = pdfi_trans_setup(ctx, &state, NULL, TRANSPARENCY_Caller_Fill);
    if (code == 0) {
        /* If we don't gsave/grestore round the fill, then the file
         * /tests_private/pdf/sumatra/954_-_dashed_lines_hardly_visible.pdf renders
         * incorrectly. However we must not gsave/grestore round the trans_setup
         * trans_teardown, because that might set pgs->soft_mask_id and if we restore
         * back to a point where that is not set then pdfwrite doesn't work properly.
         */
        code = pdfi_gsave(ctx);
        if (code < 0) goto exit;

        if (use_eofill)
            code = gs_eofill(ctx->pgs);
        else
            code = gs_fill(ctx->pgs);
        code1 = pdfi_grestore(ctx);
        if (code == 0) code = code1;

        code1 = pdfi_trans_teardown(ctx, &state);
        if (code == 0) code = code1;
    }

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

    code = ApplyStoredPath(ctx);
    if (code < 0)
        return code;

    gs_swapcolors_quick(ctx->pgs);
    code = pdfi_trans_setup(ctx, &state, NULL, TRANSPARENCY_Caller_Stroke);
    if (code == 0) {
        code = pdfi_gsave(ctx);
        if (code < 0) goto exit;

        code = gs_stroke(ctx->pgs);

        code1 = pdfi_grestore(ctx);
        if (code == 0) code = code1;

        code1 = pdfi_trans_teardown(ctx, &state);
        if (code == 0) code = code1;
    }
    gs_swapcolors_quick(ctx->pgs);

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

    code = StorePathSegment(ctx, pdfi_closepath_seg, NULL);
    if (code < 0)
        return code;

    return pdfi_stroke(ctx);
}

int pdfi_curveto(pdf_context *ctx)
{
    int code;
    double Values[6];

    code = pdfi_destack_reals(ctx, Values, 6);
    if (code < 0)
        return code;

    if (ctx->text.BlockDepth != 0)
        pdfi_set_warning(ctx, 0, NULL, W_PDF_OPINVALIDINTEXT, "pdfi_curveto", NULL);

    return StorePathSegment(ctx, pdfi_curveto_seg, (double *)&Values);
}

int pdfi_v_curveto(pdf_context *ctx)
{
    int code;
    double Values[4];

    code = pdfi_destack_reals(ctx, Values, 4);
    if (code < 0)
        return code;

    if (ctx->text.BlockDepth != 0)
        pdfi_set_warning(ctx, 0, NULL, W_PDF_OPINVALIDINTEXT, "pdfi_v_curveto", NULL);

    return StorePathSegment(ctx, pdfi_v_curveto_seg, (double *)&Values);
}

int pdfi_y_curveto(pdf_context *ctx)
{
    int code;
    double Values[4];

    code = pdfi_destack_reals(ctx, Values, 4);
    if (code < 0)
        return code;

    if (ctx->text.BlockDepth != 0)
        pdfi_set_warning(ctx, 0, NULL, W_PDF_OPINVALIDINTEXT, "pdfi_y_curveto", NULL);

    return StorePathSegment(ctx, pdfi_y_curveto_seg, (double *)&Values);
}

int pdfi_closepath(pdf_context *ctx)
{
    if (ctx->text.BlockDepth != 0)
        pdfi_set_warning(ctx, 0, NULL, W_PDF_OPINVALIDINTEXT, "pdfi_closepath", NULL);

    return StorePathSegment(ctx, pdfi_closepath_seg, NULL);
}

int pdfi_newpath(pdf_context *ctx)
{
    int code = 0, code1;

    /* This code is to deal with the wacky W and W* operators */
    if (ctx->clip_active) {
        if (ctx->PathBottom != NULL) {
            code = ApplyStoredPath(ctx);
            if (code < 0)
                return code;
        }
        if (ctx->pgs->current_point_valid) {
            if (ctx->do_eoclip)
                code = gs_eoclip(ctx->pgs);
            else
                code = gs_clip(ctx->pgs);
        }
    }
    ctx->clip_active = false;

    if (ctx->PathBottom != NULL){
        gs_free_object(ctx->memory, ctx->PathBottom, "ApplyStoredPath");
        ctx->PathTop= ctx->PathAccumulator = ctx->PathBottom = NULL;
    }

    code1 = gs_newpath(ctx->pgs);
    if (code == 0) code = code1;

    if (ctx->text.BlockDepth != 0)
        pdfi_set_warning(ctx, 0, NULL, W_PDF_OPINVALIDINTEXT, "pdfi_newpath", NULL);

    if (ctx->PathBottom != NULL)
    {
        gs_free_object(ctx->memory, ctx->PathBottom, "pdfi_newpath");
        ctx->PathBottom = ctx->PathTop = ctx->PathAccumulator = NULL;
    }
    return code;
}

int pdfi_b(pdf_context *ctx)
{
    int code;

    if (ctx->text.BlockDepth != 0)
        pdfi_set_warning(ctx, 0, NULL, W_PDF_OPINVALIDINTEXT, "pdfi_b", NULL);

    code = StorePathSegment(ctx, pdfi_closepath_seg, NULL);
    if (code < 0)
        return code;

    return pdfi_B(ctx);
}

int pdfi_b_star(pdf_context *ctx)
{
    int code;

    if (ctx->text.BlockDepth != 0)
        pdfi_set_warning(ctx, 0, NULL, W_PDF_OPINVALIDINTEXT, "pdfi_b_star", NULL);

    code = StorePathSegment(ctx, pdfi_closepath_seg, NULL);
    if (code < 0)
        return code;

    return pdfi_B_star(ctx);
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

    code = ApplyStoredPath(ctx);
    if (code < 0)
        return code;

    code = pdfi_trans_setup(ctx, &state, NULL, TRANSPARENCY_Caller_FillStroke);
    if (code == 0) {
        code = pdfi_gsave(ctx);
        if (code < 0) goto exit;

        if (use_eofill)
            code = gs_eofillstroke(ctx->pgs, &code1);
        else
            code = gs_fillstroke(ctx->pgs, &code1);

        code1 = pdfi_grestore(ctx);
        if (code == 0) code = code1;

        code1 = pdfi_trans_teardown(ctx, &state);
        if (code >= 0) code = code1;
    }

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
    int code;
    double Values[4];

    code = pdfi_destack_reals(ctx, Values, 4);
    if (code < 0)
        return code;

    if (ctx->text.BlockDepth != 0)
        pdfi_set_warning(ctx, 0, NULL, W_PDF_OPINVALIDINTEXT, "pdfi_rectpath", NULL);

    return StorePathSegment(ctx, pdfi_re_seg, (double *)&Values);
}
