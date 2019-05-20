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
#include "pdf_font.h"
#include "pdf_stack.h"
#include "pdf_font0.h"
#include "pdf_font1.h"
#include "pdf_font1C.h"
#include "pdf_font3.h"
#include "pdf_fontTT.h"


int pdfi_d0(pdf_context *ctx)
{
    if (ctx->stack_top - ctx->stack_bot >= 2)
        pdfi_pop(ctx, 2);
    else
        pdfi_clearstack(ctx);
    return 0;
}

int pdfi_d1(pdf_context *ctx)
{
    if (ctx->stack_top - ctx->stack_bot >= 6)
        pdfi_pop(ctx, 6);
    else
        pdfi_clearstack(ctx);
    return 0;
}

int pdfi_Tf(pdf_context *ctx, pdf_dict *stream_dict, pdf_dict *page_dict)
{
    float point_size = 0;
    pdf_obj *o = NULL;
    int code = 0;
    pdf_dict *font_dict = NULL;

    if (ctx->stack_top - ctx->stack_bot >= 2) {
        o = ctx->stack_top[-1];
        if (o->type == PDF_INT)
            point_size = (float)((pdf_num *)o)->value.i;
        else
            if (o->type == PDF_REAL)
                point_size = ((pdf_num *)o)->value.d;
            else {
                code = gs_note_error(gs_error_typecheck);
                goto Tf_error;
            }

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
            code = gs_note_error(gs_error_typecheck);
            goto Tf_error;
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
            code = pdfi_read_type0_font(ctx, (pdf_dict *)font_dict, stream_dict, page_dict);
            if (code < 0)
                goto Tf_error_o;
        } else {
            if (pdfi_name_is((const pdf_name *)o, "Type1")) {
                code = pdfi_read_type1_font(ctx, (pdf_dict *)font_dict, stream_dict, page_dict);
                if (code < 0)
                    goto Tf_error_o;
            } else {
                if (pdfi_name_is((const pdf_name *)o, "Type1C")) {
                    code = pdfi_read_type1C_font(ctx, (pdf_dict *)font_dict, stream_dict, page_dict);
                    if (code < 0)
                        goto Tf_error_o;
                } else {
                    if (pdfi_name_is((const pdf_name *)o, "Type3")) {
                        code = pdfi_read_type3_font(ctx, (pdf_dict *)font_dict, stream_dict, page_dict);
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

        pdfi_pop(ctx, 2);
    }
    else {
        code = gs_note_error(gs_error_typecheck);
        goto Tf_error;
    }
    return 0;

Tf_error_o:
    pdfi_countdown(o);
Tf_error:
    pdfi_countdown(font_dict);
    pdfi_pop(ctx, 2);
    return code;
}
