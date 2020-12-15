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

/* CFF (Type 1C) font handling routines */

#ifndef PDF_TYPE1C_FONT
#define PDF_TYPE1C_FONT

int pdfi_read_type1C_font(pdf_context *ctx, pdf_dict *font_dict, pdf_dict *stream_dict, pdf_dict *page_dict, gs_font **pfont);
int pdfi_cff_global_glyph_code(const gs_font *pfont, gs_const_string *gstr, gs_glyph *pglyph);
int pdfi_free_font_cff(pdf_obj *font);

int pdfi_read_cff_font(pdf_context *ctx, pdf_dict *font_dict, byte *fbuf, int64_t fbuflen, pdf_font **ppdffont, bool forcecid);

int pdfi_free_font_cidtype0(pdf_obj *font);

#endif
