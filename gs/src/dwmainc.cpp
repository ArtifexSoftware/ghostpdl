/* Copyright (C) 1996-1998, Russell Lang.  All rights reserved.
  
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


// Id: dwmainc.cpp 
// Ghostscript DLL loader for Windows 95/NT
// For WINDOWCOMPAT (console mode) application

#define STRICT
#include <windows.h>
#include <shellapi.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <dos.h>
#include <fcntl.h>
#include <io.h>
extern "C" {
#include "gscdefs.h"
#define GSREVISION gs_revision
#include "gsdll.h"
}
#include "dwmain.h"
#include "dwdll.h"

#if defined(_MSC_VER) && defined(__WIN32__)
#define _export
#endif

/* public handles */
HINSTANCE ghInstance;

const char *szDllName = "GSDLL32.DLL";


int FAR _export gsdll_callback(int message, char FAR *str, unsigned long count);

// the Ghostscript DLL class
gsdll_class gsdll;

char start_string[] = "systemdict /start get exec\n";

// program really starts at WinMain
int
new_main(int argc, char *argv[])
{
typedef char FAR * FARARGV_PTR;
int rc;

    setmode(fileno(stdin), O_BINARY);

    // load DLL
    if (gsdll.load(ghInstance, szDllName, GSREVISION)) {
	char buf[256];
	gsdll.get_last_error(buf, sizeof(buf));
	fputs(buf, stdout);
	return 1;
    }

    // initialize the interpreter
    rc = gsdll.init(gsdll_callback, (HWND)NULL, argc, argv);
    if (rc == GSDLL_INIT_QUIT) {
        gsdll.unload();
	return 0;
    }
    if (rc) {
	char buf[256];
	gsdll.get_last_error(buf, sizeof(buf));
	fputs(buf, stdout);
        gsdll.unload();
	return rc;
    }

    // if (!batch)
    gsdll.execute(start_string, strlen(start_string));
    
    gsdll.unload();

    return 0;
}


#if defined(_MSC_VER)
/* MSVC Console EXE needs main() */
int
main(int argc, char *argv[])
{
    /* need to get instance handle */
    ghInstance = GetModuleHandle(NULL);

    return new_main(argc, argv);
}
#else
/* Borland Console EXE needs WinMain() */
#pragma argsused  // ignore warning about unused arguments in next function

int PASCAL 
WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpszCmdLine, int cmdShow)
{
#if defined(_MSC_VER)    /* MSC doesn't give us _argc and _argv[] so ...   */
#define MAXCMDTOKENS 128
	int	_argc=0;
	LPSTR	_argv[MAXCMDTOKENS];
	LPSTR	p, q;
#ifdef __WIN32__
	_argv[_argc] = "gswin32.exe";
#else
	_argv[_argc] = "gswin.exe";
#endif
	// Parse command line handling quotes.
	p = lpszCmdLine;
	while (*p) {
	    // for each argument
	    while ((*p) && (*p == ' '))
		p++;	// skip over leading spaces
	    if (*p == '\042') {
	       p++;		// skip "
	       q = p;
	       // scan to end of argument
	       // doesn't handle embedded quotes
	       while ((*p) && (*p != '\042'))
		    p++;
	       _argv[++_argc] = q;
	       if (*p)
		    *p++ = '\0';
	    }
	    else if (*p) {
	       // delimited by spaces
	       q = p;
	       while ((*p) && (*p != ' '))
		    p++;
	       _argv[++_argc] = q;
	       if (*p)
		    *p++ = '\0';
	    }
	}
	_argv[++_argc] = (LPSTR)NULL;
#endif

	if (hPrevInstance) {
	    fputs("Can't run twice", stdout);
	    return FALSE;
	}

	/* copy the hInstance into a variable so it can be used */
	ghInstance = hInstance;

	return new_main(_argc, _argv);
}
#endif


int
read_stdin(char FAR *str, int len)
{
int ch;
int count = 0;
    while (count < len) {
	ch = fgetc(stdin);
	if (ch == EOF)
	    return count;
	*str++ = ch;
	count++;
	if (ch == '\n')
	    return count;
    }
    return count;
}


int FAR _export
gsdll_callback(int message, char FAR *str, unsigned long count)
{
char buf[256];
    switch (message) {
	case GSDLL_POLL:
	    // Don't check message queue because we don't
	    // create any windows.
	    // May want to return error code if abort wanted
	    break;
	case GSDLL_STDIN:
	    return read_stdin(str, count);
	case GSDLL_STDOUT:
	    fwrite(str, 1, count, stdout);
	    fflush(stdout);
	    return count;
	case GSDLL_DEVICE:
	    if (count) {
		fputs("\n\
The mswindll device is not supported by the command line version of\n\
Ghostscript.  Select a different device using -sDEVICE= as described\n\
in use.txt.\n", stdout);
	    }
	    break;
	case GSDLL_SYNC:
	    break;
	case GSDLL_PAGE:
	    break;
	case GSDLL_SIZE:
	    break;
	default:
	    sprintf(buf,"Callback: Unknown message=%d\n",message);
	    fputs(buf, stdout);
	    break;
    }
    return 0;
}


