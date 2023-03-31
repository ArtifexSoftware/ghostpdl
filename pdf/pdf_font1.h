/* Copyright (C) 2019-2023 Artifex Software, Inc.
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

/* Type 1 font handling routines */

#ifndef PDF_TYPE1_FONT
#define PDF_TYPE1_FONT

int pdfi_read_type1_font(pdf_context *ctx, pdf_dict *font_dict, pdf_dict *stream_dict, pdf_dict *page_dict, byte *fbuf, int64_t fbuflen, pdf_font **ppdffont);

int pdfi_free_font_type1(pdf_obj *font);
int pdfi_copy_type1_font(pdf_context *ctx, pdf_font *spdffont, pdf_dict *font_dict, pdf_font **tpdffont);

int pdfi_t1_global_glyph_code(const gs_font *pfont, gs_const_string *gstr, gs_glyph *pglyph);

#endif
