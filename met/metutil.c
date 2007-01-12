/* Portions Copyright (C) 2007 artofcode LLC.
   Portions Copyright (C) 1996, 2001 Artifex Software Inc.
   Portions Copyright (C) 1988, 2000 Aladdin Enterprises.
   This software is based in part on the work of the Independent JPEG Group.
   All Rights Reserved.

   This software is distributed under license and may not be copied, modified
   or distributed except as expressly authorized under the terms of that
   license.  Refer to licensing information at http://www.artifex.com/ or
   contact Artifex Software, Inc., 101 Lucas Valley Road #110,
   San Rafael, CA  94903, (415)492-9861, for further information. */
/* $Id$ */

/* metutil.c */

#include "metutil.h"
#include "string_.h"
#include <stdlib.h>
#include "ctype_.h"
#include "gserror.h"

char *
met_strdup(gs_memory_t *mem, const char *str, const char *client)
{
    char *s = NULL;   
    if ( str )
        s = (char*) gs_alloc_bytes(mem, strlen(str) + 1, client);
    if ( s )
        strcpy(s, str);
    return s;
}

int
met_cmp_and_set(char **field, const char *lhs, const char *rhs, const char *attr_name) 
{
    /* nb memory hack */
    extern gs_memory_t *gs_mem_ptr;
    int cmp = strcmp(lhs, attr_name);
    if (cmp == 0)
        *field = met_strdup(gs_mem_ptr, rhs, attr_name);
    return cmp;
}

int 
met_split(char *b, char **args, bool (*delimfunc)(char c))
{
    int cnt = 0;

    while (*b != (char)NULL) {
        while (delimfunc(*b))
            *b++ = (char)NULL;
        *args++ = b;
	cnt++;
        while((*b != (char)NULL) && (!delimfunc(*b)))
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
    const char *hextable = "0123456789ABCDEF";
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
    rgb.r /= 256.0; rgb.g /= 256.0; rgb.b /= 256.0;
    return rgb;
}


/* these operators set the gs graphics state and don't effect the
   metro client state */
private bool is_Data_delimeter(char b) 
{
    return (b == ',') || (isspace(b));
}

int
met_get_transform(gs_matrix *gsmat, ST_RscRefMatrix metmat)
{
    if (!metmat) {
        gs_make_identity(gsmat);
    } else {
        char transstring[strlen(metmat)];
        strcpy(transstring, metmat);
        /* nb wasteful */
        char *args[strlen(metmat)];
        char **pargs = args;

        if ( 6 != met_split(transstring, args, is_Data_delimeter))
            return gs_throw(-1, "Canvas RenderTransform number of args");
        /* nb checking for sane arguments */
        gsmat->xx = atof(pargs[0]);
        gsmat->xy = atof(pargs[1]);
        gsmat->yx = atof(pargs[2]);
        gsmat->yy = atof(pargs[3]);
        gsmat->tx = atof(pargs[4]);
        gsmat->ty = atof(pargs[5]);
    }
    return 0;
}
        
