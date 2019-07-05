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



/* dwdll.c */

#define STRICT
#include <windows.h>
#include <string.h>
#include <stdio.h>

#include "stdpre.h"
#include "gpgetenv.h"
#include "gscdefs.h"
#define GSREVISION gs_revision

#define GSDLLEXPORT
#define GSDLLAPI CALLBACK
#define GSDLLCALL

#include "dwdll.h"

#ifdef METRO
#ifdef _WIN64
static const char name[] = "gsdll64metro.dll";
#else
static const char name[] = "gsdll32metro.dll";
#endif
#else
#ifdef _WIN64
static const char name[] = "gsdll64.dll";
#else
static const char name[] = "gsdll32.dll";
#endif
#endif

int load_dll(GSDLL *gsdll, char *last_error, int len)
{
char fullname[1024];
char *p;
int length;
gsapi_revision_t rv;

    /* Don't load if already loaded */
    if (gsdll->hmodule)
        return 0;

    /* First try to load DLL from the same directory as EXE */
    GetModuleFileName(GetModuleHandle(NULL), fullname, sizeof(fullname));
    if ((p = strrchr(fullname,'\\')) != (char *)NULL)
        p++;
    else
        p = fullname;
    *p = '\0';
    strcat(fullname, name);
    gsdll->hmodule = LoadLibrary(fullname);

    /* Next try to load DLL with name in registry or environment variable */
    if (gsdll->hmodule < (HINSTANCE)HINSTANCE_ERROR) {
        length = sizeof(fullname);
        if (gp_getenv("GS_DLL", fullname, &length) == 0)
            gsdll->hmodule = LoadLibrary(fullname);
    }

    /* Finally try the system search path */
    if (gsdll->hmodule < (HINSTANCE)HINSTANCE_ERROR)
        gsdll->hmodule = LoadLibrary(name);

    if (gsdll->hmodule < (HINSTANCE)HINSTANCE_ERROR) {
        /* Failed */
        DWORD err = GetLastError();
        sprintf(fullname, "Can't load DLL, LoadLibrary error code %ld", err);
        strncpy(last_error, fullname, len-1);
        gsdll->hmodule = (HINSTANCE)0;
        return 1;
    }

    /* DLL is now loaded */
    /* Get pointers to functions */
#define ASSIGN_API_FN(A) \
    do { \
        gsdll->A = (PFN_gsapi_##A) GetProcAddress(gsdll->hmodule,\
                                                 "gsapi_" # A);\
	if (gsdll->A == NULL) {\
            strncpy(last_error, "Can't find gsapi_" # A "\n", len-1);\
            unload_dll(gsdll);\
            return 1;\
        }\
    } while (0)

    ASSIGN_API_FN(revision);

    /* check DLL version */
    if (gsdll->revision(&rv, sizeof(rv)) != 0) {
        sprintf(fullname, "Unable to identify Ghostscript DLL revision - it must be newer than needed.\n");
        strncpy(last_error, fullname, len-1);
        unload_dll(gsdll);
        return 1;
    }
    if (rv.revision != GSREVISION) {
        sprintf(fullname, "Wrong version of DLL found.\n  Found version %ld\n  Need version  %ld\n", rv.revision, GSREVISION);
        strncpy(last_error, fullname, len-1);
        unload_dll(gsdll);
        return 1;
    }

    /* continue loading other functions */
    ASSIGN_API_FN(new_instance);
    ASSIGN_API_FN(delete_instance);
    ASSIGN_API_FN(set_stdio);
    ASSIGN_API_FN(set_poll);
    ASSIGN_API_FN(set_display_callback);
    ASSIGN_API_FN(get_default_device_list);
    ASSIGN_API_FN(set_default_device_list);
    ASSIGN_API_FN(init_with_args);
    ASSIGN_API_FN(set_arg_encoding);
    ASSIGN_API_FN(run_string);
    ASSIGN_API_FN(exit);
    ASSIGN_API_FN(set_arg_encoding);
    ASSIGN_API_FN(run_file);
    ASSIGN_API_FN(run_string_begin);
    ASSIGN_API_FN(run_string_continue);
    ASSIGN_API_FN(run_string_end);
    ASSIGN_API_FN(run_string_with_length);
    ASSIGN_API_FN(set_param);
    ASSIGN_API_FN(add_control_path);
    ASSIGN_API_FN(remove_control_path);
    ASSIGN_API_FN(purge_control_paths);
    ASSIGN_API_FN(activate_path_control);
    ASSIGN_API_FN(is_path_control_active);
    ASSIGN_API_FN(add_fs);
    ASSIGN_API_FN(remove_fs);

    return 0;
}

void unload_dll(GSDLL *gsdll)
{
    /* Set functions to NULL to prevent use */
    gsdll->revision = NULL;
    gsdll->new_instance = NULL;
    gsdll->delete_instance = NULL;
    gsdll->init_with_args = NULL;
    gsdll->run_string = NULL;
    gsdll->exit = NULL;
    gsdll->set_stdio = NULL;
    gsdll->set_poll = NULL;
    gsdll->set_display_callback = NULL;

    if (gsdll->hmodule != (HINSTANCE)NULL)
            FreeLibrary(gsdll->hmodule);
    gsdll->hmodule = NULL;
}
