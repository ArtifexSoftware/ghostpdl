/* Copyright (C) 2001-2006 Artifex Software, Inc.
   All Rights Reserved.
  
   This software is provided AS-IS with no warranty, either express or
   implied.

   This software is distributed under license and may not be copied, modified
   or distributed except as expressly authorized under the terms of that
   license.  Refer to licensing information at http://www.artifex.com/
   or contact Artifex Software, Inc.,  7 Mt. Lassen Drive - Suite A-134,
   San Rafael, CA  94903, U.S.A., +1(415)492-9861, for further information.
*/

/* $Id$ */
/* Unix-specific routines for Ghostscript */

#include "pipe_.h"
#include "string_.h"
#include "time_.h"
#include "gx.h"
#include "gsexit.h"
#include "gp.h"

/*
 * This is the only place in Ghostscript that calls 'exit'.  Including
 * <stdlib.h> is overkill, but that's where it's declared on ANSI systems.
 * We don't have any way of detecting whether we have a standard library
 * (some GNU compilers perversely define __STDC__ but don't provide
 * an ANSI-compliant library), so we check __PROTOTYPES__ and
 * hope for the best.  We pick up getenv at the same time.
 */
#ifdef __PROTOTYPES__
#  include <stdlib.h>		/* for exit and getenv */
#else
extern void exit(int);
extern char *getenv(const char *);
#endif

#ifdef GS_DEVS_SHARED
#ifndef GS_DEVS_SHARED_DIR
#  define GS_DEVS_SHARED_DIR "/usr/lib/ghostscript/8.16"
#endif
/*
 * use shared library for drivers, always load them when starting, this
 * avoid too many modifications, and since it is supported only under linux
 * and applied as a patch (preferable).
 */
#include <sys/types.h>
#include <dirent.h>
#include <dlfcn.h>
#include <string.h>

void
gp_init(void)
{
  DIR*           dir = NULL;
  struct dirent* dirent;
  char           buff[1024];
  char*          pbuff;
  void*          handle;
  void           (*gs_shared_init)(void);

  strncpy(buff, GS_DEVS_SHARED_DIR, sizeof(buff) - 2);
  pbuff = buff + strlen(buff);
  *pbuff++ = '/'; *pbuff = '\0';

  dir = opendir(GS_DEVS_SHARED_DIR);
  if (dir == 0) return;

  while ((dirent = readdir(dir)) != 0) {
    strncpy(pbuff, dirent->d_name, sizeof(buff) - (pbuff - buff) - 1);
    if ((handle = dlopen(buff, RTLD_NOW)) != 0) {
      if ((gs_shared_init = dlsym(handle, "gs_shared_init")) != 0) {
	(*gs_shared_init)();
      } else {
      }
    }
  }

  closedir(dir);
}
#else
/* Do platform-dependent initialization. */
void
gp_init(void)
{
}
#endif

/* Do platform-dependent cleanup. */
void
gp_exit(int exit_status, int code)
{
}

/* Exit the program. */
void
gp_do_exit(int exit_status)
{
    exit(exit_status);
}

/* ------ Miscellaneous ------ */

/* Get the string corresponding to an OS error number. */
/* Unix systems support this so inconsistently that we don't attempt */
/* to figure out whether it's available. */
const char *
gp_strerror(int errnum)
{
    return NULL;
}

/* read in a MacOS 'resource' from an extended attribute. */
/* we don't try to implemented this since it requires support */
/* for Apple's HFS(+) filesystem */
int
gp_read_macresource(byte *buf, const char *filename, 
                    const uint type, const ushort id)
{
    return 0;
}

/* ------ Date and time ------ */

/* Read the current time (in seconds since Jan. 1, 1970) */
/* and fraction (in nanoseconds). */
void
gp_get_realtime(long *pdt)
{
    struct timeval tp;

#if gettimeofday_no_timezone	/* older versions of SVR4 */
    {
	if (gettimeofday(&tp) == -1) {
	    lprintf("Ghostscript: gettimeofday failed!\n");
	    tp.tv_sec = tp.tv_usec = 0;
	}
    }
#else /* All other systems */
    {
	struct timezone tzp;

	if (gettimeofday(&tp, &tzp) == -1) {
	    lprintf("Ghostscript: gettimeofday failed!\n");
	    tp.tv_sec = tp.tv_usec = 0;
	}
    }
#endif

    /* tp.tv_sec is #secs since Jan 1, 1970 */
    pdt[0] = tp.tv_sec;

    /* Some Unix systems (e.g., Interactive 3.2 r3.0) return garbage */
    /* in tp.tv_usec.  Try to filter out the worst of it here. */
    pdt[1] = tp.tv_usec >= 0 && tp.tv_usec < 1000000 ? tp.tv_usec * 1000 : 0;

#ifdef DEBUG_CLOCK
    printf("tp.tv_sec = %d  tp.tv_usec = %d  pdt[0] = %ld  pdt[1] = %ld\n",
	   tp.tv_sec, tp.tv_usec, pdt[0], pdt[1]);
#endif
}

/* Read the current user CPU time (in seconds) */
/* and fraction (in nanoseconds).  */
void
gp_get_usertime(long *pdt)
{
#if use_times_for_usertime
    struct tms tms;
    long ticks;
    const long ticks_per_sec = CLK_TCK;

    times(&tms);
    ticks = tms.tms_utime + tms.tms_stime + tms.tms_cutime + tms.tms_cstime;
    pdt[0] = ticks / ticks_per_sec;
    pdt[1] = (ticks % ticks_per_sec) * (1000000000 / ticks_per_sec);
#else
    gp_get_realtime(pdt);	/* Use an approximation on other hosts.  */
#endif
}

/* ------ Screen management ------ */

/* Get the environment variable that specifies the display to use. */
const char *
gp_getenv_display(void)
{
    return getenv("DISPLAY");
}

/* ------ Printer accessing ------ */

/* Open a connection to a printer.  See gp.h for details. */
FILE *
gp_open_printer(char fname[gp_file_name_sizeof], int binary_mode)
{
    const char *fmode = (binary_mode ? "wb" : "w");

    return (strlen(fname) == 0 ? 0 : fopen(fname, fmode));
}
FILE *
gp_open_printer_64(char fname[gp_file_name_sizeof], int binary_mode)
{
    const char *fmode = (binary_mode ? "wb" : "w");

    return (strlen(fname) == 0 ? 0 : gp_fopen_64(fname, fmode));
}

/* Close the connection to the printer. */
void
gp_close_printer(FILE * pfile, const char *fname)
{
    if (fname[0] == '|')
	pclose(pfile);
    else
	fclose(pfile);
}

/* ------ Font enumeration ------ */
 
 /* This is used to query the native os for a list of font names and
  * corresponding paths. The general idea is to save the hassle of
  * building a custom fontmap file.
  */
 
void *gp_enumerate_fonts_init(gs_memory_t *mem)
{
    return NULL;
}
         
int gp_enumerate_fonts_next(void *enum_state, char **fontname, char **path)
{
    return 0;
}
                         
void gp_enumerate_fonts_free(void *enum_state)
{
}           
