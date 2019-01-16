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


/* MS Windows (Win32) thread / semaphore / monitor implementation */
/* original multi-threading code by John Desrosiers */
#include "windows_.h"
#include "malloc_.h"
#include "gserrors.h"
#include "gpsync.h"
#include <process.h>

/* It seems that both Borland and Watcom *should* be able to cope with the
 * new style threading using _beginthreadex/_endthreadex. I am unable to test
 * this properly however, and the tests I have done lead me to believe it
 * may be problematic. Given that these platforms are not a support priority,
 * we leave the code falling back to just using the old style code (which
 * presumably works).
 *
 * The upshot of this should be that we continue to work exactly as before.
 *
 * To try using the new functions, simply omit the definition of
 * FALLBACK_TO_OLD_THREADING_FUNCTIONS below. If you find this works for
 * either Borland or Watcom, then please let us know and we can make the
 * change permanently.
 */
#if defined(__BORLANDC__)
#define FALLBACK_TO_OLD_THREADING_FUNCTIONS
#elif defined(__WATCOMC__)
#define FALLBACK_TO_OLD_THREADING_FUNCTIONS
#endif

/* ------- Synchronization primitives -------- */

/* Semaphore supports wait/signal semantics */

typedef struct win32_semaphore_s {
    HANDLE handle;		/* returned from CreateSemaphore */
} win32_semaphore;

uint
gp_semaphore_sizeof(void)
{
    return sizeof(win32_semaphore);
}

int	/* if sema <> 0 rets -ve error, 0 ok; if sema == 0, 0 movable, 1 fixed */
gp_semaphore_open(
                  gp_semaphore * sema	/* create semaphore here */
)
{
    win32_semaphore *const winSema = (win32_semaphore *)sema;

    if (winSema) {
        winSema->handle =
#ifdef METRO
            CreateSemaphoreExW(NULL, 0, max_int, NULL, 0, 0);
#else
            CreateSemaphore(NULL, 0, max_int, NULL);
#endif
        return
            (winSema->handle != NULL ? 0 :
             gs_note_error(gs_error_unknownerror));
    } else
        return 0;		/* Win32 semaphores handles may be moved */
}

int
gp_semaphore_close(
                   gp_semaphore * sema	/* semaphore to affect */
)
{
    win32_semaphore *const winSema = (win32_semaphore *)sema;

    if (winSema->handle != NULL)
        CloseHandle(winSema->handle);
    winSema->handle = NULL;
    return 0;
}

int				/* rets 0 ok, -ve error */
gp_semaphore_wait(
                  gp_semaphore * sema	/* semaphore to affect */
)
{
    win32_semaphore *const winSema = (win32_semaphore *)sema;

    return
        (
#ifdef METRO
        WaitForSingleObjectEx(winSema->handle, INFINITE, FALSE)
#else
        WaitForSingleObject(winSema->handle, INFINITE)
#endif
          == WAIT_OBJECT_0 ? 0 : gs_error_unknownerror);
}

int				/* rets 0 ok, -ve error */
gp_semaphore_signal(
                    gp_semaphore * sema	/* semaphore to affect */
)
{
    win32_semaphore *const winSema = (win32_semaphore *)sema;

    return
        (ReleaseSemaphore(winSema->handle, 1, NULL) ? 0 :
         gs_error_unknownerror);
}

/* Monitor supports enter/leave semantics */

typedef struct win32_monitor_s {
    CRITICAL_SECTION lock;	/* critical section lock */
} win32_monitor;

uint
gp_monitor_sizeof(void)
{
    return sizeof(win32_monitor);
}

int	/* if sema <> 0 rets -ve error, 0 ok; if sema == 0, 0 movable, 1 fixed */
gp_monitor_open(
                gp_monitor * mon	/* create monitor here */
)
{
    win32_monitor *const winMon = (win32_monitor *)mon;

    if (mon) {
#ifdef METRO
        InitializeCriticalSectionEx(&winMon->lock, 0, 0);	/* returns no status */
#else
        InitializeCriticalSection(&winMon->lock);	/* returns no status */
#endif
        return 0;
    } else
        return 1;		/* Win32 critical sections mutsn't be moved */
}

int
gp_monitor_close(
                 gp_monitor * mon	/* monitor to affect */
)
{
    win32_monitor *const winMon = (win32_monitor *)mon;

    DeleteCriticalSection(&winMon->lock);	/* rets no status */
    return 0;
}

int				/* rets 0 ok, -ve error */
gp_monitor_enter(
                 gp_monitor * mon	/* monitor to affect */
)
{
    win32_monitor *const winMon = (win32_monitor *)mon;

    EnterCriticalSection(&winMon->lock);	/* rets no status */
    return 0;
}

int				/* rets 0 ok, -ve error */
gp_monitor_leave(
                 gp_monitor * mon	/* monitor to affect */
)
{
    win32_monitor *const winMon = (win32_monitor *)mon;

    LeaveCriticalSection(&winMon->lock);	/* rets no status */
    return 0;
}

/* --------- Thread primitives ---------- */

typedef struct gp_thread_creation_closure_s {
    gp_thread_creation_callback_t function;	/* function to start */
    void *data;			/* magic data to pass to thread */
} gp_thread_creation_closure;

/* Origin of new threads started by gp_create_thread */
static void
gp_thread_begin_wrapper(
                        void *thread_data	/* gp_thread_creation_closure passed as magic data */
)
{
    gp_thread_creation_closure closure;

    closure = *(gp_thread_creation_closure *)thread_data;
    free(thread_data);
    (*closure.function)(closure.data);
    _endthread();
}

/* Call a function on a brand new thread */
int				/* 0 ok, -ve error */
gp_create_thread(
                 gp_thread_creation_callback_t function,	/* function to start */
                 void *data	/* magic data to pass to thread fn */
)
{
    /* Create the magic closure that thread_wrapper gets passed */
    gp_thread_creation_closure *closure =
        (gp_thread_creation_closure *)malloc(sizeof(*closure));

    if (!closure)
        return_error(gs_error_VMerror);
    closure->function = function;
    closure->data = data;

    /*
     * Start thread_wrapper.  The Watcom _beginthread returns (int)(-1) if
     * the call fails.  The Microsoft _beginthread returns -1 (according to
     * the doc, even though the return type is "unsigned long" !!!) if the
     * call fails; we aren't sure what the Borland _beginthread returns.
     * The hack with ~ avoids a source code commitment as to whether the
     * return type is [u]int or [u]long.
     *
     * BEGIN_THREAD is a macro (defined in windows_.h) because _beginthread
     * takes different arguments in Watcom C.
     */
    if (~BEGIN_THREAD(gp_thread_begin_wrapper, 128*1024, closure) != 0)
    {
        free(closure);
        return 0;
    }
    return_error(gs_error_unknownerror);
}

/* gp_thread_creation_closure passed as magic data */
static unsigned __stdcall
gp_thread_start_wrapper(void *thread_data)
{
    gp_thread_creation_closure closure;

    closure = *(gp_thread_creation_closure *)thread_data;
    free(thread_data);
    (*closure.function)(closure.data);
    _endthreadex(0);
    return 0;
}

int gp_thread_start(gp_thread_creation_callback_t function,
                    void *data, gp_thread_id *thread)
{
#ifdef FALLBACK_TO_OLD_THREADING_FUNCTIONS
    *thread = (gp_thread_id)1;
    return gp_create_thread(function, data);
#else
    /* Create the magic closure that thread_wrapper gets passed */
    HANDLE hThread;
    unsigned threadID;
    gp_thread_creation_closure *closure =
        (gp_thread_creation_closure *)malloc(sizeof(*closure));

    if (!closure)
        return_error(gs_error_VMerror);
    closure->function = function;
    closure->data = data;
    hThread = (HANDLE)_beginthreadex(NULL, 0, &gp_thread_start_wrapper,
                                     closure, 0, &threadID);
    if (hThread == (HANDLE)0)
    {
        free(closure);
        *thread = NULL;
        return_error(gs_error_unknownerror);
    }
    *thread = (gp_thread_id)hThread;
    return 0;
#endif
}

void gp_thread_finish(gp_thread_id thread)
{
#ifndef FALLBACK_TO_OLD_THREADING_FUNCTIONS
    if (thread == NULL)
        return;
#ifdef METRO
    WaitForSingleObjectEx((HANDLE)thread, INFINITE, FALSE);
#else
    WaitForSingleObject((HANDLE)thread, INFINITE);
#endif
    CloseHandle((HANDLE)thread);
#endif
}
