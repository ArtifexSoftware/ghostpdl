/*
 * Copyright (C) 1998 Aladdin Enterprises.
 * All rights reserved.
 *
 * This file is part of Aladdin Ghostscript.
 *
 * Aladdin Ghostscript is distributed with NO WARRANTY OF ANY KIND.  No author
 * or distributor accepts any responsibility for the consequences of using it,
 * or for whether it serves any particular purpose or works at all, unless he
 * or she says so in writing.  Refer to the Aladdin Ghostscript Free Public
 * License (the "License") for full details.
 *
 * Every copy of Aladdin Ghostscript must include a copy of the License,
 * normally in a plain ASCII text file named PUBLIC.  The License grants you
 * the right to copy, modify and redistribute Aladdin Ghostscript, but only
 * under certain conditions described in the License.  Among other things, the
 * License requires that the copyright notice and this notice be preserved on
 * all copies.
 */

/* pcommand.c - Utilities for PCL 5 commands */

#include "std.h"
#include "gstypes.h"
#include "gsmemory.h"
#include "gsmatrix.h"
#include "gxstate.h"
#include "gsdevice.h"
#include "pcommand.h"
#include "pcstate.h"
#include "pcparam.h"
#include "pcident.h"

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
    code = gs_putdeviceparams( gs_currentdevice(pcs->pgs),
                               (gs_param_list *)alist
                               );
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
    /* start off setting everything to 0 */
    memset(pcs, 0, sizeof(pcl_state_t));

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

    pcl_init_gstate_stk(pcs);

    /* the PCL identifier mechanism is not strictly part of the state */
    pcl_init_id();
}
