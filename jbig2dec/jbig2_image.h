/* Copyright (C) 2001-2019 Artifex Software, Inc.
   All Rights Reserved.

   This software is provided AS-IS with no warranty, either express or
   implied.

   This software is distributed under license and may not be copied,
   modified or distributed except as expressly authorized under the terms
   of the license contained in the file LICENSE in this distribution.

   Refer to licensing information at http://www.artifex.com or contact
   Artifex Software, Inc.,  1305 Grant Avenue - Suite 200, Novato,
   CA 94945, U.S.A., +1(415)492-9861, for further information.
*/

/*
    jbig2dec
*/

#ifndef _JBIG2_IMAGE_H
#define _JBIG2_IMAGE_H

typedef enum {
    JBIG2_COMPOSE_OR = 0,
    JBIG2_COMPOSE_AND = 1,
    JBIG2_COMPOSE_XOR = 2,
    JBIG2_COMPOSE_XNOR = 3,
    JBIG2_COMPOSE_REPLACE = 4
} Jbig2ComposeOp;

Jbig2Image *jbig2_image_new(Jbig2Ctx *ctx, uint32_t width, uint32_t height);
void jbig2_image_release(Jbig2Ctx *ctx, Jbig2Image *image);
Jbig2Image *jbig2_image_reference(Jbig2Ctx *ctx, Jbig2Image *image);
void jbig2_image_free(Jbig2Ctx *ctx, Jbig2Image *image);
void jbig2_image_clear(Jbig2Ctx *ctx, Jbig2Image *image, int value);
Jbig2Image *jbig2_image_resize(Jbig2Ctx *ctx, Jbig2Image *image, uint32_t width, uint32_t height, int value);
int jbig2_image_compose(Jbig2Ctx *ctx, Jbig2Image *dst, Jbig2Image *src, int x, int y, Jbig2ComposeOp op);

int jbig2_image_get_pixel(Jbig2Image *image, int x, int y);
void jbig2_image_set_pixel(Jbig2Image *image, int x, int y, bool value);

static inline int
jbig2_image_get_pixel_fast(Jbig2Image *image, int x, int y)
{
    const int byte = (x >> 3) + y * image->stride;
    const int bit = 7 - (x & 7);

    return ((image->data[byte] >> bit) & 1);
}

/* set an individual pixel value in an image */
static inline void
jbig2_image_set_pixel_fast(Jbig2Image *image, int x, int y, bool value)
{
    int scratch, mask;
    int bit, byte;

    byte = (x >> 3) + y * image->stride;
    bit = 7 - (x & 7);
    mask = (1 << bit) ^ 0xff;

    scratch = image->data[byte] & mask;
    image->data[byte] = scratch | (value << bit);
}

#endif /* _JBIG2_IMAGE_H */
