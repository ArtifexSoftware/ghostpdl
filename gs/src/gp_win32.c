/* Copyright (C) 1992, 1993, 1994 Aladdin Enterprises.  All rights reserved.
  
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

/* gp_win32.c */
/* Common platform-specific routines for MS-Windows WIN32 */
/* hacked from gp_msdos.c by Russell Lang */
#include "stdio_.h"
#include "string_.h"		/* for strerror */
#include "dos_.h"
#include "gstypes.h"
#include "gsmemory.h"		/* for gp.h */
#include "gserror.h"
#include "gserrors.h"
#include "gp.h"
#include "gpsync.h"
#include "windows_.h"

/* ------ Miscellaneous ------ */

/* Get the string corresponding to an OS error number. */
/* This is compiler-, not OS-, specific, but it is ANSI-standard and */
/* all MS-DOS and MS Windows compilers support it. */
const char *
gp_strerror(int errnum)
{	return strerror(errnum);
}

/* ------ Date and time ------ */

/* Read the current time (in seconds since Jan. 1, 1980) */
/* and fraction (in nanoseconds). */
void
gp_get_realtime(long *pdt)
{
SYSTEMTIME st;
long idate;
static const int mstart[12] =
   { 0, 31, 59, 90, 120, 151, 181, 212, 243, 273, 304, 334 };
	/* This gets UTC, not local time */
	/* We have no way of knowing the time zone correction */
	GetSystemTime(&st);
	idate = (st.wYear - 1980) * 365 +	/* days per year */
	  	((st.wYear - 1)/4 - 1979/4) +	/* intervening leap days */
		(1979/100 - (st.wYear - 1)/100) +
		((st.wYear - 1)/400 - 1979/400) +
		mstart[st.wMonth - 1] +		/* month is 1-origin */
		st.wDay - 1;			/* day of month is 1-origin */
	idate += (2 < st.wMonth
		  && (st.wYear % 4 == 0
		      && (st.wYear % 100 != 0 || st.wYear % 400 == 0)));
	pdt[0] = ((idate*24 + st.wHour) * 60 + st.wMinute) * 60 + st.wSecond;
	pdt[1] = st.wMilliseconds * 1000000;
}

/* Read the current user CPU time (in seconds) */
/* and fraction (in nanoseconds).  */
void
gp_get_usertime(long *pdt)
{	gp_get_realtime(pdt);	/* Use an approximation for now.  */
}

/* ------ Console management ------ */

/* Answer whether a given file is the console (input or output). */
/* This is not a standard gp procedure, */
/* but the MS Windows configuration needs it, */
/* and other MS-DOS configurations might need it someday. */
int
gp_file_is_console(FILE *f)
{
#ifdef __DLL__
	if ( f == NULL )
		return 1;
#else
	if ( f == NULL )
		return 0;
#endif
	if (fileno(f) <= 2)
		return 1;
	return 0;
}

/* ------ Screen management ------ */

/* Get the environment variable that specifies the display to use. */
const char *
gp_getenv_display(void)
{	return NULL;
}

/* ------ File names ------ */

/* Define the default scratch file name prefix. */
const char gp_scratch_file_name_prefix[] = "_temp_";

/* Define the name of the null output file. */
const char gp_null_file_name[] = "nul";

/* Define the name that designates the current directory. */
const char gp_current_directory_name[] = ".";

/* ------- Synchronization primitives -------- */

/* Semaphore supports wait/signal semantics */

typedef struct win32_semaphore_s {
  HANDLE       handle;          /* returned from CreateSemaphore */
} win32_semaphore;
const uint gp_semaphore_sizeof = sizeof (win32_semaphore);

int       /* if sema <> 0 rets -ve error, 0 ok; if sema == 0, 0 movable, 1 fixed */
gp_semaphore_open(
  gp_semaphore      *sema       /* create semaphore here */
)	{
	win32_semaphore *const winSema = (win32_semaphore *)sema;
	if (winSema)
	  {
	  winSema->handle = CreateSemaphore(NULL, 0, max_int, NULL);
	  return winSema->handle != NULL ? 0 : gs_note_error(gs_error_unknownerror);
	  }
   else
	  return 0;    /* Win32 semaphores handles may be moved */
	}

void
gp_semaphore_close(
  gp_semaphore      *sema       /* semaphore to affect */
)	{
	win32_semaphore *const winSema = (win32_semaphore *)sema;
	if (winSema->handle != NULL)
	  CloseHandle(winSema->handle);
	winSema->handle = NULL;
	}

int        /* rets 0 ok, -ve error */
gp_semaphore_wait(
  gp_semaphore      *sema       /* semaphore to affect */
)	{
	win32_semaphore *const winSema = (win32_semaphore *)sema;
	return WaitForSingleObject(winSema->handle, INFINITE) == WAIT_OBJECT_0
	 ? 0 : gs_error_unknownerror;
	}

int        /* rets 0 ok, -ve error */
gp_semaphore_signal(
  gp_semaphore      *sema       /* semaphore to affect */
)	{
	win32_semaphore *const winSema = (win32_semaphore *)sema;
	return
	 ReleaseSemaphore(winSema->handle, 1, NULL) ? 0 : gs_error_unknownerror;
	}


/* Monitor supports enter/leave semantics */

typedef struct win32_monitor_s {
  CRITICAL_SECTION   lock;      /* critical section lock */
} win32_monitor;
const uint gp_monitor_sizeof = sizeof (win32_monitor);

int       /* if sema <> 0 rets -ve error, 0 ok; if sema == 0, 0 movable, 1 fixed */
gp_monitor_open(
  gp_monitor      *mon       /* create monitor here */
)	{
	win32_monitor *const winMon = (win32_monitor *)mon;
	if (mon)
	  {
	  InitializeCriticalSection(&winMon->lock);   /* returns no status */
	  return 0;
	  }
	else
	  return 1;    /* Win32 critical sections mutsn't be moved */
	}

void
gp_monitor_close(
  gp_monitor      *mon       /* monitor to affect */
)	{
	win32_monitor *const winMon = (win32_monitor *)mon;
	DeleteCriticalSection(&winMon->lock);    /* rets no status */
	}

int        /* rets 0 ok, -ve error */
gp_monitor_enter(
  gp_monitor      *mon       /* monitor to affect */
)	{
	win32_monitor *const winMon = (win32_monitor *)mon;
	EnterCriticalSection(&winMon->lock);   /* rets no status */
	return 0;
	}

int        /* rets 0 ok, -ve error */
gp_monitor_leave(
  gp_monitor      *mon       /* monitor to affect */
)	{
	win32_monitor *const winMon = (win32_monitor *)mon;
	LeaveCriticalSection(&winMon->lock);   /* rets no status */
	return 0;
	}

/* --------- Thread primitives ---------- */

typedef struct gp_thread_creation_closure_s {
	gp_thread_creation_callback_t	function;	/* function to start */
	void		*data;		/* magic data to pass to thread */
} gp_thread_creation_closure;

/* Origin of new threads started by gp_create_thread */
private DWORD WINAPI
gp_thread_begin_wrapper(
	void		*thread_data		/* gp_thread_creation_closure passed as magic data */
)	{
	gp_thread_creation_closure closure = *(gp_thread_creation_closure *)thread_data;
	free(thread_data);
	(*closure.function)(closure.data);
	return 0;
	}

/* Call a function on a brand new thread */
int	/* 0 ok, -ve error */
gp_create_thread(
	gp_thread_creation_callback_t	function,	/* function to start */
	void		*data		/* magic data to pass to thread fn */
)	{
	DWORD threadID;

	/* Create the magic closure that thread_wrapper gets passed */
	gp_thread_creation_closure *closure
	 = (gp_thread_creation_closure *)malloc( sizeof(*closure) );
	if (!closure)
	  return gs_error_VMerror;
	closure->function = function;
	closure->data = data;

	/* Start thread_wrapper */
	return CreateThread(NULL, 0, gp_thread_begin_wrapper, closure, 0, &threadID)
	 ? 0 : gs_note_error(gs_error_unknownerror);
	}
