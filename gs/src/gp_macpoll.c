/* Copyright (C) 2001-2003 artofcode LLC.  All rights reserved.
  
   This software is distributed under license and may not be copied, modified
   or distributed except as expressly authorized under the terms of that
   license.  Refer to licensing information at http://www.artifex.com/ or
   contact Artifex Software, Inc., 101 Lucas Valley Road #110,
   San Rafael, CA  94903, (415)492-9861, for further information. */

/* $Id$ */
/*
 * Macintosh platform polling support for Ghostscript.
 *
 */

#ifndef __CARBON__
#include <Timer.h>
#else
#include <Carbon.h>
#endif

#include "gx.h"
#include "gp.h"
#include "gsdll.h"
#include "gpcheck.h"
#include "iapi.h"
#include "iref.h"
#include "iminst.h"
#include "imain.h"

#ifdef CHECK_INTERRUPTS

extern HWND hwndtext;

/* ------ Process message loop ------ */
/* 
 * Check messages and interrupts; return true if interrupted.
 * This is called frequently - it must be quick!
 */
int gp_check_interrupts(void)
{
	static unsigned long	lastYieldTicks = 0;
	int iRetVal = 0;
	
	if ((TickCount() - lastYieldTicks) > 2) {
	    lastYieldTicks = TickCount();
	    if (pgsdll_callback) {
		/* WARNING: The use of the old gsdll interface is deprecated. 
		 * The caller should use the newer gsapi_set_poll.
		 * If the caller needs access to "hwndtext", it should do 
		 * this via caller_handle which is passed to poll_fn.
		 */
		/* the hwnd parameter which is submitted in gsdll_init 
		 * to the DLL is returned in every gsdll_poll message 
		 * in the count parameter
		 */
		iRetVal = (*pgsdll_callback)(GSDLL_POLL, 0, (long) hwndtext);
	    } 
	    else {
		gs_main_instance *minst = gs_main_instance_default();
		if (minst && minst->poll_fn)
		    iRetVal = (*minst->poll_fn)(minst->caller_handle);
	    }
	}
	return iRetVal;
}
#endif
