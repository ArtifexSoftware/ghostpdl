/* Copyright (C) 1996, 1997 Aladdin Enterprises.  All rights reserved.
   Unauthorized use, copying, and/or distribution prohibited.
 */

/* pccprint.c */
/* PCL5c print model commands */
#include "std.h"
#include "pcommand.h"
#include "pcstate.h"
#include "gsmatrix.h"		/* for gsstate.h */
#include "gsstate.h"
#include "gsrop.h"

private int /* ESC * l <rop> O */
pcl_logical_operation(pcl_args_t *pargs, pcl_state_t *pcls)
{	uint rop = uint_arg(pargs);

	if ( rop > 255 )
	  return e_Range;
	pcls->logical_op = rop;
	return 0;
}

private int /* ESC * l <bool> R */
pcl_pixel_placement(pcl_args_t *pargs, pcl_state_t *pcls)
{	uint i = uint_arg(pargs);

	if ( i > 1 )
	  return 0;
	pcls->grid_adjust = (i == 0) ? 0.5 : 0.0;
	return 0;
}

/* Initialization */
private int
pccprint_do_init(gs_memory_t *mem)
{		/* Register commands */
	DEFINE_CLASS('*')
	  {'l', 'O',
	     PCL_COMMAND("Logical Operation", pcl_logical_operation,
			 pca_neg_error|pca_big_error)},
	  {'l', 'R',
	     PCL_COMMAND("Pixel Placement", pcl_pixel_placement,
			 pca_neg_ignore|pca_big_ignore)},
	END_CLASS
	return 0;
}
private void
pccprint_do_reset(pcl_state_t *pcls, pcl_reset_type_t type)
{	if ( type & (pcl_reset_initial | pcl_reset_printer) )
	  { pcl_args_t args;

	    arg_set_uint(&args, 252);
	    pcl_logical_operation(&args, pcls);
	    arg_set_uint(&args, 0);
	    pcl_pixel_placement(&args, pcls);
	  }
}
private int
pccprint_do_copy(pcl_state_t *psaved, const pcl_state_t *pcls,
  pcl_copy_operation_t operation)
{	if ( operation & pcl_copy_after )
	  { gs_setrasterop(pcls->pgs, psaved->logical_op);
	    gs_setfilladjust(pcls->pgs,
			     psaved->grid_adjust, psaved->grid_adjust);
	  }
	return 0;
}
const pcl_init_t pccprint_init = {
  pccprint_do_init, pccprint_do_reset, pccprint_do_copy
};
