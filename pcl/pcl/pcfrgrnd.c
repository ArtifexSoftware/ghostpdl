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


/* pcfrgrnd.c - PCL foreground object implementation */
#include "gx.h"
#include "pcommand.h"
#include "pcfont.h"
#include "pcfrgrnd.h"

/* RC routines */
gs_private_st_simple(st_frgrnd_t, pcl_frgrnd_t, "pcl foreground object");

/*
 * Free a pcl foreground object.
 */
static void
free_foreground(gs_memory_t * pmem, void *pvfrgrnd, client_name_t cname)
{
    pcl_frgrnd_t *pfrgrnd = (pcl_frgrnd_t *) pvfrgrnd;

    if (pfrgrnd->pbase != 0)
        pcl_cs_base_release(pfrgrnd->pbase);
    if (pfrgrnd->pht != 0)
        pcl_ht_release(pfrgrnd->pht);
    gs_free_object(pmem, pvfrgrnd, cname);
}

/*
 * Allocate a pcl foreground object.
 *
 * Returns 0 on success, < 0 in the event of an error.
 */
static int
alloc_foreground(pcl_state_t * pcs,
                 pcl_frgrnd_t ** ppfrgrnd, gs_memory_t * pmem)
{
    pcl_frgrnd_t *pfrgrnd = 0;

    rc_alloc_struct_1(pfrgrnd,
                      pcl_frgrnd_t,
                      &st_frgrnd_t,
                      pmem,
                      return e_Memory, "allocate pcl foreground object");
    pfrgrnd->rc.free = free_foreground;
    pfrgrnd->id = pcl_next_id(pcs);
    pfrgrnd->pbase = 0;
    pfrgrnd->pht = 0;
    *ppfrgrnd = pfrgrnd;
    return 0;
}

/*
 * Build a foreground object from a palette object, releasing any exsiting
 * foreground object.
 *
 * There is no "unshare" routine for foreground, as a foreground object cannot
 * be modified.
 *
 * Returns 0 on success, < 0 in the event of an error
 */
static int
build_foreground(pcl_state_t * pcs,
                 pcl_frgrnd_t ** ppfrgrnd,
                 const pcl_palette_t * ppalet,
                 int pal_entry, gs_memory_t * pmem)
{
    pcl_frgrnd_t *pfrgrnd = *ppfrgrnd;
    const pcl_cs_indexed_t *pindexed = ppalet->pindexed;
    int num_entries;
    bool is_default = false;
    int code = 0;

    if (!pindexed) {
        return_error(gs_error_invalidaccess);
    }

    /*
     * Check for a request for the default foreground. Since there are only
     * three fixed palettes, it is sufficient to check that the palette provided
     * is fixed and has two entries. The default foreground is black, which is
     * the second of the two entries.
     */
    if ((pindexed->pfixed) && (pindexed->num_entries == 2) && (pal_entry == 1)) {
        is_default = true;
        if (pcs->pdflt_frgrnd != 0) {
            pcl_frgrnd_copy_from(*ppfrgrnd, pcs->pdflt_frgrnd);
            return 0;
        }
    }

    num_entries = pindexed->num_entries;
    
    /* release the existing foreground */
    if (pfrgrnd != 0) {
        rc_decrement(pfrgrnd, "build pcl foreground");
        *ppfrgrnd = 0;
    }

    if ((code = alloc_foreground(pcs, ppfrgrnd, pmem)) < 0)
        return code;
    pfrgrnd = *ppfrgrnd;

    /* pal_entry is interpreted modulo the current palette size */
    if ((pal_entry < 0) || (pal_entry >= num_entries)) {
        if (pindexed->is_GL) {
            int max_pen = num_entries - 1;

            while (pal_entry > max_pen)
                pal_entry -= max_pen;
        } else {
            pal_entry %= num_entries;
            if (pal_entry < 0)
                pal_entry += num_entries;
        }
    }

    pfrgrnd->is_cmy = (ppalet->pindexed->original_cspace == 1);
    pfrgrnd->color[0] = pindexed->palette.data[3 * pal_entry];
    pfrgrnd->color[1] = pindexed->palette.data[3 * pal_entry + 1];
    pfrgrnd->color[2] = pindexed->palette.data[3 * pal_entry + 2];
    pcl_cs_base_init_from(pfrgrnd->pbase, ppalet->pindexed->pbase);
    pcl_ht_init_from(pfrgrnd->pht, ppalet->pht);

    if (is_default)
        pcl_frgrnd_init_from(pcs->pdflt_frgrnd, pfrgrnd);

    return 0;
}

/*
 * Build the default foreground. This should be called by the reset function
 * for palettes, and should only be called when the current palette is the
 * default 2-entry palette.
 */
int
pcl_frgrnd_set_default_foreground(pcl_state_t * pcs)
{
    int code = 0;

    /* check that the palette is complete */
    if ((code = pcl_palette_check_complete(pcs)) < 0)
        return code;

    return build_foreground(pcs,
                            &(pcs->pfrgrnd), pcs->ppalet, 1, pcs->memory);
}

/*
 * ESC * v # S
 *
 * Set foreground. It is not clear the handling of negative values is accurate.
 */
static int
set_foreground(pcl_args_t * pargs, pcl_state_t * pcs)
{
    int code;

    if (pcs->personality == pcl5e || pcs->raster_state.graphics_mode)
        return 0;

    /* check that the palette is complete */
    if ((code = pcl_palette_check_complete(pcs)) < 0)
        return code;

    return build_foreground(pcs,
                            &(pcs->pfrgrnd),
                            pcs->ppalet, int_arg(pargs), pcs->memory);
}

/*
 * Initialization, reset, and copy procedures.
 *
 * There is no reset procedure for this module, as the function is accomplished
 * by the palette module.
 */
static int
frgrnd_do_registration(pcl_parser_state_t * pcl_parser_state,
                       gs_memory_t * mem)
{
    DEFINE_CLASS('*') {
        'v', 'S',
            PCL_COMMAND("Set Foreground", set_foreground,
                        pca_neg_ok | pca_raster_graphics)
    }, END_CLASS return 0;
}

static int
frgrnd_do_reset(pcl_state_t * pcs, pcl_reset_type_t type)
{
    if (type & (pcl_reset_permanent)) {
        rc_decrement(pcs->pfrgrnd, "foreground reset pfrgrnd");
        rc_decrement(pcs->pdflt_frgrnd, "foreground reset pdflt_frgrnd");
        rc_decrement(pcs->pwhite_cs, "foreground reset p_white_cs");
    }
    return 0;
}

static int
frgrnd_do_copy(pcl_state_t * psaved,
               const pcl_state_t * pcs, pcl_copy_operation_t operation)
{
    if ((operation & (pcl_copy_before_call | pcl_copy_before_overlay)) != 0)
        pcl_frgrnd_init_from(psaved->pfrgrnd, pcs->pfrgrnd);
    else if ((operation & pcl_copy_after) != 0)
        pcl_frgrnd_release(((pcl_state_t *) pcs)->pfrgrnd);
    return 0;
}

/* (white pattern or white foreground color) and transparent pattern
 * is a NOP
 */
bool
is_invisible_pattern(pcl_state_t * pcs)
{
    if (pcs->pattern_transparent) {
        if (pcs->pattern_type == pcl_pattern_solid_white)
            return true;
        if (pcs->pfrgrnd->color[0] == 0xff &&
            pcs->pfrgrnd->color[1] == 0xff && pcs->pfrgrnd->color[2] == 0xff)
            return true;        /* NB: depends on CMY conversion to internal RGB */
    }
    return false;
}

const pcl_init_t pcl_frgrnd_init =
    { frgrnd_do_registration, frgrnd_do_reset, frgrnd_do_copy };
