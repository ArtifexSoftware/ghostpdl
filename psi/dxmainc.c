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


/* dxmainc.c */
/*
 * Ghostscript frontend which provides a console to the Ghostscript
 * shared library.  Load time linking to libgs.so
 * This does not support the display device.  Use dxmain.c/gsx for that,
 * or modify this program to use bare Xlib calls.
 * Compile using
 *    gcc -o gsc dxmainc.c -lgs
 *
 * The ghostscript library needs to be compiled with
 *  gcc -fPIC -g -c -Wall file.c
 *  gcc -shared -Wl,-soname,libgs.so.7 -o libgs.so.7.00 file.o -lc
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#define __PROTOTYPES__
#include "ierrors.h"
#include "iapi.h"
#include "locale_.h"

const char start_string[] = "systemdict /start get exec\n";

static int gsdll_stdin(void *instance, char *buf, int len);
static int gsdll_stdout(void *instance, const char *str, int len);
static int gsdll_stdout(void *instance, const char *str, int len);

/*********************************************************************/
/* stdio functions */

/* callback for reading stdin */
/* Use async input */
static int
gsdll_stdin(void *instance, char *buf, int len)
{
    return read(fileno(stdin), buf, len);
}

static int
gsdll_stdout(void *instance, const char *str, int len)
{
    fwrite(str, 1, len, stdout);
    fflush(stdout);
    return len;
}

static int
gsdll_stderr(void *instance, const char *str, int len)
{
    fwrite(str, 1, len, stderr);
    fflush(stderr);
    return len;
}

/*********************************************************************/

int main(int argc, char *argv[])
{
    int exit_status;
    int code = 1, code1;
    void *instance = NULL;
    int exit_code;
    char *curlocale;
    /*
     * Call setlocale(LC_CTYPE), so that we can convert PDF passwords
     * from the locale character set to UTF-8 if necessary.  Note that
     * we only do this when running as a standalone application -- we
     * can't use setlocale at all if ghostscript is built as a library,
     * because it would affect the rest of the program.  Applications
     * that use ghostscript as a library are responsible for setting
     * the locale themselves.
     *
     * For now, we ignore the return value of setlocale, since there's
     * not much we can do here if it fails.  It might be nice to set
     * a flag instead, so we could warn the user if they later enter
     * a non-ASCII PDF password that doesn't work.
     */
    curlocale = setlocale(LC_CTYPE, "");

    /* run Ghostscript */
    if ((code = gsapi_new_instance(&instance, NULL)) == 0) {
        gsapi_set_stdio(instance, gsdll_stdin, gsdll_stdout, gsdll_stderr);

        if (curlocale == NULL || strstr(curlocale, "UTF-8") != NULL || strstr(curlocale, "utf8") != NULL)
            code = gsapi_set_arg_encoding(instance, 1); /* PS_ARG_ENCODING_UTF8 = 1 */
        else {
            code = gsapi_set_arg_encoding(instance, 0); /* PS_ARG_ENCODING_LOCAL = 0 */
        }

        code = gsapi_init_with_args(instance, argc, argv);

        if (code == 0)
            code = gsapi_run_string(instance, start_string, 0, &exit_code);
        code1 = gsapi_exit(instance);
        if (code == 0 || code == gs_error_Quit)
            code = code1;
        if (code == gs_error_Quit)
            code = 0;	/* user executed 'quit' */

        gsapi_delete_instance(instance);
    }

    exit_status = 0;
    switch (code) {
        case 0:
        case gs_error_Info:
        case gs_error_Quit:
            break;
        case gs_error_Fatal:
            exit_status = 1;
            break;
        default:
            exit_status = 255;
    }

    return exit_status;
}
