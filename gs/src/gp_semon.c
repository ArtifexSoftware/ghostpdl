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


/* Construct monitors out of semaphores */
#include "std.h"
#include <semaphore.h>
#include "gserror.h"
#include "gserrors.h"
#include "gpsync.h"

typedef struct semaphore_monitor_s {
    gp_semaphore sem;		/* size is actually unknonwn */
} semaphore_monitor_t;

uint
gp_monitor_sizeof(void)
{
    return gp_semaphore_sizeof();
}

int
gp_monitor_open(gp_monitor * mon)
{
    semaphore_monitor_t * const semon = (semaphore_monitor_t *)mon;
    int code;

    if (!mon)
	return gp_semaphore_open(0);
    code = gp_semaphore_open(&semon->sem);
    if (code < 0)
	return code;
    return gp_semaphore_signal(&semon->sem);
}

void
gp_monitor_close(gp_monitor * mon)
{
    semaphore_monitor_t * const semon = (semaphore_monitor_t *)mon;

    return gp_semaphore_close(&semon->sem);
}

int
gp_monitor_enter(gp_monitor * mon)
{
    semaphore_monitor_t * const semon = (semaphore_monitor_t *)mon;

    return gp_semaphore_wait(&semon->sem);
}

int
gp_monitor_leave(gp_monitor * mon)
{
    semaphore_monitor_t * const semon = (semaphore_monitor_t *)mon;

    return gp_semaphore_signal(&semon->sem);
}
