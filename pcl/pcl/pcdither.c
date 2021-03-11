/* Copyright (C) 2001-2021 Artifex Software, Inc.
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


/* pcdither.c - PCL user-defined dither object implementation */

#include "pcommand.h"
#include "pcpalet.h"
#include "pcdither.h"

/* GC Routines */
gs_private_st_simple(st_udither_t, pcl_udither_t,
                     "pcl user defined dither matrix");

/*
 * Return a pointer to the thershold array appropriate for one color plane.
 */
const byte *
pcl_udither_get_threshold(const pcl_udither_t * pdither, int indx)
{
    uint nplanes = pdither->ptbl->nplanes;

    if (nplanes == 1)
        return pdither->ptbl->data;
    else
        return pdither->ptbl->data + indx * pcl_udither_get_size(pdither);
}

/*
 * Free a dither matrix structure.
 */
static void
free_dither_matrix(gs_memory_t * pmem, void *pvdither, client_name_t cname)
{
    pcl_udither_t *pdither = (pcl_udither_t *) pvdither;

    if (pdither->ptbl != 0)
        gs_free_object(pmem, (void *)pdither->ptbl, cname);
    gs_free_object(pmem, pvdither, cname);
}

/*
 * ESC * m <nbytes> W
 *
 * Download dither matrix.
 */
static int
download_dither_matrix(pcl_args_t * pargs, pcl_state_t * pcs)
{
    uint len = uint_arg(pargs);
    pcl_udither_t *pdither = 0;

    const pcl__udither_t *ptbl = (pcl__udither_t *) arg_data(pargs);
    uint nplanes, h, w, rlen;
    int code = 0;
    byte *bp;

#ifdef DEBUG
    if (gs_debug_c('i')) {
        pcl_debug_dump_data(pcs->memory, arg_data(pargs), uint_arg(pargs));
    }
#endif

    if (pcs->raster_state.graphics_mode)
        return 0;
    /* check for legitimate parameter values */
    if (len < 7)
        return 0;
    nplanes = ptbl->nplanes;
    h = (ptbl->height[0] << 8) + ptbl->height[1];
    w = (ptbl->width[0] << 8) + ptbl->width[1];
    rlen = nplanes * h * w + 6;
    if (((nplanes != 1) && (nplanes != 3)) ||
        (h == 0) || (w == 0) || (len < rlen))
        return e_Range;

    rc_alloc_struct_1(pdither,
                      pcl_udither_t,
                      &st_udither_t,
                      pcs->memory, return e_Memory, "download dither matrix");
    pdither->rc.free = free_dither_matrix;
    pdither->ptbl = 0;

    /* either take possession of buffer, or allocate a new one */
    if (pargs->data_on_heap)
        arg_data(pargs) = 0;
    else {
        pcl__udither_t *ptmp = 0;

        ptmp = (pcl__udither_t *) gs_alloc_bytes(pcs->memory,
                                                 rlen,
                                                 "donwload dither matrix");
        if (ptmp == 0) {
            free_dither_matrix(pdither->rc.memory,
                               pdither, "download dither matrix");
            return e_Memory;
        }
        memcpy(ptmp, ptbl, rlen);
        ptbl = ptmp;
    }
    pdither->height = h;
    pdither->width = w;
    pdither->ptbl = ptbl;

    /* do not allow the value 0 in the array - black must be black */
    bp = (byte *) ptbl->data;
    rlen -= 6;
    while (rlen-- > 0) {
        byte b = *bp;

        *bp++ = (b == 0 ? 1 : b);
    }

    /* update the dither matrix; release our reference */
    code = pcl_palette_set_udither(pcs, pdither);
    pcl_udither_release(pdither);
    return code;
}

/*
 * There is no reset or copy procedure for this module, as both functions are
 * handled at the palette level.
 */
static int
udither_do_registration(pcl_parser_state_t * pcl_parser_state,
                        gs_memory_t * pmem)
{
    DEFINE_CLASS('*') {
        'm', 'W',
            PCL_COMMAND("Download Dither Matrix", download_dither_matrix,
                        pca_bytes | pca_raster_graphics)
    }, END_CLASS return 0;
}

const pcl_init_t pcl_udither_init = { udither_do_registration, 0, 0 };
