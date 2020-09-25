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

#ifndef PDF_DEREFERENCE
#define PDF_DEREFERENCE

int replace_cache_entry(pdf_context *ctx, pdf_obj *o);
int is_compressed_object(pdf_context *ctx, uint32_t obj, uint32_t gen);
int pdfi_dereference(pdf_context *ctx, uint64_t obj, uint64_t gen, pdf_obj **object);
int pdfi_deref_loop_detect(pdf_context *ctx, uint64_t obj, uint64_t gen, pdf_obj **object);
int pdfi_read_bare_object(pdf_context *ctx, pdf_stream *s, gs_offset_t stream_offset, uint32_t objnum, uint32_t gen);
#endif
