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

/* pattern operations for the PDF interpreter */

#ifndef PDF_PATTERN_OPERATORS
#define PDF_PATTERN_OPERATORS

#include "gscolor1.h"

void pdfi_pattern_cleanup(gs_memory_t * mem, void *p);
int pdfi_pattern_set(pdf_context *ctx, pdf_dict *stream_dict,
                     pdf_dict *page_dict, pdf_name *pname, gs_client_color *cc);
int pdfi_pattern_create(pdf_context *ctx, pdf_array *color_array,
                        pdf_dict *stream_dict, pdf_dict *page_dict, gs_color_space **ppcs);
#endif
