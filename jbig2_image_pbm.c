/*
    jbig2dec
    
    Copyright (c) 2002 artofcode LLC.
    
    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    $Id: jbig2_image_pbm.c,v 1.2 2002/06/04 16:51:02 giles Exp $
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

/* take an image from a file in pbm format */
Jbig2Image *jbig2_image_read_pbm_file(char *filename)
{
    FILE *in;
    Jbig2Image *image;
    
    if ((in = fopen(filename, "rb")) == NULL) {
		fprintf(stderr, "unable to open '%s' for reading\n", filename);
		return NULL;
    }
    
    image = jbig2_image_read_pbm(in);
    
    return (image);
}

// FIXME: should handle multi-image files
Jbig2Image *jbig2_image_read_pbm(FILE *in)
{
    int i, dim[2];
    int stride, pbm_stride;
    int done;
    Jbig2Image *image;
    char c,buf[32];
    byte *data;
    
    // look for 'P4' magic
    while ((c = fgetc(in)) != 'P') {
        if (feof(in)) return NULL;
    }
    if ((c = fgetc(in)) != '4') {
        fprintf(stderr, "not a binary pbm file.\n");
        return NULL;
    }
    // read size. we must find two decimal numbers representing
    // the image dimensions. done will index whether we're
    // looking for the width of the height and i will be our
    // array index for copying strings into our buffer
    done = 0;
    i = 0;
    while (done < 2) {
        c = fgetc(in);
        // skip whitespace
        if (c == ' ' || c == '\t' || c == '\r' || c == '\n') continue;
        // skip comments
        if (c == '#') {
            while ((c = fgetc(in)) != '\n');
            continue;
        }
        if (isdigit(c)) {
            buf[i++] = c;
            while (isdigit(buf[i++] = fgetc(in))) {
                if (feof(in) || i >= 32) {
                    fprintf(stderr, "pbm parsing error\n");
                    return NULL;
                }
            }
            buf[i] = '\0';
            sscanf(buf, "%d", &dim[done]);
            i = 0;
            done++;
        }
    }
    // allocate image structure
    image = jbig2_image_new(dim[0], dim[1]);
    if (image == NULL) {
        fprintf(stderr, "could not allocate %dx%d image structure\n", dim[0], dim[1]);
        return NULL;
    }
    // the pbm data is byte-aligned, and our image struct is word-aligned,
    // so we have to index each line separately
    pbm_stride = (dim[0] + 1) >> 3;
    data = (byte *)image->data;
    for (i = 0; i < dim[1]; i++) {
        fread(data, sizeof(byte), pbm_stride, in);
        if (feof(in)) {
            fprintf(stderr, "unexpected end of pbm file.\n");
            jbig2_image_free(image);
            return NULL;
        }
        data += image->stride;
    }
    
    // success
    return image;
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
