/*
    jbig2dec
    
    Copyright (c) 2002 artofcode LLC.
    
    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    $Id: jbig2_image_png.c,v 1.2 2002/06/15 16:02:54 giles Exp $
*/

#include <stdio.h>
#include <stdlib.h>
#include <png.h>

#include "jbig2.h"
#include "jbig2_priv.h"
#include "jbig2_image.h"

/* take an image structure and write it out in png format */

int jbig2_image_write_png_file(Jbig2Image *image, char *filename)
{
    FILE *out;
    int	error;
    
    if ((out = fopen(filename, "wb")) == NULL) {
		fprintf(stderr, "unable to open '%s' for writing\n", filename);
		return 1;
    }
    
    error = jbig2_image_write_png(image, out);
    
    fclose(out);
    return (error);
}

/* write out an image struct in png format to an open file pointer */

int jbig2_image_write_png(Jbig2Image *image, FILE *out)
{
	int		i;
	png_structp	png;
	png_infop	info;
	png_bytep	rowpointer;
	
	png = png_create_write_struct(PNG_LIBPNG_VER_STRING,
		NULL, NULL, NULL);
	if (png == NULL) {
		fprintf(stderr, "unable to create png structure\n");
		return 2;
	}
	
	info = png_create_info_struct(png);
	if (info == NULL) {
            fprintf(stderr, "unable to create png info structure\n");
            png_destroy_write_struct(&png,  (png_infopp)NULL);
            return 3;
	}

	/* set/check error handling */
	if (setjmp(png_jmpbuf(png))) {
		/* we've returned here after an internal error */
		fprintf(stderr, "internal error in libpng saving file\n");
		png_destroy_write_struct(&png, &info);
		return 4;
	}

	png_init_io(png, out);
   
	/* now we fill out the info structure with our format data */
	png_set_IHDR(png, info, image->width, image->height,
		1, PNG_COLOR_TYPE_GRAY, PNG_INTERLACE_NONE,
		PNG_COMPRESSION_TYPE_DEFAULT, PNG_FILTER_TYPE_DEFAULT);
	png_write_info(png, info);

	/* png natively treates 0 as black. This will convert for us */
	png_set_invert_mono(png);
	
	/* write out each row in turn */
	rowpointer = (png_bytep)image->data;
	for(i = 0; i < image->height; i++) {
		png_write_row(png, rowpointer);
		rowpointer += image->stride;
	}
	
	/* finish and clean up */
	png_write_end(png, info);
	png_destroy_write_struct(&png, &info);
		
	return 0;
}


#ifdef TEST
int main(int argc, char *argv[])
{
	int	i,j;
	Jbig2Image	*image;
	uint32		*data;
	char		*filename = "test.png";
	
	image = jbig2_image_new(400,400);
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
	
	return jbig2_image_write_png(image, filename);
}

#endif /* TEST */
