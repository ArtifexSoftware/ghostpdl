/* Portions Copyright (C) 2001 artofcode LLC.
   Portions Copyright (C) 1996, 2001 Artifex Software Inc.
   Portions Copyright (C) 1988, 2000 Aladdin Enterprises.
   This software is based in part on the work of the Independent JPEG Group.
   All Rights Reserved.

   This software is distributed under license and may not be copied, modified
   or distributed except as expressly authorized under the terms of that
   license.  Refer to licensing information at http://www.artifex.com/ or
   contact Artifex Software, Inc., 101 Lucas Valley Road #110,
   San Rafael, CA  94903, (415)492-9861, for further information. */

/*$RCSfile$ $Revision$ */
/* POSIX pthreads implementation of semaphores */
#include "std.h"
#include <pthread.h>
#include "gserror.h"
#include "gserrors.h"
#include "gpsync.h"

uint
gp_semaphore_sizeof(void)
{
    return sizeof(pthread_mutex_t);
}

/*
 * This procedure should really check errno and return something
 * more informative....
 */
#define SEM_ERROR_CODE(scode)\
  (scode < 0 ? gs_note_error(gs_error_ioerror) : 0)

int
gp_semaphore_open(gp_semaphore * sema)
{
    pthread_mutex_t * const sem = (pthread_mutex_t *)sema;
    pthread_mutexattr_t attr;
    int scode;

    if (!sema)
	return -1;		/* semaphores are not movable */
    pthread_mutexattr_init(&attr);
    pthread_mutexattr_setkind_np(&attr, PTHREAD_MUTEX_RECURSIVE_NP);
    scode = pthread_mutex_init(sem, &attr);
    return SEM_ERROR_CODE(scode);
}

int
gp_semaphore_close(gp_semaphore * sema)
{
    pthread_mutex_t * const sem = (pthread_mutex_t *)sema;
    int scode = pthread_mutex_destroy(sem);

    return SEM_ERROR_CODE(scode);
}

int
gp_semaphore_wait(gp_semaphore * sema)
{
    pthread_mutex_t * const sem = (pthread_mutex_t *)sema;
    int scode = pthread_mutex_lock(sem);

    return SEM_ERROR_CODE(scode);
}

int
gp_semaphore_signal(gp_semaphore * sema)
{
    pthread_mutex_t * const sem = (pthread_mutex_t *)sema;
    int scode = pthread_mutex_unlock(sem);

    return SEM_ERROR_CODE(scode);
}
