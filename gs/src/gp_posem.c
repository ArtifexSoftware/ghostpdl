/* Copyright (C) 1998 Aladdin Enterprises.  All rights reserved.

   This file is part of Aladdin Ghostscript.

   Aladdin Ghostscript is distributed with NO WARRANTY OF ANY KIND.  No author
   or distributor accepts any responsibility for the consequences of using it,
   or for whether it serves any particular purpose or works at all, unless he
   or she says so in writing.  Refer to the Aladdin Ghostscript Free Public
   License (the "License") for full details.

   Every copy of Aladdin Ghostscript must include a copy of the License,
   normally in a plain ASCII text file named PUBLIC.  The License grants you
   the right to copy, modify and redistribute Aladdin Ghostscript, but only
   under certain conditions described in the License.  Among other things, the
   License requires that the copyright notice and this notice be preserved on
   all copies.
 */


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

void
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
