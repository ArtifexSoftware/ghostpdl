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

/*$RCSfile$ $Revision$ */
/* zlib decoding (decompression) filter stream */
#include "std.h"
#include "gsmemory.h"
#include "gsmalloc.h"		/* for gs_memory_default */
#include "strimpl.h"
#include "szlibxx.h"

/* Initialize the filter. */
private int
s_zlibD_init(stream_state * st)
{
    stream_zlib_state *const ss = (stream_zlib_state *)st;
    int code = s_zlib_alloc_dynamic_state(ss);

    if (code < 0)
	return ERRC;	/****** WRONG ******/
    if (inflateInit2(&ss->dynamic->zstate,
		     (ss->no_wrapper ? -ss->windowBits : ss->windowBits))
	!= Z_OK
	) {
	s_zlib_free_dynamic_state(ss);
	return ERRC;	/****** WRONG ******/
    }
    st->min_left=1;
    return 0;
}

/* Reinitialize the filter. */
private int
s_zlibD_reset(stream_state * st)
{
    stream_zlib_state *const ss = (stream_zlib_state *)st;

    if (inflateReset(&ss->dynamic->zstate) != Z_OK)
	return ERRC;	/****** WRONG ******/
    return 0;
}

/* Process a buffer */
private int
s_zlibD_process(stream_state * st, stream_cursor_read * pr,
		stream_cursor_write * pw, bool ignore_last)
{
    stream_zlib_state *const ss = (stream_zlib_state *)st;
    z_stream *zs = &ss->dynamic->zstate;
    const byte *p = pr->ptr;
    int status;

    /* Detect no input or full output so that we don't get */
    /* a Z_BUF_ERROR return. */
    if (pw->ptr == pw->limit)
	return 1;
    if (pr->ptr == pr->limit)
	return 0;
    zs->next_in = (Bytef *)p + 1;
    zs->avail_in = pr->limit - p;
    zs->next_out = pw->ptr + 1;
    zs->avail_out = pw->limit - pw->ptr;
    status = inflate(zs, Z_PARTIAL_FLUSH);
    pr->ptr = zs->next_in - 1;
    pw->ptr = zs->next_out - 1;
    switch (status) {
	case Z_OK:
	    return (pw->ptr == pw->limit ? 1 : pr->ptr > p ? 0 : 1);
	case Z_STREAM_END:
	    return EOFC;
	default:
	    return ERRC;
    }
}

/* Release the stream */
private void
s_zlibD_release(stream_state * st)
{
    stream_zlib_state *const ss = (stream_zlib_state *)st;

    inflateEnd(&ss->dynamic->zstate);
    s_zlib_free_dynamic_state(ss);
}

/* Stream template */
const stream_template s_zlibD_template = {
    &st_zlib_state, s_zlibD_init, s_zlibD_process, 1, 1, s_zlibD_release,
    s_zlib_set_defaults, s_zlibD_reset
};
