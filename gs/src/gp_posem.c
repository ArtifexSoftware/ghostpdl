/* Copyright (C) 1998 Aladdin Enterprises.  All rights reserved.
 * This software is licensed to a single customer by Artifex Software Inc.
 * under the terms of a specific OEM agreement.
 */

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
