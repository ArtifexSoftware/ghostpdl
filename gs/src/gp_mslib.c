/* Portions Copyright (C) 2001 artofcode LLC.
   Portions Copyright (C) 1996, 2001 Artifex Software Inc.
   Portions Copyright (C) 1988, 2000 Aladdin Enterprises.
   This software is based in part on the work of the Independent JPEG Group.
   All Rights Reserved.

/*$RCSfile$ $Revision$ */
/*
 * Microsoft Windows platform support for Graphics Library
 *
 * This file implements functions that differ between the graphics
 * library and the interpreter.
 */

/* ------ Process message loop ------ */
/* 
 * Check messages and interrupts; return true if interrupted.
 * This is called frequently - it must be quick!
 */
#ifdef CHECK_INTERRUPTS
int
gp_check_interrupts(void)
{
    return 0;
}
#endif
