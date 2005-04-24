/*
    jbig2dec
    
    Copyright (C) 2001-2002 artofcode LLC.
    
    This software is distributed under license and may not
    be copied, modified or distributed except as expressly
    authorized under the terms of the license contained in
    the file LICENSE in this distribution.
                                                                                
    For information on commercial licensing, go to
    http://www.artifex.com/licensing/ or contact
    Artifex Software, Inc.,  101 Lucas Valley Road #110,
    San Rafael, CA  94903, U.S.A., +1(415)492-9861.

    $Id$
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
