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
#include "pdf_array.h"
#include "pdf_loop_detect.h"
#include "pdf_colour.h"
#include "stream.h"     /* for stell() */

#include "gsiparm4.h"
#include "gsiparm3.h"

extern int pdfi_dict_from_stack(pdf_context *ctx);

int pdfi_BI(pdf_context *ctx)
{
    return pdfi_mark_stack(ctx, PDF_DICT_MARK);
}

typedef struct {
    /* Type and SubType were already checked by caller */
    /* OPI, Metadata -- do we care? */
    bool ImageMask;
    bool Interpolate;
    int64_t Height;
    int64_t Width;
    int64_t BPC;
    int64_t StructParent;
    pdf_obj *Mask;
    pdf_obj *SMask;
    pdf_obj *ColorSpace;
    pdf_obj *Intent;
    pdf_obj *Alternates;
    pdf_obj *Name; /* obsolete, do we still support? */
    /* ?? Should Decode be handled by pdfi_filter()? What is it? */
    pdf_obj *Decode;
    /* Filter and DecodeParms handled by pdfi_filter() (can probably remove, but I like the info while debugging) */
    pdf_obj *Filter;
    pdf_obj *DecodeParms;
} pdfi_image_info_t;

static void
pdfi_free_image_info_components(pdfi_image_info_t *info)
{
    if (info->Mask)
        pdfi_countdown(info->Mask);
    if (info->SMask)
        pdfi_countdown(info->SMask);
    if (info->ColorSpace)
        pdfi_countdown(info->ColorSpace);
    if (info->Intent)
        pdfi_countdown(info->Intent);
    if (info->Alternates)
        pdfi_countdown(info->Alternates);
    if (info->Name)
        pdfi_countdown(info->Name);
    if (info->Decode)
        pdfi_countdown(info->Decode);
    if (info->Filter)
        pdfi_countdown(info->Filter);
    if (info->DecodeParms)
        pdfi_countdown(info->DecodeParms);
}


/* Get image info out of dict into more convenient form, enforcing some requirements from the spec */
static int
pdfi_get_image_info(pdf_context *ctx, pdf_dict *image_dict, pdfi_image_info_t *info)
{
    int code;
    
    memset(info, 0, sizeof(*info));

    /* Required */
    code = pdfi_dict_get_int2(ctx, image_dict, "Height", "H", &info->Height);
    if (code < 0)
        goto errorExit;

    /* Required */
    code = pdfi_dict_get_int2(ctx, image_dict, "Width", "W", &info->Width);
    if (code < 0)
        goto errorExit;

    /* Optional, default false */
    code = pdfi_dict_get_bool2(ctx, image_dict, "ImageMask", "IM", &info->ImageMask);
    if (code != 0) {
        if (code != gs_error_undefined)
            goto errorExit;
        info->ImageMask = false;
    }

    /* Optional, default false */
    code = pdfi_dict_get_bool2(ctx, image_dict, "Interpolate", "I", &info->Interpolate);
    if (code != 0) {
        if (code != gs_error_undefined)
            goto errorExit;
        info->Interpolate = false;
    }

    /* Optional (Required, unless ImageMask is true)  */
    code = pdfi_dict_get_int2(ctx, image_dict, "BitsPerComponent", "BPC", &info->BPC);
    if (code < 0) {
        if (code != gs_error_undefined) {
            goto errorExit;
        }
        if (info->ImageMask) {
            info->BPC = 1; /* If ImageMask was true, and not specified, force to 1 */
        } else {
            goto errorExit; /* Required if !ImageMask, so flag error */
        }
    }
    /* TODO: spec says if ImageMask is specified, and BPC is specified, then BPC must be 1 
       Should we flag an error if this is violated?
     */

    /* Optional (apparently there is no "M" abbreviation for "Mask"? */
    code = pdfi_dict_get(ctx, image_dict, "Mask", &info->Mask);
    if (code < 0) {
        if (code != gs_error_undefined)
            goto errorExit;
    }
    
    /* Optional (apparently there is no abbreviation for "SMask"? */
    code = pdfi_dict_get(ctx, image_dict, "SMask", &info->SMask);
    if (code < 0) {
        if (code != gs_error_undefined)
            goto errorExit;
    }
    
    /* Optional (Required except for ImageMask, not allowed for ImageMask)*/
    /* TODO: Should we enforce this required/not allowed thing? */
    code = pdfi_dict_get2(ctx, image_dict, "ColorSpace", "CS", &info->ColorSpace);
    if (code < 0) {
        if (code != gs_error_undefined)
            goto errorExit;
    }

    /* Optional (default is to use from graphics state) */
    /* (no abbreviation for inline) */
    code = pdfi_dict_get(ctx, image_dict, "Intent", &info->Intent);
    if (code < 0) {
        if (code != gs_error_undefined)
            goto errorExit;
    }

    /* Optional (array of alternate image dicts, can't be nested) */
    code = pdfi_dict_get(ctx, image_dict, "Alternates", &info->Alternates);
    if (code < 0) {
        if (code != gs_error_undefined)
            goto errorExit;
    }

    /* Optional (required in PDF1.0, obsolete, do we support?) */
    code = pdfi_dict_get(ctx, image_dict, "Name", &info->Name);
    if (code < 0) {
        if (code != gs_error_undefined)
            goto errorExit;
    }

    /* Required "if image is structural content item" */
    /* TODO: Figure out what to do here */
    code = pdfi_dict_get_int(ctx, image_dict, "StructParent", &info->StructParent);
    if (code < 0) {
        if (code != gs_error_undefined)
            goto errorExit;
    }

    /* Optional */
    code = pdfi_dict_get2(ctx, image_dict, "Filter", "F", &info->Filter);
    if (code < 0) {
        if (code != gs_error_undefined)
            goto errorExit;
    }

    /* Optional */
    code = pdfi_dict_get2(ctx, image_dict, "Decode", "D", &info->Decode);
    if (code < 0) {
        if (code != gs_error_undefined)
            goto errorExit;
    }
    
    /* Optional */
    code = pdfi_dict_get2(ctx, image_dict, "DecodeParms", "DP", &info->DecodeParms);
    if (code < 0) {
        if (code != gs_error_undefined)
            goto errorExit;
    }

    return 0;

 errorExit:
    pdfi_free_image_info_components(info);
    return code;
}

/* Render a PDF image
 * pim can be type1 (or imagemask), type3, type4
 */
static int
pdfi_render_image(pdf_context *ctx, gs_pixel_image_t *pim, pdf_stream *new_stream, int comps, bool ImageMask)
{
    int code;
    gs_image_enum *penum = NULL;
    int64_t Height, Width, BPC;
    unsigned char Buffer[1024];
    uint64_t toread;

    Width = pim->Width;
    Height = pim->Height;
    BPC = pim->BitsPerComponent;

    penum = gs_image_enum_alloc(ctx->memory, "xps_parse_image_brush (gs_image_enum_alloc)");
    if (!penum) {
        code = gs_note_error(gs_error_VMerror);
        goto cleanupExit;
    }

    if (ImageMask) {
        /* for ImageMask, the code below expects the colorspace to be NULL, and instead takes the 
         * color from the current graphics state.  But we need to swap it so it will get the
         * non-stroking color space.  We will swap it back later in this routine.
         */
        gs_swapcolors(ctx->pgs);
    }
    
    /* Took this logic from gs_image_init() 
     * (the other tests in there have already been handled above
     */
    {
        gx_image_enum_common_t *pie;

        if (!ImageMask) {
            /* TODO: Can in_cachedevie ever be set in PDF? */
            if (ctx->pgs->in_cachedevice != CACHE_DEVICE_NONE) {
                code = gs_note_error(gs_error_undefined);
                goto cleanupExit;
            }
        }

        code = gs_image_begin_typed((const gs_image_common_t *)pim, ctx->pgs, ImageMask, false, &pie);
        if (code < 0)
            goto cleanupExit;
        
        code = gs_image_enum_init(penum, pie, (const gs_data_image_t *)pim, ctx->pgs);
        if (code < 0)
            goto cleanupExit;
    }
    
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
    code = 0;
    
 cleanupExit:
    if (penum)
        gs_image_cleanup_and_free_enum(penum, ctx->pgs);
    return code;
}

static int
pdfi_do_image(pdf_context *ctx, pdf_dict *page_dict, pdf_dict *stream_dict, pdf_dict *image_dict,
              pdf_stream *source, bool inline_image)
{
    pdf_stream *new_stream = NULL;
    int64_t Height, Width, BPC;
    int i, code, comps = 0, byteswide, total;
    byte c;
    bool flush = false;
    gs_color_space  *pcs = NULL;
    gs_image1_t t1image;
    gs_image4_t t4image;
    gs_image3_t t3image;
    pdfi_image_info_t image_info, mask_info;
    pdf_dict *mask_dict = NULL;
    pdf_array *mask_array = NULL;
    
    memset(&mask_info, 0, sizeof(mask_info));

    code = pdfi_get_image_info(ctx, image_dict, &image_info);
    if (code < 0)
        goto cleanupExit;

    /* TODO: Handle Alternates */
    /* If image_info.Alternates, look in the array, see if any of them are flagged as "DefaultForPrinting"
     * and if so, substitute that one for the image we are processing.
     * (it can probably be either an array, or a reference to an array, need an example to test/implement)
     * see p.274 of PDFReference.pdf
     */
    

    Width = image_info.Width;
    Height = image_info.Height;
    BPC = image_info.BPC;

    if (image_info.ImageMask) {
        comps = 1;
    }

    if (image_info.Mask != NULL) {
        if (image_info.Mask->type == PDF_ARRAY) {
            mask_array = (pdf_array *)image_info.Mask;
        } else if (image_info.Mask->type == PDF_DICT) {
            mask_dict = (pdf_dict *)image_info.Mask;
            code = pdfi_get_image_info(ctx, mask_dict, &mask_info);
            if (code < 0)
                goto cleanupExit;
        } else {
            code = gs_note_error(gs_error_typecheck);
            goto cleanupExit;
        }
    }
    
    /* NOTE: Spec says ImageMask and ColorSpace mutually exclusive */
    if (image_info.ColorSpace == NULL) {
        if (image_info.ImageMask) {
#if 0
            /* No ColorSpace is okay (expected) for ImageMask */
            gs_swapcolors(ctx->pgs);
            pcs = gs_currentcolorspace(ctx->pgs);
            gs_swapcolors(ctx->pgs);
#endif
        } else {
            /* TODO: this is invalid by the spec, no pcs so it will bail out below
             * What is correct handling?
             */
            gx_device *dev = gs_currentdevice_inline(ctx->pgs);
            comps = dev->color_info.num_components;
            /* TODO: probably don't need to do above, since we will flush anyway? */
            flush = true;
        }
    } else {
        code = pdfi_create_colorspace(ctx, image_info.ColorSpace, page_dict, stream_dict, &pcs);
        if (code < 0)
            goto cleanupExit;
        comps = gs_color_space_num_components(pcs);
    }
    /* At this point, comps and pcs are setup */
    
    /* Setup the data stream for the image data */
    code = pdfi_filter(ctx, image_dict, source, &new_stream, inline_image);
    if (code < 0)
        goto cleanupExit;

    /* Handle null pcs -- this is just swallowing the image data stream and continuing */
    /* TODO: Is this correct or should we flag error? */
    /* TODO: isn't this "flushing" only needed for inline_image? Do we really need to do it? */
    if (flush) {
        byteswide = ((Width * comps * BPC) + 7) / 8;
        total = byteswide * Height;

        for (i=0;i < total;i++) {
            code = pdfi_read_bytes(ctx, &c, 1, 1, new_stream);
            if (code < 0) {
                pdfi_close_file(ctx, new_stream);
                goto cleanupExit;
            }
        }
        code = 0;
        goto cleanupExit;
    }

    /* Get the image into a supported gs type (type1, type3, type4) */
    memset(&t1image, 0, sizeof(t1image));

    if (image_info.ImageMask) {
        /* Sets up timage.ImageMask, amongst other things */
        gs_image_t_init_adjust(&t1image, NULL, false);
        t1image.ColorSpace = NULL;
    } else {
        gs_image_t_init_adjust(&t1image, pcs, true);
        t1image.ColorSpace = pcs;
    }
    t1image.BitsPerComponent = BPC;
    t1image.Width = Width;
    t1image.Height = Height;
    t1image.ImageMatrix.xx = (float)Width;
    t1image.ImageMatrix.yy = (float)(Height * -1);
    t1image.ImageMatrix.ty = (float)Height;

    t1image.Interpolate = image_info.Interpolate;

    /* Get the decode array (required for ImageMask, probably for everything */
    if (image_info.Decode) {
        pdf_array *decode_array = (pdf_array *)image_info.Decode;
        int i;
        double num;

        if (decode_array->size > GS_IMAGE_MAX_COMPONENTS * 2) {
            code = gs_note_error(gs_error_limitcheck);
            goto cleanupExit;
        }

        for (i=0; i<decode_array->size; i++) {
            code = pdfi_array_get_number(ctx, decode_array, i, &num);
            if (code < 0)
                goto cleanupExit;
            t1image.Decode[i] = (float)num;
        }
    }


    code = pdfi_render_image(ctx, (gs_pixel_image_t *)&t1image, new_stream, comps, image_info.ImageMask);
    if (code < 0)
        goto cleanupExit;

    code = 0;
    
 cleanupExit:
    pdfi_close_file(ctx, new_stream);
    pdfi_free_image_info_components(&image_info);
    pdfi_free_image_info_components(&mask_info);
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
