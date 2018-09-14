/* Copyright (C) 2001-2018 Artifex Software, Inc.
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

#include <stdlib.h>
#include "string_.h"
#include "plapi.h"
#include "gserrors.h"

#define PJL_UEL "\033%-12345X"

int
main(int argc, char *argv[])
{
    int code, code1;
    void *minst;
    size_t uel_len = strlen(PJL_UEL);
    
    code = plapi_new_instance(&minst, (void *)0);
    if (code < 0)
        return EXIT_FAILURE;

    if (code >= 0)
        code = plapi_init_with_args(minst, argc, argv);

    if (code >= 0)
        code = plapi_run_string_begin(minst);
    if (code >= 0)
        code = plapi_run_string_continue(minst, PJL_UEL, uel_len);
    if (code >= 0)
        code = plapi_run_string_end(minst);
    if (code == gs_error_InterpreterExit)
        code = 0;

    code1 = plapi_exit(minst);
    if ((code == 0) || (code == gs_error_Quit))
        code = code1;

    plapi_delete_instance(minst);

    if ((code == 0) || (code == gs_error_Quit))
        return EXIT_SUCCESS;
    return EXIT_FAILURE;
}
