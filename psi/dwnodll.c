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


/* dwnodll.c */

#define STRICT
#include <windows.h>
#include <string.h>
#include <stdio.h>

#include "stdpre.h"
#include "gpgetenv.h"
#include "gscdefs.h"
#define GSREVISION gs_revision

#include "dwdll.h"

/* We are static linking, so just store the function addresses */
int load_dll(GSDLL *gsdll, char *last_error, int len)
{
    gsdll->new_instance = &gsapi_new_instance;
    gsdll->delete_instance = &gsapi_delete_instance;
    gsdll->set_stdio = &gsapi_set_stdio;
    gsdll->set_poll = &gsapi_set_poll;
    gsdll->set_display_callback = &gsapi_set_display_callback;
    gsdll->init_with_args = &gsapi_init_with_args;
    gsdll->set_arg_encoding = &gsapi_set_arg_encoding;
    gsdll->run_string = &gsapi_run_string;
    gsdll->exit = &gsapi_exit;
    gsdll->set_default_device_list = &gsapi_set_default_device_list;
    gsdll->get_default_device_list = &gsapi_get_default_device_list;
    return 0;
}

void unload_dll(GSDLL *gsdll)
{
}
