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


/* pcpalet.h - PCL palette object */

#ifndef pcpalet_INCLUDED
#define pcpalet_INCLUDED

#include "gx.h"
#include "gsstruct.h"
#include "gsrefct.h"
#include "pcident.h"
#include "pcstate.h"
#include "pcommand.h"
#include "pclookup.h"
#include "pcdither.h"
#include "pccid.h"
#include "pcindxed.h"
#include "pcht.h"

/*
 * The PCL palette.
 *
 * The identifier is used to indicate when colored patterns need to be
 * re-rendered.
 *
 * A curious feature of the PCL palette is not the object itself, but the
 * manner in which it is stored in the PCL state. During normal operation,
 * the PCL state does not maintain a reference to a plette. Rather, it
 * maintains a current installed palette ID, which can be searched for the
 * in the appropriate dictionary to find the current palette. Changes to
 * the current palette update this entry directly.
 *
 * In the case of a called or overlayed macro, however, the saved PCL state
 * will create its own reference to the palette, as well as retaining the
 * current palette id. in effect at the time the palette was saved. When the
 * state is subsequently restored, the save palette is once more associated
 * with the save palette id. Hence, the end of a called or overlay macro can
 * lead to a redefinition in the palette store. This is inconsistent with
 * the way all other identified objects are handled in PCL, but it is the
 * mechanism HP selected.
 */
struct pcl_palette_s
{
    rc_header rc;
    pcl_gsid_t id;
    pcl_cs_indexed_t *pindexed;
    pcl_ht_t *pht;
};

#ifndef pcl_palette_DEFINED
#define pcl_palette_DEFINED
typedef struct pcl_palette_s pcl_palette_t;
#endif

/*
 * The usual init, copy,and release macros.
 */
#define pcl_palette_init_from(pto, pfrom)   \
    BEGIN                                   \
    rc_increment(pfrom);                    \
    (pto) = (pfrom);                        \
    END

#define pcl_palette_copy_from(pto, pfrom)           \
    BEGIN                                           \
    if ((pto) != (pfrom)) {                         \
        rc_increment(pfrom);                        \
        rc_decrement(pto, "pcl_palette_copy_from"); \
        (pto) = (pfrom);                            \
    }                                               \
    END

#define pcl_palette_release(pbase)           \
    rc_decrement(pbase, "pcl_frgrnd_release")

/*
 * Get the color space type for the base color space of a palette.
 */
#define pcl_palette_get_cspace(ppalet)  \
    ((pcl_cspace_type_t)((ppalet)->pindexed->cid.cspace))

/*
 * Get the pixel encoding mode from the current palette.
 */
#define pcl_palette_get_encoding(ppalet)    \
    ((pcl_encoding_type_t)((ppalet)->pindexed->cid.encoding))

/*
 * Get the number of bits per index from the current palette.
 */
#define pcl_palette_get_bits_per_index(ppalet)  \
    ((ppalet)->pindexed->cid.bits_per_index)

/*
 * Get the number of bits per primary from the current palette.
 */
#define pcl_palette_get_bits_per_primary(ppalet, i) \
    ((ppalet)->pindexed->cid.bits_per_primary[i])

/*
 * Macro to return the number or entries in the current palette.
 */
#define pcl_palette_get_num_entries(ppalet) \
    pcl_cs_indexed_get_num_entries((ppalet)->pindexed)

/*
 * Macro to return a pointer to the array of pen widths.
 */
#define pcl_palette_get_pen_widths(ppalet)  \
    pcl_cs_indexed_get_pen_widths((ppalet)->pindexed)

/*
 * Set the normalization values for an indexed color space. This is needed
 * only for the GL/2 CR command; PCL sets normalization information via the
 * configure image data command, which builds a new indexed color space.
 *
 * Returns 0 on success, < 0 in the event of an error.
 */
int pcl_palette_CR(pcl_state_t * pcs,
                   double wht0,
                   double wht1,
                   double wht2, double blk0, double blk1, double blk2);

/*
 * Set the number of entries in a color palette. This is needed only for the
 * GL/2 NP command; PCL sets the number of entries in a palette via the
 * configure image data command, which creates a new indexed color space.
 *
 * Returns 0 on success, < 0 in the event of an error.
 */
int pcl_palette_NP(pcl_state_t * pcs, int num_entries);

/*
 * Set a pen width. Note that this does NOT change the palette id. This
 * procedure can only be called from GL/2, hence the procedure name.
 *
 * Returns 0 on success, < 0 in the even of an error;
 */
int pcl_palette_PW(pcl_state_t * pcs, int pen, double width);

/*
 * Support for the GL/2 IN command. This is actually implemented in pccid.c,
 * but an interface is included here for consistency.
 */
#define pcl_palette_IN(pcs) pcl_cid_IN(pcs)

/*
 * Support for CCITT Raster. This is actually implemented in pccid.c,
 * but an interface is included here for consistency.
 */
#define pcl_palette_CCITT_raster(pcs) pcl_cid_CCITT_raster(pcs)

/*
 * Set the render method for the palette.
 *
 * Returns 0 on success, < 0 in the event of an error.
 */
int pcl_palette_set_render_method(pcl_state_t * pcs, uint render_method);

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
int pcl_palette_set_gamma(pcl_state_t * pcs, float gamma);

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
int pcl_palette_set_lookup_tbl(pcl_state_t * pcs, pcl_lookup_tbl_t * plktbl);

/*
 * Set an entry in a color palette.
 *
 * Returns 0 on success, < 0 in the event of an error. The returned code will
 * normally be ignored.
 */
int pcl_palette_set_color(pcl_state_t * pcs, int indx, const float comps[3]
    );

/*
 * Set a palette entry to its default color.
 *
 * Returns 0 on success, < 0 in the event of an error. The returned code will
 * normally be ignored.
 */
int pcl_palette_set_default_color(pcl_state_t * pcs, int indx);

/*
 * Set the user-defined dither matrix.
 *
 * Returns 0 on success, < 0 in the event of an error.
 */
int pcl_palette_set_udither(pcl_state_t * pcs, pcl_udither_t * pdither);

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
int pcl_palette_set_cid(pcl_state_t * pcs,
                        pcl_cid_data_t * pcid, bool fixed, bool gl2);

/*
 * Set the view illuminant for a palette.
 *
 * Returns 0 on success, < 0 in the event of an error.
 */
int pcl_palette_set_view_illuminant(pcl_state_t * pcs,
                                    const gs_vector3 * pwht_pt);

/*
 * Check that all parts of a PCL palette have been built. If not, build the
 * necessary default objects.
 *
 * Returns 0 on success, < 0 in the event of an error.
 */
int pcl_palette_check_complete(pcl_state_t * pcs);

/*
 * Entry points to the palette-related commands.
 */
extern const pcl_init_t pcl_palette_init;

extern const pcl_init_t pcl_color_init;

/* free default objects (pcs->pdfl_*)
 * called at end of process.
 */
void pcl_free_default_objects(gs_memory_t * mem, pcl_state_t * pcs);

#endif /* pcpalet_INCLUDED */
