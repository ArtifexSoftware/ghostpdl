/*
    jbig2dec
    
    Copyright (c) 2001 artofcode LLC.
    
    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    $Id: jbig2_image.c,v 1.5 2002/06/17 16:30:20 giles Exp $
*/

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>

#include "jbig2.h"
#include "jbig2_priv.h"
#include "jbig2_image.h"


/* allocate a Jbig2Image structure and its associated bitmap */
Jbig2Image* jbig2_image_new(Jbig2Ctx *ctx, int width, int height)
{
	Jbig2Image	*image;
	int		stride;
	
	image = (Jbig2Image *)jbig2_alloc(ctx->allocator, sizeof(*image));
	if (image == NULL) {
		jbig2_error(ctx, JBIG2_SEVERITY_FATAL, -1,
			       "could not allocate image structure");
		return NULL;
	}
	
	stride = (((width - 1) >> 5) + 1) << 2;	/* generate a word-aligned stride */
	image->data = (uint32_t *)jbig2_alloc(ctx->allocator, (stride*height));
	if (image->data == NULL) {
                jbig2_error(ctx, JBIG2_SEVERITY_FATAL, -1,
                    "could not allocate image data buffer! [%d bytes]\n", stride*height);
		jbig2_free(ctx->allocator, image);
		return NULL;
	}
	
	image->width = width;
	image->height = height;
	image->stride = stride;
	
	return image;
}


/* free a Jbig2Image structure and its associated memory */
void jbig2_image_free(Jbig2Ctx *ctx, Jbig2Image *image)
{
	jbig2_free(ctx->allocator, image->data);
	jbig2_free(ctx->allocator, image);
}
