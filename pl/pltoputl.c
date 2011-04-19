/* Portions Copyright (C) 2001 artofcode LLC.
   Portions Copyright (C) 1996, 2001 Artifex Software Inc.
   Portions Copyright (C) 1988, 2000 Aladdin Enterprises.
   This software is based in part on the work of the Independent JPEG Group.
   All Rights Reserved.

   This software is distributed under license and may not be copied, modified
   or distributed except as expressly authorized under the terms of that
   license.  Refer to licensing information at http://www.artifex.com/ or
   contact Artifex Software, Inc., 101 Lucas Valley Road #110,
   San Rafael, CA  94903, (415)492-9861, for further information. */
/*$Id$ */

#include "string_.h"
#include "gdebug.h"
#include "gstypes.h"
#include "gsmemory.h"
#include "gsstruct.h"
#include "pltoputl.h"

/* -------------- Read stream cursor operations ---------- */

/* Initialize cursor */
int	/* returns 0 ok, else -ve error code */
pl_top_cursor_init(
        pl_top_cursor_t  *cursor,        /* cursor to init/open */
        FILE             *strm,          /* open stream to read from */
        byte             *buffer,        /* buffer to use for reading */
        unsigned         buffer_length   /* length of *buffer */
)
{
        int status;
        cursor->strm = strm;
        cursor->buffer = buffer;
        cursor->buffer_length = buffer_length;
        cursor->cursor.limit = cursor->cursor.ptr = buffer - 1;
        cursor->status = 1;   /* non-status */

        status = pl_top_cursor_next(cursor);
        return status < 0 ? status : 0;	/* report errors, not EOF */
}

/* End of data condition. */
static bool
pl_cursor_EOD(pl_top_cursor_t *cursor)
{

    /* NB review history of the second predicate. */
    return (cursor->status <= 0 && cursor->cursor.ptr <= cursor->buffer);
}

void
pl_renew_cursor_status(pl_top_cursor_t *cursor)
{
    if (pl_cursor_EOD(cursor)) {
        cursor->status = 1;
    }
}

/* Refill from input */
int    /* rets 1 ok, else 0 EOF, -ve error */
pl_top_cursor_next(
        pl_top_cursor_t *cursor       /* cursor to operate on */
)
{
        int len;

        /* Declare EOF even if chars left in buffer if no chars were consumed */
        if (pl_cursor_EOD(cursor))
            return cursor->status;

        /* Copy any remaining bytes to head of buffer */
        len = cursor->cursor.limit - cursor->cursor.ptr;
        if (len > 0)
          memmove(cursor->buffer, cursor->cursor.ptr + 1, len);
        cursor->cursor.ptr = cursor->buffer - 1;
        cursor->cursor.limit = cursor->buffer + (len - 1);

        /* Top off rest of buffer by reading stream */
        if (cursor->status > 0 && len < cursor->buffer_length) {
            cursor->status = fread((byte *)(cursor->cursor.limit + 1),
                                   1, cursor->buffer_length - len,
                                   cursor->strm);
            if (cursor->status > 0)
                cursor->cursor.limit += cursor->status;
        }

        /* Return success if there's anything in the buffer */
        return cursor->cursor.limit == cursor->cursor.ptr ? cursor->status : 1;
}

/* Deinit a read cursor */
void
pl_top_cursor_dnit(
        pl_top_cursor_t *cursor       /* cursor to operate on */
)
{
        return;
}
