/*
    jbig2dec
    
    Copyright (C) 2002 artofcode LLC.
    
    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    $Id: pbm2png.c,v 1.1 2002/06/04 16:51:02 giles Exp $
*/

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include "jbig2_image.h"

int main(int argc, char *argv[])
{
	Jbig2Image *image;
        int error;

	image = jbig2_image_read_pbm_file(argv[1]);
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
