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


/* Interface routines for IJG decoding code. */
#include "stdio_.h"
#include "string_.h"
#include "jpeglib_.h"
#include "jerror_.h"
#include "gserrors.h"
#include "gx.h"
#include "strimpl.h"
#include "sdct.h"
#include "sjpeg.h"

/*
 * Interface routines.  This layer of routines exists solely to limit
 * side-effects from using setjmp.
 */

int
gs_jpeg_create_decompress(stream_DCT_state * st)
{				/* Initialize error handling */
    gs_jpeg_error_setup(st);
    /* Establish the setjmp return context for gs_jpeg_error_exit to use. */
    if (setjmp(find_jmp_buf(st->data.common->exit_jmpbuf)))
        return_error(gs_jpeg_log_error(st));

    jpeg_stream_data_common_init(st->data.decompress);

    if (gs_jpeg_mem_init (st->memory, (j_common_ptr)&st->data.decompress->dinfo) < 0)
        return_error(gs_error_VMerror);

    jpeg_create_decompress(&st->data.decompress->dinfo);
    return 0;
}

int
gs_jpeg_read_header(stream_DCT_state * st,
                    boolean require_image)
{
    if (setjmp(find_jmp_buf(st->data.common->exit_jmpbuf)))
        return_error(gs_jpeg_log_error(st));
    return jpeg_read_header(&st->data.decompress->dinfo, require_image);
}

int
gs_jpeg_start_decompress(stream_DCT_state * st)
{
    if (setjmp(find_jmp_buf(st->data.common->exit_jmpbuf)))
        return_error(gs_jpeg_log_error(st));
#if JPEG_LIB_VERSION > 55
    return (int)jpeg_start_decompress(&st->data.decompress->dinfo);
#else
    /* in IJG version 5, jpeg_start_decompress had no return value */
    jpeg_start_decompress(&st->data.decompress->dinfo);
    return 1;
#endif
}

int
gs_jpeg_read_scanlines(stream_DCT_state * st,
                       JSAMPARRAY scanlines,
                       int max_lines)
{
    if (setjmp(find_jmp_buf(st->data.common->exit_jmpbuf)))
        return_error(gs_jpeg_log_error(st));
    return (int)jpeg_read_scanlines(&st->data.decompress->dinfo,
                                    scanlines, (JDIMENSION) max_lines);
}

int
gs_jpeg_finish_decompress(stream_DCT_state * st)
{
    int code = 0;
    if (setjmp(find_jmp_buf(st->data.common->exit_jmpbuf)))
        code = gs_note_error(gs_jpeg_log_error(st));
    if (code >= 0)
        code = (int)jpeg_finish_decompress(&st->data.decompress->dinfo);
    stream_dct_end_passthrough(st->data.decompress);
    return code;
}
