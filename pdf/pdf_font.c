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

/* Font operations for the PDF interpreter */

#include "pdf_int.h"
#include "pdf_dict.h"
#include "pdf_loop_detect.h"
#include "pdf_font.h"
#include "pdf_stack.h"
#include "pdf_font_types.h"
#include "pdf_font0.h"
#include "pdf_font1.h"
#include "pdf_font1C.h"
#include "pdf_font3.h"
#include "pdf_fontTT.h"


int pdfi_d0(pdf_context *ctx)
{
    int code = 0;
    double width[2];

    if (pdfi_count_stack(ctx) < 2) {
        code = gs_note_error(gs_error_stackunderflow);
        goto d0_error;
    }

    if (ctx->stack_top[-1]->type != PDF_INT && ctx->stack_top[-1]->type != PDF_REAL) {
        code = gs_note_error(gs_error_typecheck);
        goto d0_error;
    }
    if (ctx->stack_top[-2]->type != PDF_INT && ctx->stack_top[-2]->type != PDF_REAL) {
        code = gs_note_error(gs_error_typecheck);
        goto d0_error;
    }
    if(ctx->current_text_enum == NULL) {
        code = gs_note_error(gs_error_undefined);
        goto d0_error;
    }

    if (ctx->stack_top[-1]->type == PDF_INT)
        width[0] = (double)((pdf_num *)ctx->stack_top[-1])->value.i;
    else
        width[0] = ((pdf_num *)ctx->stack_top[-1])->value.d;
    if (ctx->stack_top[-2]->type == PDF_INT)
        width[1] = (double)((pdf_num *)ctx->stack_top[-1])->value.i;
    else
        width[1] = ((pdf_num *)ctx->stack_top[-1])->value.d;

    code = gs_text_setcharwidth(ctx->current_text_enum, width);
    if (code < 0)
        goto d0_error;
    pdfi_pop(ctx, 2);
    return 0;

d0_error:
    pdfi_clearstack(ctx);
    return code;
}

int pdfi_d1(pdf_context *ctx)
{
    int code = 0, i;
    double wbox[6];

    if (pdfi_count_stack(ctx) < 2) {
        code = gs_note_error(gs_error_stackunderflow);
        goto d1_error;
    }

    for (i=-6;i < 0;i++) {
        if (ctx->stack_top[i]->type != PDF_INT && ctx->stack_top[i]->type != PDF_REAL) {
            code = gs_note_error(gs_error_typecheck);
            goto d1_error;
        }
        if (ctx->stack_top[i]->type == PDF_INT)
            wbox[i + 6] = (double)((pdf_num *)ctx->stack_top[i])->value.i;
        else
            wbox[i + 6] = ((pdf_num *)ctx->stack_top[i])->value.d;
    }
    code = gs_text_setcachedevice(ctx->current_text_enum, wbox);
    if (code < 0)
        goto d1_error;
    pdfi_pop(ctx, 6);
    return 0;

d1_error:
    pdfi_clearstack(ctx);
    return code;
}

int pdfi_Tf(pdf_context *ctx, pdf_dict *stream_dict, pdf_dict *page_dict)
{
    double point_size = 0;
    pdf_obj *o = NULL;
    int code = 0;
    pdf_dict *font_dict = NULL;

    if (pdfi_count_stack(ctx) >= 2) {
        o = ctx->stack_top[-1];
        if (o->type == PDF_INT)
            point_size = (double)((pdf_num *)o)->value.i;
        else {
            if (o->type == PDF_REAL)
                point_size = ((pdf_num *)o)->value.d;
            else {
                code = gs_note_error(gs_error_typecheck);
                goto Tf_error;
            }
        }
        code = gs_setPDFfontsize(ctx->pgs, point_size);
        if (code < 0)
            goto Tf_error;

        o = ctx->stack_top[-2];
        if (o->type != PDF_NAME) {
            code = gs_note_error(gs_error_typecheck);
            goto Tf_error;
        }

        code = pdfi_loop_detector_mark(ctx);
        if (code < 0)
            goto Tf_error;

        code = pdfi_find_resource(ctx, (unsigned char *)"Font", (pdf_name *)o, stream_dict, page_dict, (pdf_obj **)&font_dict);
        if (code < 0)
            goto Tf_error;

        (void)pdfi_loop_detector_cleartomark(ctx);

        o = NULL;

        if (font_dict->type != PDF_DICT) {
            pdf_font *font = (pdf_font *)font_dict;

            if (font_dict->type != PDF_FONT) {
                code = gs_note_error(gs_error_typecheck);
                goto Tf_error;
            }
            /* Don't swap fonts if this is already the current font */
            if (font->pfont != (gs_font_base *)ctx->pgs->font)
                code = gs_setfont(ctx->pgs, (gs_font *)font->pfont);
            else
                pdfi_countdown(font_dict);
            pdfi_pop(ctx, 2);
            return code;
        }

        code = pdfi_dict_knownget_type(ctx, font_dict, "Type", PDF_NAME, &o);
        if (code < 0)
            goto Tf_error_o;
        if (code == 0) {
            code = gs_note_error(gs_error_undefined);
            goto Tf_error_o;
        }
        if (!pdfi_name_is((const pdf_name *)o, "Font")){
            code = gs_note_error(gs_error_typecheck);
            goto Tf_error_o;
        }
        pdfi_countdown(o);
        o = NULL;

        code = pdfi_dict_knownget_type(ctx, font_dict, "Subtype", PDF_NAME, &o);
        if (code < 0)
            goto Tf_error_o;

        if (pdfi_name_is((const pdf_name *)o, "Type0")) {
            code = pdfi_read_type0_font(ctx, (pdf_dict *)font_dict, stream_dict, page_dict, point_size);
            if (code < 0)
                goto Tf_error_o;
        } else {
            if (pdfi_name_is((const pdf_name *)o, "Type1")) {
                code = pdfi_read_type1_font(ctx, (pdf_dict *)font_dict, stream_dict, page_dict, point_size);
                if (code < 0)
                    goto Tf_error_o;
            } else {
                if (pdfi_name_is((const pdf_name *)o, "Type1C")) {
                    code = pdfi_read_type1C_font(ctx, (pdf_dict *)font_dict, stream_dict, page_dict, point_size);
                    if (code < 0)
                        goto Tf_error_o;
                } else {
                    if (pdfi_name_is((const pdf_name *)o, "Type3")) {
                        code = pdfi_read_type3_font(ctx, (pdf_dict *)font_dict, stream_dict, page_dict, point_size);
                        if (code < 0)
                            goto Tf_error_o;
                    } else {
                        if (pdfi_name_is((const pdf_name *)o, "TrueType")) {
                        } else {
                            code = gs_note_error(gs_error_undefined);
                                goto Tf_error_o;
                        }
                    }
                }
            }
        }
        pdfi_countdown(o);

        pdfi_countdown(font_dict);

        pdfi_pop(ctx, 2);
    }
    else {
        pdfi_clearstack(ctx);
        code = gs_note_error(gs_error_typecheck);
        goto Tf_error;
    }
    return 0;

Tf_error_o:
    pdfi_countdown(o);
Tf_error:
    pdfi_countdown(font_dict);
    pdfi_clearstack(ctx);
    return code;
}

int pdfi_free_font(pdf_obj *font)
{
    pdf_font *f = (pdf_font *)font;

    switch (f->pdfi_font_type) {
        case e_pdf_font_type0:
        case e_pdf_font_type1:
        case e_pdf_font_cff:
        case e_pdf_font_type3:
            return pdfi_free_font_type3((pdf_obj *)font);
            break;
        case e_pdf_cidfont_type0:
        case e_pdf_cidfont_type1:
        case e_pdf_cidfont_type2:
        case e_pdf_cidfont_type4:
        case e_pdf_font_truetype:
        default:
            return gs_note_error(gs_error_typecheck);
            break;
    }
    return 0;
}
