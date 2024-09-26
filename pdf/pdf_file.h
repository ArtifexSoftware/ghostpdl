/* Copyright (C) 2018-2024 Artifex Software, Inc.
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

/* File and decompression filter code */

/*
 * Note: file and stream handling means using the Ghostscript graphics library stream code
 * for the decompression filters (and some others not strictly decompression).
 *
 * We can think of three 'layers' of access to the raw PDF file and to streams. The
 * PDF 'parser' is responsible for all the file format access, such as finding the xref,
 * reading the xref table, dereferencing objects etc. The basic file opening and consistency
 * checks are in ghostpdf.c, the xref table and object dereferencing is in pdf_xref.c, this includes
 * handling compressed object streams and compressed xref streams. Currently all this code operates on the
 * raw file, we always read the xref up front so compressed xrefs aren't a problem, compressed object
 * streams are read from the start of the stream every time we need an uncached object.
 *
 * The next level down is the PDF 'interpreter', which parses content streams, this code is in
 * pdf_int.c and operates on one of our PDF stream objects, this may include chained compression
 * (and possibly decryption) filters. This code reads PDF tokens, such as strings, numbers and
 * PDF operators and stores them on our stack. This always works with a stream, because content
 * streams are usually compressed. If there are no compression filters then it will be the main
 * file stream but we absolutely must never rely on that.
 *
 * Finally there are the PDF operators themselves, in general thse do not access the stream at all
 * because all operands are presented on the stack. External content streams such as image XObjects
 * and Patterns are handled as a genuinely separate stream. The sole exception is inline images.
 *
 * So generally speakinc the 'parser' operates on the raw file, the 'interpreter' operates on a
 * PDF stream, and the individual operators don't need to access either.
 *
 * When we want to execute another stream we store the current file position, and then move the
 * underlying file pointer. When the substream is completed we move the file pointer back. Because we
 * haven't read any bytes from the original filter chain in the interim its state wil be undisturbed,
 * so as long as the file pointer is properly restored we can pick up exactly where we left off. This
 * neatly allows us to execute substreams without having to decompress the entire parent stream.
 * (see below)
 *
 * Our own PDF stream is defined mainly so that we can 'unread' a few bytes in the PDF interpreter.
 * While scanning for tokens we often read a byte which terminates an object, but also starts a new one,
 * its convenient to be able to 'rewind' the stream to handle this. If the PDF file were uncompressed
 * that wouldn't be a problem, we could use fseek(), but we can't go backwards in a compressed stream
 * without returning to the start of the stream, resetting the decompression filter, and reading up
 * to the point where we wanted to seek. This is because decompression filters generally have 'state'
 * which is modified by the data as it is read. Once some data has been decompressed the state has
 * been altered, and there is generally no way to reverse that change.
 * Rewinding the stream to the start and re-decompressing would of course be terribly slow. Instead we define
 * a buffer and when we want to rewind the stream a little we 'unread' bytes (we need to supply the
 * decompressed byte(s) back to the stream via a 'pdfi_unread' call, it doesn't keep track of these). Currently this has a
 * fixed buffer of 256 bytes, attempting to unread a total of more than that will result in an ioerror.
 *
 * The implication of this is that the PDF interpreter, and the operators, can only progress forwards through
 * a stream. There is limited support for rewinding a stream, provided the code buffers up the data it wants
 * to 'unread', and it doesn't exceed 256 bytes.
 */

#ifndef PDF_FILES
#define PDF_FILES
/*
 * A pdf_c_stream object maintains an 'original' stream memeber. This is only used when closing a file/filter.
 * When we apply filters to a file we supply the stream that we use as the basis for the new stream, this is
 * then stored as the 'original' member. When we close the file, we close all the chained streams until we
 * reach the 'original' member and then exit.
 *
 * This allows us to close filters applied to the PDF file without closing the underlying file/filter which was
 * in effect at the time we created the new stream.
 */

int pdfi_filter(pdf_context *ctx, pdf_stream *stream_obj, pdf_c_stream *source, pdf_c_stream **new_stream, bool inline_image);
/* pdfi_filter_no_decryption is a special function used by the xref parsing when dealing with XRefStms and should not be used
 * for anything else. The pdfi_filter routine will apply decryption as required.
 */
int pdfi_filter_no_decryption(pdf_context *ctx, pdf_stream *d, pdf_c_stream *source, pdf_c_stream **new_stream, bool inline_image);
void pdfi_close_file(pdf_context *ctx, pdf_c_stream *s);
int pdfi_read_bytes(pdf_context *ctx, byte *Buffer, uint32_t size, uint32_t count, pdf_c_stream *s);
int pdfi_read_byte(pdf_context *ctx, pdf_c_stream *s);
int pdfi_unread(pdf_context *ctx, pdf_c_stream *s, byte *Buffer, uint32_t size);
int pdfi_unread_byte(pdf_context *ctx, pdf_c_stream *s, char c);
int pdfi_seek(pdf_context *ctx, pdf_c_stream *s, gs_offset_t offset, uint32_t origin);
gs_offset_t pdfi_unread_tell(pdf_context *ctx);
gs_offset_t pdfi_tell(pdf_c_stream *s);

int pdfi_apply_SubFileDecode_filter(pdf_context *ctx, int EODCount, const char *EODString, pdf_c_stream *source, pdf_c_stream **new_stream, bool inline_image);
int pdfi_open_memory_stream(pdf_context *ctx, unsigned int size, byte **Buffer, pdf_c_stream *source, pdf_c_stream **new_stream);
int pdfi_close_memory_stream(pdf_context *ctx, byte *Buffer, pdf_c_stream *source);
int pdfi_open_memory_stream_from_stream(pdf_context *ctx, unsigned int size, byte **Buffer, pdf_c_stream *source, pdf_c_stream **new_pdf_stream, bool retain_ownership);
int pdfi_open_memory_stream_from_filtered_stream(pdf_context *ctx, pdf_stream *stream_dict, byte **Buffer, pdf_c_stream **new_pdf_stream, bool retain_ownership);
int pdfi_open_memory_stream_from_memory(pdf_context *ctx, unsigned int size, byte *Buffer, pdf_c_stream **new_pdf_stream, bool retain_ownership);
int pdfi_stream_to_buffer(pdf_context *ctx, pdf_stream *stream_dict, byte **buf, int64_t *bufferlen);

int pdfi_apply_Arc4_filter(pdf_context *ctx, pdf_string *Key, pdf_c_stream *source, pdf_c_stream **new_stream);
int pdfi_apply_AES_filter(pdf_context *ctx, pdf_string *Key, bool use_padding, pdf_c_stream *source, pdf_c_stream **new_stream);
int pdfi_apply_imscale_filter(pdf_context *ctx, pdf_string *Key, int width, int height, pdf_c_stream *source, pdf_c_stream **new_stream);

#ifdef UNUSED_FILTER
int pdfi_apply_SHA256_filter(pdf_context *ctx, pdf_c_stream *source, pdf_c_stream **new_stream);
#endif

int pdfi_open_resource_file(pdf_context *ctx, const char *fname, const int fnamelen, stream **s);
bool pdfi_resource_file_exists(pdf_context *ctx, const char *fname, const int fnamelen);
int pdfi_open_font_file(pdf_context *ctx, const char *fname, const int fnamelen, stream **s);
bool pdfi_font_file_exists(pdf_context *ctx, const char *fname, const int fnamelen);

#endif /* PDF_FILES */
