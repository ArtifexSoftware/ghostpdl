/* Copyright (C) 2001-2012 Artifex Software, Inc.
   All Rights Reserved.

   This software is provided AS-IS with no warranty, either express or
   implied.

   This software is distributed under license and may not be copied,
   modified or distributed except as expressly authorized under the terms
   of the license contained in the file LICENSE in this distribution.

   Refer to licensing information at http://www.artifex.com or contact
   Artifex Software, Inc.,  7 Mt. Lassen Drive - Suite A-134, San Rafael,
   CA  94903, U.S.A., +1(415)492-9861, for further information.
*/


/* System V Unix-specific routines for Ghostscript */

/* This file contains a couple of standard Unix library procedures */
/* that a few System V platforms don't provide. */
/* Note that this file is NOT used for SVR4 platforms. */
#include <errno.h>
#include "stdio_.h"
#include "time_.h"
#include <sys/types.h>
#include <sys/times.h>
#include <sys/stat.h>
#include <sys/param.h>

/* rename */
int
rename(const char *a, const char *b)
{
    if (access(a, 0) == -1)
        return (-1);
    unlink(b);
    if (link(a, b) == -1)
        return (-1);
    if (unlink(a) == -1) {
        unlink(b);		/* ??? */
        return (-1);
    }
    return (0);
}

/* gettimeofday */
#ifndef HZ
#  define	HZ	100	/* see sys/param.h */
#endif
int
gettimeofday(struct timeval *tvp, struct timezone *tzp)
{
    struct tms tms;
    static long offset = 0;
    long ticks;

    if (!offset) {
        time(&offset);
        offset -= (times(&tms) / HZ);
    }
    ticks = times(&tms);
    tvp->tv_sec = ticks / HZ + offset;
    tvp->tv_usec = (ticks % HZ) * (1000 * 1000 / HZ);
    return 0;
}
