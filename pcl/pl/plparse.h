/* Copyright (C) 2001-2021 Artifex Software, Inc.
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
#define e_ExitLanguage gs_error_InterpreterExit
#define pl_process_proc(proc)\
  int proc(pl_process_state_t *process_data, stream_cursor_read *pr)
typedef pl_process_proc((*pl_process_proc_t));

/*
 * Define the input processor state.  There's hardly anything here....
 */
struct pl_process_state_s
{
    pl_process_proc_t process;
    void *process_data;         /* closure data for process procedure */
};

#endif /* plparse_INCLUDED */
