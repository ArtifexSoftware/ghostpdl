/* Copyright (C) 1996, 1997 Aladdin Enterprises.  All rights reserved.
   Unauthorized use, copying, and/or distribution prohibited.
 */

/* rtcursor.c */
/* HP RTL cursor positioning commands */
#include "std.h"
#include "pcommand.h"
#include "pcstate.h"
#include "pcdraw.h"

/* Commands */

/* The HP RTL versions of these commands don't do any clamping. */

int /* ESC & a <dp> H */
rtl_horiz_cursor_pos_decipoints(pcl_args_t *pargs, pcl_state_t *pcls)
{	float x = float_arg(pargs) * 10;		/* centipoints */
	pcl_cap.x = (coord)(arg_is_signed(pargs) ? pcl_cap.x + x : x);
	return 0;
}

int /* ESC * p <units> X */
rtl_horiz_cursor_pos_units(pcl_args_t *pargs, pcl_state_t *pcls)
{	float x = float_arg(pargs) * pcls->uom_cp;	/* centipoints */
	pcl_cap.x = (coord)(arg_is_signed(pargs) ? pcl_cap.x + x : x);
	return 0;
}

int /* ESC * p <units> Y */
rtl_vert_cursor_pos_units(pcl_args_t *pargs, pcl_state_t *pcls)
{	coord y = (coord)(float_arg(pargs) * pcls->uom_cp);  /* centipoints */

	pcl_cap.y =
	  ((arg_is_signed(pargs) ? pcl_cap.y : pcls->margins.top) + y);
	return 0;
}

/* Initialization */
private int
rtcursor_do_init(gs_memory_t *mem)
{		/* Register commands */
	DEFINE_CLASS('&')
	  {'a', 'H',
	     PCL_COMMAND("Horizontal Cursor Position Decipoints",
			 rtl_horiz_cursor_pos_decipoints,
			 pca_neg_ok|pca_big_ok)},
	END_CLASS
	DEFINE_CLASS('*')
	  {'p', 'X',
	     PCL_COMMAND("Horizontal Cursor Position Units",
			 rtl_horiz_cursor_pos_units,
			 pca_neg_ok|pca_big_ok)},
	  {'p', 'Y',
	     PCL_COMMAND("Vertical Cursor Position Units",
			 rtl_vert_cursor_pos_units,
			 pca_neg_ok|pca_big_ok)},
	END_CLASS
	return 0;
}
private void
rtcursor_do_reset(pcl_state_t *pcls, pcl_reset_type_t type)
{	if ( type & pcl_reset_initial )
	  { pcl_cap.x = 0;
	    pcl_cap.y = 0;
	  }
}

const pcl_init_t rtcursor_init = {
  rtcursor_do_init, rtcursor_do_reset, 0
};
