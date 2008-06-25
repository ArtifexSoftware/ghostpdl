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
