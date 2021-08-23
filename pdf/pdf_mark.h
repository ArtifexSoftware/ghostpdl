/* Copyright (C) 2020-2021 Artifex Software, Inc.
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

/* pdfmark handling for the PDF interpreter */

#ifndef PDF_MARK
#define PDF_MARK

int pdfi_mark_from_dict(pdf_context *ctx, pdf_dict *dict, gs_matrix *ctm, const char *type);
int pdfi_mark_object(pdf_context *ctx, pdf_obj *object, const char *label);
int pdfi_mark_modDest(pdf_context *ctx, pdf_dict *dict);
int pdfi_mark_modA(pdf_context *ctx, pdf_dict *dict);
int pdfi_mark_stream(pdf_context *ctx, pdf_stream *stream);
int pdfi_mark_dict(pdf_context *ctx, pdf_dict *dict);
int pdfi_mark_embed_filespec(pdf_context *ctx, pdf_string *name, pdf_dict *filespec);
int pdfi_mark_get_objlabel(pdf_context *ctx, pdf_obj *obj, char **label);
void pdfi_write_boxes_pdfmark(pdf_context *ctx, pdf_dict *page_dict);
void pdfi_write_docinfo_pdfmark(pdf_context *ctx, pdf_dict *info_dict);

#endif
