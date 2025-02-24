/* Copyright (C) 2025 Artifex Software, Inc.
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


/* brotli encoding (compression) filter stream */
#include "std.h"
#include "strimpl.h"
#include "sbrotlix.h"

#include "brotli/encode.h"

/* Set defaults for stream parameters. */
void
s_brotliE_set_defaults(stream_state * st)
{
    stream_brotlie_state *ss = (stream_brotlie_state *)st;

    ss->mode = BROTLI_MODE_GENERIC;
    ss->windowBits = BROTLI_MAX_WINDOW_BITS;
    ss->level = 9; /* BROTLI_MAX_QUALITY is 11. 9 is 'better than zlib' */
}

/* Initialize the filter. */
static int
s_brotliE_init(stream_state * st)
{
    stream_brotlie_state *ss = (stream_brotlie_state *)st;
    int code;

    ss->enc_state = BrotliEncoderCreateInstance(s_brotli_alloc,
						s_brotli_free,
						st->memory->non_gc_memory);
    if (ss->enc_state == NULL)
        return ERRC;	/****** WRONG ******/

    if (BrotliEncoderSetParameter((BrotliEncoderState *)ss->enc_state,
				  BROTLI_PARAM_MODE,
				  ss->mode) == BROTLI_FALSE)
        goto bad;
    if (BrotliEncoderSetParameter((BrotliEncoderState *)ss->enc_state,
				  BROTLI_PARAM_QUALITY,
				  ss->level) == BROTLI_FALSE)
        goto bad;
    if (BrotliEncoderSetParameter((BrotliEncoderState *)ss->enc_state,
				  BROTLI_PARAM_LGWIN,
				  ss->windowBits) == BROTLI_FALSE)
        goto bad;

    return 0;

bad:
    BrotliEncoderDestroyInstance((BrotliEncoderState *)ss->enc_state);
    ss->enc_state = NULL;
    return ERRC;
}

/* Process a buffer */
static int
s_brotliE_process(stream_state * st, stream_cursor_read * pr,
                  stream_cursor_write * pw, bool last)
{
    stream_brotlie_state *const ss = (stream_brotlie_state *)st;
    const byte *p      = pr->ptr;
    const byte *inBuf  = p + 1;
    size_t      inLen  = pr->limit - p;
    byte       *outBuf = pw->ptr + 1;
    size_t      outLen = pw->limit - pw->ptr;
    BROTLI_BOOL status;

    status = BrotliEncoderCompressStream(
                     (BrotliEncoderState *)ss->enc_state,
                     last ? BROTLI_OPERATION_FINISH : BROTLI_OPERATION_PROCESS,
                     &inLen,
                     &inBuf,
                     &outLen,
                     &outBuf,
                     NULL);

    pr->ptr = inBuf - 1;
    pw->ptr = outBuf - 1;
    switch (status) {
        case BROTLI_TRUE:
            if (last)
            {
                if (BrotliEncoderIsFinished((BrotliEncoderState *)ss->enc_state))
                    return 0;
               return 1;
            }
            else if (pw->ptr == pw->limit)
                return 1; /* No output space left */
            else
                return 0; /* Anything else interpreted as "Need more input" */
        default:
            return ERRC;
    }
}

/* Release the stream */
static void
s_brotliE_release(stream_state * st)
{
    stream_brotlie_state *ss = (stream_brotlie_state *)st;

    if (ss->enc_state != NULL)
    {
        BrotliEncoderDestroyInstance((BrotliEncoderState *)ss->enc_state);
        ss->enc_state = NULL;
    }
}

static void
brotliE_final(const gs_memory_t *mem, void *st)
{
    stream_brotlie_state *ss = (stream_brotlie_state *)st;

    if (ss->enc_state != NULL)
    {
        BrotliEncoderDestroyInstance((BrotliEncoderState *)ss->enc_state);
        ss->enc_state = NULL;
    }
}

gs_public_st_simple_final(st_brotlie_state, stream_brotlie_state,
  "brotli encode state", brotliE_final);

/* Stream template */
const stream_template s_brotliE_template = {
    &st_brotlie_state, s_brotliE_init, s_brotliE_process, 1, 1, s_brotliE_release,
    s_brotliE_set_defaults
};
