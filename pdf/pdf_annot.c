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
#include "gspath2.h"
#include "pdf_image.h"
#include "gxfarith.h"

typedef int (*annot_func)(pdf_context *ctx, pdf_dict *annot, pdf_dict *NormAP, bool *render_done);

typedef struct {
    const char *subtype;
    annot_func func;
    bool simpleAP;
} annot_dispatch_t;


typedef int (*annot_LE_func)(pdf_context *ctx, pdf_dict *annot);

typedef struct {
    const char *name;
    annot_LE_func func;
} annot_LE_dispatch_t;


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
    code = gs_setstrokeconstantalpha(ctx->pgs, 1.0);
    code = gs_setfillconstantalpha(ctx->pgs, 1.0);
    code = gs_setalphaisshape(ctx->pgs, 0);
    code = gs_setblendmode(ctx->pgs, BLEND_MODE_Compatible);
    code = gs_settextknockout(ctx->pgs, true);
    code = gs_settextspacing(ctx->pgs, (double)0.0);
    code = gs_settextleading(ctx->pgs, (double)0.0);
    gs_settextrenderingmode(ctx->pgs, 0);
    code = gs_setwordspacing(ctx->pgs, (double)0.0);
    code = gs_settexthscaling(ctx->pgs, (double)100.0);
    code = gs_setsmoothness(ctx->pgs, 0.02); /* Match gs code */
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

static int pdfi_annot_draw_Ink(pdf_context *ctx, pdf_dict *annot, pdf_dict *NormAP, bool *render_done)
{
    int code = 0;

    /* TODO: Generate appearance (see pdf_draw.ps/Ink) */
    dbgmprintf(ctx->memory, "ANNOT: No AP generation for subtype Ink\n");
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

    code = pdfi_array_to_num_array(ctx, CL, array, length);
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
static int pdfi_annot_draw_FreeText(pdf_context *ctx, pdf_dict *annot, pdf_dict *NormAP, bool *render_done)
{
    int code = 0;
    int code1 = 0;
    bool drawbackground;
    pdf_dict *BS = NULL;
    pdf_string *DA = NULL;

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

    /* TODO: /DA */
    code = pdfi_dict_knownget_type(ctx, annot, "DA", PDF_STRING, (pdf_obj **)&DA);
    if (code < 0) goto exit;
    if (code > 0) {
        /* HACKY HACK -- just setting color for sample Bug701889.pdf */
        code = pdfi_gs_setrgbcolor(ctx, 0.898, 0.1333, 0.2157);
        if (code < 0) goto exit;
    } else {
        code = pdfi_gs_setgray(ctx, 0);
        if (code < 0) goto exit;
    }

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
    pdfi_countdown(DA);
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

static int pdfi_annot_draw_StrikeOut(pdf_context *ctx, pdf_dict *annot, pdf_dict *NormAP, bool *render_done)
{
    int code = 0;

    /* TODO: Generate appearance (see pdf_draw.ps/StrikeOut) */
    dbgmprintf(ctx->memory, "ANNOT: No AP generation for subtype StrikeOut\n");
    *render_done = true;

    return code;
}

static int pdfi_annot_draw_Underline(pdf_context *ctx, pdf_dict *annot, pdf_dict *NormAP, bool *render_done)
{
    int code = 0;

    /* TODO: Generate appearance (see pdf_draw.ps/Underline) */
    dbgmprintf(ctx->memory, "ANNOT: No AP generation for subtype Underline\n");
    *render_done = true;

    return code;
}

static int pdfi_annot_draw_Highlight(pdf_context *ctx, pdf_dict *annot, pdf_dict *NormAP, bool *render_done)
{
    int code = 0;

    /* TODO: Generate appearance (see pdf_draw.ps/Highlight) */
    dbgmprintf(ctx->memory, "ANNOT: No AP generation for subtype Highlight\n");
    *render_done = true;

    return code;
}

static int pdfi_annot_draw_Redact(pdf_context *ctx, pdf_dict *annot, pdf_dict *NormAP, bool *render_done)
{
    int code = 0;

    /* TODO: Generate appearance (see pdf_draw.ps/Redact) */
    dbgmprintf(ctx->memory, "ANNOT: No AP generation for subtype Redact\n");
    *render_done = true;

    return code;
}

static int pdfi_annot_draw_Popup(pdf_context *ctx, pdf_dict *annot, pdf_dict *NormAP, bool *render_done)
{
    int code = 0;
    bool Open = false;

    /* Render only if open */
    code = pdfi_dict_get_bool(ctx, annot, "Open", &Open);
    if (code < 0 && (code != gs_error_undefined))
        goto exit;

    code = 0;

    if (!Open) {
        *render_done = true;
        goto exit;
    }

    if (NormAP) {
        *render_done = false;
        goto exit;
    }

    /* TODO: Generate appearance (see pdf_draw.ps/Popup) */
    dbgmprintf(ctx->memory, "ANNOT: No AP generation for subtype Popup\n");
    *render_done = true;

 exit:
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

/* Draws a thing of type /Widget */
static int pdfi_annot_draw_Widget(pdf_context *ctx, pdf_dict *annot, pdf_dict *NormAP, bool *render_done)
{
    /* TODO: See top part of pdf_draw.ps/drawwidget
     * check for /FT and /T and stuff
     */
    dbgmprintf(ctx->memory, "ANNOT: No AP generation for subtype Widget\n");
    *render_done = false;
    return 0;
}

annot_dispatch_t annot_dispatch[] = {
    {"Ink", pdfi_annot_draw_Ink, true},
    {"Circle", pdfi_annot_draw_Circle, true},
    {"Stamp", pdfi_annot_draw_Stamp, true},
    {"FreeText", pdfi_annot_draw_FreeText, true},
    {"Text", pdfi_annot_draw_Text, true},
    {"StrikeOut", pdfi_annot_draw_StrikeOut, true},
    {"Underline", pdfi_annot_draw_Underline, true},
    {"Redact", pdfi_annot_draw_Redact, true},
    {"Highlight", pdfi_annot_draw_Highlight, true},
    {"Polygon", pdfi_annot_draw_Polygon, true},
    {"Square", pdfi_annot_draw_Square, true},
    {"Line", pdfi_annot_draw_Line, true},
    {"PolyLine", pdfi_annot_draw_PolyLine, true},
    {"Link", pdfi_annot_draw_Link, false},
    {"Popup", pdfi_annot_draw_Popup, false},
    {"Widget", pdfi_annot_draw_Widget, false},
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

static int pdfi_annot_preserve(pdf_context *ctx, pdf_dict *annot)
{
    return 0;
}

static int pdfi_annot_draw(pdf_context *ctx, pdf_dict *annot)
{
    pdf_name *Subtype = NULL;
    pdf_dict *NormAP = NULL;
    int code;
    annot_dispatch_t *dispatch_ptr;
    bool render_done = true;

    code = pdfi_dict_get_type(ctx, annot, "Subtype", PDF_NAME, (pdf_obj **)&Subtype);
    if (code != 0) {
        /* TODO: Set warning flag */
        dbgmprintf(ctx->memory, "WARNING: Ignoring annotation with missing Subtype\n");
        code = 0;
        goto exit;
    }

    /* See if annotation is visible */
    if (!pdfi_annot_visible(ctx, annot, Subtype))
        goto exit;

    /* See if we are rendering this type of annotation */
    if (!pdfi_annot_check_type(ctx, Subtype))
        goto exit;

    /* Get the Normal AP, if it exists */
    code = pdfi_annot_get_NormAP(ctx, annot, &NormAP);
    if (code < 0) goto exit;

    code = pdfi_gsave(ctx);
    if (code < 0) goto exit;

    /* Draw the annotation */
    for (dispatch_ptr = annot_dispatch; dispatch_ptr->subtype; dispatch_ptr ++) {
        if (pdfi_name_is(Subtype, dispatch_ptr->subtype)) {
            if (NormAP && dispatch_ptr->simpleAP)
                render_done = false;
            else
                code = dispatch_ptr->func(ctx, annot, NormAP, &render_done);
            break;
        }
    }
    if (!dispatch_ptr->subtype) {
        char str[100];
        memcpy(str, (const char *)Subtype->data, Subtype->length);
        str[Subtype->length] = '\0';
        dbgmprintf1(ctx->memory, "ANNOT: No handler for subtype %s\n", str);

        /* Not necessarily an error? We can just render the AP if there is one */
        render_done = false;
    }

    if (!render_done)
        code = pdfi_annot_draw_AP(ctx, annot, NormAP);

    (void)pdfi_grestore(ctx);

 exit:
    pdfi_countdown(Subtype);
    pdfi_countdown(NormAP);
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
        if (ctx->preserveannots && ctx->annotations_preserved)
            code = pdfi_annot_preserve(ctx, annot);
        else
            code = pdfi_annot_draw(ctx, annot);
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
