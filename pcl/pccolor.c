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

/* pccolor.c - commands to add colors to a PCL palette */
#include "std.h"
#include "pcommand.h"
#include "pcstate.h"
#include "pcpalet.h"

/*
 * Prior to being assigned into a palette, the PCL color component values are
 * simple globals; they are not saved and restored with the PCL state.
 *
 * This implies that color components may not be converted until the set
 * color index command is executed, as it is not possible to predict for
 * which palette they will be used.
 *
 * NB: contrary to the documentation, all of the set color component commands
 *     and the assign color index command are allowed but ignored in graphics
 *     mode (i.e.: they do NOT end graphics mode, as one would expect from the
 *     documentation).
 */
private float   color_comps[3];

/*
 * ESC * v <cc> A
 */
  private int
set_color_comp_1(
    pcl_args_t *    pargs,
    pcl_state_t *   pcs
)
{
    if (!pcs->raster_state.graphics_mode)
        color_comps[0] = float_arg(pargs);
    return 0;
}

/*
 * ESC * v <cc> B
 */
  private int
set_color_comp_2(
    pcl_args_t *    pargs,
    pcl_state_t *   pcs
)
{
    if (!pcs->raster_state.graphics_mode)
        color_comps[1] = float_arg(pargs);
    return 0;
}

/*
 * ESC * v <cc> C
 */
  private int
set_color_comp_3(
    pcl_args_t *    pargs,
    pcl_state_t *   pcs
)
{
    if (!pcs->raster_state.graphics_mode)
        color_comps[2] = float_arg(pargs);
    return 0;
}

/*
 * ESC * v <indx> I
 *
 * Set a color into the active palette.
 *
 * This implementation matches HP's documentation, in that out-of-range indices
 * will not affect the palette, but will reset the color component registers.
 * This matches the behavior of the HP Color Laser Jet 5/5M, but not that of
 * the HP DeskJet 1600C/CM. For the latter, negative indices are ignored and
 * the color component registers are NOT cleared, while positive indices are 
 * interpreted modulo the palette size.
 */
  private int
assign_color_index(
    pcl_args_t *    pargs,
    pcl_state_t *   pcs
)
{
    int             indx = int_arg(pargs);

    if (!pcs->raster_state.graphics_mode) {
        if ((indx >= 0) && (indx < pcl_palette_get_num_entries(pcs->ppalet)))
            pcl_palette_set_color(pcs, indx, color_comps);
        color_comps[0] = 0.0;
        color_comps[1] = 0.0;
        color_comps[2] = 0.0;
    }
    return 0;
}

/*
 * Initialization
 */
  private int
color_do_init(
    gs_memory_t *   mem
)
{
    /* Register commands */
    DEFINE_CLASS('*')
    {
        'v', 'A',
	PCL_COMMAND( "Color Component 1",
                     set_color_comp_1,
		     pca_neg_ok | pca_big_error | pca_raster_graphics
                     )
    },
    {
        'v', 'B',
	PCL_COMMAND( "Color Component 2",
                     set_color_comp_2,
		     pca_neg_ok | pca_big_error | pca_raster_graphics
                     )
    },
    {
        'v', 'C',
        PCL_COMMAND( "Color Component 3",
                     set_color_comp_3,
		     pca_neg_ok | pca_big_error | pca_raster_graphics
                     )
    },
    {
        'v', 'I',
        PCL_COMMAND( "Assign Color Index",
                     assign_color_index,
		     pca_neg_ok | pca_big_ignore | pca_raster_graphics
                     )
    },
    END_CLASS
    return 0;
}

/*
 * Handle the various forms of reset.
 */
  private void
color_do_reset(
    pcl_state_t *        pcs,
    pcl_reset_type_t    type
)
{
    static const uint   mask = (   pcl_reset_initial
                                 | pcl_reset_cold
                                 | pcl_reset_printer );

    if ((type & mask) != 0) {
        color_comps[0] = 0.0;
        color_comps[1] = 0.0;
        color_comps[2] = 0.0;
    }
}

/*
 * There is no copy operation for this module, as the color components are
 * just globals.
 */
const pcl_init_t    pcl_color_init = { color_do_init, color_do_reset, 0 };
