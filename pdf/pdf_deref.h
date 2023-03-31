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

#ifndef PDF_DEREFERENCE
#define PDF_DEREFERENCE

int replace_cache_entry(pdf_context *ctx, pdf_obj *o);
int is_compressed_object(pdf_context *ctx, uint32_t obj, uint32_t gen);
int pdfi_dereference(pdf_context *ctx, uint64_t obj, uint64_t gen, pdf_obj **object);
int pdfi_dereference_nocache(pdf_context *ctx, uint64_t obj, uint64_t gen, pdf_obj **object);
int pdfi_deref_loop_detect(pdf_context *ctx, uint64_t obj, uint64_t gen, pdf_obj **object);
int pdfi_deref_loop_detect_nocache(pdf_context *ctx, uint64_t obj, uint64_t gen, pdf_obj **object);
int pdfi_read_bare_object(pdf_context *ctx, pdf_c_stream *s, gs_offset_t stream_offset, uint32_t objnum, uint32_t gen);
int pdfi_resolve_indirect(pdf_context *ctx, pdf_obj *value, bool recurse);
int pdfi_resolve_indirect_loop_detect(pdf_context *ctx, pdf_obj *parent, pdf_obj *value, bool recurse);
#endif
