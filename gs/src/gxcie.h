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

/*$RCSfile$ $Revision$ */
/* Internal definitions for CIE color implementation */
/* Requires gxcspace.h */

#ifndef gxcie_INCLUDED
#  define gxcie_INCLUDED

#include "gscie.h"

/*
 * These color space implementation procedures are defined in gscie.c or
 * gsciemap.c, and referenced from the color space structures in gscscie.c.
 */
/*
 * We use CIExxx rather than CIEBasedxxx in some places because
 * gcc under VMS only retains 23 characters of procedure names,
 * and DEC C truncates all identifiers at 31 characters.
 */

/* Defined in gscie.c */

cs_proc_init_color(gx_init_CIE);
cs_proc_restrict_color(gx_restrict_CIEDEFG);
cs_proc_install_cspace(gx_install_CIEDEFG);
cs_proc_restrict_color(gx_restrict_CIEDEF);
cs_proc_install_cspace(gx_install_CIEDEF);
cs_proc_restrict_color(gx_restrict_CIEABC);
cs_proc_install_cspace(gx_install_CIEABC);
cs_proc_restrict_color(gx_restrict_CIEA);
cs_proc_install_cspace(gx_install_CIEA);

/* Defined in gsciemap.c */

/*
 * Test whether a CIE rendering has been defined; ensure that the joint
 * caches are loaded.  Note that the procedure may return if no rendering
 * has been defined, and returns if an error occurs.
 */
#define CIE_CHECK_RENDERING(pcs, pconc, pis, do_exit)                   \
    BEGIN                                                               \
        if (pis->cie_render == 0) {                                     \
            /* No rendering has been defined yet: return black. */      \
            pconc[0] = pconc[1] = pconc[2] = frac_0;                    \
            do_exit;                                                    \
        }                                                               \
        if (pis->cie_joint_caches->status != CIE_JC_STATUS_COMPLETED) { \
            int     code = gs_cie_jc_complete(pis, pcs);                \
                                                                        \
            if (code < 0)                                               \
                return code;                                            \
        }                                                               \
    END

/*
 * Do the common remapping operation for CIE color spaces. Returns the numver
 * of components of the concrete color space (3 if RGB, 4 if CMYK).
 */
extern  int     gx_cie_remap_finish( P4( cie_cached_vector3,
		       	                 frac *,
                                         const gs_imager_state *,
				         const gs_color_space * ) );

cs_proc_concretize_color(gx_concretize_CIEDEFG);
cs_proc_concretize_color(gx_concretize_CIEDEF);
cs_proc_concretize_color(gx_concretize_CIEABC);
cs_proc_remap_color(gx_remap_CIEABC);
cs_proc_concretize_color(gx_concretize_CIEA);

/* Defined in gscscie.c */

/* GC routines exported for gsicc.c */
extern_st(st_cie_common);
extern_st(st_cie_common_elements_t);

/* set up the common default values for a CIE color space */
extern  void    gx_set_common_cie_defaults( P2( gs_cie_common *,
                                                void *  client_data ) );

/* Load the common caches for a CIE color space */
extern  void    gx_cie_load_common_cache(P2(gs_cie_common *, gs_state *));

/* Complete loading of the common caches */
extern  void    gx_cie_common_complete(P1(gs_cie_common *));

/* "indirect" color space installation procedure */
cs_proc_install_cspace(gx_install_CIE);

/* allocate and initialize the common part of a cie color space */
extern  void *  gx_build_cie_space( P4( gs_color_space **           ppcspace,
                                        const gs_color_space_type * pcstype,
                                        gs_memory_type_ptr_t        stype,
                                        gs_memory_t *               pmem ) );

/*
 * Determine the concrete space which underlies a CIE based space. For all
 * device independent color spaces, this is dependent on the current color
 * rendering dictionary, rather than the current color space. This procedure
 * is exported for use by gsicc.c to implement ICCBased color spaces.
 */
cs_proc_concrete_space(gx_concrete_space_CIE);

#endif /* gxcie_INCLUDED */
