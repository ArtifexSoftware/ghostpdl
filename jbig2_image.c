/*
    jbig2dec
    
    Copyright (c) 2001 artofcode LLC.
    
    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    $Id: jbig2_image.c,v 1.4 2002/06/15 16:02:54 giles Exp $
*/

#include <stdio.h>
#include <stdlib.h>

#include "jbig2.h"
#include "jbig2_image.h"


/* allocate a Jbig2Image structure and its associated bitmap */
Jbig2Image* jbig2_image_new(int width, int height)
{
	Jbig2Image	*image;
	int		stride;
	
	image = malloc(sizeof(*image));
	if (image == NULL) {
		fprintf(stderr, "jbig2dec error: could not allocate image buffer!\n");
		return NULL;
	}
	
	stride = (((width - 1) >> 5) + 1) << 2;	/* generate a word-aligned stride */
	image->data = malloc(stride*height);
	if (image->data == NULL) {
		fprintf(stderr, "jbig2dec error: could not allocate image data buffer!\n");
		free(image);
		return NULL;
	}
	
	image->width = width;
	image->height = height;
	image->stride = stride;
	
	return image;
}


/* free a Jbig2Image structure and its associated memory */
void jbig2_image_free(Jbig2Image *image)
{
	free (image->data);
	free (image);
}
