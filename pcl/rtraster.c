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

/* rtraster.c - raster transfer commands */

#include "gx.h"
#include "gsmatrix.h"
#include "gscoord.h"
#include "gspath.h"
#include "gspath2.h"
#include "gsimage.h"
#include "gsiparam.h"
#include "gsiparm4.h"
#include "gsdevice.h"
#include "gsrop.h"
#include "pcstate.h"
#include "pcpalet.h"
#include "pcindexed.h"
#include "pcwhtindx.h"
#include "rtgmode.h"
#include "rtrstcmp.h"
#include "rtraster.h"

/*
 * The maximum number of planes for which seed rows need to be kept. This is the
 * larger of the maximum number of bits per index (for pixel encoding mode 0 -
 * indexed by plane) or maximum of the sum over the primaries of the number of
 * bits per primary for pixel encoding mode 2 (direct by plane). For all
 * current PCL printers, the effective bound is the the former, and is 8.
 */
#define MAX_PLANES  8

/*
 * Structure to describe a PCL raster
 *
 */
typedef struct pcl_raster_s {

    /* memory used to allocate this structure */
    gs_memory_t *       pmem;

    byte                nplanes;            /* # of planes (seed rows) */
    byte                bits_per_plane;     /* bits per plane */
    byte                nsrcs;              /* # of data sources, 1 or 3 */

    uint                transparent:1;      /* 1 ==> source transparency */
    uint                src_height_set:1;   /* source height was set */
    uint                indexed:1;          /* != 0 ==> indexed color space */
    uint                zero_is_white:1;    /* all planes 0 ==> white */
    uint                zero_is_black:1;    /* all planes 0 ==> solid color */

    int                 wht_indx;           /* white index, for indexed color
                                               space only */
    const void *        remap_ary;          /* remap array, if needed */

    gs_state *          pgs;                /* image graphic state */
    pcl_cs_indexed_t *  pindexed;           /* color space */

    gs_image_enum *     pen;                /* image enumerator */
    uint16              plane_index;        /* next plane to be received */
    uint16              rows_rendered;      /* # of source rows rendered */
    uint16              src_width;          /* usable raster width */
    uint16              src_height;         /* remaining usable raster height */

    /* buffers */
    pcl_seed_row_t *    pseed_rows;         /* seed rows, one per plane */
    byte *              cons_buff;          /* consolidation buffer */

} pcl_raster_t;

/* GC routines */
private_st_seed_row_t();
private_st_seed_row_t_element();

gs_private_st_ptrs2( st_raster_t,
                     pcl_raster_t,
                     "PCL raster object",
                     raster_enum_ptrs,
                     raster_reloc_ptrs,
                     pseed_rows,
                     cons_buff
                     );

/*
 * There is at most one image actively under construction in PCL at one time.
 * This pointer points to that image, if it exists. The pointer will be non-
 * null while in graphic mode.
 */
private pcl_raster_t *  pcur_raster;


/* forward declaration */
private int     process_zero_rows( pcl_raster_t * prast, int nrows );


/*
 * Clear the consolidation buffer, allocating it if it does not already
 * exist.
 *
 * Returns 0 on success, < 0 in the event of an error.
 */
  private int
clear_cons_buff(
    pcl_raster_t *  prast
)
{
    byte *          pcons = prast->cons_buff;
    int             npixels = prast->src_width;

    if (pcons == 0) {
        pcons = gs_alloc_bytes( prast->pmem,
                                npixels,
                                "PCL raster consolidation buff"
                                );
        if (pcons == 0)
            return e_Memory;
        prast->cons_buff = pcons;
    }
    memset(pcons, 0, npixels);

    return 0;
}

/*
 * Consolidate a set of seed rows into the consolidated row buffer.
 *
 * This routine will only be called if:
 *
 *      prast->nplanes > 1
 *      prast->bits_per_plane = 1
 *      prast->nsrcs = 1
 *
 * The output is always packed 8 bits per pixel, even if ferwer are required.
 *
 * Returns 0 on success, < 0 in the event of an error.
 */
  private int
consolidate_row(
    pcl_raster_t *  prast
)
{
    byte *          pcons;
    uint            nplanes = prast->nplanes;
    uint            npixels = prast->src_width;
    int             code, i;

    /* clear the consolidation buffer */
    if ((code = clear_cons_buff(prast)) < 0)
        return code;
    pcons = prast->cons_buff;

    /* for each plane, "or" in the appropriate bit */
    for (i = 0; i < nplanes; i++) {
        if (!prast->pseed_rows[i].is_blank) {
            uint            ishift = 0;
            const byte *    ip = prast->pseed_rows[i].pdata;
            byte *          op = pcons;
            uint            val = 0;
            int             cnt = npixels;

            while (cnt-- > 0) {
                if (ishift-- <= 0) {
                    val = *ip++;
                    ishift = 7;
                }
                *op++ |= ((val >> ishift) & 0x1) << i;
            }
        }
    }

    return 0;
}

/*
 * Create the graphic library image object needed to represent a raster.
 *
 * Returns 0 on success, < 0 in the event of an error.
 */
  private int
create_image_enumerator(
    pcl_raster_t *              prast
)
{
    int                         nplanes = prast->nplanes;
    int                         b_per_p = prast->bits_per_plane;
    int                         num_comps = (prast->indexed ? 1 : 3);
    int                         nsrcs = prast->nsrcs;
    gs_image4_t                 image;
    gs_image_enum *             pen = gs_image_enum_alloc( prast->pmem,
                                                 "Create image for PCL raster" );
    gx_image_enum_common_t *    pie = 0;
    gs_color_space *            pcspace = ( prast->indexed
                                             ? prast->pindexed->pcspace
                                             : prast->pindexed->pbase->pcspace );
    int                         code = 0;

    if (pen == 0)
        return e_Memory;

    gs_image4_t_init(&image, pcspace);
    image.Width = prast->src_width;
    image.Height = prast->src_height;
    image.CombineWithColor = true;
    image.format = ( nsrcs > 1 ? gs_image_format_component_planar
                               : gs_image_format_chunky           );

    if (nplanes > nsrcs)
        image.BitsPerComponent = 8; /* always 8 bits per pixel if consolidated */
    else
        image.BitsPerComponent = (nplanes * b_per_p) / num_comps;

    if (prast->indexed) {

        /* avoid unnecessary transparency mask in the by-plane case */
        if (prast->wht_indx >= 1 << (nplanes * b_per_p))
            image.MaskColor[0] = (1 << image.BitsPerComponent);
	else
            image.MaskColor[0] = prast->wht_indx;
        image.Decode[0] = 0.0;
        image.Decode[1] = (1 << image.BitsPerComponent) - 1;
    } else {
        int     i;

        for (i = 0; i < num_comps; i++) {
            image.Decode[2 * i] = prast->pindexed->Decode[2 * i];
            image.Decode[2 * i + 1] = prast->pindexed->Decode[2 * i + 1];

            if (image.Decode[2 * i] == 1.0)
                image.MaskColor[i] = 0;
            else if (image.Decode[2 * i + 1] == 1.0)
                image.MaskColor[i] = (1 << image.BitsPerComponent) - 1;
            else
                image.MaskColor[i] = (1 << image.BitsPerComponent);
        }
    }

    code = gs_image_begin_typed( (const gs_image_common_t *)&image,
                                 prast->pgs,
                                 true,
                                 &pie
                                 );
    if (code >= 0) 
        code = gs_image_common_init( pen,
                                     pie,
                                     (gs_data_image_t *)&image,
                                     prast->pmem,
                                     gs_currentdevice_inline(prast->pgs)
                                     );
    if (code < 0) {
        gs_free_object(prast->pmem, pen, "Create image for PCL raster");
        return code;
    }
    prast->pen = pen;
    return 0;
}

/*
 * Close the image being used to represent a raster. If the second argument is
 * true, complete the raster as well.
 *
 * This routine does NOT clear the seed rows, as their content may be needed
 * for the next row of the raster.
 *
 * NB: This routine may re-invoke itself recursively when completing the raster,
 *     as this routine will call process_zero_rows, which may once again invoke
 *     this routine. The recursion can only extend to one additional level,
 *     however, as process_zero_rows will call this routine with complete set
 *     set to false.
 */
  private void
close_raster(
    pcl_raster_t *  prast,
    bool            complete
)
{
    /* see if we need to fill in any missing rows */
    if ( complete                                   && 
         (prast->src_height > prast->rows_rendered) &&
         prast->src_height_set                        )
        (void)process_zero_rows(prast, prast->src_height - prast->rows_rendered);
    if (prast->pen != 0) {
        gs_image_cleanup(prast->pen);
        gs_free_object(prast->pmem, prast->pen, "Close PCL raster");
        prast->pen = 0;
    }
    gs_translate(prast->pgs, 0.0, (floatp)(prast->rows_rendered));
    prast->src_height -= prast->rows_rendered;
    prast->rows_rendered = 0;
}

/*
 * Process some number of zero-ed out rows, either as rasters or as a rectangle.
 *
 * Ideally, any sufficiently large regions of zero value would be rendered as
 * a rectangle, but doing so runs afoul of PCL's graphic model. Rectangles are
 * mask objects, whose value is provided by the current color/pattern/texture.
 * Images are colored objects, whose interaction with the the current color/
 * texture/raster is established by the current raster operation.
 *
 * In many cases, it is possible to emulate the effect of a colored object by
 * use of a mask object and modifications to the current pattern/color/texture
 * and the current raster operation. For the most part, however, situations in
 * which such modifications are useful do not occur often enough to be worth
 * special handling.
 *
 * There is one case that does arise with some frequency and is simple to
 * handle: 0 is white, and source transparency is on. In this case, no work
 * is necessary: just leave the output as is.
 *
 * The other case that is likely to arise often enough to be worth special
 * handling is when 0 is white but source transparency is off. In this case,
 * the current raster operation must be inverted relative to the source
 * component and a solid rectangle output. A similar situation with a black
 * rectangle does not occur very frequently, but can be handled by the same
 * technique (without inverting the raster operation), so it is handled here
 * as well.
 *
 * Zero regions of less than a kilo byte are not given special handling, so
 * as to avoid the overhead of closing and then restarting an image.
 *
 * Returns 0 on success, < 0 in the event of an error.
 */
  private int
process_zero_rows(
    pcl_raster_t *      prast,
    int                 nrows
)
{
    int                 npixels = prast->src_width;
    int                 nbytes = (npixels * prast->bits_per_plane + 7) / 8;
    int                 nplanes = prast->nplanes;
    int                 rem_rows = prast->src_height - prast->rows_rendered;
    pcl_seed_row_t *    pseed_rows = prast->pseed_rows;
    int                 code = 0;
    int                 i;

    /* clear the seed rows */
    for (i = 0; i < nplanes; i++) {
        if (!pseed_rows[i].is_blank) {
            memset(prast->pseed_rows[i].pdata, 0, nbytes);
            pseed_rows[i].is_blank = true;
        }
    }

    /* don't bother going beyond the end of the image */
    if (nrows > rem_rows)
        nrows = rem_rows;

    /* render as raster or rectangle */
    if ( ((nrows * nbytes > 1024) || (prast->pen == 0)) && 
         (prast->zero_is_white || prast->zero_is_black)   ) {
        gs_state *  pgs = prast->pgs;

        close_raster(prast, false);
        if ((prast->zero_is_black) || !prast->transparent) {
            gs_rect tmp_rect;
            bool    invert = prast->zero_is_white;

            tmp_rect.p.x = 0.0;
            tmp_rect.p.y = 0.0;
            tmp_rect.q.x = (double)npixels;
            tmp_rect.q.y = (double)nrows;
            if (invert)
                gs_setrasterop( pgs,
                                (gs_rop3_t)rop3_invert_S(gs_currentrasterop(pgs))
                                );
            gs_rectfill(prast->pgs, &tmp_rect, 1);
            if (invert)
                gs_setrasterop( pgs,
                                (gs_rop3_t)rop3_invert_S(gs_currentrasterop(pgs))
                                );
            
        }

        prast->src_height -= nrows;
        gs_translate(pgs, 0.0, (floatp)nrows);

    } else {
        int             nsrcs = prast->nsrcs;
        gs_image_enum * pen = prast->pen;
        int             cnt = 0;
        uint            size = 0;
        const byte *    pb;

        if (pen == 0) {
            if ((code = create_image_enumerator(prast)) < 0)
                return code;
            pen = prast->pen;
        }

        if (nplanes > nsrcs) {
            if ((code = clear_cons_buff(prast)) < 0)
                return code;
            cnt = nrows;
            size = npixels;
            pb = prast->cons_buff;
        } else {
            cnt = nrows * nsrcs;
            size = nbytes;
            pb = prast->pseed_rows[0].pdata;
        }

        for (i = 0; i < cnt; i++) {
            uint    dummy;

            if ((code = gs_image_next(pen, pb, size, &dummy)) < 0)
                return code;
        }
        prast->rows_rendered += nrows;
    }

    return 0;
}
  
/*
 * Process the next raster row.
 */
  private int
process_row(
    pcl_raster_t *  prast
)
{
    int             nplanes = prast->nplanes;
    gs_image_enum * pen = prast->pen;
    int             i;
    int             code = 0;

    /* check if there is anything to do */
    if (prast->rows_rendered >= prast->src_height)
        return 0;

    /* create the image enumerator if it does not already exist */
    if (pen == 0) {
        if ((code = create_image_enumerator(prast)) < 0)
            return code;
        pen = prast->pen;
    }

    /* update the raster parameters */
    prast->rows_rendered++;
    prast->plane_index = 0;

    if (prast->nsrcs == 1) {
        byte *  pb;
        int     nbytes, b_per_p;
        uint    dummy;

        /* consolidate the planes if necessary */
        if (nplanes > prast->nsrcs) {
            if ((code = consolidate_row(prast)) < 0)
                return code;
            pb = prast->cons_buff;
            b_per_p = 8;
            nbytes = prast->src_width;
        } else {
            pb = prast->pseed_rows[0].pdata;
            nbytes = prast->pseed_rows[0].size;
            b_per_p = prast->bits_per_plane;
        }

        /*
         * Remap the planes, if this is required.
         *
         * Remapping is only required for indexed color spaces. The indexed
         * by plane case will have been collapsed to an indexed by pixel case
         * by this point.
         *
         * (The macro pcl_cmap_apply_remap_ary checks for
         * prast->remap_ary == 0.)
         */
        pcl_cmap_apply_remap_ary( prast->remap_ary,
                                  pb,
                                  b_per_p,
                                  prast->src_width
                                  );

        return gs_image_next(pen, pb, nbytes, &dummy);

    } else {
        uint    dummy;
        int     nsrcs = prast->nsrcs;

        for (i = 0; (i < nsrcs) && (code >= 0); i++)
            code = gs_image_next( pen,
                                  prast->pseed_rows[i].pdata,
                                  prast->pseed_rows[i].size,
                                  &dummy
                                  );
        return code;
    }
}

/*
 * Process an input data buffer using adpative compression.
 */
  private int
process_adaptive_compress(
    pcl_raster_t *      prast,
    const byte *        pin,
    uint                insize
)
{
    pcl_seed_row_t *    pseed_row = prast->pseed_rows;
    byte *              pdata = pseed_row->pdata;
    uint                row_size = pseed_row->size;
    int                 code = 0;

    prast->plane_index = 0;
    while ((insize > 3) && (code >= 0)) {
        int     cmd = *pin++;
        uint    param = *pin++;

        param = (param << 8) + *pin++;
        insize -= 3;
        if (cmd <= 3) {
            uint    cnt = min(insize, param);

            pcl_decomp_proc[cmd](pseed_row, pin, cnt);
            insize -= cnt;
            pin += cnt;
            prast->plane_index = 1;
            code = process_row(prast);
        } else if (cmd == 4)
            code = process_zero_rows(prast, param);
        else if (cmd == 5) {
            uint            rem_rows = prast->src_height - prast->rows_rendered;
            gs_image_enum * pen = prast->pen;

            /* create the image enumerator if it does not already exist */
            if (pen == 0) {
                if ((code = create_image_enumerator(prast)) < 0)
                    return code;
                pen = prast->pen;
            }

            if (param > rem_rows)
                param = rem_rows;
            prast->rows_rendered += param;
            while ((param-- > 0) && (code >= 0)) {
                uint    dummy;

                code = gs_image_next(pen, pdata, row_size, &dummy);
            }

        } else
            break;
    }

    return code;
}

/*
 * Add a raster plane. The second operand indicates whether or not this is the
 * final plane of a row.
 */
  private int
add_raster_plane(
    const byte *    pdata,
    uint            nbytes,
    bool            end_row,
    pcl_state_t *   pcs
)
{
    pcl_raster_t *  prast = pcur_raster;
    int             comp_mode = pcs->raster_state.compression_mode;
    int             nplanes = 0;
    int             plane_index = 0;
    int             code = 0;

    /* enter raster mode implicitly if not already there */
    if (prast == 0) {
        if ((code = pcl_enter_graphics_mode(pcs, IMPLICIT)) < 0)
            return code;
        prast = pcur_raster;
    }

    /*
     * Adaptive compression (mode 5) is only available for single-plane
     * encodings, and then only if used with a transfer row (ESC * b # W)
     * command. The latter behavior matches that of the HP Color LaserJet 5/5M,
     * but not that of the DeskJet 1600C/CM, which has somewhat erratic
     * behavior in this case.
     */
    nplanes = prast->nplanes;
    if ( (comp_mode == ADAPTIVE_COMPRESS) && ((nplanes > 1) || !end_row) )
        return e_Range;

    /*
     * If all the rows that can be output have already been rendered, just
     * return.
     */
    if (prast->rows_rendered >= prast->src_height)
        return 0;

    /*
     * If all planes for this row have been entered, just ignore the current
     * data (but don't return yet, as we may still need to output the current
     * raster row).
     */
    plane_index = prast->plane_index;
    if (plane_index < nplanes) {
        pcl_seed_row_t *    pseed = prast->pseed_rows + plane_index;

        prast->plane_index++;
        if (comp_mode == ADAPTIVE_COMPRESS)
            return process_adaptive_compress(prast, pdata, nbytes);
        else
            (void)pcl_decomp_proc[comp_mode](pseed, pdata, nbytes);
    }

    return 0;
}

/*
 * Create a PCL raster object. This procedure is called when entering graphics
 * mode.
 *
 * Returns 0 on success, < 0 in the event of an error.
 */
  int
pcl_start_raster(
    uint                src_width,
    uint                src_height,
    pcl_state_t *       pcs
)
{
    pcl_raster_t *      prast = pcur_raster;
    pcl_palette_t *     ppalet = pcs->ppalet;
    pcl_cs_indexed_t *  pindexed = ppalet->pindexed;
    pcl_encoding_type_t penc = pcl_cs_indexed_get_encoding(pindexed);
    int                 seed_row_bytes = 0;
    pcl_seed_row_t *    pseed_rows = 0;
    int                 i;

    /* there can only be one raster object present at a time */
    if (prast != 0)
        pcl_complete_raster();

    prast = gs_alloc_struct( pcs->memory,
                             pcl_raster_t,
                             &st_raster_t,
                             "start PCL raster"
                             );
    if (prast == 0)
        return e_Memory;

    prast->pmem = pcs->memory;

    prast->transparent = pcs->source_transparent;
    prast->src_height_set = pcs->raster_state.src_height_set;
    prast->pgs = pcs->pgs;
    pcl_cs_indexed_init_from(prast->pindexed, pindexed);

    prast->pen = 0;
    prast->plane_index = 0;
    prast->rows_rendered = 0;
    prast->src_width = src_width;
    prast->src_height = src_height;

    /* the conslidation buffer is created when first needed */
    prast->cons_buff = 0;

    if (penc <= pcl_penc_indexed_by_pixel) {
        int     b_per_i = pcl_cs_indexed_get_bits_per_index(pindexed);

        if (penc == pcl_penc_indexed_by_plane) {
            prast->nplanes = b_per_i;
            prast->bits_per_plane = 1;
        } else { /* penc == pcl_penc_indexed_by_pixel */
            prast->nplanes = 1;
            prast->bits_per_plane = b_per_i;
        }
        prast->nsrcs = 1;
        prast->indexed = true;
        prast->zero_is_white = pcl_cs_indexed_0_is_white(pindexed);
        prast->zero_is_black = pcl_cs_indexed_0_is_black(pindexed);
        prast->remap_ary = pcl_cmap_create_remap_ary(pcs, &(prast->wht_indx));

    } else {    /* penc >= pcl_penc_direct_by_plane */
        int     b_per_primary = pcl_cs_indexed_get_bits_per_primary(pindexed, 0);

        if (penc == pcl_penc_direct_by_plane) {
            prast->nplanes = 3;
            prast->bits_per_plane = b_per_primary;
            prast->nsrcs = 3;
        } else {    /* penc == pcl_penc_direct_by_pixel */
            prast->nplanes = 1;
            prast->bits_per_plane = 3 * b_per_primary;
            prast->nsrcs = 1;
        }
        prast->indexed = false;
        prast->zero_is_white = false;
        prast->zero_is_black = true;
        prast->wht_indx = 1;    /* not significant */
        prast->remap_ary = 0;
    }

    /* allocate the seed row buffers */
    pseed_rows = gs_alloc_struct_array( prast->pmem,
                                        prast->nplanes,
                                        pcl_seed_row_t,
                                        &st_seed_row_t_element,
                                        "start PCL raster"
                                        );
    if (pseed_rows != 0) {
        int     seed_row_bytes = (prast->src_width * prast->bits_per_plane + 7)
                                 / 8;
        int     nplanes = prast->nplanes;
        int     i, j;

        for (i = 0; i < nplanes; i++) {
            byte *  pdata = gs_alloc_bytes( prast->pmem,
                                            seed_row_bytes,
                                            "start PCL raster"
                                            );

            if (pdata == 0)
                break;
            pseed_rows[i].size = seed_row_bytes;
            pseed_rows[i].pdata = pdata;
            memset(pseed_rows[i].pdata, 0, seed_row_bytes);
            pseed_rows[i].is_blank = true;
        }

        /* check if everything was successful */
        if (i == prast->nplanes) {
            prast->pseed_rows = pseed_rows;
            pcur_raster = prast;
            return 0;
        }

        /* memory exhaustion; release the already allocated seed rows */
        for (j = 0; j < i; j++)
            gs_free_object(prast->pmem, pseed_rows[i].pdata, "start PCL raster");

        gs_free_object(prast->pmem, pseed_rows, "start PCL raster");
    }

    /* must have failed due to memory exhaustion; release the raster object */
    gs_free_object(prast->pmem, prast, "start PCL raster");

    return e_Memory;
}

/*
 * Complete a raster. This is called when exiting graphics mode.
 */
  void
pcl_complete_raster(void)
{
    pcl_raster_t *  prast = pcur_raster;
    int             i;

    /* if already in raster mode, ignore */
    if (prast == 0)
        return;

    /* close the current raster */
    close_raster(prast, true);

    /* free associated objects */
    if (prast->remap_ary != 0) {
        gs_free_object( prast->pmem,
                        (void *)prast->remap_ary,
                        "Complete PCL raster"
                        );
        prast->remap_ary = 0;
    }

    if (prast->pindexed != 0) {
        pcl_cs_indexed_release(prast->pindexed);
        prast->pindexed = 0;
    }

    if (prast->pseed_rows != 0) {
        for (i = 0; i < prast->nplanes; i++) {
            if (prast->pseed_rows[i].pdata != 0)
                gs_free_object( prast->pmem,
                                prast->pseed_rows[i].pdata,
                                "Complete PCL raster"
                                );
        }
        gs_free_object(prast->pmem, prast->pseed_rows, "Complete PCL raster");
        prast->pseed_rows = 0;
    }

    if (prast->cons_buff != 0)
        gs_free_object(prast->pmem, prast->cons_buff, "Complete PCL raster");
    

    /* free the PCL raster robject itself */
    gs_free_object(prast->pmem, prast, "Complete PCL raster");
    pcur_raster = 0;
}

/*
 * ESC * b # V
 *
 * Add a plane buffer to the current set.
 */
  private int
transfer_raster_plane(
    pcl_args_t *    pargs,
    pcl_state_t *   pcs
)
{
    return add_raster_plane(arg_data(pargs), arg_data_size(pargs), false, pcs);
}

/*
 * <esc> * b # W
 *
 * Add a plane buffer to the current buffered set, and complete the current
 * raster row.
 */
  private int
transfer_raster_row(
    pcl_args_t *    pargs,
    pcl_state_t *   pcs
)
{
    const byte *    pdata = arg_data(pargs);
    int             code = 0;

    pcs->have_page = true;  /* conservative */
    code = add_raster_plane(pdata, arg_data_size(pargs), true, pcs);

    /* process any planes that have not been provided; complete the row */
    if (code >= 0) {
        int     i, nplanes = pcur_raster->nplanes;

        for (i = pcur_raster->plane_index; (i < nplanes) && (code >= 0); i++)
            code = add_raster_plane(pdata, 0, false, pcs);
    }
    if (code >= 0)
        code = process_row(pcur_raster);

    return code;
}

/*
 * <esc> * b # Y
 *
 * Skip (zero-fill) a number of raster rows. This command is ignored outside
 * of raster mode.
 *
 * Note that any incomplete plane data for the current row is discarded by this
 * command.
 */
  private int
raster_y_offset(
    pcl_args_t *    pargs,
    pcl_state_t *   pcs
)
{
    pcl_raster_t *  prast = pcur_raster;

    /* ignored outside of graphics mode */
    if ((prast != 0) && (uint_arg(pargs) > 0)) {
        pcs->have_page = true;  /* conservative */
        return process_zero_rows(prast, uint_arg(pargs));
    } else
        return 0;
}

/*
 * There is no specific copy code for this module, as both entry to and exit
 * from a macro must end graphics mode (and thus are handled by the parser).
 * There is also no explicit reset routine, as the required work is handled
 * at a higher level.
 */
  private int
raster_do_init(
    gs_memory_t *   pmem    /* ignored */
)
{
    DEFINE_CLASS('*')
    {
        'b', 'V',
        PCL_COMMAND( "Transfer Raster Plane",
                     transfer_raster_plane,
                     pca_raster_graphics | pca_bytes
                     )
    },
    {
        'b', 'W',
        PCL_COMMAND( "Transfer Raster Row",
                     transfer_raster_row,
                     pca_raster_graphics | pca_bytes
                     )
    },
    {
        'b', 'Y',
        PCL_COMMAND( "Raster Y Offset",
                     raster_y_offset,
                     pca_raster_graphics | pca_neg_ok | pca_big_clamp
                     )
    },
    END_CLASS
    return 0;
}

  private void
raster_do_reset(
    pcl_state_t *       pcs,
    pcl_reset_type_t    type
)
{
    if ((type & pcl_reset_initial) != 0)
        pcur_raster = 0;
}

const pcl_init_t    rtraster_init = { raster_do_init, raster_do_reset, 0 };
