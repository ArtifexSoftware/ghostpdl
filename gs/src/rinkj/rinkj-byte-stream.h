/* Copyright (C) 2000-2004 artofcode LLC.  All rights reserved.
  
  This software is provided AS-IS with no warranty, either express or
  implied.
  
  This software is distributed under license and may not be copied,
  modified or distributed except as expressly authorized under the terms
  of the license contained in the file LICENSE in this distribution.
  
  For more information about licensing, please refer to
  http://www.ghostscript.com/licensing/. For information on
  commercial licensing, go to http://www.artifex.com/licensing/ or
  contact Artifex Software, Inc., 101 Lucas Valley Road #110,
  San Rafael, CA  94903, U.S.A., +1(415)492-9861.
*/

/*$Id$ */
/* Bytestream abstraction for Rinkj driver. */

typedef struct _RinkjByteStream RinkjByteStream;

struct _RinkjByteStream {
  int (*write) (RinkjByteStream *self, const char *buf, int size);
};

int
rinkj_byte_stream_write (RinkjByteStream *bs, const char *buf, int size);

int
rinkj_byte_stream_puts (RinkjByteStream *bs, const char *str);

int
rinkj_byte_stream_printf (RinkjByteStream *bs, const char *fmt, ...);

int
rinkj_byte_stream_close (RinkjByteStream *bs);


RinkjByteStream *
rinkj_byte_stream_file_new (FILE *f);
