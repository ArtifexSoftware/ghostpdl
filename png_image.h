/*
    jbig2dec
    
    Copyright (c) 2001 artofcode LLC.
    
    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    $Id: png_image.h,v 1.1 2001/08/10 23:29:28 giles Exp $
*/

#ifndef _JBIG2_PNG_IMAGE_H
#define _JBIG2_PNG_IMAGE_H

/* take and image structure and write it out in png format */
int jbig2_image_write_png(Jbig2Image *image, char *filename);

#endif	/* _JBIG2_PNG_IMAGE_H */
