/* Copyright (C) 2001-2018 Artifex Software, Inc.
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

/* File and decompression filter code */

int pdfi_filter(pdf_context *ctx, pdf_dict *d, pdf_stream *source, pdf_stream **new_stream, bool inline_image);
void pdfi_close_file(pdf_context *ctx, pdf_stream *s);
int pdfi_read_bytes(pdf_context *ctx, byte *Buffer, uint32_t size, uint32_t count, pdf_stream *s);
int pdfi_unread(pdf_context *ctx, pdf_stream *s, byte *Buffer, uint32_t size);
int pdfi_seek(pdf_context *ctx, pdf_stream *s, gs_offset_t offset, uint32_t origin);
gs_offset_t pdfi_unread_tell(pdf_context *ctx);
gs_offset_t pdfi_tell(pdf_stream *s);

int pdfi_apply_SubFileDecode_filter(pdf_context *ctx, int EODCount, gs_const_string *EODString, pdf_stream *source, pdf_stream **new_stream, bool inline_image);
int pdfi_open_memory_stream(pdf_context *ctx, unsigned int size, byte **Buffer, pdf_stream *source, pdf_stream **new_stream);
int pdfi_close_memory_stream(pdf_context *ctx, byte *Buffer, pdf_stream *source);
int pdfi_open_memory_stream_from_stream(pdf_context *ctx, unsigned int size,
                                        byte **Buffer, pdf_stream *source, pdf_stream **new_pdf_stream);
int pdfi_open_memory_stream_from_filtered_stream(pdf_context *ctx, pdf_dict *stream_dict, unsigned int size,
                                        byte **Buffer, pdf_stream *source, pdf_stream **new_pdf_stream);
int pdfi_open_memory_stream_from_memory(pdf_context *ctx, unsigned int size,
                                        byte *Buffer, pdf_stream **new_pdf_stream);
int pdfi_stream_to_buffer(pdf_context *ctx, pdf_dict *stream_dict, byte **buf, int64_t *bufferlen);
