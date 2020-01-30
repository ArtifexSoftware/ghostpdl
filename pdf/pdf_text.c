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

/* Text operations for the PDF interpreter */

#include "pdf_int.h"
#include "pdf_array.h"
#include "pdf_text.h"
#include "pdf_image.h"
#include "pdf_stack.h"
#include "pdf_gstate.h"
#include "pdf_font.h"
#include "pdf_font_types.h"

#include "gsstate.h"
#include "gsmatrix.h"

static int pdfi_set_TL(pdf_context *ctx, double TL);

int pdfi_BT(pdf_context *ctx)
{
    int code;
    gs_matrix m;

    if (ctx->TextBlockDepth != 0)
        ctx->pdf_warnings |= W_PDF_NESTEDTEXTBLOCK;

    gs_make_identity(&m);
    code = gs_settextmatrix(ctx->pgs, &m);
    if (code < 0)
        return code;

    code = gs_settextlinematrix(ctx->pgs, &m);
    if (code < 0)
        return code;

    code = gs_moveto(ctx->pgs, 0, 0);
    ctx->TextBlockDepth++;
    return code;
}

int pdfi_ET(pdf_context *ctx)
{
    int code = 0;

    if (ctx->TextBlockDepth == 0) {
        ctx->pdf_warnings |= W_PDF_ETNOTEXTBLOCK;
        return_error(gs_error_syntaxerror);
    }

    ctx->TextBlockDepth--;
    if (gs_currenttextrenderingmode(ctx->pgs) >= 4)
        gs_clip(ctx->pgs);
    return code;
}

int pdfi_T_star(pdf_context *ctx)
{
    int code;
    gs_matrix m, mat;

    if (ctx->TextBlockDepth == 0) {
        ctx->pdf_warnings |= W_PDF_TEXTOPNOBT;
    }

    gs_make_identity(&m);
    m.ty += ctx->pgs->textleading;

    code = gs_matrix_multiply(&m, &ctx->pgs->textlinematrix, &mat);
    if (code < 0)
        return code;

    code = gs_settextmatrix(ctx->pgs, (gs_matrix *)&mat);
    if (code < 0)
        return code;

    code = gs_settextlinematrix(ctx->pgs, (gs_matrix *)&mat);
    return code;
}

static int pdfi_set_Tc(pdf_context *ctx, double Tc)
{
    return gs_settextspacing(ctx->pgs, Tc);
}

int pdfi_Tc(pdf_context *ctx)
{
    int code = 0;
    pdf_num *n = NULL;

    if (pdfi_count_stack(ctx) < 1) {
        pdfi_clearstack(ctx);
        return_error(gs_error_stackunderflow);
    }

    n = (pdf_num *)ctx->stack_top[-1];

    if (n->type == PDF_INT)
        code = pdfi_set_Tc(ctx, (double)n->value.i);
    else {
        if (n->type == PDF_REAL)
            code = pdfi_set_Tc(ctx, n->value.d);
        else
            code = gs_note_error(gs_error_typecheck);
    }
    pdfi_pop(ctx, 1);
    return code;
}

int pdfi_Td(pdf_context *ctx)
{
    int code;
    pdf_num *Tx = NULL, *Ty = NULL;
    gs_matrix m, mat;

    if (pdfi_count_stack(ctx) < 2) {
        pdfi_clearstack(ctx);
        return_error(gs_error_stackunderflow);
    }

    gs_make_identity(&m);

    Ty = (pdf_num *)ctx->stack_top[-1];
    Tx = (pdf_num *)ctx->stack_top[-2];

    if (Tx->type == PDF_INT) {
        m.tx = (float)Tx->value.i;
    } else {
        if (Tx->type == PDF_REAL) {
            m.tx = (float)Tx->value.d;
        } else {
            code = gs_note_error(gs_error_typecheck);
            goto Td_error;
        }
    }

    if (Ty->type == PDF_INT) {
        m.ty = (float)Ty->value.i;
    } else {
        if (Ty->type == PDF_REAL) {
            m.ty = (float)Ty->value.d;
        } else {
            code = gs_note_error(gs_error_typecheck);
            goto Td_error;
        }
    }

    if (ctx->TextBlockDepth == 0) {
        ctx->pdf_warnings |= W_PDF_TEXTOPNOBT;

        gs_make_identity(&mat);
        code = gs_settextmatrix(ctx->pgs, &mat);
        if (code < 0)
            goto Td_error;

        code = gs_settextlinematrix(ctx->pgs, &mat);
        if (code < 0)
            goto Td_error;
    }

    code = gs_matrix_multiply(&m, &ctx->pgs->textlinematrix, &mat);
    if (code < 0)
        goto Td_error;

    code = gs_settextmatrix(ctx->pgs, (gs_matrix *)&mat);
    if (code < 0)
        goto Td_error;

    code = gs_settextlinematrix(ctx->pgs, (gs_matrix *)&mat);
    if (code < 0)
        goto Td_error;

    pdfi_pop(ctx, 2);
    return code;

Td_error:
    pdfi_pop(ctx, 2);
    return code;
}

int pdfi_TD(pdf_context *ctx)
{
    int code;
    pdf_num *Tx = NULL, *Ty = NULL;
    gs_matrix m, mat;

    if (pdfi_count_stack(ctx) < 2) {
        pdfi_clearstack(ctx);
        return_error(gs_error_stackunderflow);
    }

    gs_make_identity(&m);

    Ty = (pdf_num *)ctx->stack_top[-1];
    Tx = (pdf_num *)ctx->stack_top[-2];

    if (Tx->type == PDF_INT) {
        m.tx = (float)Tx->value.i;
    } else {
        if (Tx->type == PDF_REAL) {
            m.tx = (float)Tx->value.d;
        } else {
            code = gs_note_error(gs_error_typecheck);
            goto TD_error;
        }
    }

    if (Ty->type == PDF_INT) {
        m.ty = (float)Ty->value.i;
    } else {
        if (Ty->type == PDF_REAL) {
            m.ty = (float)Ty->value.d;
        } else {
            code = gs_note_error(gs_error_typecheck);
            goto TD_error;
        }
    }

    if (ctx->TextBlockDepth == 0) {
        ctx->pdf_warnings |= W_PDF_TEXTOPNOBT;

        gs_make_identity(&mat);
        code = gs_settextmatrix(ctx->pgs, &mat);
        if (code < 0)
            goto TD_error;

        code = gs_settextlinematrix(ctx->pgs, &mat);
        if (code < 0)
            goto TD_error;
    }

    code = pdfi_set_TL(ctx, m.ty * 1.0f);
    if (code < 0)
        goto TD_error;

    code = gs_matrix_multiply(&m, &ctx->pgs->textlinematrix, &mat);
    if (code < 0)
        goto TD_error;

    code = gs_settextmatrix(ctx->pgs, (gs_matrix *)&mat);
    if (code < 0)
        goto TD_error;

    code = gs_settextlinematrix(ctx->pgs, (gs_matrix *)&mat);
    if (code < 0)
        goto TD_error;

    pdfi_pop(ctx, 2);
    return code;

TD_error:
    pdfi_pop(ctx, 2);
    return code;
}

static int pdfi_show(pdf_context *ctx, pdf_string *s)
{
    int code = 0, i;
    gs_text_enum_t *penum;
    gs_text_params_t text;
    gs_matrix mat;
    pdf_font *current_font = NULL;
    float *x_widths = NULL, *y_widths = NULL, width;
    double Tw = 0, Tc = 0;
    int Trmode = 0;

    if (ctx->TextBlockDepth == 0) {
        ctx->pdf_warnings |= W_PDF_TEXTOPNOBT;
    }

    current_font = pdfi_get_current_pdf_font(ctx);

    if (current_font == NULL) {
        /* This should be an error, but until we have all the font
         * types available we can get a NULL font even though there's
         * nthing wrong with the file, so for now don't throw an error.
         */
        return 0;
        return_error(gs_error_invalidfont);
    }

    if (current_font->pdfi_font_type == e_pdf_font_type1 ||
        current_font->pdfi_font_type == e_pdf_font_cff ||
        current_font->pdfi_font_type == e_pdf_font_type3 ||
        current_font->pdfi_font_type == e_pdf_font_cff ||
        current_font->pdfi_font_type == e_pdf_font_truetype)
    {
        /* Simple fonts, 1-byte encodings */

        if (current_font->Widths == NULL) {
            text.operation = TEXT_FROM_STRING | TEXT_DO_DRAW | TEXT_RETURN_WIDTH;
        } else {
            gs_point pt;

            x_widths = (float *)gs_alloc_bytes(ctx->memory, s->length * sizeof(float), "X widths array for text");
            y_widths = (float *)gs_alloc_bytes(ctx->memory, s->length * sizeof(float), "Y widths array for text");
            if (x_widths == NULL || y_widths == NULL)
                goto show_error;

            memset(x_widths, 0x00, s->length * sizeof(float));
            memset(y_widths, 0x00, s->length * sizeof(float));

            /* To calculate the Width (which is defined in unscaled text units) as a value in user
             * space we need to transform it by the FontMatrix
             */
            mat = current_font->pfont->FontMatrix;

            /* Same for the Tc value (its defined in unscaled text units) */
            Tc = gs_currenttextspacing(ctx->pgs);
            if (Tc != 0) {
                gs_distance_transform(ctx->pgs->textspacing, 0, &mat, &pt);
                Tc = pt.x;
            }

            for (i = 0;i < s->length; i++) {
                /* Get the width (in unscaled text units) */
                if (s->data[i] < current_font->FirstChar || s->data[i] > current_font->LastChar)
                    width = 0;
                else
                    width = current_font->Widths[s->data[i] - current_font->FirstChar];
                /* And convert the width into an appropriate value for the current environment */
                gs_distance_transform(width, 0, &mat, &pt);
                x_widths[i] = pt.x;
                /* Add any Tc value */
                x_widths[i] += Tc;
            }
            text.operation = TEXT_FROM_STRING | TEXT_RETURN_WIDTH | TEXT_REPLACE_WIDTHS;
            text.x_widths = x_widths;
            text.y_widths = y_widths;
            text.widths_size = s->length * 2;
            Tw = gs_currentwordspacing(ctx->pgs);
            if (Tw != 0) {
                text.operation |= TEXT_ADD_TO_SPACE_WIDTH;
                text.delta_space.x = Tw / ctx->pgs->PDFfontsize;
                text.delta_space.y = 0;
                text.space.s_char = 0x20;
            }
        }
        text.data.chars = (const gs_char *)s->data;
        text.size = s->length;

        Trmode = gs_currenttextrenderingmode(ctx->pgs);
        if (current_font->pdfi_font_type == e_pdf_font_type3 && Trmode != 0 && Trmode != 3)
            Trmode = 0;
    } else {
        /* CID fonts, multi-byte encodings. Not implemented yet. */
        return 0;
    }

    if (ctx->preserve_tr_mode) {
        if (Trmode == 3)
            text.operation = TEXT_DO_NONE | TEXT_RENDER_MODE_3;
        else
            text.operation |= TEXT_DO_DRAW;
        /* Text is filled, so select the fill colour.
         */
        gs_swapcolors_quick(ctx->pgs);

        code = gs_text_begin(ctx->pgs, &text, ctx->memory, &penum);
        if (code >= 0) {
            ctx->current_text_enum = penum;
            code = gs_text_process(penum);
            gs_text_release(penum, "pdfi_Tj");
            ctx->current_text_enum = NULL;
        }
        gs_swapcolors_quick(ctx->pgs);
    } else {
        if (Trmode != 0 && Trmode != 3 && !ctx->preserve_tr_mode) {
            text.operation |= TEXT_DO_FALSE_CHARPATH;
            if (Trmode < 4) {
                pdfi_gsave(ctx);
                gs_newpath(ctx->pgs);
                gs_moveto(ctx->pgs, 0, 0);
            }
        } else {
            if (Trmode == 3)
                text.operation = TEXT_DO_NONE | TEXT_RENDER_MODE_3;
            else
                text.operation |= TEXT_DO_DRAW;
            /* Text is filled, so select the fill colour.
             */
            gs_swapcolors_quick(ctx->pgs);
        }

        code = gs_text_begin(ctx->pgs, &text, ctx->memory, &penum);
        if (code >= 0) {
            ctx->current_text_enum = penum;
            code = gs_text_process(penum);
            gs_text_release(penum, "pdfi_Tj");
            ctx->current_text_enum = NULL;
        }

        if (Trmode >= 4) {
            pdfi_gsave(ctx);
            gs_newpath(ctx->pgs);
            gs_moveto(ctx->pgs, 0, 0);
            code = gs_text_begin(ctx->pgs, &text, ctx->memory, &penum);
            if (code >= 0) {
                ctx->current_text_enum = penum;
                code = gs_text_process(penum);
                gs_text_release(penum, "pdfi_Tj");
                ctx->current_text_enum = NULL;
            }
        }

        switch(Trmode) {
            case 0:
                /* Text has been drawn, put the colours back again */
                gs_swapcolors_quick(ctx->pgs);
                break;
            case 1:
                gs_stroke(ctx->pgs);
                pdfi_grestore(ctx);
                break;
            case 2:
                gs_swapcolors_quick(ctx->pgs);
                pdfi_gsave(ctx);
                gs_fill(ctx->pgs);
                pdfi_grestore(ctx);
                gs_swapcolors_quick(ctx->pgs);
                gs_stroke(ctx->pgs);
                pdfi_grestore(ctx);
                break;
            case 3:
                /* Can't happen, we drop out earlier in this case */
                break;
            /* FIXME: Need to think about this. The text is supposed to accumulate
             * a path until we hit a ET operator, only then is it supposed to execute
             * an actual clip. Even though potentially we could be required to fill
             * or stroke some portions of it in the middle.
             */
            case 4:
                gs_swapcolors_quick(ctx->pgs);
                pdfi_gsave(ctx);
                gs_fill(ctx->pgs);
                pdfi_grestore(ctx);
                gs_swapcolors_quick(ctx->pgs);
                pdfi_grestore(ctx);
//                gs_clip(ctx->pgs);
                break;
            case 5:
                pdfi_gsave(ctx);
                gs_stroke(ctx->pgs);
                pdfi_grestore(ctx);
                pdfi_grestore(ctx);
//                gs_clip(ctx->pgs);
                break;
            case 6:
                gs_swapcolors_quick(ctx->pgs);
                pdfi_gsave(ctx);
                gs_fill(ctx->pgs);
                pdfi_grestore(ctx);
                gs_swapcolors_quick(ctx->pgs);
                pdfi_gsave(ctx);
                gs_stroke(ctx->pgs);
                pdfi_grestore(ctx);
                pdfi_grestore(ctx);
//                gs_clip(ctx->pgs);
                break;
            case 7:
                pdfi_grestore(ctx);
//                gs_clip(ctx->pgs);
                break;
            default:
                pdfi_grestore(ctx);
                break;
        }
    }

show_error:
    gs_free_object(ctx->memory, x_widths, "Free X widths array on error");
    gs_free_object(ctx->memory, y_widths, "Free Y widths array on error");
    return code;
}

int pdfi_Tj(pdf_context *ctx)
{
    int code;
    pdf_string *s = NULL;
    gs_matrix saved, Trm;
    gs_point initial_point, current_point, pt;
    double linewidth = ctx->pgs->line_params.half_width;

    if (pdfi_count_stack(ctx) < 1)
        return_error(gs_error_stackunderflow);

    s = (pdf_string *)ctx->stack_top[-1];
    if (s->type != PDF_STRING)
        return_error(gs_error_typecheck);

    /* Save the CTM for later restoration */
    saved = ctm_only(ctx->pgs);
    gs_currentpoint(ctx->pgs, &initial_point);

    Trm.xx = ctx->pgs->PDFfontsize * (ctx->pgs->texthscaling / 100);
    Trm.xy = 0;
    Trm.yx = 0;
    Trm.yy = ctx->pgs->PDFfontsize;
    Trm.tx = 0;
    Trm.ty = ctx->pgs->textrise;

    gs_matrix_multiply(&Trm, &ctx->pgs->textmatrix, &Trm);

    gs_distance_transform_inverse(linewidth, 0, &Trm, &pt);
    ctx->pgs->line_params.half_width = sqrt((pt.x * pt.x) + (pt.y + pt.y));

    gs_matrix_multiply(&Trm, &ctm_only(ctx->pgs), &Trm);
    gs_setmatrix(ctx->pgs, &Trm);
    code = gs_moveto(ctx->pgs, 0, 0);
    if (code < 0)
        goto Tj_error;

    code = pdfi_show(ctx, s);

    ctx->pgs->line_params.half_width = linewidth;
    /* Update the Text matrix with the current point, for the next operation
     */
    gs_currentpoint(ctx->pgs, &current_point);
    Trm.xx = ctx->pgs->PDFfontsize * (ctx->pgs->texthscaling / 100);
    Trm.xy = 0;
    Trm.yx = 0;
    Trm.yy = ctx->pgs->PDFfontsize;
    Trm.tx = 0;
    Trm.ty = 0;
    gs_matrix_multiply(&Trm, &ctx->pgs->textmatrix, &Trm);

    gs_distance_transform(current_point.x, current_point.y, &Trm, &pt);
    ctx->pgs->textmatrix.tx += pt.x;
    ctx->pgs->textmatrix.ty += pt.y;

Tj_error:
    /* Restore the CTM to the saved value */
    gs_setmatrix(ctx->pgs, &saved);
    /* And restore the currentpoint */
    gs_moveto(ctx->pgs, initial_point.x, initial_point.y);

    pdfi_pop(ctx, 1);
    return code;
}

int pdfi_TJ(pdf_context *ctx)
{
    int code = 0, i;
    pdf_array *a = NULL;
    pdf_obj *o;
    double dx = 0;
    gs_point pt;
    gs_matrix saved, Trm;
    gs_point initial_point, current_point;

    /* TODO: for ken -- check pdfi_oc_is_off() and skip the actual rendering...
     * (see gs code pdf_ops.ps/TJ OFFlevels for appropriate logic)
     */

    if (ctx->TextBlockDepth == 0) {
        ctx->pdf_warnings |= W_PDF_TEXTOPNOBT;
    }

    if (pdfi_count_stack(ctx) < 1)
        return_error(gs_error_stackunderflow);

    a = (pdf_array *)ctx->stack_top[-1];
    if (a->type != PDF_ARRAY) {
        pdfi_pop(ctx, 1);
        return gs_note_error(gs_error_typecheck);
    }

    /* Save the CTM for later restoration */
    saved = ctm_only(ctx->pgs);
    gs_currentpoint(ctx->pgs, &initial_point);

    /* Calculate the text rendering matrix, see section 1.7 PDF Reference
     * page 409, section 5.3.3 Text Space details.
     */
    Trm.xx = ctx->pgs->PDFfontsize * (ctx->pgs->texthscaling / 100);
    Trm.xy = 0;
    Trm.yx = 0;
    Trm.yy = ctx->pgs->PDFfontsize;
    Trm.tx = 0;
    Trm.ty = 0;

    gs_matrix_multiply(&Trm, &ctx->pgs->textmatrix, &Trm);
    gs_matrix_multiply(&Trm, &ctm_only(ctx->pgs), &Trm);

    gs_setmatrix(ctx->pgs, &Trm);
    code = gs_moveto(ctx->pgs, 0, 0);
    if (code < 0)
        goto TJ_error;

    for (i = 0; i < pdfi_array_size(a); i++) {
        code = pdfi_array_get(ctx, a, (uint64_t)i, &o);
        if (code < 0)
            goto TJ_error;

        if (o->type == PDF_INT) {
            dx = (double)((pdf_num *)o)->value.i / -1000;
            gs_distance_transform(dx, 0, &ctm_only(ctx->pgs), &pt);
            ctx->pgs->current_point.x += pt.x;
        } else {
            if (o->type == PDF_REAL) {
                dx = ((pdf_num *)o)->value.d / -1000;
                gs_distance_transform(dx, 0, &ctm_only(ctx->pgs), &pt);
                ctx->pgs->current_point.x += pt.x;
            } else {
                if (o->type == PDF_STRING)
                    code = pdfi_show(ctx, (pdf_string *)o);
                else
                    code = gs_note_error(gs_error_typecheck);
            }
        }
        pdfi_countdown(o);
        if (code < 0)
            goto TJ_error;
    }

    /* Update the Text matrix with the current point, for the next operation
     */
    gs_currentpoint(ctx->pgs, &current_point);
    Trm.xx = ctx->pgs->PDFfontsize * (ctx->pgs->texthscaling / 100);
    Trm.xy = 0;
    Trm.yx = 0;
    Trm.yy = ctx->pgs->PDFfontsize;
    Trm.tx = 0;
    Trm.ty = 0;
    gs_matrix_multiply(&Trm, &ctx->pgs->textmatrix, &Trm);

    gs_distance_transform(current_point.x, current_point.y, &Trm, &pt);
    ctx->pgs->textmatrix.tx += pt.x;
    ctx->pgs->textmatrix.ty += pt.y;

TJ_error:
    /* Restore the CTM to the saved value */
    gs_setmatrix(ctx->pgs, &saved);
    /* And restore the currentpoint */
    gs_moveto(ctx->pgs, initial_point.x, initial_point.y);

    pdfi_pop(ctx, 1);
    return code;
}

static int pdfi_set_TL(pdf_context *ctx, double TL)
{
    return gs_settextleading(ctx->pgs, TL);
}

int pdfi_TL(pdf_context *ctx)
{
    int code = 0;
    pdf_num *n = NULL;

    if (pdfi_count_stack(ctx) < 1) {
        pdfi_clearstack(ctx);
        return_error(gs_error_stackunderflow);
    }

    n = (pdf_num *)ctx->stack_top[-1];

    if (n->type == PDF_INT)
        code = pdfi_set_TL(ctx, (double)(n->value.i * -1));
    else {
        if (n->type == PDF_REAL)
            code = pdfi_set_TL(ctx, n->value.d * -1.0);
        else
            code = gs_note_error(gs_error_typecheck);
    }
    pdfi_pop(ctx, 1);
    return code;
}

int pdfi_Tm(pdf_context *ctx)
{
    int code = 0, i;
    float m[6];
    pdf_num *n = NULL;
    gs_matrix mat;
    gs_point pt;

    if (pdfi_count_stack(ctx) < 6) {
        pdfi_clearstack(ctx);
        return_error(gs_error_stackunderflow);
    }
    for (i = 1;i < 7;i++) {
        n = (pdf_num *)ctx->stack_top[-1 * i];
        if (n->type == PDF_INT)
            m[6 - i] = (float)n->value.i;
        else {
            if (n->type == PDF_REAL)
                m[6 - i] = (float)n->value.d;
            else {
                pdfi_pop(ctx, 6);
                return_error(gs_error_typecheck);
            }
        }
    }
    pdfi_pop(ctx, 6);

    if (ctx->TextBlockDepth == 0) {
        ctx->pdf_warnings |= W_PDF_TEXTOPNOBT;

        gs_make_identity(&mat);
        code = gs_settextmatrix(ctx->pgs, &mat);
        if (code < 0)
            return code;

        code = gs_settextlinematrix(ctx->pgs, &mat);
        if (code < 0)
            return code;
    }

    code = gs_distance_transform((double)1.0, (double)1.0, (const gs_matrix *)&m, &pt);
    if (code < 0)
        return code;

    if (pt.x == 0.0 || pt.y == 0.0) {
        ctx->pdf_warnings |= W_PDF_DEGENERATETM;
    } else {
        code = gs_settextmatrix(ctx->pgs, (gs_matrix *)&m);
        if (code < 0)
            return code;

        code = gs_settextlinematrix(ctx->pgs, (gs_matrix *)&m);
        if (code < 0)
            return code;
    }
    return code;
}

int pdfi_Tr(pdf_context *ctx)
{
    int code = 0, mode = 0;
    pdf_num *n = NULL;

    if (pdfi_count_stack(ctx) < 1) {
        pdfi_clearstack(ctx);
        return_error(gs_error_stackunderflow);
    }

    n = (pdf_num *)ctx->stack_top[-1];

    if (n->type == PDF_INT)
        mode = n->value.i;
    else {
        if (n->type == PDF_REAL)
            mode = (int)n->value.d;
        else {
            pdfi_pop(ctx, 1);
            return_error(gs_error_typecheck);
        }
    }
    pdfi_pop(ctx, 1);

    if (mode < 0 || mode > 7)
        code = gs_note_error(gs_error_rangecheck);
    else {
        /* Detect attempts to switch fomr a clipping mode to a non-clipping
         * mode, this is defined as invalid in the spec.
         */
        if (gs_currenttextrenderingmode(ctx->pgs) > 3 && mode < 4)
            ctx->pdf_warnings |= W_PDF_BADTRSWITCH;

        gs_settextrenderingmode(ctx->pgs, mode);
    }

    return code;
}

static int pdfi_set_Ts(pdf_context *ctx, double Ts)
{
    return gs_settextrise(ctx->pgs, Ts);
}

int pdfi_Ts(pdf_context *ctx)
{
    int code = 0;
    pdf_num *n = NULL;

    if (pdfi_count_stack(ctx) < 1) {
        pdfi_clearstack(ctx);
        return_error(gs_error_stackunderflow);
    }

    n = (pdf_num *)ctx->stack_top[-1];

    if (n->type == PDF_INT)
        code = pdfi_set_Ts(ctx, (double)n->value.i);
    else {
        if (n->type == PDF_REAL)
            code = pdfi_set_Ts(ctx, n->value.d);
        else
            code = gs_note_error(gs_error_typecheck);
    }
    pdfi_pop(ctx, 1);
    return code;
}

static int pdfi_set_Tw(pdf_context *ctx, double Tw)
{
    return gs_setwordspacing(ctx->pgs, Tw);
}

int pdfi_Tw(pdf_context *ctx)
{
    int code = 0;
    pdf_num *n = NULL;

    if (pdfi_count_stack(ctx) < 1) {
        pdfi_clearstack(ctx);
        return_error(gs_error_stackunderflow);
    }

    n = (pdf_num *)ctx->stack_top[-1];

    if (n->type == PDF_INT)
        code = pdfi_set_Tw(ctx, (double)n->value.i);
    else {
        if (n->type == PDF_REAL)
            code = pdfi_set_Tw(ctx, n->value.d);
        else
            code = gs_note_error(gs_error_typecheck);
    }
    pdfi_pop(ctx, 1);
    return code;
}

int pdfi_Tz(pdf_context *ctx)
{
    int code = 0;
    pdf_num *n = NULL;

    if (pdfi_count_stack(ctx) < 1) {
        pdfi_clearstack(ctx);
        return_error(gs_error_stackunderflow);
    }

    n = (pdf_num *)ctx->stack_top[-1];

    if (n->type == PDF_INT)
        code = gs_settexthscaling(ctx->pgs, (double)n->value.i);
    else {
        if (n->type == PDF_REAL)
            code = gs_settexthscaling(ctx->pgs, n->value.d);
        else
            code = gs_note_error(gs_error_typecheck);
    }
    pdfi_pop(ctx, 1);
    return code;
}

int pdfi_singlequote(pdf_context *ctx)
{
    int code;

    if (ctx->TextBlockDepth == 0) {
        ctx->pdf_warnings |= W_PDF_TEXTOPNOBT;
    }

    if (pdfi_count_stack(ctx) < 1) {
        pdfi_clearstack(ctx);
        return_error(gs_error_stackunderflow);
    }

    code = pdfi_T_star(ctx);
    if (code < 0)
        return code;

    return pdfi_Tj(ctx);
}

int pdfi_doublequote(pdf_context *ctx)
{
    int code;
    pdf_string *s;
    pdf_num *Tw, *Tc;

    if (ctx->TextBlockDepth == 0) {
        ctx->pdf_warnings |= W_PDF_TEXTOPNOBT;
    }

    if (pdfi_count_stack(ctx) < 3) {
        pdfi_clearstack(ctx);
        return_error(gs_error_stackunderflow);
    }

    s = (pdf_string *)ctx->stack_top[-1];
    Tc = (pdf_num *)ctx->stack_top[-2];
    Tw = (pdf_num *)ctx->stack_top[-3];
    if (s->type != PDF_STRING || (Tc->type != PDF_INT && Tc->type != PDF_REAL) ||
        (Tw->type != PDF_INT && Tw->type != PDF_REAL)) {
        pdfi_pop(ctx, 3);
        return gs_note_error(gs_error_typecheck);
    }

    if (Tc->type == PDF_INT)
        code = pdfi_set_Tc(ctx, (double)Tc->value.i);
    else
        code = pdfi_set_Tc(ctx, Tc->value.d);
    if (code < 0) {
        pdfi_pop(ctx, 3);
        return code;
    }

    if (Tw->type == PDF_INT)
        code = pdfi_set_Tw(ctx, (double)Tw->value.i);
    else
        code = pdfi_set_Tw(ctx, Tw->value.d);
    if (code < 0) {
        pdfi_pop(ctx, 3);
        return code;
    }

    code = pdfi_T_star(ctx);
    if (code < 0)
        return code;

    code = pdfi_Tj(ctx);
    pdfi_pop(ctx, 3);
    return code;
}
