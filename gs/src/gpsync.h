/* Copyright (C) 1998 Aladdin Enterprises.  All rights reserved.
  
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

/*$RCSfile$ $Revision$ */
/* Interface to platform-dependent synchronization primitives */

#if !defined(gpsync_INCLUDED)
#  define gpsync_INCLUDED

/* Initial version 4/1/98 by John Desrosiers (soho@crl.com). */
/* 8/9/98 L. Peter Deutsch (ghost@aladdin.com) Changed ...sizeof to
   procedures, added some comments. */

/* -------- Synchronization primitives ------- */

/*
 * Semaphores support wait/signal semantics: a wait operation will allow
 * control to proceed iff the number of signals since semaphore creation
 * is greater than the number of waits.
 */
typedef struct {
    void *dummy_;
} gp_semaphore;

uint gp_semaphore_sizeof(P0());
/*
 * Hack: gp_semaphore_open(0) succeeds iff it's OK for the memory manager
 * to move a gp_semaphore in memory.
 */
int gp_semaphore_open(P1(gp_semaphore * sema));
int gp_semaphore_close(P1(gp_semaphore * sema));
int gp_semaphore_wait(P1(gp_semaphore * sema));
int gp_semaphore_signal(P1(gp_semaphore * sema));

/*
 * Monitors support enter/leave semantics: at most one thread can have
 * entered and not yet left a given monitor.
 */
typedef struct {
    void *dummy_;
} gp_monitor;

uint gp_monitor_sizeof(P0());
/*
 * Hack: gp_monitor_open(0) succeeds iff it's OK for the memory manager
 * to move a gp_monitor in memory.
 */
int gp_monitor_open(P1(gp_monitor * mon));
int gp_monitor_close(P1(gp_monitor * mon));
int gp_monitor_enter(P1(gp_monitor * mon));
int gp_monitor_leave(P1(gp_monitor * mon));

/*
 * A new thread starts by calling a procedure, passing it a void * that
 * allows it to gain access to whatever data it needs.
 */
typedef void (*gp_thread_creation_callback_t) (P1(void *));
int gp_create_thread(P2(gp_thread_creation_callback_t, void *));

#endif /* !defined(gpsync_INCLUDED) */
