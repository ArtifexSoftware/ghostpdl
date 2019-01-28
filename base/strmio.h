/* Copyright (C) 2001-2019 Artifex Software, Inc.
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


/* Interface for streams that mimic stdio functions (fopen, fread, fseek, ftell) */
/* Requires stream.h */

#ifndef strmio_INCLUDED
#  define strmio_INCLUDED

#include "scommon.h"

/*
 * Open a stream using a filename that can include a PS style IODevice prefix
 * If iodev_default is the '%os' device, then the file will be on the host
 * file system transparently to the caller. The "%os%" prefix can be used
 * to explicilty access the host file system.
 *
 * NOTE: sfopen() always opens files in "binary" mode on systems where that
 * is applicable - so callers should not do so themselves.
 */
stream * sfopen(const char *path, const char *mode, gs_memory_t *mem);

/*
 * Read a number of bytes from a stream, returning number read. Return count
 * will be less than count if EOF or error. Return count is number of elements.
 */
int sfread(void *ptr, size_t size, size_t count, stream *s);

/*
 * Read a byte from a stream
 */
int sfgetc(stream *s);

/*
 * Seek to a position in the stream. Returns the 0, or -1 if error
 */
int sfseek(stream *s, gs_offset_t offset, int whence);

/*
 * Seek to beginning of the file
 */
int srewind(stream *s);

/*
 * Return the current position in the stream or -1 if error.
 */
long sftell(stream *s);

/*
 * Return the EOF status, or 0 if not at EOF.
 */
int sfeof(stream *s);

/*
 * Return the error status, or 0 if no error
 */
int sferror(stream *s);

/*
 * close and free the stream. Any further access (including another call to sfclose())
 * to  the  stream results in undefined behaviour (reference to freed memory);
 */
int sfclose(stream *s);

/* Get a callout-capable stdin stream. */
int gs_get_callout_stdin(stream **ps, gs_memory_t *mem);

#endif /* strmio_INCLUDED */
