/* Portions Copyright (C) 2001 artofcode LLC.
   Portions Copyright (C) 1996, 2001 Artifex Software Inc.
   Portions Copyright (C) 1988, 2000 Aladdin Enterprises.
   This software is based in part on the work of the Independent JPEG Group.
   All Rights Reserved.

   This software is distributed under license and may not be copied, modified
   or distributed except as expressly authorized under the terms of that
   license.  Refer to licensing information at http://www.artifex.com/ or
   contact Artifex Software, Inc., 101 Lucas Valley Road #110,
   San Rafael, CA  94903, (415)492-9861, for further information. */

/*$RCSfile$ $Revision$ */
/* Interface routines for IJG decoding code. */
#include "stdio_.h"
#include "string_.h"
#include "jpeglib_.h"
#include "jerror_.h"
#include "gx.h"
#include "gserrors.h"
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
    if (setjmp(st->data.common->exit_jmpbuf))
	return_error(gs_jpeg_log_error(st));

    jpeg_create_decompress(&st->data.decompress->dinfo);
    jpeg_stream_data_common_init(st->data.decompress);
    return 0;
}

int
gs_jpeg_read_header(stream_DCT_state * st,
		    boolean require_image)
{
    if (setjmp(st->data.common->exit_jmpbuf))
	return_error(gs_jpeg_log_error(st));
    return jpeg_read_header(&st->data.decompress->dinfo, require_image);
}

int
gs_jpeg_start_decompress(stream_DCT_state * st)
{
    if (setjmp(st->data.common->exit_jmpbuf))
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
    if (setjmp(st->data.common->exit_jmpbuf))
	return_error(gs_jpeg_log_error(st));
    return (int)jpeg_read_scanlines(&st->data.decompress->dinfo,
				    scanlines, (JDIMENSION) max_lines);
}

int
gs_jpeg_finish_decompress(stream_DCT_state * st)
{
    if (setjmp(st->data.common->exit_jmpbuf))
	return_error(gs_jpeg_log_error(st));
    return (int)jpeg_finish_decompress(&st->data.decompress->dinfo);
}
