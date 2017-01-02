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

#include "plapi.h"
#include "gserrors.h"

int
main(int argc, char *argv[])
{
    int code, code1;
    void *minst;
    
    code = plapi_new_instance(&minst, (void *)0);
    if (code < 0)
	return 1;

    if (code == 0)
        code = plapi_init_with_args(minst, argc, argv);

    code1 = plapi_exit(minst);
    if ((code == 0) || (code == gs_error_Quit))
	code = code1;

    plapi_delete_instance(minst);

    if ((code == 0) || (code == gs_error_Quit))
	return 0;
    return 1;
}
