/* Copyright (C) 1998, 1999 Aladdin Enterprises.  All rights reserved.
 * This software is licensed to a single customer by Artifex Software Inc.
 * under the terms of a specific OEM agreement.
 */

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
