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


/* brotli decoding (decompression) filter stream */
#include "memory_.h"
#include "std.h"
#include "strimpl.h"
#include "sbrotlix.h"

#include "brotli/decode.h"

/* Initialize the filter. */
static int
s_brotliD_init(stream_state * st)
{
    stream_brotlid_state *ss = (stream_brotlid_state *)st;

    ss->dec_state = BrotliDecoderCreateInstance(
                                    s_brotli_alloc,
                                    s_brotli_free,
                                    st->memory->non_gc_memory);
    if (ss->dec_state == NULL)
        return ERRC;
    st->min_left=1;
    return 0;
}

/* Process a buffer */
static int
s_brotliD_process(stream_state * st, stream_cursor_read * pr,
                stream_cursor_write * pw, bool ignore_last)
{
    stream_brotlid_state *const ss = (stream_brotlid_state *)st;
    const byte *p = pr->ptr;
    const byte *inBuf  = p + 1;
    size_t      inLen  = pr->limit - p;
    byte       *outBuf = pw->ptr + 1;
    size_t      outLen = pw->limit - pw->ptr;
    BrotliDecoderResult status;

    status = BrotliDecoderDecompressStream((BrotliDecoderState *)ss->dec_state,
                                         &inLen, /* Available in */
                                         &inBuf, /* Input buffer */
                                         &outLen, /* Available out */
                                         &outBuf, /* Output buffer */
                                         NULL);
    pr->ptr = inBuf - 1;
    pw->ptr = outBuf - 1;
    switch (status) {
        case BROTLI_DECODER_RESULT_NEEDS_MORE_INPUT:
            return 0;
        case BROTLI_DECODER_RESULT_NEEDS_MORE_OUTPUT:
            return 1;
        case BROTLI_DECODER_RESULT_SUCCESS:
            return EOFC;
        default:
            return ERRC;
    }
}

/* Release the stream */
static void
s_brotliD_release(stream_state * st)
{
    stream_brotlid_state *ss = (stream_brotlid_state *)st;

    if (ss->dec_state != NULL)
    {
        BrotliDecoderDestroyInstance((BrotliDecoderState *)ss->dec_state);
        ss->dec_state = NULL;
    }
}

static void
brotliD_final(const gs_memory_t *mem, void *st)
{
    stream_brotlid_state *ss = (stream_brotlid_state *)st;

    if (ss->dec_state != NULL)
    {
        BrotliDecoderDestroyInstance((BrotliDecoderState *)ss->dec_state);
        ss->dec_state = NULL;
    }
}

gs_public_st_simple_final(st_brotlid_state, stream_brotlid_state,
        "brotli decode state", brotliD_final);

/* Stream template */
const stream_template s_brotliD_template = {
    &st_brotlid_state, s_brotliD_init, s_brotliD_process, 1, 1, s_brotliD_release
};
