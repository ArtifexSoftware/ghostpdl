/*
    jbig2dec
    
    Copyright (c) 2001-2002 artofcode LLC.
    
    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    $Id: jbig2_image.h,v 1.3 2002/06/04 16:51:02 giles Exp $
*/

/*
   this is the general image structure used by the jbig2dec library
   images are 1 bpp, packed into word-aligned rows. stride gives
   the word offset to the next row, while width and height define
   the size of the image area in pixels.
*/

#ifndef _JBIG2_IMAGE_H
#define _JBIG2_IMAGE_H

#include <stdint.h>

typedef struct _Jbig2Image {
	int		width, height, stride;
	uint32_t	*data;
} Jbig2Image;

Jbig2Image*	jbig2_image_new(int width, int height);
void		jbig2_image_free(Jbig2Image *image);

/* routines for dumping the image data in various formats */
/* FIXME: should these be in the client instead? */

int jbig2_image_write_pbm_file(Jbig2Image *image, char *filename);
int jbig2_image_write_pbm(Jbig2Image *image, FILE *out);
Jbig2Image *jbig2_image_read_pbm_file(char *filename);
Jbig2Image *jbig2_image_read_pbm(FILE *in);

#ifdef HAVE_LIBPNG
int jbig2_image_write_png_file(Jbig2Image *image, char *filename);
int jbig2_image_write_png(Jbig2Image *image, FILE *out);
#endif

#endif /* _JBIG2_IMAGE_H */