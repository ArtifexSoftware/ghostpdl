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

int 
met_split(char *b, char **args, bool (*delimfunc(char c)))
{
    int cnt = 0;

    while (*b != NULL) {
        while (delimfunc(*b))
            *b++ = NULL;
        *args++ = b;
	cnt++;
        while((*b != NULL) && (!delimfunc(*b)))
            b++;
    }
    *args = NULL;

    return cnt;
}


/* utility to expand empty arguments in a string with a sentinel value */
char *
met_expand(char *s1, const char *s2, const char delimiter, const char sentinel)
{
   char *s = s1;
   char last_char = '\0';
   while (*s2 != '\0') {
       if ((*s2 == delimiter) &&
           (last_char == '\0' || last_char == delimiter))
           *s++ = sentinel;
       last_char = *s2;
       *s++ = *s2++;
   }
   /* add the null terminator */
   *s = *s2;
   return s1;
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
        
#define HEXTABLEINDEX(chr) (strchr(hextable, (toupper(chr))) - hextable)
    rgb.r = HEXTABLEINDEX(hexstringp[1]) * 16 +
        HEXTABLEINDEX(hexstringp[2]);
    rgb.g = HEXTABLEINDEX(hexstringp[3]) * 16 +
        HEXTABLEINDEX(hexstringp[4]);
    rgb.b = HEXTABLEINDEX(hexstringp[5]) * 16 +
        HEXTABLEINDEX(hexstringp[6]);
    return rgb;
}
