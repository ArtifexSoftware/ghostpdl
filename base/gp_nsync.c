/* Copyright (C) 2001-2019 Artifex Software, Inc.
   All Rights Reserved.

   This software is provided AS-IS with no warranty, either express or
   implied.

   This software is distributed under license and may not be copied,
   modified or distributed except as expressly authorized under the terms
   of the license contained in the file LICENSE in this distribution.

   Refer to licensing information at http://www.artifex.com or contact
   Artifex Software, Inc.,  1305 Grant Avenue - Suite 200, Novato,
   CA 94945, U.S.A., +1(415)492-9861, for further information.
*/


/* Dummy thread / semaphore / monitor implementation */
#include "std.h"
#include "gserrors.h"
#include "gpsync.h"

/* ------- Synchronization primitives -------- */

/* Semaphores */

uint
gp_semaphore_sizeof(void)
{
    return sizeof(gp_semaphore);
}

int
gp_semaphore_open(gp_semaphore * sema)
{
    if (sema)
        *(int *)sema = 0;
    return 0;
}

int
gp_semaphore_close(gp_semaphore * sema)
{
    return 0;
}

int
gp_semaphore_wait(gp_semaphore * sema)
{
    if (*(int *)sema == 0)
        return_error(gs_error_unknownerror); /* no real waiting */
    --(*(int *)sema);
    return 0;
}

int
gp_semaphore_signal(gp_semaphore * sema)
{
    ++(*(int *)sema);
    return 0;
}

/* Monitors */

uint
gp_monitor_sizeof(void)
{
    return sizeof(gp_monitor);
}

int
gp_monitor_open(gp_monitor * mon)
{
    if (mon)
        mon->dummy_ = 0;
    return 0;
}

int
gp_monitor_close(gp_monitor * mon)
{
    return 0;
}

int
gp_monitor_enter(gp_monitor * mon)
{
    if (mon->dummy_ != 0)
        return_error(gs_error_unknownerror);
    mon->dummy_ = mon;
    return 0;
}

int
gp_monitor_leave(gp_monitor * mon)
{
    if (mon->dummy_ != mon)
        return_error(gs_error_unknownerror);
    mon->dummy_ = 0;
    return 0;
}

/* Thread creation */

int
gp_create_thread(gp_thread_creation_callback_t proc, void *proc_data)
{
    return_error(gs_error_unknownerror);
}

int
gp_thread_start(gp_thread_creation_callback_t proc, void *proc_data, gp_thread_id *thread)
{
    *thread = NULL;
    return_error(gs_error_unknownerror);
}

void
gp_thread_finish(gp_thread_id thread)
{
}
