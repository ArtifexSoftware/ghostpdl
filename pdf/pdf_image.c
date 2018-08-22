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
#include "pdf_file.h"
#include "pdf_dict.h"
#include "pdf_loop_detect.h"
#include "pdf_colour.h"
#include "stream.h"     /* for stell() */

extern int pdfi_dict_from_stack(pdf_context *ctx);

int pdfi_BI(pdf_context *ctx)
{
    return pdfi_mark_stack(ctx, PDF_DICT_MARK);
}

typedef struct {
    bool ImageMask;
    int64_t Height;
    int64_t Width;
    int64_t BPC;
    pdf_obj *Mask;
    pdf_obj *ColorSpace;
} pdfi_image_info_t;

static void
pdfi_free_image_info_components(pdfi_image_info_t *info)
{
    if (info->Mask)
        pdfi_countdown(info->Mask);
    if (info->ColorSpace)
        pdfi_countdown(info->ColorSpace);
}

static int
pdfi_get_image_info(pdf_context *ctx, pdf_dict *image_dict, pdfi_image_info_t *info)
{
    int code;
    pdf_obj *ImageMask;
    
    memset(info, 0, sizeof(*info));

    /* Required */
    code = pdfi_dict_get_int(ctx, image_dict, "Height", &info->Height);
    if (code == gs_error_undefined)
        code = pdfi_dict_get_int(ctx, image_dict, "H", &info->Height);
    if (code < 0)
        goto errorExit;

    /* Required */
    code = pdfi_dict_get_int(ctx, image_dict, "Width", &info->Width);
    if (code == gs_error_undefined)
        code = pdfi_dict_get_int(ctx, image_dict, "W", &info->Width);
    if (code < 0)
        goto errorExit;

    /* Required */
    code = pdfi_dict_get_int(ctx, image_dict, "BitsPerComponent", &info->BPC);
    if (code == gs_error_undefined)
        code = pdfi_dict_get_int(ctx, image_dict, "BPC", &info->BPC);
    if (code < 0)
        goto errorExit;

    /* Optional, default false */
    code = pdfi_dict_get_type(ctx, image_dict, "ImageMask", PDF_BOOL, &ImageMask);
    if (code == gs_error_undefined)
        code = pdfi_dict_get_type(ctx, image_dict, "IM", PDF_BOOL, &ImageMask);
    if (code == 0) {
        info->ImageMask = ((pdf_bool *)ImageMask)->value;
        pdfi_countdown(ImageMask);
    } else {
        if (code != gs_error_undefined)
            goto errorExit;
        info->ImageMask = false;
    }

    /* Optional */
    code = pdfi_dict_get(ctx, image_dict, "Mask", &info->Mask);
    if (code < 0) {
        if (code != gs_error_undefined)
            goto errorExit;
    }
    
    /* Optional */
    code = pdfi_dict_get(ctx, image_dict, "ColorSpace", &info->ColorSpace);
    if (code == gs_error_undefined)
        code = pdfi_dict_get(ctx, image_dict, "CS", &info->ColorSpace);
    if (code < 0) {
        if (code != gs_error_undefined)
            goto errorExit;
    }
    return 0;

 errorExit:
    pdfi_free_image_info_components(info);
    return code;
}

static int
pdfi_do_image(pdf_context *ctx, pdf_dict *page_dict, pdf_dict *stream_dict, pdf_dict *image_dict,
              pdf_stream *source, bool inline_image)
{
    pdf_stream *new_stream;
    int64_t Height, Width, BPC;
    bool return_error = false;
    int i, code, comps = 0, byteswide, total;
    byte c;
    gs_color_space  *pcs = NULL;
    gs_image_enum *penum;
    gs_image_t gsimage;
    unsigned char Buffer[1024];
    uint64_t toread;
    pdfi_image_info_t image_info;
    
    code = pdfi_get_image_info(ctx, image_dict, &image_info);
    if (code < 0)
        goto cleanupExit;

    Width = image_info.Width;
    Height = image_info.Height;
    BPC = image_info.BPC;

    if (image_info.ImageMask) {
        comps = 1;
    }

    /* TODO: Need to do something with Mask, get it into a MaskColor in the image struct
    */

    if (image_info.ColorSpace == NULL) {
        /* Do something sensible if no ColorSpace provided */
        if (comps == 0) {
            gx_device *dev = gs_currentdevice_inline(ctx->pgs);
            comps = dev->color_info.num_components;
        } else {
            pcs = gs_currentcolorspace(ctx->pgs);
        }
    } else {
        code = pdfi_create_colorspace(ctx, image_info.ColorSpace, page_dict, stream_dict, &pcs);
        //        pdfi_countdown(info.ColorSpace);
        if (code < 0)
            goto cleanupExit;
        comps = gs_color_space_num_components(pcs);
    }

    code = pdfi_filter(ctx, image_dict, source, &new_stream, inline_image);
    if (code < 0)
        goto cleanupExit;

    if (pcs == NULL) {
        byteswide = ((Width * comps * BPC) + 7) / 8;
        total = byteswide * Height;

        for (i=0;i < total;i++) {
            code = pdfi_read_bytes(ctx, &c, 1, 1, new_stream);
            if (code < 0) {
                pdfi_close_file(ctx, new_stream);
                goto cleanupExit;
            }
        }
    } else {
        memset(&gsimage, 0, sizeof(gsimage));
        gs_image_t_init(&gsimage, pcs);
        gsimage.ColorSpace = pcs;
        gsimage.BitsPerComponent = BPC;
        gsimage.Width = Width;
        gsimage.Height = Height;

        gsimage.ImageMatrix.xx = (float)Width;
        gsimage.ImageMatrix.yy = (float)(Height * -1);
        gsimage.ImageMatrix.ty = (float)Height;

        gsimage.Interpolate = 1;

        penum = gs_image_enum_alloc(ctx->memory, "xps_parse_image_brush (gs_image_enum_alloc)");
        if (!penum) {
            code = gs_error_VMerror;
            return_error = true;
            goto cleanupExit;
        }

        if ((code = gs_image_init(penum, &gsimage, false, false, ctx->pgs)) < 0)
            goto cleanupExit;

        toread = (((Width * comps * BPC) + 7) / 8) * Height;

        do {
            uint count, used, index;

            if (toread > 1024) {
                code = pdfi_read_bytes(ctx, (byte *)Buffer, 1, 1024, new_stream);
                count = 1024;
            } else {
                code = pdfi_read_bytes(ctx, (byte *)Buffer, 1, toread, new_stream);
                count = toread;
            }
            toread -= count;
            index = 0;
            do {
                if ((code = gs_image_next(penum, (byte *)&Buffer[index], count, &used)) < 0)
                    goto cleanupExit;
                count -= used;
                index += used;
            } while (count);
        } while(toread && new_stream->eof == false);

        gs_image_cleanup_and_free_enum(penum, ctx->pgs);
        pdfi_close_file(ctx, new_stream);
    }
    code = 0;
    
 cleanupExit:
    pdfi_free_image_info_components(&image_info);
    if (return_error)
        return_error(code);
    else
        return code;
}

int pdfi_ID(pdf_context *ctx, pdf_dict *stream_dict, pdf_dict *page_dict, pdf_stream *source)
{
    pdf_dict *d = NULL;
    int code;

    code = pdfi_dict_from_stack(ctx);
    if (code < 0)
        return code;

    d = (pdf_dict *)ctx->stack_top[-1];
    pdfi_countup(d);
    pdfi_pop(ctx, 1);

    code = pdfi_do_image(ctx, stream_dict, page_dict, d, source, true);
    pdfi_countdown(d);
    if (code < 0 && ctx->pdfstoponerror)
        return code;
    return 0;
}

int pdfi_EI(pdf_context *ctx)
{
    pdfi_clearstack(ctx);
    return 0;
}

int pdfi_Do(pdf_context *ctx, pdf_dict *stream_dict, pdf_dict *page_dict)
{
    int code = 0;
    pdf_name *n = NULL;
    pdf_obj *o;

    if (ctx->loop_detection == NULL) {
        pdfi_init_loop_detector(ctx);
        pdfi_loop_detector_mark(ctx);
    } else {
        pdfi_loop_detector_mark(ctx);
    }

    if (ctx->stack_top - ctx->stack_bot < 1) {
        pdfi_loop_detector_cleartomark(ctx);
        if (ctx->pdfstoponerror) {
            return_error(gs_error_stackunderflow);
        }
        return 0;
    }
    n = (pdf_name *)ctx->stack_top[-1];
    if (n->type != PDF_NAME) {
        pdfi_pop(ctx, 1);
        pdfi_loop_detector_cleartomark(ctx);
        if (ctx->pdfstoponerror)
            return_error(gs_error_typecheck);
        return 0;
    }

    code = pdfi_find_resource(ctx, (unsigned char *)"XObject", n, stream_dict, page_dict, &o);
    if (code < 0) {
        pdfi_pop(ctx, 1);
        pdfi_loop_detector_cleartomark(ctx);
        if (ctx->pdfstoponerror)
            return code;
        return 0;
    }
    if (o->type != PDF_DICT) {
        pdfi_loop_detector_cleartomark(ctx);
        pdfi_countdown(o);
        if (ctx->pdfstoponerror)
            return_error(gs_error_typecheck);
        return 0;
    }
    code = pdfi_dict_get(ctx, (pdf_dict *)o, "Subtype", (pdf_obj **)&n);
    if (code == 0) {
        pdf_dict *d = (pdf_dict *)o;
        if (n->length == 5 && memcmp(n->data, "Image", 5) == 0) {
            gs_offset_t savedoffset = pdfi_tell(ctx->main_stream);

            pdfi_seek(ctx, ctx->main_stream, d->stream_offset, SEEK_SET);
            code = pdfi_do_image(ctx, page_dict, stream_dict, d, ctx->main_stream, false);
            pdfi_seek(ctx, ctx->main_stream, savedoffset, SEEK_SET);
        } else {
            if (n->length == 4 && memcmp(n->data, "Form", 4) == 0) {
                gs_offset_t savedoffset = pdfi_tell(ctx->main_stream);

                code = pdfi_interpret_content_stream(ctx, d, page_dict);
                pdfi_seek(ctx, ctx->main_stream, savedoffset, SEEK_SET);
            } else {
                code = gs_error_typecheck;
            }
        }
        pdfi_countdown(n);
    }
    pdfi_countdown(o);
    pdfi_pop(ctx, 1);
    pdfi_loop_detector_cleartomark(ctx);
    if (code < 0 && ctx->pdfstoponerror)
        return code;
    return 0;
}
