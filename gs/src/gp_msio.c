/* Copyright (C) 1992, 1995, 1996, 1997 Aladdin Enterprises.  All rights reserved.
  
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

/* gp_msio.c */
/*
 * Streams for Windows text window
 * Original version by Russell Lang and Maurice Castro with help from
 * Programming Windows, 2nd Ed., Charles Petzold, Microsoft Press;
 * initially created from gp_dosfb.c and gp_itbc.c 5th June 1992.
 */

/* Modified for Win32 & Microsoft C/C++ 8.0 32-Bit, 26.Okt.1994 */
/* by Friedrich Nowak                                           */

/* Factored out from gp_mswin.c by JD 6/25/97 */

#include "stdio_.h"
#include <stdlib.h>
#include "gx.h"
#include "gp.h"
#include "windows_.h"
#include <shellapi.h>
#ifdef __WIN32__
#include <winspool.h>
#endif
#include "gp_mswin.h"
#include "gsdll.h"

#include "stream.h"
#include "gxiodev.h"			/* must come after stream.h */

/* Imported from gp_msdos.c */
int gp_file_is_console(P1(FILE *));


/* ====== Substitute for stdio ====== */

/* Forward references */
private void win_std_init(void);
private stream_proc_process(win_std_read_process);
private stream_proc_process(win_std_write_process);

/* Use a pseudo IODevice to get win_stdio_init called at the right time. */
/* This is bad architecture; we'll fix it later. */
private iodev_proc_init(win_stdio_init);
gx_io_device gs_iodev_wstdio = {
	"wstdio", "Special",
	{ win_stdio_init, iodev_no_open_device,
	  iodev_no_open_file, iodev_no_fopen, iodev_no_fclose,
	  iodev_no_delete_file, iodev_no_rename_file,
	  iodev_no_file_status, iodev_no_enumerate_files
	}
};

/* Do one-time initialization */
private int
win_stdio_init(gx_io_device *iodev, gs_memory_t *mem)
{
	win_std_init();		/* redefine stdin/out/err to our window routines */
	return 0;
}

/* Define alternate 'open' routines for our stdin/out/err streams. */

extern int iodev_stdin_open(P4(gx_io_device *, const char *, stream **,
			       gs_memory_t *));
private int
win_stdin_open(gx_io_device *iodev, const char *access, stream **ps,
  gs_memory_t *mem)
{	int code = iodev_stdin_open(iodev, access, ps, mem);
	stream *s = *ps;
	if ( code != 1 )
	  return code;
	s->procs.process = win_std_read_process;
	s->file = NULL;
	return 0;
}

extern int iodev_stdout_open(P4(gx_io_device *, const char *, stream **,
				gs_memory_t *));
private int
win_stdout_open(gx_io_device *iodev, const char *access, stream **ps,
  gs_memory_t *mem)
{	int code = iodev_stdout_open(iodev, access, ps, mem);
	stream *s = *ps;
	if ( code != 1 )
	  return code;
	s->procs.process = win_std_write_process;
	s->file = NULL;
	return 0;
}

extern int iodev_stderr_open(P4(gx_io_device *, const char *, stream **,
				gs_memory_t *));
private int
win_stderr_open(gx_io_device *iodev, const char *access, stream **ps,
  gs_memory_t *mem)
{	int code = iodev_stderr_open(iodev, access, ps, mem);
	stream *s = *ps;
	if ( code != 1 )
	  return code;
	s->procs.process = win_std_write_process;
	s->file = NULL;
	return 0;
}

/* Patch stdin/out/err to use our windows. */
private void
win_std_init(void)
{
	/* If stdxxx is the console, replace the 'open' routines, */
	/* which haven't gotten called yet. */

	if ( gp_file_is_console(gs_stdin) )
	  gs_findiodevice((const byte *)"%stdin", 6)->procs.open_device =
	    win_stdin_open;

	if ( gp_file_is_console(gs_stdout) )
	  gs_findiodevice((const byte *)"%stdout", 7)->procs.open_device =
	    win_stdout_open;

	if ( gp_file_is_console(gs_stderr) )
	  gs_findiodevice((const byte *)"%stderr", 7)->procs.open_device =
	    win_stderr_open;
}

private int
win_std_read_process(stream_state *st, stream_cursor_read *ignore_pr,
  stream_cursor_write *pw, bool last)
{
	int count = pw->limit - pw->ptr;

	if ( count == 0 ) 		/* empty buffer */
		return 1;

/* callback to get more input */
	count = (*pgsdll_callback)(GSDLL_STDIN, pw->ptr+1, count);
	if (count == 0) {
	    /* EOF */
	    /* what should we do? */
	    return EOFC;
	}

	pw->ptr += count;
	return 1;
}


private int
win_std_write_process(stream_state *st, stream_cursor_read *pr,
  stream_cursor_write *ignore_pw, bool last)
{	uint count = pr->limit - pr->ptr;
	(*pgsdll_callback)(GSDLL_STDOUT, (char *)(pr->ptr +1), count);
	pr->ptr = pr->limit;
	return 0;
}

/* This is used instead of the stdio version. */
/* The declaration must be identical to that in <stdio.h>. */
#if defined(_WIN32) && (defined(_MSC_VER) || defined(_WATCOM_))
#if defined(_CRTAPI2)
int _CRTAPI2
fprintf(FILE *file, const char *fmt, ...)
#else
_CRTIMP int __cdecl
fprintf(FILE *file, const char *fmt, ...)
#endif
#else
int _Cdecl _FARFUNC
fprintf(FILE _FAR *file, const char *fmt, ...)
#endif
{
int count;
va_list args;
	va_start(args,fmt);
	if ( gp_file_is_console(file) ) {
		char buf[1024];
		count = vsprintf(buf,fmt,args);
		(*pgsdll_callback)(GSDLL_STDOUT, buf, count);
	}
	else
		count = vfprintf(file, fmt, args);
	va_end(args);
	return count;
}
