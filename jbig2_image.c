/*
    jbig2dec
    
    Copyright (c) 2001-2002 artofcode LLC.
    
    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    $Id: jbig2_image.c,v 1.14 2002/07/03 21:51:09 raph Exp $
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
	
	stride = ((width - 1) >> 3) + 1; /* generate a byte-aligned stride */
	image->data = (uint8_t *)jbig2_alloc(ctx->allocator, stride*height);
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

/* composite one jbig2_image onto another */
int jbig2_image_compose(Jbig2Ctx *ctx, Jbig2Image *dst, Jbig2Image *src, 
			int x, int y, Jbig2ComposeOp op)
{
    int w, h;
    int leftbyte, rightbyte;
    int leftbits, rightbits;
    int i, j;
    uint8_t *s, *ss;
    uint8_t *d, *dd;
    uint8_t mask, highmask;
    
    if (op != JBIG2_COMPOSE_OR) {
        jbig2_error(ctx, JBIG2_SEVERITY_WARNING, -1,
            "non-OR composition modes NYI");
    }

    /* clip */
    w = src->width;
    h = src->height;
    ss = src->data;
    // FIXME: this isn't sufficient for the < 0 cases
    if (x < 0) { w += x; x = 0; }
    if (y < 0) { h += y; y = 0; } 
    w = (x + w < dst->width) ? w : dst->width - x;
    h = (y + h < dst->height) ? h : dst->height - y;
#ifdef DEBUG    
    fprintf(stderr, "composting %dx%d at (%d, %d) afer clipping\n",
        w, h, x, y);
#endif
    
    /* special case complete/strip replacement */
    if ((x == 0) && (w == src->width)) {
        memcpy(dst->data + y*dst->stride, src->data, h*src->stride);
        return 0;
    }

    leftbyte = x >> 3;
    leftbits = x & 7;
    rightbyte = (x + w - 1) >> 3;
    rightbits = (x + w) & 7;
    fprintf(stderr, "left byte:bits %d:%d right byte:bits %d:%d\n",
        leftbyte, leftbits, rightbyte, rightbits);

    /* general OR case */    
    s = ss;
    d = dd = dst->data + y*dst->stride + leftbyte;
    if (leftbyte == rightbyte) {
	mask = 0x100 - (0x100 >> w);
        for (j = 0; j < h; j++) {
            *d |= (*s & mask) >> leftbits;
            d += dst->stride;
            s += src->stride;
        }
    } else if (leftbits == 0) {
        for (j = 0; j < h; j++) {
	    for (i = leftbyte; i <= rightbyte; i++)
		*d++ |= *s++;
            d = (dd += dst->stride);
            s = (ss += src->stride);
	}
    } else {
	mask = 0x100 - (1 << leftbits);
	if (((w + 7) >> 3) < ((x + w + 7) >> 3) - (x >> 3))
	    highmask = (0x100 - (0x100 >> rightbits)) >> (8 - leftbits);
	else
	    highmask = 0x100 - (0x100 >> (w & 7));
        for (j = 0; j < h; j++) {
	    *d++ |= (*s & mask) >> leftbits;
            for(i = leftbyte; i < rightbyte - 1; i++) {
		*d |= ((s[0] & ~mask) << (8 - leftbits)) |
		    ((s[1] & mask) >> leftbits);
		s++;
		d++;
	    }
	    if (((w + 7) >> 3) < ((x + w + 7) >> 3) - (x >> 3))
		*d |= (s[0] & highmask) << (8 - leftbits);
	    else
		*d |= ((s[0] & ~mask) << (8 - leftbits)) |
		    ((s[1] & highmask) >> leftbits);
	    d = (dd += dst->stride);
	    s = (ss += src->stride);
	}
    }
            
    return 0;
}
