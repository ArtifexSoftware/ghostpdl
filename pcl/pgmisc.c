/* Copyright (C) 1997 Aladdin Enterprises.  All rights reserved.
   Unauthorized use, copying, and/or distribution prohibited.
 */

/* pgmisc.c */
/* HP-GL/2 miscellaneous support */

#include "pgmand.h"
#include "pgmisc.h"

void
hpgl_set_lost_mode(hpgl_state_t *pgls, hpgl_lost_mode_t lost_mode)
{
	if ( lost_mode == hpgl_lost_mode_entered )
	  {
#ifdef INFINITE_LOOP
	    hpgl_args_t args;
	    /* raise the pen.  Note this should handle the pcl oddity
               that when lost mode is cleared with an absolute PD we
               draw a line from the last valid position to the first
               args of pen down.  The following appends a moveto the
               current point in the gs path */
	    hpgl_args_setup(&args);
	    hpgl_PU(&args, pgls);
#endif
#ifdef DEBUG
	    dprintf("entering lost mode\n");
#endif
	  }
	pgls->g.lost_mode = lost_mode;

}

#ifdef DEBUG

/* Print an error message.  Note that function may be NULL. */
/* This procedure must return its 'code' argument: see pgmisc.h. */
int
hpgl_print_error(const char *function, const char *file, int line, int code)
{
	dprintf4("hpgl call failed\n\tcalled from: %s\n\tfile: %s\n\tline: %d\n\terror code: %d\n",
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
