/* Copyright (C) 2006-2010 Artifex Software, Inc.
   All Rights Reserved.

   This software is provided AS-IS with no warranty, either express or
   implied.

   This software is distributed under license and may not be copied, modified
   or distributed except as expressly authorized under the terms of that
   license.  Refer to licensing information at http://www.artifex.com/
   or contact Artifex Software, Inc.,  7 Mt. Lassen  Drive - Suite A-134,
   San Rafael, CA  94903, U.S.A., +1(415)492-9861, for further information.
*/

/* XPS interpreter - string manipulation functions */

#include "ghostxps.h"

void *
xps_realloc_imp(xps_context_t *ctx, void *ptr, int size, const char *func)
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
xps_strcasecmp(char *a, char *b)
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
        cpy = (char*) gs_alloc_bytes(ctx->memory, strlen(str) + 1, cname);
    if (cpy)
        strcpy(cpy, str);
    return cpy;
}

#define SEP(x)  ((x)=='/' || (x) == 0)

char *
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

    if (q == name) /* empty string is really "." */
        *q++ = '.';
    *q = '\0';

    return name;
}

void
xps_absolute_path(char *output, char *base_uri, char *path)
{
    if (path[0] == '/')
    {
        strcpy(output, path);
    }
    else
    {
        strcpy(output, base_uri);
        strcat(output, "/");
        strcat(output, path);
    }
    xps_clean_path(output);
}

