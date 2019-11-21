/* Copyright (C) 2001-2019 Artifex Software, Inc.
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

typedef bool (*annot_func)(pdf_context *ctx, pdf_dict *annot, bool *render_done);

typedef struct {
    const char *subtype;
    annot_func func;
} annot_dispatch_t;


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

/* See pdf_draw.ps/calc_annot_scale */
static int pdfi_annot_calcscale(pdf_context *ctx, pdf_dict *annot, double *xscale, double *yscale)
{
    /* TODO: Implement this */

    *xscale = 1.0;
    *yscale = 1.0;
    return 0;
}

/* See pdf_draw.ps/drawwidget */
static int pdfi_annot_draw_widget(pdf_context *ctx, pdf_dict *annot, double xscale, double yscale)
{
    /* TODO: Implement this */

    return 0;
}

static int pdfi_annot_setcolor(pdf_context *ctx, pdf_dict *annot, bool *drawit)
{
    int code = 0;
    pdf_array *C = NULL;

    *drawit = true;

    code = pdfi_dict_knownget_type(ctx, annot, "C", PDF_ARRAY, (pdf_obj **)&C);
    if (code < 0) goto exit;

    if (code == 0) {
        code = pdfi_gs_setgray(ctx, 0);
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

static int pdfi_annot_strokeborder(pdf_context *ctx, pdf_dict *annot, double width, pdf_array *dash)
{
    int code = 0;
    bool drawit;
    gs_rect rect;
    pdf_array *Rect = NULL;

    if (width <= 0)
        return 0;

    code = pdfi_gsave(ctx);

    code = pdfi_annot_setcolor(ctx, annot, &drawit);
    if (code < 0) goto exit;
    if (!drawit) goto exit;

    code = pdfi_setdash_impl(ctx, dash, 0);
    if (code < 0) goto exit;
    code = gs_setlinewidth(ctx->pgs, width);
    if (code < 0) goto exit;

    code = pdfi_dict_knownget_type(ctx, annot, "Rect", PDF_ARRAY, (pdf_obj **)&Rect);
    if (code < 0) goto exit;

    code = pdfi_array_to_gs_rect(ctx, Rect, &rect);
    if (code < 0) goto exit;

    pdfi_normalize_rect(ctx, &rect);

    /* Stroke the rectangle */
    /* Adjust rectangle by the width */
    rect.p.x += width/2;
    rect.p.y += width/2;
    rect.q.x -= width/2;
    rect.q.y -= width/2;
    code = gs_rectstroke(ctx->pgs, &rect, 1, NULL);

 exit:
    (void)pdfi_grestore(ctx);
    pdfi_countdown(Rect);
    return code;
}

/* Draw border from a Border array
 * Note: Border can be null
 */
static int pdfi_annot_draw_Border(pdf_context *ctx, pdf_dict *annot, pdf_array *Border)
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
            }
        } else {
            code = pdfi_array_alloc(ctx, 0, &dash);
            if (code < 0) goto exit;
        }
        code = pdfi_array_get_number(ctx, Border, 2, &width);
        if (code < 0) goto exit;
    }

    /* At this point we have a dash array (which could be length 0) and a width */
    code = pdfi_annot_strokeborder(ctx, annot, width, dash);

 exit:
    pdfi_countdown(dash);
    return code;
}

/* Draw border from a BS dict */
static int pdfi_annot_draw_BS(pdf_context *ctx, pdf_dict *annot, pdf_dict *BS)
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
        if (code < 0) goto exit;
    }

    /* At this point we have a dash array (which could be length 0) and a width */
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
 */
static int pdfi_annot_draw_border(pdf_context *ctx, pdf_dict *annot)
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
        code = pdfi_annot_draw_BS(ctx, annot, BS);
    } else {
        /* Note: Border can be null */
        code = pdfi_annot_draw_Border(ctx, annot, Border);
    }
    code1 = pdfi_grestore(ctx);
    if (code == 0) code = code1;

 exit:
    pdfi_countdown(BS);
    pdfi_countdown(Border);
    return code;
}

static bool pdfi_annot_draw_Link(pdf_context *ctx, pdf_dict *annot, bool *render_done)
{
    int code;
    int code1;
    double xscale, yscale;

    dbgmprintf(ctx->memory, "ANNOT: Drawing Link\n");

    code = pdfi_annot_start_transparency(ctx, annot);
    if (code < 0)
        return code;

    code = pdfi_annot_draw_border(ctx, annot);
    if (code < 0) goto exit;

    code = pdfi_annot_calcscale(ctx, annot, &xscale, &yscale);
    if (code < 0) goto exit;

    if (xscale * yscale > 0.0) {
        code = pdfi_annot_draw_widget(ctx, annot, xscale, yscale);
    } else {
        dbgmprintf(ctx->memory, "ANNOT: Ignoring annotation with scale factor of 0\n");
    }

 exit:
    code1 = pdfi_annot_end_transparency(ctx, annot);
    if (code < 0) code = code1;

    *render_done = true;
    return code;
}

static bool pdfi_annot_draw_Ink(pdf_context *ctx, pdf_dict *annot, bool *render_done)
{
    return false;
}

static bool pdfi_annot_draw_Widget(pdf_context *ctx, pdf_dict *annot, bool *render_done)
{
    return false;
}

annot_dispatch_t annot_dispatch[] = {
    {"Link", pdfi_annot_draw_Link},
    {"Ink", pdfi_annot_draw_Ink},
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
    int code;
    annot_dispatch_t *dispatch_ptr;
    bool render_done = false;

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

    code = pdfi_gsave(ctx);
    if (code < 0)
        goto exit;

    /* Draw the annotation */
    for (dispatch_ptr = annot_dispatch; dispatch_ptr->subtype; dispatch_ptr ++) {
        if (pdfi_name_is(Subtype, dispatch_ptr->subtype)) {
            code = dispatch_ptr->func(ctx, annot, &render_done);
            break;
        }
    }
    if (!render_done)
        code = pdfi_annot_draw_Widget(ctx, annot, &render_done);

    (void)pdfi_grestore(ctx);

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
