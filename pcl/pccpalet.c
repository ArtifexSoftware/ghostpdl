/* Copyright (C) 1996, 1997 Aladdin Enterprises.  All rights reserved.
   Unauthorized use, copying, and/or distribution prohibited.
 */

/* pccpalet.c */
/* PCL5c palette commands */
#include "stdio_.h"			/* std.h + NULL */
#include "pcommand.h"
#include "pcstate.h"

private int /* ESC & p <id> S */
pcl_select_palette(pcl_args_t *pargs, pcl_state_t *pcls)
{	uint id = uint_arg(pargs);
	id_set_value(pcls->palette_id, id);
	return e_Unimplemented;
}

private int /* ESC & p <id> I */
pcl_palette_control_id(pcl_args_t *pargs, pcl_state_t *pcls)
{	uint id = uint_arg(pargs);
	id_set_value(pcls->palette_control_id, id);
	return 0;
}

private int /* ESC & p <op> C */
pcl_palette_control(pcl_args_t *pargs, pcl_state_t *pcls)
{	switch ( int_arg(pargs) )
	  {
	  case 0:
	    { /* Delete all palettes not on stack. */
	      return e_Unimplemented;
	    }
	  case 1:
	    { /* Clear the palette stack. */
	      return e_Unimplemented;
	    }
	  case 2:
	    { /* Delete palette <palette_control_id>. */
	      return e_Unimplemented;
	    }
	  case 6:
	    { /* Copy active palette to <palette_control_id>. */
	      return e_Unimplemented;
	    }
	  default:
	    return 0;
	  }
	return 0;
}

/* Initialization */
private int
pccpalet_do_init(gs_memory_t *mem)
{		/* Register commands */
	DEFINE_CLASS('&')
	  {'p', 'S', {pcl_select_palette, pca_neg_ignore|pca_big_ignore}},
	  {'p', 'I', {pcl_palette_control_id, pca_neg_ignore|pca_big_ignore}},
	  {'p', 'C', {pcl_palette_control, pca_neg_ignore|pca_big_ignore}},
	END_CLASS
	return 0;
}
private void
pccpalet_do_reset(pcl_state_t *pcls, pcl_reset_type_t type)
{	if ( type & (pcl_reset_initial | pcl_reset_printer) )
	  { id_set_value(pcls->palette_id, 0);
	    id_set_value(pcls->palette_control_id, 0);
	    if ( type & pcl_reset_initial )
	      pl_dict_init(&pcls->palettes, pcls->memory, NULL);
	    else
	      { pcl_args_t args;
	        arg_set_uint(&args, 1);	/* clear palette stack */
		pcl_palette_control(&args, pcls);
	        arg_set_uint(&args, 0);	/* delete palettes not on stack */
		pcl_palette_control(&args, pcls);
	      }
	  }
}
const pcl_init_t pccpalet_init = {
  pccpalet_do_init, pccpalet_do_reset
};
