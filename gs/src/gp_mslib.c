/* Copyright (C) 1997 Aladdin Enterprises.  All rights reserved.

   This software is licensed to a single customer by Artifex Software Inc.
   under the terms of a specific OEM agreement.
 */

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
