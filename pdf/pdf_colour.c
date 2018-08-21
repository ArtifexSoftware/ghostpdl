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

/* colour operations for the PDF interpreter */

#include "pdf_int.h"
#include "pdf_colour.h"
#include "pdf_stack.h"
#include "pdf_array.h"
#include "gsicc_manage.h"

#include "pdf_file.h"
#include "stream.h"

/* Forward definitions for a routine we need */
static int pdfi_create_colorspace_by_name(pdf_context *ctx, pdf_name *name, pdf_dict *stream_dict, pdf_dict *page_dict, gs_color_space **ppcs);

/* Rendering intent is a bit of an oddity, but its clearly colour related, so we
 * deal with it here. Cover it first to get it out of the way.
 */
int pdfi_ri(pdf_context *ctx)
{
    pdf_name *n;
    int code;

    if (ctx->stack_top - ctx->stack_bot < 1) {
        pdfi_clearstack(ctx);
        if(ctx->pdfstoponerror)
            return_error(gs_error_stackunderflow);
        return 0;
    }

    if (ctx->stack_top[-1]->type != PDF_NAME) {
        pdfi_pop(ctx, 1);
        if (ctx->pdfstoponerror)
            return_error(gs_error_typecheck);
        return 0;
    }
    n = (pdf_name *)ctx->stack_top[-1];
    if (n->length == 10) {
        if (memcmp(n->data, "Perceptual", 10) == 0)
            code = gs_setrenderingintent(ctx->pgs, 0);
        else {
            if (memcmp(n->data, "Saturation", 10) == 0)
                code = gs_setrenderingintent(ctx->pgs, 2);
            else
                code = gs_error_undefined;
        }
    } else {
        if (n->length == 20) {
            if (memcmp(n->data, "RelativeColorimetric", 20) == 0)
                code = gs_setrenderingintent(ctx->pgs, 1);
            else {
                if (memcmp(n->data, "AbsoluteColoimetric", 20) == 0)
                    code = gs_setrenderingintent(ctx->pgs, 3);
                else
                    code = gs_error_undefined;
            }
        } else {
            code = gs_error_undefined;
        }
    }
    pdfi_pop(ctx, 1);
    if (code < 0 && ctx->pdfstoponerror)
        return code;
    return 0;
}

/* Start with the simple cases, where we set the colour space and colour in a single operation */
int pdfi_setgraystroke(pdf_context *ctx)
{
    pdf_num *n1;
    int code;
    double d1;

    if (ctx->stack_top - ctx->stack_bot < 1) {
        if (ctx->pdfstoponerror)
            return_error(gs_error_stackunderflow);
        pdfi_clearstack(ctx);
        return 0;
    }

    n1 = (pdf_num *)ctx->stack_top[-1];
    if (n1->type == PDF_INT){
        d1 = (double)n1->value.i;
    } else{
        if (n1->type == PDF_REAL) {
            d1 = n1->value.d;
        } else {
            if (ctx->pdfstoponerror)
                return_error(gs_error_typecheck);
            else {
                pdfi_pop(ctx, 1);
                return 0;
            }
        }
    }
    code = gs_setgray(ctx->pgs, d1);
    if (code == 0)
        pdfi_pop(ctx, 2);
    if(code < 0 && ctx->pdfstoponerror)
        return code;
    else
        return 0;
}

int pdfi_setgrayfill(pdf_context *ctx)
{
    pdf_num *n1;
    int code;
    double d1;

    if (ctx->stack_top - ctx->stack_bot < 1) {
        if (ctx->pdfstoponerror)
            return_error(gs_error_stackunderflow);
        pdfi_clearstack(ctx);
        return 0;
    }

    n1 = (pdf_num *)ctx->stack_top[-1];
    if (n1->type == PDF_INT){
        d1 = (double)n1->value.i;
    } else{
        if (n1->type == PDF_REAL) {
            d1 = n1->value.d;
        } else {
            if (ctx->pdfstoponerror)
                return_error(gs_error_typecheck);
            else {
                pdfi_pop(ctx, 1);
                return 0;
            }
        }
    }
    gs_swapcolors(ctx->pgs);
    code = gs_setgray(ctx->pgs, d1);
    gs_swapcolors(ctx->pgs);
    if (code == 0)
        pdfi_pop(ctx, 2);
    if(code < 0 && ctx->pdfstoponerror)
        return code;
    else
        return 0;
}

int pdfi_setrgbstroke(pdf_context *ctx)
{
    pdf_num *num;
    double Values[3];
    int i, code;

    if (ctx->stack_top - ctx->stack_bot < 3) {
        if (ctx->pdfstoponerror)
            return_error(gs_error_stackunderflow);
        pdfi_clearstack(ctx);
        return 0;
    }

    for (i=0;i < 3;i++){
        num = (pdf_num *)ctx->stack_top[i - 3];
        if (num->type != PDF_INT) {
            if(num->type != PDF_REAL) {
                if (ctx->pdfstoponerror)
                    return_error(gs_error_typecheck);
                else {
                    pdfi_pop(ctx, 3);
                    return 0;
                }
            }
            else
                Values[i] = num->value.d;
        } else {
            Values[i] = (double)num->value.i;
        }
    }
    code = gs_setrgbcolor(ctx->pgs, Values[0], Values[1], Values[2]);
    if (code == 0)
        pdfi_pop(ctx, 2);
    if(code < 0 && ctx->pdfstoponerror)
        return code;
    else
        return 0;
}

int pdfi_setrgbfill(pdf_context *ctx)
{
    pdf_num *num;
    double Values[3];
    int i, code;

    if (ctx->stack_top - ctx->stack_bot < 3) {
        if (ctx->pdfstoponerror)
            return_error(gs_error_stackunderflow);
        pdfi_clearstack(ctx);
        return 0;
    }

    for (i=0;i < 3;i++){
        num = (pdf_num *)ctx->stack_top[i - 3];
        if (num->type != PDF_INT) {
            if(num->type != PDF_REAL) {
                if (ctx->pdfstoponerror)
                    return_error(gs_error_typecheck);
                else {
                    pdfi_pop(ctx, 3);
                    return 0;
                }
            }
            else
                Values[i] = num->value.d;
        } else {
            Values[i] = (double)num->value.i;
        }
    }
    gs_swapcolors(ctx->pgs);
    code = gs_setrgbcolor(ctx->pgs, Values[0], Values[1], Values[2]);
    gs_swapcolors(ctx->pgs);
    if (code == 0)
        pdfi_pop(ctx, 2);
    if(code < 0 && ctx->pdfstoponerror)
        return code;
    else
        return 0;
}

int pdfi_setcmykstroke(pdf_context *ctx)
{
    pdf_num *num;
    double Values[4];
    int i, code;

    if (ctx->stack_top - ctx->stack_bot < 4) {
        if (ctx->pdfstoponerror)
            return_error(gs_error_stackunderflow);
        pdfi_clearstack(ctx);
        return 0;
    }

    for (i=0;i < 4;i++){
        num = (pdf_num *)ctx->stack_top[i - 4];
        if (num->type != PDF_INT) {
            if(num->type != PDF_REAL) {
                if (ctx->pdfstoponerror)
                    return_error(gs_error_typecheck);
                else {
                    pdfi_pop(ctx, 4);
                    return 0;
                }
            }
            else
                Values[i] = num->value.d;
        } else {
            Values[i] = (double)num->value.i;
        }
    }
    code = gs_setcmykcolor(ctx->pgs, Values[0], Values[1], Values[2], Values[3]);
    if (code == 0)
        pdfi_pop(ctx, 2);
    if(code < 0 && ctx->pdfstoponerror)
        return code;
    else
        return 0;
}

int pdfi_setcmykfill(pdf_context *ctx)
{
    pdf_num *num;
    double Values[4];
    int i, code;

    if (ctx->stack_top - ctx->stack_bot < 4) {
        if (ctx->pdfstoponerror)
            return_error(gs_error_stackunderflow);
        pdfi_clearstack(ctx);
        return 0;
    }

    for (i=0;i < 4;i++){
        num = (pdf_num *)ctx->stack_top[i - 4];
        if (num->type != PDF_INT) {
            if(num->type != PDF_REAL) {
                if (ctx->pdfstoponerror)
                    return_error(gs_error_typecheck);
                else {
                    pdfi_pop(ctx, 4);
                    return 0;
                }
            }
            else
                Values[i] = num->value.d;
        } else {
            Values[i] = (double)num->value.i;
        }
    }
    gs_swapcolors(ctx->pgs);
    code = gs_setcmykcolor(ctx->pgs, Values[0], Values[1], Values[2], Values[3]);
    gs_swapcolors(ctx->pgs);
    if (code == 0)
        pdfi_pop(ctx, 2);
    if(code < 0 && ctx->pdfstoponerror)
        return code;
    else
        return 0;
}

/* Now deal with the case where we have to set the colour space separately from the
 * colour values. We'll start with the routines to set the colour, because setting
 * colour components is relatively easy.
 */

/* First up, the SC and sc operators. These set the colour for all spaces *except*
 * ICCBased, Pattern, Separation and DeviceN
 */
int pdfi_setstrokecolor(pdf_context *ctx)
{
    const gs_color_space *  pcs = gs_currentcolorspace(ctx->pgs);
    int ncomps, i, code;
    gs_client_color cc;
    pdf_num *n;

    ncomps = cs_num_components(pcs);
    if (ctx->stack_top - ctx->stack_bot < ncomps) {
        pdfi_clearstack(ctx);
        if(ctx->pdfstoponerror)
            return_error(gs_error_stackunderflow);
        return 0;
    }
    for (i=0;i<ncomps;i++){
        n = (pdf_num *)ctx->stack_top[i - ncomps];
        if (n->type == PDF_INT) {
            cc.paint.values[i] = (float)n->value.i;
        } else {
            if (n->type == PDF_REAL) {
                cc.paint.values[i] = n->value.d;
            } else {
                pdfi_clearstack(ctx);
                if (ctx->pdfstoponerror)
                    return_error(gs_error_typecheck);
                return 0;
            }
        }
    }
    pdfi_pop(ctx, ncomps);
    code = gs_setcolor(ctx->pgs, &cc);
    if (code < 0 && ctx->pdfstoponerror)
        return code;
    return 0;
}

int pdfi_setfillcolor(pdf_context *ctx)
{
    const gs_color_space *  pcs;
    int ncomps, i, code;
    gs_client_color cc;
    pdf_num *n;

    gs_swapcolors(ctx->pgs);
    pcs = gs_currentcolorspace(ctx->pgs);
    ncomps = cs_num_components(pcs);
    if (ctx->stack_top - ctx->stack_bot < ncomps) {
        gs_swapcolors(ctx->pgs);
        pdfi_clearstack(ctx);
        if(ctx->pdfstoponerror)
            return_error(gs_error_stackunderflow);
        return 0;
    }
    for (i=0;i<ncomps;i++){
        n = (pdf_num *)ctx->stack_top[i - ncomps];
        if (n->type == PDF_INT) {
            cc.paint.values[i] = (float)n->value.i;
        } else {
            if (n->type == PDF_REAL) {
                cc.paint.values[i] = n->value.d;
            } else {
                gs_swapcolors(ctx->pgs);
                pdfi_clearstack(ctx);
                if (ctx->pdfstoponerror)
                    return_error(gs_error_typecheck);
                return 0;
            }
        }
    }
    pdfi_pop(ctx, ncomps);
    code = gs_setcolor(ctx->pgs, &cc);
    gs_swapcolors(ctx->pgs);
    if (code < 0 && ctx->pdfstoponerror)
        return code;
    return 0;
}

/* Now the SCN and scn operators. These set the colour for special spaces;
 * ICCBased, Pattern, Separation and DeviceN
 */
int pdfi_setstrokecolorN(pdf_context *ctx)
{
    const gs_color_space *  pcs = gs_currentcolorspace(ctx->pgs);
    int ncomps, i, code;
    gs_client_color cc;
    pdf_num *n;

    if (ctx->stack_top - ctx->stack_bot < 1) {
        pdfi_clearstack(ctx);
        if(ctx->pdfstoponerror)
            return_error(gs_error_stackunderflow);
        return 0;
    }
    if (ctx->stack_top[-1]->type == PDF_NAME) {
        /* FIXME Patterns */
        pdfi_clearstack(ctx);
        return 0;
    }
    ncomps = cs_num_components(pcs);
    if (ctx->stack_top - ctx->stack_bot < ncomps) {
        pdfi_clearstack(ctx);
        if(ctx->pdfstoponerror)
            return_error(gs_error_stackunderflow);
        return 0;
    }
    for (i=0;i<ncomps;i++){
        n = (pdf_num *)ctx->stack_top[i - ncomps];
        if (n->type == PDF_INT) {
            cc.paint.values[i] = (float)n->value.i;
        } else {
            if (n->type == PDF_REAL) {
                cc.paint.values[i] = n->value.d;
            } else {
                pdfi_clearstack(ctx);
                if (ctx->pdfstoponerror)
                    return_error(gs_error_typecheck);
                return 0;
            }
        }
    }
    pdfi_pop(ctx, ncomps);
    code = gs_setcolor(ctx->pgs, &cc);
    if (code < 0 && ctx->pdfstoponerror)
        return code;
    return 0;
}

int pdfi_setfillcolorN(pdf_context *ctx)
{
    const gs_color_space *  pcs;
    int ncomps, i, code;
    gs_client_color cc;
    pdf_num *n;

    gs_swapcolors(ctx->pgs);
    pcs = gs_currentcolorspace(ctx->pgs);
    if (ctx->stack_top - ctx->stack_bot < 1) {
        gs_swapcolors(ctx->pgs);
        pdfi_clearstack(ctx);
        if(ctx->pdfstoponerror)
            return_error(gs_error_stackunderflow);
        return 0;
    }
    if (ctx->stack_top[-1]->type == PDF_NAME) {
        gs_swapcolors(ctx->pgs);
        /* FIXME Patterns */
        pdfi_clearstack(ctx);
        return 0;
    }
    ncomps = cs_num_components(pcs);
    if (ctx->stack_top - ctx->stack_bot < ncomps) {
        gs_swapcolors(ctx->pgs);
        pdfi_clearstack(ctx);
        if(ctx->pdfstoponerror)
            return_error(gs_error_stackunderflow);
        return 0;
    }
    for (i=0;i<ncomps;i++){
        n = (pdf_num *)ctx->stack_top[i - ncomps];
        if (n->type == PDF_INT) {
            cc.paint.values[i] = (float)n->value.i;
        } else {
            if (n->type == PDF_REAL) {
                cc.paint.values[i] = n->value.d;
            } else {
                gs_swapcolors(ctx->pgs);
                pdfi_clearstack(ctx);
                if (ctx->pdfstoponerror)
                    return_error(gs_error_typecheck);
                return 0;
            }
        }
    }
    pdfi_pop(ctx, ncomps);
    code = gs_setcolor(ctx->pgs, &cc);
    gs_swapcolors(ctx->pgs);
    if (code < 0 && ctx->pdfstoponerror)
        return code;
    return 0;
}

/* And now, the routines to set the colour space on its own. */

/* Starting with the ICCBased colour space */

/* This routine is mostly a copy of seticc() in zicc.c */
static int pdfi_create_icc(pdf_context *ctx, char *Name, stream *s, int ncomps, float *range_buff, gs_color_space **ppcs)
{
    int                     code, k;
    gs_color_space *        pcs;
    cmm_profile_t           *picc_profile = NULL;
    int                     i, expected = 0;

    static const char *const icc_std_profile_names[] = {
            GSICC_STANDARD_PROFILES
        };
    static const char *const icc_std_profile_keys[] = {
            GSICC_STANDARD_PROFILES_KEYS
        };

    code = gs_cspace_build_ICC(&pcs, NULL, gs_gstate_memory(ctx->pgs));
    if (code < 0)
        return code;

    if (Name != NULL){
        /* Compare this to the standard profile names */
        for (k = 0; k < GSICC_NUMBER_STANDARD_PROFILES; k++) {
            if ( strcmp( Name, icc_std_profile_keys[k] ) == 0 ) {
                picc_profile = gsicc_get_profile_handle_file(icc_std_profile_names[k],
                    strlen(icc_std_profile_names[k]), gs_gstate_memory(ctx->pgs));
                break;
            }
        }
    } else {
        picc_profile = gsicc_profile_new(s, gs_gstate_memory(ctx->pgs), NULL, 0);
        if (picc_profile == NULL)
            return gs_throw(gs_error_VMerror, "pdfi_create_icc Creation of ICC profile failed");
        /* We have to get the profile handle due to the fact that we need to know
           if it has a data space that is CIELAB */
        picc_profile->profile_handle =
            gsicc_get_profile_handle_buffer(picc_profile->buffer,
                                            picc_profile->buffer_size,
                                            gs_gstate_memory(ctx->pgs));
    }

    if (picc_profile == NULL || picc_profile->profile_handle == NULL) {
        /* Free up everything, the profile is not valid. We will end up going
           ahead and using a default based upon the number of components */
        rc_decrement(picc_profile,"pdfi_create_icc");
        rc_decrement(pcs,"pdfi_create_icc");
        return -1;
    }
    code = gsicc_set_gscs_profile(pcs, picc_profile, gs_gstate_memory(ctx->pgs));
    if (code < 0) {
        rc_decrement(picc_profile,"pdfi_create_icc");
        rc_decrement(pcs,"pdfi_create_icc");
        return code;
    }
    picc_profile->num_comps = ncomps;

    picc_profile->data_cs =
        gscms_get_profile_data_space(picc_profile->profile_handle,
            picc_profile->memory);
    switch (picc_profile->data_cs) {
        case gsCIEXYZ:
        case gsCIELAB:
        case gsRGB:
            expected = 3;
            break;
        case gsGRAY:
            expected = 1;
            break;
        case gsCMYK:
            expected = 4;
            break;
        case gsNCHANNEL:
        case gsNAMED:            /* Silence warnings */
        case gsUNDEFINED:        /* Silence warnings */
            break;
    }
    if (!expected || ncomps != expected) {
        rc_decrement(picc_profile,"pdfi_create_icc");
        rc_decrement(pcs,"pdfi_create_icc");
        return_error(gs_error_rangecheck);
    }
    /* Lets go ahead and get the hash code and check if we match one of the default spaces */
    /* Later we may want to delay this, but for now lets go ahead and do it */
    gsicc_init_hash_cs(picc_profile, ctx->pgs);

    /* Set the range according to the data type that is associated with the
       ICC input color type.  Occasionally, we will run into CIELAB to CIELAB
       profiles for spot colors in PDF documents. These spot colors are typically described
       as separation colors with tint transforms that go from a tint value
       to a linear mapping between the CIELAB white point and the CIELAB tint
       color.  This results in a CIELAB value that we need to use to fill.  We
       need to detect this to make sure we do the proper scaling of the data.  For
       CIELAB images in PDF, the source is always normal 8 or 16 bit encoded data
       in the range from 0 to 255 or 0 to 65535.  In that case, there should not
       be any encoding and decoding to CIELAB.  The PDF content will not include
       an ICC profile but will set the color space to \Lab.  In this case, we use
       our seticc_lab operation to install the LAB to LAB profile, but we detect
       that we did that through the use of the is_lab flag in the profile descriptor.
       When then avoid the CIELAB encode and decode */
    if (picc_profile->data_cs == gsCIELAB) {
    /* If the input space to this profile is CIELAB, then we need to adjust the limits */
        /* See ICC spec ICC.1:2004-10 Section 6.3.4.2 and 6.4.  I don't believe we need to
           worry about CIEXYZ profiles or any of the other odds ones.  Need to check that though
           at some point. */
        picc_profile->Range.ranges[0].rmin = 0.0;
        picc_profile->Range.ranges[0].rmax = 100.0;
        picc_profile->Range.ranges[1].rmin = -128.0;
        picc_profile->Range.ranges[1].rmax = 127.0;
        picc_profile->Range.ranges[2].rmin = -128.0;
        picc_profile->Range.ranges[2].rmax = 127.0;
        picc_profile->islab = true;
    } else {
        for (i = 0; i < ncomps; i++) {
            picc_profile->Range.ranges[i].rmin = range_buff[2 * i];
            picc_profile->Range.ranges[i].rmax = range_buff[2 * i + 1];
        }
    }
    /* Now see if we are in an overide situation.  We have to wait until now
       in case this is an LAB profile which we will not overide */
    if (gs_currentoverrideicc(ctx->pgs) && picc_profile->data_cs != gsCIELAB) {
        /* Free up the profile structure */
        switch( picc_profile->data_cs ) {
            case gsRGB:
                pcs->cmm_icc_profile_data = ctx->pgs->icc_manager->default_rgb;
                break;
            case gsGRAY:
                pcs->cmm_icc_profile_data = ctx->pgs->icc_manager->default_gray;
                break;
            case gsCMYK:
                pcs->cmm_icc_profile_data = ctx->pgs->icc_manager->default_cmyk;
                break;
            default:
                break;
        }
        /* Have one increment from the color space.  Having these tied
           together is not really correct.  Need to fix that.  ToDo.  MJV */
        rc_adjust(picc_profile, -2, "pdfi_create_icc");
        rc_increment(pcs->cmm_icc_profile_data);
    }
    *ppcs = pcs;
    /* The context has taken a reference to the colorspace. We no longer need
     * ours, so drop it. */
    rc_decrement(picc_profile, "pdfi_create_icc");
    return code;
}

static int pdfi_create_iccprofile(pdf_context *ctx, pdf_dict *ICC_dict, char *cname, int64_t Length, int N, float *range, gs_color_space **ppcs)
{
    pdf_stream *profile_stream = NULL, *filtered_profile_stream = NULL;
    stream *s;
    byte *profile_buffer;
    gs_offset_t savedoffset;
    bool known;
    int code, code1;

    /* Save the current stream position, and move to the start of the profile stream */
    savedoffset = pdfi_tell(ctx->main_stream);
    pdfi_seek(ctx, ctx->main_stream, ICC_dict->stream_offset, SEEK_SET);

    /* The ICC profile reading code (irritatingly) requires a seekable stream, because it
     * rewids it to the start, then seeks to the ned to find the size, then rewids the
     * stream again.
     * Ideally we would use a ReusableStreamDecode filter here, but that is largely
     * implemented in PostScript (!) so we can't use it. What we cna do is create a
     * string sourced stream in memory, which is at least seekable.
     */
    code = pdfi_open_memory_stream_from_stream(ctx, (unsigned int)Length, &profile_buffer, ctx->main_stream, &profile_stream);
    if (code < 0) {
        pdfi_seek(ctx, ctx->main_stream, savedoffset, SEEK_SET);
        return code;
    }

    pdfi_dict_known(ICC_dict, "F", &known);
    if (!known)
        pdfi_dict_known(ICC_dict, "Filter", &known);

    /* If this is a compressed stream, we need to decompress it */
    if(known) {
        int decompressed_length = 0, bytes;
        byte c, *Buffer;
        gs_offset_t avail;

        /* This is again complicated by requiring a seekable stream, and the fact that,
         * unlike fonts, there is no Length2 key to tell us how large the uncompressed
         * stream is. We need to make a new string sourced stream containing the
         * uncompressed dtaa, because the ICC profile reading code needs to seek, and
         * we don't kow the decompressed size. But the compressed stream is at least
         * seekable, so we do 2 passes, one to find the decompressed size, then allocate
         * enough memory to hold it, and then another decompressing into that buffer.
         * We can then discard the original (compresed) stream and use the new
         * (decompressed) stream instead.
         */
        code = pdfi_filter(ctx, ICC_dict, profile_stream, &filtered_profile_stream, false);
        if (code < 0) {
            pdfi_close_memory_stream(ctx, profile_buffer, profile_stream);
            pdfi_seek(ctx, ctx->main_stream, savedoffset, SEEK_SET);
            return code;
        }
        do {
            bytes = sfgetc(filtered_profile_stream->s);
            if (bytes > 0)
                decompressed_length++;
        } while (bytes >= 0);
        pdfi_close_file(ctx, filtered_profile_stream);

        Buffer = gs_alloc_bytes(ctx->memory, decompressed_length, "pdfi_create_iccprofile (decompression buffer)");
        if (Buffer != NULL) {
            code = srewind(profile_stream->s);
            if (code >= 0) {
                code = pdfi_filter(ctx, ICC_dict, profile_stream, &filtered_profile_stream, false);
                if (code >= 0) {
                    sfread(Buffer, 1, decompressed_length, filtered_profile_stream->s);
                    pdfi_close_file(ctx, filtered_profile_stream);
                    code = pdfi_close_memory_stream(ctx, profile_buffer, profile_stream);
                    if (code >= 0) {
                        profile_buffer = Buffer;
                        code = pdfi_open_memory_stream_from_memory(ctx, (unsigned int)decompressed_length, profile_buffer, &profile_stream);
                    }
                }
            } else {
                pdfi_close_memory_stream(ctx, profile_buffer, profile_stream);
                gs_free_object(ctx->memory, Buffer, "pdfi_create_iccprofile (profile name)");
                pdfi_seek(ctx, ctx->main_stream, savedoffset, SEEK_SET);
                return code;
            }
        } else {
            pdfi_close_memory_stream(ctx, profile_buffer, profile_stream);
            pdfi_seek(ctx, ctx->main_stream, savedoffset, SEEK_SET);
            return_error(gs_error_VMerror);
        }
        if (code < 0) {
            gs_free_object(ctx->memory, Buffer, "pdfi_create_iccprofile (profile name)");
            pdfi_seek(ctx, ctx->main_stream, savedoffset, SEEK_SET);
            return code;
        }
    }

    /* Now, finally, we can call the code to create and set the profile */
    code = pdfi_create_icc(ctx, cname, profile_stream->s, (int)N, range, ppcs);

    code1 = pdfi_close_memory_stream(ctx, profile_buffer, profile_stream);

    if (code == 0)
        code = code1;

    pdfi_seek(ctx, ctx->main_stream, savedoffset, SEEK_SET);

    return code;
}

static int pdfi_create_iccbased(pdf_context *ctx, pdf_array *color_array, int index, pdf_dict *stream_dict, pdf_dict *page_dict, gs_color_space **ppcs)
{
    pdf_dict *ICC_dict = NULL;
    int64_t Length, N;
    pdf_obj *Name;
    char *cname = NULL;
    int code, code1;
    bool known;
    float range[8];

    code = pdfi_array_get(color_array, index + 1, (pdf_obj **)&ICC_dict);
    if (code < 0)
        return code;

    if (ICC_dict->type == PDF_INDIRECT) {
        pdf_indirect_ref *r = (pdf_indirect_ref *)ICC_dict;
        code = pdfi_dereference(ctx, r->ref_object_num, r->ref_generation_num, (pdf_obj **)&ICC_dict);
        pdfi_countdown(r);
        if (code < 0)
            return code;
    }
    code = pdfi_dict_get_int(ctx, ICC_dict, "Length", &Length);
    if (code < 0)
        return code;
    code = pdfi_dict_get_int(ctx, ICC_dict, "N", &N);
    if (code < 0) {
        pdfi_countdown(Length);
        return code;
    }
    pdfi_dict_known(ICC_dict, "Name", &known);
    if (known) {
        code = pdfi_dict_get(ctx, ICC_dict, "Name", &Name);
        if (code >= 0) {
            if(Name->type == PDF_STRING || Name->type == PDF_NAME) {
                cname = (char *)gs_alloc_bytes(ctx->memory, ((pdf_name *)Name)->length + 1, "pdfi_create_iccbased (profile name)");
                if (cname == NULL) {
                    pdfi_countdown(Name);
                    return_error(gs_error_VMerror);
                }
                memset(cname, 0x00, ((pdf_name *)Name)->length + 1);
                memcpy(cname, ((pdf_name *)Name)->data, ((pdf_name *)Name)->length);
            }
            pdfi_countdown(Name);
        }
    }

    pdfi_dict_known(ICC_dict, "Range", &known);
    if (known) {
        pdf_array *a;
        double dbl;
        int i;

        code = pdfi_dict_get_type(ctx, ICC_dict, "Range", PDF_ARRAY, (pdf_obj **)&a);
        if (code < 0)
            known = false;
        else {
            if (a->size >= N * 2) {
                for (i = 0; i < a->size;i++) {
                    code = pdfi_array_get_number(ctx, a, i, &dbl);
                    if (code < 0) {
                        known = false;
                        break;
                    }
                    range[i] = (float)dbl;
                }
            } else {
                known = false;
            }
        }
    }

    if (!known) {
        int i;
        for (i = 0;i < N; i++) {
            range[i * 2] = 0;
            range[(i * 2) + 1] = 1;
        }
    }

    code = pdfi_create_iccprofile(ctx, ICC_dict, cname, Length, N, range, ppcs);

    gs_free_object(ctx->memory, cname, "pdfi_create_iccbased (profile name)");

    if (code < 0) {
        /* Failed to set the ICCBased space, attempt to use the Alternate */
        pdfi_dict_known(ICC_dict, "Alternate", &known);
        if (known) {
            pdf_obj *Alternate;

            code = pdfi_dict_get(ctx, ICC_dict, "Alternate", &Alternate);
            if (code < 0)
                return code;
            code = pdfi_create_colorspace_by_name(ctx, (pdf_name *)Alternate, stream_dict, page_dict, ppcs);
            /* The Alternate should be one of the device spaces */
            pdfi_countdown(Alternate);
        } else {
            /* Use the number of components (N) to set a space.... */
            switch(N) {
                case 1:
                    *ppcs = gs_cspace_new_DeviceGray(ctx->memory);
                    if (*ppcs == NULL)
                        return_error(gs_error_VMerror);
                    code = 0;
                    break;
                case 3:
                    *ppcs = gs_cspace_new_DeviceRGB(ctx->memory);
                    if (*ppcs == NULL)
                        return_error(gs_error_VMerror);
                    code = 0;
                    break;
                case 4:
                    *ppcs = gs_cspace_new_DeviceCMYK(ctx->memory);
                    if (*ppcs == NULL)
                        return_error(gs_error_VMerror);
                    code = 0;
                    break;
                default:
                    return_error(gs_error_undefined);
                    break;
            }
        }
    }

    pdfi_countdown(ICC_dict);
    return code;
}

/* Now /Indexed spaces, essentially we just need to set the underlying space(s) and then set
 * /Indexed.
 */
static int pdfi_create_indexed(pdf_context *ctx, pdf_array *color_array, int index, pdf_dict *stream_dict, pdf_dict *page_dict, gs_color_space **ppcs)
{
    pdf_obj *space, *space1, *lookup;
    pdf_array *a;
    int code;
    int64_t hival, lookup_length;
    bool special;
    gs_color_space *pcs, *pcs_base;
    gs_color_space_index base_type;
    byte *Buffer;

    if (index != 0)
        return_error(gs_error_syntaxerror);

    code = pdfi_array_get_int(ctx, color_array, index + 2, &hival);
    if (code < 0)
        return code;

    if (hival > 255 || hival < 1)
        return_error(gs_error_syntaxerror);

    code = pdfi_array_get(color_array, index + 1, &space);
    if (code < 0)
        return code;

    code = pdfi_create_colorspace(ctx, space, stream_dict, page_dict, &pcs_base);
    if (code < 0)
        return code;

    (void)pcs_base->type->install_cspace(pcs_base, ctx->pgs);
    pdfi_countdown(space);
    if (code < 0)
        return code;

    base_type = gs_color_space_get_index(pcs_base);

    code = pdfi_array_get(color_array, index + 3, &lookup);
    if (code < 0)
        return code;

    if (lookup->type == PDF_INDIRECT) {
        /* Need to dereference, and then read the stream */
        pdf_indirect_ref *r = (pdf_indirect_ref *)lookup;
        pdf_dict *lookup_dict;
        gs_offset_t savedoffset;
        bool known;
        pdf_stream *filtered_lookup_stream = NULL;
        int64_t Length;

        code = pdfi_dereference(ctx, r->ref_object_num, r->ref_generation_num, (pdf_obj **)&lookup_dict);
        pdfi_countdown(r);
        if (code < 0) {
            pdfi_countdown(lookup_dict);
            return code;
        }

        if (lookup_dict->type == PDF_DICT) {
            savedoffset = pdfi_tell(ctx->main_stream);
            pdfi_seek(ctx, ctx->main_stream, lookup_dict->stream_offset, SEEK_SET);

            pdfi_dict_known(lookup_dict, "F", &known);
            if (!known)
                pdfi_dict_known(lookup_dict, "Filter", &known);

            /* If this is a compressed stream, we need to decompress it to discover its length
             * then create a buffer big enough to read it int, then read it.
             */
            if(known) {
                int decompressed_length = 0, bytes;
                byte c;
                gs_offset_t avail;

                code = pdfi_filter(ctx, lookup_dict, ctx->main_stream, &filtered_lookup_stream, false);
                if (code < 0) {
                    pdfi_countdown(lookup_dict);
                    pdfi_seek(ctx, ctx->main_stream, savedoffset, SEEK_SET);
                    return code;
                }
                do {
                    bytes = sfgetc(filtered_lookup_stream->s);
                    if (bytes > 0)
                        decompressed_length++;
                } while (bytes >= 0);
                pdfi_close_file(ctx, filtered_lookup_stream);

                Buffer = gs_alloc_bytes(ctx->memory, decompressed_length, "pdfi_create_indexed (decompression buffer)");
                if (Buffer != NULL) {
                    code = pdfi_seek(ctx, ctx->main_stream, lookup_dict->stream_offset, SEEK_SET);
                    if (code >= 0) {
                        code = pdfi_filter(ctx, lookup_dict, ctx->main_stream, &filtered_lookup_stream, false);
                        if (code >= 0) {
                            sfread(Buffer, 1, decompressed_length, filtered_lookup_stream->s);
                            pdfi_close_file(ctx, filtered_lookup_stream);
                        }
                    } else {
                        gs_free_object(ctx->memory, Buffer, "pdfi_create_indexed (decompression buffer)");
                        pdfi_countdown(lookup_dict);
                        pdfi_seek(ctx, ctx->main_stream, savedoffset, SEEK_SET);
                        return code;
                    }
                } else {
                    pdfi_countdown(lookup_dict);
                    pdfi_seek(ctx, ctx->main_stream, savedoffset, SEEK_SET);
                    return_error(gs_error_VMerror);
                }
                if (code < 0) {
                    pdfi_countdown(lookup_dict);
                    pdfi_seek(ctx, ctx->main_stream, savedoffset, SEEK_SET);
                    gs_free_object(ctx->memory, Buffer, "pdfi_create_iindexed (decompression buffer)");
                    return code;
                }
                lookup_length = decompressed_length;
            } else {
                /* Create a buffer and read the data into it. */
                code = pdfi_dict_get_int(ctx, lookup_dict, "Length", &Length);
                if (code < 0) {
                    pdfi_seek(ctx, ctx->main_stream, savedoffset, SEEK_SET);
                    pdfi_countdown(lookup_dict);
                    return code;
                }
                Buffer = gs_alloc_bytes(ctx->memory, Length, "pdfi_create_indexed (lookup buffer)");
                if (Buffer == NULL) {
                    pdfi_seek(ctx, ctx->main_stream, savedoffset, SEEK_SET);
                    pdfi_countdown(lookup_dict);
                    return_error(gs_error_VMerror);;
                }
                sfread(Buffer, (size_t)1, (size_t)Length, ctx->main_stream->s);
                lookup_length = Length;
            }
        } else {
            if (lookup_dict->type == PDF_STRING) {
                /* This is not legal, but Acrobat seems to accept it */
                pdf_string *lookup_string = (pdf_string *)lookup_dict;

                Buffer = gs_alloc_bytes(ctx->memory, lookup_string->length, "pdfi_create_indexed (lookup buffer)");
                memcpy(Buffer, lookup_string->data, lookup_string->length);
                lookup_length = lookup_string->length;
                pdfi_countdown(lookup_string);
            }
        }
    } else {
        Buffer = gs_alloc_bytes(ctx->memory, ((pdf_string *)lookup)->length, "pdfi_create_indexed (lookup buffer)");
        memcpy(Buffer, ((pdf_string *)lookup)->data, ((pdf_string *)lookup)->length);
        lookup_length = ((pdf_string *)lookup)->length;
        pdfi_countdown(lookup);
    }

    if (hival * cs_num_components(pcs_base) > lookup_length) {
        gs_free_object(ctx->memory, Buffer, "pdfi_create_indexed (lookup buffer)");
        return_error(gs_error_rangecheck);
        return 0;
    }

    /* If we have a named color profile and the base space is DeviceN or
       Separation use a different set of procedures to ensure the named
       color remapping code is used */
    if (ctx->pgs->icc_manager->device_named != NULL &&
        (base_type == gs_color_space_index_Separation ||
         base_type == gs_color_space_index_DeviceN))
        pcs = gs_cspace_alloc(ctx->memory, &gs_color_space_type_Indexed_Named);
    else
        pcs = gs_cspace_alloc(ctx->memory, &gs_color_space_type_Indexed);

    pcs->base_space = pcs_base;
    rc_increment_cs(pcs_base);

    pcs->params.indexed.lookup.table.size = hival;
    pcs->params.indexed.use_proc = 0;
    pcs->params.indexed.hival = hival;
    pcs->params.indexed.n_comps = cs_num_components(pcs_base);
    pcs->params.indexed.lookup.table.data = Buffer;

    *ppcs = pcs;
    return 0;
}

/* These next routines allow us to use recursion to set up colour spaces. We can set
 * colour space starting from a name (which can be a named resource) or an array.
 * If we get a name, and its a named resource we dereference it and go round again.
 * If its an array we select the correct handler (above) for that space. The space
 * handler will call pdfi_create_colorspace() to set the underlygin space(s) whcih
 * may mean calling pdfi_create_colorspace again....
 */
static int pdfi_create_colorspace_by_array(pdf_context *ctx, pdf_array *color_array, int index, pdf_dict *stream_dict, pdf_dict *page_dict, gs_color_space **ppcs)
{
    int code, code1;
    pdf_name *space = NULL;
    pdf_array *a;
    gs_color_space *pcs;

    code = pdfi_array_get(color_array, index, (pdf_obj **)&space);
    if(code == 0) {
        if (space->type == PDF_NAME) {
            code = gs_error_rangecheck;
            switch(space->length) {
                case 1:
                    if (memcmp(space->data, "G", 1) == 0) {
                        if (ppcs != NULL) {
                            *ppcs = gs_cspace_new_DeviceGray(ctx->memory);
                            if (*ppcs == NULL)
                                return_error(gs_error_VMerror);
                        } else {
                            code = gs_setgray(ctx->pgs, 1);
                            if (code < 0)
                                return code;
                        }
                        return 0;
                    }
                    if (memcmp(space->data, "I", 1) == 0) {
                        code = pdfi_create_indexed(ctx, color_array, index, stream_dict, page_dict, &pcs);
                        if (code < 0)
                            return code;

                        if (ppcs!= NULL){
                            *ppcs = pcs;
                        } else {
                            code = gs_setcolorspace(ctx->pgs, pcs);
                            /* release reference from construction */
                            rc_decrement_only_cs(pcs, "setindexedspace");
                        }
                    }
                    break;
                case 3:
                    if (memcmp(space->data, "Lab", space->length) == 0) {
                        if (ppcs != NULL) {
                            pcs = gs_cspace_new_DeviceRGB(ctx->memory);
                            if (pcs == NULL)
                                return_error(gs_error_VMerror);
                            *ppcs = pcs;
                        } else {
                            code = gs_setrgbcolor(ctx->pgs, 0, 0, 0);
                            if (code < 0)
                                return code;
                        }
                    }
                    if (memcmp(space->data, "RGB", 3) == 0){
                        if (ppcs != NULL) {
                            *ppcs = gs_cspace_new_DeviceRGB(ctx->memory);
                            if (*ppcs == NULL)
                                return_error(gs_error_VMerror);
                        } else {
                            code = gs_setrgbcolor(ctx->pgs, 0, 0, 0);
                            if (code < 0)
                                return code;
                        }
                        return 0;
                    }
                    break;
                case 4:
                    if (memcmp(space->data, "CMYK", 4) == 0){
                        if (ppcs != NULL) {
                            pcs = gs_cspace_new_DeviceCMYK(ctx->memory);
                            if (pcs == NULL)
                                return_error(gs_error_VMerror);
                            *ppcs = pcs;
                        } else {
                            code = gs_setcmykcolor(ctx->pgs, 0, 0, 0, 1);
                            if (code < 0)
                                return code;
                        }
                    }
                    break;
                case 6:
                    if (memcmp(space->data, "CalRGB", space->length) == 0) {
                        if (ppcs != NULL) {
                            pcs = gs_cspace_new_DeviceRGB(ctx->memory);
                            if (pcs == NULL)
                                return_error(gs_error_VMerror);
                            *ppcs = pcs;
                        } else {
                            code = gs_setrgbcolor(ctx->pgs, 0, 0, 0);
                            if (code < 0)
                                return code;
                        }
                    }
                    break;
                case 7:
                    if (memcmp(space->data, "CalGray", space->length) == 0) {
                        if (ppcs != NULL) {
                            pcs = gs_cspace_new_DeviceGray(ctx->memory);
                            if (pcs == NULL)
                                return_error(gs_error_VMerror);
                            *ppcs = pcs;
                        } else {
                            code = gs_setgray(ctx->pgs, 1);
                            if (code < 0)
                                return code;
                        }
                    }
                    if (memcmp(space->data, "Pattern", space->length) == 0) {
                        if (index != 0)
                            return_error(gs_error_syntaxerror);
                    }
                    if (memcmp(space->data, "DeviceN", space->length) == 0) {
                    }
                    if (memcmp(space->data, "Indexed", space->length) == 0) {
                        code = pdfi_create_indexed(ctx, color_array, index, stream_dict, page_dict, &pcs);
                        if (code < 0)
                            return code;

                        if (ppcs!= NULL){
                            *ppcs = pcs;
                        } else {
                            code = gs_setcolorspace(ctx->pgs, pcs);
                            /* release reference from construction */
                            rc_decrement_only_cs(pcs, "setindexedspace");
                        }
                    }
                    break;
                case 8:
                    if (memcmp(space->data, "ICCBased", space->length) == 0) {
                        code = pdfi_create_iccbased(ctx, color_array, index, stream_dict, page_dict, &pcs);
                        if (code < 0)
                            return code;

                        if (ppcs!= NULL){
                            *ppcs = pcs;
                        } else {
                            code = gs_setcolorspace(ctx->pgs, pcs);
                            /* release reference from construction */
                            rc_decrement_only_cs(pcs, "seticcspace");
                        }
                    }
                    break;
                case 9:
                    if (memcmp(space->data, "DeviceRGB", space->length) == 0) {
                        if (ppcs != NULL) {
                            pcs = gs_cspace_new_DeviceRGB(ctx->memory);
                            if (pcs == NULL)
                                return_error(gs_error_VMerror);
                            *ppcs = pcs;
                        } else {
                            code = gs_setrgbcolor(ctx->pgs, 0, 0, 0);
                            if (code < 0)
                                return code;
                        }
                    }
                    break;
                case 10:
                    if (memcmp(space->data, "DeviceGray", space->length) == 0) {
                        if (ppcs != NULL) {
                            pcs = gs_cspace_new_DeviceGray(ctx->memory);
                            if (pcs == NULL)
                                return_error(gs_error_VMerror);
                            *ppcs = pcs;
                        } else {
                            code = gs_setgray(ctx->pgs, 1);
                            if (code < 0)
                                return code;
                        }
                    }
                    if (memcmp(space->data, "DeviceCMYK", space->length) == 0) {
                        if (ppcs != NULL) {
                            pcs = gs_cspace_new_DeviceCMYK(ctx->memory);
                            if (pcs == NULL)
                                return_error(gs_error_VMerror);
                            *ppcs = pcs;
                        } else {
                            code = gs_setcmykcolor(ctx->pgs, 0, 0, 0, 1);
                            if (code < 0)
                                return code;
                        }
                    }
                    if (memcmp(space->data, "Separation", space->length) == 0) {
                        if (ppcs != NULL) {
                            pcs = gs_cspace_new_DeviceGray(ctx->memory);
                            if (pcs == NULL)
                                return_error(gs_error_VMerror);
                            *ppcs = pcs;
                        } else {
                            code = gs_setgray(ctx->pgs, 1);
                            if (code < 0)
                                return code;
                        }
                    }
                    break;
                default:
                    code = pdfi_find_resource(ctx, (unsigned char *)"ColorSpace", space, stream_dict, page_dict, (pdf_obj **)&a);
                    if (code < 0)
                        return code;

                    if (a->type != PDF_ARRAY) {
                        pdfi_countdown(a);
                        return_error(gs_error_typecheck);
                        return 0;
                    }

                    code = pdfi_create_colorspace_by_array(ctx, a, 0, stream_dict, page_dict, ppcs);
                    pdfi_countdown(a);
                    return code;
                    break;
            }
        } else
            code = gs_error_typecheck;
    }
    pdfi_countdown(space);
    return code;
}

static int pdfi_create_colorspace_by_name(pdf_context *ctx, pdf_name *name, pdf_dict *stream_dict, pdf_dict *page_dict, gs_color_space **ppcs)
{
    int code;
    pdf_obj *ref_space;

    switch(name->length) {
        case 1:
            if (memcmp(name->data, "G", 1) == 0) {
                if (ppcs != NULL) {
                    *ppcs = gs_cspace_new_DeviceGray(ctx->memory);
                    if (*ppcs == NULL)
                        return_error(gs_error_VMerror);
                } else {
                    code = gs_setgray(ctx->pgs, 1);
                    if (code < 0)
                        return code;
                }
                return 0;
            }
            break;
        case 3:
            if (memcmp(name->data, "RGB", 3) == 0){
                if (ppcs != NULL) {
                    *ppcs = gs_cspace_new_DeviceRGB(ctx->memory);
                    if (*ppcs == NULL)
                        return_error(gs_error_VMerror);
                } else {
                    code = gs_setrgbcolor(ctx->pgs, 0, 0, 0);
                    if (code < 0)
                        return code;
                }
                return 0;
            }
            break;
        case 4:
            if (memcmp(name->data, "CMYK", 4) == 0){
                if (ppcs != NULL) {
                    *ppcs = gs_cspace_new_DeviceCMYK(ctx->memory);
                    if (*ppcs == NULL)
                        return_error(gs_error_VMerror);
                } else {
                    code = gs_setcmykcolor(ctx->pgs, 0, 0, 0, 1);
                    if (code < 0)
                        return code;
                }
                return 0;
            }
            break;
        case 9:
            if (memcmp(name->data, "DeviceRGB", 9) == 0) {
                if (ppcs != NULL) {
                    *ppcs = gs_cspace_new_DeviceRGB(ctx->memory);
                    if (*ppcs == NULL)
                        return_error(gs_error_VMerror);
                } else {
                    code = gs_setrgbcolor(ctx->pgs, 0, 0, 0);
                    if (code < 0)
                        return code;
                }
                return 0;
            }
        case 10:
            if (memcmp(name->data, "DeviceGray", 10) == 0) {
                if (ppcs != NULL) {
                    *ppcs = gs_cspace_new_DeviceGray(ctx->memory);
                    if (*ppcs == NULL)
                        return_error(gs_error_VMerror);
                } else {
                    code = gs_setgray(ctx->pgs, 1);
                    if (code < 0)
                        return code;
                }
                return 0;
            }
            if (memcmp(name->data, "DeviceCMYK", 10) == 0) {
                if (ppcs != NULL) {
                    *ppcs = gs_cspace_new_DeviceCMYK(ctx->memory);
                    if (*ppcs == NULL)
                        return_error(gs_error_VMerror);
                } else {
                    code = gs_setcmykcolor(ctx->pgs, 0, 0, 0, 1);
                    if (code < 0)
                        return code;
                }
                return 0;
            }
        default:
            break;
    }
    code = pdfi_find_resource(ctx, (unsigned char *)"ColorSpace", name, stream_dict, page_dict, &ref_space);
    if (code < 0)
        return code;

    return pdfi_create_colorspace(ctx, ref_space, stream_dict, page_dict, ppcs);
}

int pdfi_create_colorspace(pdf_context *ctx, pdf_obj *space, pdf_dict *stream_dict, pdf_dict *page_dict, gs_color_space **ppcs)
{
    int code;
    pdf_array *a = NULL;

    if (space->type == PDF_NAME) {
        code = pdfi_create_colorspace_by_name(ctx, (pdf_name *)space, stream_dict, page_dict, ppcs);
    } else {
        if (space->type == PDF_ARRAY) {
            code = pdfi_create_colorspace_by_array(ctx, (pdf_array *)space, 0, stream_dict, page_dict, ppcs);
        } else
            return_error(gs_error_typecheck);
    }
    if (ppcs && *ppcs && code >= 0)
        (void)(*ppcs)->type->install_cspace(*ppcs, ctx->pgs);
    return code;
}

int pdfi_setcolorspace(pdf_context *ctx, pdf_obj *space, pdf_dict *stream_dict, pdf_dict *page_dict)
{
    return pdfi_create_colorspace(ctx, space, stream_dict, page_dict, NULL);
}

/* And finally, the implementation of the actual PDF operators CS and cs */
int pdfi_setstrokecolor_space(pdf_context *ctx, pdf_dict *stream_dict, pdf_dict *page_dict)
{
    int code;
    pdf_name *n = NULL;
    pdf_array *a = NULL;

    if (ctx->stack_top - ctx->stack_bot < 1) {
        if (ctx->pdfstoponerror)
            return_error(gs_error_stackunderflow);
        return 0;
    }
    if (ctx->stack_top[-1]->type != PDF_NAME) {
        pdfi_pop(ctx, 1);
        if (ctx->pdfstoponerror)
            return_error(gs_error_stackunderflow);
        return 0;
    }
    code = pdfi_setcolorspace(ctx, ctx->stack_top[-1], stream_dict, page_dict);
    pdfi_pop(ctx, 1);

    if (code < 0 && ctx->pdfstoponerror)
        return code;
    return 0;
}

int pdfi_setfillcolor_space(pdf_context *ctx, pdf_dict *stream_dict, pdf_dict *page_dict)
{
    int code;
    pdf_name *n = NULL;
    pdf_array *a = NULL;

    if (ctx->stack_top - ctx->stack_bot < 1) {
        if (ctx->pdfstoponerror)
            return_error(gs_error_stackunderflow);
        return 0;
    }
    if (ctx->stack_top[-1]->type != PDF_NAME) {
        pdfi_pop(ctx, 1);
        if (ctx->pdfstoponerror)
            return_error(gs_error_stackunderflow);
        return 0;
    }
    gs_swapcolors(ctx->pgs);
    code = pdfi_setcolorspace(ctx, ctx->stack_top[-1], stream_dict, page_dict);
    gs_swapcolors(ctx->pgs);
    pdfi_pop(ctx, 1);

    if (code < 0 && ctx->pdfstoponerror)
        return code;
    return 0;
}
