/* Copyright (C) 2001-2019 Artifex Software, Inc.
   All Rights Reserved.

   This software is provided AS-IS with no warranty, either express or
   implied.

   This software is distributed under license and may not be copied,
   modified or distributed except as expressly authorized under the terms
   of the license contained in the file LICENSE in this distribution.

   Refer to licensing information at http://www.artifex.com or contact
   Artifex Software, Inc.,  1305 Grant Avenue - Suite 200, Novato,
   CA 94945, U.S.A., +1(415)492-9861, for further information.
*/


/* $Id:$ */
/* Unix implementation of gp_defaultpapersize */

#ifdef USE_LIBPAPER
#include <paper.h>
#include <stdlib.h>

/* libpaper uses the std library's malloc() to allocate
   the string, but a normal "free" call below will
   get "hooked" if memento is in use (via malloc_.h).
   So workaround that...
*/
#ifdef MEMENTO
/* This definition *must* come before memento.h gets included
   (in this case, via malloc_.h) so that the free() call
   inside std_free() avoids the pre-processor (re)definition
   to Memento_free()
*/
static void std_free(void *ptr)
{
   free(ptr);
}
#else
#define std_free free
#endif

#endif

#include "string_.h"
#include "malloc_.h"
#include "gx.h"
#include "gp.h"

/* ------ Default paper size ------ */

/* Get the default paper size.  See gp_paper.h for details. */
int
gp_defaultpapersize(char *ptr, int *plen)
{
#ifdef USE_LIBPAPER
    const char *paper;
    bool is_systempaper;

    paperinit();

    paper = systempapername();
    if (paper)
        is_systempaper =  true;
    else {
        paper = defaultpapername();
        is_systempaper =  false;
    }

    if (paper) {
        int rc, len = strlen(paper);

        if (len < *plen) {
            /* string fits */
            strcpy(ptr, paper);
            rc = 0;
        } else {
            /* string doesn't fit */
            rc = -1;
        }
        *plen = len + 1;
        paperdone();
        if (is_systempaper)
            std_free((void *)paper);
        return rc;
    }
#endif

    /* No default paper size */

    if (*plen > 0)
        *ptr = 0;
    *plen = 1;
    return 1;
}
