/* Copyright (C) 1996, 1997, 1998 Aladdin Enterprises.  All rights
   reserved.  Unauthorized use, copying, and/or distribution
   prohibited.  */

/* pcommand.c - Utilities for PCL 5 commands */

#include "std.h"
#include "memory_.h"
#include "gstypes.h"
#include "gsmemory.h"
#include "gsmatrix.h"
#include "gxstate.h"
#include "gsdevice.h"
#include "pcommand.h"
#include "pcparse.h"
#include "pcstate.h"
#include "pcparam.h"
#include "pcident.h"
#include "pgmand.h" /* temporary */
/*
 * Get the command argument as an int, uint, or float.
 */
  int
int_value(
    const pcl_value_t * pv
)
{
    return (int)(value_is_neg(pv) ? -(int)pv->i : pv->i);
}

  uint
uint_value(
    const pcl_value_t * pv
)
{
    return pv->i;
}

  float
float_value(
    const pcl_value_t * pv
)
{
    return ( value_is_float(pv) ?
	     (float)(value_is_neg(pv) ? -(int)pv->i - pv->fraction
                                      : pv->i + pv->fraction)
                                : (float)int_value(pv) );
}


/*
 * Set an integer parameter.
 */
  private int
end_param1(
    gs_c_param_list *   alist,
    pcl_state_t *       pcs
)
{
    int                 code;
    gs_c_param_list_read(alist);
    /* put the parameters using the target device, not the forwarding
       device */
    code = gs_state_putdeviceparams(pcs->pgs, (gs_param_list *)alist);
    gs_c_param_list_release(alist);
    return code;
}

/*
 * Set a Boolean parameter.
 */
  int
put_param1_bool(
    pcl_state_t *   pcs,
    gs_param_name   pkey,
    bool            value
)
{
    gs_c_param_list list;

    gs_c_param_list_write(&list, pcs->memory);
    /*code =*/ param_write_bool((gs_param_list *)&list, pkey, &value);
    return end_param1(&list, pcs);
}

/*
 * Set a float parameter.
 */
  int
put_param1_float(
    pcl_state_t *   pcs,
    gs_param_name   pkey,
    floatp          value
)
{
    gs_c_param_list list;
    float           fval = value;

    gs_c_param_list_write(&list, pcs->memory);
    /*code =*/ param_write_float((gs_param_list *)&list, pkey, &fval);
    return end_param1(&list, pcs);
}

/*
 * Set an integer parameter.
 */
  int
put_param1_int(
    pcl_state_t *   pcs,
    gs_param_name   pkey,
    int             value
)
{
    gs_c_param_list list;

    gs_c_param_list_write(&list, pcs->memory);
    /*code =*/ param_write_int((gs_param_list *)&list, pkey, &value);
    return end_param1(&list, pcs);
}

/*
 * Set a parameter consisting of an array of two floats. This is used to pass
 * the paper size parameter to the device.
 */
  int
put_param1_float_array(
    pcl_state_t *           pcs,
    gs_param_name           pkey,
    float                   pf[2]
)
{
    gs_c_param_list         list;
    gs_param_float_array    pf_array;

    pf_array.data = pf;
    pf_array.size = 2;
    pf_array.persistent = false;

    gs_c_param_list_write(&list, pcs->memory);
    /* code = */param_write_float_array((gs_param_list *)&list, pkey, &pf_array);
    return end_param1(&list, pcs);
}

/* initilialize the parser states */
 int
pcl_do_registrations(
    pcl_state_t *pcs,
    pcl_parser_state_t *pst
)
{
    const pcl_init_t ** init;
    /* initialize gl/2 command counter */
    hpgl_init_command_index(pst->hpgl_parser_state);
    /* initialize pcl's command counter */
    pcl_init_command_index(pst);
    for (init = pcl_init_table; *init; ++init) {
	if ( (*init)->do_registration ) {
	    int     code = (*(*init)->do_registration)(pst, pcs->memory);
	    if (code < 0) {
		lprintf1("Error %d during initialization!\n", code);
		return code;
	    }
	}
    }
    return 0;
}
/*
 * Run the reset code of all the modules.
 */
  int
pcl_do_resets(
    pcl_state_t *       pcs,
    pcl_reset_type_t    type
)
{
    const pcl_init_t ** init = pcl_init_table;
    int                 code = 0;

    for ( ; *init && code >= 0; ++init) {
	if ((*init)->do_reset)
	    (*(*init)->do_reset)(pcs, type);
    }
    return code;
}

/*
 * "Cold start" initialization of the graphic state. This is provided as a
 * special routine to avoid (as much as possible) order depedencies in the
 * various reset routines used by individual modules. Some of the values
 * selected may be subsequently overridden by the reset routines; this code
 * just attempts to set them to reasonable values.
 */
  void
pcl_init_state(
    pcl_state_t *   pcs,
    gs_memory_t *   pmem
)
{
    /* some elementary fields */
    pcs->memory = pmem;
    pcs->num_copies = 1;
    pcs->output_bin = 1;
    pcs->uom_cp = 7200L / 300L;

    pcs->perforation_skip = 1;

    pcs->font_id_type = numeric_id;
    pcs->macro_id_type = numeric_id;

    pcs->rotate_patterns = true;
    pcs->source_transparent = true;
    pcs->pattern_transparent = true;

    pcs->logical_op = 252;

    pcs->monochrome_mode = false;
    pcs->render_mode = 3;

    pcs->next_id = 8UL;
    pcl_init_gstate_stk(pcs);

}
