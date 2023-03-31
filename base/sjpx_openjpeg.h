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


/* Definitions for sopj filter (OpenJpeg) */

#ifndef SJPX_OPENJPEG_INCLUDED
#define SJPX_OPENJPEG_INCLUDED

/* Requires scommon.h; strimpl.h if any templates are referenced */

#include "scommon.h"
#include "openjpeg.h"

/* define colorspace enumeration for the decompressed image data */
typedef enum {
  gs_jpx_cs_unset,  /* colorspace hasn't been set */
  gs_jpx_cs_gray,   /* single component grayscale image */
  gs_jpx_cs_rgb,    /* three component (s)RGB image */
  gs_jpx_cs_cmyk,   /* four component CMYK image */
  gs_jpx_cs_indexed /* PDF image wants raw index values */
} gs_jpx_cs;

typedef struct stream_block_s
{
	unsigned char *data;
	unsigned long size;
	unsigned long pos;
	unsigned long fill;
} stream_block;

#define JPXD_PassThrough(proc)\
  int proc(void *d, byte *Buffer, int Size)

/* Stream state for the jpx codec using openjpeg
 * We rely on our finalization call to free the
 * associated handle and pointers.
 */
typedef struct stream_jpxd_state_s
{
    stream_state_common;	/* a define from scommon.h */
    opj_codec_t *codec;
    opj_stream_t *stream;
    opj_image_t *image;
    int width, height, bpp;
    bool samescale;

    gs_jpx_cs colorspace;	/* requested output colorspace */
    bool alpha; /* return opacity channel */

    stream_block sb;

    unsigned long totalbytes; /* output total */
    unsigned long out_offset; /* output bytes already returned previously */

    int **pdata; /* pointers to image data */
    int out_numcomps; /* real number of channels to use */
    int alpha_comp; /* input index of alpha channel */
    int *sign_comps; /* compensate for signed data (signed => unsigned) */

    unsigned char *row_data;

    int PassThrough;                    /* 0 or 1 */
    bool StartedPassThrough;            /* Don't signal multiple starts for the same decode */
    JPXD_PassThrough((*PassThroughfn)); /* We don't want the stream code or
                                         * JPEG code to have to handle devices
                                         * so we use a function at the interpreter level
                                         */
    void *device;                       /* The device we need to send PassThrough data to */
} stream_jpxd_state;

extern const stream_template s_jpxd_template;

#endif
