/*
    jbig2dec
    
    Copyright (C) 2001-2002 artofcode LLC.
    
    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    $Id: memcmp.c,v 1.1 2002/08/05 17:10:44 giles Exp $
*/

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stddef.h>

/* replacement for broken memcmp() */

/*
 * compares two byte strings 'a' and 'b', both assumed to be 'len' bytes long
 * returns zero if the two strings are identical, otherwise returns the difference
 * between the values of the first two differing bytes, considered as unsigned chars
 */

int memcmp(const void *b1, const void *b2, size_t len)
{
	unsigned char *a, *b;
	unsigned char c;
	size_t i;

	a = (unsigned char *)b1;
	b = (unsigned char *)b2;
	for(i = 0; i < len; i++) {
		c = *a - *b;
		if (c) return (int)c; /* strings differ */
		a++;
		b++;
	}

	return 0; /* strings match */
}
