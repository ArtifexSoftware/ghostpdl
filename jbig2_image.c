/*
    jbig2dec
    
    Copyright (c) 2001-2002 artofcode LLC.
    
    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    $Id: jbig2_image.c,v 1.11 2002/07/03 14:56:15 giles Exp $
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
	image->data = (uint32_t *)jbig2_alloc(ctx->allocator, stride*height);
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
    int src_stride, dst_stride;
    int leftword, rightword;
    int leftbits, rightbits;
    int row, word, shift, i;
    uint32_t *s, *d;
    uint32_t mask, highmask;
    
    if (op != JBIG2_COMPOSE_OR) {
        jbig2_error(ctx, JBIG2_SEVERITY_WARNING, -1,
            "non-OR composition modes NYI");
    }

    /* clip */
    w = src->width;
    h = src->height;
    if (x < 0) { w += x; x = 0; }
    if (y < 0) { h += y; y = 0; } 
    w = (x + w < dst->width) ? w : dst->width - x;
    h = (y + h < dst->height) ? h : dst->height - y;
    
    fprintf(stderr, "composting %dx%d at (%d, %d) afer clipping\n",
        w, h, x, y);
    
    /* special case complete/strip replacement */
    if ((x == 0) && (w == src->width)) {
        memcpy(dst->data + y*dst->stride, src->data, h*src->stride);
        return 0;
    }

    /* general case */
    leftword = x >> 5;
    leftbits = x & 31;
    rightword = (x + w) >> 5;
    rightbits = (x + w) & 31;
    shift = 0;
    while (leftbits >= 1 << shift) shift++;
    mask = 0;
    if (leftbits) for (i = 0; i < leftbits; i++)
        mask = mask << 1 | 1;
    highmask = 1;    
    if (rightbits) for (i = 0; i < rightbits; i++)
        highmask = highmask << 1 | 1;
    
    /* word strides */
    src_stride = src->stride >> 2;
    dst_stride = dst->stride >> 2;
    fprintf(stderr, "leftword = %d leftbits = %d rightword = %d rightbits=%d shift=%d\n",
        leftword, leftbits, rightword, rightbits, shift);
    fprintf(stderr, "mask = 0x%08x highmask = 0x%08x\n", mask, highmask); 
    
    if (leftword == rightword) {
        for (row = 0; row < h; row++) {
            s = src->data + row*src_stride;
            d = dst->data + (y + row)*dst_stride + leftword;
            *d |= (*s & mask) << leftbits;
        }
    } else {
        mask = 0;
        for (i = 0; i < 32 - leftbits; i++)
            mask = mask << 1 | 1;
        for (row = 0; row < h; row++) {
            s = src->data + row*src_stride;
            d = dst->data + (y + row)*dst_stride + leftword;
            *d++ |= (*s & mask) << leftbits;
            for(word = leftword; word < rightword; word++) {
                *d |= (*s++ & ~mask) >> (32 - leftbits);
                *d++ |= (*s & mask) << leftbits;
            }
            *d |= (*++s & highmask) << (32 - rightbits);
        }
    }
            
    return 0;
}
