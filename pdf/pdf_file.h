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

int pdf_filter(pdf_context_t *ctx, pdf_dict *d, stream *source, stream **new_stream);

int pdf_read_bytes(pdf_context_t *ctx, byte *Buffer, uint32_t size, uint32_t count, stream *s);
int pdf_unread(pdf_context_t *ctx, byte *Buffer, uint32_t size);
int pdf_seek(pdf_context_t *ctx, stream *s, gs_offset_t offset, uint32_t origin);
