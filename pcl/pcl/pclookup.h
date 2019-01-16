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


/* pclookup.h - color lookup table structure for PCL 5c */

#ifndef pclookup_INCLUDED
#define pclookup_INCLUDED

#include "gx.h"
#include "gsstruct.h"
#include "gsrefct.h"
#include "pcommand.h"
#include "pccid.h"

/*
 * Though straightforward in principle, the color lookup table feature in
 * PCL 5c is made complicated due to its development history. Specifically,
 * while all PCL color lookup tables are involved in color mapping, the
 * nature of the mapping desired varies with each color space.
 *
 * 1. For the device specific color spaces (RGB and CMY), the color lookup
 *    table is a generalization of the gamma correction feature. Thus:
 *
 *        RGB and CMY color lookup tables override gamma correction,
 *        and vice-versa.
 *
 *        The index value to an RGB or CMY lookup table is formed from the
 *        post-normalization input intensity. In the 4-bit/pixel case with
 *        the canonical white and black points, the input value 7 would
 *        select the color rendering table entry with index in the range
 *        [119, 136], not the entry with index 7.
 *
 *    In addition, the RGB and CMY color spaces in PCL are not really
 *    different spaces, but the same space with white and black points
 *    reversed. Hence, the two lookup tables are also the same, but the
 *    latter is modified to work in an additive space:
 *
 *        tbl_rgb[i] = 255 - tbl_cmy[255 - i]
 *
 *    Note also that the gamma correction parameter is always interpreted
 *    in an additive space: gamma1 > gamma2 implies all colors rendered
 *    using gamma1 will be lighter (or possibly the same as) the
 *    corresponding color rendered with gamma2.
 *
 * 2. The lookup tables for the device independent color spaces are not
 *    generalizations for gamma correction and are intended to be used
 *    with gamma correction, and thus with the device-specific color
 *    space lookup functions.
 *
 *    The canonical ranges for the device independent color space components
 *    do not fit neatly into the [0, 255] range used by lookup tables, the
 *    image encoding, etc. PCL handles this problem by requiring that device
 *    independent color components be normalized by the application. Exactly
 *    what HP expects in these circumstances is not known, and does not
 *    seem to be implemented correctly on the CLJ 5/5M; this implementation
 *    will always assume that 0 corresponds to the minimum value and 255 to
 *    the maximum (for device independent color space components only; device
 *    dependent color space components are handled differently).
 *
 *    For the purposes of color lookup tables, this arrangement implies that
 *    for device independent color spaces, the raw input value (rounded
 *    to an integer) is used as an index to the color rendering table,
 *    without applying any normalization.
 *
 * 3. PCL allows a given palette to have multiple color lookup tables, as
 *    many as 4. Each color lookup table is associated with a color space,
 *    selected from the five supported color spaces.
 *
 *    There are more color spaces than color lookup tables that may be
 *    attached to a palette because the two device specific color spaces
 *    use the same color lookup table (which is also used by gamma correction).
 *    Associating a color lookup table with the RGB color space as opposed
 *    to the CMY color space only alters the way its entries are interpreted,
 *    using the equation given in Item (1) above.
 *
 *    HP's documentation implies that multiple device independent color lookup
 *    tables may be used for the same device independent color space.
 *    Unfortunately, while testing shows that multiple such tables are used,
 *    there is no consistency in their use. Furthermore, it is not entirely
 *    obvious how to use a CIE L*a*b* lookup table with a luminance-chrominance
 *    color space.
 *
 *    This implementation tries to achieve the most reasonable interpretation
 *    of device independent color spaces that might actually be useful:
 *
 *        The colorimetric RGB color space uses only the colorimetric
 *            lookup table
 *
 *        The CIE L*a*b* color space uses only the L*a*b* lookup table
 *
 *        The luminance-chrominance color space uses both the luminance-
 *            chrominance color lookup table and the colorimetric RGB
 *            color lookup table.
 *
 *    All of these spaces also use the device-dependent color lookup table.
 *
 * 4. A color lookup table applies when a color is USED, not when it is
 *    entered into the palette. This is a critical consideration from the
 *    point of view of an implementation, as it requires that the client-
 *    provided color values must be stored in the palette (possibly normalized
 *    in the case of device-specific color spaces).
 *
 * 5. Though gamma correction and color lookup tables for device specific
 *    color spaces overwrite each other, the former is inherited by a new
 *    palette from the current active palette, but the latter is not (all
 *    color lookup tables are cleared by the configure image data command).
 *
 *    The following text appears in HP's "PCL 5 Color Technical Reference
 *    Manual" (May, 1996 edition (part no. 5961-0940), p. 4-16:
 *
 *        Gamma correction is referred to in terms of device-dependent
 *        RGB. This command does not destroy the contents of the device-
 *        dependent color lookup tables, but setting a gamma value
 *        supercedes any lookup table input in either Device CMY or
 *        Device RGB
 *
 *    The significance of the statement "does not destroy" is difficult
 *    to determine, as color lookup tables are not inherited and there is
 *    no way to "turn-off" gamma correction (setting the gamma value to
 *    0 is equivalent to setting it to 1.0, and results in the unity
 *    map being used). Consequently, in this implementation setting a
 *    gamma correction discards the current device-specific color lookup
 *    table (if one is present).
 */

/*
 * Raw color lookup tables. Though color lookup tables exist for each color
 * space, the one for the two device-dependent color spaces is the same, and
 * is also the lookup table used for gamma correction. This lookup table is
 * implemented via transfer functions.
 *
 * This structure should only be accessed via the pcl_lookup_t structure.
 *
 * Note: this structure is defined by HP.
 */
typedef struct pcl__lookup_tbl_s
{
    byte cspace;                /* actually pcl_cspace_type_t */
    byte dummy;
    byte data[3 * 256];
} pcl__lookup_tbl_t;

/*
 * The pcl_lookup_t structure provides a reference-count header for a lookup
 * table. This is necessary because there is a way that lookup tables can
 * shared between PCL base color spaces (this could occur in the case of a
 * luminance-chrominance color space, which might have an instantiation with
 * two color tables that shares one of these color tables with another
 * instantiation).
 */
typedef struct pcl_lookup_tbl_s
{
    rc_header rc;
    const pcl__lookup_tbl_t *ptbl;
} pcl_lookup_tbl_t;

/*
 * Copy, init, and release macros.
 */
#define pcl_lookup_tbl_init_from(pto, pfrom)    \
    BEGIN                                       \
    rc_increment(pfrom);                        \
    (pto) = (pfrom);                            \
    END

#define pcl_lookup_tbl_copy_from(pto, pfrom)            \
    BEGIN                                               \
    if ((pto) != (pfrom)) {                             \
        rc_increment(pfrom);                            \
        rc_decrement(pto, "pcl_lookup_tbl_copy_from");  \
        (pto) = (pfrom);                                \
    }                                                   \
    END

#define pcl_lookup_tbl_release(plktbl)              \
    rc_decrement(plktbl, "pcl_lookup_tbl_release")

/*
 * Get the color space type of a lookup table.
 */
#define pcl_lookup_tbl_get_cspace(plktbl)           \
    ((pcl_cspace_type_t)((plktbl)->ptbl->cspace))

/*
 * Macro to retrieve a single-component color table pointer from a
 * pcl_lookup_hdr_t pointer.
 *
 * This macro is provided in preference to a conversion macro because the
 * transfer function objects require a table.
 */
#define pcl_lookup_tbl_get_tbl(plktbl, indx)                    \
    ( (plktbl) != 0 ? (plktbl)->ptbl->data + (indx) * 256 : 0 )

/*
 * External access to the color lookup table/gamma correction code.
 */
extern const pcl_init_t pcl_lookup_tbl_init;

#endif /* pclookup_INCLUDED */
