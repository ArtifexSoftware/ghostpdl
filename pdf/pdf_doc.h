/* Copyright (C) 2020-2023 Artifex Software, Inc.
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

#ifndef PDF_DOC
#define PDF_DOC

int pdfi_read_Info(pdf_context *ctx);

int pdfi_read_Root(pdf_context *ctx);
void pdfi_read_OptionalRoot(pdf_context *ctx);
void pdfi_free_OptionalRoot(pdf_context *ctx);

int pdfi_read_Pages(pdf_context *ctx);
int pdfi_get_page_dict(pdf_context *ctx, pdf_dict *d, uint64_t page_num, uint64_t *page_offset, pdf_dict **target, pdf_dict *inherited);
int pdfi_find_resource(pdf_context *ctx, unsigned char *Type, pdf_name *name, pdf_dict *dict,
                       pdf_dict *page_dict, pdf_obj **o);
int pdfi_doc_page_array_init(pdf_context *ctx);
void pdfi_doc_page_array_free(pdf_context *ctx);
int pdfi_doc_trailer(pdf_context *ctx);

#endif
