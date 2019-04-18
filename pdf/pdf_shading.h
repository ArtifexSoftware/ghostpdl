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

/* Shading operations for the PDF interpreter */

#ifndef PDF_SHADING_OPERATORS
#define PDF_SHADING_OPERATORS

int pdfi_shading_build(pdf_context *ctx, pdf_dict *stream_dict, pdf_dict *page_dict,
                       pdf_dict *sdict, gs_shading_t **ppsh, int64_t *ptype);
void pdfi_shading_free(pdf_context *ctx, gs_shading_t *psh, int64_t type);
int pdfi_shading(pdf_context *ctx, pdf_dict *stream_dict, pdf_dict *page_dict);

#endif
