/* Copyright (C) 1996, 1999 Aladdin Enterprises.  All rights reserved.

   This software is licensed to a single customer by Artifex Software Inc.
   under the terms of a specific OEM agreement.
 */

/*$RCSfile$ $Revision$ */
/* argv/argc interface to imainarg.c */

#ifndef imainarg_INCLUDED
#  define imainarg_INCLUDED

/* Define an opaque type for an interpreter instance.  See imain.h. */
#ifndef gs_main_instance_DEFINED
#  define gs_main_instance_DEFINED
typedef struct gs_main_instance_s gs_main_instance;
#endif

/*
 * As a shortcut for very high-level clients, we define a single call
 * that does the equivalent of command line invocation, passing argc
 * and argv.  This call includes calling init0 through init2.
 * argv should really be const char *[], but ANSI C requires writable
 * strings (which, however, it forbids the callee to modify!).
 */
int gs_main_init_with_args(P3(gs_main_instance * minst, int argc,
			      char *argv[]));

/*
 * Run the 'start' procedure (after processing the command line).
 * Note that in case of error, this procedure exits rather than returning.
 */
void gs_main_run_start(P1(gs_main_instance * minst));

#endif /* imainarg_INCLUDED */
