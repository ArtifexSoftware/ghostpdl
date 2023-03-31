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

/* Type 0 (CID) font handling routines */
/* Also CIDFontType0 and CIDFontType0 handling */

#ifndef PDF_TYPE0_FONT
#define PDF_TYPE0_FONT

#include "ghostpdf.h"
#include "pdf_types.h"
#include "pdf_font_types.h"

int pdfi_read_type0_font(pdf_context *ctx, pdf_dict *font_dict, pdf_dict *stream_dict, pdf_dict *page_dict, pdf_font **ppdffont);
int pdfi_free_font_type0(pdf_obj *font);

int pdfi_read_cidtype2_font(pdf_context *ctx, pdf_dict *font_dict, pdf_dict *stream_dict, pdf_dict *page_dict, byte *buf, int64_t buflen, int findex, pdf_font **ppfont);

int pdfi_free_font_cidtype2(pdf_obj *font);

int pdfi_read_cidtype0_font(pdf_context *ctx, pdf_dict *font_dict, byte *fbuf, int fbuflen, pdf_font **ppfont);

#endif
