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

/*$RCSfile$ $Revision$ */
/*
 * Microsoft Windows 3.n platform support for Graphics Library
 *
 * This file just stubs out a few references from gp_mswin.c, where the
 * real action is. This was done to avoid restructuring Windows support,
 * though that would be the right thing to do.
 *
 * Created 6/25/97 by JD
 */

#include <setjmp.h>

/// Export dummy longjmp environment
jmp_buf gsdll_env;


/// Export dummy interpreter status
int gs_exit_status = 0;


/// Dummy callback routine & export pointer to it
int
gsdll_callback(int a, char *b, unsigned long c)
{
    return 0;
}

int (*pgsdll_callback) () = gsdll_callback;
