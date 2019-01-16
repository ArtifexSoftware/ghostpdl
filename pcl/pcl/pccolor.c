/* Copyright (C) 2001-2019 Artifex Software, Inc.
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

/*
 * ESC * v <cc> A
 */
static int
set_color_comp_1(pcl_args_t * pargs, pcl_state_t * pcs)
{
    if (pcs->personality == pcl5e)
        return 0;
    if (!pcs->raster_state.graphics_mode)
        pcs->color_comps[0] = float_arg(pargs);
    return 0;
}

/*
 * ESC * v <cc> B
 */
static int
set_color_comp_2(pcl_args_t * pargs, pcl_state_t * pcs)
{
    if (pcs->personality == pcl5e)
        return 0;
    if (!pcs->raster_state.graphics_mode)
        pcs->color_comps[1] = float_arg(pargs);
    return 0;
}

/*
 * ESC * v <cc> C
 */
static int
set_color_comp_3(pcl_args_t * pargs, pcl_state_t * pcs)
{
    if (pcs->personality == pcl5e)
        return 0;
    if (!pcs->raster_state.graphics_mode)
        pcs->color_comps[2] = float_arg(pargs);
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
static int
assign_color_index(pcl_args_t * pargs, pcl_state_t * pcs)
{
    int code = 0;
    int indx = int_arg(pargs);

    if (pcs->personality == pcl5e)
        return 0;
    if (!pcs->raster_state.graphics_mode) {
        if ((indx >= 0) && (indx < pcl_palette_get_num_entries(pcs->ppalet)))
            code = pcl_palette_set_color(pcs, indx, pcs->color_comps);
        pcs->color_comps[0] = 0.0;
        pcs->color_comps[1] = 0.0;
        pcs->color_comps[2] = 0.0;
    }
    return code;
}

/*
 * Initialization
 */
static int
color_do_registration(pcl_parser_state_t * pcl_parser_state,
                      gs_memory_t * pmem)
{
    /* Register commands */
    DEFINE_CLASS('*') {
        'v', 'A',
            PCL_COMMAND("Color Component 1",
                        set_color_comp_1,
                        pca_neg_ok | pca_big_error | pca_raster_graphics |
                        pca_in_rtl)
    }, {
        'v', 'B',
            PCL_COMMAND("Color Component 2",
                        set_color_comp_2,
                        pca_neg_ok | pca_big_error | pca_raster_graphics |
                        pca_in_rtl)
    }, {
        'v', 'C',
            PCL_COMMAND("Color Component 3",
                        set_color_comp_3,
                        pca_neg_ok | pca_big_error | pca_raster_graphics |
                        pca_in_rtl)
    }, {
        'v', 'I',
            PCL_COMMAND("Assign Color Index",
                        assign_color_index,
                        pca_neg_ok | pca_big_ignore | pca_raster_graphics |
                        pca_in_rtl)
    }, END_CLASS return 0;
}

/*
 * Handle the various forms of reset.
 */
static int
color_do_reset(pcl_state_t * pcs, pcl_reset_type_t type)
{
    static const uint mask = (pcl_reset_initial
                              | pcl_reset_cold | pcl_reset_printer);

    if ((type & mask) != 0) {
        pcs->color_comps[0] = 0.0;
        pcs->color_comps[1] = 0.0;
        pcs->color_comps[2] = 0.0;
    }

    return 0;
}

/*
 * There is no copy operation for this module, as the color components are
 * just globals.
 */
const pcl_init_t pcl_color_init =
    { color_do_registration, color_do_reset, 0 };
