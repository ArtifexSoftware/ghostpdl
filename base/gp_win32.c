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


/* Common platform-specific routines for MS-Windows WIN32 */
/* originally hacked from gp_msdos.c by Russell Lang */
#include "windows_.h"
#include "malloc_.h"
#include "stdio_.h"
#include "string_.h"		/* for strerror */
#include "gstypes.h"
#include "gsmemory.h"		/* for gp.h */
#include "gserrors.h"
#include "gp.h"
#include "time_.h"

/* ------ Miscellaneous ------ */

/* Get the string corresponding to an OS error number. */
/* This is compiler-, not OS-, specific, but it is ANSI-standard and */
/* all MS-DOS and MS Windows compilers support it. */
const char *
gp_strerror(int errnum)
{
    return strerror(errnum);
}

/* ------ Date and time ------ */

/* Read the current time (in seconds since Jan. 1, 1980) */
/* and fraction (in nanoseconds). */
void
gp_get_realtime(long *pdt)
{
    time_t t;

    time(&t);
    if (sizeof(time_t) > 4) {
        pdt[0] = t & 0xFFFFFFFF;
        pdt[1] = t >> 32;
    } else {
        pdt[0] = t;
        pdt[1] = 0;
    }
}

/* Read the current user CPU time (in seconds) */
/* and fraction (in nanoseconds).  */
void
gp_get_usertime(long *pdt)
{

    LARGE_INTEGER freq;
    LARGE_INTEGER count;
    LARGE_INTEGER seconds;

    if (!QueryPerformanceFrequency(&freq)) {
        gp_get_realtime(pdt);	/* use previous method if high-res perf counter not available */
        return;
    }
    /* Get the high resolution time as seconds and nanoseconds */
    QueryPerformanceCounter(&count);

    seconds.QuadPart = (count.QuadPart / freq.QuadPart);
    pdt[0] = seconds.LowPart;
    count.QuadPart -= freq.QuadPart * seconds.QuadPart;
    count.QuadPart *= 1000000000;			/* we want nanoseconds */
    count.QuadPart /= freq.QuadPart;
    pdt[1] = count.LowPart;
}

/* ------ Console management ------ */

/* Answer whether a given file is the console (input or output). */
/* This is not a standard gp procedure, */
/* but the MS Windows configuration needs it, */
/* and other MS-DOS configurations might need it someday. */
int
gp_file_is_console(FILE * f)
{
#ifdef __DLL__
    if (f == NULL)
        return 1;
#else
    if (f == NULL)
        return 0;
#endif
    if (fileno(f) <= 2)
        return 1;
    return 0;
}

/* ------ Screen management ------ */

/* Get the environment variable that specifies the display to use. */
const char *
gp_getenv_display(void)
{
    return NULL;
}

/* ------ File names ------ */

/* Define the default scratch file name prefix. */
const char gp_scratch_file_name_prefix[] = "_temp_";

/* Define the name of the null output file. */
const char gp_null_file_name[] = "nul";

/* Define the name that designates the current directory. */
const char gp_current_directory_name[] = ".";

/* A function to decode the next codepoint of the supplied args from the
 * local windows codepage, or -1 for EOF.
 */

#if defined(__WIN32__) && !defined(METRO)
int
gp_local_arg_encoding_get_codepoint(FILE *file, const char **astr)
{
    int len;
    int c;
    char arg[3];
    wchar_t unicode[2];
    char utf8[4];

    if (file) {
        c = fgetc(file);
        if (c == EOF)
            return EOF;
    } else if (**astr) {
        c = *(*astr)++;
        if (c == 0)
            return EOF;
    } else {
        return EOF;
    }

    arg[0] = c;
    if (IsDBCSLeadByte(c)) {
        if (file) {
            c = fgetc(file);
            if (c == EOF)
                return EOF;
        } else if (**astr) {
            c = *(*astr)++;
            if (c == 0)
                return EOF;
        }
        arg[1] = c;
        len = 2;
    } else {
        len = 1;
    }

    /* Convert the string (unterminated in, unterminated out) */
    len = MultiByteToWideChar(CP_ACP, 0, arg, len, unicode, 2);

    return unicode[0];
}
#endif /* __WIN32__ */
