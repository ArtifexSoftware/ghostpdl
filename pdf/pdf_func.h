/* Copyright (C) 2018-2023 Artifex Software, Inc.
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

/* function creation for the PDF interpreter */

#ifndef PDF_FUNCTIONS
#define PDF_FUNCTIONS

int pdfi_free_function(pdf_context *ctx, gs_function_t *pfn);
int pdfi_build_function(pdf_context *ctx, gs_function_t ** ppfn, const float *shading_domain, int num_inputs, pdf_obj *stream_obj, pdf_dict *page_dict);
int pdfi_build_halftone_function(pdf_context *ctx, gs_function_t ** ppfn, byte *Buffer, int64_t Length);

#endif
