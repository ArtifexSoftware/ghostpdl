/* Copyright (C) 2001-2023 Artifex Software, Inc.
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


/* Interpreter support procedures for streams */
/* Requires scommon.h */

#ifndef istream_INCLUDED
#  define istream_INCLUDED

#include "scommon.h"
#include "imemory.h"

/* Procedures exported by zfproc.c */

        /* for zfilter.c - procedure stream initialization */
int sread_proc(ref *, stream **, gs_ref_memory_t *);
int swrite_proc(ref *, stream **, gs_ref_memory_t *);

        /* for interp.c, zfileio.c, zpaint.c - handle a procedure */
        /* callback or an interrupt */
int s_handle_read_exception(i_ctx_t *, int, const ref *, const ref *,
                            int, op_proc_t);
int s_handle_write_exception(i_ctx_t *, int, const ref *, const ref *,
                             int, op_proc_t);

#endif /* istream_INCLUDED */
