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

/* pcpalet.c - PCL 5c palette object implementation */
#include "gx.h"
#include "pldict.h"
#include "pcpalet.h"
#include "pcfrgrnd.h"

/* dictionary to hold the palette store */
private pl_dict_t   palette_store;


/* GC routines */
private_st_palette_t();

/*
 * Free a PCL palette.
 */
  private void
free_palette(
    gs_memory_t *   pmem,
    void *          pvpalet,
    client_name_t   cname
)
{
    pcl_palette_t * ppalet = (pcl_palette_t *)pvpalet;

    if (ppalet->pindexed != 0)
        pcl_cs_indexed_release(ppalet->pindexed);
    if (ppalet->pcrd != 0)
        pcl_crd_release(ppalet->pcrd);
    if (ppalet->pht != 0)
        pcl_ht_release(ppalet->pht);
    gs_free_object(pmem, pvpalet, cname);
}

/*
 * Free procedure for palettes to be used by the dictionary which implements
 * the palette store.
 */
  private void
dict_free_palette(
    gs_memory_t *   pmem,
    void *          pvpalet,
    client_name_t   cname
)
{
    pcl_palette_t * ppalet = (pcl_palette_t *)pvpalet;

    rc_decrement(ppalet, cname);
}

/*
 * Allocat a PCL palette object.
 *
 * Returns 0 on success, < 0 in the event of an error.
 */
  private int
alloc_palette(
    pcl_palette_t **    pppalet,
    gs_memory_t *       pmem
)
{
    pcl_palette_t *     ppalet = 0;

    rc_alloc_struct_1( ppalet,
                       pcl_palette_t,
                       &st_palette_t,
                       pmem,
                       return e_Memory,
                       "allocate pcl palette object"
                       );
    ppalet->rc.free = free_palette;
    ppalet->id = pcl_next_id();
    ppalet->pindexed = 0;
    ppalet->pcrd = 0;
    ppalet->pht = 0;
    *pppalet = ppalet;
    return 0;
}

/*
 * Create a unique copy of a PCL palette.
 *
 * The unsharing of PCL palettes is a bit involved due to the nature of the
 * palette store. In normal handling, the current PCL state does not maintain
 * a reference to the current palette. Rather, it keeps the current palette id.,
 * and the definition of this id. in the dictionary that implements the palette
 * store is the only reference to the palette.
 *
 * The structure of XL dictionaries is (properly) opaque, so there is no direct
 * access is available to pointer to a palette. Furthermore, the dictionary
 * get, put, and undef procedures do not recognize object structure, hence they
 * provide no native support for reference counted objects.
 *
 * To work around these problems, a separate pointer to a palette is maintained
 * in the PCL, though one that does not normally counted as a reference. The
 * make-unique operation will modify this pointer, and immediately redefine
 * the current palette id.
 *
 * To simplify the remainder of this code, unshared palettes are given new
 * identifiers, even if the unshare operation does nothing. This operation is
 * specifically overridden when modifying pen widths, since the latter are never
 * cached.
 *
 * Unlike the "unshare" functions for other objects, this function will create
 * the PCL palette object if it does not already exist.
 *
 * Returns 0 on success, < 0 in the event of an error.
 */
  private int
unshare_palette(
    pcl_state_t *       pcs
)
{
    pcl_palette_t *     ppalet = pcs->ppalet;
    pcl_palette_t *     pnew = 0;
    int                 code = 0;
    pcl_id_t            key;

    /* check if there is anything to do */
    if ((ppalet != 0) && (ppalet->rc.ref_count == 1)) {
        ppalet->id = pcl_next_id();
        return 0;
    }

    /* allocate a new palette */
    if ((code = alloc_palette(&pnew, pcs->memory)) < 0)
        return code;
    if (ppalet != 0) {
        pcl_cs_indexed_init_from(pnew->pindexed, ppalet->pindexed);
        pcl_crd_init_from(pnew->pcrd, ppalet->pcrd);
        pcl_ht_init_from(pnew->pht, ppalet->pht);
    }

    /* redefine the current palette id. */
    pcs->ppalet = pnew;
    id_set_value(key, pcs->sel_palette_id);
    code = pl_dict_put(&palette_store, id_key(key), 2, pnew);
    return (code == -1 ? e_Memory : 0);
}

/*
 * Build a default palette, and define it to be the currently selected
 * palette.
 */
  private int
build_default_palette(
    pcl_state_t *   pcs
)
{
    pcl_id_t        key;
    gs_memory_t *   pmem = pcs->memory;
    pcl_palette_t * ppalet = 0;
    int             code = alloc_palette(&ppalet, pcs->memory);

    if (code == 0)
        code = pcl_cs_indexed_build_default_cspace(&(ppalet->pindexed), pmem);
    if ((code == 0) && (pcl_default_crd == 0))
        code = pcl_crd_build_default_crd(pmem);
    if (code == 0)
        pcl_crd_init_from(ppalet->pcrd, pcl_default_crd);
    if (code == 0)
        code = pcl_ht_build_default_ht(&(ppalet->pht), pmem);
    if (code < 0)
        return code;

    id_set_value(key, pcs->sel_palette_id);
    pl_dict_undef(&palette_store, id_key(key), 2);
    pcs->ppalet = 0;
    code = pl_dict_put(&palette_store, id_key(key), 2, ppalet);
    if (code < 0)
        return e_Memory;
    pcs->ppalet = ppalet;
    return 0;
}

/*
 * Palette stack. This is implemented as a simple linked list, and is opaque
 * outside of this module.
 */
typedef struct pstack_entry_s {
    struct pstack_entry_s * pnext;
    pcl_palette_t *         ppalet;
} pstack_entry_t;

gs_private_st_ptrs1( st_pstack_entry_t,
                     pstack_entry_t,
                     "palette stack entry",
                     pstack_enum_ptrs,
                     pstack_reloc_ptrs,
                     ppalet
                     );

private pstack_entry_t *    palette_stack;

/*
 * Clear the palette stack.
 */
  private void
clear_palette_stack(
    gs_memory_t *       pmem
)
{
    pstack_entry_t *    pentry = palette_stack;

    while (pentry != 0) {
        pstack_entry_t *    pnext = pentry->pnext;

        pcl_palette_release(pentry->ppalet);
        gs_free_object(pmem, pentry, "clear palette stack");
        pentry = pnext;
    }
}

/*
 * ESC * p # P
 *
 * Push a copy of the current palette onto the palette stack, or pop one off.
 * Note the need to redefine the select palette id. in the event that a palette
 * is popped.
 */
  private int
push_pop_palette(
    pcl_args_t *    pargs,
    pcl_state_t *   pcs
)
{
    int             action = uint_arg(pargs);

    if (action == 0) {
        pstack_entry_t *    pentry;
        int                 code;

        /* if there is not yet a palette, build the default */
        if ((pcs->ppalet == 0) && ((code = build_default_palette(pcs)) < 0))
            return code;

        pentry = gs_alloc_struct( pcs->memory,
                                  pstack_entry_t,
                                  &st_pstack_entry_t,
                                  "push pcl palette"
                                  );
        if (pentry == 0)
            return e_Memory;

        pcl_palette_init_from(pentry->ppalet, pcs->ppalet);
        pentry->pnext = palette_stack;
        palette_stack = pentry;

        return 0;

    } else if (action == 1) {
        pstack_entry_t *    pentry = palette_stack;
        int                 code = 0;

        if (pentry != 0) {
            pcl_id_t    key;

            palette_stack = pentry->pnext;

            /* NB: USE INIT RATHER THAN COPY (pl_dict_put sets ref. count) */
            pcl_palette_init_from(pcs->ppalet, pentry->ppalet);

            id_set_value(key, pcs->sel_palette_id);
            code = pl_dict_put(&palette_store, id_key(key), 2, pentry->ppalet);
            gs_free_object(pcs->memory, pentry, "pop pcl palette");
        }

        return (code < 0 ? e_Memory : 0);

    } else
        return 0;
}


/*
 * Set the normalization values for an indexed color space. This is needed
 * only for the GL/2 CR command; PCL sets normalization information via the
 * configure image data command, which builds a new indexed color space.
 *
 * Returns 0 on success, < 0 in the event of an error.
 */
  int
pcl_palette_CR(
    pcl_state_t *   pcs,
    floatp          wht0,
    floatp          wht1,
    floatp          wht2,
    floatp          blk0,
    floatp          blk1,
    floatp          blk2
)
{
    int             code = unshare_palette(pcs);

    /* if the default color space must be built, it is fixed, so don't bother */
    if (pcs->ppalet->pindexed == 0)
        return code;

    if (code == 0)
        code = pcl_cs_indexed_set_norm_and_Decode( &(pcs->ppalet->pindexed),
                                                   wht0,
                                                   wht1,
                                                   wht2,
                                                   blk0,
                                                   blk1,
                                                   blk2
                                                   );
    return code;
}

/*
 * Set the number of entries in a color palette. This is needed only for the
 * GL/2 NP command; PCL sets the number of entries in a palette via the
 * configure image data command, which creates a new indexed color space.
 *
 * Returns 0 on success, < 0 in the event of an error.
 */
  int
pcl_palette_NP(
    pcl_state_t *   pcs,
    int             num_entries
)
{
    int             code = unshare_palette(pcs);

    /* if the default color space must be built, it is fixed, so don't bother */
    if (pcs->ppalet->pindexed == 0)
        return code;

    if (code == 0)
        code = pcl_cs_indexed_set_num_entries( &(pcs->ppalet->pindexed),
                                               num_entries,
                                               true
                                               );
    return code;
}

/*
 * Set the render method for the palette.
 *
 * Returns 0 on success, < 0 in the event of an error.
 */
  int
pcl_palette_set_render_method(
    pcl_state_t *    pcs,
    int              render_method
)
{
    int              code = unshare_palette(pcs);

    if ((code == 0) && (pcs->ppalet->pht == 0))
        code = pcl_ht_build_default_ht(&(pcs->ppalet->pht), pcs->memory);
    if (code == 0)
        code = pcl_ht_set_render_method(&(pcs->ppalet->pht), render_method);
    return code;
}

/*
 * Set gamma correction information for a palette.
 *
 * Gamma correction and the color lookup table for device specific color spaces
 * perform the same function, but are of different origins. Hence, while a
 * configure image data command will discard all color lookup tables, it inherits
 * the gamma configuration parameter from the existing palette. In addition,
 * while there is an "unset" command for color lookup tables, there is no such
 * command for gamma correction: to override the existing gamma correction,
 * either specify a new one or download a color correction table for a device
 * specific color space.
 *
 * Returns 0 on success, < 0 in the event of an error.
 */
  int
pcl_palette_set_gamma(
    pcl_state_t *   pcs,
    float           gamma
)
{
    int             code = unshare_palette(pcs);

    if ((code == 0) && (pcs->ppalet->pht == 0))
        code = pcl_ht_build_default_ht(&(pcs->ppalet->pht), pcs->memory);
    if (code == 0)
        code = pcl_ht_set_gamma(&(pcs->ppalet->pht), gamma);
    return code;
}

/*
 * Set color lookup table information for a palette.
 *
 * Lookup tables for device-specific and device-independent color spaces are
 * implemented in different ways. The former are implemented via transfer
 * functions, and thus affect the halftone component of the current palette.
 * The latter are implemented in the device-independent color spaces themselves,
 * and thus affect the color spaces component of the palette.
 *
 * An anachronism of the PCL is that, while color lookup tables may be set
 * individually for different color spaces, they only be cleared all at once.
 * This is accomplished by calling this routine with a null lookup table pointer.
 *
 * Returns 0 on success, < 0 in the event of an error.
 */
  int
pcl_palette_set_lookup_tbl(
    pcl_state_t *       pcs,
    pcl_lookup_tbl_t *  plktbl
)
{
    int                 code = unshare_palette(pcs);
    pcl_cspace_type_t   lktype;

    if ((code == 0) && (pcs->ppalet->pindexed == 0))
        code = pcl_cs_indexed_build_default_cspace( &(pcs->ppalet->pindexed),
                                                    pcs->memory
                                                    );
    if ((code == 0) && (pcs->ppalet->pht == 0))
        code = pcl_ht_build_default_ht(&(pcs->ppalet->pht), pcs->memory);
    if (code < 0)
        return code;

    /* all look tables are cleared simultaneously */
    if (plktbl != 0)
        lktype = pcl_lookup_tbl_get_cspace(plktbl);
    if ((plktbl == 0) || (lktype <= pcl_cspace_CMY))
        code = pcl_ht_set_lookup_tbl(&(pcs->ppalet->pht), plktbl);
    if ( (code == 0) && ((plktbl == 0) || (lktype >= pcl_cspace_Colorimetric)) )
        code = pcl_cs_indexed_update_lookup_tbl(&(pcs->ppalet->pindexed), plktbl);
    return code;
}

/*
 * Set an entry in a color palette.
 *
 * Returns 0 on success, < 0 in the event of an error. The returned code will
 * normally be ignored.
 */
  int
pcl_palette_set_color(
    pcl_state_t *   pcs,
    int             indx,
    const float     comps[3]
)
{
    int             code = unshare_palette(pcs);

    /* if the default color space must be built, it is fixed, so don't bother */
    if (pcs->ppalet->pindexed == 0)
        return code;

    if (code == 0)
        code = pcl_cs_indexed_set_palette_entry( &(pcs->ppalet->pindexed),
                                                 indx,
                                                 comps
                                                 );
    return code;
}

/*
 * Set a palette entry to its default color.
 *
 * Returns 0 on success, < 0 in the event of an error. The returned code will
 * normally be ignored.
 */
  int
pcl_palette_set_default_color(
    pcl_state_t *   pcs,
    int             indx
)
{
    int             code = unshare_palette(pcs);

    /* if the default color space must be built, it is fixed, so don't bother */
    if (pcs->ppalet->pindexed == 0)
        return code;

    if (code == 0)
        code = pcl_cs_indexed_set_default_palette_entry( &(pcs->ppalet->pindexed),
                                                         indx
                                                         );
    return code;
}

/*
 * Set a pen width. Note that this does NOT change the palette id. This
 * procedure can only be called from GL/2, hence the procedure name.
 *
 * Returns 0 on success, < 0 in the even of an error;
 */
  int
pcl_palette_PW(
    pcl_state_t *   pcs,
    int             pen,
    floatp          width
)
{
    int             code = 0;
    pcl_gsid_t      palette_id;
    pcl_palette_t * ppalet = pcs->ppalet;

    if (ppalet != 0) {
        palette_id = ppalet->id;
        if ((code = unshare_palette(pcs)) < 0)
            return code;
        ppalet = pcs->ppalet;
        ppalet->id = palette_id;
    } else if ((code = unshare_palette(pcs)) < 0)
        return code;
    return pcl_cs_indexed_set_pen_width(&(ppalet->pindexed), pen, width);
}

/*
 * Set the user-defined dither matrix.
 *
 * Returns 0 on success, < 0 in the event of an error.
 */
  int
pcl_palette_set_udither(
    pcl_state_t *   pcs,
    pcl_udither_t * pdither
)
{
    int             code = unshare_palette(pcs);

    if ((code == 0) && (pcs->ppalet->pht == 0))
        code = pcl_ht_build_default_ht(&(pcs->ppalet->pht), pcs->memory);
    if (code == 0)
        code = pcl_ht_set_udither(&(pcs->ppalet->pht), pdither);
    return code;
}

/*
 * Overwrite the current palette with new a new image data configuration.
 * This will rebuild the indexed color space, and discard any currently
 * installed color lookup tables.
 *
 * Tf the operand "fixed" is true, this procedure is being called as part of
 * a "simple color mode" command, and the resulting color palette will have
 * fixed entries.
 *
 * The boolean operand gl2 indicates if this call is being made as the result
 * of an IN command in GL/2. If so, the default set of entries in the color
 * palette is modified.
 *
 * Returns 0 on success, < 0 in the event of an error.
 */
  int
pcl_palette_set_cid(
    pcl_state_t *       pcs,
    pcl_cid_data_t *    pcid,
    bool                fixed,
    bool                gl2
)
{
    int                 code = unshare_palette(pcs);
    pcl_palette_t *     ppalet = pcs->ppalet;
    pcl_cspace_type_t   cstype_new = pcl_cid_get_cspace(pcid);
    pcl_cspace_type_t   cstype_old;

    if (code < 0)
        return code;

    /* record the old color space type, if it is present */
    if (ppalet->pindexed != 0)
        cstype_old = (pcl_cspace_type_t)ppalet->pindexed->cid.cspace;
    else
        cstype_old = cstype_new;

    /* pcl_cspace_bnuild_indexed_cspace will release the old space */
    code = pcl_cs_indexed_build_cspace( &(ppalet->pindexed),
                                        pcid,
                                        fixed,
                                        gl2,
                                        pcs->memory
                                        );

    /* if a halftone exist, inform it of the update and discard lookup tables */
    if ((code == 0) && (ppalet->pht != 0)) {
        code = pcl_ht_update_cspace(&(ppalet->pht), cstype_old, cstype_new);
        if (code == 0)
            code = pcl_ht_set_lookup_tbl(&(ppalet->pht), NULL);
    }

    return code;
}

/*
 * Set the view illuminant for a palette.
 *
 * Returns 0 on success, < 0 in the event of an error.
 */
  int
pcl_palette_set_view_illuminant(
    pcl_state_t *       pcs,
    const gs_vector3 *  pwht_pt
)
{
    int                 code = unshare_palette(pcs);

    if ((code == 0) && (pcs->ppalet->pcrd == 0)) {
        if ((code = pcl_crd_build_default_crd(pcs->memory)) == 0)
            pcl_crd_init_from(pcs->ppalet->pcrd, pcl_default_crd);
    }
    if (code == 0)
        code = pcl_crd_set_view_illuminant(&(pcs->ppalet->pcrd), pwht_pt);
    return code;
}

/*
 * Check that all parts of a PCL palette have been built. If not, build the
 * necessary default objects.
 *
 * This procedure should never be required, as the reset operation should build
 * a default palette, but it is kept in case that routine does not succeed.
 *
 * Returns 0 on success, < 0 in the event of an error.
 */
  int
pcl_palette_check_complete(
    pcl_state_t *   pcs
)
{
    pcl_palette_t * ppalet = pcs->ppalet;
    int             code = 0;

    if ( (ppalet != 0)           &&
         (ppalet->pindexed != 0) &&
         (ppalet->pcrd != 0)     &&
         (ppalet->pht != 0)        )
        return 0;

    if ((code = unshare_palette(pcs)) < 0)
        return code;
    ppalet = pcs->ppalet;
    if (ppalet->pindexed == 0)
        code = pcl_cs_indexed_build_default_cspace( &(ppalet->pindexed),
                                                    pcs->memory
                                                    );
    if ((code == 0) && (ppalet->pcrd == 0)) {
        if ((code = pcl_crd_build_default_crd(pcs->memory)) == 0)
            pcl_crd_init_from(pcs->ppalet->pcrd, pcl_default_crd);
    }
    if ((code == 0) && (ppalet->pht == 0))
        code = pcl_ht_build_default_ht(&(ppalet->pht), pcs->memory);
    return code;
}


/*
 * ESC & p # S
 *
 * Change the select palette id. Note that, since the pointer to the palette in
 * the pcl state does not count as a reference, no reference counts are adjusted.
 */
  private int
set_sel_palette_id(
    pcl_args_t *    pargs,
    pcl_state_t *   pcs
)
{
    uint            id = uint_arg(pargs);
    pcl_id_t        key;

    /* ignore attempts to select non-existent palettes */
    id_set_value(key, id);
    if ( pl_dict_lookup( &palette_store,
                         id_key(key),
                         2, 
                         (void **)&(pcs->ppalet),
                         false,
                         NULL
                         ) )
        pcs->sel_palette_id = id;
    return 0;
}

/*
 * ESC & p # I
 *
 * Set the palette control id.
 */
  private int
set_ctrl_palette_id(
    pcl_args_t *    pargs,
    pcl_state_t *   pcs
)
{
    pcs->ctrl_palette_id = uint_arg(pargs);
    return 0;
}

/*
 * Clear the palette store. This will delete the current palette, but will
 * NOT build the default palette in its place.
 */
  private void
clear_palette_store(
    pcl_state_t *   pcs
)
{
    pl_dict_enum_t  denum;
    void *          pvalue;
    gs_const_string plkey;

    pl_dict_enum_begin(&palette_store, &denum);
    while (pl_dict_enum_next(&denum, &plkey, &pvalue))
        pl_dict_undef(&palette_store, plkey.data, plkey.size);
    pcs->ppalet = 0;
}

/*
 * ESC & p # C
 *
 * Palette control
 */
  private int
palette_control(
    pcl_args_t *    pargs,
    pcl_state_t *   pcs
)
{
    int             action = int_arg(pargs);

    switch (action) {

      case 0:
        clear_palette_store(pcs);
        build_default_palette(pcs);
        break;

      case 1:
        clear_palette_stack(pcs->memory);
        break;

      case 2:
        if (pcs->ctrl_palette_id == pcs->sel_palette_id)
            build_default_palette(pcs);
        else {
            pcl_id_t  key;

            id_set_value(key, pcs->ctrl_palette_id);
            pl_dict_undef(&palette_store, id_key(key), 2);
        }
        break;

      case 6:
        if (pcs->ctrl_palette_id != pcs->sel_palette_id) {
            pcl_id_t        key;
            pcl_palette_t * pnew = 0;

            /* NB: THIS COPY_FROM IS NECESSARY - defn.'s don't ref. count */
            pcl_palette_copy_from(pnew, pcs->ppalet);
            id_set_value(key, pcs->ctrl_palette_id);
            pl_dict_put(&palette_store, id_key(key), 2, pnew);
        }
        break;

      default:
        return e_Range;
    }

    return 0;
}

/*
 * ESC * t # J
 *
 * Set render method. The rendering method is specifically part of the PCL
 * halftone structure, but the setting command is included here because it
 * must run via the palette mechanism.
 */
  private int
set_render_algorithm(
    pcl_args_t *    pargs,
    pcl_state_t *   pcs
)
{
    return pcl_palette_set_render_method(pcs, int_arg(pargs));
}


/*
 * Initialization routine for palettes.
 */
  private int
palette_do_init(
    gs_memory_t *    pmem
)
{
    DEFINE_CLASS('*')
    {
        'p', 'P',
        PCL_COMMAND( "Push/Pop Palette",
                     push_pop_palette,
                     pca_neg_ignore | pca_big_ignore
                     )
    },
    {
        't', 'J',
        PCL_COMMAND( "Render Algorithm",
                     set_render_algorithm,
                     pca_neg_ignore | pca_big_ignore
                     )
    },
    END_CLASS

    DEFINE_CLASS('&')
    {
        'p', 'S',
        PCL_COMMAND("Select Palette", set_sel_palette_id, pca_big_ignore)
    },
    {
        'p', 'I',
        PCL_COMMAND("Palette Control ID", set_ctrl_palette_id, pca_big_ignore)
    },
    {
        'p', 'C',
        PCL_COMMAND( "Palette Control",
                     palette_control,
                     pca_neg_ignore | pca_big_ignore
                     )
    },
    END_CLASS
    return 0;
}

/*
 * Reset routine for palettes.
 */
  private void
palette_do_reset(
    pcl_state_t *       pcs,
    pcl_reset_type_t    type
)
{
    int                 code;
    static const uint   mask = (   pcl_reset_initial
                                 | pcl_reset_cold
                                 | pcl_reset_printer );

    if ((type & mask) == 0)
        return;

    /* for initial reset, set up the palette store */
    if ((type & pcl_reset_initial) != 0) {
        pl_dict_init(&palette_store, pcs->memory, dict_free_palette);
        pcs->ppalet = 0;
        pcs->pfrgrnd = 0;

        /* set up the built-in render methods and dithers matrices */
        pcl_ht_init_render_methods(pcs, pcs->memory);
        palette_stack = 0;
    }

    /* clear the palette stack and store */
    clear_palette_stack(pcs->memory);
    clear_palette_store(pcs);

    /* select and control palette ID's must be set back to 0 */
    pcs->sel_palette_id = 0;
    pcs->ctrl_palette_id = 0;

    /* build the default palette and foreground */
    (void)build_default_palette(pcs);
    (void)pcl_frgrnd_set_default_foreground(pcs);
}

/*
 * The copy function for palettes.
 *
 * This procedure implements to the two most unusual features of the palette
 * object:
 *
 *     The palette pointer becomes a reference on the palette when the
 *     state is saved.
 *
 *     When the state is restored, the palette in the saved state is used
 *     to re-define the select palette id. in the palette store. Palettes are
 *     the only PCL resource object with this behavior.
 *
 * It is not clear HP intended palette to have this behavior. More likely,
 * it was a by-product of the way in which they implemented the save/restore
 * for foregrounds.
 *
 * Note that, in the restore case, the dictionary definition absorbs the
 * reference to the palette held by the saved state, so there is no need to
 * explicitly release this reference (an example of the asymmetric define/
 * undefine properties of the pl_dict_t object).
 */
  private int
palette_do_copy(
    pcl_state_t *           psaved,
    const pcl_state_t *     pcs,
    pcl_copy_operation_t    operation
)
{
    if ((operation & (pcl_copy_before_call | pcl_copy_before_overlay)) != 0)
        pcl_palette_init_from(psaved->ppalet, pcs->ppalet);
    else if ((operation & pcl_copy_after) != 0) {
        pcl_id_t    key;

        id_set_value(key, psaved->sel_palette_id);
        pl_dict_put(&palette_store, id_key(key), 2, psaved->ppalet);
    }
    return 0;
}

const pcl_init_t    pcl_palette_init = {
    palette_do_init, palette_do_reset, palette_do_copy
};
