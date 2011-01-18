/* Portions Copyright (C) 2001 artofcode LLC.
   Portions Copyright (C) 1996, 2001 Artifex Software Inc.
   Portions Copyright (C) 1988, 2000 Aladdin Enterprises.
   This software is based in part on the work of the Independent JPEG Group.
   All Rights Reserved.

   This software is distributed under license and may not be copied, modified
   or distributed except as expressly authorized under the terms of that
   license.  Refer to licensing information at http://www.artifex.com/ or
   contact Artifex Software, Inc., 101 Lucas Valley Road #110,
   San Rafael, CA  94903, (415)492-9861, for further information. */
/*$Id$ */

/* rtraster.c - raster transfer commands */

#include "memory_.h"
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
#include "pcpage.h"
#include "pcindxed.h"
#include "pcwhtidx.h"
#include "pcdraw.h"
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
     uint                interpolate:1;      /* enable interpolation */
     int                 wht_indx;           /* white index, for indexed color
                                                space only */
     const void *        remap_ary;          /* remap array, if needed */
 
     pcl_state_t *       pcs;                /* to avoid n extra operand */
     pcl_cs_indexed_t *  pindexed;           /* color space */
 
     gs_image_enum *     pen;                /* image enumerator */
     uint16              plane_index;        /* next plane to be received */
     uint16              rows_rendered;      /* # of source rows rendered */
     uint16              src_width;          /* usable raster width */
     uint16              src_height;         /* remaining usable raster height */
 
     /* objects required for opaque source/transparent pattern case */
     gs_image_enum *     mask_pen;           /* enumerator for mask */
     pcl_cs_indexed_t *  mask_pindexed;      /* special color space for mask */
     ulong               white_val;          /* value interpreted as white */
     void                (*gen_mask_row)( struct pcl_raster_s * prast );
 
     /* buffers */
     pcl_seed_row_t *    pseed_rows;         /* seed rows, one per plane */
     byte *              cons_buff;          /* consolidation buffer */
     byte *              mask_buff;          /* buffer for mask row, if needed */
 
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

/* forward declaration */
static int     process_zero_rows( gs_state * pgs, pcl_raster_t * prast, int nrows );


/*
 * Clear the consolidation buffer, allocating it if it does not already
 * exist.
 *
 * Returns 0 on success, < 0 in the event of an error.
 */
  static int
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
 * Clear the mask buffer, allocating it if it does not exist.
 *
 * Returns 0 on success, < 0 in the event of an error.
 */
  static int
clear_mask_buff(
    pcl_raster_t *  prast
)
{
    byte *          pmask = prast->mask_buff;
    int             nbytes = (prast->src_width + 7) / 8;

    if (pmask == 0) {
        pmask = gs_alloc_bytes( prast->pmem,
                                nbytes,
                                "PCL raster mask buffer"
                                );
        if (pmask == 0)
            return e_Memory;
        prast->mask_buff = pmask;
    }
    memset(pmask, 0, nbytes);

    return 0;

}

/*
 * Generate a mask row in case there are multiple data sources (in the graphic
 * library sense). This code takes much advantage of the knowledge that the
 * mutliple source case is always direct and one bit per pixel.
 */
  static void
gen_mask_multisrc(
    pcl_raster_t *  prast
)
{
    byte *          ip0 = prast->pseed_rows[0].pdata;
    byte *          ip1 = prast->pseed_rows[1].pdata;
    byte *          ip2 = prast->pseed_rows[2].pdata;
    byte *          op = prast->mask_buff;
    uint            m0 = (prast->white_val >> 16) & 0xff;
    uint            m1 = (prast->white_val >> 8) & 0xff;
    uint            m2 = prast->white_val & 0xff;
    int             nbytes = prast->pseed_rows[0].size;
    int             i;

    for (i = 0; i < nbytes; i++)
        *op++ = (*ip0++ ^ m0) & (*ip1++ ^ m1) & (*ip2++ ^ m2);
}

/*
 * Generate a mask from input data that is less than one byte. For PCL rasters
 * as implemented by this routine, such situations only occur when an integral
 * number of pixels fit within one byte, and this routine takes advantage of
 * that situation.
 */
  static void
gen_mask_subbyte(
    pcl_raster_t *  prast
)
{
    byte *          ip = prast->pseed_rows[0].pdata;
    byte *          op = prast->mask_buff;
    int             ishift = prast->bits_per_plane;
    uint            targ = prast->white_val;
    int             size = prast->src_width;
    uint            ival, oval, imask, omask;
    int             i;

    for (i = 0, ival = 0, oval = 0, imask = 0, omask = 0x80; i < size; i++) {
        if ((imask >>= ishift) == 0) {
            imask = 0xff - (0xff >> ishift);
            ival = *ip++;
        }
        if (((ival ^ targ) & imask) == 0)
            oval |= omask;
        if ((omask >>= 1) == 0) {
            *op++ = oval;
            omask = 0x80;
            oval = 0;
        }
    }
    if (omask != 0x80)
        *op++ = oval;
}

/*
 * Generate a mask from input data that has one byte per pixel.
 */
  static void
gen_mask_1byte(
    pcl_raster_t *  prast
)
{
    byte *          ip = (prast->nplanes == 1 ? prast->pseed_rows[0].pdata
                                              : prast->cons_buff);
    byte *          op = prast->mask_buff;
    uint            targ = prast->white_val;
    int             size = prast->src_width;
    uint            oval, omask;
    int             i;

    for (i = 0, oval = 0, omask = 0x80; i < size; i++) {
        if (*ip++ == targ)
            oval |= omask;
        if ((omask >>= 1) == 0) {
            *op++ = oval;
            omask = 0x80;
            oval = 0;
        }
    }
    if (omask != 0x80)
        *op++ = oval;
}

/*
 * Generate a mask row in the case that more than one byte is required per
 * pixel. The only possible such case in PCL is 8-bits per primary 3 color,
 * so this routine handles only that case.
 */
  void
gen_mask_multibyte(
    pcl_raster_t *  prast
)
{
    byte *          ip = prast->pseed_rows[0].pdata;
    byte *          op = prast->mask_buff;
    int             size = prast->src_width;
    ulong           targ = prast->white_val;
    uint            oval, omask;
    int             i;

    for (i = 0, oval = 0, omask = 0x80; i < size; i++, ip += 3) {
        ulong   ival = (((ulong)ip[0]) << 16) | (((ulong)ip[1]) << 8) | ip[2];

        if (ival == targ)
            oval |= omask;
        if ((omask >>= 1) == 0) {
            *op++ = oval;
            omask = 0x80;
            oval = 0;
        }
    }
    if (omask != 0x80)
        *op++ = oval;
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
  static int
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
            const byte *    ip = prast->pseed_rows[i].pdata;
            byte *          op = pcons;
            int             cnt = npixels;

	    for (; cnt >= 8; ip++, op += 8, cnt -= 8) {
		uint val = *ip;

		/*
		 * cons_buff was allocated with gs_alloc_bytes, so we know
		 * it is aligned for (at least) bits32 access.
		 */
#if ARCH_IS_BIG_ENDIAN
		static const bits32 spread[16] = {
		    0x00000000, 0x00000001, 0x00000100, 0x00000101,
		    0x00010000, 0x00010001, 0x00010100, 0x00010101,
		    0x01000000, 0x01000001, 0x01000100, 0x01000101,
		    0x01010000, 0x01010001, 0x01010100, 0x01010101
		};
#else
		static const bits32 spread[16] = {
		    0x00000000, 0x01000000, 0x00010000, 0x01010000,
		    0x00000100, 0x01000100, 0x00010100, 0x01010100,
		    0x00000001, 0x01000001, 0x00010001, 0x01010001,
		    0x00000101, 0x01000101, 0x00010101, 0x01010101
		};
#endif
		((bits32 *)op)[0] |= spread[val >> 4] << i;
		((bits32 *)op)[1] |= spread[val & 0xf] << i;
	    }
	    if (cnt) {
		uint ishift = 7;
		uint val = *ip;

		do {
		    *op++ |= ((val >> ishift--) & 0x1) << i;
		} while (--cnt > 0);
	    }
        }
    }

    return 0;
}

/*
 * Create an enumerator for the mask portion of an image (if required).
 *
 * Returns 0 on success, < 0 in the event of an error.
 */
  static int
create_mask_enumerator(
    pcl_raster_t *              prast
)
{
    int				transparent = prast->transparent;
    /*
     * Most elements of gs_image1_t and gs_image4_t are identical.  The only exception
     * that we care about is MaskColor in gs_image_type4_t.
     */
    union {
        gs_image1_t i1;
	gs_image4_t i4;
    }				image;
    gs_image_enum *             pen = gs_image_enum_alloc( prast->pmem,
							   "Create image for PCL raster" );
    int                         code = 0;
    const byte *                pcolor = 0;
    gx_image_enum_common_t *    pie = 0;
    pcl_state_t *               pcs = prast->pcs;

    if (pen == 0)
        return e_Memory;

    pcl_set_drawing_color(pcs, pcl_pattern_solid_white, 0, true);

    /* generate the special two entry indexed color space required */
    if (prast->indexed)
        pcolor = prast->pindexed->palette.data + 3 * prast->wht_indx;
    else {
        static const byte   cwhite[3] = { 1.0, 1.0, 1.0 };

        pcolor = cwhite;
    }
    code = pcl_cs_indexed_build_special( &(prast->mask_pindexed),
                                         prast->pindexed->pbase,
                                         pcolor,
                                         prast->pmem
                                         );

    if (code >= 0) {
        if (transparent)
            gs_image4_t_init( (gs_image4_t *) &image, prast->mask_pindexed->pcspace);
        else
            gs_image_t_init_adjust( (gs_image_t *) &image, prast->mask_pindexed->pcspace, 0);
        image.i1.Width = prast->src_width;
        image.i1.Height = prast->src_height;

	if ( pcs->personality == pcl5e )
	    image.i1.CombineWithColor = false;
	else
	    image.i1.CombineWithColor = true;
	image.i1.format = gs_image_format_chunky;
	image.i1.BitsPerComponent = 1;
	image.i1.Decode[0] = 0.0;
	image.i1.Decode[1] = 1.0;
        if (transparent)
	    image.i4.MaskColor[0] = 0.0;
    
	code = gs_image_begin_typed( (const gs_image_common_t *)&image,
				     pcs->pgs,
				     true,
				     &pie
				     );

        if (code >= 0)
            code = gs_image_common_init( pen,
                                         pie,
                                         (gs_data_image_t *)&image,
                                         gs_currentdevice_inline(pcs->pgs)
                                         );
    }

    if (code < 0)
        gs_free_object(prast->pmem, pen, "Create image for PCL raster");
    else
        prast->mask_pen = pen;

    pcl_set_drawing_color(pcs, pcs->pattern_type, pcs->pattern_id, true);
    return code;
}

/*
 * Create the graphic library image object needed to represent a raster.
 *
 * If the image does not use transparency then we need to use image type 1 processing.
 * Otherwise we need to use image type 4.  Most of the setup is the same for both
 * cases.  Thus rather than split this into two routines with a lot redundant code
 * I am keeping one routine with a union structure (image) and some conditionals.
 *
 * Returns 0 on success, < 0 in the event of an error.
 */
  static int
create_image_enumerator(
    pcl_raster_t *              prast
)
{
    int                         nplanes = prast->nplanes;
    int                         b_per_p = prast->bits_per_plane;
    int                         num_comps = (prast->indexed ? 1 : 3);
    int                         nsrcs = prast->nsrcs;
    /*
     * Most elements of gs_image1_t and gs_image4_t are identical.  The only exception
     * that we care about is MaskColor in gs_image_type4_t.
     */
    int				use_image4 = prast->transparent;
    union {
        gs_image1_t i1;
	gs_image4_t i4;
    }				image;
    gs_image_enum *             pen = gs_image_enum_alloc( prast->pmem,
                                                 "Create image for PCL raster" );
    gx_image_enum_common_t *    pie = 0;
    gs_color_space *            pcspace = ( prast->indexed
                                             ? prast->pindexed->pcspace
                                             : prast->pindexed->pbase->pcspace );
    int                         code = 0;

    if (pen == 0)
        return e_Memory;

    /*
     * There is one more case in which we will not use image type 4 processing.
     * If our color specifications are indexed and the wht_index value is greater
     * then the largest possible value given the number of index bits, then it is
     * not possible to ever get a 'white' (transparent) value.  Thus skip
     * transparency in this case.
     */
    if ((prast->indexed) && (prast->wht_indx >= 1 << (nplanes * b_per_p)))
        use_image4 = 0;

    /* we also don't use an image type 4 if the user has requested
       interpolation */
    if (prast->interpolate)
        use_image4 = 0;

    if (use_image4)
        gs_image4_t_init( (gs_image4_t *) &image, pcspace);
    else
        gs_image_t_init_adjust( (gs_image_t *) &image, pcspace, 0);
    image.i1.Width = prast->src_width;
    image.i1.Height = prast->src_height;
    image.i1.CombineWithColor = true;
    image.i1.format = ( nsrcs > 1 ? gs_image_format_component_planar
                               : gs_image_format_chunky           );

    if (nplanes > nsrcs)
        image.i1.BitsPerComponent = 8; /* always 8 bits per pixel if consolidated */
    else
        image.i1.BitsPerComponent = (nplanes * b_per_p) / num_comps;

    image.i1.Interpolate = prast->interpolate;

    if (prast->indexed) {
	if (use_image4)
            image.i4.MaskColor[0] = prast->wht_indx;
        image.i1.Decode[0] = 0.0;
        image.i1.Decode[1] = (1 << image.i1.BitsPerComponent) - 1;
    } else {
        int     i;

        for (i = 0; i < num_comps; i++) {
            image.i1.Decode[2 * i] = prast->pindexed->Decode[2 * i];
            image.i1.Decode[2 * i + 1] = prast->pindexed->Decode[2 * i + 1];

	    if (use_image4) {
                image.i4.MaskColor[i] = (1 << image.i1.BitsPerComponent);
                if (image.i1.Decode[2 * i] == 1.0)
                    image.i4.MaskColor[i] = 0;
                else if (image.i1.Decode[2 * i + 1] == 1.0)
                    image.i4.MaskColor[i] = (1 << image.i1.BitsPerComponent) - 1;
            }
        }
    }

    code = gs_image_begin_typed( (const gs_image_common_t *)&image,
                                 prast->pcs->pgs,
                                 true,
                                 &pie
                                 );
    if (code >= 0) 
        code = gs_image_common_init( pen,
                                     pie,
                                     (gs_data_image_t *)&image,
                                     gs_currentdevice_inline(prast->pcs->pgs)
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
  static void
close_raster(
    gs_state *      pgs,
    pcl_raster_t *  prast,
    bool            complete
)
{
    /* see if we need to fill in any missing rows */
    if ( complete                                   && 
         (prast->src_height > prast->rows_rendered) &&
         prast->src_height_set                        )
        (void)process_zero_rows(pgs, prast, prast->src_height - prast->rows_rendered);
    if (prast->pen != 0) {
        gs_image_cleanup(prast->pen, pgs);
        gs_free_object(prast->pmem, prast->pen, "Close PCL raster");
        prast->pen = 0;
    }
    if (prast->mask_pen != 0) {
        gs_image_cleanup(prast->mask_pen, pgs);
        gs_free_object(prast->pmem, prast->mask_pen, "Close PCL raster");
        prast->mask_pen = 0;
    }
    gs_translate(prast->pcs->pgs, 0.0, (floatp)(prast->rows_rendered));
    prast->src_height -= prast->rows_rendered;
    prast->rows_rendered = 0;
}


/*
 * Generate the white-mask corresponding to an image scanline. This is
 * necessary to implement the opaque source/transparent texture case.
 *
 * HP's specification of transparency includes one unintuitive case: opaque
 * source and transparent texture. In this case, the texture applies only to
 * the non-white portion of the source; the white portion should be rendered
 * in a solid white.
 *
 * Since the graphic library does not support mutliple textures in a single
 * rendering operation, it is necessary to split objects that have both a
 * foreground and a background into two transparent objects: one having just
 * the foreground, the other just the background. In the case of rasters it
 * is necessary to form a mask object that is the inverse of the background,
 * and "paint" it with "white". The following code accomplishes this task.
 *
 * It is, unfortunately, not possible to use the graphic libraries image mask
 * feature to implement the "white mask", because image masks in the graphic
 * library are not implemented as mask objects. Rather, they are implemented
 * as transparent colored patterns, with the foreground color taken from the
 * current color at the time the image mask is begun. Instead, a two entry
 * transparent colored image is used, whose foreground color is the current
 * white and whose background color is a transparent white.
 *
 * As always, what is considered "white" is evaluated in the source color space;
 * this varies from HP's practice, and can give unexpected results if an
 * inverting color lookup table is used.
 */
  static int
process_mask_row(
    pcl_raster_t *  prast
)
{
    int             code = clear_mask_buff(prast);
    gs_image_enum * pen = prast->mask_pen;

    if ( (code >= 0)                                                  &&
         ((pen != 0) || ((code = create_mask_enumerator(prast)) >= 0))  ) {
        uint            dummy;
        pcl_state_t *   pcs = prast->pcs;

        pen = prast->mask_pen;
        pcl_set_drawing_color(pcs, pcl_pattern_solid_white, 0, true);
        prast->gen_mask_row(prast);
        code = gs_image_next( pen,
                              prast->mask_buff,
                              (prast->src_width + 7) / 8,
                              &dummy
                              );
        pcl_set_drawing_color(pcs, pcs->pattern_type, pcs->pattern_id, true);
    }
    return code;
}

  static int
process_zero_mask_rows(
    pcl_raster_t *  prast,
    int             nrows
)
{
    int             code = clear_mask_buff(prast);
    gs_image_enum * pen = prast->mask_pen;

    if ( (code >= 0)                                                  &&
         ((pen != 0) || ((code = create_mask_enumerator(prast)) >= 0))  ) {
        uint            dummy;
        pcl_state_t *   pcs = prast->pcs;
        int             nbytes = (prast->src_width + 7) / 8;

        pen = prast->mask_pen;
        memset(prast->mask_buff, 0xff, nbytes);
        pcl_set_drawing_color(pcs, pcl_pattern_solid_white, 0, true);
        gs_setrasterop(pcs->pgs, (gs_rop3_t)rop3_know_S_1((int)0xff));
        while ((nrows-- > 0) && (code >= 0))
            code = gs_image_next(pen, prast->mask_buff, nbytes, &dummy);
        pcl_set_drawing_color(pcs, pcs->pattern_type, pcs->pattern_id, true);
    }
    return code;
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
  static int
process_zero_rows(
    gs_state *          pgs,
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
    int                 moveto_nrows = nrows;

    /* don't bother going beyond the end of the image */
    if (nrows > rem_rows) {
        nrows = rem_rows;
    }

    /* if clipping the whole raster, just update rendered rows */
    if (prast->pcs->raster_state.clip_all) {
        prast->rows_rendered += nrows;
        return 0;
    }

    /* clear the seed rows */
    for (i = 0; i < nplanes; i++) {
        if (!pseed_rows[i].is_blank) {
            memset(prast->pseed_rows[i].pdata, 0, nbytes);
            pseed_rows[i].is_blank = true;
        }
    }

    /* render as raster or rectangle */
    if ( ((nrows * nbytes > 1024) || (prast->pen == 0)) && 
         (prast->zero_is_white || prast->zero_is_black)   ) {
        gs_state *  pgs = prast->pcs->pgs;

        close_raster(pgs, prast, false);
        if ((prast->zero_is_black) || !prast->pcs->source_transparent ) {
            gs_rect tmp_rect;
            bool    invert = prast->zero_is_white;

            tmp_rect.p.x = 0.0;
            tmp_rect.p.y = 0.0;
            tmp_rect.q.x = (double)npixels;
            tmp_rect.q.y = (double)nrows;
            if (invert) {
                gs_setrasterop( pgs,
                                (gs_rop3_t)rop3_invert_S(gs_currentrasterop(pgs))
                                );
		gs_rectfill(pgs, &tmp_rect, 1 );

                gs_setrasterop( pgs,
                                (gs_rop3_t)rop3_invert_S(gs_currentrasterop(pgs))
                                );
	    } 
	    else
		gs_rectfill(pgs, &tmp_rect, 1);
            
        }

        prast->src_height -= nrows;
	/* NB HP bug CET21.04 pg 7 */
	/* NB text cap move to moveto_nrows, but raster cap moveto nrows */
        gs_translate(pgs, 0.0, (floatp)moveto_nrows); 

        return 0;

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

        if (prast->gen_mask_row != 0)
            code = process_zero_mask_rows(prast, nrows);

        return code;
    }
}
  
/*
 * Process the next raster row.
 *
 * The compression mode is provided to allow this routine to fill in any
 * missing rows. For adaptive compression (mode 5), this will be 0.
 */
  static int
process_row(
    pcl_raster_t *  prast,
    int             comp_mode   /* modified compression mode */
)
{
    int             nplanes = prast->nplanes;
    gs_image_enum * pen = prast->pen;
    int             i;
    int             code = 0;

    /* check if there is anything to do */
    if (prast->rows_rendered >= prast->src_height)
        return 0;
    else if (prast->pcs->raster_state.clip_all) {
        prast->rows_rendered++;
        return 0;
    }

    /* handle any planes not provided */
    for (i = prast->plane_index; i < nplanes; i++) {
        static  const byte  dummy = 0;

        (void)pcl_decomp_proc[comp_mode](prast->pseed_rows + i, &dummy, 0);
    }

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

        code = gs_image_next(pen, pb, nbytes, &dummy);

    } else {
        uint    dummy;
        int     nsrcs = prast->nsrcs;

        for (i = 0; (i < nsrcs) && (code >= 0); i++)
            code = gs_image_next( pen,
                                  prast->pseed_rows[i].pdata,
                                  prast->pseed_rows[i].size,
                                  &dummy
                                  );
    }

    if ((prast->gen_mask_row != 0) && (code >= 0))
        code = process_mask_row(prast);
    prast->pcs->page_marked = true;
    return code;
}

/*
 * Process an input data buffer using adpative compression.
 */
  static int
process_adaptive_compress(
    gs_state *          pgs, 
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
    while ((insize >= 3) && (code >= 0)) {
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
            code = process_row(prast, 0);
        } else if (cmd == 4)
            code = process_zero_rows(pgs, prast, param);
        else if (cmd == 5) {
            uint            rem_rows = prast->src_height - prast->rows_rendered;
            gs_image_enum * pen = prast->pen;

            if (param > rem_rows)
                param = rem_rows;

            /* if clipping the raster, just update lines rendered */
            if (prast->pcs->raster_state.clip_all) {
                prast->rows_rendered += param;
                continue;
            }

            /* create the image enumerator if it does not already exist */
            if (pen == 0) {
                if ((code = create_image_enumerator(prast)) < 0)
                    return code;
                pen = prast->pen;
            }

            if (prast->nplanes == 1) {
                prast->rows_rendered += param;
                while ((param-- > 0) && (code >= 0)) {
                    uint    dummy;

                    code = gs_image_next(pen, pdata, row_size, &dummy);
                    if ((prast->gen_mask_row != 0) && (code >= 0))
                        code = process_mask_row(prast);
                }
            } else {
                prast->plane_index = 1;
                while ( (param-- > 0) && ((code = process_row(prast, 0) >= 0)) )
                    prast->plane_index = 1;
                prast->plane_index = 0;
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
  static int
add_raster_plane(
    const byte *    pdata,
    uint            nbytes,
    bool            end_row,
    pcl_state_t *   pcs
)
{
    pcl_raster_t *  prast = (pcl_raster_t *)pcs->raster_state.pcur_raster;
    int             comp_mode = pcs->raster_state.compression_mode;
    int             nplanes = 0;
    int             plane_index = 0;
    int             code = 0;

    /* enter raster mode implicitly if not already there */
    if (prast == 0) {
        if ((code = pcl_enter_graphics_mode(pcs, IMPLICIT)) != 0)
            return code;
        prast = (pcl_raster_t *)pcs->raster_state.pcur_raster;
    }

    /*
     * Adaptive compression (mode 5) is only available for single-plane
     * encodings, and then only if used with a transfer row (ESC * b # W)
     * command. The latter behavior matches that of the HP Color LaserJet 5/5M,
     * but not that of the DeskJet 1600C/CM, which has somewhat erratic
     * behavior in this case.
     */
    nplanes = prast->nplanes;
    if ((comp_mode == ADAPTIVE_COMPRESS) && !end_row)
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
            return process_adaptive_compress(pcs->pgs, prast, pdata, nbytes);
        else
            (void)pcl_decomp_proc[comp_mode](pseed, pdata, nbytes);
    }

    return 0;
}

/*
 * Create a PCL raster object. This procedure is called when entering graphics
 * mode.
 *
 * Note that a raster must be considered "transparent" if either source or
 * pattern transparency is in effect. If only pattern transparency is set, an
 * addition mask object must be created to fill the "white" regions of the
 * raster. This object does not use the current texture; it sets the texture
 * to opaque white when it is rendered. This is in conformance with HP's
 * somewhat unintuitive interpretation of the opaque source/transparent
 * pattern situation.
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
    pcl_raster_t *      prast = (pcl_raster_t *)pcs->raster_state.pcur_raster;
    pcl_palette_t *     ppalet = pcs->ppalet;
    pcl_cs_indexed_t *  pindexed = ppalet->pindexed;
    pcl_encoding_type_t penc = pcl_cs_indexed_get_encoding(pindexed);
    pcl_seed_row_t *    pseed_rows = 0;

    /* there can only be one raster object present at a time */
    if (prast != 0)
        pcl_complete_raster(pcs);

    prast = gs_alloc_struct( pcs->memory,
                             pcl_raster_t,
                             &st_raster_t,
                             "start PCL raster"
                             );
    if (prast == 0)
        return e_Memory;

    prast->pmem = pcs->memory;

    if ( pcs->source_transparent || pcs->pattern_transparent) 
        prast->transparent = true;
    else
        prast->transparent = false;

    prast->interpolate = pcs->interpolate;
    prast->src_height_set = pcs->raster_state.src_height_set;
    prast->pcs = pcs;
    pcl_cs_indexed_init_from(prast->pindexed, pindexed);

    prast->pen = 0;
    prast->plane_index = 0;
    prast->rows_rendered = 0;
    prast->src_width = src_width;
    prast->src_height = src_height;
    prast->mask_pen = 0;
    prast->mask_pindexed = 0;
    prast->gen_mask_row = 0;

    /* the conslidation and mask buffers are created when first needed */
    prast->cons_buff = 0;
    prast->mask_buff = 0;

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
        if (i < nplanes) {

            /* memory exhaustion; release the already allocated seed rows */
            for (j = 0; j < i; j++)
                gs_free_object( prast->pmem, 
                                pseed_rows[j].pdata,
                                "start PCL raster"
                                );
            gs_free_object(prast->pmem, pseed_rows, "start PCL raster");
            pseed_rows = 0;
        }
    }

    /* check for memory exhaustion */
    if (pseed_rows == 0) {
        gs_free_object(prast->pmem, prast, "start PCL raster");
        return e_Memory;
    }

    prast->pseed_rows = pseed_rows;
    pcs->raster_state.pcur_raster = (pcl_raster_type *)prast;

    /* see if a mask is required */
    if ( !pcs->source_transparent                                      &&
         pcs->pattern_transparent                                      &&
         (!prast->indexed                                              || 
          (prast->wht_indx
                    < (1 << prast->nplanes * prast->bits_per_plane))  )  ) {

        if (!prast->indexed) {
            ulong   white_val = 0UL;

            /* direct by plane or by pixel, one or 8 bits per primary */
            prast->gen_mask_row = (prast->nsrcs > 1 ? gen_mask_multisrc
                                                    : gen_mask_multibyte);
            if (prast->pindexed->Decode[1] == 1.0)
                white_val |= ((ulong)0xff) << 16;
            if (prast->pindexed->Decode[3] == 1.0)
                white_val |= ((ulong)0xff) << 8;
            if (prast->pindexed->Decode[5] == 1.0)
                white_val |= 0xff;
            prast->white_val = white_val;

        } else if ((prast->nplanes > 1) || (prast->bits_per_plane == 8)){

            /* indexed by plane or direct by pixel, 8 bits per pixel */
            prast->gen_mask_row = gen_mask_1byte;
            prast->white_val = prast->wht_indx;

        } else {
            ulong   white_val = prast->wht_indx;
            int     n = 8 / prast->bits_per_plane;

            /* indexed by pixel, < 8 bits per pixel */
            prast->gen_mask_row = gen_mask_subbyte;
            while (n-- > 0)
                white_val |= (white_val << prast->bits_per_plane);
            prast->white_val = white_val;
        }
    }

    return 0;
}

/*
 * Complete a raster. This is called when exiting graphics mode.
 */
  void
pcl_complete_raster(pcl_state_t *pcs)
{
    pcl_raster_t *  prast = (pcl_raster_t *)pcs->raster_state.pcur_raster;
    int             i;

    /* if already in raster mode, ignore */
    if (prast == 0)
        return;

    /* close the current raster */
    close_raster(pcs->pgs, prast, true);

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
    if (prast->mask_pindexed != 0) {
        pcl_cs_indexed_release(prast->mask_pindexed);
        prast->mask_pindexed = 0;
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
    if (prast->mask_buff != 0)
        gs_free_object(prast->pmem, prast->mask_buff, "Complete PCL raster");
    

    /* free the PCL raster robject itself */
    gs_free_object(prast->pmem, prast, "Complete PCL raster");
    pcs->raster_state.pcur_raster = 0;
}

/*
 * ESC * b # V
 *
 * Add a plane buffer to the current set.
 */
  static int
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
  static int
transfer_raster_row(
    pcl_args_t *    pargs,
    pcl_state_t *   pcs
)
{
    const byte *    pdata = arg_data(pargs);
    int             comp_mode = pcs->raster_state.compression_mode;
    int             code = 0;

    code = add_raster_plane(pdata, arg_data_size(pargs), true, pcs);

    /* complete the row (execpt for adaptive compression) */
    if (comp_mode != ADAPTIVE_COMPRESS && code == 0)
        code = process_row((pcl_raster_t *)pcs->raster_state.pcur_raster, comp_mode);

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
  static int
raster_y_offset(
    pcl_args_t *    pargs,
    pcl_state_t *   pcs
)
{
    pcl_raster_t *  prast = (pcl_raster_t *)pcs->raster_state.pcur_raster;

    /* ignored outside of graphics mode */
    if ((prast != 0) && (uint_arg(pargs) > 0)) {
        return process_zero_rows(pcs->pgs, prast, uint_arg(pargs));
    } else
        return 0;
}

/*
 * ESC * b <direction> L
 *
 * set raster print direction
 */
  static int
set_line_path(
    pcl_args_t *    pargs,
    pcl_state_t *   pcs
)
{
    uint            i = uint_arg(pargs);

    if (i <= 1)
	pcs->raster_state.y_advance = (i == 1 ? -1 : 1);
    return 0;
}

/*
 * There is no specific copy code for this module, as both entry to and exit
 * from a macro must end graphics mode (and thus are handled by the parser).
 * There is also no explicit reset routine, as the required work is handled
 * at a higher level.
 */
  static int
raster_do_registration(
    pcl_parser_state_t *pcl_parser_state,
    gs_memory_t *   pmem    /* ignored */
)
{
    DEFINE_CLASS('*')
    {
        'b', 'V',
        PCL_COMMAND( "Transfer Raster Plane",
                     transfer_raster_plane,
                     pca_raster_graphics | pca_bytes | pca_in_rtl
                     )
    },
    {
        'b', 'W',
        PCL_COMMAND( "Transfer Raster Row",
                     transfer_raster_row,
                     pca_raster_graphics | pca_bytes | pca_in_rtl
                     )
    },
    {
        'b', 'Y',
        PCL_COMMAND( "Raster Y Offset",
                     raster_y_offset,
                     pca_raster_graphics | pca_neg_ok | pca_big_clamp | pca_in_rtl
                     )
    },
    {
	/* NB this command should *only* be exectuted in rtl but we
           use it in both rtl and pcl5 */
        'b', 'L',
        PCL_COMMAND( "Line Path",
                     set_line_path,
		     pca_neg_ok | pca_big_ignore | pca_in_rtl
                     )
    },
    END_CLASS
    return 0;
}

  static void
raster_do_reset(
    pcl_state_t *       pcs,
    pcl_reset_type_t    type
)
{
    if ((type & pcl_reset_initial) != 0)
        pcs->raster_state.pcur_raster = 0;
}

const pcl_init_t    rtraster_init = { raster_do_registration, raster_do_reset, 0 };
