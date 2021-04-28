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

/* Type 0 (CID) font handling routines */
/* Also CIDFontType0 and CIDFontType0 handling */

#ifndef PDF_TYPE0_FONT
#define PDF_TYPE0_FONT

#include "ghostpdf.h"
#include "pdf_types.h"
#include "pdf_font_types.h"

int pdfi_read_type0_font(pdf_context *ctx, pdf_dict *font_dict, pdf_dict *stream_dict, pdf_dict *page_dict, gs_font **pfont);
int pdfi_free_font_type0(pdf_obj *font);


int pdfi_read_cidtype2_font(pdf_context *ctx, pdf_dict *font_dict, byte *buf, int buflen, pdf_font **ppfont);
int pdfi_free_font_cidtype2(pdf_obj *font);

int pdfi_read_cidtype0_font(pdf_context *ctx, pdf_dict *font_dict, byte *fbuf, int fbuflen, pdf_font **ppfont);

#endif
