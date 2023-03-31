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

/* pdfmark handling for the PDF interpreter */

#ifndef PDF_MARK
#define PDF_MARK

int pdfi_pdfmark_from_dict(pdf_context *ctx, pdf_dict *dict, gs_matrix *ctm, const char *type);
int pdfi_pdfmark_from_objarray(pdf_context *ctx, pdf_obj **objarray, int len, gs_matrix *ctm, const char *type);
int pdfi_pdfmark_object(pdf_context *ctx, pdf_obj *object, const char *label);
int pdfi_pdfmark_modDest(pdf_context *ctx, pdf_dict *dict);
int pdfi_pdfmark_modA(pdf_context *ctx, pdf_dict *dict);
int pdfi_pdfmark_stream(pdf_context *ctx, pdf_stream *stream);
int pdfi_pdfmark_dict(pdf_context *ctx, pdf_dict *dict);
int pdfi_pdfmark_embed_filespec(pdf_context *ctx, pdf_string *name, pdf_dict *filespec);
int pdfi_pdfmark_get_objlabel(pdf_context *ctx, pdf_obj *obj, char **label);
void pdfi_pdfmark_write_boxes(pdf_context *ctx, pdf_dict *page_dict);
void pdfi_pdfmark_write_docinfo(pdf_context *ctx, pdf_dict *info_dict);

#endif
