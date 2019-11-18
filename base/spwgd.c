/* Copyright (C) 2017-2018 Artifex Software, Inc.
   All Rights Reserved.

   This software is provided AS-IS with no warranty, either express or
   implied.

   This software is distributed under license and may not be copied,
   modified or distributed except as expressly authorized under the terms
   of the license contained in the file LICENSE in this distribution.

   Refer to licensing information at http://www.artifex.com or contact
   Artifex Software, Inc.,  1305 Grant Avenue - Suite 200, Novato,
   CA 94945, U.S.A., +1(415)492-9861, for further information.
*/


/* PWGDecode filter */
#include "stdio_.h"		/* includes std.h */
#include "memory_.h"
#include "strimpl.h"
#include "spwgx.h"

/* ------ RunLengthDecode ------ */

private_st_PWGD_state();

/* Set defaults */
static void
s_PWGD_set_defaults(stream_state * st)
{
    stream_PWGD_state *const ss = (stream_PWGD_state *) st;

    (ss)->bpp = PWG_default_bpp;
    (ss)->width = PWG_default_width;
}

/* Initialize */
static int
s_PWGD_init(stream_state * st)
{
    stream_PWGD_state *const ss = (stream_PWGD_state *) st;

    (ss)->line_pos = 0;
    (ss)->line_rep = 0;
    (ss)->state = 0;
    (ss)->line_buffer = NULL;

    return 0;
}

/* Refill the buffer */
static int
s_PWGD_process(stream_state * st, stream_cursor_read * pr,
               stream_cursor_write * pw, bool last)
{
    stream_PWGD_state *const ss = (stream_PWGD_state *) st;
    register const byte *p = pr->ptr;
    register byte *q = pw->ptr;
    const byte *rlimit = pr->limit;
    byte *wlimit = pw->limit;
    int bpp = (ss->bpp+7)>>3;
    int wb = ss->width * bpp;
    int line_pos = ss->line_pos;

    if (ss->line_buffer == NULL) {
        ss->line_buffer =
                    gs_alloc_bytes_immovable(gs_memory_stable(ss->memory),
                                             wb,
                                             "s_PWGD_process(line_buffer)");
        if (ss->line_buffer == NULL)
            return ERRC;
    }

    while (1) {
        if (ss->state == 0) {
            /* Copy any buffered data out */
            if (ss->line_rep > 0) {
                int avail = wb - line_pos;
                if (avail > wlimit - q)
                    avail = wlimit - q;
                if (avail != 0)
                    memcpy(q+1, &ss->line_buffer[line_pos], avail);
                line_pos += avail;
                q += avail;
                if (line_pos == wb) {
                    line_pos = 0;
                    ss->line_rep--;
                }
                goto data_produced;
            }
            /* Now unpack data into the line buffer */
            /* Awaiting line repeat value */
            if (p == rlimit)
                goto need_data;
            ss->line_rep = (*++p) + 1;
            ss->state = 1; /* Wait for pixel repeat */
        }
        if (ss->state == 1) {
            int rep;
            /* Awaiting pixel repeat value */
            if (p == rlimit)
                goto need_data;
            rep = *++p;
            if (rep < 0x80) {
                /* Repeat the next pixel multiple times */
                ss->state = (rep+1) * bpp + 1;
                if (line_pos + ss->state - 1 > wb)
                    /* Too many repeats for this line! */
                    goto error;
            } else {
                /* Copy colors */
                ss->state = -(257 - rep) * bpp;
                if (line_pos + -ss->state > wb)
                    /* Too many pixels for this line! */
                    goto error;
            }
        }
        if (ss->state > 1) {
            /* Repeating a single pixel */
            int pixel_pos = line_pos % bpp;
            int avail = bpp - pixel_pos;
            if (avail > rlimit - p)
                avail = rlimit - p;
            if (avail != 0)
                memcpy(&ss->line_buffer[line_pos], p+1, avail);
            p += avail;
            line_pos += avail;
            pixel_pos += avail;
            ss->state -= avail;
            if (pixel_pos != bpp)
                goto need_data;
            while (ss->state > 1) {
                memcpy(&ss->line_buffer[line_pos], &ss->line_buffer[line_pos - bpp], bpp);
                line_pos += bpp;
                ss->state -= bpp;
            }
            if (line_pos == wb) {
                line_pos = 0;
                ss->state = 0;
            } else
                ss->state = 1;
        }
        if (ss->state < 0) {
            /* Copying literals */
            int avail = -ss->state;
            if (avail > rlimit - p)
                avail = rlimit - p;
            memcpy(&ss->line_buffer[line_pos], p + 1, avail);
            p += avail;
            ss->state += avail;
            line_pos += avail;
            if (ss->state)
                goto need_data;
            ss->state = 1;
        }
    }
need_data:
    {
        int status = 0; /* Need input data */
        if (0) {
error:
            status = ERRC;
        } else if (0) {
data_produced:
            status = 1; /* Need output space */
        }
        pr->ptr = p;
        pw->ptr = q;
        ss->line_pos = line_pos;
        return status;
    }
}

static void
s_PWGD_release(stream_state * st)
{
    stream_PWGD_state *const ss = (stream_PWGD_state *) st;

    gs_free_object(st->memory, ss->line_buffer, "PWGD(close)");
    ss->line_buffer = NULL;
}

/* Stream template */
const stream_template s_PWGD_template = {
    &st_PWGD_state, s_PWGD_init, s_PWGD_process, 1, 1, s_PWGD_release,
    s_PWGD_set_defaults
};
