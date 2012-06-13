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

/* $Id: gslt_image.h 2713 2007-01-02 22:22:14Z henrys $*/
/* gslt image loading interface */

#ifndef gslt_image_INCLUDED
#  define gslt_image_INCLUDED

/* primary image object type */
typedef struct gslt_image_s gslt_image_t;

/* colorspace enumeration for image data */
typedef enum {
    GSLT_GRAY,
    GSLT_GRAY_A,
    GSLT_RGB,
    GSLT_RGB_A,
    GSLT_CMYK,
    GSLT_CMYK_A,
    GSLT_UNDEFINED  /* sentinel for the last defined colorspace */
} gslt_image_colorspace;

/* definition of the image object structure */
struct gslt_image_s {
    int width;	/* image width */
    int height; /* image height */
    int stride; /* byte offset between image data rows */
    int components; /* number of components (channels) per pixel */
    int bits;	/* bits per component */
    int xres;	/* horizontal image resolution in pixels per meter */
    int yres;	/* vertical image resolution in pixels per meter */
    byte *samples; /* image data buffer */
    gslt_image_colorspace colorspace; /* image pixel component mapping */
};

/* decode an image from a memory buffer */
gslt_image_t *gslt_image_decode(gs_memory_t *mem, byte *buf, int len);

/* free an image object when it is no longer needed */
void gslt_image_free(gs_memory_t *mem, gslt_image_t *image);

/* decode a memory buffer as a particular image format */
gslt_image_t *gslt_image_decode_jpeg(gs_memory_t *mem, byte *buf, int len);
gslt_image_t *gslt_image_decode_png( gs_memory_t *mem, byte *buf, int len);
gslt_image_t *gslt_image_decode_tiff(gs_memory_t *mem, byte *buf, int len);

#endif /* gslt_image_INCLUDED */
