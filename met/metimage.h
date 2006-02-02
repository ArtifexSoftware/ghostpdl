/* Portions Copyright (C) 2001 artofcode LLC.
   Portions Copyright (C) 1996, 2001, 2005 Artifex Software Inc.
   Portions Copyright (C) 1988, 2000 Aladdin Enterprises.
   This software is based in part on the work of the Independent JPEG Group.
   All Rights Reserved.

   This software is distributed under license and may not be copied, modified
   or distributed except as expressly authorized under the terms of that
   license.  Refer to licensing information at http://www.artifex.com/ or
   contact Artifex Software, Inc., 101 Lucas Valley Road #110,
   San Rafael, CA  94903, (415)492-9861, for further information. */

/*$Id$*/

/* metro image interface */

#ifndef metimage_INCLUDED
#  define metimage_INCLUDED

#include "gsmatrix.h"

/* type for the information derived directly from the raster file format */

enum { XPS_GRAY, XPS_GRAY_A, XPS_RGB, XPS_RGB_A, XPS_CMYK, XPS_CMYK_A };

typedef struct xps_image_s xps_image_t;

struct xps_image_s
{
    int width;
    int height;
    int stride;
    int colorspace;
    int comps;
    int bits;
    int xres;
    int yres;
    byte *samples;
};

int xps_decode_jpeg(gs_memory_t *mem, byte *rbuf, int rlen, xps_image_t *image);
int xps_decode_png(gs_memory_t *mem, byte *rbuf, int rlen, xps_image_t *image);
int xps_decode_tiff(gs_memory_t *mem, byte *rbuf, int rlen, xps_image_t *image);

/* it appears all images in metro are really patterns. */

typedef struct met_pattern_s {
    xps_image_t *raster_image;
    gs_matrix Transform;
    gs_rect Viewbox;
    gs_rect Viewport;
} met_pattern_t;

#endif /* metimage_INCLUDED */
