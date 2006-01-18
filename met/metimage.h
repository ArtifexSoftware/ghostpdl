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

enum { MT_GRAY, MT_RGB, MT_CMYK, MT_GRAY_A, MT_RGB_A };

/* type for the information derived directly from the raster file format */
typedef struct met_image_s {
    int width;
    int height;
    int stride;
    int colorspace;
    int comps;
    int bits;
    int xres;
    int yres;
    byte *samples;
} met_image_t;

/* it appears all images in metro are really patterns. */
typedef struct met_pattern_s {
    met_image_t *raster_image;
    gs_matrix Transform;
    gs_rect Viewbox;
    gs_rect Viewport;
} met_pattern_t;

#endif				/* metparse_INCLUDED */
