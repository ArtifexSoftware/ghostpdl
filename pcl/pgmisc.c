/* Copyright (C) 1997 Aladdin Enterprises.  All rights reserved.
   Unauthorized use, copying, and/or distribution prohibited.
 */

/* pgmisc.c */
/* HP-GL/2 lost mode routine (currently ) */

#include "pgmand.h"
#include "pgmisc.h"

void
hpgl_set_lost_mode(hpgl_state_t *pgls, hpgl_lost_mode_t lost_mode)
{
	if ( lost_mode == hpgl_lost_mode_entered ) 
	  {
	    hpgl_args_t args;
	    /* raise the pen.  Note this should handle the pcl oddity
               that when lost mode is cleared with an absolute PD we
               draw a line from the last valid position to the first
               args of pen down. */
	    hpgl_args_setup(&args);
	    hpgl_PU(&args, pgls);
#ifdef DEBUG
	    dprintf("entering lost mode\n");
#endif
	  }
	pgls->g.lost_mode = lost_mode;

}

/* called when there is a graphics error.  Keep a breakpoint on this function */
#ifdef DEBUG
void
hpgl_error()
{
	return;
}
#endif
