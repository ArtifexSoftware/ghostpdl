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
/* POSIX semaphore interface */
#include "std.h"
#include <semaphore.h>
#include "gserror.h"
#include "gserrors.h"
#include "gpsync.h"

uint
gp_semaphore_sizeof(void)
{
    return sizeof(sem_t);
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
    sem_t * const sem = (sem_t *)sema;
    int scode;

    if (!sema)
	return -1;		/* semaphores are not movable */
    scode = sem_init(sem, 0, 0);
    return SEM_ERROR_CODE(scode);
}

int
gp_semaphore_close(gp_semaphore * sema)
{
    sem_t * const sem = (sem_t *)sema;
    int scode = sem_destroy(sem);

    return SEM_ERROR_CODE(scode);
}

int
gp_semaphore_wait(gp_semaphore * sema)
{
    sem_t * const sem = (sem_t *)sema;
    int scode = sem_wait(sem);

    return SEM_ERROR_CODE(scode);
}

int
gp_semaphore_signal(gp_semaphore * sema)
{
    sem_t * const sem = (sem_t *)sema;
    int scode = sem_post(sem);

    return SEM_ERROR_CODE(scode);
}
