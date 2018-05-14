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

/* Image operations for the PDF interpreter */

#include "pdf_int.h"
#include "pdf_stack.h"
#include "pdf_image.h"

extern int pdf_dict_from_stack(pdf_context *ctx);

int pdf_BI(pdf_context *ctx)
{
    return pdf_mark_stack(ctx, PDF_DICT_MARK);
}

int pdf_ID(pdf_context *ctx, pdf_stream *source)
{
    pdf_name *n = NULL;
    pdf_dict *d = NULL;
    pdf_stream *new_stream;
    int64_t Height, Width, BPC;
    int i, code, comps = 0, byteswide, total;
    byte c;
    pdf_obj *Mask;

    code = pdf_dict_from_stack(ctx);
    if (code < 0)
        return code;

    d = (pdf_dict *)ctx->stack_top[-1];
    pdf_countup(d);
    pdf_pop(ctx, 1);

    /* Need to process it here. Punt for now */
    code = pdf_dict_get_int(ctx, d, "Height", &Height);
    if (code == gs_error_undefined)
        code = pdf_dict_get_int(ctx, d, "H", &Height);
    if (code < 0) {
        pdf_countdown(d);
        return code;
    }

    code = pdf_dict_get_int(ctx, d, "Width", &Width);
    if (code == gs_error_undefined)
        code = pdf_dict_get_int(ctx, d, "W", &Width);
    if (code < 0) {
        pdf_countdown(d);
        return code;
    }

    code = pdf_dict_get_int(ctx, d, "BitsPerComponent", &BPC);
    if (code == gs_error_undefined)
        code = pdf_dict_get_int(ctx, d, "BPC", &BPC);
    if (code < 0) {
        pdf_countdown(d);
        return code;
    }

    code = pdf_dict_get_type(ctx, d, "ImageMask", PDF_BOOL, &Mask);
    if (code == gs_error_undefined)
        code = pdf_dict_get_type(ctx, d, "IM", PDF_BOOL, &Mask);
    if (code == 0) {
        if (((pdf_bool *)Mask)->value == true)
            comps = 1;
        pdf_countdown(Mask);
    } else {
        if (code != gs_error_undefined) {
            pdf_countdown(d);
            return code;
        }
    }

    code = pdf_dict_get_type(ctx, d, "ColorSpace", PDF_NAME, (pdf_obj **)&n);
    if (code == gs_error_undefined)
        code = pdf_dict_get_type(ctx, d, "CS", PDF_NAME, (pdf_obj **)&n);
    if (code < 0) {
        if (comps == 0) {
            gx_device *dev = gs_currentdevice_inline(ctx->pgs);
            comps = dev->color_info.num_components;
        }
    } else {
        switch(n->length){
            case 1:
                if (memcmp(n->data, "G", 1) == 0){
                    comps = 1;
                } else {
                    if (memcmp(n->data, "I", 1) == 0){
                        comps = 1;
                    } else {
                        pdf_countdown(n);
                        pdf_countdown(d);
                        return_error(gs_error_syntaxerror);
                    }
                }
                break;
            case 3:
                if (memcmp(n->data, "RGB", 3) == 0){
                    comps = 3;
                } else {
                    pdf_countdown(n);
                    pdf_countdown(d);
                    return_error(gs_error_syntaxerror);
                }
                break;
            case 4:
                if (memcmp(n->data, "CMYK", 4) == 0){
                    comps = 4;
                } else {
                    pdf_countdown(n);
                    pdf_countdown(d);
                    return_error(gs_error_syntaxerror);
                }
                break;
            case 7:
                if (memcmp(n->data, "Indexed", 7) == 0){
                    comps = 1;
                } else {
                    pdf_countdown(n);
                    pdf_countdown(d);
                    return_error(gs_error_syntaxerror);
                }
                break;
            case 9:
                if (memcmp(n->data, "DeviceRGB", 9) == 0){
                    comps = 3;
                } else {
                    pdf_countdown(n);
                    pdf_countdown(d);
                    return_error(gs_error_syntaxerror);
                }
                break;
            case 10:
                if (memcmp(n->data, "DeviceGray", 10) == 0){
                    comps = 1;
                } else {
                    if (memcmp(n->data, "DeviceCMYK", 10) == 0){
                        comps = 4;
                    } else {
                        pdf_countdown(n);
                        pdf_countdown(d);
                        return_error(gs_error_syntaxerror);
                    }
                }
                break;
            default:
                pdf_countdown(n);
                pdf_countdown(d);
                return_error(gs_error_syntaxerror);
        }
        pdf_countdown(n);
    }

    code = pdf_filter(ctx, d, source, &new_stream, true);
    if (code < 0) {
        pdf_countdown(d);
        return code;
    }

    pdf_countdown(d);

    byteswide = ((Width * comps * BPC) + 7) / 8;
    total = byteswide * Height;

    for (i=0;i < total;i++) {
        code = pdf_read_bytes(ctx, &c, 1, 1, new_stream);
        if (code < 0) {
            pdf_close_file(new_stream);
            return code;
        }
    }
    pdf_close_file(ctx, new_stream);
    return 0;
}

int pdf_EI(pdf_context *ctx)
{
    pdf_clearstack(ctx);
    return 0;
}

int pdf_Do(pdf_context *ctx)
{
    if (ctx->stack_top - ctx->stack_bot >= 1)
        pdf_pop(ctx, 1);
    return 0;
}
