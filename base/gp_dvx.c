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


/* Desqview/X-specific routines for Ghostscript */
#include "string_.h"
#include "gx.h"
#include "gsexit.h"
#include "gp.h"
#include "time_.h"

/* Do platform-dependent initialization. */
void
gp_init(void)
{
}

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
/* All reasonable compilers support it. */
const char *
gp_strerror(int errnum)
{
    return strerror(errnum);
}

/* We don't have a good way to get a serial number here, so just */
/* return what we always used to: GS_SERIALNUMBER. */
int
gp_serialnumber(void)
{
    return (int)(gs_serialnumber);
}

/* ------ Date and time ------ */

/* Read the current time (in seconds since Jan. 1, 1970) */
/* and fraction (in nanoseconds). */
void
gp_get_realtime(long *pdt)
{
    struct timeval tp;
    struct timezone tzp;

    if (gettimeofday(&tp, &tzp) == -1) {
        lprintf("Ghostscript: gettimeofday failed!\n");
        tp.tv_sec = tp.tv_usec = 0;
    }
    /* tp.tv_sec is #secs since Jan 1, 1970 */
    pdt[0] = tp.tv_sec;
    pdt[1] = tp.tv_usec * 1000;

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
    gp_get_realtime(pdt);	/* Use an approximation for now.  */
}

/* ------ Printer accessing ------ */

static int
dvx_prn_close(FILE *)
{
    fflush(stdprn);
}

/* Open a connection to a printer.  A null file name means use the */
/* standard printer connected to the machine, if any. */
/* Return NULL if the connection could not be opened. */
extern void gp_set_file_binary(int, int);
gp_file *
gp_open_printer_impl(gs_memory_t *mem,
                     const char  *fname,
                     int         *binary_mode,
                     int         (**close)(FILE *))
{
    if (strlen(fname) == 0 || !strcmp(fname, "PRN")) {
        stdprn->_flag = _IOWRT;	/* Make stdprn buffered to improve performance */
        return stdprn;
    } else
        return gp_fopen_impl(mem, fname, (*binary_mode ? "wb" : "w"));
}

/* Close the connection to the printer. */
void
gp_close_printer(gp_file * pfile, const char *fname)
{
    pfile->close(pfile);
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
