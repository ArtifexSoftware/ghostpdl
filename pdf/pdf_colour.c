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

int pdf_setgraystroke(pdf_context *ctx)
{
    pdf_num *n1;
    int code;
    double d1;

    if (ctx->stack_top - ctx->stack_bot < 1) {
        if (ctx->pdfstoponerror)
            return_error(gs_error_stackunderflow);
        pdf_clearstack(ctx);
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
                pdf_pop(ctx, 1);
                return 0;
            }
        }
    }
    code = gs_setgray(ctx->pgs, d1);
    if (code == 0)
        pdf_pop(ctx, 2);
    if(code < 0 && ctx->pdfstoponerror)
        return code;
    else
        return 0;
}

int pdf_setgrayfill(pdf_context *ctx)
{
    pdf_num *n1;
    int code;
    double d1;

    if (ctx->stack_top - ctx->stack_bot < 1) {
        if (ctx->pdfstoponerror)
            return_error(gs_error_stackunderflow);
        pdf_clearstack(ctx);
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
                pdf_pop(ctx, 1);
                return 0;
            }
        }
    }
    gs_swapcolors(ctx->pgs);
    code = gs_setgray(ctx->pgs, d1);
    gs_swapcolors(ctx->pgs);
    if (code == 0)
        pdf_pop(ctx, 2);
    if(code < 0 && ctx->pdfstoponerror)
        return code;
    else
        return 0;
}

int pdf_setrgbstroke(pdf_context *ctx)
{
    pdf_num *num;
    double Values[3];
    int i, code;

    if (ctx->stack_top - ctx->stack_bot < 3) {
        if (ctx->pdfstoponerror)
            return_error(gs_error_stackunderflow);
        pdf_clearstack(ctx);
        return 0;
    }

    for (i=0;i < 3;i++){
        num = (pdf_num *)ctx->stack_top[i - 3];
        if (num->type != PDF_INT) {
            if(num->type != PDF_REAL) {
                if (ctx->pdfstoponerror)
                    return_error(gs_error_typecheck);
                else {
                    pdf_pop(ctx, 3);
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
        pdf_pop(ctx, 2);
    if(code < 0 && ctx->pdfstoponerror)
        return code;
    else
        return 0;
}

int pdf_setrgbfill(pdf_context *ctx)
{
    pdf_num *num;
    double Values[3];
    int i, code;

    if (ctx->stack_top - ctx->stack_bot < 3) {
        if (ctx->pdfstoponerror)
            return_error(gs_error_stackunderflow);
        pdf_clearstack(ctx);
        return 0;
    }

    for (i=0;i < 3;i++){
        num = (pdf_num *)ctx->stack_top[i - 3];
        if (num->type != PDF_INT) {
            if(num->type != PDF_REAL) {
                if (ctx->pdfstoponerror)
                    return_error(gs_error_typecheck);
                else {
                    pdf_pop(ctx, 3);
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
        pdf_pop(ctx, 2);
    if(code < 0 && ctx->pdfstoponerror)
        return code;
    else
        return 0;
}

int pdf_setcmykstroke(pdf_context *ctx)
{
    pdf_num *num;
    double Values[4];
    int i, code;

    if (ctx->stack_top - ctx->stack_bot < 4) {
        if (ctx->pdfstoponerror)
            return_error(gs_error_stackunderflow);
        pdf_clearstack(ctx);
        return 0;
    }

    for (i=0;i < 4;i++){
        num = (pdf_num *)ctx->stack_top[i - 4];
        if (num->type != PDF_INT) {
            if(num->type != PDF_REAL) {
                if (ctx->pdfstoponerror)
                    return_error(gs_error_typecheck);
                else {
                    pdf_pop(ctx, 4);
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
        pdf_pop(ctx, 2);
    if(code < 0 && ctx->pdfstoponerror)
        return code;
    else
        return 0;
}

int pdf_setcmykfill(pdf_context *ctx)
{
    pdf_num *num;
    double Values[4];
    int i, code;

    if (ctx->stack_top - ctx->stack_bot < 4) {
        if (ctx->pdfstoponerror)
            return_error(gs_error_stackunderflow);
        pdf_clearstack(ctx);
        return 0;
    }

    for (i=0;i < 4;i++){
        num = (pdf_num *)ctx->stack_top[i - 4];
        if (num->type != PDF_INT) {
            if(num->type != PDF_REAL) {
                if (ctx->pdfstoponerror)
                    return_error(gs_error_typecheck);
                else {
                    pdf_pop(ctx, 4);
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
        pdf_pop(ctx, 2);
    if(code < 0 && ctx->pdfstoponerror)
        return code;
    else
        return 0;
}

static int pdf_seticcbased(pdf_context *ctx, stream *s, int ncomps, float *range_buff)
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
    if (code < 0) {
    }

#if 0
    if (dict_find_string(ICCdict, "Name", &pnameval) > 0){
        uint size = r_size(pnameval);
        char *str = (char *)gs_alloc_bytes(gs_gstate_memory(igs), size+1, "seticc");
        memcpy(str, (const char *)pnameval->value.bytes, size);
        str[size] = 0;

        /* Compare this to the standard profile names */
        for (k = 0; k < GSICC_NUMBER_STANDARD_PROFILES; k++) {
            if ( strcmp( str, icc_std_profile_keys[k] ) == 0 ) {
                picc_profile = gsicc_get_profile_handle_file(icc_std_profile_names[k],
                    strlen(icc_std_profile_names[k]), gs_gstate_memory(ctx->pgs));
                break;
            }
        }
    } else {
#endif
        picc_profile = gsicc_profile_new(s, gs_gstate_memory(ctx->pgs), NULL, 0);
        if (picc_profile == NULL)
            return gs_throw(gs_error_VMerror, "Creation of ICC profile failed");
        /* We have to get the profile handle due to the fact that we need to know
           if it has a data space that is CIELAB */
        picc_profile->profile_handle =
            gsicc_get_profile_handle_buffer(picc_profile->buffer,
                                            picc_profile->buffer_size,
                                            gs_gstate_memory(ctx->pgs));
#if 0
    }
#endif

    if (picc_profile == NULL || picc_profile->profile_handle == NULL) {
        /* Free up everything, the profile is not valid. We will end up going
           ahead and using a default based upon the number of components */
        rc_decrement(picc_profile,"seticc");
        rc_decrement(pcs,"seticc");
        return -1;
    }
    code = gsicc_set_gscs_profile(pcs, picc_profile, gs_gstate_memory(ctx->pgs));
    if (code < 0) {
        rc_decrement(picc_profile,"seticc");
        rc_decrement(pcs,"seticc");
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
        rc_decrement(picc_profile,"seticc");
        rc_decrement(pcs,"seticc");
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
        rc_adjust(picc_profile, -2, "seticc");
        rc_increment(pcs->cmm_icc_profile_data);
    }
    /* Set the color space.  We are done.  No joint cache here... */
    code = gs_setcolorspace(ctx->pgs, pcs);
    /* The context has taken a reference to the colorspace. We no longer need
     * ours, so drop it. */
    rc_decrement_only(pcs, "seticc");
    /* In this case, we already have a ref count of 2 on the icc profile
       one for when it was created and one for when it was set.  We really
       only want one here so adjust */
    rc_decrement(picc_profile,"seticc");
    return code;
}

static int setcolorspace(pdf_context *ctx, pdf_array *color_array, int index, pdf_dict *stream_dict, pdf_dict *page_dict)
{
    int code;
    pdf_name *space = NULL;
    pdf_array *a;

    code = pdf_array_get(color_array, index, (pdf_obj **)&space);
    if(code == 0) {
        if (space->type == PDF_NAME) {
            code = gs_error_rangecheck;
            switch(space->length) {
                case 3:
                    if (memcmp(space->data, "Lab", space->length) == 0) {
                        code = gs_setrgbcolor(ctx->pgs, 1, 1, 1);
                    }
                    break;
                case 6:
                    if (memcmp(space->data, "CalRGB", space->length) == 0) {
                        code = gs_setrgbcolor(ctx->pgs, 1, 1, 1);
                    }
                    break;
                case 7:
                    if (memcmp(space->data, "CalGray", space->length) == 0) {
                        code = gs_setgray(ctx->pgs, 0);
                    }
                    if (memcmp(space->data, "Pattern", space->length) == 0) {
                        if (index != 0)
                            return_error(gs_error_syntaxerror);
                    }
                    if (memcmp(space->data, "DeviceN", space->length) == 0) {
                    }
                    if (memcmp(space->data, "Indexed", space->length) == 0) {
                        if (index != 0)
                            return_error(gs_error_syntaxerror);

                        code = setcolorspace(ctx, color_array, index + 1, stream_dict, page_dict);
                        if (code < 0 && ctx->pdfstoponerror)
                            return code;
                        return 0;
                    }
                    break;
                case 8:
                    if (memcmp(space->data, "ICCBased", space->length) == 0) {
                        pdf_dict *ICC_dict = NULL;

                        code = pdf_array_get(color_array, index + 1, (pdf_obj **)&ICC_dict);
                        if (code < 0) {
                            if (ctx->pdfstoponerror)
                                return code;
                            else
                                return code;
                        }
                        if (ICC_dict->type == PDF_INDIRECT) {
                            pdf_indirect_ref *r = (pdf_indirect_ref *)ICC_dict;
                            code = pdf_dereference(ctx, r->ref_object_num, r->ref_generation_num, (pdf_obj **)&ICC_dict);
                            pdf_countdown(r);
                            if (code < 0) {
                                if (ctx->pdfstoponerror)
                                    return code;
                                else
                                    return code;
                            }
                            pdf_countdown(ICC_dict);
                        }
                    }
                    break;
                case 9:
                    if (memcmp(space->data, "DeviceRGB", space->length) == 0) {
                        code = gs_setrgbcolor(ctx->pgs, 1, 1, 1);
                    }
                    break;
                case 10:
                    if (memcmp(space->data, "DeviceGray", space->length) == 0) {
                        code = gs_setgray(ctx->pgs, 0);
                    }
                    if (memcmp(space->data, "DeviceCMYK", space->length) == 0) {
                        code = gs_setcmykcolor(ctx->pgs, 0, 0, 0, 1);
                    }
                    if (memcmp(space->data, "Separation", space->length) == 0) {
                    }
                    break;
                default:
                    code = pdf_find_resource(ctx, (unsigned char *)"ColorSpace", space, stream_dict, page_dict, (pdf_obj **)&a);
                    if (code < 0) {
                        if (ctx->pdfstoponerror)
                            return code;
                        return 0;
                    }
                    if (a->type != PDF_ARRAY) {
                        pdf_countdown(a);
                        if (ctx->pdfstoponerror)
                            return_error(gs_error_typecheck);
                        return 0;
                    }

                    code = setcolorspace(ctx, a, 0, stream_dict, page_dict);
                    pdf_countdown(a);
                    if (code < 0 && ctx->pdfstoponerror)
                        return code;
                    return 0;
                    break;
            }
        } else
            code = gs_error_typecheck;
    }
    pdf_countdown(space);
    if(ctx->pdfstoponerror)
        return code;
    return 0;
}

int pdf_setstrokecolor_space(pdf_context *ctx, pdf_dict *stream_dict, pdf_dict *page_dict)
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
        pdf_pop(ctx, 1);
        if (ctx->pdfstoponerror)
            return_error(gs_error_stackunderflow);
        return 0;
    }
    n = (pdf_name *)ctx->stack_top[-1];
    switch(n->length) {
        case 9:
            if (memcmp(n->data, "DeviceRGB", 9) == 0) {
                code = gs_setrgbcolor(ctx->pgs, 0, 0, 0);
                if (code < 0) {
                    if (ctx->pdfstoponerror)
                        return code;
                }
                return 0;
            }
        case 10:
            if (memcmp(n->data, "DeviceGray", 9) == 0) {
                code = gs_setgray(ctx->pgs, 1);
                if (code < 0) {
                    if (ctx->pdfstoponerror)
                        return code;
                }
                return 0;
            }
            if (memcmp(n->data, "DeviceCMYK", 9) == 0) {
                code = gs_setcmykcolor(ctx->pgs, 0, 0, 0, 1);
                if (code < 0) {
                    if (ctx->pdfstoponerror)
                        return code;
                }
                return 0;
            }
        default:
            break;
    }
    code = pdf_find_resource(ctx, (unsigned char *)"ColorSpace", n, stream_dict, page_dict, (pdf_obj **)&a);
    if (code < 0) {
        pdf_pop(ctx, 1);
        if (ctx->pdfstoponerror)
            return code;
        return 0;
    }
    pdf_pop(ctx, 1);
    if (a->type != PDF_ARRAY) {
        if (ctx->pdfstoponerror)
            return_error(gs_error_typecheck);
        return 0;
    }

    code = setcolorspace(ctx, a, 0, stream_dict, page_dict);

    pdf_countdown(a);
    if (code < 0 && ctx->pdfstoponerror)
        return code;
    return 0;
}

int pdf_setfillcolor_space(pdf_context *ctx, pdf_dict *stream_dict, pdf_dict *page_dict)
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
        pdf_pop(ctx, 1);
        if (ctx->pdfstoponerror)
            return_error(gs_error_stackunderflow);
        return 0;
    }
    n = (pdf_name *)ctx->stack_top[-1];
    switch(n->length) {
        case 9:
            if (memcmp(n->data, "DeviceRGB", 9) == 0) {
                gs_swapcolors(ctx->pgs);
                code = gs_setrgbcolor(ctx->pgs, 0, 0, 0);
                gs_swapcolors(ctx->pgs);
                if (code < 0) {
                    if (ctx->pdfstoponerror)
                        return code;
                }
                return 0;
            }
        case 10:
            if (memcmp(n->data, "DeviceGray", 9) == 0) {
                gs_swapcolors(ctx->pgs);
                code = gs_setgray(ctx->pgs, 1);
                gs_swapcolors(ctx->pgs);
                if (code < 0) {
                    if (ctx->pdfstoponerror)
                        return code;
                }
                return 0;
            }
            if (memcmp(n->data, "DeviceCMYK", 9) == 0) {
                gs_swapcolors(ctx->pgs);
                code = gs_setcmykcolor(ctx->pgs, 0, 0, 0, 1);
                gs_swapcolors(ctx->pgs);
                if (code < 0) {
                    if (ctx->pdfstoponerror)
                        return code;
                }
                return 0;
            }
        default:
            break;
    }
    code = pdf_find_resource(ctx, (unsigned char *)"ColorSpace", n, stream_dict, page_dict, (pdf_obj **)&a);
    if (code < 0) {
        pdf_pop(ctx, 1);
        if (ctx->pdfstoponerror)
            return code;
        return 0;
    }
    pdf_pop(ctx, 1);
    if (a->type != PDF_ARRAY) {
        if (ctx->pdfstoponerror)
            return_error(gs_error_typecheck);
        return 0;
    }

    gs_swapcolors(ctx->pgs);
    code = setcolorspace(ctx, a, 0, stream_dict, page_dict);
    gs_swapcolors(ctx->pgs);

    pdf_countdown(a);
    if (code < 0 && ctx->pdfstoponerror)
        return code;
    return 0;
}

int pdf_setstrokecolor(pdf_context *ctx)
{
    const gs_color_space *  pcs = gs_currentcolorspace(ctx->pgs);
    int ncomps, i, code;
    gs_client_color cc;
    pdf_num *n;

    ncomps = cs_num_components(pcs);
    if (ctx->stack_top - ctx->stack_bot <= ncomps) {
        pdf_clearstack(ctx);
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
                pdf_clearstack(ctx);
                if (ctx->pdfstoponerror)
                    return_error(gs_error_typecheck);
                return 0;
            }
        }
    }
    pdf_pop(ctx, ncomps);
    code = gs_setcolor(ctx->pgs, &cc);
    if (code < 0 && ctx->pdfstoponerror)
        return code;
    return 0;
}

int pdf_setfillcolor(pdf_context *ctx)
{
    const gs_color_space *  pcs = gs_currentcolorspace(ctx->pgs);
    int ncomps, i, code;
    gs_client_color cc;
    pdf_num *n;

    ncomps = cs_num_components(pcs);
    if (ctx->stack_top - ctx->stack_bot <= ncomps) {
        pdf_clearstack(ctx);
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
                pdf_clearstack(ctx);
                if (ctx->pdfstoponerror)
                    return_error(gs_error_typecheck);
                return 0;
            }
        }
    }
    pdf_pop(ctx, ncomps);
    gs_swapcolors(ctx->pgs);
    code = gs_setcolor(ctx->pgs, &cc);
    gs_swapcolors(ctx->pgs);
    if (code < 0 && ctx->pdfstoponerror)
        return code;
    return 0;
}

int pdf_setstrokecolorN(pdf_context *ctx)
{
    const gs_color_space *  pcs = gs_currentcolorspace(ctx->pgs);
    int ncomps, i, code;
    gs_client_color cc;
    pdf_num *n;

    if (ctx->stack_top - ctx->stack_bot < 1) {
        pdf_clearstack(ctx);
        if(ctx->pdfstoponerror)
            return_error(gs_error_stackunderflow);
        return 0;
    }
    if (ctx->stack_top[-1]->type == PDF_NAME) {
        /* FIXME Patterns */
        pdf_clearstack(ctx);
        return 0;
    }
    ncomps = cs_num_components(pcs);
    if (ctx->stack_top - ctx->stack_bot <= ncomps) {
        pdf_clearstack(ctx);
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
                pdf_clearstack(ctx);
                if (ctx->pdfstoponerror)
                    return_error(gs_error_typecheck);
                return 0;
            }
        }
    }
    pdf_pop(ctx, ncomps);
    code = gs_setcolor(ctx->pgs, &cc);
    if (code < 0 && ctx->pdfstoponerror)
        return code;
    return 0;
}

int pdf_setfillcolorN(pdf_context *ctx)
{
    const gs_color_space *  pcs = gs_currentcolorspace(ctx->pgs);
    int ncomps, i, code;
    gs_client_color cc;
    pdf_num *n;

    if (ctx->stack_top - ctx->stack_bot < 1) {
        pdf_clearstack(ctx);
        if(ctx->pdfstoponerror)
            return_error(gs_error_stackunderflow);
        return 0;
    }
    if (ctx->stack_top[-1]->type == PDF_NAME) {
        /* FIXME Patterns */
        pdf_clearstack(ctx);
        return 0;
    }
    ncomps = cs_num_components(pcs);
    if (ctx->stack_top - ctx->stack_bot <= ncomps) {
        pdf_clearstack(ctx);
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
                pdf_clearstack(ctx);
                if (ctx->pdfstoponerror)
                    return_error(gs_error_typecheck);
                return 0;
            }
        }
    }
    pdf_pop(ctx, ncomps);
    gs_swapcolors(ctx->pgs);
    code = gs_setcolor(ctx->pgs, &cc);
    gs_swapcolors(ctx->pgs);
    if (code < 0 && ctx->pdfstoponerror)
        return code;
    return 0;
}

int pdf_ri(pdf_context *ctx)
{
    pdf_name *n;
    int code;

    if (ctx->stack_top - ctx->stack_bot >= 1)
        pdf_pop(ctx, 1);

    if (ctx->stack_top[-1]->type != PDF_NAME) {
        pdf_pop(ctx, 1);
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
    pdf_pop(ctx, 1);
    if (code < 0 && ctx->pdfstoponerror)
        return code;
    return 0;
}
