/* Copyright (C) 1995, 1996, 1997 Aladdin Enterprises.  All rights reserved.

   This file is part of Aladdin Ghostscript.

   Aladdin Ghostscript is distributed with NO WARRANTY OF ANY KIND.  No author
   or distributor accepts any responsibility for the consequences of using it,
   or for whether it serves any particular purpose or works at all, unless he
   or she says so in writing.  Refer to the Aladdin Ghostscript Free Public
   License (the "License") for full details.

   Every copy of Aladdin Ghostscript must include a copy of the License,
   normally in a plain ASCII text file named PUBLIC.  The License grants you
   the right to copy, modify and redistribute Aladdin Ghostscript, but only
   under certain conditions described in the License.  Among other things, the
   License requires that the copyright notice and this notice be preserved on
   all copies.
 */

/* szlibe.c */
/* zlib encoding (compression) filter stream */
#include "std.h"
#include "gsmemory.h"
#include "strimpl.h"
#include "szlibx.h"

#define ss ((stream_zlib_state *)st)
#define szs (&ss->zstate)

/* Initialize the filter. */
private int
s_zlibE_init(stream_state * st)
{
    szs->zalloc = (alloc_func) s_zlib_alloc;
    szs->zfree = (free_func) s_zlib_free;
    szs->opaque = (voidpf) (st->memory ? st->memory : &gs_memory_default);
    if (deflateInit2(szs, ss->level, ss->method,
		     (ss->no_wrapper ? -ss->windowBits : ss->windowBits),
		     ss->memLevel, ss->strategy) != Z_OK)
	return ERRC;
/****** WRONG ******/
    return 0;
}

/* Reinitialize the filter. */
private int
s_zlibE_reset(stream_state * st)
{
    if (deflateReset(szs) != Z_OK)
	return ERRC;
/****** WRONG ******/
    return 0;
}

/* Process a buffer */
private int
s_zlibE_process(stream_state * st, stream_cursor_read * pr,
		stream_cursor_write * pw, bool last)
{
    const byte *p = pr->ptr;
    int status;

    /* Detect no input or full output so that we don't get */
    /* a Z_BUF_ERROR return. */
    if (pw->ptr == pw->limit)
	return 1;
    if (p == pr->limit && !last)
	return 0;
    szs->next_in = (Bytef *) p + 1;
    szs->avail_in = pr->limit - p;
    szs->next_out = pw->ptr + 1;
    szs->avail_out = pw->limit - pw->ptr;
    status = deflate(szs, (last ? Z_FINISH : Z_NO_FLUSH));
    pr->ptr = szs->next_in - 1;
    pw->ptr = szs->next_out - 1;
    switch (status) {
	case Z_OK:
	    return (pw->ptr == pw->limit ? 1 : pr->ptr > p && !last ? 0 : 1);
	case Z_STREAM_END:
	    return (last && pr->ptr == pr->limit ? 0 : ERRC);
	default:
	    return ERRC;
    }
}

/* Release the stream */
private void
s_zlibE_release(stream_state * st)
{
    deflateEnd(szs);
}

/* Stream template */
const stream_template s_zlibE_template =
{&st_zlib_state, s_zlibE_init, s_zlibE_process, 1, 1, s_zlibE_release,
 s_zlib_set_defaults, s_zlibE_reset
};

#undef ss
