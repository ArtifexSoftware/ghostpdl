/* Copyright (C) 1996-2000 Ghostgum Software Pty Ltd.  All rights reserved.
  
  This file is part of AFPL Ghostscript.
  
  AFPL Ghostscript is distributed with NO WARRANTY OF ANY KIND.  No author or
  distributor accepts any responsibility for the consequences of using it, or
  for whether it serves any particular purpose or works at all, unless he or
  she says so in writing.  Refer to the Aladdin Free Public License (the
  "License") for full details.
  
  Every copy of AFPL Ghostscript must include a copy of the License, normally
  in a plain ASCII text file named PUBLIC.  The License grants you the right
  to copy, modify and redistribute AFPL Ghostscript, but only under certain
  conditions described in the License.  Among other things, the License
  requires that the copyright notice and this notice be preserved on all
  copies.
*/

/* $Id$ */

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

static const char name[] = "gsdll32.dll";

int load_dll(GSDLL *gsdll, char *last_error, int len)
{
char fullname[1024];
const char *shortname;
char *p;
long version;
int length;
gsapi_revision_t rv;

    /* Don't load if already loaded */
    if (gsdll->hmodule)
	return 0;

    /* First try to load DLL with name in registry or environment variable */
    gsdll->hmodule = (HINSTANCE)NULL;
    length = sizeof(fullname);
    if (gp_getenv("GS_DLL", fullname, &length) == 0)
        gsdll->hmodule = LoadLibrary(fullname);

    /* Next try to load DLL first with given path */
    if (gsdll->hmodule < (HINSTANCE)HINSTANCE_ERROR)
        gsdll->hmodule = LoadLibrary(name);
    if (gsdll->hmodule < (HINSTANCE)HINSTANCE_ERROR) {
	/* failed */
	/* try again, with path of EXE */
	if ((shortname = strrchr((char *)name, '\\')) == (const char *)NULL)
	    shortname = name;
	GetModuleFileName(GetModuleHandle(NULL), fullname, sizeof(fullname));
	if ((p = strrchr(fullname,'\\')) != (char *)NULL)
	    p++;
	else
	    p = fullname;
	*p = '\0';
	strcat(fullname, shortname);
        gsdll->hmodule = LoadLibrary(fullname);
        if (gsdll->hmodule < (HINSTANCE)HINSTANCE_ERROR) {
	    /* failed again */
	    /* try once more, this time on system search path */
            gsdll->hmodule = LoadLibrary(shortname);
            if (gsdll->hmodule < (HINSTANCE)HINSTANCE_ERROR) {
		sprintf(fullname, "Couldn't load DLL, LoadLibrary error code %d", (int)gsdll->hmodule);
	 	strncpy(last_error, fullname, len-1);
		gsdll->hmodule = (HINSTANCE)0;
		return 1;
	    }
	}
    }

    /* DLL is now loaded */
    /* Get pointers to functions */
    gsdll->revision = (PFN_gsapi_revision) GetProcAddress(gsdll->hmodule, 
	"gsapi_revision");
    if (gsdll->revision == NULL) {
	strncpy(last_error, "Can't find gsapi_revision\n", len-1);
	unload_dll(gsdll);
	return 1;
    }
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
    gsdll->new_instance = (PFN_gsapi_new_instance) GetProcAddress(gsdll->hmodule, 
	"gsapi_new_instance");
    if (gsdll->new_instance == NULL) {
	strncpy(last_error, "Can't find gsapi_new_instance\n", len-1);
	unload_dll(gsdll);
	return 1;
    }

    gsdll->delete_instance = (PFN_gsapi_delete_instance) GetProcAddress(gsdll->hmodule, 
	"gsapi_delete_instance");
    if (gsdll->delete_instance == NULL) {
	strncpy(last_error, "Can't find gsapi_delete_instance\n", len-1);
	unload_dll(gsdll);
	return 1;
    }

    gsdll->set_stdio = (PFN_gsapi_set_stdio) GetProcAddress(gsdll->hmodule, 
	"gsapi_set_stdio");
    if (gsdll->set_stdio == NULL) {
	strncpy(last_error, "Can't find gsapi_set_stdio\n", len-1);
	unload_dll(gsdll);
	return 1;
    }

    gsdll->set_poll = (PFN_gsapi_set_poll) GetProcAddress(gsdll->hmodule, 
	"gsapi_set_poll");
    if (gsdll->set_poll == NULL) {
	strncpy(last_error, "Can't find gsapi_set_poll\n", len-1);
	unload_dll(gsdll);
	return 1;
    }

    gsdll->set_display_callback = (PFN_gsapi_set_display_callback) 
	GetProcAddress(gsdll->hmodule, "gsapi_set_display_callback");
    if (gsdll->set_display_callback == NULL) {
	strncpy(last_error, "Can't find gsapi_set_display_callback\n", len-1);
	unload_dll(gsdll);
	return 1;
    }

    gsdll->init_with_args = (PFN_gsapi_init_with_args) 
	GetProcAddress(gsdll->hmodule, "gsapi_init_with_args");
    if (gsdll->init_with_args == NULL) {
	strncpy(last_error, "Can't find gsapi_init_with_args\n", len-1);
	unload_dll(gsdll);
	return 1;
    }

    gsdll->run_string = (PFN_gsapi_run_string) GetProcAddress(gsdll->hmodule, 
	"gsapi_run_string");
    if (gsdll->run_string == NULL) {
	strncpy(last_error, "Can't find gsapi_run_string\n", len-1);
	unload_dll(gsdll);
	return 1;
    }

    gsdll->exit = (PFN_gsapi_exit) GetProcAddress(gsdll->hmodule, 
	"gsapi_exit");
    if (gsdll->exit == NULL) {
	strncpy(last_error, "Can't find gsapi_exit\n", len-1);
	unload_dll(gsdll);
	return 1;
    }

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


