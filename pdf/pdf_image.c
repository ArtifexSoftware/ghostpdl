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


static inline uint64_t
pdfi_get_image_data_size(gs_data_image_t *pim, int comps)
{
    int size;
    int64_t H, W, B;

    H = pim->Height;
    W = pim->Width;
    B = pim->BitsPerComponent;
        
    size = (((W * comps * B) + 7) / 8) * H;
    return size;
}

static inline uint64_t
pdfi_get_image_line_size(gs_data_image_t *pim, int comps)
{
    int size;
    int64_t W, B;

    W = pim->Width;
    B = pim->BitsPerComponent;
        
    size = (((W * comps * B) + 7) / 8);
    return size;
}

/* Find first dictionary in array that contains "/DefaultForPrinting true" */
static pdf_dict *
pdfi_find_alternate(pdf_context *ctx, pdf_obj *alt)
{
    pdf_array *array;
    pdf_obj *item;
    pdf_dict *alt_dict;
    int i;
    int code;
    bool flag;
    
    if (alt->type != PDF_ARRAY)
        return NULL;
    
    array = (pdf_array *)alt;
    for (i=0; i<array->size;i++) {
        item = array->values[i];
        if (item->type != PDF_DICT)
            continue;
        code = pdfi_dict_get_bool(ctx, (pdf_dict *)item, "DefaultForPrinting", &flag);
        if (code != 0 || !flag)
            continue;
        code = pdfi_dict_get(ctx, (pdf_dict *)item, "Image", (pdf_obj **)&alt_dict);
        if (code != 0)
            continue;
        if (alt_dict->type != PDF_DICT)
            continue;
        return alt_dict;
    }
    return NULL;
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

    /* Optional (default is probably [0,1] per component) */
    code = pdfi_dict_get2(ctx, image_dict, "Decode", "D", &info->Decode);
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
pdfi_render_image(pdf_context *ctx, gs_pixel_image_t *pim, pdf_stream *image_stream,
                  unsigned char *mask_buffer, uint64_t mask_size,
                  int comps, bool ImageMask)
{
    int code;
    bool colors_swapped = false;
    gs_image_enum *penum = NULL;
    //    unsigned char Buffer[1024];
    byte *buffer = NULL;
    uint64_t linelen, bytesleft;
    gs_const_string plane_data[GS_IMAGE_MAX_COMPONENTS];
    int main_plane, mask_plane;
    
    penum = gs_image_enum_alloc(ctx->memory, "pdfi_render_image (gs_image_enum_alloc)");
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
        colors_swapped = true;
    }
    
    /* Took this logic from gs_image_init() 
     * (the other tests in there have already been handled elsewhere)
     */
    {
        gx_image_enum_common_t *pie;

        if (!ImageMask) {
            /* TODO: Can in_cachedevice ever be set in PDF? */
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
    
    /* NOTE: I used image_file_continue() as my template for this code.
     * But this case is (hopefully) much much simpler.
     * We only handle two situations -- if there is mask_data, then we assume there are two planes.
     * If no mask_data, then there is one plane.
     */
    if (mask_buffer) {
        main_plane = 1;
        mask_plane = 0;
        plane_data[mask_plane].data = mask_buffer;
        plane_data[mask_plane].size = mask_size;
    } else {
        main_plane = 0;
    }
    
    /* Going to feed the data one line at a time.  
     * This isn't required by gs_image_next_planes(), but it might make things simpler.
     */
    linelen = pdfi_get_image_line_size((gs_data_image_t *)pim, comps);
    bytesleft = pdfi_get_image_data_size((gs_data_image_t *)pim, comps);
    buffer = gs_alloc_bytes(ctx->memory, linelen, "pdfi_render_image (buffer)");
    if (!buffer) {
        code = gs_note_error(gs_error_VMerror);
        goto cleanupExit;
    }
    while (bytesleft > 0) {
        uint used[GS_IMAGE_MAX_COMPONENTS];
        
        code = pdfi_read_bytes(ctx, buffer, 1, linelen, image_stream);
        if (code < 0) {
            goto cleanupExit;
        }
        if (code != linelen) {
            code = gs_note_error(gs_error_limitcheck);
            goto cleanupExit;
        }
        
        plane_data[main_plane].data = buffer;
        plane_data[main_plane].size = linelen;
        
        code = gs_image_next_planes(penum, plane_data, used);
        if (code < 0) {
            goto cleanupExit;
        }
        /* TODO: Deal with case where it didn't consume all the data
         * Maybe this will never happen when I feed a line at a time?
         * Does it always consume all the mask data?
         * (I am being lazy and waiting for a sample file that doesn't work...)
         */
        bytesleft -= used[main_plane];
    }

    code = 0;
    
 cleanupExit:
    if (buffer)
        gs_free_object(ctx->memory, buffer, "pdfi_render_image (buffer)");
    if (colors_swapped)
        gs_swapcolors(ctx->pgs);
    if (penum)
        gs_image_cleanup_and_free_enum(penum, ctx->pgs);
    return code;
}

/* Load up params common to the different image types */
static int
pdfi_data_image_params(pdf_context *ctx, pdfi_image_info_t *info, gs_data_image_t *pim, int comps)
{
    int code;
    
    pim->BitsPerComponent = info->BPC;
    pim->Width = info->Width;
    pim->Height = info->Height;
    pim->ImageMatrix.xx = (float)info->Width;
    pim->ImageMatrix.yy = (float)(info->Height * -1);
    pim->ImageMatrix.ty = (float)info->Height;

    pim->Interpolate = info->Interpolate;

    /* Get the decode array (required for ImageMask, probably for everything) */
    if (info->Decode) {
        pdf_array *decode_array = (pdf_array *)info->Decode;
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
            pim->Decode[i] = (float)num;
        }
    } else {
        /* Provide a default if not specified [0 1 ...] per component */
        int i;
        for (i=0; i<comps*2; i+=2) {
            pim->Decode[i] = 0.0;
            pim->Decode[i+1] = 1.0;
        }
    }
    code = 0;
    
 cleanupExit:
    return code;
}

/* NOTE: "source" is the current input stream.
 * on exit:
 *  inline_image = TRUE, stream it will point to after the image data.
 *  inline_image = FALSE, stream position undefined.
 */
static int
pdfi_do_image(pdf_context *ctx, pdf_dict *page_dict, pdf_dict *stream_dict, pdf_dict *image_dict,
              pdf_stream *source, bool inline_image)
{
    pdf_stream *new_stream = NULL;
    pdf_stream *mask_stream = NULL;
    int code, comps = 0;
    bool flush = false;
    gs_color_space  *pcs = NULL;
    gs_image1_t t1image;
    gs_image4_t t4image;
    gs_image3_t t3image;
    gs_pixel_image_t *pim;
    pdf_dict *alt_dict = NULL;
    pdfi_image_info_t image_info, mask_info;
    pdf_dict *mask_dict = NULL;
    pdf_array *mask_array = NULL;
    unsigned char *mask_buffer = NULL;
    uint64_t mask_size = 0;
    
    memset(&mask_info, 0, sizeof(mask_info));

    code = pdfi_get_image_info(ctx, image_dict, &image_info);
    if (code < 0)
        goto cleanupExit;

    /* If there is an alternate, swap it in */
    /* If image_info.Alternates, look in the array, see if any of them are flagged as "DefaultForPrinting"
     * and if so, substitute that one for the image we are processing.
     * (it can probably be either an array, or a reference to an array, need an example to test/implement)
     * see p.274 of PDFReference.pdf
     */
    
    if (image_info.Alternates != NULL) {
        alt_dict = pdfi_find_alternate(ctx, image_info.Alternates);
        if (alt_dict != NULL) {
            image_dict = alt_dict;
            pdfi_free_image_info_components(&image_info);
            code = pdfi_get_image_info(ctx, image_dict, &image_info);
            if (code < 0)
                goto cleanupExit;
        }
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
    if (image_info.ImageMask) {
        comps = 1;
        pcs = NULL;
    } else {
        if (image_info.ColorSpace == NULL) {
            /* This is invalid by the spec, ColorSpace is required if not ImageMask.
             * Will bail out below, but need to calculate the number of comps for flushing.
             */
            gx_device *dev = gs_currentdevice_inline(ctx->pgs);
            comps = dev->color_info.num_components;
            pcs = NULL;
            flush = true;
        } else {
            code = pdfi_create_colorspace(ctx, image_info.ColorSpace, page_dict, stream_dict, &pcs);
            /* TODO: image_2bpp.pdf has an image in there somewhere that fails on this call (probably ColorN) */
            if (code < 0)
                goto cleanupExit;
            comps = gs_color_space_num_components(pcs);
        }
    }

    /* Get the image into a supported gs type (type1, type3, type4) */
    if (!image_info.Mask) { /* Type 1 and ImageMask */
        memset(&t1image, 0, sizeof(t1image));
        pim = (gs_pixel_image_t *)&t1image;
    
        if (image_info.ImageMask) {
            /* Sets up timage.ImageMask, amongst other things */
            gs_image_t_init_adjust(&t1image, NULL, false);
        } else {
            gs_image_t_init_adjust(&t1image, pcs, true);
        }
    } else {
        if (mask_array) { /* Type 4 */
            memset(&t4image, 0, sizeof(t1image));
            pim = (gs_pixel_image_t *)&t4image;
            gs_image4_t_init(&t4image, NULL);
            {
                int i;
                double num;

                if (mask_array->size > GS_IMAGE_MAX_COMPONENTS * 2) {
                    code = gs_note_error(gs_error_limitcheck);
                    goto cleanupExit;
                }

                for (i=0; i<mask_array->size; i++) {
                    code = pdfi_array_get_number(ctx, mask_array, i, &num);
                    if (code < 0)
                        goto cleanupExit;
                    t4image.MaskColor[i] = (float)num;
                }
                t4image.MaskColor_is_range = true;
            }
        } else { /* Type 3 (or is it 3x?) */
            memset(&t3image, 0, sizeof(t1image));
            pim = (gs_pixel_image_t *)&t3image;
            gs_image3_t_init(&t3image, NULL, interleave_separate_source);
            code = pdfi_data_image_params(ctx, &mask_info, &t3image.MaskDict, 1);
            if (code < 0)
                goto cleanupExit;

        }
    }

    /* Setup the common params */
    pim->ColorSpace = pcs;
    code = pdfi_data_image_params(ctx, &image_info, (gs_data_image_t *)pim, comps);
    if (code < 0)
        goto cleanupExit;

    /* Grab the mask_image data buffer in advance.
     * Doing it this way because I don't want to muck with reading from
     * two streams simultaneously -- not even sure that is feasible?
     */
    if (mask_dict) {
        mask_size = pdfi_get_image_data_size((gs_data_image_t *)&t3image.MaskDict, 1);
        
        pdfi_seek(ctx, source, mask_dict->stream_offset, SEEK_SET);
        mask_buffer = gs_alloc_bytes(ctx->memory, mask_size, "pdfi_do_image (mask_buffer)");
        if (!mask_buffer) {
            code = gs_note_error(gs_error_VMerror);
            goto cleanupExit;
        }

        /* Setup the data stream for the mask data */
        code = pdfi_filter(ctx, mask_dict, source, &mask_stream, false);
        if (code < 0)
            goto cleanupExit;

        code = pdfi_read_bytes(ctx, mask_buffer, 1, mask_size, mask_stream);
        if (code < 0)
            goto cleanupExit;
    }

    
    /* Handle null ColorSpace -- this is just swallowing the image data stream and continuing */
    /* TODO: Is this correct or should we flag error? */
    if (flush) {
        if (inline_image) {
            int total;

            total = pdfi_get_image_data_size((gs_data_image_t *)pim, comps);
            pdfi_seek(ctx, source, image_dict->stream_offset + total, SEEK_SET);

            code = 0; /* TODO: Should we flag an error instead of silently swallowing? */
            goto cleanupExit;
        } else {
            code = 0; /* TODO: Should we flag an error instead of just ignoring? */
            goto cleanupExit;
        }
    }


    /* Setup the data stream for the image data */
    pdfi_seek(ctx, source, image_dict->stream_offset, SEEK_SET);
    code = pdfi_filter(ctx, image_dict, source, &new_stream, inline_image);
    if (code < 0)
        goto cleanupExit;

    /* Render the image */
    code = pdfi_render_image(ctx, pim, new_stream,
                             mask_buffer, mask_size,
                             comps, image_info.ImageMask);
    if (code < 0)
        goto cleanupExit;

    code = 0;
    
 cleanupExit:
    if (new_stream)
        pdfi_close_file(ctx, new_stream);
    if (mask_stream)
        pdfi_close_file(ctx, mask_stream);
    if (mask_buffer)
        gs_free_object(ctx->memory, mask_buffer, "pdfi_do_image (mask_buffer)");
    if (alt_dict) {
        pdfi_countdown(alt_dict);
    }

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
