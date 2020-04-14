/* Copyright (C) 2001-2020 Artifex Software, Inc.
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


/* pccsbase.h - base color space portion of the PCL 5c palette object */

#ifndef pccsbase_INCLUDED
#define pccsbase_INCLUDED

#include "gx.h"
#include "gsstruct.h"
#include "gsrefct.h"
#include "gscspace.h"
#include "pcident.h"
#include "pcommand.h"
#include "pccid.h"
#include "pclookup.h"

/*
 * The client data structure for use with color-metric RGB, CIE L*a*b*, and
 * luminanace-chrominance color spaces.
 *
 * Two color lookup table pointers are provided, but only the luminance-
 * chrominance space will ever use both; for all other space plktbl2 is
 * null. The inv_gamma and inv_gain parameters are not used by the CIE L*a*b*
 * color space.
 */
typedef struct pcl_cs_client_data_s
{
    pcl_lookup_tbl_t *plktbl1;  /* null ==> identity map */
    pcl_lookup_tbl_t *plktbl2;  /* null ==> identity map */
    float min_val[3];
    float range[3];             /* max_val - min_val */
    float inv_gamma[3];         /* 1 / gamma */
    float inv_gain[3];          /* 1 / gain */
} pcl_cs_client_data_t;

/*
 * Base color space structure for PCL 5c.
 *
 * The client_data structure is referenced by the color space, but that space
 * does not take ownership of the data nor is aware of its content, hence the
 * need to keep a separate structure that ties the two elements together.
 *
 * Note that PCL has the responsibility of keeping this structure around until
 * the cloro space is not longer needed. If it fails to do so, the client data
 * may not be available when required by the (graphic library) color space.
 * This is messy, but to do otherwise would require passing a release function
 * along with the client data.
 *
 * The type field is included to allow subsequent addition of color lookup
 * tables; the type is necessary to determine if a color lookup table is
 * applicable.
 *
 * The color space referenced by this structure is always a base color space
 * (there is no particular reason it could not be a separation of DeviceN
 * color space, but these are not required by the current PCL 5c
 * specification.)
 */
typedef struct pcl_cs_base_s
{
    rc_header rc;
    pcl_cspace_type_t type;
    pcl_cs_client_data_t client_data;
    gs_color_space *pcspace;
} pcl_cs_base_t;

/*
 * Macros to init, copy, and release PCL base color spaces.
 */
#define pcl_cs_base_init_from(pto, pfrom)           \
    BEGIN                                           \
    rc_increment(pfrom);                            \
    (pto) = (pfrom);                                \
    END

#define pcl_cs_base_copy_from(pto, pfrom)           \
    BEGIN                                           \
    if ((pto) != (pfrom)) {                         \
        rc_increment(pfrom);                        \
        rc_decrement(pto, "pcl_cs_base_copy_from"); \
        (pto) = (pfrom);                            \
    }                                               \
    END

#define pcl_cs_base_release(pbase)              \
    rc_decrement(pbase, "pcl_cs_base_release")

/*
 * Build a PCL base color space. In principle this may be invoked at any time,
 * but it should generally be called the first time a color space is required
 * after a "configure image data" command. The new color space will always be
 * given null color lookup tables (the identity map).
 *
 * Returns 0 on success, < 0 in the event of an error.
 */
int pcl_cs_base_build_cspace(pcl_cs_base_t ** ppbase,
                             const pcl_cid_data_t * pcid, gs_memory_t * pmem);

/*
 * Build a special base color space, used for setting the color white.
 * This base space is unique in that it uses the DeviceGray graphic library
 * color space.
 *
 * This routine is usually called once at initialization.
 */
int pcl_cs_base_build_white_cspace(pcl_state_t * pcs,
                                   pcl_cs_base_t ** ppbase,
                                   gs_memory_t * pmem);

/*
 * Update the lookup table information for a PCL base color space. This is
 * necessary only for lookup tables associated with device-independent color
 * spaces; lookup tables for the device dependent color spaces are implemented
 * as transfer functions.
 *
 * To reset all color lookup tables for device-independent color spaces, call
 * with a null second operand.
 *
 * Returns 1 if successful and the lookup table actually modified the base
 * color space, 0 if no modification occurred but there was no error, and < 0
 * in the event of an error.
 */
int pcl_cs_base_update_lookup_tbl(pcl_cs_base_t ** ppbase,
                                  pcl_lookup_tbl_t * plktbl);

/*
 * Install a base color space into the graphic state.
 *
 * The pointer-pointer form of the first operand is for consistency with the
 * other "install" procedures.
 *
 * Returns 0 on success, < 0 in the event of an error.
 */
int pcl_cs_base_install(pcl_cs_base_t ** ppbase, pcl_state_t * pcs);

/*
 * One-time initialization routine. This exists only to handle possible non-
 * initialization of BSS.
 */
void pcl_cs_base_init(pcl_state_t * pcs);

#endif /* pccsbase_INCLUDED */
