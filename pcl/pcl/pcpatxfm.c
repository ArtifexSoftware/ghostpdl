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


/* pcpatxfm.c - code to set the pattern transformation */

#include "gx.h"
#include "math_.h"
#include "pcpatrn.h"
#include "pcfont.h"
#include "pcpatxfm.h"

/*
 * The four rotation matrices used by PCL. Note that rotations in PCL are
 * always multiples of 90 degrees, and map the positive x-axis to the negative
 * y-axis (the opposite or the rotation sense used by PostScript).
 *
 * All of the matrices are pure rotations; they have no scaling or translation
 * components.
 */
static const gs_matrix rot_mtx[4] = {
    {1.0, 0.0, 0.0, 1.0, 0.0, 0.0},     /*   0 degrees */
    {0.0, -1.0, 1.0, 0.0, 0.0, 0.0},    /*  90 degrees */
    {-1.0, 0.0, 0.0, -1.0, 0.0, 0.0},   /* 180 degrees */
    {0.0, 1.0, -1.0, 0.0, 0.0, 0.0}     /* 270 degrees */
};

/*
 * Inverst a diagonal 2-dimensional affine transformation. This is much simpler
 * than inverting a general 2-dimensional affine transformation, hence a
 * separate routine is provided for this purpose.
 *
 * Note that both operands may point to the same matrix.
 *
 * NB: This routine ASSUMES that *pmtx1 is non singular. This is always the
 *     case for PCL, but obviously may not be in a more general setting.
 */
void
pcl_invert_mtx(const gs_matrix * pmtx1, gs_matrix * pmtx2)
{
    float xx = pmtx1->xx;
    float tx = pmtx1->tx;
    float ty = pmtx1->ty;

    if (xx == 0.0) {
        float xy = pmtx1->xy;
        float yx = pmtx1->yx;
        pmtx2->xx = 0.0;
        pmtx2->xy = 1.0 / yx;
        pmtx2->yx = 1.0 / xy;
        pmtx2->yy = 0.0;
        pmtx2->tx = -ty / xy;
        pmtx2->ty = -tx / yx;
    } else {
        float yy = pmtx1->yy;
        pmtx2->xx = 1.0 / xx;
        pmtx2->xy = 0.0;
        pmtx2->yx = 0.0;
        pmtx2->yy = 1.0 / yy;
        pmtx2->tx = -tx / xx;
        pmtx2->ty = -ty / yy;
    }
}

/*
 * Transform an aligned rectangle. Because all transformations in PCL are
 * diagonal, both the source and destination rectangles are aligned with the
 * coordinate axes, and hence may be represented by a pair of points.
 *
 * prect1 and prect2 may point to the same rectangle.
 */
void
pcl_transform_rect(const gs_rect * prect1,
                   gs_rect * prect2, const gs_matrix * pmtx)
{
    gs_point_transform(prect1->p.x, prect1->p.y, pmtx, &(prect2->p));
    gs_point_transform(prect1->q.x, prect1->q.y, pmtx, &(prect2->q));
    if (prect2->p.x > prect2->q.x) {
        double ftmp = prect2->p.x;

        prect2->p.x = prect2->q.x;
        prect2->q.x = ftmp;
    }
    if (prect2->p.y > prect2->q.y) {
        double ftmp = prect2->p.y;

        prect2->p.y = prect2->q.y;
        prect2->q.y = ftmp;
    }
}

/*
 * Create the transformation matrix corresponding to a PCL rotation.
 *
 * The rotation operand is an integer in the range 0..3; the desired angle of
 * rotation is 90 * rot.
 *
 * PCL rotations are not pure rotations, and thus cannot be uniquely identified
 * by an angle. A PCL coordinate system is always associated with a rectangular
 * (and aligned) subset of the page, which lies in the possitive quadrant of
 * the coordinate system. The origin of the coordinate system is always one
 * corner of this rectangular region. A rotation in PCL both alters the
 * orientation of the coordinate system and moves its origin to another corner,
 * so as to keep the rectangluar subregion in the positive quadrant.
 *
 * To implement such a transformation, it is necessary to know both the
 * desired change of orientation, and the dimensions of the rectangular
 * region (in the original coordinate system).
 */
void
pcl_make_rotation(int rot, double width, double height, gs_matrix * pmtx)
{
    *pmtx = rot_mtx[rot & 0x3];
    if (pmtx->xx + pmtx->yx < 0.0)
        pmtx->tx = width;
    if (pmtx->xy + pmtx->yy < 0.0)
        pmtx->ty = height;
}

/*
 * Pattern reference point manipulation.
 *
 * Logically, the pattern reference point transforms in the same manner as the
 * logical page: it is reset by changing the logical page orientation (not
 * documented, but verified empirically), it moves with the left and top offset
 * commands, but it does not change with changes in print direction.
 *
 * Ergo, the logical space in which to maintain the pattern reference point
 * is the logical page transformation. This is also the logical space in which
 * to keep the current addressable (cursor) position, but the PCL code is
 * illogical in this regard (see the code for pcl_set_print_direction in
 * pcdraw.c if you are not convinced of this); the current addressable
 * position is instead maintained in a curious "semi-print-direction" space,
 * which incorporates the orientation but not the translation component of
 * the "print direction" coordinate space.
 *
 * Hence, this code must convert the current addressable position back into
 * logical page space before making use of it. In other portions of this code
 * such a change is by using the current transformation directly the graphic
 * state. This is a bit unfortunate, as transformation must be set twice,
 * which can play havoc with transformation-sensitive caches.
 *
 * The current code instead instead directly access the matrix machinery,
 * using the graphic state only to gather the default transformation (which
 * maps the physical page to the device, with centipoints as the source unit).
 *
 * When setting up the transformation matrix to be used by a pattern, the code
 * constructs the resulting matrix in three steps:
 *
 *     Get the default transformation matrix and modify it to transform
 *         logical page space to device space. This involves incorporating
 *         the left and top offsets and the logical page orientation.
 *
 *     Translate the resulting matrix by the pattern reference point.
 *
 *     If the pattern is to be rotated by the print direction, apply the
 *        print direction rotation
 */

/*
 * Convert a floating point number that is nearly an integer to an integer.
 */
static double
adjust_param(double val)
{
    double fval = floor(val);
    double cval = ceil(val);

    return (val - fval < .001 ? fval : (cval - val < .001 ? cval : val));
}

/*
 * Form the transformation matrix required to render a pattern.
 */
void
pcl_xfm_get_pat_xfm(const pcl_state_t * pcs,
                    pcl_pattern_t * pptrn, gs_matrix * pmat)
{
    const pcl_xfm_state_t *pxfmst = &(pcs->xfm_state);
    int rot = (pcs->pat_orient - pxfmst->lp_orient) & 0x3;

    *pmat = pxfmst->lp2dev_mtx;
    pmat->tx = pcs->pat_ref_pt.x;
    pmat->ty = pcs->pat_ref_pt.y;

    /* record the referenc point used in the rendering structure */
    pptrn->ref_pt = pcs->pat_ref_pt;

    /* rotate as necessar */
    if (rot != 0)
        gs_matrix_multiply(&(rot_mtx[rot]), pmat, pmat);

    /* scale to the appropriate resolution (before print direction rotation) */
    gs_matrix_scale(pmat,
                    inch2coord(1.0 / (double) pptrn->ppat_data->xres),
                    inch2coord(1.0 / (double) pptrn->ppat_data->yres), pmat);

    /* avoid parameters that are slightly different from integers */
    pmat->xx = adjust_param(pmat->xx);
    pmat->xy = adjust_param(pmat->xy);
    pmat->yx = adjust_param(pmat->yx);
    pmat->yy = adjust_param(pmat->yy);

    /* record the rotation used for rendering */
    pptrn->orient = pcs->pat_orient;
}

/*
 * Reset the pattern reference point. This should be done each time the logical
 * page parameters change. (This would be better done via a reset, but
 * unfortunately there was no reset created for this category of even.)
 */
void
pcl_xfm_reset_pcl_pat_ref_pt(pcl_state_t * pcs)
{
    pcs->pcl_pat_ref_pt.x = 0.0;
    pcs->pcl_pat_ref_pt.y = 0.0;
    /* initialize the device coordinates to bogus values to guarantee
       the pattern is reset. */
    pcs->pat_ref_pt.x = -1;
    pcs->pat_ref_pt.y = -1;
    pcs->pat_orient = -1;
    pcs->rotate_patterns = true;
}

/*
 * Set up the pattern orientation and reference point for PCL. Note that PCL's
 * pattern reference point is kept in logical page space.
 */
void
pcl_xfm_pcl_set_pat_ref_pt(pcl_state_t * pcs)
{
    pcl_xfm_state_t *pxfmst = &(pcs->xfm_state);

    gs_point_transform(pcs->pcl_pat_ref_pt.x,
                       pcs->pcl_pat_ref_pt.y,
                       &(pxfmst->lp2dev_mtx), &(pcs->pat_ref_pt)
        );
    pcs->pat_ref_pt.x = floor(pcs->pat_ref_pt.x + 0.5);
    pcs->pat_ref_pt.y = floor(pcs->pat_ref_pt.y + 0.5);
    pcs->pat_orient = (pxfmst->lp_orient
                       +
                       (pcs->rotate_patterns ? pxfmst->print_dir : 0)) & 0x3;
}

/*
 * Set up the pattern orientatin and reference point for GL. The anchor point
 * is taken as being in the current GL space, and the current transform is
 * assumed properly set. The orientation is that of the logical page.
 */
void
pcl_xfm_gl_set_pat_ref_pt(pcl_state_t * pcs)
{
    gs_transform(pcs->pgs,
                 pcs->g.anchor_corner.x,
                 pcs->g.anchor_corner.y, &(pcs->pat_ref_pt)
        );
    pcs->pat_ref_pt.x = floor(pcs->pat_ref_pt.x + 0.5);
    pcs->pat_ref_pt.y = floor(pcs->pat_ref_pt.y + 0.5);
    pcs->pat_orient =
        (pcs->xfm_state.lp_orient + (pcs->g.rotation / 90)) & 0x3;
}

/*
 * ESC * p # R
 *
 * Set pattern reference point.
 *
 */
static int
set_pat_ref_pt(pcl_args_t * pargs, pcl_state_t * pcs)
{
    int code = 0;
    uint rotate = uint_arg(pargs);

    if (rotate <= 1) {
        code = pcl_break_underline(pcs);
        gs_point_transform((double) pcs->cap.x,
                           (double) pcs->cap.y,
                           &(pcs->xfm_state.pd2lp_mtx), &(pcs->pcl_pat_ref_pt)
            );
        pcs->rotate_patterns = (rotate == 0);
    }
    return code;
}

/*
 * Initializaton and reset routines. There is currently no copy routine, as the
 * desired transformation is reset for each object printed. No special reset
 * routine is required, as a reset of the logical page will reset the pattern
 * reference point as well.
 */
static int
xfm_do_registration(pcl_parser_state_t * pcl_parser_state, gs_memory_t * pmem)
{
    DEFINE_CLASS('*') {
        'p', 'R',
            PCL_COMMAND("Pattern Reference Point",
                        set_pat_ref_pt,
                        pca_neg_ok | pca_big_ignore | pca_in_rtl)
    }, END_CLASS return 0;
}

const pcl_init_t pcl_xfm_init = { xfm_do_registration, 0, 0 };
