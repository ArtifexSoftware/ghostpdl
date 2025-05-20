/* Copyright (C) 2001-2025 Artifex Software, Inc.
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


/* Bytestream abstraction for Rinkj driver. */

#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include "rinkj-byte-stream.h"

int
rinkj_byte_stream_write (RinkjByteStream *bs, const char *buf, int size)
{
  return bs->write (bs, buf, size);
}

int
rinkj_byte_stream_puts (RinkjByteStream *bs, const char *str)
{
  return bs->write (bs, str, strlen (str));
}

#define MAX_STRING 8192

/* Careful using this function! */
int
rinkj_byte_stream_printf (RinkjByteStream *bs, const char *fmt, ...)
{
  char str[MAX_STRING];
  int len;
  va_list ap;

  va_start (ap, fmt);
  len = vsnprintf (str, sizeof(str), fmt, ap);
  va_end (ap);
  return rinkj_byte_stream_write (bs, str, len);
}

int
rinkj_byte_stream_close (RinkjByteStream *bs)
{
  return bs->write (bs, NULL, 0);
}

/* This module just writes a byte stream to a file. */

typedef struct _RinkjByteStreamFile RinkjByteStreamFile;

struct _RinkjByteStreamFile {
  RinkjByteStream super;
  gp_file *f;
};

static int
rinkj_byte_stream_file_write (RinkjByteStream *self, const char *buf, int size)
{
  RinkjByteStreamFile *z = (RinkjByteStreamFile *)self;
  int status;

  if (size == 0)
    {
#if 1
      status = 0; /* Ghostscript wants to close the file itself. */
#else
      status = fclose (z->f);
#endif
      free (self);
      return status;
    }
  else
    {
#ifdef DEBUG_OUT
      return 0;
#endif
      status = gp_fwrite(buf, 1, size, z->f);
      if (status == size)
        return 0;
      else
        return -1;
    }
}

RinkjByteStream *
rinkj_byte_stream_file_new (gp_file *f)
{
  RinkjByteStreamFile *result;

  result = (RinkjByteStreamFile *)malloc (sizeof (RinkjByteStreamFile));
  if (result == NULL)
      return NULL;

  result->super.write = rinkj_byte_stream_file_write;
  result->f = f;

  return &result->super;
}
