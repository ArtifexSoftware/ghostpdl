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

/* Annotation handling for the PDF interpreter */

#ifndef PDF_ANNOTATIONS
#define PDF_ANNOTATIONS

int pdfi_do_annotations(pdf_context *ctx, pdf_dict *page_dict);
int pdfi_do_acroform(pdf_context *ctx, pdf_dict *page_dict);

#endif
