/*
    jbig2dec
    
    Copyright (c) 2001 artofcode LLC.
    
    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    $Id: jbig2_image.h,v 1.1 2001/08/10 23:29:28 giles Exp $
*/

/*
   this is the general image structure used by the jbig2dec library
   images are 1 bpp, packed into word-aligned rows. stride gives
   the word offset to the next row, while width and height define
   the size of the image area in pixels.
*/

typedef struct _Jbig2Image {
	int	width, height, stride;
	uint32	*data;
} Jbig2Image;

Jbig2Image*	jbig2_image_new(int width, int height);
void		jbig2_image_free(Jbig2Image *image);

