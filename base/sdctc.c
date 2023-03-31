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


/* Code common to DCT encoding and decoding streams */
#include "stdio_.h"
#include "jpeglib_.h"
#include "strimpl.h"
#include "sdct.h"
#include "sjpeg.h"

extern const stream_template s_DCTE_template;
extern const stream_template s_DCTD_template;

static void stream_dct_finalize(const gs_memory_t *cmem, void *vptr);

public_st_DCT_state();

/* Set the defaults for the DCT filters. */
void
s_DCT_set_defaults(stream_state * st)
{
    stream_DCT_state *const ss = (stream_DCT_state *) st;

    ss->jpeg_memory = ss->memory->non_gc_memory;
    ss->data.common = 0;
        /****************
          ss->data.common->Picky = 0;
          ss->data.common->Relax = 0;
         ****************/
    ss->ColorTransform = -1;
    ss->QFactor = 1.0;
    /* Clear pointers */
    ss->Markers.data = 0;
    ss->Markers.size = 0;
}

static void
stream_dct_finalize(const gs_memory_t *cmem, void *vptr)
{
    stream_state *const st = vptr;
    stream_DCT_state *const ss = (stream_DCT_state *) st;
    (void)cmem; /* unused */

    if (st->templat->process == s_DCTE_template.process) {
        gs_jpeg_destroy(ss);
        if (ss->data.compress != NULL) {
            gs_free_object(ss->data.common->memory, ss->data.compress,
                           "s_DCTE_release");
            ss->data.compress = NULL;
        }
        /* Switch the template pointer back in case we still need it. */
        st->templat = &s_DCTE_template;
    }
    else {
        stream_dct_end_passthrough(ss->data.decompress);
        gs_jpeg_destroy(ss);
        if (ss->data.decompress != NULL) {
            if (ss->data.decompress->scanline_buffer != NULL) {
                gs_free_object(gs_memory_stable(ss->data.common->memory),
                               ss->data.decompress->scanline_buffer,
                               "s_DCTD_release(scanline_buffer)");
                ss->data.decompress->scanline_buffer = NULL;
            }
            gs_free_object(ss->data.common->memory, ss->data.decompress,
                       "s_DCTD_release");
            ss->data.decompress = NULL;
        }
        /* Switch the template pointer back in case we still need it. */
        st->templat = &s_DCTD_template;
    }
}

void
stream_dct_end_passthrough(jpeg_decompress_data *jddp)
{
    char EOI[2] = {0xff, 0xD9};

    if (jddp != NULL && jddp->PassThrough != 0 && jddp->PassThroughfn != NULL) {
        (jddp->PassThroughfn)(jddp->device, (byte *)EOI, 2);
        (jddp->PassThroughfn)(jddp->device, NULL, 0);
        jddp->PassThrough = 0;
        jddp->PassThroughfn = NULL;
        jddp->StartedPassThrough = 0;
    }
}
