/*
    jbig2dec
    
    Copyright (C) 2002 artofcode LLC.
    
    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    $Id: pbm2png.c,v 1.3 2002/07/08 14:54:02 giles Exp $
*/

#ifdef HAVE_CONFIG_H
#include "config.h"
#include "config_types.h"
#elif _WIN32
#include "config_win32.h"
#endif
#ifdef HAVE_STDINT_H
#include <stdint.h>
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "jbig2.h"
#include "jbig2_image.h"

int main(int argc, char *argv[])
{
    Jbig2Ctx *ctx;
    Jbig2Image *image;
    int error;
    
    /* we need a context for the allocators */
    ctx = jbig2_ctx_new(NULL, 0, NULL, NULL, NULL);

    if (argc != 3) {
        fprintf(stderr, "usage: %s <in.pbm> <out.png>\n\n", argv[0]);
        return 1;
    }
    
    image = jbig2_image_read_pbm_file(ctx, argv[1]);
    if(image == NULL) {
        fprintf(stderr, "error reading pbm file '%s'\n", argv[1]);
        return 1;
    } else {
        fprintf(stderr, "converting %dx%d image to png format\n", image->width, image->height);
    }
    
    error = jbig2_image_write_png_file(image, argv[2]);
    if (error) {
        fprintf(stderr, "error writing png file '%s' error %d\n", argv[2], error);
    }

    return (error);
}
