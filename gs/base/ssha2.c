/* Copyright (C) 2009 Artifex Software, Inc.
   All Rights Reserved.

   This software is provided AS-IS with no warranty, either express or
   implied.

   This software is distributed under license and may not be copied, modified
   or distributed except as expressly authorized under the terms of that
   license.  Refer to licensing information at http://www.artifex.com/
   or contact Artifex Software, Inc.,  7 Mt. Lassen Drive - Suite A-134,
   San Rafael, CA  94903, U.S.A., +1(415)492-9861, for further information.
*/

/* $Id$ */
/* SHA256Encode filter */
#include "memory_.h"
#include "strimpl.h"
#include "stream.h"
#include "ssha2.h"

/* ------ SHA256Encode ------ */

private_st_SHA256E_state();

/* Initialize the state. */
static int
s_SHA256E_init(stream_state * st)
{
    stream_SHA256E_state *const ss = (stream_SHA256E_state *) st;

    SHA256_Init(&ss->sha256);
    return 0;
}

/* Process a buffer. */
static int
s_SHA256E_process(stream_state * st, stream_cursor_read * pr,
	       stream_cursor_write * pw, bool last)
{
    stream_SHA256E_state *const ss = (stream_SHA256E_state *) st;
    int status = 0;

    if (pr->ptr < pr->limit) {
	SHA256_Update(&ss->sha256, pr->ptr + 1, pr->limit - pr->ptr);
	pr->ptr = pr->limit;
    }
    if (last) {
	if (pw->limit - pw->ptr >= 32) {
	    SHA256_Final(pw->ptr + 1, &ss->sha256);
	    pw->ptr += 32;
	    status = EOFC;
	} else
	    status = 1;
    }
    return status;
}

/* Stream template */
const stream_template s_SHA256E_template = {
    &st_SHA256E_state, s_SHA256E_init, s_SHA256E_process, 1, 32
};

stream *
s_SHA256E_make_stream(gs_memory_t *mem, byte *digest, int digest_size)
{
    stream *s = s_alloc(mem, "s_SHA256E_make_stream");
    stream_state *ss = s_alloc_state(mem, s_SHA256E_template.stype, "s_SHA256E_make_stream");

    if (ss == NULL || s == NULL)
	goto err;
    ss->template = &s_SHA256E_template;
    if (s_init_filter(s, ss, digest, digest_size, NULL) < 0)
	goto err;
    s->strm = s;
    return s;
err:
    gs_free_object(mem, ss, "s_SHA256E_make_stream");
    gs_free_object(mem, s, "s_SHA256E_make_stream");
    return NULL;
}
