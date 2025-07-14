/* Copyright (C) 2001-2025 Artifex Software, Inc.
   All Rights Reserved.

   This software is provided AS-IS with no warranty, either express or
   implied.

   This software is distributed under license and may not be copied,
   modified or distributed except as expressly authorized under the terms
   of the license contained in the file LICENSE in this distribution.

   Refer to licensing information at http://www.artifex.com or contact
   Artifex Software, Inc.,  39 Mesa Street, Suite 108A, San Francisco,
   CA 94129, USA, for further information.
*/


/* XPS interpreter - string manipulation functions */

#include "ghostxps.h"

void *
xps_realloc_imp(xps_context_t *ctx, void *ptr, size_t size, const char *func)
{
    if (!ptr)
        return gs_alloc_bytes(ctx->memory, size, func);
    return gs_resize_object(ctx->memory, ptr, size, func);
}

static inline int
xps_tolower(int c)
{
    if (c >= 'A' && c <= 'Z')
        return c + 32;
    return c;
}

int
xps_strcasecmp(const char *a, const char *b)
{
        while (xps_tolower(*a) == xps_tolower(*b))
        {
                if (*a++ == 0)
                        return 0;
                b++;
        }
        return xps_tolower(*a) - xps_tolower(*b);
}

char *
xps_strdup_imp(xps_context_t *ctx, const char *str, const char *cname)
{
    char *cpy = NULL;
    if (str)
        cpy = (char*) gs_alloc_bytes(ctx->memory, (size_t)strlen(str) + 1, cname);
    if (cpy)
        strcpy(cpy, str);
    return cpy;
}

#define SEP(x)  ((x)=='/' || (x) == 0)

static char *
xps_clean_path(char *name)
{
    char *p, *q, *dotdot;
    int rooted;

    rooted = name[0] == '/';

    /*
     * invariants:
     *      p points at beginning of path element we're considering.
     *      q points just past the last path element we wrote (no slash).
     *      dotdot points just past the point where .. cannot backtrack
     *              any further (no slash).
     */
    p = q = dotdot = name + rooted;
    while (*p)
    {
        if(p[0] == '/') /* null element */
            p++;
        else if (p[0] == '.' && SEP(p[1]))
            p += 1; /* don't count the separator in case it is nul */
        else if (p[0] == '.' && p[1] == '.' && SEP(p[2]))
        {
            p += 2;
            if (q > dotdot) /* can backtrack */
            {
                while(--q > dotdot && *q != '/')
                    ;
            }
            else if (!rooted) /* /.. is / but ./../ is .. */
            {
                if (q != name)
                    *q++ = '/';
                *q++ = '.';
                *q++ = '.';
                dotdot = q;
            }
        }
        else /* real path element */
        {
            if (q != name+rooted)
                *q++ = '/';
            while ((*q = *p) != '/' && *q != 0)
                p++, q++;
        }
    }

    /* Guard against the 'blah:' case, where name == terminator. */
    if (q == name && *q != 0) /* empty string is really "." */
        *q++ = '.';
    *q = '\0';

    return name;
}

void
xps_absolute_path(char *output, char *base_uri, char *path, int output_size)
{
    if (path[0] == '/')
    {
        gs_strlcpy(output, path, output_size);
    }
    else
    {
        gs_strlcpy(output, base_uri, output_size);
        gs_strlcat(output, "/", output_size);
        gs_strlcat(output, path, output_size);
    }
    xps_clean_path(output);
}
