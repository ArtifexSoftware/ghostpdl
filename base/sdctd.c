/* Copyright (C) 2001-2019 Artifex Software, Inc.
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


/* DCT decoding filter stream */
#include "memory_.h"
#include "stdio_.h"
#include "jpeglib_.h"
#include "jerror_.h"
#include "gdebug.h"
#include "gsmemory.h"
#include "strimpl.h"
#include "sdct.h"
#include "sjpeg.h"

/* ------ DCTDecode ------ */

/* JPEG source manager procedures */
static void
dctd_init_source(j_decompress_ptr dinfo)
{
}
static const JOCTET fake_eoi[2] =
{0xFF, JPEG_EOI};
static boolean
dctd_fill_input_buffer(j_decompress_ptr dinfo)
{
    jpeg_decompress_data *jddp =
    (jpeg_decompress_data *) ((char *)dinfo -
                              offset_of(jpeg_decompress_data, dinfo));

    if (!jddp->input_eod)
        return FALSE;		/* normal case: suspend processing */
    /* Reached end of source data without finding EOI */
    WARNMS(dinfo, JWRN_JPEG_EOF);
    /* Insert a fake EOI marker */
    dinfo->src->next_input_byte = fake_eoi;
    dinfo->src->bytes_in_buffer = 2;
    jddp->faked_eoi = true;	/* so process routine doesn't use next_input_byte */
    return TRUE;
}
static void
dctd_skip_input_data(j_decompress_ptr dinfo, long num_bytes)
{
    struct jpeg_source_mgr *src = dinfo->src;
    jpeg_decompress_data *jddp =
    (jpeg_decompress_data *) ((char *)dinfo -
                              offset_of(jpeg_decompress_data, dinfo));

    if (num_bytes > 0) {
        if (num_bytes > src->bytes_in_buffer) {
            jddp->skip += num_bytes - src->bytes_in_buffer;
            src->next_input_byte += src->bytes_in_buffer;
            src->bytes_in_buffer = 0;
            return;
        }
        src->next_input_byte += num_bytes;
        src->bytes_in_buffer -= num_bytes;
    }
}

static void
dctd_term_source(j_decompress_ptr dinfo)
{
    jpeg_decompress_data *jddp =
    (jpeg_decompress_data *) ((char *)dinfo -
                              offset_of(jpeg_decompress_data, dinfo));

    stream_dct_end_passthrough(jddp);
    return;
}

/* Set the defaults for the DCTDecode filter. */
static void
s_DCTD_set_defaults(stream_state * st)
{
    s_DCT_set_defaults(st);
}

/* Initialize DCTDecode filter */
static int
s_DCTD_init(stream_state * st)
{
    stream_DCT_state *const ss = (stream_DCT_state *) st;
    struct jpeg_source_mgr *src = &ss->data.decompress->source;

    src->init_source = dctd_init_source;
    src->fill_input_buffer = dctd_fill_input_buffer;
    src->skip_input_data = dctd_skip_input_data;
    src->term_source = dctd_term_source;
    src->resync_to_restart = jpeg_resync_to_restart;	/* use default method */
    ss->data.common->memory = ss->jpeg_memory;
    ss->data.decompress->dinfo.src = src;
    ss->data.decompress->skip = 0;
    ss->data.decompress->input_eod = false;
    ss->data.decompress->faked_eoi = false;
    ss->phase = 0;
    return 0;
}

static int
compact_jpeg_buffer(stream_cursor_read *pr)
{
    byte *o, *i;

    /* Search backwards from the end for 2 consecutive 0xFFs */
    o = (byte *)pr->limit;
    while (o - pr->ptr >= 2) {
        if (*o-- == 0xFF) {
            if (*o == 0xFF)
                goto compact;
            o--;
        }
    }
    return 0;
compact:
    i = o-1;
    do {
        /* Skip i backwards over 0xFFs */
        while ((i != pr->ptr) && (*i == 0xFF))
            i--;
        /* Repeatedly copy from i to o */
        while (i != pr->ptr) {
            byte c = *i--;
            *o-- = c;
            if (c == 0xFF)
                break;
        }
    } while (i != pr->ptr);

    pr->ptr = o;
    return o - i;
}

static void
update_jpeg_header_height(JOCTET *d, size_t len, int height)
{
    int marker_len;

    for (d += 2; len > 9 && d[0] == 0xFF; d += marker_len)
    {
        int declared_height;

        marker_len = 2 + (d[2] << 8) + d[3];
        if (marker_len > len)
            break;
        len -= marker_len;

        /* We can only safely rewrite non-differential SOF markers */
        if (d[1] < 0xC0 || (0xC3 < d[1] && d[1] < 0xC9) || 0xCB < d[1])
            continue;

        declared_height = (d[5]<<8) | d[6];
        if (declared_height == 0 || declared_height > height)
        {
            d[5] = height>>8;
            d[6] = height;
        }
    }
}

/* Process a buffer */
static int
s_DCTD_process(stream_state * st, stream_cursor_read * pr,
               stream_cursor_write * pw, bool last)
{
    stream_DCT_state *const ss = (stream_DCT_state *) st;
    jpeg_decompress_data *jddp = ss->data.decompress;
    struct jpeg_source_mgr *src = jddp->dinfo.src;
    int code;
    byte *Buf;

    if_debug3m('w', st->memory, "[wdd]process avail=%u, skip=%u, last=%d\n",
               (uint) (pr->limit - pr->ptr), (uint) jddp->skip, last);
    if (jddp->skip != 0) {
        long avail = pr->limit - pr->ptr;

        if (avail < jddp->skip) {
            if (jddp->PassThrough && jddp->PassThroughfn)
                (jddp->PassThroughfn)(jddp->device, (byte *)pr->ptr + 1, (byte *)pr->limit - (byte *)pr->ptr);

            jddp->skip -= avail;
            pr->ptr = pr->limit;
            if (!last)
                return 0;	/* need more data */
            jddp->skip = 0;	/* don't skip past input EOD */
        }
        Buf = (byte *)pr->ptr + 1;
        pr->ptr += jddp->skip;
        if (jddp->PassThrough && jddp->PassThroughfn)
            (jddp->PassThroughfn)(jddp->device, Buf, pr->ptr - (Buf - 1));

        jddp->skip = 0;
    }
    src->next_input_byte = pr->ptr + 1;
    src->bytes_in_buffer = pr->limit - pr->ptr;
    Buf = (byte *)pr->ptr + 1;
    jddp->input_eod = last;
    switch (ss->phase) {
        case 0:		/* not initialized yet */
            /*
             * Adobe implementations seem to ignore leading garbage bytes,
             * even though neither the standard nor Adobe's own
             * documentation mention this.
             */
            if (jddp->PassThrough && jddp->PassThroughfn && !jddp->StartedPassThrough) {
                jddp->StartedPassThrough = 1;
                (jddp->PassThroughfn)(jddp->device, NULL, 1);
            }
            while (pr->ptr < pr->limit && pr->ptr[1] != 0xff)
                pr->ptr++;
            if (pr->ptr == pr->limit) {
                if (jddp->PassThrough && jddp->PassThroughfn)
                    (jddp->PassThroughfn)(jddp->device, Buf, pr->ptr - (Buf - 1));
                return 0;
            }
            src->next_input_byte = pr->ptr + 1;
            src->bytes_in_buffer = pr->limit - pr->ptr;
            ss->phase = 1;
            /* falls through */
        case 1:		/* reading header markers */
            if (ss->data.common->Height != 0)
            {
               /* Deliberate and naughty. We cast away a const pointer
                * here and write to a supposedly read-only stream. */
                union { const byte *c; byte *u; } u;
                u.c = pr->ptr+1;
                update_jpeg_header_height(u.u, src->bytes_in_buffer, ss->data.common->Height);
            }
            if ((code = gs_jpeg_read_header(ss, TRUE)) < 0)
                return ERRC;
            pr->ptr =
                (jddp->faked_eoi ? pr->limit : src->next_input_byte - 1);
            switch (code) {
                case JPEG_SUSPENDED:
                    if (jddp->PassThrough && jddp->PassThroughfn)
                        (jddp->PassThroughfn)(jddp->device, Buf, pr->ptr - (Buf - 1));
                    return 0;
                    /*case JPEG_HEADER_OK: */
            }

            /* 
             * Default the color transform if not set and check for
             * the Adobe marker and use Adobe's transform if the
             * marker is set.
             */
            if (ss->ColorTransform == -1) {
                if (jddp->dinfo.num_components == 3)
                    ss->ColorTransform = 1;
                else
                    ss->ColorTransform = 0;
            }
            
            if (jddp->dinfo.saw_Adobe_marker)
                ss->ColorTransform = jddp->dinfo.Adobe_transform;
            
            switch (jddp->dinfo.num_components) {
            case 3:
                jddp->dinfo.jpeg_color_space =
                    (ss->ColorTransform ? JCS_YCbCr : JCS_RGB);
                        /* out_color_space will default to JCS_RGB */
                        break;
            case 4:
                jddp->dinfo.jpeg_color_space =
                    (ss->ColorTransform ? JCS_YCCK : JCS_CMYK);
                /* out_color_space will default to JCS_CMYK */
                break;
            }
            ss->phase = 2;
            /* falls through */
        case 2:		/* start_decompress */
            if ((code = gs_jpeg_start_decompress(ss)) < 0)
                return ERRC;
            pr->ptr =
                (jddp->faked_eoi ? pr->limit : src->next_input_byte - 1);
            if (code == 0) {
                if (jddp->PassThrough && jddp->PassThroughfn)
                    (jddp->PassThroughfn)(jddp->device, Buf, pr->ptr - (Buf - 1));
                return 0;
            }
            ss->scan_line_size =
                jddp->dinfo.output_width * jddp->dinfo.output_components;
            if_debug4m('w', ss->memory, "[wdd]width=%u, components=%d, scan_line_size=%u, min_out_size=%u\n",
                       jddp->dinfo.output_width,
                       jddp->dinfo.output_components,
                       ss->scan_line_size, jddp->templat.min_out_size);
            if (ss->scan_line_size > (uint) jddp->templat.min_out_size) {
                /* Create a spare buffer for oversize scanline */
                jddp->scanline_buffer =
                    gs_alloc_bytes_immovable(gs_memory_stable(jddp->memory),
                                             ss->scan_line_size,
                                         "s_DCTD_process(scanline_buffer)");
                if (jddp->scanline_buffer == NULL)
                    return ERRC;
            }
            jddp->bytes_in_scanline = 0;
            ss->phase = 3;
            /* falls through */
        case 3:		/* reading data */
          dumpbuffer:
            if (jddp->bytes_in_scanline != 0) {
                uint avail = pw->limit - pw->ptr;
                uint tomove = min(jddp->bytes_in_scanline,
                                  avail);

                if_debug2m('w', ss->memory, "[wdd]moving %u/%u\n",
                           tomove, avail);
                memcpy(pw->ptr + 1, jddp->scanline_buffer +
                       (ss->scan_line_size - jddp->bytes_in_scanline),
                       tomove);
                pw->ptr += tomove;
                jddp->bytes_in_scanline -= tomove;
                /* calculate room after the copy,
                 * PXL typically provides room 1 exactly 1 scan, so avail == 0
                 * PDF/PS provide enough room, so avail >= 0
                 * XPS provides room ro complete image, and expects complet image copied
                 * PCL,PXL,PDF,PS copy 1 scan at a time.
                 */
                avail -= tomove;
                if ((jddp->bytes_in_scanline != 0) || /* no room for complete scan */
                    ((jddp->bytes_in_scanline == 0) && (tomove > 0) && /* 1 scancopy completed */
                     (avail < tomove) && /* still room for 1 more scan */
                     (jddp->dinfo.output_height > jddp->dinfo.output_scanline))) /* more scans to do */
                {
                     if (jddp->PassThrough && jddp->PassThroughfn) {
                        (jddp->PassThroughfn)(jddp->device, Buf, pr->ptr - (Buf - 1));
                    }
                    return 1;	/* need more room */
                }
            }
            /* while not done with image, decode 1 scan, otherwise fall into phase 4 */
            while (jddp->dinfo.output_height > jddp->dinfo.output_scanline) {
                int read;
                byte *samples;

                if (jddp->scanline_buffer != NULL)
                    samples = jddp->scanline_buffer;
                else {
                    if ((uint) (pw->limit - pw->ptr) < ss->scan_line_size) {
                        if (jddp->PassThrough && jddp->PassThroughfn) {
                            (jddp->PassThroughfn)(jddp->device, Buf, pr->ptr - (Buf - 1));
                        }
                        return 1;	/* need more room */
                    }
                    samples = pw->ptr + 1;
                }
                read = gs_jpeg_read_scanlines(ss, &samples, 1);
                if (read < 0)
                    return ERRC;
                if_debug3m('w', ss->memory, "[wdd]read returns %d, used=%u, faked_eoi=%d\n",
                           read,
                           (uint) (src->next_input_byte - 1 - pr->ptr),
                           (int)jddp->faked_eoi);
                pr->ptr =
                    (jddp->faked_eoi ? pr->limit : src->next_input_byte - 1);
                if (!read) {
                    /* We are suspending. If nothing was consumed, and the
                     * buffer was full, compact the data in the buffer. If
                     * this fails to save anything, then we'll never succeed;
                     * throw an error to avoid an infinite loop.
                     * The tricky part here is knowing "if the buffer is
                     * full"; we do that by comparing the number of bytes in
                     * the buffer with the min_in_size set for the stream.
                     */
                    /* TODO: If we ever find a file with valid data that trips
                     * this test, we should implement a scheme whereby we keep
                     * a local buffer and copy the data into it. The local
                     * buffer can be grown as required. */
                    if ((src->next_input_byte-1 == pr->ptr) &&
                        (pr->limit - pr->ptr >= ss->templat->min_in_size) &&
                        (compact_jpeg_buffer(pr) == 0))
                        return ERRC;
                    if (jddp->PassThrough && jddp->PassThroughfn) {
                        (jddp->PassThroughfn)(jddp->device, Buf, pr->ptr - (Buf - 1));
                    }
                    return 0;	/* need more data */
                }
                if (jddp->scanline_buffer != NULL) {
                    jddp->bytes_in_scanline = ss->scan_line_size;
                    goto dumpbuffer;
                }
                pw->ptr += ss->scan_line_size;
            }
            ss->phase = 4;
            /* falls through */
        case 4:		/* end of image; scan for EOI */
            if (jddp->PassThrough && jddp->PassThroughfn)
                (jddp->PassThroughfn)(jddp->device, Buf, pr->ptr - (Buf - 1));
            if ((code = gs_jpeg_finish_decompress(ss)) < 0)
                return ERRC;
            pr->ptr =
                (jddp->faked_eoi ? pr->limit : src->next_input_byte - 1);
            if (code == 0)
                return 0;
            ss->phase = 5;
            /* falls through */
        case 5:		/* we are DONE */
            return EOFC;
    }
    /* Default case can't happen.... */
    return ERRC;
}

/* Stream template */
const stream_template s_DCTD_template =
{&st_DCT_state, s_DCTD_init, s_DCTD_process, 2000, 4000, NULL,
 s_DCTD_set_defaults
};
