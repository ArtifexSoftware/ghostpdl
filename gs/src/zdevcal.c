/* Copyright (C) 1995 Aladdin Enterprises.  All rights reserved.
  
  This file is part of Aladdin Ghostscript.
  
  Aladdin Ghostscript is distributed with NO WARRANTY OF ANY KIND.  No author
  or distributor accepts any responsibility for the consequences of using it,
  or for whether it serves any particular purpose or works at all, unless he
  or she says so in writing.  Refer to the Aladdin Ghostscript Free Public
  License (the "License") for full details.
  
  Every copy of Aladdin Ghostscript must include a copy of the License,
  normally in a plain ASCII text file named PUBLIC.  The License grants you
  the right to copy, modify and redistribute Aladdin Ghostscript, but only
  under certain conditions described in the License.  Among other things, the
  License requires that the copyright notice and this notice be preserved on
  all copies.
*/

/* zdevcal.c */
/* %Calendar% IODevice */
#include "time_.h"
#include "ghost.h"
#include "gxiodev.h"
#include "istack.h"
#include "iparam.h"

/* ------ %Calendar% ------ */

private iodev_proc_get_params(calendar_get_params);
gx_io_device gs_iodev_calendar =
  { "%Calendar%", "Special",
     { iodev_no_init, iodev_no_open_device, iodev_no_open_file,
       iodev_no_fopen, iodev_no_fclose,
       iodev_no_delete_file, iodev_no_rename_file, iodev_no_file_status,
       iodev_no_enumerate_files, NULL, NULL,
       calendar_get_params, iodev_no_put_params
     }
  };

/* Get the date and time. */
private int
calendar_get_params(gx_io_device *iodev, gs_param_list *plist)
{	int code;
	time_t t;
	struct tm *pltime;
	struct tm ltime;
	bool running;

	if ( time(&t) == -1 || (pltime = localtime(&t)) == 0 )
	  { ltime.tm_sec = ltime.tm_min = ltime.tm_hour =
	      ltime.tm_mday = ltime.tm_mon = ltime.tm_year = 0;
	    running = false;
	  }
	else
	  { ltime = *pltime;
	    ltime.tm_year += 1900;
	    ltime.tm_mon++;		/* 1-origin */
	    running = true;
	  }
	if ( (code = param_write_int(plist, "Year", &ltime.tm_year)) < 0 ||
	     (code = param_write_int(plist, "Month", &ltime.tm_mon)) < 0 ||
	     (code = param_write_int(plist, "Day", &ltime.tm_mday)) < 0 ||
	     (code = param_write_int(plist, "Weekday", &ltime.tm_wday)) < 0 ||
	     (code = param_write_int(plist, "Hour", &ltime.tm_hour)) < 0 ||
	     (code = param_write_int(plist, "Minute", &ltime.tm_min)) < 0 ||
	     (code = param_write_int(plist, "Second", &ltime.tm_sec)) < 0
	   )
	  return code;
	return param_write_bool(plist, "Running", &running);
}
