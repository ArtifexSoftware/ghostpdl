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

/* gpsync.h */
/* Platform-dependent synchronization primitives */

#if !defined(gpsync_INCLUDED)
 #define gpsync_INCLUDED

/* Initial version 4/1/98 by John Desrosiers (soho@crl.com) */

/* -------- Synchronization primitives ------- */

/* Semaphore supports wait/signal semantics */
typedef struct {char filler;} gp_semaphore;
extern const uint gp_semaphore_sizeof;

int gp_semaphore_open(P1(gp_semaphore *sema));
void gp_semaphore_close(P1(gp_semaphore *sema));
int gp_semaphore_wait(P1(gp_semaphore *sema));
int gp_semaphore_signal(P1(gp_semaphore *sema));

/* Monitor supports enter/leave semantics */
typedef struct {char filler;} gp_monitor;
extern const uint gp_monitor_sizeof;

int gp_monitor_open(P1(gp_monitor *mon));
void gp_monitor_close(P1(gp_monitor *mon));
int gp_monitor_enter(P1(gp_monitor *mon));
int gp_monitor_leave(P1(gp_monitor *mon));

typedef void(*gp_thread_creation_callback_t)(P1(void*));
int gp_create_thread(P2(gp_thread_creation_callback_t, void*));

#endif /* !defined(gpsync_INCLUDED) */
