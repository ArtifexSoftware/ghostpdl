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

/* pcht.h - PCL halftone/rendering object */

#ifndef pcht_INCLUDED
#define pcht_INCLUDED

#include "gx.h"
#include "gsstruct.h"
#include "gsrefct.h"
#include "gsht1.h"
#include "gshtx.h"
#include "pcident.h"
#include "pcommand.h"
#include "pclookup.h"
#include "pcdither.h"


/*
 * Structure for built-in dither methods. "built-in" in this case refers to
 * items that cannot be modifiied by the user via the PCL interpreter; there
 * are also "fixed" dithers that are defined in the interpreter itself and
 * cannot be modified by either the output device or the user.
 *
 * Currently two types are recognized: threshold dithers (which have the
 * monotinicity property), and table dithers. Others may be added subsequently,
 * though this is a large task.
 *
 * As is the case for both PostScript and PCL, all dithers are set up for an
 * additive color spaces. This is also the organization expected by the (now
 * repaired) graphic library.
 *
 * For thresholds, data is organized as an array of bytes in (device-space)
 * by row, column, and then color plane. Each byte represents a threshold
 * level between 1 and 255. Hence, for a 3 (rgb) color 2 x 2 dither, the data
 * should be provided as:
 *
 *   red(0, 0), red(1, 0), red(0, 1), red(1, 1), green(0, 0), ..., blue(1, 1)
 * 
 * For table dithers, data is organized by (device-space) row, then column,
 * then level (the intensity level to which the given color plane corresponds),
 * then color plane. Data is one bit per pixel, high-order-bit leftmost, and
 * rows are rounded to byte boundaries. Any number of levels may be provided,
 * but the zero-intensity level (which, for subtractive color space devices,
 * is all 1's) should not be provided. Note also that some code in the graphic
 * library assumes that full intensity colors are pure (all 0's or all 1's),
 * so these may not be handled correctly if the full intensity dither has
 * both 0's and 1's.
 *
 * Thus, for a 3 color (rgb) 11 x 3 dither with 32 levels, data would be
 * organized as follows:
 *
 *   red_level_1(0..7, 0), red_level_1(8..10, 0),
 *   red_level_1(0..7, 1), red_level_1(8..10, 1),
 *   red_level_1(0..7, 2), red_level_1(8..10, 2),
 *   red_level_2(0..7, 0), red_level_2(8..10, 0),
 *   ...
 *   red_level_32(0..7, 2), red_level_32(8..10, 2),
 *   green_level_1(0..7, 0), green_level_1(8..10, 0),
 *   ...
 *   green_level_32(0..7, 2), green_level_32(8..10, 2),
 *   blue_level_1(0..7, 0), blue_level_1(8..10, 0),
 *   ...
 *   blue_level_32(0..7, 2), blue_level_32(8..10, 2)
 *
 *
 * Note that this module does NOT take ownership of built-in dither objects;
 * that is the responsibility of the caller.
 */
typedef struct pcl_ht_builtin_threshold_s {
    int             nplanes;        /* number of planes */
    int             height, width;  /* in device pixels */
    const byte *    pdata;
} pcl_ht_builtin_threshold_t;

typedef struct pcl_ht_builtin_table_dither_s {
    int             nplanes;        /* number of color planes */
    int             height, width;  /* in device pixels */
    int             nlevels;        /* number of levels in a plane; must be
                                       the same for all planes */
    const byte *    pdata;          /* width x height x num_levels x nplanes */
} pcl_ht_builtin_table_dither_t;

typedef enum {
    pcl_halftone_Threshold = 0,
    pcl_halftone_Table_Dither,
    pcl_halftone_num
} pcl_halftone_type_t;

#ifndef pcl_ht_builtin_dither_DEFINED
#define pcl_ht_builtin_dither_DEFINED
typedef struct pcl_ht_builtin_dither_s {
    pcl_halftone_type_t type;
    union {
        pcl_ht_builtin_threshold_t      thresh;
        pcl_ht_builtin_table_dither_t   tdither;
    }                   u;
} pcl_ht_builtin_dither_t;
#endif

#define private_st_ht_builtin_dither_t()                    \
    gs_private_st_composite( st_ht_builtin_dither_t,        \
                             pcl_ht_builtin_dither_t,       \
                             "pcl builtin dither object",   \
                             ht_dither_enum_ptrs,           \
                             ht_dither_reloc_ptrs           \
                             )

/*
 * Array of dithers and devices to be used for different rendering methods.
 *
 * The HT_FIXED flag indicates which methods may not be changed by the output
 * device. The ordered, clustered ordered, and user-defined dithers (both
 * color and monochrome versions) are in this category, because they must 
 * have predictable matrices for the logical operations to produce predictable
 * results.
 *
 * The HT_USERDEF flag indicates which methods make use of the user-defined
 * dither matrix.
 *
 * The HT_DECSPACE flag indicates which methods may be used with device
 * independent color spaces. If one of these methods is selected and a device
 * independent color space is set, the default rendering method is used
 * instead.
 *
 * The HT_IMONLY flag indicates that a rendering method applies only to
 * images. ***This feature is currently not supported***
 *
 * For each rendering method there is an associated color mapping method, to
 * be used with the devcmap color mapping device. A single device is used
 * rather than one device for each different mapping, as the the graphic
 * library provides no good technique for removing a device from a graphic
 * state (this is typically done by grestore, as is the case for PostScript).
 */

#define HT_NONE         0x0
#define HT_FIXED        0x1
#define HT_USERDEF      0x2
#define HT_DEVCSPACE    0x4
#define HT_IMONLY       0x8

typedef struct rend_info_s {
    uint                                flags;
    const pcl_ht_builtin_dither_t *     pbidither;
} pcl_rend_info_t;

/*
 * Client data structure for PCL halftones. This holds two pieces of
 * information: the gamma correction factor, and a pointer to the lookup table
 * for device specific color spaces. The former is used only if the latter is
 * null. The gamma correction factor must be kept, however, as it may be
 * inherited by newly created palettes, while the lookup table itself is
 * cleared (see the comment in pclookup.h for some notes on not fully
 * understood items in HP's documentation concerning these tables).
 *
 * The system maintains three of these objects, because a separate one is
 * required for each component of the (base) color space. The different
 * components are distinguished only by the comp_indx field.
 */
typedef struct pcl_ht_client_data_s {
    int                 comp_indx;
    float               inv_gamma;
    pcl_lookup_tbl_t *  plktbl;
} pcl_ht_client_data_t;

/*
 * Structure of the PCL halftone/render object.
 *
 * This structure contains a pair of halftone objects since, in principle,
 * PCL can simultaneously support two separate halftone techniques: one for
 * geometric objects (which use the foreground color), the other for images.
 * For the time being these two will always be the same.
 *
 * As is the case with all PCL objects that access modifiable reference
 * counted objects in gs, this must be kept in a one-to-one relationship with
 * the graphic library halftone objects. Hence, two of these objects will never
 * share a gs_ht structure. Unlike the colorspaces, however, there may be
 * extended periods of time when this structure has no associated graphic
 * library halftone structure.
 *
 * The id field is used to identify a specific halftone, and is updated whenever
 * the halftone changes. This is used to indicate when structures that depend
 * on the halftone must be updated.
 */
struct pcl_ht_s {
    rc_header               rc;
    pcl_ht_client_data_t    client_data[3];
    pcl_udither_t *         pdither;
    gs_string               thresholds[3];
    uint                    render_method;
    uint                    orig_render_method;
    bool                    is_gray_render_method;
    gs_ht *                 pfg_ht;
    gs_ht *                 pim_ht;
};

#define private_st_ht_t()                           \
    gs_private_st_composite( st_ht_t,               \
                             pcl_ht_t,              \
                             "pcl halftone object", \
                             ht_enum_ptrs,          \
                             ht_reloc_ptrs          \
                             )

#ifndef pcl_ht_DEFINED
#define pcl_ht_DEFINED
typedef struct pcl_ht_s         pcl_ht_t;
#endif

/*
 * The usual init, copy,and release macros.
 */
#define pcl_ht_init_from(pto, pfrom)    \
    BEGIN                               \
    rc_increment(pfrom);                \
    (pto) = (pfrom);                    \
    END

#define pcl_ht_copy_from(pto, pfrom)            \
    BEGIN                                       \
    if ((pto) != (pfrom)) {                     \
        rc_increment(pfrom);                    \
        rc_decrement(pto, "pcl_ht_copy_from");  \
        (pto) = (pfrom);                        \
    }                                           \
    END

#define pcl_ht_release(pht)             \
    rc_decrement(pht, "pcl_ht_release")

/*
 * The following routine is intended to initialize the forwarding devices used
 * for special render methods. Currently it only creates the built-in dither
 * arrays.
 */
void pcl_ht_init_render_methods(
    pcl_state_t *   pcs,
    gs_memory_t *   pmem
);

/*
 * Set up normal or monochrome print mode. The latter is accomplished by
 * remapping each of the rendering algorithms to its monochrome equivalent.
 * The handling of the snap-to-primaries rendering method (1) is almost
 * certianly wrong, but it is the best that can be done with the current
 * scheme.
 *
 * Note that the current rendering method must be set before this change
 * will take effect.
 */
void pcl_ht_set_print_mode(pcl_state_t *pcs, bool monochrome);

/*
 * Set the render method.
 *
 * Returns 0 on success, < 0 in the event of an error.
 */
int pcl_ht_set_render_method(pcl_state_t *pcs, pcl_ht_t ** ppht, uint render_method);


/** 
 * Remap render method to a gray render method iff enabled && palette is all gray 
 *
 * if the palette is gray remap the render_algorithm to a gray algo
 * if the palette is color use the original "color" render_algorithm
 * degenerates to NOP if ENABLE_AUTO_GRAY_RENDER_METHODS is false
 */ 
int pcl_ht_remap_render_method(pcl_state_t * pcs,
                               pcl_ht_t **ppht,
                               bool is_gray
                               );

/**
 * Checks if all palette entries are gray iff enabled.
 *
 * Returns true if all palette entries are gray
 * Returns false if any entry is color 
 * checks the entire palette
 * all gray palette ONLY has meaning if ENABLE_AUTO_GRAY_RENDER_METHODS is true
 * otherwise this is a NOP that always returns false.
 */
bool pcl_ht_is_all_gray_palette(pcl_state_t *pcs);

/*
 * Update the gamma parameter.
 *
 * Returns 0 on success, < 0 in the event of an error.
 */
int pcl_ht_set_gamma(pcl_ht_t ** ppht, float gamma);

/*
 * Update the color lookup table information. This takes action only for lookup
 * tables associated with device-dependent color spaces; other lookup tables
 * are handled via color spaces.
 *
 * Returns 0 on success, < 0 in the event of an error.
 */
int pcl_ht_set_lookup_tbl(
    pcl_ht_t **         ppht,
    pcl_lookup_tbl_t *  plktbl
);

/*
 * Set the user-defined dither matrix for a halftone object.
 *
 * Returns 0 on success, < 0 in the event of an error.
 */
int     pcl_ht_set_udither(
    pcl_ht_t **     ppht,
    pcl_udither_t * pdither
);

/*
 * Update the current halftone for a change in the color space.
 *
 * The color space usually does not affect the halftone, but it can in cases
 * in which a device-independent color space is used with a rendering method
 * that is not compatible with device-independent color spaces.
 */
int pcl_ht_update_cspace(
    pcl_state_t *       pcs,
    pcl_ht_t **         ppht,
    pcl_cspace_type_t   cstype_old,
    pcl_cspace_type_t   cstype_new
);

/*
 * Create the default halftone, releasing the current halftone if it exists.
 *
 * Returns 0 on success, < 0 in the event of an error.
 */
int pcl_ht_build_default_ht(
    pcl_state_t *       pcs,
    pcl_ht_t **         ppht,
    gs_memory_t *       pmem
);

/*
 * Set the given halftone into the graphic state. If the halftone doesn't
 * exist yet, create a default halftone and set it into the graphic state.
 *
 * Returns 0 on success, < 0 in the event of an error.
 */
int pcl_ht_set_halftone(
    pcl_state_t *        pcs,
    pcl_ht_t **          ppht,
    pcl_cspace_type_t    cstype,
    bool                 for_image
);

#endif  	/* pcht_INCLUDED */
