/* Copyright (C) 2001-2012 Artifex Software, Inc.
   All Rights Reserved.

   This software is provided AS-IS with no warranty, either express or
   implied.

   This software is distributed under license and may not be copied,
   modified or distributed except as expressly authorized under the terms
   of the license contained in the file LICENSE in this distribution.

   Refer to licensing information at http://www.artifex.com or contact
   Artifex Software, Inc.,  7 Mt. Lassen Drive - Suite A-134, San Rafael,
   CA  94903, U.S.A., +1(415)492-9861, for further information.
*/


/* pltoputl.h   Useful utilities for use w/pltop.c interface */

#ifndef pltoputl_INCLUDED
#  define pltoputl_INCLUDED

#include "scommon.h"

/* -------------- Read file cursor operations ---------- */
/*
 * Stream-driven reading cursor
 */
typedef struct pl_top_cursor_s {
        stream_cursor_read         cursor;        /* cursor actually used to read */
        FILE                       *strm;         /* stream that data comes from */
        unsigned char              *buffer;       /* buffer to use */
   unsigned                   buffer_length; /* # bytes in buffer */
        int                        status;        /* if <=0, status to report to caller */
} pl_top_cursor_t;

/* Init a read cursor w/specified open stream */
int pl_top_cursor_init(pl_top_cursor_t *cursor, FILE *strm, byte *buffer, unsigned bufferLength);

/* Refill from input */
int pl_top_cursor_next(pl_top_cursor_t *cursor);

/* Close read cursor */
int pl_top_cursor_close(pl_top_cursor_t *cursor);

/* Deinit a read cursor */
void pl_top_cursor_dnit(pl_top_cursor_t *cursor);

/* renew a cursor if EOD condition has been set.  This can happen if a
   PDL does not consume any data even though data is avaiable */
void pl_renew_cursor_status(pl_top_cursor_t *cursor);

#endif				/* pltoputl_INCLUDED */
