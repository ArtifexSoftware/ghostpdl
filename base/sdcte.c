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


/* DCT encoding filter stream */
#include "memory_.h"
#include "stdio_.h"
#include "jpeglib_.h"
#include "jerror_.h"
#include "gdebug.h"
#include "gsmemory.h"
#include "strimpl.h"
#include "sdct.h"
#include "sjpeg.h"

#define ICC_OVERHEAD  16		
#define MAX_MARKER_DATA_SIZE  (65535 - ICC_OVERHEAD)

public_st_jpeg_compress_data();

/* ------ DCTEncode ------ */

/* JPEG destination manager procedures */
static void
dcte_init_destination(j_compress_ptr cinfo)
{
}
static boolean
dcte_empty_output_buffer(j_compress_ptr cinfo)
{
    return FALSE;
}
static void
dcte_term_destination(j_compress_ptr cinfo)
{
}

/* Set the defaults for the DCTEncode filter. */
static void
s_DCTE_set_defaults(stream_state * st)
{
    stream_DCT_state *const ss = (stream_DCT_state *) st;

    s_DCT_set_defaults(st);
    ss->QFactor = 1.0;
    ss->ColorTransform = -1;
    ss->Markers.data = 0;
    ss->Markers.size = 0;
    ss->NoMarker = true;
}

/* Initialize DCTEncode filter */
static int
s_DCTE_init(stream_state * st)
{
    stream_DCT_state *const ss = (stream_DCT_state *) st;
    struct jpeg_destination_mgr *dest = &ss->data.compress->destination;

    dest->init_destination = dcte_init_destination;
    dest->empty_output_buffer = dcte_empty_output_buffer;
    dest->term_destination = dcte_term_destination;
    ss->data.common->memory = ss->jpeg_memory;
    ss->data.compress->cinfo.dest = dest;
    ss->phase = 0;
    ss->icc_marker = 0;
    ss->icc_position = -1;
    return 0;
}

/* Process a buffer */
static int
s_DCTE_process(stream_state * st, stream_cursor_read * pr,
               stream_cursor_write * pw, bool last)
{
    stream_DCT_state *const ss = (stream_DCT_state *) st;
    jpeg_compress_data *jcdp = ss->data.compress;
    struct jpeg_destination_mgr *dest = jcdp->cinfo.dest;

    if_debug2m('w', st->memory, "[wde]process avail=%u, last=%d\n",
               (uint) (pr->limit - pr->ptr), last);
    dest->next_output_byte = pw->ptr + 1;
    dest->free_in_buffer = pw->limit - pw->ptr;
    switch (ss->phase) {
        case 0:		/* not initialized yet */
            if (gs_jpeg_start_compress(ss, TRUE) < 0)
                return ERRC;
            if_debug4m('w', st->memory, "[wde]width=%u, height=%u, components=%d, scan_line_size=%u\n",
                       jcdp->cinfo.image_width,
                       jcdp->cinfo.image_height,
                       jcdp->cinfo.input_components,
                       ss->scan_line_size);
            pw->ptr = dest->next_output_byte - 1;
            ss->phase = 1;
            /* falls through */
        case 1:		/* initialized, Markers not written */
            if (pw->limit - pw->ptr < ss->Markers.size)
                return 1;
            memcpy(pw->ptr + 1, ss->Markers.data, ss->Markers.size);
            pw->ptr += ss->Markers.size;
            ss->phase = 2;
            /* falls through */
        case 2:		/* still need to write Adobe marker */
            if (!ss->NoMarker) {
                static const byte Adobe[] =
                {
                    0xFF, JPEG_APP0 + 14, 0, 14,	/* parameter length */
                    'A', 'd', 'o', 'b', 'e',
                    0, 100,	/* Version */
                    0, 0,	/* Flags0 */
                    0, 0,	/* Flags1 */
                    0		/* ColorTransform */
                };

#define ADOBE_MARKER_LEN sizeof(Adobe)
                if (pw->limit - pw->ptr < ADOBE_MARKER_LEN)
                    return 1;
                memcpy(pw->ptr + 1, Adobe, ADOBE_MARKER_LEN);
                pw->ptr += ADOBE_MARKER_LEN;
                *pw->ptr = ss->ColorTransform;
#undef ADOBE_MARKER_LEN
            }
            dest->next_output_byte = pw->ptr + 1;
            dest->free_in_buffer = pw->limit - pw->ptr;
            ss->phase = 3;
            /* falls through */
        case  3:     
            /* If we have it, then write out the ICC profile */
            /* Due to size limitations allowed in APP0 markers, the profile 
               may have to be written in mutiple markers */
	        if (ss->icc_profile != NULL) {
		        static const char marker[2] = {0xFF, 0xE2};  /* JPEG_APP0 + 2 */
                byte num_mark;

                /* Number of markers */
                num_mark = ss->icc_profile->buffer_size / MAX_MARKER_DATA_SIZE;
                if (num_mark * MAX_MARKER_DATA_SIZE < ss->icc_profile->buffer_size) {
                    num_mark++;
                }
		        while (ss->icc_marker < num_mark) {
		            ulong offset = ss->icc_marker * MAX_MARKER_DATA_SIZE;
		            ulong size;

                    size = ss->icc_profile->buffer_size - offset;
                    if (size > MAX_MARKER_DATA_SIZE)
                      size = MAX_MARKER_DATA_SIZE;

                    /* In this case we are just getting started with the 
                       header of the marker.  Write that portion out */
		            if (ss->icc_position == -1) {
			            byte length_byte[2];
			            byte curr_mark = ss->icc_marker + 1;
			            ulong total_length;

			            if ((uint) (pw->limit - pw->ptr) < (sizeof(marker) + ICC_OVERHEAD))
		   	                return 1;
			            total_length = size + ICC_OVERHEAD;
			            memcpy(pw->ptr + 1, marker, sizeof(marker));
			            length_byte[0] = total_length >> 8;
			            length_byte[1] = total_length & 0xFF;
			            memcpy(pw->ptr + 3, length_byte, sizeof(length_byte));
			            memcpy(pw->ptr + 5, "ICC_PROFILE", 12); /* Null included */
			            memcpy(pw->ptr + 17, &curr_mark, 1);
			            memcpy(pw->ptr + 18, &num_mark, 1);
			            pw->ptr += sizeof(marker) + ICC_OVERHEAD;
			            ss->icc_position = 0;
		            }
                    /* Now write out the actual profile data */
		            while (ss->icc_position < size) {
			            ulong avail_bytes, num_bytes;

			            avail_bytes = (ulong) (pw->limit - pw->ptr);
			            if (avail_bytes == 0)
			                return 1;
			            num_bytes = (size - ss->icc_position);
			            if (num_bytes > avail_bytes)
			                num_bytes = avail_bytes;
			            memcpy(pw->ptr + 1,  ss->icc_profile->buffer + offset + ss->icc_position, num_bytes);
			            ss->icc_position += num_bytes;
			            pw->ptr += num_bytes;
		            }
                    /* Move on to the next marker */
		            ++ss->icc_marker;
		            ss->icc_position = -1;
	    	    }
	            dest->next_output_byte = pw->ptr + 1;
	            dest->free_in_buffer = pw->limit - pw->ptr;
	        }
	        ss->phase = 4;
	        /* falls through */
        case 4:		/* markers written, processing data */
            while (jcdp->cinfo.image_height > jcdp->cinfo.next_scanline) {
                int written;

                /*
                 * The data argument for jpeg_write_scanlines is
                 * declared as a JSAMPARRAY.  There is no corresponding
                 * const type, so we must remove const from the
                 * argument that we are passing here.  (Tom Lane of IJG
                 * judges that providing const analogues of the
                 * interface types wouldn't be worth the trouble.)
                 */
                /*const */ byte *samples = (byte *) (pr->ptr + 1);

                if_debug1m('w', st->memory, "[wde]next_scanline=%u\n",
                           jcdp->cinfo.next_scanline);
                if ((uint) (pr->limit - pr->ptr) < ss->scan_line_size) {
                    if (last)
                        return ERRC;	/* premature EOD */
                    return 0;	/* need more data */
                }
                written = gs_jpeg_write_scanlines(ss, &samples, 1);
                if (written < 0)
                    return ERRC;
                if_debug3m('w', st->memory, "[wde]write returns %d, used=%u, written=%u\n",
                           written,
                           (uint) (samples - 1 - pr->ptr),
                           (uint) (dest->next_output_byte - 1 - pw->ptr));
                pw->ptr = dest->next_output_byte - 1;
                if (!written)
                    return 1;	/* output full */
                pr->ptr += ss->scan_line_size;
            }
            ss->phase = 5;
            /* falls through */
        case 5:		/* all data processed, finishing */
            /* jpeg_finish_compress can't suspend, so write its output
             * to a fixed-size internal buffer.
             */
            dest->next_output_byte = jcdp->finish_compress_buf;
            dest->free_in_buffer = sizeof(jcdp->finish_compress_buf);
            if (gs_jpeg_finish_compress(ss) < 0)
                return ERRC;
            jcdp->fcb_size =
                dest->next_output_byte - jcdp->finish_compress_buf;
            jcdp->fcb_pos = 0;
            ss->phase = 6;
            /* falls through */
        case 6:		/* copy the final data to the output */
            if (jcdp->fcb_pos < jcdp->fcb_size) {
                int count = min(jcdp->fcb_size - jcdp->fcb_pos,
                                pw->limit - pw->ptr);

                if_debug1m('w', st->memory, "[wde]copying final %d\n", count);
                memcpy(pw->ptr + 1, jcdp->finish_compress_buf + jcdp->fcb_pos,
                       count);
                jcdp->fcb_pos += count;
                pw->ptr += count;
                if (jcdp->fcb_pos < jcdp->fcb_size)
                    return 1;
            }
            return EOFC;
    }
    /* Default case can't happen.... */
    return ERRC;
}

/* Stream template */
const stream_template s_DCTE_template =
{&st_DCT_state, s_DCTE_init, s_DCTE_process, 1000, 4000, NULL,
 s_DCTE_set_defaults
};
