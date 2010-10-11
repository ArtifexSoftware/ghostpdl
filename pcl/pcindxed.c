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

/* pcindexed.c - PCL indexed color space implementation */
#include "math_.h"
#include "string_.h"
#include "gx.h"
#include "pcmtx3.h"
#include "pccid.h"
#include "pccsbase.h"
#include "pcpalet.h"

/* default GL/2 pen width, in plotter units (1016 ploter units per inch) */
static const float dflt_pen_width = 14.0;

/* the image data configuration for the default color space */
static const pcl_cid_hdr_t  dflt_cid_hdr = {
    pcl_cspace_RGB,                /* color space type */
    pcl_penc_indexed_by_plane,     /* pixel encoding type */
    1,                             /* bits per index */
    { 1, 1, 1 }                        /* bits per primary (3 components) */
};

/*
 * GC routines
 */
  static
ENUM_PTRS_BEGIN(pcl_cs_indexed_enum_ptrs)
        return 0;
    ENUM_PTR(0, pcl_cs_indexed_t, pbase);
    ENUM_PTR(1, pcl_cs_indexed_t, pcspace);
    ENUM_STRING_PTR(2, pcl_cs_indexed_t, palette);
ENUM_PTRS_END

  static
RELOC_PTRS_BEGIN(pcl_cs_indexed_reloc_ptrs)
    RELOC_PTR(pcl_cs_indexed_t, pbase);
    RELOC_PTR(pcl_cs_indexed_t, pcspace);
    RELOC_STRING_PTR(pcl_cs_indexed_t, palette);
RELOC_PTRS_END

private_st_cs_indexed_t();


/*
 * Find the smallest non-negative integral exponent of 2 larger than 
 * or equal to the given number; 8 => 2^3 12 => 2^4
 * Note: in/out should be unsigned.
 */
  static int
get_pow_2(
    int     num
)
{
    int      i;
    unsigned power_2 = 1;

    for (i = 0; (unsigned)num > power_2; ++i)  
	power_2 <<= 1;
    return i;
}

/*
 * Free a PCL indexed color space structure.
 */
  static void
free_indexed_cspace(
    gs_memory_t *       pmem,
    void *              pvindexed,
    client_name_t       cname
)
{
    pcl_cs_indexed_t *  pindexed = (pcl_cs_indexed_t *)pvindexed;

    pcl_cs_base_release(pindexed->pbase);
    rc_decrement(pindexed->pcspace, "free_indexed_cspace");
    gs_free_object(pmem, pvindexed, cname);
}

/*
 * Allocate a PCL indexed color space.
 *
 * Because a PCL indexed color space and the associated graphic library
 * indexed color space must be kept in a one-to-one relationship, the latter
 * color space is allocated here as well. This requires that the base color
 * space be an operand.
 *
 * Returns 0 on success, < 0 in the event of an error.
 */
  static int
alloc_indexed_cspace(
    pcl_cs_indexed_t ** ppindexed,
    pcl_cs_base_t *     pbase,
    gs_memory_t *       pmem
)
{
    pcl_cs_indexed_t *  pindexed = 0;
    int                 code = 0;
    byte *              bp = 0;
    rc_alloc_struct_1( pindexed,
                       pcl_cs_indexed_t,
                       &st_cs_indexed_t,
                       pmem,
                       return e_Memory,
                       "allocate pcl indexed color space"
                       );
    pindexed->rc.free = free_indexed_cspace;
    pindexed->pfixed = false;
    pindexed->is_GL = false;
    pcl_cs_base_init_from(pindexed->pbase, pbase);
    pindexed->pcspace = 0;
    pindexed->num_entries = 0;
    pindexed->palette.data = 0;
    pindexed->palette.size = 0;

    bp = gs_alloc_string( pmem,
                          3 * pcl_cs_indexed_palette_size,
                          "allocate pcl indexed color space"
                          );
    if (bp == 0) {
        free_indexed_cspace(pmem, pindexed, "allocate pcl indexed color space");
        return e_Memory;
    }
    pindexed->palette.data = bp;
    pindexed->palette.size = 3 * pcl_cs_indexed_palette_size;

    code = gs_cspace_build_Indexed( &(pindexed->pcspace),
                                    pbase->pcspace,
                                    pcl_cs_indexed_palette_size,
                                    (gs_const_string *)&(pindexed->palette),
                                    pmem
                                    );
    if (code < 0) {
        free_indexed_cspace(pmem, pindexed, "allocate pcl indexed color space");
        return code;
    }

    *ppindexed = pindexed;
    return 0;
}

/*
 * Make a PCL indexed color space unique.
 *
 * Note that neither the palette nor the graphic state indexed color space can
 * be shared between two PCL indexed color spaces. The two would need to be
 * shared in tandem, which would require common reference counting between the
 * pair, which would require yet another PCL object, and so on.
 *
 * Returns 0 on success, < 0 in the event of an error.
 */
  static int
unshare_indexed_cspace(
    pcl_cs_indexed_t ** ppindexed
)
{
    pcl_cs_indexed_t *  pindexed = *ppindexed;
    pcl_cs_indexed_t *  pnew = 0;
    int                 code = 0;
    int                 num_entries = pindexed->num_entries;

    /* check if there is anything to do */
    if (pindexed->rc.ref_count == 1)
        return 0;
    rc_decrement(pindexed, "unshare PCL indexed color space");

    /* allocate a new indexed color space */
    code = alloc_indexed_cspace( ppindexed,
                                 pindexed->pbase,
                                 pindexed->rc.memory
                                 );
    if (code < 0)
        return code;
    pnew = *ppindexed;

    /* copy fields various and sundry */
    pnew->pfixed = pindexed->pfixed;
    pnew->cid = pindexed->cid;
    pnew->original_cspace = pindexed->original_cspace;
    pnew->num_entries = pindexed->num_entries;
    pnew->palette.size = pindexed->palette.size;
    memcpy(pnew->palette.data, pindexed->palette.data, pindexed->palette.size);
    memcpy(pnew->pen_widths, pindexed->pen_widths, num_entries * sizeof(float));
    memcpy(pnew->norm, pindexed->norm, 3 * sizeof(pindexed->norm[0]));
    memcpy(pnew->Decode, pindexed->Decode, 6 * sizeof(float));

    return 0;
}


/*
 * Fill in the default entries in a color palette. This is handled separately
 * for each color space type.
 *
 * The manner in which the default color palette is specified by HP is,
 * unfortunately, completely dependent on their particular implementation.
 * HP maintains palettes in device space, and initializes palettes with
 * device space colors. Hence, these palettes are affected by gamma correction
 * and the lookup tables for the device color space, but not by any lookup
 * tables for device independent color spaces (even if the base color space is
 * device independent), nor by range specifications in the color space
 * parameters.
 *
 * Because this implementation of PCL 5c works strictly in source color space,
 * it is not possible to achieve the same result. Instead, this implementation
 * will give accurate default color space values only for the RGB and CMY
 * color space values, and will give approximate results for the other color
 * spaces. The default colors will be modified by all applicable color lookup
 * tables, though some compensation is provided for component ranges.
 *
 * For applications, this should not matter: presumably an application does
 * not select a device independent color space in order to achieve a device-
 * specific color. For tests such as the PCL 5c FTS, it is more likely that
 * a discrepancy will be visible, because these tests illustrate the default
 * palette for each color space.
 */

/*
 * Convert a device-independent color intensity value to the range [0, 255].
 */
  static int
convert_comp_val(
    floatp  val,
    floatp  min_val,
    floatp  range
)
{
    val = 255.0 * (val - min_val) / range;
    return ( val < 0.0 ? 0 : (val > 255.0 ? 255 : (int)floor(val + 0.5)) );
}

/*
 * Set the default palette for device specific color spaces.
 */
  static void
set_dev_specific_default_palette(
    pcl_cs_base_t *     pbase,      /* ignored in this case */
    byte *              palette,
    const byte *        porder,
    int                 start,
    int                 num
)
{
    int                 i;
    static const byte   cmy_default[8 * 3] = {
                                255, 255, 255,  /* white */
                                  0, 255, 255,  /* cyan */
                                255,   0, 255,  /* magenta */
                                  0,   0, 255,  /* blue */
                                255, 255,   0,  /* yellow */
                                  0, 255,   0,  /* green */
                                255,   0,   0,  /* red */
                                  0,   0,   0   /* black */
                            };

    /* fill in the num_entries - 1 values from the RGB default */
    for (i = start; i < num + start; i++) {
        palette[3 * i] = cmy_default[3 * porder[i]];
        palette[3 * i + 1] = cmy_default[3 * porder[i] + 1];
        palette[3 * i + 2] = cmy_default[3 * porder[i] + 2];
    }
}

/*
 * Set the default palette for a colorimetric RGB space.
 *
 * The assumption used in this case is that if the user specified primaries,
 * presumably they want those primaries. Further, it is assumed that the red
 * primary is associated with red, the green with green, and so on. Should
 * the user specify a "blue" color as the red primary, then the default
 * palette will have blue where red is normally expected.
 *
 * An adjustment is made for the specified ranges.
 */
  static void
set_colmet_default_palette(
    pcl_cs_base_t *     pbase,
    byte *              palette,
    const byte *        porder,
    int                 start,
    int                 num
)
{
    float *             pmin = pbase->client_data.min_val;
    float *             prange = pbase->client_data.range;
    int                 i;
    static const float  colmet_default[8 * 3] = {
                              1.0,    1.0,    1.0,  /* white */
                              0.0,    1.0,    1.0,  /* cyan */
                              1.0,    0.0,    1.0,  /* magenta */
                              0.0,    0.0,    1.0,  /* blue */
                              1.0,    1.0,    0.0,  /* yellow */
                              0.0,    1.0,    0.0,  /* green */
                              1.0,    0.0,    0.0,  /* red */
                              0.0,    0.0,    0.0   /* black */
                        };

    /* fill in the num_entries - 1 values from the colorimetric default */
    for (i = start; i < start + num; i++) {
        int             j;
        byte *          pb = palette + 3 * i;
        const float *   pdef = colmet_default + 3 * porder[i];

        for (j = 0; j < 3; j++)
            pb[j] = convert_comp_val(pdef[j], pmin[j], prange[j]);
    }
}

/*
 * Set the default palette for a CIE L*a*b* color space.
 *
 * The values provided will, for a sufficiently capable device, generate the
 * SMPTE-C primaries (assuming that ranges and/or color lookup tables don't
 * get in the way). Most printing devices cannot generate these colors, so
 * a somewhat different output is produced, but this is as close as we can
 * come to a "device independent" set of default entries.
 *
 * The code provides some compensation for range: if the desired value is 
 * within the permitted range, the palette entry intensity (always in the range
 * [0, 1]) will be adjusted so as to achieve it; otherwise the intensity will
 * be set to the appropriate bound. Obviously, none of this works if a
 * color lookup table is subsequently installed, but it is as much as can be
 * achieved under the current arrangement.
 */
  static void
set_CIELab_default_palette(
    pcl_cs_base_t *     pbase,
    byte *              palette,
    const byte *        porder,
    int                 start,
    int                 num
)
{
    float *             pmin = pbase->client_data.min_val;
    float *             prange = pbase->client_data.range;
    int                 i;
    static const float  lab_default[8 * 3] = {
                            100.0,    0.0,    0.0,  /* white */
                             91.1,  -43.4,  -14.1,  /* cyan */
                             61.6,   91.0,  -59.2,  /* magenta */
                             35.3,   72.0, -100.0,  /* blue */
                             96.6,  -21.3,   95.4,  /* yellow */
                             87.0,  -80.7,   84.0,  /* green */
                             53.2,   74.4,   67.7,  /* red */
                              0.0,    0.0,    0.0   /* black */
                        };

    for (i = start; i < start + num; i++) {
        int             j;
        byte *          pb = palette + 3 * i;
        const float *   pdef = lab_default + 3 * porder[i];

        for (j = 0; j < 3; j++)
            pb[j] = convert_comp_val(pdef[j], pmin[j], prange[j]);
    }
}

/*
 * Set the default palette for a luminance-chrominance color space.
 *
 * The arrangement used for this color space is based on the one used for
 * the colorimetric color space. The user specifies a set of primaries
 * for the color space underlying the luminance-chrominance color space,
 * and we provide those primaries in the default palette. Since the
 * palette entries themselves are in the luminance-chrominance color space,
 * the default values must be converted to that color space.
 * 
 * An adjustment is made for the specified ranges.
 */
  static void
set_lumchrom_default_palette(
    pcl_cs_base_t *             pbase,
    byte *                      palette,
    const byte *                porder,
    int                         start,
    int                         num
)
{
    gs_matrix3 *                pxfm = gs_cie_abc_MatrixABC(pbase->pcspace);
    float *                     pmin = pbase->client_data.min_val;
    float *                     prange = pbase->client_data.range;
    pcl_mtx3_t                  tmp_mtx;
    int                         i;
    static  const pcl_vec3_t    lumchrom_default[8] = {
	                          { 1.0, 1.0, 1.0 },    /* white */
                                  { 0.0, 1.0, 1.0 },    /* cyan */
                                  { 1.0, 0.0, 1.0 },    /* magenta */
                                  { 0.0, 0.0, 1.0 },    /* blue */
                                  { 1.0, 1.0, 0.0 },    /* yellow */
                                  { 0.0, 1.0, 0.0 },    /* green */
                                  { 1.0, 0.0, 0.0 },    /* red */
                                  { 0.0, 0.0, 0.0 }     /* black */
                            };

    /* form the primaries to component values matrix */
    pcl_mtx3_convert_from_gs(&tmp_mtx, pxfm);
    pcl_mtx3_invert(&tmp_mtx, &tmp_mtx);

    for (i = start; i < start + num; i++) {
        pcl_vec3_t  compvec;
        byte *      pb = palette + 3 * i;
        int         j;

        pcl_vec3_xform(&(lumchrom_default[porder[i]]), &compvec, &tmp_mtx);
        for (j = 0; j < 3; j++)
            pb[j] = convert_comp_val(compvec.va[j], pmin[j], prange[j]);
    }
}

/*
 * Set the default values for a specific range of an indexed PCL color space.
 * The starting point is indicated by start; the number of subsequent entires
 * to be set by num (for PCL, start is always 0 and num is always the number of
 * entires in the palette, but that may not be the case for GL/2).
 *
 * If gl2 is true, this call is being made from GL/2, hence the GL/2 default
 * palettes should be used.
 *
 * Returns 0 if successful, < 0 in case of an error.
 */
  static int
set_default_entries(
    pcl_cs_indexed_t  * pindexed,
    int                 start,
    int                 num,
    bool                gl2
)
{
    /* array of procedures to set the default palette entries */
    static void         (*const set_default_palette[(int)pcl_cspace_num])(
                                                pcl_cs_base_t * pbase,
                                                byte *          palette,
                                                const byte *    porder,
                                                int             start,
                                                int             num 
                                                                    ) = {
                        set_dev_specific_default_palette,   /* RGB */
                        set_dev_specific_default_palette,   /* CMY */
                        set_colmet_default_palette,         /* colorimetric RGB */
                        set_CIELab_default_palette,         /* CIE L*a*b* */
                        set_lumchrom_default_palette        /* luminance-
                                                             * chrominance */
                        };

    /*
     * For each color space, 8 palette entries are stored in the canonical
     * CMY order; any palette entries beyond the first 8 always default to
     * black. These arrays are incorporated into procedures that handle the
     * generation of colors for each color space type. For the bits per index
     * settings of 1, 2, or >= 3, an order array is provided, to indicate the
     * order in which the default palette entries should be entered into the
     * palette. Separate arrays are provided for the RGB and CMY color spaces,
     * and for GL/2 default colors.
     */
    static const byte   order_1[] = { 0, 7 };
    static const byte   cmy_order_2[] = { 0, 1, 2, 7 };
    static const byte   cmy_order_3[] = { 0, 1, 2, 3, 4, 5, 6, 7 };
    static const byte   rgb_order_2[] = { 7, 6, 5, 0 };
    static const byte   rgb_order_3[] = { 7, 6, 5, 4, 3, 2, 1, 0 };
    static const byte   gl2_order_2[] = { 0, 7, 6, 5 };
    static const byte   gl2_order_3[] = { 0, 7, 6, 5, 4, 3, 2, 1 };
    static const byte * cmy_order[3] = {order_1, cmy_order_2, cmy_order_3};
    static const byte * rgb_order[3] = {order_1, rgb_order_2, rgb_order_3};
    static const byte * gl2_order[3] = {order_1, gl2_order_2, gl2_order_3};

    int                 type = pindexed->cid.cspace;
    int                 orig_type = pindexed->original_cspace;
    int                 bits = pindexed->cid.bits_per_index - 1;
    const byte *        porder;
    int                 cnt = (num + start > 8 ? 8 - start : num);
    int                 i;

    if (bits > 2)
        bits = 2;
    if (gl2)
        porder = gl2_order[bits];
    /* check for a rgb or colorimetric.  If the colorimetric is being
       substituted for device CMY use the cmy order */
    else if (((type == pcl_cspace_RGB) || (type == pcl_cspace_Colorimetric)) && (orig_type != pcl_cspace_CMY) )
        porder = rgb_order[bits];
    else
        porder = cmy_order[bits];

    /* set the default colors for up to the first 8 entries */
    set_default_palette[(int)type]( pindexed->pbase,
                                    pindexed->palette.data,
                                    porder,
                                    start,
                                    cnt
                                    );

    /* all other entries are black (always comp. value 0).  For
       simplicity we reset all of the the remaining pallete data. */
    if (num > cnt) {
        int bytes_initialized = 3 * (start + cnt);
        int bytes_left = (3 * pcl_cs_indexed_palette_size) - bytes_initialized;
        memset(pindexed->palette.data + bytes_initialized, 0, bytes_left);
    }

    /* set the default widths */
    for (i = start; i < num; i++)
        pindexed->pen_widths[i] = dflt_pen_width;

    return 0;
}

/*
 * Generate the normalization and, if appropriate, Decode arrays that correspond
 * to a pcl_cid_data structure.
 *
 * Normalization in PCL and GL/2 involves a modification of a component color
 * setting prior to its being stored in the palette. This is set in PCL via
 * the black and white reference points for device specific color spaces, and
 * in GL/2 via the CR command. The latter provides the much more general
 * interface, which in turn drives the implementation.
 *
 * In PCL, white and black reference points may be set only for the device-
 * dependent color spaces, and may only be set once when a palette is created
 * (via the configure image data command; the min/max ranges for the device
 * independent color spaces have a different interpretation). The CR command,
 * on the other hand, applies to all color spaces, and can used to modfiy the
 * range without otherwise changing the current color palette.
 *
 * All color spaces in this implementation are set up so that, in the palette,
 * 0 represents the minimum intensity, and 1 the maximum intensity. For a
 * component that has black and white reference points of blk and wht,
 * respectively, the normalization applied is:
 *
 *     tmp_val = ((i_val - blk) / (wht - blk)
 *
 *     o_val = (tmp_val < 0 ? 0 : (tmp_val > 1 ? 1 : tmp_val))
 *
 * Because palette entries are stored as integers in the range [0, 255], the
 * actual form in which the normalization data are stored (and the modified
 * normalization calculation) are:
 *
 *     blkref = blk
 *
 *     inv_range = 255.0 / (wht - blk)
 *
 *     tmp_val = (i_val - blkref) * inv_range + 0.5;
 *
 *     o_val = (tmp_val < 0 ? 0 : (tmp_val > 255 ? 255 : floor(tmp_val + 0.5)))
 *
 * For a primary that uses an n-bit representation, the default values for
 * blk and wht are 0 and 2^n - 1, respectively. (For HP's implementation this
 * only applies to device-dependent color spaces; wht for device-independent
 * color spaces is always 255. There seems to be no reason for such a
 * restriction, hence it is not used here. This is unlikely to cause difficulty
 * in practice, as it is unlikely a device-independent color space will ever
 * be used with anything other than 8-bits per primary.)
 *
 * Note that for the CMY color space, the white and black points are reversed.
 * This is the only distinction between the RGB and CMY color spaces.
 *
 * The Decode array is used for images, and thus has different forms
 * depending on the pixel encoding mode. For the "direct by" cases
 * it incorporates normalization information; for the "indexed by"
 * cases its contents are dictated by the size of the palette.
 */
  int
pcl_cs_indexed_set_norm_and_Decode(
    pcl_cs_indexed_t **     ppindexed,
    floatp                  wht0,
    floatp                  wht1,
    floatp                  wht2,
    floatp                  blk0,
    floatp                  blk1,
    floatp                  blk2
)
{
    pcl_cs_indexed_t *      pindexed = *ppindexed;
    pcl_encoding_type_t     enc = (pcl_encoding_type_t)pindexed->cid.encoding;
    pcl_cs_indexed_norm_t * pnorm;
    int                     code = 0;

    /* ignore request if palette is fixed */
    if (pindexed->pfixed)
        return 0;

    /* get a unique copy of the color space */
    if ((code = unshare_indexed_cspace(ppindexed)) < 0)
        return code;
    pindexed = *ppindexed;
    pnorm = pindexed->norm;

    /* set up for the additive space */
    pnorm[0].blkref = blk0;
    pnorm[0].inv_range = (wht0 == blk0 ? 0.0 : 255.0 / (wht0 - blk0));
    pnorm[1].blkref = blk1;
    pnorm[1].inv_range = (wht1 == blk1 ? 0.0 : 255.0 / (wht1 - blk1));
    pnorm[2].blkref = blk2;
    pnorm[2].inv_range = (wht2 == blk2 ? 0.0 : 255.0 / (wht2 - blk2));

    /*
     * Build the Decode array to be used with images.
     *
     * If an "indexed by" pixel encoding scheme is being used, the color
     * space for images is the same color space used for the foreground, and
     * the Decode array is the canonical array for Indexed color spaces.
     *
     * If a "direct by" pixel encoding is being used, the Decode array must
     * yield the same color component values as would be produced by use of
     * the "color component" commands. Thus, for any color component intensity
     * a, we must have
     *
     *    (a - blkref) / range = Dmin + a * (Dmax - Dmin) / (2^n - 1)
     *
     * where blkref and range are the black reference point and range value for
     * this component (computed above), Dmin and Dmax are the first and second
     * elements of the Decode array for this component, and n is the number of
     * bits per pixel. Setting a = 0, this yields:
     *
     *    -blkref / range = Dmin
     *
     * Substituting this into the original equation yields:
     *
     *    (a - blkref) / range = a * (Dmax + blkref/range) / (2^n - 1)
     *                                - blkref / range;
     *
     * Adding blkref / range to both sides,  and multiplying both sides by
     * range * (2^n - 1) / a yields:
     *
     *     2^n - 1 = range * Dmax + blkref
     *
     * or
     *
     *     (2^n - 1 - blkref) / range = Dmax
     *
     * Note that this arrangement requires the image/color space code to
     * properly handle out of range values.
     */
    if (enc >= pcl_penc_direct_by_plane) {
        int     i;
        float * pdecode = pindexed->Decode;

        for (i = 0; i < 3; i++) {
            int     nbits = pindexed->cid.bits_per_primary[i];
            floatp  inv_range = pnorm[i].inv_range;

            if (inv_range == 0.0)
                inv_range = 254;
            pdecode[2 * i] = -pnorm[i].blkref * inv_range / 255.0;
            pdecode[2 * i + 1] = ((float)((1L << nbits) - 1) - pnorm[i].blkref)
                                    * inv_range / 255.0;
        }
    } else {
        pindexed->Decode[0] = 0.0;
        pindexed->Decode[1] = 0.0;  /* modified subsequently */
    }
    return 0;
}

/*
 * Change the number of entries in an PCL indexed color space palette. For
 * PCL itself, this occurs only when a palette is created, and is determined
 * by the number of bits per index. The NP command in GL/2, on the other hand,
 * can override the palette size for an existing palette.
 *
 * The gl2 boolean indicates if this call is being made from GL/2 (either the
 * IN or NP command). The routine needs to know this so as to set the
 * appropriate default colors.
 *
 * Returns 0 on success, < 0 in the event of an error.
 */
  int
pcl_cs_indexed_set_num_entries(
    pcl_cs_indexed_t ** ppindexed,
    int                 new_num,
    bool                gl2
)
{
    pcl_cs_indexed_t *  pindexed = *ppindexed;
    int                 bits = get_pow_2(new_num);
    int                 old_num = pindexed->num_entries;
    int                 code = 0;

    /* ignore request if palette is fixed */
    if (pindexed->pfixed)
        return 0;

    pindexed->is_GL = gl2;

    /*
     * Set new_num to the smallest larger power of 2 less than
     * pcl_cs_indexed_palette_size.
     */
    bits = ( bits > pcl_cs_indexed_palette_size_log 
                 ? pcl_cs_indexed_palette_size_log
                 : bits );
    new_num = 1L << bits;

    /* check if there is anything to do */
    if (new_num == old_num)
        return 0;

    /* make sure the palette is unique */
    if ((code = unshare_indexed_cspace(ppindexed)) < 0)
        return code;
    pindexed = *ppindexed;
    pindexed->num_entries = new_num;
    pindexed->cid.bits_per_index = bits;

    /* check if the Decode array must be updated */
    if (pindexed->cid.encoding < pcl_penc_direct_by_plane)
        pindexed->Decode[1] = new_num - 1;

    /* if the palette grew, write in default colors and widths */
    if (new_num > old_num)
        set_default_entries(pindexed, old_num, new_num, gl2);
    return 0;
}

/*
 * Update the lookup table information for an indexed color space.
 *
 * Because lookup tables can modify base color spaces, this operation is
 * rather ugly. If the base color space is changed, it is necessary to release
 * and re-build the graphic library indexed color space as well, as this will
 * now have references to obsolete parts of the graphic library base color
 * space.
 *
 * Fortunately, this operation does not happen very often.
 *
 * Returns 0 if successful, < 0 in the event of an error.
 */
  int
pcl_cs_indexed_update_lookup_tbl(
    pcl_cs_indexed_t ** ppindexed,
    pcl_lookup_tbl_t *  plktbl
)
{
    pcl_cs_indexed_t *  pindexed = *ppindexed;
    pcl_cspace_type_t   cstype = (pcl_cspace_type_t)pindexed->cid.cspace;
    pcl_cspace_type_t   lktype;
    int                 code = 0;

    /* make some simple checks for not-interesting color spaces */
    if (plktbl != 0)
        lktype = pcl_lookup_tbl_get_cspace(plktbl);
    if ( (plktbl != 0)                                            &&
         ((cstype < lktype) || (lktype < pcl_cspace_Colorimetric))  )
        return 0;

    /* make a unique copy of the indexed color space */
    if ((code = unshare_indexed_cspace(ppindexed)) < 0)
        return code;
    pindexed = *ppindexed;

    /* update the base color space, if appropriate */
    code = pcl_cs_base_update_lookup_tbl(&(pindexed->pbase), plktbl);
    if (code <= 0)
        return code;

    /* a positive return code indicates we have to rebuild the
       palette.  First copy the paletted data, it will be freed when
       the color space is released. */
    {
        uint size = 3 * pcl_cs_indexed_palette_size;
        byte *bp = gs_alloc_string(pindexed->rc.memory, size,
                                   "pcl_cs_indexed_update_lookup_tbl");
        if ( bp == NULL )
            return e_Memory;
        memcpy(bp, pindexed->palette.data, 3 * pcl_cs_indexed_palette_size);
        rc_decrement(pindexed->pcspace, "pcl_cs_indexed_update_lookup_tbl");
        pindexed->palette.data = bp;
    }

    /* now rebuild it */
    return gs_cspace_build_Indexed( &(pindexed->pcspace),
                                    pindexed->pbase->pcspace,
                                    pcl_cs_indexed_palette_size,
                                    (gs_const_string *)&(pindexed->palette),
                                    pindexed->rc.memory
                                    );
   
}

/*
 * Update an entry in the palette of a PCL indexed color space.
 *
 * Returns 0 on success, < 0 in the event of an error.
 */
  int
pcl_cs_indexed_set_palette_entry(
    pcl_cs_indexed_t ** ppindexed,
    int                 indx,
    const float         comps[3]
)
{
    pcl_cs_indexed_t *  pindexed = *ppindexed;
    int                 code;
    int                 i;

    /* ignore request if palette is fixed */
    if (pindexed->pfixed)
        return 0;

    /*
     * Verify that the index is in range. This code obeys HP's documentation,
     * and the implementation in the CLJ 5/5M. The DJ 1600C/CM behaves
     * differently; it sets indx = indx % num_entries, but only if indx is
     * non-negative.
     */
    if ((indx < 0) || (indx >= pindexed->num_entries))
        return e_Range;

    /* get a unique copy of the indexed color space */
    if ((code = unshare_indexed_cspace(ppindexed)) < 0)
        return code;
    pindexed = *ppindexed;    

    /* normalize and store the entry */
    indx *= 3;
    for (i = 0; i < 3; i++) {
        pcl_cs_indexed_norm_t * pn = &(pindexed->norm[i]);
        floatp                  val = comps[i];

        if (pn->inv_range == 0)
	    val = (val >= pn->blkref ? 255.0 : 0.0);
        else {
            val = (val - pn->blkref) * pn->inv_range;
            val = (val < 0.0 ? 0.0 : (val > 255.0 ? 255.0 : val));
        }
        pindexed->palette.data[indx + i] = (byte)val;
    }
    return 0;
}

/*
 * Default the contents of a palette entry.
 *
 * This request can only come from GL/2, hence there is no gl2 boolean.
 *
 * Returns 0 on success, < 0 in the event of an error.
 */
  int
pcl_cs_indexed_set_default_palette_entry(
    pcl_cs_indexed_t ** ppindexed,
    int                 indx
)
{
    pcl_cs_indexed_t *  pindexed = *ppindexed;
    int                 code;

    /*
     * Verify that the index is in range. This code obeys HP's documentation,
     * and the implementation in the CLJ 5/5M. The DJ 1600C/CM behaves
     * differently; it sets indx = indx % num_entries, but only if indx is
     * non-negative.
     */
    if ((indx < 0) || (indx >= pindexed->num_entries))
        return e_Range;

    /* get a unique copy of the indexed color space */
    if ((code = unshare_indexed_cspace(ppindexed)) < 0)
        return code;
    pindexed = *ppindexed;    

    return set_default_entries(*ppindexed, indx, 1, true);
}

/*
 * Set a pen width in a palette. Units used are still TBD.
 *
 * Returns 0 if successful, < 0 in case of error.
 */
  int
pcl_cs_indexed_set_pen_width(
    pcl_cs_indexed_t ** ppindexed,
    int                 pen,
    floatp              width
)
{
    pcl_cs_indexed_t *  pindexed = *ppindexed;
    int                 code;

    /* check for out-of-range pen */
    if ((pen < 0) || (pen > pindexed->num_entries))
        return e_Range;     /* probably should be a different error */

    if ((code = unshare_indexed_cspace(ppindexed)) < 0)
        return code;
    pindexed = *ppindexed;

    pindexed->pen_widths[pen] = width;
    return 0;
}

/*
 * Build a PCL indexed color space.
 *
 * To maintain the one-to-one relationship between the PCL indexed color space
 * and the graphic library indexed color space, the latter is created at the
 * same time as the former. Hence, the base PCL color space must be created
 * first (it is required to create the graphics library indexed color space),
 * and released once it has been referenced by the (PCL) indexed color space.
 *
 * The boolean gl2 indicates if this request came from the GL/2 IN command.
 *
 * Returns 0 if successful, < 0 in case of error.
 */

  int
pcl_cs_indexed_build_cspace(
    pcl_state_t *           pcs,
    pcl_cs_indexed_t **     ppindexed,
    const pcl_cid_data_t *  pcid,
    bool                    pfixed,
    bool                    gl2,
    gs_memory_t *           pmem
)
{
    pcl_cs_indexed_t *      pindexed = *ppindexed;
    pcl_cspace_type_t       type = pcl_cid_get_cspace(pcid);
    int                     bits = pcl_cid_get_bits_per_index(pcid);
    floatp                  wht_ref[3];
    floatp                  blk_ref[3];
    pcl_cs_base_t *         pbase = 0;
    bool                    is_default = false;
    int                     code = 0;
    /*
     * Check if the default color space is being requested. Since there are
     * only three fixed spaces, it is sufficient to check that palette is
     * fixed and has 1-bit per pixel.
     */
    if (pfixed && (pcid->u.hdr.bits_per_index == dflt_cid_hdr.bits_per_index)) {
        is_default = true;
        if (pcs->pdflt_cs_indexed != 0) {
            pcl_cs_indexed_copy_from(*ppindexed, pcs->pdflt_cs_indexed);
            return 0;
        }
    }

    /* release the existing color space, if present */
    if (pindexed != 0)
        rc_decrement(pindexed, "build indexed color space");

    /* build the base color space */
    if ((code = pcl_cs_base_build_cspace(&pbase, pcid, pmem)) < 0)
        return code;

    /* build the indexed color space */
    if ((code = alloc_indexed_cspace(ppindexed, pbase, pmem)) < 0) {
        pcl_cs_base_release(pbase);
        return code;
    }
    pindexed = *ppindexed;

    /* release our extra reference of the base color space */
    pcl_cs_base_release(pbase);
    pbase = 0;

    /* copy the header of the configure image data structure */
    pindexed->cid = pcid->u.hdr;

    /* copy in the original color space if there is a substitution in
       effect.  There will be the no color spaces type if no use cie
       substitution is in effect */
    pindexed->original_cspace = pcid->original_cspace;

    /* set up the normalization information */
    if ((pcid->len > 6) && (type < pcl_cspace_Colorimetric)) {
        const pcl_cid_dev_long_t *  pdev = &(pcid->u.dev);
        int                         i;

        for (i = 0; i < 3; i++) {
            wht_ref[i] = pdev->white_ref[i];
            blk_ref[i] = pdev->black_ref[i];
        }
    } else {
        int     i;

        for (i = 0; i < 3; i++) {
            wht_ref[i] = (1L << pcl_cid_get_bits_per_primary(pcid, i)) - 1; 
            blk_ref[i] = 0.0;
        }

        /* reverse for the CMY color space */
        if ( (type == pcl_cspace_CMY) || (pcid->original_cspace == pcl_cspace_CMY) ) {
            int     i;

            for (i = 0; i < 3; i++) {
                floatp  ftmp = wht_ref[i];

                wht_ref[i] = blk_ref[i];
                blk_ref[i] = ftmp;
            }
        }
    }
    pcl_cs_indexed_set_norm_and_Decode( ppindexed,
                                        wht_ref[0], wht_ref[1], wht_ref[2],
                                        blk_ref[0], blk_ref[1], blk_ref[2]
                                        );

    /* set the palette size and the default palette entries */
    pcl_cs_indexed_set_num_entries(ppindexed, 1L << bits, gl2);

    /* now can indicate if the palette is fixed */
    pindexed->pfixed = pfixed;

    /* record if this is the default */
    if (is_default)
        pcl_cs_indexed_init_from(pcs->pdflt_cs_indexed, pindexed);

    return 0;
}

/*
 * Build the default indexed color space. This function is usually called only
 * once, at initialization time.
 *
 * Returns 0 on success, < 0 
 */
  int
pcl_cs_indexed_build_default_cspace(
    pcl_state_t *               pcs,
    pcl_cs_indexed_t **         ppindexed,
    gs_memory_t *               pmem
)
{
    if (pcs->pdflt_cs_indexed == 0) {
        pcs->dflt_cid_data.len = 6;
        pcs->dflt_cid_data.u.hdr = dflt_cid_hdr;
        pcs->dflt_cid_data.original_cspace = pcl_cspace_num;
        return pcl_cs_indexed_build_cspace( pcs,
					    ppindexed,
                                            &pcs->dflt_cid_data,
                                            true,
                                            false,
                                            pmem
                                            );
    } else {
        pcl_cs_indexed_copy_from(*ppindexed, pcs->pdflt_cs_indexed);
        return 0;
    }
}

/*
 * Special indexed color space constructor, for building a 2 entry indexed color
 * space based on an existing base color space. The first color is always set
 * to white, while the second entry takes the value indicated by pcolor1.
 *
 * This reoutine is used to build the two-entry indexed color spaces required
 * for creating opaque "uncolored" patterns.
 */
  int
pcl_cs_indexed_build_special(
    pcl_cs_indexed_t **         ppindexed,
    pcl_cs_base_t *             pbase,
    const byte *                pcolor1,
    gs_memory_t *               pmem
)
{
    static const pcl_cid_hdr_t  cid = { pcl_cspace_White, /* ignored */
                                        pcl_penc_indexed_by_pixel,
                                        1,
                                        { 8, 8, 8}        /* ignored */ };
    static const floatp         wht_ref[3] = { 255.0, 255.0, 255.0 };
    static const floatp         blk_ref[3] = { 0.0, 0.0, 0.0 };

    pcl_cs_indexed_t *          pindexed;
    int                         i, code = 0;

    /* build the indexed color space */
    if ((code = alloc_indexed_cspace(ppindexed, pbase, pmem)) < 0)
        return code;
    pindexed = *ppindexed;
    pindexed->pfixed = false;
    pindexed->cid = cid;
    pindexed->num_entries = 2;

    /* set up the normalization information - not strictly necessary */
    pcl_cs_indexed_set_norm_and_Decode( ppindexed,
                                        wht_ref[0], wht_ref[1], wht_ref[2],
                                        blk_ref[0], blk_ref[1], blk_ref[2]
                                        );
    pindexed->Decode[1] = 1;

    for (i = 0; i < 3; i++) {
        pindexed->palette.data[i] = 255;
        pindexed->palette.data[i + 3] = pcolor1[i];
    }

    /* the latter are not strictly necessary */
    pindexed->pen_widths[0] = dflt_pen_width;
    pindexed->pen_widths[1] = dflt_pen_width;

    return 0;
}

/*
 * Install an indexed color space into the graphic state. If no indexed color
 * space exists yet, build a default color space.
 *
 * Returns 0 on success, < 0 in the event of an error.
 */
  int
pcl_cs_indexed_install(
    pcl_cs_indexed_t ** ppindexed,
    pcl_state_t *       pcs
)
{
    pcl_cs_indexed_t *  pindexed = *ppindexed;
    int                 code = 0;

    if (pindexed == 0) {
        code = pcl_cs_indexed_build_default_cspace(pcs, ppindexed, pcs->memory);
        if (code < 0)
            return code;
        pindexed = *ppindexed;
    }

    return gs_setcolorspace(pcs->pgs, pindexed->pcspace);
}

/*
 * Return true if the given entry in the color palette represents white,
 * false otherwise.
 *
 * As with many other parts of this code, the determination of what is "white"
 * is done, for practical reasons, in source color space.  HP's implementations
 * make the same determination in device color space (but prior to dithering).
 * In the absence of color lookup tables, the two will give the same result.
 * An inverting color lookup table will, however, cause the two approaches to
 * vary.
 */
  bool
pcl_cs_indexed_is_white(
    const pcl_cs_indexed_t *    pindexed,
    int                         indx
)
{
    const byte *                pb = 0;

    if (pindexed == 0)
        return true;
    if ((indx < 0) || (indx >= pindexed->num_entries))
        return false;
    pb = pindexed->palette.data + 3 * indx;
    return (pb[0] == 0xff) && (pb[1] == 0xff) && (pb[2] == 0xff);
}

/*
 * Return true if the given entry in the color palette is black, false
 * otherwise.
 *
 * The determination of "blackness" is made in source space, rather than in
 * device space (prior to dither). The latter would be more correct, but is
 * not as easily accomplished, and only in very unusual circumstances will the
 * two produce different results.
 */
  bool
pcl_cs_indexed_is_black(
    const pcl_cs_indexed_t *    pindexed,
    int                         indx
)
{
    const byte *                pb = 0;

    if ((pindexed == 0) || (indx < 0) || (indx >= pindexed->num_entries))
        return false;
    pb = pindexed->palette.data + 3 * indx;
    return (pb[0] == 0) && (pb[1] == 0) && (pb[2] == 0);
}

/*
 * One time initialization. This exists only because of the possibility that
 * BSS may not be initialized.
 */
  void
pcl_cs_indexed_init(pcl_state_t *pcs)
{
    pcs->pdflt_cs_indexed = 0;
}
