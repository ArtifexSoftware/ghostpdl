/*
    jbig2dec
    
    Copyright (c) 2001 artofcode LLC.
    
    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    $Id: jbig2_page.h,v 1.1 2002/06/15 14:15:12 giles Exp $
*/

#ifndef JBIG2_PAGE_H
#define JBIG2_PAGE_H

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>

#include "jbig2dec.h"


struct _Jbig2PageInfo {
	uint32_t	height, width;	/* in pixels */
	uint32_t	x_resolution,
                        y_resolution;	/* in pixels per meter */
	uint16_t	stripe_size;
	bool		striped;
	uint8_t		flags;
};

#endif /* JBIG2_PAGE_H */
