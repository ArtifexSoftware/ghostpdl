/*
    jbig2dec
    
    Copyright (c) 2002 artofcode LLC.
    
    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    $Id: jbig2_image_pbm.c,v 1.1 2002/05/08 02:36:04 giles Exp $
*/

#include <stdio.h>

#include "jbig2dec.h"
#include "jbig2_image.h"

/* take an image structure and write it to a file in pbm format */

int jbig2_image_write_pbm_file(Jbig2Image *image, char *filename)
{
    FILE *out;
    int	error;
    
    if ((out = fopen(filename, "wb")) == NULL) {
		fprintf(stderr, "unable to open '%s' for writing\n", filename);
		return 1;
    }
    
    error = jbig2_image_write_pbm(image, out);
    
    fclose(out);
    return (error);
}

/* write out an image struct as a pbm stream to an open file pointer */

int jbig2_image_write_pbm(Jbig2Image *image, FILE *out)
{
        int i, short_stride, extra_bits;
        byte *p = (byte *)image->data;
        
        // pbm header
        fprintf(out, "P4\n%d %d\n", image->width, image->height);
        
        // pbm format pads only to the next byte boundary
        // so we figure our output byte stride and fixup
        short_stride = image->width >> 3;
        extra_bits = image->width - (short_stride << 3);
        fprintf(stderr, "creating %dx%d pbm image, short_stride %d, extra_bits %d\n",
            image->width, image->height, short_stride, extra_bits);
        // write out each row
        for(i = 0; i < image->height; i++) {
            fwrite(p, sizeof(byte), short_stride, out);
            if (extra_bits) fwrite(p + short_stride, sizeof(byte), 1, out);
            p += image->stride;
        }
        
        /* success */
	return 0;
}


#ifdef TEST
int main(int argc, char *argv[])
{
	int	i,j;
	Jbig2Image	*image;
	uint32		*data;
	char		*filename = "test.pbm";
	
	image = jbig2_image_new(384,51);
	if (image == NULL) {
		fprintf(stderr, "failed to create jbig2 image structure!\n");
		exit(1);
	}
	
	fprintf(stderr, "creating checkerboard test image '%s'\n", filename);
	data = image->data;
	for (j = 0; j < image->height; j++) {
		for (i = 0; i < image->stride >> 2; i++) {
			*data++ = ((j & 16) >> 4) ? 0x0000FFFF: 0xFFFF0000;
		}
	}
	
	return jbig2_image_write_pbm_file(image, filename);
}

#endif /* TEST */
