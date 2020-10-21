/* Copyright (C) 2001-2020 Artifex Software, Inc.
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

/* Annotation handling for the PDF interpreter */

#include "pdf_int.h"
#include "plmain.h"
#include "pdf_stack.h"
#include "pdf_annot.h"
#include "pdf_file.h"
#include "pdf_dict.h"
#include "pdf_array.h"
#include "pdf_loop_detect.h"
#include "pdf_colour.h"
#include "pdf_trans.h"
#include "pdf_gstate.h"
#include "pdf_misc.h"
#include "pdf_optcontent.h"
#include "pdf_annot.h"
#include "pdf_colour.h"
#include "pdf_deref.h"
#include "gspath2.h"
#include "pdf_image.h"
#include "gxfarith.h"
#include "pdf_mark.h"

typedef int (*annot_func)(pdf_context *ctx, pdf_dict *annot, pdf_dict *NormAP, bool *render_done);

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

    if (!ctx->page_has_transparency)
        return 0;

    /* TODO: ps code does something with /BM which is apparently the Blend Mode,
     * but I am not sure that the value is actually used properly.  Look into it.
     * (see pdf_draw.ps/startannottransparency)
     */
    code = gs_clippath(ctx->pgs);
    if (code < 0)
        return code;
    code = pdfi_trans_begin_simple_group(ctx, false, false, false);
    (void)gs_newpath(ctx->pgs);
    return code;
}

static int pdfi_annot_end_transparency(pdf_context *ctx, pdf_dict *annot)
{
    if (!ctx->page_has_transparency)
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

/* See pdf_draw.ps/drawwidget (draws the AP for any type of thingy) */
static int pdfi_annot_draw_AP(pdf_context *ctx, pdf_dict *annot, pdf_dict *NormAP)
{
    int code = 0;
    gs_rect rect;
    pdf_array *BBox = NULL;
    gs_rect bbox;
    pdf_array *Matrix = NULL;
    gs_matrix matrix;
    double xscale, yscale;

    if (NormAP == NULL)
        return 0;

    code = pdfi_op_q(ctx);
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
    code = gs_initgraphics(ctx->pgs);
    ctx->TextBlockDepth = 0;
    /* TODO: FIXME */

    code = pdfi_annot_Rect(ctx, annot, &rect);
    if (code < 0) goto exit;

    code = pdfi_dict_knownget_type(ctx, NormAP, "BBox", PDF_ARRAY, (pdf_obj **)&BBox);
    if (code < 0) goto exit;
    code = pdfi_array_to_gs_rect(ctx, BBox, &bbox);
    if (code < 0) goto exit;

    code = pdfi_dict_knownget_type(ctx, NormAP, "Matrix", PDF_ARRAY, (pdf_obj **)&Matrix);
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
            code = 0;
            goto exit;
        }

        /* Scale it */
        code = gs_scale(ctx->pgs, xscale, yscale);
        if (code < 0) goto exit;

        /* Compensate for non-zero origin of BBox */
        code = gs_translate(ctx->pgs, -bbox.p.x, -bbox.p.y);
        if (code < 0) goto exit;
    }

    /* Render the annotation */
    code = pdfi_do_image_or_form(ctx, NULL, ctx->CurrentPageDict, NormAP);
    if (code < 0) goto exit;

 exit:
    (void)pdfi_op_Q(ctx);
    pdfi_countdown(BBox);
    pdfi_countdown(Matrix);
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
        /* TODO: set warning flag */
        dbgmprintf(ctx->memory, "WARNING: Annotation Border array invalid\n");
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
                /* TODO: set warning flag */
                dbgmprintf(ctx->memory, "WARNING: Annotation Border Dash array invalid\n");
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

            code = pdfi_array_put_int(ctx, dash, 0, 3);
            if (code < 0) goto exit;
        }
    } else {
        /* Empty array */
        code = pdfi_array_alloc(ctx, 0, &dash);
        pdfi_countup(dash);
        if (code < 0) goto exit;
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
        memcpy(str, (const char *)LE->data, LE->length);
        str[LE->length] = '\0';
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

    code = pdfi_dict_knownget(ctx, annot, "LE", (pdf_obj **)&LE);
    if (code <= 0)
        goto exit;
    if (LE->type != PDF_ARRAY && LE->type != PDF_NAME) {
        code = gs_note_error(gs_error_typecheck);
        goto exit;
    }

    dx = x2 - x1;
    dy = y2 - y1;
    code = gs_atan2_degrees(dy, dx, &angle);
    if (code < 0)
        angle = 0;

    if (LE->type == PDF_ARRAY) {
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
static int pdfi_annot_get_NormAP(pdf_context *ctx, pdf_dict *annot, pdf_dict **NormAP)
{
    int code;
    pdf_dict *AP = NULL;
    pdf_dict *baseAP = NULL;
    pdf_name *AS = NULL;

    *NormAP = NULL;

    code = pdfi_dict_knownget_type(ctx, annot, "AP", PDF_DICT, (pdf_obj **)&AP);
    if (code <= 0) goto exit;

    code = pdfi_dict_knownget_type(ctx, AP, "N", PDF_DICT, (pdf_obj **)&baseAP);
    if (code <= 0) goto exit;

    code = 0;

    if (pdfi_dict_is_stream(ctx, baseAP)) {
        /* Use baseAP for the AP, and get all the refcnt's right */
        pdfi_countdown(AP);
        AP = baseAP;
        baseAP = NULL;
    } else {
        code = pdfi_dict_knownget_type(ctx, annot, "AS", PDF_NAME, (pdf_obj **)&AS);
        if (code < 0) goto exit;
        if (code == 0) {
            dbgmprintf(ctx->memory, "WARNING Annotation has non-stream AP but no AS.  Don't know what to render. Skipping\n");
            goto exit;
        }

        pdfi_countdown(AP);
        AP = NULL;
        /* Lookup the AS in the NormAP and use that as the AP */
        code = pdfi_dict_get_by_key(ctx, baseAP, AS, (pdf_obj **)&AP);
        if (code < 0) goto exit;
        if (AP->type != PDF_DICT) {
            code = gs_note_error(gs_error_typecheck);
            goto exit;
        }
    }

   *NormAP = AP;
   pdfi_countup(AP);

 exit:
    pdfi_countdown(AP);
    pdfi_countdown(AS);
    pdfi_countdown(baseAP);
    return code;
}

static int pdfi_annot_draw_Link(pdf_context *ctx, pdf_dict *annot, pdf_dict *NormAP, bool *render_done)
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
    if (code < 0) code = code1;

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
    double x0, y0, x1, y1, x2, y2, x3, y3, xc1, yc1, xc2, yc2, xc3, yc3;
    double len1, len2, len3, k1, k2, xm1, ym1, xm2, ym2;
    double ctrl1_x, ctrl1_y, ctrl2_x, ctrl2_y;
    const double smooth_value = 1; /* from 0..1 range */

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

        code = pdfi_array_get_number(ctx, points, 2, &x2);
        if (code < 0) goto exit;
        code = pdfi_array_get_number(ctx, points, 3, &y2);
        if (code < 0) goto exit;
        if (num_points == 4) {
            code = gs_lineto(ctx->pgs, x2, y2);
            goto stroke;
        }

        x0 = 2*x1 - x2;
        y0 = 2*y1 - y2;

        for (j = 4; j <= num_points; j += 2) {
            if (j < num_points) {
                code = pdfi_array_get_number(ctx, points, j, &x3);
                if (code < 0) goto exit;
                code = pdfi_array_get_number(ctx, points, j+1, &y3);
                if (code < 0) goto exit;
            } else {
                x3 = 2*x2 - x1;
                y3 = 2*y2 - y1;
            }

            xc1 = (x0 + x1) / 2.0;
            yc1 = (y0 + y1) / 2.0;
            xc2 = (x1 + x2) / 2.0;
            yc2 = (y1 + y2) / 2.0;
            xc3 = (x2 + x3) / 2.0;
            yc3 = (y2 + y3) / 2.0;

            len1 = hypot(x1 - x0, y1 - y0);
            len2 = hypot(x2 - x1, y2 - y1);
            len3 = hypot(x3 - x2, y3 - y2);

            k1 = len1 / (len1 + len2);
            k2 = len2 / (len2 + len3);

            xm1 = xc1 + (xc2 - xc1) * k1;
            ym1 = yc1 + (yc2 - yc1) * k1;

            xm2 = xc2 + (xc3 - xc2) * k2;
            ym2 = yc2 + (yc3 - yc2) * k2;

            ctrl1_x = xm1 + (xc2 - xm1) * smooth_value + x1 - xm1;
            ctrl1_y = ym1 + (yc2 - ym1) * smooth_value + y1 - ym1;

            ctrl2_x = xm2 + (xc2 - xm2) * smooth_value + x2 - xm2;
            ctrl2_y = ym2 + (yc2 - ym2) * smooth_value + y2 - ym2;

            code = gs_curveto(ctx->pgs, ctrl1_x, ctrl1_y, ctrl2_x, ctrl2_y, x2, y2);
            if (code < 0) goto exit;

            x0 = x1, x1 = x2, x2 = x3;
            y0 = y1, y1 = y2, y2 = y3;
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
static int pdfi_annot_draw_Ink(pdf_context *ctx, pdf_dict *annot, pdf_dict *NormAP, bool *render_done)
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
static int pdfi_annot_draw_Circle(pdf_context *ctx, pdf_dict *annot, pdf_dict *NormAP, bool *render_done)
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
static int pdfi_annot_draw_stamp_text(pdf_context *ctx, pdfi_annot_stamp_text_t *text)
{
    if (!text->text)
        return 0;

    /* TODO: Figure this out later (see pdf_draw.ps/text) */
    return 0;
}

/* Generate appearance (see pdf_draw.ps/Stamp)
 *
 * If there was an AP it was already handled.
 * For testing, see tests_private/pdf/PDF_1.7_FTS/fts_32_3204.pdf
 */
static int pdfi_annot_draw_Stamp(pdf_context *ctx, pdf_dict *annot, pdf_dict *NormAP, bool *render_done)
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
    if (!stamp_type->type) {
        stamp_type = &pdfi_annot_stamp_types[STAMP_DRAFT_INDEX];
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
        code = pdfi_annot_draw_stamp_text(ctx, &stamp_type->text[i]);
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

/* Process the /DA string by either running it through the interpreter
 *  or providing a bunch of defaults
 */
static int pdfi_annot_process_DA(pdf_context *ctx, pdf_dict *annot)
{
    int code = 0;
    pdf_string *DA = NULL;

    code = pdfi_dict_knownget_type(ctx, annot, "DA", PDF_STRING, (pdf_obj **)&DA);
    if (code < 0) goto exit;
    if (code > 0) {
        code = pdfi_interpret_inner_content_string(ctx, DA, annot,
                                                   ctx->CurrentPageDict, false, "DA");
        if (code < 0) goto exit;
    } else {
        /* TODO: Setup defaults if there is no DA */
        code = pdfi_gs_setgray(ctx, 0);
        if (code < 0) goto exit;
    }

 exit:
    pdfi_countdown(DA);
    return code;
}

/* FreeText -- Draw text with border around it.
 *
 * See pdf_draw.ps/FreeText
 * If there was an AP it was already handled
 */
static int pdfi_annot_draw_FreeText(pdf_context *ctx, pdf_dict *annot, pdf_dict *NormAP, bool *render_done)
{
    int code = 0;
    int code1 = 0;
    bool drawbackground;
    pdf_dict *BS = NULL;

    *render_done = false;

    code = pdfi_annot_start_transparency(ctx, annot);
    if (code < 0) goto exit1;

    code = pdfi_annot_opacity(ctx, annot);
    if (code < 0) goto exit;

    /* Set the (background?) color if applicable */
    code = pdfi_annot_setcolor(ctx, annot, false, &drawbackground);
    if (code < 0) goto exit;

    /* Only draw rectangle if a color was specified */
    if (drawbackground) {
        code = pdfi_annot_fillRect(ctx, annot);
        if (code < 0) goto exit;
    }

    code = pdfi_annot_process_DA(ctx, annot);
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

    /* TODO: /Rotate */
    /* TODO: /Contents */

    code = pdfi_annot_draw_CL(ctx, annot);
    if (code < 0) goto exit;

 exit:
    code1 = pdfi_annot_end_transparency(ctx, annot);
    if (code >= 0)
        code = code1;
 exit1:
    *render_done = true;
    pdfi_countdown(BS);
    return code;
}

/* Generate appearance (see pdf_draw.ps/Text)
 * This draws the default "Note" thingy which looks like a little page.
 * TODO: Spec lists a whole bunch of different /Name with different appearances that we don't handle.
 * See page 1 of tests_private/pdf/sumatra/annotations_without_appearance_streams.pdf
 *
 * If there was an AP it was already handled.
 */
static int pdfi_annot_draw_Text(pdf_context *ctx, pdf_dict *annot, pdf_dict *NormAP, bool *render_done)
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

    code = gs_setlinecap(ctx->pgs, 1);
    if (code < 0) goto exit;

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
static int pdfi_annot_draw_StrikeOut(pdf_context *ctx, pdf_dict *annot, pdf_dict *NormAP, bool *render_done)
{
    *render_done = true;
    return pdfi_annot_draw_line_offset(ctx, annot, 3/7.);
}

/* Generate appearance (see pdf_draw.ps/Underline)
 *
 * If there was an AP it was already handled.
 */
static int pdfi_annot_draw_Underline(pdf_context *ctx, pdf_dict *annot, pdf_dict *NormAP, bool *render_done)
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
static int pdfi_annot_draw_Highlight(pdf_context *ctx, pdf_dict *annot, pdf_dict *NormAP, bool *render_done)
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

        if (ctx->page_has_transparency) {
            code = pdfi_trans_begin_simple_group(ctx, false, false, false);
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
static int pdfi_annot_draw_Squiggly(pdf_context *ctx, pdf_dict *annot, pdf_dict *NormAP, bool *render_done)
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
static int pdfi_annot_draw_Redact(pdf_context *ctx, pdf_dict *annot, pdf_dict *NormAP, bool *render_done)
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

/* Display a string */
static int
pdfi_annot_display_text(pdf_context *ctx, double size, const char *font,
                            double x, double y, pdf_string *text)
{
    /* TODO: Implement this.  See pdf_draw.ps/Popup */
    dbgmprintf(ctx->memory, "ANNOT: Rendering of text not implemented\n");
    return 0;
}

/* Handle PopUp (see pdf_draw.ps/Popup)
 * Renders only if /Open=true
 *
 * If there's an AP, caller will handle it on return
 */
static int pdfi_annot_draw_Popup(pdf_context *ctx, pdf_dict *annot, pdf_dict *NormAP, bool *render_done)
{
    int code = 0, code1 = 0;
    bool Open = false;
    pdf_dict *Parent = NULL;
    pdf_string *Contents = NULL;
    pdf_string *T = NULL;
    bool has_color;
    gs_rect rect, rect2;
    bool need_grestore = false;
    double x, y;

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
        x = rect.p.x + 5;
        y = rect.q.y - 30;
        code = pdfi_annot_display_text(ctx, 9, "Helvetica", x, y, Contents);
        if (code < 0) goto exit;
    }

    /* Draw small, thin box (the frame around the top /T text) */
    rect2.p.x = rect.p.x;
    rect2.p.y = rect.q.y - 15;
    rect2.q.x = rect.q.x;
    rect2.q.y = rect.p.y + (rect.q.y - rect.p.y);

    gs_rectfill(ctx->pgs, &rect2, 1);
    if (code < 0) goto exit;
    pdfi_gs_setgray(ctx, 0);
    if (code < 0) goto exit;
    gs_rectstroke(ctx->pgs, &rect2, 1, NULL);
    if (code < 0) goto exit;

    /* Display /T in Helvetica */
    code = pdfi_dict_knownget_type(ctx, Parent, "T", PDF_STRING, (pdf_obj **)&T);
    if (code < 0) goto exit;
    if (code > 0) {
        x = rect.p.x + 2; /* TODO: Center it based on stringwidth */
        y = rect.q.y - 11;
        code = pdfi_annot_display_text(ctx, 9, "Helvetica", x, y, T);
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
static int pdfi_annot_draw_Line(pdf_context *ctx, pdf_dict *annot, pdf_dict *NormAP, bool *render_done)
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
static int pdfi_annot_draw_PolyLine(pdf_context *ctx, pdf_dict *annot, pdf_dict *NormAP, bool *render_done)
{
    int code = 0;
    int code1 = 0;
    pdf_array *Vertices = NULL;
    bool drawit;
    int size;
    double x1, y1, x2, y2;

    code = pdfi_annot_start_transparency(ctx, annot);
    if (code < 0) goto exit1;

    code = pdfi_dict_knownget_type(ctx, annot, "Vertices", PDF_ARRAY, (pdf_obj **)&Vertices);
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

 exit:
    code1 = pdfi_annot_end_transparency(ctx, annot);
    if (code >= 0)
        code = code1;

 exit1:
    *render_done = true;
    pdfi_countdown(Vertices);
    return code;
}

/* Generate appearance (see pdf_draw.ps/Polygon)
 *
 * If there was an AP it was already handled.
 */
static int pdfi_annot_draw_Polygon(pdf_context *ctx, pdf_dict *annot, pdf_dict *NormAP, bool *render_done)
{
    int code = 0;
    int code1 = 0;
    pdf_array *Vertices = NULL;
    bool drawit;

    code = pdfi_annot_start_transparency(ctx, annot);
    if (code < 0) goto exit1;

    code = pdfi_dict_knownget_type(ctx, annot, "Vertices", PDF_ARRAY, (pdf_obj **)&Vertices);
    if (code < 0) goto exit;

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
static int pdfi_annot_draw_Square(pdf_context *ctx, pdf_dict *annot, pdf_dict *NormAP, bool *render_done)
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

/* Render Widget with no AP
 */
static int pdfi_annot_render_Widget(pdf_context *ctx, pdf_dict *annot)
{
    int code = 0;
    pdf_dict *MK = NULL;
    bool drawit = false;
    gs_rect rect;
    bool need_grestore = false;

    code = pdfi_dict_knownget_type(ctx, annot, "MK", PDF_DICT, (pdf_obj **)&MK);
    /* Don't try to render if no MK dict */
    if (code <= 0) goto exit;

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

    pdfi_gsave(ctx);
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
    pdfi_countdown(MK);
    return code;
}

/* Draws a thing of type /Widget */
static int pdfi_annot_draw_Widget(pdf_context *ctx, pdf_dict *annot, pdf_dict *NormAP, bool *render_done)
{
    int code = 0;
    bool found_T = false;
    bool found_TF = false;
    pdf_obj *T = NULL;
    pdf_obj *TF = NULL;
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
    currdict = annot;
    pdfi_countup(currdict);
    while (true) {
        code = pdfi_dict_knownget(ctx, currdict, "T", &T);
        if (code < 0) goto exit;
        if (code > 0) {
            found_T = true;
            break;
        }
        code = pdfi_dict_knownget(ctx, currdict, "TF", &TF);
        if (code < 0) goto exit;
        if (code > 0) {
            found_TF = true;
            break;
        }
        /* Check for Parent */
        code = pdfi_dict_knownget_type(ctx, currdict, "Parent", PDF_DICT, (pdf_obj **)&Parent);
        if (code < 0) goto exit;
        if (code == 0)
            break;
        pdfi_countdown(currdict);
        currdict = Parent;
        pdfi_countup(currdict);
    }

    code = 0;
    if (!found_T && !found_TF) {
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
    pdfi_countdown(T);
    pdfi_countdown(TF);
    pdfi_countdown(Parent);
    pdfi_countdown(currdict);
    return code;
}

/* Handle Annotations that are not implemented */
static int pdfi_annot_draw_NotImplemented(pdf_context *ctx, pdf_dict *annot, pdf_dict *NormAP, bool *render_done)
{
    int code = 0;
    pdf_name *Subtype = NULL;
    char str[100];

    code = pdfi_dict_get_type(ctx, annot, "Subtype", PDF_NAME, (pdf_obj **)&Subtype);
    if (code < 0) goto exit;

    memcpy(str, (const char *)Subtype->data, Subtype->length);
    str[Subtype->length] = '\0';
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

    if (ctx->printed) {
        /* Even if Print flag (bit 3) is off, will print if 3D */
        is_visible = ((F & 0x4) != 0) || is_3D;
    } else {
        /* Not NoView (bit 7) */
        is_visible = (F & 0x80) == 0;
    }

 exit:
    return is_visible;
}

/* Check to see if subtype is on the ShowAnnotTypes list
 * Defaults to true if there is no list
 */
static bool pdfi_annot_check_type(pdf_context *ctx, pdf_name *subtype)
{
    /* TODO: Need to implement */
    return true;
}

/* Check to see if subtype is on the PreserveAnnotTypes list
 * Defaults to true if there is no list
 */
static bool pdfi_annot_preserve_type(pdf_context *ctx, pdf_name *subtype)
{
    /* TODO: Need to implement */
    return true;
}

static int pdfi_annot_draw(pdf_context *ctx, pdf_dict *annot, pdf_name *subtype)
{
    pdf_dict *NormAP = NULL;
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
        memcpy(str, (const char *)subtype->data, subtype->length);
        str[subtype->length] = '\0';
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
    int size = 30;
    char *buf;

    buf = (char *)gs_alloc_bytes(ctx->memory, size, "pdfi_annot_preserve_nextformlabel(buf)");
    if (buf == NULL)
        return_error(gs_error_VMerror);
    snprintf(buf, size, "{FormName%d}", ctx->pdfwrite_form_counter++);
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

    if (QP->type != PDF_ARRAY) {
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
    pdf_obj *Value = NULL;
    byte *labeldata = NULL;
    int labellen;
    int form_id;
    pdf_indirect_ref *ref = NULL;
    gx_device *device = gs_currentdevice(ctx->pgs);

    code = pdfi_dict_get(ctx, annot, "AP", (pdf_obj **)&AP);
    if (code < 0) goto exit;

    if (AP->type != PDF_DICT) {
        /* Invalid AP, just delete it because I dunno what to do...
         * TODO: Should flag a warning here
         */
        code = pdfi_dict_delete_pair(ctx, annot, AP_key);
        goto exit;
    }

    code = pdfi_dict_first(ctx, AP, (pdf_obj **)&Key, &Value, &index);
    while (code >= 0) {
        if (Value->type == PDF_DICT) {
            /* Get a form label */
            code = pdfi_annot_preserve_nextformlabel(ctx, &labeldata, &labellen);
            if (code < 0) goto exit;

            /* Notify the driver of the label name */
            code = (*dev_proc(device, dev_spec_op))
                (device, gxdso_pdf_form_name, labeldata, labellen);

            /* Draw the high-level form */
            code = pdfi_do_highlevel_form(ctx, ctx->CurrentPageDict, (pdf_dict *)Value);
            if (code < 0) goto exit;

            /* Get the object number (form_id) of the high level form */
            code = (*dev_proc(device, dev_spec_op))
                (device, gxdso_get_form_ID, &form_id, sizeof(int));

            /* Create an indirect ref to form object */
            code = pdfi_alloc_object(ctx, PDF_INDIRECT, 0, (pdf_obj **)&ref);
            if (code < 0) goto exit;
            ref->ref_object_num = form_id;
            ref->ref_generation_num = 0;
            ref->is_label = true;

            /* Put it in the dict */
            code = pdfi_dict_put_obj(ctx, AP, (pdf_obj *)Key, (pdf_obj *)ref);
            if (code < 0) goto exit;
        }

        pdfi_countdown(Key);
        Key = NULL;
        pdfi_countdown(Value);
        Value = NULL;
        gs_free_object(ctx->memory, labeldata, "pdfi_annot_preserve_modAP(labeldata)");
        labeldata = NULL;

        code = pdfi_dict_next(ctx, AP, (pdf_obj **)&Key, &Value, &index);
        if (code == gs_error_undefined) {
            code = 0;
            break;
        }
    }
    if (code < 0) goto exit;

 exit:
    if (labeldata)
        gs_free_object(ctx->memory, labeldata, "pdfi_annot_preserve_modAP(labeldata)");
    pdfi_countdown(AP);
    pdfi_countdown(Key);
    pdfi_countdown(Value);
    return code;
}

/* Make a temporary copy of the annotation dict with some fields left out or
 * modified, then do a pdfmark on it
 */
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
    code = pdfi_alloc_object(ctx, PDF_DICT, dictsize, (pdf_obj **)&tempdict);
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

        if (pdfi_name_is(Key, "Popup") || pdfi_name_is(Key, "IRT") || pdfi_name_is(Key, "RT") ||
            pdfi_name_is(Key, "P") || pdfi_name_is(Key, "Parent")) {
            /* Delete some keys
             * These would not be handled correctly and are optional.
             * (see pdf_draw.ps/loadannot())
             * TODO: Could probably handle some of these since they are typically
             * just references, and we do have a way to handle references?
             * Look into it later...
             */
            code = pdfi_dict_delete_pair(ctx, tempdict, Key);
            if (code < 0) goto exit;
        } else if (pdfi_name_is(Key, "AP")) {
            /* Special handling for AP -- have fun! */
            code = pdfi_annot_preserve_modAP(ctx, tempdict, Key);
            if (code < 0) goto exit;
        } else if (pdfi_name_is(Key, "QuadPoints")) {
            code = pdfi_annot_preserve_modQP(ctx, tempdict, Key);
            if (code < 0) goto exit;
        } else if (pdfi_name_is(Key, "A")) {
            code = pdfi_mark_modA(ctx, tempdict, Key, subtype, &resolve);
            if (code < 0) goto exit;
        } else if (pdfi_name_is(Key, "Dest")) {
            if (ctx->no_pdfmark_dests) {
                /* If omitting dests, such as for multi-page output, then omit this whole annotation */
                code = 0;
                goto exit;
            }
            code = pdfi_mark_modDest(ctx, tempdict, Key, subtype);
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
        } else {
            resolve = true;
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
            code = pdfi_loop_detector_mark(ctx);
            code = pdfi_loop_detector_add_object(ctx, annot->object_num);
            if (Value->object_num != 0)
                code = pdfi_loop_detector_add_object(ctx, Value->object_num);
            code = pdfi_resolve_indirect(ctx, Value, false);
            if (code < 0) goto exit;
            (void)pdfi_loop_detector_cleartomark(ctx); /* Clear to the mark for the current loop */
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

    /* TODO: Loop detection no longer necessary here, since we took care of it above.
     *
     * (but there is no harm in playing it safe?)
     */
    code = pdfi_loop_detector_mark(ctx);
    code = pdfi_loop_detector_add_object(ctx, annot->object_num);
    if (pdfi_name_is(subtype, "Link"))
        code = pdfi_mark_from_dict(ctx, tempdict, &ctm, "LNK");
    else
        code = pdfi_mark_from_dict(ctx, tempdict, &ctm, "ANN");
    (void)pdfi_loop_detector_cleartomark(ctx); /* Clear to the mark for the current loop */
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

    if (ctx->preserveannots && ctx->annotations_preserved)
        code = pdfi_annot_preserve(ctx, annot, Subtype);
    else
        code = pdfi_annot_draw(ctx, annot, Subtype);

 exit:
    pdfi_countdown(Subtype);
    return code;
}

int pdfi_do_annotations(pdf_context *ctx, pdf_dict *page_dict)
{
    int code = 0;
    pdf_array *Annots = NULL;
    pdf_dict *annot = NULL;
    int i;

    if (!ctx->showannots)
        return 0;

    code = pdfi_dict_knownget_type(ctx, page_dict, "Annots", PDF_ARRAY, (pdf_obj **)&Annots);
    if (code <= 0)
        return code;

    for (i = 0; i < pdfi_array_size(Annots); i++) {
        code = pdfi_array_get_type(ctx, Annots, i, PDF_DICT, (pdf_obj **)&annot);
        if (code < 0)
            continue;
        code = pdfi_annot_handle(ctx, annot);
        if (code < 0)
            goto exit;
        pdfi_countdown(annot);
        annot = NULL;
    }

 exit:
    pdfi_countdown(annot);
    pdfi_countdown(Annots);
    return code;
}

/* Get value from a field, or its Parents (recursive)
 * Returns <0 on error, 0 if not found, >0 if found
 */
static int pdfi_form_get_inheritable(pdf_context *ctx, pdf_dict *field, const char *Key, pdf_obj_type type, pdf_obj **o)
{
    int code = 0;
    pdf_dict *Parent = NULL;

    /* Check this field */
    code = pdfi_dict_knownget_type(ctx, field, Key, type, o);
    if (code != 0) goto exit;

    /* If not found, recursively check Parent, if any */
    code = pdfi_dict_knownget_type(ctx, field, "Parent", PDF_DICT, (pdf_obj **)&Parent);
    if (code <= 0) goto exit;
    code = pdfi_form_get_inheritable(ctx, Parent, Key, type, o);

 exit:
    pdfi_countdown(Parent);
    return code;
}

/* draw field Btn */
static int pdfi_form_draw_Btn(pdf_context *ctx, pdf_dict *AcroForm, pdf_dict *field, pdf_dict *AP)
{
    int code;
    bool Radio = false;
    bool Pushbutton = false;
    pdf_num *Ff = NULL;
    int64_t value;

    /* TODO: There can be Kids array. I think it should be handled in caller.
     * Need an example...
     */
    if (AP != NULL) {
        code = pdfi_annot_draw_AP(ctx, field, AP);
        goto exit;
    }

    /* Get Ff field (default is 0) */
    code = pdfi_form_get_inheritable(ctx, field, "Ff", PDF_INT, (pdf_obj **)&Ff);
    if (code < 0) goto exit;

    if (code == 0)
        value = 0;
    else
        value = Ff->value.i;
    Radio = value & 0x10000; /* Bit 16 */
    Pushbutton = value & 0x20000; /* Bit 17 */

    dmprintf(ctx->memory, "WARNING: AcroForm field 'Btn' with no AP not implemented.\n");
    dmprintf2(ctx->memory, "       : Radio = %s, Pushbutton = %s.\n",
              Radio ? "TRUE" : "FALSE", Pushbutton ? "TRUE" : "FALSE");
 exit:
    return 0;
}

/* draw field Tx */
static int pdfi_form_draw_Tx(pdf_context *ctx, pdf_dict *AcroForm, pdf_dict *field, pdf_dict *AP)
{
    int code = 0;

    if (AP != NULL)
        return pdfi_annot_draw_AP(ctx, field, AP);

    dmprintf(ctx->memory, "WARNING: AcroForm field 'Tx' with no AP not implemented.\n");
    return code;
}

/* draw field Ch */
static int pdfi_form_draw_Ch(pdf_context *ctx, pdf_dict *AcroForm, pdf_dict *field, pdf_dict *AP)
{
    int code = 0;
    bool NeedAppearances = false; /* TODO: Spec says default is false, gs code uses true... */

    code = pdfi_dict_get_bool(ctx, AcroForm, "NeedAppearances", &NeedAppearances);
    if (code < 0 && code != gs_error_undefined) goto exit;

    if (!NeedAppearances && AP != NULL)
        return pdfi_annot_draw_AP(ctx, field, AP);

    dmprintf(ctx->memory, "WARNING: AcroForm field 'Ch' with no AP not implemented.\n");
 exit:
    return code;
}

/* draw field Sig */
static int pdfi_form_draw_Sig(pdf_context *ctx, pdf_dict *AcroForm, pdf_dict *field, pdf_dict *AP)
{
    dmprintf(ctx->memory, "WARNING: AcroForm field 'Sig' not implemented.\n");
    return 0;
}

/* draw terminal field */
static int pdfi_form_draw_terminal(pdf_context *ctx, pdf_dict *Page, pdf_dict *AcroForm, pdf_dict *field)
{
    int code = 0;
    pdf_indirect_ref *P = NULL;
    pdf_name *FT = NULL;
    pdf_dict *AP = NULL;

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

    if (pdfi_name_is(FT, "Btn")) {
        code = pdfi_form_draw_Btn(ctx, AcroForm, field, AP);
    } else if (pdfi_name_is(FT, "Tx")) {
        code = pdfi_form_draw_Tx(ctx, AcroForm, field, AP);
    } else if (pdfi_name_is(FT, "Ch")) {
        code = pdfi_form_draw_Ch(ctx, AcroForm, field, AP);
    } else if (pdfi_name_is(FT, "Sig")) {
        code = pdfi_form_draw_Sig(ctx, AcroForm, field, AP);
    } else {
        dmprintf(ctx->memory, "*** WARNING unknown field FT ignored\n");
        /* TODO: Handle warning better */
        code = 0;
    }

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
static int pdfi_form_draw_field(pdf_context *ctx, pdf_dict *Page, pdf_dict *AcroForm, pdf_dict *field)
{
    int code = 0;
    pdf_array *Kids = NULL;
    pdf_dict *child = NULL;
    pdf_dict *Parent = NULL;
    int i;

    code = pdfi_dict_knownget_type(ctx, field, "Kids", PDF_ARRAY, (pdf_obj **)&Kids);
    if (code < 0) goto exit;
    if (code == 0) {
        code = pdfi_form_draw_terminal(ctx, Page, AcroForm, field);
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
        code = pdfi_form_draw_terminal(ctx, Page, AcroForm, field);
        goto exit;
    }

    pdfi_countdown(child);
    child = NULL;
    /* Render the Kids (recursive) */
    for (i=0; i<pdfi_array_size(Kids); i++) {
        code = pdfi_array_get_type(ctx, Kids, i, PDF_DICT, (pdf_obj **)&child);
        if (code < 0) goto exit;

        code = pdfi_form_draw_field(ctx, Page, AcroForm, child);
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
    pdf_dict *AcroForm = NULL;
    pdf_array *Fields = NULL;
    pdf_dict *field = NULL;
    int i;

    if (!ctx->showacroform)
        return 0;

    code = pdfi_dict_knownget_type(ctx, ctx->Root, "AcroForm", PDF_DICT, (pdf_obj **)&AcroForm);
    if (code <= 0)
        goto exit;

    code = pdfi_dict_knownget_type(ctx, AcroForm, "Fields", PDF_ARRAY, (pdf_obj **)&Fields);
    if (code <= 0)
        goto exit;

    for (i=0; i<pdfi_array_size(Fields); i++) {
        code = pdfi_array_get_type(ctx, Fields, i, PDF_DICT, (pdf_obj **)&field);
        if (code < 0)
            continue;
        code = pdfi_form_draw_field(ctx, page_dict, AcroForm, field);
        if (code < 0)
            goto exit;
        pdfi_countdown(field);
        field = NULL;
    }

 exit:
    pdfi_countdown(field);
    pdfi_countdown(Fields);
    pdfi_countdown(AcroForm);
    return code;
}
