/* Copyright (C) 2018-2021 Artifex Software, Inc.
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
#include "pdf_doc.h"
#include "pdf_colour.h"
#include "pdf_pattern.h"
#include "pdf_stack.h"
#include "pdf_array.h"
#include "pdf_misc.h"
#include "gsicc_manage.h"
#include "gsicc_profilecache.h"
#include "gsicc_create.h"
#include "gsptype2.h"

#include "pdf_file.h"
#include "pdf_dict.h"
#include "pdf_loop_detect.h"
#include "pdf_func.h"
#include "pdf_shading.h"
#include "gscsepr.h"
#include "stream.h"
#include "strmio.h"
#include "gscdevn.h"
#include "gxcdevn.h"
#include "gscolor.h"    /* For gs_setgray() and friends */
#include "gsicc.h"      /* For gs_cspace_build_ICC() */
#include "gsstate.h"    /* For gs_gdsvae() and gs_grestore() */

/* Forward definitions for a routine we need */
static int pdfi_create_colorspace_by_array(pdf_context *ctx, pdf_array *color_array, int index,
                                           pdf_dict *stream_dict, pdf_dict *page_dict,
                                           gs_color_space **ppcs, bool inline_image);
static int pdfi_create_colorspace_by_name(pdf_context *ctx, pdf_name *name,
                                          pdf_dict *stream_dict, pdf_dict *page_dict,
                                          gs_color_space **ppcs, bool inline_image);

/* This is used only from the page level interpreter code, we need to know the number
 * of spot colours in a PDF file, which we have to pass to the device for spot colour
 * rendering. We deal with it here because its colour related.
 * The PDF context has a page-level object which maintains a list of the spot colour
 * names seen so far, so we can ensure we don't end up with duplictaes.
 */
static int pdfi_check_for_spots_by_name(pdf_context *ctx, pdf_name *name,
                                        pdf_dict *parent_dict, pdf_dict *page_dict, pdf_dict *spot_dict)
{
    pdf_obj *ref_space;
    int code;

    if (pdfi_name_is(name, "G")) {
        return 0;
    } else if (pdfi_name_is(name, "RGB")) {
        return 0;
    } else if (pdfi_name_is(name, "CMYK")) {
        return 0;
    } else if (pdfi_name_is(name, "DeviceRGB")) {
        return 0;
    } else if (pdfi_name_is(name, "DeviceGray")) {
        return 0;
    } else if (pdfi_name_is(name, "DeviceCMYK")) {
        return 0;
    } else if (pdfi_name_is(name, "Pattern")) {
        /* TODO: I think this is fine... */
        return 0;
    } else {
        code = pdfi_find_resource(ctx, (unsigned char *)"ColorSpace", name, parent_dict, page_dict, &ref_space);
        if (code < 0)
            return code;

        /* recursion */
        return pdfi_check_ColorSpace_for_spots(ctx, ref_space, parent_dict, page_dict, spot_dict);
    }
    return 0;
}

static int pdfi_check_for_spots_by_array(pdf_context *ctx, pdf_array *color_array,
                                         pdf_dict *parent_dict, pdf_dict *page_dict, pdf_dict *spot_dict)
{
    pdf_name *space = NULL;
    pdf_array *a = NULL;
    int code = 0;

    if (!spot_dict)
        return 0;

    code = pdfi_array_get_type(ctx, color_array, 0, PDF_NAME, (pdf_obj **)&space);
    if (code != 0)
        goto exit;

    code = 0;
    if (pdfi_name_is(space, "G")) {
        goto exit;
    } else if (pdfi_name_is(space, "I") || pdfi_name_is(space, "Indexed")) {
        pdf_obj *base_space;

        code = pdfi_array_get(ctx, color_array, 1, &base_space);
        if (code == 0) {
            code = pdfi_check_ColorSpace_for_spots(ctx, base_space, parent_dict, page_dict, spot_dict);
            (void)pdfi_countdown(base_space);
        }
        goto exit;
    } else if (pdfi_name_is(space, "Pattern")) {
        pdf_obj *base_space = NULL;
        uint64_t size = pdfi_array_size(color_array);

        /* Array of size 1 "[ /Pattern ]" is okay, just do nothing. */
        if (size == 1)
            goto exit;
        /* Array of size > 2 we don't handle (shouldn't happen?) */
        if (size != 2) {
            dbgmprintf1(ctx->memory,
                        "WARNING: checking Pattern for spots, expected array size 2, got %lu\n",
                        size);
            goto exit;
        }
        /* "[/Pattern base_space]" */
        code = pdfi_array_get(ctx, color_array, 1, &base_space);
        if (code == 0) {
            code = pdfi_check_ColorSpace_for_spots(ctx, base_space, parent_dict, page_dict, spot_dict);
            (void)pdfi_countdown(base_space);
        }
        goto exit;
    } else if (pdfi_name_is(space, "Lab")) {
        goto exit;
    } else if (pdfi_name_is(space, "RGB")) {
        goto exit;
    } else if (pdfi_name_is(space, "CMYK")) {
        goto exit;
    } else if (pdfi_name_is(space, "CalRGB")) {
        goto exit;
    } else if (pdfi_name_is(space, "CalGray")) {
        goto exit;
    } else if (pdfi_name_is(space, "ICCBased")) {
        goto exit;
    } else if (pdfi_name_is(space, "DeviceRGB")) {
        goto exit;
    } else if (pdfi_name_is(space, "DeviceGray")) {
        goto exit;
    } else if (pdfi_name_is(space, "DeviceCMYK")) {
        goto exit;
    } else if (pdfi_name_is(space, "DeviceN")) {
        bool known = false;
        pdf_obj *dummy, *name;
        int i;

        pdfi_countdown(space);
        code = pdfi_array_get_type(ctx, color_array, 1, PDF_ARRAY, (pdf_obj **)&space);
        if (code != 0)
            goto exit;

        for (i=0;i < pdfi_array_size((pdf_array *)space); i++) {
            code = pdfi_array_get_type(ctx, (pdf_array *)space, (uint64_t)i, PDF_NAME, &name);
            if (code < 0)
                goto exit;

            if (pdfi_name_is((const pdf_name *)name, "Cyan") || pdfi_name_is((const pdf_name *)name, "Magenta") ||
                pdfi_name_is((const pdf_name *)name, "Yellow") || pdfi_name_is((const pdf_name *)name, "Black") ||
                pdfi_name_is((const pdf_name *)name, "None") || pdfi_name_is((const pdf_name *)name, "All")) {

                pdfi_countdown(name);
                continue;
            }

            code = pdfi_dict_known_by_key(ctx, spot_dict, (pdf_name *)name, &known);
            if (code < 0) {
                pdfi_countdown(name);
                goto exit;
            }
            if (known) {
                pdfi_countdown(name);
                continue;
            }

            code = pdfi_object_alloc(ctx, PDF_INT, 0, &dummy);
            if (code < 0)
                goto exit;

            code = pdfi_dict_put_obj(ctx, spot_dict, name, dummy);
            pdfi_countdown(name);
            if (code < 0)
                break;
        }
        goto exit;
    } else if (pdfi_name_is(space, "Separation")) {
        bool known = false;
        pdf_obj *dummy;

        pdfi_countdown(space);
        code = pdfi_array_get_type(ctx, color_array, 1, PDF_NAME, (pdf_obj **)&space);
        if (code != 0)
            goto exit;

        if (pdfi_name_is((const pdf_name *)space, "Cyan") || pdfi_name_is((const pdf_name *)space, "Magenta") ||
            pdfi_name_is((const pdf_name *)space, "Yellow") || pdfi_name_is((const pdf_name *)space, "Black") ||
            pdfi_name_is((const pdf_name *)space, "None") || pdfi_name_is((const pdf_name *)space, "All"))
            goto exit;
        code = pdfi_dict_known_by_key(ctx, spot_dict, space, &known);
        if (code < 0 || known)
            goto exit;

        code = pdfi_object_alloc(ctx, PDF_INT, 0, &dummy);
        if (code < 0)
            goto exit;

        code = pdfi_dict_put_obj(ctx, spot_dict, (pdf_obj *)space, dummy);
        goto exit;
    } else {
        code = pdfi_find_resource(ctx, (unsigned char *)"ColorSpace",
                                  space, parent_dict, page_dict, (pdf_obj **)&a);
        if (code < 0)
            goto exit;

        if (a->type != PDF_ARRAY) {
            code = gs_note_error(gs_error_typecheck);
            goto exit;
        }

        /* recursion */
        code = pdfi_check_for_spots_by_array(ctx, a, parent_dict, page_dict, spot_dict);
    }

 exit:
    if (space)
        pdfi_countdown(space);
    if (a)
        pdfi_countdown(a);
    return code;
}

int pdfi_check_ColorSpace_for_spots(pdf_context *ctx, pdf_obj *space, pdf_dict *parent_dict,
                                    pdf_dict *page_dict, pdf_dict *spot_dict)
{
    int code;

    if (!spot_dict)
        return 0;

    code = pdfi_loop_detector_mark(ctx);
    if (code < 0)
        return code;

    if (space->type == PDF_NAME) {
        code = pdfi_check_for_spots_by_name(ctx, (pdf_name *)space, parent_dict, page_dict, spot_dict);
    } else {
        if (space->type == PDF_ARRAY) {
            code = pdfi_check_for_spots_by_array(ctx, (pdf_array *)space, parent_dict, page_dict, spot_dict);
        } else {
            pdfi_loop_detector_cleartomark(ctx);
            return 0;
        }
    }

    (void)pdfi_loop_detector_cleartomark(ctx);
    return code;
}

/* Rendering intent is a bit of an oddity, but its clearly colour related, so we
 * deal with it here. Cover it first to get it out of the way.
 */
int pdfi_ri(pdf_context *ctx)
{
    pdf_name *n;
    int code;

    if (pdfi_count_stack(ctx) < 1)
        return_error(gs_error_stackunderflow);

    if (ctx->stack_top[-1]->type != PDF_NAME) {
        pdfi_pop(ctx, 1);
        return_error(gs_error_typecheck);
    }
    n = (pdf_name *)ctx->stack_top[-1];
    code = pdfi_setrenderingintent(ctx, n);
    pdfi_pop(ctx, 1);
    return code;
}

/*
 * Pattern lifetime management turns out to be more complex than we would ideally like. Although
 * Patterns are reference counted, and contain a client_data pointer, they don't have a gs_notify
 * setup. This means that there's no simlpe way for us to be informed when a Pattern is released
 * We could patch up the Pattern finalize() method, replacing it with one of our own which calls
 * the original finalize() but that seems like a really nasty hack.
 * For the time being we put code in pdfi_grestore() to check for Pattern colour spaces being
 * restored away, but we also need to check for Pattern spaces being replaced in the current
 * graphics state. We define 'pdfi' variants of several graphics library colour management
 * functions to 'wrap' these with code to check for replacement of Patterns.
 * This comment is duplicated in pdf_pattern.c
 */

static void pdfi_cspace_free_callback(gs_memory_t * mem, void *cs)
{
    gs_color_space *pcs = (gs_color_space *)cs;
    pdf_context *ctx = (pdf_context *)pcs->interpreter_data;
    gs_function_t *pfn;

    if (gs_color_space_get_index(pcs) == gs_color_space_index_Separation) {
        /* Handle cleanup of Separation functions if applicable */
        pfn = gs_cspace_get_sepr_function(pcs);
        if (pfn)
            pdfi_free_function(ctx, pfn);
    }

    if (gs_color_space_get_index(pcs) == gs_color_space_index_DeviceN) {
        /* Handle cleanup of DeviceN functions if applicable */
        pfn = gs_cspace_get_devn_function(pcs);
        if (pfn)
            pdfi_free_function(ctx, pfn);
    }
}

int pdfi_gs_setgray(pdf_context *ctx, double d)
{
    int code = 0;

    /* PDF Reference 1.7 p423, any colour operators in a CharProc, following a d1, should be ignored */
    if (ctx->text.inside_CharProc && ctx->text.CharProc_is_d1)
        return 0;

    if (ctx->page.DefaultGray_cs != NULL) {
        gs_client_color cc;

        code = gs_setcolorspace(ctx->pgs, ctx->page.DefaultGray_cs);
        if (code < 0)
            return code;
        pdfi_set_colour_callback(ctx->pgs->color[0].color_space, ctx, NULL);
        cc.paint.values[0] = d;
        return gs_setcolor(ctx->pgs, &cc);
    } else {
        code = gs_setgray(ctx->pgs, d);
        if (code < 0)
            return code;
        pdfi_set_colour_callback(ctx->pgs->color[0].color_space, ctx, pdfi_cspace_free_callback);
    }
    return 0;
}

int pdfi_gs_setrgbcolor(pdf_context *ctx, double r, double g, double b)
{
    int code = 0;

    /* PDF Reference 1.7 p423, any colour operators in a CharProc, following a d1, should be ignored */
    if (ctx->text.inside_CharProc && ctx->text.CharProc_is_d1)
        return 0;

    if (ctx->page.DefaultRGB_cs != NULL) {
        gs_client_color cc;

        code = gs_setcolorspace(ctx->pgs, ctx->page.DefaultRGB_cs);
        if (code < 0)
            return code;
        pdfi_set_colour_callback(ctx->pgs->color[0].color_space, ctx, NULL);
        cc.paint.values[0] = r;
        cc.paint.values[1] = g;
        cc.paint.values[2] = b;
        return gs_setcolor(ctx->pgs, &cc);
    } else {
        code = gs_setrgbcolor(ctx->pgs, r, g, b);
        if (code < 0)
            return code;
        pdfi_set_colour_callback(ctx->pgs->color[0].color_space, ctx, pdfi_cspace_free_callback);
    }
    return 0;
}

static int pdfi_gs_setcmykcolor(pdf_context *ctx, double c, double m, double y, double k)
{
    int code = 0;

    /* PDF Reference 1.7 p423, any colour operators in a CharProc, following a d1, should be ignored */
    if (ctx->text.inside_CharProc && ctx->text.CharProc_is_d1)
        return 0;

    if (ctx->page.DefaultCMYK_cs != NULL) {
        gs_client_color cc;

        code = gs_setcolorspace(ctx->pgs, ctx->page.DefaultCMYK_cs);
        if (code < 0)
            return code;
        pdfi_set_colour_callback(ctx->pgs->color[0].color_space, ctx, NULL);
        cc.paint.values[0] = c;
        cc.paint.values[1] = m;
        cc.paint.values[2] = y;
        cc.paint.values[3] = k;
        return gs_setcolor(ctx->pgs, &cc);
    } else {
        code = gs_setcmykcolor(ctx->pgs, c, m, y, k);
        if (code < 0)
            return code;
        pdfi_set_colour_callback(ctx->pgs->color[0].color_space, ctx, pdfi_cspace_free_callback);
    }
    return 0;
}

int pdfi_gs_setcolorspace(pdf_context *ctx, gs_color_space *pcs)
{
    /* If the target colour space is already the current colour space, don't
     * bother to do anything.
     */
    if (ctx->pgs->color[0].color_space->id != pcs->id) {
        /* PDF Reference 1.7 p423, any colour operators in a CharProc, following a d1, should be ignored */
        if (ctx->text.inside_CharProc && ctx->text.CharProc_is_d1)
            return 0;

        pdfi_set_colour_callback(pcs, ctx, pdfi_cspace_free_callback);
        return gs_setcolorspace(ctx->pgs, pcs);
    }
    return 0;
}

/* Start with the simple cases, where we set the colour space and colour in a single operation */
int pdfi_setgraystroke(pdf_context *ctx)
{
    pdf_num *n1;
    int code;
    double d1;

    if (pdfi_count_stack(ctx) < 1)
        return_error(gs_error_stackunderflow);

    n1 = (pdf_num *)ctx->stack_top[-1];
    if (n1->type == PDF_INT){
        d1 = (double)n1->value.i;
    } else{
        if (n1->type == PDF_REAL) {
            d1 = n1->value.d;
        } else {
            pdfi_pop(ctx, 1);
            return_error(gs_error_typecheck);
        }
    }
    gs_swapcolors_quick(ctx->pgs);
    code = pdfi_gs_setgray(ctx, d1);
    gs_swapcolors_quick(ctx->pgs);
    pdfi_pop(ctx, 1);
    return code;
}

int pdfi_setgrayfill(pdf_context *ctx)
{
    pdf_num *n1;
    int code;
    double d1;

    if (pdfi_count_stack(ctx) < 1)
        return_error(gs_error_stackunderflow);

    n1 = (pdf_num *)ctx->stack_top[-1];
    if (n1->type == PDF_INT){
        d1 = (double)n1->value.i;
    } else{
        if (n1->type == PDF_REAL) {
            d1 = n1->value.d;
        } else {
            pdfi_pop(ctx, 1);
            return_error(gs_error_typecheck);
        }
    }
    code = pdfi_gs_setgray(ctx, d1);
    pdfi_pop(ctx, 1);
    return code;
}

int pdfi_setrgbstroke(pdf_context *ctx)
{
    pdf_num *num;
    double Values[3];
    int i, code;

    if (pdfi_count_stack(ctx) < 3) {
        pdfi_clearstack(ctx);
        return_error(gs_error_stackunderflow);
    }

    for (i=0;i < 3;i++){
        num = (pdf_num *)ctx->stack_top[i - 3];
        if (num->type != PDF_INT) {
            if(num->type != PDF_REAL) {
                pdfi_pop(ctx, 3);
                return_error(gs_error_typecheck);
            }
            else
                Values[i] = num->value.d;
        } else {
            Values[i] = (double)num->value.i;
        }
    }
    gs_swapcolors_quick(ctx->pgs);
    code = pdfi_gs_setrgbcolor(ctx, Values[0], Values[1], Values[2]);
    gs_swapcolors_quick(ctx->pgs);
    pdfi_pop(ctx, 3);
    return code;
}

/* Non-standard operator that is used in some annotation /DA
 * Expects stack to be [r g b]
 */
int pdfi_setrgbfill_array(pdf_context *ctx)
{
    int code;
    pdf_array *array = NULL;

    pdfi_set_warning(ctx, 0, NULL, W_PDF_NONSTANDARD_OP, "pdfi_setrgbfill_array", (char *)"WARNING: Non-standard 'r' operator");

    if (pdfi_count_stack(ctx) < 1)
        return_error(gs_error_stackunderflow);

    array = (pdf_array *)ctx->stack_top[-1];
    if (array->type != PDF_ARRAY) {
        code = gs_note_error(gs_error_typecheck);
        goto exit;
    }

    code = pdfi_setcolor_from_array(ctx, array);
 exit:
    pdfi_pop(ctx, 1);
    return code;
}

int pdfi_setrgbfill(pdf_context *ctx)
{
    pdf_num *num;
    double Values[3];
    int i, code;

    if (pdfi_count_stack(ctx) < 3) {
        pdfi_clearstack(ctx);
        return_error(gs_error_stackunderflow);
    }

    for (i=0;i < 3;i++){
        num = (pdf_num *)ctx->stack_top[i - 3];
        if (num->type != PDF_INT) {
            if(num->type != PDF_REAL) {
                pdfi_pop(ctx, 3);
                return_error(gs_error_typecheck);
            }
            else
                Values[i] = num->value.d;
        } else {
            Values[i] = (double)num->value.i;
        }
    }
    code = pdfi_gs_setrgbcolor(ctx, Values[0], Values[1], Values[2]);
    pdfi_pop(ctx, 3);
    return code;
}

int pdfi_setcmykstroke(pdf_context *ctx)
{
    pdf_num *num;
    double Values[4];
    int i, code;

    if (pdfi_count_stack(ctx) < 4) {
        pdfi_clearstack(ctx);
        return_error(gs_error_stackunderflow);
    }

    for (i=0;i < 4;i++){
        num = (pdf_num *)ctx->stack_top[i - 4];
        if (num->type != PDF_INT) {
            if(num->type != PDF_REAL) {
                pdfi_pop(ctx, 4);
                return_error(gs_error_typecheck);
            }
            else
                Values[i] = num->value.d;
        } else {
            Values[i] = (double)num->value.i;
        }
    }
    gs_swapcolors_quick(ctx->pgs);
    code = pdfi_gs_setcmykcolor(ctx, Values[0], Values[1], Values[2], Values[3]);
    gs_swapcolors_quick(ctx->pgs);
    pdfi_pop(ctx, 4);
    return code;
}

int pdfi_setcmykfill(pdf_context *ctx)
{
    pdf_num *num;
    double Values[4];
    int i, code;

    if (pdfi_count_stack(ctx) < 4) {
        pdfi_clearstack(ctx);
        return_error(gs_error_stackunderflow);
    }

    for (i=0;i < 4;i++){
        num = (pdf_num *)ctx->stack_top[i - 4];
        if (num->type != PDF_INT) {
            if(num->type != PDF_REAL) {
                pdfi_pop(ctx, 4);
                return_error(gs_error_typecheck);
            }
            else
                Values[i] = num->value.d;
        } else {
            Values[i] = (double)num->value.i;
        }
    }
    code = pdfi_gs_setcmykcolor(ctx, Values[0], Values[1], Values[2], Values[3]);
    pdfi_pop(ctx, 4);
    return code;
}

/* Do a setcolor using values in an array
 * Will do gray, rgb, cmyk for sizes 1,3,4
 * Anything else is an error
 */
int pdfi_setcolor_from_array(pdf_context *ctx, pdf_array *array)
{
    int code = 0;
    uint64_t size;
    double values[4];

    size = pdfi_array_size(array);
    if (size != 1 && size != 3 && size != 4) {
        code = gs_note_error(gs_error_rangecheck);
        goto exit;
    }

    code = pdfi_array_to_num_array(ctx, array, values, 0, size);
    if (code < 0) goto exit;

    switch (size) {
    case 1:
        code = pdfi_gs_setgray(ctx, values[0]);
        break;
    case 3:
        code = pdfi_gs_setrgbcolor(ctx, values[0], values[1], values[2]);
        break;
    case 4:
        code = pdfi_gs_setcmykcolor(ctx, values[0], values[1], values[2], values[3]);
        break;
    default:
        break;
    }

 exit:
    return code;
}

/* Get colors from top of stack into a client color */
static int
pdfi_get_color_from_stack(pdf_context *ctx, gs_client_color *cc, int ncomps)
{
    int i;
    pdf_num *n;

    if (pdfi_count_stack(ctx) < ncomps) {
        pdfi_clearstack(ctx);
        return_error(gs_error_stackunderflow);
    }
    for (i=0;i<ncomps;i++){
        n = (pdf_num *)ctx->stack_top[i - ncomps];
        if (n->type == PDF_INT) {
            cc->paint.values[i] = (float)n->value.i;
        } else {
            if (n->type == PDF_REAL) {
                cc->paint.values[i] = n->value.d;
            } else {
                pdfi_clearstack(ctx);
                return_error(gs_error_typecheck);
            }
        }
    }
    pdfi_pop(ctx, ncomps);
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
    const gs_color_space *  pcs;
    int ncomps, code;
    gs_client_color cc;

    gs_swapcolors_quick(ctx->pgs);
    pcs = gs_currentcolorspace(ctx->pgs);
    ncomps = cs_num_components(pcs);
    code = pdfi_get_color_from_stack(ctx, &cc, ncomps);
    if (code == 0) {
        code = gs_setcolor(ctx->pgs, &cc);
    }
    gs_swapcolors_quick(ctx->pgs);
    return code;
}

int pdfi_setfillcolor(pdf_context *ctx)
{
    const gs_color_space *  pcs = gs_currentcolorspace(ctx->pgs);
    int ncomps, code;
    gs_client_color cc;

    ncomps = cs_num_components(pcs);
    code = pdfi_get_color_from_stack(ctx, &cc, ncomps);
    if (code == 0) {
        code = gs_setcolor(ctx->pgs, &cc);
    }
    return code;
}

static inline bool
pattern_instance_uses_base_space(const gs_pattern_instance_t * pinst)
{
    return pinst->type->procs.uses_base_space(
                   pinst->type->procs.get_pattern(pinst) );
}

/* Now the SCN and scn operators. These set the colour for special spaces;
 * ICCBased, Pattern, Separation and DeviceN
 */
int
pdfi_setcolorN(pdf_context *ctx, pdf_dict *stream_dict, pdf_dict *page_dict, bool is_fill)
{
    gs_color_space *pcs;
    gs_color_space *base_space = NULL;
    int ncomps=0, code = 0;
    gs_client_color cc;
    bool is_pattern = false;

    if (!is_fill) {
        gs_swapcolors_quick(ctx->pgs);
    }
    pcs = gs_currentcolorspace(ctx->pgs);

    if (pdfi_count_stack(ctx) < 1) {
        code = gs_note_error(gs_error_stackunderflow);
        goto cleanupExit;
    }

    if (pcs->type == &gs_color_space_type_Pattern)
        is_pattern = true;
    if (is_pattern) {
        if (ctx->stack_top[-1]->type != PDF_NAME) {
            pdfi_clearstack(ctx);
            code = gs_note_error(gs_error_syntaxerror);
            goto cleanupExit;
        }
        base_space = pcs->base_space;
        code = pdfi_pattern_set(ctx, stream_dict, page_dict, (pdf_name *)ctx->stack_top[-1], &cc);
        pdfi_pop(ctx, 1);
        if (code < 0) {
            /* Ignore the pattern if we failed to set it */
            pdfi_set_warning(ctx, 0, NULL, W_PDF_BADPATTERN, "pdfi_setcolorN", (char *)"PATTERN: Error setting pattern");
            code = 0;
            goto cleanupExit;
        }
        if (base_space && pattern_instance_uses_base_space(cc.pattern))
            ncomps = cs_num_components(base_space);
        else
            ncomps = 0;
    } else {
        ncomps = cs_num_components(pcs);
        cc.pattern = NULL;
    }

    if (ncomps > 0)
        code = pdfi_get_color_from_stack(ctx, &cc, ncomps);
    if (code < 0)
        goto cleanupExit;

    if (pcs->type == &gs_color_space_type_Indexed) {
        if (cc.paint.values[0] < 0)
            cc.paint.values[0] = 0.0;
        else
        {
            if (cc.paint.values[0] > pcs->params.indexed.hival)
                cc.paint.values[0] = (float)pcs->params.indexed.hival;
            else
            {
                if (cc.paint.values[0] != floor(cc.paint.values[0]))
                {
                    if (cc.paint.values[0] - floor(cc.paint.values[0]) < 0.5)
                        cc.paint.values[0] = floor(cc.paint.values[0]);
                    else
                        cc.paint.values[0] = ceil(cc.paint.values[0]);
                }
            }
        }
    }

    code = gs_setcolor(ctx->pgs, &cc);

    if (is_pattern)
        /* cc is a local scope variable, holding a reference to a pattern.
         * We need to count the refrence down before the variable goes out of scope
         * in order to prevent the pattern leaking.
         */
        rc_decrement(cc.pattern, "pdfi_setcolorN");

cleanupExit:
    if (!is_fill)
        gs_swapcolors_quick(ctx->pgs);
    return code;
}

/* And now, the routines to set the colour space on its own. */

/* Starting with the ICCBased colour space */

/* This routine is mostly a copy of seticc() in zicc.c */
static int pdfi_create_icc(pdf_context *ctx, char *Name, stream *s, int ncomps, int *icc_N, float *range_buff, gs_color_space **ppcs)
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

    if (ppcs!= NULL)
        *ppcs = NULL;

    /* We need to use the graphics state memory, and beyond that we need to use stable memory, because the
     * ICC cache can persist until the end of job, and so the profile (which is stored in teh cache)
     * must likewise persist.
     */
    code = gs_cspace_build_ICC(&pcs, NULL, (gs_gstate_memory(ctx->pgs))->stable_memory);
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
        if (s == NULL)
            return_error(gs_error_undefined);

        picc_profile = gsicc_profile_new(s, gs_gstate_memory(ctx->pgs), NULL, 0);
        if (picc_profile == NULL) {
            rc_decrement(pcs,"pdfi_create_icc");
            return gs_throw(gs_error_VMerror, "pdfi_create_icc Creation of ICC profile failed");
        }
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
    /* Return the number of components the ICC profile has */
    *icc_N = expected;
    if (expected != ncomps)
        ncomps = expected;

#if 0
    if (!expected || ncomps != expected) {
        rc_decrement(picc_profile,"pdfi_create_icc");
        rc_decrement(pcs,"pdfi_create_icc");
        return_error(gs_error_rangecheck);
    }
#endif

    picc_profile->num_comps = ncomps;
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

    if (ppcs!= NULL){
        *ppcs = pcs;
        pdfi_set_colour_callback(pcs, ctx, pdfi_cspace_free_callback);
    } else {
        code = pdfi_gs_setcolorspace(ctx, pcs);
        rc_decrement_only_cs(pcs, "pdfi_seticc_cal");
    }

    /* The context has taken a reference to the colorspace. We no longer need
     * ours, so drop it. */
    rc_decrement(picc_profile, "pdfi_create_icc");
    return code;
}

static int pdfi_create_iccprofile(pdf_context *ctx, pdf_stream *ICC_obj, char *cname,
                                  int64_t Length, int N, int *icc_N, float *range, gs_color_space **ppcs)
{
    pdf_c_stream *profile_stream = NULL;
    byte *profile_buffer;
    gs_offset_t savedoffset;
    int code, code1;

    /* Save the current stream position, and move to the start of the profile stream */
    savedoffset = pdfi_tell(ctx->main_stream);
    pdfi_seek(ctx, ctx->main_stream, pdfi_stream_offset(ctx, ICC_obj), SEEK_SET);

    /* The ICC profile reading code (irritatingly) requires a seekable stream, because it
     * rewinds it to the start, then seeks to the end to find the size, then rewinds the
     * stream again.
     * Ideally we would use a ReusableStreamDecode filter here, but that is largely
     * implemented in PostScript (!) so we can't use it. What we can do is create a
     * string sourced stream in memory, which is at least seekable.
     */
    code = pdfi_open_memory_stream_from_filtered_stream(ctx, ICC_obj, Length, &profile_buffer, ctx->main_stream, &profile_stream, true);
    if (code < 0) {
        pdfi_seek(ctx, ctx->main_stream, savedoffset, SEEK_SET);
        return code;
    }

    /* Now, finally, we can call the code to create and set the profile */
    code = pdfi_create_icc(ctx, cname, profile_stream->s, (int)N, icc_N, range, ppcs);

    code1 = pdfi_close_memory_stream(ctx, profile_buffer, profile_stream);

    if (code == 0)
        code = code1;

    pdfi_seek(ctx, ctx->main_stream, savedoffset, SEEK_SET);

    return code;
}

static int pdfi_create_iccbased(pdf_context *ctx, pdf_array *color_array, int index, pdf_dict *stream_dict, pdf_dict *page_dict, gs_color_space **ppcs, bool inline_image)
{
    pdf_stream *ICC_obj = NULL;
    pdf_dict *dict; /* Alias to avoid tons of casting */
    pdf_array *a;
    int64_t Length, N;
    pdf_obj *Name = NULL;
    char *cname = NULL;
    int code;
    bool known = true;
    float range[8];
    int icc_N;
    gs_color_space *pcs = NULL;

    code = pdfi_array_get_type(ctx, color_array, index + 1, PDF_STREAM, (pdf_obj **)&ICC_obj);
    if (code < 0)
        return code;
    code = pdfi_dict_from_obj(ctx, (pdf_obj *)ICC_obj, &dict);
    if (code < 0)
        return code;

    Length = pdfi_stream_length(ctx, ICC_obj);
    code = pdfi_dict_get_int(ctx, dict, "N", &N);
    if (code < 0)
        goto done;
    code = pdfi_dict_knownget(ctx, dict, "Name", &Name);
    if (code > 0) {
        if(Name->type == PDF_STRING || Name->type == PDF_NAME) {
            cname = (char *)gs_alloc_bytes(ctx->memory, ((pdf_name *)Name)->length + 1, "pdfi_create_iccbased (profile name)");
            if (cname == NULL) {
                code = gs_note_error(gs_error_VMerror);
                goto done;
            }
            memset(cname, 0x00, ((pdf_name *)Name)->length + 1);
            memcpy(cname, ((pdf_name *)Name)->data, ((pdf_name *)Name)->length);
        }
    }
    if (code < 0)
        goto done;

    code = pdfi_dict_knownget_type(ctx, dict, "Range", PDF_ARRAY, (pdf_obj **)&a);
    if (code < 0)
        goto done;
    if (code > 0) {
        double dbl;
        int i;

        if (pdfi_array_size(a) >= N * 2) {
            for (i = 0; i < pdfi_array_size(a);i++) {
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
        pdfi_countdown(a);
    } else
        known = false;

    /* We don't just use the final else clause above for setting the defaults
     * because we also want to use these if there's a problem with the
     * supplied data. In this case we also want to overwrite any partial
     * data we might have read
     */
    if (!known) {
        int i;
        for (i = 0;i < N; i++) {
            range[i * 2] = 0;
            range[(i * 2) + 1] = 1;
        }
    }

    code = pdfi_create_iccprofile(ctx, ICC_obj, cname, Length, N, &icc_N, range, &pcs);

    /* This is just plain hackery for the benefit of Bug696690.pdf. The old PostScript PDF interpreter says:
     * %% This section is to deal with the horrible pair of files in Bug #696690 and Bug #696120
     * %% These files have ICCBased spaces where the value of /N and the number of components
     * %% in the profile differ. In addition the profile in Bug #696690 is invalid. In the
     * %% case of Bug #696690 the /N value is correct, and the profile is wrong, in the case
     * %% of Bug #696120 the /N value is incorrect and the profile is correct.
     * %% We 'suspect' that Acrobat uses the fact that Bug #696120 is a pure image to detect
     * %% that the /N is incorrect, we can't be sure whether it uses the profile or just uses
     * %% the /N to decide on a device space.
     * We can't precisely duplicate the PostScript approach, but we now set the actual ICC profile
     * and therefore use the number of components in the profile. However, we pass back the number
     * of components in icc_N. We then check to see if N and icc_N are the same, if they are not we
     * try to set a devcie colour using the profile. If that fails (bad profile) then we enter the fallback
     * just as if we had failed to set the profile.
     */
    if (code >= 0 && N != icc_N) {
        gs_client_color cc;
        int i;

        gs_gsave(ctx->pgs);
        code = gs_setcolorspace(ctx->pgs, pcs);
        if (code == 0) {
            cc.pattern = 0;
            for (i = 0;i < icc_N; i++)
                cc.paint.values[i] = 0;
            code = gs_setcolor(ctx->pgs, &cc);
            if (code == 0)
                code = gx_set_dev_color(ctx->pgs);
        }
        gs_grestore(ctx->pgs);
    }

    if (code < 0) {
        pdf_obj *Alternate = NULL;

        if (pcs != NULL)
            rc_decrement(pcs,"pdfi_create_iccbased");

        /* Failed to set the ICCBased space, attempt to use the Alternate */
        code = pdfi_dict_knownget(ctx, dict, "Alternate", &Alternate);
        if (code > 0) {
            /* The Alternate should be one of the device spaces, therefore a Name object. If its not, fallback to using /N */
            if (Alternate->type == PDF_NAME)
                code = pdfi_create_colorspace_by_name(ctx, (pdf_name *)Alternate, stream_dict,
                                                      page_dict, ppcs, inline_image);
            pdfi_countdown(Alternate);
            if (code == 0) {
                pdfi_set_warning(ctx, 0, NULL, W_PDF_BADICC_USE_ALT, "pdfi_create_iccbased", NULL);
                goto done;
            }
        }
        /* Use the number of components *from the profile* to set a space.... */
        pdfi_set_warning(ctx, 0, NULL, W_PDF_BADICC_USECOMPS, "pdfi_create_iccbased", NULL);
        switch(N) {
            case 1:
                pcs = gs_cspace_new_DeviceGray(ctx->memory);
                if (pcs == NULL)
                    code = gs_note_error(gs_error_VMerror);
                break;
            case 3:
                pcs = gs_cspace_new_DeviceRGB(ctx->memory);
                if (pcs == NULL)
                    code = gs_note_error(gs_error_VMerror);
                break;
            case 4:
                pcs = gs_cspace_new_DeviceCMYK(ctx->memory);
                if (pcs == NULL)
                    code = gs_note_error(gs_error_VMerror);
                break;
            default:
                code = gs_note_error(gs_error_undefined);
                break;
        }
    }
    if (ppcs!= NULL)
        *ppcs = pcs;
    else {
        if (pcs != NULL) {
            code = pdfi_gs_setcolorspace(ctx, pcs);
            /* release reference from construction */
            rc_decrement_only_cs(pcs, "setseparationspace");
        }
    }


done:
    if (cname)
        gs_free_object(ctx->memory, cname, "pdfi_create_iccbased (profile name)");
    pdfi_countdown(Name);
    pdfi_countdown(ICC_obj);
    return code;
}

/*
 * This, and pdfi_set_cal() below are copied from the similarly named routines
 * in zicc.c
 */
/* Install a ICC type color space and use the ICC LABLUT profile. */
static int
pdfi_seticc_lab(pdf_context *ctx, float *range_buff, gs_color_space **ppcs)
{
    int                     code;
    gs_color_space *        pcs;
    int                     i;

    /* build the color space object */
    /* We need to use the graphics state memory, and beyond that we need to use stable memory, because the
     * ICC cache can persist until the end of job, and so the profile (which is stored in teh cache)
     * must likewise persist.
     */
    code = gs_cspace_build_ICC(&pcs, NULL, (gs_gstate_memory(ctx->pgs))->stable_memory);
    if (code < 0)
        return code;

    /* record the current space as the alternative color space */
    /* Get the lab profile.  It may already be set in the icc manager.
       If not then lets populate it.  */
    if (ctx->pgs->icc_manager->lab_profile == NULL ) {
        /* This can't happen as the profile
           should be initialized during the
           setting of the user params */
        return_error(gs_error_unknownerror);
    }
    /* Assign the LAB to LAB profile to this color space */
    code = gsicc_set_gscs_profile(pcs, ctx->pgs->icc_manager->lab_profile, gs_gstate_memory(ctx->pgs));
    if (code < 0)
        return code;

    pcs->cmm_icc_profile_data->Range.ranges[0].rmin = 0.0;
    pcs->cmm_icc_profile_data->Range.ranges[0].rmax = 100.0;
    for (i = 1; i < 3; i++) {
        pcs->cmm_icc_profile_data->Range.ranges[i].rmin =
            range_buff[2 * (i-1)];
        pcs->cmm_icc_profile_data->Range.ranges[i].rmax =
            range_buff[2 * (i-1) + 1];
    }
    if (ppcs!= NULL){
        *ppcs = pcs;
        pdfi_set_colour_callback(pcs, ctx, pdfi_cspace_free_callback);
    } else {
        code = pdfi_gs_setcolorspace(ctx, pcs);
        rc_decrement_only_cs(pcs, "pdfi_seticc_lab");
    }

    return code;
}

static int pdfi_create_Lab(pdf_context *ctx, pdf_array *color_array, int index, pdf_dict *stream_dict, pdf_dict *page_dict, gs_color_space **ppcs)
{
    int code = 0, i;
    pdf_dict *Lab_dict = NULL;
    pdf_array *Range = NULL;
    float RangeBuf[4];
    double f;

    code = pdfi_array_get_type(ctx, color_array, index + 1, PDF_DICT, (pdf_obj **)&Lab_dict);
    if (code < 0)
        return code;

    code = pdfi_dict_get_type(ctx, Lab_dict, "Range", PDF_ARRAY, (pdf_obj **)&Range);
    if (code < 0) {
        goto exit;
    }
    if (pdfi_array_size(Range) != 4){
        code = gs_note_error(gs_error_rangecheck);
        goto exit;
    }

    for (i=0; i < 4; i++) {
        code = pdfi_array_get_number(ctx, Range, (uint64_t)i, &f);
        if (code < 0)
            goto exit;
        RangeBuf[i] = (float)f;
    }

    code = pdfi_seticc_lab(ctx, RangeBuf, ppcs);

exit:
    pdfi_countdown(Lab_dict);
    pdfi_countdown(Range);
    return code;
}

/* Install an ICC space from the PDF CalRGB or CalGray types */
static int
pdfi_seticc_cal(pdf_context *ctx, float *white, float *black, float *gamma,
           float *matrix, int num_colorants, ulong dictkey, gs_color_space **ppcs)
{
    int                     code = 0;
    gs_color_space *        pcs;
    int                     i;
    cmm_profile_t           *cal_profile;

    /* See if the color space is in the profile cache */
    pcs = gsicc_find_cs(dictkey, ctx->pgs);
    if (pcs == NULL ) {
        /* build the color space object.  Since this is cached
           in the profile cache which is a member variable
           of the graphic state, we will want to use stable
           memory here */
        code = gs_cspace_build_ICC(&pcs, NULL, ctx->pgs->memory->stable_memory);
        if (code < 0)
            return code;
        /* There is no alternate for this.  Perhaps we should set DeviceRGB? */
        pcs->base_space = NULL;
        /* Create the ICC profile from the CalRGB or CalGray parameters */
        /* We need to use the graphics state memory, in case we are running under Ghostscript. */
        cal_profile = gsicc_create_from_cal(white, black, gamma, matrix,
                                            ctx->pgs->memory->stable_memory, num_colorants);
        if (cal_profile == NULL) {
            rc_decrement(pcs, "seticc_cal");
            return_error(gs_error_VMerror);
        }
        /* Assign the profile to this color space */
        /* Apparently the memory pointer passed here here is not actually used, but we will supply
         * the graphics state memory allocator, because that's what the colour space should be using.
         */
        code = gsicc_set_gscs_profile(pcs, cal_profile, ctx->pgs->memory);
        /* profile is created with ref count of 1, gsicc_set_gscs_profile()
         * increments the ref count, so we need to decrement it here.
         */
        rc_decrement(cal_profile, "seticc_cal");
        if (code < 0) {
            rc_decrement(pcs, "seticc_cal");
            return code;
        }
        for (i = 0; i < num_colorants; i++) {
            pcs->cmm_icc_profile_data->Range.ranges[i].rmin = 0;
            pcs->cmm_icc_profile_data->Range.ranges[i].rmax = 1;
        }
        /* Add the color space to the profile cache */
        gsicc_add_cs(ctx->pgs, pcs, dictkey);
    } else {
        /* We're passing back a new reference, increment the count */
        rc_adjust_only(pcs, 1, "pdfi_seticc_cal, return cached ICC profile");
    }

    if (ppcs!= NULL){
        *ppcs = pcs;
    } else {
        code = pdfi_gs_setcolorspace(ctx, pcs);
        rc_decrement_only_cs(pcs, "pdfi_seticc_cal");
    }

    return code;
}

static int pdfi_create_CalGray(pdf_context *ctx, pdf_array *color_array, int index, pdf_dict *stream_dict, pdf_dict *page_dict, gs_color_space **ppcs)
{
    int code = 0, i;
    pdf_dict *CalGray_dict = NULL;
    pdf_array *PDFArray = NULL;
    /* The default values here are as per the PDF 1.7 specification, there is
     * no default for the WhitePoint as it is a required entry. The Matrix is
     * not specified for CalGray, but we need it for the general 'pdfi_set_icc'
     * routine, so we use the same default as CalRGB.
     */
    float WhitePoint[3], BlackPoint[3] = {0.0f, 0.0f, 0.0f}, Gamma = 1.0f;
    float Matrix[9] = {1.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f, 1.0f};
    double f;

    code = pdfi_array_get_type(ctx, color_array, index + 1, PDF_DICT, (pdf_obj **)&CalGray_dict);
    if (code < 0)
        return code;

    code = pdfi_dict_get_type(ctx, CalGray_dict, "WhitePoint", PDF_ARRAY, (pdf_obj **)&PDFArray);
    if (code < 0) {
        pdfi_countdown(PDFArray);
        goto exit;
    }
    if (pdfi_array_size(PDFArray) != 3){
        code = gs_note_error(gs_error_rangecheck);
        goto exit;
    }

    for (i=0; i < 3; i++) {
        code = pdfi_array_get_number(ctx, PDFArray, (uint64_t)i, &f);
        if (code < 0)
            goto exit;
        WhitePoint[i] = (float)f;
    }
    pdfi_countdown(PDFArray);
    PDFArray = NULL;

    /* Check the WhitePoint values, the PDF 1.7 reference states that
     * Xw ad Zw must be positive and Yw must be 1.0
     */
    if (WhitePoint[0] < 0 || WhitePoint[2] < 0 || WhitePoint[1] != 1.0f) {
        code = gs_note_error(gs_error_rangecheck);
        goto exit;
    }

    if (pdfi_dict_knownget_type(ctx, CalGray_dict, "BlackPoint", PDF_ARRAY, (pdf_obj **)&PDFArray)) {
        if (pdfi_array_size(PDFArray) != 3){
            code = gs_note_error(gs_error_rangecheck);
            goto exit;
        }
        for (i=0; i < 3; i++) {
            code = pdfi_array_get_number(ctx, PDFArray, (uint64_t)i, &f);
            if (code < 0)
                goto exit;
            /* The PDF 1.7 reference states that all three components of the BlackPoint
             * (if present) must be positive.
             */
            if (f < 0) {
                code = gs_note_error(gs_error_rangecheck);
                goto exit;
            }
            BlackPoint[i] = (float)f;
        }
        pdfi_countdown(PDFArray);
        PDFArray = NULL;
    }

    if (pdfi_dict_knownget_number(ctx, CalGray_dict, "Gamma", &f))
        Gamma = (float)f;
    /* The PDF 1.7 reference states that Gamma
     * (if present) must be positive.
     */
    if (Gamma < 0) {
        code = gs_note_error(gs_error_rangecheck);
        goto exit;
    }

    code = pdfi_seticc_cal(ctx, WhitePoint, BlackPoint, &Gamma, Matrix, 1, color_array->object_num, ppcs);

exit:
    pdfi_countdown(PDFArray);
    pdfi_countdown(CalGray_dict);
    return code;
}

static int pdfi_create_CalRGB(pdf_context *ctx, pdf_array *color_array, int index, pdf_dict *stream_dict, pdf_dict *page_dict, gs_color_space **ppcs)
{
    int code = 0, i;
    pdf_dict *CalRGB_dict = NULL;
    pdf_array *PDFArray = NULL;
    /* The default values here are as per the PDF 1.7 specification, there is
     * no default for the WhitePoint as it is a required entry
     */
    float WhitePoint[3], BlackPoint[3] = {0.0f, 0.0f, 0.0f}, Gamma[3] = {1.0f, 1.0f, 1.0f};
    float Matrix[9] = {1.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f, 1.0f};
    double f;

    code = pdfi_array_get_type(ctx, color_array, index + 1, PDF_DICT, (pdf_obj **)&CalRGB_dict);
    if (code < 0)
        return code;

    code = pdfi_dict_get_type(ctx, CalRGB_dict, "WhitePoint", PDF_ARRAY, (pdf_obj **)&PDFArray);
    if (code < 0) {
        pdfi_countdown(PDFArray);
        goto exit;
    }
    if (pdfi_array_size(PDFArray) != 3){
        code = gs_note_error(gs_error_rangecheck);
        goto exit;
    }

    for (i=0; i < 3; i++) {
        code = pdfi_array_get_number(ctx, PDFArray, (uint64_t)i, &f);
        if (code < 0)
            goto exit;
        WhitePoint[i] = (float)f;
    }
    pdfi_countdown(PDFArray);
    PDFArray = NULL;

    /* Check the WhitePoint values, the PDF 1.7 reference states that
     * Xw ad Zw must be positive and Yw must be 1.0
     */
    if (WhitePoint[0] < 0 || WhitePoint[2] < 0 || WhitePoint[1] != 1.0f) {
        code = gs_note_error(gs_error_rangecheck);
        goto exit;
    }

    if (pdfi_dict_knownget_type(ctx, CalRGB_dict, "BlackPoint", PDF_ARRAY, (pdf_obj **)&PDFArray)) {
        if (pdfi_array_size(PDFArray) != 3){
            code = gs_note_error(gs_error_rangecheck);
            goto exit;
        }
        for (i=0; i < 3; i++) {
            code = pdfi_array_get_number(ctx, PDFArray, (uint64_t)i, &f);
            if (code < 0)
                goto exit;
            /* The PDF 1.7 reference states that all three components of the BlackPoint
             * (if present) must be positive.
             */
            if (f < 0) {
                code = gs_note_error(gs_error_rangecheck);
                goto exit;
            }
            BlackPoint[i] = (float)f;
        }
        pdfi_countdown(PDFArray);
        PDFArray = NULL;
    }

    if (pdfi_dict_knownget_type(ctx, CalRGB_dict, "Gamma", PDF_ARRAY, (pdf_obj **)&PDFArray)) {
        if (pdfi_array_size(PDFArray) != 3){
            code = gs_note_error(gs_error_rangecheck);
            goto exit;
        }
        for (i=0; i < 3; i++) {
            code = pdfi_array_get_number(ctx, PDFArray, (uint64_t)i, &f);
            if (code < 0)
                goto exit;
            Gamma[i] = (float)f;
        }
        pdfi_countdown(PDFArray);
        PDFArray = NULL;
    }

    if (pdfi_dict_knownget_type(ctx, CalRGB_dict, "Matrix", PDF_ARRAY, (pdf_obj **)&PDFArray)) {
        if (pdfi_array_size(PDFArray) != 9){
            code = gs_note_error(gs_error_rangecheck);
            goto exit;
        }
        for (i=0; i < 9; i++) {
            code = pdfi_array_get_number(ctx, PDFArray, (uint64_t)i, &f);
            if (code < 0)
                goto exit;
            Matrix[i] = (float)f;
        }
        pdfi_countdown(PDFArray);
        PDFArray = NULL;
    }
    code = pdfi_seticc_cal(ctx, WhitePoint, BlackPoint, Gamma, Matrix, 3, color_array->object_num, ppcs);

exit:
    pdfi_countdown(PDFArray);
    pdfi_countdown(CalRGB_dict);
    return code;
}

static int pdfi_create_Separation(pdf_context *ctx, pdf_array *color_array, int index, pdf_dict *stream_dict, pdf_dict *page_dict, gs_color_space **ppcs, bool inline_image)
{
    pdf_obj *o = NULL;
    pdf_name *name = NULL, *NamedAlternate = NULL;
    pdf_array *ArrayAlternate = NULL;
    pdf_obj *transform = NULL;
    int code;
    gs_color_space *pcs = NULL, *pcs_alt = NULL;
    gs_function_t * pfn = NULL;
    separation_type sep_type;

    code = pdfi_array_get_type(ctx, color_array, index + 1, PDF_NAME, (pdf_obj **)&name);
    if (code < 0)
        goto pdfi_separation_error;

    sep_type = SEP_OTHER;
    if (name->length == 4 && memcmp(name->data, "None", 4) == 0)
        sep_type = SEP_NONE;
    if (name->length == 3 && memcmp(name->data, "All", 3) == 0)
        sep_type = SEP_ALL;

    code = pdfi_array_get(ctx, color_array, index + 2, &o);
    if (code < 0)
        goto pdfi_separation_error;

    if (o->type == PDF_NAME) {
        NamedAlternate = (pdf_name *)o;
        code = pdfi_create_colorspace_by_name(ctx, NamedAlternate, stream_dict, page_dict, &pcs_alt, inline_image);
        if (code < 0)
            goto pdfi_separation_error;

    } else {
        if (o->type == PDF_ARRAY) {
            ArrayAlternate = (pdf_array *)o;
            code = pdfi_create_colorspace_by_array(ctx, ArrayAlternate, 0, stream_dict, page_dict, &pcs_alt, inline_image);
            if (code < 0)
                goto pdfi_separation_error;
        }
        else {
            code = gs_error_typecheck;
            goto pdfi_separation_error;
        }
    }

    code = pdfi_array_get(ctx, color_array, index + 3, &transform);
    if (code < 0)
        goto pdfi_separation_error;

    code = pdfi_build_function(ctx, &pfn, NULL, 1, transform, page_dict);
    if (code < 0)
        goto pdfi_separation_error;

    code = gs_cspace_new_Separation(&pcs, pcs_alt, ctx->memory);
    if (code < 0)
        goto pdfi_separation_error;

    rc_decrement(pcs_alt, "pdfi_create_Separation");
    pcs->params.separation.mem = ctx->memory;
    pcs->params.separation.sep_type = sep_type;
    pcs->params.separation.sep_name = (char *)gs_alloc_bytes(ctx->memory->non_gc_memory, name->length + 1, "pdfi_setseparationspace(ink)");
    memcpy(pcs->params.separation.sep_name, name->data, name->length);
    pcs->params.separation.sep_name[name->length] = 0x00;

    code = gs_cspace_set_sepr_function(pcs, pfn);
    if (code < 0)
        goto pdfi_separation_error;

    if (ppcs!= NULL){
        /* FIXME
         * I can see no justification for this whatever, but if I don't do this then some
         * files with images in a /Separation colour space come out incorrectly. Even surrounding
         * this with a gsave/grestore pair causes differences.
         */
        code = pdfi_gs_setcolorspace(ctx, pcs);
        *ppcs = pcs;
    } else {
        code = pdfi_gs_setcolorspace(ctx, pcs);
        /* release reference from construction */
        rc_decrement_only_cs(pcs, "setseparationspace");
    }

    pdfi_countdown(name);
    pdfi_countdown(NamedAlternate);
    pdfi_countdown(ArrayAlternate);
    pdfi_countdown(transform);
    return_error(0);

pdfi_separation_error:
    pdfi_free_function(ctx, pfn);
    if (pcs_alt != NULL)
        rc_decrement_only_cs(pcs_alt, "setseparationspace");
    if(pcs != NULL)
        rc_decrement_only_cs(pcs, "setseparationspace");
    pdfi_countdown(name);
    pdfi_countdown(NamedAlternate);
    pdfi_countdown(ArrayAlternate);
    pdfi_countdown(transform);
    return code;
}

static int pdfi_create_DeviceN(pdf_context *ctx, pdf_array *color_array, int index, pdf_dict *stream_dict, pdf_dict *page_dict, gs_color_space **ppcs, bool inline_image)
{
    pdf_obj *o = NULL;
    pdf_name *NamedAlternate = NULL;
    pdf_array *ArrayAlternate = NULL, *inks = NULL;
    pdf_obj *transform = NULL;
    pdf_dict *attributes = NULL;
    pdf_dict *Colorants = NULL, *Process = NULL;
    gs_color_space *process_space = NULL;
    int code;
    uint64_t ix;
    gs_color_space *pcs = NULL, *pcs_alt = NULL;
    gs_function_t * pfn = NULL;

    /* Start with the array of inks */
    code = pdfi_array_get_type(ctx, color_array, index + 1, PDF_ARRAY, (pdf_obj **)&inks);
    if (code < 0)
        goto pdfi_devicen_error;

    for (ix = 0;ix < pdfi_array_size(inks);ix++) {
        pdf_name *ink_name = NULL;

        code = pdfi_array_get_type(ctx, inks, ix, PDF_NAME, (pdf_obj **)&ink_name);
        if (code < 0)
            return code;

        if (ink_name->length == 3 && memcmp(ink_name->data, "All", 3) == 0) {
            pdfi_set_warning(ctx, 0, NULL, W_PDF_DEVICEN_USES_ALL, "pdfi_create_DeviceN", (char *)"WARNING: DeviceN space using /All ink name");
        }
        pdfi_countdown(ink_name);
        ink_name = NULL;
    }

    /* Sigh, Acrobat allows this, even though its contra the spec. Convert to
     * a /Separation space, and then return. Actually Acrobat does not always permit this, see
     * tests_private/comparefiles/Testform.v1.0.2.pdf.
     */
    if (pdfi_array_size(inks) == 1) {
        pdf_name *ink_name = NULL;
        pdf_array *sep_color_array = NULL;
        pdf_obj *obj = NULL;

        code = pdfi_array_get_type(ctx, inks, 0, PDF_NAME, (pdf_obj **)&ink_name);
        if (code < 0)
            goto pdfi_devicen_error;

        if (ink_name->length == 3 && memcmp(ink_name->data, "All", 3) == 0) {
            code = pdfi_array_alloc(ctx, 4, &sep_color_array);
            if (code < 0)
                goto all_error;
            pdfi_countup(sep_color_array);
            code = pdfi_name_alloc(ctx, (byte *)"Separation", 10, &obj);
            if (code < 0)
                goto all_error;
            pdfi_countup(obj);
            code = pdfi_array_put(ctx, sep_color_array, 0, obj);
            if (code < 0)
                goto all_error;
            pdfi_countdown(obj);
            obj = NULL;
            code = pdfi_array_put(ctx, sep_color_array, 1, (pdf_obj *)ink_name);
            if (code < 0)
                goto all_error;
            code = pdfi_array_get(ctx, color_array, index + 2, &obj);
            if (code < 0)
                goto all_error;
            code = pdfi_array_put(ctx, sep_color_array, 2, obj);
            if (code < 0)
                goto all_error;
            pdfi_countdown(obj);
            obj = NULL;
            code = pdfi_array_get(ctx, color_array, index + 3, &obj);
            if (code < 0)
                goto all_error;
            code = pdfi_array_put(ctx, sep_color_array, 3, obj);
            if (code < 0)
                goto all_error;
            pdfi_countdown(obj);
            obj = NULL;

            code = pdfi_create_Separation(ctx, sep_color_array, 0, stream_dict, page_dict, ppcs, inline_image);
            if (code < 0)
                goto all_error;
all_error:
            pdfi_countdown(ink_name);
            pdfi_countdown(sep_color_array);
            pdfi_countdown(obj);
            pdfi_countdown(inks);
            return code;
        } else
            pdfi_countdown(ink_name);
    }

    /* Deal with alternate space */
    code = pdfi_array_get(ctx, color_array, index + 2, &o);
    if (code < 0)
        goto pdfi_devicen_error;

    if (o->type == PDF_NAME) {
        NamedAlternate = (pdf_name *)o;
        code = pdfi_create_colorspace_by_name(ctx, NamedAlternate, stream_dict, page_dict, &pcs_alt, inline_image);
        if (code < 0)
            goto pdfi_devicen_error;

    } else {
        if (o->type == PDF_ARRAY) {
            ArrayAlternate = (pdf_array *)o;
            code = pdfi_create_colorspace_by_array(ctx, ArrayAlternate, 0, stream_dict, page_dict, &pcs_alt, inline_image);
            if (code < 0) {
                pdfi_countdown(o);
                goto pdfi_devicen_error;
            }
        }
        else {
            code = gs_error_typecheck;
            pdfi_countdown(o);
            goto pdfi_devicen_error;
        }
    }

    /* Now the tint transform */
    code = pdfi_array_get(ctx, color_array, index + 3, &transform);
    if (code < 0)
        goto pdfi_devicen_error;

    code = pdfi_build_function(ctx, &pfn, NULL, 1, transform, page_dict);
    if (code < 0)
        goto pdfi_devicen_error;

    code = gs_cspace_new_DeviceN(&pcs, pdfi_array_size(inks), pcs_alt, ctx->memory);
    if (code < 0)
        return code;

    rc_decrement(pcs_alt, "pdfi_create_DeviceN");
    pcs_alt = NULL;
    pcs->params.device_n.mem = ctx->memory;

    for (ix = 0;ix < pdfi_array_size(inks);ix++) {
        pdf_name *ink_name;

        ink_name = NULL;
        code = pdfi_array_get_type(ctx, inks, ix, PDF_NAME, (pdf_obj **)&ink_name);
        if (code < 0)
            goto pdfi_devicen_error;

        pcs->params.device_n.names[ix] = (char *)gs_alloc_bytes(ctx->memory->non_gc_memory, ink_name->length + 1, "pdfi_setdevicenspace(ink)");
        memcpy(pcs->params.device_n.names[ix], ink_name->data, ink_name->length);
        pcs->params.device_n.names[ix][ink_name->length] = 0x00;
        pdfi_countdown(ink_name);
    }

    code = gs_cspace_set_devn_function(pcs, pfn);
    if (code < 0)
        goto pdfi_devicen_error;

    if (pdfi_array_size(color_array) >= index + 5) {
        pdf_obj *ColorSpace = NULL;
        pdf_array *Components = NULL;
        pdf_obj *subtype = NULL;

        code = pdfi_array_get_type(ctx, color_array, index + 4, PDF_DICT, (pdf_obj **)&attributes);
        if (code < 0)
            goto pdfi_devicen_error;

        code = pdfi_dict_knownget(ctx, attributes, "Subtype", (pdf_obj **)&subtype);
        if (code < 0)
            goto pdfi_devicen_error;

        if (code == 0) {
            pcs->params.device_n.subtype = gs_devicen_DeviceN;
        } else {
            if (subtype->type == PDF_NAME || subtype->type == PDF_STRING) {
                if (memcmp(((pdf_name *)subtype)->data, "DeviceN", 7) == 0) {
                    pcs->params.device_n.subtype = gs_devicen_DeviceN;
                } else {
                    if (memcmp(((pdf_name *)subtype)->data, "NChannel", 8) == 0) {
                        pcs->params.device_n.subtype = gs_devicen_NChannel;
                    } else {
                        pdfi_countdown(subtype);
                        goto pdfi_devicen_error;
                    }
                }
                pdfi_countdown(subtype);
            } else {
                pdfi_countdown(subtype);
                goto pdfi_devicen_error;
            }
        }

        code = pdfi_dict_knownget_type(ctx, attributes, "Process", PDF_DICT, (pdf_obj **)&Process);
        if (code < 0)
            goto pdfi_devicen_error;

        if (Process != NULL && pdfi_dict_entries(Process) != 0) {
            int ix = 0;
            pdf_obj *name;

            code = pdfi_dict_get(ctx, Process, "ColorSpace", (pdf_obj **)&ColorSpace);
            if (code < 0)
                goto pdfi_devicen_error;

            code = pdfi_create_colorspace(ctx, ColorSpace, stream_dict, page_dict, &process_space, inline_image);
            pdfi_countdown(ColorSpace);
            if (code < 0)
                goto pdfi_devicen_error;

            pcs->params.device_n.devn_process_space = process_space;

            code = pdfi_dict_get_type(ctx, Process, "Components", PDF_ARRAY, (pdf_obj **)&Components);
            if (code < 0)
                goto pdfi_devicen_error;

            pcs->params.device_n.num_process_names = pdfi_array_size(Components);
            pcs->params.device_n.process_names = (char **)gs_alloc_bytes(pcs->params.device_n.mem->non_gc_memory, pdfi_array_size(Components) * sizeof(char *), "pdfi_devicen(Processnames)");
            if (pcs->params.device_n.process_names == NULL) {
                code = gs_error_VMerror;
                goto pdfi_devicen_error;
            }

            for (ix = 0; ix < pcs->params.device_n.num_process_names; ix++) {
                code = pdfi_array_get(ctx, Components, ix, &name);
                if (code < 0) {
                    pdfi_countdown(Components);
                    goto pdfi_devicen_error;
                }

                if (name->type == PDF_NAME || name->type == PDF_STRING) {
                    pcs->params.device_n.process_names[ix] = (char *)gs_alloc_bytes(pcs->params.device_n.mem->non_gc_memory, ((pdf_name *)name)->length + 1, "pdfi_devicen(Processnames)");
                    if (pcs->params.device_n.process_names[ix] == NULL) {
                        pdfi_countdown(Components);
                        pdfi_countdown(name);
                        code = gs_error_VMerror;
                        goto pdfi_devicen_error;
                    }
                    memcpy(pcs->params.device_n.process_names[ix], ((pdf_name *)name)->data, ((pdf_name *)name)->length);
                    pcs->params.device_n.process_names[ix][((pdf_name *)name)->length] = 0x00;
                    pdfi_countdown(name);
                } else {
                    pdfi_countdown(Components);
                    pdfi_countdown(name);
                    goto pdfi_devicen_error;
                }
            }
            pdfi_countdown(Components);
        }

        code = pdfi_dict_knownget_type(ctx, attributes, "Colorants", PDF_DICT, (pdf_obj **)&Colorants);
        if (code < 0)
            goto pdfi_devicen_error;

        if (Colorants != NULL && pdfi_dict_entries(Colorants) != 0) {
            uint64_t ix = 0;
            pdf_obj *Colorant = NULL, *Space = NULL;
            char *colorant_name;
            gs_color_space *colorant_space = NULL;

            code = pdfi_dict_first(ctx, Colorants, &Colorant, &Space, &ix);
            if (code < 0)
                goto pdfi_devicen_error;

            do {
                if (Space->type != PDF_STRING && Space->type != PDF_NAME && Space->type != PDF_ARRAY) {
                    pdfi_countdown(Space);
                    pdfi_countdown(Colorant);
                    code = gs_note_error(gs_error_typecheck);
                    goto pdfi_devicen_error;
                }
                if (Colorant->type != PDF_STRING && Colorant->type != PDF_NAME) {
                    pdfi_countdown(Space);
                    pdfi_countdown(Colorant);
                    code = gs_note_error(gs_error_typecheck);
                    goto pdfi_devicen_error;
                }

                code = pdfi_create_colorspace(ctx, Space, stream_dict, page_dict, &colorant_space, inline_image);
                if (code < 0) {
                    pdfi_countdown(Space);
                    pdfi_countdown(Colorant);
                    goto pdfi_devicen_error;
                }

                colorant_name = (char *)gs_alloc_bytes(pcs->params.device_n.mem->non_gc_memory, ((pdf_name *)Colorant)->length + 1, "pdfi_devicen(colorant)");
                if (colorant_name == NULL) {
                    rc_decrement_cs(colorant_space, "pdfi_devicen(colorant)");
                    pdfi_countdown(Space);
                    pdfi_countdown(Colorant);
                    code = gs_note_error(gs_error_VMerror);
                    goto pdfi_devicen_error;
                }
                memcpy(colorant_name, ((pdf_name *)Colorant)->data, ((pdf_name *)Colorant)->length);
                colorant_name[((pdf_name *)Colorant)->length] = 0x00;

                code = gs_attach_colorant_to_space(colorant_name, pcs, colorant_space, pcs->params.device_n.mem->non_gc_memory);
                if (code < 0) {
                    gs_free_object(pcs->params.device_n.mem->non_gc_memory, colorant_name, "pdfi_devicen(colorant)");
                    rc_decrement_cs(colorant_space, "pdfi_devicen(colorant)");
                    pdfi_countdown(Space);
                    pdfi_countdown(Colorant);
                    code = gs_note_error(gs_error_VMerror);
                    goto pdfi_devicen_error;
                }

                /* We've attached the colorant colour space to the DeviceN space, we no longer need this
                 * reference to it, so discard it.
                 */
                rc_decrement_cs(colorant_space, "pdfi_devicen(colorant)");
                pdfi_countdown(Space);
                pdfi_countdown(Colorant);
                Space = Colorant = NULL;

                code = pdfi_dict_next(ctx, Colorants, &Colorant, &Space, &ix);
                if (code == gs_error_undefined)
                    break;

                if (code < 0) {
                    pdfi_countdown(Space);
                    pdfi_countdown(Colorant);
                    goto pdfi_devicen_error;
                }
            }while (1);
        }
    }

    if (ppcs!= NULL){
        *ppcs = pcs;
        pdfi_set_colour_callback(pcs, ctx, pdfi_cspace_free_callback);
    } else {
        code = pdfi_gs_setcolorspace(ctx, pcs);
        /* release reference from construction */
        rc_decrement_only_cs(pcs, "setdevicenspace");
    }
    pdfi_countdown(Process);
    pdfi_countdown(Colorants);
    pdfi_countdown(attributes);
    pdfi_countdown(inks);
    pdfi_countdown(NamedAlternate);
    pdfi_countdown(ArrayAlternate);
    pdfi_countdown(transform);
    return_error(0);

pdfi_devicen_error:
    pdfi_free_function(ctx, pfn);
    if (pcs_alt != NULL)
        rc_decrement_only_cs(pcs_alt, "setseparationspace");
    if(pcs != NULL)
        rc_decrement_only_cs(pcs, "setseparationspace");
    pdfi_countdown(Process);
    pdfi_countdown(Colorants);
    pdfi_countdown(attributes);
    pdfi_countdown(inks);
    pdfi_countdown(NamedAlternate);
    pdfi_countdown(ArrayAlternate);
    pdfi_countdown(transform);
    return code;
}

/* Now /Indexed spaces, essentially we just need to set the underlying space(s) and then set
 * /Indexed.
 */
static int
pdfi_create_indexed(pdf_context *ctx, pdf_array *color_array, int index,
                    pdf_dict *stream_dict, pdf_dict *page_dict, gs_color_space **ppcs, bool inline_image)
{
    pdf_obj *space=NULL, *lookup=NULL;
    int code;
    int64_t hival, lookup_length = 0;
    int num_values;
    gs_color_space *pcs=NULL, *pcs_base=NULL;
    gs_color_space_index base_type;
    byte *Buffer = NULL;

    if (index != 0)
        return_error(gs_error_syntaxerror);

    code = pdfi_array_get_int(ctx, color_array, index + 2, &hival);
    if (code < 0)
        return code;

    if (hival > 255 || hival < 0)
        return_error(gs_error_syntaxerror);

    code = pdfi_array_get(ctx, color_array, index + 1, &space);
    if (code < 0)
        goto exit;

    code = pdfi_create_colorspace(ctx, space, stream_dict, page_dict, &pcs_base, inline_image);
    if (code < 0)
        goto exit;

    (void)pcs_base->type->install_cspace(pcs_base, ctx->pgs);

    base_type = gs_color_space_get_index(pcs_base);

    code = pdfi_array_get(ctx, color_array, index + 3, &lookup);
    if (code < 0)
        goto exit;

    if (lookup->type == PDF_STREAM) {
        code = pdfi_stream_to_buffer(ctx, (pdf_stream *)lookup, &Buffer, &lookup_length);
        if (code < 0)
            goto exit;
    } else if (lookup->type == PDF_STRING) {
        /* This is not legal, but Acrobat seems to accept it */
        pdf_string *lookup_string = (pdf_string *)lookup; /* alias */

        Buffer = gs_alloc_bytes(ctx->memory, lookup_string->length, "pdfi_create_indexed (lookup buffer)");
        if (Buffer == NULL) {
            code = gs_note_error(gs_error_VMerror);
            goto exit;
        }

        memcpy(Buffer, lookup_string->data, lookup_string->length);
        lookup_length = lookup_string->length;
    } else {
        code = gs_note_error(gs_error_typecheck);
        goto exit;
    }

    num_values = (hival+1) * cs_num_components(pcs_base);
    if (num_values > lookup_length) {
        dmprintf2(ctx->memory, "WARNING: pdfi_create_indexed() got %ld values, expected at least %d values\n",
                  lookup_length, num_values);
        code = gs_note_error(gs_error_rangecheck);
        goto exit;
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

    /* NOTE: we don't need to increment the reference to pcs_base, since it is already 1 */
    pcs->base_space = pcs_base;

    pcs->params.indexed.lookup.table.size = num_values;
    pcs->params.indexed.use_proc = 0;
    pcs->params.indexed.hival = hival;
    pcs->params.indexed.n_comps = cs_num_components(pcs_base);
    pcs->params.indexed.lookup.table.data = Buffer;
    Buffer = NULL;

    if (ppcs != NULL) {
        *ppcs = pcs;
        pdfi_set_colour_callback(pcs, ctx, pdfi_cspace_free_callback);
    }
    else {
        code = pdfi_gs_setcolorspace(ctx, pcs);
        /* release reference from construction */
        rc_decrement_only_cs(pcs, "setindexedspace");
    }

 exit:
    if (code != 0)
        rc_decrement(pcs_base, "pdfi_create_indexed(pcs_base) error");
    if (Buffer)
        gs_free_object(ctx->memory, Buffer, "pdfi_create_indexed (decompression buffer)");
    pdfi_countdown(space);
    pdfi_countdown(lookup);
    return code;
}

static int pdfi_create_DeviceGray(pdf_context *ctx, gs_color_space **ppcs)
{
    int code = 0;

    if (ppcs != NULL) {
        if (ctx->page.DefaultGray_cs != NULL) {
            *ppcs = ctx->page.DefaultGray_cs;
            rc_increment(*ppcs);
        } else {
            *ppcs = gs_cspace_new_DeviceGray(ctx->memory);
            if (*ppcs == NULL)
                code = gs_note_error(gs_error_VMerror);
            else {
                code = ((gs_color_space *)*ppcs)->type->install_cspace(*ppcs, ctx->pgs);
                if (code < 0) {
                    rc_decrement_only_cs(*ppcs, "pdfi_create_DeviceGray");
                    *ppcs = NULL;
                }
            }
            if (*ppcs != NULL)
                pdfi_set_colour_callback(*ppcs, ctx, pdfi_cspace_free_callback);
        }
    } else {
        code = pdfi_gs_setgray(ctx, 0);
    }
    return code;
}

static int pdfi_create_DeviceRGB(pdf_context *ctx, gs_color_space **ppcs)
{
    int code = 0;

    if (ppcs != NULL) {
        if (ctx->page.DefaultRGB_cs != NULL) {
            *ppcs = ctx->page.DefaultRGB_cs;
            rc_increment(*ppcs);
        } else {
            *ppcs = gs_cspace_new_DeviceRGB(ctx->memory);
            if (*ppcs == NULL)
                code = gs_note_error(gs_error_VMerror);
            else {
                code = ((gs_color_space *)*ppcs)->type->install_cspace(*ppcs, ctx->pgs);
                if (code < 0) {
                    rc_decrement_only_cs(*ppcs, "pdfi_create_DeviceRGB");
                    *ppcs = NULL;
                }
            }
            if (*ppcs != NULL)
                pdfi_set_colour_callback(*ppcs, ctx, pdfi_cspace_free_callback);
        }
    } else {
        code = pdfi_gs_setrgbcolor(ctx, 0, 0, 0);
    }
    return code;
}

static int pdfi_create_DeviceCMYK(pdf_context *ctx, gs_color_space **ppcs)
{
    int code = 0;

    if (ppcs != NULL) {
        if (ctx->page.DefaultCMYK_cs != NULL) {
            *ppcs = ctx->page.DefaultCMYK_cs;
            rc_increment(*ppcs);
        } else {
            *ppcs = gs_cspace_new_DeviceCMYK(ctx->memory);
            if (*ppcs == NULL)
                code = gs_note_error(gs_error_VMerror);
            else {
                code = ((gs_color_space *)*ppcs)->type->install_cspace(*ppcs, ctx->pgs);
                if (code < 0) {
                    rc_decrement_only_cs(*ppcs, "pdfi_create_DeviceCMYK");
                    *ppcs = NULL;
                }
            }
            if (*ppcs != NULL)
                pdfi_set_colour_callback(*ppcs, ctx, pdfi_cspace_free_callback);
        }
    } else {
        code = pdfi_gs_setcmykcolor(ctx, 0, 0, 0, 1);
    }
    return code;
}

static int pdfi_create_JPX_space(pdf_context *ctx, const char *name, int num_components, gs_color_space **ppcs)
{
    int code, icc_N;
    float range_buff[6] = {0.0f, 1.0f, 0.0f, 1.0f, 0.0f, 1.0f};

    code = pdfi_create_icc(ctx, (char *)name, NULL, num_components, &icc_N, range_buff, ppcs);
    return code;
}

/* These next routines allow us to use recursion to set up colour spaces. We can set
 * colour space starting from a name (which can be a named resource) or an array.
 * If we get a name, and its a named resource we dereference it and go round again.
 * If its an array we select the correct handler (above) for that space. The space
 * handler will call pdfi_create_colorspace() to set the underlying space(s) which
 * may mean calling pdfi_create_colorspace again....
 */
static int
pdfi_create_colorspace_by_array(pdf_context *ctx, pdf_array *color_array, int index,
                                pdf_dict *stream_dict, pdf_dict *page_dict, gs_color_space **ppcs,
                                bool inline_image)
{
    int code;
    pdf_name *space = NULL;
    pdf_array *a = NULL;

    code = pdfi_array_get_type(ctx, color_array, index, PDF_NAME, (pdf_obj **)&space);
    if (code != 0)
        goto exit;

    code = 0;
    if (pdfi_name_is(space, "G") || pdfi_name_is(space, "DeviceGray")) {
        if (pdfi_name_is(space, "G") && !inline_image) {
            pdfi_set_warning(ctx, 0, NULL, W_PDF_BAD_INLINECOLORSPACE, "pdfi_create_colorspace_by_array", NULL);
            if (ctx->args.pdfstoponwarning)
                return_error(gs_error_syntaxerror);
        }
        code = pdfi_create_DeviceGray(ctx, ppcs);
    } else if (pdfi_name_is(space, "I") || pdfi_name_is(space, "Indexed")) {
        if (pdfi_name_is(space, "I") && !inline_image) {
            pdfi_set_warning(ctx, 0, NULL, W_PDF_BAD_INLINECOLORSPACE, "pdfi_create_colorspace_by_array", NULL);
            if (ctx->args.pdfstoponwarning)
                return_error(gs_error_syntaxerror);
        }
        code = pdfi_create_indexed(ctx, color_array, index, stream_dict, page_dict, ppcs, inline_image);
    } else if (pdfi_name_is(space, "Lab")) {
        code = pdfi_create_Lab(ctx, color_array, index, stream_dict, page_dict, ppcs);
    } else if (pdfi_name_is(space, "RGB") || pdfi_name_is(space, "DeviceRGB")) {
        if (pdfi_name_is(space, "RGB") && !inline_image) {
            pdfi_set_warning(ctx, 0, NULL, W_PDF_BAD_INLINECOLORSPACE, "pdfi_create_colorspace_by_array", NULL);
            if (ctx->args.pdfstoponwarning)
                return_error(gs_error_syntaxerror);
        }
        code = pdfi_create_DeviceRGB(ctx, ppcs);
    } else if (pdfi_name_is(space, "CMYK") || pdfi_name_is(space, "DeviceCMYK")) {
        if (pdfi_name_is(space, "CMYK") && !inline_image) {
            pdfi_set_warning(ctx, 0, NULL, W_PDF_BAD_INLINECOLORSPACE, "pdfi_create_colorspace_by_array", NULL);
            if (ctx->args.pdfstoponwarning)
                return_error(gs_error_syntaxerror);
        }
        code = pdfi_create_DeviceCMYK(ctx, ppcs);
    } else if (pdfi_name_is(space, "CalRGB")) {
        code = pdfi_create_CalRGB(ctx, color_array, index, stream_dict, page_dict, ppcs);
    } else if (pdfi_name_is(space, "CalGray")) {
        code = pdfi_create_CalGray(ctx, color_array, index, stream_dict, page_dict, ppcs);
    } else if (pdfi_name_is(space, "Pattern")) {
        if (index != 0)
            code = gs_note_error(gs_error_syntaxerror);
        else
            code = pdfi_pattern_create(ctx, color_array, stream_dict, page_dict, ppcs);
    } else if (pdfi_name_is(space, "DeviceN")) {
        code = pdfi_create_DeviceN(ctx, color_array, index, stream_dict, page_dict, ppcs, inline_image);
    } else if (pdfi_name_is(space, "ICCBased")) {
        code = pdfi_create_iccbased(ctx, color_array, index, stream_dict, page_dict, ppcs, inline_image);
    } else if (pdfi_name_is(space, "Separation")) {
        code = pdfi_create_Separation(ctx, color_array, index, stream_dict, page_dict, ppcs, inline_image);
    } else {
        code = pdfi_find_resource(ctx, (unsigned char *)"ColorSpace",
                                  space, (pdf_dict *)stream_dict, page_dict, (pdf_obj **)&a);
        if (code < 0)
            goto exit;

        if (a->type != PDF_ARRAY) {
            code = gs_note_error(gs_error_typecheck);
            goto exit;
        }

        /* recursion */
        code = pdfi_create_colorspace_by_array(ctx, a, 0, stream_dict, page_dict, ppcs, inline_image);
    }

 exit:
    pdfi_countdown(space);
    pdfi_countdown(a);
    return code;
}

static int
pdfi_create_colorspace_by_name(pdf_context *ctx, pdf_name *name,
                               pdf_dict *stream_dict, pdf_dict *page_dict,
                               gs_color_space **ppcs, bool inline_image)
{
    int code = 0;

    if (pdfi_name_is(name, "G") || pdfi_name_is(name, "DeviceGray")) {
        if (pdfi_name_is(name, "G") && !inline_image) {
            pdfi_set_warning(ctx, 0, NULL, W_PDF_BAD_INLINECOLORSPACE, "pdfi_create_colorspace_by_name", NULL);
            if (ctx->args.pdfstoponwarning)
                return_error(gs_error_syntaxerror);
        }
        code = pdfi_create_DeviceGray(ctx, ppcs);
    } else if (pdfi_name_is(name, "RGB") || pdfi_name_is(name, "DeviceRGB")) {
        if (pdfi_name_is(name, "RGB") && !inline_image) {
            pdfi_set_warning(ctx, 0, NULL, W_PDF_BAD_INLINECOLORSPACE, "pdfi_create_colorspace_by_name", NULL);
            if (ctx->args.pdfstoponwarning)
                return_error(gs_error_syntaxerror);
        }
        code = pdfi_create_DeviceRGB(ctx, ppcs);
    } else if (pdfi_name_is(name, "CMYK") || pdfi_name_is(name, "DeviceCMYK")) {
        if (pdfi_name_is(name, "CMYK") && !inline_image) {
            pdfi_set_warning(ctx, 0, NULL, W_PDF_BAD_INLINECOLORSPACE, "pdfi_create_colorspace_by_name", NULL);
            if (ctx->args.pdfstoponwarning)
                return_error(gs_error_syntaxerror);
        }
        code = pdfi_create_DeviceCMYK(ctx, ppcs);
    } else if (pdfi_name_is(name, "Pattern")) {
        code = pdfi_pattern_create(ctx, NULL, stream_dict, page_dict, ppcs);
    } else if (pdfi_name_is(name, "esRGBICC")) {                /* These 4 spaces are 'special' for JPX images          */
        code = pdfi_create_JPX_space(ctx, "esrgb", 3, ppcs);    /* the names are non-standad and must match those in    */
    } else if (pdfi_name_is(name, "rommRGBICC")) {              /* pdfi_image_get_color() in pdf_image.c                */
        code = pdfi_create_JPX_space(ctx, "rommrgb", 3, ppcs);  /* Note that the Lab space for JPX images does not use  */
    } else if (pdfi_name_is(name, "sRGBICC")) {                 /* a special space but simply constructs an appropriate */
        code = pdfi_create_JPX_space(ctx, "srgb", 3, ppcs);     /* pdf_array object with the corerct contents for an    */
    } else if (pdfi_name_is(name, "sGrayICC")) {                /* Lab space with a D65 white point.                    */
        code = pdfi_create_JPX_space(ctx, "sgray", 1, ppcs);
    } else {
        pdf_obj *ref_space = NULL;
        code = pdfi_find_resource(ctx, (unsigned char *)"ColorSpace", name, (pdf_dict *)stream_dict,
                                  page_dict, &ref_space);
        if (code < 0)
            return code;

        /* recursion */
        code = pdfi_create_colorspace(ctx, ref_space, stream_dict, page_dict, ppcs, inline_image);
        pdfi_countdown(ref_space);
        return code;
    }

    /* If we got here, it's a recursion base case, and ppcs should have been set if requested */
    if (ppcs != NULL && *ppcs == NULL)
        code = gs_note_error(gs_error_VMerror);
    return code;
}

/*
 * Gets icc profile data from the provided stream.
 * Position in the stream is NOT preserved.
 * This is raw data, not filtered, so no need to worry about compression.
 * (Used for JPXDecode images)
 */
int
pdfi_create_icc_colorspace_from_stream(pdf_context *ctx, pdf_c_stream *stream, gs_offset_t offset,
                                       unsigned int length, int comps, int *icc_N, gs_color_space **ppcs)
{
    pdf_c_stream *profile_stream = NULL;
    byte *profile_buffer;
    int code, code1;
    float range[8] = {0,1,0,1,0,1,0,1};

    /* Move to the start of the profile data */
    pdfi_seek(ctx, stream, offset, SEEK_SET);

    /* The ICC profile reading code (irritatingly) requires a seekable stream, because it
     * rewinds it to the start, then seeks to the end to find the size, then rewinds the
     * stream again.
     * Ideally we would use a ReusableStreamDecode filter here, but that is largely
     * implemented in PostScript (!) so we can't use it. What we can do is create a
     * string sourced stream in memory, which is at least seekable.
     */
    code = pdfi_open_memory_stream_from_stream(ctx, length, &profile_buffer, stream, &profile_stream, true);
    if (code < 0) {
        return code;
    }

    /* Now, finally, we can call the code to create and set the profile */
    code = pdfi_create_icc(ctx, NULL, profile_stream->s, comps, icc_N, range, ppcs);

    code1 = pdfi_close_memory_stream(ctx, profile_buffer, profile_stream);

    if (code == 0)
        code = code1;

    return code;
}

int pdfi_create_colorspace(pdf_context *ctx, pdf_obj *space, pdf_dict *stream_dict, pdf_dict *page_dict, gs_color_space **ppcs, bool inline_image)
{
    int code;

    code = pdfi_loop_detector_mark(ctx);
    if (code < 0)
        return code;

    if (space->type == PDF_NAME) {
        code = pdfi_create_colorspace_by_name(ctx, (pdf_name *)space, stream_dict, page_dict, ppcs, inline_image);
    } else {
        if (space->type == PDF_ARRAY) {
            code = pdfi_create_colorspace_by_array(ctx, (pdf_array *)space, 0, stream_dict, page_dict, ppcs, inline_image);
        } else {
            pdfi_loop_detector_cleartomark(ctx);
            return_error(gs_error_typecheck);
        }
    }
    if (ppcs && *ppcs && code >= 0)
        (void)(*ppcs)->type->install_cspace(*ppcs, ctx->pgs);

    (void)pdfi_loop_detector_cleartomark(ctx);
    return code;
}

int pdfi_setcolorspace(pdf_context *ctx, pdf_obj *space, pdf_dict *stream_dict, pdf_dict *page_dict)
{
    return pdfi_create_colorspace(ctx, space, stream_dict, page_dict, NULL, false);
}

/* And finally, the implementation of the actual PDF operators CS and cs */
int pdfi_setstrokecolor_space(pdf_context *ctx, pdf_dict *stream_dict, pdf_dict *page_dict)
{
    int code;

    if (pdfi_count_stack(ctx) < 1)
        return_error(gs_error_stackunderflow);

    if (ctx->stack_top[-1]->type != PDF_NAME) {
        pdfi_pop(ctx, 1);
        return_error(gs_error_stackunderflow);
    }
    gs_swapcolors_quick(ctx->pgs);
    code = pdfi_setcolorspace(ctx, ctx->stack_top[-1], stream_dict, page_dict);
    gs_swapcolors_quick(ctx->pgs);
    pdfi_pop(ctx, 1);

    return code;
}

int pdfi_setfillcolor_space(pdf_context *ctx, pdf_dict *stream_dict, pdf_dict *page_dict)
{
    int code;

    if (pdfi_count_stack(ctx) < 1)
        return_error(gs_error_stackunderflow);

    if (ctx->stack_top[-1]->type != PDF_NAME) {
        pdfi_pop(ctx, 1);
        return_error(gs_error_stackunderflow);
    }
    code = pdfi_setcolorspace(ctx, ctx->stack_top[-1], stream_dict, page_dict);
    pdfi_pop(ctx, 1);

    return code;
}


/*
 * Set device outputintent from stream
 * see zicc.c/zset_outputintent()
 */
static int pdfi_device_setoutputintent(pdf_context *ctx, pdf_dict *profile_dict, stream *stream)
{
    int code = 0;
    gs_gstate *pgs = ctx->pgs;
    gx_device *dev = gs_currentdevice(pgs);
    cmm_dev_profile_t *dev_profile;
    int64_t N;
    int ncomps, dev_comps;
    int expected = 0;
    cmm_profile_t *picc_profile = NULL;
    cmm_profile_t *source_profile = NULL;
    gsicc_manager_t *icc_manager = pgs->icc_manager;
    gs_color_space_index index;

    if_debug0m(gs_debug_flag_icc, ctx->memory, "[icc] Using OutputIntent\n");

    /* get dev_profile and try initing it if fail first time */
    code = dev_proc(dev, get_profile)(dev, &dev_profile);
    if (code < 0)
        return code;

    if (dev_profile == NULL) {
        code = gsicc_init_device_profile_struct(dev, NULL, 0);
        if (code < 0)
            return code;
        code = dev_proc(dev, get_profile)(dev, &dev_profile);
        if (code < 0)
            return code;
    }
    if (dev_profile->oi_profile != NULL) {
        return 0;  /* Allow only one setting of this object */
    }

    code = pdfi_dict_get_int(ctx, profile_dict, "N", &N);
    if (code < 0)
        goto exit;
    ncomps = (int)N;

    picc_profile = gsicc_profile_new(stream, gs_gstate_memory(pgs), NULL, 0);
    if (picc_profile == NULL) {
        code = gs_note_error(gs_error_VMerror);
        goto exit;
    }
    picc_profile->num_comps = ncomps;
    picc_profile->profile_handle =
        gsicc_get_profile_handle_buffer(picc_profile->buffer,
                                        picc_profile->buffer_size,
                                        gs_gstate_memory(pgs));
    if (picc_profile->profile_handle == NULL) {
        code = gs_note_error(gs_error_VMerror);
        goto exit;
    }

    picc_profile->data_cs =
        gscms_get_profile_data_space(picc_profile->profile_handle,
            picc_profile->memory);
    switch (picc_profile->data_cs) {
        case gsCIEXYZ:
        case gsCIELAB:
        case gsRGB:
            expected = 3;
            source_profile = icc_manager->default_rgb;
            break;
        case gsGRAY:
            expected = 1;
            source_profile = icc_manager->default_gray;
            break;
        case gsCMYK:
            expected = 4;
            source_profile = icc_manager->default_cmyk;
            break;
        case gsNCHANNEL:
            expected = 0;
            break;
        case gsNAMED:
        case gsUNDEFINED:
            break;
    }
    if (expected && ncomps != expected) {
        code = gs_note_error(gs_error_rangecheck);
        goto exit;
    }
    gsicc_init_hash_cs(picc_profile, pgs);

    /* All is well with the profile.  Lets set the stuff that needs to be set */
    dev_profile->oi_profile = picc_profile;
    rc_increment(picc_profile);
    picc_profile->name = (char *) gs_alloc_bytes(picc_profile->memory,
                                                 MAX_DEFAULT_ICC_LENGTH,
                                                 "pdfi_color_setoutputintent");
    strncpy(picc_profile->name, OI_PROFILE, strlen(OI_PROFILE));
    picc_profile->name[strlen(OI_PROFILE)] = 0;
    picc_profile->name_length = strlen(OI_PROFILE);
    /* Set the range of the profile */
    gsicc_set_icc_range(&picc_profile);

    /* If the output device has a different number of components, then we are
       going to set the output intent as the proofing profile, unless the
       proofing profile has already been set.

       If the device has the same number of components (and color model) then as
       the profile we will use this as the output profile, unless someone has
       explicitly set the output profile.

       Finally, we will use the output intent profile for the default profile
       of the proper Device profile in the icc manager, again, unless someone
       has explicitly set this default profile.
    */
    dev_comps = dev_profile->device_profile[GS_DEFAULT_DEVICE_PROFILE]->num_comps;
    index = gsicc_get_default_type(dev_profile->device_profile[GS_DEFAULT_DEVICE_PROFILE]);
    if (ncomps == dev_comps && index < gs_color_space_index_DevicePixel) {
        /* The OI profile is the same type as the profile for the device and a
           "default" profile for the device was not externally set. So we go
           ahead and use the OI profile as the device profile.  Care needs to be
           taken here to keep from screwing up any device parameters.   We will
           use a keyword of OIProfile for the user/device parameter to indicate
           its usage.  Also, note conflicts if one is setting object dependent
           color management */
        dev_profile->device_profile[GS_DEFAULT_DEVICE_PROFILE] = picc_profile;
        rc_increment(picc_profile);
        if_debug0m(gs_debug_flag_icc, ctx->memory, "[icc] OutputIntent used for device profile\n");
    } else {
        if (dev_profile->proof_profile == NULL) {
            /* This means that we should use the OI profile as the proofing
               profile.  Note that if someone already has specified a
               proofing profile it is unclear what they are trying to do
               with the output intent.  In this case, we will use it
               just for the source data below */
            dev_profile->proof_profile = picc_profile;
            rc_increment(picc_profile);
            if_debug0m(gs_debug_flag_icc, ctx->memory, "[icc] OutputIntent used for proof profile\n");
        }
    }
    /* Now the source colors.  See which source color space needs to use the
       output intent ICC profile */
    index = gsicc_get_default_type(source_profile);
    if (index < gs_color_space_index_DevicePixel) {
        /* source_profile is currently the default.  Set it to the OI profile */
        switch (picc_profile->data_cs) {
            case gsGRAY:
                if_debug0m(gs_debug_flag_icc, ctx->memory, "[icc] OutputIntent used source Gray\n");
                icc_manager->default_gray = picc_profile;
                rc_increment(picc_profile);
                break;
            case gsRGB:
                if_debug0m(gs_debug_flag_icc, ctx->memory, "[icc] OutputIntent used source RGB\n");
                icc_manager->default_rgb = picc_profile;
                rc_increment(picc_profile);
                break;
            case gsCMYK:
                if_debug0m(gs_debug_flag_icc, ctx->memory, "[icc] OutputIntent used source CMYK\n");
                icc_manager->default_cmyk = picc_profile;
                rc_increment(picc_profile);
                break;
            default:
                break;
        }
    }

 exit:
    if (picc_profile != NULL)
        rc_decrement(picc_profile, "pdfi_color_setoutputintent");
    return code;
}

/*
 * intent_dict -- the outputintent dictionary
 * profile -- the color profile (a stream)
 *
 */
int pdfi_color_setoutputintent(pdf_context *ctx, pdf_dict *intent_dict, pdf_stream *profile)
{
    pdf_c_stream *profile_stream = NULL;
    byte *profile_buffer;
    gs_offset_t savedoffset;
    int code, code1;
    int64_t Length;
    pdf_dict *profile_dict;

    code = pdfi_dict_from_obj(ctx, (pdf_obj *)profile, &profile_dict);
    if (code < 0)
        return code;

    /* Save the current stream position, and move to the start of the profile stream */
    savedoffset = pdfi_tell(ctx->main_stream);
    pdfi_seek(ctx, ctx->main_stream, pdfi_stream_offset(ctx, profile), SEEK_SET);

    Length = pdfi_stream_length(ctx, profile);

    /* The ICC profile reading code (irritatingly) requires a seekable stream, because it
     * rewinds it to the start, then seeks to the end to find the size, then rewinds the
     * stream again.
     * Ideally we would use a ReusableStreamDecode filter here, but that is largely
     * implemented in PostScript (!) so we can't use it. What we can do is create a
     * string sourced stream in memory, which is at least seekable.
     */
    code = pdfi_open_memory_stream_from_filtered_stream(ctx, profile, Length, &profile_buffer, ctx->main_stream, &profile_stream, true);
    if (code < 0)
        goto exit;

    /* Create and set the device profile */
    code = pdfi_device_setoutputintent(ctx, profile_dict, profile_stream->s);

    code1 = pdfi_close_memory_stream(ctx, profile_buffer, profile_stream);

    if (code == 0)
        code = code1;

 exit:
    pdfi_seek(ctx, ctx->main_stream, savedoffset, SEEK_SET);
    return code;
}
