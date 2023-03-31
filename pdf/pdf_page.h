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

/* Page-level operations for the PDF interpreter */

#ifndef PDF_PAGE_OPERATORS
#define PDF_PAGE_OPERATORS

int pdfi_page_render(pdf_context *ctx, uint64_t page_num, bool init_graphics);
int pdfi_page_info(pdf_context *ctx, uint64_t page_num, pdf_dict **info_dict, bool extended);
int pdfi_page_graphics_begin(pdf_context *ctx);
int pdfi_page_get_dict(pdf_context *ctx, uint64_t page_num, pdf_dict **dict);
int pdfi_page_get_number(pdf_context *ctx, pdf_dict *target_dict, uint64_t *page_num);

#endif
