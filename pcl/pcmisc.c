/* Copyright (C) 1996 Aladdin Enterprises.  All rights reserved.
   Unauthorized use, copying, and/or distribution prohibited.
 */

/* pcmisc.c */
/* PCL5 miscellaneous and debugging commands */
#include "std.h"
#include "pcommand.h"
#include "pcstate.h"

private int /* ESC & s <bool> C */
pcl_end_of_line_wrap(pcl_args_t *pargs, pcl_state_t *pcls)
{	uint i = uint_arg(pargs);

	if ( i > 1 )
	  return e_Range;
	pcls->end_of_line_wrap = i == 0;
	return 0;
}

private int /* ESC Y */
pcl_enable_display_functions(pcl_args_t *pargs, pcl_state_t *pcls)
{	pcls->display_functions = true;
	return 0;
}

/* We export this procedure so we can detect the end of display fns mode. */
int /* ESC Z */
pcl_disable_display_functions(pcl_args_t *pargs, pcl_state_t *pcls)
{	pcls->display_functions = false;
	return 0;
}

/* Initialization */
private int
pcmisc_do_init(gs_memory_t *mem)
{		/* Register commands */
	DEFINE_CLASS_COMMAND_ARGS('&', 's', 'C', "End of Line Wrap",
				  pcl_end_of_line_wrap,
				  pca_neg_error|pca_big_error)
	DEFINE_ESCAPE_ARGS('Y', "Enable Display Functions",
			   pcl_enable_display_functions, pca_in_macro)
	DEFINE_ESCAPE_ARGS('Z', "Disable Display Functions",
			   pcl_disable_display_functions, pca_in_macro)
	return 0;
}
private void
pcmisc_do_reset(pcl_state_t *pcls, pcl_reset_type_t type)
{	if ( type & (pcl_reset_initial | pcl_reset_printer) )
	  { pcls->end_of_line_wrap = false;
	    pcls->display_functions = false;
	  }
}
const pcl_init_t pcmisc_init = {
  pcmisc_do_init, pcmisc_do_reset
};
