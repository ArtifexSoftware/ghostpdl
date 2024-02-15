/* Copyright (C) 2018-2024 Artifex Software, Inc.
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

/* Image operations for the PDF interpreter */

#include "pdf_int.h"
#include "pdf_stack.h"
#include "pdf_font_types.h"
#include "pdf_gstate.h"
#include "pdf_doc.h"
#include "pdf_page.h"
#include "pdf_image.h"
#include "pdf_file.h"
#include "pdf_dict.h"
#include "pdf_array.h"
#include "pdf_loop_detect.h"
#include "pdf_colour.h"
#include "pdf_trans.h"
#include "pdf_misc.h"
#include "pdf_optcontent.h"
#include "pdf_mark.h"
#include "stream.h"     /* for stell() */
#include "gsicc_cache.h"

#include "gspath2.h"
#include "gsiparm4.h"
#include "gsiparm3.h"
#include "gsipar3x.h"
#include "gsform1.h"
#include "gstrans.h"
#include "gxdevsop.h"               /* For special ops */
#include "gspath.h"         /* For gs_moveto() and friends */
#include "gsstate.h"        /* For gs_setoverprintmode() */
#include "gscoord.h"        /* for gs_concat() and others */

int pdfi_BI(pdf_context *ctx)
{
    if (ctx->text.BlockDepth != 0)
        pdfi_set_warning(ctx, 0, NULL, W_PDF_OPINVALIDINTEXT, "pdfi_BI", NULL);

    return pdfi_mark_stack(ctx, PDF_DICT_MARK);
}

typedef struct {
    int comps;
    int bpc;
    uint32_t cs_enum;
    bool iccbased;
    bool no_data;
    bool is_valid;
    uint32_t icc_offset;
    uint32_t icc_length;
} pdfi_jpx_info_t;

typedef struct {
    /* Type and SubType were already checked by caller */
    /* OPI, Metadata -- do we care? */
    bool ImageMask;
    bool Interpolate;
    int64_t Length;
    int64_t Height;
    int64_t Width;
    int64_t BPC;
    int64_t StructParent;
    int64_t SMaskInData;
    pdf_obj *Mask;
    pdf_obj *SMask;
    pdf_obj *ColorSpace;
    pdf_name *Intent;
    pdf_obj *Alternates;
    pdf_obj *Name; /* obsolete, do we still support? */
    pdf_obj *Decode;
    pdf_dict *OC;  /* Optional Content */
    /* Filter and DecodeParms handled by pdfi_filter() (can probably remove, but I like the info while debugging) */
    bool is_JPXDecode;
    pdf_obj *Filter;
    pdf_obj *DecodeParms;

    /* Convenience variables (save these here instead of passing around as params) */
    pdf_dict *page_dict;
    pdf_dict *stream_dict;
    bool inline_image;
    pdfi_jpx_info_t jpx_info;
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
    if (info->OC)
        pdfi_countdown(info->OC);
    if (info->Filter)
        pdfi_countdown(info->Filter);
    if (info->DecodeParms)
        pdfi_countdown(info->DecodeParms);
    memset(info, 0, sizeof(*info));
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
pdfi_data_size_from_image_info(pdfi_image_info_t *info, int comps)
{
    int size;
    int64_t H, W, B;

    H = info->Height;
    W = info->Width;
    B = info->BPC;

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
static pdf_stream *
pdfi_find_alternate(pdf_context *ctx, pdf_obj *alt)
{
    pdf_array *array = NULL;
    pdf_obj *item = NULL;
    pdf_stream *alt_stream = NULL;
    int i;
    int code;
    bool flag;

    if (pdfi_type_of(alt) != PDF_ARRAY)
        return NULL;

    array = (pdf_array *)alt;
    for (i=0; i<pdfi_array_size(array);i++) {
        code = pdfi_array_get_type(ctx, array, (uint64_t)i, PDF_DICT, &item);
        if (code != 0)
            continue;
        code = pdfi_dict_get_bool(ctx, (pdf_dict *)item, "DefaultForPrinting", &flag);
        if (code != 0 || !flag) {
            pdfi_countdown(item);
            item = NULL;
            continue;
        }
        code = pdfi_dict_get_type(ctx, (pdf_dict *)item, "Image", PDF_STREAM, (pdf_obj **)&alt_stream);
        pdfi_countdown(item);
        item = NULL;
        if (code != 0)
            continue;
        return alt_stream;
    }
    return NULL;
}

#define READ32BE(i) (((i)[0] << 24) | ((i)[1] << 16) | ((i)[2] << 8) | (i)[3])
#define READ16BE(i) (((i)[0] << 8) | (i)[1])
#define K4(a, b, c, d) ((a << 24) + (b << 16) + (c << 8) + d)
#define LEN_IHDR 14
#define LEN_DATA 2048

/* Returns either < 0, or exactly 8 */
static int
get_box(pdf_context *ctx, pdf_c_stream *source, int length, uint32_t *box_len, uint32_t *box_val)
{
    int code;
    byte blob[4];

    if (length < 8)
        return_error(gs_error_limitcheck);
    code = pdfi_read_bytes(ctx, blob, 1, 4, source);
    if (code < 0)
        return code;
    *box_len = READ32BE(blob);
    if (*box_len < 8)
        return_error(gs_error_limitcheck);
    code = pdfi_read_bytes(ctx, blob, 1, 4, source);
    if (code < 0)
        return code;
    *box_val = READ32BE(blob);

    if(ctx->args.pdfdebug)
        dbgmprintf3(ctx->memory, "JPXFilter: BOX: l:%d, v:%x (%4.4s)\n", *box_len, *box_val, blob);
    return 8;
}

/* Scan JPX image for header info */
static int
pdfi_scan_jpxfilter(pdf_context *ctx, pdf_c_stream *source, int length, pdfi_jpx_info_t *info)
{
    uint32_t box_len = 0;
    uint32_t box_val = 0;
    int code;
    byte ihdr_data[LEN_IHDR];
    byte *data = NULL;
    int data_buf_len = 0;
    int avail = length;
    int bpc = 0;
    int comps = 0;
    int cs_meth = 0;
    uint32_t cs_enum = 0;
    bool got_color = false;

    if (ctx->args.pdfdebug)
        dbgmprintf1(ctx->memory, "JPXFilter: Image length %d\n", length);

    /* Clear out the info param */
    memset(info, 0, sizeof(pdfi_jpx_info_t));

    info->no_data = false;

    /* Allocate a data buffer that hopefully is big enough */
    data_buf_len = LEN_DATA;
    data = gs_alloc_bytes(ctx->memory, data_buf_len, "pdfi_scan_jpxfilter (data)");
    if (!data) {
        code = gs_note_error(gs_error_VMerror);
        goto exit;
    }

    /* Find the 'jp2h' box, skipping over everything else */
    while (avail > 0) {
        code = get_box(ctx, source, avail, &box_len, &box_val);
        if (code < 0)
            goto exit;
        avail -= 8;
        box_len -= 8;
        if (box_len <= 0 || box_len > avail) {
            dmprintf1(ctx->memory, "WARNING: invalid JPX header, box_len=0x%x\n", box_len+8);
            code = gs_note_error(gs_error_syntaxerror);
            goto exit;
        }
        if (box_val == K4('j','p','2','h')) {
            break;
        }
        pdfi_seek(ctx, source, box_len, SEEK_CUR);
        avail -= box_len;
    }
    if (avail <= 0) {
        info->no_data = true;
        code = gs_note_error(gs_error_ioerror);
        goto exit;
    }

    /* Now we are only looking inside the jp2h box */
    avail = box_len;

    /* The first thing in the 'jp2h' box is an 'ihdr', get that */
    code = get_box(ctx, source, avail, &box_len, &box_val);
    if (code < 0)
        goto exit;
    avail -= 8;
    box_len -= 8;
    if (box_val != K4('i','h','d','r')) {
        code = gs_note_error(gs_error_syntaxerror);
        goto exit;
    }
    if (box_len != LEN_IHDR) {
        code = gs_note_error(gs_error_syntaxerror);
        goto exit;
    }

    /* Get things we care about from ihdr */
    code = pdfi_read_bytes(ctx, ihdr_data, 1, LEN_IHDR, source);
    if (code < 0)
        goto exit;
    avail -= LEN_IHDR;
    comps = READ16BE(ihdr_data+8);
    if (ctx->args.pdfdebug)
        dbgmprintf1(ctx->memory, "    COMPS: %d\n", comps);
    bpc = ihdr_data[10];
    if (bpc != 255)
        bpc += 1;
    if (ctx->args.pdfdebug)
        dbgmprintf1(ctx->memory, "    BPC: %d\n", bpc);

    /* Parse the rest of the things */
    while (avail > 0) {
        code = get_box(ctx, source, avail, &box_len, &box_val);
        if (code < 0)
            goto exit;
        avail -= 8;
        box_len -= 8;
        if (box_len <= 0) {
            code = gs_note_error(gs_error_syntaxerror);
            goto exit;
        }
        /* Re-alloc buffer if it wasn't big enough (unlikely) */
        if (box_len > data_buf_len) {
            if (ctx->args.pdfdebug)
                dbgmprintf2(ctx->memory, "data buffer (size %d) was too small, reallocing to size %d\n",
                      data_buf_len, box_len);
            gs_free_object(ctx->memory, data, "pdfi_scan_jpxfilter (data)");
            data_buf_len = box_len;
            data = gs_alloc_bytes(ctx->memory, data_buf_len, "pdfi_scan_jpxfilter (data)");
            if (!data) {
                code = gs_note_error(gs_error_VMerror);
                goto exit;
            }
        }
        code = pdfi_read_bytes(ctx, data, 1, box_len, source);
        if (code < 0)
            goto exit;
        avail -= box_len;
        switch(box_val) {
        case K4('b','p','c','c'):
            {
                int i;
                int bpc2;

                bpc2 = data[0];
                for (i=1;i<comps;i++) {
                    if (bpc2 != data[i]) {
                        emprintf(ctx->memory,
                                 "*** Error: JPX image colour channels do not all have the same colour depth\n");
                        emprintf(ctx->memory,
                                 "    Output may be incorrect.\n");
                    }
                }
                bpc = bpc2+1;
                if (ctx->args.pdfdebug)
                    dbgmprintf1(ctx->memory, "    BPCC: %d\n", bpc);
            }
            break;
        case K4('c','o','l','r'):
            if (got_color) {
                if (ctx->args.pdfdebug)
                    dbgmprintf(ctx->memory, "JPXFilter: Ignore extra COLR specs\n");
                break;
            }
            cs_meth = data[0];
            if (cs_meth == 1)
                cs_enum = READ32BE(data+3);
            else if (cs_meth == 2 || cs_meth == 3) {
                /* This is an ICCBased color space just sitting there in the buffer.
                 * TODO: I could create the colorspace now while I have the buffer,
                 * but code flow is more consistent if I do it later.  Could change this.
                 *
                 * NOTE: cs_meth == 3 is apparently treated the same as 2.
                 * No idea why... it's really not documented anywhere.
                 */
                info->iccbased = true;
                info->icc_offset = pdfi_tell(source) - (box_len-3);
                info->icc_length = box_len - 3;
                if (ctx->args.pdfdebug)
                    dbgmprintf5(ctx->memory, "JPXDecode: COLR Meth %d at offset %d(0x%x), length %d(0x%x)\n",
                                cs_meth, info->icc_offset, info->icc_offset,
                                info->icc_length, info->icc_length);
                cs_enum = 0;
            } else {
                if (ctx->args.pdfdebug)
                    dbgmprintf1(ctx->memory, "JPXDecode: COLR unexpected method %d\n", cs_meth);
                cs_enum = 0;
            }
            if (ctx->args.pdfdebug)
                dbgmprintf2(ctx->memory, "    COLR: M:%d, ENUM:%d\n", cs_meth, cs_enum);
            got_color = true;
            break;
        case K4('p','c','l','r'):
            /* Apparently we just grab the BPC out of this */
            if (ctx->args.pdfdebug)
                dbgmprintf7(ctx->memory, "    PCLR Data: %x %x %x %x %x %x %x\n",
                      data[0], data[1], data[2], data[3], data[4], data[5], data[6]);
            bpc = data[3];
            bpc = (bpc & 0x7) + 1;
            if (ctx->args.pdfdebug)
                dbgmprintf1(ctx->memory, "    PCLR BPC: %d\n", bpc);
            break;
        case K4('c','d','e','f'):
            dbgmprintf(ctx->memory, "JPXDecode: CDEF not supported yet\n");
            break;
        default:
            break;
        }

    }

    info->comps = comps;
    info->bpc = bpc;
    info->cs_enum = cs_enum;
    info->is_valid = true;

 exit:
    if (data)
        gs_free_object(ctx->memory, data, "pdfi_scan_jpxfilter (data)");
    /* Always return 0 -- there are cases where this no image header at all, and just ignoring
     * the header seems to work.  In this case is_valid will be false, so we know not to rely on
     * the data from it.
     * Sample: tests_private/comparefiles/Bug694873.pdf
     */
    return 0;
}

/* Get image info out of dict into more convenient form, enforcing some requirements from the spec */
static int
pdfi_get_image_info(pdf_context *ctx, pdf_stream *image_obj,
                    pdf_dict *page_dict, pdf_dict *stream_dict, bool inline_image,
                    pdfi_image_info_t *info)
{
    int code;
    double temp_f;
    pdf_dict *image_dict = NULL;

    memset(info, 0, sizeof(*info));
    info->page_dict = page_dict;
    info->stream_dict = stream_dict;
    info->inline_image = inline_image;

    /* Not Handled: "ID", "OPI" */

    /* Length if it's in a stream dict */
    info->Length = pdfi_stream_length(ctx, image_obj);

    code = pdfi_dict_from_obj(ctx, (pdf_obj *)image_obj, &image_dict);
    if (code < 0)
        goto errorExit;

    /* Required */
    code = pdfi_dict_get_number2(ctx, image_dict, "Height", "H", &temp_f);
    if (code < 0)
        goto errorExit;
    /* This is bonkers, but... Bug695872.pdf has /W and /H which are real numbers */
    info->Height = (int)temp_f;
    if ((int)temp_f != (int)(temp_f+.5)) {
        pdfi_set_warning(ctx, 0, NULL, W_PDF_BAD_IMAGEDICT, "pdfi_get_image_info", NULL);
        if (ctx->args.pdfstoponwarning) {
            code = gs_note_error(gs_error_rangecheck);
            goto errorExit;
        }
    }
    if (info->Height < 0) {
        pdfi_set_warning(ctx, 0, NULL, W_PDF_BAD_IMAGEDICT, "pdfi_get_image_info", NULL);
        if (ctx->args.pdfstoponwarning) {
            code = gs_note_error(gs_error_rangecheck);
            goto errorExit;
        }
        info->Height = 0;
    }

    /* Required */
    code = pdfi_dict_get_number2(ctx, image_dict, "Width", "W", &temp_f);
    if (code < 0)
        goto errorExit;
    info->Width = (int)temp_f;
    if ((int)temp_f != (int)(temp_f+.5)) {
        pdfi_set_warning(ctx, 0, NULL, W_PDF_BAD_IMAGEDICT, "pdfi_get_image_info", NULL);
        if (ctx->args.pdfstoponwarning) {
            code = gs_note_error(gs_error_rangecheck);
            goto errorExit;
        }
    }
    if (info->Width < 0) {
        pdfi_set_warning(ctx, 0, NULL, W_PDF_BAD_IMAGEDICT, "pdfi_get_image_info", NULL);
        if (ctx->args.pdfstoponwarning) {
            code = gs_note_error(gs_error_rangecheck);
            goto errorExit;
        }
        info->Width = 0;
    }

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

    /* Optional (Required, unless ImageMask is true)
     * But apparently for JPXDecode filter, this can be omitted.
     * Let's try a default of 1 for now...
     */
    code = pdfi_dict_get_int2(ctx, image_dict, "BitsPerComponent", "BPC", &info->BPC);
    if (code < 0) {
        if (code != gs_error_undefined) {
            goto errorExit;
        }
        info->BPC = 1;
    }
    else if (info->BPC != 1 && info->BPC != 2 && info->BPC != 4 && info->BPC != 8 && info->BPC != 16) {
        code = gs_note_error(gs_error_rangecheck);
        goto errorExit;
    }
    /* TODO: spec says if ImageMask is specified, and BPC is specified, then BPC must be 1
       Should we flag an error if this is violated?
     */

    /* Optional (apparently there is no "M" abbreviation for "Mask"? */
    code = pdfi_dict_get(ctx, image_dict, "Mask", &info->Mask);
    if (code < 0) {
        /* A lack of a Mask is not an error. If there is a genuine error reading
         * the Mask, ignore it unless PDFSTOPONWARNING is set. We can still render
         * the image. Arguably we should not, and Acrobat doesn't, but the current
         * GS implementation does.
         */
        if (code != gs_error_undefined) {
           pdfi_set_warning(ctx, 0, NULL, W_PDF_BAD_IMAGEDICT, "pdfi_get_image_info", NULL);
           if (ctx->args.pdfstoponwarning)
                goto errorExit;
        }
    }
    if (info->Mask != NULL && (pdfi_type_of(info->Mask) != PDF_ARRAY && pdfi_type_of(info->Mask) != PDF_STREAM)) {
        pdfi_countdown(info->Mask);
        info->Mask = NULL;
        pdfi_set_warning(ctx, 0, NULL, W_PDF_BAD_IMAGEDICT, "pdfi_get_image_info", NULL);
        if (ctx->args.pdfstoponwarning)
            goto errorExit;
    }

    /* Optional (apparently there is no abbreviation for "SMask"? */
    code = pdfi_dict_get(ctx, image_dict, "SMask", &info->SMask);
    if (code < 0) {
        if (code != gs_error_undefined) {
            /* Broken SMask, Warn, and ignore the SMask */
            pdfi_set_warning(ctx, 0, NULL, W_PDF_BAD_IMAGEDICT, "pdfi_get_image_info", (char *)"*** Warning: Image has invalid SMask.  Ignoring it");
            if (ctx->args.pdfstoponwarning)
                goto errorExit;
            code = 0;
        }
    } else {
        if (pdfi_type_of(info->SMask) == PDF_NAME) {
            pdf_obj *o = NULL;

            code = pdfi_find_resource(ctx, (unsigned char *)"ExtGState", (pdf_name *)info->SMask, image_dict, page_dict, &o);
            if (code >= 0) {
                pdfi_countdown(info->SMask);
                info->SMask = o;
            }
        }

        if (pdfi_type_of(info->SMask) != PDF_STREAM){
            pdfi_countdown(info->SMask);
            info->SMask = NULL;
            pdfi_set_warning(ctx, 0, NULL, W_PDF_BAD_IMAGEDICT, "pdfi_get_image_info", NULL);
            if (ctx->args.pdfstoponwarning)
                goto errorExit;
        }
    }

    /* Optional, for JPXDecode filter images
     * (If non-zero, then SMask shouldn't be  specified)
     * Default: 0
     */
    code = pdfi_dict_get_int(ctx, image_dict, "SMaskInData", &info->SMaskInData);
    if (code < 0) {
        if (code != gs_error_undefined)
            goto errorExit;
        info->SMaskInData = 0;
    }

    /* Optional (Required except for ImageMask, not allowed for ImageMask)*/
    /* TODO: Should we enforce this required/not allowed thing? */
    code = pdfi_dict_get2(ctx, image_dict, "ColorSpace", "CS", &info->ColorSpace);
    if (code < 0) {
        if (code != gs_error_undefined)
            goto errorExit;
    }
    if (info->ColorSpace != NULL && (pdfi_type_of(info->ColorSpace) != PDF_NAME && pdfi_type_of(info->ColorSpace) != PDF_ARRAY)) {
        pdfi_countdown(info->ColorSpace);
        info->ColorSpace = NULL;
        pdfi_set_warning(ctx, 0, NULL, W_PDF_BAD_IMAGEDICT, "pdfi_get_image_info", NULL);
        if (ctx->args.pdfstoponwarning)
            goto errorExit;
    }

    /* Optional (default is to use from graphics state) */
    /* (no abbreviation for inline) */
    code = pdfi_dict_get_type(ctx, image_dict, "Intent", PDF_NAME, (pdf_obj **)&info->Intent);
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
    if (info->Alternates != NULL && pdfi_type_of(info->Alternates) != PDF_ARRAY) {
        pdfi_countdown(info->Alternates);
        info->Alternates = NULL;
        pdfi_set_warning(ctx, 0, NULL, W_PDF_BAD_IMAGEDICT, "pdfi_get_image_info", NULL);
        if (ctx->args.pdfstoponwarning)
            goto errorExit;
    }

    /* Optional (required in PDF1.0, obsolete, do we support?) */
    code = pdfi_dict_get(ctx, image_dict, "Name", &info->Name);
    if (code < 0) {
        if (code != gs_error_undefined)
            goto errorExit;
    }
    if (info->Name != NULL && pdfi_type_of(info->Name) != PDF_NAME) {
        pdfi_countdown(info->Name);
        info->Name = NULL;
        pdfi_set_warning(ctx, 0, NULL, W_PDF_BAD_IMAGENAME, "pdfi_get_image_info", NULL);
        if (ctx->args.pdfstoponwarning)
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
    if (info->Decode != NULL && pdfi_type_of(info->Decode) != PDF_ARRAY) {
        pdfi_countdown(info->Decode);
        info->Decode = NULL;
        pdfi_set_warning(ctx, 0, NULL, W_PDF_BAD_IMAGEDICT, "pdfi_get_image_info", NULL);
        if (ctx->args.pdfstoponwarning)
            goto errorExit;
    }

    /* Optional "Optional Content" */
    code = pdfi_dict_get_type(ctx, image_dict, "OC", PDF_DICT, (pdf_obj **)&info->OC);
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
    if (info->Filter != NULL && (pdfi_type_of(info->Filter) != PDF_NAME && pdfi_type_of(info->Filter) != PDF_ARRAY)) {
        pdfi_countdown(info->Filter);
        info->Filter = NULL;
        pdfi_set_warning(ctx, 0, NULL, W_PDF_BAD_IMAGEDICT, "pdfi_get_image_info", NULL);
        if (ctx->args.pdfstoponwarning)
            goto errorExit;
    }

    /* Check and set JPXDecode flag for later */
    info->is_JPXDecode = false;
    if (info->Filter && pdfi_type_of(info->Filter) == PDF_NAME) {
        if (pdfi_name_is((pdf_name *)info->Filter, "JPXDecode"))
            info->is_JPXDecode = true;
    }

    /* Optional */
    code = pdfi_dict_get2(ctx, image_dict, "DecodeParms", "DP", &info->DecodeParms);
    if (code < 0) {
        if (code != gs_error_undefined)
            goto errorExit;
    }
    if (info->DecodeParms != NULL && (pdfi_type_of(info->DecodeParms) != PDF_DICT && pdfi_type_of(info->DecodeParms) != PDF_ARRAY)) {
        /* null is legal. Pointless but legal. */
        if (pdfi_type_of(info->DecodeParms) != PDF_NULL) {
            pdfi_set_warning(ctx, 0, NULL, W_PDF_BAD_IMAGEDICT, "pdfi_get_image_info", NULL);
            if (ctx->args.pdfstoponwarning)
                goto errorExit;
        }
        pdfi_countdown(info->DecodeParms);
        info->DecodeParms = NULL;
    }

    return 0;

 errorExit:
    pdfi_free_image_info_components(info);
    return code;
}

static int pdfi_check_inline_image_keys(pdf_context *ctx, pdf_dict *image_dict)
{
    bool known = false;

    pdfi_dict_known(ctx, image_dict, "BPC", &known);
    if (known)
        goto error_inline_check;
    pdfi_dict_known(ctx, image_dict, "CS", &known);
    if (known)
        goto error_inline_check;
    pdfi_dict_known(ctx, image_dict, "D", &known);
    if (known)
        goto error_inline_check;
    pdfi_dict_known(ctx, image_dict, "DP", &known);
    if (known)
        goto error_inline_check;
    pdfi_dict_known(ctx, image_dict, "F", &known);
    if (known)
        goto error_inline_check;
    pdfi_dict_known(ctx, image_dict, "H", &known);
    if (known)
        goto error_inline_check;
    pdfi_dict_known(ctx, image_dict, "IM", &known);
    if (known)
        goto error_inline_check;
    pdfi_dict_known(ctx, image_dict, "I", &known);
    if (known)
        goto error_inline_check;
    pdfi_dict_known(ctx, image_dict, "W", &known);
    if (known)
        goto error_inline_check;

    return 0;

error_inline_check:
    pdfi_set_warning(ctx, 0, NULL, W_PDF_BAD_INLINEIMAGEKEY, "pdfi_check_inline_image_keys", NULL);
    if (ctx->args.pdfstoponwarning)
        return_error(gs_error_syntaxerror);
    return 0;
}

/* Render a PDF image
 * pim can be type1 (or imagemask), type3, type4
 */
static int
pdfi_render_image(pdf_context *ctx, gs_pixel_image_t *pim, pdf_c_stream *image_stream,
                  unsigned char *mask_buffer, uint64_t mask_size,
                  int comps, bool ImageMask)
{
    int code;
    gs_image_enum *penum = NULL;
    uint64_t bytes_left;
    uint64_t bytes_used = 0;
    uint64_t bytes_avail = 0;
    gs_const_string plane_data[GS_IMAGE_MAX_COMPONENTS];
    int main_plane=0, mask_plane=0;
    bool no_progress = false;
    int min_left;

#if DEBUG_IMAGES
    dbgmprintf(ctx->memory, "pdfi_render_image BEGIN\n");
#endif

    code = pdfi_gsave(ctx);
    if (code < 0)
        return code;

    /* Disable overprint mode for images that are not a mask */
    if (!ImageMask)
        gs_setoverprintmode(ctx->pgs, 0);

    penum = gs_image_enum_alloc(ctx->memory, "pdfi_render_image (gs_image_enum_alloc)");
    if (!penum) {
        code = gs_note_error(gs_error_VMerror);
        goto cleanupExit;
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

    bytes_left = pdfi_get_image_data_size((gs_data_image_t *)pim, comps);
    while (bytes_left > 0) {
        uint used[GS_IMAGE_MAX_COMPONENTS];

        while ((bytes_avail = sbufavailable(image_stream->s)) <= (min_left = sbuf_min_left(image_stream->s))) {
            switch (image_stream->s->end_status) {
            case 0:
                s_process_read_buf(image_stream->s);
                continue;
            case EOFC:
            case INTC:
            case CALLC:
                break;
            default:
                /* case ERRC: */
                code = gs_note_error(gs_error_ioerror);
                goto cleanupExit;
            }
            break;		/* for EOFC */
        }

        /*
         * Note that in the EOF case, we can get here with no data
         * available.
         */
        if (bytes_avail >= min_left)
            bytes_avail = (bytes_avail - min_left); /* may be 0 */

        plane_data[main_plane].data = sbufptr(image_stream->s);
        plane_data[main_plane].size = bytes_avail;

        code = gs_image_next_planes(penum, plane_data, used, false);
        if (code < 0) {
            goto cleanupExit;
        }
        /* If no data was consumed twice in a row, then there must be some kind of error,
         * even if the error code was not < 0.  Saw this with pdfwrite device.
         */
        if (used[main_plane] == 0 && used[mask_plane] == 0) {
            if (no_progress) {
                code = gs_note_error(gs_error_unknownerror);
                goto cleanupExit;
            }
            no_progress = true;
        } else {
            no_progress = false;
        }

        (void)sbufskip(image_stream->s, used[main_plane]);

        /* It might not always consume all the data, but so far the only case
         * I have seen with that was one that had mask data.
         * In that case, it used all of plane 0, and none of plane 1 on the first pass.
         * (image_2bpp.pdf)
         *
         * Anyway, this math should handle that case (as well as a case where it consumed only
         * part of the data, if that can actually happen).
         */
        bytes_used = used[main_plane];
        bytes_left -= bytes_used;
        bytes_avail -= bytes_used;
    }

    code = 0;

 cleanupExit:
    if (penum)
        gs_image_cleanup_and_free_enum(penum, ctx->pgs);
    pdfi_grestore(ctx);
#if DEBUG_IMAGES
    dbgmprintf(ctx->memory, "pdfi_render_image END\n");
#endif
    return code;
}

/* Load up params common to the different image types */
static int
pdfi_data_image_params(pdf_context *ctx, pdfi_image_info_t *info,
                       gs_data_image_t *pim, int comps, gs_color_space *pcs)
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

        if (pdfi_array_size(decode_array) > GS_IMAGE_MAX_COMPONENTS * 2) {
            code = gs_note_error(gs_error_limitcheck);
            goto cleanupExit;
        }

        for (i=0; i<pdfi_array_size(decode_array); i++) {
            code = pdfi_array_get_number(ctx, decode_array, i, &num);
            if (code < 0)
                goto cleanupExit;
            pim->Decode[i] = (float)num;
        }
    } else {
        /* Provide a default if not specified [0 1 ...] per component */
        int i;
        float minval, maxval;

        /* TODO: Is there a less hacky way to identify Indexed case? */
        if (pcs && (pcs->type == &gs_color_space_type_Indexed ||
                    pcs->type == &gs_color_space_type_Indexed_Named)) {
            /* Default value is [0,N], where N=2^n-1, our hival (which depends on BPC)*/
            minval = 0.0;
            maxval = (float)((1 << info->BPC) - 1);
        } else {
            bool islab = false;

            if (pcs && pcs->cmm_icc_profile_data != NULL)
                islab = pcs->cmm_icc_profile_data->islab;

            if(islab) {
                pim->Decode[0] = 0.0;
                pim->Decode[1] = 100.0;
                pim->Decode[2] = pcs->cmm_icc_profile_data->Range.ranges[1].rmin;
                pim->Decode[3] = pcs->cmm_icc_profile_data->Range.ranges[1].rmax;
                pim->Decode[4] = pcs->cmm_icc_profile_data->Range.ranges[2].rmin;
                pim->Decode[5] = pcs->cmm_icc_profile_data->Range.ranges[2].rmax;
                return 0;
            } else {
                minval = 0.0;
                maxval = 1.0;
            }
        }
        for (i=0; i<comps*2; i+=2) {
            pim->Decode[i] = minval;
            pim->Decode[i+1] = maxval;
        }
    }
    code = 0;

 cleanupExit:
    return code;
}

/* Returns number of components in Matte array or 0 if not found, <0 if error */
static int
pdfi_image_get_matte(pdf_context *ctx, pdf_obj *smask_obj, float *vals, int size, bool *has_Matte)
{
    int i;
    pdf_array *Matte = NULL;
    int code;
    double f;
    pdf_dict *smask_dict = NULL;

    *has_Matte = false;
    code = pdfi_dict_from_obj(ctx, smask_obj, &smask_dict);
    if (code < 0)
        goto exit;

    code = pdfi_dict_knownget_type(ctx, smask_dict, "Matte",
                                   PDF_ARRAY, (pdf_obj **)&Matte);
    if (code <= 0)
        goto exit;

    *has_Matte = true;
    if (pdfi_array_size(Matte) > size) {
        code = gs_note_error(gs_error_rangecheck);
        goto exit;
    }

    for (i = 0; i < pdfi_array_size(Matte); i++) {
        code = pdfi_array_get_number(ctx, Matte, (uint64_t)i, &f);
        if (code < 0)
            goto exit;
        vals[i] = (float)f;
    }
    if (i == pdfi_array_size(Matte))
        code = i;

 exit:
    pdfi_countdown(Matte);
    return code;
}

/* See ztrans.c/zbegintransparencymaskimage() and pdf_draw.ps/doimagesmask */
static int
pdfi_do_image_smask(pdf_context *ctx, pdf_c_stream *source, pdfi_image_info_t *image_info, bool *has_Matte)
{
    gs_rect bbox = { { 0, 0} , { 1, 1} };
    gs_transparency_mask_params_t params;
    gs_offset_t savedoffset = 0;
    int code, code1;
    pdfi_int_gstate *igs = (pdfi_int_gstate *)ctx->pgs->client_data;
    pdf_stream *stream_obj = NULL;

#if DEBUG_IMAGES
    dbgmprintf(ctx->memory, "pdfi_do_image_smask BEGIN\n");
#endif

    code = pdfi_loop_detector_mark(ctx);
    if (code < 0)
        return code;

    if (pdfi_type_of(image_info->SMask) != PDF_STREAM)
        return_error(gs_error_typecheck);

    if (image_info->SMask->object_num != 0) {
        if (pdfi_loop_detector_check_object(ctx, image_info->SMask->object_num))
            return gs_note_error(gs_error_circular_reference);
        code = pdfi_loop_detector_add_object(ctx, image_info->SMask->object_num);
        if (code < 0)
            goto exit;
    }

    gs_trans_mask_params_init(&params, TRANSPARENCY_MASK_Luminosity);

    code = pdfi_image_get_matte(ctx, image_info->SMask, params.Matte, GS_CLIENT_COLOR_MAX_COMPONENTS, has_Matte);

    if (code >= 0)
        params.Matte_components = code;

    code = gs_begin_transparency_mask(ctx->pgs, &params, &bbox, true);
    if (code < 0)
        goto exit;
    savedoffset = pdfi_tell(ctx->main_stream);
    code = pdfi_gsave(ctx);

    /* Disable SMask for inner image */
    pdfi_gstate_smask_free(igs);

    gs_setstrokeconstantalpha(ctx->pgs, 1.0);
    gs_setfillconstantalpha(ctx->pgs, 1.0);
    gs_setblendmode(ctx->pgs, BLEND_MODE_Compatible);

    pdfi_seek(ctx, ctx->main_stream,
              pdfi_stream_offset(ctx, (pdf_stream *)image_info->SMask), SEEK_SET);

    switch (pdfi_type_of(image_info->SMask)) {
        case PDF_DICT:
            code = pdfi_obj_dict_to_stream(ctx, (pdf_dict *)image_info->SMask, &stream_obj, false);
            if (code == 0) {
                code = pdfi_do_image_or_form(ctx, image_info->stream_dict,
                                             image_info->page_dict, (pdf_obj *)stream_obj);

                pdfi_countdown(stream_obj);
            }
            break;
        case PDF_STREAM:
            code = pdfi_do_image_or_form(ctx, image_info->stream_dict,
                                         image_info->page_dict, image_info->SMask);
            break;
        default:
            code = gs_note_error(gs_error_typecheck);
            goto exit;
    }

    pdfi_seek(ctx, ctx->main_stream, savedoffset, SEEK_SET);

    code1 = pdfi_grestore(ctx);
    if (code < 0)
        code = code1;
    code1 = gs_end_transparency_mask(ctx->pgs, TRANSPARENCY_CHANNEL_Opacity);
    if (code < 0)
        code = code1;

 exit:
#if DEBUG_IMAGES
    dbgmprintf(ctx->memory, "pdfi_do_image_smask END\n");
#endif
    pdfi_loop_detector_cleartomark(ctx);
    return code;
}


/* Setup for transparency (see pdf_draw.ps/doimage) */
static int
pdfi_image_setup_trans(pdf_context *ctx, pdfi_trans_state_t *state)
{
    int code;
    gs_rect bbox;

    /* We need to create a bbox in order to pass it to the transparency setup,
     * which (potentially, at least, uses it to set up a transparency group.
     * Setting up a 1x1 path, and establishing it's BBox will work, because
     * the image scaling is already in place. We don't want to disturb the
     * graphics state, so do this inside a gsave/grestore pair.
     */
    code = pdfi_gsave(ctx);
    if (code < 0)
        return code;

    code = gs_newpath(ctx->pgs);
    if (code < 0)
        goto exit;
    code = gs_moveto(ctx->pgs, 1.0, 1.0);
    if (code < 0)
        goto exit;
    code = gs_lineto(ctx->pgs, 0., 0.);
    if (code < 0)
        goto exit;

    code = pdfi_get_current_bbox(ctx, &bbox, false);
    if (code < 0)
        goto exit;

    pdfi_grestore(ctx);

    code = pdfi_trans_setup(ctx, state, &bbox, TRANSPARENCY_Caller_Image);
 exit:
    return code;
}


/* Setup a type 4 image, particularly the MaskColor array.
 * Handles error situations like pdf_draw.ps/makemaskimage
 */
static int
pdfi_image_setup_type4(pdf_context *ctx, pdfi_image_info_t *image_info,
                       gs_image4_t *t4image,
                       pdf_array *mask_array, gs_color_space *pcs)
{
    int i;
    double num;
    int code = 0;
    int bpc = image_info->BPC;
    uint mask = (1 << bpc) - 1;
    uint maxval = mask;
    int64_t intval;
    bool had_range_error = false;
    bool had_float_error = false;
    /* Check for special case of Indexed and BPC=1 (to match AR)
     * See bugs: 692852, 697919, 689717
     */
    bool indexed_case = (pcs && pcs->type == &gs_color_space_type_Indexed && bpc == 1);

    memset(t4image, 0, sizeof(gs_image4_t));
    gs_image4_t_init(t4image, NULL);

    if (pdfi_array_size(mask_array) > GS_IMAGE_MAX_COMPONENTS * 2) {
        code = gs_note_error(gs_error_rangecheck);
        goto exit;
    }

    for (i=0; i<pdfi_array_size(mask_array); i++) {
        code = pdfi_array_get_int(ctx, mask_array, i, &intval);
        if (code == gs_error_typecheck) {
            code = pdfi_array_get_number(ctx, mask_array, i, &num);
            if (code == 0) {
                intval = (int64_t)(num + 0.5);
                had_float_error = true;
            }
        }
        if (code < 0)
            goto exit;
        if (intval > maxval) {
            had_range_error = true;
            if (indexed_case) {
                if (i == 0) {
                    /* If first component is invalid, AR9 ignores the mask. */
                    code = gs_note_error(gs_error_rangecheck);
                    goto exit;
                } else {
                    /* If second component is invalid, AR9 replace it with 1. */
                    intval = 1;
                }
            } else {
                if (bpc != 1) {
                    /* If not special handling, just mask it off to be in range */
                    intval &= mask;
                }
            }
        }
        t4image->MaskColor[i] = intval;
    }
    t4image->MaskColor_is_range = true;

    /* Another special handling (see Bug701468)
     * If 1 BPC and the two entries are not the same, ignore the mask
     */
    if (!indexed_case && bpc == 1 && had_range_error) {
        if (t4image->MaskColor[0] != t4image->MaskColor[1]) {
            code = gs_note_error(gs_error_rangecheck);
            goto exit;
        } else {
            t4image->MaskColor[0] &= mask;
            t4image->MaskColor[1] &= mask;
        }
    }

    code = 0;

 exit:
    if (had_float_error) {
        pdfi_set_warning(ctx, 0, NULL, W_PDF_IMAGE_ERROR, "pdfi_image_setup_type4", (char *)"*** Error: Some elements of Mask array are not integers");
    }
    if (had_range_error) {
        pdfi_set_warning(ctx, 0, NULL, W_PDF_IMAGE_ERROR, "pdfi_image_setup_type4", (char *)"*** Error: Some elements of Mask array are out of range");
    }
    return code;
}

/* Setup a type 3x image
 */
static int
pdfi_image_setup_type3x(pdf_context *ctx, pdfi_image_info_t *image_info,
                        gs_image3x_t *t3ximage, pdfi_image_info_t *smask_info, int comps)
{
    int code = 0;
    gs_image3x_mask_t *mask;

    memset(t3ximage, 0, sizeof(*t3ximage));
    gs_image3x_t_init(t3ximage, NULL);
    if (gs_getalphaisshape(ctx->pgs))
        mask = &t3ximage->Shape;
    else
        mask = &t3ximage->Opacity;
    mask->InterleaveType = 3;

    code = pdfi_image_get_matte(ctx, image_info->SMask, mask->Matte, GS_CLIENT_COLOR_MAX_COMPONENTS, &mask->has_Matte);
    if (code < 0)
        return code;

    code = pdfi_data_image_params(ctx, smask_info, &mask->MaskDict, comps, NULL);
    return code;
}

static int pdfi_create_JPX_Lab(pdf_context *ctx, pdf_obj **ColorSpace)
{
    int code, i;
    pdf_name *SpaceName = NULL, *WhitePointName = NULL;
    pdf_dict *Params = NULL;
    pdf_array *WhitePoint = NULL;
    double WP[3] = {0.9505, 1.0, 1.0890};
    pdf_num *num = NULL;

    *ColorSpace = NULL;
    code = pdfi_name_alloc(ctx, (byte *)"Lab", 3, (pdf_obj **)&SpaceName);
    if (code < 0)
        goto cleanupExit;
    pdfi_countup(SpaceName);

    code = pdfi_dict_alloc(ctx, 1, &Params);
    if (code < 0)
        goto cleanupExit;
    pdfi_countup(Params);

    code = pdfi_name_alloc(ctx, (byte *)"WhitePoint", 3, (pdf_obj **)&WhitePointName);
    if (code < 0)
        goto cleanupExit;
    pdfi_countup(WhitePointName);

    code = pdfi_array_alloc(ctx, 3, &WhitePoint);
    if (code < 0)
        goto cleanupExit;

    for (i = 0; i < 3; i++) {
        code = pdfi_object_alloc(ctx, PDF_REAL, 0, (pdf_obj **)&num);
        if (code < 0)
            goto cleanupExit;
        num->value.d = WP[i];
        pdfi_countup(num);
        code = pdfi_array_put(ctx, WhitePoint, i, (pdf_obj *)num);
        if (code < 0)
            goto cleanupExit;
        pdfi_countdown(num);
    }
    num = NULL;

    code = pdfi_dict_put_obj(ctx, Params, (pdf_obj *)WhitePointName, (pdf_obj *)WhitePoint, true);
    if (code < 0)
        goto cleanupExit;

    pdfi_countdown(WhitePointName);
    WhitePointName = NULL;
    pdfi_countdown(WhitePoint);
    WhitePoint = NULL;

    code = pdfi_array_alloc(ctx, 2, (pdf_array **)ColorSpace);
    if (code < 0)
        goto cleanupExit;

    code = pdfi_array_put(ctx, (pdf_array *)*ColorSpace, 0, (pdf_obj *)SpaceName);
    if (code < 0)
        goto cleanupExit;
    pdfi_countdown(SpaceName);
    SpaceName = NULL;

    code = pdfi_array_put(ctx, (pdf_array *)*ColorSpace, 1, (pdf_obj *)Params);
    if (code < 0)
        goto cleanupExit;
    pdfi_countdown(Params);

    return 0;

cleanupExit:
    pdfi_countdown(*ColorSpace);
    pdfi_countdown(SpaceName);
    pdfi_countdown(Params);
    pdfi_countdown(WhitePointName);
    pdfi_countdown(WhitePoint);
    pdfi_countdown(num);
    return code;
}

static int
pdfi_image_get_color(pdf_context *ctx, pdf_c_stream *source, pdfi_image_info_t *image_info,
                     int *comps, ulong dictkey, gs_color_space **pcs)
{
    int code = 0;
    pdfi_jpx_info_t *jpx_info = &image_info->jpx_info;
    pdf_obj *ColorSpace = NULL;
    char *backup_color_name = NULL;
    bool using_enum_cs = false;

    /* NOTE: Spec says ImageMask and ColorSpace mutually exclusive
     * We need to make sure we do not have both set or we could end up treating
     * an image as a mask or vice versa and trying to use a non-existent colour space.
     */
    if (image_info->ImageMask) {
        if (image_info->ColorSpace == NULL) {
                *comps = 1;
                *pcs = NULL;
                if (image_info->Mask) {
                    pdfi_countdown(image_info->Mask);
                    image_info->Mask = NULL;
                }
                return 0;
        } else {
            if (image_info->BPC != 1 || image_info->Mask != NULL) {
                pdfi_set_error(ctx, 0, NULL, E_IMAGE_MASKWITHCOLOR, "pdfi_image_get_color", "BitsPerComonent is not 1, so treating as an image");
                image_info->ImageMask = 0;
            }
            else {
                pdfi_set_error(ctx, 0, NULL, E_IMAGE_MASKWITHCOLOR, "pdfi_image_get_color", "BitsPerComonent is 1, so treating as a mask");
                pdfi_countdown(image_info->ColorSpace);
                image_info->ColorSpace = NULL;
                *comps = 1;
                *pcs = NULL;
                return 0;
            }
        }
    }

    ColorSpace = image_info->ColorSpace;
    if (ColorSpace)
        pdfi_countup(ColorSpace);
    if (ColorSpace == NULL) {
        if (image_info->is_JPXDecode) {
            /* The graphics library doesn't support 12-bit images, so the openjpeg layer
             * (see sjpx_openjpeg.c/decode_image()) is going to translate the 12-bits up to 16-bits.
             * That means we just treat it as 16-bit when rendering, so force the value
             * to 16 here.
             */
            if (jpx_info->bpc == 12) {
                jpx_info->bpc = 16;
            }
            image_info->BPC = jpx_info->bpc;

            if (jpx_info->iccbased) {
                int dummy; /* Holds number of components read from the ICC profile, we ignore this here */

                code = pdfi_create_icc_colorspace_from_stream(ctx, source, jpx_info->icc_offset,
                                                              jpx_info->icc_length, jpx_info->comps, &dummy,
                                                              dictkey, pcs);
                if (code < 0) {
                    dmprintf2(ctx->memory,
                              "WARNING JPXDecode: Error setting icc colorspace (offset=%d,len=%d)\n",
                              jpx_info->icc_offset, jpx_info->icc_length);
                    goto cleanupExit;
                }
                *comps = gs_color_space_num_components(*pcs);
                goto cleanupExit;
            } else {
                char *color_str = NULL;

                /* TODO: These colorspace names are pulled from the gs code (jp2_csp_dict), but need
                 * to be implemented to actually work.
                 */
                backup_color_name = (char *)"DeviceRGB";
                switch(jpx_info->cs_enum) {
                case 12:
                    color_str = (char *)"DeviceCMYK";
                    break;
                case 14:
                    /* All the other colour spaces are set by name, either a device name or a
                     * 'special' internal name (same as Ghostscript) which is picked up in
                     * pdfi_create_colorspace_by_name() in pdf_colour.c. These names must
                     * match!
                     * However for Lab we need to set a White Point and its simplest just
                     * to create an appropriate color space array here.
                     */
                    code = pdfi_create_JPX_Lab(ctx, &ColorSpace);
                    if (code < 0)
                        goto cleanupExit;
                    break;
                case 16:
                    color_str = (char *)"sRGBICC";
                    break;
                case 17:
                    color_str = (char *)"sGrayICC";
                    backup_color_name = (char *)"DeviceGray";
                    break;
                case 18:
                    color_str = (char *)"DeviceRGB";
                    break;
                case 20:
                case 24:
                    color_str = (char *)"esRGBICC";
                    break;
                case 21:
                    color_str = (char *)"rommRGBICC";
                    break;
                default:
                    {
                        char extra_info[gp_file_name_sizeof];
                        /* TODO: Could try DeviceRGB instead of erroring out? */
                        gs_snprintf(extra_info, sizeof(extra_info), "**** Error: JPXDecode: Unsupported EnumCS %d\n", jpx_info->cs_enum);
                        pdfi_set_error(ctx, 0, NULL, E_PDF_IMAGECOLOR_ERROR, "pdfi_image_get_color", extra_info);
                        code = gs_note_error(gs_error_rangecheck);
                        goto cleanupExit;
                    }
                }

                /* Make a ColorSpace for the name */
                if (color_str != NULL) {
                    code = pdfi_name_alloc(ctx, (byte *)color_str, strlen(color_str), &ColorSpace);
                    if (code < 0)
                        goto cleanupExit;
                    pdfi_countup(ColorSpace);
                    using_enum_cs = true;
                }
            }
        } else {
            /* Assume DeviceRGB colorspace */
            pdfi_set_warning(ctx, 0, NULL, W_PDF_BAD_IMAGEDICT, "pdfi_image_get_color", (char *)"**** Error: image has no /ColorSpace key; assuming /DeviceRGB");
            code = pdfi_name_alloc(ctx, (byte *)"DeviceRGB", strlen("DeviceRGB"), &ColorSpace);
            if (code < 0)
                goto cleanupExit;
            pdfi_countup(ColorSpace);
        }
    } else {
        /* Override BPC from JPXDecode if applicable
         * Sample: tests_private/comparefiles/Bug695387.pdf
         */
        if (image_info->is_JPXDecode && jpx_info->is_valid)
            image_info->BPC = jpx_info->bpc;
    }

    /* At this point ColorSpace is either a string we just made, or the one from the Image */
    code = pdfi_create_colorspace(ctx, ColorSpace,
                                  image_info->stream_dict, image_info->page_dict,
                                  pcs, image_info->inline_image);
    if (code < 0) {
        dmprintf(ctx->memory, "WARNING: Image has unsupported ColorSpace ");
        if (pdfi_type_of(ColorSpace) == PDF_NAME) {
            pdf_name *name = (pdf_name *)ColorSpace;
            char str[100];
            int length = name->length;

            if (length > 0) {
                if (length > 99)
                    length = 99;

                memcpy(str, (const char *)name->data, length);
                str[length] = '\0';
                dmprintf1(ctx->memory, "NAME:%s\n", str);
            }
        } else {
            dmprintf(ctx->memory, "(not a name)\n");
        }

        /* If we were trying an enum_cs, attempt to use backup_color_name instead */
        if (using_enum_cs) {
            pdfi_countdown(ColorSpace);
            code = pdfi_name_alloc(ctx, (byte *)backup_color_name, strlen(backup_color_name), &ColorSpace);
            if (code < 0)
                goto cleanupExit;
            pdfi_countup(ColorSpace);
            /* Try to set the backup name */
            code = pdfi_create_colorspace(ctx, ColorSpace,
                                          image_info->stream_dict, image_info->page_dict,
                                          pcs, image_info->inline_image);

            if (code < 0) {
                pdfi_set_error(ctx, 0, NULL, E_PDF_IMAGECOLOR_ERROR, "pdfi_image_get_color", NULL);
                goto cleanupExit;
            }
        } else {
            pdfi_set_error(ctx, 0, NULL, E_PDF_IMAGECOLOR_ERROR, "pdfi_image_get_color", NULL);
            goto cleanupExit;
        }
    }
    *comps = gs_color_space_num_components(*pcs);

 cleanupExit:
    pdfi_countdown(ColorSpace);
    return code;
}

/* Make a fake SMask dict from a JPX SMaskInData */
static int
pdfi_make_smask_dict(pdf_context *ctx, pdf_stream *image_stream, pdfi_image_info_t *image_info,
                     int comps)
{
    int code = 0;
    pdf_dict *smask_dict = NULL;
    pdf_stream *fake_smask = NULL;
    pdf_array *array = NULL;
    pdf_array *matte = NULL;
    pdf_dict *image_dict = NULL, *dict = NULL; /* alias */

    if (image_info->SMask != NULL) {
        dmprintf(ctx->memory, "ERROR SMaskInData when there is already an SMask?\n");
        goto exit;
    }

    if (pdfi_type_of(image_stream) != PDF_STREAM) {
        code = gs_note_error(gs_error_typecheck);
        goto exit;
    }
    code = pdfi_dict_from_obj(ctx, (pdf_obj *)image_stream, &image_dict);
    if (code < 0) goto exit;

    /* Make a new stream object and a dict for it */
    code = pdfi_object_alloc(ctx, PDF_STREAM, 0, (pdf_obj **)&fake_smask);
    if (code < 0) goto exit;
    pdfi_countup(fake_smask);

    code = pdfi_dict_alloc(ctx, 32, &smask_dict);
    if (code < 0) goto exit;

    fake_smask->stream_dict = smask_dict;
    pdfi_countup(smask_dict);
    smask_dict = NULL;
    dict = fake_smask->stream_dict; /* alias */

    /* Copy everything from the image_dict */
    code = pdfi_dict_copy(ctx, dict, image_dict);
    fake_smask->stream_offset = image_stream->stream_offset;

    code = pdfi_dict_put_int(ctx, dict, "SMaskInData", 0);
    if (code < 0) goto exit;

    code = pdfi_dict_put_name(ctx, dict, "ColorSpace", "DeviceGray");
    if (code < 0) goto exit;

    /* BPC needs to come from the jpxinfo */
    code = pdfi_dict_put_int(ctx, dict, "BitsPerComponent", image_info->jpx_info.bpc);
    if (code < 0) goto exit;

    /* "Alpha" is a non-standard thing that gs code uses to tell
     * the jpxfilter that it is doing an SMask.  I guess we can do
     * the same, since we're making this dictionary anyway.
     */
    code = pdfi_dict_put_bool(ctx, dict, "Alpha", true);
    if (code < 0) goto exit;

    /* Make an array [0,1] */
    code = pdfi_array_alloc(ctx, 2, &array);
    if (code < 0) goto exit;
    pdfi_countup(array);
    code = pdfi_array_put_int(ctx, array, 0, 0);
    if (code < 0) goto exit;
    code = pdfi_array_put_int(ctx, array, 1, 1);
    if (code < 0) goto exit;
    code = pdfi_dict_put(ctx, dict, "Decode", (pdf_obj *)array);
    if (code < 0) goto exit;

    /* Make Matte array if needed */
    /* This just makes an array [0,0,0...] of size 'comps'
     * See pdf_draw.ps/makeimagekeys
     * TODO: The only sample in our test suite that triggers this path is fts_17_1718.pdf
     * and this code being there or not makes no difference on that sample, so.. ???
     */
    if (image_info->SMaskInData == 2) {
        int i;
        code = pdfi_array_alloc(ctx, comps, &matte);
        if (code < 0) goto exit;
        pdfi_countup(matte);
        for (i=0; i<comps; i++) {
            code = pdfi_array_put_int(ctx, matte, i, 0);
            if (code < 0) goto exit;
        }
        code = pdfi_dict_put(ctx, dict, "Matte", (pdf_obj *)matte);
        if (code < 0) goto exit;
    }

    image_info->SMask = (pdf_obj *)fake_smask;

 exit:
    if (code < 0) {
        pdfi_countdown(fake_smask);
        pdfi_countdown(smask_dict);
    }
    pdfi_countdown(array);
    pdfi_countdown(matte);
    return code;
}

/* NOTE: "source" is the current input stream.
 * on exit:
 *  inline_image = TRUE, stream it will point to after the image data.
 *  inline_image = FALSE, stream position undefined.
 */
static int
pdfi_do_image(pdf_context *ctx, pdf_dict *page_dict, pdf_dict *stream_dict, pdf_stream *image_stream,
              pdf_c_stream *source, bool inline_image)
{
    pdf_c_stream *new_stream = NULL, *SFD_stream = NULL;
    int code = 0, code1 = 0;
    int comps = 0;
    gs_color_space  *pcs = NULL;
    gs_image1_t t1image;
    gs_image4_t t4image;
    gs_image3_t t3image;
    gs_image3x_t t3ximage;
    gs_pixel_image_t *pim = NULL;
    pdf_stream *alt_stream = NULL;
    pdfi_image_info_t image_info, mask_info, smask_info;
    pdf_stream *mask_stream = NULL;
    pdf_stream *smask_stream = NULL; /* only non-null for imagetype 3x (PreserveSMask) */
    pdf_array *mask_array = NULL;
    unsigned char *mask_buffer = NULL;
    uint64_t mask_size = 0;
    pdfi_int_gstate *igs = (pdfi_int_gstate *)ctx->pgs->client_data;
    bool transparency_group = false;
    bool op_blend_mode = false;
    int blend_mode;
    bool need_smask_cleanup = false;
    bool maybe_jpxdecode = false;
    pdfi_trans_state_t trans_state;
    int saved_intent;
    gs_offset_t stream_offset;
    float save_strokeconstantalpha = 0.0f, save_fillconstantalpha = 0.0f;
    int trans_required;

#if DEBUG_IMAGES
    dbgmprintf(ctx->memory, "pdfi_do_image BEGIN\n");
#endif
    memset(&mask_info, 0, sizeof(mask_info));
    memset(&smask_info, 0, sizeof(mask_info));

    /* Make sure the image is a stream (which we will assume in later code) */
    if (pdfi_type_of(image_stream) != PDF_STREAM)
        return_error(gs_error_typecheck);

    if (!inline_image) {
        pdf_dict *image_dict = NULL;

        /* If we are not processing an inline image, check to see if any of the abbreviated
         * keys are present in the image dictionary. If they are, and we need to abort, we'll
         * get an error return, otherwise we can continue.
         */
        code = pdfi_dict_from_obj(ctx, (pdf_obj *)image_stream, &image_dict);
        if (code < 0)
            return code;

        code = pdfi_check_inline_image_keys(ctx, image_dict);
        if (code < 0)
            return code;
    }

    /* Save current rendering intent so we can put it back if it is modified */
    saved_intent = gs_currentrenderingintent(ctx->pgs);

    code = pdfi_get_image_info(ctx, image_stream, page_dict, stream_dict, inline_image, &image_info);
    if (code < 0)
        goto cleanupExit;

    /* Don't render this if turned off */
    if (pdfi_oc_is_off(ctx))
        goto cleanupExit;
    /* If there is an OC dictionary, see if we even need to render this */
    if (image_info.OC) {
        if (!(ctx->device_state.writepdfmarks && ctx->args.preservemarkedcontent)) {
            if (!pdfi_oc_is_ocg_visible(ctx, image_info.OC))
                goto cleanupExit;
        }
    }

    /* If there is an alternate, swap it in */
    /* If image_info.Alternates, look in the array, see if any of them are flagged as "DefaultForPrinting"
     * and if so, substitute that one for the image we are processing.
     * (it can probably be either an array, or a reference to an array, need an example to test/implement)
     * see p.274 of PDFReference.pdf
     */

    if (image_info.Alternates != NULL) {
        alt_stream = pdfi_find_alternate(ctx, image_info.Alternates);
        if (alt_stream != NULL) {
            image_stream = alt_stream;
            pdfi_free_image_info_components(&image_info);
            code = pdfi_get_image_info(ctx, image_stream, page_dict, stream_dict, inline_image, &image_info);
            if (code < 0)
                goto cleanupExit;
        }
    }

    /* Grab stream_offset after alternate has (possibly) set */
    stream_offset = pdfi_stream_offset(ctx, image_stream);

    /* See if it might be a JPXDecode image even though not in the header */
    if (image_info.ColorSpace == NULL && !image_info.ImageMask)
        maybe_jpxdecode = true;

    /* Handle JPXDecode filter pre-scan of header */
    if ((maybe_jpxdecode || image_info.is_JPXDecode) && !inline_image) {
        pdfi_seek(ctx, source, stream_offset, SEEK_SET);
        code = pdfi_scan_jpxfilter(ctx, source, image_info.Length, &image_info.jpx_info);
        if (code < 0 && image_info.is_JPXDecode)
            goto cleanupExit;

        /* I saw this JPXDecode images that have SMaskInData */
        if (image_info.jpx_info.no_data)
            image_info.is_JPXDecode = false;

        if (code == 0 && maybe_jpxdecode)
            image_info.is_JPXDecode = true;
    }

    /* Set the rendering intent if applicable */
    if (image_info.Intent) {
        code = pdfi_setrenderingintent(ctx, image_info.Intent);
        if (code < 0) {
            /* TODO: Flag a warning on this?  Sample fts_17_1706.pdf has misspelled Intent
               which gs renders without flagging an error */
#if DEBUG_IMAGES
            dbgmprintf(ctx->memory, "WARNING: Image with unexpected Intent\n");
#endif
        }
    }

    /* Get the color for this image */
    code = pdfi_image_get_color(ctx, source, &image_info, &comps, image_stream->object_num, &pcs);
    if (code < 0)
        goto cleanupExit;

    if (image_info.BPC != 1 && image_info.BPC != 2 && image_info.BPC != 4 && image_info.BPC != 8 && image_info.BPC != 16) {
        code = gs_note_error(gs_error_rangecheck);
        goto cleanupExit;
    }

    /* Set the colorspace */
    if (pcs) {
        gs_color_space  *pcs1 = pcs;

        code = pdfi_gs_setcolorspace(ctx, pcs);
        if (code < 0)
            goto cleanupExit;

        if (pcs->type->index == gs_color_space_index_Indexed)
            pcs1 = pcs->base_space;

        /* It is possible that we get no error returned from setting an
         * ICC space, but that we are not able when rendering to create a link
         * between the ICC space and the output device profile.
         * The PostScript PDF interpreter sets the colour after setting the space, which
         * (eventually) causes us to set the device colour, and that actually creates the
         * link. This is apparntly the only way we can detect this error. Otherwise we
         * would carry on until we tried to render the image, and that would fail with
         * a not terribly useful error of -1. So here we try to set the device colour,
         * for images in an ICC profile space. If that fails then we try to manufacture
         * a Device space from the number of components in the profile.
         * I do feel this is something we should be able to handle better!
         */
        if (pcs1->type->index == gs_color_space_index_ICC)
        {
            gs_client_color         cc;
            int comp = 0;
            pdf_obj *ColorSpace = NULL;

            cc.pattern = 0;
            for (comp = 0; comp < pcs1->cmm_icc_profile_data->num_comps;comp++)
                cc.paint.values[comp] = 0;

            code = gs_setcolor(ctx->pgs, &cc);
            if (code < 0)
                goto cleanupExit;

            code = gx_set_dev_color(ctx->pgs);
            if (code < 0) {
                pdfi_set_warning(ctx, 0, NULL, W_PDF_BAD_ICC_PROFILE_LINK, "pdfi_do_image", "Attempting to use profile /N to create a device colour space");
                /* Possibly we couldn't create a link profile, soemthing wrong with the ICC profile, try to use a device space */
                switch(pcs1->cmm_icc_profile_data->num_comps) {
                    case 1:
                        code = pdfi_name_alloc(ctx, (byte *)"DeviceGray", 10, &ColorSpace);
                        if (code < 0)
                            goto cleanupExit;
                        pdfi_countup(ColorSpace);
                        break;
                    case 3:
                        code = pdfi_name_alloc(ctx, (byte *)"DeviceRGB", 9, &ColorSpace);
                        if (code < 0)
                            goto cleanupExit;
                        pdfi_countup(ColorSpace);
                        break;
                    case 4:
                        code = pdfi_name_alloc(ctx, (byte *)"DeviceCMYK", 10, &ColorSpace);
                        if (code < 0)
                            goto cleanupExit;
                        pdfi_countup(ColorSpace);
                        break;
                    default:
                        code = gs_error_unknownerror;
                        goto cleanupExit;
                        break;
                }
                if (pcs != NULL)
                    rc_decrement_only_cs(pcs, "pdfi_do_image");
                /* At this point ColorSpace is either a string we just made, or the one from the Image */
                code = pdfi_create_colorspace(ctx, ColorSpace,
                                  image_info.stream_dict, image_info.page_dict,
                                  &pcs, image_info.inline_image);
                pdfi_countdown(ColorSpace);
                if (code < 0)
                    goto cleanupExit;

                code = pdfi_gs_setcolorspace(ctx, pcs);
                if (code < 0)
                    goto cleanupExit;

                /* Try to set the device color again as that failure
                   is why we are here. If it fails again, something
                   is very wrong */
                code = gx_set_dev_color(ctx->pgs);
                if (code < 0)
                    goto cleanupExit;
            }
        }
    }
    else {
        if (image_info.ImageMask == 0) {
            code = gs_note_error(gs_error_undefined);
            goto cleanupExit;
        }
    }

    /* Make a fake SMask dict if needed for JPXDecode */
    if (ctx->page.has_transparency && image_info.is_JPXDecode && image_info.SMaskInData != 0) {
        code = pdfi_make_smask_dict(ctx, image_stream, &image_info, comps);
        if (code < 0)
            goto cleanupExit;
    }

    if (ctx->page.has_transparency == true && image_info.SMask != NULL) {
        bool has_Matte = false;

        /* If this flag is set, then device will process the SMask and we need do nothing
         * here (e.g. pdfwrite).
         */
        if (!ctx->device_state.preserve_smask) {
            code = pdfi_do_image_smask(ctx, source, &image_info, &has_Matte);
            if (code < 0)
                goto cleanupExit;
            need_smask_cleanup = true;
        }

        /* If we are in an overprint situation this group will need
           to have its blend mode set to compatible overprint. */
        if (ctx->page.needs_OP) {
            if (pdfi_trans_okOPcs(ctx)) {
                if (gs_currentfilloverprint(ctx->pgs)) {
                    blend_mode = gs_currentblendmode(ctx->pgs);
                    code = gs_setblendmode(ctx->pgs, BLEND_MODE_CompatibleOverprint);
                    op_blend_mode = true;
                    if (code < 0)
                        goto cleanupExit;
                }
            }
        }

        if (has_Matte)
            code = pdfi_trans_begin_isolated_group(ctx, true, pcs);
        else
            code = pdfi_trans_begin_isolated_group(ctx, true, NULL);
        if (code < 0)
            goto cleanupExit;
        transparency_group = true;
    } else if (igs->SMask) {
        code = pdfi_trans_begin_isolated_group(ctx, false, NULL);
        if (code < 0)
            goto cleanupExit;
        transparency_group = true;
    }

    if (transparency_group && !ctx->device_state.preserve_smask) {
        save_strokeconstantalpha = gs_getstrokeconstantalpha(ctx->pgs);
        save_fillconstantalpha = gs_getfillconstantalpha(ctx->pgs);
        gs_setstrokeconstantalpha(ctx->pgs, 1.0);
        gs_setfillconstantalpha(ctx->pgs, 1.0);
    }

    /* Get the Mask data either as an array or a dict, if present */
    if (image_info.Mask != NULL) {
        switch (pdfi_type_of(image_info.Mask)) {
            case PDF_ARRAY:
                mask_array = (pdf_array *)image_info.Mask;
                break;
            case PDF_STREAM:
                mask_stream = (pdf_stream *)image_info.Mask;
                code = pdfi_get_image_info(ctx, mask_stream, page_dict,
                                           stream_dict, inline_image, &mask_info);
                if (code < 0)
                    goto cleanupExit;
                break;
            default:
                pdfi_countdown(image_info.Mask);
                image_info.Mask = NULL;
                pdfi_set_warning(ctx, 0, NULL, W_PDF_MASK_ERROR, "pdfi_do_image", NULL);
        }
    }

    /* Get the SMask info if we will need it (Type 3x images) */
    if (image_info.SMask && pdfi_type_of(image_info.SMask) == PDF_STREAM && ctx->device_state.preserve_smask) {
        /* smask_dict non-NULL is used to flag a Type 3x image below */
        smask_stream = (pdf_stream *)image_info.SMask;
        code = pdfi_get_image_info(ctx, smask_stream, page_dict, stream_dict,
                                   inline_image, &smask_info);
        if (code < 0)
            goto cleanupExit;
    }

    /* Get the image into a supported gs type (type1, type3, type4, type3x) */
    if (!image_info.Mask && !smask_stream) { /* Type 1 and ImageMask */
        memset(&t1image, 0, sizeof(t1image));
        pim = (gs_pixel_image_t *)&t1image;

        if (image_info.ImageMask) {
            /* Sets up timage.ImageMask, amongst other things */
            gs_image_t_init_adjust(&t1image, NULL, false);
        } else {
            gs_image_t_init_adjust(&t1image, pcs, true);
        }
    } else if (smask_stream) { /* Type 3x */
        /* In the event where we have both /SMask and /Mask, favour /SMask
           See tests_private/pdf/sumatra/1901_-_tiling_inconsistencies.pdf
         */
        mask_stream = NULL;
        code = pdfi_image_setup_type3x(ctx, &image_info, &t3ximage, &smask_info, comps);
        if (code < 0) {
            /* If this got an error, setup as a Type 1 image */
            /* NOTE: I did this error-handling the same as for Type 4 image below.
             * Dunno if it's better to do this or to just abort the whole image?
             */
            memset(&t1image, 0, sizeof(t1image));
            pim = (gs_pixel_image_t *)&t1image;
            gs_image_t_init_adjust(&t1image, pcs, true);
        } else {
            pim = (gs_pixel_image_t *)&t3ximage;
        }
    } else {
        if (mask_array) { /* Type 4 */
            code = pdfi_image_setup_type4(ctx, &image_info, &t4image, mask_array, pcs);
            if (code < 0) {
                /* If this got an error, setup as a Type 1 image */
                memset(&t1image, 0, sizeof(t1image));
                pim = (gs_pixel_image_t *)&t1image;
                gs_image_t_init_adjust(&t1image, pcs, true);
            } else {
                pim = (gs_pixel_image_t *)&t4image;
            }
        } else { /* Type 3 */
            memset(&t3image, 0, sizeof(t3image));
            pim = (gs_pixel_image_t *)&t3image;
            gs_image3_t_init(&t3image, NULL, interleave_separate_source);
            code = pdfi_data_image_params(ctx, &mask_info, &t3image.MaskDict, 1, NULL);
            if (code < 0)
                goto cleanupExit;
        }
    }


    /* At this point pim points to a structure containing the specific type
     * of image, and then we can handle it generically from here.
     * The underlying gs image functions will do different things for different
     * types of images.
     */

    /* Setup the common params */
    pim->ColorSpace = pcs;
    code = pdfi_data_image_params(ctx, &image_info, (gs_data_image_t *)pim, comps, pcs);
    if (code < 0)
        goto cleanupExit;

    /* Grab the mask_image data buffer in advance.
     * Doing it this way because I don't want to muck with reading from
     * two streams simultaneously -- not even sure that is feasible?
     */
    if (smask_stream) {
        mask_size = ((((smask_info.Width * smask_info.BPC) + 7) / 8) * smask_info.Height);
        /* This will happen only in case of PreserveSMask (Type 3x) */
        code = pdfi_stream_to_buffer(ctx, smask_stream, &mask_buffer, (int64_t *)&mask_size);
        if (code < 0)
            goto cleanupExit;
    } else if (mask_stream) {
        /* Calculate expected mask size */
        mask_size = ((((t3image.MaskDict.BitsPerComponent * (int64_t)t3image.MaskDict.Width) + 7) / 8) * (int64_t)t3image.MaskDict.Height);
        code = pdfi_stream_to_buffer(ctx, mask_stream, &mask_buffer, (int64_t *)&mask_size);
        if (code < 0)
            goto cleanupExit;
    }
    /* Setup the data stream for the image data */
    if (!inline_image) {
        pdfi_seek(ctx, source, stream_offset, SEEK_SET);

        code = pdfi_apply_SubFileDecode_filter(ctx, 0, "endstream", source, &SFD_stream, false);
        if (code < 0)
            goto cleanupExit;
        source = SFD_stream;
    }

    code = pdfi_filter(ctx, image_stream, source, &new_stream, inline_image);
    if (code < 0)
        goto cleanupExit;

    /* This duplicates the code in gs_img.ps; if we have an imagemask, with 1 bit per component (is there any other kind ?)
     * and the image is to be interpolated, and we are nto sending it to a high level device. Then check the scaling.
     * If we are scaling up (in device space) by afactor of more than 2, then we install the ImScaleDecode filter,
     * which interpolates the input data by a factor of 4.
     * The scaling of 2 is arbitrary (says so in gs_img.ps) but we use it for consistency. The scaling of the input
     * by 4 is just a magic number, the scaling is always by 4, and we need to know it so we can adjust the Image Matrix
     * and Width and Height values.
     */
    if (image_info.ImageMask == 1 && image_info.BPC == 1 && image_info.Interpolate == 1 && !ctx->device_state.HighLevelDevice)
    {
        pdf_c_stream *s = new_stream;
        gs_matrix mat4 = {4, 0, 0, 4, 0, 0}, inverseIM;
        gs_point pt, pt1;
        float s1, s2;

        code = gs_matrix_invert(&pim->ImageMatrix, &inverseIM);
        if (code < 0)
            goto cleanupExit;

        code = gs_distance_transform(0, 1, &inverseIM, &pt);
        if (code < 0)
            goto cleanupExit;

        code = gs_distance_transform(pt.x, pt.y, &ctm_only(ctx->pgs), &pt1);
        if (code < 0)
            goto cleanupExit;

        s1 = sqrt(pt1.x * pt1.x + pt1.y * pt1.y);

        code = gs_distance_transform(1, 0, &inverseIM, &pt);
        if (code < 0)
            goto cleanupExit;

        code = gs_distance_transform(pt.x, pt.y, &ctm_only(ctx->pgs), &pt1);
        if (code < 0)
            goto cleanupExit;

        s2 = sqrt(pt1.x * pt1.x + pt1.y * pt1.y);

        if (s1 > 2.0 || s2 > 2.0) {
            code = pdfi_apply_imscale_filter(ctx, 0, image_info.Width, image_info.Height, s, &new_stream);
            if (code < 0)
                goto cleanupExit;
            /* This adds the filter to the 'chain' of filter we created. When we close this filter
             * it closes all the filters in the chain, back to either the SubFileDecode filter or the
             * original main stream. If we don't patch this up then we leak memory for any filters
             * applied in pdfi_filter above.
             */
            new_stream->original = s->original;

            /* We'e created a new 'new_stream', which is a C stream, to hold the filter chain
             * but we still need to free the original C stream 'wrapper' we created with pdfi_filter()
             * Do that now.
             */
            gs_free_object(ctx->memory, s, "free stream replaced by adding image scaling filter");
            image_info.Width *= 4;
            image_info.Height *= 4;
            pim->Width *= 4;
            pim->Height *= 4;
            code = gs_matrix_multiply(&pim->ImageMatrix, &mat4, &pim->ImageMatrix);
            if (code < 0)
                goto cleanupExit;
        }
    }

    trans_required = pdfi_trans_required(ctx);

    if (trans_required) {
        code = pdfi_image_setup_trans(ctx, &trans_state);
        if (code < 0)
            goto cleanupExit;
    }

    /* Render the image */
    code = pdfi_render_image(ctx, pim, new_stream,
                             mask_buffer, mask_size,
                             comps, image_info.ImageMask);
    if (code < 0) {
        if (ctx->args.pdfdebug)
            dmprintf1(ctx->memory, "WARNING: pdfi_do_image: error %d from pdfi_render_image\n", code);
    }

    if (trans_required) {
        code1 = pdfi_trans_teardown(ctx, &trans_state);
        if (code == 0)
            code = code1;
    }

 cleanupExit:
    if (code < 0)
        pdfi_set_warning(ctx, code, NULL, W_PDF_IMAGE_ERROR, "pdfi_do_image", NULL);

    code = 0;  /* suppress errors */

    if (transparency_group) {
        if (!ctx->device_state.preserve_smask) {
            gs_setstrokeconstantalpha(ctx->pgs, save_strokeconstantalpha);
            gs_setfillconstantalpha(ctx->pgs, save_fillconstantalpha);
        }
        pdfi_trans_end_isolated_group(ctx);
        if (need_smask_cleanup)
            pdfi_trans_end_smask_notify(ctx);
    }

    if (op_blend_mode) {
        code = gs_setblendmode(ctx->pgs, blend_mode);
    }

    if (new_stream)
        pdfi_close_file(ctx, new_stream);
    if (SFD_stream)
        pdfi_close_file(ctx, SFD_stream);
    if (mask_buffer)
        gs_free_object(ctx->memory, mask_buffer, "pdfi_do_image (mask_buffer)");

    pdfi_countdown(alt_stream);

    pdfi_free_image_info_components(&image_info);
    pdfi_free_image_info_components(&mask_info);
    pdfi_free_image_info_components(&smask_info);

    if (pcs != NULL)
        rc_decrement_only_cs(pcs, "pdfi_do_image");

    /* Restore the rendering intent */
    gs_setrenderingintent(ctx->pgs, saved_intent);

#if DEBUG_IMAGES
    dbgmprintf(ctx->memory, "pdfi_do_image END\n");
#endif
    return code;
}

int pdfi_ID(pdf_context *ctx, pdf_dict *stream_dict, pdf_dict *page_dict, pdf_c_stream *source)
{
    pdf_dict *d = NULL;
    int code;
    pdf_stream *image_stream;

    if (ctx->text.BlockDepth != 0)
        pdfi_set_warning(ctx, 0, NULL, W_PDF_OPINVALIDINTEXT, "pdfi_ID", NULL);

    /* we want to have the indirect_num and indirect_gen of the created dictionary
     * be 0, because we are reading from a stream, and the stream has already
     * been decrypted, we don't need to decrypt any strings contained in the
     * inline dictionary.
     */
    code = pdfi_dict_from_stack(ctx, 0, 0, false);
    if (code < 0)
        /* pdfi_dict_from_stack cleans up the stack so we don't need to in case of an error */
        return code;

    d = (pdf_dict *)ctx->stack_top[-1];
    pdfi_countup(d);
    pdfi_pop(ctx, 1);

    code = pdfi_obj_dict_to_stream(ctx, d, &image_stream, true);
    if (code < 0)
        goto error;

    code = pdfi_do_image(ctx, page_dict, stream_dict, image_stream, source, true);
error:
    pdfi_countdown(image_stream);
    pdfi_countdown(d);
    return code;
}

int pdfi_EI(pdf_context *ctx)
{
    if (ctx->text.BlockDepth != 0)
        pdfi_set_warning(ctx, 0, NULL, W_PDF_OPINVALIDINTEXT, "pdfi_EI", NULL);

/*    pdfi_clearstack(ctx);*/
    return 0;
}

/* see .execgroup */
int pdfi_form_execgroup(pdf_context *ctx, pdf_dict *page_dict, pdf_stream *xobject_obj,
                        gs_gstate *GroupGState, gs_color_space *pcs, gs_client_color *pcc, gs_matrix *matrix)
{
    int code;
    pdfi_int_gstate *igs = (pdfi_int_gstate *)ctx->pgs->client_data;

    code = pdfi_gsave(ctx);
    if (code < 0)
        goto exit;

    if (GroupGState) {
        code = pdfi_gs_setgstate(ctx->pgs, GroupGState);
        if (code < 0)
            goto exit2;
    }

    /* Override the colorspace if specified */
    if (pcs) {
        code = pdfi_gs_setcolorspace(ctx, pcs);
        if (code < 0)
            goto exit2;
        code = gs_setcolor(ctx->pgs, (const gs_client_color *)pcc);
        if (code < 0)
            goto exit2;
    }

    /* Disable the SMask */
    pdfi_gstate_smask_free(igs);

    gs_setblendmode(ctx->pgs, BLEND_MODE_Compatible);
    gs_setstrokeconstantalpha(ctx->pgs, 1.0);
    gs_setfillconstantalpha(ctx->pgs, 1.0);

    if (matrix)
        code = gs_concat(ctx->pgs, matrix);
    if (code < 0) {
        goto exit2;
    }
    code = pdfi_run_context(ctx, xobject_obj, page_dict, false, "FORM");

 exit2:
    if (code != 0)
        (void)pdfi_grestore(ctx);
    else
        code = pdfi_grestore(ctx);
 exit:
    return code;
}

/* See zbeginform() */
static int pdfi_form_highlevel_begin(pdf_context *ctx, pdf_dict *form_dict, gs_matrix *CTM,
                                     gs_rect *BBox, gs_matrix *form_matrix)
{
    int code = 0;
    gx_device *cdev = gs_currentdevice_inline(ctx->pgs);
    gs_form_template_t template;
    gs_point ll, ur;
    gs_fixed_rect box;

    memset(&template, 0, sizeof(template));

    template.CTM = *CTM; /* (structure copy) */
    template.BBox = *BBox; /* (structure copy) */
    template.form_matrix = *form_matrix; /* (structure copy) */
    template.FormID = -1;
    template.pcpath = ctx->pgs->clip_path;
    template.pgs = ctx->pgs;

    code = dev_proc(cdev, dev_spec_op)(cdev, gxdso_form_begin, &template, 0);
    /* return value > 0 means the device sent us back a matrix
     * and wants the CTM set to that.
     * TODO: No idea if we need this nonsense.  See zbeginform()
     */
    if (code > 0)
    {
        gs_setmatrix(ctx->pgs, &template.CTM);
        gs_distance_transform(template.BBox.p.x, template.BBox.p.y, &template.CTM, &ll);
        gs_distance_transform(template.BBox.q.x, template.BBox.q.y, &template.CTM, &ur);

        /* A form can legitimately have negative co-ordinates in paths
         * because it can be translated. But we always clip paths to the
         * page which (clearly) can't have negative co-ordinates. NB this
         * wouldn't be a problem if we didn't reset the CTM, but that would
         * break the form capture.
         * So here we temporarily set the clip to permit negative values,
         * fortunately this works.....
         */
        /* We choose to permit negative values of the same magnitude as the
         * positive ones.
         */

        box.p.x = float2fixed(ll.x);
        box.p.y = float2fixed(ll.y);
        box.q.x = float2fixed(ur.x);
        box.q.y = float2fixed(ur.y);

        if (box.p.x < 0) {
            if(box.p.x * -1 > box.q.x)
                box.q.x = box.p.x * -1;
        } else {
            if (fabs(ur.x) > fabs(ll.x))
                box.p.x = box.q.x * -1;
            else {
                box.p.x = float2fixed(ll.x * -1);
                box.q.x = float2fixed(ll.x);
            }
        }
        if (box.p.y < 0) {
            if(box.p.y * -1 > box.q.y)
                box.q.y = box.p.y * -1;
        } else {
            if (fabs(ur.y) > fabs(ll.y))
                box.p.y = box.q.y * -1;
            else {
                box.p.y = float2fixed(ll.y * -1);
                box.q.y = float2fixed(ll.y);
            }
        }
        /* This gets undone when we grestore after the form is executed */
        code = gx_clip_to_rectangle(ctx->pgs, &box);
    }

    return code;
}

/* See zendform() */
static int pdfi_form_highlevel_end(pdf_context *ctx)
{
    int code = 0;
    gx_device *cdev = gs_currentdevice_inline(ctx->pgs);

    code = dev_proc(cdev, dev_spec_op)(cdev, gxdso_form_end, 0, 0);
    return code;
}

/* See bug #702560. The original file has a Form XObject which is not a stream. Instead
 * the Form XObject has a /Contents key which points to a stream dictionary. This is plainly
 * illegal but, as always, Acrobat can open it....
 * If PDFSTOPONERROR is true then we just exit. Otherwise we look for a /Contents key in the stream
 * dictionary. If we find one we dereference the object to get a stream dictionary, then merge the
 * two dictionaries, ensuring the stream offset is correct, and proceed as if that's what we'd
 * always had. If we don't have a /Contents key then exit with a typecheck error.
 *
 * Sets *hacked_stream to the /Contents stream if it found one.
 */
static int pdfi_form_stream_hack(pdf_context *ctx, pdf_dict *form_dict, pdf_stream **hacked_stream)
{
    int code = 0;
    pdf_stream *stream_obj = NULL;

    *hacked_stream = NULL;

    if (pdfi_type_of(form_dict) == PDF_STREAM)
        return 0;

    if (!ctx->args.pdfstoponerror) {
        pdf_obj *Parent = NULL;
        pdf_dict *d = NULL;
        pdf_dict *stream_dict = NULL;

        code = pdfi_dict_knownget_type(ctx, form_dict, "Contents", PDF_STREAM,
                                       (pdf_obj **)&stream_obj);
        if (code < 0 || stream_obj == NULL) {
            pdfi_set_error(ctx, 0, NULL, E_PDF_BADSTREAMDICT, "pdfi_form_stream_hack", NULL);
            code = gs_note_error(gs_error_typecheck);
            goto exit;
        }

        d = form_dict;
        pdfi_countup(d);
        do {
            code = pdfi_dict_knownget(ctx, d, "Parent", (pdf_obj **)&Parent);
            if (code > 0 && pdfi_type_of(Parent) == PDF_DICT) {
                if (Parent->object_num == stream_obj->object_num) {
                    pdfi_countdown(d);
                    pdfi_countdown(Parent);
                    pdfi_set_error(ctx, 0, NULL, E_PDF_BADSTREAMDICT, "pdfi_form_stream_hack", NULL);
                    code = gs_note_error(gs_error_undefined);
                    goto exit;
                }
                pdfi_countdown(d);
                d = (pdf_dict *)Parent;
            } else {
                pdfi_countdown(d);
                break;
            }
        } while (1);

        code = pdfi_dict_from_obj(ctx, (pdf_obj *)stream_obj, &stream_dict);
        if (code < 0) {
            pdfi_set_error(ctx, 0, NULL, E_PDF_BADSTREAMDICT, "pdfi_form_stream_hack", NULL);
            goto exit;
        }
        pdfi_set_warning(ctx, 0, NULL, W_PDF_STREAM_HAS_CONTENTS, "pdfi_form_stream_hack", NULL);
        code = pdfi_merge_dicts(ctx, stream_dict, form_dict);
        /* Having merged the dictionaries, we don't want the Contents key in the stream dict.
         * We do want to leave it in the form dictionary, in case we use this form again.
         * Leaving the reference in the stream dicttionary leads to a reference counting problem
         * because stream_dict is contained in stream_obj so stream_obj becomes self-referencing.
         */
        pdfi_dict_delete(ctx, stream_dict, "Contents");
    } else {
        pdfi_set_error(ctx, 0, NULL, E_PDF_BADSTREAMDICT, "pdfi_form_stream_hack", NULL);
        code = gs_note_error(gs_error_typecheck);
    }
    if (code == 0) {
        *hacked_stream = stream_obj;
        pdfi_countup(stream_obj);
    }

 exit:
    pdfi_countdown(stream_obj);
    return code;
}

static int pdfi_do_form(pdf_context *ctx, pdf_dict *page_dict, pdf_stream *form_obj)
{
    int code, code1 = 0;
    bool group_known = false, known = false;
    bool do_group = false;
    pdf_array *FormMatrix = NULL;
    gs_matrix formmatrix, CTM;
    gs_rect bbox;
    pdf_array *BBox = NULL;
    bool save_PreservePDFForm;
    pdf_stream *form_stream = NULL; /* Alias */
    pdf_stream *hacked_stream = NULL;
    pdf_dict *form_dict;
    gs_color_space *pcs = NULL;
    gs_client_color cc, *pcc;

#if DEBUG_IMAGES
    dbgmprintf(ctx->memory, "pdfi_do_form BEGIN\n");
#endif
    if (pdfi_type_of(form_obj) != PDF_STREAM) {
        code = pdfi_form_stream_hack(ctx, (pdf_dict *)form_obj, &hacked_stream);
        if (code < 0)
            return code;
        form_stream = hacked_stream;
    } else
        form_stream = (pdf_stream *)form_obj;

    code = pdfi_dict_from_obj(ctx, (pdf_obj *)form_stream, &form_dict);
    if (code < 0)
        goto exit;

    code = pdfi_dict_known(ctx, form_dict, "Group", &group_known);
    if (code < 0)
        goto exit;
    if (group_known && ctx->page.has_transparency)
        do_group = true;

    /* Grab the CTM before it gets modified */
    code = gs_currentmatrix(ctx->pgs, &CTM);
    if (code < 0) goto exit1;

    code = pdfi_op_q(ctx);
    if (code < 0) goto exit1;

    code = pdfi_dict_knownget_type(ctx, form_dict, "Matrix", PDF_ARRAY, (pdf_obj **)&FormMatrix);
    if (code < 0) goto exit1;

    code = pdfi_array_to_gs_matrix(ctx, FormMatrix, &formmatrix);
    if (code < 0) goto exit1;

    code = pdfi_dict_known(ctx, form_dict, "BBox", &known);
    if (known) {
        code = pdfi_dict_get_type(ctx, form_dict, "BBox", PDF_ARRAY, (pdf_obj **)&BBox);
        if (code < 0) goto exit1;
    } else {
        pdfi_set_error(ctx, 0, NULL, E_PDF_MISSING_BBOX, "pdfi_do_form", "");
        if (ctx->args.pdfstoponerror) {
            code = gs_note_error(gs_error_undefined);
            goto exit;
        }
    }

    code = pdfi_array_to_gs_rect(ctx, BBox, &bbox);
    if (code < 0) goto exit1;

    if (bbox.q.x - bbox.p.x == 0.0 || bbox.q.y - bbox.p.y == 0.0) {
        pdfi_set_warning(ctx, 0, NULL, W_PDF_FORM_CLIPPEDOUT, "pdfi_do_form", "");
        code = 0;
        if (ctx->PreservePDFForm) {
            code = pdfi_form_highlevel_begin(ctx, form_dict, &CTM, &bbox, &formmatrix);
            if (code >= 0)
                code = pdfi_form_highlevel_end(ctx);
        }
        goto exit1;
    }

    code = gs_concat(ctx->pgs, &formmatrix);
    if (code < 0) goto exit1;

    code = gs_rectclip(ctx->pgs, &bbox, 1);
    if (code < 0) goto exit1;

    if (ctx->PreservePDFForm) {
        code = pdfi_form_highlevel_begin(ctx, form_dict, &CTM, &bbox, &formmatrix);
        if (code < 0) goto exit1;
    }
    save_PreservePDFForm = ctx->PreservePDFForm;
    ctx->PreservePDFForm = false; /* Turn off in case there are any sub-forms */

    if (do_group) {
        code = pdfi_loop_detector_mark(ctx);
        if (code < 0) goto exit1;

        /* Save the current color space in case it gets changed */
        pcs = gs_currentcolorspace(ctx->pgs);
        rc_increment(pcs);
        pcc = (gs_client_color *)gs_currentcolor(ctx->pgs);
        cc = *pcc;

        code = pdfi_trans_begin_form_group(ctx, page_dict, form_dict);
        (void)pdfi_loop_detector_cleartomark(ctx);
        if (code < 0) goto exit1;

        code = pdfi_form_execgroup(ctx, page_dict, form_stream, NULL, pcs, &cc, NULL);
        code1 = pdfi_trans_end_group(ctx);
        if (code == 0) code = code1;
    } else {
        bool saved_decrypt_strings = ctx->encryption.decrypt_strings;

        /* We can run a Form even when we aren't running a page content stresm,
         * eg for an annotation, and we need to *not* decrypt strings in that
         * case (the content stream will be decrypted and strings in content
         * streams are not additionally encrypted).
         */
        ctx->encryption.decrypt_strings = false;
        code = pdfi_run_context(ctx, form_stream, page_dict, false, "FORM");
        ctx->encryption.decrypt_strings = saved_decrypt_strings;
    }

    ctx->PreservePDFForm = save_PreservePDFForm;
    if (ctx->PreservePDFForm) {
        code = pdfi_form_highlevel_end(ctx);
    }

 exit1:
    code1 = pdfi_op_Q(ctx);
    if (code == 0) code = code1;

 exit:
    pdfi_countdown(FormMatrix);
    pdfi_countdown(BBox);
    pdfi_countdown(hacked_stream);
    if (pcs)
        rc_decrement_only_cs(pcs, "pdfi_do_form(pcs)");
#if DEBUG_IMAGES
    dbgmprintf(ctx->memory, "pdfi_do_form END\n");
#endif
    if (code < 0)
        return code;
    return 0;
}

int pdfi_do_highlevel_form(pdf_context *ctx, pdf_dict *page_dict, pdf_stream *form_stream)
{
    int code = 0;

    ctx->PreservePDFForm = true;
    code = pdfi_do_form(ctx, page_dict, form_stream);
    ctx->PreservePDFForm = false;

    return code;
}

int pdfi_do_image_or_form(pdf_context *ctx, pdf_dict *stream_dict,
                                 pdf_dict *page_dict, pdf_obj *xobject_obj)
{
    int code;
    pdf_name *n = NULL;
    pdf_dict *xobject_dict;
    bool known = false;

    code = pdfi_dict_from_obj(ctx, xobject_obj, &xobject_dict);
    if (code < 0)
        return code;

    /* Check Optional Content status */
    code = pdfi_dict_known(ctx, xobject_dict, "OC", &known);
    if (code < 0)
        return code;

    if (known) {
        pdf_dict *OCDict = NULL;
        bool visible = false;
        gx_device *cdev = gs_currentdevice_inline(ctx->pgs);

        code = pdfi_dict_get(ctx, xobject_dict, "OC", (pdf_obj **)&OCDict);
        if (code < 0)
            return code;

        if (pdfi_type_of(OCDict) == PDF_DICT) {
            if (ctx->device_state.writepdfmarks && ctx->args.preservemarkedcontent) {
                code = pdfi_pdfmark_dict(ctx, OCDict);
                if (code < 0)
                    pdfi_set_warning(ctx, 0, NULL, W_PDF_DO_OC_FAILED, "pdfi_do_image_or_form", NULL);
                code = dev_proc(cdev, dev_spec_op)(cdev, gxdso_pending_optional_content, &OCDict->object_num, 0);
                if (code < 0)
                    pdfi_set_warning(ctx, 0, NULL, W_PDF_DO_OC_FAILED, "pdfi_do_image_or_form", NULL);
            } else {
                visible = pdfi_oc_is_ocg_visible(ctx, OCDict);
                if (!visible) {
                    pdfi_countdown(OCDict);
                    return 0;
                }
            }
        } else {
            pdfi_set_warning(ctx, 0, NULL, W_PDF_BAD_OCDICT, "pdfi_do_image_or_form", NULL);
        }
        pdfi_countdown(OCDict);
    }

#if DEBUG_IMAGES
    dbgmprintf1(ctx->memory, "pdfi_do_image_or_form BEGIN (OBJ = %d)\n", xobject_obj->object_num);
#endif
    code = pdfi_trans_set_params(ctx);
    if (code < 0)
        return code;

    code = pdfi_dict_get(ctx, xobject_dict, "Subtype", (pdf_obj **)&n);
    if (code < 0) {
        if (code == gs_error_undefined) {
            int code1 = 0;
            /* This is illegal, because we have no way to tell is an XObject is a Form
             * or Image object. However it seems Acrobat just assumes that it's a Form!
             * See test file /tests_private/pdf/PDFIA1.7_SUBSET/CATX2063.pdf
             */
            code1 = pdfi_name_alloc(ctx, (byte *)"Form", 4, (pdf_obj **)&n);
            if (code1 == 0) {
                pdfi_countup(n);
                pdfi_set_error(ctx, 0, NULL, E_PDF_NO_SUBTYPE, "pdfi_do_image_or_form", "Assuming a Form XObject");
            }
            if (ctx->args.pdfstoponerror)
                goto exit;
        }
        else
            goto exit;
    }
    if (pdfi_type_of(n) != PDF_NAME) {
        code = gs_note_error(gs_error_typecheck);
        goto exit;
    }

    if (pdfi_name_is(n, "Image")) {
        gs_offset_t savedoffset;

        if (pdfi_type_of(xobject_obj) != PDF_STREAM) {
            code = gs_note_error(gs_error_typecheck);
            goto exit;
        }
        savedoffset = pdfi_tell(ctx->main_stream);
        code = pdfi_do_image(ctx, page_dict, stream_dict, (pdf_stream *)xobject_obj,
                             ctx->main_stream, false);
        pdfi_seek(ctx, ctx->main_stream, savedoffset, SEEK_SET);
    } else if (pdfi_name_is(n, "Form")) {
        /* In theory a Form must be a stream, but we don't check that here
         * because there is a broken case where it can be a dict.
         * So pdfi_do_form() will handle that crazy case if it's not actually a stream.
         */
        code = pdfi_do_form(ctx, page_dict, (pdf_stream *)xobject_obj);
    } else if (pdfi_name_is(n, "PS")) {
        pdfi_set_error(ctx, 0, NULL, E_PDF_PS_XOBJECT_IGNORED, "pdfi_do_image_or_form", "");
        if (ctx->args.pdfstoponerror)
            code = gs_note_error(gs_error_typecheck);
        else
            code = 0; /* Swallow silently */
    } else {
        code = gs_error_typecheck;
    }

 exit:
    pdfi_countdown(n);
#if DEBUG_IMAGES
    dbgmprintf(ctx->memory, "pdfi_do_image_or_form END\n");
#endif
    return code;
}

int pdfi_Do(pdf_context *ctx, pdf_dict *stream_dict, pdf_dict *page_dict)
{
    int code = 0;
    pdf_name *n = NULL;
    pdf_obj *o = NULL;
    pdf_dict *sdict = NULL;
    bool known = false, AddedParent = false;

    if (pdfi_count_stack(ctx) < 1) {
        code = gs_note_error(gs_error_stackunderflow);
        goto exit1;
    }
    n = (pdf_name *)ctx->stack_top[-1];
    pdfi_countup(n);
    pdfi_pop(ctx, 1);

    if (pdfi_type_of(n) != PDF_NAME) {
        code = gs_note_error(gs_error_typecheck);
        goto exit1;
    }

    if (ctx->text.BlockDepth != 0)
        pdfi_set_warning(ctx, 0, NULL, W_PDF_OPINVALIDINTEXT, "pdfi_Do", NULL);

    code = pdfi_loop_detector_mark(ctx);
    if (code < 0)
        goto exit1;
    code = pdfi_find_resource(ctx, (unsigned char *)"XObject", n, (pdf_dict *)stream_dict, page_dict, &o);
    if (code < 0)
        goto exit;

    if (pdfi_type_of(o) != PDF_STREAM && pdfi_type_of(o) != PDF_DICT) {
        code = gs_note_error(gs_error_typecheck);
        goto exit;
    }

    /* This doesn't count up the stream dictionary, so we don't need to count it down later */
    code = pdfi_dict_from_obj(ctx, o, &sdict);
    if (code < 0)
        goto exit;

    code = pdfi_dict_known(ctx, sdict, "Parent", &known);
    if (code < 0)
        goto exit;
    /* Add a Parent ref, unless it happens to be a circular reference
     * (sample Bug298226.pdf -- has a circular ref and adding the Parent caused memory leak)
     */
    if (!known && sdict->object_num != stream_dict->object_num) {
        code = pdfi_dict_put(ctx, sdict, "Parent", (pdf_obj *)stream_dict);
        if (code < 0)
            goto exit;
        pdfi_countup(sdict);
        AddedParent = true;
    }

    (void)pdfi_loop_detector_cleartomark(ctx);
    /* NOTE: Used to have a pdfi_gsave/pdfi_grestore around this, but it actually makes
     * things render incorrectly (and isn't in the PS code).
     * It also causes demo.ai.pdf to crash.
     * I don't really understand... (all transparency related, though, so nothing surprises me...)
     * (there are some q/Q and gsave/grestore in the code under this)
     *
     * Original Comment:
     * The image or form might change the colour space (or indeed other aspects
     * of the graphics state, if its a Form XObject. So gsave/grestore round it
     * to prevent unexpected changes.
     */
    //    pdfi_gsave(ctx);
    code = pdfi_do_image_or_form(ctx, stream_dict, page_dict, o);
    //    pdfi_grestore(ctx);
    pdfi_countdown(n);
    pdfi_countdown(o);
    if (AddedParent == true) {
        if (code >= 0)
            code = pdfi_dict_delete(ctx, sdict, "Parent");
        else
            (void)pdfi_dict_delete(ctx, sdict, "Parent");
        pdfi_countdown(sdict);
    }
    return code;

exit:
    (void)pdfi_loop_detector_cleartomark(ctx);
exit1:
    pdfi_countdown(n);
    pdfi_countdown(o);
    return code;
}
