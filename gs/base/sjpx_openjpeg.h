/* Copyright (C) 2001-2012 Artifex Software, Inc.
   All Rights Reserved.

   This software is provided AS-IS with no warranty, either express or
   implied.

   This software is distributed under license and may not be copied,
   modified or distributed except as expressly authorized under the terms
   of the license contained in the file LICENSE in this distribution.

   Refer to licensing information at http://www.artifex.com or contact
   Artifex Software, Inc.,  7 Mt. Lassen Drive - Suite A-134, San Rafael,
   CA  94903, U.S.A., +1(415)492-9861, for further information.
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

/* Stream state for the jpx codec using openjpeg
 * We rely on our finalization call to free the
 * associated handle and pointers.
 */
typedef struct stream_jpxd_state_s
{
    stream_state_common;	/* a define from scommon.h */
    const gs_memory_t *jpx_memory;
	opj_dinfo_t *opj_dinfo_p;
	opj_image_t *image;
	int width, height, bpp;
	bool samescale;

	gs_jpx_cs colorspace;	/* requested output colorspace */
    bool alpha; /* return opacity channel */

    unsigned char *inbuf;	/* input data buffer */
    unsigned long inbuf_size;
    unsigned long inbuf_fill;

	unsigned long totalbytes; /* output total */
	unsigned long out_offset; /* output bytes already returned previously */
	unsigned long img_offset; /* offset in the image data buffer for each channel, only used when output bpp%8 !=0 */

	int **pdata; /* pointers to image data */
	int out_numcomps; /* real number of channels to use */
	int alpha_comp; /* input index of alpha channel */
	int *sign_comps; /* compensate for signed data (signed => unsigned) */
} stream_jpxd_state;

extern const stream_template s_jpxd_template;

#endif 
