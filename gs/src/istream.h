/* Copyright (C) 1994, 1995, 1999 Aladdin Enterprises.  All rights reserved.

   This software is licensed to a single customer by Artifex Software Inc.
   under the terms of a specific OEM agreement.
 */

/*$RCSfile$ $Revision$ */
/* Interpreter support procedures for streams */
/* Requires scommon.h */

#ifndef istream_INCLUDED
#  define istream_INCLUDED

/* Procedures exported by zfproc.c */

	/* for zfilter.c - procedure stream initialization */
int sread_proc(P3(ref *, stream **, gs_ref_memory_t *));
int swrite_proc(P3(ref *, stream **, gs_ref_memory_t *));

	/* for interp.c, zfileio.c, zpaint.c - handle a procedure */
	/* callback or an interrupt */
int s_handle_read_exception(P6(i_ctx_t *, int, const ref *, const ref *,
			       int, op_proc_t));
int s_handle_write_exception(P6(i_ctx_t *, int, const ref *, const ref *,
				int, op_proc_t));

#endif /* istream_INCLUDED */
