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


/* pcxfmst.h - transformation information structures in the PCL state */

#ifndef pcxfmst_INCLUDED
#define pcxfmst_INCLUDED

#include "gx.h"
#include "gsmatrix.h"
#include "gxfixed.h"
#include "pccoord.h"

/*
 * Structure for paper size parameters. Note that these values are all coords
 * (centipoints).
 */
typedef struct pcl_paper_size_s
{
    coord width, height;        /* physical page size */
    coord offset_portrait;      /* offset of logical page left edge from
                                 * the physical page in portrait orientations */
    coord offset_landscape;     /* ditto for landscape orientations */
} pcl_paper_size_t;

/*
 * The table of paper sizes
 */
typedef struct pcl_paper_type_s
{
    uint tag;
    const char *pname;
    pcl_paper_size_t psize;
} pcl_paper_type_t;

/*
 * Geometric transformation structure for PCL.
 *
 * Except for GL/2, PCL deals strictly in diagonal transformations: all
 * transformations are compositions of 90 degree rotations, scaling, and
 * translations. Thus, maintaining a full matrix representation is overkill.
 * For the most part, however, any gains in performance from use of a simpler
 * representation would be lost due ot the non-general nature of the code,
 * so a normal matrix representations and set of matrix operations is used.
 *
 * Because orientation plays such a significant role in PCL, both the logical
 * page orientation and the orientation of the print direction relative to
 * the logical page are kept separately, even though this information is
 * implicit in the transformations.
 *
 * For all of the transformations used here, units are centi-points.
 *
 * For historical reasons, the print direction coordinate system identified by
 * the pd2lp_mtx field does not include any translation. Hence the origin of
 * of the coordinate space is at the intersection of two logical page boundaries
 * (one of which is also a physical page boundary) rather than at the
 * intersection of a logical page boundary and the top margin. We term this
 *  the "pseudo print direction" space.
 *
 * The page space to device space transformation, which is established by the
 * output device, is not kept here. It is the default transformation of the
 * graphic state, and thus does not need to be kept separately. This code
 * makes the implicit assumption that this transformation matrix is also
 * diagonal, which is the case for all reasonable devices.
 *
 * This structure also maintains a copy of the printable region
 * rectangle in logical page space.  It is used by the raster module
 * to calculate default raster destination dimensions and usable
 * raster source dimensions.
 *
 * fields:
 *
 *      left_offset_cp  left and top offset registrations, in centipoints;
 *      top_offset_cp   these move the logical page on the physica page but
 *                      do not change the logical page size
 *
 *      paper_size      pointer to a structure describing the current paper
 *                      size (including logical page offsets)
 *
 *      lp_orient       logical page orientation, 0..3
 *
 *      print_dir       print direction (relative to logical page), divided
 *                      by 90 (thus in the range 0..3)
 *
 *      lp2pg_mtx       logical page space to page space transformation
 *
 *      lp2dev_mtx      logical page to device space transformation; this is
 *                      used as the current matrix in some situations, and
 *                      for conversion of pattern reference points
 *
 *      pd2lp_mtx       "pseudo print direction" space to logical page space
 *                      transformation
 *
 *      pd2dev_mtx      "pseudo print direction" space to device space
 *                      transformation; the is commonly used as the current
 *                      transformation for PCL objects (other than rasters)
 *
 *      lp_size         dimensions of the logical page in logical pace
 *                      space
 *
 *      pd_size         dimensions of the logical page in print direction
 *                      space
 *
 *      lp_print_rect   printable region rectangle, in logical page space
 *
 */
typedef struct pcl_xfm_state_s
{

    float left_offset_cp;
    float top_offset_cp;

    const pcl_paper_size_t *paper_size;
    byte lp_orient;
    byte print_dir;

    /* the remaining fields are filled in by update_xfm_state() */
    gs_matrix lp2pg_mtx;
    gs_matrix lp2dev_mtx;
    gs_matrix pd2lp_mtx;
    gs_matrix pd2dev_mtx;

    /* height and width of logical page, in centipoints */
    coord_point_t lp_size;
    coord_point_t pd_size;

    gs_rect lp_print_rect;
} pcl_xfm_state_t;

/*
 * Convert a point from "semi print direction" space to logical page space.
 */
#define pcl_xfm_to_logical_page_space(pcs, ppt)    \
    gs_point_transform(	(ppt)->x,                       \
                        (ppt)->y,                       \
                        &((pcs)->xfm_state.pd2lp_mtx),  \
                        ppt                             \
                        )

/*
 * Structure for text region margins. These are all in centipoint relative
 * to the current "pseudo print direction" space.
 */
typedef struct pcl_margins_s
{
    coord left;                 /* measured from left edge */
    coord right;                /* measured from *left* edge */
    coord top;                  /* measured from top */
    coord length;               /* text_length, distance from top to bottom */
} pcl_margins_t;

#endif /* pcxfmst_INCLUDED */
