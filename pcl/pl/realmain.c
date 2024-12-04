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

#include <stdlib.h>
#include "string_.h"
#include "plapi.h"
#include "gserrors.h"
#include "locale_.h"

#define PJL_UEL "\033%-12345X"

int
main(int argc, char *argv[])
{
    int code, code1;
    void *minst = NULL;
    size_t uel_len = strlen(PJL_UEL);
    int dummy;
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
    code = gsapi_new_instance(&minst, (void *)0);
    if (code < 0)
        return EXIT_FAILURE;

    if (curlocale == NULL || strstr(curlocale, "UTF-8") != NULL || strstr(curlocale, "utf8") != NULL)
        code = gsapi_set_arg_encoding(minst, 1); /* PS_ARG_ENCODING_UTF8 = 1 */
    else {
        code = gsapi_set_arg_encoding(minst, 0); /* PS_ARG_ENCODING_LOCAL = 0 */
    }

    code = gsapi_init_with_args(minst, argc, argv);
    if (code >= 0)
        code = gsapi_run_string_begin(minst, 0, &dummy);
    if (code >= 0)
        code = gsapi_run_string_continue(minst, PJL_UEL, uel_len, 0, &dummy);
    if (code >= 0)
        code = gsapi_run_string_end(minst, 0, &dummy);
    if (code == gs_error_InterpreterExit)
        code = 0;

    code1 = gsapi_exit(minst);
    if ((code == 0) || (code == gs_error_Quit))
        code = code1;

    gsapi_delete_instance(minst);

    if ((code == 0) || (code == gs_error_Quit))
        return EXIT_SUCCESS;
    return EXIT_FAILURE;
}
