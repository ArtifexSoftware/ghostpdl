/* Copyright (C) 2001-2020 Artifex Software, Inc.
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


int pdfi_skip_white(pdf_context *ctx, pdf_stream *s);
int pdfi_read_token(pdf_context *ctx, pdf_stream *s, uint32_t indirect_num, uint32_t indirect_gen);
int pdfi_alloc_object(pdf_context *ctx, pdf_obj_type type, unsigned int size, pdf_obj **obj);
void pdfi_free_object(pdf_obj *o);

int pdfi_make_name(pdf_context *ctx, byte *key, uint32_t size, pdf_obj **o);

int pdfi_read_dict(pdf_context *ctx, pdf_stream *s, uint32_t indirect_num, uint32_t indirect_gen);

int replace_cache_entry(pdf_context *ctx, pdf_obj *o);

int is_compressed_object(pdf_context *ctx, uint32_t obj, uint32_t gen);
int pdfi_dereference(pdf_context *ctx, uint64_t obj, uint64_t gen, pdf_obj **object);
int pdfi_deref_loop_detect(pdf_context *ctx, uint64_t obj, uint64_t gen, pdf_obj **object);

int pdfi_read_xref(pdf_context *ctx);

int pdfi_repair_file(pdf_context *ctx);

int pdfi_read_Info(pdf_context *ctx);
int pdfi_read_Pages(pdf_context *ctx);
int pdfi_read_Root(pdf_context *ctx);
void pdfi_read_OptionalRoot(pdf_context *ctx);
void pdfi_free_OptionalRoot(pdf_context *ctx);
int pdfi_get_page_dict(pdf_context *ctx, pdf_dict *d, uint64_t page_num, uint64_t *page_offset, pdf_dict **target, pdf_dict *inherited);
void local_save_stream_state(pdf_context *ctx, stream_save *local_save);
void local_restore_stream_state(pdf_context *ctx, stream_save *local_save);
void cleanup_context_interpretation(pdf_context *ctx, stream_save *local_save);
void initialise_stream_save(pdf_context *ctx);
int pdfi_run_context(pdf_context *ctx, pdf_dict *stream_dict, pdf_dict *page_dict, bool stoponerror, const char *desc);
int pdfi_interpret_inner_content_stream(pdf_context *ctx, pdf_dict *stream_dict, pdf_dict *page_dict, bool stoponerror, const char *desc);
int pdfi_interpret_content_stream(pdf_context *ctx, pdf_dict *stream_dict, pdf_dict *page_dict);

int pdfi_find_resource(pdf_context *ctx, unsigned char *Type, pdf_name *name, pdf_dict *stream_dict, pdf_dict *page_dict, pdf_obj **o);

#endif
