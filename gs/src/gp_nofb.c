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
