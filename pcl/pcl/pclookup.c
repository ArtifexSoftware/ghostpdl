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


/* pclookup.c - color lookup table implementation for PCL 5c */

#include "pcpalet.h"
#include "pclookup.h"

/* RC routines */
gs_private_st_simple(st_lookup_tbl_t, pcl_lookup_tbl_t,
                     "pcl color lookup table");

/*
 * Free lookup table.
 */
static void
free_lookup_tbl(gs_memory_t * pmem, void *pvlktbl, client_name_t cname)
{
    pcl_lookup_tbl_t *plktbl = (pcl_lookup_tbl_t *) pvlktbl;

    if (plktbl->ptbl != 0)
        gs_free_object(pmem, (void *)plktbl->ptbl, cname);
    gs_free_object(pmem, pvlktbl, cname);
}

/*
 * ESC * l <nbytes> W
 *
 * Set color lookup table.
 */
static int
set_lookup_tbl(pcl_args_t * pargs, pcl_state_t * pcs)
{
    uint len = uint_arg(pargs);
    pcl_lookup_tbl_t *plktbl = 0;
    pcl__lookup_tbl_t *ptbl = 0;
    int code = 0;

#ifdef DEBUG
    if (gs_debug_c('i')) {
        pcl_debug_dump_data(pcs->memory, arg_data(pargs), uint_arg(pargs));
    }
#endif

    if (pcs->personality == pcl5e || pcs->raster_state.graphics_mode)
        return 0;

    /* check for clearing of lookup tables, and for incorrect size */
    if (len == 0)
        return pcl_palette_set_lookup_tbl(pcs, NULL);
    else if (len != sizeof(pcl__lookup_tbl_t))
        return e_Range;

    rc_alloc_struct_1(plktbl,
                      pcl_lookup_tbl_t,
                      &st_lookup_tbl_t,
                      pcs->memory, return e_Memory, "set color lookup table");
    plktbl->rc.free = free_lookup_tbl;
    plktbl->ptbl = 0;

    /* either take possession of buffer, or allocate a new one */
    if (pargs->data_on_heap) {
        ptbl = (pcl__lookup_tbl_t *) arg_data(pargs);
        arg_data(pargs) = 0;
    } else {
        ptbl = (pcl__lookup_tbl_t *) gs_alloc_bytes(pcs->memory,
                                                    sizeof(pcl__lookup_tbl_t),
                                                    "set color lookup table");
        if (ptbl == 0) {
            free_lookup_tbl(plktbl->rc.memory, plktbl,
                            "set color lookup table");
            return e_Memory;
        }
        memcpy(ptbl, arg_data(pargs), sizeof(pcl__lookup_tbl_t));
    }
    plktbl->ptbl = ptbl;

    /* for the CMY color space, convert to RGB color space */
    if (pcl_lookup_tbl_get_cspace(plktbl) == pcl_cspace_CMY) {
        int i;

        for (i = 0; i < 128; i++) {
            byte b1 = ptbl->data[i];

            byte b2 = ptbl->data[255 - i];

            ptbl->data[i] = 255 - b2;
            ptbl->data[255 - i] = 255 - b1;
        }
        ptbl->cspace = (byte) pcl_cspace_RGB;
    }

    /* update the current palette; release our reference to the lookup table */
    code = pcl_palette_set_lookup_tbl(pcs, plktbl);
    pcl_lookup_tbl_release(plktbl);
    return code;
}

/*
 * ESC * t # I
 *
 * Set gamma correction
 */
static int
set_gamma_correction(pcl_args_t * pargs, pcl_state_t * pcs)
{
    float gamma = float_arg(pargs);

    if (pcs->personality == pcl5e || pcs->raster_state.graphics_mode)
        return 0;
    if ((gamma < 0.0) || (gamma > (float)((1L << 15) - 1)))
        return 0;
    else
        return pcl_palette_set_gamma(pcs, gamma);
}

/*
 * There is no copy or reset code for this module, as both copying and reset
 * are handled by the PCL palette module (using macros defined in pclookup.h).
 */
static int
lookup_do_registration(pcl_parser_state_t * pcl_parser_state,
                       gs_memory_t * pmem)
{
    DEFINE_CLASS('*') {
        'l', 'W',
            PCL_COMMAND("Color Lookup Tables", set_lookup_tbl,
                        pca_bytes | pca_raster_graphics)
    }, {
        't', 'I',
            PCL_COMMAND("Gamma Correction",
                        set_gamma_correction,
                        pca_neg_ignore | pca_big_ignore | pca_raster_graphics)
    }, END_CLASS return 0;
}

const pcl_init_t pcl_lookup_tbl_init = { lookup_do_registration, 0, 0 };
