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
/* Construct monitors out of semaphores */
#include "std.h"
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

int
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
