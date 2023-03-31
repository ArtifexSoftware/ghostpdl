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

/* Image operations for the PDF interpreter */

#ifndef PDF_IMAGE_OPERATORS
#define PDF_IMAGE_OPERATORS

int pdfi_BI(pdf_context *ctx);
int pdfi_EI(pdf_context *ctx);
int pdfi_ID(pdf_context *ctx, pdf_dict *stream_dict, pdf_dict *page_dict, pdf_c_stream *source);
int pdfi_Do(pdf_context *ctx, pdf_dict *stream_dict, pdf_dict *page_dict);
int pdfi_do_highlevel_form(pdf_context *ctx, pdf_dict *page_dict, pdf_stream *form_stream);
int pdfi_do_image_or_form(pdf_context *ctx, pdf_dict *stream_dict, pdf_dict *page_dict, pdf_obj *xobject_obj);
int pdfi_form_execgroup(pdf_context *ctx, pdf_dict *page_dict, pdf_stream *xobject_dict,
                        gs_gstate *GroupGState, gs_color_space *pcs, gs_client_color *pcc, gs_matrix *matrix);

#endif
