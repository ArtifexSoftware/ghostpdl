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

/* xxx.c - code for PCL and GL/2 patterns, including color */

#include "gx.h"
#include "gsuid.h"
#include "gsmatrix.h"
#include "gspcolor.h"
#include "pccid.h"
#include "pcfont.h"
#include "pcpalet.h"
#include "pcfrgrnd.h"
#include "pcht.h"
#include "pcwhtindx.h"
#include "pcpatrn.h"
#include "pcbiptrn.h"
#include "pcuptrn.h"
#include "pcpatxfm.h"



/*
 * The base color space for setting the color white. Unlike all other color
 * spaces in PCL, this uses the DeviceGray color space in the graphic library.
 *
 * A copy of the default halftone is also maintained for setting the color
 * "white". Since the halftone carries the device-specific color lookup tables,
 * a non-standard halftone might end up mapping white to black (definitely
 * undesirable).
 */
private const gs_paint_color    white_paint = {{ 1.0, 0.0, 0.0, 0.0 }};

/* GC routines */
private_st_ccolor_t();


/*
 * Convert a color value specified as a three-element byte array, or an index
 * to a palette, into a gs_paint_color structure.
 */
  private void
convert_color_to_paint(
    const byte *        pcomp,
    gs_paint_color *    ppaint
)
{
    ppaint->values[0] = (float)pcomp[0] / 255.0;
    ppaint->values[1] = (float)pcomp[1] / 255.0;
    ppaint->values[2] = (float)pcomp[2] / 255.0;
    ppaint->values[3] = 0.0;
}

  private void
convert_index_to_paint(
    int                 indx,
    gs_paint_color *    ppaint
)
{
    ppaint->values[0] = indx;
    ppaint->values[1] = 0.0;
    ppaint->values[2] = 0.0;
    ppaint->values[3] = 0.0;
}

/*
 * Set the halftone and color rendering dictionary objects from the
 * current palette. This will check that the palette is complete.
 *
 * Returns 0 on success, < 0 in the event of an error.
 */
  private int
set_ht_crd_from_palette(
    pcl_state_t *       pcs
)
{
    int                 code = 0;
    pcl_cspace_type_t   cstype = pcl_palette_get_cspace(pcs->ppalet);
    pcl_palette_t *     ppalet = 0;

    /* check that the palette is complete */
    pcl_palette_check_complete(pcs);
    ppalet = pcs->ppalet;

    /* install crd and ht */
    code = pcl_ht_set_halftone(&(ppalet->pht), cstype, false, pcs);
    if (code == 0)
        code = pcl_crd_set_crd(&(ppalet->pcrd), pcs);
    return code;
}

/*
 * Set the halftone and color rendering dictionary objects from the current
 * foreground.
 *
 * Returns 0 on success, < 0 in the event of an error.
 */
  private int
set_ht_crd_from_foreground(
    pcl_state_t *       pcs
)
{
    int                 code = 0;
    pcl_frgrnd_t *      pfrgrnd = pcs->pfrgrnd;
    pcl_cspace_type_t   cstype = pcl_frgrnd_get_cspace(pfrgrnd);

    /* install crd and ht */
    code = pcl_ht_set_halftone(&(pfrgrnd->pht), cstype, false, pcs);
    if (code == 0)
        code = pcl_crd_set_crd(&(pfrgrnd->pcrd), pcs);
    return code;
}


/*
 * Free a PCL client color structure.
 */
  private void
free_ccolor(
    gs_memory_t *   pmem,
    void *          pvccolor,
    client_name_t   cname
)
{
    pcl_ccolor_t *  pccolor = (pcl_ccolor_t *)pvccolor;

    pcl_pattern_data_release(pccolor->ppat_data);
    pcl_cs_indexed_release(pccolor->pindexed);
    pcl_cs_base_release(pccolor->pbase);
    if (pccolor->prast != 0)
        gs_free_object(pmem, (void *)pccolor->prast, cname);
    gs_pattern_reference(&(pccolor->ccolor), -1);
    gs_free_object(pmem, pvccolor, cname);
}

/*
 * Make a unique copy of a PCL client color structure. This function also
 * serves as the allocator function.
 *
 * Because the byte array pointed to by the prast structure is not reference
 * counted, the "copy" of the client color created this procedure will always
 * have this pointer set to NULL. This is unfortunate and ugly, as it implies
 * the copy cannot be used immediately as a client color, but it causes no
 * trouble in the current context because:
 *
 *     prast is non-null only for colored patterns
 *     the client color for a colored pattern would only be unshared
 *         if it was about to be rendered, in which case the prast
 *         array would need to be recreated.
 *
 * Newly created colors are solid white.
 */
  private int
unshare_ccolor(
    pcl_ccolor_t ** ppccolor,
    gs_memory_t *   pmem
)
{
    pcl_ccolor_t *  pold = *ppccolor;
    pcl_ccolor_t *  pnew = 0;

    if ((pold != NULL) && (pold->rc.ref_count == 1)) {
        if (pold->prast != 0)
            gs_free_object( pmem,
                            (void *)pold->prast,
                            "unshared PCL client color"
                            );
        pold->prast = 0;
        return 0;
    }
    rc_decrement(pold, "unshare PCL client color object");

    rc_alloc_struct_1( pnew,
                       pcl_ccolor_t,
                       &st_ccolor_t,
                       pmem,
                       return e_Memory,
                       "allocate PCL client color"
                       );
    pnew->rc.free = free_ccolor;
    pnew->prast = 0;

    if (pold != 0) {
        pnew->type = pold->type;
        pcl_pattern_data_init_from(pnew->ppat_data, pold->ppat_data);
        pcl_cs_indexed_init_from(pnew->pindexed, pold->pindexed);
        pcl_cs_base_init_from(pnew->pbase, pold->pbase);
        pnew->ccolor = pold->ccolor;
        gs_pattern_reference(&(pnew->ccolor), 1);
    } else {
        pnew->type = pcl_ccolor_unpatterned;
        pnew->ppat_data = 0;
        pnew->pindexed = 0;

        /* set the color space to pure white */
        pnew->pbase = 0;
        (void)pcl_cs_base_build_white_cspace(&(pnew->pbase), pmem);
        pnew->ccolor.paint = white_paint;
        pnew->ccolor.pattern = 0;
    }

    *ppccolor = pnew;
    return 0;
}

/*
 * Set a solid color (unpattered) color. This is handled separately from 
 * patterns as it will usually not be necessary to build a PCL client color
 * structure in this case.
 *
 * Exactly one of the pair of operands pbase and pindex shoud be non-null.
 */
  private int
set_unpatterned_color(
    pcl_state_t *           pcs,
    pcl_cs_indexed_t *      pindexed,
    pcl_cs_base_t *         pbase,
    const gs_paint_color *  ppaint
)
{
    pcl_ccolor_t *          pcur = pcs->pids->pccolor;
    pcl_ccolor_type_t       type = (pcur != 0 ? pcur->type
                                              : pcl_ccolor_unpatterned);
    int                     code = 0;

    if ( (pcur != 0)                                         &&
         (type == pcl_ccolor_unpatterned)                    &&
         (pcur->pindexed == pindexed)                        &&
         (pcur->pbase == pbase)                              &&
         (pcur->ccolor.paint.values[0] == ppaint->values[0]) &&
         (pcur->ccolor.paint.values[1] == ppaint->values[1]) &&
         (pcur->ccolor.paint.values[2] == ppaint->values[2])   )
        return 0;

    if ( (code = unshare_ccolor(&(pcs->pids->pccolor), pcs->memory)) < 0 )
        return code;
    pcur = pcs->pids->pccolor;

    pcur->type = pcl_ccolor_unpatterned;
    if (pcur->ppat_data != 0) {
        pcl_pattern_data_release(pcur->ppat_data);
        pcur->ppat_data = 0;
    }

    if (pindexed != 0) {
        if ((type != pcl_ccolor_unpatterned) || (pcur->pindexed != pindexed))
             code = pcl_cs_indexed_install(&pindexed, pcs);
    } else {    /* pbase != 0 */
        if ((type != pcl_ccolor_unpatterned) || (pcur->pbase != pbase))
            code = pcl_cs_base_install(&pbase, pcs);
    }
    if (code < 0)
        return code;
    pcl_cs_indexed_copy_from(pcur->pindexed, pindexed);
    pcl_cs_base_copy_from(pcur->pbase, pbase);

    gs_pattern_reference(&(pcur->ccolor), -1);
    pcur->ccolor.pattern = 0;
    pcur->ccolor.paint = *ppaint;
    return gs_setcolor(pcs->pgs, &(pcur->ccolor));
}

/*
 * Set a patterned color space. Note that this is a substantially different
 * operation from setting a solid (unpatterned) color.
 */
  private int
set_patterned_color(
    pcl_state_t *       pcs,
    pcl_ccolor_t *      pnew
)
{
    pcl_ccolor_t *      pcur = pcs->pids->pccolor;
    int                 code = 0;

    /* check if already set */
    if (pcur == pnew)
        return 0;

    /* set a base color space for the pattern, if necessary */
    if (pnew->type == pcl_ccolor_mask_pattern) {

        if ( (pnew->pindexed != 0)                              &&
             ((pcur == 0) || (pcur->pindexed != pnew->pindexed))  )
            code = pcl_cs_indexed_install(&(pnew->pindexed), pcs);
             
        if ( (pnew->pbase != 0)                           &&
             ((pcur == 0) || (pcur->pbase != pnew->pbase))  )
            code = pcl_cs_base_install(&(pnew->pbase), pcs);
    }
    if (code < 0)
        return code;

    /* set the pattern instance */
    if ((code = gs_setpattern(pcs->pgs, &(pnew->ccolor))) >= 0)
        pcl_ccolor_copy_from(pcs->pids->pccolor, pnew);
    return code;
}

/*
 * Check if the pattern pointed to by pptrn has a rendering and, if so, whether
 * or not that rendering is appropriate for the curren environment.
 *
 * If the rendering is appropriate, install the conrresponding color; otherwise
 * return false.
 *
 * The case of GL uncolored patterns rendered with patterns opaque is unique.
 * In this case the pattern must be generated as a colored pattern (mask
 * patterns are always transparent), with a specially generated 2-entry
 * indexed color space. The cache_id field identifies the palette that gave
 * rise to this pattern, but not the entry from that palette that was used
 * as the foreground color. Hence, for this case alone there must be a match
 * on the pen number key. To avoid the need for a much qualified check in this
 * case, we use the convention that the pen number field is zero in all but
 * this case.
 *
 * Note that a difference in the paint color does not require re-rendering;
 * these fields are ignored for colored patterns, and for mask patterns changes
 * only require another call to gs_setpattern.
 */
  private bool
check_pattern_rendering(
    pcl_state_t *           pcs,
    pcl_pattern_t *         pptrn,
    bool                    use_frgrnd,     /* else use palette */
    uint                    pen_num,
    bool                    colored,
    const gs_paint_color *  ppaint
)
{
    pcl_ccolor_t *          pccolor = 0;
    bool                    transp = pcs->pattern_transparent;

    /* check the common parameters first */
    if ( (pptrn->orient != pcs->pat_orient)          ||
         (pptrn->ref_pt.x != pcs->pat_ref_pt.x)      ||
         (pptrn->ref_pt.y != pcs->pat_ref_pt.y)        )
        return false;

    if (colored) {
        pcl_gsid_t  cache_id = ( use_frgrnd ? pcs->pfrgrnd->id
                                            : pcs->ppalet->id  );

        /* check that there is a rendering in the appropriate environment */
        if ( ((pccolor = pptrn->pcol_ccolor) == 0)       ||
             (pptrn->transp != pcs->pattern_transparent) ||
             (pptrn->cache_id != cache_id)               ||
             (pptrn->pen != pen_num)                       )
            return false;

    } else {    /* mask pattern */
        pcl_cs_indexed_t *  pindexed = (use_frgrnd ? 0 : pcs->ppalet->pindexed);
        pcl_cs_base_t *     pbase = (use_frgrnd ? pcs->pfrgrnd->pbase : 0);

        /* check if there is a rendering */
        if ((pccolor = pptrn->pmask_ccolor) == 0)
            return false;

        /* handle changes in the "foreground" color or color space */
        if ( (pccolor->ccolor.paint.values[0] != ppaint->values[0]) ||
             (pccolor->ccolor.paint.values[1] != ppaint->values[1]) ||
             (pccolor->ccolor.paint.values[2] != ppaint->values[2]) ||
             (pccolor->pindexed != pindexed)                        ||
             (pccolor->pbase != pbase)                                ) {

            /* get a unique copy, and update the painting information */
            if (unshare_ccolor(&(pptrn->pmask_ccolor), pcs->memory) < 0)
                return false;
            pccolor = pptrn->pmask_ccolor;

            pcl_cs_indexed_copy_from(pccolor->pindexed, pindexed);
            pcl_cs_base_copy_from(pccolor->pbase, pbase);
            pccolor->ccolor.paint = *ppaint;
        }
    }

    return (set_patterned_color(pcs, pccolor) == 0);
}

/*
 * Render and set a pattern.
 *
 * In a somewhat unfortunate division of labor, this code sets some but not
 * all of the cache keys in the pattern. The fields set are transparency,
 * orientation, and reference point. The caller is responsible for setting
 * the cache_id and pen number.
 *
 * If this routine is called, it may be assumed that the pattern needs to be
 * rendered.
 *
 */
  private int
render_pattern(
    pcl_state_t *           pcs,
    pcl_pattern_t *         pptrn,
    pcl_ccolor_type_t       type,
    pcl_cs_indexed_t *      pindexed,
    pcl_cs_base_t *         pbase,
    const gs_paint_color *  ppaint,
    int                     wht_indx,
    bool                    remap
)
{
    int                     code = 0;
    pcl_ccolor_t *          pccolor = 0;
    int                     wht_index = 0;
    gs_color_space *        pcspace;
    gs_matrix               mat;
    gs_depth_bitmap         pixinfo;

    /*
     * If the orientation or reference point has changed, discard both
     * renderings. Otherwise, just discard the one being modified.
     *
     * Note that the unshare function doubles as an allocator.
     */
    if ( (pptrn->orient != pcs->pat_orient)          ||
         (pptrn->ref_pt.x != pcs->pat_ref_pt.x)      ||
         (pptrn->ref_pt.y != pcs->pat_ref_pt.y)        ) {
        if (type == pcl_ccolor_mask_pattern) {
            pcl_ccolor_release(pptrn->pcol_ccolor);
            pptrn->pcol_ccolor = 0;
        } else {    /* type == pcl_ccolor_colored_pattern */
            pcl_ccolor_release(pptrn->pmask_ccolor);
            pptrn->pmask_ccolor = 0;
        }
    } 

    /* un-share, or allocate, the appropriate client color */
    if (type == pcl_ccolor_mask_pattern) {
        code = unshare_ccolor(&(pptrn->pmask_ccolor), pcs->memory);
        pccolor = pptrn->pmask_ccolor;
        pcspace = 0;
    } else {    /* type == pcl_ccolor_colored_pattern */
        code = unshare_ccolor(&(pptrn->pcol_ccolor), pcs->memory);
        pccolor = pptrn->pcol_ccolor;
        pcspace = pindexed->pcspace;
        pptrn->transp = pcs->pattern_transparent;
    }
    if (code < 0)
        return code;

    /* discard the existing pattern instance */
    gs_pattern_reference(&(pccolor->ccolor), -1);
    pccolor->ccolor.pattern = 0;

    /* initialize the pattern data, if the client color is newly allocated */
    pccolor->type = type;
    pcl_pattern_data_copy_from(pccolor->ppat_data, pptrn->ppat_data);

    /* set up the transformation and transparency information */
    pcl_xfm_get_pat_xfm(pcs, pptrn, &mat);

    /* set up the white index and remap as necessary */
    if (remap) {
        code = pcl_cmap_map_raster( pindexed,
                                    &wht_indx,
                                    &(pptrn->ppat_data->pixinfo),
                                    &pixinfo,
                                    true,
                                    pcs->memory
                                    );
        if (code < 0)
            return code;
        pcspace = pindexed->pcspace;
        if (pixinfo.data != pptrn->ppat_data->pixinfo.data)
            pccolor->prast = pixinfo.data;
    } else
        pixinfo = pptrn->ppat_data->pixinfo;

    /* render the pattern */
    code = gs_makepixmappattern( &(pccolor->ccolor),
                                 &pixinfo,
                                 (pcspace == 0),
                                 &mat,
                                 no_UniqueID,
                                 pcspace,
                                 wht_indx,
                                 pcs->pgs,
                                 pcs->memory
                                 );

    /* if all is OK, install the pattern; otherwise clear the pattern */
    if (code >= 0) {
        pcl_cs_indexed_copy_from(pccolor->pindexed, pindexed);
        pcl_cs_base_copy_from(pccolor->pbase, pbase);
        if (type == pcl_ccolor_mask_pattern)
            pccolor->ccolor.paint = *ppaint;
        code = set_patterned_color(pcs, pccolor);
    }
    return code;
}

/*
 * Set a pattern using the foreground parameters.
 *
 * The for-image boolean indicates if the foreground is being set to render
 * a PCL raster. If so, and if the halftone/CRD combination for the foreground
 * is not the same as for the palette, a colored pattern must be generated.
 * This is because the CRD/halftone combination for the palette will be
 * current at the time the pattern is loaded into the cache, and thus the
 * color would be evaluated in the wrong context.
 *
 * Returns 0 on success, < 0 in the event of an error.
 */
  private int
set_frgrnd_pattern(
    pcl_state_t *       pcs,
    pcl_pattern_t *     pptrn,
    bool                for_image
)
{
    pcl_frgrnd_t *      pfrgrnd = pcs->pfrgrnd;
    pcl_cs_base_t *     pbase = pfrgrnd->pbase;
    pcl_cs_indexed_t *  pindexed = 0;
    pcl_ccolor_type_t   type = pcl_ccolor_mask_pattern;
    gs_paint_color      paint;
    bool                colored = false;
    int                 code = set_ht_crd_from_foreground(pcs);
    int                 wht_indx = (pcs->pattern_transparent ? 0 : 2);

    if (code < 0)
        return code;

    if ( (pfrgrnd->pht == pcs->ppalet->pht)  &&
         (pfrgrnd->pcrd == pcs->ppalet->pcrd)  )
        for_image = false;
    colored = (for_image || !pcs->pattern_transparent);

    convert_color_to_paint(pfrgrnd->color, &paint);
    if (check_pattern_rendering(pcs, pptrn, true, 0, colored, &paint))
        return 0;

    /* build the two-entry palette if necessary */
    if (colored) {
        code = pcl_cs_indexed_build_special( &pindexed,
                                             pbase,
                                             pfrgrnd->color,
                                             pcs->memory
                                             );
        if (code < 0)
            return code;
        pbase = 0;
        type = pcl_ccolor_colored_pattern;
    }

    code = render_pattern( pcs,
                           pptrn,
                           type,
                           pindexed,
                           pbase,
                           &paint,
                           wht_indx,
                           false
                           );

    /* release the extra reference to the indexed color space */
    if (colored) {
        pcl_cs_indexed_release(pindexed);
        if (code >= 0) {
            pptrn->pen = 0;
            pptrn->cache_id = pfrgrnd->id;
        }
    }

    return code;
}

/*
 * Set an uncolored pattern using the palette parameters and indicated pen
 * number.
 *
 * Returns 0 on success, < 0 in the event of an error.
 */
  private int
set_uncolored_palette_pattern(
    pcl_state_t *       pcs,
    pcl_pattern_t *     pptrn,
    int                 pen
)
{
    pcl_cs_indexed_t *  pindexed = pcs->ppalet->pindexed;
    pcl_ccolor_type_t   type = pcl_ccolor_mask_pattern;
    gs_paint_color      paint;
    bool                colored = !pcs->pattern_transparent;
    int                 code = set_ht_crd_from_palette(pcs);

    if (code < 0)
        return code;

    convert_index_to_paint(pen, &paint);
    if (check_pattern_rendering(pcs, pptrn, false, pen, colored, &paint))
        return 0;

    /* build the two-entry palette if necessary */
    if (colored) {
        code = pcl_cs_indexed_build_special( &pindexed,
                                             pindexed->pbase,
                                             &(pindexed->palette.data[3 * pen]),
                                             pcs->memory
                                             );
        if (code < 0)
            return code;
        type = pcl_ccolor_colored_pattern;
    }

    code = render_pattern(pcs, pptrn, type, pindexed, NULL, &paint, 2, false);

    /* release the extra reference to the indexed color space */
    if (colored) {
        pcl_cs_indexed_release(pindexed);
        if (code >= 0) {
            pptrn->pen = pen;
            pptrn->cache_id = pcs->ppalet->id;
        }
    }

    return code;
}

/*
 * Set a colored pattern.
 *
 * Returns 0 on success, < 0 in the event of an error.
 */
  private int
set_colored_pattern(
    pcl_state_t *       pcs,
    pcl_pattern_t *     pptrn
)
{
    pcl_cs_indexed_t *  pindexed = pcs->ppalet->pindexed;
    pcl_gsid_t          cache_id = pcs->ppalet->id;
    bool                remap = pcs->pattern_transparent;
    int                 code = set_ht_crd_from_palette(pcs);

    if (code < 0)
        return code;

    if (check_pattern_rendering(pcs, pptrn, false, 0, true, NULL))
        return 0;

    code = render_pattern( pcs,
                           pptrn,
                           pcl_ccolor_colored_pattern,
                           pindexed,
                           NULL,
                           &white_paint,
                           (remap ? 0 : pindexed->num_entries),
                           remap
                           );
    if (code >= 0) {
        pptrn->pen = 0;
        pptrn->cache_id = cache_id;
    }

    return code;
}

/*
 * Routines to set the current color space to a specific pattern. Since patterns
 * have different sources, each with its own system of identifiers, there are
 * several such routines. Each routine takes the current PCL state (including
 * the GL/2 state) as an operand; the additional operands, if any, vary by
 * routine type.
 *
 *    pattern_set_white     No additional operands. This routine
 *                          will set the color space to DeviceRGB, the
 *                          color to white, but will not override the
 *                          source or pattern transparency. A special
 *                          halftone with a null transfer function is used,
 *                          to avoid possible re-mapping of white to some
 *                          other color.
 *
 *    pattern_set_pen       One additional operand, the pen number. Uses
 *                          the indexed color space from the current palette
 *                          as the color space, and the pen number as the
 *                          color. The halftone and color rendering
 *                          dictionaries are taken from the current
 *                          palette.
 *
 *                          Note that this routine does NOT set the current
 *                          line width (it does not have sufficient
 *                          information to convert the dimensions).
 *                              
 *    pattern_set_frgrnd    One additional operands. Sets currect base
 *                          color space as the color space, and the
 *                          foreground color as the current color.
 *                          The third (NOT second) operand indicates if the
 *                          foreground is being set to render a PCL raster.
 *                          The latter case involves special considerations;
 *                          see the paragraphs at the bottom of this comment
 *                          for additional information.
 *
 *    pattern_set_shade_pcl
 *                          Two additional operands, the shade indensity
 *                          value and whether or not the shade pattern is
 *                          being set to render a PCL raster.
 *
 *                          This procedure is intended to be used by PCL.
 *                          It generates the shade as a colored pattern from
 *                          a two-element color palette consisting of the
 *                          canonical white for the current base color space
 *                          and the foreground color. The color space in the
 *                          graphic state is set to Pattern color space and
 *                          the current color is set to be the generated
 *                          pattern. The halftone and color rendering dictionary
 *                          objects are also taken from the current
 *                          foreground.
 *
 *                          Special considerations apply if the pattern is
 *                          being set to render a PCL raster. See the 
 *                          paragraphs at the bottom of this comment for
 *                          additional information.
 *
 *    pattern_set_shade_gl  Two additional operands; the first provides
 *                          the pattern intensity, the latter the pen 
 *                          number to be be used as the foreground color.
 *                          This routine generates the pattern using a two
 *                          entry palette consisting of canonical white in
 *                          the current base color space, and the operand
 *                          pen color from the current palette as the
 *                          second. The color space in the graphic state
 *                          is set to Pattern color space and the current
 *                          color is set to the generated pattern.  The
 *                          halftone and color rendering dictionary objects
 *                          are taken from the current palette.
 *
 *    pattern_set_hatch_pcl
 *                          Two additional operands, the index of the cross
 *                          hatch pattern and an indication of whether or not
 *                          the pattern is being set to render a PCL raster.
 *
 *                          Similar to pcl_pattern_set_shade_pcl, but for
 *                          cross-hatch patterns.
 *
 *    pattern_set_hatch_gl  Two additional operands; the first provides
 *                          the index of the cross hatch pattern, the
 *                          latter the pen number to be used as the
 *                          foreground color. Similar to
 *                          pcl_pattern_set_shade_gl, but for cross hatch
 *                          patterns.
 *
 *    pattern_set_user_pcl
 *                          Two additional operands, the user pattern id. and
 *                          and indicator of whether or not the pattern is being
 *                          set to render a PCL raster.
 *
 *                          Handling is based on whether the pattern is
 *                          colored or uncolored.
 *
 *                          For uncolored patterns, handling is similar
 *                          to pcl_pattern_set_shade_pcl. The special
 *                          consideration for rasters apply only in this case.
 *
 *                          For colored patterns, the pattern is generated
 *                          using the indexed color space in the current
 *                          color palette. The color space in the graphic
 *                          state is set to Pattern color space, the current
 *                          color is set to the generated pattern, and the
 *                          halftone and color rendering dictionary objects
 *                          are taken from the current palette.
 *
 *    pattern_set_user_gl   Two additional operands. The first provides the
 *                          PCL user-defined pattern id. The second is the
 *                          current pen. Handling is base on whether the
 *                          pattern is colored or uncolored.
 *
 *                          For uncolored patterns, handling is similar
 *                          to pcl_pattern_set_shade_gl.
 *
 *                          For colored patterns, handling is similar to
 *                          pcl_pattern_set_user_pcl.
 *
 *    pattern_set_gl_RF     One additional operand, the index of the
 *                          GL/2 user defined pattern, created with the
 *                          RF command. Handling is similar to
 *                          pcl_pattern_set_user_pcl for colored patterns.
 *
 * All procedures return 0 on success, < 0 in the event of an error.
 *
 * If that did not seem to make things complex enough, some additional
 * complications arise for PCL rasters. This is because PCL rasters are
 * themselves colored objects, and thus require a CRD and a halftone to
 * render. In PCL, rasters may also interact with the current color, which
 * may be using a different CRD and halftone.
 *
 * The CRD and halftone to be used with rasters are taken from the current
 * PCL palette. If the current pattern is either solid foreground (the most
 * typical case) or an uncolored pattern, the current "texture" is generated
 * using the CRD and halftone from the PCL foreground. The two may not be the
 * same, and unfortunately there is room for only one of each in the graphic
 * state.
 *
 * The solution is to make use of an additional graphic state. Patterns in the
 * graphic library have their own graphic state. Hence, but converting a solid
 * foreground into an uncolored pattern which happens to be "on" everywhere, it
 * is possible to achieve the desired result. For performance reasons, this
 * should be done only when necessary.
 *
 * Still not handled correctly is the case in which the current pattern type
 * is solid white, and a raster is being generated. This case is, however, so
 * rare as to not be of concern.
 */

  private int
pattern_set_white(
    pcl_state_t *   pcs,
    int             arg1,   /* ignored */
    int             arg2    /* ignored */
)
{
    int             code = set_ht_crd_from_foreground(pcs);
    pcl_cs_base_t * pwhite_cs = 0;
    pcl_ht_t *      pdflt_ht = 0;

    if (code < 0)
        return code;

    /* build the pure white color space and default halftone if necessary */
    if ((code = pcl_cs_base_build_white_cspace(&pwhite_cs, pcs->memory)) >= 0)
        code = pcl_ht_build_default_ht(&pdflt_ht, pcs->memory);

    /* set the halftone and color space */
    if (code >= 0)
        code = pcl_ht_set_halftone(&pdflt_ht, pcl_cspace_RGB, false, pcs);
    pcl_ht_release(pdflt_ht);
    if (code >= 0)
        code = set_unpatterned_color(pcs, NULL, pwhite_cs, &white_paint);
    pcl_cs_base_release(pwhite_cs);
    return code;
}

  private int
pattern_set_pen(
    pcl_state_t *       pcs,
    int                 pen,
    int                 arg2    /* ignored */
)
{
    pcl_cs_indexed_t *  pindexed = pcs->ppalet->pindexed;
    int                 num_entries = pindexed->num_entries;
    int                 code = 0;

    /* put the pen number in the proper range */
    if (pen >= num_entries)
        pen = (pen % (num_entries - 1)) + 1;

    /* set halftone and crd from the palette */
    code = set_ht_crd_from_palette(pcs);

    if (code >= 0) {
        gs_paint_color  paint;

        convert_index_to_paint(pen, &paint);
        code = set_unpatterned_color(pcs, pindexed, NULL, &paint);
    }
    return code;
}

  private int
pattern_set_frgrnd(
    pcl_state_t *   pcs,
    int             arg1,   /* ignored */
    int             for_image
)
{
    pcl_frgrnd_t *  pfrgrnd = pcs->pfrgrnd;
    pcl_palette_t * ppalet = pcs->ppalet;
    int             code = set_ht_crd_from_foreground(pcs);

    if (code < 0)
        return code;

    /* check if a solid pattern should be substituted */
    if ( for_image                          &&
         ((pfrgrnd->pht != ppalet->pht)  ||
          (pfrgrnd->pcrd != ppalet->pcrd)  )  ) {
        code = set_frgrnd_pattern(pcs, pcl_pattern_get_solid_pattern(), true);
        if (code >= 0)
            code = set_ht_crd_from_palette(pcs);
    } else {
        gs_paint_color  paint;

        convert_color_to_paint(pfrgrnd->color, &paint);
        code = set_unpatterned_color(pcs, NULL, pfrgrnd->pbase, &paint);
    }

    return code;
}

  private int
pattern_set_shade_pcl(
    pcl_state_t *   pcs,
    int             inten,  /* intensity value */
    int             for_image
)
{
    pcl_pattern_t * pptrn = pcl_pattern_get_shade(inten);

    if (pptrn == 0)
        return ( inten > 0 ? pattern_set_frgrnd(pcs, 0, for_image)
                            : pattern_set_white(pcs, 0, 0) );
    else {
        int     code = 0;

        pcl_xfm_pcl_set_pat_ref_pt(pcs);
        code = set_frgrnd_pattern(pcs, pptrn, for_image);
    
        if (for_image && (code >= 0))
            code = set_ht_crd_from_palette(pcs);
        return code;
    }
}

  private int
pattern_set_shade_gl(
    pcl_state_t *   pcs,
    int             inten,  /* intensity value */
    int             pen     /* pen number for foreground */
)
{
    pcl_pattern_t * pptrn = pcl_pattern_get_shade(inten);

    if (pptrn == 0)
        return ( inten <= 0 ? pattern_set_pen(pcs, pen, 0)
                            : pattern_set_white(pcs, 0, 0)   );
    else {
        pcl_xfm_gl_set_pat_ref_pt(pcs);
        return set_uncolored_palette_pattern(pcs, pptrn, pen);
    }
}

  private int
pattern_set_hatch_pcl(
    pcl_state_t *   pcs,
    int             indx,   /* cross-hatch pattern index */
    int             for_image
)
{
    pcl_pattern_t * pptrn = pcl_pattern_get_cross(indx);

    if (pptrn == 0)
        return pattern_set_frgrnd(pcs, 0, for_image);
    else {
        int     code = 0;

        pcl_xfm_pcl_set_pat_ref_pt(pcs);
        code = set_frgrnd_pattern(pcs, pptrn, for_image);
        if (for_image && (code >= 0))
            code = set_ht_crd_from_palette(pcs);
        return code;
    }
}

  private int
pattern_set_hatch_gl(
    pcl_state_t *   pcs,
    int             indx,   /* cross-hatch pattern index */
    int             pen     /* pen number for foreground */
)
{
    pcl_pattern_t * pptrn = pcl_pattern_get_cross(indx);

    if (pptrn == 0)
        return pattern_set_pen(pcs, pen, 0);
    else {
        pcl_xfm_gl_set_pat_ref_pt(pcs);
        return set_uncolored_palette_pattern(pcs, pptrn, pen);
    }
}

  private int
pattern_set_user_pcl(
    pcl_state_t *   pcs,
    int             id,     /* pattern id. */
    int             for_image
)
{
    pcl_pattern_t * pptrn = pcl_pattern_get_pcl_uptrn(id);

    if (pptrn == 0)
        return pattern_set_frgrnd(pcs, 0, for_image);
    else {
        pcl_xfm_pcl_set_pat_ref_pt(pcs);
        if (pptrn->ppat_data->type == pcl_pattern_uncolored) {
            int     code = set_frgrnd_pattern(pcs, pptrn, for_image);

            if (for_image && (code >= 0))
                code = set_ht_crd_from_palette(pcs);
            return code;
        } else
            return set_colored_pattern(pcs, pptrn);
    }
}

  private int
pattern_set_user_gl(
    pcl_state_t *   pcs,
    int             id,     /* pattern id. */
    int             pen     /* pen to use for foreground */
)
{
    pcl_pattern_t * pptrn = pcl_pattern_get_pcl_uptrn(id);

    if (pptrn == 0)
        return pattern_set_pen(pcs, 0, 0);
    else {
        pcl_xfm_gl_set_pat_ref_pt(pcs);
        if (pptrn->ppat_data->type == pcl_pattern_uncolored)
            return set_uncolored_palette_pattern(pcs, pptrn, pen);
        else
            return set_colored_pattern(pcs, pptrn);
    }
}

  private int
pattern_set_gl_RF(
    pcl_state_t *   pcs,
    int             indx,   /* GL/2 RF pattern index */
    int             arg2    /* ignored */
)
{
    pcl_pattern_t * pptrn = pcl_pattern_get_gl_uptrn(indx);

    if (pptrn == 0)
        return pattern_set_pen(pcs, 0, 0);
    else {
        pcl_xfm_gl_set_pat_ref_pt(pcs);
        return set_colored_pattern(pcs, pptrn);
    }
}


/*
 * Return the appropriate "set" procedure, given a PCL pattern type.
 */
  pcl_pattern_set_proc_t
pcl_pattern_get_proc_PCL(
    pcl_pattern_source_t                    pattern_source
)
{
    static  const pcl_pattern_set_proc_t    procs[] = { pattern_set_frgrnd,
                                                        pattern_set_white,
                                                        pattern_set_shade_pcl,
                                                        pattern_set_hatch_pcl,
                                                        pattern_set_user_pcl,
                                                        0,
                                                        pattern_set_pen };

    return procs[(int)pattern_source];
}

/*
 * Returen the appropriate "set" procedure, given a GL fill type specification.
 */
  pcl_pattern_set_proc_t
pcl_pattern_get_proc_FT(
    hpgl_FT_pattern_source_t pattern_source
)
{
    if ( (pattern_source == hpgl_FT_pattern_solid_pen1) ||
         (pattern_source == hpgl_FT_pattern_solid_pen2)   )
        return pattern_set_pen;
    else if (pattern_source == hpgl_FT_pattern_shading)
        return pattern_set_shade_gl;
    else if (pattern_source == hpgl_FT_pattern_RF)
        return pattern_set_gl_RF;
    else if (pattern_source == hpgl_FT_pattern_cross_hatch)
        return pattern_set_hatch_gl;
    else if (pattern_source == hpgl_FT_pattern_user_defined)
        return pattern_set_user_gl;
    else
        return 0;
}

/*
 * Returen the appropriate "set" procedure, given a GL screened vector
 * specification.
 */
  pcl_pattern_set_proc_t
pcl_pattern_get_proc_SV(
    hpgl_FT_pattern_source_t pattern_source
)
{
    if (pattern_source == hpgl_SV_pattern_solid_pen)
        return pattern_set_pen;
    else if (pattern_source == hpgl_SV_pattern_shade)
        return pattern_set_shade_gl;
    else if (pattern_source == hpgl_SV_pattern_RF)
        return pattern_set_gl_RF;
    else if (pattern_source == hpgl_SV_pattern_cross_hatch)
        return pattern_set_hatch_gl;
    else if (pattern_source == hpgl_SV_pattern_user_defined)
        return pattern_set_user_gl;
    else
        return 0;
}


/*
 * ESC * c <#/id> G
 *
 * Set the pattern id.
 */
  private int
set_pattern_id(
    pcl_args_t *    pargs,
    pcl_state_t *   pcs
)
{
    pcs->pattern_id = int_arg(pargs);
    return 0;
}

/*
 * ESC * v <bool> N
 *
 * Set source transparency mode
 */
  private int
set_source_transparency_mode(
    pcl_args_t *    pargs,
    pcl_state_t *   pcs
)
{
    uint            i = uint_arg(pargs);

    if (i <= 1) {
        pcl_break_underline(pcs);
        pcs->source_transparent = (i == 0);
    }
    return 0;
}

/*
 * ESC * v <bool> O
 *
 * Set pattern transparency mode.
 */
  private int
set_pattern_transparency_mode(
    pcl_args_t *    pargs,
    pcl_state_t *   pcs
)
{
    uint            i = uint_arg(pargs);

    if (i <= 1) {
        pcl_break_underline(pcs);
        pcs->pattern_transparent = (i == 0);
    }
    return 0;
}

/*
 * ESC * v # T
 *
 * Set current pattern id.
 */
  private int
select_current_pattern(
    pcl_args_t *    pargs,
    pcl_state_t *   pcs
)
{
    uint            i = uint_arg(pargs);

    if (i <= (int)pcl_pattern_user_defined) {
        pcl_break_underline(pcs);
        pcs->current_pattern_id = pcs->pattern_id;
        pcs->pattern_type = (pcl_pattern_source_t)i;
    }
    return 0;
}

/*
 * ESC * o # W
 *
 * Driver configuration command. Currently just a stub that absorbs the data.
 */
  private int
set_driver_configuration(
    pcl_args_t *    pargs,  /* ignored */
    pcl_state_t *   pcs     /* ignored */
)
{
    return 0;
}


/*
 * Initialization and reset routines.
 */
  private int
pattern_do_init(
    gs_memory_t *   pmem
)
{
    DEFINE_CLASS('*')
    {
        'c', 'G',
        PCL_COMMAND( "Pattern ID",
                     set_pattern_id,
                     pca_neg_ignore | pca_big_ignore
                     )
    },
    {
        'v', 'N',
        PCL_COMMAND( "Source Transparency Mode",
                     set_source_transparency_mode,
                     pca_neg_ignore | pca_big_ignore
                     )
    },
    {
        'v', 'O',
        PCL_COMMAND( "Pattern Transparency Mode",
                     set_pattern_transparency_mode,
                     pca_neg_ignore | pca_big_ignore
                     )
    },
    {
        'v', 'T',
        PCL_COMMAND( "Select Current Pattern",
                     select_current_pattern,
                     pca_neg_ignore | pca_big_ignore
                     )
    },
    {
        'o', 'W',
        PCL_COMMAND( "Driver Configuration Command",
                     set_driver_configuration,
                     pca_bytes
                     )
    },
    END_CLASS
    return 0;
 
}

  private void
pattern_do_reset(
    pcl_state_t *       pcs,
    pcl_reset_type_t    type
)
{
    static const uint   mask = (   pcl_reset_initial
                                 | pcl_reset_cold
                                 | pcl_reset_printer );

    if ((type & mask) != 0) {
        if ((type  & pcl_reset_initial) != 0)
            pcl_pattern_init_bi_patterns(pcs->memory);
        pcs->pattern_transparent = true;
        pcs->source_transparent = true;
        pcs->pattern_id = 0;
        pcs->current_pattern_id = 0;
        pcs->pattern_type = pcl_pattern_solid_frgrnd;
    }
}

const pcl_init_t    pcl_pattern_init = { pattern_do_init, pattern_do_reset, 0 };
    
