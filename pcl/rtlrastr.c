/* Copyright (C) 1996, 1997 Aladdin Enterprises.  All rights reserved.
   Unauthorized use, copying, and/or distribution prohibited.
 */

/* rtlrastr.c */
/* HP RTL facilities not included in PCL5e/5C */
#include "std.h"
#include "pcommand.h"
#include "pcstate.h"
#include "rtraster.h"

private int /* ESC * b <direction> L */
pcl_line_path(pcl_args_t *pargs, pcl_state_t *pcls)
{	uint i = uint_arg(pargs);
	if ( i > 1 )
	  return 0;
	pcls->raster.y_direction = (i ? -1 : 1);
	return 0;
}

/* Initialization */
private int
rtlrastr_do_init(gs_memory_t *mem)
{		/* Register commands */
	DEFINE_CLASS('*')
	  {'b', 'L',
	     PCL_COMMAND("Line Path", pcl_line_path,
			 pca_neg_ignore|pca_big_ignore)},
	END_CLASS
	return 0;
}
const pcl_init_t rtlrastr_init = {
  rtlrastr_do_init
};
