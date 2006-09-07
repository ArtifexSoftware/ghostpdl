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

/* pccrd.h - PCL interface to color rendering dictionaries */

#ifndef pccrd_INCLUDED
#define pccrd_INCLUDED

#include "gx.h"
#include "gsstruct.h"
#include "gsrefct.h"
#include "gsmatrix.h"
#include "gscspace.h"
#include "gscolor2.h"
#include "gscie.h"
#include "gscrd.h"
#include "pcident.h"
#include "pcstate.h"

/*
 * In PCL, color rendering dictionaries are set by the device, but the
 * default white point is specified by the language, and the white point 
 * may be modified by the language via the "View Illuminant" command.
 *
 * Though trivial in principle, this arrangement is a bit awkward for the
 * graphic library as there is no make-unique functionality provided 
 * for color rendering dictionaries (none is necessary for PostScript as
 * color rendering dictionaries may not be modified once they are set).
 *
 * This code hides some of the structure of the graphic library color
 * rendering objects from the rest of the PCL code.
 */

/*
 * The PCL id exists to provide an identifier for a color rendering 
 * dictionary. Having such an identifier is important to avoid the need 
 * to repeatedly re-insert the color rendering dictionary into the 
 * graphic state.
 *
 * The is_dflt_illum flag is used to optimize the case in which a CRD that
 * already uses the default view illuminant is once again set to use this 
 * view illuminant.
 */
struct pcl_crd_s {
    rc_header         rc;
    bool              is_dflt_illum;
    gs_cie_render *   pgscrd;
};

#define private_st_crd_t()                                  \
    gs_private_st_ptrs1( st_crd_t,                          \
                         pcl_crd_t,                         \
                         "pcl color rendering dictionary",  \
                         crd_enum_ptrs,                     \
                         crd_reloc_ptrs,                    \
                         pgscrd                             \
                         )

#ifndef pcl_crd_DEFINED
#define pcl_crd_DEFINED
typedef struct pcl_crd_s        pcl_crd_t;
#endif

/*
 * The usual copy, init, and release macros.
 */
#define pcl_crd_init_from(pto, pfrom)   \
    BEGIN                               \
    rc_increment(pfrom);                \
    (pto) = (pfrom);                    \
    END

#define pcl_crd_copy_from(pto, pfrom)           \
    BEGIN                                       \
    if ((pto) != (pfrom)) {                     \
        rc_increment(pfrom);                    \
        rc_decrement(pto, "pcl_crd_copy_from"); \
        (pto) = (pfrom);                        \
    }                                           \
    END

#define pcl_crd_release(pindexed)           \
    rc_decrement(pindexed, "pcl_crd_release")

/*
 * Unlike other elements of the PCL "palette", color rendering dictionaries
 * are for the most part not a feature that can be controlled from the language.
 * Except for the white point, the parameters of a color rendering dictionary
 * are determined by the output device rather than the language.
 */

/*
 * Build the default color rendering dictionary.
 *
 * This routine should be called only once, and then only when there is no
 * current CRD.
 *
 * Returns 0 on success, < 0 in the event of an error.
 */
int pcl_crd_build_default_crd(pcl_state_t *pcs);

/*
 * Set the viewing illuminant.
 *
 * Though this code goes through the motions of an "unshare" operation, it
 * will almost always allocate a new structure, as the CRD will be referred
 * to both by the palette and the graphic state.
 *
 * Returns 0 on success, < 0 in the event of an error.
 */
int pcl_crd_set_view_illuminant(
    pcl_state_t *       pcs,
    pcl_crd_t **        ppcrd,
    const gs_vector3 *  pwht_pt
);

/*
 * Set a color rendering dictionary into the graphic state. If the rendering
 * dictionary does not yet exist, create a default color rendering dictionary.
 *
 * Returns 0 on success, < 0 in the event of an error.
 */
int pcl_crd_set_crd(pcl_crd_t **ppcrd, pcl_state_t *pcs);

void free_crd(gs_memory_t *pmem, void *pvcrd, client_name_t cname);

#endif  	/* pccrd_INCLUDED */
