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


/* 'main' program for Ghostscript */
#include "ghost.h"
#include "imain.h"
#include "imainarg.h"
#include "iapi.h"
#include "iminst.h"
#include "ierrors.h"
#include "gsmalloc.h"
#include "locale_.h"

#ifdef __GNUC__
#  if (__GNUC__ == 2 && __GNUC_MINOR__ == 96)
#    define NEED_COMMIT_STACK 1
#  endif
#endif

/* Define an optional array of strings for testing. */
/*#define RUN_STRINGS */
#ifdef RUN_STRINGS
static const char *run_strings[] =
{
    "2 vmreclaim /SAVE save def 2 vmreclaim",
    "(saved\n) print flush",
    "SAVE restore (restored\n) print flush 2 vmreclaim",
    "(done\n) print flush quit",
    0
};

#endif

static const char start_string[] = "systemdict /start get exec\n";

#ifdef NEED_COMMIT_STACK
/*
 * It is well known that GCC 2.96 for x86 sometimes forgets to adjust $esp
 * and leaves automatic variables at small distance below the stack pointer.
 * Apparently, when the access to the automatic variable causes a page fault
 * Linux sends a SEGV signal if the access happens below the stack pointer.
 * Pre-loading the stack pages resolves the problem.
 */
static void
commit_stack_pages( void )
{
    char buf[65536]; /* In most cases GS lives in 64K stack */
    int i;
    for ( i = 0; i < sizeof(buf) - 1; i += 1024)
        buf[i] = 0;
}
#endif

int
main(int argc, char *argv[])
{
    int exit_status, code;
    void *minst = NULL;
    char *curlocale;

#ifdef NEED_COMMIT_STACK   /* hack for bug in gcc 2.96 */
    commit_stack_pages();
#endif

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
    code = gsapi_new_instance(&minst, NULL);

    if (code >= 0) {
        if (curlocale == NULL || strstr(curlocale, "UTF-8") != NULL || strstr(curlocale, "utf8") != NULL)
            code = gsapi_set_arg_encoding(minst, 1); /* PS_ARG_ENCODING_UTF8 = 1 */
        else {
            code = gsapi_set_arg_encoding(minst, 0); /* PS_ARG_ENCODING_LOCAL = 0 */
        }
    }

    if (code >= 0)
      code = gsapi_init_with_args(minst, argc, argv);
#ifdef RUN_STRINGS
    {				/* Run a list of strings (for testing). */
        const char **pstr = run_strings;

        for (; *pstr; ++pstr) {
            int exit_code;
            ref error_object;

            fprintf(stdout, "{%s} =>\n", *pstr);
            fflush(stdout);
            code = gsapi_run_string(minst, *pstr, 0, &exit_status);
            zflush(minst->i_ctx_p);
            fprintf(stdout, " => code = %d\n", code);
            fflush(stdout);
            if (code < 0) {
                gs_to_exit(1);
                return 1;
            }
        }
    }
#endif

    if (code == 0)
        code = gsapi_run_string(minst, start_string, 0, &exit_status);

    exit_status = gsapi_exit(minst);
    if( code >= 0 && exit_status < 0)
        code = exit_status;

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

    if (minst)
        gsapi_delete_instance(minst);

    switch (exit_status) {
        case 0:
            exit_status =  exit_OK;
            break;
        case 1:
            exit_status =  exit_FAILED;
            break;
    }
    return exit_status;
}
