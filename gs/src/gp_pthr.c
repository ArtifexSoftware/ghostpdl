/* Copyright (C) 1998, 1999 Aladdin Enterprises.  All rights reserved.
 * This software is licensed to a single customer by Artifex Software Inc.
 * under the terms of a specific OEM agreement.
 */

/*$RCSfile$ $Revision$ */
/* pthreads interface */
#include "std.h"
#include "malloc_.h"
#include <pthread.h>
#include "gserror.h"
#include "gserrors.h"
#include "gpsync.h"

/*
 * In order to deal with the type mismatch between our thread API, where
 * the starting procedure returns void, and the API defined by pthreads,
 * where the procedure returns void *, we need to create a wrapper
 * closure.
 */
typedef struct gp_thread_creation_closure_s {
    gp_thread_creation_callback_t proc;  /* actual start procedure */
    void *proc_data;			/* closure data for proc */
} gp_thread_creation_closure_t;

/* Wrapper procedure called to start the new thread. */
private void *
gp_thread_begin_wrapper(void *thread_data /* gp_thread_creation_closure_t * */)
{
    gp_thread_creation_closure_t closure;

    closure = *(gp_thread_creation_closure_t *)thread_data;
    free(thread_data);
    DISCARD(closure.proc(closure.proc_data));
    return NULL;		/* return value is ignored */
}

int
gp_create_thread(gp_thread_creation_callback_t proc, void *proc_data)
{
    gp_thread_creation_closure_t *closure =
	(gp_thread_creation_closure_t *)malloc(sizeof(*closure));
    pthread_t ignore_thread;
    pthread_attr_t attr;
    int code;

    if (!closure)
	return_error(gs_error_VMerror);
    closure->proc = proc;
    closure->proc_data = proc_data;
    pthread_attr_init(&attr);
    pthread_attr_setdetachstate(&attr, 1);
    code = pthread_create(&ignore_thread, &attr, gp_thread_begin_wrapper,
			  closure);
    if (code) {
	free(closure);
	return_error(gs_error_ioerror);
    }
    return 0;
}
