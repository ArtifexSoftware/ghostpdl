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

int pdfi_create_Encoding(pdf_context *ctx, pdf_obj *pdf_Encoding, pdf_obj *font_Encoding, pdf_obj **Encoding);
gs_glyph pdfi_encode_char(gs_font * pfont, gs_char chr, gs_glyph_space_t not_used);
int pdfi_glyph_index(gs_font *pfont, byte *str, uint size, uint *glyph);
int pdfi_glyph_name(gs_font * pfont, gs_glyph glyph, gs_const_string * pstr);
int pdfi_decode_glyph(gs_font * font, gs_glyph glyph, int ch, ushort *unicode_return, unsigned int length);

/* This is in pdf_fapi.c, but since it is the only exported function
   from that module (so far) it doesn't seem worth a new header
 */
int pdfi_fapi_passfont(pdf_font *font, int subfont, char *fapi_request,
                 char *file_name, byte * font_data, int font_data_len);

int pdfi_fapi_check_cmap_for_GID(gs_font *pfont, uint c, uint *g);

int pdfi_init_font_directory(pdf_context *ctx);

int pdfi_load_font(pdf_context *ctx, pdf_dict *stream_dict, pdf_dict *page_dict, pdf_dict *font_dict, gs_font **ppfont, bool cidfont);

int pdfi_load_dict_font(pdf_context *ctx, pdf_dict *stream_dict, pdf_dict *page_dict, pdf_dict *font_dict, double point_size);

/* Loads a (should be!) non-embedded font by name
   Only currently works for the Type 1 font set from romfs.
 */
int pdfi_load_font_by_name_string(pdf_context *ctx, const byte *fontname, size_t length, pdf_obj **ppdffont);

/* Convenience function for using fonts created by
   pdfi_load_font_by_name_string
 */
int pdfi_set_font_internal(pdf_context *ctx, pdf_obj *fontobj, double point_size);

int pdfi_font_set_internal_string(pdf_context *ctx, const char *fontname, double point_size);
int pdfi_font_set_internal_name(pdf_context *ctx, pdf_name *fontname, double point_size);
bool pdfi_font_known_symbolic(pdf_obj *basefont);


enum {
  GLYPH_W0_WIDTH_INDEX = 0,
  GLYPH_W0_HEIGHT_INDEX = 1,
  GLYPH_W1_WIDTH_INDEX = 2,
  GLYPH_W1_HEIGHT_INDEX = 3,
  GLYPH_W1_V_X_INDEX = 4,
  GLYPH_W1_V_Y_INDEX = 5
};

int pdfi_get_cidfont_glyph_metrics(gs_font *pfont, gs_glyph cid, double *widths, bool vertical);
int pdfi_font_generate_pseudo_XUID(pdf_context *ctx, pdf_dict *fontdict, gs_font_base *pfont);
#endif
