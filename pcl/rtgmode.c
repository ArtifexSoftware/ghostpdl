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

/* rtgmode.c - PCL graphics (raster) mode */
#include "gx.h"
#include "math_.h"
#include "gsmatrix.h"
#include "gscoord.h"
#include "gsrect.h"
#include "gsstate.h"
#include "pcstate.h"
#include "pcpatxfm.h"
#include "pcpage.h"
#include "pcindxed.h"
#include "pcpalet.h"
#include "pcursor.h"
#include "pcdraw.h"
#include "rtraster.h"
#include "rtrstcmp.h"
#include "rtgmode.h"

/*
 * Intersect a rectangle with the positive quadrant.
 */
  static void
intersect_with_positive_quadrant(
    gs_rect *   prect
)
{
    if (prect->p.x < 0.0) {
        prect->p.x = 0.0;
        prect->q.x = (prect->q.x < 0.0 ? 0.0 : prect->q.x);
    }
    if (prect->p.y < 0.0) {
        prect->p.y = 0.0;
        prect->q.y = (prect->q.y < 0.0 ? 0.0 : prect->q.y);
    }
}

/*
 * Get the effective printing region in raster space
 */
static void
get_raster_print_rect( const gs_memory_t *mem, 
		       const gs_rect *      plp_print_rect,
		       gs_rect *            prst_print_rect,
		       const gs_matrix *    prst2lp
		       )
{
    gs_matrix            lp2rst;

    pcl_invert_mtx(prst2lp, &lp2rst);
    pcl_transform_rect(mem, plp_print_rect, prst_print_rect, &lp2rst);
    intersect_with_positive_quadrant(prst_print_rect);
}

/*
 * Enter raster graphics mode.
 *
 * The major function of this routine is to establish the raster to device
 * space transformations. This is rather involved:
 *
 * 1. The first feature to be established is the orientation of raster space
 *    relative to page space. Three state parameters are involved in
 *    determining this orientation: the logical page orientation, the current
 *    print direction, and the raster presentation mode. These are combined
 *    in the following manner:
 *
 *        tr = (print_direction / 90) + logical_page_orientation
 *
 *        raster_rotate = (presentation_mode == 0 ? tr : tr & 0x2)
 *
 * 2. The next step is to determine the location of the origin of the raster
 *    to page transformation. Intially this origin is set at the appropriate
 *    corner of the logical page, based on the orientation determined above.
 *    The origin is then shift based on the manner in which graphics mode is
 *    entered (the mode operand):
 *
 *        If entry is IMPLICIT (i.e.: via a transfer data command rather than
 *        an enter graphics mode command), translation by the existing left
 *        graphics margin is used, in the orientation of raster space.
 *
 *        If entry is via an enter graphics mode command which specifies moving
 *        the origin to the logical page boundary (NO_SCALE_LEFT_MARG (0) or
 *        SCALE_LEFT_MARG (2)), action depends on whether or not horizontal
 *        access of print direction space and of raster space are the same:
 *
 *            if there are the same, the origin is left unchanged
 *
 *            if they are not the same, the origin is shifted 1/6" (1200 centi-
 *            points) in the positive horizontal raster space axis.
 *
 *        The latter correction is not documented by HP, and there is no clear
 *        reason why it should apply, but it has been verified to be the case
 *        for all HP products testd.
 *
 *        If entry is via an enter graphics mode command with specifies use
 *        of the current point (NO_SCALE_CUR_PT(1) or SCALE_CUR_PT(3)), the
 *        current point is transformed to raster space and its "horizontal"
 *        component is used as the new graphics margin.
 *
 *    Irrespective of how the "horizontal" component of the raster image origin
 *    is specified, the vertical component is always derived from the current
 *    addressable point, by converting the point to raster space.
 *
 * 3. Next, the scale of the raster to page space transformation is established.
 *    This depends on whether or not PCL raster scaling is to be employed.
 *    For raster scaling to be used, all of the following must hold:
 *
 *        the scale_raster flag in the PCL raster state must be set
 *        the current palette must be writable
 *        the raster source height and width must have been explicitly set
 *
 *    The scale_raster flag in the PCL raster state is normally set by the
 *    enter raster graphics command. Hence, if graphics mode is entered
 *    explicitly, the first requirement follows the behavior of the HP Color
 *    LaserJet 5/5M. The DeskJet 1600C/CM behaves differently: it will never
 *    user raster scaling if graphics mode is entered implicitly.
 *
 *    The reason for the second requirement is undoubtedly related to some
 *    backwards compatibility requirement, but is otherwise obscure. The
 *    restriction is, however, both document and uniformly applied by all
 *    HP products that support raster scaling.
 *
 *    If raster scaling is not used, the scale of raster space is determined
 *    by the ratio of the graphics resolution (set by the graphics resolution
 *    command) and unit of page space (centi-points). This factor is applied
 *    in both scan directions.
 *
 *    If scaling is employed, the situation is somewhat more complicated. It
 *    is necessary, in this case, to know which of the raster destination
 *    dimensions have been explicitly set:
 *
 *        If both dimensions are specified, the ration of these dimensions
 *        to the source raster width and height determine the raster scale.
 *
 *        If only one destination dimension is specified, the ratio of this
 *        dimension to the corresponding source dimension determins the
 *        raster scale for both dimensions; With strange interactions with 
 *        the 1200centipoint margin and rotated pages (Bug emulation).
 *
 *        If neither dimension is specified, the page printable region is
 *        transformed to raster space, the intersection of this with the
 *        positive quadrant is taken. The dimensions of the resulting region
 *        are compared with the dimensions of the source raster. The smaller
 *        of the two dest_dim / src_dim ratios is used as the ratio for 
 *        the raster scale in both dimensions (i.e.: select the largest
 *        isotropic scaling that does not cause clipping).
 *
 * 4. Finally, the extent of raster space must be determined. This is done by
 *    converting the page printable region to raster space and intersecting
 *    the result with the positive quadrant. This region is used to determine
 *    the useable source raster width and height.
 *        
 */
   int
pcl_enter_graphics_mode(
    pcl_state_t *       pcs,
    pcl_gmode_entry_t   mode
)
{
    floatp                  scale_x, scale_y;
    pcl_xfm_state_t *       pxfmst = &(pcs->xfm_state);
    pcl_raster_state_t *    prstate = &(pcs->raster_state);
    float                   gmargin_cp = (float)prstate->gmargin_cp;
    gs_point                cur_pt;
    gs_matrix               rst2lp, rst2dev, lp2rst;
    gs_rect                 print_rect;
    uint                    src_wid, src_hgt;
    int                     rot;
    int                     code = 0;
    double                  dwid, dhgt;
    int                     clip_x, clip_y;
    /*
     * Check if the raster is to be clipped fully; see rtrstst.h for details.
     * Since this is a discontinuous effect, the equality checks below
     * should be made while still in centipoints.
     */
    prstate->clip_all = ( (pcs->cap.x == pxfmst->pd_size.x) ||
                          (pcs->cap.y == pxfmst->pd_size.y)   );

    /* create to raster space to logical page space transformation */
    rot = pxfmst->lp_orient + pxfmst->print_dir;
    if (prstate->pres_mode_3)
        rot &= 0x2;
    rot = (rot - pxfmst->lp_orient) & 0x3;
    if (prstate->y_advance == -1)
        rot = (rot + 2) & 0x3;
    pcl_make_rotation(rot, pxfmst->lp_size.x, pxfmst->lp_size.y, &rst2lp);
    pcl_invert_mtx(&rst2lp, &lp2rst);

    /* convert the current point to raster space */
    cur_pt.x = (double)pcs->cap.x;
    cur_pt.y = (double)pcs->cap.y;
    pcl_xfm_to_logical_page_space(pcs, &cur_pt);
    gs_point_transform(cur_pt.x, cur_pt.y, &lp2rst, &cur_pt);

    /* translate the origin of the forward transformation */
    if (((int)mode & 0x1) != 0)
        gmargin_cp = cur_pt.x;
    gs_matrix_translate(&rst2lp, gmargin_cp, cur_pt.y, &rst2lp);
    prstate->gmargin_cp = gmargin_cp;

    /* isotropic scaling with missing parameter is based on clipped raster dimensions */

    /* transform the clipping window to raster space */
    get_raster_print_rect(pcs->memory, &(pxfmst->lp_print_rect), &print_rect, &rst2lp);
    dwid = print_rect.q.x - print_rect.p.x;
    dhgt = print_rect.q.y - print_rect.p.y;

    clip_x = pxfmst->lp_print_rect.p.x;  /* if neg then: */
    clip_y = pxfmst->lp_print_rect.p.y;  /* = 1200centipoints */

    /* set the matrix scale */
    if ( !prstate->scale_raster       ||
         !prstate->src_width_set      ||
         !prstate->src_height_set     ||
         (pcs->ppalet->pindexed->pfixed  && mode == IMPLICIT) ) {
        scale_x = 7200.0 / (floatp)prstate->resolution;
        scale_y = scale_x;

    } else if (prstate->dest_width_set) {
	scale_x = (floatp)prstate->dest_width_cp / (floatp)prstate->src_width;

	if ( clip_x < 0 && pxfmst->lp_orient == 3 ) { 
	    scale_y = (floatp)(prstate->dest_width_cp - clip_y ) / (floatp)prstate->src_width;
	    if ( rot == 2 && scale_y <=  2* prstate->src_width) /* empirical test 1 */
		scale_y = scale_x;   
	}
	else if ( clip_x < 0 && pxfmst->lp_orient == 1 && rot == 3 ) {
	    scale_y = (floatp)(prstate->dest_width_cp - clip_y) / (floatp)prstate->src_width;

	    if ( prstate->dest_width_cp <= 7200 )  /* empirical test 2 */
		scale_y = (floatp)(prstate->dest_width_cp + clip_y) / (floatp)prstate->src_width;
	}
	else 
	    scale_y = scale_x;

        if (prstate->dest_height_set) 
	    scale_y = (floatp)prstate->dest_height_cp / (floatp)prstate->src_height;

    } else if (prstate->dest_height_set) {    	 
	scale_x = scale_y = (floatp)prstate->dest_height_cp / (floatp)prstate->src_height;
    } else {

        /* select isotropic scaling with no clipping */
	scale_x = (floatp)dwid / (floatp)prstate->src_width;
	scale_y = (floatp)dhgt / (floatp)prstate->src_height;
        if (scale_x > scale_y)
            scale_x = scale_y;
        else
            scale_y = scale_x;
    }

    gs_matrix_scale(&rst2lp, scale_x, scale_y, &rst2lp);
    gs_matrix_multiply(&rst2lp, &(pxfmst->lp2dev_mtx), &rst2dev);

    rst2dev.tx = (double)((int)(rst2dev.tx + 0.5));
    rst2dev.ty = (double)((int)(rst2dev.ty + 0.5));
    /*
     * Set up the graphic stat for rasters. This turns out to be more difficult
     * than might first be imagined.
     *
     * One problem is that two halftones may be needed simultaneously:
     *
     *     the foreground CRD and halftone, in case the current "texture" is a
     *     a solid color or an uncolored pattern
     *
     *     the palette CRD and halftone, to be used in rendering the raster
     *     itself
     *
     * Since the graphic state can only hold one CRD and one halftone method
     * at a time, this presents a bit of a problem.
     *
     * To get around the problem, an extra graphic state is necessary. Patterns
     * in the graphic library are given their own graphic state. Hence, by
     * replacing a solid color with an uncolored pattern that takes the
     * foreground value everywhere, the desired effect can be achieved. Code
     * in pcpatrn.c handles these matters.
     *
     * The second problem is a limitation in the graphic library's support of
     * CIE color spaces. These spaces require a joint cache, which is only
     * created when the color space is installed in the graphic state. However,
     * the current color space at the time a raster is rendered may need to
     * be a pattern color space, so that the proper interaction between the
     * raster and the texture generated by the pattern. To work around this
     * problem, we install the raster's color space in the current graphic
     * state, perform a gsave, then place what may be a patterned color space
     * in the new graphic state.
     */
    pcl_set_graphics_state(pcs);
    pcl_set_drawing_color(pcs, pcl_pattern_raster_cspace, 0, true);
    pcl_gsave(pcs);
    pcl_set_drawing_color(pcs, pcs->pattern_type, pcs->current_pattern_id, true);
    gs_setmatrix(pcs->pgs, &rst2dev);

    /* translate the origin of the forward transformation */
    /* tansform the clipping window to raster space; udpate source dimensions */
    get_raster_print_rect(pcs->memory, &(pxfmst->lp_print_rect), &print_rect, &rst2lp);

    /* min size is 1 pixel */
    src_wid = max(1, (uint)(floor(print_rect.q.x) - floor(print_rect.p.x)));
    src_hgt = max(1, (uint)(floor(print_rect.q.y) - floor(print_rect.p.y)));
    if (prstate->src_width_set && (src_wid > prstate->src_width))
        src_wid = prstate->src_width;
    if (prstate->src_height_set && (src_hgt > prstate->src_height))
        src_hgt = prstate->src_height;

    if (src_wid <= 0 || src_hgt <= 0) {
        pcl_grestore(pcs);
        return 1; /* hack, we want to return a non critical warning */
    }
    /* determine (conservatively) if the region of interest has been
       marked */
    pcs->page_marked = true;
    if ((code = pcl_start_raster(src_wid, src_hgt, pcs)) >= 0)
        prstate->graphics_mode = true;
    else
        pcl_grestore(pcs);
    return code;
}

/*
 * End (raster) graphics mode. This may be called explicitly by either of the
 * end graphics mode commands (<esc>*rB or <esc>*rC), or implicitly by any
 * commmand which is neither legal nor ignored in graphics mode.
 */
  int
pcl_end_graphics_mode(
    pcl_state_t *   pcs
)
{
    gs_point        cur_pt;
    gs_matrix       dev2pd;

    /* close the raster; exit graphics mode */
    pcl_complete_raster(pcs);
    pcs->raster_state.graphics_mode = false;

    /* get the new current point; then restore the graphic state */
    gs_transform(pcs->pgs, 0.0, 0.0, &cur_pt);
    pcl_grestore(pcs);

    /* transform the new point back to "pseudo print direction" space */
    pcl_invert_mtx(&(pcs->xfm_state.pd2dev_mtx), &dev2pd);
    gs_point_transform(cur_pt.x, cur_pt.y, &dev2pd, &cur_pt);
    pcl_set_cap_x(pcs, (coord)(cur_pt.x + 0.5), false, false);
    return pcl_set_cap_y( pcs,
                          (coord)(cur_pt.y + 0.5) - pcs->margins.top,
                          false,
                          false,
                          false,
                          false
                          );
}


/*
 * ESC * t # R
 *
 * Set raster graphics resolution. 
 * The value provided will be rounded up to the nearest legal value or down to 600dpi. 
 * 75 100 150 200 300 600 are legal;  120 and 85.7143 are multiples of 75 but not legal.
 */
  static int
set_graphics_resolution(
    pcl_args_t *    pargs,
    pcl_state_t *   pcs
)
{
    uint            res = arg_is_present(pargs) ? uint_arg(pargs) : 75;
    uint            qi = 600 / res;


    /* HP does not allow 120 dpi or 85.7 dpi as a resolution */
    qi = (qi == 0 ? 1 : (qi > 8 ? 8 : (qi == 7 ? 6 : (qi == 5 ? 4 : qi))));

    /* ignore if already in graphics mode */
    if (!pcs->raster_state.graphics_mode)
        pcs->raster_state.resolution = 600 / qi;

    return 0;
}

/*
 * ESC * r # F
 *
 * Set raster graphics presentation mode.
 *
 * This command is ignored if values other than 0 and 3 are provided, ignoring
 * any sign. The command is also ignored inside graphics mode.
 */
  static int
set_graphics_presentation_mode(
    pcl_args_t *    pargs,
    pcl_state_t *   pcs
)
{
    uint            mode = uint_arg(pargs);

    if (!pcs->raster_state.graphics_mode) {
        if (mode == 3)
            pcs->raster_state.pres_mode_3 = 1;
        else if (mode == 0)
            pcs->raster_state.pres_mode_3 = 0;
    }

    return 0;
}

/*
 * ESC * r # S
 *
 * Set raster width. Note that the useable width may be less due to clipping.
 * This implementation ignores the sign of the dimension, which matches the
 * behavior of the HP Color LaserJet 5/5M. The behavior of the of the DeskJet
 * 1600C/CM differs: it ignores the command if a negative operand is provided.
 *
 * This command is ignored in graphics mode.
 */
  static int
set_src_raster_width(
    pcl_args_t *    pargs,
    pcl_state_t *   pcs
)
{
    if (!pcs->raster_state.graphics_mode) {
        pcs->raster_state.src_width = uint_arg(pargs);
        pcs->raster_state.src_width_set = true;
    }
    return 0;
}

/*
 * ESC * r # t
 *
 * Set raster height. Note that the useable height may be less due to clipping.
 * This implementation ignores the sign of the dimension, which matches the
 * behavior of the HP Color LaserJet 5/5M. The behavior of the of the DeskJet
 * 1600C/CM differs: it ignores the command if a negative operand is provided.
 *
 * This command is ignored in graphics mode.
 */
  static int
set_src_raster_height(
    pcl_args_t *    pargs,
    pcl_state_t *   pcs
)
{
    if (!pcs->raster_state.graphics_mode) {
        pcs->raster_state.src_height = uint_arg(pargs);
        pcs->raster_state.src_height_set = true;
    }
    return 0;
}

/*
 * ESC * b # M
 *
 * Set compresson method.
 *
 * This command is unique among PCL commands in that it is interpreted both
 * inside and outside of graphics mode, and its execution neither starts nor
 * ends graphic mode.
 *
 * It is not possible to use adaptive compression (mode 5) with mutliple plane
 * pixel encodings, but it is not possible to check for a conflict at this
 * point as the pixel encoding may be changed before any raster data is
 * transfered. Hence, the transfer raster data command must perform the required
 * check.
 */
  static int
set_compression_method(
    pcl_args_t *    pargs,
    pcl_state_t *   pcs
)
{
    uint            mode = uint_arg(pargs);

    if ( (mode < count_of(pcl_decomp_proc))                                 &&
         ((pcl_decomp_proc[mode] != 0) || (mode == (uint)ADAPTIVE_COMPRESS))  )
        pcs->raster_state.compression_mode = mode;
    return 0;
}

/*
 * ESC * t # H
 *
 * Set destination raster width, in decipoints. This implementation follows that
 * of the HP Color LaserJet 5/5M in that it ignores the sign of the operand; the
 * DeskJet 1600 C/CM has different behavior. Note that the stored value is in
 * centi-points, while the operand is in deci-points.
 *
 * Though it is not noted in the "PCL 5 Color Technical Reference Manual", this
 * command is ignored in graphics mode.
 */
  static int
set_dest_raster_width(
    pcl_args_t *    pargs,
    pcl_state_t *   pcs
)
{
    if (!pcs->raster_state.graphics_mode) {
	if ( arg_is_present(pargs) ) {
	    uint    dw = 10 * fabs(float_arg(pargs));

	    pcs->raster_state.dest_width_cp = dw;
	    pcs->raster_state.dest_width_set = (dw != 0);
	}
	else
	    pcs->raster_state.dest_width_set = false;
    }
    return 0;
}

/*
 * ESC * t # V
 *
 * Set destination raster height, in decipoints. This implementation follows that
 * of the HP Color LaserJet 5/5M in that it ignores the sign of the operand; the
 * DeskJet 1600 C/CM has different behavior. Note that the stored value is in
 * centi-points, while the operand is in deci-points.
 *
 * Though it is not noted in the "PCL 5 Color Technical Reference Manual", this
 * command is ignored in graphics mode.
 */
  static int
set_dest_raster_height(
    pcl_args_t *    pargs,
    pcl_state_t *   pcs
)
{
    if (!pcs->raster_state.graphics_mode) {	
	if ( arg_is_present(pargs) ) {
	    uint    dh = 10 * fabs(float_arg(pargs));

	    pcs->raster_state.dest_height_cp = dh;
	    pcs->raster_state.dest_height_set = (dh != 0);
	}
	else
	    pcs->raster_state.dest_height_set = false;
    }
    return 0;
}

/*
 * ESC * r # A
 *
 * Start raster graphics mode.
 *
 * See the commment ahead of the procedure pcl_enter_graphics mode above for
 * a discussion of the rather curios manner in which the left raster graphics
 * margin is set below.
 */
  static int
start_graphics_mode(
    pcl_args_t *            pargs,
    pcl_state_t *           pcs
)
{
    pcl_gmode_entry_t       mode = (pcl_gmode_entry_t)uint_arg(pargs);
    pcl_raster_state_t *    prstate = &(pcs->raster_state);

    if (mode > SCALE_CUR_PTR)
        mode = NO_SCALE_LEFT_MARG;
    if (!prstate->graphics_mode) {
        int r90 = (pcs->xfm_state.lp_orient + pcs->xfm_state.print_dir) & 0x1;

        prstate->scale_raster = ((((int)mode) & 0x2) != 0);
        prstate->gmargin_cp = 0;
        if (prstate->pres_mode_3 && (r90 != 0))
            prstate->gmargin_cp += inch2coord(1.0 / 6.0);
        pcl_enter_graphics_mode(pcs, mode);
    }
    return 0;
}

/*
 * ESC * r # B
 *
 * End raster graphics mode - old style.
 */
  static int
end_graphics_mode_B(
    pcl_args_t *        pargs,
    pcl_state_t *       pcs
)
{
    if (pcs->raster_state.graphics_mode)
        pcl_end_graphics_mode(pcs);
    return 0;
}

/*
 * ESC * r # C
 *
 * End raster graphics mode - new style. This resets the compression mode and
 * the left grahics margin, in addition to ending graphics mode.
 */
  static int
end_graphics_mode_C(
    pcl_args_t *        pargs,
    pcl_state_t *       pcs
)
{
    if (pcs->raster_state.graphics_mode)
        pcl_end_graphics_mode(pcs);
    pcs->raster_state.gmargin_cp = 0L;
    pcs->raster_state.compression_mode = 0;
    return 0;
}


/*
 * Initialization
 */
  static int
gmode_do_registration(
    pcl_parser_state_t *pcl_parser_state,
    gs_memory_t *   pmem    /* ignored */
)
{
    DEFINE_CLASS('*')
    {
        't', 'R',
         PCL_COMMAND( "Raster Graphics Resolution",
                      set_graphics_resolution,
                      pca_raster_graphics | pca_neg_ok | pca_big_clamp | pca_in_rtl
                      )
    },
    {
        'r', 'F',
         PCL_COMMAND( "Raster Graphics Presentation Mode",
                      set_graphics_presentation_mode,
                      pca_raster_graphics | pca_neg_ok | pca_big_ignore | pca_in_rtl
                      )
    },
    {
        'r', 'S',
         PCL_COMMAND( "Source Raster Width",
                      set_src_raster_width,
                      pca_raster_graphics | pca_neg_ok | pca_big_clamp | pca_in_rtl
                      )
    },
    {
        'r', 'T',
        PCL_COMMAND( "Source Raster_Height",
                     set_src_raster_height,
                     pca_raster_graphics | pca_neg_ok | pca_big_clamp | pca_in_rtl
                     )
    },
    {
        'b', 'M',
        PCL_COMMAND( "Set Compresion Method",
                     set_compression_method,
                     pca_raster_graphics | pca_neg_ok | pca_big_ignore | pca_in_rtl
                     )
    },
    {
        't', 'H',
        PCL_COMMAND( "Destination Raster Width",
                     set_dest_raster_width,
                     pca_raster_graphics | pca_neg_ok | pca_big_ignore | pca_in_rtl
                     )
    },
    {
        't', 'V',
        PCL_COMMAND( "Destination Raster Height",
                     set_dest_raster_height,
                     pca_raster_graphics | pca_neg_ok | pca_big_ignore | pca_in_rtl
                     )
    },
    {
        'r', 'A',
        PCL_COMMAND( "Start Raster Graphics",
                     start_graphics_mode,
                     pca_raster_graphics | pca_neg_ok | pca_big_clamp | pca_in_rtl
                     )
    },
    {
        'r', 'B',
        PCL_COMMAND( "End Raster Graphics (Old)",
                     end_graphics_mode_B,
                     pca_raster_graphics | pca_neg_ok | pca_big_ok | pca_in_rtl
                     )
    },
    {
        'r', 'C',
        PCL_COMMAND( "End Raster Graphics (New)",
                     end_graphics_mode_C,
                     pca_raster_graphics | pca_neg_ok | pca_big_ok | pca_in_rtl
                     )
    },
    END_CLASS
    return 0;
}

  static void
gmode_do_reset(
    pcl_state_t *       pcs,
    pcl_reset_type_t    type
)
{
    static  const uint  mask = (  pcl_reset_initial
                                | pcl_reset_printer
                                | pcl_reset_overlay );

    if ((type & mask) != 0) {
        pcl_raster_state_t *    prstate = &(pcs->raster_state);

	prstate->gmargin_cp = 0L;
        prstate->resolution = 75;
        prstate->pres_mode_3 = true;
        prstate->scale_raster = false;
        prstate->src_width_set = false;
        prstate->src_height_set = false;
        prstate->dest_width_set = false;
        prstate->dest_height_set = false;
        prstate->scale_algorithm = 0;
        prstate->graphics_mode = false;
        prstate->compression_mode = NO_COMPRESS;
        prstate->y_advance = 1;
    }

    if (type &  pcl_reset_permanent)
        if (pcs->raster_state.graphics_mode) {
            /* pcl_end_graphics will call grestore, likely there will
               be no other graphic states on the stack, because
               everything has been taken down for permananent reset,
               if that is the case the grestore will result in a
               wraparound gsave, see the code in gsstate, thus the
               need for a subsequent gs_grestore_only. */
            pcl_end_graphics_mode(pcs);
            gs_grestore_only(pcs->pgs);
        }
}

const pcl_init_t    rtgmode_init = { gmode_do_registration, gmode_do_reset, 0 };
