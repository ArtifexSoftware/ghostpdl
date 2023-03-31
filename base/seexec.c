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


/* eexec filters */
#include "stdio_.h"		/* includes std.h */
#include "strimpl.h"
#include "sfilter.h"
#include "gscrypt1.h"
#include "scanchar.h"

/* ------ eexecEncode ------ */

/* Encoding is much simpler than decoding, because we don't */
/* worry about initial characters or hex vs. binary (the client */
/* has to take care of these aspects). */

private_st_exE_state();

/* Process a buffer */
static int
s_exE_process(stream_state * st, stream_cursor_read * pr,
              stream_cursor_write * pw, bool last)
{
    stream_exE_state *const ss = (stream_exE_state *) st;
    const byte *p = pr->ptr;
    byte *q = pw->ptr;
    uint rcount = pr->limit - p;
    uint wcount = pw->limit - q;
    uint count;
    int status;

    if (rcount <= wcount)
        count = rcount, status = 0;
    else
        count = wcount, status = 1;
    gs_type1_encrypt(q + 1, p + 1, count, (crypt_state *)&ss->cstate);
    pr->ptr += count;
    pw->ptr += count;
    return status;
}

/* Stream template */
const stream_template s_exE_template = {
    &st_exE_state, NULL, s_exE_process, 1, 2
};

/* ------ eexecDecode ------ */

private_st_exD_state();

/* Set defaults. */
static void
s_exD_set_defaults(stream_state * st)
{
    stream_exD_state *const ss = (stream_exD_state *) st;

    ss->binary = -1;		/* unknown */
    ss->lenIV = 4;
    ss->hex_left = max_long;
    /* Clear pointers for GC */
    ss->pfb_state = 0;
    ss->keep_spaces = false;    /* PS mode */
    ss->is_leading_space = true;
}

/* Initialize the state for reading and decrypting. */
/* Decrypting streams are not positionable. */
static int
s_exD_init(stream_state * st)
{
    stream_exD_state *const ss = (stream_exD_state *) st;

    ss->odd = -1;
    ss->skip = ss->lenIV;
    return 0;
}

/* Process a buffer. */
static int
s_exD_process(stream_state * st, stream_cursor_read * pr,
              stream_cursor_write * pw, bool last)
{
    stream_exD_state *const ss = (stream_exD_state *) st;
    const byte *p = pr->ptr;
    byte *q = pw->ptr;
    int skip = ss->skip;
    int rcount = pr->limit - p;
    int wcount = pw->limit - q;
    int status = 0;
    int count = (wcount < rcount ? (status = 1, wcount) : rcount);

    if (ss->binary < 0) {
        /*
         * This is the very first time we're filling the buffer.
         */
        const byte *const decoder = scan_char_decoder;
        int i;

        if (ss->pfb_state == 0 && !ss->keep_spaces) {
            /*
             * Skip '\t', '\r', '\n', ' ' at the beginning of the input stream,
             * because Adobe PS interpreters do this. Don't skip '\0' or '\f'.
             * Acrobat Reader doesn't skip spaces at all.
             */
            for (; rcount; rcount--, p++) {
                byte c = p[1];
                if(c != '\t' && c != char_CR && c != char_EOL && c != ' ')
                    break;
            }
            pr->ptr = p;
            count = min(wcount, rcount);
        }

        /*
         * Determine whether this is ASCII or hex encoding.
         * Adobe's documentation doesn't actually specify the test
         * that eexec should use, but we believe the following
         * gives correct answers even on certain non-conforming
         * PostScript files encountered in practice:
         */
        if (rcount < 8 && !last)
            return 0;

        ss->binary = 0;
        for (i = min(8, rcount); i > 0; i--)
            if (!(decoder[p[i]] <= 0xf ||
                  decoder[p[i]] == ctype_space)
                ) {
                ss->binary = 1;
                break;
            }
    }
    if (ss->binary) {
        /*
         * There is no need to pause at the end of the binary portion.
         * The padding bytes (which are in the text portion, in hexadecimal)
         * do their job, provided the write buffer is <= 256 bytes long.
         * This is (hopefully) ensured by the comment just above the
         * definition of s_exD_template.
         */
        pr->ptr = p + count;
    } else {
        /*
         * We only ignore leading whitespace, in an attempt to
         * keep from reading beyond the end of the encrypted data;
         * but some badly coded files require us to ignore % also.
         */
        stream_cursor_read r;
        const byte *start;

hp:	r = *pr;
        start = r.ptr;
        if (r.limit - r.ptr > ss->hex_left)
            r.limit = r.ptr + ss->hex_left;
        status = s_hex_process(&r, pw, &ss->odd,
          (ss->is_leading_space ? hex_ignore_leading_whitespace : hex_break_on_whitespace));
        if (status == 2) {
            ss->is_leading_space = true;
            status = 1;
        } else
            ss->is_leading_space = false;
        pr->ptr = r.ptr;
        ss->hex_left -= r.ptr - start;
        /*
         * Check for having finished a prematurely decoded hex section of
         * a PFB file.
         */
        if (ss->hex_left == 0)
            ss->binary = 1;
        count = pw->ptr - q;
        if (status < 0 && ss->odd < 0) {
            if (count) {
                --p;
                status = 0;	/* reprocess error next time */
            } else if (p > pr->ptr && p < pr->limit && *p == '%')
                goto hp;	/* ignore % */
        }
        p = q;
    }
    if (skip >= count && skip != 0) {
        gs_type1_decrypt(q + 1, p + 1, count,
                         (crypt_state *) & ss->cstate);
        ss->skip -= count;
        count = 0;
        status = 0;
    } else {
        gs_type1_decrypt(q + 1, p + 1, skip,
                         (crypt_state *) & ss->cstate);
        count -= skip;
        gs_type1_decrypt(q + 1, p + 1 + skip, count,
                         (crypt_state *) & ss->cstate);
        ss->skip = 0;
    }
    pw->ptr = q + count;
    return status;
}

/* Stream template */
/*
 * The specification of eexec decoding requires that it never read more than
 * 512 source bytes ahead.  The only reliable way to ensure this is to
 * limit the size of the output buffer to 256.  We set it a little smaller
 * so that it will stay under the limit even after adding min_in_size
 * for a subsequent filter in a pipeline.  Note that we have to specify
 * a size of at least 128 so that filter_read won't round it up.
 * The value of 132 is samll enough for the sample file of the bug 689577 but
 * still sufficient for comparefiles/fonttest.pdf .
 */
const stream_template s_exD_template = {
    &st_exD_state, s_exD_init, s_exD_process, 8, 132,
    NULL, s_exD_set_defaults
};
