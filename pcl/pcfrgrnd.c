/* Copyright (C) 1996, 1997, 1998 Aladdin Enterprises.  All rights
   reserved.  Unauthorized use, copying, and/or distribution
   prohibited.  */

/* pcfrgrnd.c - PCL foreground object implementation */
#include "gx.h"
#include "pcommand.h"
#include "pcfont.h"
#include "pcfrgrnd.h"

/* the default foreground, built from the default palette */
private pcl_frgrnd_t *  pdflt_frgrnd;


/* GC routines */
private_st_frgrnd_t();


/*
 * Free a pcl foreground object.
 */
  private void
free_foreground(
    gs_memory_t *   pmem,
    void *          pvfrgrnd,
    client_name_t   cname
)
{
    pcl_frgrnd_t *  pfrgrnd = (pcl_frgrnd_t *)pvfrgrnd;

    if (pfrgrnd->pbase != 0)
        pcl_cs_base_release(pfrgrnd->pbase);
    if (pfrgrnd->pht != 0)
        pcl_ht_release(pfrgrnd->pht);
    if (pfrgrnd->pcrd != 0)
        pcl_crd_release(pfrgrnd->pcrd);
    gs_free_object(pmem, pvfrgrnd, cname);
}

/*
 * Allocate a pcl foreground object.
 *
 * Returns 0 on success, < 0 in the event of an error.
 */
  private int
alloc_foreground(
    pcl_frgrnd_t ** ppfrgrnd,
    gs_memory_t *   pmem
)
{
    pcl_frgrnd_t *  pfrgrnd = 0;

    rc_alloc_struct_1( pfrgrnd,
                       pcl_frgrnd_t,
                       &st_frgrnd_t,
                       pmem,
                       return e_Memory,
                       "allocate pcl foreground object"
                       );
    pfrgrnd->rc.free = free_foreground;
    pfrgrnd->id = pcl_next_id();
    pfrgrnd->pbase = 0;
    pfrgrnd->pht = 0;
    pfrgrnd->pcrd = 0;
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
  private int
build_foreground(
    pcl_frgrnd_t **             ppfrgrnd,
    const pcl_palette_t *       ppalet,
    int                         pal_entry,
    gs_memory_t *               pmem
)
{
    pcl_frgrnd_t *              pfrgrnd = *ppfrgrnd;
    const pcl_cs_indexed_t *    pindexed = ppalet->pindexed;
    int                         num_entries = pindexed->num_entries;
    bool                        is_default = false;
    int                         code = 0;

    /* 
     * Check for a request for the default foreground. Since there are only
     * three fixed palettes, it is sufficient to check that the palette provided
     * is fixed and has two entries. The default foreground is black, which is
     * the second of the two entries.
     */
    if ( (pindexed != 0)    &&
         (pindexed->pfixed)  &&
         (num_entries == 2) &&
         (pal_entry == 1)     ) {
        is_default = true;
        if (pdflt_frgrnd != 0) {
            pcl_frgrnd_copy_from(*ppfrgrnd, pdflt_frgrnd);
            return 0;
        }
    }

    /* release the existing foreground */
    if (pfrgrnd != 0) {
        rc_decrement(pfrgrnd, "build pcl foreground");
        *ppfrgrnd = 0;
    }

    if ((code = alloc_foreground(ppfrgrnd, pmem)) < 0)
        return code;
    pfrgrnd = *ppfrgrnd;

    /* pal_entry is interpreted modulo the current palette size */
    if ((pal_entry < 0) || (pal_entry >= num_entries)) {
        pal_entry %= num_entries;
        if (pal_entry < 0)
            pal_entry += num_entries;
    }

    pfrgrnd->color[0] = pindexed->palette.data[3 * pal_entry];
    pfrgrnd->color[1] = pindexed->palette.data[3 * pal_entry + 1];
    pfrgrnd->color[2] = pindexed->palette.data[3 * pal_entry + 2];
    pcl_cs_base_init_from(pfrgrnd->pbase, ppalet->pindexed->pbase);
    pcl_ht_init_from(pfrgrnd->pht, ppalet->pht);
    pcl_crd_init_from(pfrgrnd->pcrd, ppalet->pcrd);

    if (is_default)
        pcl_frgrnd_init_from(pdflt_frgrnd, pfrgrnd);

    return 0;
}

/*
 * Build the default foreground. This should be called by the reset function
 * for palettes, and should only be called when the current palette is the
 * default 2-entry palette.
 */
  int
pcl_frgrnd_set_default_foreground(
    pcl_state_t *   pcs
)
{
    int             code = 0;

    /* check that the palette is complete */
    if ((code = pcl_palette_check_complete(pcs)) < 0)
        return code;

    return build_foreground( &(pcs->pfrgrnd),
                             pcs->ppalet,
                             1,
                             pcs->memory
                             );
}

/*
 * ESC * v # S
 *
 * Set foreground. It is not clear the handling of negative values is accurate.
 */
  private int
set_foreground(
    pcl_args_t *        pargs,
    pcl_state_t *       pcs
)
{
    int                 code;

    pcl_break_underline(pcs);

    /* check that the palette is complete */
    if ((code = pcl_palette_check_complete(pcs)) < 0)
        return code;

    return build_foreground( &(pcs->pfrgrnd),
                             pcs->ppalet,
                             int_arg(pargs),
                             pcs->memory
                             );
}


/*
 * Initialization, reset, and copy procedures.
 *
 * There is no reset procedure for this module, as the function is accomplished
 * by the palette module.
 */
   private int
frgrnd_do_init(
    gs_memory_t *   pmem
)
{
    DEFINE_CLASS('*')
    {
        'v', 'S', 
        PCL_COMMAND("Set Foreground", set_foreground, pca_neg_ok)
    },
    END_CLASS

    /* handle possible non-initialization of BSS */
    pdflt_frgrnd = 0;

    return 0;
}

  private int
frgrnd_do_copy(
    pcl_state_t *           psaved,
    const pcl_state_t *     pcs,
    pcl_copy_operation_t    operation
)
{
    if ((operation & (pcl_copy_before_call | pcl_copy_before_overlay)) != 0)
        pcl_frgrnd_init_from(psaved->pfrgrnd, pcs->pfrgrnd);
    else if ((operation & pcl_copy_after) != 0)
        pcl_frgrnd_release(((pcl_state_t *)pcs)->pfrgrnd);
    return 0;
}

const pcl_init_t    pcl_frgrnd_init = { frgrnd_do_init, 0, frgrnd_do_copy };
