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

#include "gx.h"

int
gp_check_interrupts(const gs_memory_t *mem)
{
    return 0;
}
#endif
