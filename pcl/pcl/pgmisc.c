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


/* pgmisc.c */
/* HP-GL/2 miscellaneous support */

#include "pgmand.h"
#include "pgmisc.h"

bool
current_units_out_of_range(hpgl_real_t x)
{
    const hpgl_real_t max = (1 << 30) - 1;
    const hpgl_real_t min = -(1 << 30);

    /* Not much to do about the floating point equality checking here
       The problem is the coordinates should already be in an exact
       (fixed) representation at this time but they are not so we make
       due with the following. */
    return (x < min || x > max);
}

void
hpgl_set_lost_mode(hpgl_state_t * pgls, hpgl_lost_mode_t lost_mode)
{
    if (lost_mode == hpgl_lost_mode_entered) {
#ifdef INFINITE_LOOP
        hpgl_args_t args;

        /* raise the pen.  Note this should handle the pcl oddity
           that when lost mode is cleared with an absolute PD we
           draw a line from the last valid position to the first
           args of pen down.  The following appends a moveto the
           current point in the gs path */
        hpgl_args_setup(&args);
        hpgl_call(hpgl_PU(&args, pgls));
#endif
#ifdef DEBUG
        dmprintf(pgls->memory, "entering lost mode\n");
#endif
    }
    pgls->g.lost_mode = lost_mode;

}

#ifdef DEBUG

/* Print an error message.  Note that function may be NULL. */
/* This procedure must return its 'code' argument: see pgmisc.h. */
int
hpgl_print_error(const gs_memory_t * mem,
                 const char *function, const char *file, int line, int code)
{
    dmprintf4(mem,
              "hpgl call failed\n\tcalled from: %s\n\tfile: %s\n\tline: %d\n\terror code: %d\n",
              (function == 0 ? "" : function), file, line, code);
    hpgl_error();
    return code;
}

/* called when there is a graphics error.  Keep a breakpoint on this function */
void
hpgl_error()
{
    return;
}
#endif
