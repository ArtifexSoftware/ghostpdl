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

#ifndef PDF_CHECK
#define PDF_CHECK

int pdfi_check_page(pdf_context *ctx, pdf_dict *page_dict, pdf_array **fonts_array, pdf_array **spots_array, bool do_setup);

int pdfi_check_Pattern_transparency(pdf_context *ctx, pdf_dict *pattern,
                                    pdf_dict *page_dict, bool *transparent, bool *BM_Not_Normal);

#endif
