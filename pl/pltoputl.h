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
int pl_top_cursor_init(P4(pl_top_cursor_t *cursor, FILE *strm, byte *buffer, unsigned bufferLength));

/* Refill from input */
int pl_top_cursor_next(P1(pl_top_cursor_t *cursor));

/* Close read cursor */
int pl_top_cursor_close(P1(pl_top_cursor_t *cursor));

/* Deinit a read cursor */
void pl_top_cursor_dnit(P1(pl_top_cursor_t *cursor));

#endif				/* pltoputl_INCLUDED */
