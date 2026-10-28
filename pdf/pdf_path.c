/* Copyright (C) 2018-2026 Artifex Software, Inc.
   All Rights Reserved.

   This software is provided AS-IS with no warranty, either express or
   implied.

   This software is distributed under license and may not be copied,
   modified or distributed except as expressly authorized under the terms
   of the license contained in the file LICENSE in this distribution.

   Refer to licensing information at http://www.artifex.com or contact
   Artifex Software, Inc.,  39 Mesa Street, Suite 108A, San Francisco,
   CA 94129, USA, for further information.
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
#include "gxdevsop.h"               /* For special ops */

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
    int size = 0;
    path_record *PathRecord;

    switch (segment)
    {
        case pdfi_moveto_seg:
        case pdfi_lineto_seg:
            size = 2;
            break;
        case pdfi_re_seg:
        case pdfi_v_curveto_seg:
        case pdfi_y_curveto_seg:
            size = 4;
                break;
        case pdfi_curveto_seg:
            size = 6;
            break;
        case pdfi_closepath_seg:
            break;
        default:
            return_error(gs_error_undefined);
            break;
    }
    if (ctx->PathRecord == NULL) {
        ctx->PathRecord = (path_record *)gs_alloc_bytes(ctx->memory, sizeof(path_record), "StorePathSegment");
        if (ctx->PathRecord == NULL)
            return_error(gs_error_VMerror);
        memset(ctx->PathRecord, 0x00, sizeof(path_record));
    }
    PathRecord = ctx->PathRecord;

    if (PathRecord->PathSegments == NULL) {
        PathRecord->PathSegments = (char *)gs_alloc_bytes(ctx->memory, 1024, "StorePathSegment");
        if (PathRecord->PathSegments == NULL)
            return_error(gs_error_VMerror);
        PathRecord->PathSegmentsCurrent = PathRecord->PathSegments;
        PathRecord->PathSegmentsTop = PathRecord->PathSegments + 1024;
    }
    if (PathRecord->PathSegmentsCurrent == PathRecord->PathSegmentsTop) {
        char *new_accumulator = NULL;
        uint64_t old_size;

        old_size = PathRecord->PathSegmentsCurrent - PathRecord->PathSegments;
        new_accumulator = (char *)gs_alloc_bytes(ctx->memory, old_size + 1024, "StorePathSegment");
        if (new_accumulator == NULL)
            return_error(gs_error_VMerror);
        memcpy(new_accumulator, PathRecord->PathSegments, old_size);
        PathRecord->PathSegmentsCurrent = new_accumulator + old_size;
        gs_free_object(ctx->memory, PathRecord->PathSegments, "StorePathSegment");
        PathRecord->PathSegments = new_accumulator;
        PathRecord->PathSegmentsTop = PathRecord->PathSegments + old_size + 1024;
    }

    if (PathRecord->PathPts == NULL) {
        PathRecord->PathPts = (double *)gs_alloc_bytes(ctx->memory, 4096, "StorePathSegment");
        if (PathRecord->PathPts == NULL)
            return_error(gs_error_VMerror);
        PathRecord->PathPtsCurrent = PathRecord->PathPts;
        PathRecord->PathPtsTop = PathRecord->PathPts + (4096 / sizeof(double));
    }
    if (PathRecord->PathPtsCurrent + size > PathRecord->PathPtsTop) {
        double *new_accumulator = NULL;
        uint64_t old_size;

        old_size = (char *)PathRecord->PathPtsCurrent - (char *)PathRecord->PathPts;
        new_accumulator = (double *)gs_alloc_bytes(ctx->memory, old_size + 4096, "StorePathSegment");
        if (new_accumulator == NULL)
            return_error(gs_error_VMerror);
        memcpy(new_accumulator, PathRecord->PathPts, old_size);
        PathRecord->PathPtsCurrent = new_accumulator + (old_size / sizeof(double));
        gs_free_object(ctx->memory, PathRecord->PathPts, "StorePathSegment");
        PathRecord->PathPts = new_accumulator;
        PathRecord->PathPtsTop = PathRecord->PathPts + ((old_size + 4096) / sizeof(double));
    }

    *(PathRecord->PathSegmentsCurrent++) = (char)segment;
    switch (segment)
    {
        case pdfi_moveto_seg:
        case pdfi_lineto_seg:
            memcpy(PathRecord->PathPtsCurrent, pts, 2 * sizeof(double));
            PathRecord->PathPtsCurrent += 2;
            break;
        case pdfi_re_seg:
        case pdfi_v_curveto_seg:
        case pdfi_y_curveto_seg:
            memcpy(PathRecord->PathPtsCurrent, pts, 4 * sizeof(double));
            PathRecord->PathPtsCurrent += 4;
            break;
        case pdfi_curveto_seg:
            memcpy(PathRecord->PathPtsCurrent, pts, 6 * sizeof(double));
            PathRecord->PathPtsCurrent += 6;
            break;
        case pdfi_closepath_seg:
            break;
    }
    return 0;
}

int FreePathRecord (pdf_context *ctx)
{
    path_record *PathRecord;

    if (ctx->PathRecord == NULL)
        return 0;

    PathRecord = ctx->PathRecord;

    gs_free_object(ctx->memory, PathRecord->PathSegments, "FreePathRecord");
    PathRecord->PathSegmentsTop = PathRecord->PathSegmentsCurrent = PathRecord->PathSegments = NULL;
    gs_free_object(ctx->memory, PathRecord->PathPts, "FreePathRecord");
    PathRecord->PathPtsTop = PathRecord->PathPtsCurrent = PathRecord->PathPts = NULL;
    gs_free_object(ctx->memory, PathRecord, "FreePathRecord");
    ctx->PathRecord = NULL;

    return 0;
}

static int ApplyStoredPath(pdf_context *ctx)
{
    int code = 0;
    char *op = NULL;
    double *dpts = NULL;
    double *current;
    path_record *PathRecord;

    if (ctx->PathRecord == NULL)
        return 0;

    PathRecord = ctx->PathRecord;

    if (PathRecord->PathSegments == NULL) {
        code = gs_note_error(gs_error_unknownerror);
        goto error;
    }

    if (PathRecord->PathPts == NULL) {
        code = gs_note_error(gs_error_unknownerror);
        goto error;
    }

    if (ctx->pgs->current_point_valid) {
        code = gs_newpath(ctx->pgs);
        if (code < 0)
            goto error;
    }

    op = PathRecord->PathSegments;
    dpts = PathRecord->PathPts;
    current = dpts; /* Stop stupid compilers complaining. */

    while (op < PathRecord->PathSegmentsCurrent) {
        if (dpts > PathRecord->PathPtsCurrent) {
            code = gs_note_error(gs_error_unknownerror);
            goto error;
        }

        switch(*op++) {
            case pdfi_moveto_seg:
                code = gs_moveto(ctx->pgs, dpts[0], dpts[1]);
                current = dpts;
                dpts+= 2;
                break;
            case pdfi_lineto_seg:
                code = gs_lineto(ctx->pgs, dpts[0], dpts[1]);
                current = dpts;
                dpts+= 2;
                break;
            case pdfi_re_seg:
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
                current = dpts;
                dpts+= 4;
                break;
            case pdfi_v_curveto_seg:
                code = gs_curveto(ctx->pgs, current[0], current[1], dpts[0], dpts[1], dpts[2], dpts[3]);
                current = dpts + 2;
                dpts+= 4;
                break;
            case pdfi_y_curveto_seg:
                code = gs_curveto(ctx->pgs, dpts[0], dpts[1], dpts[2], dpts[3], dpts[2], dpts[3]);
                current = dpts + 2;
                dpts+= 4;
                break;
            case pdfi_curveto_seg:
                code = gs_curveto(ctx->pgs, dpts[0], dpts[1], dpts[2], dpts[3], dpts[4], dpts[5]);
                current = dpts + 4;
                dpts+= 6;
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

error:
    FreePathRecord(ctx);
    return code;
}

int pdfi_moveto (pdf_context *ctx)
{
    int code;
    double xy[2];

    if (ctx->text.BlockDepth != 0) {
        if (ctx->text.TextClip) {
            gx_device *dev = gs_currentdevice_inline(ctx->pgs);

            ctx->text.TextClip = false;
            (void)dev_proc(dev, dev_spec_op)(dev, gxdso_hilevel_text_clip, (void *)0, 1);
        }
        code = pdfi_set_warning_stop(ctx, gs_note_error(gs_error_syntaxerror), NULL, W_PDF_OPINVALIDINTEXT, "pdfi_moveto", NULL);
        if (code < 0)
            return code;
    }

    code = pdfi_destack_reals(ctx, xy, 2);
    if (code < 0)
        return code;

    return StorePathSegment(ctx, pdfi_moveto_seg, (double *)&xy);
}

int pdfi_lineto (pdf_context *ctx)
{
    int code;
    double xy[2];

    if (ctx->text.BlockDepth != 0) {
        ctx->text.BlockDepth = 0;
        if (ctx->text.TextClip) {
            gx_device *dev = gs_currentdevice_inline(ctx->pgs);

            ctx->text.TextClip = false;
            (void)dev_proc(dev, dev_spec_op)(dev, gxdso_hilevel_text_clip, (void *)0, 1);
        }
        code = pdfi_set_warning_stop(ctx, gs_note_error(gs_error_syntaxerror), NULL, W_PDF_OPINVALIDINTEXT, "pdfi_lineto", NULL);
        if (code < 0)
            return code;
    }

    code = pdfi_destack_reals(ctx, xy, 2);
    if (code < 0)
        return code;

    return StorePathSegment(ctx, pdfi_lineto_seg, (double *)&xy);
}

static int pdfi_fill_inner(pdf_context *ctx, bool use_eofill)
{
    int code=0, code1;
    pdfi_trans_state_t state;

    if (ctx->text.BlockDepth != 0) {
        ctx->text.BlockDepth = 0;
        if (ctx->text.TextClip) {
            gx_device *dev = gs_currentdevice_inline(ctx->pgs);

            ctx->text.TextClip = false;
            (void)dev_proc(dev, dev_spec_op)(dev, gxdso_hilevel_text_clip, (void *)0, 1);
        }
        code = pdfi_set_warning_stop(ctx, gs_note_error(gs_error_syntaxerror), NULL, W_PDF_OPINVALIDINTEXT, "pdfi_fill_inner", NULL);
        if (code < 0)
            return code;
    }

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

    if (ctx->text.BlockDepth != 0) {
        ctx->text.BlockDepth = 0;
        if (ctx->text.TextClip) {
            gx_device *dev = gs_currentdevice_inline(ctx->pgs);

            ctx->text.TextClip = false;
            (void)dev_proc(dev, dev_spec_op)(dev, gxdso_hilevel_text_clip, (void *)0, 1);
        }
        code = pdfi_set_warning_stop(ctx, gs_note_error(gs_error_syntaxerror), NULL, W_PDF_OPINVALIDINTEXT, "pdfi_stroke", NULL);
        if (code < 0)
            return code;
    }

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

    if (ctx->text.BlockDepth != 0) {
        ctx->text.BlockDepth = 0;
        if (ctx->text.TextClip) {
            gx_device *dev = gs_currentdevice_inline(ctx->pgs);

            ctx->text.TextClip = false;
            (void)dev_proc(dev, dev_spec_op)(dev, gxdso_hilevel_text_clip, (void *)0, 1);
        }
        code = pdfi_set_warning_stop(ctx, gs_note_error(gs_error_syntaxerror), NULL, W_PDF_OPINVALIDINTEXT, "pdfi_closepath_stroke", NULL);
        if (code < 0)
            return code;
    }

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

    if (ctx->text.BlockDepth != 0) {
        ctx->text.BlockDepth = 0;
        if (ctx->text.TextClip) {
            gx_device *dev = gs_currentdevice_inline(ctx->pgs);

            ctx->text.TextClip = false;
            (void)dev_proc(dev, dev_spec_op)(dev, gxdso_hilevel_text_clip, (void *)0, 1);
        }
        code = pdfi_set_warning_stop(ctx, gs_note_error(gs_error_syntaxerror), NULL, W_PDF_OPINVALIDINTEXT, "pdfi_curveto", NULL);
        if (code < 0)
            return code;
    }

    return StorePathSegment(ctx, pdfi_curveto_seg, (double *)&Values);
}

int pdfi_v_curveto(pdf_context *ctx)
{
    int code;
    double Values[4];

    code = pdfi_destack_reals(ctx, Values, 4);
    if (code < 0)
        return code;

    if (ctx->text.BlockDepth != 0) {
        ctx->text.BlockDepth = 0;
        if (ctx->text.TextClip) {
            gx_device *dev = gs_currentdevice_inline(ctx->pgs);

            ctx->text.TextClip = false;
            (void)dev_proc(dev, dev_spec_op)(dev, gxdso_hilevel_text_clip, (void *)0, 1);
        }
        code = pdfi_set_warning_stop(ctx, gs_note_error(gs_error_syntaxerror), NULL, W_PDF_OPINVALIDINTEXT, "pdfi_v_curveto", NULL);
        if (code < 0)
            return code;
    }

    return StorePathSegment(ctx, pdfi_v_curveto_seg, (double *)&Values);
}

int pdfi_y_curveto(pdf_context *ctx)
{
    int code;
    double Values[4];

    code = pdfi_destack_reals(ctx, Values, 4);
    if (code < 0)
        return code;

    if (ctx->text.BlockDepth != 0) {
        ctx->text.BlockDepth = 0;
        if (ctx->text.TextClip) {
            gx_device *dev = gs_currentdevice_inline(ctx->pgs);

            ctx->text.TextClip = false;
            (void)dev_proc(dev, dev_spec_op)(dev, gxdso_hilevel_text_clip, (void *)0, 1);
        }
        code = pdfi_set_warning_stop(ctx, gs_note_error(gs_error_syntaxerror), NULL, W_PDF_OPINVALIDINTEXT, "pdfi_y_curveto", NULL);
        if (code < 0)
            return code;
    }

    return StorePathSegment(ctx, pdfi_y_curveto_seg, (double *)&Values);
}

int pdfi_closepath(pdf_context *ctx)
{
    int code;

    if (ctx->text.BlockDepth != 0) {
        ctx->text.BlockDepth = 0;
        if (ctx->text.TextClip) {
            gx_device *dev = gs_currentdevice_inline(ctx->pgs);

            ctx->text.TextClip = false;
            (void)dev_proc(dev, dev_spec_op)(dev, gxdso_hilevel_text_clip, (void *)0, 1);
        }
        code = pdfi_set_warning_stop(ctx, gs_note_error(gs_error_syntaxerror), NULL, W_PDF_OPINVALIDINTEXT, "pdfi_closepath", NULL);
        if (code < 0)
            return code;
    }

    return StorePathSegment(ctx, pdfi_closepath_seg, NULL);
}

int pdfi_newpath(pdf_context *ctx)
{
    int code = 0, code1;

    /* This code is to deal with the wacky W and W* operators */
    if (ctx->clip_active) {
        if (ctx->PathRecord != NULL) {
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

    if (ctx->PathRecord != NULL)
        FreePathRecord(ctx);

    code1 = gs_newpath(ctx->pgs);
    if (code == 0) code = code1;

    if (ctx->text.BlockDepth != 0) {
        ctx->text.BlockDepth = 0;
        if (ctx->text.TextClip) {
            gx_device *dev = gs_currentdevice_inline(ctx->pgs);

            ctx->text.TextClip = false;
            (void)dev_proc(dev, dev_spec_op)(dev, gxdso_hilevel_text_clip, (void *)0, 1);
        }
        code = pdfi_set_warning_stop(ctx, gs_note_error(gs_error_syntaxerror), NULL, W_PDF_OPINVALIDINTEXT, "pdfi_newpath", NULL);
        if (code < 0)
            return code;
    }

    return code;
}

int pdfi_b(pdf_context *ctx)
{
    int code;

    if (ctx->text.BlockDepth != 0) {
        ctx->text.BlockDepth = 0;
        if (ctx->text.TextClip) {
            gx_device *dev = gs_currentdevice_inline(ctx->pgs);

            ctx->text.TextClip = false;
            (void)dev_proc(dev, dev_spec_op)(dev, gxdso_hilevel_text_clip, (void *)0, 1);
        }
        code = pdfi_set_warning_stop(ctx, gs_note_error(gs_error_syntaxerror), NULL, W_PDF_OPINVALIDINTEXT, "pdfi_b", NULL);
        if (code < 0)
            return code;
    }

    code = StorePathSegment(ctx, pdfi_closepath_seg, NULL);
    if (code < 0)
        return code;

    return pdfi_B(ctx);
}

int pdfi_b_star(pdf_context *ctx)
{
    int code;

    if (ctx->text.BlockDepth != 0) {
        ctx->text.BlockDepth = 0;
        if (ctx->text.TextClip) {
            gx_device *dev = gs_currentdevice_inline(ctx->pgs);

            ctx->text.TextClip = false;
            (void)dev_proc(dev, dev_spec_op)(dev, gxdso_hilevel_text_clip, (void *)0, 1);
        }
        code = pdfi_set_warning_stop(ctx, gs_note_error(gs_error_syntaxerror), NULL, W_PDF_OPINVALIDINTEXT, "pdfi_b_star", NULL);
        if (code < 0)
            return code;
    }

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

    if (ctx->text.BlockDepth != 0) {
        ctx->text.BlockDepth = 0;
        if (ctx->text.TextClip) {
            gx_device *dev = gs_currentdevice_inline(ctx->pgs);

            ctx->text.TextClip = false;
            (void)dev_proc(dev, dev_spec_op)(dev, gxdso_hilevel_text_clip, (void *)0, 1);
        }
        code = pdfi_set_warning_stop(ctx, gs_note_error(gs_error_syntaxerror), NULL, W_PDF_OPINVALIDINTEXT, "pdfi_B_inner", NULL);
        if (code < 0)
            goto exit;
    }

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

    if (ctx->text.BlockDepth != 0) {
        ctx->text.BlockDepth = 0;
        if (ctx->text.TextClip) {
            gx_device *dev = gs_currentdevice_inline(ctx->pgs);

            ctx->text.TextClip = false;
            (void)dev_proc(dev, dev_spec_op)(dev, gxdso_hilevel_text_clip, (void *)0, 1);
        }
        code = pdfi_set_warning_stop(ctx, gs_note_error(gs_error_syntaxerror), NULL, W_PDF_OPINVALIDINTEXT, "pdfi_B_clip", NULL);
        if (code < 0)
            return code;
    }

    return code;
}

int pdfi_eoclip(pdf_context *ctx)
{
    int code = gs_eoclip(ctx->pgs);

    if (ctx->text.BlockDepth != 0) {
        ctx->text.BlockDepth = 0;
        if (ctx->text.TextClip) {
            gx_device *dev = gs_currentdevice_inline(ctx->pgs);

            ctx->text.TextClip = false;
            (void)dev_proc(dev, dev_spec_op)(dev, gxdso_hilevel_text_clip, (void *)0, 1);
        }
        code = pdfi_set_warning_stop(ctx, gs_note_error(gs_error_syntaxerror), NULL, W_PDF_OPINVALIDINTEXT, "pdfi_eoclip", NULL);
        if (code < 0)
            return code;
    }

     return code;
}

int pdfi_rectpath(pdf_context *ctx)
{
    int code;
    double Values[4];

    code = pdfi_destack_reals(ctx, Values, 4);
    if (code < 0)
        return code;

    if (ctx->text.BlockDepth != 0) {
        ctx->text.BlockDepth = 0;
        if (ctx->text.TextClip) {
            gx_device *dev = gs_currentdevice_inline(ctx->pgs);

            ctx->text.TextClip = false;
            (void)dev_proc(dev, dev_spec_op)(dev, gxdso_hilevel_text_clip, (void *)0, 1);
        }
        code = pdfi_set_warning_stop(ctx, gs_note_error(gs_error_syntaxerror), NULL, W_PDF_OPINVALIDINTEXT, "pdfi_rectpath", NULL);
        if (code < 0)
            return code;
    }

    return StorePathSegment(ctx, pdfi_re_seg, (double *)&Values);
}
