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


/* POSIX pthreads threads / semaphore / monitor implementation */
#include "std.h"
#include "string_.h"
#include "malloc_.h"
#include "unistd_.h" /* for __USE_UNIX98 */
#include <pthread.h>
#include "gserrors.h"
#include "gpsync.h"
#include "assert_.h"
/*
 * Thanks to Larry Jones <larry.jones@sdrc.com> for this revision of
 * Aladdin's original code into a form that depends only on POSIX APIs.
 */

/*
 * Some old versions of the pthreads library define
 * pthread_attr_setdetachstate as taking a Boolean rather than an enum.
 * Compensate for this here.
 */
#ifndef PTHREAD_CREATE_DETACHED
#  define PTHREAD_CREATE_DETACHED 1
#endif

/* ------- Synchronization primitives -------- */

/* Semaphore supports wait/signal semantics */

typedef struct pt_semaphore_t {
    int count;
    pthread_mutex_t mutex;
    pthread_cond_t cond;
} pt_semaphore_t;

uint
gp_semaphore_sizeof(void)
{
    return sizeof(pt_semaphore_t);
}

/*
 * This procedure should really check errno and return something
 * more informative....
 */
#define SEM_ERROR_CODE(scode)\
  (scode != 0 ? gs_note_error(gs_error_ioerror) : 0)

int
gp_semaphore_open(gp_semaphore * sema)
{
    pt_semaphore_t * const sem = (pt_semaphore_t *)sema;
    int scode;

#ifdef MEMENTO
    if (Memento_squeezing()) {
         /* If squeezing, we nobble all the locking functions to do nothing.
          * We also ensure we never actually create threads (elsewhere),
          * so this is still safe. */
        memset(&sem->mutex, 0, sizeof(sem->mutex));
        memset(&sem->cond, 0, sizeof(sem->cond));
        return 0;
    }
#endif

    if (!sema)
        return -1;		/* semaphores are not movable */
    sem->count = 0;
    scode = pthread_mutex_init(&sem->mutex, NULL);
    if (scode == 0)
    {
        scode = pthread_cond_init(&sem->cond, NULL);
        if (scode)
            pthread_mutex_destroy(&sem->mutex);
    }
    if (scode)
        memset(sem, 0, sizeof(*sem));
    return SEM_ERROR_CODE(scode);
}

int
gp_semaphore_close(gp_semaphore * sema)
{
    pt_semaphore_t * const sem = (pt_semaphore_t *)sema;
    int scode, scode2;

#ifdef MEMENTO
    if (Memento_squeezing())
        return 0;
#endif

    scode = pthread_cond_destroy(&sem->cond);
    scode2 = pthread_mutex_destroy(&sem->mutex);
    if (scode == 0)
        scode = scode2;
    return SEM_ERROR_CODE(scode);
}

int
gp_semaphore_wait(gp_semaphore * sema)
{
    pt_semaphore_t * const sem = (pt_semaphore_t *)sema;
    int scode, scode2;

#ifdef MEMENTO
    if (Memento_squeezing()) {
         /* If squeezing, we nobble all the locking functions to do nothing.
          * We also ensure we never actually create threads (elsewhere),
          * so this is still safe. */
        return 0;
    }
#endif

    scode = pthread_mutex_lock(&sem->mutex);
    if (scode != 0)
        return SEM_ERROR_CODE(scode);
    while (sem->count == 0) {
        scode = pthread_cond_wait(&sem->cond, &sem->mutex);
        if (scode != 0)
            break;
    }
    if (scode == 0)
        --sem->count;
    scode2 = pthread_mutex_unlock(&sem->mutex);
    if (scode == 0)
        scode = scode2;
    return SEM_ERROR_CODE(scode);
}

int
gp_semaphore_signal(gp_semaphore * sema)
{
    pt_semaphore_t * const sem = (pt_semaphore_t *)sema;
    int scode, scode2;

#ifdef MEMENTO
    if (Memento_squeezing())
        return 0;
#endif

    scode = pthread_mutex_lock(&sem->mutex);
    if (scode != 0)
        return SEM_ERROR_CODE(scode);
    if (sem->count++ == 0)
        scode = pthread_cond_signal(&sem->cond);
    scode2 = pthread_mutex_unlock(&sem->mutex);
    if (scode == 0)
        scode = scode2;
    return SEM_ERROR_CODE(scode);
}

/* Monitor supports enter/leave semantics */

/*
 * We need PTHREAD_MUTEX_RECURSIVE behavior, but this isn't
 * supported on all pthread platforms, so if it's available
 * we'll use it, otherwise we'll emulate it.
 * GS_RECURSIVE_MUTEXATTR is set by the configure script
 * on Unix-like machines to the attribute setting for
 * PTHREAD_MUTEX_RECURSIVE - on linux this is usually
 * PTHREAD_MUTEX_RECURSIVE_NP
 */
typedef struct gp_pthread_recursive_s {
    pthread_mutex_t mutex;	/* actual mutex */
#ifndef GS_RECURSIVE_MUTEXATTR
    pthread_t	self_id;	/* owner */
    int lcount;
#endif
} gp_pthread_recursive_t;

uint
gp_monitor_sizeof(void)
{
    return sizeof(gp_pthread_recursive_t);
}

int
gp_monitor_open(gp_monitor * mona)
{
    pthread_mutex_t *mon;
    int scode;
    pthread_mutexattr_t attr;
    pthread_mutexattr_t *attrp = NULL;

    if (!mona)
        return -1;		/* monitors are not movable */

#ifdef MEMENTO
    if (Memento_squeezing()) {
         memset(mona, 0, sizeof(*mona));
         return 0;
    }
#endif

#ifdef GS_RECURSIVE_MUTEXATTR
    attrp = &attr;
    scode = pthread_mutexattr_init(attrp);
    if (scode < 0) goto done;

    scode = pthread_mutexattr_settype(attrp, GS_RECURSIVE_MUTEXATTR);
    if (scode < 0) {
        goto done;
    }
#else     
    ((gp_pthread_recursive_t *)mona)->self_id = 0;	/* Not valid unless mutex is locked */
    ((gp_pthread_recursive_t *)mona)->lcount = 0;
#endif

    mon = &((gp_pthread_recursive_t *)mona)->mutex;
    scode = pthread_mutex_init(mon, attrp);
    if (attrp)
        (void)pthread_mutexattr_destroy(attrp);
done:
    return SEM_ERROR_CODE(scode);
}

int
gp_monitor_close(gp_monitor * mona)
{
    pthread_mutex_t * const mon = &((gp_pthread_recursive_t *)mona)->mutex;
    int scode;

#ifdef MEMENTO
    if (Memento_squeezing())
         return 0;
#endif

    scode = pthread_mutex_destroy(mon);
    return SEM_ERROR_CODE(scode);
}

int
gp_monitor_enter(gp_monitor * mona)
{
    pthread_mutex_t * const mon = (pthread_mutex_t *)mona;
    int scode;

#ifdef MEMENTO
    if (Memento_squeezing()) {
         return 0;
    }
#endif

#ifdef GS_RECURSIVE_MUTEXATTR
    scode = pthread_mutex_lock(mon);
#else
    assert(((gp_pthread_recursive_t *)mona)->lcount >= 0);

    if ((scode = pthread_mutex_trylock(mon)) == 0) {
        ((gp_pthread_recursive_t *)mona)->self_id = pthread_self();
        ((gp_pthread_recursive_t *)mona)->lcount++;
    } else {
        if (pthread_equal(pthread_self(),((gp_pthread_recursive_t *)mona)->self_id)) {
            ((gp_pthread_recursive_t *)mona)->lcount++;
            scode = 0;
        }
        else {
            /* we were not the owner, wait */
            scode = pthread_mutex_lock(mon);
            ((gp_pthread_recursive_t *)mona)->self_id = pthread_self();
            ((gp_pthread_recursive_t *)mona)->lcount++;
        }
    }
#endif
    return SEM_ERROR_CODE(scode);
}

int
gp_monitor_leave(gp_monitor * mona)
{
    pthread_mutex_t * const mon = (pthread_mutex_t *)mona;
    int scode = 0;

#ifdef MEMENTO
    if (Memento_squeezing())
         return 0;
#endif

#ifdef GS_RECURSIVE_MUTEXATTR
    scode = pthread_mutex_unlock(mon);
#else
    assert(((gp_pthread_recursive_t *)mona)->lcount > 0 && ((gp_pthread_recursive_t *)mona)->self_id != 0);

    if (pthread_equal(pthread_self(),((gp_pthread_recursive_t *)mona)->self_id)) {
      if ((--((gp_pthread_recursive_t *)mona)->lcount) == 0) {
          ((gp_pthread_recursive_t *)mona)->self_id = 0;	/* Not valid unless mutex is locked */
          scode = pthread_mutex_unlock(mon);

      }
    }
    else {
        scode = -1 /* should be EPERM */;
    }
#endif
    return SEM_ERROR_CODE(scode);
}

/* --------- Thread primitives ---------- */

/*
 * In order to deal with the type mismatch between our thread API, where
 * the starting procedure returns void, and the API defined by pthreads,
 * where the procedure returns void *, we need to create a wrapper
 * closure.
 */
typedef struct gp_thread_creation_closure_s {
    gp_thread_creation_callback_t proc;  /* actual start procedure */
    void *proc_data;			/* closure data for proc */
} gp_thread_creation_closure_t;

/* Wrapper procedure called to start the new thread. */
static void *
gp_thread_begin_wrapper(void *thread_data /* gp_thread_creation_closure_t * */)
{
    gp_thread_creation_closure_t closure;

    closure = *(gp_thread_creation_closure_t *)thread_data;
    free(thread_data);
    DISCARD(closure.proc(closure.proc_data));
    return NULL;		/* return value is ignored */
}

int
gp_create_thread(gp_thread_creation_callback_t proc, void *proc_data)
{
    gp_thread_creation_closure_t *closure;
    pthread_t ignore_thread;
    pthread_attr_t attr;
    int code;

#ifdef MEMENTO
    if (Memento_squeezing()) {
        eprintf("Can't create threads when memory squeezing with forks\n");
        Memento_bt();
        return_error(gs_error_VMerror);
    }
#endif

    closure = (gp_thread_creation_closure_t *)malloc(sizeof(*closure));
    if (!closure)
        return_error(gs_error_VMerror);
    closure->proc = proc;
    closure->proc_data = proc_data;
    pthread_attr_init(&attr);
    pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);
    code = pthread_create(&ignore_thread, &attr, gp_thread_begin_wrapper,
                          closure);
    if (code) {
        free(closure);
        return_error(gs_error_ioerror);
    }
    return 0;
}

int
gp_thread_start(gp_thread_creation_callback_t proc, void *proc_data,
                gp_thread_id *thread)
{
    gp_thread_creation_closure_t *closure =
        (gp_thread_creation_closure_t *)malloc(sizeof(*closure));
    pthread_t new_thread;
    pthread_attr_t attr;
    int code;

    if (!closure)
        return_error(gs_error_VMerror);
    closure->proc = proc;
    closure->proc_data = proc_data;
    pthread_attr_init(&attr);
    pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_JOINABLE);
    code = pthread_create(&new_thread, &attr, gp_thread_begin_wrapper,
                          closure);
    if (code) {
        *thread = NULL;
        free(closure);
        return_error(gs_error_ioerror);
    }
    *thread = (gp_thread_id)new_thread;
    return 0;
}

void gp_thread_finish(gp_thread_id thread)
{
    if (thread == NULL)
        return;
    pthread_join((pthread_t)thread, NULL);
}

void (gp_monitor_label)(gp_monitor * mona, const char *name)
{
    pthread_mutex_t * const mon = &((gp_pthread_recursive_t *)mona)->mutex;

    (void)mon;
    (void)name;
    Bobbin_label_mutex(mon, name);
}

void (gp_semaphore_label)(gp_semaphore * sema, const char *name)
{
    pt_semaphore_t * const sem = (pt_semaphore_t *)sema;

    (void)sem;
    (void)name;
    Bobbin_label_mutex(&sem->mutex, name);
    Bobbin_label_cond(&sem->cond, name);
}

void (gp_thread_label)(gp_thread_id thread, const char *name)
{
    (void)thread;
    (void)name;
    Bobbin_label_thread((pthread_t)thread, name);
}
