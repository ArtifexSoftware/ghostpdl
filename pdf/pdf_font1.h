/* Copyright (C) 2019-2021 Artifex Software, Inc.
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

/* Type 1 font handling routines */

#ifndef PDF_TYPE1_FONT
#define PDF_TYPE1_FONT

int pdfi_read_type1_font(pdf_context *ctx, pdf_dict *font_dict, pdf_dict *stream_dict, pdf_dict *page_dict, gs_font **pfont);
int pdfi_free_font_type1(pdf_obj *font);
int pdfi_t1_global_glyph_code(const gs_font *pfont, gs_const_string *gstr, gs_glyph *pglyph);

#endif
