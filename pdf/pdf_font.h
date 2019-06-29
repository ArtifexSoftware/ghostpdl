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

/* Font operations for the PDF interpreter */

#include "pdf_font_types.h"
#include "pdf_stack.h"

#ifndef PDF_FONT_OPERATORS
#define PDF_FONT_OPERATORS

int pdfi_d0(pdf_context *ctx);
int pdfi_d1(pdf_context *ctx);
int pdfi_Tf(pdf_context *ctx, pdf_dict *stream_dict, pdf_dict *page_dict);
int pdfi_free_font(pdf_obj *font);

static inline void pdfi_countup_current_font(pdf_context *ctx)
{
    pdf_font *font;

    if (ctx->pgs->font != NULL) {
        font = (pdf_font *)ctx->pgs->font->client_data;
        pdfi_countup(font);
    }
}

static inline void pdfi_countdown_current_font(pdf_context *ctx)
{
    pdf_font *font;

    if (ctx->pgs->font != NULL) {
        font = (pdf_font *)ctx->pgs->font->client_data;
        pdfi_countdown(font);
    }
}

static inline pdf_font *pdfi_get_current_pdf_font(pdf_context *ctx)
{
    pdf_font *font;

    if (ctx->pgs->font != NULL) {
        font = (pdf_font *)ctx->pgs->font->client_data;
        return(font);
    }
    return NULL;
}

#endif
