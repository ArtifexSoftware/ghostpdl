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
void pcl_invert_mtx(const gs_matrix * pmtx1, gs_matrix * pmtx2);

/* transform a rectangel via a diagonal matrix */
void pcl_transform_rect(const gs_rect * prect1,
                        gs_rect * prect2, const gs_matrix * pmtx);

/*
 * create a 90 degree rotation matrix (counter clockwise rotation) that includes
 * the transformation required to keep the indicated rectangle in the positive
 * quadrant (with the origin at one corner of the rectangle).
 */
void pcl_make_rotation(int rot,
                       double width, double height, gs_matrix * pmtx);

/*
 * Form the transformation matrix required to render a pattern.
 *
 * Note that, though the rotation implicit in the print direction may apply
 * to a pattern transformation, the translation never does.
 */
void pcl_xfm_get_pat_xfm(const pcl_state_t * pcs,
                         pcl_pattern_t * pptrn, gs_matrix * pmat);

/*
 * Reset the PCL pattern reference point. This should be done each time the
 * logical page parameters change. (This would be better done via a reset, but
 * unfortunately there was no reset created for this category of event.)
 */
void pcl_xfm_reset_pcl_pat_ref_pt(pcl_state_t * pcs);

/*
 * Set up the pattern orientation and reference point for PCL. Note that PCL's
 * pattern reference point is kept in logical page space.
 */
void pcl_xfm_pcl_set_pat_ref_pt(pcl_state_t * pcs);

/*
 * Set up the pattern orientatin and reference point for GL. The anchor point
 * is taken as being in the current GL space, and the current transform is
 * assumed properly set. The orientation is that of the logical page.
 */
void pcl_xfm_gl_set_pat_ref_pt(pcl_state_t * pcs);

extern const pcl_init_t pcl_xfm_init;

#endif /* pcpatxfm_INCLUDED */
