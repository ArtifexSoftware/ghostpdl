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


/* Print values in ASCII form on a stream */

#ifndef spprint_INCLUDED
#  define spprint_INCLUDED

#include "stdpre.h"
#include "scommon.h"

/* Put a character on a stream. */
#define stream_putc(s, c) spputc(s, c)

/* Put a byte array on a stream. */
int stream_write(stream * s, const void *ptr, uint count);

/* Put a string on a stream. */
int stream_puts(stream * s, const char *str);

/*
 * Print (a) floating point number(s) using a format.  This is needed
 * because %f format always prints a fixed number of digits after the
 * decimal point, and %g format may use %e format, which PDF disallows.
 * These functions return a pointer to the next %-element of the format, or
 * to the terminating 0.
 */
const char *pprintg1(stream * s, const char *format, double v);
const char *pprintg2(stream * s, const char *format, double v1, double v2);
const char *pprintg3(stream * s, const char *format,
                     double v1, double v2, double v3);
const char *pprintg4(stream * s, const char *format,
                     double v1, double v2, double v3, double v4);
const char *pprintg6(stream * s, const char *format,
                     double v1, double v2, double v3, double v4,
                     double v5, double v6);

/*
 * The rest of these printing functions exist solely because the ANSI C
 * "standard" for functions with a variable number of arguments is not
 * implemented properly or consistently across compilers.
 */
/* Print (an) int value(s) using a format. */
const char *pprintd1(stream * s, const char *format, int v);
const char *pprintd2(stream * s, const char *format, int v1, int v2);
const char *pprintd3(stream * s, const char *format,
                     int v1, int v2, int v3);
const char *pprintd4(stream * s, const char *format,
                     int v1, int v2, int v3, int v4);

/* Print a long value using a format. */
const char *pprintld1(stream * s, const char *format, long v);
const char *pprintld2(stream * s, const char *format, long v1, long v2);
const char *pprintld3(stream * s, const char *format,
                      long v1, long v2, long v3);

/* Print (a) string(s) using a format. */
const char *pprints1(stream * s, const char *format, const char *str);
const char *pprints2(stream * s, const char *format,
                     const char *str1, const char *str2);
const char *pprints3(stream * s, const char *format,
                     const char *str1, const char *str2, const char *str3);

#endif /* spprint_INCLUDED */
