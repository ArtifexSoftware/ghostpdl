/* Copyright (C) 2018-2023 Artifex Software, Inc.
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

/* colour operations for the PDF interpreter */

#ifndef PDF_COLOUR_OPERATORS
#define PDF_COLOUR_OPERATORS

#include "gscolor1.h"
#include "gscspace.h"
#include "pdf_stack.h"  /* for pdfi_countup/countdown */
#include "pdf_misc.h"   /* for pdf_name_cmp */

static inline void pdfi_set_colourspace_name(pdf_context *ctx, gs_color_space *pcs, pdf_name *n)
{
    if (pcs->interpreter_data != NULL) {
        pdf_obj *o = (pdf_obj *)(pcs->interpreter_data);
        if (o != NULL && pdfi_type_of(o) == PDF_NAME) {
            pdfi_countdown(o);
            pcs->interpreter_data = NULL;
        }
    }

    if (n != NULL) {
        pcs->interpreter_data = n;
        pdfi_countup(n);
    } else {
        if (pcs->interpreter_data == NULL)
            pcs->interpreter_data = ctx;
    }
}

static inline void pdfi_set_colour_callback(gs_color_space *pcs, pdf_context *ctx, gs_cspace_free_proc_t pdfi_cspace_free_callback)
{
    if (pcs->interpreter_data == NULL)
        pcs->interpreter_data = ctx;
    pcs->interpreter_free_cspace_proc = pdfi_cspace_free_callback;
}

static inline int check_same_current_space(pdf_context *ctx, pdf_name *n)
{
    pdf_obj *o = (pdf_obj *)(ctx->pgs->color[0].color_space->interpreter_data);

    if (o == NULL || pdfi_type_of(o) != PDF_NAME)
        return 0;

    if (pdfi_name_cmp(n, (pdf_name *)o) == 0) {
        if (n->object_num == o->object_num && n->indirect_num == o->indirect_num)
            return 1;
    }
    return 0;
}

int pdfi_setgraystroke(pdf_context *ctx);
int pdfi_setgrayfill(pdf_context *ctx);
int pdfi_setrgbstroke(pdf_context *ctx);
int pdfi_setrgbfill(pdf_context *ctx);
int pdfi_setrgbfill_array(pdf_context *ctx);
int pdfi_setcmykstroke(pdf_context *ctx);
int pdfi_setcmykfill(pdf_context *ctx);
int pdfi_setstrokecolor_space(pdf_context *ctx, pdf_dict *stream_dict, pdf_dict *page_dict);
int pdfi_setfillcolor_space(pdf_context *ctx, pdf_dict *stream_dict, pdf_dict *page_dict);
int pdfi_setstrokecolor(pdf_context *ctx);
int pdfi_setfillcolor(pdf_context *ctx);
int pdfi_setcolorN(pdf_context *ctx, pdf_dict *stream_dict, pdf_dict *page_dict, bool is_fill);
int pdfi_ri(pdf_context *ctx);

int pdfi_setcolor_from_array(pdf_context *ctx, pdf_array *array);
int pdfi_gs_setgray(pdf_context *ctx, double d);
int pdfi_gs_setrgbcolor(pdf_context *ctx, double r, double g, double b);

/* For potential use by other types of object (images etc) */
int pdfi_gs_setcolorspace(pdf_context *ctx, gs_color_space *pcs);
int pdfi_setcolorspace(pdf_context *ctx, pdf_obj *space, pdf_dict *stream_dict, pdf_dict *page_dict);
int pdfi_create_colorspace(pdf_context *ctx, pdf_obj *space, pdf_dict *stream_dict, pdf_dict *page_dict, gs_color_space **ppcs, bool inline_image);
int pdfi_create_icc_colorspace_from_stream(pdf_context *ctx, pdf_c_stream *stream, gs_offset_t offset,
                                           unsigned int length, int comps, int *icc_N, ulong dictkey, gs_color_space **ppcs);

/* Page level spot colour detection and enumeration */
int pdfi_check_ColorSpace_for_spots(pdf_context *ctx, pdf_obj *space, pdf_dict *parent_dict, pdf_dict *page_dict, pdf_dict *spot_dict);

int pdfi_color_setoutputintent(pdf_context *ctx, pdf_dict *intent_dict, pdf_stream *profile);

/* Sets up the DefaultRGB, DefaultCMYK and DefaultGray colour spaces if present */
int pdfi_setup_DefaultSpaces(pdf_context *ctx, pdf_dict *source_dict);

#endif
