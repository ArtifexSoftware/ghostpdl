/* Copyright (C) 1996-2001 Ghostgum Software Pty Ltd.  All rights reserved.
  
  This software is provided AS-IS with no warranty, either express or
  implied.
  
  This software is distributed under license and may not be copied,
  modified or distributed except as expressly authorized under the terms
  of the license contained in the file LICENSE in this distribution.
  
  For more information about licensing, please refer to
  http://www.ghostscript.com/licensing/. For information on
  commercial licensing, go to http://www.artifex.com/licensing/ or
  contact Artifex Software, Inc., 101 Lucas Valley Road #110,
  San Rafael, CA  94903, U.S.A., +1(415)492-9861.
*/

/* $Id$ */

/* Public Application Programming Interface to Ghostscript interpreter */

#include "string_.h"
#include "errors.h"
#include "gscdefs.h"
#include "gstypes.h"
#include "iapi.h"	/* Public API */
#include "iref.h"
#include "iminst.h"
#include "imain.h"
#include "imainarg.h"


/*
 * GLOBAL WARNING GLOBAL WARNING GLOBAL WARNING GLOBAL WARNING
 *
 *                 The nasty global variables
 *
 * GLOBAL WARNING GLOBAL WARNING GLOBAL WARNING GLOBAL WARNING
 */
int gsapi_instance_counter = 0;
/* extern gs_main_instance *gs_main_instance_default(); */


/* Return revision numbers and strings of Ghostscript. */
/* Used for determining if wrong GSDLL loaded. */
/* This may be called before any other function. */
GSDLLEXPORT int GSDLLAPI
gsapi_revision(gsapi_revision_t *pr, int rvsize)
{
    if (rvsize < sizeof(gsapi_revision_t))
	return sizeof(gsapi_revision_t);
    pr->product = gs_product;
    pr->copyright = gs_copyright;
    pr->revision = gs_revision;
    pr->revisiondate = gs_revisiondate;
    return 0;
}

/* Create a new instance of Ghostscript. */
/* We do not support multiple instances, so make sure
 * we use the default instance only once.
 */
GSDLLEXPORT int GSDLLAPI 
gsapi_new_instance(gs_main_instance **pinstance, void *caller_handle)
{
    gs_main_instance *minst;
    if (gsapi_instance_counter != 0) {
	*pinstance = NULL;
	return e_Fatal;
    }
    gsapi_instance_counter++;
    minst = gs_main_instance_default();
    minst->caller_handle = caller_handle;
    minst->stdin_fn = NULL;
    minst->stdout_fn = NULL;
    minst->stderr_fn = NULL;
    minst->poll_fn = NULL;
    minst->display = NULL;
    minst->i_ctx_p = NULL;
    *pinstance = minst;
    return 0;
}

/* Destroy an instance of Ghostscript */
/* We do not support multiple instances, so make sure
 * we use the default instance only once.
 */
GSDLLEXPORT void GSDLLAPI 
gsapi_delete_instance(gs_main_instance *minst)
{
    if ((gsapi_instance_counter > 0) && (minst != NULL)) {
	minst->caller_handle = NULL;
	minst->stdin_fn = NULL;
	minst->stdout_fn = NULL;
	minst->stderr_fn = NULL;
	minst->poll_fn = NULL;
	minst->display = NULL;
	gsapi_instance_counter--;
    }
}

/* Set the callback functions for stdio */
GSDLLEXPORT int GSDLLAPI 
gsapi_set_stdio(gs_main_instance *minst,
    int(GSDLLCALL *stdin_fn)(void *caller_handle, char *buf, int len),
    int(GSDLLCALL *stdout_fn)(void *caller_handle, const char *str, int len),
    int(GSDLLCALL *stderr_fn)(void *caller_handle, const char *str, int len))
{
    if (minst == NULL)
	return e_Fatal;
    minst->stdin_fn = stdin_fn;
    minst->stdout_fn = stdout_fn;
    minst->stderr_fn = stderr_fn;
    return 0;
}

/* Set the callback function for polling */
GSDLLEXPORT int GSDLLAPI 
gsapi_set_poll(gs_main_instance *minst, 
    int(GSDLLCALL *poll_fn)(void *caller_handle))
{
    if (minst == NULL)
	return e_Fatal;
    minst->poll_fn = poll_fn;
    return 0;
}

/* Set the display callback structure */
GSDLLEXPORT int GSDLLAPI 
gsapi_set_display_callback(gs_main_instance *minst, display_callback *callback)
{
    if (minst == NULL)
	return e_Fatal;
    minst->display = callback;
    return 0;
}


/* Initialise the interpreter */
GSDLLEXPORT int GSDLLAPI 
gsapi_init_with_args(gs_main_instance *minst, int argc, char **argv)
{
    if (minst == NULL)
	return e_Fatal;
    return gs_main_init_with_args(minst, argc, argv);
}



/* The gsapi_run_* functions are like gs_main_run_* except
 * that the error_object is omitted.
 * An error object in minst is used instead.
 */

/* Setup up a suspendable run_string */
GSDLLEXPORT int GSDLLAPI 
gsapi_run_string_begin(gs_main_instance *minst, int user_errors, 
	int *pexit_code)
{
    if (minst == NULL)
	return e_Fatal;

    return gs_main_run_string_begin(minst, 
	user_errors, pexit_code, &minst->error_object);
}


GSDLLEXPORT int GSDLLAPI 
gsapi_run_string_continue(gs_main_instance *minst, 
	const char *str, uint length, int user_errors, int *pexit_code)
{
    if (minst == NULL)
	return e_Fatal;

    return gs_main_run_string_continue(minst,
	str, length, user_errors, pexit_code, &minst->error_object);
}

GSDLLEXPORT int GSDLLAPI 
gsapi_run_string_end(gs_main_instance *minst, 
	int user_errors, int *pexit_code)
{
    if (minst == NULL)
	return e_Fatal;

    return gs_main_run_string_end(minst,
	user_errors, pexit_code, &minst->error_object);
}

GSDLLEXPORT int GSDLLAPI 
gsapi_run_string_with_length(gs_main_instance *minst, 
	const char *str, uint length, int user_errors, int *pexit_code)
{
    if (minst == NULL)
	return e_Fatal;

    return gs_main_run_string_with_length(minst,
	str, length, user_errors, pexit_code, &minst->error_object);
}

GSDLLEXPORT int GSDLLAPI 
gsapi_run_string(gs_main_instance *minst, 
	const char *str, int user_errors, int *pexit_code)
{
    return gsapi_run_string_with_length(minst, 
	str, (uint)strlen(str), user_errors, pexit_code);
}

GSDLLEXPORT int GSDLLAPI 
gsapi_run_file(gs_main_instance *minst, const char *file_name, 
	int user_errors, int *pexit_code)
{
    if (minst == NULL)
	return e_Fatal;

    return gs_main_run_file(minst,
	file_name, user_errors, pexit_code, &minst->error_object);
}


/* Exit the interpreter */
GSDLLEXPORT int GSDLLAPI 
gsapi_exit(gs_main_instance *minst)
{
    if (minst == NULL)
	return e_Fatal;

    gs_exit(0);
    return 0;
}

/* Visual tracer : */
struct vd_trace_interface_s;
extern struct vd_trace_interface_s * vd_trace0;
GSDLLEXPORT void GSDLLAPI
gsapi_set_visual_tracer(P1(struct vd_trace_interface_s *I))
{   vd_trace0 = I;
}

/* end of iapi.c */
