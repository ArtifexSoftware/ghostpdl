/* Copyright (C) 1996, 1997, 1998 Aladdin Enterprises.  All rights
   reserved.  Unauthorized use, copying, and/or distribution
   prohibited.  */

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
    pcl_state_t *   pcs
)
{
    uint            i = uint_arg(pargs);

    if (i <= 1)
	pcs->raster_state.y_advance = (i == 1 ? -1 : 1);
    return 0;
}

/*
 * Initialization
 */
  private int
rtlrastr_do_registration(
    pcl_parser_state_t *pcl_parser_state,
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

const pcl_init_t    rtlrastr_init = { rtlrastr_do_registration, 0, 0 };
