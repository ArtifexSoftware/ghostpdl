/* Copyright (C) 2001-2020 Artifex Software, Inc.
   All Rights Reserved.

   This software is provided AS-IS with no warranty, either express or
   implied.

   This software is distributed under license and may not be copied,
   modified or distributed except as expressly authorized under the terms
   of the license contained in the file LICENSE in this distribution.

   Refer to licensing information at http://www.artifex.com or contact
   Artifex Software, Inc.,  1305 Grant Avenue - Suite 200, Novato,
   CA 94945, U.S.A., +1(415)492-9861, for further information.
*/


/* pcmisc.c */
/* PCL5 miscellaneous and debugging commands */
#include "std.h"
#include "pcommand.h"
#include "pcstate.h"

static int                      /* ESC & s <bool> C */
pcl_end_of_line_wrap(pcl_args_t * pargs, pcl_state_t * pcs)
{
    uint i = uint_arg(pargs);

    if (i > 1)
        return e_Range;
    pcs->end_of_line_wrap = i == 0;
    return 0;
}

static int                      /* ESC Y */
pcl_enable_display_functions(pcl_args_t * pargs, pcl_state_t * pcs)
{
    pcs->display_functions = true;
    return 0;
}

/* We export this procedure so we can detect the end of display fns mode. */
int                             /* ESC Z */
pcl_disable_display_functions(pcl_args_t * pargs, pcl_state_t * pcs)
{
    pcs->display_functions = false;
    return 0;
}

/* Initialization */
static int
pcmisc_do_registration(pcl_parser_state_t * pcl_parser_state,
                       gs_memory_t * mem)
{                               /* Register commands */
    DEFINE_CLASS_COMMAND_ARGS('&', 's', 'C', "End of Line Wrap",
                              pcl_end_of_line_wrap,
                              pca_neg_error | pca_big_error)
        DEFINE_ESCAPE_ARGS('Y', "Enable Display Functions",
                           pcl_enable_display_functions, pca_in_macro)
        DEFINE_ESCAPE_ARGS('Z', "Disable Display Functions",
                           pcl_disable_display_functions, pca_in_macro)
        return 0;
}
static int
pcmisc_do_reset(pcl_state_t * pcs, pcl_reset_type_t type)
{
    if (type & (pcl_reset_initial | pcl_reset_printer | pcl_reset_overlay)) {
        pcs->end_of_line_wrap = false;
        pcs->display_functions = false;
    }
    return 0;
}
const pcl_init_t pcmisc_init = {
    pcmisc_do_registration, pcmisc_do_reset
};
