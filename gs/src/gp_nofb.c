/* Copyright (C) 1993 Aladdin Enterprises.  All rights reserved.

   This software is licensed to a single customer by Artifex Software Inc.
   under the terms of a specific OEM agreement.
 */

/*$RCSfile$ $Revision$ */
/* Dummy routines for Ghostscript platforms with no frame buffer management */
#include "gx.h"
#include "gp.h"
#include "gxdevice.h"

/* ------ Screen management ------ */

/* Initialize the console. */
void
gp_init_console(void)
{
}

/* Write a string to the console. */
void
gp_console_puts(const char *str, uint size)
{
    fwrite(str, 1, size, stdout);
}

/* Make the console current on the screen. */
int
gp_make_console_current(gx_device * dev)
{
    return 0;
}

/* Make the graphics current on the screen. */
int
gp_make_graphics_current(gx_device * dev)
{
    return 0;
}
