/* Copyright (C) 1994 Aladdin Enterprises.  All rights reserved.
  
  This software is provided AS-IS with no warranty, either express or
  implied.
  
  This software is distributed under license and may not be copied,
  modified or distributed except as expressly authorized under the terms
  of the license contained in the file LICENSE in this distribution.
  
  For more information about licensing, please refer to
  http://www.ghostscript.com/licensing/. For information on
  commercial licensing, go to http://www.artifex.com/licensing/ or
  contact Artifex Software, Inc., 101 Lucas Valley Road #110,
  San Rafael, CA  94903, U.S.A., +1(415)492-9861.
*/

/*$RCSfile$ $Revision$ */
/* IJG entry point wrappers */
/* Requires sdct.h, jpeg/jpeglib.h */

#ifndef sjpeg_INCLUDED
#  define sjpeg_INCLUDED

/*
 * Each routine gs_jpeg_xxx is equivalent to the IJG entry point jpeg_xxx,
 * except that
 *   (a) it takes a pointer to stream_DCT_state instead of just the IJG
 *       jpeg_(de)compress_data struct;
 *   (b) it catches any error exit from the IJG code and converts it into
 *       an error return value per Ghostscript custom.  A negative return
 *       value is an error code, except for gs_jpeg_alloc_xxx which return
 *       NULL (indicating e_VMerror).
 */

/* Common to encode/decode */

void gs_jpeg_error_setup(P1(stream_DCT_state * st));
int gs_jpeg_log_error(P1(stream_DCT_state * st));
JQUANT_TBL *gs_jpeg_alloc_quant_table(P1(stream_DCT_state * st));
JHUFF_TBL *gs_jpeg_alloc_huff_table(P1(stream_DCT_state * st));
int gs_jpeg_destroy(P1(stream_DCT_state * st));

/* Encode */

int gs_jpeg_create_compress(P1(stream_DCT_state * st));
int gs_jpeg_set_defaults(P1(stream_DCT_state * st));
int gs_jpeg_set_colorspace(P2(stream_DCT_state * st,
			      J_COLOR_SPACE colorspace));
int gs_jpeg_set_linear_quality(P3(stream_DCT_state * st,
				  int scale_factor,
				  boolean force_baseline));
int gs_jpeg_set_quality(P3(stream_DCT_state * st,
			   int quality,
			   boolean force_baseline));
int gs_jpeg_start_compress(P2(stream_DCT_state * st,
			      boolean write_all_tables));
int gs_jpeg_write_scanlines(P3(stream_DCT_state * st,
			       JSAMPARRAY scanlines,
			       int num_lines));
int gs_jpeg_finish_compress(P1(stream_DCT_state * st));

/* Decode */

int gs_jpeg_create_decompress(P1(stream_DCT_state * st));
int gs_jpeg_read_header(P2(stream_DCT_state * st,
			   boolean require_image));
int gs_jpeg_start_decompress(P1(stream_DCT_state * st));
int gs_jpeg_read_scanlines(P3(stream_DCT_state * st,
			      JSAMPARRAY scanlines,
			      int max_lines));
int gs_jpeg_finish_decompress(P1(stream_DCT_state * st));

#endif /* sjpeg_INCLUDED */
