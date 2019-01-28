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


/* pcindxed.h - PCL indexed color space object */

#ifndef pcindexed_INCLUDED
#define pcindexed_INCLUDED

#include "gx.h"
#include "gsstruct.h"
#include "gsrefct.h"
#include "gscspace.h"
#include "pcident.h"
#include "pcstate.h"
#include "pccid.h"
#include "pclookup.h"
#include "pccsbase.h"

/*
 * Size of a PCL color palette. All palettes are allocated at this size, as
 * GL/2 may change the size of a palette after it has been created.
 *
 * The overall size must be a power of 2, and for all practical purposes must
 * be >= 8. The PCL documentation does not specify a value for this parameter,
 * though in chapter 7 of the "PCL 5 Color Technical Reference Manual" it
 * is indicated as being implementation specific. For all of the PCL 5c devices
 * we have tested, the value is 8.
 */
#define pcl_cs_indexed_palette_size_log 8
#define pcl_cs_indexed_palette_size     (1 << pcl_cs_indexed_palette_size_log)

/*
 * Structure to hold the parameters that normalize raw color values to values
 * usable for the base color space. The formula used is:
 *
 *     out = 255 * (in - blkref) / (whtref - blkref)
 *
 * The black and white reference points can be set in PCL only for the device-
 * dependent color spaces (the max/min parameters for device-independent color
 * spaces have a different interpretation). The parameters may be set for all
 * color spaces, however, via the CR command in GL/2.
 *
 * Note that for the CMY color space, the white and black reference points are
 * reversed.
 */
typedef struct pcl_cs_indexed_norm_s
{
    float blkref;
    float inv_range;            /* 255 / (whtref - blkref) */
} pcl_cs_indexed_norm_t;

/*
 * PCL indexed color space. This consists of:
 *
 *   A copy of the configure image data header (short form) which was used
 *       to create this color space (only the header is necessary; all the
 *       other relevant information is captured in the normalization and
 *       graphic library color space structures)
 *
 *   A PCL base color space, which in turn consists of a graphic library
 *       base color space and its associated client data (including lookup
 *       tables for device-independent color spaces)
 *
 *   A graphic library indexed color space
 *
 *   An indication of how many entries in the palette are currently
 *       "accessible" (this is required to support the GL/2 "NP" command)
 *
 *   The palette used by the indexed color space. This is always the maximum
 *       size, as GL/2 can change the number of entries in a palette without
 *       changing the palette (via the NP command)
 *
 *       In a reasonable world this palette would be a fixed size array of
 *       bytes in the (PCL) indexed color space structure. Unfortunately,
 *       the GhostScript graphic library is not a reasonable world in this
 *       respect. The table used by an indexed color space must be a string,
 *       which means it must be allocated separately. The gs_string structure
 *       kept in this structure is a pointer to this separately allocated
 *       string.
 *
 *   The set of pen widths. This has nothing to do with colors, but since
 *       pens in GL/2 are associated with palette entries in PCL, this is
 *       the logical place to put this information; issues concerning which
 *       units to use are still TBD.
 *
 *       Since there is no PostScript-centric library which must make use of
 *       these widths, they can be kept in a sensible array.
 *
 *   A boolean to indicate if a palette is fixed (with respect to colors; all
 *       palettes have settable pen widths).
 *
 *   A identifier, which is used to indicate when objects dependent on this
 *       color space must be regenerated. The identifier is incremented each
 *       time the color space is modified (but not when pen widths are
 *       modified, as the width information is not cached).
 *
 *   A structure for normalizing raw input values to component values in
 *       the palette; this supports the black/white reference points for
 *       each color component
 *
 *   A Decode array, to be used with images. This incorporates the same
 *       information as provided by the conversion parameters described in
 *       the previous item, but in a different form
 *
 * Note that the graphic state color space is referenced twice in this
 * structure: once by the indexed color space and again by the PCL base color
 * space. This arrangement is necessary because the graphic state base color
 * space does not take ownership of its client data.
 *
 * Though the palette is stored as a separate object, it is not possible
 * to share graphic library indexed color spaces between two PCL indexed color
 * spaces. Given the current structure of graphic state color spaces, this
 * problem is not easily remedied, as both the palette and the pcl base color
 * space must be kept in a one-to-one relationship with the graphic library
 * indexed color space.
 */

struct pcl_cs_indexed_s
{
    rc_header rc;
    bool pfixed;
    pcl_cid_hdr_t cid;
    pcl_cs_base_t *pbase;
    gs_color_space *pcspace;
    int original_cspace;
    int num_entries;
    gs_string palette;
    bool is_GL;
    float pen_widths[pcl_cs_indexed_palette_size];
    pcl_cs_indexed_norm_t norm[3];
    float Decode[6];
};

#ifndef pcl_cs_indexed_DEFINED
#define pcl_cs_indexed_DEFINED
typedef pcl_cs_indexed_s pcl_cs_indexed_t;
#endif

/*
 * The usual copy, init, and release macros.
 */
#define pcl_cs_indexed_init_from(pto, pfrom)\
    BEGIN                                   \
    rc_increment(pfrom);                    \
    (pto) = (pfrom);                        \
    END

#define pcl_cs_indexed_copy_from(pto, pfrom)            \
    BEGIN                                               \
    if ((pto) != (pfrom)) {                             \
        rc_increment(pfrom);                            \
        rc_decrement(pto, "pcl_cs_indexed_copy_from");  \
        (pto) = (pfrom);                                \
    }                                                   \
    END

#define pcl_cs_indexed_release(pindexed)                \
    rc_decrement(pindexed, "pcl_cs_indexed_release")

/*
 * Get the color space type for the base color space of an indexed color space.
 */
#define pcl_cs_indexed_get_cspace(pindexed) \
    ((pcl_cspace_type_t)((pindexed)->cid.cspace))

/*
 * Get the pixel encoding mode from an indexed color space.
 */
#define pcl_cs_indexed_get_encoding(pindexed)   \
    ((pcl_encoding_type_t)((pindexed)->cid.encoding))

/*
 * Get the number of bits per index from an indexed color space
 */
#define pcl_cs_indexed_get_bits_per_index(pindexed) \
    ((pindexed)->cid.bits_per_index)

/*
 * Get the number of bits per primary from an indexed color space.
 */
#define pcl_cs_indexed_get_bits_per_primary(pindexed, i)    \
    ((pindexed)->cid.bits_per_primary[i])

/*
 * Nacro to return the number of entries in the current color space.
 */
#define pcl_cs_indexed_get_num_entries(pindexed)    ((pindexed)->num_entries)

/*
 * Macro to return a pointer to the array of pen widths. Note that the pointer
 * is a const float.
 */
#define pcl_cs_indexed_get_pen_widths(pindexed) \
    ((const float *)((pindexed)->pen_widths))

/*
 * Generate the normalization and, if appropriate, Decode array that correspond
 * to a pcl_cid_data structure.
 *
 * Returns 0 on success, < 0 in the event of an error.
 */
int pcl_cs_indexed_set_norm_and_Decode(pcl_cs_indexed_t ** ppindexed,
                                       double wht0,
                                       double wht1,
                                       double wht2,
                                       double blk0, double blk1, double blk2);

/*
 * Change the number of entries in an PCL indexed color space palette.
 *
 * The gl2 boolean indicates if this call is being made from GL/2 (either the
 * IN or NP command).
 *
 * Returns 0 on success, < 0 in the event of an error.
 */
int pcl_cs_indexed_set_num_entries(pcl_cs_indexed_t ** ppindexed,
                                   int new_num, bool gl2);

/*
 * Update the lookup table information for an indexed color space.
 *
 * Returns 0 if successful, < 0 in the event of an error.
 */
int pcl_cs_indexed_update_lookup_tbl(pcl_cs_indexed_t ** pindexed,
                                     pcl_lookup_tbl_t * plktbl);

/*
 * Update an entry in the palette of a PCL indexed color space.
 *
 * Returns 0 on success, < 0 in the event of an error.
 */
int pcl_cs_indexed_set_palette_entry(pcl_cs_indexed_t ** ppindexed,
                                     int indx, const float comps[3]
    );

/*
 * Default the contents of a palette entry.
 *
 * This request can only come from GL/2, hence there is no gl2 boolean.
 *
 * Returns 0 on success, < 0 in the event of an error.
 */
int pcl_cs_indexed_set_default_palette_entry(pcl_cs_indexed_t ** ppindexed,
                                             int indx);

/*
 * Set a pen width in a palette. Units used are still TBD.
 *
 * Returns 0 if successful, < 0 in case of error.
 */
int pcl_cs_indexed_set_pen_width(pcl_cs_indexed_t ** ppindexed,
                                 int pen, double width);

/*
 * Build a PCL indexed color space.
 *
 * The boolean gl2 indicates if this request came from the GL/2 IN command.
 *
 * Returns 0 if successful, < 0 in case of error.
 */
int pcl_cs_indexed_build_cspace(pcl_state_t * pcs,
                                pcl_cs_indexed_t ** ppindexed,
                                const pcl_cid_data_t * pcid,
                                bool fixed, bool gl2, gs_memory_t * pmem);

/*
 * Build the default indexed color space. This function is usually called only
 * once, at initialization time.
 *
 * Returns 0 on success, < 0
 */
int pcl_cs_indexed_build_default_cspace(pcl_state_t * pcs,
                                        pcl_cs_indexed_t ** ppindexed,
                                        gs_memory_t * pmem);

/*
 * Special indexed color space constructor, for building a 2 entry indexed color
 * space based on an existing base color space. The first color is always set
 * to white, while the second entry takes the value indicated by pcolor1.
 *
 * This reoutine is used to build the two-entry indexed color spaces required
 * for creating opaque "uncolored" patterns.
 */
int pcl_cs_indexed_build_special(pcl_cs_indexed_t ** ppindexed,
                                 pcl_cs_base_t * pbase,
                                 const byte * pcolor1, gs_memory_t * pmem);

/*
 * Install an indexed color space into the graphic state. If not indexed
 * color space exists yet, build a default color space.
 *
 * Returns 0 on success, < 0 in the event of an error.
 */
int pcl_cs_indexed_install(pcl_cs_indexed_t ** ppindexed, pcl_state_t * pcs);

/*
 * Two routines to determine if an entry of a color palette is either white
 * or black, and two macros to specialize these to the first entry (index 0).
 *
 * The determination of "whiteness" and "blackness" is made in source space.
 * This is not fully legitimate, as the HP's implementations make this
 * determination in device space. However, only in unusual circumnstances will
 * the two give different results, and the former is much simpler to implement
 * in the current system.
 */
bool pcl_cs_indexed_is_white(const pcl_cs_indexed_t * pindexed, int indx);

bool pcl_cs_indexed_is_black(const pcl_cs_indexed_t * pindexed, int indx);

#define pcl_cs_indexed_0_is_white(pindexed) \
    pcl_cs_indexed_is_white(pindexed, 0)
#define pcl_cs_indexed_0_is_black(pindexed) \
    pcl_cs_indexed_is_black(pindexed, 0)

/*
 * One time initialization. This exists only because of the possibility that
 * BSS may not be initialized.
 */
void pcl_cs_indexed_init(pcl_state_t * pcs);

#endif /* pcindexed_INCLUDED */
