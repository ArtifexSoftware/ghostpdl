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

/* rtlrastr.c - HP RTL facilities not included in PCL5e/5C */

#include "std.h"
#include "pcommand.h"
#include "pcstate.h"
#include "rtgmode.h"

/*
 * ESC * b <direction> L
 *
 * set raster print direction
 */
  private int
set_line_path(
    pcl_args_t *    pargs,
    pcl_state_t *   pcls
)
{
    uint            i = uint_arg(pargs);

    if (i <= 1)
	pcls->raster_state.y_advance = (i == 1 ? -1 : 1);
    return 0;
}

/*
 * Initialization
 */
  private int
rtlrastr_do_init(
    gs_memory_t *   mem
)
{
    DEFINE_CLASS('*')
    {
        'b', 'L',
        PCL_COMMAND( "Line Path",
                     set_line_path,
		     pca_neg_ok | pca_big_ignore
                     )
    },
    END_CLASS
   return 0;
}

const pcl_init_t    rtlrastr_init = { rtlrastr_do_init, 0, 0 };
