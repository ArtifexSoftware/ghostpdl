/* Copyright (C) 1996 Aladdin Enterprises.  All rights reserved.
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
	pcls->cap.x = (coord)(arg_is_signed(pargs) ? pcls->cap.x + x : x);
	return 0;
}

int /* ESC * p <units> X */
rtl_horiz_cursor_pos_units(pcl_args_t *pargs, pcl_state_t *pcls)
{	float x = float_arg(pargs) * pcls->uom_cp;	/* centipoints */
	pcls->cap.x = (coord)(arg_is_signed(pargs) ? pcls->cap.x + x : x);
	return 0;
}

int /* ESC * p <units> Y */
rtl_vert_cursor_pos_units(pcl_args_t *pargs, pcl_state_t *pcls)
{	float y = float_arg(pargs) * pcls->uom_cp;	/* centipoints */
	pcls->cap.y =
	  (coord)((arg_is_signed(pargs) ? pcls->cap.y : pcls->top_margin) + y);
	return 0;
}

/* Initialization */
private int
rtcursor_do_init(gs_memory_t *mem)
{		/* Register commands */
	DEFINE_CLASS('&')
	  {'a', 'H', {rtl_horiz_cursor_pos_decipoints, pca_neg_ok|pca_big_ok}},
	END_CLASS
	DEFINE_CLASS('*')
	  {'p', 'X', {rtl_horiz_cursor_pos_units, pca_neg_ok|pca_big_ok}},
	  {'p', 'Y', {rtl_vert_cursor_pos_units, pca_neg_ok|pca_big_ok}},
	END_CLASS
	return 0;
}
private void
rtcursor_do_reset(pcl_state_t *pcls, pcl_reset_type_t type)
{	if ( type & pcl_reset_initial )
	  { pcls->cap.x = 0;
	    pcls->cap.y = 0;
	  }
}
private int
rtcursor_do_copy(pcl_state_t *psaved, const pcl_state_t *pcls,
  pcl_copy_operation_t operation)
{	if ( operation & pcl_copy_after )
	  { /* Don't restore the cursor position or stack. */
	    psaved->cap = pcls->cap;
	    psaved->cursor_stack = pcls->cursor_stack;
	  }
	return 0;
}
const pcl_init_t rtcursor_init = {
  rtcursor_do_init, rtcursor_do_reset, rtcursor_do_copy
};
