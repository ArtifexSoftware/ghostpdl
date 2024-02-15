/* Copyright (C) 2019-2024 Artifex Software, Inc.
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

/* Annotation handling for the PDF interpreter */

#include "pdf_int.h"
#include "pdf_stack.h"
#include "pdf_annot.h"
#include "pdf_file.h"
#include "pdf_dict.h"
#include "pdf_array.h"
#include "pdf_loop_detect.h"
#include "pdf_colour.h"
#include "pdf_trans.h"
#include "pdf_font_types.h"
#include "pdf_gstate.h"
#include "pdf_misc.h"
#include "pdf_optcontent.h"
#include "pdf_annot.h"
#include "pdf_colour.h"
#include "pdf_deref.h"
#include "pdf_image.h"
#include "pdf_mark.h"
#include "pdf_font.h"
#include "pdf_text.h"

#include "gspath2.h"
#include "gxfarith.h"
#include "gxdevsop.h"               /* For special ops */
#include "gsstrtok.h"               /* For gs_strtok */
#include "gscoord.h"        /* for gs_concat() and others */
#include "gsline.h"         /* For gs_setlinejoin() and friends */
#include "gsutil.h"        /* For gs_next_ids() */
#include "gspaint.h"        /* For gs_fill() and friends */

/* Detect if there is a BOM at beginning of string */
#define IS_UTF8(str) (!strcmp((char *)(str), "\xef\xbb\xbf"))
#define IS_UTF16(str) (!strcmp((char *)(str), "\xfe\xff"))

typedef int (*annot_func)(pdf_context *ctx, pdf_dict *annot, pdf_obj *NormAP, bool *render_done);

typedef struct {
    const char *subtype;
    annot_func func;
    bool simpleAP; /* Just Render AP if it exists? (false means more complicated) */
} annot_dispatch_t;


typedef int (*annot_LE_func)(pdf_context *ctx, pdf_dict *annot);

typedef struct {
    const char *name;
    annot_LE_func func;
} annot_LE_dispatch_t;

typedef int (*annot_preserve_func)(pdf_context *ctx, pdf_dict *annot, pdf_name *subtype);

typedef struct {
    const char *subtype;
    annot_preserve_func func;
} annot_preserve_dispatch_t;

static int pdfi_annot_start_transparency(pdf_context *ctx, pdf_dict *annot)
{
    int code;

    if (!ctx->page.has_transparency)
        return 0;

    /* TODO: ps code does something with /BM which is apparently the Blend Mode,
     * but I am not sure that the value is actually used properly.  Look into it.
     * (see pdf_draw.ps/startannottransparency)
     */
    code = gs_clippath(ctx->pgs);
    if (code < 0)
        return code;
    code = pdfi_trans_begin_simple_group(ctx, NULL, false, false, false);
    (void)gs_newpath(ctx->pgs);
    return code;
}

static int pdfi_annot_end_transparency(pdf_context *ctx, pdf_dict *annot)
{
    if (!ctx->page.has_transparency)
        return 0;

    return pdfi_trans_end_simple_group(ctx);
}

/* Get width from BS, if it exists
 * Default to 1.0 if not
 */
static int pdfi_annot_get_BS_width(pdf_context *ctx, pdf_dict *annot, double *width)
{
    int code;
    pdf_dict *BS = NULL;

    *width = 1.0;

    code = pdfi_dict_knownget_type(ctx, annot, "BS", PDF_DICT, (pdf_obj **)&BS);
    if (code <= 0)
        goto exit;

    code = pdfi_dict_knownget_number(ctx, BS, "W", width);

 exit:
    pdfi_countdown(BS);
    return code;
}

/* Set both stroke opacity and fill opacity to either CA or ca
 * For annotations, the one value seems to control both.
 */
static int pdfi_annot_opacity(pdf_context *ctx, pdf_dict *annot)
{
    int code = 0;
    double CA;

    /* CA -- opacity */
    code = pdfi_dict_knownget_number(ctx, annot, "CA", &CA);
    if (code > 0) {
        code = gs_setstrokeconstantalpha(ctx->pgs, CA);
        if (code < 0) goto exit;
        code = gs_setfillconstantalpha(ctx->pgs, CA);
        goto exit;
    }
    /* If CA not found, we also check for 'ca' even though it's not in the spec */
    code = pdfi_dict_knownget_number(ctx, annot, "ca", &CA);
    if (code > 0) {
        code = gs_setstrokeconstantalpha(ctx->pgs, CA);
        if (code < 0) goto exit;
        code = gs_setfillconstantalpha(ctx->pgs, CA);
    }
 exit:
    return code;
}

/* Apply RD to provided rect */
static int pdfi_annot_applyRD(pdf_context *ctx, pdf_dict *annot, gs_rect *rect)
{
    int code;
    pdf_array *RD = NULL;
    gs_rect rd;

    code = pdfi_dict_knownget_type(ctx, annot, "RD", PDF_ARRAY, (pdf_obj **)&RD);
    if (code <= 0) goto exit;

    code = pdfi_array_to_gs_rect(ctx, RD, &rd);
    if (code < 0) goto exit;

    rect->p.x += rd.p.x;
    rect->p.y += rd.p.y;
    rect->q.x -= rd.q.x;
    rect->q.y -= rd.q.y;

 exit:
    pdfi_countdown(RD);
    return code;
}

static int pdfi_annot_Rect(pdf_context *ctx, pdf_dict *annot, gs_rect *rect)
{
    int code;
    pdf_array *Rect = NULL;

    code = pdfi_dict_knownget_type(ctx, annot, "Rect", PDF_ARRAY, (pdf_obj **)&Rect);
    if (code < 0) goto exit;

    code = pdfi_array_to_gs_rect(ctx, Rect, rect);
    if (code < 0) goto exit;

    pdfi_normalize_rect(ctx, rect);

 exit:
    pdfi_countdown(Rect);
    return code;
}

static int pdfi_annot_position_AP(pdf_context *ctx, pdf_dict *annot, pdf_stream *AP)
{
    int code = 0;
    gs_rect rect;
    pdf_array *BBox = NULL;
    gs_rect bbox;
    pdf_array *Matrix = NULL;
    gs_matrix matrix;
    double xscale, yscale;
    pdf_dict *Annot_dict;

    code = pdfi_dict_from_obj(ctx, (pdf_obj *)AP, &Annot_dict);
    if (code < 0)
        return code;

    /* TODO: graphicsbeginpage, textbeginpage
     * These should match what was done at the beginning of the page,
     * but current implementation has things kind of mixed around.
     * Need to figure out exactly what to do.  I'd like it to be one
     * function in both places.
     * see pdfi_page_render()
     */
    /* graphicsbeginpage() */
    /* textbeginpage() */
    /* code = gs_initgraphics(ctx->pgs); */
    ctx->text.BlockDepth = 0;
    /* TODO: FIXME */

    code = pdfi_annot_Rect(ctx, annot, &rect);
    if (code < 0) goto exit;

    code = pdfi_dict_knownget_type(ctx, Annot_dict, "BBox", PDF_ARRAY, (pdf_obj **)&BBox);
    if (code < 0) goto exit;
    code = pdfi_array_to_gs_rect(ctx, BBox, &bbox);
    if (code < 0) goto exit;

    if (bbox.q.x - bbox.p.x == 0.0 || bbox.q.y - bbox.p.y == 0.0) {
        /* If the Bounding Box has 0 width or height, then the scaling calculation below will
         * end up with a division by zero, causing the CTM to go infinite.
         * We handle the broken BBox elsewhere, so for now just discard it, preventing
         * us breaking the CTM.
         */
        pdfi_countdown(BBox);
        BBox = NULL;
    }

    code = pdfi_dict_knownget_type(ctx, Annot_dict, "Matrix", PDF_ARRAY, (pdf_obj **)&Matrix);
    if (code < 0) goto exit;
    code = pdfi_array_to_gs_matrix(ctx, Matrix, &matrix);
    if (code < 0) goto exit;

    xscale = yscale = 1.0;

    code = gs_translate(ctx->pgs, rect.p.x, rect.p.y);
    if (code < 0) goto exit;

    if (BBox != NULL) {
        pdfi_bbox_transform(ctx, &bbox, &matrix);

        /* Calculate scale factor */
        xscale = (rect.q.x - rect.p.x) / (bbox.q.x - bbox.p.x);
        yscale = (rect.q.y - rect.p.y) / (bbox.q.y - bbox.p.y);

        if (xscale * yscale <= 0) {
            dbgmprintf(ctx->memory, "ANNOT: Ignoring annotation with scale factor of 0\n");
            if (ctx->args.pdfstoponerror)
                code = gs_note_error(gs_error_rangecheck);
            else
                code = 0;
            pdfi_set_warning(ctx, 0, NULL, W_PDF_ZEROSCALE_ANNOT, "pdfi_annot_position_AP", "");
            goto exit;
        }

        /* Scale it */
        code = gs_scale(ctx->pgs, xscale, yscale);
        if (code < 0) goto exit;

        /* Compensate for non-zero origin of BBox */
        code = gs_translate(ctx->pgs, -bbox.p.x, -bbox.p.y);
        if (code < 0) goto exit;
    }

 exit:
    pdfi_countdown(BBox);
    pdfi_countdown(Matrix);
    return code;
}

/* See pdf_draw.ps/drawwidget (draws the AP for any type of thingy) */
static int pdfi_annot_draw_AP(pdf_context *ctx, pdf_dict *annot, pdf_obj *NormAP)
{
    int code = 0;

    if (NormAP == NULL)
        return 0;
    if (pdfi_type_of(NormAP) == PDF_NULL)
        return 0;
    if (pdfi_type_of(NormAP) != PDF_STREAM)
        return_error(gs_error_typecheck);

    code = pdfi_op_q(ctx);
    if (code < 0)
        return code;

    code = pdfi_annot_position_AP(ctx, annot, (pdf_stream *)NormAP);
    if (code < 0)
        goto exit;

    /* Render the annotation */
    code = pdfi_do_image_or_form(ctx, NULL, ctx->page.CurrentPageDict, NormAP);

exit:
    (void)pdfi_op_Q(ctx);
    return code;
}

static int pdfi_annot_setcolor_key(pdf_context *ctx, pdf_dict *annot, const char *key,
                                   bool usedefault, bool *drawit)
{
    int code = 0;
    pdf_array *C = NULL;

    *drawit = true;

    code = pdfi_dict_knownget_type(ctx, annot, key, PDF_ARRAY, (pdf_obj **)&C);
    if (code < 0) goto exit;

    if (code == 0) {
        if (usedefault)
            code = pdfi_gs_setgray(ctx, 0);
        else
            *drawit = false;
    } else {
        if (pdfi_array_size(C) == 0) {
            code = 0;
            *drawit = false;
        } else {
            code = pdfi_setcolor_from_array(ctx, C);
        }
    }

 exit:
    if (code < 0)
        *drawit = false;
    pdfi_countdown(C);
    return code;
}

static int pdfi_annot_setinteriorcolor(pdf_context *ctx, pdf_dict *annot, bool usedefault, bool *drawit)
{
    return pdfi_annot_setcolor_key(ctx, annot, "IC", usedefault, drawit);
}

static int pdfi_annot_setcolor(pdf_context *ctx, pdf_dict *annot, bool usedefault, bool *drawit)
{
    return pdfi_annot_setcolor_key(ctx, annot, "C", usedefault, drawit);
}

/* Stroke border using current path */
static int pdfi_annot_strokeborderpath(pdf_context *ctx, pdf_dict *annot, double width, pdf_array *dash)
{
    int code = 0;

    if (width <= 0)
        return 0;

    code = pdfi_setdash_impl(ctx, dash, 0);
    if (code < 0) goto exit;
    code = gs_setlinewidth(ctx->pgs, width);
    if (code < 0) goto exit;

    code = gs_stroke(ctx->pgs);

 exit:
    return code;
}

/* Fill border path */
static int pdfi_annot_fillborderpath(pdf_context *ctx, pdf_dict *annot)
{
    int code;
    bool drawit;

    code = pdfi_gsave(ctx);
    if (code < 0) return code;

    code = pdfi_annot_opacity(ctx, annot);
    if (code < 0) goto exit;
    code = pdfi_annot_setinteriorcolor(ctx, annot, false, &drawit);
    if (code < 0) goto exit;
    if (drawit)
        code = gs_fill(ctx->pgs);

 exit:
    (void)pdfi_grestore(ctx);
    return code;
}


/* Draw a path from a rectangle */
static int pdfi_annot_rect_path(pdf_context *ctx, gs_rect *rect)
{
    int code;

    code = gs_moveto(ctx->pgs, rect->p.x, rect->p.y);
    if (code < 0) goto exit;
    code = gs_lineto(ctx->pgs, rect->q.x, rect->p.y);
    if (code < 0) goto exit;
    code = gs_lineto(ctx->pgs, rect->q.x, rect->q.y);
    if (code < 0) goto exit;
    code = gs_lineto(ctx->pgs, rect->p.x, rect->q.y);
    if (code < 0) goto exit;
    code = gs_closepath(ctx->pgs);

 exit:
    return code;
}

/* Make a rectangle path from Rect (see /re, /normal_re in pdf_ops.ps)
 * See also /Square
 * Not sure if I have to worry about the /inside_text_re here
 */
static int pdfi_annot_RectRD_path(pdf_context *ctx, pdf_dict *annot)
{
    gs_rect rect;
    int code;

    code = pdfi_annot_Rect(ctx, annot, &rect);
    if (code < 0) goto exit;

    code = pdfi_annot_applyRD(ctx, annot, &rect);
    if (code < 0) goto exit;

    code = pdfi_annot_rect_path(ctx, &rect);

 exit:
    return code;
}

/* Adjust rectangle by specified width */
static void pdfi_annot_rect_adjust(pdf_context *ctx, gs_rect *rect, double width)
{
    rect->p.x += width;
    rect->p.y += width;
    rect->q.x -= width;
    rect->q.y -= width;
}

/* Fill Rect with current color */
static int pdfi_annot_fillRect(pdf_context *ctx, pdf_dict *annot)
{
    int code;
    gs_rect rect;

    code = pdfi_gsave(ctx);
    if (code < 0) return code;

    code = pdfi_annot_Rect(ctx, annot, &rect);
    if (code < 0) goto exit;

    code = gs_rectclip(ctx->pgs, &rect, 1);
    if (code < 0) goto exit;

    code = pdfi_annot_applyRD(ctx, annot, &rect);
    if (code < 0) goto exit;

    code = gs_rectfill(ctx->pgs, &rect, 1);
    if (code < 0) goto exit;

 exit:
    (void)pdfi_grestore(ctx);
    return code;
}

/* Stroke border, drawing path from rect */
static int pdfi_annot_strokeborder(pdf_context *ctx, pdf_dict *annot, double width, pdf_array *dash)
{
    int code = 0;
    gs_rect rect;

    if (width <= 0)
        return 0;

    code = pdfi_gsave(ctx);

    code = pdfi_setdash_impl(ctx, dash, 0);
    if (code < 0) goto exit;

    code = gs_setlinewidth(ctx->pgs, width);
    if (code < 0) goto exit;

    code = pdfi_annot_Rect(ctx, annot, &rect);
    if (code < 0) goto exit;

    code = pdfi_annot_applyRD(ctx, annot, &rect);
    if (code < 0) goto exit;


    /* Stroke the rectangle */
    /* Adjust rectangle by the width */
    pdfi_annot_rect_adjust(ctx, &rect, width/2);
    code = gs_rectstroke(ctx->pgs, &rect, 1, NULL);

 exit:
    (void)pdfi_grestore(ctx);
    return code;
}

/* Draw border from a Border array
 * Note: Border can be null
 */
static int pdfi_annot_draw_Border(pdf_context *ctx, pdf_dict *annot, pdf_array *Border, bool usepath)
{
    pdf_array *dash = NULL;
    int code = 0;
    uint64_t size = 0;
    double width = 0;

    if (Border)
        size = pdfi_array_size(Border);
    if (Border &&  size < 3) {
        pdfi_set_warning(ctx, 0, NULL, W_PDF_ANNOT_ERROR, "pdfi_annot_draw_Border", "WARNING: Annotation Border array invalid");
        goto exit;
    }
    if (!Border) {
        code = pdfi_array_alloc(ctx, 0, &dash);
        pdfi_countup(dash);
        if (code < 0) goto exit;
        width = 1;
    } else {
        if (size > 3) {
            code = pdfi_array_get_type(ctx, Border, 3, PDF_ARRAY, (pdf_obj **)&dash);
            if (code < 0) {
                pdfi_set_warning(ctx, 0, NULL, W_PDF_ANNOT_ERROR,
                                 "pdfi_annot_draw_Border", "WARNING: Annotation Border Dash array invalid");
                code = pdfi_array_alloc(ctx, 0, &dash);
                if (code < 0) goto exit;
                pdfi_countup(dash);
            }
        } else {
            code = pdfi_array_alloc(ctx, 0, &dash);
            if (code < 0) goto exit;
            pdfi_countup(dash);
        }
        code = pdfi_array_get_number(ctx, Border, 2, &width);
        if (code < 0) goto exit;
    }

    /* At this point we have a dash array (which could be length 0) and a width */
    if (usepath)
        code = pdfi_annot_strokeborderpath(ctx, annot, width, dash);
    else
        code = pdfi_annot_strokeborder(ctx, annot, width, dash);

 exit:
    pdfi_countdown(dash);
    return code;
}

/* Draw border from a BS dict */
static int pdfi_annot_draw_BS(pdf_context *ctx, pdf_dict *annot, pdf_dict *BS, bool usepath)
{
    double W;
    int code = 0;
    pdf_name *S = NULL;
    pdf_array *dash = NULL;

    /* Get the width */
    code = pdfi_dict_knownget_number(ctx, BS, "W", &W);
    if (code < 0) goto exit;
    if (code == 0)
        W = 1; /* Default */

    /* TODO: Some junk about scaling to UserUnit */
    /* ... */

    /* Lookup border style */
    code = pdfi_dict_knownget_type(ctx, BS, "S", PDF_NAME, (pdf_obj **)&S);
    if (code < 0) goto exit;

    if (code > 0 && pdfi_name_is(S, "D")) {
        /* Handle Dash array */
        code = pdfi_dict_knownget_type(ctx, BS, "D", PDF_ARRAY, (pdf_obj **)&dash);
        if (code < 0) goto exit;

        /* If there is no Dash array, then create one containing a 3 (the default) */
        if (code == 0) {
            code = pdfi_array_alloc(ctx, 1, &dash);
            if (code < 0) goto exit;
            pdfi_countup(dash);

            code = pdfi_array_put_int(ctx, dash, 0, 3);
            if (code < 0) goto exit;
        }
    } else {
        /* Empty array */
        code = pdfi_array_alloc(ctx, 0, &dash);
        if (code < 0) goto exit;
        pdfi_countup(dash);
    }

    /* At this point we have a dash array (which could be length 0) and a width */
    if (usepath)
        code = pdfi_annot_strokeborderpath(ctx, annot, W, dash);
    else
        code = pdfi_annot_strokeborder(ctx, annot, W, dash);

 exit:
    pdfi_countdown(S);
    pdfi_countdown(dash);
    return code;
}

/* Draw the border
 * This spec seems to be scattered around.  There might be:
 *  /Border -- an array of 3-4 elements, where the optional 4th element is a "dash array"
 *   (default value: [0 0 1])
 *  /BS -- a Border Style dictionary
 *  Spec says /Border would be ignored in favor of /BS
 *
 * usepath -- indicates whether to stroke existing path, or use Rect to draw a path first
 */
static int pdfi_annot_draw_border(pdf_context *ctx, pdf_dict *annot, bool usepath)
{
    int code, code1;
    pdf_dict *BS = NULL;
    pdf_array *Border = NULL;

    code = pdfi_dict_knownget_type(ctx, annot, "BS", PDF_DICT, (pdf_obj **)&BS);
    if (code < 0) goto exit;
    code = pdfi_dict_knownget_type(ctx, annot, "Border", PDF_ARRAY, (pdf_obj **)&Border);
    if (code < 0) goto exit;

    code = pdfi_gsave(ctx);
    if (code < 0) goto exit;

    if (BS) {
        code = pdfi_annot_draw_BS(ctx, annot, BS, usepath);
    } else {
        /* Note: Border can be null */
        code = pdfi_annot_draw_Border(ctx, annot, Border, usepath);
    }
    code1 = pdfi_grestore(ctx);
    if (code == 0) code = code1;

 exit:
    pdfi_countdown(BS);
    pdfi_countdown(Border);
    return code;
}

/*********** BEGIN Font/Text ***************/

/* Set font */
static int
pdfi_annot_set_font(pdf_context *ctx, const char *font, double size)
{
    int code = 0;

    if (font != NULL) {
        code = pdfi_font_set_internal_string(ctx, font, size);
        if (code < 0) goto exit;
    }

 exit:
    return code;
}

/* Get value from a field, or its Parents (recursive)
 * If no Parent, will also check ctx->AcroForm
 * Returns <0 on error, 0 if not found, >0 if found
 */
static int pdfi_form_get_inheritable(pdf_context *ctx, pdf_dict *field, const char *Key,
                                     pdf_obj_type type, pdf_obj **o)
{
    int code = 0;
    pdf_dict *Parent = NULL;
    bool known = false;

    /* Check this field */
    code = pdfi_dict_knownget_type(ctx, field, Key, type, o);
    if (code != 0) goto exit1;

    code = pdfi_loop_detector_mark(ctx);
    if (code < 0)
        goto exit;

    /* Check for Parent. Do not store the dereferenced Parent back to the dictionary
     * as this can cause circular references.
     */
    code = pdfi_dict_known(ctx, field, "Parent", &known);
    if (code >= 0 && known == true)
    {
        code = pdfi_dict_get_no_store_R(ctx, field, "Parent", (pdf_obj **)&Parent);
        if (code < 0)
            goto exit;

        if (pdfi_type_of(Parent) != PDF_DICT) {
            if (pdfi_type_of(Parent) == PDF_INDIRECT) {
                pdf_indirect_ref *o = (pdf_indirect_ref *)Parent;

                code = pdfi_dereference(ctx, o->ref_object_num, o->ref_generation_num, (pdf_obj **)&Parent);
                pdfi_countdown(o);
                goto exit;
            } else {
                code = gs_note_error(gs_error_typecheck);
                goto exit;
            }
        }
        code = pdfi_form_get_inheritable(ctx, Parent, Key, type, o);
        if (code <= 0) {
            if (ctx->AcroForm)
                code = pdfi_dict_knownget_type(ctx, ctx->AcroForm, Key, type, o);
        }
    } else {
        /* No Parent, so check AcroForm, if any */
        if (ctx->AcroForm)
            code = pdfi_dict_knownget_type(ctx, ctx->AcroForm, Key, type, o);
    }

exit:
    (void)pdfi_loop_detector_cleartomark(ctx);

exit1:
    pdfi_countdown(Parent);
    return code;
}

/* Get int value from a field, or its Parents (recursive)
 * Returns <0 on error, 0 if not found, >0 if found
 */
static int pdfi_form_get_inheritable_int(pdf_context *ctx, pdf_dict *field, const char *Key,
                                         int64_t *val)
{
    int code = 0;
    pdf_num *num = NULL;

    *val = 0;
    code = pdfi_form_get_inheritable(ctx, field, Key, PDF_INT, (pdf_obj **)&num);
    if (code < 0) goto exit;

    if (code > 0)
        *val = num->value.i;

 exit:
    pdfi_countdown(num);
    return code;
}

/* Scan DA to see if font size is 0, if so make a new DA with font size scaled by gs_rect */
static int pdfi_form_modDA(pdf_context *ctx, pdf_string *DA, pdf_string **mod_DA, gs_rect *rect)
{
    char *token, *prev_token;
    char *parse_str = NULL;
    bool did_mod = false;
    int code = 0;
    double size;
    char size_str[20];
    char *last;
    pdf_string *newDA = NULL;

    /* Make a copy of the string because we are going to destructively parse it */
    parse_str = (char *)gs_alloc_bytes(ctx->memory, DA->length+1, "pdfi_annot_display_text(strbuf)");
    if (parse_str == NULL) {
        code = gs_note_error(gs_error_VMerror);
        goto exit;
    }
    memcpy(parse_str, DA->data, DA->length);
    parse_str[DA->length] = 0; /* Null terminate */

    /* find the 'Tf' token, if any */
    token = gs_strtok(parse_str, " ", &last);
    prev_token = NULL;
    while (token != NULL) {
        if (!strcmp(token, "Tf"))
            break;
        prev_token = token;
        token = gs_strtok(NULL, " ", &last);
    }

    /* See if we found it */
    if (!token)
        goto exit;

    /* See if there was a prev_token and it was "0" */
    if (!(prev_token && !strcmp(prev_token, "0")))
        goto exit;

    /* Case with '<font> 0 Tf', need to calculate correct size */
    size = (rect->q.y - rect->p.y) * .75; /* empirical from gs code make_tx_da */

    snprintf(size_str, sizeof(size_str), "%g ", size);

    /* Create a new DA and reassemble the old DA into it */
    code = pdfi_object_alloc(ctx, PDF_STRING, DA->length+strlen(size_str)+1, (pdf_obj **)&newDA);
    if (code < 0)
        goto exit;
    pdfi_countup(newDA);

    strncpy((char *)newDA->data, (char *)DA->data, prev_token-parse_str);
    strncpy((char *)newDA->data + (prev_token-parse_str), size_str, strlen(size_str)+1);
    strncpy((char *)newDA->data + strlen((char *)newDA->data), (char *)DA->data + (token - parse_str),
            DA->length-(token-parse_str));
    newDA->length = strlen((char *)newDA->data);
    did_mod = true;

 exit:
    /* If didn't need to mod it, just copy the original and get a ref */
    if (did_mod) {
        *mod_DA = newDA;
    } else {
        *mod_DA = DA;
        pdfi_countup(DA);
    }

    if (parse_str)
        gs_free_object(ctx->memory, parse_str, "pdfi_form_modDA(parse_str)");

    return code;
}

/* Process the /DA string by either running it through the interpreter
 *  or providing a bunch of defaults
 *
 * According to the spec, if there is a "<fontname> <size> Tf" in the /DA,
 * and the <size> = 0, we autosize it using the annotation's rect.  (see PDF1.7, pages 678-679)
 *
 * rect -- the annotation's rect, for auto-sizing the font
 * is_form -- is this a form, so check inheritable?
 */
static int pdfi_annot_process_DA(pdf_context *ctx, pdf_dict *page_dict, pdf_dict *annot, gs_rect *rect,
                                 bool is_form)
{
    int code = 0;
    pdf_string *DA = NULL;
    pdf_string *mod_DA = NULL;
    pdf_dict *resource_dict = annot;  /* dict to use for resources, alias no need to refcnt */
    pdf_dict *DR = NULL;
    bool known;

    /* TODO -- see gs code make_tx_da, need to handle case where we need to set a UTF-16 font */

    if (!page_dict)
        page_dict = ctx->page.CurrentPageDict;

    if (is_form) {
        code = pdfi_dict_known(ctx, annot, "DR", &known);
        if (code < 0) goto exit;
        /* If there is no "DR" and no Parent in the annotation, have it use the AcroForm instead
         * This is a hack.  May need to use fancier inheritance approach.
         */
        if (!known) {
            code = pdfi_dict_known(ctx, annot, "Parent", &known);
            if (code < 0) goto exit;
            if (!known && ctx->AcroForm != NULL)
                resource_dict = ctx->AcroForm;
        }
        code = pdfi_form_get_inheritable(ctx, annot, "DA", PDF_STRING, (pdf_obj **)&DA);
    } else {
        code = pdfi_dict_knownget_type(ctx, annot, "DA", PDF_STRING, (pdf_obj **)&DA);
    }
    if (code < 0) goto exit;
    if (code > 0) {
        if (is_form) {
            code = pdfi_form_modDA(ctx, DA, &mod_DA, rect);
            if (code < 0) goto exit;
        } else {
            mod_DA = DA;
            pdfi_countup(mod_DA);
        }

        code = pdfi_interpret_inner_content_string(ctx, mod_DA, resource_dict,
                                                   page_dict, false, "DA");
        if (code < 0) goto exit;
        /* If no font got set, set one */
        if (pdfi_get_current_pdf_font(ctx) == NULL) {
            code = pdfi_annot_set_font(ctx, "Helvetica", 12.0);
            if (code < 0) goto exit;
        }
    } else {
        code = pdfi_gs_setgray(ctx, 0);
        if (code < 0) goto exit;
        code = pdfi_annot_set_font(ctx, "Helvetica", 12.0);
        if (code < 0) goto exit;
    }

 exit:
    pdfi_countdown(DR);
    pdfi_countdown(DA);
    pdfi_countdown(mod_DA);
    return code;
}

/* Put a string into buffer as a hexstring and return ptr to end of it
 * Skip over any BOM that is present.
 */
static char *pdfi_get_hexstring(pdf_context *ctx, char *outbuf, byte *inbuf, int len)
{
    int i;
    char *ptr;

    i = 0;
    /* skip over BOM if it's there */
    if (IS_UTF16(inbuf)) {
        i += 2; /* UTF-16 */
    } else if (IS_UTF16(inbuf)) {
        i += 3; /* UTF-8 */
    }

    ptr = outbuf;
    *ptr++ = '<';
    for ( ; i<len; i++) {
        snprintf(ptr, 3, "%02X", *(inbuf+i));
        ptr += 2;
    }

    *ptr++ = '>';
    *ptr = 0;

    return ptr;
}

/* Display text at current position (inside BT/ET) */
static int
pdfi_annot_display_nexttext(pdf_context *ctx, pdf_dict *annot, pdf_string *text)
{
    char *strbuf = NULL;
    size_t buflen = 50 + text->length*2; /* 50 to account for formatting, plus the text itself */
    int code = 0;
    char *ptr;

    strbuf = (char *)gs_alloc_bytes(ctx->memory, buflen, "pdfi_annot_display_text(strbuf)");
    if (strbuf == NULL)
        return_error(gs_error_VMerror);
    ptr = pdfi_get_hexstring(ctx, strbuf, text->data, text->length);
    strncpy(ptr, " Tj", buflen-strlen(strbuf));

    code = pdfi_interpret_inner_content_c_string(ctx, strbuf, annot,
                                               ctx->page.CurrentPageDict, false, "Annot text Tj");
    if (code < 0) goto exit;

 exit:
    if (strbuf)
        gs_free_object(ctx->memory, strbuf, "pdfi_annot_display_text(strbuf)");
    return code;
}

/* Display text with positioning (inside BT/ET) */
static int
pdfi_annot_display_text(pdf_context *ctx, pdf_dict *annot, double x, double y, pdf_string *text)
{
    char *strbuf = NULL;
    size_t buflen = 50 + text->length*2; /* 50 to account for formatting, plus the text itself */
    int code = 0;
    char *ptr;

    strbuf = (char *)gs_alloc_bytes(ctx->memory, buflen, "pdfi_annot_display_text(strbuf)");
    if (strbuf == NULL)
        return_error(gs_error_VMerror);
    snprintf(strbuf, buflen, "%g %g Td ", x, y);
    ptr = strbuf + strlen(strbuf);
    ptr = pdfi_get_hexstring(ctx, ptr, text->data, text->length);
    strncpy(ptr, " Tj", buflen-strlen(strbuf));

    code = pdfi_interpret_inner_content_c_string(ctx, strbuf, annot,
                                               ctx->page.CurrentPageDict, false, "Annot text Tj");
    if (code < 0) goto exit;

 exit:
    if (strbuf)
        gs_free_object(ctx->memory, strbuf, "pdfi_annot_display_text(strbuf)");
    return code;
}

/* Get Text height for current font (assumes font is already set, inside BT/ET)
 * TODO: See /Tform implementation that uses FontBBox and ScaleMatrix instead
 * Maybe use this technique as a backup if those entries aren't available?
 */
static int
pdfi_annot_get_text_height(pdf_context *ctx, double *height)
{
    int code;
    pdf_string *temp_string = NULL;
    gs_rect bbox;
    gs_point awidth;

    if (ctx->pgs->PDFfontsize == 0) {
        *height = 0;
        return 0;
    }

    code = pdfi_obj_charstr_to_string(ctx, "Hy", &temp_string);
    if (code < 0)
        goto exit;

    /* Find the bbox of the string "Hy" */
    code = pdfi_string_bbox(ctx, temp_string, &bbox, &awidth, false);
    if (code < 0)
        goto exit;

    *height = bbox.q.y - bbox.p.y;

 exit:
    pdfi_countdown(temp_string);
    return code;
}

/* Display a simple text string (outside BT/ET) */
static int
pdfi_annot_display_simple_text(pdf_context *ctx, pdf_dict *annot, double x, double y, pdf_string *text)
{
    int code = 0;
    int code1 = 0;

    code = pdfi_BT(ctx);
    if (code < 0)
        return code;

    code = pdfi_annot_display_text(ctx, annot, x, y, text);
    code1 = pdfi_ET(ctx);
    if (code == 0) code = code1;

    return code;
}

/* Display a centered text string (outside BT/ET) */
static int
pdfi_annot_display_centered_text(pdf_context *ctx, pdf_dict *annot, gs_rect *rect, pdf_string *text)
{
    int code = 0;
    int code1 = 0;
    gs_rect bbox;
    gs_point awidth;
    double x, y;

    code = pdfi_BT(ctx);
    if (code < 0)
        return code;

    /* Get width of the string */
    code = pdfi_string_bbox(ctx, text, &bbox, &awidth, false);
    if (code < 0) goto exit;

    /* Center the title in the box */
    x = rect->p.x + ((rect->q.x - rect->p.x) - awidth.x) / 2;
    y = rect->q.y - 11;

    code = pdfi_annot_display_text(ctx, annot, x, y, text);
    if (code < 0) goto exit;

 exit:
    code1 = pdfi_ET(ctx);
    if (code == 0) code = code1;
    return code;
}

/* Display a string formatted to fit in rect (outside BT/ET)
 *
 * TODO: I am sharing code between the FreeText and field/Tx implementation.
 * The gs code has completely different implementations for these.
 * I am not sure if there are some different assumptions about font encodings?
 * The Tx field can apparently be UTF8 or UTF16, whereas the FreeText implementation just assumed
 * it was ASCII.
 * If you see some weird behavior with character mappings or line breaks, then
 * this might be something to revisit.
 */
static int
pdfi_annot_display_formatted_text(pdf_context *ctx, pdf_dict *annot,
                                  gs_rect *rect, pdf_string *text, bool is_UTF16)
{
    double x;
    double lineheight = 0;
    double y_start;
    double x_start, x_max;
    gs_rect bbox;
    gs_point awidth; /* Advance width */
    int code = 0;
    int code1 = 0;
    pdf_string *temp_string = NULL;
    int i;
    byte ch;
    bool firstchar = true;
    bool linestart = true;
    int charlen;

    if (is_UTF16)
        charlen = 2;
    else
        charlen = 1;

    if (ctx->pgs->PDFfontsize == 0)
        return 0;

    code = pdfi_BT(ctx);
    if (code < 0)
        return code;

    /* Allocate a temp string to use, length 1 char */
    code = pdfi_object_alloc(ctx, PDF_STRING, charlen, (pdf_obj **)&temp_string);
    if (code < 0) goto exit;
    pdfi_countup(temp_string);

    code = pdfi_annot_get_text_height(ctx, &lineheight);
    if (code < 0) goto exit;

    y_start = rect->q.y - lineheight;
    x_start = rect->p.x;
    x_max = rect->q.x;
    x = x_start;

    for (i=0; i<text->length; i+=charlen) {
        int j;

        if (linestart) {
            x = x_start;
        }

        for (j = 0; j < charlen; j++) {
            ch = text->data[i+j];
            temp_string->data[j] = ch;
        }

        /* If EOL character encountered, move down to next line */
        if (charlen == 1) { /* Can only check this for ASCII font */
            if (ch == '\r' || ch == '\n') {
                if (linestart == true) {
                    pdf_string dummy;

                    dummy.length = 0;
                    code = pdfi_annot_display_text(ctx, annot, 0, -lineheight, &dummy);
                    if (code < 0) goto exit;
                }
                linestart = true;
                continue;
            }
        }

        /* get size of the character */
        code = pdfi_string_bbox(ctx, temp_string, &bbox, &awidth, false);
        if (code < 0) goto exit;

        if (!linestart && ((x + awidth.x) > x_max)) {
            x = x_start;
            linestart = true;
        }

        /* display the character */
        if (firstchar) {
            code = pdfi_annot_display_text(ctx, annot, x, y_start, temp_string);
            firstchar = false;
        } else {
            if (linestart)
                code = pdfi_annot_display_text(ctx, annot, 0, -lineheight, temp_string);
            else
                code = pdfi_annot_display_nexttext(ctx, annot, temp_string);
        }
        if (code < 0) goto exit;
        x += awidth.x;
        linestart = false;
    }

 exit:
    code1 = pdfi_ET(ctx);
    if (code == 0) code = code1;
    pdfi_countdown(temp_string);
    return code;
}

/*********** END Font/Text ***************/


/*********** BEGIN /LE ***************/

/* Draw Butt LE */
static int pdfi_annot_draw_LE_Butt(pdf_context *ctx, pdf_dict *annot)
{
    int code;
    double width;
    double seglength;

    code = pdfi_annot_get_BS_width(ctx, annot, &width);
    if (code < 0) goto exit;

    seglength = 3*width;
    code = gs_moveto(ctx->pgs, 0, -seglength);
    if (code < 0) goto exit;
    code = gs_lineto(ctx->pgs, 0, seglength);
    if (code < 0) goto exit;
    code = pdfi_annot_draw_border(ctx, annot, true);
    if (code < 0) goto exit;

 exit:
    return code;
}

/* Draw Circle LE */
static int pdfi_annot_draw_LE_Circle(pdf_context *ctx, pdf_dict *annot)
{
    double radius;
    int code;
    double width;

    code = pdfi_annot_get_BS_width(ctx, annot, &width);
    if (code < 0) goto exit;

    code = pdfi_gsave(ctx);
    if (code < 0) goto exit;

    radius = width * 2.5;
    code = gs_moveto(ctx->pgs, radius, 0);
    if (code < 0) goto exit_grestore;
    code = gs_arc(ctx->pgs, 0, 0, radius, 0, 360);
    if (code < 0) goto exit_grestore;
    code = pdfi_annot_opacity(ctx, annot);
    if (code < 0) goto exit_grestore;
    code = pdfi_annot_fillborderpath(ctx, annot);
    if (code < 0) goto exit_grestore;

    code = pdfi_grestore(ctx);
    if (code < 0) goto exit;

    radius = width * 3;
    code = gs_moveto(ctx->pgs, radius, 0);
    if (code < 0) goto exit;
    code = gs_arc(ctx->pgs, 0, 0, radius, 0, 360);
    if (code < 0) goto exit;
    code = pdfi_annot_draw_border(ctx, annot, true);
    goto exit;

 exit_grestore:
    (void)pdfi_grestore(ctx);
 exit:
    return code;
}

/* Draw Diamond LE */
static int pdfi_annot_draw_LE_Diamond(pdf_context *ctx, pdf_dict *annot)
{
    int code;
    double width;
    double seglength;

    code = pdfi_annot_get_BS_width(ctx, annot, &width);
    if (code < 0) goto exit;

    code = pdfi_gsave(ctx);
    if (code < 0) goto exit;

    seglength = width * 2.5;
    code = gs_moveto(ctx->pgs, 0, -seglength);
    if (code < 0) goto exit_grestore;
    code = gs_lineto(ctx->pgs, -seglength, 0);
    if (code < 0) goto exit_grestore;
    code = gs_lineto(ctx->pgs, 0, seglength);
    if (code < 0) goto exit_grestore;
    code = gs_lineto(ctx->pgs, seglength, 0);
    if (code < 0) goto exit_grestore;
    code = gs_closepath(ctx->pgs);
    if (code < 0) goto exit_grestore;
    code = pdfi_annot_opacity(ctx, annot);
    if (code < 0) goto exit_grestore;
    code = pdfi_annot_fillborderpath(ctx, annot);
    if (code < 0) goto exit_grestore;

    code = pdfi_grestore(ctx);
    if (code < 0) goto exit;

    seglength = width * 3;
    code = gs_moveto(ctx->pgs, 0, -seglength);
    if (code < 0) goto exit;
    code = gs_lineto(ctx->pgs, -seglength, 0);
    if (code < 0) goto exit;
    code = gs_lineto(ctx->pgs, 0, seglength);
    if (code < 0) goto exit;
    code = gs_lineto(ctx->pgs, seglength, 0);
    if (code < 0) goto exit;
    code = gs_closepath(ctx->pgs);
    if (code < 0) goto exit;
    code = pdfi_annot_draw_border(ctx, annot, true);
    goto exit;

 exit_grestore:
    (void)pdfi_grestore(ctx);
 exit:
    return code;
}

/* Draw Square LE */
static int pdfi_annot_draw_LE_Square(pdf_context *ctx, pdf_dict *annot)
{
    int code;
    double width;
    double seglength;

    code = pdfi_annot_get_BS_width(ctx, annot, &width);
    if (code < 0) goto exit;

    code = pdfi_gsave(ctx);
    if (code < 0) goto exit;

    seglength = width * 2.5;
    code = gs_moveto(ctx->pgs, -seglength, -seglength);
    if (code < 0) goto exit_grestore;
    code = gs_lineto(ctx->pgs, -seglength, seglength);
    if (code < 0) goto exit_grestore;
    code = gs_lineto(ctx->pgs, seglength, seglength);
    if (code < 0) goto exit_grestore;
    code = gs_lineto(ctx->pgs, seglength, -seglength);
    if (code < 0) goto exit_grestore;
    code = gs_closepath(ctx->pgs);
    if (code < 0) goto exit_grestore;
    code = pdfi_annot_opacity(ctx, annot);
    if (code < 0) goto exit_grestore;
    code = pdfi_annot_fillborderpath(ctx, annot);
    if (code < 0) goto exit_grestore;

    code = pdfi_grestore(ctx);
    if (code < 0) goto exit;

    seglength = width * 3;
    code = gs_moveto(ctx->pgs, -seglength, -seglength);
    if (code < 0) goto exit;
    code = gs_lineto(ctx->pgs, -seglength, seglength);
    if (code < 0) goto exit;
    code = gs_lineto(ctx->pgs, seglength, seglength);
    if (code < 0) goto exit;
    code = gs_lineto(ctx->pgs, seglength, -seglength);
    if (code < 0) goto exit;
    code = gs_closepath(ctx->pgs);
    if (code < 0) goto exit;
    code = pdfi_annot_draw_border(ctx, annot, true);
    goto exit;

 exit_grestore:
    (void)pdfi_grestore(ctx);
 exit:
    return code;
}

/* Draw Slash LE */
static int pdfi_annot_draw_LE_Slash(pdf_context *ctx, pdf_dict *annot)
{
    int code;
    double width;
    double seglength;

    code = pdfi_annot_get_BS_width(ctx, annot, &width);
    if (code < 0) goto exit;

    seglength = 3*width;
    code = gs_rotate(ctx->pgs, 330);
    if (code < 0) goto exit;
    code = gs_moveto(ctx->pgs, 0, -seglength);
    if (code < 0) goto exit;
    code = gs_lineto(ctx->pgs, 0, seglength);
    if (code < 0) goto exit;
    code = pdfi_annot_draw_border(ctx, annot, true);
    if (code < 0) goto exit;

 exit:
    return code;
}

/* Draw ClosedArrow LE */
static int pdfi_annot_draw_LE_ClosedArrow(pdf_context *ctx, pdf_dict *annot)
{
    int code;
    double width;
    double seglen;

    code = pdfi_annot_get_BS_width(ctx, annot, &width);
    if (code < 0) goto exit;

    code = pdfi_gsave(ctx);
    if (code < 0) goto exit_grestore;

    code = gs_setlinejoin(ctx->pgs, 0);
    if (code < 0) goto exit_grestore;
    code = gs_moveto(ctx->pgs, -width*6, -width*4);
    if (code < 0) goto exit_grestore;
    code = gs_lineto(ctx->pgs, -width/1.2, 0);
    if (code < 0) goto exit_grestore;
    code = gs_lineto(ctx->pgs, -width*6, width*4);
    if (code < 0) goto exit_grestore;
    code = gs_closepath(ctx->pgs);
    if (code < 0) goto exit_grestore;
    code = pdfi_annot_draw_border(ctx, annot, true);
    if (code < 0) goto exit_grestore;

    code = pdfi_grestore(ctx);
    if (code < 0) goto exit;

    code = gs_translate(ctx->pgs, -1.3*width, 0);
    if (code < 0) goto exit;
    seglen = width / 2;
    code = gs_moveto(ctx->pgs, -seglen*8.4, -seglen*5.9);
    if (code < 0) goto exit;
    code = gs_lineto(ctx->pgs, -seglen/1.2, 0);
    if (code < 0) goto exit;
    code = gs_lineto(ctx->pgs, -seglen*8.4, seglen*5.9);
    if (code < 0) goto exit;
    code = gs_closepath(ctx->pgs);
    if (code < 0) goto exit;
    code = pdfi_annot_opacity(ctx, annot);
    if (code < 0) goto exit;
    code = pdfi_annot_fillborderpath(ctx, annot);
    goto exit;

 exit_grestore:
    (void)pdfi_grestore(ctx);
 exit:
    return code;
}

/* Draw OpenArrow LE */
static int pdfi_annot_draw_LE_OpenArrow(pdf_context *ctx, pdf_dict *annot)
{
    int code;
    double width;

    code = pdfi_annot_get_BS_width(ctx, annot, &width);
    if (code < 0) goto exit;

    code = gs_setlinejoin(ctx->pgs, 0);
    if (code < 0) goto exit;

    code = gs_moveto(ctx->pgs, -width*6, -width*4);
    if (code < 0) goto exit;
    code = gs_lineto(ctx->pgs, -width/1.2, 0);
    if (code < 0) goto exit;
    code = gs_lineto(ctx->pgs, -width*6, width*4);
    if (code < 0) goto exit;
    code = pdfi_annot_draw_border(ctx, annot, true);
    if (code < 0) goto exit;

 exit:
    return code;
}

/* Draw RClosedArrow LE */
static int pdfi_annot_draw_LE_RClosedArrow(pdf_context *ctx, pdf_dict *annot)
{
    int code;

    code = gs_rotate(ctx->pgs, 180);
    if (code < 0) goto exit;
    code = pdfi_annot_draw_LE_ClosedArrow(ctx, annot);
    if (code < 0) goto exit;

 exit:
    return code;
}

/* Draw ROpenArrow LE */
static int pdfi_annot_draw_LE_ROpenArrow(pdf_context *ctx, pdf_dict *annot)
{
    int code;

    code = gs_rotate(ctx->pgs, 180);
    if (code < 0) goto exit;
    code = pdfi_annot_draw_LE_OpenArrow(ctx, annot);
    if (code < 0) goto exit;

 exit:
    return code;
}

/* Draw None or null LE (do nothing) */
static int pdfi_annot_draw_LE_None(pdf_context *ctx, pdf_dict *annot)
{
    return 0;
}

annot_LE_dispatch_t annot_LE_dispatch[] = {
    {"Butt", pdfi_annot_draw_LE_Butt},
    {"Circle", pdfi_annot_draw_LE_Circle},
    {"Diamond", pdfi_annot_draw_LE_Diamond},
    {"Slash", pdfi_annot_draw_LE_Slash},
    {"Square", pdfi_annot_draw_LE_Square},
    {"ClosedArrow", pdfi_annot_draw_LE_ClosedArrow},
    {"OpenArrow", pdfi_annot_draw_LE_OpenArrow},
    {"RClosedArrow", pdfi_annot_draw_LE_RClosedArrow},
    {"ROpenArrow", pdfi_annot_draw_LE_ROpenArrow},
    {"None", pdfi_annot_draw_LE_None},
    { NULL, NULL},
};

/* Draw one line ending at (x,y) */
static int pdfi_annot_draw_LE_one(pdf_context *ctx, pdf_dict *annot, pdf_name *LE,
                                  double x, double y, double angle)
{
    int code;
    int code1;
    annot_LE_dispatch_t *dispatch_ptr;

    code = pdfi_gsave(ctx);
    if (code < 0) goto exit1;

    code = gs_translate(ctx->pgs, x, y);
    code = gs_moveto(ctx->pgs, 0, 0);
    code = gs_rotate(ctx->pgs, angle);

    /* Draw the LE */
    for (dispatch_ptr = annot_LE_dispatch; dispatch_ptr->name; dispatch_ptr ++) {
        if (pdfi_name_is(LE, dispatch_ptr->name)) {
            code = dispatch_ptr->func(ctx, annot);
            break;
        }
    }
    if (!dispatch_ptr->name) {
        char str[100];
        memcpy(str, (const char *)LE->data, LE->length < 100 ? LE->length : 99);
        str[LE->length < 100 ? LE->length : 99] = '\0';
        dbgmprintf1(ctx->memory, "ANNOT: WARNING No handler for LE %s\n", str);
    }

 exit1:
    code1 = pdfi_grestore(ctx);
    if (code < 0)
        code = code1;
    return code;
}

/* Draw line endings using LE entry in annotation dictionary
 * Draws one at (x1,y1) and one at (x2,y2)
 * If LE is a name instead of an array, only draws at x2,y2 (but needs x1,y1 for angle)
 *  (defaults to None if not there)
 *
 * which -- tells whether to draw both ends (0) or just the first one (1) or second one (2)
 */
static int pdfi_annot_draw_LE(pdf_context *ctx, pdf_dict *annot,
                              double x1, double y1, double x2, double y2, int which)
{
    pdf_obj *LE = NULL;
    pdf_name *LE1 = NULL;
    pdf_name *LE2 = NULL;
    double dx, dy;
    double angle;
    int code;
    pdf_obj_type type;

    code = pdfi_dict_knownget(ctx, annot, "LE", (pdf_obj **)&LE);
    if (code <= 0)
        goto exit;
    type = pdfi_type_of(LE);
    if (type != PDF_ARRAY && type != PDF_NAME) {
        code = gs_note_error(gs_error_typecheck);
        goto exit;
    }

    dx = x2 - x1;
    dy = y2 - y1;
    code = gs_atan2_degrees(dy, dx, &angle);
    if (code < 0)
        angle = 0;

    if (type == PDF_ARRAY) {
        code = pdfi_array_get_type(ctx, (pdf_array *)LE, 0, PDF_NAME, (pdf_obj **)&LE1);
        if (code < 0) goto exit;

        code = pdfi_array_get_type(ctx, (pdf_array *)LE, 1, PDF_NAME, (pdf_obj **)&LE2);
        if (code < 0) goto exit;
    } else {
        LE1 = (pdf_name *)LE;
        LE = NULL;
    }
    if (LE1 && (!which || which == 1)) {
        code = pdfi_annot_draw_LE_one(ctx, annot, LE1, x1, y1, angle+180);
        if (code < 0) goto exit;
    }

    if (LE2 && (!which || which == 2)) {
        code = pdfi_annot_draw_LE_one(ctx, annot, LE2, x2, y2, angle);
        if (code < 0) goto exit;
    }

 exit:
    pdfi_countdown(LE);
    pdfi_countdown(LE1);
    pdfi_countdown(LE2);
    return code;
}
/*********** END /LE ***************/


/* Get the Normal AP dictionary/stream, if it exists
 * Not an error if it doesn't exist, it will just be NULL
 */
static int pdfi_annot_get_NormAP(pdf_context *ctx, pdf_dict *annot, pdf_obj **NormAP)
{
    int code;
    pdf_dict *AP_dict = NULL;
    pdf_stream *AP = NULL;
    pdf_obj *baseAP = NULL;
    pdf_name *AS = NULL;

    *NormAP = NULL;

    code = pdfi_dict_knownget_type(ctx, annot, "AP", PDF_DICT, (pdf_obj **)&AP_dict);
    if (code <= 0) goto exit;

    code = pdfi_dict_knownget(ctx, AP_dict, "N", (pdf_obj **)&baseAP);
    if (code < 0) goto exit;

    /* Look for /R and /D if there was no /N */
    if (code == 0) {
        pdfi_set_warning(ctx, 0, NULL, W_PDF_ANNOT_AP_ERROR, "pdfi_annot_get_NormAP", "*** Error: Annotation (AP) lacks the mandatory normal (N) appearance");

        code = pdfi_dict_knownget(ctx, AP_dict, "R", (pdf_obj **)&baseAP);
        if (code < 0) goto exit;

        if (code == 0) {
            code = pdfi_dict_knownget(ctx, AP_dict, "D", (pdf_obj **)&baseAP);
            if (code < 0) goto exit;
        }
    }

    /* Nothing found */
    if (code == 0) goto exit;

    switch (pdfi_type_of(baseAP)) {
        case PDF_STREAM:
            /* Use baseAP for the AP */
            AP = (pdf_stream *)baseAP;
            baseAP = NULL;
            break;
        case PDF_DICT:
            code = pdfi_dict_knownget_type(ctx, annot, "AS", PDF_NAME, (pdf_obj **)&AS);
            if (code < 0) goto exit;
            if (code == 0) {
                pdfi_set_warning(ctx, 0, NULL, W_PDF_ANNOT_AP_ERROR, "pdfi_annot_get_NormAP", "WARNING Annotation has non-stream AP but no AS.  Don't know what to render");
                goto exit;
            }

            /* Lookup the AS in the NormAP and use that as the AP */
            code = pdfi_dict_get_by_key(ctx, (pdf_dict *)baseAP, AS, (pdf_obj **)&AP);
            if (code < 0) {
                /* Apparently this is not an error, just silently don't have an AP */
                *NormAP = (pdf_obj *)TOKEN_null;
                code = 0;
                goto exit;
            }
            if (pdfi_type_of(AP) != PDF_STREAM) {
                code = gs_note_error(gs_error_typecheck);
                goto exit;
            }
            break;
        default:
            code = gs_error_typecheck;
            goto exit;
    }

   *NormAP = (pdf_obj *)AP;
   pdfi_countup(AP);

 exit:
    pdfi_countdown(AP_dict);
    pdfi_countdown(AP);
    pdfi_countdown(AS);
    pdfi_countdown(baseAP);
    return code;
}

static int pdfi_annot_draw_Link(pdf_context *ctx, pdf_dict *annot, pdf_obj *NormAP, bool *render_done)
{
    int code;
    int code1;
    bool drawit;

    dbgmprintf(ctx->memory, "ANNOT: Drawing Link\n");

    code = pdfi_annot_start_transparency(ctx, annot);
    if (code < 0)
        return code;

    code = pdfi_annot_setcolor(ctx, annot, true, &drawit);
    if (code < 0) goto exit;
    if (!drawit) goto exit;

    code = pdfi_annot_draw_border(ctx, annot, false);
    if (code < 0) goto exit;

    code = pdfi_annot_draw_AP(ctx, annot, NormAP);

 exit:
    code1 = pdfi_annot_end_transparency(ctx, annot);
    if (code == 0) code = code1;

    *render_done = true;
    return code;
}

/* Draw a path from an InkList
 * see zpdfops.c/zpdfinkpath()
 *
 * Construct a smooth path passing though a number of points  on the stack
 * for PDF ink annotations. The program is based on a very simple method of
 * smoothing polygons by Maxim Shemanarev.
 * http://www.antigrain.com/research/bezier_interpolation/
 *
 * InkList is array of arrays, each representing a stroked path
 */
static int pdfi_annot_draw_InkList(pdf_context *ctx, pdf_dict *annot, pdf_array *InkList)
{
    int code = 0;
    pdf_array *points = NULL;
    int i;
    int num_points;
    double x1, y1, x2, y2;

    for (i=0; i<pdfi_array_size(InkList); i++) {
        int j;

        code = pdfi_array_get_type(ctx, InkList, i, PDF_ARRAY, (pdf_obj **)&points);
        if (code < 0)
            goto exit;

        num_points = pdfi_array_size(points);
        if (num_points < 2)
            goto exit;

        code = pdfi_array_get_number(ctx, points, 0, &x1);
        if (code < 0) goto exit;
        code = pdfi_array_get_number(ctx, points, 1, &y1);
        if (code < 0) goto exit;
        code = gs_moveto(ctx->pgs, x1, y1);
        if (code < 0) goto exit;
        if (num_points == 2)
            goto stroke;

        for (j = 2; j < num_points; j += 2) {
            code = pdfi_array_get_number(ctx, points, j, &x2);
            if (code < 0) goto exit;
            code = pdfi_array_get_number(ctx, points, j+1, &y2);
            if (code < 0) goto exit;
            code = gs_lineto(ctx->pgs, x2, y2);
            if (code < 0) goto exit;
        }

    stroke:
        code = gs_stroke(ctx->pgs);
        if (code < 0) goto exit;
        pdfi_countdown(points);
        points = NULL;
    }

 exit:
    pdfi_countdown(points);
    return code;
}

/* Draw a path from a Path array (see 2.0 spec 12.5.6.13)
 * Path is array of arrays.
 * First array is 2 args for moveto
 * The rest of the arrays are either 2 args for lineto or 6 args for curveto
 */
static int pdfi_annot_draw_Path(pdf_context *ctx, pdf_dict *annot, pdf_array *Path)
{
    int code = 0;
    pdf_array *points = NULL;
    double array[6];
    int i;
    int num_points;

    for (i=0; i<pdfi_array_size(Path); i++) {
        code = pdfi_array_get_type(ctx, Path, i, PDF_ARRAY, (pdf_obj **)&points);
        if (code < 0)
            goto exit;

        num_points = pdfi_array_size(points);
        if (num_points != 2 && num_points != 6) {
            code = gs_note_error(gs_error_rangecheck);
            goto exit;
        }

        code = pdfi_array_to_num_array(ctx, points, array, 0, num_points);
        if (code < 0) goto exit;

        if (i == 0) {
            code = gs_moveto(ctx->pgs, array[0], array[1]);
            if (code < 0) goto exit;
        } else {
            if (num_points == 2) {
                code = gs_lineto(ctx->pgs, array[0], array[1]);
                if (code < 0) goto exit;
            } else {
                code = gs_curveto(ctx->pgs, array[0], array[1],
                                  array[2], array[3], array[4], array[5]);
                if (code < 0) goto exit;
            }
        }

        pdfi_countdown(points);
        points = NULL;
    }

    code = pdfi_annot_draw_border(ctx, annot, true);

 exit:
    pdfi_countdown(points);
    return code;
}

/* Generate appearance (see pdf_draw.ps/Ink
 *
 * If there was an AP it was already handled.
 */
static int pdfi_annot_draw_Ink(pdf_context *ctx, pdf_dict *annot, pdf_obj *NormAP, bool *render_done)
{
    int code = 0;
    int code1 = 0;
    bool drawit;
    pdf_array *array = NULL;

    code = pdfi_annot_setcolor(ctx, annot, false, &drawit);
    if (code <0 || !drawit)
        goto exit1;

    code = pdfi_annot_start_transparency(ctx, annot);
    if (code < 0) goto exit1;

    code = gs_setlinewidth(ctx->pgs, 1);
    if (code < 0) goto exit;
    code = gs_setlinecap(ctx->pgs, 1);
    if (code < 0) goto exit;
    code = gs_setlinejoin(ctx->pgs, 1);
    if (code < 0) goto exit;

    code = pdfi_dict_knownget_type(ctx, annot, "InkList", PDF_ARRAY, (pdf_obj **)&array);
    if (code < 0) goto exit;
    if (code > 0) {
        code = pdfi_annot_draw_InkList(ctx, annot, array);
        goto exit;
    }
    code = pdfi_dict_knownget_type(ctx, annot, "Path", PDF_ARRAY, (pdf_obj **)&array);
    if (code < 0) goto exit;
    if (code > 0) {
        code = pdfi_annot_draw_Path(ctx, annot, array);
        goto exit;
    }

 exit:
    code1 = pdfi_annot_end_transparency(ctx, annot);
    if (code >= 0)
        code = code1;
 exit1:
    pdfi_countdown(array);
    *render_done = true;
    return code;
}

/* from pdf_draw.ps/drawellipse
 * "Don Lancaster's code for drawing an ellipse"
 * https://www.tinaja.com/glib/ellipse4.pdf
 *
/magic 0.55228475 0.00045 sub store  % improved value
/drawellipse {
2 div /yrad exch store
2 div /xrad exch store
/xmag xrad magic mul store
/ymag yrad magic mul store xrad
neg 0 moveto
xrad neg ymag xmag neg yrad 0 yrad curveto
xmag  yrad xrad ymag xrad 0 curveto
xrad ymag neg  xmag yrad neg 0  yrad neg curveto
xmag neg yrad neg xrad neg ymag neg xrad neg  0 curveto
} bind def

 */
static int pdfi_annot_drawellipse(pdf_context *ctx, double width, double height)
{
    int code = 0;
    double magic = .55228475 - .00045; /* Magic value */
    double xrad = width / 2;
    double yrad = height / 2;
    double xmag = xrad * magic;
    double ymag = yrad * magic;

    code = gs_moveto(ctx->pgs, -xrad, 0);
    if (code < 0) return code;
    code = gs_curveto(ctx->pgs, -xrad, ymag, -xmag, yrad, 0, yrad);
    if (code < 0) return code;
    code = gs_curveto(ctx->pgs, xmag, yrad, xrad, ymag, xrad, 0);
    if (code < 0) return code;
    code = gs_curveto(ctx->pgs, xrad, -ymag, xmag, -yrad, 0, -yrad);
    if (code < 0) return code;
    code = gs_curveto(ctx->pgs, -xmag, -yrad, -xrad, -ymag, -xrad, 0);

    return code;
}

/* Generate appearance (see pdf_draw.ps/Circle)
 *
 * If there was an AP it was already handled.
 */
static int pdfi_annot_draw_Circle(pdf_context *ctx, pdf_dict *annot, pdf_obj *NormAP, bool *render_done)
{
    int code = 0;
    int code1 = 0;
    gs_rect rect;
    bool drawit;
    double width, height;

    code = pdfi_annot_start_transparency(ctx, annot);
    if (code < 0) goto exit1;

    code = pdfi_annot_Rect(ctx, annot, &rect);
    if (code < 0) goto exit;

    code = pdfi_annot_applyRD(ctx, annot, &rect);
    if (code < 0) goto exit;

    /* Translate to the center of the ellipse */
    width = rect.q.x - rect.p.x;
    height = rect.q.y - rect.p.y;
    code = gs_translate(ctx->pgs, rect.p.x + width/2, rect.p.y + height/2);
    if (code < 0) goto exit;

    /* Draw the ellipse */
    code = pdfi_annot_drawellipse(ctx, width, height);
    if (code < 0) goto exit;

    code = pdfi_annot_fillborderpath(ctx, annot);
    if (code < 0) goto exit;

    code = pdfi_annot_setcolor(ctx, annot, false, &drawit);
    if (code < 0) goto exit;

    if (drawit) {
        code = pdfi_annot_draw_border(ctx, annot, true);
        if (code < 0) goto exit;
    }

 exit:
    code1 = pdfi_annot_end_transparency(ctx, annot);
    if (code >= 0)
        code = code1;
 exit1:
    *render_done = true;
    return code;
}


/*********** BEGIN /Stamp ***************/
typedef struct {
    double r,g,b;
} pdfi_annot_stamp_rgb_t;

#define STAMP_RED {0xef/255.,0x40/255.,0x23/255.}
#define STAMP_GREEN {0x2f/255.,0xae/255.,0x49/255.}
#define STAMP_BLUE {0x00/255.,0x72/255.,0xbc/255.}

typedef struct {
    const char *text;
    double y;
    double h;
} pdfi_annot_stamp_text_t;

typedef struct {
    const char *type;
    pdfi_annot_stamp_rgb_t rgb;
    pdfi_annot_stamp_text_t text[2];
} pdfi_annot_stamp_type_t;

#define STAMP_1LINE(text,y,h) {{text,y,h},{0}}
#define STAMP_2LINE(text,y,h,text2,y2,h2) {{text,y,h},{text2,y2,h2}}

static pdfi_annot_stamp_type_t pdfi_annot_stamp_types[] = {
    {"Approved", STAMP_GREEN, STAMP_1LINE("APPROVED", 13, 30)},
    {"AsIs", STAMP_RED, STAMP_1LINE("AS IS", 13, 30)},
    {"Confidential", STAMP_RED, STAMP_1LINE("CONFIDENTIAL", 17, 20)},
    {"Departmental", STAMP_BLUE, STAMP_1LINE("DEPARTMENTAL", 17, 20)},
    {"Draft", STAMP_RED, STAMP_1LINE("DRAFT", 13, 30)},
    {"Experimental", STAMP_BLUE, STAMP_1LINE("EXPERIMENTAL", 17, 20)},
    {"Expired", STAMP_RED, STAMP_1LINE("EXPIRED", 13, 30)},
    {"Final", STAMP_RED, STAMP_1LINE("FINAL", 13, 30)},
    {"ForComment", STAMP_GREEN, STAMP_1LINE("FOR COMMENT", 17, 20)},
    {"ForPublicRelease", STAMP_GREEN, STAMP_2LINE("FOR PUBLIC",26,18,"RELEASE",8.5,18)},
    {"NotApproved", STAMP_RED, STAMP_1LINE("NOT APPROVED", 17, 20)},
    {"NotForPublicRelease", STAMP_RED, STAMP_2LINE("NOT FOR",26,18,"PUBLIC RELEASE",8.5,18)},
    {"Sold", STAMP_BLUE, STAMP_1LINE("SOLD", 13, 30)},
    {"TopSecret", STAMP_RED, STAMP_1LINE("TOP SECRET", 14, 26)},
    {0}
};
/* Use "Draft" if not found, make sure this value is correct if above array changes */
#define STAMP_DRAFT_INDEX 4

/* Draw the frame for the stamp.  This is taken straight form the PS code. */
static int pdfi_annot_draw_stamp_frame(pdf_context *ctx)
{
    int code;

    code = gs_moveto(ctx->pgs, 6.0, 0.0);
    if (code < 0) goto exit;
    code = gs_arcto(ctx->pgs, 190, 0, 190, 6, 6, 0);
    if (code < 0) goto exit;
    code = gs_arcto(ctx->pgs, 190, 47, 184, 47, 6, 0);
    if (code < 0) goto exit;
    code = gs_arcto(ctx->pgs, 0, 47, 0, 41, 6, 0);
    if (code < 0) goto exit;
    code = gs_arcto(ctx->pgs, 0, 0, 6, 0, 6, 0);
    if (code < 0) goto exit;
    code = gs_closepath(ctx->pgs);
    if (code < 0) goto exit;

    code = gs_moveto(ctx->pgs, 10, 4);
    if (code < 0) goto exit;
    code = gs_arcto(ctx->pgs, 185, 4, 185, 9, 5, 0);
    if (code < 0) goto exit;
    code = gs_arcto(ctx->pgs, 185, 43, 180, 43, 5, 0);
    if (code < 0) goto exit;
    code = gs_arcto(ctx->pgs, 5, 43, 5, 38, 5, 0);
    if (code < 0) goto exit;
    code = gs_arcto(ctx->pgs, 5, 4, 9, 4, 5, 0);
    if (code < 0) goto exit;
    code = gs_closepath(ctx->pgs);
    if (code < 0) goto exit;
    code = gs_eofill(ctx->pgs);
    if (code < 0) goto exit;

 exit:
    return code;
}

/* draw text for stamp */
static int pdfi_annot_draw_stamp_text(pdf_context *ctx, pdf_dict *annot, gs_rect *rect,
                                      pdfi_annot_stamp_text_t *text)
{
    int code = 0;
    int code1 = 0;
    pdf_string *string = NULL;
    gs_rect bbox;
    gs_point awidth;
    double x,y;

    if (!text->text)
        return 0;

    code = pdfi_BT(ctx);
    if (code < 0)
        return code;

    code = pdfi_annot_set_font(ctx, "Times-Bold", text->h);
    if (code < 0) goto exit;

    code = pdfi_obj_charstr_to_string(ctx, text->text, &string);
    if (code < 0) goto exit;


    /* At this point the graphics state is slightly rotated and current position
     * is at the lower left corner of the stamp's box.
     * The math here matches what the gs code does, so it's kind of confusing.
     */
    /* Get width of the string */
    code = pdfi_string_bbox(ctx, string, &bbox, &awidth, false);
    if (code < 0) goto exit;

    /* Calculate offset from corner (95 is hard-coded value based on size of the stamp I think) */
    x = 95 - (bbox.q.x-bbox.p.x)/2; /* hard-coded value taken from gs code */
    y = text->y;

    code = pdfi_gsave(ctx);
    code = pdfi_gs_setgray(ctx, .75);
    code = pdfi_annot_display_simple_text(ctx, annot, x+1, y-1, string);
    if (code < 0) goto exit;
    code = pdfi_grestore(ctx);

    code = pdfi_annot_display_simple_text(ctx, annot, x, y, string);
    if (code < 0) goto exit;

 exit:
    code1 = pdfi_ET(ctx);
    if (code == 0) code = code1;
    pdfi_countdown(string);
    return 0;
}

/* Generate appearance (see pdf_draw.ps/Stamp)
 *
 * If there was an AP it was already handled.
 * For testing, see tests_private/pdf/PDF_1.7_FTS/fts_32_3204.pdf
 */
static int pdfi_annot_draw_Stamp(pdf_context *ctx, pdf_dict *annot, pdf_obj *NormAP, bool *render_done)
{
    int code = 0;
    int code1 = 0;
    gs_rect rect;
    double width, height;
    pdfi_annot_stamp_type_t *stamp_type;
    pdf_name *Name = NULL;
    double xscale, yscale;
    double angle;
    int i;

    code = pdfi_annot_start_transparency(ctx, annot);
    if (code < 0) goto exit1;

    code = pdfi_annot_Rect(ctx, annot, &rect);
    if (code < 0) goto exit;

    code = pdfi_dict_knownget_type(ctx, annot, "Name", PDF_NAME, (pdf_obj **)&Name);
    if (code <= 0) goto exit;

    /* Translate to the center of Rect */
    width = rect.q.x - rect.p.x;
    height = rect.q.y - rect.p.y;
    code = gs_translate(ctx->pgs, rect.p.x + width/2, rect.p.y + height/2);
    if (code < 0) goto exit;

    /* This math comes from the PS code.  I assume the values are related to
       the h and y values in the various Stamp type data.
    */
    /* Scale by the minimum of the two ratios, but make sure it's not 0 */
    yscale = height / 50.;
    xscale = width / 190.;
    if (xscale < yscale)
        yscale = xscale;
    if (yscale <= 0.0)
        yscale = 1.0;
    code = gs_scale(ctx->pgs, yscale, yscale);
    if (code < 0) goto exit;

    /* Search through types and find a match, if no match use Draft */
    for (stamp_type = pdfi_annot_stamp_types; stamp_type->type; stamp_type ++) {
        if (Name && pdfi_name_is(Name, stamp_type->type))
            break;
    }

    /* TODO: There are a bunch of Stamp Names built into AR that are not part of
     * the spec.  Since we don't know about them, we will just display nothing.
     * Example: the "Faces" stamps, such as FacesZippy
     * See file tests_private/pdf/uploads/annots-noAP.pdf
     */
    if (!stamp_type->type) {
        pdfi_set_warning(ctx, 0, NULL, W_PDF_ANNOT_ERROR, "pdfi_annot_draw_Stamp", "WARNING: Annotation: No AP, unknown Stamp Name, omitting");
        code = 0;
        goto exit;
    }

    /* Draw the frame */
    code = pdfi_gs_setrgbcolor(ctx, stamp_type->rgb.r, stamp_type->rgb.g,
                               stamp_type->rgb.b);
    if (code < 0) goto exit;

    code = gs_translate(ctx->pgs, -95, -25);
    if (code < 0) goto exit;
    code = gs_atan2_degrees(2.0, 190.0, &angle);
    if (code < 0) goto exit;
    code = gs_rotate(ctx->pgs, angle);
    if (code < 0) goto exit;

    /* Draw frame in gray */
    code = pdfi_gsave(ctx);
    code = gs_translate(ctx->pgs, 1, -1);
    code = pdfi_gs_setgray(ctx, 0.75);
    code = pdfi_annot_draw_stamp_frame(ctx);
    code = pdfi_grestore(ctx);

    /* Draw frame colored */
    code = pdfi_annot_draw_stamp_frame(ctx);

    /* Draw the text */
    for (i=0; i<2; i++) {
        code = pdfi_annot_draw_stamp_text(ctx, annot, &rect, &stamp_type->text[i]);
        if (code < 0) goto exit;
    }

 exit:
    code1 = pdfi_annot_end_transparency(ctx, annot);
    if (code >= 0)
        code = code1;
 exit1:
    pdfi_countdown(Name);
    *render_done = true;
    return code;
}
/*********** END /Stamp ***************/


/* Draw Callout Line (CL) */
static int pdfi_annot_draw_CL(pdf_context *ctx, pdf_dict *annot)
{
    int code;
    pdf_array *CL = NULL;
    double array[6];
    int length;

    code = pdfi_dict_knownget_type(ctx, annot, "CL", PDF_ARRAY, (pdf_obj **)&CL);
    if (code <= 0) goto exit;

    length = pdfi_array_size(CL);

    if (length != 4 && length != 6) {
        code = gs_note_error(gs_error_syntaxerror);
        goto exit;
    }

    code = pdfi_array_to_num_array(ctx, CL, array, 0, length);
    if (code < 0) goto exit;

    /* Draw the line */
    code = gs_moveto(ctx->pgs, array[0], array[1]);
    if (code < 0) goto exit;
    code = gs_lineto(ctx->pgs, array[2], array[3]);
    if (code < 0) goto exit;
    if (length == 6) {
        code = gs_lineto(ctx->pgs, array[4], array[5]);
        if (code < 0) goto exit;
    }
    code = gs_stroke(ctx->pgs);

    /* Line ending
     * NOTE: This renders a different arrow than the gs code, but I used the
     * same LE code I used elsewhere, which is clearly better.  So there will be diffs...
     */
    code = pdfi_annot_draw_LE(ctx, annot, array[0], array[1], array[2], array[3], 1);
    if (code < 0) goto exit;

 exit:
    pdfi_countdown(CL);
    return code;
}

/* FreeText -- Draw text with border around it.
 *
 * See pdf_draw.ps/FreeText
 * If there was an AP it was already handled
 */
static int pdfi_annot_draw_FreeText(pdf_context *ctx, pdf_dict *annot, pdf_obj *NormAP, bool *render_done)
{
    int code = 0;
    int code1 = 0;
    bool drawbackground;
    pdf_dict *BS = NULL;
    pdf_string *Contents = NULL;
    gs_rect annotrect, modrect;
    int64_t Rotate;

    *render_done = false;

    code = pdfi_annot_start_transparency(ctx, annot);
    if (code < 0) goto exit1;

    code = pdfi_annot_opacity(ctx, annot);
    if (code < 0) goto exit;

    code = pdfi_annot_Rect(ctx, annot, &annotrect);
    if (code < 0) goto exit;

    /* Set the (background?) color if applicable */
    code = pdfi_annot_setcolor(ctx, annot, false, &drawbackground);
    if (code < 0) goto exit;

    /* Only draw rectangle if a color was specified */
    if (drawbackground) {
        code = pdfi_annot_fillRect(ctx, annot);
        if (code < 0) goto exit;
    }

    /* Set DA (Default Appearance)
     * This will generally set a color and font.
     *
     * TODO: Unclear if this is supposed to also determine the color
     * of the border box, but it seems like it does in gs, so we set the DA
     * before drawing the border, like gs does.
     */
    code = pdfi_annot_process_DA(ctx, NULL, annot, &annotrect, false);
    if (code < 0) goto exit;

    /* Draw border around text */
    /* TODO: gs-compatible implementation is commented out.  Would rather just delete it... */
#if 1
    /* TODO: Prefer to do it this way! */
    code = pdfi_annot_draw_border(ctx, annot, false);
    if (code < 0) goto exit;
#else
    /* NOTE: I would really like to just call pdfi_annot_draw_border()
     * without checking for BS, but the implementation in gs is subtly different (see below)
     * and I want to make it possible to bmpcmp.
     */
    code = pdfi_dict_knownget_type(ctx, annot, "BS", PDF_DICT, (pdf_obj **)&BS);
    if (code < 0) goto exit;
    if (BS) {
        code = pdfi_annot_draw_border(ctx, annot, false);
        if (code < 0) goto exit;
    } else {
        gs_rect rect;

        /* Draw our own border */
        code = pdfi_gs_setgray(ctx, 0);
        if (code < 0) goto exit;

        /* NOTE: This is almost identical to pdfi_annot_strokeborder() with a width=1.0
         * except it adjusts the rectangle by 1.0 instead of 0.5.
         * Should probably just call that function, but I want to match gs implementation
         * exactly for bmpcmp purposes.
         */
        code = gs_setlinewidth(ctx->pgs, 1);
        if (code < 0) goto exit;

        code = pdfi_annot_Rect(ctx, annot, &rect);
        if (code < 0) goto exit;

        pdfi_annot_rect_adjust(ctx, &rect, 1);
        code = gs_rectstroke(ctx->pgs, &rect, 1, NULL);
        if (code < 0) goto exit;
    }
#endif

    code = pdfi_annot_draw_CL(ctx, annot);
    if (code < 0) goto exit;

    /* Modify rect by RD if applicable */
    code = pdfi_annot_applyRD(ctx, annot, &annotrect);
    if (code < 0) goto exit;

    /* /Rotate */
    code = pdfi_dict_get_int_def(ctx, annot, "Rotate", &Rotate, 0);
    if (code < 0) goto exit;

    modrect = annotrect; /* structure copy */
    /* TODO: I think this can all be done in a generic way using gs_transform() but it
     * makes my head explode...
     */
    switch(Rotate) {
    case 270:
        code = gs_moveto(ctx->pgs, modrect.q.y, modrect.q.x);
        if (code < 0) goto exit;
        code = gs_rotate(ctx->pgs, 270.0);
        modrect.p.x = -annotrect.q.y;
        modrect.q.x = -annotrect.p.y;
        modrect.p.y = annotrect.p.x;
        modrect.q.y = annotrect.q.x;
        break;
    case 180:
        code = gs_moveto(ctx->pgs, modrect.q.x, modrect.p.y);
        if (code < 0) goto exit;
        code = gs_rotate(ctx->pgs, 180.0);
        modrect.p.x = -annotrect.q.x;
        modrect.q.x = -annotrect.p.x;
        modrect.p.y = -annotrect.q.y;
        modrect.q.y = -annotrect.p.y;
        break;
        break;
    case 90:
        code = gs_moveto(ctx->pgs, modrect.p.y, modrect.p.x);
        if (code < 0) goto exit;
        code = gs_rotate(ctx->pgs, 90.0);
        modrect.p.x = annotrect.p.y;
        modrect.q.x = annotrect.q.y;
        modrect.p.y = -annotrect.q.x;
        modrect.q.y = -annotrect.p.x;
        break;
    case 0:
    default:
        modrect.p.x += 2;
        modrect.p.y += 2;
        modrect.q.x -= 2;
        modrect.q.y -= 2;

        /* Move to initial position (slightly offset from the corner of the rect) */
        code = gs_moveto(ctx->pgs, modrect.p.x, modrect.q.y);
        if (code < 0) goto exit;
        break;
    }

    /* /Contents */
    code = pdfi_dict_knownget_type(ctx, annot, "Contents", PDF_STRING, (pdf_obj **)&Contents);
    if (code < 0) goto exit;
    if (code > 0) {
        code = pdfi_annot_display_formatted_text(ctx, annot, &modrect, Contents, false);
        if (code < 0) goto exit;
    }

 exit:
    code1 = pdfi_annot_end_transparency(ctx, annot);
    if (code >= 0)
        code = code1;
 exit1:
    *render_done = true;
    pdfi_countdown(BS);
    pdfi_countdown(Contents);
    return code;
}

/* Generate appearance (see pdf_draw.ps/Text)
 * This draws the default "Note" thingy which looks like a little page.
 * TODO: Spec lists a whole bunch of different /Name with different appearances that we don't handle.
 * See page 1 of tests_private/pdf/sumatra/annotations_without_appearance_streams.pdf
 *
 * If there was an AP it was already handled.
 */
static int pdfi_annot_draw_Text(pdf_context *ctx, pdf_dict *annot, pdf_obj *NormAP, bool *render_done)
{
    int code = 0;
    int code1 = 0;
    gs_rect rect;

    code = pdfi_annot_start_transparency(ctx, annot);
    if (code < 0) goto exit1;

    code = pdfi_annot_Rect(ctx, annot, &rect);
    if (code < 0) goto exit;
    code = gs_translate(ctx->pgs, rect.p.x, rect.p.y);
    if (code < 0) goto exit;

    /* Draw a page icon (/Note) */
    /* TODO: All the other /Name appearances */
    code = gs_translate(ctx->pgs, 0.5, (rect.q.y-rect.p.y)-18.5);
    if (code < 0) goto exit;
    code = gs_setlinewidth(ctx->pgs, 1.0);
    if (code < 0) goto exit;
    code = pdfi_gs_setgray(ctx, 0.75);
    if (code < 0) goto exit;
    code = gs_moveto(ctx->pgs, 0.5, -1);
    if (code < 0) goto exit;
    code = gs_lineto(ctx->pgs, 10, -1);
    if (code < 0) goto exit;
    code = gs_lineto(ctx->pgs, 15, 4);
    if (code < 0) goto exit;
    code = gs_lineto(ctx->pgs, 15, 17.5);
    if (code < 0) goto exit;
    code = gs_stroke(ctx->pgs);
    if (code < 0) goto exit;

    code = gs_moveto(ctx->pgs, 0, 0);
    if (code < 0) goto exit;
    code = gs_lineto(ctx->pgs, 9, 0);
    if (code < 0) goto exit;
    code = gs_lineto(ctx->pgs, 14, 5);
    if (code < 0) goto exit;
    code = gs_lineto(ctx->pgs, 14, 18);
    if (code < 0) goto exit;
    code = gs_lineto(ctx->pgs, 0, 18);
    if (code < 0) goto exit;
    code = gs_closepath(ctx->pgs);
    if (code < 0) goto exit;

    code = pdfi_gsave(ctx);
    if (code >= 0) {
        code = pdfi_gs_setgray(ctx, 0.5);
        code = gs_fill(ctx->pgs);
        code = pdfi_grestore(ctx);
    } else
        goto exit;

    code = pdfi_gs_setgray(ctx, 0);
    if (code < 0) goto exit;
    code = gs_stroke(ctx->pgs);
    if (code < 0) goto exit;

    code = gs_moveto(ctx->pgs, 3, 8);
    if (code < 0) goto exit;
    code = gs_lineto(ctx->pgs, 7.5, 8);
    if (code < 0) goto exit;
    code = gs_moveto(ctx->pgs, 3, 11);
    if (code < 0) goto exit;
    code = gs_lineto(ctx->pgs, 10, 11);
    if (code < 0) goto exit;
    code = gs_moveto(ctx->pgs, 3, 14);
    if (code < 0) goto exit;
    code = gs_lineto(ctx->pgs, 10, 14);
    if (code < 0) goto exit;
    code = gs_moveto(ctx->pgs, 9, 0);
    if (code < 0) goto exit;
    code = gs_lineto(ctx->pgs, 9, 5);
    if (code < 0) goto exit;
    code = gs_lineto(ctx->pgs, 14, 5);
    if (code < 0) goto exit;
    code = gs_stroke(ctx->pgs);
    if (code < 0) goto exit;

 exit:
    code1 = pdfi_annot_end_transparency(ctx, annot);
    if (code >= 0)
        code = code1;
 exit1:
    *render_done = true;
    return code;
}

/* Converts array of 4 points to a basis and 2 dimensions (6 values altogether)
 *    x0 y0 x1 y1 x2 y2 x3 y3 -> x0 y0 x1-x0 y1-y0 x2-x0 y2-y0
 *
 *  ^
 *  |
 *  * (x2,y2)    * (x3,y3)
 *  |
 *  |
 *  *------------*->
 *  (x0,y0)      (x1,y1)
 */
static void pdfi_annot_quadpoints2basis(pdf_context *ctx, double *array,
                                        double *px0, double *py0,
                                        double *dx1, double *dy1,
                                        double *dx2, double *dy2)
{
    double x0,x1,x2,x3,y0,y1,y2,y3;
    double minx, miny;
    int i, mindex;

    /* comment from the PS code
    % The text is oriented with respect to the vertex with the smallest
    % y value (or the leftmost of those, if there are two such vertices)
    % (x0, y0) and the next vertex in a counterclockwise direction
    % (x1, y1), regardless of whether these are the first two points in
    % the QuadPoints array.
    */

    /* starting min point */
    minx = array[0];
    miny = array[1];
    mindex = 0;
    /* This finds the index of the point with miny and if there were two points with same
     * miny, it finds the one with minx
     */
    for (i=2; i<8; i+=2) {
        if ((array[i+1] == miny && array[i] < minx) ||  (array[i+1] < miny)) {
            /* Set new min point */
            miny = array[i+1];
            minx = array[i];
            mindex = i;
        }
    }

    /* Pull out the points such that x0 = minx, y0 = miny, etc */
    i = mindex;
    x0 = array[i];
    y0 = array[i+1];
    i += 2;
    if (i == 8) i = 0;
    x1 = array[i];
    y1 = array[i+1];
    i += 2;
    if (i == 8) i = 0;
    x2 = array[i];
    y2 = array[i+1];
    i += 2;
    if (i == 8) i = 0;
    x3 = array[i];
    y3 = array[i+1];

    /* Make it go counterclockwise by picking lower of these 2 points */
    if (y3 < y1) {
        y1 = y3;
        x1 = x3;
    }

    /* Convert into a starting point and two sets of lengths */
    *px0 = x0;
    *py0 = y0;
    *dx1 = x1 - x0;
    *dy1 = y1 - y0;
    *dx2 = x2 - x0;
    *dy2 = y2 - y0;
}

/* Generate appearance for Underline or StrikeOut (see pdf_draw.ps/Underline and Strikeout)
 *
 * offset -- how far above the QuadPoints box bottom to draw the line
 *
 */
static int pdfi_annot_draw_line_offset(pdf_context *ctx, pdf_dict *annot, double offset)
{
    int code = 0;
    bool drawit;
    pdf_array *QuadPoints = NULL;
    double array[8];
    int size;
    int num_quads;
    int i;
    double x0, y0, dx1, dy1, dx2, dy2;
    double linewidth;

    code = pdfi_annot_setcolor(ctx, annot, false, &drawit);
    if (code <0 || !drawit)
        goto exit;

    code = pdfi_dict_knownget_type(ctx, annot, "QuadPoints", PDF_ARRAY, (pdf_obj **)&QuadPoints);
    if (code <= 0) goto exit;

    size = pdfi_array_size(QuadPoints);
    num_quads = size / 8;

    for (i=0; i<num_quads; i++) {
        code = pdfi_array_to_num_array(ctx, QuadPoints, array, i*8, 8);
        if (code < 0) goto exit;

        pdfi_annot_quadpoints2basis(ctx, array, &x0, &y0, &dx1, &dy1, &dx2, &dy2);

        /* Acrobat draws the line at offset-ratio of the box width from the bottom of the box
         * and 1/16 thick of the box width.  /Rect is ignored.
         */
        linewidth = sqrt((dx2*dx2) + (dy2*dy2)) / 16.;
        code = gs_setlinewidth(ctx->pgs, linewidth);
        if (code < 0) goto exit;
        code = gs_moveto(ctx->pgs, dx2*offset + x0 , dy2*offset + y0);
        if (code < 0) goto exit;
        code = gs_lineto(ctx->pgs, dx2*offset + x0 + dx1, dy2*offset + y0 + dy1);
        if (code < 0) goto exit;
        code = gs_stroke(ctx->pgs);
        if (code < 0) goto exit;
    }

 exit:
    pdfi_countdown(QuadPoints);
    return code;
}

/* Generate appearance (see pdf_draw.ps/StrikeOut)
 *
 * If there was an AP it was already handled.
 */
static int pdfi_annot_draw_StrikeOut(pdf_context *ctx, pdf_dict *annot, pdf_obj *NormAP, bool *render_done)
{
    *render_done = true;
    return pdfi_annot_draw_line_offset(ctx, annot, 3/7.);
}

/* Generate appearance (see pdf_draw.ps/Underline)
 *
 * If there was an AP it was already handled.
 */
static int pdfi_annot_draw_Underline(pdf_context *ctx, pdf_dict *annot, pdf_obj *NormAP, bool *render_done)
{
    *render_done = true;
    return pdfi_annot_draw_line_offset(ctx, annot, 1/7.);
}

/* Connect 2 points with an arc that has max distance from the line segment to the arc
 * equal to 1/4 of the radius.
 */
static int pdfi_annot_highlight_arc(pdf_context *ctx, double x0, double y0,
                                    double x1, double y1)
{
    int code = 0;
    double dx, dy;
    double xc, yc, r, a1, a2;

    dx = x1 - x0;
    dy = y1 - y0;

    yc = (y1+y0)/2. - 15./16.*dx;
    xc = (x1+x0)/2. + 15./16.*dy;
    r = sqrt((y1-yc)*(y1-yc) + (x1-xc)*(x1-xc));
    code = gs_atan2_degrees(y1-yc, x1-xc, &a1);
    if (code < 0)
        a1 = 0;
    code = gs_atan2_degrees(y0-yc, x0-xc, &a2);
    if (code < 0)
        a2 = 0;
    code = gs_arcn(ctx->pgs, xc, yc, r, a2, a1);

    return code;
}

/* Generate appearance (see pdf_draw.ps/Highlight)
 *
 * If there was an AP it was already handled.
 */
static int pdfi_annot_draw_Highlight(pdf_context *ctx, pdf_dict *annot, pdf_obj *NormAP, bool *render_done)
{
    int code = 0;
    bool drawit;
    pdf_array *QuadPoints = NULL;
    double array[8];
    int size;
    int num_quads;
    int i;

    code = pdfi_annot_setcolor(ctx, annot, false, &drawit);
    if (code <0 || !drawit)
        goto exit;

    code = gs_setlinecap(ctx->pgs, 1);
    if (code < 0) goto exit;

    code = pdfi_dict_knownget_type(ctx, annot, "QuadPoints", PDF_ARRAY, (pdf_obj **)&QuadPoints);
    if (code <= 0) goto exit;

    size = pdfi_array_size(QuadPoints);
    num_quads = size / 8;

    for (i=0; i<num_quads; i++) {
        code = pdfi_array_to_num_array(ctx, QuadPoints, array, i*8, 8);
        if (code < 0) goto exit;

        code = gs_moveto(ctx->pgs, array[2], array[3]);
        if (code < 0) goto exit;

        code = pdfi_annot_highlight_arc(ctx, array[2], array[3], array[6], array[7]);
        if (code < 0) goto exit;

        code = gs_lineto(ctx->pgs, array[4], array[5]);
        if (code < 0) goto exit;

        code = pdfi_annot_highlight_arc(ctx, array[4], array[5], array[0], array[1]);
        if (code < 0) goto exit;

        code = gs_closepath(ctx->pgs);
        if (code < 0) goto exit;

        if (ctx->page.has_transparency) {
            code = pdfi_trans_begin_simple_group(ctx, NULL, false, false, false);
            if (code < 0) goto exit;

            code = gs_setblendmode(ctx->pgs, BLEND_MODE_Multiply);
            if (code < 0) {
                (void)pdfi_trans_end_simple_group(ctx);
                goto exit;
            }
            code = gs_fill(ctx->pgs);
            (void)pdfi_trans_end_simple_group(ctx);
            if (code < 0) goto exit;
        } else {
            code = gs_stroke(ctx->pgs);
            if (code < 0) goto exit;

            code = gs_newpath(ctx->pgs);
            if (code < 0) goto exit;
        }
    }

 exit:
    pdfi_countdown(QuadPoints);
    return code;
}

/* Generate appearance (see pdf_draw.ps/Squiggly)
 *
 * If there was an AP it was already handled.
 */
static int pdfi_annot_draw_Squiggly(pdf_context *ctx, pdf_dict *annot, pdf_obj *NormAP, bool *render_done)
{
    int code = 0;
    bool drawit;
    pdf_array *QuadPoints = NULL;
    double array[8];
    int size;
    int num_quads;
    int i;
    double x0, y0, dx1, dy1, dx2, dy2;
    double p1, p2, p12;
    int count;
    bool need_grestore = false;
    gs_matrix matrix;

    code = pdfi_annot_setcolor(ctx, annot, false, &drawit);
    if (code <0 || !drawit)
        goto exit;

    code = gs_setlinecap(ctx->pgs, gs_cap_round);
    if (code < 0) goto exit;
    code = gs_setlinejoin(ctx->pgs, gs_join_round);
    if (code < 0) goto exit;
    code = gs_setlinewidth(ctx->pgs, 1.0);
    if (code < 0) goto exit;

    code = pdfi_dict_knownget_type(ctx, annot, "QuadPoints", PDF_ARRAY, (pdf_obj **)&QuadPoints);
    if (code <= 0) goto exit;

    size = pdfi_array_size(QuadPoints);
    num_quads = size / 8;

    for (i=0; i<num_quads; i++) {
        code = pdfi_array_to_num_array(ctx, QuadPoints, array, i*8, 8);
        if (code < 0) goto exit;

        code = pdfi_gsave(ctx);
        if (code < 0) goto exit;
        need_grestore = true;

        /* Make clipping region */
        code = gs_moveto(ctx->pgs, array[6], array[7]);
        if (code < 0) goto exit;
        code = gs_lineto(ctx->pgs, array[4], array[5]);
        if (code < 0) goto exit;
        code = gs_lineto(ctx->pgs, array[0], array[1]);
        if (code < 0) goto exit;
        code = gs_lineto(ctx->pgs, array[2], array[3]);
        if (code < 0) goto exit;
        code = gs_closepath(ctx->pgs);
        if (code < 0) goto exit;
        code = gs_clip(ctx->pgs);
        if (code < 0) goto exit;
        code = gs_newpath(ctx->pgs);
        if (code < 0) goto exit;

        pdfi_annot_quadpoints2basis(ctx, array, &x0, &y0, &dx1, &dy1, &dx2, &dy2);

        code = gs_translate(ctx->pgs, x0+dx2/56+dx2/72, y0+dy2/56+dy2/72);
        if (code < 0) goto exit;

        p2 = sqrt(dx2*dx2 + dy2*dy2);
        p1 = sqrt(dx1*dx1 + dy1*dy1);

        if (p2 > 0 && p1 > 0) {
            p12 = p2 / p1;

            count = (int)((1/p12) * 4 + 1);

            matrix.xx = dx1 * p12;
            matrix.xy = dy1 * p12;
            matrix.yx = dx2;
            matrix.yy = dy2;
            matrix.tx = 0.0;
            matrix.ty = 0.0;
            code = gs_concat(ctx->pgs, &matrix);
            if (code < 0) goto exit;

            code = gs_scale(ctx->pgs, 1/40., 1/72.);
            if (code < 0) goto exit;

            code = gs_moveto(ctx->pgs, 0, 0);
            if (code < 0) goto exit;

            while (count >= 0) {
                code = gs_lineto(ctx->pgs, 5, 10);
                if (code < 0) goto exit;
                code = gs_lineto(ctx->pgs, 10, 0);
                if (code < 0) goto exit;
                code = gs_translate(ctx->pgs, 10, 0);
                if (code < 0) goto exit;

                count --;
            }
            code = gs_stroke(ctx->pgs);
            if (code < 0) goto exit;
        }

        need_grestore = false;
        code = pdfi_grestore(ctx);
        if (code < 0) goto exit;
    }

 exit:
    if (need_grestore)
        (void)pdfi_grestore(ctx);
    pdfi_countdown(QuadPoints);
    *render_done = true;
    return code;
}

/* Generate appearance (see pdf_draw.ps/Redact)
 *
 * If there was an AP it was already handled.
 */
static int pdfi_annot_draw_Redact(pdf_context *ctx, pdf_dict *annot, pdf_obj *NormAP, bool *render_done)
{
    /* comment from PS code:
    %% Redact annotations are part of a process, a Redact annotation is only present
    %% until the content is removed, before that the content should be present and
    %% I believe we should print it. So take no action for Redact annotations if they
    %% have no appearance.
    */
    /* Render nothing, don't print warning */
    *render_done = true;
    return 0;
}

/* Handle PopUp (see pdf_draw.ps/Popup)
 * Renders only if /Open=true
 *
 * If there's an AP, caller will handle it on return
 */
static int pdfi_annot_draw_Popup(pdf_context *ctx, pdf_dict *annot, pdf_obj *NormAP, bool *render_done)
{
    int code = 0, code1 = 0;
    bool Open = false;
    pdf_dict *Parent = NULL;
    pdf_string *Contents = NULL;
    pdf_string *T = NULL;
    bool has_color;
    gs_rect rect, rect2;
    bool need_grestore = false;

    /* Render only if open */
    code = pdfi_dict_get_bool(ctx, annot, "Open", &Open);
    if (code < 0 && (code != gs_error_undefined))
        goto exit1;

    code = 0;

    /* Don't render if not Open */
    if (!Open) {
        *render_done = true;
        goto exit1;
    }

    /* Let caller render if there is an AP */
    if (NormAP) {
        *render_done = false;
        goto exit1;
    }

    /* regardless what happens next, tell caller we rendered it */
    *render_done = true;

    code = pdfi_annot_start_transparency(ctx, annot);
    if (code < 0) goto exit1;

    code = gs_setlinewidth(ctx->pgs, 0.05);

    /* Use Parent to get color */
    code = pdfi_dict_knownget_type(ctx, annot, "Parent", PDF_DICT, (pdf_obj **)&Parent);
    if (code < 0) goto exit;
    if (code == 0) {
        code = pdfi_dict_knownget_type(ctx, annot, "P", PDF_DICT, (pdf_obj **)&Parent);
        if (code < 0) goto exit;
    }
    if (code == 0) {
        /* If no parent, we will use the annotation itself */
        Parent = annot;
        pdfi_countup(Parent);
    }

    /* Set color if there is one specified */
    code = pdfi_annot_setcolor(ctx, Parent, false, &has_color);
    if (code < 0) goto exit;

    /* Set a default color if nothing specified */
    if (!has_color) {
        code = pdfi_gs_setrgbcolor(ctx, 1.0, 1.0, 0);
        if (code < 0) goto exit;
    }

    code = pdfi_annot_Rect(ctx, annot, &rect);
    if (code < 0) goto exit;

    /* Draw big box (the frame around the /Contents) */
    code = pdfi_gsave(ctx);
    if (code < 0) goto exit;
    need_grestore = true;
    code = pdfi_gs_setgray(ctx, 1);
    if (code < 0) goto exit;
    code = gs_rectfill(ctx->pgs, &rect, 1);
    if (code < 0) goto exit;
    code = pdfi_gs_setgray(ctx, 0);
    if (code < 0) goto exit;
    code = gs_rectstroke(ctx->pgs, &rect, 1, NULL);
    if (code < 0) goto exit;
    code = pdfi_grestore(ctx);
    need_grestore = false;
    if (code < 0) goto exit;

    /* Display /Contents in Helvetica */
    code = pdfi_dict_knownget_type(ctx, Parent, "Contents", PDF_STRING, (pdf_obj **)&Contents);
    if (code < 0) goto exit;
    if (code > 0) {
        gs_rect text_rect;

        code = pdfi_gsave(ctx);
        if (code < 0) goto exit;
        need_grestore = true;
        code = pdfi_gs_setgray(ctx, 0);
        if (code < 0) goto exit;

        text_rect.p.x = rect.p.x + 3;
        text_rect.q.x = rect.q.x - 3;
        text_rect.p.y = rect.p.y + 3;
        text_rect.q.y = rect.q.y - 18;

        code = pdfi_annot_set_font(ctx, "Helvetica", 9);
        if (code < 0) goto exit;

        code = pdfi_annot_display_formatted_text(ctx, annot, &text_rect, Contents, false);
        if (code < 0) goto exit;

        code = pdfi_grestore(ctx);
        need_grestore = false;
        if (code < 0) goto exit;
    }

    /* Draw small, thin box (the frame around the top /T text) */
    rect2.p.x = rect.p.x;
    rect2.p.y = rect.q.y - 15;
    rect2.q.x = rect.q.x;
    rect2.q.y = rect.p.y + (rect.q.y - rect.p.y);

    code = gs_rectfill(ctx->pgs, &rect2, 1);
    if (code < 0) goto exit;
    code = pdfi_gs_setgray(ctx, 0);
    if (code < 0) goto exit;
    code = gs_rectstroke(ctx->pgs, &rect2, 1, NULL);
    if (code < 0) goto exit;

    /* Display /T in Helvetica */
    code = pdfi_dict_knownget_type(ctx, Parent, "T", PDF_STRING, (pdf_obj **)&T);
    if (code < 0) goto exit;
    if (code > 0) {
        code = pdfi_annot_set_font(ctx, "Helvetica", 9);
        if (code < 0) goto exit;

        code = pdfi_annot_display_centered_text(ctx, annot, &rect, T);
        if (code < 0) goto exit;

    }

 exit:
    if (need_grestore) {
        code1= pdfi_grestore(ctx);
        if (code == 0) code = code1;
    }
    code1 = pdfi_annot_end_transparency(ctx, annot);
    if (code == 0) code = code1;
 exit1:
    pdfi_countdown(Parent);
    pdfi_countdown(Contents);
    pdfi_countdown(T);
    return code;
}

/* Generate appearance (see pdf_draw.ps/Line)
 *
 * If there was an AP it was already handled.
 */
static int pdfi_annot_draw_Line(pdf_context *ctx, pdf_dict *annot, pdf_obj *NormAP, bool *render_done)
{
    int code = 0;
    int code1 = 0;
    gs_rect lrect;
    pdf_array *L = NULL;
    bool drawit;

    code = pdfi_annot_start_transparency(ctx, annot);
    if (code < 0) goto exit1;

    code = pdfi_dict_knownget_type(ctx, annot, "L", PDF_ARRAY, (pdf_obj **)&L);
    if (code < 0) goto exit;

    code = pdfi_array_to_gs_rect(ctx, L, &lrect);
    if (code < 0) goto exit;

    code = pdfi_annot_setcolor(ctx, annot, false, &drawit);
    if (code < 0) goto exit;

    /* Handle LE */
    code = pdfi_annot_draw_LE(ctx, annot, lrect.p.x, lrect.p.y, lrect.q.x, lrect.q.y, 0);
    if (code < 0) goto exit;

    /* Draw the actual line */
    code = gs_moveto(ctx->pgs, lrect.p.x, lrect.p.y);
    if (code < 0) goto exit;
    code = gs_lineto(ctx->pgs, lrect.q.x, lrect.q.y);
    if (code < 0) goto exit;
    code = pdfi_annot_draw_border(ctx, annot, true);
    if (code < 0) goto exit;

 exit:
    code1 = pdfi_annot_end_transparency(ctx, annot);
    if (code >= 0)
        code = code1;
 exit1:
    *render_done = true;
    pdfi_countdown(L);
    return code;
}

/* Create a path from an array of points */
static int pdfi_annot_path_array(pdf_context *ctx, pdf_dict *annot, pdf_array *Vertices)
{
    int code = 0;
    int i;

    for (i=0; i<pdfi_array_size(Vertices); i+=2) {
        double x,y;

        code = pdfi_array_get_number(ctx, Vertices, i, &x);
        if (code < 0) goto exit;
        code = pdfi_array_get_number(ctx, Vertices, i+1, &y);
        if (code < 0) goto exit;

        if (i == 0) {
            code = gs_moveto(ctx->pgs, x, y);
            if (code < 0) goto exit;
        } else {
            code = gs_lineto(ctx->pgs, x, y);
            if (code < 0) goto exit;
        }
    }

 exit:
    return code;
}

/* Generate appearance (see pdf_draw.ps/PolyLine)
 * NOTE: as of 5-7-20, the gs implementation of this is actually broken
 *
 * If there was an AP it was already handled.
 */
static int pdfi_annot_draw_PolyLine(pdf_context *ctx, pdf_dict *annot, pdf_obj *NormAP, bool *render_done)
{
    int code = 0;
    int code1 = 0;
    pdf_array *Vertices = NULL, *Path = NULL, *Line = NULL;
    bool drawit;
    int size;
    double x1, y1, x2, y2, x3, y3, ends[8];
    bool Vertices_known = false, Path_known = false;

    code = pdfi_annot_start_transparency(ctx, annot);
    if (code < 0) goto exit1;

    code = pdfi_dict_known(ctx, annot, "Vertices", &Vertices_known);
    if (code < 0) goto exit1;
    code = pdfi_dict_known(ctx, annot, "Path", &Path_known);
    if (code < 0) goto exit1;

    code = pdfi_gsave(ctx);
    if (code < 0) goto exit1;

    /* If both are known, or neither are known, then there is a problem */
    if (Vertices_known == Path_known) {
        code = gs_note_error(gs_error_undefined);
        goto exit;
    }

    if (Vertices_known) {
        code = pdfi_dict_get_type(ctx, annot, "Vertices", PDF_ARRAY, (pdf_obj **)&Vertices);
        if (code < 0) goto exit;

        size = pdfi_array_size(Vertices);
        if (size == 0) {
            code = 0;
            goto exit;
        }
        code = pdfi_annot_path_array(ctx, annot, Vertices);
        if (code < 0) goto exit1;

        code = pdfi_annot_setcolor(ctx, annot, false, &drawit);
        if (code < 0) goto exit;

        code = pdfi_annot_draw_border(ctx, annot, true);
        if (code < 0) goto exit;

        if (size >= 4) {
            code = pdfi_array_get_number(ctx, Vertices, 0, &x1);
            if (code < 0) goto exit;
            code = pdfi_array_get_number(ctx, Vertices, 1, &y1);
            if (code < 0) goto exit;
            code = pdfi_array_get_number(ctx, Vertices, 2, &x2);
            if (code < 0) goto exit;
            code = pdfi_array_get_number(ctx, Vertices, 3, &y2);
            if (code < 0) goto exit;
            code = pdfi_annot_draw_LE(ctx, annot, x1, y1, x2, y2, 1);
            if (code < 0) goto exit;

            code = pdfi_array_get_number(ctx, Vertices, size-4, &x1);
            if (code < 0) goto exit;
            code = pdfi_array_get_number(ctx, Vertices, size-3, &y1);
            if (code < 0) goto exit;
            code = pdfi_array_get_number(ctx, Vertices, size-2, &x2);
            if (code < 0) goto exit;
            code = pdfi_array_get_number(ctx, Vertices, size-1, &y2);
            if (code < 0) goto exit;
            code = pdfi_annot_draw_LE(ctx, annot, x1, y1, x2, y2, 2);
            if (code < 0) goto exit;
        }
    } else {
        int i = 0;
        int state = 0;

        memset(ends, 0x00, 8 * sizeof(double));
        code = pdfi_dict_get_type(ctx, annot, "Path", PDF_ARRAY, (pdf_obj **)&Path);
        if (code < 0) goto exit;

        if (pdfi_array_size(Path) < 2)
            goto exit;

        code = pdfi_annot_setcolor(ctx, annot, false, &drawit);
        if (code < 0) goto exit;

        code = pdfi_annot_draw_border(ctx, annot, true);
        if (code < 0) goto exit;

        for (i = 0; i < pdfi_array_size(Path); i++) {
            code = pdfi_array_get_type(ctx, Path, i, PDF_ARRAY, (pdf_obj **)&Line);
            if (code < 0)
                goto exit;
            switch(pdfi_array_size(Line)) {
                case 2:
                    code = pdfi_array_get_number(ctx, Line, 0, &x1);
                    if (code < 0)
                        goto exit;
                    code = pdfi_array_get_number(ctx, Line, 1, &y1);
                    if (code < 0)
                        goto exit;
                    if (state == 0) {
                        state = 1;
                        code = gs_moveto(ctx->pgs, x1, y1);
                        ends[0] = ends[4] = x1;
                        ends[1] = ends[5] = y1;
                    } else {
                        if (state == 1) {
                            ends[2] = ends[6] = x1;
                            ends[3] = ends[7] = y1;
                            state = 2;
                        } else {
                            ends[4] = ends[6];
                            ends[5] = ends[7];
                            ends[6] = x1;
                            ends[7] = y1;
                        }
                        code = gs_lineto(ctx->pgs, x1, y1);
                    }
                    pdfi_countdown(Line);
                    Line = NULL;
                    break;
                case 6:
                    code = pdfi_array_get_number(ctx, Line, 0, &x1);
                    if (code < 0)
                        goto exit;
                    code = pdfi_array_get_number(ctx, Line, 1, &y1);
                    if (code < 0)
                        goto exit;
                    code = pdfi_array_get_number(ctx, Line, 2, &x2);
                    if (code < 0)
                        goto exit;
                    code = pdfi_array_get_number(ctx, Line, 3, &y2);
                    if (code < 0)
                        goto exit;
                    code = pdfi_array_get_number(ctx, Line, 4, &x3);
                    if (code < 0)
                        goto exit;
                    code = pdfi_array_get_number(ctx, Line, 5, &y3);
                    if (code < 0)
                        goto exit;
                    if (state == 1) {
                        ends[2] = x1;
                        ends[3] = y1;
                        ends[4] = x2;
                        ends[5] = y2;
                        ends[6] = x3;
                        ends[7] = y3;
                        state = 2;
                    }
                    ends[4] = x2;
                    ends[5] = y2;
                    ends[6] = x3;
                    ends[7] = y3;
                    code = gs_curveto(ctx->pgs, x1, y1, x2, y2, x3, y3);
                    pdfi_countdown(Line);
                    Line = NULL;
                    break;
                default:
                    pdfi_countdown(Line);
                    Line = NULL;
                    code = gs_note_error(gs_error_rangecheck);
                    break;
            }
            if (code < 0)
                break;
        }
        if (code < 0)
            goto exit;

        code = gs_stroke(ctx->pgs);
        if (code < 0)
            goto exit;

        code = pdfi_annot_draw_LE(ctx, annot, ends[0], ends[1], ends[2], ends[3], 1);
        if (code < 0)
            goto exit;
        code = pdfi_annot_draw_LE(ctx, annot, ends[4], ends[5], ends[6], ends[7], 2);
    }

 exit:
    code1 = pdfi_annot_end_transparency(ctx, annot);
    if (code >= 0)
        code = code1;
    code1 = pdfi_grestore(ctx);
    if (code >= 0) code = code1;

 exit1:
    *render_done = true;
    pdfi_countdown(Line);
    pdfi_countdown(Vertices);
    pdfi_countdown(Path);
    return code;
}

/* Generate appearance (see pdf_draw.ps/Polygon)
 *
 * If there was an AP it was already handled.
 */
static int pdfi_annot_draw_Polygon(pdf_context *ctx, pdf_dict *annot, pdf_obj *NormAP, bool *render_done)
{
    int code = 0;
    int code1 = 0;
    pdf_array *Vertices = NULL;
    bool drawit;

    code = pdfi_annot_start_transparency(ctx, annot);
    if (code < 0) goto exit1;

    code = pdfi_dict_knownget_type(ctx, annot, "Vertices", PDF_ARRAY, (pdf_obj **)&Vertices);
    if (code <= 0) goto exit;

    code = pdfi_annot_path_array(ctx, annot, Vertices);
    if (code < 0) goto exit1;

    code = gs_closepath(ctx->pgs);

    /* NOTE: The logic here seems a bit wonky.  Why only set opacity if there was a fill?
     * Anyway, it is based on the ps code (pdf_draw.ps/Polygon).
     * Maybe can be improved.
     */
    code = pdfi_annot_setinteriorcolor(ctx, annot, false, &drawit);
    if (code < 0) goto exit;
    if (drawit) {
        code = pdfi_annot_fillborderpath(ctx, annot);
        if (code < 0) goto exit;
        code = pdfi_annot_opacity(ctx, annot); /* TODO: Why only on this path? */
        if (code < 0) goto exit;
    }
    code = pdfi_annot_setcolor(ctx, annot, false, &drawit);
    if (code < 0) goto exit;
    if (drawit) {
        code = pdfi_annot_draw_border(ctx, annot, true);
        if (code < 0) goto exit;
    }

 exit:
    code1 = pdfi_annot_end_transparency(ctx, annot);
    if (code >= 0)
        code = code1;
 exit1:
    *render_done = true;
    pdfi_countdown(Vertices);
    return code;
}

/* Generate appearance (see pdf_draw.ps/Square)
 *
 * If there was an AP it was already handled.
 */
static int pdfi_annot_draw_Square(pdf_context *ctx, pdf_dict *annot, pdf_obj *NormAP, bool *render_done)
{
    int code = 0;
    int code1 = 0;
    bool drawit;

    code = pdfi_annot_start_transparency(ctx, annot);
    if (code < 0) goto exit1;

    code = pdfi_annot_setinteriorcolor(ctx, annot, false, &drawit);
    if (code < 0) goto exit;
    if (drawit) {
        code = pdfi_annot_opacity(ctx, annot);
        if (code < 0) goto exit;
        code = pdfi_annot_fillRect(ctx, annot);
        if (code < 0) goto exit;

        code = pdfi_annot_setcolor(ctx, annot, false, &drawit);
        if (code < 0) goto exit;

        if (drawit) {
            code = pdfi_annot_draw_border(ctx, annot, false);
            if (code < 0) goto exit;
        }
    } else {
        code = pdfi_annot_RectRD_path(ctx, annot);
        if (code < 0) goto exit;

        code = pdfi_annot_setcolor(ctx, annot, false, &drawit);
        if (code < 0) goto exit;

        if (drawit) {
            code = pdfi_annot_draw_border(ctx, annot, true);
            if (code < 0) goto exit;
        }
    }

 exit:
    code1 = pdfi_annot_end_transparency(ctx, annot);
    if (code >= 0)
        code = code1;
 exit1:
    *render_done = true;
    return code;
}

static int pdfi_annot_render_MK_box(pdf_context *ctx, pdf_dict *annot, pdf_dict *MK)
{
    int code = 0;
    bool drawit = false;
    gs_rect rect;
    bool need_grestore = false;

    /* Basically just render the rectangle around the widget for now */
    code = pdfi_annot_Rect(ctx, annot, &rect);
    if (code < 0) goto exit;

    code = pdfi_gsave(ctx);
    if (code < 0) goto exit;
    need_grestore = true;
    code = pdfi_annot_setcolor_key(ctx, MK, "BG", false, &drawit);
    if (code < 0) goto exit;
    if (drawit)
        code = gs_rectfill(ctx->pgs, &rect, 1);
    if (code < 0) goto exit;
    need_grestore = false;
    code = pdfi_grestore(ctx);
    if (code < 0) goto exit;

    code = pdfi_gsave(ctx);
    if (code < 0) goto exit;
    need_grestore = true;
    code = pdfi_annot_setcolor_key(ctx, MK, "BC", false, &drawit);
    if (code < 0) goto exit;
    if (drawit)
        code = gs_rectstroke(ctx->pgs, &rect, 1, NULL);
    if (code < 0) goto exit;
    need_grestore = false;
    code = pdfi_grestore(ctx);
    if (code < 0) goto exit;

    /* TODO: Stuff with can-regenerate-ap ?? */

 exit:
    if (need_grestore)
        (void)pdfi_grestore(ctx);
    return code;
}

typedef enum {
    PDFI_FORM_FF_READONLY = 1, /* Bit 1 */
    PDFI_FORM_FF_REQUIRED = 1 << 1, /* Bit 2 */
    PDFI_FORM_FF_NOEXPORT = 1 << 2, /* Bit 3 */
    PDFI_FORM_FF_MULTILINE = 1 << 12, /* Bit 13 */
    PDFI_FORM_FF_PASSWORD = 1 << 13, /* Bit 14 */
    PDFI_FORM_FF_NOTOGGLETOOFF = 1 << 14, /* Bit 15 */
    PDFI_FORM_FF_RADIO = 1 << 15, /* Bit 16 */
    PDFI_FORM_FF_PUSHBUTTON = 1 << 16, /* Bit 17 */
    PDFI_FORM_FF_COMBO = 1 << 17, /* Bit 18 */
    PDFI_FORM_FF_EDIT = 1 << 18, /* Bit 19 */
    PDFI_FORM_FF_SORT = 1 << 19, /* Bit 20 */
    PDFI_FORM_FF_FILESELECT = 1 << 20, /* Bit 21 */
    PDFI_FORM_FF_MULTISELECT = 1 << 21, /* Bit 22 */
    PDFI_FORM_FF_DONOTSPELLCHECK = 1 << 22, /* Bit 23 */
    PDFI_FORM_FF_DONOTSCROLL = 1 << 23, /* Bit 24 */
    PDFI_FORM_FF_COMB = 1 << 24, /* Bit 25 */
    PDFI_FORM_FF_RICHTEXT = 1 << 25, /* Bit 26 */ /* also PDFI_FORM_FF_RADIOSINUNISON */
    PDFI_FORM_FF_COMMITONSELCHANGE = 1 << 26, /* Bit 27 */
} pdfi_form_Ff;

/* draw field Btn */
static int pdfi_form_draw_Btn(pdf_context *ctx, pdf_dict *field, pdf_obj *AP)
{
    int code;
    bool Radio = false;
    bool Pushbutton = false;
    int64_t value;

    /* TODO: There can be Kids array. I think it should be handled in caller.
     * Need an example...
     */
    if (AP != NULL) {
        code = pdfi_annot_draw_AP(ctx, field, AP);
        goto exit;
    }

    /* Get Ff field (default is 0) */
    code = pdfi_form_get_inheritable_int(ctx, field, "Ff", &value);
    if (code < 0) goto exit;

    Radio = value & PDFI_FORM_FF_RADIO;
    Pushbutton = value & PDFI_FORM_FF_PUSHBUTTON;

    dmprintf(ctx->memory, "WARNING: AcroForm field 'Btn' with no AP not implemented.\n");
    dmprintf2(ctx->memory, "       : Radio = %s, Pushbutton = %s.\n",
              Radio ? "TRUE" : "FALSE", Pushbutton ? "TRUE" : "FALSE");
 exit:
    return 0;
}

/* Modify V from Tx widget to skip past the BOM and indicate if UTF-16 was found */
static int pdfi_form_modV(pdf_context *ctx, pdf_string *V, pdf_string **mod_V, bool *is_UTF16)
{
    bool did_mod = false;
    int code = 0;
    int skip = 0;
    pdf_string *newV = NULL;

    *is_UTF16 = false;

    if (IS_UTF8(V->data)) {
        skip = 3;
    } else if (IS_UTF16(V->data)) {
        skip = 2;
        *is_UTF16 = true;
    }

    if (skip == 0)
        goto exit;

    /* Create a new V that skips over the BOM */
    code = pdfi_object_alloc(ctx, PDF_STRING, V->length-skip, (pdf_obj **)&newV);
    if (code < 0)
        goto exit;
    pdfi_countup(newV);

    memcpy(newV->data, V->data+skip, newV->length);
    did_mod = true;

 exit:
    /* If didn't need to mod it, just copy the original and get a ref */
    if (did_mod) {
        *mod_V = newV;
    } else {
        *mod_V = V;
        pdfi_countup(V);
    }
    return code;
}

/* Display simple text (no multiline or comb) */
static int pdfi_form_Tx_simple(pdf_context *ctx, pdf_dict *annot, gs_rect *rect, pdf_string *V,
                               int64_t Ff, int64_t Q, bool is_UTF16)
{
    int code = 0;
    gs_rect modrect;
    double lineheight = 0;
    gs_rect bbox;
    gs_point awidth;
    double y_adjust = 0.0f, x_adjust = 0.0f;

    modrect = *rect; /* structure copy */

    code = pdfi_annot_get_text_height(ctx, &lineheight);
    if (code < 0) goto exit;

    /* text placement adjustments */
    switch (Q) {
    case 0: /* Left-justified */
        x_adjust = 2; /* empirical value */
        break;
    case 1: /* Centered */
        /* Get width of the string */
        code = pdfi_string_bbox(ctx, V, &bbox, &awidth, false);
        if (code < 0) goto exit;
        x_adjust = ((rect->q.x - rect->p.x) - awidth.x) / 2;
        break;
    case 2: /* Right-justified */
        /* Get width of the string */
        code = pdfi_string_bbox(ctx, V, &bbox, &awidth, false);
        if (code < 0) goto exit;
        x_adjust = rect->q.x - awidth.x;
        break;
    }

    /* Center vertically */
    y_adjust = ((rect->q.y - rect->p.y) - lineheight) / 2;

    modrect.p.x += x_adjust;
    modrect.p.y += y_adjust;
    modrect.p.y += (lineheight + 6.) / 10.; /* empirical */

    code = pdfi_annot_display_simple_text(ctx, annot, modrect.p.x, modrect.p.y, V);
    if (code < 0) goto exit;
 exit:
    return code;
}

/* Display text from Tx MULTILINE */
static int pdfi_form_Tx_multiline(pdf_context *ctx, pdf_dict *annot, gs_rect *rect, pdf_string *V,
                                  int64_t Ff, int64_t Q, bool is_UTF16)
{
    int code = 0;
    gs_rect modrect;

    modrect = *rect; /* structure copy */
    /* empirical tweaks */
    modrect.p.x += 2;
    modrect.p.y += 2;
    modrect.q.x -= 2;
    modrect.q.y -= 2;

    code = pdfi_annot_display_formatted_text(ctx, annot, &modrect, V, is_UTF16);
    if (code < 0) goto exit;

 exit:
    return code;
}


/* Display text from Tx COMB */
static int pdfi_form_Tx_comb(pdf_context *ctx, pdf_dict *annot, gs_rect *rect, pdf_string *V,
                             int64_t Ff, int64_t Q, int64_t MaxLen, bool is_UTF16)
{
    int code = 0;

    /* TODO: Implement... Need a sample that uses COMB! */
    code = pdfi_form_Tx_simple(ctx, annot, rect, V, Ff, Q, is_UTF16);
    return code;
}


/* Display text from Tx in a field or widget */
static int pdfi_form_display_Tx(pdf_context *ctx, pdf_dict *annot, gs_rect *rect, pdf_string *V,
                                  int64_t Ff, int64_t Q, int64_t MaxLen)
{
    int code = 0;
    pdf_string *modV = NULL;
    bool is_UTF16;

    /* TODO: If we wanted to preserve this for pdfwrite, we would have to generate
     * a stream for the AP.  See PDF1.7 spec, Variable Text (page 677).
     */

    /* Get modified V that skips the BOM, if any */
    code = pdfi_form_modV(ctx, V, &modV, &is_UTF16);
    if (code < 0) goto exit;

    if (Ff & PDFI_FORM_FF_MULTILINE) {
        code = pdfi_form_Tx_multiline(ctx, annot, rect, modV, Ff, Q, is_UTF16);
    } else if (Ff & PDFI_FORM_FF_COMB) {
        code = pdfi_form_Tx_comb(ctx, annot, rect, modV, Ff, Q, MaxLen, is_UTF16);
    } else {
        code = pdfi_form_Tx_simple(ctx, annot, rect, modV, Ff, Q, is_UTF16);
    }

 exit:
    pdfi_countdown(modV);
    return code;
}


/* At least for now, Tx and Ch are handled the same */
static int pdfi_form_draw_Tx_Ch(pdf_context *ctx, pdf_dict *annot)
{
    int code = 0;
    pdf_string *V = NULL;
    gs_rect annotrect;
    int64_t Ff;
    int64_t MaxLen;
    int64_t Q;

    code = pdfi_annot_Rect(ctx, annot, &annotrect);
    if (code < 0) goto exit;

    /* Set DA (Default Appearance)
     * This will generally set a color and font.
     */
    code = pdfi_annot_process_DA(ctx, annot, annot, &annotrect, true);
    if (code < 0) goto exit;

    code = pdfi_form_get_inheritable_int(ctx, annot, "Ff", &Ff);
    if (code < 0) goto exit;

    code = pdfi_form_get_inheritable_int(ctx, annot, "MaxLen", &MaxLen);
    if (code < 0) goto exit;

    code = pdfi_form_get_inheritable_int(ctx, annot, "Q", &Q);
    if (code < 0) goto exit;

    code = pdfi_dict_knownget_type(ctx, annot, "V", PDF_STRING, (pdf_obj **)&V);
    if (code < 0) goto exit;
    if (code > 0) {
        code = pdfi_form_display_Tx(ctx, annot, &annotrect, V, Ff, Q, MaxLen);
        if (code < 0) goto exit;
    }

 exit:
    pdfi_countdown(V);
    return code;
}

/* draw field Tx */
static int pdfi_form_draw_Tx(pdf_context *ctx, pdf_dict *annot, pdf_obj *AP)
{
    if (AP != NULL)
        return pdfi_annot_draw_AP(ctx, annot, AP);

    return pdfi_form_draw_Tx_Ch(ctx, annot);
}

/* draw field Ch */
static int pdfi_form_draw_Ch(pdf_context *ctx, pdf_dict *field, pdf_obj *AP)
{
    if (!ctx->NeedAppearances && AP != NULL)
        return pdfi_annot_draw_AP(ctx, field, AP);

    return pdfi_form_draw_Tx_Ch(ctx, field);
}

/* draw field Sig */
static int pdfi_form_draw_Sig(pdf_context *ctx, pdf_dict *field, pdf_obj *AP)
{
    int code = 0;

    if (!ctx->NeedAppearances && AP != NULL)
        return pdfi_annot_draw_AP(ctx, field, AP);

    dmprintf(ctx->memory, "WARNING: AcroForm field 'Sig' with no AP not implemented.\n");

    return code;
}

/* TODO: The spec handles "regenerate appearance", which it does by generating
 * a new AP stream for the field, and then rendering it as a form.
 * The gs code does try to handle this case.
 * For now I am just going to generate the data inline, without building it as a Form/Stream.
 * If we have want to preserve the AP (i.e. for pdfwrite device, generate the AP and include
 * it in the output, rather than just rendering it into the output) then this needs to change.
 * Currently gs doesn't try to preserve AcroForms, so we aren't losing any functionality by
 * not doing this.  But it is a place for a future enhancement.
 *
 * See also, the comment on pdfi_annot_preserve_Widget().
 */
static int pdfi_annot_render_field(pdf_context *ctx, pdf_dict *field, pdf_name *FT, pdf_obj *AP)
{
    int code;

    if (pdfi_name_is(FT, "Btn")) {
        code = pdfi_form_draw_Btn(ctx, field, AP);
    } else if (pdfi_name_is(FT, "Tx")) {
        code = pdfi_form_draw_Tx(ctx, field, AP);
    } else if (pdfi_name_is(FT, "Ch")) {
        code = pdfi_form_draw_Ch(ctx, field, AP);
    } else if (pdfi_name_is(FT, "Sig")) {
        code = pdfi_form_draw_Sig(ctx, field, AP);
    } else {
        dmprintf(ctx->memory, "*** WARNING unknown field FT ignored\n");
        /* TODO: Handle warning better */
        code = 0;
    }

    return code;
}

/* Render Widget with no AP
 */
static int pdfi_annot_render_Widget(pdf_context *ctx, pdf_dict *annot)
{
    int code = 0;
    pdf_dict *MK = NULL;
    pdf_name *FT = NULL;

    /* Render box around the widget using MK */
    code = pdfi_dict_knownget_type(ctx, annot, "MK", PDF_DICT, (pdf_obj **)&MK);
    if (code < 0) goto exit;
    if (code > 0) {
        code = pdfi_annot_render_MK_box(ctx, annot, MK);
        if (code < 0) goto exit;
    }

    /* Render the other contents */
    code = pdfi_dict_knownget_type(ctx, annot, "FT", PDF_NAME, (pdf_obj **)&FT);
    if (code < 0) goto exit;
    if (code > 0) {
        code = pdfi_annot_render_field(ctx, annot, FT, NULL);
        if (code < 0) goto exit;
    }

 exit:
    pdfi_countdown(FT);
    pdfi_countdown(MK);
    return code;
}

/* Draws a thing of type /Widget */
static int pdfi_annot_draw_Widget(pdf_context *ctx, pdf_dict *annot, pdf_obj *NormAP, bool *render_done)
{
    int code = 0;
    bool found_T = false;
    bool found_FT = false, known = false;
    pdf_obj *T = NULL;
    pdf_obj *FT = NULL;
    pdf_dict *Parent = NULL;
    pdf_dict *currdict = NULL;

    /* From pdf_draw.ps/drawwidget:
  % Acrobat doesn't draw Widget annotations unles they have both /FT
  % (which is defined as required) and /T keys present. Annoyingly
  % these can either be inherited from the Form Definition Field
  % dictionary (via the AcroForm tree) or present directly in the
  % annotation, so we need to check the annotation to make sure its
  % a Widget, then follow any /Parent key up to the root node
  % extracting and storing any FT or T keys as we go (we only care if
  % these are present, their value is immaterial). If after all that
  % both keys are not present, then we don't draw the annotation.
    */

    /* TODO: See top part of pdf_draw.ps/drawwidget
     * check for /FT and /T and stuff
     */
    code = pdfi_loop_detector_mark(ctx);
    if (code < 0)
        goto exit;

    currdict = annot;
    pdfi_countup(currdict);
    while (true) {
        if (currdict->object_num != 0) {
            code = pdfi_loop_detector_add_object(ctx, currdict->object_num);
            if (code < 0)
                break;
        }

        code = pdfi_dict_knownget(ctx, currdict, "T", &T);
        if (code < 0) goto exit;
        if (code > 0) {
            found_T = true;
            pdfi_countdown(T);
            T = NULL;
            if (found_FT)
                break;
        }
        code = pdfi_dict_knownget(ctx, currdict, "FT", &FT);
        if (code < 0) goto exit;
        if (code > 0) {
            found_FT = true;
            pdfi_countdown(FT);
            FT = NULL;
            if (found_T)
                break;
        }
        /* Check for Parent. Do not store the dereferenced Parent back to the dictionary
         * as this can cause circular references.
         */
        code = pdfi_dict_known(ctx, currdict, "Parent", &known);
        if (code >= 0 && known == true)
        {
            code = pdfi_dict_get_no_store_R(ctx, currdict, "Parent", (pdf_obj **)&Parent);
            if (code < 0) {
                (void)pdfi_loop_detector_cleartomark(ctx);
                goto exit;
            }
            if (pdfi_type_of(Parent) != PDF_DICT) {
                if (pdfi_type_of(Parent) == PDF_INDIRECT) {
                    pdf_indirect_ref *o = (pdf_indirect_ref *)Parent;

                    code = pdfi_dereference(ctx, o->ref_object_num, o->ref_generation_num, (pdf_obj **)&Parent);
                    pdfi_countdown(o);
                    if (code < 0)
                        break;
                } else {
                    break;
                }
            }
            pdfi_countdown(currdict);
            currdict = Parent;
            Parent = NULL;
        } else
            break;
    }

    (void)pdfi_loop_detector_cleartomark(ctx);

    code = 0;
    if (!found_T || !found_FT) {
        *render_done = true;
        dmprintf(ctx->memory, "**** Warning: A Widget annotation dictionary lacks either the FT or T key.\n");
        dmprintf(ctx->memory, "              Acrobat ignores such annoataions, annotation will not be rendered.\n");
        dmprintf(ctx->memory, "              Output may not be as expected.\n");
        goto exit;
    }

    if (NormAP) {
        /* Let caller render it */
        *render_done = false;
        goto exit;
    }

    /* No AP, try to render the Widget ourselves */
    code = pdfi_annot_render_Widget(ctx, annot);
    *render_done = true;

 exit:
    pdfi_countdown(Parent);
    pdfi_countdown(currdict);
    return code;
}

/* Handle Annotations that are not implemented */
static int pdfi_annot_draw_NotImplemented(pdf_context *ctx, pdf_dict *annot, pdf_obj *NormAP, bool *render_done)
{
    int code = 0;
    pdf_name *Subtype = NULL;
    char str[100];

    code = pdfi_dict_get_type(ctx, annot, "Subtype", PDF_NAME, (pdf_obj **)&Subtype);
    if (code < 0) goto exit;

    memcpy(str, (const char *)Subtype->data, Subtype->length < 100 ? Subtype->length : 99);
    str[Subtype->length < 100 ? Subtype->length : 99] = '\0';
    dbgmprintf1(ctx->memory, "ANNOT: No AP, default appearance for Subtype %s Not Implemented\n", str);

 exit:
    *render_done = false;
    pdfi_countdown(Subtype);
    return code;
}

annot_dispatch_t annot_dispatch[] = {
    /* These are in the same order as the PDF 2.0 spec 12.5.6.1 */
    {"Text", pdfi_annot_draw_Text, true},
    {"Link", pdfi_annot_draw_Link, false},
    {"FreeText", pdfi_annot_draw_FreeText, true},
    {"Line", pdfi_annot_draw_Line, true},
    {"Square", pdfi_annot_draw_Square, true},
    {"Circle", pdfi_annot_draw_Circle, true},
    {"Polygon", pdfi_annot_draw_Polygon, true},
    {"PolyLine", pdfi_annot_draw_PolyLine, true},
    {"Highlight", pdfi_annot_draw_Highlight, true},
    {"Underline", pdfi_annot_draw_Underline, true},
    {"Squiggly", pdfi_annot_draw_Squiggly, true},
    {"StrikeOut", pdfi_annot_draw_StrikeOut, true},
    {"Caret", pdfi_annot_draw_NotImplemented, true},
    {"Stamp", pdfi_annot_draw_Stamp, true},
    {"Ink", pdfi_annot_draw_Ink, true},
    {"Popup", pdfi_annot_draw_Popup, false},
    {"FileAttachment", pdfi_annot_draw_NotImplemented, true},
    {"Sound", pdfi_annot_draw_NotImplemented, true},
    {"Movie", pdfi_annot_draw_NotImplemented, true},
    {"Screen", pdfi_annot_draw_NotImplemented, true},
    {"Widget", pdfi_annot_draw_Widget, false},
    {"PrinterMark", pdfi_annot_draw_NotImplemented, true},
    {"TrapNet", pdfi_annot_draw_NotImplemented, true},
    {"Watermark", pdfi_annot_draw_NotImplemented, true},
    {"3D", pdfi_annot_draw_NotImplemented, true},
    {"Redact", pdfi_annot_draw_Redact, true},
    {"Projection", pdfi_annot_draw_NotImplemented, true},
    {"RichMedia", pdfi_annot_draw_NotImplemented, true},
    { NULL, NULL},
};

/* Check if annotation is visible
 * (from ps code:)
%
%   The PDF annotation F (flags) integer is bit encoded.
%   Bit 1 (LSB) Invisible:  1 --> Do not display if no handler.
%         Note:  We have no handlers but we ignore this bit.
%   Bit 2 Hidden:  1 --> Do not display.  We will not display if this bit is set.
%   Bit 3 Print:  1 --> Display if printing.  We will display if this bit set
%         (and not hidden) and Printed is true
%   Bit 4 NoZoom:  1 --> Do not zoom annotation even if image is zoomed.
%   Bit 5 NoRotate:  1 --> Do not rotate annotation even if image is rotated.
%   Bit 6 NoView:  0 --> Display if this is a 'viewer'.  We will display
%         if this bit is not set (and not hidden) and Printed is false
%   Bit 7 Read Only - 1 --> No interaction.  We ignore this bit
%
%   AR8 and AR9 print 3D annotations even if Print flag is off. Bug 691486.
%
*/
static bool pdfi_annot_visible(pdf_context *ctx, pdf_dict *annot, pdf_name *subtype)
{
    int code;
    bool is_3D = false;
    bool is_visible = true;
    int64_t F;

    if (pdfi_name_is(subtype, "3D"))
        is_3D = true;

    code = pdfi_dict_get_int(ctx, annot, "F", &F);
    if (code < 0)
        F = 0;

    if ((F & 0x2) != 0) { /* Bit 2 -- Hidden */
        is_visible = false; /* Hidden */
        goto exit;
    }

    if (ctx->args.printed) {
        /* Even if Print flag (bit 3) is off, will print if 3D */
        is_visible = ((F & 0x4) != 0) || is_3D;
    } else {
        /* Not NoView (bit 6) */
        is_visible = (F & 0x20) == 0;
    }

 exit:
    return is_visible;
}

/* Check to see if subtype is on the ShowAnnotTypes list
 * Defaults to true if there is no list
 */
static bool pdfi_annot_check_type(pdf_context *ctx, pdf_name *subtype)
{
    char **ptr;

    /* True if no list */
    if (!ctx->args.showannottypes)
        return true;

    /* Null terminated list, return true if subtype is on the list */
    ptr = ctx->args.showannottypes;
    while (*ptr) {
        char *str = *ptr;
        if (pdfi_name_is(subtype, str))
            return true;
        ptr ++;
    }
    /* List exists, but subtype is not on it */
    return false;
}

/* Check to see if subtype is on the PreserveAnnotTypes list
 * Defaults to true if there is no list
 */
static bool pdfi_annot_preserve_type(pdf_context *ctx, pdf_name *subtype)
{
    char **ptr;

    /* True if no list */
    if (!ctx->args.preserveannottypes)
        return true;

    /* Null terminated list, return true if subtype is on the list */
    ptr = ctx->args.preserveannottypes;
    while (*ptr) {
        char *str = *ptr;
        if (pdfi_name_is(subtype, str))
            return true;
        ptr ++;
    }
    /* List exists, but subtype is not on it */
    return false;
}

static int pdfi_annot_draw(pdf_context *ctx, pdf_dict *annot, pdf_name *subtype)
{
    pdf_obj *NormAP = NULL;
    int code = 0;
    annot_dispatch_t *dispatch_ptr;
    bool render_done = true;

    /* See if annotation is visible */
    if (!pdfi_annot_visible(ctx, annot, subtype))
        goto exit;

    /* See if we are rendering this type of annotation */
    if (!pdfi_annot_check_type(ctx, subtype))
        goto exit;

    /* Get the Normal AP, if it exists */
    code = pdfi_annot_get_NormAP(ctx, annot, &NormAP);
    if (code < 0) goto exit;

    code = pdfi_gsave(ctx);
    if (code < 0) goto exit;

    /* Draw the annotation */
    for (dispatch_ptr = annot_dispatch; dispatch_ptr->subtype; dispatch_ptr ++) {
        if (pdfi_name_is(subtype, dispatch_ptr->subtype)) {
            if (NormAP && dispatch_ptr->simpleAP)
                render_done = false;
            else
                code = dispatch_ptr->func(ctx, annot, NormAP, &render_done);
            break;
        }
    }
    if (!dispatch_ptr->subtype) {
        char str[100];
        memcpy(str, (const char *)subtype->data, subtype->length < 100 ? subtype->length : 99);
        str[subtype->length < 100 ? subtype->length : 99] = '\0';
        dbgmprintf1(ctx->memory, "ANNOT: No handler for subtype %s\n", str);

        /* Not necessarily an error? We can just render the AP if there is one */
        render_done = false;
    }

    if (!render_done)
        code = pdfi_annot_draw_AP(ctx, annot, NormAP);

    (void)pdfi_grestore(ctx);

 exit:
    pdfi_countdown(NormAP);
    return code;
}

/* Create a string containing form label
 * I don't think the format actually matters, though it probably needs to be unique
 * Just use a counter to ensure uniqueness
 *
 * Format: {FormName%d}
 */
static int pdfi_annot_preserve_nextformlabel(pdf_context *ctx, byte **data, int *len)
{
    int size = 40;
    char *buf;
    gs_id counter = gs_next_ids(ctx->memory, 1);

    buf = (char *)gs_alloc_bytes(ctx->memory, size, "pdfi_annot_preserve_nextformlabel(buf)");
    if (buf == NULL)
        return_error(gs_error_VMerror);
    snprintf(buf, size, "{FN%ld}", counter);
    *len = strlen(buf);
    *data = (byte *)buf;
    return 0;
}

/* Modify the QuadPoints array to correct for some BS in the pdfwrite driver.
 * comment from gs code (pdf_draw.ps/ApplyCMTToQuadPoints()):
  %% Nasty hackery here really. We need to undo the HWResolution scaling which
  %% is done by pdfwrite. Default is 720 dpi, so 0.1. We also need to make
  %% sure that any translation of the page (because its been rotated for example)
  %% is also modified by the requisite amount. SO we ned to calculate a matrix
  %% which does the scaling and concatenate it with the current matrix.
  %% Do this inside a gsave/grestore pair to avoid side effects!
 *
 * TODO: In practice, when I was debugging this code I found that it does nothing
 * useful, since the matrice it calculates is essentially the identity matrix.
 * I am not sure under what circumstance it is needed?
 */
static int pdfi_annot_preserve_modQP(pdf_context *ctx, pdf_dict *annot, pdf_name *QP_key)
{
    int code = 0;
    pdf_array *QP = NULL;
    gx_device *device = gs_currentdevice(ctx->pgs);
    gs_matrix devmatrix, ctm, matrix;
    uint64_t arraysize, index;
    double old_x, old_y;
    gs_point point;

    code = pdfi_dict_get(ctx, annot, "QuadPoints", (pdf_obj **)&QP);
    if (code < 0) goto exit;

    if (pdfi_type_of(QP) != PDF_ARRAY) {
        /* Invalid QuadPoints, just delete it because I dunno what to do...
         * TODO: Should flag a warning here
         */
        code = pdfi_dict_delete_pair(ctx, annot, QP_key);
        goto exit;
    }

    arraysize = pdfi_array_size(QP);
    if (arraysize % 8 != 0) {
        /* TODO: Flag a warning -- must be sets of 8 values (4 points) */
        code = gs_note_error(gs_error_syntaxerror);
        goto exit;
    }


    /* Get device matrix and adjust by default 72.0 (no idea, just following PS code...) */
    devmatrix.xx = 72.0 / device->HWResolution[0];
    devmatrix.xy = 0;
    devmatrix.yx = 0;
    devmatrix.yy = 72.0 / device->HWResolution[1];
    devmatrix.tx = 0;
    devmatrix.ty = 0;

    /* Get the CTM */
    gs_currentmatrix(ctx->pgs, &ctm);

    /* Get matrix to adjust the QuadPoints */
    code = gs_matrix_multiply(&ctm, &devmatrix, &matrix);
    if (code < 0) goto exit;

    /* Transform all the points by the calculated matrix */
    for (index = 0; index < arraysize; index += 2) {
        code = pdfi_array_get_number(ctx, QP, index, &old_x);
        if (code < 0) goto exit;
        code = pdfi_array_get_number(ctx, QP, index+1, &old_y);
        if (code < 0) goto exit;

        code = gs_point_transform(old_x, old_y, &matrix, &point);
        if (code < 0) goto exit;

        code = pdfi_array_put_real(ctx, QP, index, point.x);
        if (code < 0) goto exit;
        code = pdfi_array_put_real(ctx, QP, index+1, point.y);
        if (code < 0) goto exit;
    }


 exit:
    pdfi_countdown(QP);
    return code;
}

/* Build a high level form for the AP and replace it with an indirect reference
 *
 * There can multiple AP in the dictionary, so do all of them.
 */
static int pdfi_annot_preserve_modAP(pdf_context *ctx, pdf_dict *annot, pdf_name *AP_key)
{
    int code = 0;
    pdf_dict *AP = NULL;
    uint64_t index;
    pdf_name *Key = NULL;
    pdf_indirect_ref *Value = NULL;
    byte *labeldata = NULL;
    int labellen;
    int form_id;
    gx_device *device = gs_currentdevice(ctx->pgs);
    bool found_ap = false; /* found at least one AP stream? */
    pdf_obj *object = NULL;

    code = pdfi_dict_get(ctx, annot, "AP", (pdf_obj **)&AP);
    if (code < 0) goto exit;

    if (pdfi_type_of(AP) != PDF_DICT) {
        /* This is an invalid AP, we will flag and delete it below */
        found_ap = false;
        goto exit;
    }

    code = pdfi_dict_key_first(ctx, AP, (pdf_obj **)&Key, &index);
    while (code >= 0) {
        found_ap = true;
        code = pdfi_dict_get_no_deref(ctx, AP, Key, (pdf_obj **)&Value);
        if (code < 0) goto exit;

        /* Handle indirect object */
        if (pdfi_type_of(Value) != PDF_INDIRECT)
            goto loop_continue;

        /* Dereference it */
        code = pdfi_dereference(ctx, Value->ref_object_num, Value->ref_generation_num, &object);
        if (code < 0) goto exit;

        if (pdfi_type_of(object) == PDF_STREAM) {
            /* Get a form label */
            code = pdfi_annot_preserve_nextformlabel(ctx, &labeldata, &labellen);
            if (code < 0) goto exit;

            /* Notify the driver of the label name */
            code = (*dev_proc(device, dev_spec_op))
                (device, gxdso_pdf_form_name, labeldata, labellen);

            code = pdfi_op_q(ctx);
            if (code < 0)
                goto exit;

            code = pdfi_annot_position_AP(ctx, annot, (pdf_stream *)object);
            /* Draw the high-level form */
            code = pdfi_do_highlevel_form(ctx, ctx->page.CurrentPageDict, (pdf_stream *)object);
            (void)pdfi_op_Q(ctx);
            if (code < 0) goto exit;

            /* Get the object number (form_id) of the high level form */
            code = (*dev_proc(device, dev_spec_op))
                (device, gxdso_get_form_ID, &form_id, sizeof(int));

            /* Save the highlevel form info for pdfi_obj_indirect_str() */
            Value->highlevel_object_num = form_id;
            Value->is_highlevelform = true;

        }

    loop_continue:
        pdfi_countdown(Key);
        Key = NULL;
        pdfi_countdown(Value);
        Value = NULL;
        pdfi_countdown(object);
        object = NULL;
        gs_free_object(ctx->memory, labeldata, "pdfi_annot_preserve_modAP(labeldata)");
        labeldata = NULL;

        code = pdfi_dict_key_next(ctx, AP, (pdf_obj **)&Key, &index);
        if (code == gs_error_undefined) {
            code = 0;
            break;
        }
    }
    if (code < 0) goto exit;

 exit:
    /* If there was no AP found, then delete the key completely.
     * (Bug697951.pdf)
     */
    if (!found_ap) {
        /* TODO: Flag a warning for broken file? */
        code = pdfi_dict_delete_pair(ctx, annot, AP_key);
    }

    if (labeldata)
        gs_free_object(ctx->memory, labeldata, "pdfi_annot_preserve_modAP(labeldata)");
    pdfi_countdown(AP);
    pdfi_countdown(Key);
    pdfi_countdown(Value);
    pdfi_countdown(object);
    return code;
}

/* Make a temporary copy of the annotation dict with some fields left out or
 * modified, then do a pdfmark on it
 */

const char *PermittedKeys[] = {
    /* These keys are valid for all annotation types, we specifically do not allow /P or /Parent */
    "Type",
    "Subtype",
    "Rect",
    "Contents",
    "NM",
    "M",
    "F",
    "AP",
    "AS",
    "Border",
    "C",
    "StructParent",
    "OC",
    "AF",
    "ca",
    "CA",
    "BM",
    "Lang",
    /* Keys by annotation type (some are common to more than one type, only one entry per key) */
    /* Markup Annotations we specifically do not permit RT, IRT or Popup */
    "T",
    "RC",
    "CreationDate",
    "Subj",
    "IT",
    "ExData",
    /* Text annotations */
    "Open",
    "Name",
    "State",
    "StateModel",
    /* This isn't specified as being allowed, but Acrobat does something with it, so we need to preserve it */
    "Rotate",
    /* Link annotations */
    "A",
    "Dest",
    "H",
    "PA",
    "QuadPoints",
    /* FreeText annotations */
    "DA",
    "Q",
    "DS",
    "CL",
    "IT",
    "BE",
    "RD",
    "BS",
    "LE",
    /* Line Annotations */
    "L",
    "LE",
    "IC",
    "LL",
    "LLE",
    "Cap",
    "LLO",
    "CP",
    "Measure",
    "CO",
    /* Square and Circle annotations */
    "Path",
    /* Polygon and PolyLine annotations */
    "Vertices",
    /* Text Markup annotations */
    /* Caret annotations */
    "Sy",
    /* Rubber Stamp annotations */
    /* Ink annotations */
    "InkList",
    /* Popup annotations */
    "Open",
    /* File attachment annotation */
    "FS",
    /* Sound annotations */
    "Sound",
    /* Movie annotations */
    "Movie",
    /* Screen annotations */
    "MK",
    "AA",
    /* We don't handle Widget annotations as annotations, we draw them */
    /* Printer's Mark annotations */
    /* Trap Network annotations */
    /* Watermark annotations */
    "FixedPrint",
    "Matrix",
    "H",
    "V",
    /* Redaction annotations */
    "RO",
    "OverlayText",
    "Repeat",
    /* Projection annotations */
    /* 3D and RichMedia annotations */
};

static int isKnownKey(pdf_context *ctx, pdf_name *Key)
{
    int i = 0;

    for (i = 0; i < sizeof(PermittedKeys) / sizeof (const char *); i++) {
        if (pdfi_name_is(Key, PermittedKeys[i]))
            return 1;
    }
    return 0;
}

static int pdfi_annot_preserve_mark(pdf_context *ctx, pdf_dict *annot, pdf_name *subtype)
{
    int code = 0;
    gs_matrix ctm;
    pdf_dict *tempdict = NULL;
    uint64_t dictsize;
    uint64_t index;
    pdf_name *Key = NULL;
    pdf_obj *Value = NULL;
    bool resolve = false;

    /* Create a temporary copy of the annot dict */
    dictsize = pdfi_dict_entries(annot);
    code = pdfi_dict_alloc(ctx, dictsize, &tempdict);
    if (code < 0) goto exit;
    pdfi_countup(tempdict);
    code = pdfi_dict_copy(ctx, tempdict, annot);
    if (code < 0) goto exit;


     /* see also: 'loadannot' in gs code */
    /* Go through the dict, deleting and modifying some entries
     * Note that I am iterating through the original dict because I will
     * be deleting some keys from tempdict and the iterators wouldn't work right.
     */
    code = pdfi_dict_key_first(ctx, annot, (pdf_obj **)&Key, &index);
    while (code >= 0) {
        resolve = false;

        if (!isKnownKey(ctx, Key)) {
            code = pdfi_dict_delete_pair(ctx, tempdict, Key);
            if (code < 0) goto exit;
        } else {
            if (pdfi_name_is(Key, "AP")) {
                /* Special handling for AP -- have fun! */
                code = pdfi_annot_preserve_modAP(ctx, tempdict, Key);
                if (code < 0) goto exit;
            } else if (pdfi_name_is(Key, "QuadPoints")) {
                code = pdfi_annot_preserve_modQP(ctx, tempdict, Key);
                if (code < 0) goto exit;
            } else if (pdfi_name_is(Key, "A")) {
                code = pdfi_pdfmark_modA(ctx, tempdict);
                if (code < 0) goto exit;
            } else if (pdfi_name_is(Key, "Dest")) {
                if (ctx->args.no_pdfmark_dests) {
                    /* If omitting dests, such as for multi-page output, then omit this whole annotation */
                    code = 0;
                    goto exit;
                }
                code = pdfi_pdfmark_modDest(ctx, tempdict);
                if (code < 0) goto exit;
            } else if (pdfi_name_is(Key, "StructTreeRoot")) {
                /* TODO: Bug691785 has Link annots with /StructTreeRoot
                 * It is super-circular, and causes issues.
                 * GS code only adds in certain values for Link so it doesn't
                 * run into a problem.  I am just going to delete it.
                 * There should be a better solution to handle circular stuff
                 * generically.
                 */
                code = pdfi_dict_delete_pair(ctx, tempdict, Key);
                if (code < 0) goto exit;
            } else if (pdfi_name_is(Key, "Sound") || pdfi_name_is(Key, "Movie")) {
                resolve = false;
            } else {
                resolve = true;
            }
        }
        if (resolve) {
            code = pdfi_dict_get_by_key(ctx, annot, (const pdf_name *)Key, &Value);
            if (code < 0) goto exit;

            /* Pre-resolve indirect references of any arrays/dicts
             * TODO: I am doing this because the gs code does it, but I
             * noticed it only goes down one level, not recursively through
             * everything.  I am not sure what the point is in that case.
             * For now, also doing only one level.
             */
            code = pdfi_resolve_indirect_loop_detect(ctx, (pdf_obj *)annot, Value, false);
            if (code < 0) goto exit;
        }

        pdfi_countdown(Key);
        Key = NULL;
        pdfi_countdown(Value);
        Value = NULL;

        code = pdfi_dict_key_next(ctx, annot, (pdf_obj **)&Key, &index);
        if (code == gs_error_undefined) {
            code = 0;
            break;
        }
    }
    if (code < 0) goto exit;

    /* Do pdfmark from the tempdict */
    gs_currentmatrix(ctx->pgs, &ctm);

    if (pdfi_name_is(subtype, "Link"))
        code = pdfi_pdfmark_from_dict(ctx, tempdict, &ctm, "LNK");
    else
        code = pdfi_pdfmark_from_dict(ctx, tempdict, &ctm, "ANN");
    if (code < 0) goto exit;

 exit:
    pdfi_countdown(tempdict);
    pdfi_countdown(Key);
    pdfi_countdown(Value);
    return code;
}

static int pdfi_annot_preserve_default(pdf_context *ctx, pdf_dict *annot, pdf_name *subtype)
{
    return pdfi_annot_preserve_mark(ctx, annot, subtype);
}

static int pdfi_annot_preserve_Link(pdf_context *ctx, pdf_dict *annot, pdf_name *subtype)
{
    int code = 0;

    /* TODO: The gs code does a whole bunch of stuff I don't understand yet.
     * I think doing the default behavior might work in most cases?
     * See: pdf_draw.ps/preserveannottypes dict/Link()
     */
    code = pdfi_annot_preserve_mark(ctx, annot, subtype);
    return code;
}

static int pdfi_annot_preserve_Widget(pdf_context *ctx, pdf_dict *annot, pdf_name *subtype)
{
    /* According to the gs implementation:
%% Widget annotations are only used with AcroForms, and since we don't preserve AcroForms
%% we don't want to preserve widget annotations either, because the consumer of the new
%% PDF won't know what values they should take. So we draw widget annotations instead. If we
%% ever preserve AcroForms then we should alter this to preserve Widgets as well.
     *
     * TODO: See also, comment on pdfi_annot_render_field().
     */

    return pdfi_annot_draw(ctx, annot, subtype);
}

/* TODO: When I started writing this, I thought these handlers were necessary because
 * the PS code has a bunch of special cases.  As it turns out, I think it could easily have
 * been done without the handlers.   Maybe they should be taken out...
 */
annot_preserve_dispatch_t annot_preserve_dispatch[] = {
    {"Link", pdfi_annot_preserve_Link},
    {"Widget", pdfi_annot_preserve_Widget},
    {"Circle", pdfi_annot_preserve_default},
    {"FileAttachment", pdfi_annot_preserve_default},
    {"FreeText", pdfi_annot_preserve_default},
    {"Highlight", pdfi_annot_preserve_default},
    {"Ink", pdfi_annot_preserve_default},
    {"Line", pdfi_annot_preserve_default},
    {"Movie", pdfi_annot_preserve_default},
    {"PolyLine", pdfi_annot_preserve_default},
    {"Popup", pdfi_annot_preserve_default},
    //    {"Screen", pdfi_annot_preserve_default},  /* TODO: fts_07_0709.pdf */
    {"Sound", pdfi_annot_preserve_default},
    {"Square", pdfi_annot_preserve_default},
    {"Squiggly", pdfi_annot_preserve_default},
    {"StrikeOut", pdfi_annot_preserve_default},
    {"Underline", pdfi_annot_preserve_default},
    {"Stamp", pdfi_annot_preserve_default},
    {"Text", pdfi_annot_preserve_default},
    {"TrapNet", pdfi_annot_preserve_default},
    { NULL, NULL},
};

static int pdfi_annot_preserve(pdf_context *ctx, pdf_dict *annot, pdf_name *subtype)
{
    int code = 0;
    annot_preserve_dispatch_t *dispatch_ptr;

    /* If not preserving this subtype, draw it instead */
    if (!pdfi_annot_preserve_type(ctx, subtype))
        return pdfi_annot_draw(ctx, annot, subtype);

    /* Handle the annotation */
    for (dispatch_ptr = annot_preserve_dispatch; dispatch_ptr->subtype; dispatch_ptr ++) {
        if (pdfi_name_is(subtype, dispatch_ptr->subtype)) {
            code = dispatch_ptr->func(ctx, annot, subtype);
            break;
        }
    }

    /* If there is no handler, just draw it */
    /* NOTE: gs does a drawwidget here instead (?) */
    if (!dispatch_ptr->subtype)
        code = pdfi_annot_draw(ctx, annot, subtype);

    return code;
}

static int pdfi_annot_handle(pdf_context *ctx, pdf_dict *annot)
{
    int code = 0;
    pdf_name *Subtype = NULL;

    code = pdfi_dict_get_type(ctx, annot, "Subtype", PDF_NAME, (pdf_obj **)&Subtype);
    if (code != 0) {
        /* TODO: Set warning flag */
        dbgmprintf(ctx->memory, "WARNING: Ignoring annotation with missing Subtype\n");
        code = 0;
        goto exit;
    }

    if (ctx->args.preserveannots && ctx->device_state.annotations_preserved)
        code = pdfi_annot_preserve(ctx, annot, Subtype);
    else
        code = pdfi_annot_draw(ctx, annot, Subtype);

 exit:
    pdfi_countdown(Subtype);
    if (code < 0) {
        dbgmprintf(ctx->memory, "WARNING: Error handling annotation\n");
        pdfi_set_error(ctx, code, NULL, E_PDF_BAD_ANNOTATION, "pdfi_annot_handle", "");
        if (!ctx->args.pdfstoponerror)
            code = 0;
    }
    return code;
}

int pdfi_do_annotations(pdf_context *ctx, pdf_dict *page_dict)
{
    int code = 0;
    pdf_array *Annots = NULL;
    pdf_dict *annot = NULL;
    int i;

    if (!ctx->args.showannots)
        return 0;

    code = pdfi_dict_knownget_type(ctx, page_dict, "Annots", PDF_ARRAY, (pdf_obj **)&Annots);
    if (code <= 0)
        return code;

    for (i = 0; i < pdfi_array_size(Annots); i++) {
        code = pdfi_array_get_type(ctx, Annots, i, PDF_DICT, (pdf_obj **)&annot);
        if (code < 0) {
            if (code == gs_error_typecheck)
                pdfi_set_warning(ctx, 0, NULL, W_PDF_ANNOT_BAD_TYPE, "pdfi_do_annotations", "");
            continue;
        }
        code = pdfi_annot_handle(ctx, annot);
        if (code < 0 && ctx->args.pdfstoponerror)
            goto exit;
        pdfi_countdown(annot);
        annot = NULL;
    }

 exit:
    pdfi_countdown(annot);
    pdfi_countdown(Annots);
    return code;
}

/* draw terminal field */
static int pdfi_form_draw_terminal(pdf_context *ctx, pdf_dict *Page, pdf_dict *field)
{
    int code = 0;
    pdf_indirect_ref *P = NULL;
    pdf_name *FT = NULL;
    pdf_obj *AP = NULL;

    /* See if the field goes on this page */
    /* NOTE: We know the "P" is an indirect ref, so just fetch it that way.
     * If we fetch the actual object, it will result in a cyclical reference in the cache
     * that causes a memory leak, so don't do that.
     * (The cyclical reference is because the object containing P is actually inside P)
     */
    code = pdfi_dict_get_ref(ctx, field, "P", &P);
    if (code < 0) {
        if (code == gs_error_undefined)
            code = 0;
        goto exit;
    }

    if (P->ref_object_num != Page->object_num) {
        /* Not this page */
        code = 0;
        goto exit;
    }

    /* Render the field */

    /* NOTE: The spec says the FT is inheritable, implying it might be present in
     * a Parent and not explicitly in a Child.  The gs implementation doesn't seem
     * to handle this case, but I am going to go ahead and handle it.  We will figure
     * out if this causes diffs later, if ever...
     */
    code = pdfi_form_get_inheritable(ctx, field, "FT", PDF_NAME, (pdf_obj **)&FT);
    if (code <= 0) goto exit;

    code = pdfi_annot_get_NormAP(ctx, field, &AP);
    if (code < 0) goto exit;

    code = pdfi_annot_render_field(ctx, field, FT, AP);
    if (code < 0) goto exit;

 exit:
    pdfi_countdown(FT);
    pdfi_countdown(P);
    pdfi_countdown(AP);
    return code;
}

/* From pdf_draw.ps/draw_form_field():
% We distinguish 4 types of nodes on the form field tree:
%  - non-terminal field - has a kid that refers to the parent (or anywhere else)
%  - terminal field with separate widget annotations - has a kid that doesn't have a parent
%  - terminal field with a merged widget annotation - has no kids
%  - widget annotation - has /Subtype and /Rect
%
% The recursive enumeration of the form fields doesn't descend into widget annotations.
*/
static int pdfi_form_draw_field(pdf_context *ctx, pdf_dict *Page, pdf_dict *field)
{
    int code = 0;
    pdf_array *Kids = NULL;
    pdf_dict *child = NULL;
    pdf_dict *Parent = NULL;
    int i;

    code = pdfi_dict_knownget_type(ctx, field, "Kids", PDF_ARRAY, (pdf_obj **)&Kids);
    if (code < 0) goto exit;
    if (code == 0) {
        code = pdfi_form_draw_terminal(ctx, Page, field);
        goto exit;
    }

    /* Handle Kids */
    if (pdfi_array_size(Kids) <= 0) {
        dmprintf(ctx->memory, "*** Error: Ignoring empty /Kids array in Form field.\n");
        dmprintf(ctx->memory, "    Output may be incorrect.\n");
        /* TODO: Set warning flag */
        code = 0;
        goto exit;
    }

    /* Check first child to see if it has a parent */
    code = pdfi_array_get_type(ctx, Kids, 0, PDF_DICT, (pdf_obj **)&child);
    if (code < 0) goto exit;
    code = pdfi_dict_knownget_type(ctx, child, "Parent", PDF_DICT, (pdf_obj **)&Parent);
    if (code < 0) goto exit;

    /* If kid has no parent, then treat this as terminal field */
    if (code == 0) {
        /* TODO: This case isn't tested because no examples available.
         * I think it's only relevant for Btn, and not sure if it
         * should be dealt with here or in Btn routine.
         */
        code = pdfi_form_draw_terminal(ctx, Page, field);
        goto exit;
    }

    pdfi_countdown(child);
    child = NULL;
    /* Render the Kids (recursive) */
    for (i=0; i<pdfi_array_size(Kids); i++) {
        code = pdfi_array_get_type(ctx, Kids, i, PDF_DICT, (pdf_obj **)&child);
        if (code < 0) goto exit;

        code = pdfi_form_draw_field(ctx, Page, child);
        if (code < 0) goto exit;

        pdfi_countdown(child);
        child = NULL;
    }

 exit:
    pdfi_countdown(child);
    pdfi_countdown(Kids);
    pdfi_countdown(Parent);
    return code;
}

int pdfi_do_acroform(pdf_context *ctx, pdf_dict *page_dict)
{
    int code = 0;
    pdf_array *Fields = NULL;
    pdf_dict *field = NULL;
    int i;

    if (!ctx->args.showacroform)
        return 0;

    if (!ctx->AcroForm)
        return 0;

    code = pdfi_dict_knownget_type(ctx, ctx->AcroForm, "Fields", PDF_ARRAY, (pdf_obj **)&Fields);
    if (code <= 0)
        goto exit;

    for (i=0; i<pdfi_array_size(Fields); i++) {
        code = pdfi_array_get_type(ctx, Fields, i, PDF_DICT, (pdf_obj **)&field);
        if (code < 0)
            continue;
        code = pdfi_form_draw_field(ctx, page_dict, field);
        if (code < 0)
            goto exit;
        pdfi_countdown(field);
        field = NULL;
    }

 exit:
    pdfi_countdown(field);
    pdfi_countdown(Fields);
    return code;
}
