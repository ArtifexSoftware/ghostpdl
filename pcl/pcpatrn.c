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

/* pcpatrn.c - code for PCL and GL/2 patterns, including color */

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
 * Allocate a 6-byte string for use with a 2-entry palette. This is needed
 * because "uncolored" PCL patterns must be rendered as colored patterns in
 * the graphic library so than may be make opaque when necessary.
 *
 * The first entry of the palette, which consists of the first three bytes
 * of the string, is always set to { 255, 255, 255 }, which is canonical
 * white in all color spaces. This may, of course, not actually be white,
 * due to the use of lookup tables, but currently we can't deal with that
 * problem.
 *
 * Returns 0 on success, < 0 in the event of an error.
 */
  private int
alloc_palette(
    pcl_pattern_t * pptrn,
    gs_memory_t *   pmem
)
{
    pptrn->palette.data = gs_alloc_string(pmem, 6, "allocate 2-entry palette");

    if (pptrn->palette.data == 0)
        return e_Memory;

    pptrn->palette.size = 6;
    pptrn->palette.data[0] = 255;
    pptrn->palette.data[1] = 255;
    pptrn->palette.data[2] = 255;
    return 0;
}

/*
 * Install a pattern in the graphic state, if necessary. The pattern is assumed
 * have been properly rendered by the time this procedure is called.
 *
 * Returns 0 on success, < 0 in the event of an error.
 */
  private int
install_pattern(
    pcl_state_t *   pcs,
    pcl_pattern_t * pptrn
)
{
    if (pptrn->ccolor_id == pcs->ids.pattern_id)
        return 0;
    else {
        int     code = gs_setpattern(pcs->pgs, &(pptrn->ccolor));

        if (code >= 0) {
            pcs->ids.pattern_id = pptrn->ccolor_id;
            pcs->ids.cspace_id = 0L;
            code = 0;
        }
        return code;
    }
}

/*
 * Check if a pattern has been rendered in the current environment. The second
 * and third operands  are the pen number and cache_id to be checked against,
 * respectively.
 *
 * If the pattern has appropriately rendered, install it if it is not already
 * installed in the graphic state.
 */
  private bool
check_pattern_rendered(
    pcl_state_t *   pcs,
    pcl_pattern_t * pptrn,
    pcl_gsid_t      cache_id,
    int             pen_num
)
{
    if ( (pptrn->orient != pcs->pat_orient)     ||
         (pptrn->pen_num != pen_num)            ||
         (pptrn->ref_pt.x != pcs->pat_ref_pt.x) ||
         (pptrn->ref_pt.y != pcs->pat_ref_pt.y) ||
         (pptrn->cache_id != cache_id)            )
        return false;
    else
        return (install_pattern(pcs, pptrn) == 0);
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
 * Render a PCL uncolored pattern, and install it. If we have gotten this
 * far, assume the pattern needs to be rendered.
 *
 * The operand color space is the base color space to be used for rendering.
 *
 * Returns 0 on success, < 0 in the event of an error.
 */
  private int
render_uncolored_pattern(
    pcl_state_t *       pcs,
    pcl_pattern_t *     pptrn,
    gs_color_space *    pbase_cspace,
    pcl_gsid_t          id,
    int                 pen_num
)
{
    int                 code = 0;
    gs_matrix           mat;

    /* release the existing 2-entry color space, if present */
    if (pptrn->pcspace != 0) {
        gs_cspace_release(pptrn->pcspace);
        gs_free_object( pcs->memory,
                        (void *)pptrn->pcspace,
                        "render uncolored pattern"
                        );
        pptrn->pcspace = 0;
    }

    /* build the 2-entry color space */
    code = gs_cspace_build_Indexed( &(pptrn->pcspace),
                                    pbase_cspace,
                                    2,
                                    (gs_const_string *)&(pptrn->palette),
                                    pcs->memory
                                    );
    if (code < 0)
        return code;

    /* set up the transformation and cache key */
    pcl_xfm_get_pat_xfm(pcs, pptrn, &mat);
    pptrn->pen_num = pen_num;
    pptrn->cache_id = id;
    pptrn->ccolor_id = pcl_next_id();

    /* render the pattern */
    code = gs_makepixmappattern( &(pptrn->ccolor),
                                 &(pptrn->pixinfo),
                                 false,
                                 &mat,
                                 no_UniqueID,
                                 pptrn->pcspace,
                                 0,         /* foreground always opaque */
                                 pcs->pgs,
                                 pcs->memory
                                 );

    /* if all is OK, install the pattern; otherwise clear the pattern */
    if (code >= 0)
        code = install_pattern(pcs, pptrn);
    else
        pptrn->orient = -1;

    return code;
}

/*
 * Set a pattern using the foreground parameters.
 *
 * Returns 0 on success, < 0 in the event of an error.
 */
  private int
set_frgrnd_pattern(
    pcl_state_t *       pcs,
    pcl_pattern_t *     pptrn
)
{
    int                 code = 0;

    if (check_pattern_rendered(pcs, pptrn, pcs->pfrgrnd->id, -1))
        return 0;

    /* if necessary, allocate the 2-entry palette */
    if ( (pptrn->palette.data == 0)                      &&
         ((code = alloc_palette(pptrn, pcs->memory)) < 0)  )
        return code;

    /* the second entry of the palette is the foreground color */
    pptrn->palette.data[3] = pcs->pfrgrnd->color[0];
    pptrn->palette.data[4] = pcs->pfrgrnd->color[1];
    pptrn->palette.data[5] = pcs->pfrgrnd->color[2];

    /* set the current halftone and crd from the foreground */
    set_ht_crd_from_foreground(pcs);

    /* render the uncolored pattern */
    return render_uncolored_pattern( pcs,
                                     pptrn,
                                     pcs->pfrgrnd->pbase->pcspace,
                                     pcs->pfrgrnd->id,
                                     -1
                                     );
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
    int                 pen_num
)
{
    int                 code;

    if (check_pattern_rendered(pcs, pptrn, pcs->ppalet->id, pen_num))
        return 0;

    /* if necessary, allocate the 2-entry palette */
    if ( (pptrn->palette.data == 0)                      &&
         ((code = alloc_palette(pptrn, pcs->memory)) < 0)  )
        return code;

    /* the second entry of the palette is the foreground color */
    pptrn->palette.data[3] = pcs->pfrgrnd->color[pen_num];
    pptrn->palette.data[4] = pcs->pfrgrnd->color[pen_num];
    pptrn->palette.data[5] = pcs->pfrgrnd->color[pen_num];

    /* set the current halftone and crd from the foreground */
    set_ht_crd_from_palette(pcs);

    /* render the uncolored pattern */
    return render_uncolored_pattern( pcs,
                                     pptrn,
                                     pcs->ppalet->pindexed->pbase->pcspace,
                                     pcs->ppalet->id,
                                     pen_num
                                     );
    
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
    int                 code = 0;
    int                 wht_indx;
    gs_matrix           mat;
    gs_depth_bitmap     pixinfo;

    if (check_pattern_rendered(pcs, pptrn, pcs->ppalet->id, -1))
        return 0;

    /* release any copy of the render information, if applicable */
    if (pptrn->prast != 0) {
        gs_free_object( pcs->memory,
                        (void *)pptrn->prast,
                        "render colored pattern"
                        );
        pptrn->prast = 0;
    }

    /* find the white index, modifying the pattern pximap if necessary */
    code = pcl_cmap_map_raster( pcs->ppalet->pindexed,
                                &wht_indx,
                                &(pptrn->pixinfo),
                                &pixinfo,
                                true,
                                pcs->memory
                                );
    if (code < 0)
        return code;
    if (pixinfo.data != pptrn->pixinfo.data)
        pptrn->prast = pixinfo.data;

    /* set up the transformation and cache key */
    pcl_xfm_get_pat_xfm(pcs, pptrn, &mat);
    pptrn->pen_num = -1;
    pptrn->cache_id = pcs->ppalet->id;
    pptrn->ccolor_id = pcl_next_id();

    /* set the current halftone and crd from the foreground */
    set_ht_crd_from_palette(pcs);

    /* render the colored pattern */
    code = gs_makepixmappattern( &(pptrn->ccolor),
                                 &pixinfo,
                                 false,
                                 &mat,
                                 pptrn->ccolor_id,
                                 pcs->ppalet->pindexed->pcspace,
                                 wht_indx,
                                 pcs->pgs,
                                 pcs->memory
                                 );

    if (code < 0)
        pptrn->orient = -1;
    else
        code = install_pattern(pcs, pptrn);
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
 */
  private int
pattern_set_white(
    pcl_state_t *                   pcs,
    int                             arg1,   /* ignored */
    int                             arg2    /* ignored */
)
{
    static gs_color_space *         pdevgray;
    static const gs_client_color    white = { { { 1.0, 0.0, 0.0, 0.0 } }, 0 };
    static pcl_ht_t *               pdflt_ht;
    int                             code = 0;

    /* set the default halftone; build it first if required */
    if (pdflt_ht == 0)
        code = pcl_ht_build_default_ht(&pdflt_ht, pcs->memory);
    if (code == 0)
        code = pcl_ht_set_halftone(&pdflt_ht, pcl_cspace_RGB, false, pcs);

    /* set device gray color space; build it first if required */
    if ((code == 0) && (pdevgray == 0))
        code = gs_cspace_build_DeviceGray(&pdevgray, pcs->memory);
    if (code == 0)
        code = gs_setcolorspace(pcs->pgs, pdevgray);

    /* set the color to white */
    if (code == 0)
        code = gs_setcolor(pcs->pgs, &white);

    return code;
}

  private int
pattern_set_pen(
    pcl_state_t *   pcs,
    int             pen,
    int             arg2    /* ignored */
)
{
    gs_client_color ccol;
    int             num_entries = 0;
    pcl_palette_t * ppalet = 0;
    int             code = 0;

    /* set halftone and crd from the palette */
    code = set_ht_crd_from_palette(pcs);

    /* put the pen number in the proper range */
    ppalet = pcs->ppalet;
    num_entries = ppalet->pindexed->num_entries;
    if (pen > num_entries)
        pen = (pen % (num_entries - 1)) + 1;

    /* set the indexed color space from the current palette */
    if (code == 0)
        code = pcl_cs_indexed_install(&(ppalet->pindexed), pcs);

    /* set the proper color */
    ccol.paint.values[0] = pen;
    if (code == 0) {
        code = gs_setcolor(pcs->pgs, &ccol);
        pcs->ids.pattern_id = 0L;
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
    gs_client_color ccol;
    int             code = 0;
    pcl_frgrnd_t *  pfrgrnd = pcs->pfrgrnd;
    pcl_palette_t * ppalet = pcs->ppalet;


    /* check if a solid pattern should be substituted */
    if ( for_image                                                         &&
         ((pfrgrnd->pht != ppalet->pht) || (pfrgrnd->pcrd != ppalet->pcrd))  )
        code = set_frgrnd_pattern(pcs, pcl_pattern_get_solid_pattern());
    else {

        /* set the halftone and crd from the foreground */
        code = set_ht_crd_from_foreground(pcs);
        pfrgrnd = pcs->pfrgrnd;

        /* install the base color space */
        if (code == 0)
            code = pcl_cs_base_install(&(pfrgrnd->pbase), pcs);

        /* set the current color */
        ccol.paint.values[0] = (floatp)(pfrgrnd->color[0]) / 255.0;
        ccol.paint.values[1] = (floatp)(pfrgrnd->color[1]) / 255.0;
        ccol.paint.values[2] = (floatp)(pfrgrnd->color[2]) / 255.0;
 
        if (code == 0) {
            code = gs_setcolor(pcs->pgs, &ccol);
            pcs->ids.pattern_id = 0L;
        }
    }

    if (for_image && (code >= 0))
        code = set_ht_crd_from_palette(pcs);

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
        return ( inten <= 0 ? pattern_set_frgrnd(pcs, 0, 0)
                            : pattern_set_white(pcs, 0, 0) );
    else {
        int     code = 0;

        pcl_xfm_pcl_set_pat_ref_pt(pcs);
        code = set_frgrnd_pattern(pcs, pptrn);
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
        return pattern_set_frgrnd(pcs, 0, 0);
    else {
        int     code = 0;

        pcl_xfm_pcl_set_pat_ref_pt(pcs);
        code = set_frgrnd_pattern(pcs, pptrn);
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
        return pattern_set_frgrnd(pcs, 0, 0);
    else {
        pcl_xfm_pcl_set_pat_ref_pt(pcs);
        if (pptrn->type == pcl_pattern_uncolored) {
            int     code = set_frgrnd_pattern(pcs, pptrn);

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
        if (pptrn->type == pcl_pattern_uncolored)
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
                                                        0 };

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
            pcl_pattern_init_bi_patterns();
        pcs->pattern_transparent = true;
        pcs->source_transparent = true;
        pcs->pattern_id = 0;
        pcs->current_pattern_id = 0;
        pcs->pattern_type = pcl_pattern_solid_frgrnd;
    }
}

const pcl_init_t    pcl_pattern_init = { pattern_do_init, pattern_do_reset, 0 };
