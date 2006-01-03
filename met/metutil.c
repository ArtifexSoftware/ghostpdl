/* Portions Copyright (C) 2001 artofcode LLC.
   Portions Copyright (C) 1996, 2001 Artifex Software Inc.
   Portions Copyright (C) 1988, 2000 Aladdin Enterprises.
   This software is based in part on the work of the Independent JPEG Group.
   All Rights Reserved.

   This software is distributed under license and may not be copied, modified
   or distributed except as expressly authorized under the terms of that
   license.  Refer to licensing information at http://www.artifex.com/ or
   contact Artifex Software, Inc., 101 Lucas Valley Road #110,
   San Rafael, CA  94903, (415)492-9861, for further information. */
/* $Id:*/

/* metutil.c */

#include "metutil.h"
#include <stdlib.h>

int
met_cmp_and_set(char **field, const char *lhs, const char *rhs, const char *attr_name) 
{
    int cmp = strcmp(lhs, attr_name);
    if (cmp == 0)
        *field = rhs;
    return cmp;
}

void 
met_split(char *b, char **args, bool (*delimfunc(char c)))
{
    while (*b != NULL) {
        while (delimfunc(*b))
            *b++ = NULL;
        *args++ = b;
        while((*b != NULL) && (!delimfunc(*b)))
            b++;
    }
    *args = NULL;
}

/* I am sure there is a better (more robust) meme for this... */
rgb_t
met_hex2rgb(char *hexstring)
{
    char *hextable = "0123456789ABCDEF";
    rgb_t rgb;
    char *hexstringp = hexstring;
    /* nb need to look into color specification */
    if (strlen(hexstring) == 9)
        hexstringp += 2;
        
#define HEXTABLEINDEX(chr) (strchr(hextable, (chr)) - hextable)
    rgb.r = HEXTABLEINDEX(hexstringp[1]) * 16 +
        HEXTABLEINDEX(hexstringp[2]);
    rgb.g = HEXTABLEINDEX(hexstringp[3]) * 16 +
        HEXTABLEINDEX(hexstringp[4]);
    rgb.b = HEXTABLEINDEX(hexstringp[5]) * 16 +
        HEXTABLEINDEX(hexstringp[6]);
    return rgb;
}
