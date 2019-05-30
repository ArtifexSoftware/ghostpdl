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


/* argv/argc interface to imainarg.c */

#ifndef imainarg_INCLUDED
#  define imainarg_INCLUDED

#include "std.h"
#include "imain.h"

/*
 * As a shortcut for very high-level clients, we define a single call
 * that does the equivalent of command line invocation, passing argc
 * and argv.  This call includes calling init0 through init2.
 * argv should really be const char *[], but ANSI C requires writable
 * strings (which, however, it forbids the callee to modify!).
 */
int gs_main_init_with_args(gs_main_instance * minst, int argc, char *argv[]);

/*
 * Split init functions; gs_main_init_with_args01 and 2 together do
 * the same as gs_main_init_with_args, but allow for additional
 * options to be processed in between. */
int gs_main_init_with_args01(gs_main_instance * minst, int argc, char *argv[]);
int gs_main_init_with_args2(gs_main_instance * minst);

/*
 * Run the 'start' procedure (after processing the command line).
 */
int gs_main_run_start(gs_main_instance * minst);

/*
 * By default gs assumes that all args are in utf8 format. If not, the caller
 * should call this function to specify a decoding function in advance.
 * Allocation should be done with the standard gs allocation functions, as
 * the returned string may be freed using the same.
 */
typedef int (gs_arg_get_codepoint)(gp_file *, const char **);

void gs_main_inst_arg_decode(gs_main_instance * minst,
                             gs_arg_get_codepoint *get_codepoint);

gs_arg_get_codepoint *gs_main_inst_get_arg_decode(gs_main_instance * minst);

#endif /* imainarg_INCLUDED */
