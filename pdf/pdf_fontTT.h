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

/* TrueType font handling routines */

#ifndef PDF_TRUETYPE_FONT
#define PDF_TRUETYPE_FONT
int pdfi_find_post_entry(gs_font_type42 *pfont, gs_const_string *gname, uint *cc);
int pdfi_read_truetype_font(pdf_context *ctx, pdf_dict *font_dict, pdf_dict *stream_dict, pdf_dict *page_dict, byte *buf, int64_t buflen, int findex, pdf_font **ppdffont);
int pdfi_copy_truetype_font(pdf_context *ctx, pdf_font *spdffont, pdf_dict *font_dict, pdf_font **tpdffont);
int pdfi_free_font_truetype(pdf_obj *font);

#endif
