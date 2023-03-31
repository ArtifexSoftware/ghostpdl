/* Copyright (C) 2001-2023 Artifex Software, Inc.
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


/* Print values in ASCII form on a stream */
#include "math_.h"		/* for fabs */
#include "stdio_.h"		/* for stream.h */
#include "string_.h"		/* for strchr */
#include "stream.h"
#include "spprint.h"

/* ------ Output ------ */

/* Put a byte array on a stream. */
int
stream_write(stream * s, const void *ptr, uint count)
{
    uint used;

    sputs(s, (const byte *)ptr, count, &used);
    return (int)used;
}

/* Put a string on a stream. */
int
stream_puts(stream * s, const char *str)
{
    uint len = strlen(str);
    uint used;
    int status = sputs(s, (const byte *)str, len, &used);

    return (status >= 0 && used == len ? 0 : EOF);
}

/* Print a format string up to the first variable substitution. */
/* Return a pointer to the %, or to the terminating 0 if no % found. */
static const char *
pprintf_scan(stream * s, const char *format)
{
    const char *fp = format;

    for (; *fp != 0; ++fp) {
        if (*fp == '%') {
            if (fp[1] != '%')
                break;
            ++fp;
        }
        sputc(s, *fp);
    }
    return fp;
}

/* Print a short string on a stream. */
static void
pputs_short(stream *s, const char *str)
{
    const char *p = str;

    for (; *p; ++p)
        sputc(s, *p);
}

/* Print (an) int value(s) using a format. */
const char *
pprintd1(stream * s, const char *format, int v)
{
    const char *fp = pprintf_scan(s, format);
    char str[25];

#ifdef DEBUG
    if (*fp == 0 || fp[1] != 'd')	/* shouldn't happen! */
        lprintf1("Bad format in pprintd1: %s\n", format);
#endif
    gs_snprintf(str, sizeof(str), "%d", v);
    pputs_short(s, str);
    return pprintf_scan(s, fp + 2);
}
const char *
pprintd2(stream * s, const char *format, int v1, int v2)
{
    return pprintd1(s, pprintd1(s, format, v1), v2);
}
const char *
pprintd3(stream * s, const char *format, int v1, int v2, int v3)
{
    return pprintd2(s, pprintd1(s, format, v1), v2, v3);
}
const char *
pprintd4(stream * s, const char *format, int v1, int v2, int v3, int v4)
{
    return pprintd2(s, pprintd2(s, format, v1, v2), v3, v4);
}

/* Print (a) floating point number(s) using a format. */
/* See gdevpdfx.h for why this is needed. */
const char *
pprintg1(stream * s, const char *format, double v)
{
    const char *fp = pprintf_scan(s, format);
    char dot, str[150];

#ifdef DEBUG
    if (*fp == 0 || fp[1] != 'g')	/* shouldn't happen! */
        lprintf1("Bad format in pprintg: %s\n", format);
#endif
    gs_snprintf(str, sizeof(str), "%f", 1.5);
    dot = str[1]; /* locale-dependent */
    gs_snprintf(str, sizeof(str), "%g", v);
    if (strchr(str, 'e')) {
        /* Bad news.  Try again using f-format. */
        gs_snprintf(str, sizeof(str), (fabs(v) > 1 ? "%1.1f" : "%1.8f"), v);
    }
    /* Juggling locales isn't thread-safe. Posix me harder. */
    if (dot != '.') {
        char *pdot = strchr(str, dot);
        if (pdot)
            *pdot = '.';
    }
    pputs_short(s, str);
    return pprintf_scan(s, fp + 2);
}
const char *
pprintg2(stream * s, const char *format, double v1, double v2)
{
    return pprintg1(s, pprintg1(s, format, v1), v2);
}
const char *
pprintg3(stream * s, const char *format, double v1, double v2, double v3)
{
    return pprintg2(s, pprintg1(s, format, v1), v2, v3);
}
const char *
pprintg4(stream * s, const char *format, double v1, double v2, double v3,
         double v4)
{
    return pprintg2(s, pprintg2(s, format, v1, v2), v3, v4);
}
const char *
pprintg6(stream * s, const char *format, double v1, double v2, double v3,
         double v4, double v5, double v6)
{
    return pprintg3(s, pprintg3(s, format, v1, v2, v3), v4, v5, v6);
}

/* Print a long value using a format. */
const char *
pprintld1(stream *s, const char *format, long v)
{
    const char *fp = pprintf_scan(s, format);
    char str[25];

#ifdef DEBUG
    if (*fp == 0 || fp[1] != 'l' || fp[2] != 'd')	/* shouldn't happen! */
        lprintf1("Bad format in pprintld: %s\n", format);
#endif
    gs_snprintf(str, sizeof(str), "%ld", v);
    pputs_short(s, str);
    return pprintf_scan(s, fp + 3);
}
const char *
pprintld2(stream *s, const char *format, long v1, long v2)
{
    return pprintld1(s, pprintld1(s, format, v1), v2);
}
const char *
pprintld3(stream *s, const char *format, long v1, long v2, long v3)
{
    return pprintld2(s, pprintld1(s, format, v1), v2, v3);
}

/* Print a size_t value using a format. */
const char *
pprintzd1(stream *s, const char *format, size_t v)
{
    const char *fp = pprintf_scan(s, format);
    char str[25];
    const size_t z = strlen("%"PRIdSIZE);

#ifdef DEBUG
    size_t i;

    for (i = 0; i < z; i++)
        if (fp[i] != ("%"PRIdSIZE)[i])
            break;
    if (i != z)
        lprintf1("Bad format in pprintzd: %s\n", format);
#endif
    gs_snprintf(str, sizeof(str), "%"PRIdSIZE, v);
    pputs_short(s, str);
    return pprintf_scan(s, fp + z);
}
const char *
pprintzd2(stream *s, const char *format, size_t v1, size_t v2)
{
    return pprintzd1(s, pprintzd1(s, format, v1), v2);
}
const char *
pprintzd3(stream *s, const char *format, size_t v1, size_t v2, size_t v3)
{
    return pprintzd2(s, pprintzd1(s, format, v1), v2, v3);
}

/* Print an int64_t value using a format. */
const char *
pprinti64d1(stream *s, const char *format, int64_t v)
{
    const char *fp = pprintf_scan(s, format);
    char str[25];
    const size_t z = strlen("%"PRId64);

#ifdef DEBUG
    size_t i;

    for (i = 0; i < z; i++)
        if (fp[i] != ("%"PRId64)[i])
            break;
    if (i != z)
        lprintf1("Bad format in pprinti64d: %s\n", format);
#endif
    gs_snprintf(str, sizeof(str), "%"PRId64, v);
    pputs_short(s, str);
    return pprintf_scan(s, fp + z);
}
const char *
pprinti64d2(stream *s, const char *format, int64_t v1, int64_t v2)
{
    return pprinti64d1(s, pprinti64d1(s, format, v1), v2);
}
const char *
pprinti64d3(stream *s, const char *format, int64_t v1, int64_t v2, int64_t v3)
{
    return pprinti64d2(s, pprinti64d1(s, format, v1), v2, v3);
}

/* Print (a) string(s) using a format. */
const char *
pprints1(stream * s, const char *format, const char *str)
{
    const char *fp = pprintf_scan(s, format);

#ifdef DEBUG
    if (*fp == 0 || fp[1] != 's')	/* shouldn't happen! */
        lprintf1("Bad format in pprints: %s\n", format);
#endif
    pputs_short(s, str);
    return pprintf_scan(s, fp + 2);
}
const char *
pprints2(stream * s, const char *format, const char *str1, const char *str2)
{
    return pprints1(s, pprints1(s, format, str1), str2);
}
const char *
pprints3(stream * s, const char *format, const char *str1, const char *str2,
         const char *str3)
{
    return pprints2(s, pprints1(s, format, str1), str2, str3);
}
