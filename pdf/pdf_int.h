/* Copyright (C) 2018-2021 Artifex Software, Inc.
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

#ifndef PDF_INTERPRETER
#define PDF_INTERPRETER

#include "ghostpdf.h"
#include "pdf_types.h"


int pdfi_skip_white(pdf_context *ctx, pdf_c_stream *s);
int pdfi_skip_eol(pdf_context *ctx, pdf_c_stream *s);
int pdfi_skip_comment(pdf_context *ctx, pdf_c_stream *s);
int pdfi_read_token(pdf_context *ctx, pdf_c_stream *s, uint32_t indirect_num, uint32_t indirect_gen);

int pdfi_name_alloc(pdf_context *ctx, byte *key, uint32_t size, pdf_obj **o);

int pdfi_read_dict(pdf_context *ctx, pdf_c_stream *s, uint32_t indirect_num, uint32_t indirect_gen);

void local_save_stream_state(pdf_context *ctx, stream_save *local_save);
void local_restore_stream_state(pdf_context *ctx, stream_save *local_save);
void cleanup_context_interpretation(pdf_context *ctx, stream_save *local_save);
void initialise_stream_save(pdf_context *ctx);
int pdfi_run_context(pdf_context *ctx, pdf_stream *stream_obj, pdf_dict *page_dict, bool stoponerror, const char *desc);
int pdfi_interpret_inner_content_buffer(pdf_context *ctx, byte *content_data, uint32_t content_length,
                                        pdf_dict *stream_dict, pdf_dict *page_dict,
                                        bool stoponerror, const char *desc);
int pdfi_interpret_inner_content_c_string(pdf_context *ctx, char *content_string,
                                          pdf_dict *stream_dict, pdf_dict *page_dict,
                                          bool stoponerror, const char *desc);
int pdfi_interpret_inner_content_string(pdf_context *ctx, pdf_string *content_string,
                                        pdf_dict *stream_dict, pdf_dict *page_dict,
                                        bool stoponerror, const char *desc);
int pdfi_interpret_inner_content_stream(pdf_context *ctx, pdf_stream *stream_obj, pdf_dict *page_dict, bool stoponerror, const char *desc);
int pdfi_interpret_content_stream(pdf_context *ctx, pdf_c_stream *content_stream, pdf_stream *stream_obj, pdf_dict *page_dict);

#endif
