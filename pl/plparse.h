/* Portions Copyright (C) 2001 artofcode LLC.
   Portions Copyright (C) 1996, 2001 Artifex Software Inc.
   Portions Copyright (C) 1988, 2000 Aladdin Enterprises.
   This software is based in part on the work of the Independent JPEG Group.
   All Rights Reserved.

   This software is distributed under license and may not be copied, modified
   or distributed except as expressly authorized under the terms of that
   license.  Refer to licensing information at http://www.artifex.com/ or
   contact Artifex Software, Inc., 101 Lucas Valley Road #110,
   San Rafael, CA  94903, (415)492-9861, for further information. */
/*$Id$ */

/* plparse.h */
/* Interface to all (H-P) language parsers */

#ifndef plparse_INCLUDED
#  define plparse_INCLUDED

#include "scommon.h"

/*
 * H-P printers can switch languages more or less ad lib.  Currently,
 * we have to be able to handle 4 languages: PJL, PCL5*, HP-GL/2,
 * and PCL XL.  This file provides a tiny bit of structure common to
 * all languages and parsers, for the use of front-end programs that
 * don't want to worry about such things.
 *
 * At any given moment, the interpreter's state consists of:
 *	- Job parameters set by some mix of PJL and the current language;
 *	- User defaults for the current language;
 *	- Parsing state for the current language;
 *	- Parameters, graphics state, and downloaded data for the current
 *	language.
 * Currently we don't try to impose much structure on this, only enough
 * to make it possible to write the language switching code.
 */
/*
 * Define an abstract type for the switchable input processor state.
 */
typedef struct pl_process_state_s pl_process_state_t;
/*
 * Define the type for a parser.  The parser reads data from the input
 * buffer and returns either:
 *	>=0 - OK, more input is needed.
 *	e_ExitLanguage - A UEL or other return to the default parser was
 *	detected.
 *	other <0 value - an error was detected.
 */
#define e_ExitLanguage (-102)	/* e_InterpreterExit */
#define pl_process_proc(proc)\
  int proc(pl_process_state_t *process_data, stream_cursor_read *pr)
typedef pl_process_proc((*pl_process_proc_t));
/*
 * Define the input processor state.  There's hardly anything here....
 */
struct pl_process_state_s {
  pl_process_proc_t process;
  void *process_data;	/* closure data for process procedure */
};

#endif					/* plparse_INCLUDED */
