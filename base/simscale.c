/* Copyright (C) 2001-2024 Artifex Software, Inc.
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


/* Image mask interpolation filter */

#include "memory_.h"
#include "gserrors.h"
#include "strimpl.h"
#include "sisparam.h"
#include "simscale.h"
#include "simscale_foo.h"

gs_private_st_ptrs2(st_imscale_state, stream_imscale_state,
                    "ImscaleDecode state",
                    imscale_state_enum_ptrs, imscale_state_reloc_ptrs,
                    window, dst);

static void
s_imscale_release(stream_state *st)
{
    stream_imscale_state *const ss = (stream_imscale_state *) st;
    gs_memory_t *mem = ss->memory;

    gs_free_object(mem, (void *)ss->window, "imscale window");
    ss->window = 0;
    gs_free_object(mem, (void *)ss->dst, "imscale dst");
    ss->dst = 0;
}

static int
s_imscale_init(stream_state *st)
{
    stream_imscale_state *const ss = (stream_imscale_state *) st;
    gs_memory_t *mem = ss->memory;
    int bytesin = (ss->params.WidthIn + 7) >> 3;
    int bytesout = (ss->params.WidthIn + 1) >> 1;

    ss->src_y = 0;
    ss->src_offset = 0;
    ss->src_size = bytesin;
    ss->src_line_padded = bytesin + 10;

    ss->dst_line_padded = bytesout + 10; /* to compensate for overshoots in zoom() */
    ss->dst_line_size = bytesout;
    ss->dst_size = bytesout*4;
    ss->dst_offset = ss->dst_size;
    ss->dst_togo = (long long)ss->dst_size * ss->params.HeightIn;
    ss->window = (byte *)gs_alloc_byte_array(mem, ss->src_line_padded, 5, "imscale window");
    ss->dst = (byte *)gs_alloc_bytes(mem, ss->dst_line_padded * 4, "imscale dst");
    if (ss->window == NULL || ss->dst == NULL)
        return_error(gs_error_VMerror);
    memset(ss->window, 0xff, ss->src_line_padded * 5);
    return 0;
}

static void
zoom_line(stream_imscale_state *ss) {
    /* src_y is 2 scan lines ahead of dst_y/4, although the latter counter is implicit.
     * For instance, during the 1st call to this function, src_y == 2, dst_y == 0.
     * (src_y + 3) % 5 == 0 and points to the beginning of the window.
     * (src_y + 4) % 5 == 1 and points to the next line.
     * (src_y    ) % 5 == 2 and points to the last scanned line.
     * The next 2 lines in the window correspond to the blank lines above the first
     * line of the image.
     */
	unsigned char * const p0 = ss->window + (ss->src_line_padded * ((ss->src_y + 1) % 5));
	unsigned char * const p1 = ss->window + (ss->src_line_padded * ((ss->src_y + 2) % 5));
	unsigned char * const p2 = ss->window + (ss->src_line_padded * ((ss->src_y + 3) % 5));
	unsigned char * const p3 = ss->window + (ss->src_line_padded * ((ss->src_y + 4) % 5));
	unsigned char * const p4 = ss->window + (ss->src_line_padded * ((ss->src_y    ) % 5));

	/* Pointers to the lines in the destination buffer. */
	unsigned char * const dst0 = ss->dst;
	unsigned char * const dst1 = ss->dst + ss->dst_line_padded;
	unsigned char * const dst2 = ss->dst + 2*ss->dst_line_padded;
	unsigned char * const dst3 = ss->dst + 3*ss->dst_line_padded;
    unsigned int i;

    /* r0..r4 are shift registers that contain 5x5 bit matrix and serve
     * as buffers for byte-based access to memory. The registers have
     * the following structure and initial content.
     * r0: ........ ........ ......11 XXXxxxxx
     * r1: ........ ........ .11XXXxx xxx00000
     * r2: ........ ....11XX Xxxxxxyy yyyyyy00
     * r3: .......1 1XXXxxxx xyyyyyyy y0000000
     * r3: ..11XXXx xxxxyyyy yyyyzzzz zzzz0000
     * where
     * '.' denotes an unused bit
     * '1' denotes the initial blank bits that precede leading bits of every line
     * 'X' denotes leading bits of the 1st byte. '1' and 'X' belong to 5x5 bit matrix
     * 'x' denotes remaining bits of the 1st byte
     * 'y','z' denote the positions of the following bytes
     * '0' denotes the initial empty bits
     */
    uint32_t r0  = 0x300 | p0[0];
    uint32_t r1  = 0x6000 | p1[0] << 5;
    uint32_t r2  = 0xc0000 | p2[0] << 10 | p2[1] << 2;
    uint32_t r3  = 0x1800000 | p3[0] << 15 | p3[1] << 7;
    uint32_t r4  = 0x30000000 | p4[0] << 20 | p4[1] << 12 | p4[2] << 4;

#define ZOOM(r0, r1, r2, r3, r4) imscale_foo((r0 & 0x3e0) | (r1 & 0x7c00) | (r2 & 0xf8000) | (r3 & 0x1f00000) | (r4 & 0x3e000000))
#define SHIFT(r0, r1, r2, r3, r4) r0 <<= 1, r1 <<= 1, r2 <<= 1, r3 <<= 1, r4 <<= 1
#define LOAD(n,i) r##n |= p##n[i]
#define STORE(i)  dst0[i] = out, dst1[i] = out >> 8, dst2[i] = out >> 16, dst3[i] = out >> 24

    /* Possible improvement: buffer output in a 64-bit accumulator and write 16-bit chunks. */
    /* Unfortunately in this case big- and little-endian systems need different code. */
    for (i=0; i < ss->src_size; i++) {
        uint32_t out;                         /* 0 5 2 7 4 : number of empty bits in r0..r4 */

        out = ZOOM(r0, r1, r2, r3, r4) << 4;
        SHIFT(r0, r1, r2, r3, r4);            /* 1 6 3 8 5 : every counter increases by 1 */
        LOAD(3, i+2);                         /* 1 6 3 0 5 : load r3 because it has 8 empty bits */
        out |= ZOOM(r0, r1, r2, r3, r4);
        SHIFT(r0, r1, r2, r3, r4);            /* 2 7 4 1 6 : and so on */
        STORE(4*i);
        out = ZOOM(r0, r1, r2, r3, r4) << 4;
        SHIFT(r0, r1, r2, r3, r4);            /* 3 8 5 2 7 */
        LOAD(1, i+1);                         /* 3 0 5 2 7 */
        out |= ZOOM(r0, r1, r2, r3, r4);
        SHIFT(r0, r1, r2, r3, r4);            /* 4 1 6 3 8 */
        STORE(4*i+1);
        LOAD(4, i+3);                         /* 4 1 6 3 0 */
        out = ZOOM(r0, r1, r2, r3, r4) << 4;
        SHIFT(r0, r1, r2, r3, r4);            /* 5 2 7 4 1 */
        out |= ZOOM(r0, r1, r2, r3, r4);
        STORE(4*i+2);
        SHIFT(r0, r1, r2, r3, r4);            /* 6 3 8 5 2 */
        LOAD(2, i+2);                         /* 6 3 0 5 2 */
        out = ZOOM(r0, r1, r2, r3, r4) << 4;
        SHIFT(r0, r1, r2, r3, r4);            /* 7 4 1 6 3 */
        out |= ZOOM(r0, r1, r2, r3, r4);
        STORE(4*i+3);
        SHIFT(r0, r1, r2, r3, r4);            /* 8 5 2 7 4 */
        LOAD(0, i+1);                         /* 0 5 2 7 4 */
    }
#undef ZOOM
#undef SHIFT
#undef LOAD
#undef STORE
}


static int
s_imscale_process(stream_state *st, stream_cursor_read *pr,
                stream_cursor_write *pw, bool last)
{
    stream_imscale_state *const ss = (stream_imscale_state *) st;

    while (1) {
        if (ss->dst_togo <= 0)
            return EOFC;
    	/* deliver data from dst buffer */
        if (ss->dst_offset < ss->dst_size) {
            uint ncopy = min(pw->limit - pw->ptr, ss->dst_size - ss->dst_offset);

            if (ncopy == 0)
                return 1;
            ss->dst_togo -= ncopy;

            while (ncopy) {
                int line = ss->dst_offset / ss->dst_line_size;
                int offset = ss->dst_offset % ss->dst_line_size;
                int linecopy = min(ncopy, ss->dst_line_size - offset);

                memcpy(pw->ptr + 1, (byte *)ss->dst + line * ss->dst_line_padded  + offset, linecopy);
                pw->ptr += linecopy;
                ss->dst_offset += linecopy;
                ncopy -= linecopy;
            }
        }

        /* output a row, if possible */
        if (ss->dst_offset == ss->dst_size &&  /* dst is empty */
        		ss->src_offset == ss->src_size) { /* src is full */
        	if (ss->src_y >= 2) {
        	    zoom_line(ss);
                ss->dst_offset = 0;
        	}
            ss->src_offset = 0;
            ss->src_y += 1;
        }

        /* input into window */
        if (ss->src_offset < ss->src_size) {
            uint rleft = pr->limit - pr->ptr;
            uint ncopy = min(rleft, ss->src_size - ss->src_offset);

            if (ss->src_y >= ss->params.HeightIn) {
                last = true;
                ncopy = 0;
            }
            if (rleft == 0 && !last)
                return 0; /* need more input */
            if (ncopy) {
                memcpy(ss->window + ss->src_line_padded * (ss->src_y % 5) + ss->src_offset, pr->ptr + 1, ncopy);
                ss->src_offset += ncopy;
                pr->ptr += ncopy;
            } else {
                memset(ss->window + ss->src_line_padded * (ss->src_y % 5) + ss->src_offset, 0xff, ss->src_size - ss->src_offset);
                ss->src_offset = ss->src_size;
            }
        }
    }
}

const stream_template s_imscale_template = {
    &st_imscale_state, s_imscale_init, s_imscale_process, 1, 1,
    s_imscale_release
};
