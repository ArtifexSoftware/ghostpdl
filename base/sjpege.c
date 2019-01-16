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


/* Interface routines for IJG encoding code. */
#include "stdio_.h"
#include "string_.h"
#include "jpeglib_.h"
#include "jerror_.h"
#include "gx.h"
#include "strimpl.h"
#include "sdct.h"
#include "sjpeg.h"

/*
 * Interface routines.  This layer of routines exists solely to limit
 * side-effects from using setjmp.
 */

int
gs_jpeg_create_compress(stream_DCT_state * st)
{				/* Initialize error handling */
    gs_jpeg_error_setup(st);
    /* Establish the setjmp return context for gs_jpeg_error_exit to use. */
    if (setjmp(find_jmp_buf(st->data.common->exit_jmpbuf)))
        return_error(gs_jpeg_log_error(st));

    jpeg_stream_data_common_init(st->data.compress);

    if (gs_jpeg_mem_init (st->memory, (j_common_ptr)&st->data.compress->cinfo) < 0)
        return_error(gs_error_VMerror);

    jpeg_create_compress(&st->data.compress->cinfo);
    return 0;
}

int
gs_jpeg_set_defaults(stream_DCT_state * st)
{
    if (setjmp(find_jmp_buf(st->data.common->exit_jmpbuf)))
        return_error(gs_jpeg_log_error(st));
    jpeg_set_defaults(&st->data.compress->cinfo);
    return 0;
}

int
gs_jpeg_set_colorspace(stream_DCT_state * st,
                       J_COLOR_SPACE colorspace)
{
    if (setjmp(find_jmp_buf(st->data.common->exit_jmpbuf)))
        return_error(gs_jpeg_log_error(st));
    jpeg_set_colorspace(&st->data.compress->cinfo, colorspace);
    return 0;
}

int
gs_jpeg_set_linear_quality(stream_DCT_state * st,
                           int scale_factor, boolean force_baseline)
{
    if (setjmp(find_jmp_buf(st->data.common->exit_jmpbuf)))
        return_error(gs_jpeg_log_error(st));
    jpeg_set_linear_quality(&st->data.compress->cinfo,
                            scale_factor, force_baseline);
    return 0;
}

int
gs_jpeg_set_quality(stream_DCT_state * st,
                    int quality, boolean force_baseline)
{
    if (setjmp(find_jmp_buf(st->data.common->exit_jmpbuf)))
        return_error(gs_jpeg_log_error(st));
    jpeg_set_quality(&st->data.compress->cinfo,
                     quality, force_baseline);
    return 0;
}

int
gs_jpeg_start_compress(stream_DCT_state * st,
                       boolean write_all_tables)
{
    if (setjmp(find_jmp_buf(st->data.common->exit_jmpbuf)))
        return_error(gs_jpeg_log_error(st));
    jpeg_start_compress(&st->data.compress->cinfo, write_all_tables);
    return 0;
}

int
gs_jpeg_write_scanlines(stream_DCT_state * st,
                        JSAMPARRAY scanlines,
                        int num_lines)
{
    if (setjmp(find_jmp_buf(st->data.common->exit_jmpbuf)))
        return_error(gs_jpeg_log_error(st));
    return (int)jpeg_write_scanlines(&st->data.compress->cinfo,
                                     scanlines, (JDIMENSION) num_lines);
}

int
gs_jpeg_finish_compress(stream_DCT_state * st)
{
    if (setjmp(find_jmp_buf(st->data.common->exit_jmpbuf)))
        return_error(gs_jpeg_log_error(st));
    jpeg_finish_compress(&st->data.compress->cinfo);
    return 0;
}
