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

/* pcpatxfm.h - interface for the pattern and general PCL transformation code */

#ifndef pcpatxfm_INCLUDED
#define pcpatxfm_INCLUDED

#include "gx.h"
#include "gsmatrix.h"
#include "gscoord.h"
#include "pcstate.h"
#include "pcommand.h"
#include "pcpatrn.h"


/* invert a diagonal matrix (assumed to be non-signular) */
extern  void    pcl_invert_mtx( const gs_matrix * pmtx1, gs_matrix * pmtx2 );

/* transform a rectangel via a diagonal matrix */
extern  void    pcl_transform_rect(
    const gs_rect *     prect1,
    gs_rect *           prect2,
    const gs_matrix *   pmtx
);

/*
 * create a 90 degree rotation matrix (counter clockwise rotation) that includes
 * the transformation required to keep the indicated rectangle in the positive
 * quadrant (with the origin at one corner of the rectangle).
 */
extern  void    pcl_make_rotation(
    int           rot,
    floatp        width,
    floatp        height,
    gs_matrix *   pmtx
);

/*
 * Form the transformation matrix required to render a pattern.
 *
 * Note that, though the rotation implicit in the print direction may apply
 * to a pattern transformation, the translation never does.
 */
extern  void    pcl_xfm_get_pat_xfm(
    const pcl_state_t * pcs,
    pcl_pattern_t *     pptrn,
    gs_matrix *         pmat
);

/*
 * Reset the PCL pattern reference point. This should be done each time the
 * logical page parameters change. (This would be better done via a reset, but
 * unfortunately there was no reset created for this category of event.)
 */
extern  void    pcl_xfm_reset_pcl_pat_ref_pt( pcl_state_t * pcs );

/*
 * Set up the pattern orientation and reference point for PCL. Note that PCL's
 * pattern reference point is kept in logical page space.
 */
extern  void    pcl_xfm_pcl_set_pat_ref_pt( pcl_state_t * pcs );

/*
 * Set up the pattern orientatin and reference point for GL. The anchor point
 * is taken as being in the current GL space, and the current transform is
 * assumed properly set. The orientation is that of the logical page.
 */
extern  void    pcl_xfm_gl_set_pat_ref_pt( pcl_state_t * pcs );

extern  const pcl_init_t    pcl_xfm_init;

#endif  	/* pcpatxfm_INCLUDED */
